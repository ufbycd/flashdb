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

typedef enum _Db_err{
	DB_OK  = 1,
	DB_ERR = 0,
}Db_err;

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


typedef struct _queue {
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr writeAddr;
	Db_addr readAddr;
	size_t	 data_len;
	int		flags;
	const void	*pctrl;
//	struct {
//		unsigned LOCK:1;
//		unsigned FULL:1;
//	};
} Queue;

typedef enum _type1{
	NONE1,
	TEST,
	ELEC,
	GEN,
	WATER,
	GAS,
}type1_t;

typedef enum _type2{
	NONE2,
	SEC,
	MIN,
	HOUR,
	DAY,
	WEEK,
	MONTH,
	YEAR,
} type2_t;

/* 数据库控制块 */
typedef const struct _Ctrl{
	type1_t		type1;
	type2_t		type2;
	type2_t		child_type2;
	size_t	 	data_len;
	int			max_num;
	Queue	 	*pque;
} Ctrl;

//typedef const struct _Ctrl Ctrl;

#define O_RDONLY	     00
#define O_WRONLY	     01
//#define O_RDWR		     02

void db_init(void);
int db_write(Queue *pque, void *pdata, Db_time *ptime, size_t len);
int db_read(Queue *pque, void *pdata, Db_time *ptime, size_t len);

#endif /* DB_H_ */
