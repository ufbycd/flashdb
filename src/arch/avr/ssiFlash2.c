/*
 *
 * ssiFlash2.c
 *
 * Created: 2012/2/29 16:50:21
 *  Author: tsewaileung
 * 
 * 1) Derived from my M-Cortex M3's "ssiflash3.c" that runs in IAR-ARMS
 * 2) Handle code specific ST Microelectronics SPI Flash M25PE80
 * 3) Add the original TI's API "unsigned long SSIFlashWrite(unsigned long ulAddress, 
 *    unsigned long ulLength, unsigned char *pucSrc)", which supports multiple-page
 *    program.   
 * 4) On 1 Mar 2012, the function "SSIFlashWrite(...)" returns zero if the data length is
 *    longer than 150.  It was realized that the time-out value is not long enough.
 *    In "SSIFlashWrite(uint32_t ulAddress, uint32_t ulLength, uint8_t *pucSrc)", the time-out
 *    value after writing data has been lengthen by 100 times for long-data write as follow.
 *    //if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE)) return(ulLength-ulLeft);        
 *    // temporarily lengthen the time by 100 times for length longer than 150 bytes (in Ah Yang EKO)
 *    if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE*100)) return(ulLength-ulLeft);
 *
 *
 *
 */ 

#ifdef __AVR__

//*****************************************************************************
//
// The rate of the SSI clock and derived values.
// To read each status, send 8-bit instruction and then send one dummy byte to get 8-bit status (=> 16 bits)
// As D3 runs at 32MHz & SPI runs at 1/4 system clock, SPI clock rate = 32/4 = 8MHz
//*****************************************************************************
#define SSI_CLK_RATE            8000000
#define SSI_CLKS_PER_MS         (SSI_CLK_RATE / 1000)
#define STATUS_READS_PER_MS     (SSI_CLKS_PER_MS / 16)


//*****************************************************************************
//
// Labels for the instructions supported by the ST Microelectronics M25PE80
// Table 6 on Page 21 in the datasheet
//*****************************************************************************
// change to ST Microelectronics M25PE80 (page 21 in Data Sheet)
#define INSTR_WREN              0x06
#define INSTR_WRDI              0x04
#define INSTR_RDID              0x9F
#define INSTR_RDSR              0x05
#define INSTR_WRLR              0xE5
#define INSTR_WRSR              0x01
#define INSTR_RDLR              0xE8
#define INSTR_READ              0x03
#define INSTR_FAST_READ         0x0B
#define INSTR_PW                0x0A
#define INSTR_PP                0x02
#define INSTR_PE                0xDB
#define INSTR_SSE               0x20
#define INSTR_SE                0xD8
#define INSTR_BE                0xC7
#define INSTR_DP                0xB9
#define INSTR_RDP               0xAB


//*****************************************************************************
//
// Status register bit definitions (Page 24)
//
//*****************************************************************************
#define STATUS_WIP              0x01
#define STATUS_WEL              0x02
#define STATUS_BP0              0x04
#define STATUS_BP1              0x08
#define STATUS_BP2              0x10
#define STATUS_SWRD             0x80


//*****************************************************************************
//
// Manufacturer and device IDs that we expect to see (Table 7 on Page 23).
//
//*****************************************************************************
#define MANUFACTURER_ID         0x20
#define MEMORY_TYPE             0x80
#define MEMORY_CAP              0x14


//*****************************************************************************
//
// Refer to M25PE80's memory organization as shown on Page 18
// Common terms: Page, Sector, Subsector.
// 
//*****************************************************************************
#define M25PE80_PAGE_SIZE       256
#define M25PE80_SECTOR_SIZE     65536
#define M25PE80_SUBSECTOR_SIZE  4096
#define M25PE80_CHIP_SIZE       1048576


