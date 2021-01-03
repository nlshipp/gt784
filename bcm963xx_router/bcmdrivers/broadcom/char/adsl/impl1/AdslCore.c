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
 * AdslCore.c -- Bcm ADSL core driver
 *
 * Description:
 *	This file contains BCM ADSL core driver 
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.12 $
 *
 * $Id: AdslCore.c,v 1.12 2011/09/07 22:10:53 dkhoo Exp $
 *
 * $Log: AdslCore.c,v $
 * Revision 1.12  2011/09/07 22:10:53  dkhoo
 * cover case where userspace triggers datapump loading and datapump is changed
 *
 * Revision 1.11  2011/08/05 23:29:20  dkhoo
 * [All]modify driver to quicken datapump switching time, save nvram when datapump successfuly train line
 *
 * Revision 1.10  2011/07/07 08:28:39  lzhang
 * use "sys-version-no" to display "V1000F"
 *
 * Revision 1.9  2011/06/24 01:52:39  dkhoo
 * [All]Per PLM, update DSL driver to 23j and bonding datapump to Av4bC035d
 *
 * Revision 1.7  2004/07/20 23:45:48  ilyas
 * Added driver version info, SoftDslPrintf support. Fixed G.997 related issues
 *
 * Revision 1.6  2004/06/10 00:20:33  ilyas
 * Added L2/L3 and SRA
 *
 * Revision 1.5  2004/05/06 20:03:51  ilyas
 * Removed debug printf
 *
 * Revision 1.4  2004/05/06 03:24:02  ilyas
 * Added power management commands
 *
 * Revision 1.3  2004/04/30 17:58:01  ilyas
 * Added framework for GDB communication with ADSL PHY
 *
 * Revision 1.2  2004/04/27 00:33:38  ilyas
 * Fixed buffer in shared SDRAM checking for EOC messages
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

#if defined(_CFE_)
#include "lib_types.h"
#include "lib_string.h"
void setGdbOn(void){}
int  isGdbOn(void) {return(0);}
#endif

#include "AdslCoreMap.h"

#include "softdsl/SoftDsl.h"
#include "softdsl/CircBuf.h"
#include "softdsl/BlankList.h"
#include "softdsl/BlockUtil.h"
#include "softdsl/Flatten.h"
#include "softdsl/AdslXfaceData.h"

#if defined(__KERNEL__)
#include <linux/version.h>
#include <linux/kernel.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#include <linux/bottom_half.h>	/* for local_bh_disable/local_bh_enable */
#else
#include <linux/interrupt.h>
#endif
#include "bcm_map.h"
#include "bcm_intr.h"
#include "boardparms.h"
#include "board.h"

#elif defined(_CFE_)
#include "bcm_map.h"
#include "bcm_intr.h"
#include "boardparms.h"
#include "board.h"

#elif defined(VXWORKS)

#if defined(CONFIG_BCM96348)
#include "6348_common.h"
#elif defined(CONFIG_BCM96338)
#include "6338_common.h"
#elif defined(CONFIG_BCM96358)
#include "6358_common.h"
#elif defined(CONFIG_BCM96368)
#include "6368_common.h"
#elif defined(CONFIG_BCM96362)
#include "6362_common.h"
#elif defined(CONFIG_BCM96328)
#include "6328_common.h"
#elif defined(CONFIG_BCM963268)
#include "63268_common.h"
#else
#error	"Unknown 963x8 chip"
#endif

#endif /* VXWORKS */

#ifdef ADSL_SELF_TEST
#include "AdslSelfTest.h"
#endif

#include "AdslCore.h"

#include "AdslCoreFrame.h"

#if defined(ADSL_PHY_FILE) || defined(ADSL_PHY_FILE2)
#include "AdslFile.h"
#else

#if defined(CONFIG_BCM963x8)

#if defined(CONFIG_BCM96368)
#ifdef ADSL_ANNEXC
#include "adslcore6368C/adsl_lmem.h"
#include "adslcore6368C/adsl_sdram.h"
#elif defined(ADSL_ANNEXB)
#include "adslcore6368B/adsl_lmem.h"
#include "adslcore6368B/adsl_sdram.h"
#elif defined(ADSL_SADSL)
#include "adslcore6368SA/adsl_lmem.h"
#include "adslcore6368SA/adsl_sdram.h"
#elif defined(SUPPORT_DSL_BONDING)
#include "adslcore6368bnd/adsl_lmem.h"
#include "adslcore6368bnd/adsl_sdram.h"
#else
#include "adslcore6368/adsl_lmem.h"
#include "adslcore6368/adsl_sdram.h"
#endif

#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
#ifdef ADSL_ANNEXC
#include "adslcore6362C/adsl_lmem.h"
#include "adslcore6362C/adsl_sdram.h"
#elif defined(ADSL_ANNEXB)
#include "adslcore6362B/adsl_lmem.h"
#include "adslcore6362B/adsl_sdram.h"
#elif defined(ADSL_SADSL)
#include "adslcore6362SA/adsl_lmem.h"
#include "adslcore6362SA/adsl_sdram.h"
#else
#include "adslcore6362/adsl_lmem.h"
#include "adslcore6362/adsl_sdram.h"
#endif

#elif defined(CONFIG_BCM963268)
#ifdef ADSL_ANNEXC
#include "adslcore63268C/adsl_lmem.h"
#include "adslcore63268C/adsl_sdram.h"
#elif defined(ADSL_ANNEXB)
#include "adslcore63268B/adsl_lmem.h"
#include "adslcore63268B/adsl_sdram.h"
#elif defined(ADSL_SADSL)
#include "adslcore63268SA/adsl_lmem.h"
#include "adslcore63268SA/adsl_sdram.h"
#elif defined(SUPPORT_DSL_BONDING)
#include "adslcore63268bnd/adsl_lmem.h"
#include "adslcore63268bnd/adsl_sdram.h"
#else
#include "adslcore63268/adsl_lmem.h"
#include "adslcore63268/adsl_sdram.h"
#endif

#else   /* 48 platform */
#ifdef ADSL_ANNEXC
#include "adslcore6348C/adsl_lmem.h"
#include "adslcore6348C/adsl_sdram.h"
#elif defined(ADSL_ANNEXB)
#include "adslcore6348B/adsl_lmem.h"
#include "adslcore6348B/adsl_sdram.h"
#elif defined(ADSL_SADSL)
#include "adslcore6348SA/adsl_lmem.h"
#include "adslcore6348SA/adsl_sdram.h"
#else
#include "adslcore6348/adsl_lmem.h"
#include "adslcore6348/adsl_sdram.h"
#endif  /* 48 platform */

#endif

#endif /* of CONFIG_BCM963x8 */

#endif /* ADSL_PHY_FILE */

#ifdef G997_1_FRAMER
#include "softdsl/G997.h"
#endif

#ifdef ADSL_MIB
#include "softdsl/AdslMib.h"
#endif

#ifdef G992P3
#include "softdsl/G992p3OvhMsg.h"
#undef	G992P3_DEBUG
#define	G992P3_DEBUG
#endif

#include "softdsl/SoftDsl.gh"
#include "BcmAdslDiag.h"
#include "BcmOs.h"
#include "DiagDef.h"
#include "AdslDrvVersion.h"

#include <stdarg.h>

#if defined (AEI_VDSL_CUSTOMER_NCS) && defined (SUPPORT_DSL_BONDING)
unsigned char dslDatapump = 0;
unsigned char nvram_dslDatapump = 0;
#endif

#ifdef AEI_VDSL_TOGGLE_DATAPUMP
extern int toggle_reset;
unsigned char firstTimeLoading = 1;
#endif

#undef	SDRAM_HOLD_COUNTERS
#undef	BBF_IDENTIFICATION

#ifdef CONFIG_BCM963x8

#ifdef DEBUG_L2_RET_L0
static ulong	gL2Start				= 0;
#endif

//static ulong	gTimeInL2Ms			= 0;
#define	gTimeInL2Ms(x)		(((dslVarsStruct *)(x))->timeInL2Ms)

static ulong	gL2SwitchL0TimeMs	= (ulong)-1;
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
static ulong	gTimeToWarn 		= 0;
#endif
#define		kMemPrtyWarnTime	(5 * 1000)	/* 5 Seconds */

#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348)

#define		kMemEnPrtySetMsk	(MEMC_EN_PRIOR | MEMC_ADSL_HPE)
#define		kMemPrtyRegAddr		(MEMC_BASE+MEMC_PRIOR)

#elif defined(CONFIG_BCM96358)

#define		kMemEnPrtySetMsk	(MEMC_SEL_PRIORITY | MEMC_HIPRRTYQEN | MEMC_MIPS1HIPREN)
#define		kMemPrtyRegAddr		(MEMC_BASE+MEMC_CONFIG)

#elif defined(CONFIG_BCM96368)

#define	VDSL_CLK_RATIO		(6 << VDSL_CLK_RATIO_SHIFT)
#define	PI_CNTR_CYCLES		0x3
#define	VDSL_ANALOG_RESET		(1 << 3)
#define	VDSL_AFE_BONDING_CLK_MASK	(1 << 5)
#define	VDSL_AFE_BONDING_64MHZ_CLK	(1 << 5)
#define	VDSL_AFE_BONDING_100MHZ_CLK	(0 << 5)
#ifdef PHY_BLOCK_TEST
#define	VDSL_AFE_BONDING_CLK		VDSL_AFE_BONDING_100MHZ_CLK
#else
#define	VDSL_AFE_BONDING_CLK		VDSL_AFE_BONDING_100MHZ_CLK
#endif

#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
#define ADSL_CTRL_BASE       0xb0001900
volatile ulong *pXdslControlReg = (ulong *) ADSL_CTRL_BASE;
#define	ADSL_CLK_RATIO_SHIFT		8
#define	ADSL_CLK_RATIO_MASK		(0x1F << ADSL_CLK_RATIO_SHIFT)
#define	ADSL_CLK_RATIO		(0x0 << ADSL_CLK_RATIO_SHIFT)

#define	XDSL_ANALOG_RESET		(1 << 3)
#define	XDSL_PHY_RESET		(1 << 2)
#define	XDSL_MIPS_RESET		(1 << 1)
#define	XDSL_MIPS_POR_RESET		(1 << 0)

#elif defined(CONFIG_BCM963268)

volatile ulong *pXdslControlReg = (ulong *)0xb0001800;
#define	XDSL_ANALOG_RESET		0	/* Does not exist */
#define	XDSL_PHY_RESET		(1 << 2)
#define	XDSL_MIPS_RESET		(1 << 1)
#define	XDSL_MIPS_POR_RESET		(1 << 0)

#define	XDSL_AFE_GLOBAL_PWR_DOWN	(1 << 5)

#ifndef MISC_IDDQ_CTRL_VDSL_MIPS
#define	MISC_IDDQ_CTRL_VDSL_MIPS	(1 << 10)
#endif

#define	GPIO_PAD_CTRL_AFE_SHIFT	8
#define	GPIO_PAD_CTRL_AFE_MASK	(0xF << GPIO_PAD_CTRL_AFE_SHIFT)
#define	GPIO_PAD_CTRL_AFE_VALUE	(0x9 << GPIO_PAD_CTRL_AFE_SHIFT)
#else

#error	"Unknown 963x8 chip"

#endif

#endif /* CONFIG_BCM963x8 */

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)

#if defined(__KERNEL__)
extern void BcmAdslCoreResetPhy(int copyImage);
#endif
extern void BcmXdslCoreSendAfeInfo(int toPhy);
void XdslCoreProcessDrvConfigRequest(int lineId, volatile ulong	*pDrvConfigAddr);
#ifdef CONFIG_VDSL_SUPPORTED
extern void SetupReferenceClockTo6306(void);
extern int IsAfe6306ChipUsed(void);
static void XdslCoreHwReset6306(void);
void XdslCoreSetExtAFELDMode(int ldMode);
#ifdef CONFIG_BCM96368
extern void PLLPowerUpSequence6306(void);
void XdslCoreSetExtAFEClk(int afeClk);
#endif
#ifdef CONFIG_BCM963268
extern int BcmXdslCoreGetAfeBoardId(ulong *pAfeIds);
#endif
#endif	/* CONFIG_VDSL_SUPPORTED */

#endif	/* !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358) */

extern ulong BcmAdslCoreGetCycleCount(void);
extern ulong BcmAdslCoreCycleTimeElapsedUs(ulong cnt1, ulong cnt0);
extern void setGdbOn(void);
extern int  isGdbOn(void);
extern void *memcpy( void *, const void *, unsigned int );
extern int sprintf( char *, const char *, ... );

typedef ulong (*adslCoreStatusCallback) (dslStatusStruct *status, char *pBuf, int len);

/* Local vars */

void * AdslCoreGetOemParameterData (int paramId, int **ppLen, int *pMaxLen);
static void AdslCoreSetSdramTrueSize(void);

static ulong AdslCoreIdleStatusCallback (dslStatusStruct *status, char *pBuf, int len)
{
	return 0;
}

adslPhyInfo	adslCorePhyDesc = {
	0xA0000000 | (ADSL_SDRAM_TOTAL_SIZE - ADSL_PHY_SDRAM_PAGE_SIZE), 0, 0, ADSL_PHY_SDRAM_START_4,
	0, 0, 0, 0,
	NULL, {0,0,0,0}
};

#ifdef SUPPORT_STATUS_BACKUP
#define	STAT_BKUP_BUF_SIZE	16384
#define	STAT_BKUP_THRESHOLD	3072
#define	MIN(a,b) (((a)<(b))?(a):(b))
static int gStatusBackupThreshold = STAT_BKUP_THRESHOLD;
static stretchBufferStruct *pLocSbSta = NULL;
static stretchBufferStruct *pCurSbSta = NULL;
#endif

AdslXfaceData		* volatile pAdslXface	= NULL;
AdslOemSharedData	* volatile pAdslOemData = NULL;
adslCoreStatusCallback	pAdslCoreStatusCallback = AdslCoreIdleStatusCallback;
int					adslCoreSelfTestMode = kAdslSelfTestLMEM;
ulong				adslPhyXfaceOffset = ADSL_PHY_XFACE_OFFSET;

//Boolean				adslCoreShMarginMonEnabled = AC_FALSE;
//ulong				adslCoreLOMTimeout = (ulong) -1;
//long				adslCoreLOMTime = -1;
//Boolean				adslCoreOvhMsgPrintEnabled = AC_FALSE;
#define	gXdslCoreShMarginMonEnabled(x)		(((dslVarsStruct *)(x))->xdslCoreShMarginMonEnabled)
#define	gXdslCoreLOMTimeout(x)				(((dslVarsStruct *)(x))->xdslCoreLOMTimeout)
#define	gXdslCoreLOMTime(x)				(((dslVarsStruct *)(x))->xdslCoreLOMTime)
#define	gXdslCoreOvhMsgPrintEnabled(x)		(((dslVarsStruct *)(x))->xdslCoreOvhMsgPrintEnabled)


dslVarsStruct	acDslVars[MAX_DSL_LINE];
dslVarsStruct	*gCurDslVars = &acDslVars[0];
#ifdef SUPPORT_DSL_BONDING
volatile ulong	*acAhifStatePtr[MAX_DSL_LINE] = { NULL, NULL };
#else
volatile ulong	*acAhifStatePtr[MAX_DSL_LINE] = { NULL };
#endif
ulong	acAhifStateMode = 0; /* 0 - old sync on XMT only, 1 -  +RCV showtime */ 

//#define	gDslVars	(gCurDslVars)

Boolean	acBlockStatusRead = AC_FALSE;
#define	ADSL_SDRAM_RESERVED			32
#define	ADSL_INIT_MARK				0xDEADBEEF
#define	ADSL_INIT_TIME				(30*60*1000)

typedef  struct {
	ulong		initMark;
	ulong		timeCnt;
	/* add more fields here */
} sdramReservedAreaStruct;
sdramReservedAreaStruct	*pSdramReserved = NULL;
ulong	adslCoreEcUpdateMask = 0;

#ifdef VXWORKS
int	ejtagEnable = 0;
#endif

//static unsigned long timeUpdate=0;
//static int pendingFrFlag=0;
#define gTimeUpdate(x)		(((dslVarsStruct *)(x))->timeUpdate)
#define gPendingFrFlag(x)	(((dslVarsStruct *)(x))->pendingFrFlag)

#ifdef SUPPORT_DSL_BONDING
static void *XdslCoreSetCurDslVars(uchar lineId)
{
	if(lineId < MAX_DSL_LINE)
		gCurDslVars = &acDslVars[lineId];
	return((void *)gCurDslVars);
}
#endif

void * XdslCoreGetCurDslVars(void)
{
	return((void *)gCurDslVars);
}

void *XdslCoreGetDslVars(unsigned char lineId)
{
	return((lineId < MAX_DSL_LINE)? (void*)&acDslVars[lineId]: (void*)&acDslVars[0]);
}

void XdslCoreSetOvhMsgPrintFlag(uchar lineId, Boolean flag)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);
	gXdslCoreOvhMsgPrintEnabled(pDslVars) = flag;
}

void XdslCoreSetAhifState(uchar lineId, ulong state, ulong event)
{
	volatile ulong	*p = acAhifStatePtr[lineId];
	if ( (NULL != p)  && ((acAhifStateMode != 0) || (0 == event)) )
		*p = state;
}

/*
**
**		ADSL Core SDRAM memory functions
**
*/

void *AdslCoreGetPhyInfo(void)
{
	return &adslCorePhyDesc;
}

void *AdslCoreGetSdramPageStart(void)
{
	return (void *) adslCorePhyDesc.sdramPageAddr;
}

void *AdslCoreGetSdramImageStart(void)
{
	return (void *) adslCorePhyDesc.sdramImageAddr;
}

unsigned long AdslCoreGetSdramImageSize(void)
{
	return adslCorePhyDesc.sdramImageSize;
}

static void AdslCoreSetSdramTrueSize(void)
{
	unsigned long		data[2];

	memcpy((void*)&data[0], (void*)(adslCorePhyDesc.sdramImageAddr+adslCorePhyDesc.sdramImageSize-8), 8);
	
	if( ((unsigned long)-1 == (data[0] + data[1])) &&
		(data[1] > adslCorePhyDesc.sdramImageSize) &&
		(data[1] < ADSL_SDRAM_IMAGE_SIZE)) {
		AdslDrvPrintf(TEXT("*** PhySdramSize got adjusted: 0x%X => 0x%X ***\n"), (uint) adslCorePhyDesc.sdramImageSize, (uint)data[1]);
		adslCorePhyDesc.sdramImageSize = data[1];
	}
	adslCorePhyDesc.sdramImageSize = (adslCorePhyDesc.sdramImageSize+0xF) & ~0xF;
}

void	AdslCoreSetXfaceOffset(unsigned long lmemAddr, unsigned long lmemSize)
{
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
	unsigned long		data[2];
	
	memcpy((void*)&data[0], (void*)(lmemAddr+lmemSize-8), 8);
	
	if( ((unsigned long)-1 == (data[0] + data[1])) &&
		(data[1] > lmemSize)
#if defined(CONFIG_BCM96368)
		&& (data[1] <= 0x5FF90)
#elif defined(CONFIG_BCM96362)
		&& (data[1] <= 0x27F90)
#elif defined(CONFIG_BCM96328)
		&& (data[1] <= 0x21F90)
#elif defined(CONFIG_BCM963268)
		&& (data[1] <= 0x65790)
#endif
	)
	{
		AdslDrvPrintf(TEXT("*** XfaceOffset: 0x%X => 0x%X ***\n"), (uint) adslPhyXfaceOffset, (uint)data[1]);
		adslPhyXfaceOffset = data[1];
	}	
#endif
}

