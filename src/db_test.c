/*
 * db_test.c
 *
 *  Created on: 2012-10-6
 *      Author: chenss
 */

#include "system.h"
#include "db.h"
#include "lcut/lcut.h"
#include <string.h>

void testQuesInit(lcut_tc_t *tc, void *data)
{
	Queue *pque;
	type2_t type2;
	const type2_t type2s[] = {MINUS, DAY, WEEK, MONTH};
	Db_addr headAddrs[ARRAY_LENG(type2s)];
	Test_data d = {0};
	Db_time t = {0};
	int i;

	extern const struct
	{
		Db_addr startAddr;
		Db_addr endAddr;
		Db_addr earseSize;
	} db;

	for(i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_R | DB_A);
		LCUT_ASSERT(tc, pque != NULL);

		LCUT_ASSERT(
				tc,
				pque->startAddr >= db.startAddr && pque->startAddr < db.endAddr);
		LCUT_ASSERT(tc,
				pque->endAddr > db.startAddr && pque->endAddr <= db.endAddr);
		LCUT_INT_EQUAL(tc, 0, pque->startAddr % db.earseSize);
		LCUT_INT_EQUAL(tc, 0, (pque->endAddr + 1) % db.earseSize);
		LCUT_ASSERT(tc, pque->headAddr >= pque->startAddr);
		LCUT_ASSERT(tc, pque->headAddr < pque->endAddr);

		LCUT_ASSERT(tc, pque->pctrl != NULL);

		headAddrs[i] = pque->headAddr;
		LCUT_ASSERT(tc, db_append(pque, &d, sizeof(d), &t));

		LCUT_ASSERT(tc, db_close(pque));
	}

	db_init();	// 重新初始化

	for(i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_R);
		LCUT_ASSERT(tc, pque != NULL);

		LCUT_ASSERT(tc, pque->headAddr != headAddrs[i]);
		db_seek(pque, -1, SEEK_SET, 0);
		LCUT_ASSERT(tc, pque->accessAddr == headAddrs[i]);

		LCUT_ASSERT(tc, db_close(pque));
	}
}

void testOpen(lcut_tc_t *tc, void *data)
{
	Queue *pque;
	type2_t type2;
	const type2_t type2s[] = {MINUS, DAY, WEEK, MONTH};
	int i;

	for(i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_R);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_R, pque->flags);
		LCUT_ASSERT(tc, db_close(pque));

		pque = db_open(TEST, type2, DB_R | DB_W);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_R | DB_W, pque->flags);
		LCUT_ASSERT(tc, db_close(pque));

		pque = db_open(TEST, type2, DB_R | DB_A);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_R | DB_A, pque->flags);
		LCUT_ASSERT(tc, db_close(pque));
	}

	pque = db_open(NONE1, MINUS, DB_R);
	LCUT_ASSERT(tc, pque == NULL);
}

void testSeek(lcut_tc_t *tc, void *data)
{
	Queue *pque;
	type2_t type2;
	const type2_t type2s[] = {MINUS, DAY, WEEK, MONTH};
	Db_addr addr;
	int i;

	for(i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_R);
		LCUT_ASSERT(tc, pque != NULL);

		pque->accessAddr = 0x00;
		db_seek(pque, 0, SEEK_SET, 0);

		addr = pque->accessAddr;
		LCUT_ASSERT(tc, addr == pque->headAddr);
		LCUT_ASSERT(tc, addr >= pque->startAddr && addr < pque->endAddr);

		db_seek(pque, 123, SEEK_CUR, 0);
		LCUT_ASSERT(tc, addr >= pque->startAddr && addr < pque->endAddr);

		db_seek(pque, -123, SEEK_CUR, 0);
		LCUT_ASSERT(tc, pque->accessAddr == addr);

		db_seek(pque, 0, SEEK_CUR, 0);
		LCUT_ASSERT(tc, pque->accessAddr == addr);

		LCUT_ASSERT(tc, db_close(pque));
	}

}

