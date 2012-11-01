/*
 * db_util.c
 *
 *  Created on: 2012-10-13
 *      Author: chenss
 */

#include "db.h"


typedef struct _que_range{
	Queue 	*pque;
	bool	range_avail;
	Db_addr beginAddr;
	Db_addr endAddr;
	Db_time early_time;
}Que_range;


extern size_t _data_len(const Queue *pran);
extern inline type2_t _get_type2(const Queue *pque);


void db_next_time(Db_time *ptime, type2_t type2, int dire)
{

}

/**
 *
 * @param pran
 * @param from
 * @param to
 * @return
 * @note 为适应local time被改小的情况，从最新数据开始向后读取数据才能读取到有效的数据
 *       并且在向后读取的过程中总是只选取时间较小的数据
 */
bool db_set_range(Que_range *pran, const Db_time *from, const Db_time *to, type2_t begin)
{
	Queue *pque;

	assert(pran != NULL);
	assert(from != NULL);
	assert(to != NULL);

	pque = pran->pque;

	if(db_locate(pque, from, begin) < 0)
		return false;

	pran->beginAddr = pque->accessAddr;

	if(db_locate(pque, to, begin) < 0)
		return false;

	pran->endAddr = pque->accessAddr;

	pran->range_avail = true;
	memset(&pran->early_time, 0xff, sizeof(pran->early_time));

	return true;
}

bool db_read_range(Que_range *pran, void *pdata, size_t data_len, Db_time *ptime)
{
	assert(pran != NULL);
	assert(pran->pque != NULL);
	assert(pran->range_avail);

	pran->pque->dire = DIRE_BACKWARD;

	while(1)
	{
		if(!db_read(pran->pque, pdata, data_len, ptime))
			return false;

		if(db_time_cmp(_get_type2(pran->pque), ptime, &pran->early_time) >= 0)
			continue;
	}

	memcpy(&pran->early_time, ptime, sizeof(pran->early_time));

	return false;
}

typedef void (*Sum_func)(const void *pdata, void *psum);

bool db_sum(Queue *pque, Db_time *ptime, type2_t quantum, Sum_func sum_func, void *psum)
{

	return false;
}

bool db_sum_range(Que_range *pran, Sum_func sum_func, void *psum)
{
	uint8_t data[_data_len(pran->pque)];

	while(db_read_range(pran, &data, sizeof(data), NULL))
	{
		sum_func(&data, psum);
	}

	return true;
}

bool db_sum_all(Queue *pque, Sum_func sum_func, void *psum)
{

	return false;
}