void * AdslCoreSetSdramImageAddr(ulong lmem2, ulong sdramSize)
{
	ulong sdramPhyImageAddrBkup=0;
	
	if (0 == lmem2) {
		lmem2 = (sdramSize > 0x40000) ? 0x20000 : 0x40000;
		adslCorePhyDesc.sdramPhyImageAddr = ADSL_PHY_SDRAM_START + lmem2;
	}
	else {
		sdramPhyImageAddrBkup = adslCorePhyDesc.sdramPhyImageAddr;
		adslCorePhyDesc.sdramPhyImageAddr = lmem2;
	}
	
	lmem2 &= (ADSL_PHY_SDRAM_PAGE_SIZE-1);
	
	if(lmem2 < ADSL_PHY_SDRAM_BIAS) {
		adslCorePhyDesc.sdramPhyImageAddr = sdramPhyImageAddrBkup;
		return NULL;
	}
	
	adslCorePhyDesc.sdramImageAddr = adslCorePhyDesc.sdramPageAddr + lmem2;
#if (ADSL_PHY_SDRAM_PAGE_SIZE == 0x200000) && (defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358))
	if ((lmem2 & 0x00FFFFFF) < 0x100000)	/* old 256K PHY over orig. 2M */
		adslCorePhyDesc.sdramImageAddr += (0x200000 - 0x80000);
#endif
	adslCorePhyDesc.sdramImageSize = sdramSize;
	pSdramReserved = (void *) (adslCorePhyDesc.sdramPageAddr + ADSL_PHY_SDRAM_PAGE_SIZE - sizeof(sdramReservedAreaStruct));
	AdslDrvPrintf(TEXT("pSdramPHY=0x%X, 0x%X 0x%X\n"), (uint) pSdramReserved, (uint)pSdramReserved->timeCnt, (uint)pSdramReserved->initMark);
	
	adslCoreEcUpdateMask = 0;
	if (ADSL_INIT_MARK != pSdramReserved->initMark) {
		pSdramReserved->timeCnt = 0;
		pSdramReserved->initMark = ADSL_INIT_MARK;
	}
	if (pSdramReserved->timeCnt >= ADSL_INIT_TIME)
		adslCoreEcUpdateMask |= kDigEcShowtimeFastUpdateDisabled;
	
	return (void *) adslCorePhyDesc.sdramImageAddr;
}

static Boolean AdslCoreIsPhySdramAddr(void *ptr)
{
	ulong	addr = ((ulong) ptr) | 0xA0000000;

	return (addr >= adslCorePhyDesc.sdramImageAddr) && (addr < (adslCorePhyDesc.sdramPageAddr + ADSL_PHY_SDRAM_PAGE_SIZE));
}

#define ADSL_PHY_SDRAM_SHARED_START		(adslCorePhyDesc.sdramPageAddr + ADSL_PHY_SDRAM_PAGE_SIZE - ADSL_SDRAM_RESERVED)

#define SHARE_MEM_REQUIRE			2048	/* 936 bytes worst case, but will use 2KB */
static int	adslPhyShareMemIsCalloc		= 0;
static ulong	adslPhyShareMemSizeAllow	= 0;
static void	* adslPhyShareMemStart		= NULL;
static void	*pAdslSharedMemAlloc		= NULL;

void AdslCoreSharedMemInit(void)
{
	int shareMemAvailable = ADSL_SDRAM_IMAGE_SIZE - ADSL_SDRAM_RESERVED - AdslCoreGetSdramImageSize();

	printk("%s: shareMemAvailable=%d\n", __FUNCTION__, shareMemAvailable);
	
	if( adslPhyShareMemIsCalloc == 0 ) { /* If calloc() earlier, then will continure to use it; might be from Diags download */
		if(  shareMemAvailable < SHARE_MEM_REQUIRE ) {
			adslPhyShareMemStart = calloc(1,SHARE_MEM_REQUIRE);
			adslPhyShareMemStart = (void *)(0xA0000000 | ((ulong)adslPhyShareMemStart + SHARE_MEM_REQUIRE));
			adslPhyShareMemIsCalloc = 1;
			adslPhyShareMemSizeAllow = SHARE_MEM_REQUIRE - 3;
		}
		else {
			adslPhyShareMemStart = (void *) ADSL_PHY_SDRAM_SHARED_START;
			adslPhyShareMemSizeAllow = shareMemAvailable - 3;
		}
	}

	pAdslSharedMemAlloc = adslPhyShareMemStart;
	
}

void AdslCoreSharedMemFree(void *p)
{
	pAdslSharedMemAlloc = adslPhyShareMemStart;
}

void *AdslCoreSharedMemAlloc(long size)
{
	ulong	addr;
	
	BcmCoreDpcSyncEnter();
	addr = ((ulong)pAdslSharedMemAlloc - (ulong)size) & ~3;
	if (addr < ((ulong)adslPhyShareMemStart - adslPhyShareMemSizeAllow)) {
		BcmAdslCoreDiagWriteStatusString(0, "***No shared SDRAM ptr=0x%X size=%d\n", (int) pAdslSharedMemAlloc, size);
		AdslCoreSharedMemInit();
		addr = ((ulong)pAdslSharedMemAlloc - (ulong)size) & ~3;
	}

	pAdslSharedMemAlloc = (void *) addr;
	BcmCoreDpcSyncExit();
	
	return pAdslSharedMemAlloc;
}

void *AdslCoreGdbAlloc(long size)
{
	uchar	*p;

	p = (void *) (adslCorePhyDesc.sdramImageAddr + adslCorePhyDesc.sdramImageSize);
	adslCorePhyDesc.sdramImageSize += (size + 0xF) & ~0xF;
	return p;
}

void AdslCoreGdbFree(void *p)
{
	adslCorePhyDesc.sdramImageSize = ((ulong) p - adslCorePhyDesc.sdramImageAddr);
}


/*
**
**		ADSL Core Status/Command functions 
**
*/

void AdslCoreSetL2Timeout(ulong val)
{
	if( 0 == val)
		gL2SwitchL0TimeMs = (ulong)-1;
	else
		gL2SwitchL0TimeMs = val * 1000;	/* Convert # of Sec to Ms */
}
void AdslCoreIndicateLinkPowerStateL2(uchar lineId)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);
	
	gTimeInL2Ms(pDslVars) = 0;
#ifdef DEBUG_L2_RET_L0
	bcmOsGetTime(&gL2Start);
#endif
}

void AdslCoreIndicateLinkDown(uchar lineId)
{
	dslStatusStruct status;
	void *pDslVars = XdslCoreGetDslVars(lineId);
#ifdef SUPPORT_DSL_BONDING
	status.code = (lineId << DSL_LINE_SHIFT) | kDslEscapeToG994p1Status;
#else
	status.code = kDslEscapeToG994p1Status;
#endif
	AdslMibStatusSnooper(pDslVars, &status);
}

void AdslCoreIndicateLinkUp(uchar lineId)
{
	dslStatusStruct status;
#ifdef SUPPORT_DSL_BONDING
	status.code = kDslTrainingStatus | (lineId << DSL_LINE_SHIFT);
#else
	status.code = kDslTrainingStatus;
#endif
	status.param.dslTrainingInfo.code = kDslG992p2RxShowtimeActive;
	status.param.dslTrainingInfo.value= 0;
	AdslMibStatusSnooper(XdslCoreGetDslVars(lineId), &status);
	
#ifdef SUPPORT_DSL_BONDING
	status.code = kDslTrainingStatus | (lineId << DSL_LINE_SHIFT);
#else
	status.code = kDslTrainingStatus;
#endif
	status.param.dslTrainingInfo.code = kDslG992p2TxShowtimeActive;
	status.param.dslTrainingInfo.value= 0;

	AdslMibStatusSnooper(XdslCoreGetDslVars(lineId), &status);
}

void AdslCoreIndicateLinkPowerState(uchar lineId, int pwrState)
{
	dslStatusStruct status;
#ifdef SUPPORT_DSL_BONDING
	status.code = kDslConnectInfoStatus | (lineId << DSL_LINE_SHIFT);
#else
	status.code = kDslConnectInfoStatus;
#endif
	status.param.dslConnectInfo.code = kG992p3PwrStateInfo;
	status.param.dslConnectInfo.value= pwrState;
	AdslMibStatusSnooper(XdslCoreGetDslVars(lineId), &status);
}

AC_BOOL AdslCoreCommandWrite(dslCommandStruct *cmdPtr)
{
	int				n;

	n = FlattenBufferCommandWrite(&pAdslXface->sbCmd, cmdPtr);
	if (n > 0) {
#if 0 && !defined(BCM_CORE_NO_HARDWARE)
		volatile ulong	*pAdslEnum;

		pAdslEnum = (ulong *) XDSL_ENUM_BASE;
		pAdslEnum[ADSL_HOSTMESSAGE] = 1;
#endif
		return AC_TRUE;
	}
	return AC_FALSE;
}

int AdslCoreFlattenCommand(void *cmdPtr, void *dstPtr, ulong nAvail)
{
	return FlattenCommand (cmdPtr, dstPtr, nAvail);
}

AC_BOOL AdslCoreCommandIsPending(void)
{
	return StretchBufferGetReadAvail(&pAdslXface->sbCmd) ? AC_TRUE : AC_FALSE;
}

AC_BOOL AdslCoreCommandHandler(void *cmdPtr)
{
	dslCommandStruct	*cmd = (dslCommandStruct *) cmdPtr;
#ifdef G992P3
	g992p3DataPumpCapabilities	*pG992p3Cap = cmd->param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA;
	g992p3DataPumpCapabilities	*pTmpG992p3Cap;
	int							n;
#ifdef G992P5
	g992p3DataPumpCapabilities	*pG992p5Cap = cmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA;
#endif
#ifdef G993
	g993p2DataPumpCapabilities	*pG993p2Cap = cmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA;
#endif
#endif
	AC_BOOL	bRes;
	void	*pDslVars;
	uchar	lineId = DSL_LINE_ID(cmd->command);
	pDslVars = XdslCoreGetDslVars(lineId);

	switch (DSL_COMMAND_CODE(cmd->command)) {
#ifdef ADSL_MIB
		case kDslDiagStartBERT:
			AdslMibClearBertResults(pDslVars);
			break;
		case kDslDiagStopBERT:
		{
			adslMibInfo			*pMibInfo;
			ulong				mibLen;

			mibLen = sizeof(adslMibInfo);
			pMibInfo = (void *) AdslCoreGetObjectValue (lineId, NULL, 0, NULL, &mibLen);
			
			BcmAdslCoreDiagWriteStatusString(lineId, "BERT Status = %s\n"
					"BERT Total Time	= %lu sec\n"
					"BERT Elapsed Time = %lu sec\n",
					pMibInfo->adslBertStatus.bertSecCur != 0 ? "RUNNING" : "NOT RUNNING",
					pMibInfo->adslBertStatus.bertSecTotal, pMibInfo->adslBertStatus.bertSecElapsed);
			if (pMibInfo->adslBertStatus.bertSecCur != 0)
				AdslCoreBertStopEx(lineId);
			else
				BcmAdslCoreDiagWriteStatusString(lineId, "BERT_EX results: totalBits=0x%08lX%08lX, errBits=0x%08lX%08lX\n",
					pMibInfo->adslBertStatus.bertTotalBits.cntHi, pMibInfo->adslBertStatus.bertTotalBits.cntLo,
					pMibInfo->adslBertStatus.bertErrBits.cntHi, pMibInfo->adslBertStatus.bertErrBits.cntLo);
		}
			break;
		case kDslDyingGaspCmd:
			AdslMibSetLPR(pDslVars);
			break;
#endif
#ifdef G992P3
		case kDslStartPhysicalLayerCmd:

			if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2)) {
				n = sizeof(g992p3DataPumpCapabilities);
#ifdef G992P5
				if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p))
					n += sizeof(g992p3DataPumpCapabilities);
#endif
#ifdef G993
				if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
					n += sizeof(g993p2DataPumpCapabilities);
#endif
				pTmpG992p3Cap = AdslCoreSharedMemAlloc(n+0x10);
				if (NULL != pTmpG992p3Cap) {
					*pTmpG992p3Cap = *pG992p3Cap;
#ifdef G992P5
					if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p))
						*(pTmpG992p3Cap+1) = *pG992p5Cap;
#endif
#ifdef G993
					if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
						*((g993p2DataPumpCapabilities *)(pTmpG992p3Cap+2)) = *pG993p2Cap;
#endif
				}
				else
					return AC_FALSE;
				cmd->param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA = pTmpG992p3Cap;
#ifdef G992P5
				cmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA = pTmpG992p3Cap+1;
#endif
#ifdef G993
				if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
					cmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA = (g993p2DataPumpCapabilities *)(pTmpG992p3Cap+2);
				else
					cmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA = NULL;
#endif

			}
			
			AdslCoreIndicateLinkPowerState(lineId, 0);
			break;

		case kDslTestCmd:
			AdslCoreIndicateLinkPowerState(lineId, 0);
			break;

		case kDslOLRRequestCmd:
			if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2) && !AdslCoreIsPhySdramAddr(cmd->param.dslOLRRequest.carrParamPtr)) {
				if ((kAdslModAdsl2p == AdslMibGetModulationType(pDslVars)) ||
					(kVdslModVdsl2 == AdslMibGetModulationType(pDslVars)))
					n = cmd->param.dslOLRRequest.nCarrs * sizeof(dslOLRCarrParam2p);
				else
					n = cmd->param.dslOLRRequest.nCarrs * sizeof(dslOLRCarrParam);
				if (NULL != (pTmpG992p3Cap = AdslCoreSharedMemAlloc(n))) {
					memcpy (pTmpG992p3Cap, cmd->param.dslOLRRequest.carrParamPtr, n);
					cmd->param.dslOLRRequest.carrParamPtr = pTmpG992p3Cap;
				}
				else
					return AC_FALSE;
			}
			break;
#endif

		case kDslIdleCmd:
			// AdslCoreIndicateLinkDown(lineId);
			break;

		case kDslLoopbackCmd:
			AdslCoreIndicateLinkUp(lineId);
			break;

		default:
			break;
	}
	bRes = AdslCoreCommandWrite(cmd);

#ifdef DIAG_DBG
	if(bRes == AC_FALSE)
		printk("%s: AdslCoreCommandWrite failed!", __FUNCTION__);
#endif

#ifdef G992P3
	if (kDslStartPhysicalLayerCmd == DSL_COMMAND_CODE(cmd->command)) {
		cmd->param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA = pG992p3Cap;
#ifdef G992P5
		cmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA = pG992p5Cap;
#endif
#ifdef G993
		cmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA = pG993p2Cap;
#endif

	}
#endif

	return bRes;
}


int AdslCoreStatusRead (dslStatusStruct *status)
{
	int		n;

	if (acBlockStatusRead)
		return 0;
#ifdef SUPPORT_STATUS_BACKUP
	n = FlattenBufferStatusRead(pCurSbSta, status);
#else
	n = FlattenBufferStatusRead(&pAdslXface->sbSta, status);
#endif
	if (n < 0) {
		BcmAdslCoreDiagWriteStatusString (0, "Status read failure: len=%d, st.code=%lu, st.value=%ld\n",
			-n, (unsigned long) status->code, status->param.value);
		n = 0;
	}

	return n;
}

void AdslCoreStatusReadComplete (int nBytes)
{
#ifdef SUPPORT_STATUS_BACKUP
	FlattenBufferReadComplete (pCurSbSta, nBytes);
#else
	FlattenBufferReadComplete (&pAdslXface->sbSta, nBytes);
#endif
}

AC_BOOL AdslCoreStatusAvail (void)
{
	return StretchBufferGetReadAvail(&pAdslXface->sbSta) ? AC_TRUE : AC_FALSE;
}

void *AdslCoreStatusReadPtr (void)
{
#ifdef SUPPORT_STATUS_BACKUP
	void *p = StretchBufferGetReadPtr(pCurSbSta);
#else
	void *p = StretchBufferGetReadPtr(&pAdslXface->sbSta);
#endif

#ifdef FLATTEN_ADDR_ADJUST
#ifdef SUPPORT_STATUS_BACKUP
	if(pCurSbSta == &pAdslXface->sbSta)
		p = ADSL_ADDR_TO_HOST( p);
#else
	p = ADSL_ADDR_TO_HOST( p);
#endif
#endif
	return p;
}

int AdslCoreStatusAvailTotal (void)
{
#ifdef SUPPORT_STATUS_BACKUP
	return StretchBufferGetReadAvailTotal(pCurSbSta);
#else
	return StretchBufferGetReadAvailTotal(&pAdslXface->sbSta);
#endif
}

#ifdef SUPPORT_STATUS_BACKUP
AC_BOOL AdslCoreSwitchCurSbStatToSharedSbStat(void)
{
	AC_BOOL res = FALSE;
	if(pCurSbSta != &pAdslXface->sbSta) {
		pCurSbSta = &pAdslXface->sbSta;
		/* Need to re-init to avoid the potential of wrap-around when backing up statuses */
		FlattenBufferInit(pLocSbSta, pLocSbSta+sizeof(stretchBufferStruct), STAT_BKUP_BUF_SIZE, kMaxFlattenedStatusSize);
		res = TRUE;
	}
	return res;
}


#ifdef ADSLDRV_STATUS_PROFILING
extern ulong gBkupStartAtIntrCnt;
extern ulong adslCoreIntrCnt;
extern ulong gBkupStartAtdpcCnt;
extern ulong adslCoreIsrTaskCnt;
#endif
/* Called from interrupt, the bottom half is scheduled but have not started processing statuses yet */
void AdslCoreBkupSbStat(void)
{
	int byteAvail;
	void *rdPtr, *wrPtr;
	
	if(pCurSbSta != pLocSbSta) {
		if((AdslCoreStatusAvailTotal() >= gStatusBackupThreshold) && (NULL != pLocSbSta)) {
#ifdef ADSLDRV_STATUS_PROFILING
			gBkupStartAtIntrCnt=adslCoreIntrCnt;
			gBkupStartAtdpcCnt=adslCoreIsrTaskCnt;
#endif
			byteAvail = StretchBufferGetReadAvail(&pAdslXface->sbSta);
			/* Loop 2 times max when the shared SB byteAvailTotal includes wrap-around part */
			while((byteAvail > 0) && (byteAvail <= (StretchBufferGetWriteAvail(pLocSbSta) - 12))) {
				wrPtr = StretchBufferGetWritePtr(pLocSbSta);
				rdPtr = StretchBufferGetReadPtr(&pAdslXface->sbSta);
#ifdef FLATTEN_ADDR_ADJUST
				rdPtr = ADSL_ADDR_TO_HOST( rdPtr);
#endif
				BlockLongMove((((byteAvail>>2)+3)&~3), (long *)rdPtr, (long *)wrPtr);
				StretchBufferReadUpdate(&pAdslXface->sbSta, byteAvail);
				StretchBufferWriteUpdate(pLocSbSta, byteAvail);
				pCurSbSta = pLocSbSta;
				byteAvail = StretchBufferGetReadAvail(&pAdslXface->sbSta);
			}
		}
	}
	else {
		/* Backup has been started and the bottom half is still not running, continue backing up statuses */
		/* Loop once most of the time as this is processed right after PHY raised the interrupt for a status written */
		byteAvail = StretchBufferGetReadAvail(&pAdslXface->sbSta);
		while((byteAvail > 0) && (byteAvail <= (StretchBufferGetWriteAvail(pLocSbSta) - 12))) {
			wrPtr = StretchBufferGetWritePtr(pLocSbSta);
			rdPtr = StretchBufferGetReadPtr(&pAdslXface->sbSta);
#ifdef FLATTEN_ADDR_ADJUST
			rdPtr = ADSL_ADDR_TO_HOST( rdPtr);
#endif
			BlockLongMove((((byteAvail>>2)+3)&~3), (long *)rdPtr, (long *)wrPtr);
			StretchBufferReadUpdate(&pAdslXface->sbSta, byteAvail);
			StretchBufferWriteUpdate(pLocSbSta, byteAvail);
			byteAvail = StretchBufferGetReadAvail(&pAdslXface->sbSta);
		}
	}
}
#endif

