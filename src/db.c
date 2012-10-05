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

/// 是否精简分钟数据的info结构的内容
#define SIMPLIFY_MINUS_INFO 0
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
	Db_addr endAddr;
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
		{ TEST, MINUS, 	NONE1,	sizeof(Test_data), MINUS_NUM, 0},
		{ TEST, DAY, 	MINUS,	sizeof(Test_data), DAY_NUM,	1},
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
		pque->dire = 0;
	}
	else if(flags == DB_A)
	{
		pque = _get_que(pctrl);
		pque->flags = flags;
		pque->dire = 0;
	}
#else
	pque = _get_que(pctrl);
	pque->flags = flags;
	pque->dire = 0;
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

/** 以复制队列结构的方式打开队列
 *
 * @param pque_buf
 * @param type1
 * @param type2
 * @param flags
 * @return
 * @note	以复制方式打开的队列无需调用 db_close 来关闭
 */
bool _open_by_copy(Queue *pque_buf, type1_t type1, type2_t type2, int flags, ...)
{
	Ctrl *pctrl;

	pctrl = _get_ctrl(type1, type2);
	if(pctrl == NULL)
		return false;

	memcpy(pque_buf, _get_que(pctrl), sizeof(Queue));
	pque_buf->flags = flags;
	pque_buf->dire = 0;

	return true;
}

/** 判断队列是否有子类数据
 *
 * @param pque
 * @return
 */
#if 0
static bool _have_child(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;
	if(pctrl->child_type2 == NONE2)
		return false;

	return true;
}
#endif

/** 获取队列的info结构的长度
 *
 * @param pque
 * @return
 */
