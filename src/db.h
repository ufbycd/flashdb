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


typedef struct _Test_Data
{
	uint32_t val;
} Test_data;


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
	int		flags;
	int 	dire;
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
	MINUS,
	HOUR,
	DAY,
	WEEK,
	MONTH,
	YEAR,
} type2_t;


#define DB_N 	0	/** 无访问 */
#define DB_R	1	/** 只读 */
#define DB_RW	2	/** 读写 */
#define DB_RA	3	/** 追加 */

#define DIRE_CUR	0 /* 访问后指针不变 */
#define DIRE_BACK	-1 /* 访问后指针往后移动 */
#define DIRE_FOR	1 /* 访问后指针往前移动 */

/* The possibilities for the third argument to `fseek'.
   These values should not be changed.  */
#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
//#define SEEK_END	2	/* Seek from end of file.  */

#ifndef EOF
# define EOF (-1)
#endif

void db_init(void);
Queue *db_open(type1_t type1, type2_t type2, int flags, ...);
bool db_close(Queue *pque);

bool db_seek(Queue *pque, int ndata, int whence, int dire);
bool db_locate(Queue *pque, Db_time *plocate_time, int deep);

bool db_read(Queue *pque, void *pdata, size_t data_len, Db_time *ptime);
bool db_append(Queue *pque, void *pdata, size_t data_len, Db_time *ptime);
//bool db_write(Queue *pque, void *pdata, size_t data_len, Db_time *ptime);

int db_time_match(type2_t type2, Db_time *pt1, Db_time *pt2);


#endif /* DB_H_ */
