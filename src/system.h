/*
 * system.h
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#include <stdio.h>
//#define NDEBUG
#include <assert.h>

#ifdef __WINNT__
#	include "windef.h"
	typedef enum _bool{false = FALSE, true = TRUE} bool;
#elif defined __linux__
	#include <sys/types.h>
	typedef u_int8_t	uint8_t;
	typedef u_int16_t	uint16_t;
	typedef u_int32_t	uint32_t;
	typedef u_int64_t	uint64_t;
#	define FALSE 0
#	define TRUE  1
	typedef enum _bool{false = FALSE, true = TRUE} bool;
#else

#endif

#ifndef __linux__
	typedef __INT8_TYPE__	int8_t;
	typedef __INT16_TYPE__	int16_t;
	typedef __INT32_TYPE__ 	int32_t;
	typedef __INT64_TYPE__ 	int64_t;
	typedef __UINT8_TYPE__ 	uint8_t;
	typedef __UINT16_TYPE__ uint16_t;
	typedef __UINT32_TYPE__ uint32_t;
	typedef __UINT64_TYPE__	uint64_t;
#endif

/* 数组的长度 */
#define ARRAY_LENG(a) (sizeof(a) / sizeof(a[0]))
/* 得到一个field在结构体(struct)中的偏移量 */
#define FPOS(type, field) ( (size_t) &(( type *)0)-> field )
/* 得到一个结构体中field所占用的字节数 */
#define FSIZ(type, field) sizeof( ((type *)0)->field )

#endif /* SYSTEM_H_ */
