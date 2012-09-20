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

int main(int argc, char **argv)
{
	puts("!!!Hello ARM World!!!"); /* prints !!!Hello ARM World!!! */

	db_init();


	return EXIT_SUCCESS;
}