void testAppend(lcut_tc_t *tc, void *data)
{
	Queue *pque;
	Db_time t = {0}, rt;
	Test_data d = {0}, rd;
	Db_addr headAddr, accessAddr;
	int i;

	pque = db_open(TEST, MINUS, DB_R | DB_A);
	LCUT_ASSERT(tc, pque != NULL);

	for(i = 0; i < 123; ++i)
	{
		headAddr = pque->headAddr;

		d.val = i;
		LCUT_ASSERT(tc, db_append(pque, &d, sizeof(d), &t));
		LCUT_ASSERT(tc, pque->headAddr != headAddr);
	}

	db_seek(pque, 0, SEEK_SET, -1);

	for(i -= 1; i > (123 - 48 * 2); --i)
	{
		accessAddr = pque->accessAddr;

		d.val = i;
		memset(&rd, 0xffff, sizeof(rd));
		memset(&rt, 0xffff, sizeof(rt));
		LCUT_ASSERT(tc, db_read(pque, &rd, sizeof(rd), &rt));
		LCUT_ASSERT(tc, pque->accessAddr != accessAddr);

		LCUT_INT_EQUAL(tc, 0, memcmp(&rd, &d, sizeof(rd)));
		LCUT_INT_EQUAL(tc, 0, memcmp(&rt, &t, sizeof(rt)));
	}

	LCUT_ASSERT(tc, db_close(pque));
}

void testTimeCmp(lcut_tc_t *tc, void *data)
{
	union
	{
		Db_time t;
		int8_t b[sizeof(Db_time)];
	} t1, t2;
	type2_t type2;

	memset(&t1, 1, sizeof(t1));
	memset(&t2, 1, sizeof(t2));
	for(type2 = MINUS; type2 <= YEAR; type2++)
	{
		memset(&t1, 1, sizeof(t1));
		LCUT_INT_EQUAL(tc, 0, db_time_cmp(type2, &t1.t, &t2.t));

		t1.b[type2 - 1] = 2;
		LCUT_INT_EQUAL(tc, 1, db_time_cmp(type2, &t1.t, &t2.t));
		LCUT_INT_EQUAL(tc, -1, db_time_cmp(type2, &t2.t, &t1.t));
		if(type2 < YEAR)
		{
			LCUT_INT_EQUAL(tc, 0, db_time_cmp(type2 + 1, &t1.t, &t2.t));
		}
	}
}

void testLocate(lcut_tc_t *tc, void *data)
{
	Queue *pminus_que, *pday_que;
	Test_data d = {0};
	Db_time t = {30, 23, 31, 44, 10, 12}, tt = {00, 00, 01, 44, 11, 12};

	pminus_que = db_open(TEST, MINUS, DB_R | DB_A);
	LCUT_ASSERT(tc, pminus_que != NULL);
	pday_que = db_open(TEST, DAY, DB_R | DB_A);
	LCUT_ASSERT(tc, pday_que != NULL);

	LCUT_ASSERT(tc, db_erase(pminus_que));
	LCUT_ASSERT(tc, db_erase(pday_que));

	LCUT_INT_EQUAL(tc, NONE2, db_locate(pminus_que, &t, MINUS));
	LCUT_INT_EQUAL(tc, NONE2, db_locate(pday_que, &t, DAY));

	LCUT_ASSERT(tc, db_append(pminus_que, &d, sizeof(d), &t));
	LCUT_ASSERT(tc, db_append(pday_que, &d, sizeof(d), &t));
	LCUT_ASSERT(tc, db_append(pminus_que, &d, sizeof(d), &tt));

	LCUT_INT_EQUAL(tc, MINUS, db_locate(pminus_que, &t, MINUS));
	LCUT_INT_EQUAL(tc, DAY, db_locate(pday_que, &t, DAY));
	LCUT_INT_EQUAL(tc, MINUS, db_locate(pminus_que, &t, DAY));

	db_close(pminus_que);
	db_close(pday_que);
}

void testAppendToNotEmptySpace(lcut_tc_t *tc, void *data)
{

}

void testBrokenData(lcut_tc_t *tc, void *data)
{

}

void test_db(void)
{
	lcut_ts_t *suite = NULL;

	LCUT_TEST_BEGIN("database test", NULL, NULL);

	LCUT_TS_INIT(suite, "Database - Normal Test", NULL, NULL);
	LCUT_TC_ADD(suite, testQuesInit, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testOpen, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testSeek, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testAppend, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testTimeCmp, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testLocate, NULL, NULL, NULL);
	LCUT_TS_ADD(suite);

	LCUT_TS_INIT(suite, "Database - Limit Test", NULL, NULL);
	LCUT_TC_ADD(suite, testAppendToNotEmptySpace, NULL, NULL, NULL);
	LCUT_TC_ADD(suite, testBrokenData, NULL, NULL, NULL);
	LCUT_TS_ADD(suite);

	LCUT_TEST_RUN();
	LCUT_TEST_REPORT();
	LCUT_TEST_END();
}