//*****************************************************************************
//
// The number of times we query the device status waiting for it to be idle
// after various operations have taken place and during initialization.
// Follow "ssiflash1.c" settings
//*****************************************************************************
#define MAX_BUSY_POLL_IDLE              100
#define MAX_BUSY_POLL_ERASE_PAGE        (STATUS_READS_PER_MS * 250)
#define MAX_BUSY_POLL_ERASE_SECTOR      (STATUS_READS_PER_MS * 1000)
#define MAX_BUSY_POLL_ERASE_CHIP        (STATUS_READS_PER_MS * 10000)
#define MAX_BUSY_POLL_PROGRAM_PAGE      (STATUS_READS_PER_MS * 3)

#include "ssiFlash2.h"
#include "SPIDrv1.h"								// for low-level SPI communication


// CODE IMPLEMENTATION

//*****************************************************************************
//
// Reads the serial flash device status register as shown in Fig 10 on Page 25
//
// This function reads the serial flash status register and returns the value.
//
// \return Returns the current contents of the serial flash device status
// register.
//
// As shown on Page 24, it is possible to read status even in Program, Erase
// or Write cycles.
//*****************************************************************************
uint8_t SSIFlashStatusGet()
{
  uint8_t ucStatus;
  // Assert the chip select for the serial flash.
  Start_SPI();
  // Send the READ Status Register instruction as shown on Page 24 and read back a dummy byte.
  ucStatus=SPI_TxRx(INSTR_RDSR);
  // Write a dummy byte (i.e. 0xff) then read back the actual status.
  ucStatus=SPI_TxRx(0xFF);  
  // Deassert the chip select for the serial flash.
  End_SPI();
  // Return the status read.
  return ucStatus;
}

//*****************************************************************************
//
// Waits for the flash device to report that it is idle.
//
// \param ulMaxRetries is the maximum number of times the serial flash device
// should be polled before we give up and report an error.  If this value is
// 0, the function continues polling for 2^32 times.
//
// This function polls the serial flash device and returns when either the
// maximum number of polling attempts is reached or the device reports that it
// is no longer busy.
//
// \return Returns \e 0 if the device reports that it is idle before the
// specified number of polling attempts is exceeded or \e 1 otherwise.
//
//*****************************************************************************
uint8_t SSIFlashIdleWait(uint32_t ulMaxRetries)
{	
	uint32_t ulDelay;    
	uint8_t ucStatus;
	// Wait for the device to be ready to receive an instruction.
	ulDelay = ulMaxRetries;
	do 
	{		
		// read the status first
		ucStatus=SSIFlashStatusGet();
		// return 0 immediately if WIP=0 (ie. no Program, Write, Erase cycle)
		if ((ucStatus & STATUS_WIP)==0) return 0;
	} while (--ulDelay!=0);
	// exceeding the count & still not idle, return 1
	return 1;    
}

// Get SPI Flash IDs - Manufacturer ID, Memory Type, Memory Capacity as stated on Page 23
void GetFlashIDs(uint8_t *manuID, uint8_t *memType, uint8_t *memCap)
{	
	// clear CS to 0 for SPI communication
	Start_SPI();
	// send "read ID" instruction
	SPI_TxRx(INSTR_RDID);
	// read back three IDs by dummy send
	*manuID=SPI_TxRx(0xFF);
	*memType=SPI_TxRx(0xFF);
	*memCap=SPI_TxRx(0xFF);
	// set CS to 1 to stop SPI
	End_SPI();			
}

//*****************************************************************************
//
// Initiate M25PE80 SPI Flash by setting AVR IO for SPI communication
//
// Check SPI Flash idle or not.  If the flash does not report idle after polling 
// for 2^32 times, this is regarded as failure and returns 1.
//
// Then, read the Flash's IDs for two times (the first time data discarded as 
// they are correct for the first time).  
//
// Use the IDs read at the second time and compare with those on Page 23.
//
// \return Returns \e 0 if the IDs are correct or \e 1 otherwise.
//
//*****************************************************************************
uint8_t InitSPIFlash()
{
	uint8_t manuID, memType, memCap;
	// first, initialize AVR pins for SPI communication
	InitSPI();
	// long wait for SPI Flash to be idle; if not idle, return 1
	if (SSIFlashIdleWait(0)) return 1;
	// if Flash ready, check IDs
	// discard the IDs at the first attempt, which I found they are not correct
	GetFlashIDs(&manuID,&memType,&memCap);
	// get the IDs in the second attempt for checking
	GetFlashIDs(&manuID,&memType,&memCap);
	if ((manuID!=MANUFACTURER_ID) || (memType!=MEMORY_TYPE) || (memCap!=MEMORY_CAP)) return 1;
	// if all three IDs are correct, return 0
	return 0;
}

