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
 * AdslCore.h -- Internal definitions for ADSL core driver
 *
 * Description:
 *	Internal definitions for ADSL core driver
 *
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.4 $
 *
 * $Id: AdslCore.h,v 1.4 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslCore.h,v $
 * Revision 1.4  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.4  2004/05/06 03:24:02  ilyas
 * Added power management commands
 *
 * Revision 1.3  2004/04/30 17:58:01  ilyas
 * Added framework for GDB communication with ADSL PHY
 *
 * Revision 1.2  2004/04/13 01:07:19  ilyas
 * Merged the latest ADSL driver changes for RTEMS
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/
#undef	PHY_BLOCK_TEST
//#define	PHY_BLOCK_TEST
//#define	PHY_BLOCK_TEST_SAR
#undef	ADSLDRV_STATUS_PROFILING
//#define	ADSLDRV_STATUS_PROFILING

#define	LD_MODE_VDSL	0
#define	LD_MODE_ADSL	1

typedef unsigned char		AC_BOOL;
#define	AC_TRUE				1
#define	AC_FALSE			0

/* Adsl Core events */

#define	ACEV_LINK_UP							1
#define	ACEV_LINK_DOWN							2
#define	ACEV_G997_FRAME_RCV						4
#define	ACEV_G997_FRAME_SENT					8
#define	ACEV_SWITCH_RJ11_PAIR					0x10
#define ACEV_G994_NONSTDINFO_RECEIVED			0x20
#define ACEV_LINK_POWER_L3						0x40
#define ACEV_RATE_CHANGE					0x80


typedef struct _AdslCoreConnectionRates {
    int		fastUpRate;
    int		fastDnRate;
    int		intlUpRate;
    int		intlDnRate;
} AdslCoreConnectionRates;

void AdslCoreSetL2Timeout(unsigned long val);
void AdslCoreIndicateLinkPowerStateL2(unsigned char lineId);
AC_BOOL AdslCoreSetSDRAMBaseAddr(void *pAddr);
void	*AdslCoreGetSDRAMBaseAddr(void);
AC_BOOL AdslCoreSetVcEntry (int gfc, int port, int vpi, int vci, int pti_clp);
void	AdslCoreSetStatusCallback(void *pCallback);
void	*AdslCoreGetStatusCallback(void);
void	AdslCoreSetShowtimeMarginMonitoring(unsigned char lineId, unsigned char monEnabled, long showtimeMargin, long lomTimeSec);
AC_BOOL AdslCoreSetStatusBuffer(void *bufPtr, int bufSize);
AC_BOOL XdslCoreIs2lpActive(unsigned char lineId, unsigned char direction);
AC_BOOL XdslCoreIsVdsl2Mod(unsigned char lineId);
AC_BOOL XdslCoreIsXdsl2Mod(unsigned char lineId);
AC_BOOL AdslCoreLinkState (unsigned char lineId);
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
AC_BOOL AdslCoreGetErrorSampleStatus (unsigned char lineId);
#endif
int		AdslCoreLinkStateEx (unsigned char lineId);
void	AdslCoreGetConnectionRates (unsigned char lineId, AdslCoreConnectionRates *pAdslRates);

void	AdslCoreSetNumTones(unsigned char lineId, int numTones);
int		AdslCoreSetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen);
int		AdslCoreGetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen);
AC_BOOL AdslCoreBertStart(unsigned char lineId, unsigned long totalBits);
AC_BOOL AdslCoreBertStop(unsigned char lineId);
void	AdslCoreBertStartEx(unsigned char lineId, unsigned long bertSec);
void	AdslCoreBertStopEx(unsigned char lineId);
void	AdslCoreResetStatCounters(unsigned char lineId);

