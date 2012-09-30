/**
 * db.c
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 *      @todo 引入单元测试框架
 */

#include "db.h"
#include "flash.h"
#include <stdlib.h>
#include <string.h>

/// 是否支持对同一队列同时有多个read open
#define MULTIPLE_READ 0

/// 是否支持地址修正
#define NEED_CORRECT_ADDRESS 0

#define DATA_AVAIL 0xfe
#define DATA_NAN   0xff

static const struct
{
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr earseSize;
} db = {0x0100, 0xff00, EARSE_SIZE};

typedef struct _Db_child
{
	Db_addr startAddr;
} Db_child;

typedef struct _Db_Info
{
	uint8_t symbol;
	uint8_t checksum;
	Db_time time;
	Db_child child;
} Db_Info;

/* 数据库控制块 */
typedef const struct
{
	type1_t type1;
	type2_t type2;
	type2_t child_type2;
	size_t data_len;
	int max_num;
//	Queue *pque;
	int		que_index;
} Ctrl;

typedef struct _Test_Data
{
	uint32_t val[4];
} Test_data;

static Ctrl ctrls[] = {
		{ TEST, MIN, 	NONE1,	sizeof(Test_data), 2500, 0},
		{ TEST, DAY, 	MIN,	sizeof(Test_data), 48,	1},
		{ TEST, WEEK, 	DAY,	sizeof(Test_data), 7,	2},
		{ TEST, MONTH, 	DAY,	sizeof(Test_data), 14,	3},
		{ TEST, YEAR, 	MONTH,	sizeof(Test_data), 2,	4},
};

static Queue ques[ARRAY_LENG(ctrls)];

#define DEBUG 1
#if DEBUG
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

static inline void _init(void);
static inline bool _write(Db_addr addr, void *pdata, size_t len);
static inline bool _read(Db_addr addr, void *pdata, size_t len);
static bool _init_ques(void);
static size_t _size_of_info(Queue *pque);

void db_init(void)
{
	Db_addr all, used;
	float use_pec, kb;

	_init();
	_init_ques();

	all = db.endAddr - db.startAddr;
	used = ques[ARRAY_LENG(ques) - 1].endAddr - ques[0].startAddr;
	use_pec = 100 * (float)used / (float)all;
	kb = (float)used / 1024;

	debug("db info:\n");
	debug("startAddr = %#08x\n", db.startAddr);
	debug("endAddr   = %#08x\n", db.endAddr);
	debug("earseSize = %#08x\n", db.earseSize);
	debug("used: %#08x / %#08x = %.2f%%, %.2fKB\n", used, all, use_pec, kb);
}

static Ctrl *_get_ctrl(type1_t type1, type2_t type2)
{
	int i;
	Ctrl *pctrl = NULL;

	for(i = 0; i < sizeof(ctrls) / sizeof(ctrls[0]); ++i)
	{
		if(ctrls[i].type1 != type1)
		{
			continue;
		}
		else if(ctrls[i].type2 == type2)
		{
			pctrl = &ctrls[i];
		}
	}

	return pctrl;
}

static Queue *_get_que(Ctrl *pctrl)
{
	assert(pctrl != NULL);

#if 0
	int i;

	for(i = 0; i < ARRAY_LENG(ctrls); i++)
	{
		if(pctrl == &ctrls[i])
		{
			return &ques[i];
		}
	}
	return NULL;
#else
	return &ques[pctrl->que_index];
#endif

}

Queue *db_open(type1_t type1, type2_t type2, int flags, ...)
{
	Ctrl *pctrl;
	Queue *pque = NULL;

	pctrl = _get_ctrl(type1, type2);
	if(pctrl == NULL )
		return NULL ;

#if MULTIPLE_READ
	if(flags == O_RDONLY)
	{
		pque = (Queue *)malloc(sizeof(Queue));
		if(pque == NULL )
		{
			return NULL ;
		}

		memcpy(pque, _get_que(pctrl), sizeof(Queue));
		pque->flags = flags;
		pque->pctrl = pctrl;
	}
	else if(flags == O_WRONLY)
	{
		pque = _get_que(pctrl);
		pque->flags = flags;
		pque->pctrl = pctrl;
	}
#else
	pque = _get_que(pctrl);
	pque->flags = flags;
	pque->pctrl = pctrl;
#endif

	return pque;
}