//*****************************************************************************
//
//! Returns the size of a page for this device as shown on Page 18
//!
//! This function returns the size of an eraseable page for the serial flash
//! device.  All addresses passed to SSIFlashPageErase() must be aligned on
//! a block boundary.
//!
//! \return Returns the number of bytes in a page.
//
//*****************************************************************************
unsigned long SSIFlashPageSizeGet()
{
	// This device support 256-bytes Pages
	return M25PE80_PAGE_SIZE;
}

//*****************************************************************************
//
//! Returns the size of a sector for this device as shown on Page 18
//!
//! This function returns the size of an eraseable sector for the serial flash
//! device.  All addresses passed to SSIFlashSectorErase() must be aligned on
//! a sector boundary.
//!
//! \return Returns the number of bytes in a sector.
//
//*****************************************************************************
unsigned long SSIFlashSectorSizeGet()
{
	// This device supports 64KB sectors.
	return M25PE80_SECTOR_SIZE;
}

//*****************************************************************************
//
//! Returns the total amount of storage offered by this device as shown on Page 18
//!
//! This function returns the size of the programmable area provided by the
//! attached SSI flash device.
//!
//! \return Returns the number of bytes in the device.
//
//*****************************************************************************
unsigned long SSIFlashChipSizeGet()
{
    // This device is 1MB in size.
    return M25PE80_CHIP_SIZE;
}

//*****************************************************************************
//
// Write an instruction to the serial flash such as 
//
// \param ucInstruction is the instruction code that is to be sent.
// \param ucData is a pointer to optional instruction data that will be sent
// following the instruction code provided in \e ucInstruction.  This parameter
// is ignored if \e usLen is 0.
// \param usLen is the length of the optional instruction data.
//
// This function writes an instruction to the serial flash along with any
// provided instruction-specific data.  On return, the flash chip select
// remains asserted allowing an immediate call to SSIFlashInstructionRead().
// To finish an instruction and deassert the chip select, a call must be made
// to "End_SPI()" after this call.
//
// NEED TO WORK WITH "SSIFlashInstructionDataWrite()" or "SSIFlashInstructionRead()"
// AND "End_SPI()" to accomplish the required function.
//
// Before initiating the instruction write, call SSIFlashIdleWait(...).
// a) If SSIFlashIdleWait(...) returns 1, quit to return 1; at that time,
//    CS is not set to high => no need to call "SSIFlashInstructionEnd()".
// b) If SSIFlashIdleWait(...) returns 0, proceed to send the Instruction and 
//    the associated data.
//
// Return 0 if success or 1 if failure
//*****************************************************************************
uint8_t SSIFlashInstructionWrite(uint8_t ucInstruction, uint8_t *ucData, uint16_t usLen)
{
	uint32_t ulLoop;
	uint32_t ulDummy;
	// make sure the flash is ready to accept instructions
	if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE)) return 1;
	// if ready, proceed to write instruction process.
	// Assert the chip select for the serial flash.
	Start_SPI();
	// Send the instruction byte and receive a dummy byte to pace the transaction.
	ulDummy=SPI_TxRx(ucInstruction);
	// Send any optional bytes related to the instruction
	for(ulLoop = 0; ulLoop < (uint32_t)usLen; ulLoop++)	
	{
		ulDummy=SPI_TxRx(ucData[ulLoop]);    
	}		
	// return 0 if success
	return 0;
}

