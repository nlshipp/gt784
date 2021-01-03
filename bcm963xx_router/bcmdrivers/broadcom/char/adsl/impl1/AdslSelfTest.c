/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/****************************************************************************
 *
 * AdslSelfTest.c -- ADSL self test module
 *
 * Description:
 *	ADSL self test module
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslSelfTest.c,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslSelfTest.c,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/


#include "AdslMibDef.h"
#include "AdslCore.h"
#include "AdslCoreMap.h"
#include "AdslSelfTest.h"

#ifdef ADSL_SELF_TEST

#include "softdsl/SoftDsl.h"
#include "softdsl/BlockUtil.h"
#include "softdsl/AdslXfaceData.h"

#ifdef ADSL_PHY_FILE
#include "AdslFile.h"
#else

#if defined(CONFIG_BCM963x8)
#include "adslcoreTest6348/adsl_selftest_lmem.h"
#include "adslcoreTest6348/adsl_selftest_sdram.h"
#endif

#endif /* ADSL_PHY_FILE */

/* from AdslCore.c */

extern AdslXfaceData	*pAdslXface;

/* Locals */

int							adslStMode = 0;
int							adslStRes  = 0;

#define RESET_ADSL_CORE(pAdslReg) do {		\
	DISABLE_ADSL_CORE(pAdslReg);			\
	ENABLE_ADSL_CORE(pAdslReg);				\
} while (0);

#define ENABLE_ADSL_CORE(pAdslReg) do {		\
	pAdslReg[ADSL_CORE_RESET] = 0;			\
    HOST_MIPS_STALL(20);					\
} while (0)

#define DISABLE_ADSL_CORE(pAdslReg) do {	\
	pAdslReg[ADSL_CORE_RESET] = 1;			\
    HOST_MIPS_STALL(20);					\
} while (0)

#define ENABLE_ADSL_MIPS(pAdslReg) do {		\
    pAdslReg[ADSL_MIPS_RESET] = 0x2;		\
} while (0)

#define HOST_MIPS_STALL(cnt)				\
    do { int _stall_count = (cnt); while (_stall_count--) ; } while (0)

#define	MEM_PATTERN0	0x55555555
#define	MEM_PATTERN1	0xAAAAAAAA

AC_BOOL AdslLmemTestPattern(ulong pattern1, ulong pattern2)
{
	volatile	ulong	*pLmem	= (void *) HOST_LMEM_BASE;
#if defined(CONFIG_BCM96348)
	volatile	ulong	*pLmemEnd= (void *) (HOST_LMEM_BASE + 0x22000);
#else
	volatile	ulong	*pLmemEnd= (void *) (HOST_LMEM_BASE + 0x18000);
#endif
	volatile	static  ulong	val;
	register	ulong	i, newVal;
	register	AC_BOOL	bTestRes = AC_TRUE;

	AdslDrvPrintf(TEXT("ST: LMEM test started, pattern1 = 0x%lX pattern2 = 0x%lX\n"), pattern1, pattern2);
	do {
		*(pLmem + 0) = pattern1;
		*(pLmem + 1) = pattern2;
#if 0
		if (0 == ((long) pLmem & 0x7FFF))
			*pLmem = 1;
#endif
		pLmem += 2;
	} while (pLmem != pLmemEnd);

	pLmem	= (void *) HOST_LMEM_BASE;

	do {
		for (i = 0; i < 10; i++)
			val = 0;
		newVal = *(pLmem+0);
#if 0
		__asm__ __volatile__ ("nop\n");
		newVal = *(pLmem+0);
#endif
		if (pattern1 != newVal) {
			AdslDrvPrintf(TEXT("ST: LMEM error at address 0x%lX, pattern=0x%08lX, memVal=0x%08lX(0x%08lX), diff=0x%08lX\n"), 
				(long) pLmem, pattern1, newVal, *(pLmem+0), newVal ^ pattern1);
			bTestRes = AC_FALSE;
		}

		for (i = 0; i < 10; i++)
			val = 0;
		newVal = *(pLmem+1);
#if 0
		__asm__ __volatile__ ("nop\n");
		newVal = *(pLmem+1);
#endif
		if (pattern2 != newVal) {
			AdslDrvPrintf(TEXT("ST: LMEM error at address 0x%lX, pattern=0x%08lX, memVal=0x%08lX(0x%08lX), diff=0x%08lX\n"), 
				(long) (pLmem+1), pattern2, newVal, *(pLmem+1), newVal ^ pattern2);
			bTestRes = AC_FALSE;
		}

		pLmem += 2;
	} while (pLmem != pLmemEnd);
	AdslDrvPrintf(TEXT("ST: LMEM test finished\n"));
	return bTestRes;
}