bool db_close(Queue *pque)
{

	assert(pque != NULL);

#if MULTIPLE_READ
	if(pque->flags == O_RDONLY)
	{
		/* 保存当前的读地址，以使下次打开时的读地址与当前的一致 */
		Queue *pmain_que = _get_que(pque->pctrl);
		pmain_que->readAddr = pque->readAddr;
		pmain_que->flags = O_NOACCESS;

		free(pque);

		return true;
	}
#endif

	pque->flags = O_NOACCESS;

	return true;
}

static bool _have_child(Queue *pque)
{
	Ctrl *pctrl;

	pctrl = pque->pctrl;
	if(pctrl->child_type2 == NONE2)
		return false;

	return true;
}

static size_t _size_of_info(Queue *pque)
{
	Db_Info info;

	if(_have_child(pque))
		return sizeof(info);
	else
		return sizeof(info.symbol) + sizeof(info.checksum) + sizeof(info.time.min)
		        + sizeof(info.time.hour);
}

static uint8_t _calc_checksum(Queue *pque, Db_Info *pinfo, void *pdata, size_t data_len)
{
	uint8_t *p;
	uint8_t checksum = 0x55;
	int i;
	size_t info_len;

	assert(pque != NULL);
	assert(pinfo != NULL);
	assert(pdata != NULL);

	p = (uint8_t *)pinfo;
	info_len = _size_of_info(pque);
	for(i = 0; i < info_len; i++)
	{
		checksum += p[i];
	}

	p = pdata;
	for(i = 0; i < data_len; i++)
	{
		checksum += p[i];
	}

	return checksum;
}

static Db_addr _get_next_addr(Queue *pque, int dire)
{
	Db_addr addr, curAddr;
	size_t 	len;
	Db_addr	area_len;

	if(pque->flags == O_RDONLY)
		curAddr = pque->readAddr;
	else if(pque->flags == O_WRONLY)
		curAddr = pque->writeAddr;
	else
		curAddr = 0x00;

	len = _size_of_info(pque) + pque->data_len;
	area_len = pque->endAddr - pque->startAddr;

	if(dire > 0)
		addr = curAddr + len;
	else if(dire < 0)
		addr = curAddr - len;
	else
		addr = curAddr;

	if(addr + len > pque->endAddr)
		addr = pque->startAddr;
	else if(addr < pque->startAddr)
		addr = pque->endAddr - len - (area_len % len);

#if NEED_CORRECT_ADDRESS
	/* 校正地址 */
	size_t 	offset;
	offset = (addr - pque->startAddr) % len;
	addr -= offset;
#endif

	return addr;
}

static Db_child _get_child(Queue *pque)
{
	Db_child child;
	Ctrl *pctrl;
	Queue *pchild_que;

	assert(pque != NULL);

	pctrl = pque->pctrl;
	if(pctrl->child_type2 != NONE2)
	{
		pchild_que = db_open(pctrl->type1, pctrl->child_type2, O_WRONLY);
		assert(pchild_que != NULL);

		child.startAddr = _get_next_addr(pchild_que, -1);
		db_close(pchild_que);
	}
	else
	{
		child.startAddr = 0x00;
	}

	return child;
}

/**
 *
 * @param pctrl
 * @param pdata
 * @param ptime
 * @param len
 * @return
 * @todo	验证写入的正确性
 * @todo	处理坏块
 */
bool db_write(Queue *pque, void *pdata, Db_time *ptime, size_t len)
{
	Db_Info info;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(len == pque->data_len);
	assert(pque->flags == O_WRONLY);

	info.symbol = DATA_AVAIL;
	info.time = *ptime;
	info.child = _get_child(pque);
	info.checksum = _calc_checksum(pque, &info, pdata, len);

	_write(pque->writeAddr, &info, _size_of_info(pque));
	_write(pque->writeAddr + _size_of_info(pque), pdata, len);

	pque->writeAddr = _get_next_addr(pque, 1);

	return true;
}

/**
 *
 * @param pctrl
 * @param pdata
 * @param len
 * @param dirt
 * @return
 * @todo	处理坏块
 */