//*****************************************************************************
//
// Write additional data following an instruction to the serial flash.
//
// \param ucData is a pointer to data that will be sent to the device.
// \param usLen is the length of the data to send.
//
// This function writes a block of data to the serial flash device.  Typically
// this will be data to be written to a flash page.
//
// It is assumed that SSIFlashInstructionWrite() has previously been called to
// set the device chip select and send the initial instruction to the device.
//
//*****************************************************************************
void SSIFlashInstructionDataWrite(uint8_t *ucData, uint16_t usLen)
{
	uint32_t ulLoop;
	uint32_t ulDummy;
	// Send the data to the device.
	for(ulLoop = 0; ulLoop < (uint32_t) usLen; ulLoop++)
	{
		ulDummy=SPI_TxRx(ucData[ulLoop]);    
	}
}

//*****************************************************************************
//
// Read data from the serial flash following the write portion of an
// instruction.
//
// \param ucData is a pointer to storage for the bytes read from the serial
// flash.
// \param ulLen is the number of bytes to read from the device.
//
// This function read a given number of bytes from the device.  It is assumed
// that the flash chip select is already asserted on entry and that an
// instruction has previously been written using a call to
// SSIFlashInstructionWrite().
//
//*****************************************************************************
void SSIFlashInstructionRead(uint8_t *ucData, uint32_t ulLen)
{
	uint8_t ulData;
	uint32_t ulLoop;
	// For each requested byte...
	for(ulLoop = 0; ulLoop < ulLen; ulLoop++)
	{
		// write dummy data so as to read the data 
		ulData=SPI_TxRx(0xFF);
		ucData[ulLoop]=ulData;
	}	
}

//*****************************************************************************
//
// Sends a command to the flash to enable program and erase operations as shown 
// in Fig 7 on Page 22.
// 
// This function sends a write enable instruction to the serial flash device
// in preparation for an erase or program operation.
//
// \return Returns \b 0 if the instruction was accepted or \b 1 if
// write operations could not be enabled.
//
// Attempt to send out INSTR_WREN.  
// If not OK, return 1. 
// If OK,  check WEL bit = 1 or not.
// If WEL=1, return 0.  Otherwise, return 1.
//*****************************************************************************
uint8_t SSIFlashWriteEnable()
{
	uint8_t ucStatus;
	// try to send INSTR_WREN.  If fail, quit to return 1.
	if (SSIFlashInstructionWrite(INSTR_WREN, (unsigned char *)0, 0)) return 1;
	// if ready sent INSTR_WREN, end the instruction
	End_SPI();
	// check whether WEL is set to 1 or not
	ucStatus=SSIFlashStatusGet();	
	// if WEL set, return 0
	if ((ucStatus & STATUS_WEL)==STATUS_WEL) return 0;
	// if WEL not set, return 1
	return 1;  
}

//*****************************************************************************
//
//! Determines if the serial flash is able to accept a new instruction
//! and copy Status Byte to *status.
//!
//! This function reads the serial flash status register and determines whether
//! or not WIP=1 (busy) or 0.  No new instruction may be issued to
//! the device if it is busy.
//!
//! \return Returns \b 1 if the device is busy and unable to receive a new
//! instruction or \b 0 if it is idle.
//
//*****************************************************************************
uint8_t SSIFlashIsBusy(uint8_t *status)
{
	uint8_t ucStatus;
	// Get the flash status.
	ucStatus = SSIFlashStatusGet();
	*status=ucStatus;
	// return 0 if idle
	if ((ucStatus & STATUS_WIP)==0) return 0;
	// return 1 if busy
	return 1;
}