static size_t _info_len(Queue *pque)
{
	Ctrl *pctrl;
	Db_Info info;
	size_t len = 0;

	assert(pque != NULL);

	pctrl = pque->pctrl;

#if SIMPLIFY_MINUS_INFO
	if(pctrl->type2 == MINUS)
	{
		len =  sizeof(info.symbol) + sizeof(info.checksum) + sizeof(info.time.min)
				+ sizeof(info.time.hour);
	}
	else
#endif
	if(pctrl->child_type2 == NONE2)
	{
		len = sizeof(info.symbol) + sizeof(info.checksum) + sizeof(info.time);
	}
	else
	{
		len = sizeof(info);
	}

	return len;
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
static size_t _info_data_len(Queue *pque)
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

	len = _info_data_len(pque);
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
	Queue child_que;
	bool res;

	assert(pque != NULL);

	pctrl = pque->pctrl;
	if(pctrl->child_type2 != NONE2)
	{
		res = _open_by_copy(&child_que, pctrl->type1, pctrl->child_type2, DB_R);
		assert(res == true);

		child.endAddr = _get_next_addr(&child_que, child_que.headAddr, -1);
	}
	else
	{
		child.endAddr = 0x00;
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
 * @todo	处理数据被意外损坏的情况
 */
bool db_append(Queue *pque, void *pdata, size_t data_len, Db_time *ptime)
{
	Db_Info info;

	assert(pque != NULL);
	assert(pdata != NULL);
	assert(data_len == _data_len(pque));
	assert(pque->flags == DB_A);
	assert(pque->headAddr >= pque->startAddr);
	assert(pque->headAddr < pque->endAddr);

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
 * @todo	处理数据被意外损坏的情况
 * @todo	若分钟数据的info结构被精简，则从其对应的日数据中获取完整的时间信息
 * @todo	若上述todo不能实现，则在分钟数据的info结构中添加界定标志以区分不同日的分钟数据
 */
bool db_read(Queue *pque, void *pdata, size_t data_len, Db_time *ptime)
{
	Db_Info info;
	uint8_t buf[data_len];
	bool stat;

	assert(pque != NULL);
//	assert(pdata != NULL);
	assert(pque->flags == DB_R);
	if(pdata)
		assert(data_len == _data_len(pque));
	assert(pque->accessAddr >= pque->startAddr);
	assert(pque->accessAddr < pque->endAddr);

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
 * @todo  处理数据被意外损坏的情况
 */
#if 0
static bool _read_time(Queue *pque, Db_time *ptime)
{
	assert(pque != NULL);
	assert(ptime != NULL);

	return db_read(pque, NULL, _data_len(pque), ptime);
}
#endif

/** 读取当前位置的info结构
 *
 * @param pque
 * @param pinfo
 * @return
 */
static bool _read_info(Queue *pque, Db_Info *pinfo)
{
	assert(pque != NULL);
	assert(pinfo != NULL);
	assert(pque->accessAddr >= pque->startAddr);
	assert(pque->accessAddr < pque->endAddr);

	return _read(pque->accessAddr, pinfo, sizeof(Db_Info));
}
/** 移动队列访问指针
 *
 * @param pque
 * @param sym
 * @return
 * @note	要定位到队列的尾，需要在队列结构中添加尾指针成员
 */
bool db_seek(Queue *pque, int ndata, int whence, int dire)
{
	int ldire = 0;
	Db_addr addr = 0x00;

	assert(pque != NULL);

	switch (whence)
	{
		case SEEK_CUR:
			addr = pque->accessAddr;
			break;

		case SEEK_SET:
			addr = _get_next_addr(pque, pque->headAddr, -1);
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

	return true;
}

/** 获取队列的子类数据的类型
 *
 * @param pque
 * @return
 */
#if 0
static type2_t _get_child_type2(Queue *pque)
{
	Ctrl *pctrl;

	assert(pque != NULL);

	pctrl = pque->pctrl;

	return pctrl->child_type2;
}
#endif

/** 判断队列是否有父类数据
 *
 * @param pctrl
 * @return
 */
static bool _have_parent(Ctrl *pctrl)
{
	int i;

	for (i = 0; i < ARRAY_LENG(ctrls); ++i)
	{
		if(ctrls[i].type1 != pctrl->type1)
			continue;

		if(ctrls[i].child_type2 == pctrl->type2)
			return true;
	}

	return false;
}

/** 获取队列的父类数据的类型
 *
 * @param pctrl
 * @return
 */
static type2_t _get_parent_type2(Ctrl *pctrl)
{
	int i;

	for (i = 0; i < ARRAY_LENG(ctrls); ++i)
	{
		if(ctrls[i].type1 != pctrl->type1)
			continue;

		if(ctrls[i].child_type2 == pctrl->type2)
			return ctrls[i].type2;
	}

	return NONE2;
}

/** 判断给定数据类型上的两时间是否相同
 *
 * @param pt1
 * @param pt2
 * @param type2
 * @return
 */
static int _time_match(type2_t type2, Db_time *pt1, Db_time *pt2)
{
	union {
		uint64_t v;
		Db_time  t;
	} t1, t2;
	int dv, res;

	assert(pt1 != NULL);
	assert(pt2 != NULL);

	t1.v = 0;
	t2.v = 0;
	switch (type2)
	{
		case MINUS:
			t1.t.min = pt1->min;
			t2.t.min = pt2->min;
			/* no break */
		case HOUR:
			t1.t.hour = pt1->hour;
			t1.t.hour = pt1->hour;
#if SIMPLIFY_MINUS_INFO
			if(type2 == MINUS)	// 分钟数据只有分、时的时间
				break;
#endif
			/* no break */
		case DAY:
			t1.t.day = pt1->day;
			t2.t.day = pt2->day;
			/* no break */
		case WEEK:
			t1.t.weeks = pt1->weeks;
			t2.t.weeks = pt2->weeks;
			/* no break */
		case MONTH:
			t1.t.month = pt1->month;
			t2.t.month = pt2->month;
			/* no break */
		case YEAR:
			t1.t.year = pt1->year;
			t2.t.year = pt2->year;
			break;

		default:
			debug("Wrong Type2: %d\n", type2);
			return -1;
			break;
	}

	dv = t1.v - t2.v;
	if(dv == 0)
		res = 0;
	else if(dv > 0)
		res = 1;
	else
		res = -1;

	return res;
}

/** 定位某类型的数据在队列中的某时间点上的位置
 *
 * @param pque
 * @param plocate_time
 * @param deep
 * @return
 * @todo	返回实现定位到数据类型
 */
bool db_locate(Queue *pque, Db_time *plocate_time, int deep)
{
	Ctrl *pctrl = NULL;
	Db_time rtime;
	Db_addr locateAddr, curAddr;
	int dire;
	bool match;
	int res;

	assert(pque != NULL);
	assert(plocate_time!= NULL);

	pctrl = pque->pctrl;
	curAddr = pque->accessAddr;
	dire = pque->dire;

	if(deep && _have_parent(pctrl))
	{
		Queue parent_que;
		Db_Info info;

		if(!_open_by_copy(&parent_que, pctrl->type1, _get_parent_type2(pctrl), DB_R))
			return false;

		if(!db_locate(&parent_que, plocate_time, deep - 1))
			return false;

		if(!_read_info(&parent_que, &info))
			return false;

		pque->accessAddr = info.child.endAddr;
		pque->dire = -1;
	}
	else
	{
		db_seek(pque, 0, SEEK_SET, -1);
	}

	match = false;
	while(1)
	{
		locateAddr = pque->accessAddr;

		if(!db_read(pque, NULL, _data_len(pque), &rtime))
			break;

		res = _time_match(pctrl->type2, &rtime, plocate_time);
		if(res == 0)
		{
			match = true;
			curAddr = locateAddr;
			break;
		}
		else if(res * pque->dire > 0)	// 若已超过给定时间，则无需再继续搜索
		{
			break;
		}
	}

	pque->dire = dire;
	pque->accessAddr = curAddr;

	return match;
}

/** 找出队列的头部
 *
 * @param pque
 * @return
 */
static Db_addr _find_queue_head(Queue *pque)
{
	size_t info_data_len;
	Db_addr  findAddr;
	uint8_t symbol;
	bool found = false;
	
	assert(pque != NULL);

	info_data_len = _info_data_len(pque);
	
	findAddr = pque->startAddr;
	symbol = 0x00;
	while((findAddr + info_data_len) < pque->endAddr)
	{
		_read(findAddr + FPOS(Db_Info, symbol), &symbol, sizeof(symbol));  //只读取数据的标识
		if(symbol == DATA_NAN)	//如果是空数据，说明找到了最新数据，即头
		{
			found = true;
			break;
		}
		
		findAddr += info_data_len;
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
	size_t info_datas_len;	// 所有 info信息 + 待存储数据 长度之和
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
		info_datas_len = _info_data_len(pque) * ctrls[i].max_num;
		que_len = ( 2 + info_datas_len / db.earseSize) * db.earseSize;	// 确保队列的长度为擦除大小的整数倍
		pque->endAddr = pque->startAddr + que_len - 1;

		debug("Address: [%#08x, %#08x]\n", pque->startAddr, pque->endAddr);

		pque->headAddr = _find_queue_head(pque);
		pque->accessAddr = _get_next_addr(pque, pque->headAddr, -1);
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
