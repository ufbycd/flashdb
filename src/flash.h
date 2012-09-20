/*
 * flash.h
 *
 *  Created on: 2012-9-19
 *      Author: chenss
 */

#ifndef FLASH_H_
#define FLASH_H_

#include "system.h"

#define EARSE_SIZE	0x0100

typedef struct _Flash{
	uint32_t	totalSize;
	uint32_t	pageSize;
	uint32_t	eraseSize;
	int			writeTime;
	int			speed;
}flash_t;

extern const flash_t flash;

void flash_init(void);
bool flash_write(uint32_t addr, void * buf, uint32_t len);
bool flash_read(uint32_t addr, void * buf, uint32_t len);

#endif /* FLASH_H_ */