//*****************************************************************************
//
//! Erases the contents of a single serial flash page as shown on Page 38.
//!
//! \param ulAddress is the start address of the page which is to be erased.
//! This value must be an integer multiple of the page size returned
//! by SSIFlashPageSizeGet().
//! \param bSync should be set to \b true if the function is to block until
//! the erase operation is complete or \b false to return immediately after
//! the operation is started.
//!
//! This function erases a single page of the serial flash, returning all
//! bytes in that page to their erased value of 0xFF.  The page size and,
//! hence, start address granularity can be determined by calling
//! SSIFlashPageSizeGet().
//!
//! The function may be synchronous (\e bSync set to \b 1) or asynchronous
//! (\e bSync set to \b 0).  If asynchronous, the caller is responsible for
//! ensuring that no further serial flash operations are requested until the
//! erase operation has completed.  The state of the device may be queried by
//! calling SSIFlashIsBusy().
//!
//! \return Returns \b 0 on success or \b 1 on failure.
//
//*****************************************************************************
uint8_t SSIFlashPageErase(uint32_t ulAddress, uint8_t bSync)
{
	uint8_t bRetcode;
	uint8_t pucBuffer[3];
	// Make sure the address passed is aligned correctly (ie 0x000000, 0x000100, 0x000200, etc).
	if(ulAddress & (M25PE80_PAGE_SIZE - 1)) return 1;
	// try to send INSTR_WREN to set WREN bit; if not OK, quit to return 1.
	if (SSIFlashWriteEnable()) return 1;
	// Now perform the instruction we need to erase the sector (MSB first: 23, 22, ... 2, 1, 0)
	pucBuffer[0] = (uint8_t)((ulAddress >> 16) & 0xFF);
	pucBuffer[1] = (uint8_t)((ulAddress >> 8) & 0xFF);
	pucBuffer[2] = (uint8_t)(ulAddress & 0xFF);
	// try to send INSTR_PE with three address bytes as shown in Fig 18 on Page 38
	if (SSIFlashInstructionWrite(INSTR_PE, pucBuffer, 3)) return 1;
	End_SPI();
	// if blocking, check WIP = 0 or not within MAX_BUSY_POLL_ERASE_PAGE times. 
	// if WIP=0, return 0.  If expire MAX_BUSY_POLL_ERASE_PAGE, return 1.
    if(bSync) bRetcode = SSIFlashIdleWait(MAX_BUSY_POLL_ERASE_PAGE);
    // if non-blocking, quit to return 0.
    else bRetcode=0;
	return bRetcode;
}

//*****************************************************************************
//
//! Erases the contents of a single serial flash sector as shown on Page 40.
//!
//! \param ulAddress is the start address of the sector which is to be erased.
//! This value must be an integer multiple of the sector size returned
//! by SSIFlashSectorSizeGet().
//! \param bSync should be set to \b true if the function is to block until
//! the erase operation is complete or \b false to return immediately after
//! the operation is started.
//!
//! This function erases a single sector of the serial flash, returning all
//! bytes in that sector to their erased value of 0xFF.  The sector size and,
//! hence, start address granularity can be determined by calling
//! SSIFlashSectorSizeGet().
//!
//! The function may be synchronous (\e bSync set to \b 1) or asynchronous
//! (\e bSync set to \b 0).  If asynchronous, the caller is responsible for
//! ensuring that no further serial flash operations are requested until the
//! erase operation has completed.  The state of the device may be queried by
//! calling SSIFlashIsBusy().
//!
//! \return Returns \b 0 on success or \b 1 on failure.
//
//*****************************************************************************
uint8_t SSIFlashSectorErase(uint32_t ulAddress, uint8_t bSync)
{
	uint8_t bRetcode;
	uint8_t pucBuffer[3];
	// Make sure the address passed is aligned correctly (ie. 0x000000, 0x010000, 0x020000, etc)
	if(ulAddress & (M25PE80_SECTOR_SIZE - 1)) return 1;
	// try to send INSTR_WREN to set WREN bit; if not OK, quit to return 1.
	if (SSIFlashWriteEnable()) return 1;
	// Now perform the instruction we need to erase the sector (MSB first: 23, 22, ... 2, 1, 0)
	pucBuffer[0] = (uint8_t)((ulAddress >> 16) & 0xFF);
	pucBuffer[1] = (uint8_t)((ulAddress >> 8) & 0xFF);
	pucBuffer[2] = (uint8_t)(ulAddress & 0xFF);
	// try to send INSTR_SE with three address bytes as shown in Fig 20 on Page 40
	if (SSIFlashInstructionWrite(INSTR_SE, pucBuffer, 3)) return 1;
	End_SPI();
	// if blocking, check WIP = 0 or not within MAX_BUSY_POLL_ERASE_SECTOR times. 
	// if WIP=0, return 0.  If expire MAX_BUSY_POLL_ERASE_SECTOR, return 1.
	if(bSync) bRetcode = SSIFlashIdleWait(MAX_BUSY_POLL_ERASE_SECTOR);
	// if non-blocking, quit to return 0.
	else bRetcode=0;
	return bRetcode;
}

