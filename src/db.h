/*
 * db.h
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#ifndef DB_H_
#define DB_H_

#include "system.h"
#include <time.h>

typedef uint32_t Db_addr;

typedef struct _Db_Info {
	uint8_t	flag;
	uint8_t	checksum;
}Db_Info;

typedef struct _Db_Child {
	Db_addr	childAddr;
}Db_child;

typedef struct _Db_Data{
	Db_Info	info;

	void 		*pdata;
	size_t		len;
}Db_Data;

typedef struct _queue {
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr writeAddr;
	Db_addr readAddr;
//	struct {
//		unsigned LOCK:1;
//		unsigned FULL:1;
//	};
} Queue;

/* 数据库控制块 */
typedef struct
{
	int			type1;
	int			type2;
	size_t	 	data_len;
	int			max_num;
	Queue	 	*pque;
	Queue		*pchildQue;
} Que_ctrl;


void db_init(void);
int db_write(int type1, int type2, void *pdata, size_t len);
int db_read(int type1, int type2, void *pdata, size_t len, int dire);

#endif /* DB_H_ */