AC_BOOL AdslLmemTest(void)
{
	AC_BOOL		bTestRes;

	bTestRes  = AdslLmemTestPattern(MEM_PATTERN0, MEM_PATTERN1);
	bTestRes &= AdslLmemTestPattern(MEM_PATTERN1, MEM_PATTERN0);
	return bTestRes;
}

int	AdslSelfTestRun(int stMode)
{
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	volatile ulong	*pAdslLMem = (ulong *) HOST_LMEM_BASE;
	volatile AdslXfaceData	*pAdslX = (AdslXfaceData *) ADSL_LMEM_XFACE_DATA;
	ulong			*pAdslSdramImage;
	ulong	 i, tmp;

	adslStMode = stMode;
	if (0 == stMode) {
		adslStRes = kAdslSelfTestCompleted;
		return adslStRes;
	}

	adslStRes = kAdslSelfTestInProgress;

	/* take ADSL core out of reset */
	RESET_ADSL_CORE(pAdslEnum);

	pAdslEnum[ADSL_INTMASK_I] = 0;
	pAdslEnum[ADSL_INTMASK_F] = 0;

	if (!AdslLmemTest()) {
		adslStRes = kAdslSelfTestCompleted;
		return adslStRes;
	}

#if 0 && defined(CONFIG_BCM96348)
	adslStRes = kAdslSelfTestCompleted | stMode;
	AdslDrvPrintf(TEXT("ST: Completed. res = 0x%lX\n"), adslStRes);
	RESET_ADSL_CORE(pAdslEnum);
	return adslStRes;
#endif		

	/* Copying ADSL core program to LMEM and SDRAM */
#ifdef ADSL_PHY_FILE
	if (!AdslFileLoadImage("/etc/adsl/adsl_test_phy.bin", pAdslLMem, NULL)) {
		adslStRes = kAdslSelfTestCompleted;
		return adslStRes;
	}
	pAdslSdramImage = AdslCoreGetSdramImageStart();
#else
#ifndef  ADSLDRV_LITTLE_ENDIAN
	BlockByteMove ((sizeof(adsl_selftest_lmem)+0xF) & ~0xF, (void *)adsl_selftest_lmem, (uchar *) pAdslLMem);
#else
	for (tmp = 0; tmp < ((sizeof(adsl_selftest_lmem)+3) >> 2); tmp++)
		pAdslLMem[tmp] = ADSL_ENDIAN_CONV_LONG(((ulong *)adsl_selftest_lmem)[tmp]);
#endif
	pAdslSdramImage = AdslCoreSetSdramImageAddr(((ulong *) adsl_selftest_lmem)[2], sizeof(adsl_selftest_sdram));
	BlockByteMove (AdslCoreGetSdramImageSize(), (void *)adsl_selftest_sdram, (void*)pAdslSdramImage);
#endif

	BlockByteClear (sizeof(AdslXfaceData), (void *)pAdslX);
	pAdslX->sdramBaseAddr = (void *) pAdslSdramImage;

	pAdslEnum[ADSL_HOSTMESSAGE] = stMode;

	ENABLE_ADSL_MIPS(pAdslEnum);

	/* wait for ADSL MIPS to start self-test */
	 
	for (i = 0; i < 10000; i++) {
		tmp = pAdslEnum[ADSL_HOSTMESSAGE];
		if (tmp & (kAdslSelfTestInProgress | kAdslSelfTestCompleted)) {
			break;
		}
		HOST_MIPS_STALL(40);
	}
	AdslDrvPrintf(TEXT("ST: Wait to Start. tmp = 0x%lX\n"), tmp);

	do {
		if (tmp & kAdslSelfTestCompleted) {
			adslStRes = tmp & (~kAdslSelfTestInProgress);
			break;
		}
		if (0 == (tmp & kAdslSelfTestInProgress)) {
			adslStRes = tmp;
			break;
		}

		/* wait for ADSL MIPS to finish self-test */

		for (i = 0; i < 10000000; i++) {
			tmp = pAdslEnum[ADSL_HOSTMESSAGE];
			if (0 == (tmp & kAdslSelfTestInProgress))
				break;
			HOST_MIPS_STALL(40);
		}
		adslStRes = tmp;
	} while (0);
	AdslDrvPrintf(TEXT("ST: Completed. tmp = 0x%lX, res = 0x%lX\n"), tmp, adslStRes);

	RESET_ADSL_CORE(pAdslEnum);
	return adslStRes;
}

int AdslSelfTestGetResults(void)
{
	return adslStRes;
}


#endif /* ADSL_SELF_TEST */