//*****************************************************************************
//
//! Erases the entire serial flash device as shown on Page 41.
//!
//! \param bSync should be set to \b 1 if the function is to block until
//! the erase operation is complete or \b 0 to return immediately after
//! the operation is started.
//!
//! This function erases the entire serial flash device, returning all
//! bytes in the device to their erased value of 0xFF.
//!
//! The function may be synchronous (\e bSync set to \b 1) or asynchronous
//! (\e bSync set to \b 0).  If asynchronous, the caller is responsible for
//! ensuring that no further serial flash operations are requested until the
//! erase operation has completed.  The state of the device may be queried by
//! calling SSIFlashIsBusy().
//!
//! \return Returns \b 0 on success or \b 1 on failure.
//
//*****************************************************************************
uint8_t SSIFlashChipErase(uint8_t bSync)
{
	uint8_t bRetcode;
	// try to send INSTR_WREN to set WREN bit; if not OK, quit to return 1.
	if (SSIFlashWriteEnable()) return 1;
	// try to send INSTR_BE as shown in Fig 21 on Page 41
	if (SSIFlashInstructionWrite(INSTR_BE, (unsigned char *)0, 0)) return 1;
	End_SPI();
	// if blocking, check WIP = 0 or not within MAX_BUSY_POLL_ERASE_CHIP times. 
	// if WIP=0, return 0.  If expire MAX_BUSY_POLL_ERASE_CHIP, return 1.
	if(bSync) bRetcode = SSIFlashIdleWait(MAX_BUSY_POLL_ERASE_CHIP);
	// if non-blocking, quit to return 0.
	else bRetcode=0;
	return bRetcode;
}

//*****************************************************************************
//
//! Reads a block of serial flash into a buffer as shown on Page 28.
//!
//! \param ulAddress is the serial flash address of the first byte to be read.
//! \param ulLength is the number of bytes of data to read.
//! \param pucDst is a pointer to storage for the data read.
//!
//! This function reads a contiguous block of data from a given address in the
//! serial flash device into a buffer supplied by the caller.
//!
//! \return Returns the number of bytes read.
//
//*****************************************************************************
uint32_t SSIFlashRead(uint32_t ulAddress, uint32_t ulLength, uint8_t *pucDst)
{
	uint8_t pucBuffer[3];
	// Send the read instruction and start address.
	pucBuffer[0] = (uint8_t)((ulAddress >> 16) & 0xFF);
	pucBuffer[1] = (uint8_t)((ulAddress >> 8) & 0xFF);
	pucBuffer[2] = (uint8_t)(ulAddress & 0xFF);
	// try to send INSTR_READ with three address bytes as shown in Fig 12 on Page 28
    // if not OK, return 0.
	if (SSIFlashInstructionWrite(INSTR_READ, pucBuffer, 3)) return 0;
	// Read back the data.
	SSIFlashInstructionRead(pucDst, ulLength);
	End_SPI();
	return(ulLength);
}

