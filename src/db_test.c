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
		db_close(pque);

		pque = db_open(TEST, type2, DB_RW);
		LCUT_ASSERT(tc, pque != NULL);
		db_close(pque);

		pque = db_open(TEST, type2, DB_A);
		LCUT_ASSERT(tc, pque != NULL);
		db_close(pque);
	}

	pque = db_open(TEST, NONE2, DB_R);
	LCUT_ASSERT(tc, pque == NULL);
}

void testAppend(lcut_tc_t *tc, void *data)
{
	Queue *pque;
	Db_time t = {0}, rt;
	Test_data d = {0}, rd;
	int i;

	pque = db_open(TEST, MINUS, DB_A);
	LCUT_ASSERT(tc, pque != NULL);

	LCUT_ASSERT(tc, db_seek(pque, 1, SEEK_SET, 1));

	for (i = 0; i < 100; ++i)
	{
		d.val = i;
		LCUT_ASSERT(tc, db_append(pque, &d, sizeof(d), &t));
		memset(&rd, 0xffff, sizeof(rd));
		memset(&rt, 0xffff, sizeof(rt));
		LCUT_ASSERT(tc, db_read(pque, &rd, sizeof(rd), &rt));

		LCUT_INT_EQUAL(tc, 0, memcmp(&rd, &d, sizeof(rd)));
		LCUT_INT_EQUAL(tc, 0, memcmp(&rt, &t, sizeof(rt)));
	}

	db_close(pque);
}

void test_db(void)
{
    lcut_ts_t   *suite = NULL;

    LCUT_TEST_BEGIN("database test", NULL, NULL);

    LCUT_TS_INIT(suite, "database", NULL, NULL);
    LCUT_TC_ADD(suite, testOpen, NULL, NULL, NULL);
    LCUT_TC_ADD(suite, testAppend, NULL, NULL, NULL);
    LCUT_TS_ADD(suite);

    LCUT_TEST_RUN();
    LCUT_TEST_REPORT();
    LCUT_TEST_END();
}