static AC_BOOL AdsCoreStatBufInitialized(void *bufPtr)
{
    volatile int		cnt;

	if (bufPtr != StretchBufferGetStartPtr(&pAdslXface->sbSta))
		return AC_FALSE;

	if (bufPtr != StretchBufferGetReadPtr(&pAdslXface->sbSta))
		return AC_FALSE;

	cnt = 20;
	do {
	} while (--cnt != 0);

	return AC_TRUE;
}

#define AdsCoreStatBufAssigned()	((pAdslXface->sbSta.pExtraEnd != NULL) && (NULL != pAdslXface->sbSta.pEnd) && (NULL != pAdslXface->sbSta.pStart))
#define TmElapsedUs(cnt0)			BcmAdslCoreCycleTimeElapsedUs(BcmAdslCoreGetCycleCount(), cnt0)


AC_BOOL AdslCoreSetStatusBuffer(void *bufPtr, int bufSize)
{
	dslCommandStruct	cmd;
	
	if (NULL == bufPtr)
		bufPtr = (void *) (adslCorePhyDesc.sdramImageAddr + adslCorePhyDesc.sdramImageSize);
	bufPtr = SDRAM_ADDR_TO_ADSL(bufPtr);
	cmd.command = kDslSetStatusBufferCmd;
	cmd.param.dslStatusBufSpec.pBuf = bufPtr;
	cmd.param.dslStatusBufSpec.bufSize = bufSize;

	acBlockStatusRead = AC_TRUE;

	if (!AdslCoreCommandWrite(&cmd)) {
		acBlockStatusRead = AC_FALSE;
		return AC_FALSE;
	}

	do {
	} while (AdsCoreStatBufInitialized(bufPtr));

	acBlockStatusRead = AC_FALSE;
	return AC_TRUE;
}

/*
**
**		G.997 callback and interface functions
**
*/

#ifdef G997_1_FRAMER

//#define	kG997MaxRxPendingFrames		16
//#define	kG997MaxTxPendingFrames		16

#define AdslCoreOvhMsgSupported(gDslV)		XdslMibIsXdsl2Mod(gDslV)

//circBufferStruct		g997RxFrCB;
//void				*g997RxFrBuf[kG997MaxRxPendingFrames];
//dslFrameBuffer		*g997RxCurBuf = NULL;
#define	gG997RxFrCB(x)		(((dslVarsStruct *)(x))->g997RxFrCB)
#define	gG997RxFrBuf(x)		(((dslVarsStruct *)(x))->g997RxFrBuf)
#define	gG997RxCurBuf(x)	(((dslVarsStruct *)(x))->g997RxCurBuf)

#if 0
typedef struct {
	dslFrame		fr;
	dslFrameBuffer	frBuf;
#ifdef G992P3
	dslFrameBuffer	frBufHdr;
	uchar			eocHdr[4];
#endif
} ac997FramePoolItem;
#endif

//ac997FramePoolItem	g997TxFrBufPool[kG997MaxTxPendingFrames];
#define	gG997TxFrBufPool(x)	(((dslVarsStruct *)(x))->g997TxFrBufPool)
//void *				g997TxFrList = NULL;
#define	gG997TxFrList(x)	(((dslVarsStruct *)(x))->g997TxFrList)

Boolean AdslCoreCommonCommandHandler(void *gDslV, dslCommandStruct *cmdPtr)
{
	Boolean res;
#ifdef SUPPORT_DSL_BONDING
	cmdPtr->command |= (gLineId(gDslV) << DSL_LINE_SHIFT);
#endif
	res = AdslCoreCommandWrite(cmdPtr);
#ifdef SUPPORT_DSL_BONDING
	cmdPtr->command &= (gLineId(gDslV) << DSL_LINE_SHIFT);
#endif
	return res;
}

void AdslCoreCommonStatusHandler(void *gDslV, dslStatusStruct *status)
{
	uchar	lineId = gLineId(gDslV);
		
	switch (DSL_STATUS_CODE(status->code)) {
		case kDslConnectInfoStatus:
			BcmAdslCoreDiagWriteStatusString(lineId, "AdslCoreCommonSH (ConnInfo): code=%d, val=%d", 
				status->param.dslConnectInfo.code, status->param.dslConnectInfo.value);
			if ((kG992p3PwrStateInfo == status->param.dslConnectInfo.code) && 
				(3 == status->param.dslConnectInfo.value)) {
				AdslCoreIndicateLinkPowerState(lineId, 3);
				BcmAdslCoreNotify(lineId, ACEV_LINK_POWER_L3);
			}	
			break;
		case kDslGetOemParameter:
			{
			void	*pOemData;
			int		maxLen, *pLen;

			pOemData = AdslCoreGetOemParameterData (status->param.dslOemParameter.paramId, &pLen, &maxLen);
			status->param.dslOemParameter.dataLen = *pLen;
			status->param.dslOemParameter.dataPtr = pOemData;
			if (0 == status->param.dslOemParameter.dataLen)
				status->param.dslOemParameter.dataPtr = NULL;
			}
			break;
		default:
			break;
	}
}

void AdslCoreStatusHandler(void *gDslV, dslStatusStruct *status)
{
	(*pAdslCoreStatusCallback) (status, NULL, 0);
}

void __SoftDslPrintf(void *gDslV, char *fmt, int argNum, ...)
{
	dslStatusStruct		status;
	va_list				ap;

	va_start(ap, argNum);
#ifdef SUPPORT_DSL_BONDING
	status.code = kDslPrintfStatus | (gLineId(gDslV) << DSL_LINE_SHIFT);
#else
	status.code = kDslPrintfStatus;
#endif
	status.param.dslPrintfMsg.fmt = fmt;
	status.param.dslPrintfMsg.argNum = 0;
	status.param.dslPrintfMsg.argPtr = (void *)ap;
	va_end(ap);

	(*pAdslCoreStatusCallback) (&status, NULL, 0);
}
 
Boolean AdslCoreG997CommandHandler(void *gDslV, dslCommandStruct *cmdPtr)
{
	Boolean res;

	if ((kDslSendEocCommand == DSL_COMMAND_CODE(cmdPtr->command)) &&
		(kDslClearEocSendFrame == cmdPtr->param.dslClearEocMsg.msgId)) {
		if (AdslCoreIsPhySdramAddr(cmdPtr->param.dslClearEocMsg.dataPtr))
			cmdPtr->param.dslClearEocMsg.msgType &= ~kDslClearEocMsgDataVolatileMask;
		else 
			cmdPtr->param.dslClearEocMsg.msgType |= kDslClearEocMsgDataVolatileMask;
	}
#ifdef SUPPORT_DSL_BONDING
	cmdPtr->command |= (gLineId(gDslV) << DSL_LINE_SHIFT);
#endif
	res = AdslCoreCommandWrite(cmdPtr);
#ifdef SUPPORT_DSL_BONDING
	cmdPtr->command &= (gLineId(gDslV) << DSL_LINE_SHIFT);
#endif
	return res;
}

void AdslCoreG997StatusHandler(void *gDslV, dslStatusStruct *status)
{
}

#ifdef G992P3_DEBUG
void AdslCorePrintDebugData(void *gDslVars)
{
	BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), "gDslVars=0x%X, gG997Vars=0x%X, gG992p3OvhMsgVars=0x%X, gAdslMibVars=0x%X", 
				gDslVars, &gG997Vars, &gG992p3OvhMsgVars, &gAdslMibVars);
}

void AdslCoreG997PrintFrame(void *gDslVars, char *hdr, dslFrame *pFrame)
{
	dslFrameBuffer		*pBuf;
	uchar			*pData;
	int				len, i;
	char				str[1000], *pStr;
	Boolean			bFirstBuf = true;

	if (!gXdslCoreOvhMsgPrintEnabled(gDslVars)) {
		BcmAdslCoreWriteOvhMsg(gDslVars, hdr, pFrame);
		return;
	}

	BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), " G.997 frame %s: pFr = 0x%X, len = %ld", hdr, (int) pFrame, DslFrameGetLength(gDslVars, pFrame));
	pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);

	while (NULL != pBuf) {
		len   = DslFrameBufferGetLength(gDslVars, pBuf);
		pData = DslFrameBufferGetAddress(gDslVars, pBuf);

		if (bFirstBuf)
			pStr = str + sprintf(str, "  frameBuf: addr=0x%X, len=%d, (%s PRI%d %s %d) data = ", (int) pData, len, hdr,
				pData[0] & 3, (pData[1] & 2) ? "RSP" : "CMD", pData[1] & 1);
		else
			pStr = str + sprintf(str, "  frameBuf: addr=0x%X, len=%d, data = ", (int) pData, len);
		bFirstBuf = false;
		for (i = 0; i < len; i++) {
			pStr += sprintf(pStr, "0x%X ", *pData++);
			if ((str + sizeof(str) - pStr) < 10) {
				BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), str);
				pStr = str + sprintf(str, "  Cont data[%d-] = ", i+1);
			}
		}
		BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), str);
		pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
	}
}
#endif

void AdslCoreG997Init(uchar lineId)
{
	uchar	*p;
	int		i;
	
	dslVarsStruct	*pDslVars = (dslVarsStruct	*)XdslCoreGetDslVars(lineId);
	
	CircBufferInit (&pDslVars->g997RxFrCB, pDslVars->g997RxFrBuf, sizeof(pDslVars->g997RxFrBuf));
	pDslVars->g997RxCurBuf = NULL;

	pDslVars->g997TxFrList = NULL;
	p = (void *) &pDslVars->g997TxFrBufPool;
	for (i = 0; i < kG997MaxTxPendingFrames; i++) {
		BlankListAdd(&pDslVars->g997TxFrList, (void*) p);
		p += sizeof(pDslVars->g997TxFrBufPool[0]);
	}

}

int __AdslCoreG997SendComplete(void *gDslVars, void *userVc, ulong mid, dslFrame *pFrame)
{
	ac997FramePoolItem	*pFB = (ac997FramePoolItem	*)pFrame;
//	char * pBuf = DslFrameBufferGetAddress(gDslVars, &pFB->frBuf);
	char * pBuf = DslFrameBufferGetAddress(gDslVars, &pFB->frBuf);

	if( NULL != pBuf )
		free(pBuf);
	BlankListAdd(&gG997TxFrList(gDslVars), pFrame);
	BcmAdslCoreNotify(gLineId(gDslVars), ACEV_G997_FRAME_SENT);
	
	return 1;
}

int AdslCoreG997SendComplete(void *gDslVars, void *userVc, ulong mid, dslFrame *pFrame)
{
#ifdef G992P3
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2) && (kG992p3OvhMsgFrameBufCnt == pFrame->bufCnt)) {
		G992p3OvhMsgSendCompleteFrame(gDslVars, userVc, mid, pFrame);
#if 0 && defined(G992P3_DEBUG)
		BcmAdslCoreDiagWriteStatusString (gLineId(gDslVars), "Frame sent (from G992p3OvhMsg) pFrame = 0x%lX\n", (long) pFrame);
#endif
		return 1;
	}
#endif
#if 0 && defined(G992P3_DEBUG)
	BcmAdslCoreDiagWriteStatusString (gLineId(gDslVars), "Frame sent (NOT from G992p3OvhMsg) gDslVars = 0x%lX, pFrame = 0x%lX, pBuf = 0x%lX, bufCnt = %d\n",
	      (long)gDslVars, (long) pFrame, (long)DslFrameGetFirstBuffer(gDslVars, pFrame), (int)pFrame->bufCnt);
#endif
	return __AdslCoreG997SendComplete(gDslVars, userVc, mid, pFrame);
}

#ifdef G992P3_DEBUG
int TstG997SendFrame (void *gDslV, void *userVc, ulong mid, dslFrame *pFrame)
{
	AdslCoreG997PrintFrame(gDslV, "TX", pFrame);
//	return G997SendFrame(gDslVars, userVc, mid, pFrame);
	return G997SendFrame(gDslV, userVc, mid, pFrame);
}
#endif

int AdslCoreG997IndicateRecevice (void *gDslVars, void *userVc, ulong mid, dslFrame *pFrame)
{
	void	**p;
	int res;
#ifdef G992P3
#ifdef G992P3_DEBUG
	AdslCoreG997PrintFrame(gDslVars, "RX", pFrame);
#endif
	if (AdslCoreOvhMsgSupported(gDslVars)) {
		res = G992p3OvhMsgIndicateRcvFrame(gDslVars, NULL, 0, pFrame);
		if (res) {
			if( 1 == res )
				//G997ReturnFrame(gDslVars, NULL, 0, pFrame);
				G997ReturnFrame(gDslVars, NULL, 0, pFrame);
			return 1;
		}
	}
	else
		*(((ulong*) DslFrameGetLinkFieldAddress(gDslVars,pFrame)) + 2) = 0;
#endif
	if (CircBufferGetWriteAvail(&gG997RxFrCB(gDslVars)) > 0) {
		if( gPendingFrFlag(gDslVars)==0){
			gPendingFrFlag(gDslVars)=1;
			gTimeUpdate(gDslVars)=0;
		}
		p = CircBufferGetWritePtr(&gG997RxFrCB(gDslVars));
		*p = pFrame;
		CircBufferWriteUpdate(&gG997RxFrCB(gDslVars), sizeof(void *));
		BcmAdslCoreNotify(gLineId(gDslVars), ACEV_G997_FRAME_RCV);
	}
	else 
		BcmAdslCoreDiagWriteStatusString (gLineId(gDslVars), "Frame Received but cannot be read as Buffer is full");
	
	return 1;
}

void AdslCoreSetL3(uchar lineId)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);

	if (!AdslCoreLinkState(lineId)) {
		AdslCoreIndicateLinkPowerState(lineId, 3);
		BcmAdslCoreNotify(lineId, ACEV_LINK_POWER_L3);
	}
	if (AdslCoreOvhMsgSupported(pDslVars))
		G992p3OvhMsgSetL3(pDslVars);
}

void AdslCoreSetL0(uchar lineId)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);
	
	if (AdslCoreOvhMsgSupported(pDslVars))
		G992p3OvhMsgSetL0(pDslVars);
}

/* */
#if 0
int AdslCoreG997SendFrame (dslFrame * pFrame)
{
	return G997SendFrame(gDslVars, NULL, 0, pFrame);
}
#endif
AC_BOOL AdslCoreG997ReturnFrame (void *pDslVars)
{
	dslFrame **p;
	AC_BOOL	 res = AC_FALSE;

	if (CircBufferGetReadAvail(&gG997RxFrCB(pDslVars)) > 0) {
		p = CircBufferGetReadPtr(&gG997RxFrCB(pDslVars));
#ifdef G992P3
		G992p3OvhMsgReturnFrame(pDslVars, NULL, 0, *p);
#endif
		res = G997ReturnFrame(pDslVars, NULL, 0, *p);
		CircBufferReadUpdate(&gG997RxFrCB(pDslVars), sizeof(void *));
	}
	return res;
}

AC_BOOL AdslCoreG997SendData(unsigned char lineId, void *buf, int len)
{
	ac997FramePoolItem	*pFB;
	AC_BOOL	 res = AC_FALSE;
	void *gDslVars = XdslCoreGetDslVars(lineId);

	pFB = BlankListGet(&gG997TxFrList(gDslVars));
	if (NULL == pFB)
		return res;

	DslFrameInit (gDslVars, &pFB->fr);
	DslFrameBufferSetAddress (gDslVars, &pFB->frBuf, buf);
	DslFrameBufferSetLength (gDslVars, &pFB->frBuf, len);
#ifdef G992P3
	if (AdslCoreOvhMsgSupported(gDslVars)) {
		DslFrameBufferSetAddress (gDslVars, &pFB->frBufHdr, pFB->eocHdr);
		DslFrameBufferSetLength (gDslVars, &pFB->frBufHdr, 4);
		DslFrameEnqueBufferAtBack (gDslVars, &pFB->fr, &pFB->frBufHdr);
		DslFrameEnqueBufferAtBack (gDslVars, &pFB->fr, &pFB->frBuf);
		res = G992p3OvhMsgSendClearEocFrame(gDslVars, &pFB->fr);
	}
	else
#endif
	{
		DslFrameEnqueBufferAtBack (gDslVars, &pFB->fr, &pFB->frBuf);
		AdslCoreG997PrintFrame(gDslVars,"TX",&pFB->fr);
		res = G997SendFrame(gDslVars, NULL, 0, &pFB->fr);
	}

	if( AC_FALSE == res )
		BlankListAdd(&gG997TxFrList(gDslVars), &pFB->fr);

	return res;
}

#if 0
void TestG997SendData(void)
{
	static	uchar buf[1024] = { 0xFF, 0x3 };
	int			  i;

	__SoftDslPrintf(gDslVars, "TestG997SendData:", 0);
	for (i = 0; i < 266; i++)
		buf[2+i] = i & 0xFF;
	AdslCoreG997SendData(buf, 260);
}

static	dslFrame		frRcv;
static  dslFrameBuffer	frRcvBuf;
static  uchar			frRcvData[512];

void TestG997RcvDataInv(int param)
{
	__SoftDslPrintf(gDslVars, "TestG997RcvDataInv: param=%d\n", 0, param);

	frRcvData[0] = 1;		/* kG992p3PriorityNormal */
	frRcvData[1] = 0;		/* kG992p3Cmd */
	frRcvData[2] = 0x43;	/* kG992p3OvhMsgCmdClearEOC */
	frRcvData[3] = 1;		/* kG992p3OvhMsgCmdInvId */

	DslFrameInit (gDslVars, &frRcv);
	DslFrameBufferSetAddress (gDslVars, &frRcvBuf, frRcvData);
	DslFrameBufferSetLength (gDslVars, &frRcvBuf, 4);
	DslFrameEnqueBufferAtBack (gDslVars, &frRcv, &frRcvBuf);

	AdslCoreG997IndicateRecevice (gDslVars, NULL, 0, &frRcv);
}

void TestG997RcvData(int msgLen)
{
	int				i, frLen;
	dslFrame		*pFrame;
	dslFrameBuffer	*pFrBuf;
	uchar			*pData;

	__SoftDslPrintf(gDslVars, "TestG997RcvData: msgLen=%d\n", 0, msgLen);
	if (0 == msgLen)
		msgLen = 64;
	if (msgLen > sizeof(frRcvData))
		msgLen = sizeof(frRcvData);

	frRcvData[0] = 1;	/* kG992p3PriorityNormal */
	frRcvData[1] = 0;	/* kG992p3Cmd */
	frRcvData[2] = 0x8;	/* kG992p3OvhMsgCmdClearEOC */
	frRcvData[3] = 1;

	for (i = 0; i < msgLen; i++)
		frRcvData[4+i] = i & 0xFF;

	DslFrameInit (gDslVars, &frRcv);
	DslFrameBufferSetAddress (gDslVars, &frRcvBuf, frRcvData);
	DslFrameBufferSetLength (gDslVars, &frRcvBuf, msgLen+4);
	DslFrameEnqueBufferAtBack (gDslVars, &frRcv, &frRcvBuf);

	AdslCoreG997IndicateRecevice (gDslVars, NULL, 0, &frRcv);

	/* check how the frame was received */

	if (CircBufferGetReadAvail(&g997RxFrCB) == 0) {
		__SoftDslPrintf(gDslVars, "TestG997RcvData FAILED: no frame received\n", 0);
		return;
	}

	pFrame = *(dslFrame **)CircBufferGetReadPtr(&g997RxFrCB);
	if (pFrame != &frRcv) {
		__SoftDslPrintf(gDslVars, "TestG997RcvData FAILED: rcvFr=0x%X sentFr=0x%X\n", 0, pFrame, &frRcv);
		return;
	}
	pFrBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
	if (pFrBuf != &frRcvBuf) {
		__SoftDslPrintf(gDslVars, "TestG997RcvData FAILED: rcvFrBuf=0x%X sentFrBuf=0x%X\n", 0, pFrBuf, &frRcvBuf);
		return;
	}
	frLen   = DslFrameBufferGetLength(gDslV, pFrBuf);
	pData = DslFrameBufferGetAddress(gDslV, pFrBuf);
	__SoftDslPrintf(gDslVars, "TestG997RcvData: pData=0x%X len=%d\n", 0, pData, frLen);
	if ((pData != (frRcvData+4)) || (frLen != msgLen)) {
		__SoftDslPrintf(gDslVars, "TestG997RcvData FAILED: pData=0x%X len=%d\n", 0, pData, frLen);
		return;
	}

	/* return frame through G992p3 */

	CircBufferReadUpdate(&g997RxFrCB, sizeof(void *));
	G992p3OvhMsgReturnFrame(gDslVars, NULL, 0, pFrame);
	pFrBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
	frLen   = DslFrameBufferGetLength(gDslV, pFrBuf);
	pData = DslFrameBufferGetAddress(gDslV, pFrBuf);
	__SoftDslPrintf(gDslVars, "TestG997RcvData(RET): pData=0x%X len=%d\n", 0, pData, frLen);
	if ((pData != frRcvData) || (frLen != (msgLen+4))) {
		__SoftDslPrintf(gDslVars, "TestG997RcvData(RET) FAILED: pData=0x%X len=%d\n", 0, pData, frLen);
		return;
	}
}
#endif

