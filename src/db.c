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
/// 是否在地址递变时执行地址修正
#define DO_CORRECT_ADDRESS 0
/// 是否需要加密数据
#define NEED_ENCRYPT 1

#if NEED_ENCRYPT
#	define ENCRYPT(a) ((a) ^ 0x55)
#else
#	define ENCRYPT(a) (a)
#endif

/// @note 目前要求加密与解密必须完全相同
#define DECRYPT(a) ENCRYPT(a)

#define DATA_AVAIL DECRYPT(0xfe)
#define DATA_NAN   DECRYPT(0xff)

#define YEAR_NUM 	2
#define MONTH_NUM 14
#define WEEK_NUM	7
#define DAY_NUM	48
#define MINUS_NUM	(DAY_NUM * 48)

/// @todo 提供擦除的掩码值,以优化地址递变和擦除的判断
static const struct
{
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr earseSize;
} db = {.startAddr = 0x0155,
		.endAddr   = 0xfeff,
		.earseSize = EARSE_SIZE
};

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
		{ TEST, MIN, 	NONE1,	sizeof(Test_data), MINUS_NUM, 0},
		{ TEST, DAY, 	MIN,	sizeof(Test_data), DAY_NUM,	1},
		{ TEST, WEEK, 	DAY,	sizeof(Test_data), WEEK_NUM,	2},
		{ TEST, MONTH, 	DAY,	sizeof(Test_data), MONTH_NUM,	3},
		{ TEST, YEAR, 	MONTH,	sizeof(Test_data), YEAR_NUM,	4},
};

static Queue ques[ARRAY_LENG(ctrls)];

#define DEBUG 1
#if DEBUG
#define debug(fmt, args...) printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif

static inline void _init(void);
static bool _write(Db_addr addr, void *pdata, size_t len);
static bool _read(Db_addr addr, void *pdata, size_t len);
static bool _init_ques(void);
static size_t _info_len(Queue *pque);

/** db初始化
 *
 */
void db_init(void)
{
	Db_addr all, used;
	float use_pec, kb;

	_init();	// 初始化硬件

	debug("db info:\n");
	debug("startAddr = %#08x\n", db.startAddr);
	debug("endAddr   = %#08x\n", db.endAddr);
	debug("earseSize = %#08x\n", db.earseSize);
	debug("Sizeof ques[] = %u\n", sizeof(ques));

	_init_ques();	// 初始化队列

	all = db.endAddr - db.startAddr + 1;
	used = ques[ARRAY_LENG(ques) - 1].endAddr - ques[0].startAddr + 1;
	use_pec = 100.0 * used / all;
	kb = used / 1024.0;

	debug("data used: %#08x / %#08x = %.2f%%, %.2fKB\n", used, all, use_pec, kb);
}

/** 获取某数据对应的控制快
 *
 * @param type1
 * @param type2
 * @return
 */
static Ctrl *_get_ctrl(type1_t type1, type2_t type2)
{
	int i;
	Ctrl *pctrl = NULL;

	for(i = 0; i < ARRAY_LENG(ctrls); ++i)
	{
		if(ctrls[i].type1 != type1)
		{
			continue;
		}
		else if(ctrls[i].type2 == type2)
		{
			pctrl = &ctrls[i];
			break;
		}
	}

	return pctrl;
}

/** 获取队列结构
 *
 * @param pctrl
 * @return
 */
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

/** 打开队列
 *
 * @param type1
 * @param type2
 * @param flags
 * @return
 * @todo	合并队列结构中的flags 和 dire
 */
Queue *db_open(type1_t type1, type2_t type2, int flags, ...)
{
	Ctrl *pctrl;
	Queue *pque = NULL;

	pctrl = _get_ctrl(type1, type2);

	assert(pctrl != NULL);

#if MULTIPLE_READ
	if(flags == DB_R)
	{
		pque = (Queue *)malloc(sizeof(Queue));
		if(pque == NULL )
		{
			return NULL ;
		}

		memcpy(pque, _get_que(pctrl), sizeof(Queue));
		pque->flags = flags;
	}
	else if(flags == DB_A)
	{
		pque = _get_que(pctrl);
		pque->flags = flags;
	}
#else
	pque = _get_que(pctrl);
	pque->flags = flags;
#endif

	return pque;
}