bool db_read(Queue *pque, void *pdata, Db_time *ptime, size_t len)
{
	Db_Info info;
	bool stat;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(len == pque->data_len);
	assert(pque->flags == O_RDONLY);

	stat = _read(pque->readAddr, &info, _size_of_info(pque));
	if(info.symbol != DATA_AVAIL)
		return false;

	stat &= _read(pque->readAddr + _size_of_info(pque), pdata, len);
	if(info.checksum != _calc_checksum(pque, &info, pdata, len))
		return false;

	if(ptime != NULL )
	{
		memcpy(ptime, &info.time, sizeof(Db_time));
	}
//	pque->readAddr = _get_next_addr(pque, -1);

	return true;
}

static bool _read_time(Queue *pque, Db_time *ptime)
{

	return false;
}

bool db_seek(Queue *pque, int sym)
{

	return 0;
}

static type2_t _get_child_type(Queue *pque)
{
	Ctrl *pctrl = pque->pctrl;

	return pctrl->child_type2;
}

static bool _time_match(Db_time *pt1, Db_time *pt2, type2_t type2)
{
	return false;
}

Queue * db_locate(type1_t type1, type2_t type2, Db_time *ptime)
{
	Queue *pque;
	type2_t locate_type2, next_type2;
	Db_time rtime;
	bool match;

//	pctrl = get_ctrl_by_type(type1, YEAR);
//	pque = _get_que(pctrl);
	next_type2 = YEAR;
	do
	{
		locate_type2 = next_type2;
		pque = db_open(type1, locate_type2, O_RDONLY);
		if(pque == NULL )
			return NULL ;

		match = FALSE;
		while(1)
		{
			if(_read_time(pque, &rtime) < 0)
				break;

			if(_time_match(ptime, &rtime, locate_type2))
			{
				match = TRUE;
				break;
			}

			if(db_seek(pque, -1) == EOF)
				break;
		}

		if(match && locate_type2 == type2)
		{
			return pque;
		}
		else if(match)
		{
			next_type2 = _get_child_type(pque);
			db_close(pque);
		}
		else
		{
			db_close(pque);
			return NULL ;
		}

	} while(locate_type2 != type2);

	return NULL ;
}

/**
 *
 * @param pque
 * @return
 * @todo 确认整个头部为空的
 */
static Db_addr _find_queue_head(Queue *pque)
{
	size_t db_data_len;
	Db_addr  findAddr;
	uint8_t symbol;
	bool found = false;
	
	assert(pque != NULL);

	db_data_len = _size_of_info(pque) + pque->data_len;
	
	findAddr = pque->startAddr;
	symbol = 0x00;
	while((findAddr + db_data_len) < pque->endAddr)
	{
		_read(findAddr + FPOS(Db_Info, symbol), &symbol, sizeof(symbol));  //只读取数据的标识
		if(symbol == DATA_NAN)	//如果是空数据，说明找到了最新数据，即头
		{
			found = true;
			break;
		}
		
		findAddr += db_data_len;
	}
	
	if(!found)
		findAddr = pque->startAddr;
	
	return findAddr;
}

/** 初始化队列
 *
 * @return
 */
static bool _init_ques(void)
{
	Db_addr addr;
	Queue *pque;
	size_t datas_len;	// 所有 info信息 + 待存储数据 长度之和
	size_t que_len;			// 队列所需的长度
	int i;
	bool stat = true;

	addr = db.startAddr;
	for(i = 0; i < ARRAY_LENG(ctrls); i++)
	{
		pque = &ques[i];
		pque->pctrl = &ctrls[i];

		pque->startAddr = addr;
		datas_len = (_size_of_info(pque) + ctrls[i].data_len) * ctrls[i].max_num;
		que_len = ( 2 + datas_len / db.earseSize) * db.earseSize;
		pque->endAddr = pque->startAddr + que_len;

		pque->readAddr = 0x00;
		pque->writeAddr = _find_queue_head(pque);
		pque->data_len = ctrls[i].data_len;
		pque->flags = O_NOACCESS;

		if(pque->endAddr > db.endAddr)
		{
			debug("DB Error: Out of range!\n");
			stat = false;
		}

		addr = pque->endAddr + 1;
	}

	return stat;
}

static inline void _init(void)
{
	flash_init();
}

/**
 *
 * @param addr
 * @param pdata
 * @param len
 * @return
 */
static inline bool _write(Db_addr addr, void *pdata, size_t len)
{
	return flash_write(addr, pdata, len);
}

/**
 *
 * @param addr
 * @param pdata
 * @param len
 * @return
 */
static inline bool _read(Db_addr addr, void *pdata, size_t len)
{
	return flash_read(addr, pdata, len);
}