int AdslCoreG997FrameReceived(unsigned char lineId)
{
	void *gDslVars = XdslCoreGetDslVars(lineId);
	
	return(CircBufferGetReadAvail(&gG997RxFrCB(gDslVars)) > 0);
}

void *AdslCoreG997BufGet(unsigned char lineId, dslFrameBuffer *pBuf, int *pLen)
{
	void *gDslVars = XdslCoreGetDslVars(lineId);

	if (NULL == pBuf)
		return NULL;

	*pLen = DslFrameBufferGetLength(gDslVars, pBuf);
	return DslFrameBufferGetAddress(gDslVars, pBuf);
}

void *AdslCoreG997FrameGet(unsigned char lineId, int *pLen)
{
	dslFrame *pFrame;
	void *gDslVars = XdslCoreGetDslVars(lineId);
	
	if (CircBufferGetReadAvail(&gG997RxFrCB(gDslVars)) == 0)
		return NULL;

	pFrame = *(dslFrame **)CircBufferGetReadPtr(&gG997RxFrCB(gDslVars));
	gG997RxCurBuf(gDslVars) = DslFrameGetFirstBuffer(gDslVars, pFrame);
	gPendingFrFlag(gDslVars) = 0;
	gTimeUpdate(gDslVars) = 0;
	return AdslCoreG997BufGet(lineId, gG997RxCurBuf(gDslVars), pLen);
}

void *AdslCoreG997FrameGetNext(unsigned char lineId, int *pLen)
{
	void *gDslVars = XdslCoreGetDslVars(lineId);

	if (NULL == gG997RxCurBuf(gDslVars))
		return NULL;

	gG997RxCurBuf(gDslVars) = DslFrameGetNextBuffer(gDslVars, gG997RxCurBuf(gDslVars));
	return AdslCoreG997BufGet(lineId, gG997RxCurBuf(gDslVars), pLen);
}

void AdslCoreG997FrameFinished(unsigned char lineId)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);

	AdslCoreG997ReturnFrame (pDslVars);
}


#endif /* G997_1_FRAMER */

/*
**
**		ADSL MIB functions
**
*/

#ifdef ADSL_MIB

int AdslCoreMibNotify(void *gDslV, ulong event)
{
	uchar lineId = gLineId(gDslV);
	
	if (event & kAdslEventLinkChange)
		BcmAdslCoreNotify (lineId, AdslCoreLinkState(lineId) ? ACEV_LINK_UP : ACEV_LINK_DOWN);
	else if (event & kAdslEventRateChange){
		BcmAdslCoreNotify (lineId, ACEV_RATE_CHANGE);
		G992p3OvhMsgSetRateChangeFlag(gDslV);
	}
	return 0;
}
#if 0
void * AdslCoreMibGetData (int dataId, void *pAdslMibData)
{
	return AdslMibGetData (gDslVars, dataId, pAdslMibData);
}
#endif
#endif

/*
**
**		Interface functions
**
*/

void setParam (ulong *addr, ulong val)
{
	*addr = val;
}

ulong getParam (ulong *addr)
{
	return *addr;
} 

#if 0 && defined(CONFIG_BCM963x8)

#define AFE_REG_BASE		0xFFF58000
#define	AFE_REG_DATA		(AFE_REG_BASE + 0xC)
#define	AFE_REG_CTRL		(AFE_REG_BASE + 0x8)

void writeAFE(ulong reg, ulong val)
{
	ulong	cycleCnt0;

	setParam((ulong *) AFE_REG_DATA, val & 0xff);
	/* need to wait 16 usecs here */
	cycleCnt0 = BcmAdslCoreGetCycleCount();
	while (TmElapsedUs(cycleCnt0) < 16)	;
	setParam((ulong *) AFE_REG_CTRL, (reg << 8) | 0x05);
	while (getParam((ulong *) AFE_REG_CTRL) & 0x01) ;
	/* need to wait 16 usecs here */
	cycleCnt0 = BcmAdslCoreGetCycleCount();
	while (TmElapsedUs(cycleCnt0) < 16)	;
	setParam((ulong *) AFE_REG_CTRL, (reg << 8) | 0x01);
	while (getParam((ulong *) AFE_REG_CTRL) & 0x01) ;
}

ulong readAFE(ulong reg)
{
	setParam((ulong *) AFE_REG_CTRL, (reg << 8) | 0x01);
	while (getParam((ulong *) AFE_REG_CTRL) & 0x01) ;
	return getParam((ulong *) AFE_REG_DATA);
}

#endif /* 0 && CONFIG_BCM963x8 */

static __inline void writeAdslEnum(int offset, int value)
{
    volatile unsigned int *penum = ((unsigned int *)XDSL_ENUM_BASE);
    penum[offset] = value;
    return;
}

#define RESET_STALL \
    do { int _stall_count = 20; while (_stall_count--) ; } while (0)

#if defined(CONFIG_BCM96368)
#define RESET_ADSL_CORE \
	GPIO->VDSLControl &= ~(VDSL_CORE_RESET | VDSL_MIPS_POR_RESET | VDSL_ANALOG_RESET); \
	RESET_STALL;											\
	GPIO->VDSLControl |= VDSL_CORE_RESET | VDSL_MIPS_POR_RESET | VDSL_ANALOG_RESET

#define ENABLE_ADSL_CORE \
	GPIO->VDSLControl |= VDSL_CORE_RESET |VDSL_MIPS_POR_RESET | VDSL_ANALOG_RESET

#define DISABLE_ADSL_CORE \
	GPIO->VDSLControl &= ~(VDSL_CORE_RESET | VDSL_MIPS_POR_RESET | VDSL_ANALOG_RESET)

#define ENABLE_ADSL_MIPS \
	GPIO->VDSLControl = GPIO->VDSLControl | \
						VDSL_CORE_RESET |	\
						VDSL_MIPS_RESET |	\
						VDSL_MIPS_POR_RESET |	\
						VDSL_ANALOG_RESET

#define DISABLE_ADSL_MIPS \
	GPIO->VDSLControl = GPIO->VDSLControl & ~VDSL_MIPS_RESET

#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)

#define RESET_ADSL_CORE \
	pXdslControlReg[0] &= ~(XDSL_PHY_RESET | XDSL_MIPS_POR_RESET | XDSL_ANALOG_RESET); \
	RESET_STALL; \
	pXdslControlReg[0] |= XDSL_PHY_RESET | XDSL_MIPS_POR_RESET | XDSL_ANALOG_RESET

#define ENABLE_ADSL_CORE \
	pXdslControlReg[0] |= XDSL_PHY_RESET | XDSL_MIPS_POR_RESET | XDSL_ANALOG_RESET

#define DISABLE_ADSL_CORE \
	pXdslControlReg[0] &= ~(XDSL_PHY_RESET | XDSL_MIPS_POR_RESET | XDSL_ANALOG_RESET)

#define ENABLE_ADSL_MIPS \
	pXdslControlReg[0] |= XDSL_PHY_RESET | \
						XDSL_MIPS_RESET | \
						XDSL_MIPS_POR_RESET | \
						XDSL_ANALOG_RESET

#define DISABLE_ADSL_MIPS \
	pXdslControlReg[0] &= ~XDSL_MIPS_RESET

#else
#define RESET_ADSL_CORE \
    writeAdslEnum(ADSL_CORE_RESET, 0x1); \
    RESET_STALL;						 \
    writeAdslEnum(ADSL_CORE_RESET, 0x0); \
    RESET_STALL

#define ENABLE_ADSL_CORE \
    writeAdslEnum(ADSL_CORE_RESET, 0x0); \
    RESET_STALL
    
#define DISABLE_ADSL_CORE \
    writeAdslEnum(ADSL_CORE_RESET, 0x1)

#define ENABLE_ADSL_MIPS \
    writeAdslEnum(ADSL_MIPS_RESET, 0x2)
    
#define DISABLE_ADSL_MIPS \
    writeAdslEnum(ADSL_MIPS_RESET, 0x3)

#endif

static AC_BOOL AdslCoreLoadImage(void)
{
	volatile ulong	*pAdslLMem = (ulong *) HOST_LMEM_BASE;

#ifdef ADSL_PHY_FILE2
	if (!AdslFileReadFile("/etc/adsl/adsl_lmem.bin", pAdslLMem))
		return AC_FALSE;
	{
	void *pSdramImage;

	pSdramImage = AdslCoreSetSdramImageAddr(pAdslLMem[2], 0);
	return (AdslFileReadFile("/etc/adsl/adsl_sdram.bin", pSdramImage) != 0);
	}
#elif defined(ADSL_PHY_FILE)
#if defined (AEI_VDSL_CUSTOMER_NCS) && defined (SUPPORT_DSL_BONDING)


#ifdef AEI_VDSL_TOGGLE_DATAPUMP
        if (toggle_reset) {
		dslDatapump = (dslDatapump + 1) % 3;
		toggle_reset = 0;
        }   
        else {
                /* strange case with A2pbC030d4 and Zyxel IES 1000 going through channel analysis
                   and then failing, seems like userspace will also trigger a datapump 
                   reload here so need to cover this case. 
                 */
		kerSysGetDslDatapump(&nvram_dslDatapump);

		if (firstTimeLoading) {
			kerSysGetDslDatapump(&dslDatapump);
			nvram_dslDatapump = dslDatapump;
			firstTimeLoading = 0;
                }
	}
#else
	kerSysGetDslDatapump(&dslDatapump);
#endif

	if (dslDatapump == 1) {
		printk("@@@@@@@loading ADSL Bonding datapump@@@@@@@\n");
		return (AdslFileLoadImage("/etc/adsl/adsl_phy2.bin", (void*)pAdslLMem, NULL) != 0);
	}
       
	if (dslDatapump == 2) {
		printk("@@@@@@@loading VDSL supercombo non-bonding datapump@@@@@@@\n");
		return (AdslFileLoadImage("/etc/adsl/adsl_phy3.bin", (void*)pAdslLMem, NULL) != 0);
	}
	else {
		printk("@@@@@@@loading VDSL Bonding datapump@@@@@@@@\n");
		return (AdslFileLoadImage("/etc/adsl/adsl_phy.bin", (void*)pAdslLMem, NULL) != 0);
	}
#else
	return (AdslFileLoadImage("/etc/adsl/adsl_phy.bin", (void*)pAdslLMem, NULL) != 0);
#endif
#else
	/* Copying ADSL core program to LMEM and SDRAM */
#ifndef  ADSLDRV_LITTLE_ENDIAN
	BlockByteMove ((sizeof(adsl_lmem)+0xF) & ~0xF, (void *)adsl_lmem, (uchar *) pAdslLMem);
#else
	for (tmp = 0; tmp < ((sizeof(adsl_lmem)+3) >> 2); tmp++)
		pAdslLMem[tmp] = ADSL_ENDIAN_CONV_LONG(((ulong *)adsl_lmem)[tmp]);
#endif

	AdslCoreSetSdramImageAddr(((ulong *) adsl_lmem)[2], sizeof(adsl_sdram));
	BlockByteMove (AdslCoreGetSdramImageSize(), (void *)adsl_sdram, AdslCoreGetSdramImageStart());
	return AC_TRUE;
#endif
}

AdslOemSharedData		adslOemDataSave;
Boolean					adslOemDataSaveFlag = AC_FALSE;
Boolean					adslOemDataModified = AC_FALSE;

static int StrCmp(char *s1, char *s2, int n)
{
	while (n > 0) {
		if (*s1++ != *s2++)
			return 1;
		n--;
	}
	return 0;
}

static int Str2Num(char *s, char **psEnd)
{
	int		n = 0;

	while ((*s >= '0') && (*s <= '9'))
		n = n*10 + (*s++ - '0');

	*psEnd = s;
	return n;
}

static char *StrSkip(char *pVer)
{
	while ((*pVer == ' ') || (*pVer == '\t') || (*pVer == '_'))
		pVer++;
	return pVer;
}

static char *AdslCoreParseStdCaps(char *pVer, adslPhyInfo *pInfo)
{
	if ('2' == pVer[0]) {
		ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAdsl2);
		ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAdslReAdsl2);
		if ('p' == pVer[1]) {
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAdsl2p);
			pVer++;
		}
		pVer++;
	}
	if ('v' == pVer[0]) {
		ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyVdslG993p2);
		if ( (pVer[1] >= '0') && (pVer[1] <= '9') )	/* 3 or 6 band */
			pVer ++;
		pVer++;
	}
	
	if('b' == pVer[0])
		pVer++;	/* Bonding Phy */
	
	return pVer;
}

static char *AdslCoreParseVersionString(char *sVer, adslPhyInfo *pInfo)
{
	static char adslVerStrAnchor[] = "Version";
	char  *pVer = sVer;
#ifdef AEI_VDSL_CUSTOMER_V1000F	
	static char adslVerStr[28] = {'\0'};
#endif	
	while (StrCmp(pVer, adslVerStrAnchor, sizeof(adslVerStrAnchor)-1) != 0) {
		pVer++;
		if (0 == *pVer)
			return pVer;
	}

	pVer += sizeof(adslVerStrAnchor)-1;
	pVer = StrSkip(pVer);
	if (0 == *pVer)
		return pVer;

#ifdef AEI_VDSL_CUSTOMER_V1000F
	snprintf(adslVerStr, sizeof(adslVerStr), "%s-%s", "V1000F", pVer);
	pInfo->pVerStr	= adslVerStr;
#else
	pInfo->pVerStr	= pVer;
#endif
	pInfo->chipType	= kAdslPhyChipUnknown;
	switch (*pVer) {
		case 'a':
		case 'A':
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAnnexA);
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyG992p2Init);
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyT1P413);
			if ('0' == pVer[1]) {
				pInfo->chipType	= kAdslPhyChip6345;
				pVer += 2;
			}
			else
				pVer = AdslCoreParseStdCaps(pVer+1, pInfo);
			break;

		case 'B':
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAnnexB);
			pVer = AdslCoreParseStdCaps(pVer+1, pInfo);
			break;

		case 'S':
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhySADSL);
			pVer += 1;
			break;

		case 'I':
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAnnexI);
		case 'C':
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyAnnexC);
			ADSL_PHY_SET_SUPPORT(pInfo, kAdslPhyG992p2Init);
			pVer += 1;
			break;
	}

	pVer = StrSkip(pVer);
	if (0 == *pVer)
		return pVer;

	if (kAdslPhyChipUnknown == pInfo->chipType) {
		switch (*pVer) {
			case 'a':
			case 'A':
				pInfo->chipType	= kAdslPhyChip6345 | (pVer[1] - '0');
				pVer += 2;
				break;
			case 'b':
			case 'B':
				pInfo->chipType	= kAdslPhyChip6348 | (pVer[1] - '0');
				pVer += 2;
				break;
			case 'c':
			case 'C':
				pInfo->chipType = kAdslPhyChip6368 | (pVer[1] - '0');
				pVer += 2;
				break;
			case 'd':
			case 'D':
				pInfo->chipType = kAdslPhyChip6362 | (pVer[1] - '0');
				pVer += 2;
				break;
			default:
				pVer += 2;
				break;
		}
	}

	pVer = StrSkip(pVer);
	if (0 == *pVer)
		return pVer;

	pInfo->mjVerNum = Str2Num(pVer, &pVer);
	if (0 == *pVer)
		return pVer;

	if ((*pVer >= 'a') && (*pVer <= 'z')) {
		int		n;

		pInfo->mnVerNum = (*pVer - 'a' + 1) * 100;
		n = Str2Num(pVer+1, &pVer);
		pInfo->mnVerNum += n;
	}
	return pVer;
}

void AdslCoreExtractPhyInfo(AdslOemSharedData *pOemData, adslPhyInfo *pInfo)
{
	char				*pVer;
	int					i;
	Boolean				bPhyFeatureSet;
	dslCommandStruct	cmd;

	pInfo->fwType		= 0;
	pInfo->chipType		= kAdslPhyChipUnknown;
	pInfo->mjVerNum		= 0;
	pInfo->mnVerNum		= 0;
	pInfo->pVerStr		= NULL;
	for (i = 0; i < sizeof(pInfo->features)/sizeof(pInfo->features[0]);i++)
		pInfo->features[i]	= 0;

	if (NULL == pAdslOemData)
		return;

	if (NULL == (pVer = AdslCoreGetVersionString()))
		return;

	pVer = AdslCoreParseVersionString(pVer, pInfo);

	bPhyFeatureSet = false;
	for (i = 0; i < sizeof(pInfo->features)/sizeof(pInfo->features[0]); i++)
		if (pAdslXface->gfcTable[i] != 0) {
			bPhyFeatureSet = true;
			break;
		}

	if (bPhyFeatureSet) {
		for (i = 0; i < sizeof(pInfo->features)/sizeof(pInfo->features[0]); i++) {
			pInfo->features[i] = pAdslXface->gfcTable[i];
			pAdslXface->gfcTable[i] = 0;
		}

		cmd.command = kDslAtmVcMapTableChanged;
		cmd.param.value = 0;
		AdslCoreCommandHandler(&cmd);
	}

#if defined(BBF_IDENTIFICATION)
	{
	char	*p, *pEocVer = pOemData->eocVersion;
	int		n;

	*pEocVer++ = AdslFeatureSupported(pInfo->features, kAdslPhyAnnexB) ? 'B' : 'A';
	if (AdslFeatureSupported(pInfo->features, kAdslPhyAdslReAdsl2))
		*pEocVer++ = 'p';
	if (AdslFeatureSupported(pInfo->features, kAdslPhyVdslG993p2)) {
		if (AdslFeatureSupported(pInfo->features, kAdslPhyVdsl30a))
			*pEocVer++ = '6';
		else if (AdslFeatureSupported(pInfo->features, kAdslPhyBonding))
			*pEocVer++ = '3';
		*pEocVer++ = 'v';
	}
	if (AdslFeatureSupported(pInfo->features, kAdslPhyBonding))
		*pEocVer++ = 'b';

	*pEocVer++ = '0' + pInfo->mjVerNum/10;
	*pEocVer++ = '0' + pInfo->mjVerNum%10;
	if (pInfo->mnVerNum != 0) {
		n = pInfo->mnVerNum/100;
		*pEocVer++ = 'a' - 1 + n;
		n = pInfo->mnVerNum - n * 100;
		if (n != 0)
			*pEocVer++ = '0' + n;
	}
	*pEocVer++  = ('_' == *pVer) ? '!' : '.';

	p = ADSL_DRV_VER_STR;
	while ((*p != ' ') && (*p != 0) && (*p != '_'))
		*pEocVer++ = *p++;
	*pEocVer++  = ('_' == *p) ? '!' : ' ';

	*pEocVer++ = '0' + ((PERF->RevID >> 20) & 0xF);
	*pEocVer++ = '0' + ((PERF->RevID >> 16) & 0xF);

	p = pEocVer;
	while (p < (char *) pOemData->eocVersion + 16)
		*p ++ = 0;
	pOemData->eocVersionLen = pEocVer - (char *) pOemData->eocVersion;
	}
#endif
}

