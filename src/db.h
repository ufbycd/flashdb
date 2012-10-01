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

typedef struct _Db_time{
	int8_t		min;
	int8_t		hour;
	int8_t		day;
	int8_t		weeks;
	int8_t		month;
	int8_t		year;
}Db_time;

typedef uint32_t Db_addr;

typedef struct _queue {
	Db_addr startAddr;
	Db_addr endAddr;
	Db_addr headAddr;
	Db_addr accessAddr;
	size_t	 data_len;
	int		flags;
	const void	*pctrl;
//	struct {
//		unsigned LOCK:1;
//		unsigned FULL:1;
//	};
}Queue;

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


#define O_NOACCESS	00
#define O_RDONLY	01
#define O_WRONLY	02
//#define O_RDWR	03

/* The possibilities for the third argument to `fseek'.
   These values should not be changed.  */
#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

void db_init(void);
Queue *db_open(type1_t type1, type2_t type2, int flags, ...);
bool db_close(Queue *pque);

bool db_seek(Queue *pque, int sym);
Queue *db_locate(type1_t type1, type2_t type2, Db_time *ptime);

bool db_read(Queue *pque, void *pdata, Db_time *ptime, size_t data_len);
bool db_append(Queue *pque, void *pdata, Db_time *ptime, size_t data_len);
//bool db_write(Queue *pque, void *pdata, Db_time *ptime, size_t data_len);


#endif /* DB_H_ */
