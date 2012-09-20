/************************************************************************
 *     COPYRIGHT NOTICE
 *     Copyright (c) 2011, ������̩��ʢʵҵ���޹�˾	����Ȩ������
 *     All rights reserved.
 *
 * @file	I2CEEProm.c
 * @brief	
 *
 * 
 *
 * @version		
 * @author     	chenss
 * @date       	2011-5-13
 *
************************************************************************/

#include "system.h"
#include "flash.h"
#include <stdio.h>
#include <string.h>

const flash_t flash = {
		.totalSize 	= 256 * 4,
		.pageSize  	= 256,
		.eraseSize 	= EARSE_SIZE,
		.writeTime 	= 5,
		.speed		= 400,
};

static const char *x86flash = "flash.bin";

#define DEBUG 1
#if DEBUG
	#define debug(fmt, args...) printf(fmt, ##args)
#else
	#define debug(fmt, args...)
#endif


/**
 *
 *
 *
 * @param
 * @return
 * @note
 */
void flash_init(void)
{
	FILE *fp;
	uint8_t mbuf[flash.pageSize];
	int pageCount = flash.totalSize / flash.pageSize;

	memset(mbuf, 0xffffffff, sizeof(mbuf));

	fp = fopen(x86flash, "r+b");
	if(fp == NULL)
	{
		debug("file not exist, creating new file: %s.\n", x86flash);

		fp = fopen(x86flash, "w+b");
		if(fp == NULL)
		{
			debug("Can not create file: %s!\n", x86flash);
			return;
		}

		if(fwrite(mbuf, pageCount, sizeof(mbuf), fp) != sizeof(mbuf) * pageCount)
		{
			debug("Can not init file!\n");
			fclose(fp);
			return;
		}
	}

	fclose(fp);

	debug("Flash information:\n");
	debug("totalSize  = %u bytes\n", flash.totalSize);
	debug("pageSize   = %u bytes\n", flash.pageSize);
	debug("eraseSize  = %u bytes\n", flash.eraseSize);
	debug("writeTime  = %u Ms\n", flash.writeTime);
	debug("speed      = %u KHz\n", flash.speed);


//	EEPromTest();
}
/**
 *
 *
 *
 * @param
 * @return
 * @note
 */
#if 0
bool EEPromTest(void)
{
	bool stat;
	uint16_t rval, wval, bval;

	stat = EEPromRead(0, &rval, sizeof(rval));
	bval = rval;
	wval = ~rval;

	stat &= EEPromWrite(0, &wval, sizeof(rval));
	stat &= EEPromRead(0, &rval, sizeof(rval));
	stat &= EEPromWrite(0, &bval, sizeof(rval));

	if(stat && (wval == rval))
	{
		debug("EEProm is OK.\n");
		return TRUE;
	}
	else
	{
		debug("EEProm is Fail.\n");
		return FALSE;
	}
}
#endif
/*****************************************************************
 * @berif	дһϵ�����
 *
 *
 *
 * @param	addr �ֽڵ�ַ
 * @param	buf		 ��ݻ���
 * @param	len		 ��д����ݵĳ���
 * @return	����ȷ����TRUE�����򷵻�FALSE
 ******************************************************************/
bool flash_write(uint32_t addr, void * const buf, uint32_t len)
{
	FILE *wf;

	wf = fopen(x86flash, "r+b");
	if(wf == NULL)
	{
		debug("Can not open file: %s!\n", x86flash);
		return FALSE;
	}

	if(fseek(wf, addr, SEEK_SET) != 0)
	{
		debug("Can not seek file: %s!\n", x86flash);
		fclose(wf);
		return FALSE;
	}

	if(fwrite(buf, 1, len, wf) != len)
	{
		debug("file write error!\n");
		fclose(wf);
		return FALSE;
	}

	fclose(wf);
	return TRUE;
}
/*****************************************************************
 * @berif	��ȡһϵ����ݵ�
 *
 *
 *
 * @param	addr �ֽڵ�ַ
 * @param	buf		 ��ݻ���
 * @param	len		 ���ȡ��ݵĳ���
 * @return	����ȷ����TRUE�����򷵻�FALSE
 ******************************************************************/
bool flash_read(uint32_t addr, void *buf, uint32_t len)
{
	FILE *rf;

	rf = fopen(x86flash, "rb");
	if(rf == NULL)
	{
		debug("Can not open file: %s!\n", x86flash);
		return FALSE;
	}

	if(fseek(rf, addr, SEEK_SET) != 0)
	{
		debug("Can not seek file: %s!\n", x86flash);
		fclose(rf);
		return FALSE;
	}

	if(fread(buf, 1, len, rf) != len)
	{
		debug("file read error!\n");
		fclose(rf);
		return FALSE;
	}

	fclose(rf);
	return TRUE;
}

