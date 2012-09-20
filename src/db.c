/*
 * db.c
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#include "db.h"
#include "flash.h"

#define DATA_AVAIL 0xfe
#define DATA_NAN   0xff

static const struct
{
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr earseSize;
} db = { 0x0100, 0xff00, EARSE_SIZE };

typedef struct _Test_Data
{
	uint32_t val;
} Test_data;

static Queue ques[7];

static const Ctrl ctrls[] = {
		{ TEST, SEC, sizeof(Test_data), 120, &ques[0], NULL },
		{ TEST, MIN, sizeof(Test_data), 120, &ques[1], &ques[0] },
		{ TEST, HOUR, sizeof(Test_data), 48, &ques[2], &ques[1] },
		{ TEST, DAY, sizeof(Test_data), 30, &ques[3], &ques[2] },
		{ TEST, WEEK, sizeof(Test_data),	 7,	 &ques[4],	&ques[3]},
		{ TEST, MONTH, sizeof(Test_data), 12, &ques[5], &ques[3] },
		{ TEST, YEAR, sizeof(Test_data), 2, &ques[6], &ques[5] },
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
static int db_malloc(void);

void db_init(void)
{
	_init();

	if(db_malloc() == -1)
	{
//		debug
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

Ctrl *db_open(type1_t type1, type2_t type2, int flags, ...)
{
	return NULL;
}

int db_close(Ctrl *pctrl)
{
	return 0;
}

static uint8_t _calc_checksum(Db_Info *pinfo, void *pdata, size_t len)
{
	return 0;
}

static Db_child _get_child(Ctrl *pctrl)
{
	Db_child child;

	return child;
}

static Db_addr _get_next_addr(Ctrl *pctrl, int dirt)
{
	return 0;
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
Db_err db_write(Ctrl *pctrl, void *pdata, Db_time *ptime, size_t len)
{
	Queue *pque;
	Db_Info info;

	assert(pctrl != NULL);
	assert(pdata != NULL);
	assert(len == pctrl->data_len);

	pque = pctrl->pque;

	info.flag = DATA_AVAIL;
	info.time = *ptime;
	info.child = _get_child(pctrl);
	info.checksum = _calc_checksum(&info, pdata, len);

	_write(pque->writeAddr, &info, sizeof(info));
	_write(pque->writeAddr +  sizeof(info), pdata, len);

	pque->writeAddr = _get_next_addr(pctrl, 1);

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
int db_read(Ctrl *pctrl, void *pdata, size_t len, int dirt)
{
	Queue *pque;
	Db_Info info;
	bool stat;

	assert(pctrl != NULL);
	assert(pdata != NULL);
	assert(len == pctrl->data_len);

	pque = pctrl->pque;

	stat = _read(pque->readAddr, &info, sizeof(info));
	if(info.flag != DATA_AVAIL)
		return DB_ERR;

	stat &= _read(pque->readAddr + sizeof(info), pdata, len);
	if(info.checksum != _calc_checksum(&info, pdata, len))
		return DB_ERR;

	pque->readAddr = _get_next_addr(pctrl, dirt);

	return DB_OK;
}

static int db_read_time(Ctrl *pctrl, Db_time *ptime)
{

	return 0;
}

int db_seek(Ctrl *pctrl, int sym)
{

	return 0;
}

type2_t get_child_type(Ctrl *pctrl)
{
	return 0;
}

bool db_time_match(Db_time *pt1, Db_time *pt2, type2_t type2)
{
	return FALSE;
}

Ctrl * db_locate(type1_t type1, type2_t type2, Db_time *ptime)
{
	Ctrl *pctrl;
	type2_t locate_type2, next_type2;
	Db_time rtime;
	bool match;

//	pctrl = get_ctrl_by_type(type1, YEAR);
//	pque = pctrl->pque;
	next_type2 = YEAR;
	do
	{
		locate_type2 = next_type2;
		pctrl = db_open(type1, locate_type2, O_RDONLY);
		if(pctrl == NULL)
			return NULL;

		match = FALSE;
		while(1)
		{
			if(db_read_time(pctrl, &rtime) < 0)
				break;

			if(db_time_match(ptime, &rtime, locate_type2))
			{
				match = TRUE;
				break;
			}

			if(db_seek(pctrl, -1) == EOF)
				break;
		}

		if(match && locate_type2 == type2)
		{
			return pctrl;
		}
		else if(match)
		{
			next_type2 = get_child_type(pctrl);
			db_close(pctrl);
		}
		else
		{
			db_close(pctrl);
			return NULL;
		}

	} while(locate_type2 != type2);

	return NULL;
}

static int db_malloc()
{

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
