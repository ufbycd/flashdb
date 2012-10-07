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

	extern const struct
	{
		Db_addr startAddr;
		Db_addr endAddr;
		Db_addr earseSize;
	} db;

	for (int i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_RA);
		LCUT_ASSERT(tc, pque != NULL);

		LCUT_ASSERT(tc, pque->startAddr >= db.startAddr && pque->startAddr < db.endAddr);
		LCUT_ASSERT(tc, pque->endAddr > db.startAddr && pque->endAddr <= db.endAddr);
		LCUT_INT_EQUAL(tc, 0, pque->startAddr % db.earseSize);
		LCUT_INT_EQUAL(tc, 0, (pque->endAddr + 1) % db.earseSize);

		LCUT_ASSERT(tc, pque->pctrl != NULL);

		headAddrs[i] = pque->headAddr;
		LCUT_ASSERT(tc, db_append(pque, &d, sizeof(d), &t));

		LCUT_ASSERT(tc, db_close(pque));
	}

	db_init();	// 重新初始化

	for (int i = 0; i < ARRAY_LENG(type2s); ++i)
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

	for (int i = 0; i < ARRAY_LENG(type2s); ++i)
	{
		type2 = type2s[i];

		pque = db_open(TEST, type2, DB_R);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_R, pque->flags);
		LCUT_ASSERT(tc, db_close(pque));

		pque = db_open(TEST, type2, DB_RW);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_RW, pque->flags);
		LCUT_ASSERT(tc, db_close(pque));

		pque = db_open(TEST, type2, DB_RA);
		LCUT_ASSERT(tc, pque != NULL);
		LCUT_INT_EQUAL(tc, DB_RA, pque->flags);
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

	for (int i = 0; i < ARRAY_LENG(type2s); ++i)
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

	pque = db_open(TEST, MINUS, DB_RA);
	LCUT_ASSERT(tc, pque != NULL);

	for (i = 0; i < 123; ++i)
	{
		headAddr = pque->headAddr;

		d.val = i;
		LCUT_ASSERT(tc, db_append(pque, &d, sizeof(d), &t));
		LCUT_ASSERT(tc, pque->headAddr != headAddr);
	}

	db_seek(pque, 0, SEEK_SET, -1);

	for (i -= 1; i > (123 - 48 * 2); --i)
	{
		accessAddr = pque->accessAddr;

		d.val = i;
		memset(&rd, 0xffff, sizeof(rd));
		memset(&rt, 0xffff, sizeof(rt));
		LCUT_ASSERT(tc, db_read(pque, &rd, sizeof(rd), &rt));

		LCUT_INT_EQUAL(tc, 0, memcmp(&rd, &d, sizeof(rd)));
		LCUT_INT_EQUAL(tc, 0, memcmp(&rt, &t, sizeof(rt)));

		LCUT_ASSERT(tc, pque->accessAddr != accessAddr);
	}

	LCUT_ASSERT(tc, db_close(pque));
}

void testTimeMatch(lcut_tc_t *tc, void *data)
{

}

void testLocate(lcut_tc_t *tc, void *data)
{

}

void testAppendToNotEmptySpace(lcut_tc_t *tc, void *data)
{

}

void testBrokenData(lcut_tc_t *tc, void *data)
{

}



void test_db(void)
{
    lcut_ts_t   *suite = NULL;

    LCUT_TEST_BEGIN("database test", NULL, NULL);

    LCUT_TS_INIT(suite, "Database - Normal Test", NULL, NULL);
    LCUT_TC_ADD(suite, testQuesInit, NULL, NULL, NULL);
    LCUT_TC_ADD(suite, testOpen, NULL, NULL, NULL);
    LCUT_TC_ADD(suite, testSeek, NULL, NULL, NULL);
    LCUT_TC_ADD(suite, testAppend, NULL, NULL, NULL);
    LCUT_TC_ADD(suite, testTimeMatch, NULL, NULL, NULL);
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