void AdslCoreSaveOemData(void)
{
	adslOemDataSaveFlag = false;
	if ((NULL != pAdslOemData) && adslOemDataModified) {
		BlockByteMove ((sizeof(adslOemDataSave) + 3) & ~0x3, (void *)pAdslOemData, (void *) &adslOemDataSave);
		adslOemDataSaveFlag = true;
		AdslDrvPrintf(TEXT("Saving OEM data from 0x%X\n"), (int) pAdslOemData);
	}
}

void AdslCoreRestoreOemData(void)
{
	if ((NULL != pAdslOemData) && adslOemDataSaveFlag) {
		BlockByteMove ((FLD_OFFSET(AdslOemSharedData,g994RcvNonStdInfo) + 3) & ~0x3, (void *) &adslOemDataSave, (void *)pAdslOemData);
		BcmAdslCoreDiagWriteStatusString(0, "Restoring OEM data from 0x%X\n", (int) pAdslOemData);
	}
}

void AdslCoreProcessOemDataAddrMsg(dslStatusStruct *status)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	int	i, len;
#endif

	pAdslOemData = (void *) status->param.value;
	AdslCoreExtractPhyInfo(pAdslOemData, &adslCorePhyDesc);
	AdslCoreRestoreOemData();
	adslOemDataSaveFlag = AC_FALSE;
	adslOemDataModified = AC_FALSE;

#ifdef G992P3
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2)) {
		if (NULL != pAdslOemData->clEocBufPtr)
			pAdslOemData->clEocBufPtr = ADSL_ADDR_TO_HOST(pAdslOemData->clEocBufPtr);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding)) {
		len = pAdslOemData->clEocBufLen/MAX_DSL_LINE;
		for(i = 0; i < MAX_DSL_LINE; i++) {
			void *pDslVars = XdslCoreGetDslVars((uchar)i);
			G997SetTxBuffer(pDslVars, len, pAdslOemData->clEocBufPtr+(i*len));
		}
	}
	else {
		G997SetTxBuffer(XdslCoreGetDslVars(0), pAdslOemData->clEocBufLen, pAdslOemData->clEocBufPtr);
	}
#else
		G997SetTxBuffer(XdslCoreGetDslVars(0), pAdslOemData->clEocBufLen, pAdslOemData->clEocBufPtr);
#endif
	}
	
#endif
	cmd.command = kDslFilterSNRMarginCmd;
	cmd.param.value = 0xFF;
	AdslCoreCommandHandler(&cmd);
#ifdef SUPPORT_DSL_BONDING
	/* Do we have to send this per line for bonding PHY ??? */
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding)) {
		cmd.command = kDslFilterSNRMarginCmd | (1 << DSL_LINE_SHIFT);
		cmd.param.value = 0xFF;
		AdslCoreCommandHandler(&cmd);
	}
#endif

}

void AdslCoreStop(void)
{
	AdslCoreIndicateLinkDown(0);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		AdslCoreIndicateLinkDown(1);
#endif
	AdslCoreSaveOemData();
	writeAdslEnum(ADSL_INTMASK_I, 0);
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	writeAdslEnum(ADSL_INTMASK_F, 0);
#endif
	DISABLE_ADSL_MIPS;
	RESET_ADSL_CORE;
}

//#define BCM6348_ADSL_MIPS_213MHz
#define BCM6348_ADSL_MIPS_240MHz

#if defined(CONFIG_BCM96348)

#define	PLL_VALUE(n1,n2,m1r,m2r,m1c,m1b,m2b)	(n1<<20) | (n2<<15) | (m1r<<12) | (m2r<<9) | (m1c<<6) | (m1b<<3) | m2b

void AdslCoreSetPllClockA0(void)
{
	ulong	reg, cnt;

#if defined(BCM6348_ADSL_MIPS_213MHz) || defined(BCM6348_ADSL_MIPS_240MHz)
	*((ulong volatile *) 0xFFFE0038) = 0x0073B548;
#else
	*((ulong volatile *) 0xFFFE0038) = 0x0073F5C8;
#endif
	reg = *((ulong volatile *) 0xFFFE0028);
	*((ulong volatile *) 0xFFFE0028) = reg & ~0x00000400;
	cnt = BcmAdslCoreGetCycleCount();
	while (TmElapsedUs(cnt) < 16) ;
	*((ulong volatile *) 0xFFFE0028) = reg |  0x00000400;
#if defined(BCM6348_ADSL_MIPS_213MHz) || defined(BCM6348_ADSL_MIPS_240MHz)
	*((ulong volatile *) 0xFFFE3F94) = 0x80;
#endif
}

void AdslCoreSetPllClockA1(void)
{
#if defined(BCM6348_ADSL_MIPS_213MHz) || defined(BCM6348_ADSL_MIPS_240MHz)
	ulong	reg, cnt;

#ifdef BCM6348_ADSL_MIPS_213MHz
	*((ulong volatile *) 0xFFFE0038) = PLL_VALUE(3,7, 3,2, 2, 0,0);
#endif
#ifdef BCM6348_ADSL_MIPS_240MHz
	*((ulong volatile *) 0xFFFE0038) = PLL_VALUE(5,7, 3,2, 3, 0,2);
#endif

	reg = *((ulong volatile *) 0xFFFE0028);
	*((ulong volatile *) 0xFFFE0028) = reg & ~0x00000400;
	cnt = BcmAdslCoreGetCycleCount();
	while (TmElapsedUs(cnt) < 16) ;
	*((ulong volatile *) 0xFFFE0028) = reg |  0x00000400;

#ifdef BCM6348_ADSL_MIPS_213MHz
	*((ulong volatile *) 0xFFFE3F94) = 0x80;
#endif
#ifdef BCM6348_ADSL_MIPS_240MHz
	*((ulong volatile *) 0xFFFE3F94) = 0x8C;
#endif

#endif
}

#endif /* CONFIG_BCM96348 */

void AdslCoreSetPllClock(void)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	ulong	reg, chipId;

#if defined(__KERNEL__) || defined(_CFE_)
	reg = PERF->RevID;
#else
	reg = *((ulong volatile *) 0xFFFE0000);
#endif
	chipId = reg >> 16;
#endif

#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96358)
	if ((chipId == 0x6338) || (chipId == 0x6358)) {
		*((ulong volatile *) (XDSL_ENUM_BASE + 0xF94)) = 0x294;
	}
#elif defined(CONFIG_BCM96348)
	if ((reg & 0xFFFF0000) == 0x63480000) {
		if (reg >= 0x634800A1)
			AdslCoreSetPllClockA1();
		else
			AdslCoreSetPllClockA0();
	}
#elif defined(CONFIG_BCM96368)
#if 1
    /* Making sure the PI counter's are up and ready to skew the clocks */
    /* Suppose to be done in the boot loader */
	DDR->MIPSPhaseCntl |= PH_CNTR_EN;

    /* When going into Auto-Mode Make Phase Cycles = 1 */
    /* PHY_AFE_CYCLES [31:28] */
    /* PHY_CYCLES     [27:24] */
    /* PHY_CPU_CYCLES [19:16] */
    DDR->DSLCpuPhaseCntr = (DDR->DSLCpuPhaseCntr & ~DSL_PHY_AFE_PI_CNTR_CYCLES_MASK) |
        (PI_CNTR_CYCLES << DSL_PHY_AFE_PI_CNTR_CYCLES_SHIFT);
    DDR->DSLCpuPhaseCntr = (DDR->DSLCpuPhaseCntr & ~DSL_PHY_PI_CNTR_CYCLES_MASK) |
        (PI_CNTR_CYCLES << DSL_PHY_PI_CNTR_CYCLES_SHIFT);
    DDR->DSLCpuPhaseCntr = (DDR->DSLCpuPhaseCntr & ~DSL_CPU_PI_CNTR_CYCLES_MASK) |
        (PI_CNTR_CYCLES << DSL_CPU_PI_CNTR_CYCLES_SHIFT);
    /* Enable Auto Tracking Mode for DSL CPU with DSL PHY, and DSL_PHY_AFE in VdslPhyClockAlign() */
    DDR->DSLCpuPhaseCntr |= DSL_CPU_PI_CNTR_EN;
#endif
	/* 6 - 400Mhz */
	GPIO->VDSLControl = (GPIO->VDSLControl & ~VDSL_CLK_RATIO_MSK) | VDSL_CLK_RATIO;

#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)

	pXdslControlReg[0] = (pXdslControlReg[0] & ~ADSL_CLK_RATIO_MASK) | ADSL_CLK_RATIO;
#elif defined(CONFIG_BCM963268)
	MISC->miscIddqCtrl &= ~(MISC_IDDQ_CTRL_ADSL_PHY | MISC_IDDQ_CTRL_VDSL_MIPS);
	if(IsAfe6306ChipUsed()) {
		ulong afeIds[2];
		BcmXdslCoreGetAfeBoardId(&afeIds[0]);
		if((afeIds[0] & AFE_CHIP_MASK) == AFE_CHIP_6306_BITMAP) {
#ifdef BP_GET_INT_AFE_DEFINED
			unsigned short bpGpio;
			if(BP_SUCCESS == BpGetAFELDRelayGpio(&bpGpio)) {
				if((afeIds[0] & AFE_LD_MASK) == AFE_LD_ISIL1556_BITMAP) 
					kerSysSetGpio(bpGpio, kGpioActive);	/* Activate the switch relay to use the Intersil LD */
				else
					kerSysSetGpio(bpGpio, kGpioInactive);	/* De-activate the switch relay */
			}
#else
			/* Old Linux tree, assume the gpio pin for the switch relay is 39: Broadcom ref board */
			if((afeIds[0] & AFE_LD_MASK) == AFE_LD_ISIL1556_BITMAP)
				kerSysSetGpio( 39, kGpioActive);	/* Activate the switch relay to use the Intersil LD */
#endif
			/* Power off the internal AFE */
			pXdslControlReg[0] |= XDSL_AFE_GLOBAL_PWR_DOWN;
		}
		else {
			/* Most likely bonding target, power up the internal AFE */
			pXdslControlReg[0] &= ~XDSL_AFE_GLOBAL_PWR_DOWN;
		}
		GPIO->GPIOMode |= GPIO_MODE_ADSL_SPI_MOSI | GPIO_MODE_ADSL_SPI_MISO;
		GPIO->PadControl = (GPIO->PadControl & ~GPIO_PAD_CTRL_AFE_MASK) | GPIO_PAD_CTRL_AFE_VALUE;
	}
	else
		pXdslControlReg[0] &= ~XDSL_AFE_GLOBAL_PWR_DOWN;	/* Power up the internal AFE */
#endif
}

#ifdef	SDRAM_HOLD_COUNTERS
#define	rd_shift			11
#define	rt_shift			16
#define	rs_shift			21

#define INSTR_CODE(code)	#code
#define GEN_MFC_INSTR(code,rd)	__asm__ __volatile__( ".word\t" INSTR_CODE(code) "\n\t" : : : "$" #rd)
#define GEN_MTC_INSTR(code)		__asm__ __volatile__( ".word\t" INSTR_CODE(code) "\n\t")

#define opcode_MTC0			0x40800000
#define opcode_MFC0			0x40000000

#define MTC0_SEL(rd,rt,sel)	GEN_MTC_INSTR(opcode_MTC0 | (rd << rd_shift) | (rt << rt_shift) | (sel))
#define MFC0_SEL(rd,rt,sel)	GEN_MFC_INSTR(opcode_MFC0 | (rd << rd_shift) | (rt << rt_shift) | (sel), rt)


void AdslCoreStartSdramHoldCounters(void)
{
	__asm__ __volatile__("li\t$6,0x80000238\n\t" : :: "$6");
	MTC0_SEL(25,6, 6);

	__asm__ __volatile__("li\t  $7,0x7\n\t" : :: "$7");
	/* __asm__ __volatile__("li\t  $7,0x8\n\t" : :: "$7"); */
	__asm__ __volatile__("sll\t $8, $7, 2\n\t" : :: "$8");
	__asm__ __volatile__("sll\t $9, $7, 18\n\t" : :: "$9");
	__asm__ __volatile__("li\t  $10, 0x80008100\n\t" : :: "$10");
	__asm__ __volatile__("or\t  $10, $10, $8\n\t" : :: "$10");
	__asm__ __volatile__("or\t  $10, $10, $9\n\t" : :: "$10");

	MTC0_SEL(25,0, 0);
	MTC0_SEL(25,0, 1);
	MTC0_SEL(25,0, 2);
	MTC0_SEL(25,0, 3);

	MTC0_SEL(25,10,4);

/*
li    $6, 0x8000_0238
mtc0    $6, $25, 6        // select sys_events to test_mux

li    $7, 0x7                // pick a value between [15:6], that corresponds to sys_event[9:0]
sll    $8, $7, 2
sll    $9, $7, 18
li    $10, 0x8000_8100     // counter1 counts active HIGH, counter0 counts positive edge
or    $10, $10, $8
or    $10, $10, $9

mtc0    $0, $25, 0        // initialize counters to ZEROs
mtc0    $0, $25, 1
mtc0    $0, $25, 2
mtc0    $0, $25, 3
 
mtc0    $10, $25, 4    // start counting
*/
}
 
ulong AdslCoreReadtSdramHoldEvents(void)
{
	MFC0_SEL(25,2,0);
}

ulong AdslCoreReadtSdramHoldTime(void)
{
	MFC0_SEL(25,2,1);
}

void AdslCorePrintSdramCounters(void)
{
	BcmAdslCoreDiagWriteStatusString(0, "SDRAM Hold: Events = %d, Total Time = %d", 
		AdslCoreReadtSdramHoldEvents(),
		AdslCoreReadtSdramHoldTime());
}
#endif /* SDRAM_HOLD_COUNTERS */

#ifndef MEM_MONITOR_ID
#define MEM_MONITOR_ID	0
#endif

#ifdef PHY_BLOCK_TEST
#define	HW_RESET_DELAY	500000
#else
#define	HW_RESET_DELAY	1000000
#endif

#ifdef CONFIG_BCM96368

#define COUNTLIMIT      200
#define ERRCNTLIMIT     10
#define NVALIDSAMPLE    50
#define PHASE_OFFSET    (0*NVALIDSAMPLE)   // set to 2, -2 for margin test, 0 for nominal

