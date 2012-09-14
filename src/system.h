/*
 * system.h
 *
 *  Created on: 2012-9-8
 *      Author: chenss
 */

#ifndef SYSTEM_H_
#define SYSTEM_H_

#ifdef _WIN32
	#include "windef.h"
#elif defined __linux__
	#include <sys/types.h>
	#define uint8_t 	u_int8_t
	#define uint16_t  	u_int16_t
	#define uint32_t 	u_int32_t
#else

#endif

#ifndef __linux__
	typedef char 			int8_t;
	typedef short	 		int16_t;
	typedef int 			int32_t;
	typedef long 			int64_t;
	typedef unsigned char 	uint8_t;
	typedef unsigned short uint16_t;
	typedef unsigned int 	uint32_t;
	typedef unsigned long	uint64_t;
#endif


#endif /* SYSTEM_H_ */
