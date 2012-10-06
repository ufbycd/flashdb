/*
 * db_test.c
 *
 *  Created on: 2012-10-6
 *      Author: chenss
 */

#include "system.h"
#include "db.h"
#include "lcut/lcut.h"

void testOpen(lcut_tc_t *tc, void *data)
{
	Queue *pque;

	pque = db_open(TEST, MINUS, DB_R);
	LCUT_ASSERT(tc, "", pque != NULL);

	db_close(pque);
}

void test_db(void)
{
    lcut_ts_t   *suite = NULL;

    LCUT_TEST_BEGIN("database test", NULL, NULL);

    LCUT_TS_INIT(suite, "database", NULL, NULL);
    LCUT_TC_ADD(suite, "", testOpen, NULL, NULL, NULL);
    LCUT_TS_ADD(suite);

    LCUT_TEST_RUN();
    LCUT_TEST_REPORT();
    LCUT_TEST_END();
}
