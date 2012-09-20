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
	#include "windef.h"
	typedef enum _bool{false = FALSE, true = TRUE} bool;
#elif defined __linux__
	#include <sys/types.h>
	typedef u_int8_t	uint8_t;
	typedef u_int16_t	uint16_t;
	typedef u_int32_t	uint32_t;

#else

#endif

#ifndef __linux__
	typedef char 			int8_t;
	typedef short	 		int16_t;
	typedef int 			int32_t;
	typedef long 			int64_t;
	typedef unsigned char 	uint8_t;
	typedef unsigned short 	uint16_t;
	typedef unsigned int 	uint32_t;
	typedef unsigned long	uint64_t;
#endif


#endif /* SYSTEM_H_ */
