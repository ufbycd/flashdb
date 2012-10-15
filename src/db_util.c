/*
 * db_util.c
 *
 *  Created on: 2012-10-13
 *      Author: chenss
 */

#include "db.h"

void db_next_time(Db_time *ptime, type2_t type2, int dire)
{

}

typedef struct _range{
	Db_addr beginAddr;
	Db_addr endAddr;
}Range;

bool db_set_range(Queue *pque, Db_time *from, Db_time *to, int dire, Range *prange)
{

	return false;
}

bool db_range_read(Queue *pque, void *pdata, size_t data_len, Db_time *ptime, Range *prange)
{

	return false;
}

typedef void (*Sum_method)(void *pdata, void *psum);

bool db_sum(Queue *pque, Db_time *ptime, type2_t type2, Sum_method sum_method, void *psum)
{

	return false;
}
