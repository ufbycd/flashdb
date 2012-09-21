
#ifdef __AVR__

#ifndef SSIFLASH2_H_
#define SSIFLASH2_H_

// need to include the following statement in order to get this unit file compiled
#include <avr/io.h>
#include "uart.h"

/*********************************************************************
*
*       Global function prototypes
*
**********************************************************************
*/

// Initiate M25PE80 SPI Flash (0: found and initialize SPI Flash; 1: no valid SPI Flash or error)
extern uint8_t InitSPIFlash();
// Returns the size of a page for this device as shown on Page 18
extern unsigned long SSIFlashPageSizeGet();
// Returns the size of a sector for this device as shown on Page 18
extern unsigned long SSIFlashSectorSizeGet();
// Returns the total amount of storage offered by this device as shown on Page 18
extern unsigned long SSIFlashChipSizeGet();
// Determines if the serial flash is able to accept a new instruction (1: busy; 0: idle)
extern uint8_t SSIFlashIsBusy(uint8_t *status);
// erase one page with the starting address specified (bSync=1; wait; bSync=0; return immediately)
// 0: success; 1: failure
extern uint8_t SSIFlashPageErase(uint32_t ulAddress, uint8_t bSync);
// erase one sector with the starting address specified (bSync=1; wait; bSync=0; return immediately)
// 0: success; 1: failure
extern uint8_t SSIFlashSectorErase(uint32_t ulAddress, uint8_t bSync);
// erase the entire SPI Flash (bSync=1; wait; bSync=0; return immediately)
// 0: success; 1: failure
extern uint8_t SSIFlashChipErase(uint8_t bSync);
// reads a block of serial flash into a buffer and returns the no of bytes read
extern uint32_t SSIFlashRead(uint32_t ulAddress, uint32_t ulLength, uint8_t *pucDst);
// Program one page with data and return the no of bytes written 
// If touching the last byte or needing to write to next page, *nextPage becomes 1.  Otherwise, it is 0.
extern uint16_t SSIFlashProgPage(uint32_t ulAddress, uint16_t ulLength, uint8_t *pucSrc, uint8_t *nextPage);
// TI's reference API to write data on several consecutive pages but ASSUME all these pages have been erased.
extern uint32_t SSIFlashWrite(uint32_t ulAddress, uint32_t ulLength, uint8_t *pucSrc);

extern void EngMD_flash_test(void);

#endif /* SSIFLASH2_H_ */

#endif /* __AVR__ */