/** 关闭已打开的队列
 *
 * @param pque
 * @return
 */
bool db_close(Queue *pque)
{

	assert(pque != NULL);

#if MULTIPLE_READ
	if(pque->flags == DB_R)
	{
		/* 保存当前的读地址，以使下次打开时的读地址与当前的一致 */
		Queue *pmain_que = _get_que(pque->pctrl);
		pmain_que->accessAddr = pque->accessAddr;
		pmain_que->flags = DB_N;
		pmain_que->dire = 0;

		free(pque);

		return true;
	}
#endif

	pque->flags = DB_N;
	pque->dire = 0;

	return true;
}

/** 判断队列是否有子类数据
 *
 * @param pque
 * @return
 */
static bool _have_child(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;
	if(pctrl->child_type2 == NONE2)
		return false;

	return true;
}

/** 获取队列的info结构的长度
 *
 * @param pque
 * @return
 */
static size_t _info_len(Queue *pque)
{
	Db_Info info;

	assert(pque != NULL);

	if(_have_child(pque))
		return sizeof(info);
	else
		return sizeof(info.symbol) + sizeof(info.checksum) + sizeof(info.time.min)
		        + sizeof(info.time.hour);
}

/** 获取客户数据的长度
 *
 * @param pque
 * @return
 */
static size_t _data_len(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;

	return pctrl->data_len;
}

/** 获取客户数据和其info结构的长度之和
 *
 * @param pque
 * @return
 */
static size_t _db_data_len(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;

	return pctrl->data_len + _info_len(pque);
}

/** 计算校验码
 *
 * @param pque
 * @param pinfo
 * @param pdata
 * @param data_len
 * @return
 */
