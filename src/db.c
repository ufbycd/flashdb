/*
 * db.c
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#include "db.h"

static const struct
{
	Db_addr startAddr;
	Db_addr endAddr;
	int earse_size;
} db = { 0x0100, 0xff00, 0xff };

typedef struct _Test_Data
{
	uint32_t val;
} Test_data;

static Queue ques[6];

static const Que_ctrl ctrls[] = { { TEST, SEC, sizeof(Test_data), 120, &ques[0], NULL }, { TEST,
		MIN, sizeof(Test_data), 120, &ques[1], &ques[0] }, { TEST, HOUR, sizeof(Test_data), 48,
		&ques[2], &ques[1] }, { TEST, DAY, sizeof(Test_data), 30, &ques[3], &ques[2] }, { TEST,
		MONTH, sizeof(Test_data), 12, &ques[4], &ques[3] }, { TEST, YEAR, sizeof(Test_data), 2,
		&ques[5], &ques[4] }, };

static void _hw_init(void);
static int _write(Db_addr addr, void *pdata, size_t len);
static int _read(Db_addr addr, void *pdata, size_t len);
static int db_malloc();

void db_init(void)
{
	_hw_init();
	if(db_malloc() == -1)
	{
//		debug
	}

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

Que_ctrl *db_open(type1_t type1, type2_t type2, int flags, ...)
{
	return NULL ;
}

int db_write(Que_ctrl *pctrl, void *pdata, Db_time *ptime, size_t len)
{

	return 0;
}

int db_read(Que_ctrl *pctrl, type2_t type2, void *pdata, size_t len, int dire)
{

	return 0;
}

static int db_read_time(Que_ctrl *pctrl, Db_time *ptime)
{

	return 0;
}

int db_lseek(type1_t type1, type2_t type2, int sym)
{

	return 0;
}

type2_t get_child_type(Que_ctrl *pctrl)
{
	return 0;
}

Que_ctrl * db_locate(type1_t type1, type2_t type2, Db_time *ptime)
{
	Que_ctrl *pctrl;
	Queue *pque;
	type2_t locate_type2, next_type2;
	Db_time rtime;
	int res;
	bool match;

//	pctrl = get_ctrl_by_type(type1, YEAR);
//	pque = pctrl->pque;
	next_type2 = YEAR;
	do
	{
		locate_type2 = next_type2;
		pctrl = db_open(type1, locate_type2, O_RDONLY);
		if(pctrl == NULL )
			return NULL ;

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

			if(db_lseek(pctrl, -1) == EOF)
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
			return NULL ;

	} while(locate_type2 != type2);

	return NULL ;
}

static int db_malloc()
{

	return 0;
}

static void _hw_init(void)
{

}

static int _write(Db_addr addr, void *pdata, size_t len)
{

	return 0;
}

static int _read(Db_addr addr, void *pdata, size_t len)
{

	return 0;
}
