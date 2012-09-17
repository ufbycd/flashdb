/*
 * db.c
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#include "db.h"

static const struct {
	Db_addr startAddr;
	Db_addr endAddr;
	int			block;
}db = {0x0100, 0xff00, 0xff};

typedef struct _Test_Data
{
	uint32_t val;
} Test_data;

static Queue ques[2];

static const Que_ctrl ctrls[] = {
		{0,		0, 	sizeof(Test_data), 10, 	&ques[0], 	NULL },
		{0,		1,	sizeof(Test_data), 10		,&ques[1], 	&ques[0]}
};

static void _hw_init(void);
static int _write(Db_addr addr, void *pdata, size_t len);
static int _read(Db_addr addr, void *pdata, size_t len);

void db_init(void)
{
	_hw_init();

}

int db_write(int type1, int type2, void *pdata, size_t len)
{

	return 0;
}

int db_read(int type1, int type2, void *pdata, size_t len, int dire)
{

	return 0;
}

int db_leek(int type1, int type2, int sym)
{

	return 0;
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