//*****************************************************************************
//
//! Program a page of data to the serial flash device using INSTR_PP as shown 
//! on Page 32 (i.e. 1 to 0 but not 0 to 1).
//!
//! \param ulAddress is the first serial flash address to be written.
//! \param ulLength is the number of bytes of data to write.
//! \param pucSrc is a pointer to the data which is to be written.
//! \param exceedPage is a pointer to byte that indicates whether the current
//! page boundary is exceeded after the writing operation
//!
//! This function writes a block of data into the serial flash device at the
//! given address.  It is assumed that the area to be written has previously
//! been erased.
//!
//! \return Returns the number of bytes written.
//
// Unlike TI's SSIFlashWrite, this function only programs bytes in one page.
// If the length exceeds the existing page's boundary, the remain bytes will
// NOT be programmed.  The function returns the exact no. of bytes programmed.
// If the existing page's last byte is written, *nextPage is set to 1.  Else,
// *nextPage is set to zero.
//
//*****************************************************************************
uint16_t SSIFlashProgPage(uint32_t ulAddress, uint16_t ulLength, uint8_t *pucSrc, uint8_t *nextPage)
{ 
	uint8_t pucBuffer[3];
	uint16_t noofByteCanBeWritten,noofByteToBeWritten;
	// *nextpage is set to zero as default
	*nextPage=0;
	// not allow ulLength to be zero
	if (ulLength==0) return 0;
	// determine the no. of bytes to be programmed in the current page.
	noofByteCanBeWritten=M25PE80_PAGE_SIZE - (ulAddress & (M25PE80_PAGE_SIZE - 1));    
	// within the page, ulLength is the no. of bytes to be written & *nextPage = 0
	if (ulLength<noofByteCanBeWritten) noofByteToBeWritten=ulLength;
	// write the last byte (just write up to that byte or need to write to next page)
	// uLength is the maximum number within this page & *nextPage=1
	// users need to issue another "SSIFlashWriteToPage(...)" for remaining bytes
	else 
	{
		noofByteToBeWritten=noofByteCanBeWritten;
		*nextPage=1;
	}
	// try to send INSTR_WREN to set WREN bit; if not OK, quit to return 0.
	if (SSIFlashWriteEnable()) return 0;
	// If WREN is set, proceed to Page Program for a chunk of data into the page.
	pucBuffer[0] = (uint8_t)((ulAddress >> 16) & 0xFF);
	pucBuffer[1] = (uint8_t)((ulAddress >> 8) & 0xFF);
	pucBuffer[2] = (uint8_t)(ulAddress & 0xFF);
	// try to send INSTR_PP; if not OK quit to return 0.
	if (SSIFlashInstructionWrite(INSTR_PP, pucBuffer, 3)) return 0;
	// if INSTR_PP is sent, send the data bytes out.
	SSIFlashInstructionDataWrite(pucSrc, noofByteToBeWritten);
	End_SPI();
	return noofByteToBeWritten;
}

//*****************************************************************************
//
//! Writes a block of data to the serial flash device using INSTR_PP.
//!
//! \param ulAddress is the first serial flash address to be written.
//! \param ulLength is the number of bytes of data to write.
//! \param pucSrc is a pointer to the data which is to be written.
//!
//! This function writes a block of data into the serial flash device at the
//! given address.  It is assumed that the area to be written has previously
//! been erased.
//!
//! \return Returns the number of bytes written.
//
//*****************************************************************************
uint32_t SSIFlashWrite(uint32_t ulAddress, uint32_t ulLength, uint8_t *pucSrc)
{    
    uint32_t ulLeft, ulStart, ulPageLeft;
    uint8_t pucBuffer[3];
    uint8_t *pucStart;
    
    // Wait for the flash to be idle
    // If still busy after time-out, return 0 (no byte to be written).
    if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE)) return 0;   
    // Get set up to start writing pages.
    ulStart = ulAddress;
    ulLeft = ulLength;
    pucStart = pucSrc;    
    // Keep writing pages until we have nothing left to write.    
    while(ulLeft)
    {        
        // How many bytes do we have to write in the current page?
        ulPageLeft = M25PE80_PAGE_SIZE - (ulStart & (M25PE80_PAGE_SIZE - 1));
        // How many bytes can we write in the current page?
        ulPageLeft = (ulPageLeft >= ulLeft) ? ulLeft : ulPageLeft;
        // Enable write operations;	if WEL still not set, quit & return the no of bytes written.
        if(SSIFlashWriteEnable())
        {            
            // If we can't write enable the device, exit, telling the caller
            // how many bytes we've already written.         
            return(ulLength - ulLeft);
        }		        
        // Else, WEL is ready.  Write a chunk of data into one flash page.        
        pucBuffer[0] = (uint8_t)((ulStart >> 16) & 0xFF);
        pucBuffer[1] = (uint8_t)((ulStart >> 8) & 0xFF);
        pucBuffer[2] = (uint8_t)(ulStart & 0xFF);
		
		// try to send INSTR_PP; if not OK quit to return no of bytes written.
		if (SSIFlashInstructionWrite(INSTR_PP, pucBuffer, 3)) return(ulLength - ulLeft);
		// if INSTR_PP is sent, send the data bytes out.
		SSIFlashInstructionDataWrite(pucStart, ulPageLeft);		
		End_SPI();        
        // Wait for the page write to complete; if still busy after time-out, return no of bytes written.
		//if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE)) return(ulLength-ulLeft);        
		// temporarily lengthen the time by 100 times for length longer than 150 bytes (in Ah Yang EKO)
		if (SSIFlashIdleWait(MAX_BUSY_POLL_IDLE*100)) return(ulLength-ulLeft);
        // Update our pointers and counters for the next page.        
        ulLeft -= ulPageLeft;
        ulStart += ulPageLeft;
        pucStart += ulPageLeft;
    }
    // If we get here, all is well and we wrote all the required data.    
    return(ulLength);
}