static uint8_t _calc_checksum(Queue *pque, Db_Info *pinfo, void *pdata, size_t data_len)
{
	uint8_t *p;
	uint8_t checksum = 0x55;
	int i;
	size_t info_len;

	assert(pque != NULL);
	assert(pinfo != NULL);
	assert(pdata != NULL);

	p = (uint8_t *)pinfo + sizeof(pinfo->symbol) + sizeof(pinfo->checksum);
	info_len = _info_len(pque) - (sizeof(pinfo->symbol) + sizeof(pinfo->checksum));
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

/** 获取队列在当前位置的下一地址
 *
 * @param pque
 * @param dire
 * @return
 */
static Db_addr _get_next_addr(Queue *pque, Db_addr curAddr, int dire)
{
	Db_addr addr;
	size_t 	len;
	Db_addr	area_len;

	assert(pque != NULL);

	len = _db_data_len(pque);
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

#if DO_CORRECT_ADDRESS
	/* 校正地址 */
	size_t 	offset;
	offset = (addr - pque->startAddr) % len;
	addr -= offset;
#endif

	return addr;
}

/** 获取队列的子类数据的地址信息
 *
 * @param pque
 * @return
 */
static Db_child _get_child(Queue *pque)
{
	Db_child child;
	Ctrl *pctrl;
	Queue *pchild_que;

	assert(pque != NULL);

	pctrl = pque->pctrl;
	if(pctrl->child_type2 != NONE2)
	{
		pchild_que = db_open(pctrl->type1, pctrl->child_type2, DB_R);
		assert(pchild_que != NULL);

		child.startAddr = _get_next_addr(pchild_que, pchild_que->headAddr, -1);
		db_close(pchild_que);
	}
	else
	{
		child.startAddr = 0x00;
	}

	return child;
}

/** 顺序追加数据
 *
 * @param pctrl
 * @param pdata
 * @param ptime
 * @param len
 * @return
 * @todo	验证写入的正确性
 * @todo	处理坏块
 */
bool db_append(Queue *pque, void *pdata, size_t data_len, Db_time *ptime)
{
	Db_Info info;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(data_len == _data_len(pque));
	assert(pque->flags == DB_A);

	info.symbol = DATA_AVAIL;
	info.time = *ptime;
	info.child = _get_child(pque);
	info.checksum = _calc_checksum(pque, &info, pdata, data_len);

	_write(pque->headAddr, &info, _info_len(pque));
	_write(pque->headAddr + _info_len(pque), pdata, data_len);

	pque->headAddr = _get_next_addr(pque, pque->headAddr, 1);

	return true;
}

/** 随机读取数据
 *
 * @param pque
 * @param pdata
 * @param len
 * @return
 * @todo	处理坏块
 */
bool db_read(Queue *pque, void *pdata, size_t data_len, Db_time *ptime)
{
	Db_Info info;
	uint8_t buf[data_len];
	bool stat;

	assert(pque != NULL);
//	assert(pdata != NULL);
	assert(data_len == _data_len(pque));
	assert(pque->flags == DB_R);

	if(pdata == NULL)
		pdata = &buf;

	stat = _read(pque->accessAddr, &info, _info_len(pque));
	if(info.symbol != DATA_AVAIL)
		return false;

	stat &= _read(pque->accessAddr + _info_len(pque), pdata, data_len);
	if(info.checksum != _calc_checksum(pque, &info, pdata, data_len))
		return false;

	if(ptime != NULL )
	{
		memcpy(ptime, &info.time, sizeof(Db_time));
	}

	pque->accessAddr = _get_next_addr(pque, pque->accessAddr, pque->dire);

	return true;
}

/** 随机写入数据
 *
 * @param pque
 * @param pdata
 * @param ptime
 * @param data_len
 * @return
 */
#if 0
bool db_write(Queue *pque, void *pdata, size_t data_len, Db_time *ptime)
{


	assert(pque != NULL);
	assert(pdata != NULL);
	assert(data_len == pque->data_len);
	assert(pque->flags == DB_A);



	return false;
}
#endif

/** 读取时间
 *
 * @param pque	队列
 * @param ptime	时间输出
 * @return 成功为true,否则为false
 * @todo  处理坏块
 */
static bool _read_time(Queue *pque, Db_time *ptime)
{
	assert(pque != NULL);
	assert(ptime != NULL);

	return db_read(pque, NULL, _data_len(pque), ptime);
}

/** 移动队列访问指针
 *
 * @param pque
 * @param sym
 * @return
 * @todo	要定位到队列的尾，需要在队列结构中添加尾指针成员
 */
bool db_seek(Queue *pque, int ndata, int whence, int dire)
{
	int ldire = 0;
	Db_addr addr = 0x00;

	switch (whence)
	{
		case SEEK_CUR:
			addr = pque->accessAddr;
			break;

		case SEEK_SET:
			addr = pque->headAddr;
			break;

		case SEEK_END:
			debug("SEEK_END is not supported!\n");
			return false;
			break;

		default:
			debug("Seek error!\n");
			break;
	}

	if(ndata > 0)
		ldire = 1;
	else if (ndata < 0)
		ldire = -1;
	else
		ldire = 0;

	while(ndata)
	{
		addr = _get_next_addr(pque, addr, ldire);
		ndata -= ldire;
	}

	pque->accessAddr = addr;
	pque->dire = dire;

	assert(pque != NULL);

	return true;
}

/** 获取队列的子类数据的类型
 *
 * @param pque
 * @return
 */
static type2_t _get_child_type(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;

	return pctrl->child_type2;
}

/** 判断给定数据类型上的两时间是否相同
 *
 * @param pt1
 * @param pt2
 * @param type2
 * @return
 */
static bool _time_match(type2_t type2, Db_time *pt1, Db_time *pt2)
{
	bool stat = false;

	assert(pt1 != NULL);
	assert(pt2 != NULL);

	switch (type2)
	{
		case YEAR:
			if(pt1->year == pt2->year)
				stat = true;
			break;

		case MONTH:
			if(pt1->year == pt2->year &&
					pt1->month == pt2->month)
				stat = true;
			break;

		case WEEK:
			if(pt1->year == pt2->year &&
					pt1->weeks == pt2->weeks)
				stat = true;
			break;

		case DAY:
			if(pt1->year == pt2->year &&
					pt1->month == pt2->month &&
					pt1->day == pt2->day)
				stat = true;
			break;

		case MIN:
			if(pt1->year == pt2->year &&
					pt1->month == pt2->month &&
					pt1->day == pt2->day &&
					pt1->hour == pt2->hour &&
					pt1->min == pt2->min)
				stat = true;
			break;

		default:
			debug("Wrong Type2: %d\n", type2);
			break;
	}

	return stat;
}

/** 定位某类型的数据在队列中的某时间点上的位置
 *
 * @param type1
 * @param type2
 * @param ptime
 * @return
 * @todo	区分周数据的定位
 */
Queue * db_locate(type1_t type1, type2_t type2, Db_time *ptime)
{
	Queue *pque;
	type2_t locate_type2, next_type2;
	Db_time rtime;
	bool match;

	assert(ptime != NULL);

	next_type2 = YEAR;
	do
	{
		locate_type2 = next_type2;
		pque = db_open(type1, locate_type2, DB_R);
		assert(pque != NULL);

		db_seek(pque, 0, SEEK_SET, -1);

		match = FALSE;
		while(1)
		{
			if(_read_time(pque, &rtime) < 0)
				break;

			if(_time_match(locate_type2, ptime, &rtime))
			{
				match = TRUE;
				break;
			}
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

/** 找出队列的头部
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

	db_data_len = _db_data_len(pque);
	
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
	
	debug("Head: %#08x\n", findAddr);

	return findAddr;
}

/** 初始化队列
 *
 * @return
 */
static bool _init_ques(void)
{
	Db_addr addr, left;
	Queue *pque;
	size_t datas_len;	// 所有 info信息 + 待存储数据 长度之和
	size_t que_len;			// 队列所需的长度
	int i;
	bool stat = true;

	addr = db.startAddr;

	/* 确保队列的起始地址位于页的起始地址 */
	left = addr % db.earseSize;
	if(left)
	{
		addr += db.earseSize - left;
	}

	for(i = 0; i < ARRAY_LENG(ctrls); i++)
	{
		pque = &ques[i];
		pque->pctrl = &ctrls[i];

		pque->startAddr = addr;
		datas_len = (_info_len(pque) + ctrls[i].data_len) * ctrls[i].max_num;
		que_len = ( 2 + datas_len / db.earseSize) * db.earseSize;
		pque->endAddr = pque->startAddr + que_len - 1;

		debug("Address: [%#08x, %#08x]\n", pque->startAddr, pque->endAddr);

		pque->headAddr = _find_queue_head(pque);
		pque->accessAddr = pque->headAddr;
		pque->flags = DB_N;
		pque->dire = 0;

		if(pque->endAddr > db.endAddr)
		{
			debug("DB Init Error: Out of range!\n");
			stat = false;
			break;
		}

		addr = pque->endAddr + 1;
	}

	return stat;
}

/** 底层硬件的初始化
 *
 */
static inline void _init(void)
{
	flash_init();
}

/** 数据加密
 *
 * @param dst
 * @param src
 * @param len
 */
static void _encrypt(void *dst, void *src, size_t len)
{
	int i;
	uint8_t *pd = dst;
	uint8_t *ps = src;

	assert(dst != NULL);
	assert(src != NULL);

	for(i = 0; i < len; i++)
		pd[i] = ENCRYPT(ps[i]);
}

/** 数据解密
 *
 * @param dst
 * @param src
 * @param len
 */
static inline void _decrypt(void *dst, void *src, size_t len)
{
	_encrypt(dst, src, len);
}

/** 底层硬件的写操作
 *
 * @param addr
 * @param pdata
 * @param len
 * @return
 */
static bool _write(Db_addr addr, void *pdata, size_t len)
{
	assert(pdata != NULL);

#if NEED_ENCRYPT
	uint8_t buf[len];

	_encrypt(&buf, pdata, len);
	pdata = &buf;
#endif

	return flash_write(addr, pdata, len);
}

/** 底层硬件的读操作
 *
 * @param addr
 * @param pdata
 * @param len
 * @return
 */
static bool _read(Db_addr addr, void *pdata, size_t len)
{
	assert(pdata != NULL);

#if NEED_ENCRYPT
	uint8_t buf[len];

	if(!flash_read(addr, &buf, len))
		return false;

	_decrypt(pdata, &buf, len);

	return true;
#else
	return flash_read(addr, pdata, len);
#endif
}
