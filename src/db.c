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
	uint8_t flag;
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
	Queue *pque;
} Ctrl;

typedef struct _Test_Data
{
	uint32_t val;
} Test_data;

static Queue ques[7];

static Ctrl ctrls[] = {
		{ TEST, SEC,	NONE1,	sizeof(Test_data), 120, &ques[0]},
		{ TEST, MIN, 	SEC,	sizeof(Test_data), 120, &ques[1]},
		{ TEST, HOUR, 	MIN,	sizeof(Test_data), 48, &ques[2]},
		{ TEST, DAY, 	HOUR,	sizeof(Test_data), 30, &ques[3]},
		{ TEST, WEEK, 	DAY,	sizeof(Test_data), 7, 	&ques[4]},
		{ TEST, MONTH, 	DAY,	sizeof(Test_data), 12, &ques[5]},
		{ TEST, YEAR, 	MONTH,	sizeof(Test_data), 2, 	&ques[6]},
};

#define DEBUG 1
#if DEBUG
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

static inline void _init(void);
static inline int _write(Db_addr addr, void *pdata, size_t len);
static inline int _read(Db_addr addr, void *pdata, size_t len);
static int _malloc(void);
static int _fill_heads(void);
static size_t _size_of_info(Queue *pque);

void db_init(void)
{
	_init();

	if(_malloc() == -1)
	{
//		debug
	}

	if(_fill_heads() < 0)
	{
		debug("Fill Heads Error!\n");
	}

	debug("db info:\n");
	debug("startAddr = %#08x\n", db.startAddr);
	debug("endAddr   = %#08x\n", db.endAddr);
	debug("earseSize = %#08x\n", db.earseSize);
	debug("used: %%u / %%u = %%.2f\n");
}

//static Que_ctrl *get_ctrl_by_type(type1_t type1, type2_t type2)
//{
//
//	return NULL ;
//}
//
//static Que_ctrl *get_ctrl_by_queue(Queue *pque)
//{
//
//	return NULL ;
//}

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

Queue *db_open(type1_t type1, type2_t type2, int flags, ...)
{
	Ctrl *pctrl;
	Queue *pque = NULL;

	pctrl = _get_ctrl(type1, type2);
	if(pctrl == NULL )
		return NULL ;

	assert(pctrl->pque != NULL);

	if(flags == O_RDONLY)
	{
		pque = (Queue *)malloc(sizeof(Queue));
		if(pque == NULL )
		{
			return NULL ;
		}

		memcpy(pque, pctrl->pque, sizeof(Queue));
		pque->flags = flags;
		pque->pctrl = pctrl;
	}
	else if(flags == O_WRONLY)
	{
		pque = pctrl->pque;
		pque->flags = flags;
		pque->pctrl = pctrl;
	}

	return pque;
}

int db_close(Queue *pque)
{
	if(pque->flags == O_RDONLY)
	{
		assert(pque != NULL);

		/* 保存当前的读地址，以使下次打开时的读地址与当前的一致 */
		Ctrl *pctrl = pque->pctrl;
		pctrl->pque->readAddr = pque->readAddr;

		free(pque);
	}

	return 0;
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
		return sizeof(info.flag) + sizeof(info.checksum) + sizeof(info.time.min)
		        + sizeof(info.time.hour);
}

static uint8_t _calc_checksum(Queue *pque, Db_Info *pinfo, void *pdata, size_t data_len)
{
	return 0;
}

static Db_addr _get_next_addr(Queue *pque, int dirt)
{
	return 0;
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
int db_write(Queue *pque, void *pdata, Db_time *ptime, size_t len)
{
	Db_Info info;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(len == pque->data_len);
	assert(pque->flags == O_WRONLY);

	info.flag = DATA_AVAIL;
	info.time = *ptime;
	info.child = _get_child(pque);
	info.checksum = _calc_checksum(pque, &info, pdata, len);

	_write(pque->writeAddr, &info, _size_of_info(pque));
	_write(pque->writeAddr + _size_of_info(pque), pdata, len);

	pque->writeAddr = _get_next_addr(pque, 1);

	return DB_OK;
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
int db_read(Queue *pque, void *pdata, Db_time *ptime, size_t len)
{
	Db_Info info;
	bool stat;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(len == pque->data_len);
	assert(pque->flags == O_RDONLY);

	stat = _read(pque->readAddr, &info, _size_of_info(pque));
	if(info.flag != DATA_AVAIL)
		return DB_ERR;

	stat &= _read(pque->readAddr + _size_of_info(pque), pdata, len);
	if(info.checksum != _calc_checksum(pque, &info, pdata, len))
		return DB_ERR;

	if(ptime != NULL )
	{
		memcpy(ptime, &info.time, sizeof(Db_time));
	}
//	pque->readAddr = _get_next_addr(pque, -1);

	return DB_OK;
}

static int _read_time(Queue *pque, Db_time *ptime)
{

	return 0;
}

int db_seek(Queue *pque, int sym)
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
	return FALSE;
}

Queue * db_locate(type1_t type1, type2_t type2, Db_time *ptime)
{
	Queue *pque;
	type2_t locate_type2, next_type2;
	Db_time rtime;
	bool match;

//	pctrl = get_ctrl_by_type(type1, YEAR);
//	pque = pctrl->pque;
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

static int _malloc(void)
{

	return 0;
}

static Db_addr _find_queue_head(Queue *pque)
{
	return 0;
}

static int _fill_heads(void)
{
	Queue *pque = NULL;

	_find_queue_head(pque);

	return 0;
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
 * @todo	匹配返回值原型
 */
static inline int _write(Db_addr addr, void *pdata, size_t len)
{
	return flash_write(addr, pdata, len);
}

/**
 *
 * @param addr
 * @param pdata
 * @param len
 * @return
 * @todo	匹配返回值原型
 */
static inline int _read(Db_addr addr, void *pdata, size_t len)
{
	return flash_read(addr, pdata, len);
}