//======================EngMode FLASH test================================

#define SECTOR0_START_ADD	0x00000000
#define SECTOR1_START_ADD	0x00010000
#define SECTOR2_START_ADD	0x00020000
#define SECTOR3_START_ADD	0x00030000
#define SECTOR4_START_ADD	0x00040000
#define SECTOR5_START_ADD	0x00050000
#define SECTOR6_START_ADD	0x00060000
#define SECTOR7_START_ADD	0x00070000
#define SECTOR8_START_ADD	0x00080000
#define SECTOR9_START_ADD	0x00090000
#define SECTOR10_START_ADD	0x000A0000
#define SECTOR11_START_ADD	0x000B0000
#define SECTOR12_START_ADD	0x000C0000
#define SECTOR13_START_ADD	0x000D0000
#define SECTOR14_START_ADD	0x000E0000
#define SECTOR15_START_ADD	0x000F0000

#define FLASH_BUF_LENGTH	/*0x00001000*/0x00000100


static void flash_sector_test(uint32_t addr, uint8_t data)
{
	
	uint16_t i;
	uint32_t sadd = addr;
	uint32_t length = 0;

	uint8_t wr_buf[FLASH_BUF_LENGTH];
	uint8_t rd_buf[FLASH_BUF_LENGTH];

	for(i=0; i<FLASH_BUF_LENGTH; i++)
	{
		wr_buf[i]= data;
		rd_buf[i] = 0;
	}

	length = SSIFlashWrite(sadd, FLASH_BUF_LENGTH, wr_buf);
	if (length == FLASH_BUF_LENGTH)
	{
		length = 0;
		serial0_tx_PGM(PSTR("write flash successful..\r\n"));
		length = SSIFlashRead(sadd, FLASH_BUF_LENGTH, rd_buf);
		if (length == FLASH_BUF_LENGTH)
		{
			for (i=0; i<FLASH_BUF_LENGTH; i++)
			{
				if (wr_buf[i] != rd_buf[i])
				{
					serial0_tx_PGM(PSTR("data cmp err..\r\n"));
					return;
				}
			}
			serial0_tx_PGM(PSTR("read flash data OK..\r\n"));
		}
		else 
		{
			serial0_tx_PGM(PSTR("read data no enouth length..\r\n"));
		}
		
	}
	else 
	{
		serial0_tx_PGM(PSTR("write flash fail..\r\n"));
	}
	return;
}


static void flash_all_sector_test(void)
{
	uint16_t i;

	SSIFlashPageErase(SECTOR15_START_ADD, 1);
	
	flash_sector_test(SECTOR15_START_ADD, 0xaa);

	SSIFlashPageErase(SECTOR15_START_ADD, 1);
	//for (i=0; i<1000; i++);
	
	flash_sector_test(SECTOR15_START_ADD, 0x55);
}


extern void EngMD_flash_test(void)
{

	flash_all_sector_test();
	serial0_tx_PGM(PSTR("flash test complete..\r\n\n"));

}

#endif /* __AVR__ */
