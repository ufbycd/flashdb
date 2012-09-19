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

typedef struct _Db_time{
	uint8_t		sec;
	uint8_t		min;
	uint8_t		hour;
	uint8_t		day;
	uint8_t		month;
	uint8_t		year;
}Db_time;

typedef struct _Db_child {
	Db_addr		startAddr;
}Db_child;

typedef struct _Db_Info {
	uint8_t		flag;
	uint8_t		checksum;
	Db_time		time;
	Db_child	child;
}Db_Info;

typedef struct _Db_Data{
	Db_Info		info;
	void 		*pdata;
	size_t		data_len;
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

typedef enum _type1{
	TEST,
	ELEC,
	GEN,
	WATER,
	GAS,
}type1_t;

typedef enum _type2{
	SEC,
	MIN,
	HOUR,
	DAY,
	WEEK,
	MONTH,
	YEAR,
} type2_t;

/* 数据库控制块 */
typedef struct
{
	type1_t		type1;
	type2_t		type2;
	size_t	 	data_len;
	int			max_num;
	Queue	 	*pque;
	Queue		*pchildQue;
} Que_ctrl;

#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02

#define EOF				-1

void db_init(void);
int db_write(int type1, int type2, void *pdata, Db_time *ptime, size_t len);
int db_read(int type1, int type2, void *pdata, size_t len, int dire);

#endif /* DB_H_ */