static int VdslPhyClockAlign(void)
{
    uint32  tmp, PhaseCntl;
    int     i, j, err_cnt, tmpPhaseValue, PrePhaseValue, PhaseValue, PhaseCount;
    
    // Check if the PHY clocks are phase adjusted with ubus phase
    PhaseCntl = DDR->DeskewDLLCntl;
    if (( PhaseCntl & (1<<12)) != 0 ) {
        printk("Clocks for AFE and QPROC are phase adjusted with ubus clock phase, DeskewDLLCntl = 0x%08X\n", (uint)PhaseCntl);
        tmp = DDR->Spare2;
        printk("Clocks for QPROC and AFE are aligned with AFE_syn_status = 0x%02X, QPROC_syn_status = 0x%02X\n", (uint)tmp&0x7f, (uint)(tmp>>7)&0x7f);
        return 1;
    }
    
    printk("Clocks for QPROC and AFE are being aligned with step through ... \n");
    
    // VDSL PHY AFE clock alignment

    // printk("DSL PHY AFE clock alignment ...\n");

    PhaseCntl  = DDR->DSLCorePhaseCntl;
    tmp        = DDR->Spare2 & 0x7f;    // AFE Phase value
    PhaseCount = (PhaseCntl<<2) >> 18;   // sign extend, then shift to LSB
    // printk("AFE init phase, PhaseCntl = 0x%08X, PhaseCount = 0x%08X, syn_status = 0x%02X\n", PhaseCntl, PhaseCount, tmp);

    PrePhaseValue = 0;
    for (i = 0; i < COUNTLIMIT; i++) {
        PhaseValue = PHASE_OFFSET;
        err_cnt    = 0;
        // search for a stable read of the phase value
        for (j=1; j<=ERRCNTLIMIT+NVALIDSAMPLE; j++) {
            tmp = DDR->Spare2 & 0x7f;    // AFE Phase value
            tmp = DDR->Spare2 & 0x7f;    // AFE Phase value
            switch (tmp) {
                case 0x07: 
                case 0x03: 
                case 0x01: 
                case 0x00: tmpPhaseValue =-10; break;
                case 0x40: tmpPhaseValue = -5; break;
                case 0x60: tmpPhaseValue = -3; break;
                case 0x70: tmpPhaseValue = -1; break;
                case 0x78: tmpPhaseValue =  1; break;
                case 0x7C: tmpPhaseValue =  3; break;
                case 0x7E: tmpPhaseValue =  5; break;
                case 0x7F: 
                case 0x3F: 
                case 0x1F: 
                case 0x0F: tmpPhaseValue = 10; break;

                default  : tmpPhaseValue =  0; 
                           err_cnt++; 
                           if (err_cnt == ERRCNTLIMIT)  {
                               printk("Clock failed to align, too many random phase value, sync_status = 0x%02X\n", (uint)tmp);
                               return 0;
                           }
                           break;
            }
            if (tmpPhaseValue == -10) {
                PhaseValue = PHASE_OFFSET - 10*NVALIDSAMPLE;    // (-10 * 50 = -500)
                break;                                          // for(j=1; 
            }
            if (tmpPhaseValue ==  10) {
                PhaseValue = PHASE_OFFSET + 10*NVALIDSAMPLE;    // ( 10 * 50 = -500)
                break;                                          // for(j=1;
            }

            PhaseValue += tmpPhaseValue;
            if (j - err_cnt == NVALIDSAMPLE ) break;            // for(j=1;
        }

        if (PhaseValue > 0) {
            if ((PhaseValue > 7*NVALIDSAMPLE) || (PrePhaseValue >= 0)) {    // 4*50=200
                PhaseCount--;
                PhaseCntl = (PhaseCntl&0xffff) | ((PhaseCount&0x3fff)<<16);      // AFE count down
                DDR->DSLCorePhaseCntl = PhaseCntl;
            } else if (PrePhaseValue < 0) {
                if (PhaseValue + PrePhaseValue > 0) {   // previous Phase Value is more close to 0
                    PhaseCount--;
                    PhaseCntl = (PhaseCntl&0xffff) | ((PhaseCount&0x3fff)<<16);  // AFE count down
                    DDR->DSLCorePhaseCntl = PhaseCntl;
                }
                break;      // for (count = 1; clock is aligned, we are done
            }
        } else if (PhaseValue < 0) {
            if ((PhaseValue < -7*NVALIDSAMPLE) || (PrePhaseValue <= 0)) {
                PhaseCount++;
                PhaseCntl = (PhaseCntl&0xffff) | ((PhaseCount&0x3fff)<<16) | (1<<30);      // AFE count up
                DDR->DSLCorePhaseCntl = PhaseCntl;      // AFE count up
            } else if (PrePhaseValue > 0) {
                 if (PhaseValue + PrePhaseValue < 0) {   // previous Phase Value is more close to 0
                     PhaseCount++;
                     PhaseCntl = (PhaseCntl&0xffff) | ((PhaseCount&0x3fff)<<16) | (1<<30);  // AFE count up
                     DDR->DSLCorePhaseCntl = PhaseCntl;
                 }
                 break;      // for (count = 1; clock is aligned, we are done
            }
        } else { // (PhaseValue == 0)
            break;      // for (count = 1; clock is aligned, PhaseValue=0, we are done
        }
        PrePhaseValue = PhaseValue;
        // printk("AFE iteration i = %d, PhaseValue = %d, PhaseCntl = 0x%08X\n", i, PhaseValue, PhaseCntl);
    }

    if (i == COUNTLIMIT) {
        printk("Clocks failed to align, COUNTLIMIT reached, adjusted PhaseValue = %04d, PhaseCount = %04d\n", PhaseValue, PhaseCount);
        return 0;
    }

    printk("AFE is aligned, i = %03d, PhaseValue = %04d, PhaseCntl = 0x%08X\n", i, PhaseValue, (uint)PhaseCntl);

    // VDSL PHY QProc clock alignment

    // printk("VDSL PHY QProc clock alignment ...\n");

    PhaseCntl  = DDR->DSLCorePhaseCntl;
    tmp        = ((DDR->Spare2)>>7) & 0x7f;     // QPROC Phase value
    PhaseCount = (PhaseCntl<<18) >> 18;         // sign extend, then shift to LSB
    // printk("QPROC init phase, PhaseCntl = 0x%08X, PhaseCount = 0x%08X, syn_status = 0x%02X\n", PhaseCntl, PhaseCount, tmp);

    PrePhaseValue = 0;
    for (i = 0; i < COUNTLIMIT; i++) {
        PhaseValue = PHASE_OFFSET;
        err_cnt    = 0;
        // search for a stable read of the phase value
        for (j=1; j<=ERRCNTLIMIT+NVALIDSAMPLE; j++) {
            tmp = ((DDR->Spare2)>>7) & 0x7f;    // QPROC Phase value
            tmp = ((DDR->Spare2)>>7) & 0x7f;    // QPROC Phase value
            switch (tmp) {
                case 0x07: 
                case 0x03: 
                case 0x01: 
                case 0x00: tmpPhaseValue =-10; break;
                case 0x40: tmpPhaseValue = -5; break;
                case 0x60: tmpPhaseValue = -3; break;
                case 0x70: tmpPhaseValue = -1; break;
                case 0x78: tmpPhaseValue =  1; break;
                case 0x7C: tmpPhaseValue =  3; break;
                case 0x7E: tmpPhaseValue =  5; break;
                case 0x7F: 
                case 0x3F: 
                case 0x1F: 
                case 0x0F: tmpPhaseValue = 10; break;

                default  : tmpPhaseValue =  0; 
                           err_cnt++; 
                           if (err_cnt == ERRCNTLIMIT)  {
                               printk("Clock failed to align, too many random phase value, sync_status = 0x%02X\n", (uint)tmp);
                               return 0;
                           }
                           break;
            }
            if (tmpPhaseValue == -10) {
                PhaseValue = PHASE_OFFSET - 10*NVALIDSAMPLE;    // (-10 * 50 = -500)
                break;                                          // for(j=1; 
            }
            if (tmpPhaseValue ==  10) {
                PhaseValue = PHASE_OFFSET + 10*NVALIDSAMPLE;    // ( 10 * 50 = -500)
                break;                                          // for(j=1;
            }

            PhaseValue += tmpPhaseValue;
            if (j - err_cnt == NVALIDSAMPLE ) break;            // for(j=1;
        }

        if (PhaseValue > 0) {
            if ((PhaseValue > 7*NVALIDSAMPLE) || (PrePhaseValue >= 0)) {    // 4*50=200
                PhaseCount--;
                PhaseCntl = (PhaseCntl&0xffff0000) | ((PhaseCount & 0x3fff) << 0);      // QPROC count down
                DDR->DSLCorePhaseCntl = PhaseCntl;
            } else if (PrePhaseValue < 0) {
                if (PhaseValue + PrePhaseValue > 0) {   // previous Phase Value is more close to 0
                    PhaseCount--;
                    PhaseCntl = (PhaseCntl&0xffff0000) | ((PhaseCount & 0x3fff) << 0);  // QPROC count down
                    DDR->DSLCorePhaseCntl = PhaseCntl;
                }
                break;      // for (count = 1; clock is aligned, we are done
            }
        } else if (PhaseValue < 0) {
            if ((PhaseValue < -7*NVALIDSAMPLE) || (PrePhaseValue <= 0)) {
                PhaseCount++;
                PhaseCntl  = (PhaseCntl&0xffff0000) | ((PhaseCount & 0x3fff) << 0) | (1<<14);         // QPROC count up
                DDR->DSLCorePhaseCntl = PhaseCntl;      // AFE count up
            } else if (PrePhaseValue > 0) {
                 if (PhaseValue + PrePhaseValue < 0) {   // previous Phase Value is more close to 0
                     PhaseCount++;
                     PhaseCntl  = (PhaseCntl&0xffff0000) | ((PhaseCount & 0x3fff) << 0) | (1<<14);    // QPROC count up
                     DDR->DSLCorePhaseCntl = PhaseCntl;
                 }
                 break;      // for (count = 1; clock is aligned, we are done
            }
        } else { // (PhaseValue == 0)
            break;      // for (count = 1; clock is aligned, PhaseValue=0, we are done
        }
        PrePhaseValue = PhaseValue;
        // printk("QPROC iteration i = %03d, PhaseValue = %04d, PhaseCntl = 0x%08X\n", i, PhaseValue, PhaseCntl);
    }

    if (i == COUNTLIMIT) {
        printk("Clocks failed to align, COUNTLIMIT reached, adjusted PhaseValue = %03d, PhaseCount = %04d\n", PhaseValue, PhaseCount);
        return 0;
    }

    printk("QPROC is aligned, i = %03d, PhaseValue = %04d, PhaseCntl = 0x%08X\n", i, PhaseValue, (uint)PhaseCntl);


    // Clock is aligned now, print out some info

    tmp = DDR->Spare2;
    printk("Clocks for QPROC and AFE are aligned with syn_status AFE = 0x%02X, QPROC = 0x%02X\n", (uint)tmp&0x7F, (uint)(tmp>>7)&0x7f);

    tmp = *((uint32 volatile *) 0xb0f570f8);
    printk("AFE  phase control reg @0xb0f570f8 default actual = 0x%08X, exp = 0x0021c38f\n", (uint)tmp);
    
    tmp = *((uint32 volatile *) 0xb0f5f0c0);
    printk("QPRC phase control reg @0xb0f5f0c0 default actual = 0x%08X, exp = 0x0421c38f\n", (uint)tmp);

    return 1;
    // Clock is aligned now, reset the sync registers

    /* Enable Auto Tracking Mode for DSL CPU, DSL PHY, and DSL_PHY_AFE */
    // DDR->DSLCpuPhaseCntr |= DSL_PHY_PI_CNTR_EN | DSL_PHY_AFE_PI_CNTR_EN;
}
#elif defined(CONFIG_BCM96328)
#define PI_PHASE_OFFSET                     0x29
#define PI_PHASE_READ_CNT                   200
#define PI_PHASE_SEARCH_RANGE               (416*4)

static void shiftPiPhase(int shiftCnt, int isInc)
{
    ulong tmpValue;
    ulong phase;
    int   phaseDiff;
    int   i;

    tmpValue = DDR->PI_DSL_PHY_CTL;
    phase = tmpValue & 0x3fff;
    tmpValue &= ~0x3fff;    /* mask out phase */
    tmpValue &= ~0x4000;    /* mask out inc/dec */
    phaseDiff = -1;         /* default is decrease */
    if (isInc) {
        tmpValue |= 0x4000;
        phaseDiff = 1;
    }

    for (i=0; i<shiftCnt; i++) {
        phase += phaseDiff;
        DDR->PI_DSL_PHY_CTL = tmpValue|phase;
    }
} 

static int getPiPhase(int readCnt)
{
    ulong   regValue;
    int     zeroCnt = 0;
    int     oneCnt = 0;
    int     loopCnt;

    for (loopCnt = 0; loopCnt < readCnt; loopCnt++) {
        regValue = *((ulong volatile *) 0xb0d5f0c4); /* RBUS_QPROC_CLK_SYN_STATUS */
        if (regValue & 1)
            oneCnt++;
        else 
            zeroCnt++;
    }

    if (oneCnt != 0 && zeroCnt != 0) 
        return -1;      /* un-reliable */ 
    else if (oneCnt != 0) 
        return 1; 
    else 
        return 0; 
} 

static int XdslPhyClockAlign(void)
{
   uint regValue, cycleCnt0;
   int   prevPhase, currPhase;
   int   currLoopCnt, prevLoopCnt;
   int   zero2OnePhase = 0;

   /* Disable HW auto PI sync */ 
   printk("Disable HW auto PI ...\n");
   *((ulong volatile *) 0xb0d5f0c0) = 0x00700387;
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);
   regValue &= ~0x100000;
   DDR->PI_DSL_PHY_CTL = regValue;
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);

   /* Collect init phase */ 
   printk("Reset PI counter ...\n");
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);
   regValue &= 0x3fff; 
   shiftPiPhase(regValue, 0);  /* shift left to make phase equal to 0 */ 
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);

   /* look for 0->1 or 1->0 transitions */ 
   printk("Look for transitions ...\n");
   prevPhase = getPiPhase(PI_PHASE_READ_CNT);
   currPhase = prevPhase; 
   prevLoopCnt = 0; 

   for (currLoopCnt = 0; currLoopCnt <= PI_PHASE_SEARCH_RANGE; currLoopCnt++) { 
       shiftPiPhase(1, 1);
       /* Need to wait 5 usecs here */
      cycleCnt0 = BcmAdslCoreGetCycleCount();
      while (TmElapsedUs(cycleCnt0) < 5)
       currPhase = getPiPhase(PI_PHASE_READ_CNT); 
       if (currPhase >= 0) { 
           if (currPhase != prevPhase) { 
               printk(
                   "Transition found at (%4d -> %4d): %1d -> %1d\n",
                   prevLoopCnt, currLoopCnt, prevPhase, currPhase);
               if (currPhase == 1 && currLoopCnt > PI_PHASE_OFFSET) {
                   if (zero2OnePhase == 0)
                       zero2OnePhase = currLoopCnt;
                   if ((currLoopCnt - zero2OnePhase) <= 8)
                       zero2OnePhase = currLoopCnt;
              }
           }
           prevPhase = currPhase;
           prevLoopCnt = currLoopCnt;
       }
   } 
   printk("Zero2OnePhase ==> 0x%4d\n", zero2OnePhase);
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);

   /* Align clock phase to be zero2OnePhase - PI_PHASE_OFFSET */ 
   printk("Reset PI counter ...\n");
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);
   regValue &= 0x3fff; 
   shiftPiPhase(regValue, 0);  /* shift left to make phase equal to 0 */ 
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);
   printk("Align PI counter to 1st zero to one minus PI offset\n");
   shiftPiPhase(zero2OnePhase - PI_PHASE_OFFSET, 1);  /* shift right */ 
   regValue = DDR->PI_DSL_PHY_CTL;
   printk("regDdrCtlPiDslPhyCtl ==> 0x%08x\n", regValue);

   return 1;
} 
#endif /* defined(CONFIG_BCM96328) */

#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)

#ifdef CONFIG_VDSL_SUPPORTED
static void XdslCoreHwReset6306(void)
{
#ifdef BP_GET_EXT_AFE_DEFINED
	unsigned short bpGpio;

	if(BP_SUCCESS == BpGetExtAFEResetGpio(&bpGpio)) {
		AdslDrvPrintf(TEXT("*** XdslCoreHwReset6306 ***\n"));
		kerSysSetGpio(bpGpio, kGpioInactive);
		kerSysSetGpio(bpGpio, kGpioActive);
		kerSysSetGpio(bpGpio, kGpioInactive);
	}
#else
	BCMOS_DECLARE_IRQFLAGS(flags);

	AdslDrvPrintf(TEXT("*** XdslCoreHwReset6306 ***\n"));
	/* Default to 96368MBG board*/
	/* miwang (7/16/10) should call kerSysSetGpioState to do this,
	 * but kerSysSetGpioState cannot reach these high bits, so
	 * just acquire gpio spinlock and access here.
	 */
	BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);
	GPIO->GPIODir_high |= 0x00000008;
	GPIO->GPIOio_high |= 0x00000008;
	GPIO->GPIOio_high &= ~0x00000008;
	GPIO->GPIOio_high |= 0x00000008;
	BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);
#endif
}
#endif

void XdslCoreSetExtAFELDMode(int ldMode)
{
#ifdef BP_GET_EXT_AFE_DEFINED
	unsigned short bpGpioPwr, bpGpioMode;

	if((BP_SUCCESS == BpGetExtAFELDPwrGpio(&bpGpioPwr)) &&
		(BP_SUCCESS == BpGetExtAFELDModeGpio(&bpGpioMode))) {
#ifdef CONFIG_BCM963268
		/* By default the CFE has this bit set - control from PHY, but PHY is passing control to the Host, so clear it */
		GPIO->GPIOBaseMode &= ~GPIO_BASE_VDSL_PHY_OVERRIDE_1;
#endif
		kerSysSetGpio(bpGpioPwr, kGpioInactive);	/* Turn Off LD power*/
		kerSysSetGpio(bpGpioMode, (LD_MODE_ADSL==ldMode)? kGpioActive: kGpioInactive);	/* ACTIVE-ADSL/INACTIVE-VDSL */
		kerSysSetGpio(bpGpioPwr, kGpioActive);	/* Turn On LD power*/
		BcmAdslCoreDiagWriteStatusString(0, "*** External AFE LD Mode: %s ***\n", (LD_MODE_ADSL==ldMode)? "ADSL": "VDSL");
	}
#ifdef CONFIG_BCM963268
	else {
		/* Building with old Linux tree, use hard-coded gpio pins */
		bpGpioMode=12;
		bpGpioPwr=13;
		GPIO->GPIOBaseMode &= ~GPIO_BASE_VDSL_PHY_OVERRIDE_1;
		kerSysSetGpio(bpGpioPwr, kGpioInactive);	/* Turn Off LD power*/
		kerSysSetGpio(bpGpioMode, (LD_MODE_ADSL==ldMode)? kGpioActive: kGpioInactive);	/* ACTIVE-ADSL/INACTIVE-VDSL */
		kerSysSetGpio(bpGpioPwr, kGpioActive);	/* Turn On LD power*/
		BcmAdslCoreDiagWriteStatusString(0, "*** External AFE LD Mode: %s ***\n", (LD_MODE_ADSL==ldMode)? "ADSL": "VDSL");
	}
#endif
#endif	/* BP_GET_EXT_AFE_DEFINED */
}

#ifdef CONFIG_BCM96368
void XdslCoreSetExtAFEClk(int afeClk)
{
	GPIO->VDSLControl = (GPIO->VDSLControl & ~VDSL_AFE_BONDING_CLK_MASK) |
			(afeClk & VDSL_AFE_BONDING_CLK_MASK);
	BcmAdslCoreDiagWriteStatusString(0, "*** External AFE Clk: %s ***\n",
			(GPIO->VDSLControl & VDSL_AFE_BONDING_CLK_MASK) ? "64Mhz": "100Mhz");
}
#endif

void XdslCoreProcessDrvConfigRequest(int lineId, volatile ulong	*pDrvConfigAddr)
{
	ulong ctrlMsg = *pDrvConfigAddr >> 1;
#if defined(CONFIG_VDSL_SUPPORTED) && defined(BP_GET_INT_AFE_DEFINED)
	unsigned short bpGpioPwr, bpGpioMode;
	volatile drvRegControl *pDrvRegCtrl;
#endif

	switch(ctrlMsg) {
#ifdef CONFIG_VDSL_SUPPORTED
#ifdef BP_GET_INT_AFE_DEFINED
		case kDslDrvConfigGetIntLDGpio:
			pDrvRegCtrl = (drvRegControl *)pDrvConfigAddr;
			if((BP_SUCCESS == BpGetIntAFELDPwrGpio(&bpGpioPwr)) &&
				(BP_SUCCESS == BpGetIntAFELDModeGpio(&bpGpioMode))) {
				pDrvRegCtrl->bpGpioPwr = bpGpioPwr;
				pDrvRegCtrl->bpGpioMode = bpGpioMode;
			}
			else {
				pDrvRegCtrl->bpGpioPwr = (ulong)-1;
				pDrvRegCtrl->bpGpioMode = (ulong)-1;
			}
			break;
		case kDslDrvConfigGetExtLDGpio:
			pDrvRegCtrl = (drvRegControl *)pDrvConfigAddr;
			if((BP_SUCCESS == BpGetExtAFELDPwrGpio(&bpGpioPwr)) &&
				(BP_SUCCESS == BpGetExtAFELDModeGpio(&bpGpioMode))) {
				pDrvRegCtrl->bpGpioPwr = bpGpioPwr;
				pDrvRegCtrl->bpGpioMode = bpGpioMode;
			}
			else {
				pDrvRegCtrl->bpGpioPwr = (ulong)-1;
				pDrvRegCtrl->bpGpioMode = (ulong)-1;
			}
			break;
#endif
		case kDslDrvConfigSetLD6302Adsl:
			XdslCoreSetExtAFELDMode(LD_MODE_ADSL);
			break;
		case kDslDrvConfigSetLD6302Vdsl:
			XdslCoreSetExtAFELDMode(LD_MODE_VDSL);
			break;
#if defined(CONFIG_BCM96368)
		case kDslDrvConfigSet6306DIFClk64:
			XdslCoreSetExtAFEClk(VDSL_AFE_BONDING_64MHZ_CLK);
			break;
		case kDslDrvConfigSet6306DIFClk100:
			XdslCoreSetExtAFEClk(VDSL_AFE_BONDING_100MHZ_CLK);
			break;
#endif
		case kDslDrvConfigReset6306:
			XdslCoreHwReset6306();
			break;
#endif	/* CONFIG_VDSL_SUPPORTED */
		case kDslDrvConfigRegRead:
			((drvRegControl *) pDrvConfigAddr)->regValue = 
				*(ulong *) (((drvRegControl *) pDrvConfigAddr)->regAddr);
			break;
		case kDslDrvConfigRegWrite:
			*(ulong *) (((drvRegControl *) pDrvConfigAddr)->regAddr) =
				((drvRegControl *) pDrvConfigAddr)->regValue;
			break;
		case kDslDrvConfigAhifStatePtr:
			acAhifStatePtr[lineId] = pDrvConfigAddr + 1;
			acAhifStateMode = (*(pDrvConfigAddr + 1) == (ulong) -2) ? 1 : 0;
			*(pDrvConfigAddr + 1) = acAhifStateMode;
			break;
	}
	*pDrvConfigAddr |= 1;	/* Ack to PHY */
}
#endif

#ifdef LMEM_ACCESS_WORKAROUND
extern void BcmAdslCoreTestLMEM(void);
#define	ALT_BOOT_VECTOR_SHIFT	16
#define	ALT_BOOT_VECTOR_MASK		(0xFFF << ALT_BOOT_VECTOR_SHIFT)
#define	ALT_BOOT_VECTOR_LMEM		(0xB90 << ALT_BOOT_VECTOR_SHIFT)
#define	ALT_BOOT_VECTOR_SDRAM	(((adslCorePhyDesc.sdramImageAddr&0xFFF00000) >> 20) << ALT_BOOT_VECTOR_SHIFT)

AC_BOOL AdslCoreIsPhyMipsRunning(void)
{
	return(XDSL_MIPS_RESET == (pXdslControlReg[0] & XDSL_MIPS_RESET));
}