int		AdslCoreGetOemParameter (int paramId, void *buf, int len);
int		AdslCoreSetOemParameter (int paramId, void *buf, int len);
char	*AdslCoreGetVersionString(void);
char	*AdslCoreGetBuildInfoString(void);
int		AdslCoreGetSelfTestMode(void);
void	AdslCoreSetSelfTestMode(int stMode);
int		AdslCoreGetSelfTestResults(void);
void	*AdslCoreSetSdramImageAddr(unsigned long lmem2, unsigned long sdramSize);
int		AdslCoreFlattenCommand(void *cmdPtr, void *dstPtr, unsigned long nAvail);
void	AdslCoreSetL3(unsigned char lineId);
void	AdslCoreSetL0(unsigned char lineId);
void	AdslCoreSetXfaceOffset(unsigned long lmemAddr, unsigned long lmemSize);

void	*AdslCoreGetPhyInfo(void);
void	*AdslCoreGetSdramPageStart(void);
void	*AdslCoreGetSdramImageStart(void);
unsigned long AdslCoreGetSdramImageSize(void);
void	AdslCoreSharedMemInit(void);
void	*AdslCoreSharedMemAlloc(long size);
void	AdslCoreSharedMemFree(void *p);
void	*AdslCoreGdbAlloc(long size);
void	AdslCoreGdbFree(void *p);

AC_BOOL AdslCoreInit(void);
void	AdslCoreUninit(void);
AC_BOOL AdslCoreHwReset(AC_BOOL bCoreReset);
void AdslCoreStop(void);

AC_BOOL AdslCoreIntHandler(void);
int	AdslCoreIntTaskHandler(int nMaxStatus);
void	AdslCoreTimer (unsigned long tMs);
AC_BOOL AdslCoreCommandHandler(void *cmdPtr);

AC_BOOL AdslCoreG997SendData(unsigned char lineId, void *buf, int len);

int AdslCoreG997FrameReceived(unsigned char lineId);
void	*AdslCoreG997FrameGet(unsigned char lineId, int *pLen);
void	*AdslCoreG997FrameGetNext(unsigned char lineId, int *pLen);
void	AdslCoreG997FrameFinished(unsigned char lineId);

void	BcmAdslCoreNotify(unsigned char lineId, long acEvent);
AC_BOOL BcmAdslCoreLogEnabled(void);
int		BcmAdslCoreDiagWrite(void *pBuf, int len);
void	BcmAdslCoreDiagClose(void);

/* synchronization with ADSL PHY DPC/bottom half */
#ifdef __KERNEL__
#ifdef CONFIG_SMP
void SMP_DpcSyncEnter(void);
void SMP_DpcSyncExit(void);
#define	BcmCoreDpcSyncEnter()		SMP_DpcSyncEnter()
#define	BcmCoreDpcSyncExit()		SMP_DpcSyncExit()
#else
#define	BcmCoreDpcSyncEnter()		local_bh_disable()
#define	BcmCoreDpcSyncExit()		local_bh_enable()
#endif
#elif defined(VXWORKS)
#define	BcmCoreDpcSyncEnter()		taskLock()
#define	BcmCoreDpcSyncExit()		taskUnlock()
#else
#define	BcmCoreDpcSyncEnter()
#define	BcmCoreDpcSyncExit()
#endif


/* debug macros */

#ifdef VXWORKS
#define	TEXT(__str__)		__str__
#define	AdslDrvPrintf		printf
#elif defined(__KERNEL__)
#define	TEXT(__str__)		__str__
#define	AdslDrvPrintf		printk
#define	PHY_PROFILE_SUPPORT
#elif defined(TARG_OS_RTEMS)
#define	TEXT(__str__)		__str__
#include "BcmOs.h"
#define AdslDrvPrintf           BcmOsEventLog 
#elif defined(_WIN32_WCE)
void NKDbgPrintfW(void *pwStr, ...);
#ifndef TEXT
#define TEXT(quote)			L##quote
#endif
#define	AdslDrvPrintf		NKDbgPrintfW
#elif defined(_CFE_)
#include "lib_printf.h"
#define	TEXT(__str__)		__str__
#define	AdslDrvPrintf		printf
#endif
