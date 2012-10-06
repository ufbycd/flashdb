/*
 ============================================================================
 Name        : flashdb.c
 Author      : chenss
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "db.h"

extern void test_db(void);

int main(int argc, char **argv)
{
	puts("!!!Hello World!!!"); /* prints !!!Hello ARM World!!! */

	db_init();
	test_db();

	return EXIT_SUCCESS;
}