void AdslCoreRunPhyMipsInSDRAM(void)
{
	pXdslControlReg[0] &= ~ALT_BOOT_VECTOR_MASK;
	pXdslControlReg[0] |= ALT_BOOT_VECTOR_SDRAM;
	*(volatile ulong *)adslCorePhyDesc.sdramImageAddr = 0x42000020;	/* wait/loop instruction */
	*(volatile ulong *)(adslCorePhyDesc.sdramImageAddr+4) = 0x08040000;
	*(volatile ulong *)(adslCorePhyDesc.sdramImageAddr+8) = 0;
	ENABLE_ADSL_MIPS;
	printk("*** %s: Started ***\n", __FUNCTION__);
}
#endif

static AC_BOOL __AdslCoreHwReset(AC_BOOL bCoreReset)
{
#ifndef BCM_CORE_NO_HARDWARE
	volatile ulong			*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	volatile AdslXfaceData	*pAdslX = NULL;
	ulong				to, tmp, cycleCnt0;
#ifdef PHY_BLOCK_TEST
	volatile ulong			*pAdslLmem = (void *) HOST_LMEM_BASE;
	ulong				memMonitorIdIntialVal;
#endif
#ifdef CONFIG_VDSL_SUPPORTED
	int afe6306ChipUsed = IsAfe6306ChipUsed();
#endif
	AdslCoreIndicateLinkDown(0);
	
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		AdslCoreIndicateLinkDown(1);
#endif
	if (!adslOemDataSaveFlag)
		AdslCoreSaveOemData();

	if (bCoreReset) {
		/* take ADSL core out of reset */
		RESET_ADSL_CORE;
		
#ifdef CONFIG_BCM96368
		if(0x636800a0 != PERF->RevID) {
			if(!VdslPhyClockAlign()) {
				AdslDrvPrintf(TEXT("AdslCoreHwReset:  Failed to align VDSL Phy clock\n"));
				return AC_FALSE;
			}
		}
#elif defined(CONFIG_BCM96328)
		if (0x632800a0 == PERF->RevID) {
			if(!XdslPhyClockAlign()) {
				AdslDrvPrintf(TEXT("AdslCoreHwReset:  Failed to align ADSL Phy clock\n"));
				return AC_FALSE;
			}
		}
#endif

#ifdef LMEM_ACCESS_WORKAROUND
		if( (0x632680a0 == PERF->RevID) || (0x632680b0 == PERF->RevID) ) {
			AdslCoreLoadImage();	/* Needed to get the SDRAM base address */
			AdslCoreRunPhyMipsInSDRAM();
		}
#endif
		if (!(tmp = AdslCoreLoadImage())) {
			AdslDrvPrintf(TEXT("AdslCoreHwReset:  Failed to load ADSL PHY image (%ld)\n"), tmp);
			return AC_FALSE;
		}
	}

	{
	extern void FlattenBufferClearStat(void);
	FlattenBufferClearStat();
	}
	
	AdslCoreSetSdramTrueSize();
	AdslCoreSharedMemInit();
	pAdslX = (AdslXfaceData *) ADSL_LMEM_XFACE_DATA;
	BlockByteClear (sizeof(AdslXfaceData), (void *)pAdslX);
	pAdslX->sdramBaseAddr = (void *) adslCorePhyDesc.sdramImageAddr;

	for(tmp = 0; tmp < MAX_DSL_LINE; tmp++)
		acAhifStatePtr[tmp] = NULL;

	/* now take ADSL core MIPS out of reset */
#ifdef VXWORKS
	if (ejtagEnable) {
		int *p = (int *)0xfff0001c;
		*p = 1;
	}
#endif

	pAdslXface = (AdslXfaceData *) pAdslX;
	pAdslOemData = NULL;

#ifdef SUPPORT_STATUS_BACKUP
	if(NULL == pLocSbSta) {
		pLocSbSta = calloc(1, sizeof(stretchBufferStruct)+STAT_BKUP_BUF_SIZE+kMaxFlattenedStatusSize);
		if(pLocSbSta)
			FlattenBufferInit(pLocSbSta, pLocSbSta+sizeof(stretchBufferStruct), STAT_BKUP_BUF_SIZE, kMaxFlattenedStatusSize);
		else
			AdslDrvPrintf(TEXT("AdslCoreHwReset:  Failed to calloc for pLocSbSta\n"));	/* No back up status buffer */
	}
	pCurSbSta = &pAdslXface->sbSta;
#endif
	
	/* clear and enable interrupts */

	tmp = pAdslEnum[ADSL_INTSTATUS_I];
	pAdslEnum[ADSL_INTSTATUS_I] = tmp;
	tmp = pAdslEnum[ADSL_INTMASK_I];
	pAdslEnum[ADSL_INTMASK_I] = tmp & ~MSGINT_MASK_BIT;
	
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	tmp = pAdslEnum[ADSL_INTSTATUS_F];
	pAdslEnum[ADSL_INTSTATUS_F] = tmp;
	pAdslEnum[ADSL_INTMASK_F] = 0;
#endif

#ifdef PHY_BLOCK_TEST
	memMonitorIdIntialVal = pAdslLmem[MEM_MONITOR_ID];
#endif
	setGdbOn();
#ifdef CONFIG_VDSL_SUPPORTED
	if(afe6306ChipUsed) {
		XdslCoreHwReset6306();
		SetupReferenceClockTo6306();
		XdslCoreHwReset6306();
#ifdef CONFIG_BCM96368	/* For the 63268, this will be done in PHY */
		PLLPowerUpSequence6306();
		XdslCoreHwReset6306();
#endif
#ifdef CONFIG_BCM963268
		/* PHY control Line Driver mode internally, except for bonding PHY under 63268A0/63268B0 chip, will pass
		line1 LD control to the Host through a status.  For Diags downloaded PHY, put back the default(control from PHY)
		in case this downloaded PHY will control the LD mode internally, such as non bonding PHY */
		if( !bCoreReset && ((0x632680a0 == PERF->RevID) || (0x632680b0 == PERF->RevID)) )
			GPIO->GPIOBaseMode |= GPIO_BASE_VDSL_PHY_OVERRIDE_1;
#endif
	}
#endif
#if defined(LMEM_ACCESS_WORKAROUND)
	if( (0x632680a0 == PERF->RevID) || (0x632680b0 == PERF->RevID) ) {
		DISABLE_ADSL_MIPS;
		pXdslControlReg[0] &= ~ALT_BOOT_VECTOR_MASK;
		pXdslControlReg[0] |= ALT_BOOT_VECTOR_LMEM;
	}
#endif
	ENABLE_ADSL_MIPS;

#ifdef VXWORKS
	if (ejtagEnable) {
		int read(int fd, void *buf, int n);

		int *p = (int *)0xfff0001c;
		char ch[16];
		printf("Enter any key (for EJTAG)\n");
		read(0, ch, 1);
		*p = 0;
	}
#endif

	/* wait for ADSL core to initialize */

	cycleCnt0 = BcmAdslCoreGetCycleCount();
	to = TmElapsedUs(cycleCnt0);
#ifdef PHY_BLOCK_TEST
	AdslDrvPrintf(TEXT("**** BLOCK TEST BUILD ***\n"));
	while (!AdsCoreStatBufAssigned() && ((to=TmElapsedUs(cycleCnt0)) < HW_RESET_DELAY) &&
		((memMonitorIdIntialVal==pAdslLmem[MEM_MONITOR_ID] ) || (0==pAdslLmem[MEM_MONITOR_ID])));
#else
	while (!AdsCoreStatBufAssigned() && ((to = TmElapsedUs(cycleCnt0)) < HW_RESET_DELAY));
#endif

#ifdef PHY_BLOCK_TEST
	if ((memMonitorIdIntialVal==pAdslLmem[MEM_MONITOR_ID]) ||
		(0==pAdslLmem[MEM_MONITOR_ID]))	/* GLOBAL_EVENT_ZERO_BSS */
#else
	if (!AdsCoreStatBufAssigned())
#endif
	{
#ifdef PHY_BLOCK_TEST
		if(AdsCoreStatBufAssigned())
			AdslDrvPrintf(TEXT("AdslCoreHwReset:  Non-blocktest PHY is running\n"));
		else
			AdslDrvPrintf(TEXT("AdslCoreHwReset:  No PR received from PHY in %lu uSec\n"), to);
		AdslDrvPrintf(TEXT("AdslCoreHwReset:  pAdslLmem[MEM_MONITOR_ID]=0x%lX\n"), pAdslLmem[MEM_MONITOR_ID]);
#else
		AdslDrvPrintf(TEXT("AdslCoreHwReset:  ADSL PHY initialization timeout %lu uSec\n"), to);
		DISABLE_ADSL_MIPS;
		return AC_FALSE;
#endif
	}

#ifdef SUPPORT_STATUS_BACKUP
#ifdef PHY_BLOCK_TEST
	if(AdsCoreStatBufAssigned())	/* Non-blocktest PHY is running */
		gStatusBackupThreshold = MIN( STAT_BKUP_THRESHOLD, (StretchBufferGetSize(&pAdslXface->sbSta) >> 1));
#else
	gStatusBackupThreshold = MIN( STAT_BKUP_THRESHOLD, (StretchBufferGetSize(&pAdslXface->sbSta) >> 1));
#endif
	AdslDrvPrintf(TEXT("AdslCoreHwReset:  pLocSbSta=%x bkupThreshold=%d\n"), (uint)pLocSbSta, gStatusBackupThreshold);
#endif

#ifdef PHY_BLOCK_TEST
	if(AdsCoreStatBufAssigned())	/* Non-blocktest PHY is running */
#endif
	{
		dslStatusStruct status;
		dslStatusCode	statusCode;
		
		cycleCnt0 = BcmAdslCoreGetCycleCount();

		do {
			status.code = 0;
			statusCode = 0;
			if ((tmp = AdslCoreStatusRead (&status)) != 0) {
				statusCode = DSL_STATUS_CODE(status.code);
				AdslCoreStatusReadComplete (tmp);

				if (kDslOemDataAddrStatus == statusCode) {
					AdslDrvPrintf(TEXT("AdslCoreHwReset:  AdslOemDataAddr = 0x%lX\n"), status.param.value);
					AdslCoreProcessOemDataAddrMsg(&status);
					break;
				}
			}
		} while ((to = TmElapsedUs(cycleCnt0)) < HW_RESET_DELAY);
		
		if (kDslOemDataAddrStatus != statusCode) {
			AdslDrvPrintf(TEXT("AdslCoreHwReset:  ADSL PHY OemData status read timeout %lu usec\n"), to);
			DISABLE_ADSL_MIPS;
			return AC_FALSE;
		}
	}

	tmp = pAdslEnum[ADSL_INTMASK_I];
	pAdslEnum[ADSL_INTMASK_I] = tmp | MSGINT_MASK_BIT;

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
#if defined(CONFIG_BCM96368)
	if(afe6306ChipUsed) {
		if(ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) {
			XdslCoreSetExtAFEClk(VDSL_AFE_BONDING_100MHZ_CLK);
			XdslCoreSetExtAFELDMode(LD_MODE_VDSL);
		}
		else {
			XdslCoreSetExtAFEClk(VDSL_AFE_BONDING_64MHZ_CLK);
			XdslCoreSetExtAFELDMode(LD_MODE_ADSL);
		}
	}
	/* This is hack for 96368.
	 * Interrupt from PHY indicating to GDB that breakpoint is hit 
	 * is lost while getting status after PHY code download. It generated again.
	 */
	if (isGdbOn())
		pAdslEnum[ADSL_Core2HostMsg] = 0x1234;
#endif /* CONFIG_BCM96368 */
#ifdef PHY_BLOCK_TEST
	if(AdsCoreStatBufAssigned())	/* Non-blocktest PHY is running */
#endif
	{
	BcmXdslCoreSendAfeInfo(1);	/* Send Afe info to PHY */
	BcmAdslDiagSendHdr();		/* Send DslDiag Hdr to PHY */
	}
#endif
	BcmXdslCoreDiagSendPhyInfo();	/* Send PHY info to DslDiag */

#endif /* BCM_CORE_NO_HARDWARE */

	return AC_TRUE;
}

AC_BOOL AdslCoreHwReset(AC_BOOL bCoreReset)
{
	int		cnt = 0;

	do {
		if (__AdslCoreHwReset(bCoreReset))
			return AC_TRUE;
		cnt++;
	} while (cnt < 3);
	
	return AC_FALSE;
}

AC_BOOL AdslCoreInit(void)
{
	dslVarsStruct	*pDslVars;
	unsigned char	lineId;
	
#ifndef BCM_CORE_NO_HARDWARE
	if (NULL != pAdslXface)
		return AC_FALSE;
#endif

	for(lineId = 0; lineId < MAX_DSL_LINE; lineId++) {
	pDslVars = (dslVarsStruct *)XdslCoreGetDslVars(lineId);
	pDslVars->lineId = lineId;
	pDslVars->timeUpdate = 0;
	pDslVars->pendingFrFlag = 0;
	pDslVars->xdslCoreOvhMsgPrintEnabled = AC_FALSE;
	pDslVars->timeInL2Ms = 0;
	pDslVars->xdslCoreShMarginMonEnabled = AC_FALSE;
	pDslVars->xdslCoreLOMTimeout = (ulong)-1;
	pDslVars->xdslCoreLOMTime = -1;
	pDslVars->dataPumpCommandHandlerPtr = AdslCoreCommonCommandHandler;
	pDslVars->externalStatusHandlerPtr  = AdslCoreStatusHandler;
	DslFrameAssignFunctions (pDslVars->DslFrameFunctions, DslFrameNative);
	AdslCoreAssignDslFrameFunctions (pDslVars->DslFrameFunctions);
	acAhifStatePtr[lineId] = NULL;
	
#ifdef G997_1_FRAMER
	{
		upperLayerFunctions	g997UpperLayerFunctions;

		AdslCoreG997Init(lineId);
		g997UpperLayerFunctions.rxIndicateHandlerPtr = AdslCoreG997IndicateRecevice;
		g997UpperLayerFunctions.txCompleteHandlerPtr = AdslCoreG997SendComplete;
		g997UpperLayerFunctions.statusHandlerPtr = AdslCoreG997StatusHandler;
		
		G997Init((void *)pDslVars,0,  32, 128, 8, &g997UpperLayerFunctions, AdslCoreG997CommandHandler);
	}
#endif

#ifdef ADSL_MIB
	AdslMibInit((void *)pDslVars,AdslCoreCommonCommandHandler);
	AdslMibSetNotifyHandler((void *)pDslVars, AdslCoreMibNotify);
#endif

#ifdef G992P3
#ifdef G992P3_DEBUG
	G992p3OvhMsgInit((void *)pDslVars, 0, G997ReturnFrame, TstG997SendFrame, __AdslCoreG997SendComplete, AdslCoreCommonCommandHandler, AdslCoreCommonStatusHandler);
#else
	G992p3OvhMsgInit((void *)pDslVars, 0, G997ReturnFrame, G997SendFrame, __AdslCoreG997SendComplete, AdslCoreCommonCommandHandler, AdslCoreCommonStatusHandler);
#endif
#endif

#ifdef ADSL_SELF_TEST
	{
	ulong	stRes;

	AdslSelfTestRun(adslCoreSelfTestMode);
	stRes = AdslCoreGetSelfTestResults();
	AdslDrvPrintf (TEXT("AdslSelfTestResults = 0x%X %s\n"), stRes, 
			(stRes == (kAdslSelfTestCompleted | adslCoreSelfTestMode)) ? "PASSED" : "FAILED");
	}
#endif
	}
	
	AdslCoreSetPllClock();
	
	return AdslCoreHwReset(AC_TRUE);
}

void AdslCoreUninit(void)
{
#ifdef G997_1_FRAMER
	void *pDslVars;
	unsigned char lineId;
#endif
#ifndef BCM_CORE_NO_HARDWARE
	DISABLE_ADSL_MIPS;
	DISABLE_ADSL_CORE;
#endif

#ifdef G997_1_FRAMER
	for (lineId = 0; lineId < MAX_DSL_LINE; lineId++) {
		pDslVars = XdslCoreGetDslVars(lineId);
		G997Close(pDslVars);
	}
#endif

	pAdslXface = NULL;
}

AC_BOOL AdslCoreSetSDRAMBaseAddr(void *pAddr)
{
	if (NULL != pAdslXface)
		return AC_FALSE;

	adslCorePhyDesc.sdramPageAddr = 0xA0000000 | (((ulong) pAddr) & ~(ADSL_PHY_SDRAM_PAGE_SIZE-1));
	
	return AC_TRUE;
}

void * AdslCoreGetSDRAMBaseAddr()
{
	return (void *) adslCorePhyDesc.sdramPageAddr;
}

AC_BOOL AdslCoreSetVcEntry (int gfc, int port, int vpi, int vci, int pti_clp)
{
	dslCommandStruct	cmd;

	if ((NULL == pAdslXface) || (gfc <= 0) || (gfc > 15))
		return AC_FALSE;

#ifndef  ADSLDRV_LITTLE_ENDIAN
	pAdslXface->gfcTable[gfc-1] = (vpi << 20) | ((vci & 0xFFFF) << 4) | (pti_clp & 0xF);
#else
	{
	ulong	tmp = (vpi << 20) | ((vci & 0xFFFF) << 4) | (pti_clp & 0xF);
	pAdslXface->gfcTable[gfc-1] = ADSL_ENDIAN_CONV_LONG(tmp);
	}
#endif

	cmd.command = kDslAtmVcMapTableChanged;
	cmd.param.value = gfc;
	AdslCoreCommandHandler(&cmd);

	return AC_TRUE;
}

void AdslCoreSetStatusCallback(void *pCallback)
{
	if (NULL == pCallback)
		pAdslCoreStatusCallback = AdslCoreIdleStatusCallback;
	else
		pAdslCoreStatusCallback = pCallback;
}

void *AdslCoreGetStatusCallback(void)
{
	return (AdslCoreIdleStatusCallback == pAdslCoreStatusCallback ? NULL : pAdslCoreStatusCallback);
}

void AdslCoreSetShowtimeMarginMonitoring(uchar lineId, uchar monEnabled, long showtimeMargin, long lomTimeSec)
{
	void *pDslVars = XdslCoreGetDslVars(lineId);
	
	AdslMibSetShowtimeMargin(pDslVars, showtimeMargin);
	gXdslCoreShMarginMonEnabled(pDslVars) = (monEnabled != 0);
	gXdslCoreLOMTimeout(pDslVars) = ((ulong) lomTimeSec) * 1000;
}

void AdslCoreG997FrameBufferFlush(void *gDslVars)
{
	char* frameReadPtr;
	int bufLen;
	uchar lineId = gLineId(gDslVars);
	
	frameReadPtr=(char*)AdslCoreG997FrameGet(lineId, &bufLen);
	
	while(frameReadPtr!=NULL){
		AdslCoreG997ReturnFrame(gDslVars);
		frameReadPtr=(char*)AdslCoreG997FrameGet(lineId, &bufLen);
		BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), "Line %d: G997Frame returned to free-pool as it was not being read", lineId);
	}
	gTimeUpdate(gDslVars) = 0;
	gPendingFrFlag(gDslVars) = 0;
}

void AdslCoreTimer (ulong tMs)
{
	int		i;
	void		*pDslVars;
	
	for(i = 0; i < MAX_DSL_LINE; i++) {
		if ((1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;
		pDslVars = XdslCoreGetDslVars((uchar)i);
		if(gPendingFrFlag(pDslVars)){
			gTimeUpdate(pDslVars)+=tMs;
			if (gTimeUpdate(pDslVars) > 5000)
				AdslCoreG997FrameBufferFlush(pDslVars);
		}
	}
	
	if (pSdramReserved->timeCnt < ADSL_INIT_TIME) {
		pSdramReserved->timeCnt += tMs;
		if (pSdramReserved->timeCnt >= ADSL_INIT_TIME) {
			dslCommandStruct	cmd;

			adslCoreEcUpdateMask |= kDigEcShowtimeFastUpdateDisabled;
			BcmAdslCoreDiagWriteStatusString (0, "AdslCoreEcUpdTmr: timeMs=%d ecUpdMask=0x%X\n", pSdramReserved->timeCnt, adslCoreEcUpdateMask);

			cmd.command = kDslSetDigEcShowtimeUpdateModeCmd;
			cmd.param.value = kDslSetDigEcShowtimeUpdateModeSlow;
			AdslCoreCommandHandler(&cmd);
		}
	}
	
#ifdef G997_1_FRAMER
	G997Timer(XdslCoreGetDslVars(0), tMs);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		G997Timer(XdslCoreGetDslVars(1), tMs);
#endif
#endif

#ifdef ADSL_MIB
	for(i = 0; i < MAX_DSL_LINE; i++) {
		if ((1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;
		pDslVars = XdslCoreGetDslVars((uchar)i);
		AdslMibTimer(pDslVars, tMs);
		if (gXdslCoreLOMTime(pDslVars) >= 0)
			gXdslCoreLOMTime(pDslVars) += tMs;
	}
#endif

#ifdef G992P3
	for(i = 0; i < MAX_DSL_LINE; i++) {
		if ((1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;
		pDslVars = XdslCoreGetDslVars((uchar)i);
		if (AdslCoreOvhMsgSupported(pDslVars))
			G992p3OvhMsgTimer(pDslVars, tMs);
	}
#endif

#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	gTimeToWarn += tMs;
	if( gTimeToWarn > kMemPrtyWarnTime ) {
		gTimeToWarn = 0;
		if( (*(ulong *)kMemPrtyRegAddr &  kMemEnPrtySetMsk) != kMemEnPrtySetMsk ) {
			AdslDrvPrintf(TEXT("AdslDrv: WARNING!!! Memory Priority is not correctly set!\n"));
			__SoftDslPrintf(pDslVars,"AdslDrv: WARNING!!! Memory Priority is not correctly set!\n",0);
		}
	}
#endif

	for(i = 0; i < MAX_DSL_LINE; i++) {
		if ((1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;
		pDslVars = XdslCoreGetDslVars((uchar)i);
		if( 2 == AdslMibPowerState(pDslVars) ) {
			gTimeInL2Ms(pDslVars) += tMs;
			if(gTimeInL2Ms(pDslVars) >= gL2SwitchL0TimeMs) {
#ifdef DEBUG_L2_RET_L0
				ulong cTime;
				bcmOsGetTime(&cTime);
				printk("line: %d L2 -> L0: %d Sec\n", i, ((cTime-gL2Start)*BCMOS_MSEC_PER_TICK)/1000);
#endif			
				AdslCoreSetL0((uchar)i);
				gTimeInL2Ms(pDslVars) = 0;
			}
		}
	}
}

AC_BOOL AdslCoreIntHandler(void)
{
#ifndef BCM_CORE_NO_HARDWARE
	{
		volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
		ulong		tmp;

		tmp = pAdslEnum[ADSL_INTSTATUS_I];
		pAdslEnum[ADSL_INTSTATUS_I] = tmp;
		
		if( !(tmp & MSGINT_BIT) )
			return 0;
	}
#endif

	return AdslCoreStatusAvail ();
}

extern Bool	gSharedMemAllocFromUserContext;
static ulong	lastStatTimeMs = 0;
#ifdef SDRAM_HOLD_COUNTERS
static ulong	lastSdramHoldTimeMs = 0;
#endif

void AdslCoreSetTime(ulong timeMs)
{
	lastStatTimeMs = timeMs;
}

void Adsl2UpdateCOErrCounter(dslStatusStruct *status)
{
  adslMibInfo *pMib;
  ulong mibLen;
  ulong *counters=(ulong *)( status->param.dslConnectInfo.buffPtr);

  mibLen=sizeof(adslMibInfo);
  pMib=(void *) AdslMibGetObjectValue(XdslCoreGetCurDslVars(),NULL,0,NULL,&mibLen);
  counters[kG992ShowtimeNumOfFEBE]=pMib->adslStat.xmtStat.cntSFErr;
  counters[kG992ShowtimeNumOfFECC]=pMib->adslStat.xmtStat.cntRSCor;
  counters[kG992ShowtimeNumOfFHEC]=pMib->atmStat.xmtStat.cntHEC;
}

#if defined(SUPPORT_STATUS_BACKUP) || defined(ADSLDRV_STATUS_PROFILING)
extern int	gByteProc;
#endif
int AdslCoreIntTaskHandler(int nMaxStatus)
{
	ulong			tmp, tMs;
	dslStatusStruct	status;
	dslStatusCode	statusCode;
	int				mibRetCode = 0, nStatProcessed = 0;
	void			*gDslVars;
	uchar			lineId;

	tMs = lastStatTimeMs;
	
#if defined(CONFIG_SMP) && defined(__KERNEL__)
	BcmCoreDpcSyncEnter();
#endif

	if (!AdslCoreCommandIsPending() && (0 == gSharedMemAllocFromUserContext))
		AdslCoreSharedMemFree(NULL);

	while ((tmp = AdslCoreStatusRead (&status)) > 0) {
#if defined(SUPPORT_STATUS_BACKUP) || defined(ADSLDRV_STATUS_PROFILING)
		gByteProc += tmp;
#endif
		statusCode = DSL_STATUS_CODE(status.code);
#ifdef SUPPORT_DSL_BONDING
		lineId = DSL_LINE_ID(status.code);
		gDslVars = XdslCoreSetCurDslVars(lineId);
#else
		lineId = 0;
		gDslVars = XdslCoreGetCurDslVars();
#endif

#if defined(__KERNEL__) && (!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
		if(kDslResetPhyReqStatus == statusCode) {
#if defined(CONFIG_SMP) && defined(__KERNEL__)
			BcmCoreDpcSyncExit();
#endif
			BcmAdslCoreResetPhy(status.param.value);
			return 0;
		}
		else if( kDslRequestDrvConfig == statusCode) {
			XdslCoreProcessDrvConfigRequest(lineId, ADSL_ADDR_TO_HOST(status.param.value));
			AdslCoreStatusReadComplete (tmp);
			continue;
		}
#ifdef CONFIG_BCM_PWRMNGT_DDR_SR_API
		else if( kDslPwrMgrSrAddrStatus == statusCode )
			BcmPwrMngtRegisterLmemAddr(ADSL_ADDR_TO_HOST(status.param.value));
#endif
#endif

		if (XdslMibIsXdsl2Mod(gDslVars) && (statusCode == kDslConnectInfoStatus) && (status.param.dslConnectInfo.code == kG992ShowtimeMonitoringStatus))
			Adsl2UpdateCOErrCounter(&status);

		tMs = (*pAdslCoreStatusCallback) (&status, AdslCoreStatusReadPtr(), tmp-4);
		
#ifdef ADSL_MIB
		mibRetCode = AdslMibStatusSnooper(gDslVars, &status);

		if (gXdslCoreShMarginMonEnabled(gDslVars)) {
			adslMibInfo		*pMibInfo;
			ulong			mibLen;
			dslCommandStruct	cmd;
			
			mibLen = sizeof(adslMibInfo);
			pMibInfo = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
			
			if (pMibInfo->adslPhys.adslCurrStatus & kAdslPhysStatusLOM) {
				/* AdslDrvPrintf (TEXT("LOM detected = 0x%lX\n"), pMibInfo->adslPhys.adslCurrStatus); */
				if (gXdslCoreLOMTime(gDslVars) < 0)
					gXdslCoreLOMTime(gDslVars) = 0;
				if (gXdslCoreLOMTime(gDslVars) > gXdslCoreLOMTimeout(gDslVars)) {
					gXdslCoreLOMTime(gDslVars) = -1;
#ifdef SUPPORT_DSL_BONDING
					cmd.command = kDslStartRetrainCmd | (lineId << DSL_LINE_SHIFT);
#else
					cmd.command = kDslStartRetrainCmd;
#endif
					AdslCoreCommandHandler(&cmd);
				}
			}
			else
				gXdslCoreLOMTime(gDslVars) = -1;
		}
#endif
#ifdef G997_1_FRAMER
		G997StatusSnooper (gDslVars, &status);
#endif
#ifdef G992P3
		if (AdslCoreOvhMsgSupported(gDslVars))
			G992p3OvhMsgStatusSnooper (gDslVars, &status);
#endif

		switch (statusCode) {
			case kDslEscapeToG994p1Status:
				break;

			case kDslOemDataAddrStatus:
				BcmAdslCoreDiagWriteStatusString(0, "Regular AdslOemDataAddr = 0x%lX\n", status.param.value);
				AdslCoreProcessOemDataAddrMsg(&status);
				break;

			case kAtmStatus:
				if (kAtmStatBertResult == status.param.atmStatus.code) {
					ulong	nBits;

					nBits = AdslMibBertContinueEx(gDslVars, 
						status.param.atmStatus.param.bertInfo.totalBits,
						status.param.atmStatus.param.bertInfo.errBits);
					if (nBits != 0)
						AdslCoreBertStart(gLineId(gDslVars), nBits);
				}
				break;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
			case kDslReceivedEocCommand:
				if (kDslVectoringErrorSamples == status.param.dslClearEocMsg.msgId)
					BcmAdslCoreDiagWriteStatusString(lineId, "AdslCore: ErrorSamples: %d", mibRetCode);
				if ((kDslVectoringErrorSamples == status.param.dslClearEocMsg.msgId) && (mibRetCode != 0))
					bcmOsWakeupMonitorTask();
				break;
#endif
		}
		
		AdslCoreStatusReadComplete (tmp);

		if( ++nStatProcessed >= nMaxStatus )
			break;
	}

	if (tMs != lastStatTimeMs) {
		AdslCoreTimer(tMs - lastStatTimeMs);
		lastStatTimeMs = tMs;
	}

#ifdef SDRAM_HOLD_COUNTERS
	if ((tMs - lastSdramHoldTimeMs) > 2000) {
		BcmAdslCoreDiagWriteStatusString(0, "SDRAM Hold: Time=%d ms, HoldEvents=%d, HoldTime=%d\n"), 
			tMs - lastSdramHoldTimeMs,
			AdslCoreReadtSdramHoldEvents(),
			AdslCoreReadtSdramHoldTime());
		AdslCoreStartSdramHoldCounters();
		lastSdramHoldTimeMs = tMs;
	}
#endif

#if defined(CONFIG_SMP) && defined(__KERNEL__)
	BcmCoreDpcSyncExit();
#endif

	return nStatProcessed;

}

AC_BOOL XdslCoreIs2lpActive(unsigned char lineId, unsigned char direction)
{
	return XdslMibIs2lpActive(XdslCoreGetDslVars(lineId), direction);
}

AC_BOOL XdslCoreIsVdsl2Mod(unsigned char lineId)
{
	return XdslMibIsVdsl2Mod(XdslCoreGetDslVars(lineId));
}

AC_BOOL XdslCoreIsXdsl2Mod(unsigned char lineId)
{
	return XdslMibIsXdsl2Mod(XdslCoreGetDslVars(lineId));
}
AC_BOOL AdslCoreLinkState (unsigned char lineId)
{
	return AdslMibIsLinkActive(XdslCoreGetDslVars(lineId));
}

int	AdslCoreLinkStateEx (unsigned char lineId)
{
	return AdslMibTrainingState(XdslCoreGetDslVars(lineId));
}

#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
AC_BOOL AdslCoreGetErrorSampleStatus (unsigned char lineId) {
  return AdslMibGetErrorSampleStatus(XdslCoreGetDslVars(lineId));
}
#endif

void AdslCoreGetConnectionRates (unsigned char lineId, AdslCoreConnectionRates *pAdslRates)
{
    void *pDslVars = XdslCoreGetDslVars(lineId);
    pAdslRates->fastUpRate = AdslMibGetGetChannelRate (pDslVars, kAdslXmtDir, kAdslFastChannel);
    pAdslRates->fastDnRate = AdslMibGetGetChannelRate (pDslVars, kAdslRcvDir, kAdslFastChannel);
    pAdslRates->intlUpRate = AdslMibGetGetChannelRate (pDslVars, kAdslXmtDir, kAdslIntlChannel);
    pAdslRates->intlDnRate = AdslMibGetGetChannelRate (pDslVars, kAdslRcvDir, kAdslIntlChannel);

#if !defined(__KERNEL__) && !defined(TARG_OS_RTEMS) && !defined(_CFE_)
    if (0 == pAdslRates->intlDnRate) {
            pAdslRates->intlDnRate = pAdslRates->fastDnRate;
            pAdslRates->fastDnRate = 0;
    }
    if (0 == pAdslRates->intlUpRate) {
            pAdslRates->intlUpRate = pAdslRates->fastUpRate;
            pAdslRates->fastUpRate = 0;
    }
#endif
}

void AdslCoreSetNumTones (uchar lineId, int numTones)
{
	AdslMibSetNumTones(XdslCoreGetDslVars(lineId), numTones);
}

int	AdslCoreSetObjectValue (uchar lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	return AdslMibSetObjectValue (XdslCoreGetDslVars(lineId), objId, objIdLen, dataBuf, dataBufLen);
}

int	AdslCoreGetObjectValue (uchar lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	return AdslMibGetObjectValue (XdslCoreGetDslVars(lineId), objId, objIdLen, dataBuf, dataBufLen);
}

AC_BOOL AdslCoreBertStart(uchar lineId, ulong totalBits)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslDiagStartBERT | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslDiagStartBERT;
#endif
	cmd.param.value = totalBits;
	return AdslCoreCommandHandler(&cmd);
}

AC_BOOL AdslCoreBertStop(uchar lineId)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslDiagStopBERT | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslDiagStopBERT;
#endif
	return AdslCoreCommandHandler(&cmd);
}

void AdslCoreBertStartEx(uchar lineId, ulong bertSec)
{
	ulong nBits;

	void *pDslVars = XdslCoreGetDslVars(lineId);
	
	AdslMibBertStartEx(pDslVars, bertSec);
	nBits = AdslMibBertContinueEx(pDslVars, 0, 0);
	if (nBits != 0)
		AdslCoreBertStart(lineId, nBits);
}

void AdslCoreBertStopEx(uchar lineId)
{
	AdslMibBertStopEx(XdslCoreGetDslVars(lineId));
}

void AdslCoreResetStatCounters(uchar lineId)
{
#ifdef ADSL_MIB
	AdslMibResetConectionStatCounters(XdslCoreGetDslVars(lineId));
#endif
}


void * AdslCoreGetOemParameterData (int paramId, int **ppLen, int *pMaxLen)
{
	int		maxLen = 0,i;
	ulong	*pLen = NULL;
	char	*pData = NULL;

	switch (paramId) {
		case kDslOemG994VendorId:
			pLen = &pAdslOemData->g994VendorIdLen;
			maxLen = kAdslOemVendorIdMaxSize;
			pData = pAdslOemData->g994VendorId;
			__SoftDslPrintf(XdslCoreGetCurDslVars(),"AdslCoreGetOemParameter paramId:%d pLen:%d",0,paramId,pAdslOemData->g994VendorIdLen);
			for(i=0;i<*pLen;i++)
				__SoftDslPrintf(XdslCoreGetCurDslVars(),"AdslCoreGetOemParameter pData[i]=%c",0,pData[i]);
			break;
		case kDslOemG994XmtNSInfo:
			pLen = &pAdslOemData->g994XmtNonStdInfoLen;
			maxLen = kAdslOemNonStdInfoMaxSize;
			pData = pAdslOemData->g994XmtNonStdInfo;
			break;
		case kDslOemG994RcvNSInfo:
			pLen = &pAdslOemData->g994RcvNonStdInfoLen;
			maxLen = kAdslOemNonStdInfoMaxSize;
			pData = pAdslOemData->g994RcvNonStdInfo;
			break;
		case kDslOemEocVendorId:
			pLen = &pAdslOemData->eocVendorIdLen;
			maxLen = kAdslOemVendorIdMaxSize;
			pData = pAdslOemData->eocVendorId;
			break;
		case kDslOemEocVersion:
			pLen = &pAdslOemData->eocVersionLen;
			maxLen=  kAdslOemVersionMaxSize;
			pData = pAdslOemData->eocVersion;
			break;
		case kDslOemEocSerNum:
			pLen = &pAdslOemData->eocSerNumLen;
			maxLen= kAdslOemSerNumMaxSize;
			pData = pAdslOemData->eocSerNum;
			break;
		case kDslOemT1413VendorId:
			pLen = &pAdslOemData->t1413VendorIdLen;
			maxLen = kAdslOemVendorIdMaxSize;
			pData = pAdslOemData->t1413VendorId;
			break;
		case kDslOemT1413EocVendorId:
			pLen = &pAdslOemData->t1413EocVendorIdLen;
			maxLen = kAdslOemVendorIdMaxSize;
			pData = pAdslOemData->t1413EocVendorId;
			break;

	}

	*ppLen = (int *)pLen;
	*pMaxLen = maxLen;
	return pData;
}

int AdslCoreGetOemParameter (int paramId, void *buf, int len)
{
	int		maxLen, paramLen,i;
	int		*pLen;
	char	*pData;

	if (NULL == pAdslOemData)
		return 0;

	pData = AdslCoreGetOemParameterData (paramId, &pLen, &maxLen);
	__SoftDslPrintf(XdslCoreGetCurDslVars(),"AdslCoreGetOemParameter paramId:%d pLen:%d",0,paramId,*pLen);
	for(i=0;i<*pLen;i++)
		__SoftDslPrintf(XdslCoreGetCurDslVars(),"AdslCoreGetOemParameter pData[i]=%c",0,pData[i]);
	if (NULL == buf)
		return maxLen;

	paramLen = (NULL != pLen ? *pLen : 0);
	if (len > paramLen)
		len = paramLen;
	if (len > 0)
		memcpy (buf, pData, len);
	return len;
}

int AdslCoreSetOemParameter (int paramId, void *buf, int len)
{
	int		maxLen;
	int		*pLen;
	char	*pData;
	char    *str;
	str=(char *)buf;
	__SoftDslPrintf(XdslCoreGetCurDslVars(),"AdslCoreSetOemParameter paramId=%d len=%d buf=%c%c..%c%c",0,paramId,len,*str,*(str+1),*(str+len-2),*(str+len-1));
	if (NULL == pAdslOemData)
		return 0;

	pData = AdslCoreGetOemParameterData (paramId, &pLen, &maxLen);
	if (len > maxLen)
		len = maxLen;
	if (len > 0)
		memcpy (pData, buf, len);
	*pLen = len;
	adslOemDataModified = AC_TRUE;
	return len;
}

char * AdslCoreGetVersionString(void)
{
	return ADSL_ADDR_TO_HOST(pAdslOemData->gDslVerionStringPtr);
}

char * AdslCoreGetBuildInfoString(void)
{
	return ADSL_ADDR_TO_HOST(pAdslOemData->gDslBuildDataStringPtr);
}

/*
**
**		ADSL_SELF_TEST functions
**
*/

int  AdslCoreGetSelfTestMode(void)
{
	return adslCoreSelfTestMode;
}

void AdslCoreSetSelfTestMode(int stMode)
{
	adslCoreSelfTestMode = stMode;
}

#ifdef ADSL_SELF_TEST

int  AdslCoreGetSelfTestResults(void)
{
	return AdslSelfTestGetResults();
}

#else  /* ADSL_SELF_TEST */

int  AdslCoreGetSelfTestResults(void)
{
	return adslCoreSelfTestMode | kAdslSelfTestCompleted;
}

#endif /* ADSL_SELF_TEST */

