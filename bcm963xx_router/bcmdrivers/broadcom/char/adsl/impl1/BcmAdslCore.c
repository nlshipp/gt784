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
 * BcmCoreTest.c -- Bcm ADSL core driver main
 *
 * Description:
 *	This file contains BCM ADSL core driver system interface functions
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.12 $
 *
 * $Id: BcmAdslCore.c,v 1.12 2011/08/05 23:28:43 dkhoo Exp $
 *
 * $Log: BcmAdslCore.c,v $
 * Revision 1.12  2011/08/05 23:28:43  dkhoo
 * [All]modify driver to quicken datapump switching time, save nvram when datapump successfuly train line
 *
 * Revision 1.11  2011/07/07 05:58:31  lzhang
 * Phy-R enabled for ADSL2+ and VDSL2 for both downstream and upstream.
 *
 * Revision 1.10  2011/06/24 01:52:39  dkhoo
 * [All]Per PLM, update DSL driver to 23j and bonding datapump to Av4bC035d
 *
 * Revision 1.6  2004/07/20 23:45:48  ilyas
 * Added driver version info, SoftDslPrintf support. Fixed G.997 related issues
 *
 * Revision 1.5  2004/06/10 00:20:33  ilyas
 * Added L2/L3 and SRA
 *
 * Revision 1.4  2004/04/30 17:58:01  ilyas
 * Added framework for GDB communication with ADSL PHY
 *
 * Revision 1.3  2004/04/28 20:30:38  ilyas
 * Test code for GDB frame processing
 *
 * Revision 1.2  2004/04/12 23:20:03  ilyas
 * Merged RTEMS changes
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

#ifdef VXWORKS
#define RTOS	VXWORKS
#endif

#ifdef _WIN32_WCE
#include <windows.h>
#include <types.h>
#endif

#include "softdsl/AdslCoreDefs.h"

#if defined(_CFE_)
#include "lib_types.h"
#include "lib_string.h"
void BcmAdslCoreGdbTask(void){}
void BcmAdslCoreGdbCmd(void *pCmdData, int len){}
#endif

/* Includes. */
#ifdef __KERNEL__
#include <linux/interrupt.h>
#include <linux/version.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define	LINUX_KERNEL_2_6
#endif
#endif /* __KERNEL__ */

#include "BcmOs.h"
#if defined(CONFIG_BCM96338)
#include "6338_common.h"
#include "6338_map.h"
#define	XDSL_CLK_EN	ADSL_CLK_EN
#elif defined(CONFIG_BCM96348)
#include "6348_common.h"
#include "6348_map.h"
#elif defined(CONFIG_BCM96358)
#include "6358_common.h"
#include "6358_map.h"
#define	XDSL_CLK_EN	ADSL_CLK_EN
#elif defined(CONFIG_BCM96368)
#include "6368_common.h"
#include "6368_map.h"
#define	XDSL_CLK_EN	(VDSL_CLK_EN | PHYMIPS_CLK_EN | VDSL_BONDING_EN | VDSL_AFE_EN | VDSL_QPROC_EN)
#elif defined(CONFIG_BCM96362)
#include "6362_common.h"
#include "6362_map.h"
#define	XDSL_CLK_EN	(ADSL_CLK_EN | PHYMIPS_CLK_EN | ADSL_AFE_EN | ADSL_QPROC_EN)
#elif defined(CONFIG_BCM96328)
#include "6328_common.h"
#include "6328_map.h"
#define	XDSL_CLK_EN	(ADSL_CLK_EN | PHYMIPS_CLK_EN | ADSL_AFE_EN | ADSL_QPROC_EN)
#elif defined(CONFIG_BCM963268)
#include "63268_common.h"
#include "63268_map.h"
#define	XDSL_CLK_EN	(VDSL_CLK_EN | PHYMIPS_CLK_EN | VDSL_AFE_EN | VDSL_QPROC_EN)
#endif

#ifdef VXWORKS
#include "interrupt.h"
#ifdef ADSL_IRQ
#undef	INTERRUPT_ID_ADSL
#define INTERRUPT_ID_ADSL   ADSL_IRQ
#endif
#elif defined(TARG_OS_RTEMS)
#include <stdarg.h>
#include "bspcfg.h"
#ifndef BD_BCM63XX_TIMER_CLOCK_INPUT
#define BD_BCM63XX_TIMER_CLOCK_INPUT BD_BCM6345_TIMER_CLOCK_INPUT
#endif
#define FPERIPH BD_BCM63XX_TIMER_CLOCK_INPUT
#else
#include "boardparms.h"
#endif /* VXWORKS */

#include "board.h"

#if !defined(TARG_OS_RTEMS) && !defined(VXWORKS)
#include "bcm_intr.h"
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
#define INTERRUPT_ID_ADSL   INTERRUPT_ID_XDSL
#endif
#endif

#include "bcmadsl.h"
#include "BcmAdslCore.h"
#include "AdslCore.h"
#include "AdslCoreMap.h"

#define EXCLUDE_CYGWIN32_TYPES
#include "softdsl/SoftDsl.h"
#include "softdsl/SoftDsl.gh"

#include "BcmAdslDiag.h"
#include "DiagDef.h"

#include "AdslDrvVersion.h"

#ifdef HMI_QA_SUPPORT
#include "BcmXdslHmi.h"
#endif

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
#include <bcmxtmcfg.h>
#include "softdsl/SoftDslG993p2.h"

#ifndef SUPPORT_TRAFFIC_TYPE_PTM_RAW
#define SUPPORT_TRAFFIC_TYPE_PTM_RAW	0
#endif
#ifndef SUPPORT_TRAFFIC_TYPE_PTM_BONDED
#define SUPPORT_TRAFFIC_TYPE_PTM_BONDED	0
#endif
#ifndef SUPPORT_TRAFFIC_TYPE_ATM_BONDED
#define SUPPORT_TRAFFIC_TYPE_ATM_BONDED	0
#endif

#ifdef BP_AFE_DEFAULT
#if (BP_AFE_CHIP_INT != (AFE_CHIP_INT << AFE_CHIP_SHIFT))
#error Inconsistent BP_AFE_CHIP_INT definition
#endif
#if (BP_AFE_LD_ISIL1556 != (AFE_LD_ISIL1556 << AFE_LD_SHIFT))
#error Inconsistent BP_AFE_LD_ISIL1556 definition
#endif
#if (BP_AFE_FE_ANNEXA != (AFE_FE_ANNEXA << AFE_FE_ANNEX_SHIFT))
#error Inconsistent BP_AFE_FE_ANNEXA definition
#endif
#if (BP_AFE_FE_REV_ISIL_REV1 != (AFE_FE_REV_ISIL_REV1 << AFE_FE_REV_SHIFT))
#error Inconsistent BP_AFE_FE_REV_ISIL_REV1 definition
#endif
#endif	/* BP_AFE_DEFAULT */
#endif	/* CONFIG_BCM96368 and newer chips */

#ifdef CONFIG_BCM96348
#define BCM6348_PLL_WORKAROUND
#endif

#undef	ADSLCORE_ENABLE_LONG_REACH
/* #define  DTAG_UR2 */

#define ADSL_MIPS_STATUS_TIMEOUT_MS		60000

#if (ADSL_OEM_G994_VENDOR_ID != kDslOemG994VendorId)
#error Inconsistent ADSL_OEM_G994_VENDOR_ID definition
#endif
#if (ADSL_OEM_G994_XMT_NS_INFO != kDslOemG994XmtNSInfo)
#error Inconsistent ADSL_OEM_G994_XMT_NS_INFO definition
#endif
#if (ADSL_OEM_G994_RCV_NS_INFO != kDslOemG994RcvNSInfo)
#error Inconsistent ADSL_OEM_G994_RCV_NS_INFOdefinition
#endif
#if (ADSL_OEM_EOC_VENDOR_ID != kDslOemEocVendorId)
#error Inconsistent ADSL_OEM_EOC_VENDOR_ID definition
#endif
#if (ADSL_OEM_EOC_VERSION != kDslOemEocVersion)
#error Inconsistent ADSL_OEM_EOC_VERSION definition
#endif
#if (ADSL_OEM_EOC_SERIAL_NUMBER != kDslOemEocSerNum)
#error Inconsistent ADSL_OEM_EOC_SERIAL_NUMBER definition
#endif
#if (ADSL_OEM_T1413_VENDOR_ID != kDslOemT1413VendorId)
#error Inconsistent ADSL_OEM_T1413_VENDOR_ID definition
#endif
#if (ADSL_OEM_T1413_EOC_VENDOR_ID != kDslOemT1413EocVendorId)
#error Inconsistent ADSL_OEM_T1413_EOC_VENDOR_ID definition
#endif
#if (ADSL_XMT_GAIN_AUTO != kDslXmtGainAuto)
#error Inconsistent ADSL_XMT_GAIN_AUTO definition
#endif


/* External vars */
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
extern int toggle_reset;
#endif

extern char     isGdbOn(void);
extern int		g_nAdslExit;
extern long	adslCoreEcUpdateMask;
extern void AdslCoreSetTime(ulong timeMs);
//LGD_FOR_TR098
extern unsigned long g_ShowtimeStartTicks[];

#ifdef SUPPORT_STATUS_BACKUP
extern void AdslCoreBkupSbStat(void);
extern AC_BOOL AdslCoreSwitchCurSbStatToSharedSbStat(void);
#endif
#if defined(ADSLDRV_STATUS_PROFILING) || defined(SUPPORT_STATUS_BACKUP)
extern int AdslCoreStatusAvailTotal (void);
#endif
extern AC_BOOL AdslCoreStatusAvail (void);

#if defined(__KERNEL__) 

#if ( (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)) && \
	(defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)) )
#if defined(CONFIG_BCM_ATM_BONDING_ETH) || defined(CONFIG_BCM_ATM_BONDING_ETH_MODULE)
extern int atmbonding_enabled ;
#endif
#endif

#endif

/* Local vars */

#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
LOCAL OS_SEMID	irqSemId = (OS_SEMID)  0;
LOCAL OS_TASKID	IrqTid	 = (OS_TASKID) 0;
#else
LOCAL void *	irqDpcId = NULL;
LOCAL AC_BOOL	dpcScheduled=AC_FALSE;
#endif

LOCAL OS_TICKS	pingLastTick, statLastTick;

#ifdef ADSLDRV_STATUS_PROFILING
LOCAL OS_TICKS	printTicks = 0;
LOCAL ulong	intrDurTotal=0, intrDurMin=(ulong)-1, intrDurMax=0;
LOCAL ulong	intrDurMaxAtIntrCnt=0;
LOCAL ulong	intrDuringSchedCnt=0;
LOCAL ulong	dpcDurTotal=0, dpcDurMin=(ulong)-1, dpcDurMax=0;
LOCAL ulong	dpcDurMaxAtDpcCnt=0;
LOCAL int	dpcDurMaxAtByteAvail=0;

LOCAL ulong	dpcDelayTotal=0, dpcDelayMin=(ulong)-1, dpcDelayMax=0;
LOCAL ulong	dpcDelayMaxAtDpcCnt=0;
LOCAL int	dpcDelayMaxAtByteAvail=0;

LOCAL int	byteAvailMax=0, byteAvailMaxAtByteProc=0, byteAvailMaxAtNumStatProc=0;
LOCAL ulong	byteAvailMaxAtDpcCnt=0, byteAvailMaxAtDpcDur=0, byteAvailMaxAtDpcDelay=0;

LOCAL int	byteProcMax=0, byteProcMaxAtNumStatProc=0;
LOCAL ulong	byteProcMaxAtDpcCnt=0, byteProcMaxAtDpcDur=0, byteProcMaxAtDpcDelay=0;

LOCAL int	nStatProcMax=0, nStatProcMaxAtDpcCnt=0, nStatProcMaxAtByteAvail=0, nStatProcMaxAtByteProc=0;

LOCAL ulong	dpcScheduleTimeStamp=0;
int			gByteAvail=0;
#endif

#if defined(SUPPORT_STATUS_BACKUP) || defined(ADSLDRV_STATUS_PROFILING)
int			gByteProc=0;
ulong		gBkupStartAtIntrCnt=0, gBkupStartAtdpcCnt=0;
#endif

#ifdef SUPPORT_DSL_BONDING
LOCAL long		acPendingEvents[MAX_DSL_LINE] = {0, 0};
LOCAL long		acL3Requested[MAX_DSL_LINE] = {0, 0};
LOCAL OS_TICKS	acL3StartTick[MAX_DSL_LINE] = {0, 0};
#else
LOCAL long		acPendingEvents[MAX_DSL_LINE] = {0};
LOCAL long		acL3Requested[MAX_DSL_LINE] = {0};
LOCAL OS_TICKS	acL3StartTick[MAX_DSL_LINE] = {0};
#endif
void (*bcmNotify)(unsigned char) = BcmAdsl_Notify;

dslCommandStruct	adslCoreConnectionParam[MAX_DSL_LINE];
dslCommandStruct	adslCoreCfgCmd[MAX_DSL_LINE]; 
adslCfgProfile		adslCoreCfgProfile[MAX_DSL_LINE];
#ifdef SUPPORT_DSL_BONDING
adslCfgProfile		*pAdslCoreCfgProfile[MAX_DSL_LINE] = {NULL, NULL};
#else
adslCfgProfile		*pAdslCoreCfgProfile[MAX_DSL_LINE] = {NULL};
#endif
#ifdef G992P3
g992p3DataPumpCapabilities	g992p3Param[MAX_DSL_LINE];
#endif
#ifdef G992P5
g992p3DataPumpCapabilities	g992p5Param[MAX_DSL_LINE];
#endif
#ifdef G993
g993p2DataPumpCapabilities g993p2Param[MAX_DSL_LINE];
#endif
Bool			adslCoreInitialized = AC_FALSE;
#ifdef SUPPORT_DSL_BONDING
Bool			adslCoreConnectionMode[MAX_DSL_LINE] = {AC_FALSE, AC_FALSE};
#else
Bool			adslCoreConnectionMode[MAX_DSL_LINE] = {AC_FALSE};
#endif
ulong			adslCoreIntrCnt		= 0;
ulong			adslCoreIsrTaskCnt	= 0;
#ifdef SUPPORT_DSL_BONDING
long			adslCoreXmtGain [MAX_DSL_LINE] = {ADSL_XMT_GAIN_AUTO, ADSL_XMT_GAIN_AUTO};
Bool			adslCoreXmtGainChanged[MAX_DSL_LINE] = {AC_FALSE, AC_FALSE};
#else
long			adslCoreXmtGain	[MAX_DSL_LINE] = {ADSL_XMT_GAIN_AUTO};
Bool			adslCoreXmtGainChanged[MAX_DSL_LINE] = {AC_FALSE};
#endif
#ifdef VXWORKS
Bool			adslCoreAlwaysReset = AC_FALSE;
#else
Bool			adslCoreAlwaysReset = AC_TRUE;
#endif
Bool			adslCoreStarted = AC_FALSE;
Bool			adslCoreResetPending = AC_FALSE;
ulong			adslCoreCyclesPerMs = 0;
Bool			adslCoreMuteDiagStatus = AC_FALSE;
#ifdef SUPPORT_DSL_BONDING
ulong			adslCoreHsModeSwitchTime[MAX_DSL_LINE] = {0, 0};
#else
ulong			adslCoreHsModeSwitchTime[MAX_DSL_LINE] = {0};
#endif
int			gConsoleOutputEnable = 1;
Bool		gDiagDataLog = AC_FALSE;
OS_TICKS	gDiagLastCmdTime = 0;
OS_TICKS	gDiagLastPingTime = 0;
ulong		gPhyPCAddr =HOST_LMEM_BASE + 0x5B0;

Bool			gSharedMemAllocFromUserContext = 0;

ulong		gDiagSyncTime = 0;
ulong		gDiagSyncTimeLastCycleCount = 0;

#if defined(PHY_BLOCK_TEST) && defined(PHY_BLOCK_TEST_SAR)
Bool			adslNewImg = AC_FALSE;
#endif

struct {
	ulong		rcvCtrl, rcvAddr, rcvPtr;
	ulong		xmtCtrl, xmtAddr, xmtPtr;
} adslCoreDiagState;

typedef struct {
	int	nStatus;
	void	*pAddr;
	int	len;
	int	maxLen;	
} xdslSavedStatInfo;

typedef struct {
	Bool	saveStatInitialized;
	Bool	saveStatStarted;
	Bool	saveStatContinous;
	Bool saveStatDisableOnRetrain;
	int	bufSize;
	void	*pBuf;
	int	curSplitStatEndLen;
	xdslSavedStatInfo	*pCurrentSavedStatInfo;
	xdslSavedStatInfo	savedStatInfo[2];
} xdslSavedStatCtrl;

DiagDebugData	diagDebugCmd = { 0, 0 };
#ifdef __kerSysGetAfeId
ulong           nvramAfeId[2] = {0};
#endif

#if defined(__KERNEL__) && (!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
char				*pBkupImage = NULL;
ulong			bkupSdramSize = 0;
ulong			bkupLmemSize = 0;
#endif


#if !defined(CONFIG_BCM96358) && defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
int BcmXdslCoreGetAfeBoardId(ulong *pAfeIds);
#endif

LOCAL void BcmAdslCoreSetConnectionParam(unsigned char lineId, dslCommandStruct *pDslCmd, dslCommandStruct *pCfgCmd, adslCfgProfile *pAdslCfg);
LOCAL Bool BcmXdslDiagStatSaveLocalIsActive(void);
LOCAL void BcmXdslDiagStatSaveLocal(ulong cmd, char *statStr, long n, char *p1, long n1);

BCMADSL_STATUS BcmAdslDiagStatSaveInit(void *pAddr, int len);
BCMADSL_STATUS BcmAdslDiagStatSaveContinous(void);
BCMADSL_STATUS BcmAdslDiagStatSaveStart(void);
BCMADSL_STATUS BcmAdslDiagStatSaveStop(void);
BCMADSL_STATUS BcmAdslDiagStatSaveUnInit(void);
BCMADSL_STATUS BcmAdslDiagStatSaveGet(PADSL_SAVEDSTATUS_INFO pSavedStatInfo);

void BcmAdslCoreDiagWriteStatus(dslStatusStruct *status, char *pBuf, int len);

#ifdef PHY_PROFILE_SUPPORT
LOCAL void BcmAdslCoreProfilingStart(void);
void BcmAdslCoreProfilingStop(void);
#endif

LOCAL void BcmAdslCoreEnableSnrMarginData(unsigned char lineId);
LOCAL void BcmAdslCoreDisableSnrMarginData(unsigned char lineId);
ulong BcmAdslCoreStatusSnooper(dslStatusStruct *status, char *pBuf, int len);
void BcmAdslCoreAfeTestStatus(dslStatusStruct *status);
void BcmAdslCoreSendBuffer(ulong statusCode, uchar *bufPtr, ulong bufLen);
void BcmAdslCoreSendDmaBuffers(ulong statusCode, int bufNum);

#if 1 || defined(PHY_BLOCK_TEST)
void BcmAdslCoreProcessTestCommand(void);
Bool BcmAdslCoreIsTestCommand(void);
void BcmAdslCoreDebugTimer(void);
void BcmAdslCoreDebugSendCommand(char *fileName, ushort cmd, ulong offset, ulong len, ulong bufAddr);
void BcmAdslCoreDebugPlaybackStop(void);
void BcmAdslCoreDebugPlaybackResume(void);
void BcmAdslCoreDebugReset(void);
void BcmAdslCoreDebugReadEndOfFile(void);
#endif
void BcmAdslCoreGdbTask(void);
#ifdef LINUX_KERNEL_2_6
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
LOCAL irqreturn_t BcmCoreInterruptHandler(int irq, void * dev_id);
#else
LOCAL irqreturn_t BcmCoreInterruptHandler(int irq, void * dev_id, struct pt_regs *ptregs);
#endif
#else
UINT32 BcmCoreInterruptHandler (void);
#endif
void BcmCoreAtmVcInit(void);
void BcmCoreAtmVcSet(void);

#ifdef CONFIG_VDSL_SUPPORTED

int spiTo6306Read(int address);
void spiTo6306Write(int address, unsigned int data);
int spiTo6306IndirectRead(int address);
void spiTo6306IndirectWrite(int address, unsigned int data);
void SetupReferenceClockTo6306(void);
void PLLPowerUpSequence6306(void);
int IsAfe6306ChipUsed(void);

#define     CTRL_CONTROL_ADSL_MODE          0
#define     CTRL_CONTROL_VDSL_MODE          1

static int  vsb6306VdslMode = CTRL_CONTROL_ADSL_MODE;

#endif


#define BCMSWAP32(val) \
	((uint32)( \
		(((uint32)(val) & (uint32)0x000000ffUL) << 24) | \
		(((uint32)(val) & (uint32)0x0000ff00UL) <<  8) | \
		(((uint32)(val) & (uint32)0x00ff0000UL) >>  8) | \
		(((uint32)(val) & (uint32)0xff000000UL) >> 24) ))
		
#define	STAT_REC_HEADER		0x55AA1234
#define	DIAG_STRING_STATUS	1
#define	DIAG_BINARY_STATUS	2
#define	DIAG_STATTYPE_LINEID_SHIFT	13
#define	DIAG_MIN_BUF_SIZE	2048

static struct {
	ulong	statSign;
	ulong	statId;
} statHdr = { STAT_REC_HEADER, 0 };

static xdslSavedStatCtrl	gSaveStatCtrl = {AC_FALSE, AC_FALSE, AC_FALSE, AC_FALSE, 0, NULL, 0, NULL, {{0,NULL,0,0}, {0,NULL,0,0}}};

#if defined(CONFIG_SMP) && defined(__KERNEL__)
spinlock_t	gSpinLock;
static ulong	spinCount[2] = {0,0};
#ifdef SPINLOCK_CALL_STAT
static ulong	spinLockNestedCallEnter[2] = {0,0};
static ulong	spinLockCalledEnter[2] = {0,0};
static ulong	spinLockNestedCallExit[2] = {0,0};
static ulong	spinLockCalledExit[2] = {0,0};
#endif
void SMP_DpcSyncEnter(void)
{
	int cpuId;
	
	local_bh_disable();
	cpuId = smp_processor_id();
	if (cpuId !=0 && cpuId != 1) {
		printk("unexpected CPU value! %d\n", cpuId);
		return;
	}
	spinCount[cpuId]++;
	if (spinCount[cpuId] != 1) {
#ifdef SPINLOCK_CALL_STAT
		spinLockNestedCallEnter[cpuId]++;
#endif
		return;	/* Prevent nested calls */
	}
#ifdef SPINLOCK_CALL_STAT
	spinLockCalledEnter[cpuId]++;
#endif
	spin_lock_bh(&gSpinLock);
}

void SMP_DpcSyncExit(void)
{
	int cpuId = smp_processor_id();
	
	if (cpuId !=0 && cpuId != 1) {
		printk("unexpected CPU value! %d\n", cpuId);
		local_bh_enable();
		return;
	}
	spinCount[cpuId]--;
	if (spinCount[cpuId] != 0) {
		local_bh_enable();
#ifdef SPINLOCK_CALL_STAT
		spinLockNestedCallExit[cpuId]++;
#endif
		return;
	}
#ifdef SPINLOCK_CALL_STAT
	spinLockCalledExit[cpuId]++;
#endif
	spin_unlock_bh(&gSpinLock);
	local_bh_enable();
}
#endif

#ifdef SUPPORT_MULTI_DIAG_CLIENT
#define	DIAG_CLIENTID_DSL	0
#define	DIAG_CLIENTID_XTM	1
#define	DIAG_CLIENTID_WLAN	2

#define	DIAG_DBG_PRINT_STR	0	/* Print string to log */
#define	DIAG_DBG_DATA_DUMP	1	/* Dump data to log */
#define	DIAG_DBG_WRITE_FILE	2	/* Write data to file */

void BcmDiagDbgDataSend(uchar clientId, ulong code, void *p, int len, char *str)
{

}
#endif

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
void BcmAdslDiagEnablePrintStatCmd(void)
{
	dslCommandStruct	cmd;

	cmd.command = kDslDiagFrameHdrCmd;
	cmd.param.dslStatusBufSpec.pBuf = 0;
	cmd.param.dslStatusBufSpec.bufSize = 0;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslDiagDisablePrintStatCmd(void)
{
	dslCommandStruct	cmd;
	cmd.command = kDslDiagSetupCmd;
	cmd.param.dslDiagSpec.setup = 0;
	cmd.param.dslDiagSpec.eyeConstIndex1 = 0;
	cmd.param.dslDiagSpec.eyeConstIndex2 = 0;
	cmd.param.dslDiagSpec.logTime = 0;
	BcmCoreCommandHandlerSync(&cmd);

}
#endif

BCMADSL_STATUS BcmAdslDiagStatSaveInit(void *pAddr, int len)
{
	if((NULL == pAddr) || (len < DIAG_MIN_BUF_SIZE)) {
		BcmAdslCoreDiagWriteStatusString(0, "%: Failed pAddr=%X len=%d\n", __FUNCTION__, (ulong)pAddr, len);
		return BCMADSL_STATUS_ERROR;
	}
	
	BcmCoreDpcSyncEnter();
	statHdr.statSign = BCMSWAP32(STAT_REC_HEADER);
	memset((void*)&gSaveStatCtrl, 0, sizeof(gSaveStatCtrl));
	gSaveStatCtrl.pBuf = pAddr;
	gSaveStatCtrl.bufSize = len;
	gSaveStatCtrl.saveStatStarted = AC_FALSE;
	gSaveStatCtrl.saveStatContinous = AC_FALSE;
	gSaveStatCtrl.saveStatDisableOnRetrain = AC_FALSE;
	gSaveStatCtrl.savedStatInfo[0].nStatus = 0;
	gSaveStatCtrl.savedStatInfo[0].len = 0;
	gSaveStatCtrl.savedStatInfo[0].pAddr = pAddr;
	gSaveStatCtrl.savedStatInfo[0].maxLen = len/2;
	gSaveStatCtrl.savedStatInfo[1].nStatus = 0;
	gSaveStatCtrl.savedStatInfo[1].len = 0;
	gSaveStatCtrl.savedStatInfo[1].pAddr = pAddr + len/2;
	gSaveStatCtrl.savedStatInfo[1].maxLen = len - len/2;
	gSaveStatCtrl.pCurrentSavedStatInfo = &gSaveStatCtrl.savedStatInfo[0];
	gSaveStatCtrl.saveStatInitialized = AC_TRUE;
	BcmCoreDpcSyncExit();
	
	return BCMADSL_STATUS_SUCCESS;
}

BCMADSL_STATUS BcmAdslDiagStatSaveContinous(void)
{
	if((AC_TRUE == gSaveStatCtrl.saveStatInitialized) && (AC_FALSE == gSaveStatCtrl.saveStatStarted) ) {
		gSaveStatCtrl.saveStatContinous = AC_TRUE;
		return BCMADSL_STATUS_SUCCESS;
	}
	
	return BCMADSL_STATUS_ERROR;
}

BCMADSL_STATUS BcmAdslDiagStatSaveStart(void)
{
	if(AC_TRUE != gSaveStatCtrl.saveStatInitialized)
		return BCMADSL_STATUS_ERROR;
	
	gSaveStatCtrl.saveStatStarted = AC_TRUE;
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
	BcmAdslDiagEnablePrintStatCmd();
#endif
	return BCMADSL_STATUS_SUCCESS;
}

BCMADSL_STATUS BcmAdslDiagStatSaveStop(void)
{
#ifdef SAVE_STAT_LOCAL_DBG
	printk("**%s:**\n", __FUNCTION__);
#endif
	gSaveStatCtrl.saveStatStarted = AC_FALSE;
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
	BcmAdslDiagDisablePrintStatCmd();
#endif
	return BCMADSL_STATUS_SUCCESS;
}

BCMADSL_STATUS BcmAdslDiagStatSaveUnInit(void)
{
	gSaveStatCtrl.saveStatStarted = AC_FALSE;
	gSaveStatCtrl.saveStatInitialized = AC_FALSE;
	gSaveStatCtrl.saveStatContinous = AC_FALSE;
	gSaveStatCtrl.saveStatDisableOnRetrain = AC_FALSE;
	return BCMADSL_STATUS_SUCCESS;
}

BCMADSL_STATUS BcmAdslDiagStatSaveGet(PADSL_SAVEDSTATUS_INFO pSavedStatInfo)
{
	xdslSavedStatInfo *p, *p1;
	
	if(AC_TRUE != gSaveStatCtrl.saveStatInitialized) {
		BcmAdslCoreDiagWriteStatusString(0, "%s: gSaveStatCtrl has not been initialized", __FUNCTION__);
		return BCMADSL_STATUS_ERROR;
	}
	
	BcmCoreDpcSyncEnter();
#if 1
	BcmAdslCoreDiagWriteStatusString(0, "pCurAddr=%x pAddr0=%x len0=%d nStatus0=%d, pAddr1=%x len1=%d nStatus1=%d\n",
		(uint)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr,
		(uint)gSaveStatCtrl.savedStatInfo[0].pAddr,
		gSaveStatCtrl.savedStatInfo[0].len,
		gSaveStatCtrl.savedStatInfo[0].nStatus,
		(uint)gSaveStatCtrl.savedStatInfo[1].pAddr,
		gSaveStatCtrl.savedStatInfo[1].len,
		gSaveStatCtrl.savedStatInfo[1].nStatus);
	printk("pCurAddr=%x pAddr0=%x len0=%d nStatus0=%d, pAddr1=%x len1=%d nStatus1=%d\n",
		(uint)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr,
		(uint)gSaveStatCtrl.savedStatInfo[0].pAddr,
		gSaveStatCtrl.savedStatInfo[0].len,
		gSaveStatCtrl.savedStatInfo[0].nStatus,
		(uint)gSaveStatCtrl.savedStatInfo[1].pAddr,
		gSaveStatCtrl.savedStatInfo[1].len,
		gSaveStatCtrl.savedStatInfo[1].nStatus);
#endif
	
	if(gSaveStatCtrl.pCurrentSavedStatInfo == &gSaveStatCtrl.savedStatInfo[1]) {
		p = &gSaveStatCtrl.savedStatInfo[0];
		p1 = &gSaveStatCtrl.savedStatInfo[1];
	}
	else {
		if(gSaveStatCtrl.savedStatInfo[1].len > 0) {
			p = &gSaveStatCtrl.savedStatInfo[1];
			p1= &gSaveStatCtrl.savedStatInfo[0];
		}
		else {
			p = &gSaveStatCtrl.savedStatInfo[0];
			p1= &gSaveStatCtrl.savedStatInfo[1];
		}
	}
	pSavedStatInfo->nStatus = p->nStatus;
	pSavedStatInfo->pAddr = p->pAddr;
	pSavedStatInfo->len = p->len;	
	pSavedStatInfo->nStatus += p1->nStatus;
	pSavedStatInfo->pAddr1 = p1->pAddr;
	pSavedStatInfo->len1 = p1->len;
	BcmCoreDpcSyncExit();
	
	return BCMADSL_STATUS_SUCCESS;
}

void BcmXdslCoreDiagStatSaveDisableOnRetrainSet(void)
{
	gSaveStatCtrl.saveStatDisableOnRetrain = TRUE;
}

LOCAL Bool BcmXdslDiagStatSaveLocalIsActive(void)
{
	return( AC_TRUE == gSaveStatCtrl.saveStatStarted );
}

#define	BUF_SPACE_AVAILABLE(x)	((x).pCurrentSavedStatInfo->maxLen - (x).pCurrentSavedStatInfo->len)

#ifdef SAVE_STAT_LOCAL_DBG
LOCAL void BcmXdslDiagPrintStatSaveLocalInfo(int switchToBufNum, int curStatLen)
{
	int prevBufNum = (1 == switchToBufNum) ? 0: 1;
	
	printk("**Switch to buf%d: nStatus%d=%d len%d=%d, nStatus%d=%d len%d=%d buf%dSpaceAvail=%d, curStatLen=%d**\n",
		switchToBufNum, switchToBufNum, gSaveStatCtrl.pCurrentSavedStatInfo->nStatus,
		switchToBufNum, gSaveStatCtrl.pCurrentSavedStatInfo->len,
		prevBufNum, gSaveStatCtrl.savedStatInfo[prevBufNum].nStatus,
		prevBufNum, gSaveStatCtrl.savedStatInfo[prevBufNum].len,
		prevBufNum, gSaveStatCtrl.savedStatInfo[prevBufNum].maxLen - gSaveStatCtrl.savedStatInfo[prevBufNum].len,
		curStatLen);
}
#endif

LOCAL void BcmXdslDiagStatSaveLocal(ulong cmd, char *statStr, long n, char *p1, long n1)
{
#ifdef SUPPORT_DSL_BONDING
	ulong lineId;
#endif
	ulong statId;
	int len = (int)(n+n1);

	if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (len + sizeof(statHdr)) ) {
		if(gSaveStatCtrl.pCurrentSavedStatInfo == &gSaveStatCtrl.savedStatInfo[0]) {
			gSaveStatCtrl.pCurrentSavedStatInfo = &gSaveStatCtrl.savedStatInfo[1];
#ifdef SAVE_STAT_LOCAL_DBG
			BcmXdslDiagPrintStatSaveLocalInfo(1, len);
#endif
			gSaveStatCtrl.pCurrentSavedStatInfo->nStatus = 0;
			gSaveStatCtrl.pCurrentSavedStatInfo->len = 0;
			if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (len + sizeof(statHdr)) )
				return;
		}
		else {
			/* The second buffer is full */
			if(AC_FALSE == gSaveStatCtrl.saveStatContinous)
				return;
			
			gSaveStatCtrl.pCurrentSavedStatInfo = &gSaveStatCtrl.savedStatInfo[0];
#ifdef SAVE_STAT_LOCAL_DBG
			BcmXdslDiagPrintStatSaveLocalInfo(0, len);
#endif
			gSaveStatCtrl.pCurrentSavedStatInfo->nStatus = 0;
			gSaveStatCtrl.pCurrentSavedStatInfo->len = 0;
			if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (len + sizeof(statHdr)) )
				return;
		}
	}
#ifdef SUPPORT_DSL_BONDING
	lineId = (cmd & DIAG_LINE_MASK) >> DIAG_LINE_SHIFT;
	if ( statusInfoData == (cmd & ~DIAG_LINE_MASK))
		statId = (DIAG_BINARY_STATUS | (lineId << DIAG_STATTYPE_LINEID_SHIFT)) << 16;
	else
		statId = (DIAG_STRING_STATUS | (lineId << DIAG_STATTYPE_LINEID_SHIFT)) << 16;
#else
	if ( statusInfoData == cmd )
		statId = DIAG_BINARY_STATUS << 16;
	else
		statId = DIAG_STRING_STATUS << 16;
#endif
	statHdr.statId = statId | (len & 0xFFFF);
	statHdr.statId = BCMSWAP32(statHdr.statId);
	
	memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
		(void*)&statHdr, sizeof(statHdr));
	gSaveStatCtrl.pCurrentSavedStatInfo->len += sizeof(statHdr);
	
	memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
		(void*)statStr, n);
	gSaveStatCtrl.pCurrentSavedStatInfo->len += n;
	
	if( NULL != p1) {
		memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
			(void*)p1, n1);
		gSaveStatCtrl.pCurrentSavedStatInfo->len += n1;
	}
	gSaveStatCtrl.pCurrentSavedStatInfo->nStatus++;
}

/* NOTE: Call to BcmXdslDiagFirstSplitStatSaveLocal(), BcmXdslDiagContSplitStatSaveLocal(), and BcmXdslDiagLastSplitStatSaveLocal()
* need to be protected if not from the bottom half context
*/
LOCAL void BcmXdslDiagFirstSplitStatSaveLocal(ulong cmd, int totalLen, void *statStr, int n, void *p1, int n1)
{
#ifdef SUPPORT_DSL_BONDING
	ulong lineId;
#endif
	ulong statId;

	if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (totalLen + sizeof(statHdr)) ) {
		if(gSaveStatCtrl.pCurrentSavedStatInfo == &gSaveStatCtrl.savedStatInfo[0]) {
			gSaveStatCtrl.pCurrentSavedStatInfo = &gSaveStatCtrl.savedStatInfo[1];
#ifdef SAVE_STAT_LOCAL_DBG
			BcmXdslDiagPrintStatSaveLocalInfo(1, totalLen);
#endif
			gSaveStatCtrl.pCurrentSavedStatInfo->nStatus = 0;
			gSaveStatCtrl.pCurrentSavedStatInfo->len = 0;
			if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (totalLen + sizeof(statHdr)) ) {
				gSaveStatCtrl.curSplitStatEndLen = 0;
				return;
			}
		}
		else {
			/* The second buffer is full */
			if(AC_FALSE == gSaveStatCtrl.saveStatContinous) {
				gSaveStatCtrl.curSplitStatEndLen = 0;
				return;
			}
			
			gSaveStatCtrl.pCurrentSavedStatInfo = &gSaveStatCtrl.savedStatInfo[0];
#ifdef SAVE_STAT_LOCAL_DBG
			BcmXdslDiagPrintStatSaveLocalInfo(0, totalLen);
#endif
			gSaveStatCtrl.pCurrentSavedStatInfo->nStatus = 0;
			gSaveStatCtrl.pCurrentSavedStatInfo->len = 0;
			if( BUF_SPACE_AVAILABLE(gSaveStatCtrl) < (totalLen + sizeof(statHdr)) ) {
				gSaveStatCtrl.curSplitStatEndLen = 0;
				return;
			}
		}
	}
#ifdef SUPPORT_DSL_BONDING
	lineId = (cmd & DIAG_LINE_MASK) >> DIAG_LINE_SHIFT;
	if ( statusInfoData == (cmd & ~DIAG_LINE_MASK))
		statId = (DIAG_BINARY_STATUS | (lineId << DIAG_STATTYPE_LINEID_SHIFT)) << 16;
	else
		statId = (DIAG_STRING_STATUS | (lineId << DIAG_STATTYPE_LINEID_SHIFT)) << 16;
#else
	if ( statusInfoData == cmd )
		statId = DIAG_BINARY_STATUS << 16;
	else
		statId = DIAG_STRING_STATUS << 16;
#endif
	gSaveStatCtrl.curSplitStatEndLen = gSaveStatCtrl.pCurrentSavedStatInfo->len + sizeof(statHdr) + totalLen;
	statHdr.statId = statId | (totalLen & 0xFFFF);
	statHdr.statId = BCMSWAP32(statHdr.statId);
	
	memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
		(void*)&statHdr, sizeof(statHdr));
	gSaveStatCtrl.pCurrentSavedStatInfo->len += sizeof(statHdr);
	
	memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
		statStr, n);
	gSaveStatCtrl.pCurrentSavedStatInfo->len += n;
	
	if( NULL != p1) {
		memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
			p1, n1);
		gSaveStatCtrl.pCurrentSavedStatInfo->len += n1;
	}
	gSaveStatCtrl.pCurrentSavedStatInfo->nStatus++;
}

LOCAL void BcmXdslDiagContSplitStatSaveLocal(void *p, int n)
{
	if(gSaveStatCtrl.curSplitStatEndLen) {
		memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
			p, n);
		gSaveStatCtrl.pCurrentSavedStatInfo->len += n;
	}
}

LOCAL void BcmXdslDiagLastSplitStatSaveLocal(void *p, int n)
{
	if(gSaveStatCtrl.curSplitStatEndLen) {
		memcpy((void*)((int)gSaveStatCtrl.pCurrentSavedStatInfo->pAddr+gSaveStatCtrl.pCurrentSavedStatInfo->len),
			p, n);
		gSaveStatCtrl.pCurrentSavedStatInfo->len += n;
		if(gSaveStatCtrl.curSplitStatEndLen != gSaveStatCtrl.pCurrentSavedStatInfo->len) {
#ifdef SAVE_STAT_LOCAL_DBG
			printk("*** %s: curSplitStatEndLen=%d pCurrentSavedStatInfo->len=%d lastStatLen=%d ***\n",
				__FUNCTION__, gSaveStatCtrl.curSplitStatEndLen,
				gSaveStatCtrl.pCurrentSavedStatInfo->len, n);
#endif
			gSaveStatCtrl.pCurrentSavedStatInfo->len = gSaveStatCtrl.curSplitStatEndLen;
		}
		gSaveStatCtrl.curSplitStatEndLen = 0;
	}
}

#ifdef ADSLDRV_STATUS_PROFILING
void BcmXdslCoreDrvProfileInfoClear(void)
{
	BcmCoreDpcSyncEnter();	/* Should disable interrupt b/w the first 2 lines, but it's OK for experimenting */
	adslCoreIntrCnt=intrDurTotal=intrDurTotal=intrDurMax=intrDurMaxAtIntrCnt=intrDuringSchedCnt=0;
	intrDurMin=(ulong)-1;
	adslCoreIsrTaskCnt=dpcDurTotal=dpcDurTotal=dpcDurMax=dpcDurMaxAtDpcCnt=dpcDurMaxAtByteAvail=0;
	dpcDurMin=(ulong)-1;
	dpcDelayTotal=dpcDelayTotal=dpcDelayMax=dpcDelayMaxAtDpcCnt=dpcDelayMaxAtByteAvail=0;
	dpcDelayMin=(ulong)-1;
	byteAvailMax=byteAvailMaxAtDpcCnt=byteAvailMaxAtDpcDur=byteAvailMaxAtDpcDelay=byteAvailMaxAtNumStatProc=byteAvailMaxAtByteProc=0;
	byteProcMax=byteProcMaxAtDpcCnt=byteProcMaxAtDpcDur=byteProcMaxAtDpcDelay=byteProcMaxAtNumStatProc=0;
	nStatProcMax=nStatProcMaxAtDpcCnt=nStatProcMaxAtByteAvail=nStatProcMaxAtByteProc=0;
	gByteAvail=gByteProc=gBkupStartAtIntrCnt=gBkupStartAtdpcCnt=0;
	BcmCoreDpcSyncExit();
}

void BcmXdslCoreDrvProfileInfoPrint(void)
{
	OS_TICKS	ticks;
	int	tMs;
	
	if(!adslCoreIntrCnt || !adslCoreIsrTaskCnt)
		return;
	
	bcmOsGetTime(&ticks);
	tMs = (ticks - printTicks) * BCMOS_MSEC_PER_TICK;
	
	BcmAdslCoreDiagWriteStatusString(0,
		"DrvProfileInfoPrintInterval=%lums, syncTime=%lusec: intrCnt=%lu, durShedIntrCnt=%lu dpcCnt=%lu\n"
		"  intrDurMin=%lu, intrDurAvg=%lu, intrDurMax=%lu at intrCnt=%lu\n"
		"  dpcDurMin=%lu, dpcDurAvg=%lu, dpcDurMax=%lu at dpcCnt=%lu byteAvail=%d\n"
		"  dpcDelayMin=%lu, dpcDelayAvg=%lu, dpcDelayMax=%lu at dpcCnt=%ld byteAvail=%d\n"
		"  byteAvailMax=%d at dpcCnt=%lu dpcDuration=%lu dpcDelay=%lu nStatProc=%d byteProc=%d\n"
		"  byteProcMax =%d at dpcCnt=%lu dpcDuration=%lu dpcDelay=%lu nStatProc=%d\n"
		"  nStatProcMax=%d at dpcCnt=%lu byteAvail=%d byteProc=%d\n"
		"  last bkup at: intrCnt=%lu dpcCnt=%lu, snapshot of: byteAvail=%d byteProc=%d\n",
		tMs, BcmAdslCoreDiagGetSyncTime(), adslCoreIntrCnt, intrDuringSchedCnt, adslCoreIsrTaskCnt,
		intrDurMin, intrDurTotal/adslCoreIntrCnt, intrDurMax, intrDurMaxAtIntrCnt,
		dpcDurMin, dpcDurTotal/adslCoreIsrTaskCnt, dpcDurMax, dpcDurMaxAtDpcCnt, dpcDurMaxAtByteAvail,
		dpcDelayMin, dpcDelayTotal/adslCoreIsrTaskCnt, dpcDelayMax, dpcDelayMaxAtDpcCnt, dpcDelayMaxAtByteAvail,
		byteAvailMax, byteAvailMaxAtDpcCnt, byteAvailMaxAtDpcDur, byteAvailMaxAtDpcDelay, byteAvailMaxAtNumStatProc, byteAvailMaxAtByteProc,
		byteProcMax, byteProcMaxAtDpcCnt, byteProcMaxAtDpcDur, byteProcMaxAtDpcDelay, byteProcMaxAtNumStatProc,
		nStatProcMax, nStatProcMaxAtDpcCnt, nStatProcMaxAtByteAvail, nStatProcMaxAtByteProc,
		gBkupStartAtIntrCnt, gBkupStartAtdpcCnt, gByteAvail, gByteProc);
	
	printTicks=ticks;
}
#endif

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
Boolean IsValidAfeId(ulong afeId)
{
	ulong  n;

	if (0 == afeId)
		return false;

	n = (afeId & AFE_CHIP_MASK) >> AFE_CHIP_SHIFT;
	if ((0 == n) || (n > AFE_CHIP_6306))
		return false;
	n = (afeId & AFE_LD_MASK) >> AFE_LD_SHIFT;
	if ((0 == n) || (n > AFE_LD_6302))
		return false;

	return true;
}

int BcmXdslCoreGetAfeBoardId(ulong *pAfeIds)
{
	int				i, res;
	ulong			mibLen;
	adslMibInfo		*pMibInfo;

#ifdef BP_AFE_DEFAULT
	pAfeIds[0] = BP_AFE_DEFAULT;
	pAfeIds[1] = BP_AFE_DEFAULT;
#else
	char	boardIdStr[BP_BOARD_ID_LEN];
	pAfeIds[0] = 0;
	pAfeIds[1] = 0;
#endif

#ifdef BP_AFE_DEFAULT
	res = BpGetDslPhyAfeIds(pAfeIds);
	if( res  != BP_SUCCESS )
		BcmAdslCoreDiagWriteStatusString(0, "%s: BpGetDslPhyAfeIds() Error(%d)!\n", __FUNCTION__, res);
#ifdef __kerSysGetAfeId
	for (i = 0;i < 2; i++)
		if (IsValidAfeId(nvramAfeId[i]))
			pAfeIds[i] = nvramAfeId[i];
#endif
#else
	res = BpGetBoardId(boardIdStr);
	if( BP_SUCCESS == res ) {
		if( 0 == strcmp("96368MVWG", boardIdStr) ) {
			pAfeIds[0] = (AFE_CHIP_INT << AFE_CHIP_SHIFT) |
						(AFE_LD_6302 << AFE_LD_SHIFT) |
						(AFE_FE_REV_6302_REV1 << AFE_FE_REV_SHIFT);
		}
		else if( (0 == strcmp("96368VVW", boardIdStr)) || (0 == strcmp("96368SV2", boardIdStr)) ) {
			pAfeIds[0] = (AFE_CHIP_INT << AFE_CHIP_SHIFT) |
						(AFE_LD_ISIL1556 << AFE_LD_SHIFT) |
						(AFE_FE_REV_ISIL_REV1 << AFE_FE_REV_SHIFT);
		}
		else
			BcmAdslCoreDiagWriteStatusString(0, "%s: Unknown boardId \"%s\"\n", __FUNCTION__, boardIdStr);
		pAfeIds[0] |= (AFE_FE_ANNEXA << AFE_FE_ANNEX_SHIFT);
	}
	else
		BcmAdslCoreDiagWriteStatusString(0, "%s: BpGetBoardId() Error(%d)!\n", __FUNCTION__, res);
#endif
	mibLen = sizeof(adslMibInfo);
	pMibInfo = (void *) AdslCoreGetObjectValue (0, NULL, 0, NULL, &mibLen);
	pMibInfo->afeId[0] = pAfeIds[0];
	pMibInfo->afeId[1] = pAfeIds[1];
	return res;
}

void BcmXdslCoreSendAfeInfo(int toPhy)
{
	int	i;
	union {
		dslStatusStruct status;
		dslCommandStruct cmd;
	} t;
	
	ulong	afeIds[2];
	afeDescStruct	afeInfo;

	memset((void*)&afeInfo, 0, sizeof(afeInfo));
	afeInfo.verId = AFE_DESC_VERSION(AFE_DESC_VER_MJ,AFE_DESC_VER_MN);
	afeInfo.size = sizeof(afeDescStruct);
	afeInfo.chipId = PERF->RevID;

	BcmXdslCoreGetAfeBoardId(&afeIds[0]);
	
	for(i = 0; i < MAX_DSL_LINE; i++) {
		if( (1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;	/* Bonding driver with non bonding PHY */
		afeInfo.boardAfeId = afeIds[i];
		if( toPhy ) {
			/* Send info to PHY */
			if( 0 == afeInfo.boardAfeId ) {
#ifdef CONFIG_VDSL_SUPPORTED
				afeInfo.boardAfeId =
					(AFE_CHIP_INT << AFE_CHIP_SHIFT) |
					(AFE_LD_ISIL1556 << AFE_LD_SHIFT) |
					(AFE_FE_REV_ISIL_REV1 << AFE_FE_REV_SHIFT) |
					(AFE_FE_ANNEXA << AFE_FE_ANNEX_SHIFT);
				BcmAdslCoreDiagWriteStatusString (0, "*** Drv:  AfeId is not defined.  Using driver's default afeId(0x%08X)\n",
					afeInfo.boardAfeId);
#else
				BcmAdslCoreDiagWriteStatusString (0, "*** Drv:  AfeId is not defined.  Skip sending to PHY\n");
				continue;
#endif
			}
			t.cmd.command = kDslSendEocCommand | (i << DSL_LINE_SHIFT);
			t.cmd.param.dslClearEocMsg.msgId = kDslAfeInfoCmd;
			gSharedMemAllocFromUserContext=1;
			t.cmd.param.dslClearEocMsg.dataPtr = AdslCoreSharedMemAlloc(sizeof(afeDescStruct));
			memcpy(t.cmd.param.dslClearEocMsg.dataPtr,	(void *)&afeInfo, sizeof(afeDescStruct));
			t.cmd.param.dslClearEocMsg.msgType = sizeof(afeDescStruct);
			BcmCoreCommandHandlerSync(&t.cmd);
			gSharedMemAllocFromUserContext=0;
		}
		else {
			/* Send info to DslDiags */
			t.status.code = kDslReceivedEocCommand | (i << DSL_LINE_SHIFT);
			t.status.param.dslClearEocMsg.msgId = kDslAfeInfoCmd;
			t.status.param.dslClearEocMsg.dataPtr = (void *)&afeInfo;
			t.status.param.dslClearEocMsg.msgType = sizeof(afeDescStruct);
			BcmAdslCoreDiagWriteStatus(&t.status, (char *)&t.status, sizeof(t.status.code) + sizeof(t.status.param.dslClearEocMsg));
		}
		BcmAdslCoreDiagWriteStatusString (0, "*** Drv: Sending afeId(0x%08X) to %s\n",
			afeInfo.boardAfeId, (toPhy) ? "PHY": "DslDiags");
	}
}
#endif

void BcmXdslCoreDiagSendPhyInfo(void)
{
	dslStatusStruct status;

	status.code = kDslReceivedEocCommand;
	status.param.dslClearEocMsg.msgId = kDslPhyInfoCmd;
	status.param.dslClearEocMsg.dataPtr = AdslCoreGetPhyInfo();
	status.param.dslClearEocMsg.msgType = sizeof(adslPhyInfo);
	BcmAdslCoreDiagWriteStatus(&status, (char *)&status, sizeof(status.code) + sizeof(status.param.dslClearEocMsg));

}

void BcmAdslCoreDiagDataLogNotify(int set)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	if(set) {
		gConsoleOutputEnable = 0;
		gDiagDataLog = AC_TRUE;
	}
	else {
		gConsoleOutputEnable = 1;
		gDiagDataLog = AC_FALSE;
	}
#endif
}

char * BcmAdslCoreDiagScrambleString(char *s)
{
	char	*p = s;

	while (*p != 0) {
		*p = ~(*p);
		p++;
	}

	return s;
}


LOCAL Bool BcmAdslCoreCanReset(void)
{
#ifdef VXWORKS
	if (adslCoreAlwaysReset)
		return 1;
	return (BcmAdslCoreStatusSnooper == AdslCoreGetStatusCallback());
#else
	return adslCoreAlwaysReset;
#endif
}

ulong BcmAdslCoreGetCycleCount(void)
{
	ulong	cnt; 
#ifdef _WIN32_WCE
	cnt = 0;
#else
	__asm volatile("mfc0 %0, $9":"=d"(cnt));
#endif
	return(cnt); 
}

ulong BcmAdslCoreCycleTimeElapsedUs(ulong cnt1, ulong cnt0)
{
	cnt1 -= cnt0;

	if (cnt1 < ((ulong) -1) / 1000)
		return (cnt1 * 1000 / adslCoreCyclesPerMs);
	else
		return (cnt1 / (adslCoreCyclesPerMs / 1000));
}

ulong BcmAdslCoreCalibrate(void)
{
	OS_TICKS	tick0, tick1;
	ulong		cnt; 

	if (adslCoreCyclesPerMs != 0)
		return adslCoreCyclesPerMs;

	bcmOsGetTime(&tick1);
	do {
		bcmOsGetTime(&tick0);
	} while (tick0 == tick1);

	cnt = BcmAdslCoreGetCycleCount();
	do {
		bcmOsGetTime(&tick1);
		tick1 = (tick1 - tick0) * BCMOS_MSEC_PER_TICK;
	} while (tick1 < 60);
	cnt = BcmAdslCoreGetCycleCount() - cnt;
	adslCoreCyclesPerMs = cnt / tick1;
	return adslCoreCyclesPerMs;
}

void BcmAdslCoreDelay(ulong timeMs)
{
	OS_TICKS	tick0, tick1;

	bcmOsGetTime(&tick0);
	do {
		bcmOsGetTime(&tick1);
		tick1 = (tick1 - tick0) * BCMOS_MSEC_PER_TICK;
	} while (tick1 < timeMs);
}

void BcmAdslCoreSetWdTimer(ulong timeUs)
{
	TIMER->WatchDogDefCount = timeUs * (FPERIPH/1000000);
	TIMER->WatchDogCtl = 0xFF00;
	TIMER->WatchDogCtl = 0x00FF;
}

/***************************************************************************
** Function Name: BcmAdslCoreCheckBoard
** Description  : Checks the presense of Bcm ADSL core
** Returns      : 1
***************************************************************************/
Bool BcmAdslCoreCheckBoard()
{
	return 1;
}

void BcmAdslCoreStop(void)
{
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
#endif
#ifdef CONFIG_BCM_PWRMNGT_DDR_SR_API
	BcmPwrMngtRegisterLmemAddr(NULL);
#endif
	bcmOsGetTime(&statLastTick);
	BcmAdslDiagDisable();
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	adslCoreDiagState.rcvAddr = pAdslEnum[RCV_ADDR_INTR];
	adslCoreDiagState.rcvCtrl = pAdslEnum[RCV_CTL_INTR];
	adslCoreDiagState.rcvPtr =  pAdslEnum[RCV_PTR_INTR];
	adslCoreDiagState.xmtAddr = pAdslEnum[XMT_ADDR_INTR];
	adslCoreDiagState.xmtCtrl = pAdslEnum[XMT_CTL_INTR];
	adslCoreDiagState.xmtPtr =  pAdslEnum[XMT_PTR_INTR];
#endif
	AdslCoreStop();
	adslCoreStarted = AC_FALSE;
#if defined(__KERNEL__)
	dpcScheduled = AC_FALSE;
#endif
}

void __BcmAdslCoreStart(int diagDataMap, Bool bRestoreImage)
{
	int	i;
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
#endif

	gPhyPCAddr = HOST_LMEM_BASE + 0x5B0;
	adslCoreResetPending = AC_FALSE;
	bcmOsGetTime(&statLastTick);
	pingLastTick = statLastTick;
	AdslCoreHwReset(bRestoreImage);
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	pAdslEnum[RCV_ADDR_INTR] = adslCoreDiagState.rcvAddr;
	pAdslEnum[RCV_CTL_INTR]  = adslCoreDiagState.rcvCtrl;
	pAdslEnum[RCV_PTR_INTR]  = adslCoreDiagState.rcvPtr;
	pAdslEnum[XMT_ADDR_INTR] = adslCoreDiagState.xmtAddr;
	pAdslEnum[XMT_CTL_INTR]  = adslCoreDiagState.xmtCtrl;
	pAdslEnum[XMT_PTR_INTR]  = adslCoreDiagState.xmtPtr;
#endif
	for(i = 0; i < MAX_DSL_LINE; i++) {
		if( (1 == i) && !ADSL_PHY_SUPPORT(kAdslPhyBonding))
			break;	/* Bonding driver with non bonding PHY */
		BcmAdslCoreSetConnectionParam((uchar)i, &adslCoreConnectionParam[i], &adslCoreCfgCmd[i], pAdslCoreCfgProfile[i]);
		BcmCoreCommandHandlerSync(&adslCoreCfgCmd[i]);
		BcmCoreCommandHandlerSync(&adslCoreConnectionParam[i]);
	}

	adslCoreStarted = AC_TRUE;
}

#ifdef  BCM6348_PLL_WORKAROUND

Bool			adslCoreReverbPwrRcv = AC_FALSE;
ulong			adslCoreReverbPwr = 0;
void AdslCoreSetPllClock(void);

void BcmAdslCoreReverbPwrClear(void)
{
	adslCoreReverbPwrRcv = AC_FALSE;
	adslCoreReverbPwr = 0;
}

void BcmAdslCoreReverbPwrSet(ulong pwr)
{
	adslCoreReverbPwrRcv = AC_TRUE;
	adslCoreReverbPwr = pwr;
}

Bool BcmAdslCoreReverbPwrGet(ulong *pPwr)
{
	*pPwr = adslCoreReverbPwr;
	return adslCoreReverbPwrRcv;
}

void BcmAdslCoreHandlePLL(ulong to)
{
	ulong		pwr, tryCnt;
	OS_TICKS	tick0, tick1;

	tryCnt = 0;
	do {
		BcmAdslCoreDelay(10);
		BcmAdslCoreSetTestMode(0, kDslTestEstimatePllPhase);
		BcmAdslCoreDelay(300);
		BcmAdslCoreReverbPwrClear();
		BcmAdslCoreSetTestMode(0, kDslTestReportPllPhaseStatus);
		bcmOsGetTime(&tick0);
		do {
			bcmOsSleep (10/BCMOS_MSEC_PER_TICK);
			bcmOsGetTime(&tick1);
			tick1 = (tick1 - tick0) * BCMOS_MSEC_PER_TICK;
			if (tick1 > to)
				break;
		} while (!BcmAdslCoreReverbPwrGet(&pwr));
		if (!BcmAdslCoreReverbPwrGet(&pwr)) {
			BcmAdslCoreDiagWriteStatusString(0, "No PLL test status");
			break;
		}

		if (pwr < 5000) {
			BcmAdslCoreDiagWriteStatusString(0, "PLL test passed, pwr = %d", pwr);
			break;
		}
		
		BcmAdslCoreDiagWriteStatusString(
			0,
			"PLL test failed, pwr = %d\n"
			"Resetting PLL and ADSL PHY", pwr);
		AdslCoreSetPllClock();

		BcmAdslCoreStop();
		__BcmAdslCoreStart(-1, AC_TRUE);
	} while (++tryCnt < 5);
}

void BcmAdslCoreHandlePLLReset(void)
{
	if (PERF->RevID >= 0x634800A1)
		return;
	adslCoreMuteDiagStatus = AC_TRUE;
	BcmAdslCoreDiagWriteStatusString(0, "<<<Start 6348 PLL check");
	BcmAdslCoreHandlePLL(20);
	adslCoreMuteDiagStatus = AC_FALSE;
	BcmAdslCoreDiagWriteStatusString(0, ">>>6348 PLL check complete");
}

void BcmAdslCoreHandlePLLInit(void)
{
	if (PERF->RevID >= 0x634800A1)
		return;
	BcmCoreCommandHandlerSync(&adslCoreCfgCmd[0]);
	BcmCoreCommandHandlerSync(&adslCoreConnectionParam[0]);
	BcmAdslCoreHandlePLLReset();
}

#endif

void BcmAdslCoreStart(int diagDataMap, Bool bRestoreImage)
{
#ifdef PHY_BLOCK_TEST
	BcmAdslCoreDebugReset();
#endif
	__BcmAdslCoreStart(diagDataMap, bRestoreImage);
#if 1 && defined(BCM6348_PLL_WORKAROUND)
	BcmAdslCoreHandlePLLReset();
#endif
#ifndef PHY_BLOCK_TEST	/* PHY features are populated after __BcmAdslCoreStart() */
	if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
		BcmAdslCoreDebugReset();
#endif
	BcmAdslDiagReset(diagDataMap);
	BcmAdslDiagEnable();
	BcmAdslCoreLogWriteConnectionParam(&adslCoreConnectionParam[0]);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		BcmAdslCoreLogWriteConnectionParam(&adslCoreConnectionParam[1]);
#endif
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	BcmCoreAtmVcSet();
#endif
}

/***************************************************************************
** Function Name: BcmAdslCoreReset
** Description  : Completely resets ADSL MIPS 
** Returns      : None.
***************************************************************************/
void BcmAdslCoreReset(int diagDataMap)
{
	BcmAdslCoreStop();
	BcmAdslCoreStart(diagDataMap, AC_TRUE);
}

/***************************************************************************
** Function Name: BcmAdslCoreStatusSnooper
** Description  : Some DSL status processing
** Returns      : 1
***************************************************************************/
ulong BcmAdslCoreStatusSnooper(dslStatusStruct *status, char *pBuf, int len)
{
	long				val;
	adslMibInfo			*pMibInfo;
	ulong				mibLen;
	static	Bool		bCheckBertEx = false;
	dslStatusCode		statusCode;
	uchar			lineId;

	bcmOsGetTime(&statLastTick);
	pingLastTick = statLastTick;
	
	statusCode = DSL_STATUS_CODE(status->code);
	lineId = DSL_LINE_ID(status->code);

	if (!adslCoreMuteDiagStatus || (kDslExceptionStatus == statusCode))
		BcmAdslCoreDiagStatusSnooper(status, pBuf, len);

	if (bCheckBertEx) {
		bCheckBertEx = false;
		mibLen = sizeof(adslMibInfo);
		pMibInfo = (void *) AdslCoreGetObjectValue (lineId, NULL, 0, NULL, &mibLen);
		if (0 == pMibInfo->adslBertStatus.bertSecCur)
			BcmAdslCoreDiagWriteStatusString(
				lineId,
				"BERT_EX results: totalBits=0x%08lX%08lX, errBits=0x%08lX%08lX",
				pMibInfo->adslBertStatus.bertTotalBits.cntHi, 
				pMibInfo->adslBertStatus.bertTotalBits.cntLo,
				pMibInfo->adslBertStatus.bertErrBits.cntHi, 
				pMibInfo->adslBertStatus.bertErrBits.cntLo);
	}

	switch (statusCode) {
		case kDslEpcAddrStatus:
			gPhyPCAddr = (ulong)(ADSL_ADDR_TO_HOST(status->param.value));
			break;
		case kDslEscapeToG994p1Status:
			if(gSaveStatCtrl.saveStatDisableOnRetrain)
				BcmAdslDiagStatSaveStop();
			break;
		case kDslExceptionStatus:
#if 0 || defined(ADSL_SELF_TEST)
			{
			ulong	*sp, spAddr;
			int		i, stackSize;

			AdslDrvPrintf (TEXT("DSL Exception:\n"));
			sp = (ulong*) status->param.dslException.sp;
			for (i = 0; i < 28; i += 4)
				AdslDrvPrintf (TEXT("R%d=0x%08lX\tR%d=0x%08lX\tR%d=0x%08lX\tR%d=0x%08lX\n"),
					i+1, sp[i], i+2, sp[i+1], i+3, sp[i+2], i+4, sp[i+3]);
			AdslDrvPrintf (TEXT("R29=0x%08lX\tR30=0x%08lX\tR31=0x%08lX\n"), sp[28], sp[29], sp[30]);

			sp = (ulong*) status->param.dslException.argv;
			AdslDrvPrintf (TEXT("argv[0] (EPC) = 0x%08lX\n"), sp[0]);
			for (i = 1; i < status->param.dslException.argc; i++)
				AdslDrvPrintf (TEXT("argv[%d]=0x%08lX\n"), i, sp[i]);
			AdslDrvPrintf (TEXT("Exception stack dump:\n"));

			sp = (ulong*) status->param.dslException.sp;
			spAddr = sp[28];
#ifdef FLATTEN_ADDR_ADJUST
			sp = (ulong *)LMEM_ADDR_TO_HOST(spAddr);
			stackSize = 64;
#else
			sp = (ulong *) status->param.dslException.stackPtr;
			stackSize = status->param.dslException.stackLen;
#endif
			for (i = 0; i < stackSize; i += 8)
				{
				AdslDrvPrintf (TEXT("%08lX: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n"),
					spAddr + (i*4), sp[0], sp[1], sp[2], sp[3], sp[4], sp[5], sp[6], sp[7]);
				sp += 8;
				}
			}
#endif
			if (BcmAdslCoreCanReset())
				adslCoreResetPending = AC_TRUE;
			else {
				int	i;
				for(i = 0; i < MAX_DSL_LINE; i++)
					adslCoreConnectionMode[i] = AC_FALSE;
			}
			break;
		case kDslTrainingStatus:
			val = status->param.dslTrainingInfo.value;
			switch (status->param.dslTrainingInfo.code) {
				case kDslG992p2RxShowtimeActive:
					BcmAdslCoreEnableSnrMarginData(lineId);
					break;
				case kDslG994p1RcvNonStandardInfo:
					BcmAdslCoreNotify(lineId, ACEV_G994_NONSTDINFO_RECEIVED);
					break;
				default:
					break;
			}
			break;

		case kDslShowtimeSNRMarginInfo:
			if (status->param.dslShowtimeSNRMarginInfo.nCarriers != 0)
				BcmAdslCoreDisableSnrMarginData(lineId);
			break;
		case kDslDataAvailStatus:
			BcmAdslCoreAfeTestStatus(status);
			break;
		case kAtmStatus: 
			switch (status->param.atmStatus.code) {
				case kAtmStatBertResult:
					bCheckBertEx = true;
					break;
				default:
					break;
			}
			break;
#ifdef BCM6348_PLL_WORKAROUND
		case kDslTestPllPhaseResult:
			BcmAdslCoreReverbPwrSet(status->param.value);
			break;
#endif
		default:
			break;
	}
	return statLastTick * BCMOS_MSEC_PER_TICK;
}

/***************************************************************************
** Function Name: BcmCoreInterruptHandler
** Description  : Interrupt service routine that is called when there is an
**                core ADSL PHY interrupt.  Signals event to the task to
**                process the interrupt condition.
** Returns      : 1
***************************************************************************/
#ifdef LINUX_KERNEL_2_6
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
LOCAL irqreturn_t BcmCoreInterruptHandler(int irq, void * dev_id)
#else
LOCAL irqreturn_t BcmCoreInterruptHandler(int irq, void * dev_id, struct pt_regs *ptregs)
#endif
#else
UINT32 BcmCoreInterruptHandler (void)
#endif
{
	Bool	bRunTask = AC_FALSE;
	
#ifdef	ADSLDRV_STATUS_PROFILING
	OS_TICKS	tick0, tick1;
	tick0=BcmAdslCoreGetCycleCount();
#endif

	adslCoreIntrCnt++;
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
	BcmAdslCoreDiagIntrHandler();
#endif

	if(AdslCoreIntHandler())
		bRunTask = AC_TRUE;
#if (!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)) && defined(__KERNEL__)
	else
	if(BcmCoreDiagXdslEyeDataAvail())
		bRunTask = AC_TRUE;
#endif
	else
	if (isGdbOn())
		bRunTask = AC_TRUE;
#ifdef PHY_BLOCK_TEST
	else
	if (BcmAdslCoreIsTestCommand())
		bRunTask = AC_TRUE;
#else
	else
	if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
		if (BcmAdslCoreIsTestCommand())
			bRunTask = AC_TRUE;
#endif

	if (bRunTask) {
#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
		bcmOsSemGive (irqSemId);
#else
		if(AC_FALSE==dpcScheduled) {
			bcmOsDpcEnqueue(irqDpcId);
			dpcScheduled=AC_TRUE;
#ifdef ADSLDRV_STATUS_PROFILING
			dpcScheduleTimeStamp=tick0;
#endif
		}
#ifdef SUPPORT_STATUS_BACKUP
		else {
			AdslCoreBkupSbStat();
#ifdef ADSLDRV_STATUS_PROFILING
			intrDuringSchedCnt++;
#endif
		}
#endif /* SUPPORT_STATUS_BACKUP */
#endif	/* Linux */
	}
	
#ifndef BCM_CORE_TEST
	BcmHalInterruptEnable (INTERRUPT_ID_ADSL);
#endif

#ifdef ADSLDRV_STATUS_PROFILING
	tick1=BcmAdslCoreCycleTimeElapsedUs(BcmAdslCoreGetCycleCount(), tick0);
	if(tick1 < intrDurMin)
		intrDurMin=tick1;
	if(tick1 > intrDurMax) {
		intrDurMax=tick1;
		intrDurMaxAtIntrCnt=adslCoreIntrCnt;
	}
	intrDurTotal += tick1;
#endif

#ifdef LINUX_KERNEL_2_6
	return IRQ_HANDLED;
#else
	return 1;
#endif
}


/***************************************************************************
** Function Name: BcmCoreDpc
** Description  : Processing of ADSL PHY interrupt 
** Returns      : None.
***************************************************************************/
LOCAL void BcmCoreDpc(void * arg)
{
	int	numStatProc, numStatToProcMax;
	volatile ulong	*pAdslEnum;
#ifdef ADSLDRV_STATUS_PROFILING
	OS_TICKS	tick0, tick1;
#endif

	if( g_nAdslExit == 1 )
		return;
	
	pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	pAdslEnum[ADSL_INTSTATUS_I]=pAdslEnum[ADSL_INTSTATUS_I];	/* Clear pending status interrupt, and disable it */
	pAdslEnum[ADSL_INTMASK_I] &= (~MSGINT_MASK_BIT);
#ifdef ADSLDRV_STATUS_PROFILING
	tick0=BcmAdslCoreGetCycleCount();
	tick1=BcmAdslCoreCycleTimeElapsedUs(tick0, dpcScheduleTimeStamp);
	gByteAvail=0;
#endif
#if defined(ADSLDRV_STATUS_PROFILING) || defined(SUPPORT_STATUS_BACKUP)
	gByteProc=0;
#endif
	dpcScheduled=AC_FALSE;
	
	adslCoreIsrTaskCnt++;

	BcmAdslCoreDiagIsrTask();
	BcmAdslCoreGdbTask();
	
#ifdef PHY_BLOCK_TEST
	BcmAdslCoreProcessTestCommand();
#else
	if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
		BcmAdslCoreProcessTestCommand();
#endif

#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
	if(BcmCoreDiagLogActive(DIAG_DATA_EYE))
		BcmCoreDiagEyeDataFrameSend();
#endif

#if defined(__KERNEL__) && (defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338))
	if( gDiagDataLog )
		numStatToProcMax = 2;
	else
		numStatToProcMax = 0xffff;
#else
	numStatToProcMax = 0xffff;
#endif

	numStatProc=0;
#ifdef SUPPORT_STATUS_BACKUP
processStat:
#endif
#ifdef ADSLDRV_STATUS_PROFILING
	gByteAvail+=AdslCoreStatusAvailTotal();
#endif

	numStatProc += AdslCoreIntTaskHandler(numStatToProcMax);

#ifdef SUPPORT_STATUS_BACKUP
	if(AdslCoreSwitchCurSbStatToSharedSbStat()) {
		BcmAdslCoreDiagWriteStatusString(0, "%s: bkup SB processed at intrCnt=%lu dpcCnt=%lu, byteProc=%d nStatProc=%d shByteAvail=%d\n",
			__FUNCTION__, adslCoreIntrCnt, adslCoreIsrTaskCnt, gByteProc, numStatProc, AdslCoreStatusAvailTotal());
#ifdef ADSLDRV_STATUS_PROFILING
		BcmXdslCoreDrvProfileInfoPrint();
#endif
		goto processStat;
	}
#endif

	pAdslEnum[ADSL_INTSTATUS_I]=pAdslEnum[ADSL_INTSTATUS_I];	/* Clear pending status interrupt, and enable it */
	pAdslEnum[ADSL_INTMASK_I] |= MSGINT_MASK_BIT;

#if defined( __KERNEL__)
	if( AdslCoreStatusAvail()
#if defined(PHY_BLOCK_TEST)
		|| BcmAdslCoreIsTestCommand()
#endif
		)
		bcmOsDpcEnqueue(irqDpcId);
#endif

#ifdef ADSLDRV_STATUS_PROFILING
	if(tick1 < dpcDelayMin)
		dpcDelayMin=tick1;
	if(tick1 > dpcDelayMax) {
		dpcDelayMax=tick1;
		dpcDelayMaxAtDpcCnt=adslCoreIsrTaskCnt;
		dpcDelayMaxAtByteAvail=gByteAvail;
	}
	
	if(numStatProc > nStatProcMax) {
		nStatProcMax = numStatProc;
		nStatProcMaxAtDpcCnt=adslCoreIsrTaskCnt;
		nStatProcMaxAtByteAvail=gByteAvail;
		nStatProcMaxAtByteProc=gByteProc;
	}
	
	if(gByteAvail > byteAvailMax) {
		byteAvailMaxAtDpcCnt=adslCoreIsrTaskCnt;
		byteAvailMaxAtDpcDelay=tick1;
		byteAvailMaxAtNumStatProc=numStatProc;
		byteAvailMaxAtByteProc=gByteProc;
	}
	
	if(gByteProc > byteProcMax) {
		byteProcMaxAtDpcCnt=adslCoreIsrTaskCnt;
		byteProcMaxAtDpcDelay=tick1;
		byteProcMaxAtNumStatProc=numStatProc;
	}
	dpcDelayTotal += tick1;
	
	tick1=BcmAdslCoreCycleTimeElapsedUs(BcmAdslCoreGetCycleCount(), tick0);
	/* tick1 = dpcDuration */
	if(gByteAvail > byteAvailMax) {
		byteAvailMax=gByteAvail;
		byteAvailMaxAtDpcDur=tick1;
	}
	
	if(gByteProc > byteProcMax) {
		byteProcMax = gByteProc;
		byteProcMaxAtDpcDur=tick1;
	}
	
	if(tick1 < dpcDurMin)
		dpcDurMin=tick1;
	if(tick1 > dpcDurMax) {
		dpcDurMax=tick1;
		dpcDurMaxAtDpcCnt=adslCoreIsrTaskCnt;
		dpcDurMaxAtByteAvail=gByteAvail;
	}
	
	dpcDurTotal += tick1;
#endif
}

#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
/***************************************************************************
** Function Name: BcmCoreIsrTask
** Description  : Runs in a separate thread of execution. Returns from blocking
**                on an event when an core ADSL PHY interrupt.  
** Returns      : None.
***************************************************************************/
LOCAL void BcmCoreIsrTask(void)
{
	while (TRUE) {
		bcmOsSemTake(irqSemId, OS_WAIT_FOREVER);

        if( g_nAdslExit == 1 )
            break;

		BcmCoreDpc(NULL);
	}
}
#endif

#if defined(G992P3) || defined(G992P5)
int BcmAdslCoreLogSaveAdsl2Capabilities(long *cmdData, g992p3DataPumpCapabilities *pCap)
{
	cmdData[0]  = (long)pCap->rcvNTREnabled;
	cmdData[1]  = (long)pCap->shortInitEnabled;
	cmdData[2]  = (long)pCap->diagnosticsModeEnabled;
	cmdData[3]  = (long)pCap->featureSpectrum;
	cmdData[4]  = (long)pCap->featureOverhead;
	cmdData[5]  = (long)pCap->featureTPS_TC[0];
	cmdData[6]  = (long)pCap->featurePMS_TC[0];
	cmdData[7]  = (long)pCap->featureTPS_TC[1];
	cmdData[8]  = (long)pCap->featurePMS_TC[1];
	cmdData[9]  = (long)pCap->featureTPS_TC[2];
	cmdData[10] = (long)pCap->featurePMS_TC[2];
	cmdData[11] = (long)pCap->featureTPS_TC[3];
	cmdData[12] = (long)pCap->featurePMS_TC[3];
	cmdData[13] = (long)pCap->readsl2Upstream;
	cmdData[14] = (long)pCap->readsl2Downstream;
	cmdData[15] = (long)pCap->sizeIDFT;
	cmdData[16] = (long)pCap->fillIFFT;
	cmdData[17] = (long)pCap->minDownOverheadDataRate;
	cmdData[18] = (long)pCap->minUpOverheadDataRate;
	cmdData[19] = (long)pCap->maxDownATM_TPSTC;
	cmdData[20] = (long)pCap->maxUpATM_TPSTC;
	cmdData[21] = (long)pCap->minDownATM_TPS_TC[0];
	cmdData[22] = (long)pCap->maxDownATM_TPS_TC[0];
	cmdData[23] = (long)pCap->minRevDownATM_TPS_TC[0];
	cmdData[24] = (long)pCap->maxDelayDownATM_TPS_TC[0];
	cmdData[25] = (long)pCap->maxErrorDownATM_TPS_TC[0];
	cmdData[26] = (long)pCap->minINPDownATM_TPS_TC[0];
	cmdData[27] = (long)pCap->minUpATM_TPS_TC[0];
	cmdData[28] = (long)pCap->maxUpATM_TPS_TC[0];
	cmdData[29] = (long)pCap->minRevUpATM_TPS_TC[0];
	cmdData[30] = (long)pCap->maxDelayUpATM_TPS_TC[0];
	cmdData[31] = (long)pCap->maxErrorUpATM_TPS_TC[0];
	cmdData[32] = (long)pCap->minINPUpATM_TPS_TC[0];
	cmdData[33] = (long)pCap->maxDownPMS_TC_Latency[0];
	cmdData[34] = (long)pCap->maxUpPMS_TC_Latency[0];
	return 35;
}
#endif

#ifdef G993
int BcmAdslCoreLogSaveVdsl2Capabilities(long *cmdData, g993p2DataPumpCapabilities *pCap)
{
	cmdData[0]  = (long)pCap->verId;
	cmdData[1]  = (long)pCap->size;
	cmdData[2]  = (long)pCap->profileSel;
	cmdData[3]  = (long)pCap->maskUS0;
	cmdData[4]  = (long)pCap->cfgFlags;
	cmdData[5]  = (long)pCap->maskEU;
	cmdData[6]  = (long)pCap->maskADLU;
	return 7;
}
#endif

void BcmAdslCoreLogWriteConnectionParam(dslCommandStruct *pDslCmd)
{
	long	cmdData[120];	/* Worse case was 100 ulongs */
	int		n;

	if (0 == (n = AdslCoreFlattenCommand(pDslCmd, cmdData, sizeof(cmdData))))
		return;
	n >>= 2;

#ifdef	G993
	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
		n--;		/* undo the pointer to carrierInfoG993p2AnnexA */
#endif

#ifdef	G992P3
#ifdef	G992P5
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p))
		n--;		/* undo the pointer to carrierInfoG992p5AnnexA */
#endif
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
		n--;		/* undo the pointer to carrierInfoG992p3AnnexA */
#endif

#ifdef	G992P3
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
		n += BcmAdslCoreLogSaveAdsl2Capabilities(cmdData+n, pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA);
#endif
#ifdef	G992P5
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p))
		n += BcmAdslCoreLogSaveAdsl2Capabilities(cmdData+n, pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA);
#endif

#ifdef	G993
	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
		n += BcmAdslCoreLogSaveVdsl2Capabilities(cmdData+n, pDslCmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA);
#endif

	BcmAdslCoreDiagWriteLog(commandInfoData, cmdData, n);
}

/***************************************************************************
** Function Name: BcmAdslCoreSetConnectionParam
** Description  : Sets default connection parameters
** Returns      : None.
***************************************************************************/

#if defined(G992P3) || defined(G992P5)
LOCAL void SetAdsl2Caps(Boolean bP5Cap, dslCommandStruct *pDslCmd, adslCfgProfile *pAdslCfg)
{
	g992p3DataPumpCapabilities	*pG992p3Cap;
	ulong						phyCfg, modCfg;
	ulong						adsl2Cfg;

	pG992p3Cap = bP5Cap ? 
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA : 
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA;

	if (NULL == pAdslCfg) {
		phyCfg = 0;
		modCfg = kAdslCfgModAny;
		adsl2Cfg = 0;
	}
	else {
		phyCfg = pAdslCfg->adslAnnexAParam;
		modCfg = phyCfg & kAdslCfgModMask;
		adsl2Cfg = pAdslCfg->adsl2Param;
	}

	if (kAdslCfgModAny == (modCfg & kAdslCfgModMask)) {
		modCfg = kAdslCfgModMask;	/* all enabled */
		adsl2Cfg |= kAdsl2CfgReachExOn;
	}

	if (bP5Cap) {
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.downstreamMinCarr = 33;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.downstreamMaxCarr = 511;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.upstreamMinCarr = 6;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.upstreamMaxCarr = 31;
#ifdef G992P1_ANNEX_B
		if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB)) {
#ifdef DTAG_UR2
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.upstreamMinCarr = 33;
#endif
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5.upstreamMaxCarr = 59;
		}
#endif
	}
	
	if (modCfg & kAdslCfgModAdsl2Only)
		pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p3AnnexA | kG994p1;
	
	if (bP5Cap && (modCfg & kAdslCfgModAdsl2pOnly))
		pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p5AnnexA | kG994p1;
	
	pG992p3Cap->rcvNTREnabled = 0;
	pG992p3Cap->shortInitEnabled = 0;
	pG992p3Cap->diagnosticsModeEnabled = 0;
	pG992p3Cap->featureSpectrum = 0x10;
	pG992p3Cap->featureOverhead = 0x0f;
	
	if (0 == (phyCfg & kAdslCfgTpsTcMask))
		pG992p3Cap->featureTPS_TC[0] = kG994p1G992p3AnnexADownATM_TPS_TC | kG994p1G992p3AnnexAUpATM_TPS_TC;
	else {
		pG992p3Cap->featureTPS_TC[0] = 0;
		if (phyCfg & kAdslCfgTpsTcAtmAdsl)
			pG992p3Cap->featureTPS_TC[0] |= kG994p1G992p3AnnexADownATM_TPS_TC | kG994p1G992p3AnnexAUpATM_TPS_TC;
		if (phyCfg & kAdslCfgTpsTcPtmAdsl)
			pG992p3Cap->featureTPS_TC[0] |= kG994p1G992p3AnnexADownPTM_TPS_TC | kG994p1G992p3AnnexAUpPTM_TPS_TC;
	}
	if (phyCfg & kAdslCfgTpsTcPtmPreMask)
		pDslCmd->param.dslModeSpec.capabilities.auxFeatures |= kDslPTMPreemptionDisable;

	pG992p3Cap->featurePMS_TC[0] = 0x03;
	pG992p3Cap->featureTPS_TC[1] = 0;
	pG992p3Cap->featurePMS_TC[1] = 0;
	pG992p3Cap->featureTPS_TC[2] = 0;
	pG992p3Cap->featurePMS_TC[2] = 0;
	pG992p3Cap->featureTPS_TC[3] = 0;
	pG992p3Cap->featurePMS_TC[3] = 0;
		
#ifdef	READSL2
	if (ADSL_PHY_SUPPORT(kAdslPhyAdslReAdsl2) && (modCfg & kAdslCfgModAdsl2Only) && (adsl2Cfg & kAdsl2CfgReachExOn)) {
		pG992p3Cap->featureSpectrum |= kG994p1G992p3AnnexLReachExtended;
		if (0 == (adsl2Cfg & kAdsl2CfgAnnexLMask))
			adsl2Cfg |= kAdsl2CfgAnnexLUpWide | kAdsl2CfgAnnexLUpNarrow;
		pG992p3Cap->readsl2Upstream   = 0;
		pG992p3Cap->readsl2Downstream = kG994p1G992p3AnnexLDownNonoverlap;
		if (adsl2Cfg & kAdsl2CfgAnnexLUpWide)
			pG992p3Cap->readsl2Upstream |= kG994p1G992p3AnnexLUpWideband;
		if (adsl2Cfg & kAdsl2CfgAnnexLUpNarrow)
			pG992p3Cap->readsl2Upstream |= kG994p1G992p3AnnexLUpNarrowband;
		if (adsl2Cfg & kAdsl2CfgAnnexLDnOvlap)
			pG992p3Cap->readsl2Downstream &= ~kG994p1G992p3AnnexLDownNonoverlap;
	}
#endif

	pG992p3Cap->sizeIDFT = bP5Cap ? 9 : 8;
	pG992p3Cap->fillIFFT = 2;
	pG992p3Cap->minDownOverheadDataRate = 3;
	pG992p3Cap->minUpOverheadDataRate = 3;
	pG992p3Cap->maxDownATM_TPSTC = 1;
	pG992p3Cap->maxUpATM_TPSTC = 1;
	pG992p3Cap->minDownATM_TPS_TC[0] = 1;
#ifndef _CFE_
#if ( (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)) && \
	(defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)) )
#if (defined(CONFIG_BCM_ATM_BONDING_ETH) || defined(CONFIG_BCM_ATM_BONDING_ETH_MODULE)) && !defined(NO_BITRATE_LIMIT)
	if (atmbonding_enabled == 1)
		pG992p3Cap->maxDownATM_TPS_TC[0] = bP5Cap ? 2600 : 3800;
	else
		pG992p3Cap->maxDownATM_TPS_TC[0] = 4095;
#else
	pG992p3Cap->maxDownATM_TPS_TC[0] = 4095;
#endif
#else
	pG992p3Cap->maxDownATM_TPS_TC[0] = 4095;
#endif
#else
	pG992p3Cap->maxDownATM_TPS_TC[0] = 4095;
#endif
	pG992p3Cap->minRevDownATM_TPS_TC[0] = 8;
	pG992p3Cap->maxDelayDownATM_TPS_TC[0] = 32;
	pG992p3Cap->maxErrorDownATM_TPS_TC[0] = 2;
	pG992p3Cap->minINPDownATM_TPS_TC[0] = 0;
	pG992p3Cap->minUpATM_TPS_TC[0] = 1;
	pG992p3Cap->maxUpATM_TPS_TC[0] = 2000;
	pG992p3Cap->minRevUpATM_TPS_TC[0] = 8;
	pG992p3Cap->maxDelayUpATM_TPS_TC[0] = 20;
	pG992p3Cap->maxErrorUpATM_TPS_TC[0] = 2;
	pG992p3Cap->minINPUpATM_TPS_TC[0] = 0;
	pG992p3Cap->maxDownPMS_TC_Latency[0] = 4095;
	pG992p3Cap->maxUpPMS_TC_Latency[0] = 4095;
}
#endif /* defined(G992P3) || defined(G992P5) */

LOCAL void BcmAdslCoreSetConnectionParam(unsigned char lineId, dslCommandStruct *pDslCmd, dslCommandStruct *pCfgCmd, adslCfgProfile *pAdslCfg)
{
	ulong	phyCfg, modCfg;
#ifdef CONFIG_VDSL_SUPPORTED
	g993p2DataPumpCapabilities	*pG993p2Cap = NULL;
	XTM_INTERFACE_CFG	interfaceCfg;
	ushort		*pUsIfSupportedTrafficTypes = NULL;
#endif

	if (NULL != pAdslCfg) {
#ifdef CONFIG_VDSL_SUPPORTED
		BcmAdslCoreDiagWriteStatusString (lineId, "Line %d: AnnexCParam=0x%08X AnnexAParam=0x%08X adsl2=0x%08X vdslParam=0x%08X vdslParam1=0x%08X\n",
			lineId, (uint)pAdslCfg->adslAnnexCParam, (uint)pAdslCfg->adslAnnexAParam, (uint) pAdslCfg->adsl2Param,
			(uint)pAdslCfg->vdslParam, (uint)pAdslCfg->vdslParam1);
		if((uint) pAdslCfg->adslAnnexAParam & kDslCfgModVdsl2Only)
			if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
				AdslCoreSetNumTones(lineId, kVdslMibMaxToneNum/2);
			else
				AdslCoreSetNumTones(lineId, kVdslMibMaxToneNum);
		else 
			AdslCoreSetNumTones(lineId, kAdslMibMaxToneNum);
#else
		BcmAdslCoreDiagWriteStatusString (lineId, "AnnexCParam=0x%08X AnnexAParam=0x%08X adsl2=0x%08X\n",
			(uint)pAdslCfg->adslAnnexCParam, (uint)pAdslCfg->adslAnnexAParam, (uint) pAdslCfg->adsl2Param);
#endif
	}
	else
		BcmAdslCoreDiagWriteStatusString (lineId, "BcmAdslCoreSetConnectionParam: no profile\n");

	if (NULL != pAdslCfg) {
#ifdef G992_ANNEXC
		phyCfg = pAdslCfg->adslAnnexCParam;
#else
		phyCfg = pAdslCfg->adslAnnexAParam;
#endif
		adslCoreHsModeSwitchTime[lineId] = pAdslCfg->adslHsModeSwitchTime;
		modCfg = phyCfg & kAdslCfgModMask;
	}
	else {
		phyCfg = 0;
		modCfg = kAdslCfgModAny;
	}
	
#ifdef SUPPORT_DSL_BONDING
	pDslCmd->command = kDslStartPhysicalLayerCmd | (lineId << DSL_LINE_SHIFT);
#else
	pDslCmd->command = kDslStartPhysicalLayerCmd;
#endif
	pDslCmd->param.dslModeSpec.direction = kATU_R;
	pDslCmd->param.dslModeSpec.capabilities.modulations = kG994p1;
	pDslCmd->param.dslModeSpec.capabilities.minDataRate = 1;
	pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 48;
	pDslCmd->param.dslModeSpec.capabilities.features = 0;
	pDslCmd->param.dslModeSpec.capabilities.auxFeatures = (0x00
				| kG994p1PreferToExchangeCaps
				| kG994p1PreferToDecideMode
				| kDslGinpDsSupported
				| kDslGinpUsSupported
				);
#ifdef AEI_VDSL_CUSTOMER_V1000F
	pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 = (0x00
			| kDslFireDsSupported
			| kDslFireUsSupported
			);
#else
	pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 = 0;
#endif
	if ((NULL != pAdslCfg) && (phyCfg & kAdslCfgPwmSyncClockOn))
		AfePwmSetSyncClockFreq(pDslCmd->param.dslModeSpec.capabilities.auxFeatures, pAdslCfg->adslPwmSyncClockFreq);

	pDslCmd->param.dslModeSpec.capabilities.demodCapabilities = (
				kSoftwareTimeErrorDetectionEnabled |
				kHardwareAGCEnabled |
				kDigitalEchoCancellorEnabled |
				kHardwareTimeTrackingEnabled);

	if((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask))
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |=  kDslBitSwapEnabled;

	if (phyCfg & kAdslCfgExtraMask) {
		if (kAdslCfgTrellisOn == (phyCfg & kAdslCfgTrellisMask))
			pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslTrellisEnabled;
		if (0 == (phyCfg & kAdslCfgTrellisMask))
			pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslTrellisEnabled;
		if (kAdslCfgLOSMonitoringOff == (phyCfg & kAdslCfgLOSMonitoringMask))
			pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslAutoRetrainDisabled;

		pDslCmd->param.dslModeSpec.capabilities.noiseMargin = 
			(kAdslCfgDefaultTrainingMargin != pAdslCfg->adslTrainingMarginQ4) ? 
				pAdslCfg->adslTrainingMarginQ4 : 0x60;
		AdslCoreSetShowtimeMarginMonitoring(
			lineId,
			phyCfg & kAdslCfgMarginMonitoringOn ? AC_TRUE : AC_FALSE,
			pAdslCfg->adslShowtimeMarginQ4,
			pAdslCfg->adslLOMTimeThldSec);
	}
	else {
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslTrellisEnabled;
		pDslCmd->param.dslModeSpec.capabilities.noiseMargin = 0x60;
	}

#ifdef G992P1

#ifdef G992P1_ANNEX_A
	if (ADSL_PHY_SUPPORT(kAdslPhyAnnexA)) {
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslG994AnnexAMultimodeEnabled;
		pDslCmd->param.dslModeSpec.capabilities.modulations &= ~(kG994p1 | kT1p413);
		if (kAdslCfgModAny == (modCfg & kAdslCfgModMask)) {
#ifdef CONFIG_VDSL_SUPPORTED
			if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2) || ADSL_PHY_SUPPORT(kAdslPhyBonding))
			{
#ifdef AEI_VDSL_CUSTOMER_NCS
				modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly | kAdslCfgModT1413Only;
#else
				modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly;
#endif
			}
			else
			{
				modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly | kAdslCfgModT1413Only; /* 6367-ADSL only */
			}
#else
			modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly | kAdslCfgModT1413Only;
#endif
		}
#ifdef G994P1
		if (modCfg & (kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly))
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG994p1;
		if (modCfg & kAdslCfgModGdmtOnly)
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p1AnnexA;
#endif

#ifdef T1P413
		if (ADSL_PHY_SUPPORT(kAdslPhyT1P413) && (modCfg & kAdslCfgModT1413Only))
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kT1p413 /* | kG992p1AnnexA */;
#endif
		pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 255;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1.downstreamMinCarr = 33;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1.downstreamMaxCarr = 254;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1.upstreamMinCarr = 6;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1.upstreamMaxCarr = 31;
		pDslCmd->param.dslModeSpec.capabilities.features |= (kG992p1ATM | kG992p1RACK1);
		pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 400;
		pDslCmd->param.dslModeSpec.capabilities.features |= kG992p1HigherBitRates;
		
		if((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask))
			pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslAturXmtPowerCutbackEnabled;
		pDslCmd->param.dslModeSpec.capabilities.subChannelInfo = (
					kSubChannelLS0Upstream | kSubChannelASODownstream);
#ifdef G992P1_ANNEX_A_USED_FOR_G992P2
		if (ADSL_PHY_SUPPORT(kAdslPhyG992p2Init)) {
			if (modCfg & kAdslCfgModGliteOnly)
				pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p2AnnexAB;
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p2.downstreamMinCarr = 33;
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p2.downstreamMaxCarr = 126;
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p2.upstreamMinCarr = 6;
			pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p2.upstreamMaxCarr = 31;
			pDslCmd->param.dslModeSpec.capabilities.features |= kG992p2RACK1;
		}
#endif
	}
#endif /* G992P1_ANNEX_A */

#ifdef G992P1_ANNEX_B
	if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB) || ADSL_PHY_SUPPORT(kAdslPhySADSL)) {
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslG994AnnexAMultimodeEnabled;
		if (kAdslCfgModAny == (modCfg & kAdslCfgModMask))
			modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly | kAdslCfgModT1413Only;
		pDslCmd->param.dslModeSpec.capabilities.modulations &= ~(kG994p1 | kT1p413);
		if (modCfg & kAdslCfgModGdmtOnly)
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p1AnnexB;
		if (modCfg & (kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly))
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG994p1;
		if (modCfg & kAdslCfgModT1413Only)
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kT1p413;
		pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 255;
#ifdef SADSL_DEFINES
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.downstreamMinCarr = 59;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.downstreamMaxCarr = 254;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMinCarr = 7;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMaxCarr = 58;
#else
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.downstreamMinCarr = 61;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.downstreamMaxCarr = 254;
#ifdef DTAG_UR2
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMinCarr = 33;
#else
		pDslCmd->param.dslModeSpec.capabilities.features |= kG992p1AnnexBUpstreamTones1to32;
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMinCarr = 28;
#endif
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMaxCarr = 59;
#endif
		pDslCmd->param.dslModeSpec.capabilities.features |= (kG992p1AnnexBATM | kG992p1AnnexBRACK1) ; 
#if 1
		pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 400;
		pDslCmd->param.dslModeSpec.capabilities.features |= kG992p1HigherBitRates;
#endif
		if((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask))
			pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslAturXmtPowerCutbackEnabled;

		pDslCmd->param.dslModeSpec.capabilities.subChannelInfoAnnexB = (
					kSubChannelLS0Upstream | kSubChannelASODownstream);
#ifdef G992P1_ANNEX_A_USED_FOR_G992P2
		if (ADSL_PHY_SUPPORT(kAdslPhyG992p2Init)) {
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p2AnnexAB;
			pDslCmd->param.dslModeSpec.capabilities.features |= kG992p2RACK1;
		}
#endif
	}
#endif	/* G992P1_ANNEX_B */

#ifdef G992_ANNEXC
	if (kAdslCfgModAny == (modCfg & kAdslCfgModMask))
		modCfg = kAdslCfgModGdmtOnly | kAdslCfgModGliteOnly;
	pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p1AnnexC;
	if (modCfg & kAdslCfgModGliteOnly)
		pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p2AnnexC;
	pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 400;
	/* pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 255; */

	if (pAdslCfg && (pAdslCfg->adslAnnexCParam & kAdslCfgCentilliumCRCWorkAroundMask))
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslCentilliumCRCWorkAroundEnabled;
	
#ifndef G992P1_ANNEX_I
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexC.downstreamMinCarr = 33;
#else
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexC.downstreamMinCarr = 6;
#endif

	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexC.downstreamMaxCarr = 254;
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexC.upstreamMinCarr = 6;
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexC.upstreamMaxCarr = 31;
	pDslCmd->param.dslModeSpec.capabilities.features |= (kG992p1ATM | kG992p1RACK1);
	if (pAdslCfg && (kAdslCfgFBM == (pAdslCfg->adslAnnexCParam & kAdslCfgBitmapMask)))
		pDslCmd->param.dslModeSpec.capabilities.features |= kG992p1AnnexCDBM;

	pDslCmd->param.dslModeSpec.capabilities.features |= kG992p1HigherBitRates;
	pDslCmd->param.dslModeSpec.capabilities.subChannelInfoAnnexC = (
				kSubChannelLS0Upstream | kSubChannelASODownstream);
#if defined(ADSLCORE_ENABLE_LONG_REACH) && defined(G992_ANNEXC_LONG_REACH) 
	pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= kDslAnnexCPilot48 | kDslAnnexCReverb33_63; 
#endif
#endif /* G992_ANNEXC */

#ifdef G992P1_ANNEX_I
	pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= ~kG994p1PreferToDecideMode;
	pDslCmd->param.dslModeSpec.capabilities.auxFeatures |= kG994p1PreferToMPMode;
	pDslCmd->param.dslModeSpec.capabilities.modulations |= (kG992p1AnnexI>>4);
	/* pDslCmd->param.dslModeSpec.capabilities.modulations &= ~kG992p2AnnexC; */
	pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 1023;
	/* pDslCmd->param.dslModeSpec.capabilities.maxDataRate = 255; */

	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.downstreamMinCarr = 33;
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.downstreamMaxCarr = 511;
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.upstreamMinCarr = 6;
	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.upstreamMaxCarr = 31;
	if (NULL == pAdslCfg)
		modCfg |= kAdslCfgUpstreamMax;

	if (kAdslCfgUpstreamMax == (modCfg & kAdslCfgUpstreamModeMask))
		modCfg |= kAdslCfgUpstreamDouble;

	if (modCfg & kAdslCfgUpstreamDouble)
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.upstreamMaxCarr = 
			(modCfg & kAdslCfgNoSpectrumOverlap) ? 62 : 63;
	else
		pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.upstreamMaxCarr = 31;

	pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.downstreamMinCarr = 
		(modCfg & kAdslCfgNoSpectrumOverlap ? 
					pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p1AnnexI.upstreamMaxCarr + 2 :
					7);

	pDslCmd->param.dslModeSpec.capabilities.subChannelInfoAnnexI = (
				kSubChannelLS0Upstream | kSubChannelASODownstream);
#endif /* G992P1_ANNEX_I */

#ifdef G992P3
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2)) 
		SetAdsl2Caps(false, pDslCmd, pAdslCfg);
#endif /* G992P3 */

#ifdef G992P5
	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p)) 
		SetAdsl2Caps(true, pDslCmd, pAdslCfg);
#endif /* G992P5 */

#endif /* G992P1 */

#ifdef CONFIG_VDSL_SUPPORTED
	if(XTMSTS_SUCCESS == BcmXtm_GetInterfaceCfg(PORT_PHY0_PATH0, &interfaceCfg))
		pUsIfSupportedTrafficTypes = (ushort *)((int)&interfaceCfg.ulIfLastChange + 4);
	else
		pUsIfSupportedTrafficTypes = NULL;

	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) {
		pG993p2Cap = pDslCmd->param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA;
	
		if (NULL == pAdslCfg)
			modCfg = kAdslCfgModAny;
		else
			modCfg = pAdslCfg->adslAnnexAParam & kAdslCfgModMask;

		if (kAdslCfgModAny == (modCfg & kAdslCfgModMask))
			modCfg = kAdslCfgModMask;				/* all enabled */
		
		if (modCfg & kDslCfgModVdsl2Only)
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG993p2AnnexA;

		pG993p2Cap->verId = 0;
		pG993p2Cap->size = sizeof(g993p2DataPumpCapabilities);

		if((NULL != pAdslCfg) && (pAdslCfg->vdslParam & (kVdslUS0Mask |kVdslProfileMask1)) ) {
			pG993p2Cap->profileSel = pAdslCfg->vdslParam & (ADSL_PHY_SUPPORT(kAdslPhyVdsl30a) ? kVdslProfileMask1 : kVdslProfileMask);
			pG993p2Cap->maskUS0 = (pAdslCfg->vdslParam & kVdslUS0Mask) >> kVdslUS0MaskShift;
		}

		pG993p2Cap->maskUS0 &= pG993p2Cap->profileSel;
		pG993p2Cap->cfgFlags = CfgFlagsDynamicFFeatureDisable;	/* Per Jin's request, DynF is disabled by default */
		
		if (0 != (phyCfg & kAdslCfgTpsTcMask)) {
			if (0 == (phyCfg & kAdslCfgTpsTcAtmVdsl))
				pG993p2Cap->cfgFlags |= CfgFlagsVdslNoAtmSupport;
			if (0 == (phyCfg & kAdslCfgTpsTcPtmVdsl))
				pG993p2Cap->cfgFlags |= CfgFlagsVdslNoPtmSupport;
		}
		
#ifdef G992P1_ANNEX_B
		if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB))
			pG993p2Cap->maskEU = 0x00060000;
		else
#endif
			pG993p2Cap->maskEU = 0x000703FF ;

		pG993p2Cap->maskADLU = 0x0 ;
	}
#endif	/* #ifdef CONFIG_VDSL_SUPPORTED */

	if ((NULL != pAdslCfg) && (pAdslCfg->adsl2Param & kAdsl2CfgAnnexMEnabled)) {
		if(pAdslCfg->adsl2Param & kAdsl2CfgAnnexMOnly)
			pDslCmd->param.dslModeSpec.capabilities.modulations = 0;
		pDslCmd->param.dslModeSpec.capabilities.modulations |= kG994p1;
		pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 = (pAdslCfg->adsl2Param & kAdsl2CfgAnnexMPsdMask) >> kAdsl2CfgAnnexMPsdShift;
		if(pAdslCfg->adsl2Param & kAdsl2CfgAnnexMpXMask) {
			if (pAdslCfg->adsl2Param & kAdsl2CfgAnnexMp3)
				pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p3AnnexM;
			if (pAdslCfg->adsl2Param & kAdsl2CfgAnnexMp5)
				pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p5AnnexM;
		}
		else
			pDslCmd->param.dslModeSpec.capabilities.modulations |= kG992p3AnnexM | kG992p5AnnexM;
	}

	if((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask) )
		pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 |= (kDslFireDsSupported | kDsl24kbyteInterleavingEnabled);

	if ((NULL != pAdslCfg) &&
	((phyCfg & kAdslCfgDemodCapMask) || ~((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask)))) {
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities |= 
			pAdslCfg->adslDemodCapMask & pAdslCfg->adslDemodCapValue;
		pDslCmd->param.dslModeSpec.capabilities.demodCapabilities &= 
			~(pAdslCfg->adslDemodCapMask & ~pAdslCfg->adslDemodCapValue);
	}

	pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 |= adslCoreEcUpdateMask;

	if ((NULL != pAdslCfg) &&
	((phyCfg & kAdslCfgDemodCap2Mask) || ~((phyCfg & kAdslCfgDemodCapMask) || (phyCfg & kAdslCfgDemodCap2Mask)))) {
		pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 |= 
			pAdslCfg->adslDemodCap2Mask & pAdslCfg->adslDemodCap2Value;
		pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5 &= 
			~(pAdslCfg->adslDemodCap2Mask & ~pAdslCfg->adslDemodCap2Value);
	}
	
	if (NULL != pAdslCfg) {
		if(0 != pAdslCfg->xdslAuxFeaturesMask) {
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures |= 
				pAdslCfg->xdslAuxFeaturesMask & pAdslCfg->xdslAuxFeaturesValue;
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= 
				~(pAdslCfg->xdslAuxFeaturesMask & ~pAdslCfg->xdslAuxFeaturesValue);
		}
#ifdef G993
		if ((ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) && (0 != pAdslCfg->vdslCfgFlagsMask)) {
			pG993p2Cap->cfgFlags |= pAdslCfg->vdslCfgFlagsMask & pAdslCfg->vdslCfgFlagsValue;
			pG993p2Cap->cfgFlags &= ~(pAdslCfg->vdslCfgFlagsMask & ~pAdslCfg->vdslCfgFlagsValue);
		}
#endif
	}
	
#ifdef CONFIG_VDSL_SUPPORTED
	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) {
		if(pUsIfSupportedTrafficTypes) {
			if((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_PTM_RAW) != 0)
				pG993p2Cap->cfgFlags |= CfgFlagsRawEthernetDS;
			else
				pG993p2Cap->cfgFlags &= ~CfgFlagsRawEthernetDS;
		}
		else
			pG993p2Cap->cfgFlags &= ~CfgFlagsRawEthernetDS;
	}
#ifdef SUPPORT_DSL_BONDING
	if(pUsIfSupportedTrafficTypes) {
		if(((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_PTM_BONDED) != 0) &&
			ADSL_PHY_SUPPORT(kAdslPhyBonding))
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures |= kDslPtmBondingSupported;
		else
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= ~kDslPtmBondingSupported;
		
		if(((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_ATM_BONDED) != 0) &&
			ADSL_PHY_SUPPORT(kAdslPhyBonding))
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures |= kDslAtmBondingSupported;
		else
			pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= ~kDslAtmBondingSupported;
	}
	else {
		/* If can not get upperlay info, assume these features are not supported */
		pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= ~kDslAtmBondingSupported;
		pDslCmd->param.dslModeSpec.capabilities.auxFeatures &= ~kDslPtmBondingSupported;
	}
#endif
#endif	/* #ifdef CONFIG_VDSL_SUPPORTED */

#if 0
	printk("***%s:\nAnnexAParam=0x%08x xdslAuxFeaturesMask=0x%08x xdslAuxFeaturesValue=0x%08x\n"
		"adslDemodCapMask=0x%08x adslDemodCapValue=0x%08x\n"
		"adslDemodCap2Mask=0x%08x adslDemodCap2Value=0x%08x\n"
		"vdslCfgFlagsMask=0x%08x vdslCfgFlagsValue=0x%08x\n",
		__FUNCTION__, (uint)pAdslCfg->adslAnnexAParam, (uint)pAdslCfg->xdslAuxFeaturesMask,
		(uint)pAdslCfg->xdslAuxFeaturesValue, (uint)pAdslCfg->adslDemodCapMask,
		(uint)pAdslCfg->adslDemodCapValue, (uint)pAdslCfg->adslDemodCap2Mask,
		(uint)pAdslCfg->adslDemodCap2Value, (uint)pAdslCfg->vdslCfgFlagsMask,
		(uint)pAdslCfg->vdslCfgFlagsValue);
	printk("***%s:\nmodulations=0x%08x auxFeatures=0x%08x demodCapabilities=0x%08x subChannelInfop5=0x%08x\n\n",
		__FUNCTION__,
		(uint)pDslCmd->param.dslModeSpec.capabilities.modulations,
		(uint)pDslCmd->param.dslModeSpec.capabilities.auxFeatures,
		(uint)pDslCmd->param.dslModeSpec.capabilities.demodCapabilities,
		(uint)pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5);
#endif

	adslCoreCfgProfile[lineId].adslDemodCapValue = pDslCmd->param.dslModeSpec.capabilities.demodCapabilities;
	adslCoreCfgProfile[lineId].adslDemodCap2Value = pDslCmd->param.dslModeSpec.capabilities.subChannelInfop5;
	adslCoreCfgProfile[lineId].xdslAuxFeaturesValue = pDslCmd->param.dslModeSpec.capabilities.auxFeatures;
#ifdef G993
	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))
		adslCoreCfgProfile[lineId].vdslCfgFlagsValue = pG993p2Cap->cfgFlags;
#endif

	/* initialize extra configuration command. The code assumes extra cfg. is less that sizeof(dslCommandStruct) */
	{
	long				*pProfCfg, i;
	dslExtraCfgCommand  *pExtraCfg;

	pCfgCmd->command = kDslSendEocCommand;
	pCfgCmd->param.dslClearEocMsg.msgId = kDslExtraPhyCfgCmd;
	pCfgCmd->param.dslClearEocMsg.msgType = 16 | (0 << 16) | kDslClearEocMsgDataVolatile;
	pExtraCfg = (void *) ((char *)&pCfgCmd->param + sizeof(pCfgCmd->param.dslClearEocMsg));
	pCfgCmd->param.dslClearEocMsg.dataPtr = (void *) pExtraCfg;
	pProfCfg = &pAdslCfg->xdslCfg1Mask;

	pExtraCfg->phyExtraCfg[0] = 0;
	pExtraCfg->phyExtraCfg[1] = 0;
	pExtraCfg->phyExtraCfg[2] = 0;
	pExtraCfg->phyExtraCfg[3] = 0;
	/* assumes xdslCfg?Mask/Value are contiguous in adslCfgProfile */
	for (i= 0; i < 4; i++) {
		pExtraCfg->phyExtraCfg[i] |= pProfCfg[0] & pProfCfg[1];
		pExtraCfg->phyExtraCfg[i] &= ~(pProfCfg[0] & ~pProfCfg[1]);
		pProfCfg += 2;
	}
	}

	if (NULL != pAdslCfg) {
		adslCoreCfgProfile[lineId].adslDemodCapMask = pAdslCfg->adslDemodCapMask | adslCoreCfgProfile[lineId].adslDemodCapValue;
		adslCoreCfgProfile[lineId].adslDemodCap2Mask = pAdslCfg->adslDemodCap2Mask | adslCoreCfgProfile[lineId].adslDemodCap2Value;
		adslCoreCfgProfile[lineId].xdslAuxFeaturesMask = pAdslCfg->xdslAuxFeaturesMask | adslCoreCfgProfile[lineId].xdslAuxFeaturesValue;
#ifdef G993
		adslCoreCfgProfile[lineId].vdslParam = pAdslCfg->vdslParam;
		adslCoreCfgProfile[lineId].vdslCfgFlagsMask = pAdslCfg->vdslCfgFlagsMask | adslCoreCfgProfile[lineId].vdslCfgFlagsValue;
		if( NULL != pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA ) {
			g992p3DataPumpCapabilities	*pG992p5Cap = pDslCmd->param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA;
			if( (pAdslCfg->maxDsDataRateKbps <= 0x3FFFC) && ((pAdslCfg->maxDsDataRateKbps >> 2) > 0) &&
				(pAdslCfg->maxUsDataRateKbps <= 0x3FFFC) && ((pAdslCfg->maxUsDataRateKbps >> 2) > 0) &&
				(pAdslCfg->maxAggrDataRateKbps <= 0x7FFF8) && ((pAdslCfg->maxAggrDataRateKbps >> 3) > 0) ) {
				pG992p5Cap->maxUpPTM_TPS_TC[2] = pAdslCfg->maxUsDataRateKbps >> 2;	/* 4000 BPS */
				pG992p5Cap->maxDownPTM_TPS_TC[2] = pAdslCfg->maxDsDataRateKbps >> 2;
				pG992p5Cap->maxDownPTM_TPS_TC[3] = pAdslCfg->maxAggrDataRateKbps >> 3;	/* 8000 BPS */
				adslCoreCfgProfile[lineId].maxUsDataRateKbps = pAdslCfg->maxUsDataRateKbps;
				adslCoreCfgProfile[lineId].maxDsDataRateKbps = pAdslCfg->maxDsDataRateKbps;
				adslCoreCfgProfile[lineId].maxAggrDataRateKbps = pAdslCfg->maxAggrDataRateKbps;
			}
			else {
				/* Invalid configuration.  Let PHY use it's default value */
				pG992p5Cap->maxUpPTM_TPS_TC[2] = 0;
				pG992p5Cap->maxDownPTM_TPS_TC[2] = 0;
				pG992p5Cap->maxDownPTM_TPS_TC[3] = 0;
				adslCoreCfgProfile[lineId].maxUsDataRateKbps = 0;
				adslCoreCfgProfile[lineId].maxDsDataRateKbps = 0;
				adslCoreCfgProfile[lineId].maxAggrDataRateKbps = 0;
			}
		}
#endif
	}
}

/***************************************************************************
** Function Name: BcmAdslCoreConnectionReset
** Description  : Restarts ADSL connection if core is initialized
** Returns      : None.
***************************************************************************/
void BcmAdslCoreConnectionReset(unsigned char lineId)
{
	if (adslCoreConnectionMode[lineId]) {
#if 0
		BcmAdslCoreReset(-1);
#else
		OS_TICKS	tick0, tick1;
	
		BcmAdslCoreConnectionStop(lineId);
		bcmOsGetTime(&tick0);
		do {
			bcmOsGetTime(&tick1);
			tick1 = (tick1 - tick0) * BCMOS_MSEC_PER_TICK;
		} while (tick1 < 40);
		BcmAdslCoreConnectionStart(lineId);
#endif
	}
}

void BcmAdslCoreSendXmtGainCmd(unsigned char lineId, int gain)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslSetXmtGainCmd | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslSetXmtGainCmd;
#endif
	cmd.param.value = gain;
	BcmCoreCommandHandlerSync(&cmd);
}

/**************************************************************************
** Function Name: BcmAdslCoreConfigure
** Description  : This function is called by ADSL driver change ADSL PHY
** Returns      : None.
**************************************************************************/
void BcmAdslCoreConfigure(unsigned char lineId, adslCfgProfile *pAdslCfg)
{
	long	pair;

	if (NULL == pAdslCfg)
		pair = kAdslCfgLineInnerPair;
	else {
#if defined(G992P1_ANNEX_A)
		pair = pAdslCfg->adslAnnexAParam & kAdslCfgLinePairMask;
#elif defined(G992_ANNEXC)
		pair = pAdslCfg->adslAnnexCParam & kAdslCfgLinePairMask;
#else
		pair = kAdslCfgLineInnerPair;
#endif
	}
	BcmAdsl_ConfigureRj11Pair(pair);

#ifdef G992P3
	adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA = &g992p3Param[lineId];
#endif
#ifdef G992P5
	adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA = &g992p5Param[lineId];
#endif
#ifdef G993
	adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA = &g993p2Param[lineId];
#endif
	if (NULL != pAdslCfg) {
		adslCoreCfgProfile[lineId] = *pAdslCfg;
		pAdslCoreCfgProfile[lineId] = &adslCoreCfgProfile[lineId];
	}
	else 
		pAdslCoreCfgProfile[lineId] = NULL;
	
	BcmAdslCoreSetConnectionParam(lineId, &adslCoreConnectionParam[lineId], &adslCoreCfgCmd[lineId], pAdslCfg);
	BcmAdslCoreConnectionReset(lineId);
}


LOCAL int StrCpy(char *dst, char *src)
{
	char	*dst0 = dst;

	while (*src != 0)
		*dst++ = *src++;
	*dst = 0;
	return (dst - dst0);
}

/**************************************************************************
** Function Name: BcmAdslCoreGetVersion
** Description  : Changes ADSL version information
** Returns      : STS_SUCCESS 
**************************************************************************/
void BcmAdslCoreGetVersion(adslVersionInfo *pAdslVer)
{
	adslPhyInfo		*pInfo = AdslCoreGetPhyInfo();
	int				phyVerLen = 0, n;

	pAdslVer->phyMjVerNum = pInfo->mjVerNum;
	pAdslVer->phyMnVerNum = pInfo->mnVerNum;
	
	pAdslVer->phyVerStr[0] = 0;
	if (NULL != pInfo->pVerStr)
		phyVerLen = StrCpy(pAdslVer->phyVerStr, pInfo->pVerStr);
	
	if (ADSL_PHY_SUPPORT(kAdslPhyAnnexA))
		pAdslVer->phyType = kAdslTypeAnnexA;
	else if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB))
		pAdslVer->phyType = kAdslTypeAnnexB;
	else if (ADSL_PHY_SUPPORT(kAdslPhySADSL))
		pAdslVer->phyType = kAdslPhySADSL;
	else if (ADSL_PHY_SUPPORT(kAdslPhyAnnexC))
		pAdslVer->phyType = kAdslTypeAnnexC;
	else
		pAdslVer->phyType = kAdslTypeUnknown;

	pAdslVer->drvMjVerNum = ADSL_DRV_MJ_VERNUM;
	pAdslVer->drvMnVerNum = (ushort) ((ADSL_DRV_MN_VERSYM - 'a') << (16 - 5)) | ADSL_DRV_MN_VERNUM;
	StrCpy(pAdslVer->drvVerStr, ADSL_DRV_VER_STR);
#if 1
	n = StrCpy(pAdslVer->phyVerStr + phyVerLen, ".d");
	StrCpy(pAdslVer->phyVerStr + phyVerLen + n, pAdslVer->drvVerStr);
#endif
}

/**************************************************************************
** Function Name: BcmAdslCoreInit
** Description  : This function is called by ADSL driver to init core
**                ADSL PHY
** Returns      : None.
**************************************************************************/
unsigned char ctlmG994p1ControlXmtNonStdInfoMsg[] = {
    0x02, 0x12, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x50, 0xC0, 0xA8, 0x01, 0x01,
    0x03, 0x0E, 0x00, 0x00, 0x0E, 0xB5, 0x00, 0x50,
    0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x07, 0x02,
    0x09, 
#if defined(ADSLCORE_ENABLE_LONG_REACH) && defined(G992_ANNEXC_LONG_REACH)
	0x01,
#else
	0x00,
#endif
	0x00
};

void BcmAdslCoreInit(void)
{
	int	diagMap;
	int	i;
	
#if defined(CONFIG_SMP) && defined(__KERNEL__) && defined(SPINLOCK_CALL_STAT)
	printk("spinLockCalledEnter=0x%08X\n", (int)spinLockCalledEnter);
	printk("spinLockNestedCallEnter=0x%08X\n", (int)spinLockNestedCallEnter);
	printk("spinLockCalledExit=0x%08X\n", (int)spinLockCalledExit);
	printk("spinLockNestedCallExit=0x%08X\n", (int)spinLockNestedCallExit);
#endif

	BcmAdslCoreCalibrate();
	for(i = 0; i < MAX_DSL_LINE; i++) {
		adslCoreConnectionMode[i] = AC_FALSE;
		acPendingEvents[i] = 0;
	}
	adslCoreMuteDiagStatus = AC_FALSE;
	
	bcmOsGetTime(&statLastTick);
	
#ifdef ADSLDRV_STATUS_PROFILING
	printTicks=statLastTick;
#endif

#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
	bcmOsSemCreate(NULL, &irqSemId);
#if defined(TARG_OS_RTEMS)
	bcmOsTaskCreate("DSLC", 20*1024, 255, BcmCoreIsrTask, 0, &IrqTid);
#else
	bcmOsTaskCreate("BcmCoreIrq", 20*1024, 6, BcmCoreIsrTask, 0, &IrqTid);
#endif
#else /* Linux */
	irqDpcId = bcmOsDpcCreate(BcmCoreDpc, NULL);
#endif

#ifndef BCM_CORE_TEST
	BcmHalMapInterrupt((void*)BcmCoreInterruptHandler, 0, INTERRUPT_ID_ADSL);
	BcmHalInterruptEnable (INTERRUPT_ID_ADSL);
#endif

#ifndef CONFIG_BCM96348
	PERF->blkEnables |= XDSL_CLK_EN;
#endif
	
	diagMap = BcmAdslDiagTaskInit();
	BcmCoreAtmVcInit();
	
	if (NULL == AdslCoreGetStatusCallback())
		AdslCoreSetStatusCallback(BcmAdslCoreStatusSnooper);

	adslCoreResetPending = AC_FALSE;

	AdslCoreSetTime(statLastTick * BCMOS_MSEC_PER_TICK);
	
#ifdef __kerSysGetAfeId
	kerSysGetAfeId(nvramAfeId);
#endif

#if defined(CONFIG_SMP) && defined(__KERNEL__)
	spin_lock_init(&gSpinLock);
#endif

	if ( !AdslCoreInit() ) {
		BcmAdslCoreUninit();
		return;
	}
	
	for(i = 0; i < MAX_DSL_LINE; i++)
		BcmAdslCoreConfigure((uchar)i, pAdslCoreCfgProfile[i]);

#if 1 && defined(BCM6348_PLL_WORKAROUND)
	BcmAdslCoreHandlePLLInit();
#endif

	BcmAdslDiagInit(diagMap);

#if 0 || defined(ADSLCORE_ENABLE_LONG_REACH)
	/* BcmAdslCoreSetOemParameter (ADSL_OEM_G994_VENDOR_ID, "\x00""\x00""\x00""\xF0""\x00\x00""\x00\x00", 8); */
	BcmAdslCoreSetOemParameter (
		ADSL_OEM_G994_XMT_NS_INFO, 
		ctlmG994p1ControlXmtNonStdInfoMsg, 
		sizeof(ctlmG994p1ControlXmtNonStdInfoMsg));
#endif
#ifdef G992P1_ANNEX_I
	BcmAdslCoreSetOemParameter (ADSL_OEM_G994_VENDOR_ID, "\xB5""\x00""BDCM""\x54\x4D", 8);
	//BcmAdslCoreSetOemParameter (ADSL_OEM_G994_VENDOR_ID, "\xB5""\x00""BDCM""\x00\x01", 8);
#endif

#ifdef HMI_QA_SUPPORT
	BcmAdslCoreHmiInit();
#endif

	adslCoreInitialized = AC_TRUE;
	adslCoreStarted = AC_TRUE;
}

/**************************************************************************
** Function Name: BcmAdslCoreConnectionStart
** Description  : This function starts ADSL PHY normal connection operations
** Returns      : None.
**************************************************************************/
void BcmAdslCoreConnectionStart(unsigned char lineId)
{
#ifdef CONFIG_VDSL_SUPPORTED
	XTM_INTERFACE_CFG	interfaceCfg;
	ushort		*pUsIfSupportedTrafficTypes = NULL;
	g993p2DataPumpCapabilities	 *pG993p2Cap =NULL;
#endif

	if (!adslCoreInitialized)
		return;

	BcmAdslCoreLogWriteConnectionParam(&adslCoreConnectionParam[lineId]);
	if (adslCoreXmtGainChanged[lineId]) {
		adslCoreXmtGainChanged[lineId] = AC_FALSE;
		BcmAdslCoreSendXmtGainCmd(lineId, adslCoreXmtGain[lineId]);
	}

	BcmCoreDpcSyncEnter();
	if ((adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.modulations & kT1p413) && 
		!(adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities & kDslG994AnnexAMultimodeEnabled))
	{
		dslCommandStruct cmd;
#ifdef SUPPORT_DSL_BONDING
		cmd.command = kDslSetG994p1T1p413SwitchTimerCmd | (lineId << DSL_LINE_SHIFT);
#else
		cmd.command = kDslSetG994p1T1p413SwitchTimerCmd;
#endif
		cmd.param.value = adslCoreHsModeSwitchTime[lineId];
		AdslCoreCommandHandler(&cmd);
	}
#ifdef CONFIG_VDSL_SUPPORTED
	if(XTMSTS_SUCCESS == BcmXtm_GetInterfaceCfg(PORT_PHY0_PATH0, &interfaceCfg))
		pUsIfSupportedTrafficTypes = (ushort *)((int)&interfaceCfg.ulIfLastChange + 4);
	else
		pUsIfSupportedTrafficTypes = NULL;

	if (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) {
		pG993p2Cap = adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA;
		if(pUsIfSupportedTrafficTypes) {
			if((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_PTM_RAW) != 0)
				pG993p2Cap->cfgFlags |= CfgFlagsRawEthernetDS;
			else
				pG993p2Cap->cfgFlags &= ~CfgFlagsRawEthernetDS;
		}
		else
			pG993p2Cap->cfgFlags &= ~CfgFlagsRawEthernetDS;
	}
#ifdef SUPPORT_DSL_BONDING
	if(pUsIfSupportedTrafficTypes) {
		if(((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_PTM_BONDED) != 0) &&
			ADSL_PHY_SUPPORT(kAdslPhyBonding))
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures |= kDslPtmBondingSupported;
		else
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures &= ~kDslPtmBondingSupported;
		
		if(((*pUsIfSupportedTrafficTypes & SUPPORT_TRAFFIC_TYPE_ATM_BONDED) != 0) &&
			ADSL_PHY_SUPPORT(kAdslPhyBonding))
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures |= kDslAtmBondingSupported;
		else
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures &= ~kDslAtmBondingSupported;
	}
	else {
		/* If can not obtain upperlayer feature info, assume they are not supported */
		adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures &= ~kDslPtmBondingSupported;
		adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.auxFeatures &= ~kDslAtmBondingSupported;
	}
#endif
#endif /* #ifdef CONFIG_VDSL_SUPPORTED */

	AdslCoreCommandHandler(&adslCoreCfgCmd[lineId]);
	AdslCoreCommandHandler(&adslCoreConnectionParam[lineId]);
#if defined(DTAG_UR2) && !defined(ANNEX_J)
	{
	dslCommandStruct	cmd;
	uchar xmtToneMap[8] = {0,0,0,0, 0xFE, 0xFF, 0xFF, 0xFF};
	uchar rcvToneMap = 0xFF;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslTestCmd | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslTestCmd;
#endif
	cmd.param.dslTestSpec.type = kDslTestToneSelection;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtStartTone = 0;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones = 60;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvStartTone = 60;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvNumOfTones = 8;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap = xmtToneMap;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvMap = &rcvToneMap;
	AdslCoreCommandHandler(&cmd);
	}
#endif
	adslCoreConnectionMode[lineId] = AC_TRUE;
	BcmCoreDpcSyncExit();
}

/**************************************************************************
** Function Name: BcmAdslCoreConnectionStop
** Description  : This function stops ADSL PHY connection operations and 
**				  puts ADSL PHY in idle mode
** Returns      : None.
**************************************************************************/
void BcmAdslCoreConnectionStop(unsigned char lineId)
{
	dslCommandStruct	cmd;

	if (!adslCoreInitialized)
		return;

	adslCoreConnectionMode[lineId] = FALSE;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslIdleCmd | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslIdleCmd;
#endif
	BcmCoreCommandHandlerSync(&cmd);
}

/**************************************************************************
** Function Name: BcmAdslCoreConnectionUninit
** Description  : This function disables ADSL PHY
** Returns      : None.
**************************************************************************/
void BcmAdslCoreUninit(void)
{
#ifndef BCM_CORE_TEST
	BcmHalInterruptDisable (INTERRUPT_ID_ADSL);
#ifdef __KERNEL__
	free_irq(INTERRUPT_ID_ADSL, 0);
#endif
#endif

	if (adslCoreInitialized)
		AdslCoreUninit();

	BcmAdslDiagTaskUninit();

#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
	if (irqSemId != 0 )
		bcmOsSemGive (irqSemId);
#endif
	adslCoreInitialized = AC_FALSE;
}

/***************************************************************************
** Function Name: BcmAdslCoreGetConnectionInfo
** Description  : This function is called by ADSL driver to obtain
**                connection info from core ADSL PHY
** Returns      : None.
***************************************************************************/
void BcmAdslCoreGetConnectionInfo(unsigned char lineId, PADSL_CONNECTION_INFO pConnectionInfo)
{
	AdslCoreConnectionRates	acRates;
	OS_TICKS				ticks;
	int 					tMs;
#ifndef PHY_BLOCK_TEST
	int					tPingMs;
#endif

	bcmOsGetTime(&ticks);

#ifdef ADSLDRV_STATUS_PROFILING
	tMs = (ticks - printTicks) * BCMOS_MSEC_PER_TICK;
	if (tMs >= 3000)
		BcmXdslCoreDrvProfileInfoPrint();
#endif

#ifndef PHY_LOOPBACK
	if (adslCoreResetPending) {
		pConnectionInfo->LinkState = BCM_ADSL_LINK_DOWN;
		pConnectionInfo->ulFastDnStreamRate = 0;
		pConnectionInfo->ulInterleavedDnStreamRate = 0;
		pConnectionInfo->ulFastUpStreamRate = 0;
		pConnectionInfo->ulInterleavedUpStreamRate = 0;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
		pConnectionInfo->errorSamplesAvailable = 0;
#endif
#ifdef __KERNEL__
		if (!in_softirq())
#endif
		{
		adslCoreResetPending = AC_FALSE;
		BcmAdslCoreReset(DIAG_DATA_EYE);
		}
		return;
	}

	if (!adslCoreConnectionMode[lineId]) {
		pConnectionInfo->LinkState = BCM_ADSL_LINK_DOWN;
		pConnectionInfo->ulFastDnStreamRate = 0;
		pConnectionInfo->ulInterleavedDnStreamRate = 0;
		pConnectionInfo->ulFastUpStreamRate = 0;
		pConnectionInfo->ulInterleavedUpStreamRate = 0;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
		pConnectionInfo->errorSamplesAvailable = 0;
#endif
		return;
	}
	
	if (AdslCoreLinkState(lineId)) {
		pConnectionInfo->LinkState = BCM_ADSL_LINK_UP;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
		pConnectionInfo->errorSamplesAvailable = AdslCoreGetErrorSampleStatus(lineId);
#endif
		AdslCoreGetConnectionRates (lineId, &acRates);
		pConnectionInfo->ulFastDnStreamRate = acRates.fastDnRate;
		pConnectionInfo->ulInterleavedDnStreamRate = acRates.intlDnRate;
		pConnectionInfo->ulFastUpStreamRate = acRates.fastUpRate;
		pConnectionInfo->ulInterleavedUpStreamRate = acRates.intlUpRate;
	}
	else {
		pConnectionInfo->LinkState = BCM_ADSL_LINK_DOWN;
		pConnectionInfo->ulFastDnStreamRate = 0;
		pConnectionInfo->ulInterleavedDnStreamRate = 0;
		pConnectionInfo->ulFastUpStreamRate = 0;
		pConnectionInfo->ulInterleavedUpStreamRate = 0;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
		pConnectionInfo->errorSamplesAvailable = 0;
#endif
		switch (AdslCoreLinkStateEx(lineId)) {
			case kAdslTrainingG994:	
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G994;
				break;
			case kAdslTrainingG992Started:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G992_STARTED;
				break;
			case kAdslTrainingG992ChanAnalysis:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G992_CHANNEL_ANALYSIS;
				break;
			case kAdslTrainingG992Exchange:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G992_EXCHANGE;
				break;
			case kAdslTrainingG993Started:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G993_STARTED;
				break;
			case kAdslTrainingG993ChanAnalysis:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G993_CHANNEL_ANALYSIS;
				break;
			case kAdslTrainingG993Exchange:
				pConnectionInfo->LinkState = BCM_ADSL_TRAINING_G993_EXCHANGE;
				break;
		}
	}

	//LGD_FOR_TR098
	pConnectionInfo->ShowtimeStart = (ticks - g_ShowtimeStartTicks[lineId])*BCMOS_MSEC_PER_TICK/1000;	

#if !defined(PHY_BLOCK_TEST)
	if (adslCoreStarted && !isGdbOn() && !ADSL_PHY_SUPPORT(kAdslPhyPlayback)) {
		tMs = (ticks - statLastTick) * BCMOS_MSEC_PER_TICK;
		tPingMs = (ticks - pingLastTick) * BCMOS_MSEC_PER_TICK;
		if (
#if defined(AEI_VDSL_TOGGLE_DATAPUMP)
		toggle_reset || 
#endif
		(tMs > ADSL_MIPS_STATUS_TIMEOUT_MS) && (tPingMs > 1000)) 
		{
			dslCommandStruct	cmd;

			BcmAdslCoreDiagWriteStatusString(lineId, "ADSL MIPS inactive. Sending Ping\n");
			cmd.command = kDslPingCmd;
			BcmCoreCommandHandlerSync(&cmd);
			pingLastTick = ticks;

			if (
#if defined(AEI_VDSL_TOGGLE_DATAPUMP)
			toggle_reset || 
#endif
			tMs > (ADSL_MIPS_STATUS_TIMEOUT_MS + 5000)) 
			{
				int i; ulong cycleCnt0;
			
				BcmAdslCoreDiagWriteStatusString(lineId, "Resetting ADSL MIPS\n");
				for(i = 0; i < 20; i++) {
					BcmAdslCoreDiagWriteStatusString(lineId, "PC = 0x%08x\n", *(volatile ulong *)gPhyPCAddr); // (HOST_LMEM_BASE + 0x5E8)
					cycleCnt0 = BcmAdslCoreGetCycleCount();
					while (BcmAdslCoreCycleTimeElapsedUs(BcmAdslCoreGetCycleCount(), cycleCnt0) < 5);
				}
				
				if (BcmAdslCoreCanReset())
					adslCoreResetPending = AC_TRUE;
			}
		}
	}
#endif
#else	/* PHY_LOOPBACK */
		pConnectionInfo->LinkState = BCM_ADSL_LINK_UP;
		pConnectionInfo->ulFastDnStreamRate = 99000000;
		pConnectionInfo->ulInterleavedDnStreamRate = 99000000;
		pConnectionInfo->ulFastUpStreamRate = 50000000;
		pConnectionInfo->ulInterleavedUpStreamRate = 50000000;
#endif

	if (acL3StartTick[lineId] != 0) {
		tMs = (ticks - acL3StartTick[lineId]) * BCMOS_MSEC_PER_TICK;
		if (tMs > 20000) {
			acL3StartTick[lineId] = 0;
			BcmAdslCoreConnectionStart(lineId);
		}
	}

#if defined (AEI_VDSL_CUSTOMER_QWEST_Q1000)
	pConnectionInfo->demodCapabilities = adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities;
	pConnectionInfo->demodCapabilities2 = adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities2;
#endif
}

LOCAL void BcmAdslCoreEnableSnrMarginData(unsigned char lineId)
{
	dslCommandStruct	cmd;
	cmd.command = kDslFilterSNRMarginCmd | (lineId << DSL_LINE_SHIFT);
	cmd.param.value = 0;
	AdslCoreCommandHandler(&cmd);
}

LOCAL void BcmAdslCoreDisableSnrMarginData(unsigned char lineId)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslFilterSNRMarginCmd | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslFilterSNRMarginCmd;
#endif
	cmd.param.value = 1;
	AdslCoreCommandHandler(&cmd);
}

int	BcmAdslCoreSetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	int		res;
	BcmCoreDpcSyncEnter();
	res = AdslCoreSetObjectValue (lineId, objId, objIdLen, dataBuf, dataBufLen);
	BcmCoreDpcSyncExit();
	return res;
}

int	BcmAdslCoreGetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	uchar	*oid = (uchar *) objId;
	int		res;

	BcmCoreDpcSyncEnter();
	if ((objIdLen > 1) && (kOidAdslPrivate == oid[0]) && (kOidAdslPrivShowtimeMargin == oid[1]))
		BcmAdslCoreEnableSnrMarginData(lineId);
	res = AdslCoreGetObjectValue (lineId, objId, objIdLen, dataBuf, dataBufLen);
	BcmCoreDpcSyncExit();
	return res;
}

void BcmAdslCoreStartBERT(unsigned char lineId, unsigned long totalBits)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslDiagStartBERT | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslDiagStartBERT;
#endif
	cmd.param.value = totalBits;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreStopBERT(unsigned char lineId)
{
	dslCommandStruct	cmd;
#ifdef SUPPORT_DSL_BONDING
	cmd.command = kDslDiagStopBERT | (lineId << DSL_LINE_SHIFT);
#else
	cmd.command = kDslDiagStopBERT;
#endif
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreBertStartEx(unsigned char lineId, unsigned long bertSec)
{
	BcmCoreDpcSyncEnter();
	AdslCoreBertStartEx(lineId, bertSec);
	BcmCoreDpcSyncExit();
}

void BcmAdslCoreBertStopEx(unsigned char lineId)
{
	BcmCoreDpcSyncEnter();
	AdslCoreBertStopEx(lineId);
	BcmCoreDpcSyncExit();
}

#ifndef DYING_GASP_API

#if defined(CONFIG_BCM963x8)
/* The BCM6348 cycles per microsecond is really variable since the BCM6348
 * MIPS speed can vary depending on the PLL settings.  However, an appoximate
 * value of 120 will still work OK for the test being done.
 */
#define	CYCLE_PER_US	120
#endif
#define	DG_GLITCH_TO	(100*CYCLE_PER_US)

#if !defined(__KERNEL__) && !defined(_CFE_)
#define BpGetDyingGaspExtIntr(pIntrNum)		*(ulong *) (pIntrNum) = 0
#endif

Bool BcmAdslCoreCheckPowerLoss(void)
{
	ulong	clk0;
	ulong	ulIntr;

	ulIntr = 0;
	clk0 = BcmAdslCoreGetCycleCount();

	UART->Data = 'D';
	UART->Data = '%';
	UART->Data = 'G';

#if defined(CONFIG_BCM963x8) && !defined(VXWORKS)
	do {
		ulong		clk1;

		clk1 = BcmAdslCoreGetCycleCount();		/* time cleared */
		/* wait a little to get new reading */
		while ((BcmAdslCoreGetCycleCount()-clk1) < CYCLE_PER_US*2)
			;
	} while ((PERF->IrqStatus & (1 << (INTERRUPT_ID_DG - INTERNAL_ISR_TABLE_OFFSET))) && ((BcmAdslCoreGetCycleCount() - clk0) < DG_GLITCH_TO));

	if (!(PERF->IrqStatus & (1 << (INTERRUPT_ID_DG - INTERNAL_ISR_TABLE_OFFSET)))) {
		BcmHalInterruptEnable( INTERRUPT_ID_DG );
		AdslDrvPrintf (TEXT(" - Power glitch detected. Duration: %ld us\n"), (BcmAdslCoreGetCycleCount() - clk0)/CYCLE_PER_US);
		return AC_FALSE;
	}
#endif
	return AC_TRUE;
}
#endif /* DYING_GASP_API */

void BcmAdslCoreSendDyingGasp(int powerCtl)
{
    int    i;
    dslCommandStruct    cmd;

    for(i = 0; i < MAX_DSL_LINE; i++)
    if (kAdslTrainingConnected == AdslCoreLinkStateEx(i)) {
        cmd.command = kDslDyingGaspCmd | (i << DSL_LINE_SHIFT);
        cmd.param.value = powerCtl != 0 ? 1 : 0;
#ifdef __KERNEL__
        if (!in_irq())
#endif
            BcmCoreDpcSyncEnter();

        AdslCoreCommandHandler(&cmd);
        AdslCoreCommandHandler(&cmd);

#ifdef __KERNEL__
        if (!in_irq())
#endif
            BcmCoreDpcSyncExit();
    }
    else {
        AdslDrvPrintf (TEXT(" - Power failure detected. ADSL Link down.\n"));
    }

#ifndef DYING_GASP_API
    BcmAdslCoreSetWdTimer(1000000);
#if defined(CONFIG_BCM963x8)
    PERF->blkEnables &= ~(EMAC_CLK_EN | USBS_CLK_EN | SAR_CLK_EN);
#endif
#endif
}

extern long DbgAddr(long a);
#define DBG_ADDR(a)	DbgAddr(a)

void BcmAdslCoreDebugReadMem(unsigned char lineId, DiagDebugData *pDbgCmd)
{
	ULONG				n, frCnt, dataSize;
	DiagProtoFrame		*pDiagFrame;
	DiagDebugData		*pDbgRsp;
	char				testFrame[1000], *pData, *pMem, *pMemEnd;

	pDiagFrame	= (void *) testFrame;
	pDbgRsp		= (void *) pDiagFrame->diagData;
	pData		= (char *) pDbgRsp->diagData;
	*(short *)pDiagFrame->diagHdr.logProtoId = *(short *) LOG_PROTO_ID;
	pDiagFrame->diagHdr.logPartyId	= LOG_PARTY_CLIENT | (lineId << DIAG_PARTY_LINEID_SHIFT);
	pDiagFrame->diagHdr.logCommmand = LOG_CMD_DEBUG;

	pDbgRsp->cmd = pDbgCmd->cmd;
	pDbgRsp->cmdId = pDbgCmd->cmdId;
	dataSize = sizeof(testFrame) - (pData - testFrame) - 16;
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
	if(pDbgCmd->param1 >= 0xfff00000)
		return;
#endif	
	pMem = (char *) DBG_ADDR(pDbgCmd->param1);
	pMemEnd = pMem + pDbgCmd->param2;

	frCnt	 = 0;
	while (pMem != pMemEnd) {
		n = pMemEnd - pMem;
		if (n > dataSize)
			n = dataSize;

		pDbgRsp->param1 = (ulong) pMem;
		pDbgRsp->param2 = n;
		memcpy (pData, pMem, n);
		BcmAdslCoreDiagWrite(testFrame, (pData - testFrame) + n);

		pMem += n;
		frCnt = (frCnt + 1) & 0xFF;

		if (0 == (frCnt & 7))
			BcmAdslCoreDelay(40);
	}
}

extern void XdslCoreSetOvhMsgPrintFlag(uchar lineId, Boolean flag);

#if 0
uint	lmemTestData = 0xa5a5a5a5;
int	lmemTestDataLen = 256; /* # of ulong */
int	lmemTestRun = 1;
uint	lmemTestLoop=0;
void BcmAdslCoreTestLMEM(void)
{
	int	i;
	uint	*pLmemAddr = (uint *)HOST_LMEM_BASE;
	
	printk("&lmemTestRun=%08X, lmemTestDataLen=%d, lmemTestData=%08X\n",
		(uint)&lmemTestRun, lmemTestDataLen, lmemTestData);
	
	while( 1== lmemTestRun) {
		if(0 == lmemTestLoop%1000)
			printk(".");
		for(i=0; i<lmemTestDataLen; i++)
			*(pLmemAddr+i)=lmemTestData;
		for(i=0; i<lmemTestDataLen; i++) {
			if(*(pLmemAddr+i) != lmemTestData) {
				printk("*** Loop: %d word: %d, expected val=%08x, read val=%08x\n",
					lmemTestLoop, i, lmemTestData, *(pLmemAddr+i));
			}
		}
		lmemTestLoop++;
	}
}
#endif

void BcmAdslCoreDebugCmd(unsigned char lineId, void *pMsg)
{
	int	res;
	DiagDebugData	*pDbgCmd = (DiagDebugData *) pMsg;

	switch (pDbgCmd->cmd) {
		case DIAG_DEBUG_CMD_SYNC_CPE_TIME:
			BcmAdslCoreDiagSetSyncTime(pDbgCmd->param1);
			break;
#ifdef CONFIG_VDSL_SUPPORTED
		case DIAG_DEBUG_CMD_IND_READ_6306:
			res = spiTo6306IndirectRead((int)pDbgCmd->param1);
			BcmAdslCoreDiagWriteStatusString(lineId, "Indirect Read 0x%08x = 0x%08x\n",
				(uint)pDbgCmd->param1, (uint)res);
			break;
		case DIAG_DEBUG_CMD_IND_WRITE_6306:
			spiTo6306IndirectWrite((int)pDbgCmd->param1, (int)pDbgCmd->param2);
			BcmAdslCoreDiagWriteStatusString(lineId, "Indirect Write addr: 0x%08x data: 0x%08x\n",
				(uint)pDbgCmd->param1, (uint)pDbgCmd->param2);
			break;
		case DIAG_DEBUG_CMD_READ_6306:
			res = spiTo6306Read((int)pDbgCmd->param1);
			BcmAdslCoreDiagWriteStatusString(lineId, "Direct Read 0x%08x = 0x%08x\n",
				(uint)pDbgCmd->param1, (uint)res);
			break;
		case DIAG_DEBUG_CMD_WRITE_6306:
			spiTo6306Write((int)pDbgCmd->param1, (int)pDbgCmd->param2);
			BcmAdslCoreDiagWriteStatusString(lineId, "Direct Write addr: 0x%08x data: 0x%08x\n",
				(uint)pDbgCmd->param1, (uint)pDbgCmd->param2);
			break;
#endif
		case DIAG_DEBUG_CMD_READ_MEM:
			BcmAdslCoreDebugReadMem(lineId, pDbgCmd);
			break;

		case DIAG_DEBUG_CMD_SET_MEM:
			{
			ulong	*pAddr = (ulong *) DBG_ADDR(pDbgCmd->param1);

			*pAddr = pDbgCmd->param2;
			}
			break;

		case DIAG_DEBUG_CMD_RESET_CONNECTION:
			BcmAdslCoreConnectionReset(lineId);
			break;

		case DIAG_DEBUG_CMD_RESET_PHY:
#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
			if(pBkupImage) {
				vfree(pBkupImage);
				pBkupImage = NULL;
				bkupLmemSize = 0;
				bkupSdramSize = 0;
			}
#endif
			BcmAdslCoreReset(DIAG_DATA_EYE);

			if (DIAG_DEBUG_CMD_LOG_DATA == diagDebugCmd.cmd) {
				BcmAdslCoreDiagStartLog(diagDebugCmd.param1, diagDebugCmd.param2);
				diagDebugCmd.cmd = 0;
			}
			break;

#if 1 && defined(BCM6348_PLL_WORKAROUND)
		case 99:
			AdslCoreSetPllClock();
			BcmAdslCoreReset(DIAG_DATA_EYE);
			break;
#endif

		case DIAG_DEBUG_CMD_RESET_CHIP:
			BcmAdslCoreSetWdTimer (1000);
			break;

		case DIAG_DEBUG_CMD_EXEC_FUNC:
			{
			int (*pExecFunc) (ulong param) = (void *) DBG_ADDR(pDbgCmd->param1);

			res = (*pExecFunc) (pDbgCmd->param2);
			BcmAdslCoreDiagWriteStatusString(lineId, "CMD_EXEC_FUNC at 0x%X param=%d: result=%d", pExecFunc, pDbgCmd->param2, res);
			}
			break;

		case DIAG_DEBUG_CMD_WRITE_FILE:
			if (ADSL_MIPS_LMEM_ADDR(pDbgCmd->param1))
				pDbgCmd->param1 = (ulong)LMEM_ADDR_TO_HOST(pDbgCmd->param1);
			else if (ADSL_MIPS_SDRAM_ADDR(pDbgCmd->param1))
				pDbgCmd->param1 = (ulong)SDRAM_ADDR_TO_HOST(pDbgCmd->param1);
			BcmAdslCoreSendBuffer(kDslDataAvailStatus | (lineId << DSL_LINE_SHIFT), (void*) pDbgCmd->param1, pDbgCmd->param2);
			break;

		case DIAG_DEBUG_CMD_LOG_SAMPLES:
			{
			dslCommandStruct	cmd;
			int					nDmaBlk;

			BcmAdslCoreDiagDmaInit();

			nDmaBlk = BcmAdslCoreDiagGetDmaBlockNum();
			if (nDmaBlk < 3)
				break;
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
			BcmAdslCoreDiagSetBufDesc();
#endif
			bcmOsSleep (100/BCMOS_MSEC_PER_TICK);
			cmd.command = kDslDiagSetupCmd;
			cmd.param.dslDiagSpec.setup = kDslDiagEnableEyeData | kDslDiagEnableLogData;
			cmd.param.dslDiagSpec.eyeConstIndex1 = 63;
			cmd.param.dslDiagSpec.eyeConstIndex2 = 64;
			cmd.param.dslDiagSpec.logTime = (ulong) (1 - nDmaBlk);
			BcmCoreCommandHandlerSync(&cmd);
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
			bcmOsSleep (100/BCMOS_MSEC_PER_TICK);
			BcmAdslCoreSendDmaBuffers(kDslDataAvailStatus, nDmaBlk - 2);
#endif
			}
			break;

		case DIAG_DEBUG_CMD_LOG_DATA:
			if (pDbgCmd->cmdId == DIAG_DEBUG_CMD_LOG_AFTER_RESET)
				diagDebugCmd = *pDbgCmd;
			else
				BcmAdslCoreDiagStartLog(pDbgCmd->param1, pDbgCmd->param2);
			break;

#if 1 || defined(PHY_BLOCK_TEST)
		case DIAG_DEBUG_CMD_PLAYBACK_STOP:
			BcmAdslCoreDebugPlaybackStop();
			break;

		case DIAG_DEBUG_CMD_PLAYBACK_RESUME:
			BcmAdslCoreDebugPlaybackResume();
			break;
#endif

		case DIAG_DEBUG_CMD_G992P3_DEBUG:
			XdslCoreSetOvhMsgPrintFlag(lineId, (pDbgCmd->param1 != 0));
			break;
		case DIAG_DEBUG_CMD_CLEAREOC_LOOPBACK:
			{
			extern int ClearEOCLoopBackEnabled;
			
			ClearEOCLoopBackEnabled= (pDbgCmd->param1 != 0);
			}
			break;
		case DIAG_DEBUG_CMD_SET_OEM:
		{
			char *str = (char *)pDbgCmd + sizeof(DiagDebugData);
			AdslDrvPrintf("DIAG_DEBUG_CMD_SET_OEM: paramId = %lu len = %lu\n", pDbgCmd->param1,pDbgCmd->param2);
			str[pDbgCmd->param2] = 0;
			AdslDrvPrintf("str: %s\n", str);
			res=AdslCoreSetOemParameter(pDbgCmd->param1,str,(int)pDbgCmd->param2);
			AdslDrvPrintf("Set len = %d\n", res);
		}
			break;
		case DIAG_DEBUG_CMD_G992P3_DIAG_MODE: 
			BcmAdslCoreSetAdslDiagMode(lineId, pDbgCmd->param1);
			break;
		case DIAG_DEBUG_CMD_ANNEXM_CFG:
			if (pDbgCmd->param2 != 0) {
				if (-1 == (long) pDbgCmd->param2)
					pDbgCmd->param2 = 0;
				adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA->readsl2Upstream = 
				adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA->readsl2Upstream =
					pDbgCmd->param2 & 0xFF;
				adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p3AnnexA->readsl2Downstream = 
				adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p5AnnexA->readsl2Downstream =
					(pDbgCmd->param2 >> 8) & 0xFF;
			}
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5 = pDbgCmd->param1 & (kAdsl2CfgAnnexMPsdMask >> kAdsl2CfgAnnexMPsdShift);
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5 |= adslCoreEcUpdateMask;
			BcmAdslCoreConnectionReset(lineId);
			break;
		case DIAG_DEBUG_CMD_SET_L2_TIMEOUT:
			AdslCoreSetL2Timeout(pDbgCmd->param1);
			break;
	}
}

void BcmAdslCoreResetStatCounters(unsigned char lineId)
{
	BcmCoreDpcSyncEnter();
	AdslCoreResetStatCounters(lineId);
	BcmCoreDpcSyncExit();
}

void BcmAdslCoreSetTestMode(unsigned char lineId, int testMode)
{
	dslCommandStruct	cmd;

	if (ADSL_TEST_DIAGMODE == testMode) {
		BcmAdslCoreSetAdslDiagMode(lineId, 1);
		return;
	}

	BcmCoreDpcSyncEnter();
	if (ADSL_TEST_L3 == testMode) {
		acL3Requested[lineId]= 1;
		AdslCoreSetL3(lineId);
	}
	else if (ADSL_TEST_L0 == testMode) {
		AdslCoreSetL0(lineId);
	}
	else {
#ifdef NTR_SUPPORT
		if( kDslTestNtrStart == testMode ) {
			long len;
			adslMibInfo *pMib = (adslMibInfo *)AdslCoreGetObjectValue (lineId, NULL, 0, NULL, &len);
			len = sizeof(dslNtrCfg);
			cmd.command = kDslSendEocCommand | (lineId << DSL_LINE_SHIFT);
			cmd.param.dslClearEocMsg.msgId = kDslNtrConfig;
			cmd.param.dslClearEocMsg.msgType = len;
			gSharedMemAllocFromUserContext=1;
			cmd.param.dslClearEocMsg.dataPtr = (char *) AdslCoreSharedMemAlloc(len);
			memcpy(cmd.param.dslClearEocMsg.dataPtr, &pMib->ntrCfg, len);
			AdslCoreCommandHandler(&cmd);
			gSharedMemAllocFromUserContext=0;
		}
#endif
		cmd.command = kDslTestCmd | (lineId << DSL_LINE_SHIFT);
		cmd.param.dslTestSpec.type = testMode;
		AdslCoreCommandHandler(&cmd);
	}
	BcmCoreDpcSyncExit();
}
void BcmAdslCoreSetTestExecutionDelay(unsigned char lineId, int testMode, ulong value)
{
	dslCommandStruct	cmd;

	BcmCoreDpcSyncEnter();
	cmd.command = kDslTestCmd | (lineId << DSL_LINE_SHIFT);
	cmd.param.dslTestSpec.type =kDslTestExecuteDelay ;
	cmd.param.dslTestSpec.param.value=value;
	AdslCoreCommandHandler(&cmd);
	BcmCoreDpcSyncExit();
}
void BcmAdslCoreSetAdslDiagMode(unsigned char lineId, int diagMode)
{
#ifdef G992P3
	dslCommandStruct	cmd;
	adslMibInfo			*pMibInfo;
	ulong				mibLen;

	if (!adslCoreStarted)
		return;

	if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2) || ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) {
		cmd.command = kDslIdleCmd | (lineId << DSL_LINE_SHIFT);
		BcmCoreCommandHandlerSync(&cmd);

		BcmAdslCoreDelay(40);

		pMibInfo = (void *) AdslCoreGetObjectValue (lineId, NULL, 0, NULL, &mibLen);
		if (diagMode != 0)
			pMibInfo->adslPhys.adslLDCompleted = 1;		/* 1-Started, 2-Completed, -1-Failed */
		g992p5Param[lineId].diagnosticsModeEnabled = 
		g992p3Param[lineId].diagnosticsModeEnabled = (diagMode != 0) ? 1 : 0;
		BcmCoreCommandHandlerSync(&adslCoreCfgCmd[lineId]);
		BcmCoreCommandHandlerSync(&adslCoreConnectionParam[lineId]);
		g992p3Param[lineId].diagnosticsModeEnabled = 0;
		g992p5Param[lineId].diagnosticsModeEnabled = 0;
	}
#endif
}

void BcmAdslCoreSelectTones(
	unsigned char	lineId,
	int		xmtStartTone,
	int		xmtNumTones,
	int		rcvStartTone,
	int		rcvNumTones,
	char	*xmtToneMap,
	char	*rcvToneMap)
{
	dslCommandStruct	cmd;

	cmd.command = kDslTestCmd | (lineId << DSL_LINE_SHIFT);
	cmd.param.dslTestSpec.type = kDslTestToneSelection;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtStartTone = xmtStartTone;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones = xmtNumTones;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvStartTone = rcvStartTone;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvNumOfTones = rcvNumTones;
	cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap = xmtToneMap;
	cmd.param.dslTestSpec.param.toneSelectSpec.rcvMap = rcvToneMap;
#if 1
	{
		int		i;

		AdslDrvPrintf(TEXT("xmtStartTone=%ld, xmtNumTones=%ld, rcvStartTone=%ld, rcvNumTones=%ld \nxmtToneMap="),
			cmd.param.dslTestSpec.param.toneSelectSpec.xmtStartTone,
			cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones,
			cmd.param.dslTestSpec.param.toneSelectSpec.rcvStartTone,
			cmd.param.dslTestSpec.param.toneSelectSpec.rcvNumOfTones);
		for (i = 0; i < ((cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones+7)>>3); i++)
			AdslDrvPrintf(TEXT("%02X "), cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap[i]);
		AdslDrvPrintf(TEXT("\nrcvToneMap="));
		for (i = 0; i < ((cmd.param.dslTestSpec.param.toneSelectSpec.rcvNumOfTones+7)>>3); i++)
			AdslDrvPrintf(TEXT("%02X "), cmd.param.dslTestSpec.param.toneSelectSpec.rcvMap[i]);
	}
	AdslDrvPrintf(TEXT("\n"));
#endif

	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreDiagRearrangeSelectTones(int *xmtStartTone, int *xmtNumTones,int *rcvStartTone, int *rcvNumTones, char *rcvToneMap, char *xmtToneMap)
{
	*rcvStartTone = *xmtNumTones - 8;
	*rcvNumTones  = 512 - *rcvStartTone;
	if ((*rcvToneMap - *xmtToneMap) != (32 >> 3)) {
		int		i;
			for (i = (32 >> 3); i < (*xmtNumTones >> 3); i++)
			xmtToneMap[i] = rcvToneMap[i - (32 >> 3)];
	}
	//rcvToneMap   += (*rcvStartTone - 32) >> 3;
	return;
}

void BcmAdslCoreDiagSelectTones(
	unsigned char	lineId,
	int		xmtStartTone,
	int		xmtNumTones,
	int		rcvStartTone,
	int		rcvNumTones,
	char	*xmtToneMap,
	char	*rcvToneMap)
{
#ifdef G992P1_ANNEX_B
	if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB)) {
		xmtNumTones  = (adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG992p1AnnexB.upstreamMaxCarr + 7) & ~7;
		BcmAdslCoreDiagRearrangeSelectTones(&xmtStartTone, &xmtNumTones, &rcvStartTone, &rcvNumTones,rcvToneMap,xmtToneMap);
	}
#endif
	if((adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.modulations & kG992p3AnnexM) ||
		(adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.modulations & kG992p5AnnexM) )
	{
		xmtNumTones  = 64;
		BcmAdslCoreDiagRearrangeSelectTones(&xmtStartTone, &xmtNumTones, &rcvStartTone, &rcvNumTones, rcvToneMap, xmtToneMap);
		rcvToneMap+=(rcvStartTone - 32) >> 3;
	}
	BcmAdslCoreSelectTones(lineId, xmtStartTone,xmtNumTones, rcvStartTone,rcvNumTones, xmtToneMap,rcvToneMap);
}

Bool BcmAdslCoreSetSDRAMBaseAddr(void *pAddr)
{
	return AdslCoreSetSDRAMBaseAddr(pAddr);
}

Bool BcmAdslCoreSetVcEntry (int gfc, int port, int vpi, int vci, int pti_clp)
{
	return AdslCoreSetVcEntry(gfc, port, vpi, vci, pti_clp);
}

Bool BcmAdslCoreSetGfc2VcMapping(Bool bOn)
{
	dslCommandStruct	cmd;

	cmd.command = kDslAtmGfcMappingCmd;
	cmd.param.value = bOn;
	BcmCoreCommandHandlerSync(&cmd);
	return AC_TRUE;
}

Bool BcmAdslCoreSetAtmLoopbackMode(void)
{
	dslCommandStruct	cmd;

#if 1
	BcmAdslCoreReset(DIAG_DATA_EYE);
#else
	BcmAdslCoreConnectionStop();
#endif
	BcmAdslCoreDelay(100);

	cmd.command = kDslLoopbackCmd;
	BcmCoreCommandHandlerSync(&cmd);
	return AC_TRUE;
}

int BcmAdslCoreGetOemParameter (int paramId, void *buf, int len)
{
	return AdslCoreGetOemParameter (paramId, buf, len);
}

int BcmAdslCoreSetOemParameter (int paramId, void *buf, int len)
{
	return AdslCoreSetOemParameter (paramId, buf, len);
}

int BcmAdslCoreSetXmtGain(unsigned char lineId, int gain)
{
	if ((gain != ADSL_XMT_GAIN_AUTO) && ((gain < -22) || (gain > 2)))
		return BCMADSL_STATUS_ERROR;

#if 1
	if (gain != adslCoreXmtGain[lineId]) {
		adslCoreXmtGain[lineId] = gain;
		adslCoreXmtGainChanged[lineId] = AC_TRUE;
		if (adslCoreConnectionMode[lineId])
			BcmAdslCoreConnectionReset(lineId);
	}
#else
	adslCoreXmtGain = gain;
	BcmAdslCoreSendXmtGainCmd(gain);
#endif
	return BCMADSL_STATUS_SUCCESS;
}


int  BcmAdslCoreGetSelfTestMode(void)
{
	return AdslCoreGetSelfTestMode();
}

void BcmAdslCoreSetSelfTestMode(int stMode)
{
	AdslCoreSetSelfTestMode(stMode);
}

int  BcmAdslCoreGetSelfTestResults(void)
{
	return AdslCoreGetSelfTestResults();
}



ADSL_LINK_STATE BcmAdslCoreGetEvent (unsigned char lineId)
{
	int		adslState;

	if (0 == acPendingEvents[lineId])
		adslState = -1;
	else if (acPendingEvents[lineId] & ACEV_LINK_UP) {
		adslState = BCM_ADSL_LINK_UP;
		acPendingEvents[lineId] &= ~ACEV_LINK_UP;
	}
	else if (acPendingEvents[lineId] & ACEV_LINK_DOWN) {
		adslState = BCM_ADSL_LINK_DOWN;
		acPendingEvents[lineId] &= ~ACEV_LINK_DOWN;
	}
	else if (acPendingEvents[lineId] & ACEV_G997_FRAME_RCV) {
		adslState = BCM_ADSL_G997_FRAME_RECEIVED;
		acPendingEvents[lineId] &= ~ACEV_G997_FRAME_RCV;
	}
	else if (acPendingEvents[lineId] & ACEV_G997_FRAME_SENT) {
		adslState = BCM_ADSL_G997_FRAME_SENT;
		acPendingEvents[lineId] &= ~ACEV_G997_FRAME_SENT;
	}
	else if (acPendingEvents[lineId] & ACEV_SWITCH_RJ11_PAIR) {
		adslState = ADSL_SWITCH_RJ11_PAIR;
		acPendingEvents[lineId] &= ~ACEV_SWITCH_RJ11_PAIR;
	}
	else if (acPendingEvents[lineId] & ACEV_G994_NONSTDINFO_RECEIVED) {
		adslState = BCM_ADSL_G994_NONSTDINFO_RECEIVED;
		acPendingEvents[lineId] &= ~ACEV_G994_NONSTDINFO_RECEIVED;
	}
	else {
		adslState = BCM_ADSL_EVENT;
		acPendingEvents[lineId] = 0;
	}

	return adslState;
}


Bool BcmAdslCoreG997SendData(unsigned char lineId, void *buf, int len)
{
	Bool	bRet;

	BcmCoreDpcSyncEnter();
	bRet = AdslCoreG997SendData(lineId, buf, len);
	BcmCoreDpcSyncExit();
	return bRet;
}

void *BcmAdslCoreG997FrameGet(unsigned char lineId, int *pLen)
{
	return AdslCoreG997FrameGet(lineId, pLen);
}

void *BcmAdslCoreG997FrameGetNext(unsigned char lineId, int *pLen)
{
	return AdslCoreG997FrameGetNext(lineId, pLen);
}

void BcmAdslCoreG997FrameFinished(unsigned char lineId)
{
	BcmCoreDpcSyncEnter();
	AdslCoreG997FrameFinished(lineId);
	BcmCoreDpcSyncExit();
}



void BcmAdslCoreNotify(unsigned char lineId, long acEvent)
{
	if (ACEV_LINK_POWER_L3 == acEvent) {
		dslCommandStruct	cmd;
		cmd.command = kDslIdleCmd | (lineId << DSL_LINE_SHIFT);
		AdslCoreCommandHandler(&cmd);

		if (!acL3Requested[lineId]) {
			bcmOsGetTime(&acL3StartTick[lineId]);
			if (0 == acL3StartTick[lineId])
				acL3StartTick[lineId]= 1;
		}
		acL3Requested[lineId] = 0;
		return;
	}
	else if (ACEV_RATE_CHANGE == acEvent) {
		/* TO DO: Uncomment when the DS packet loss is fixed
		BcmXdslNotifyRateChange(lineId);
		*/
		return;
	}

	acPendingEvents[lineId] |= acEvent;
	(*bcmNotify)(lineId);
}

int BcmAdslCoreGetConstellationPoints (int toneId, ADSL_CONSTELLATION_POINT *pointBuf, int numPoints)
{
	return BcmAdslDiagGetConstellationPoints (toneId, pointBuf, numPoints);
}

/*
**
**	ATM EOP workaround functions
**
*/


void BcmAdslCoreAtmSetPortId(int path, int portId)
{
	dslCommandStruct	cmd;
	
	cmd.command = kDslAtmVcControlCmd;
	if(path==0)
		cmd.param.value = kDslAtmSetIntlPortId | portId;
	else if(path==1)
		cmd.param.value = kDslAtmSetFastPortId | portId;
	BcmCoreCommandHandlerSync(&cmd);
}

ulong   atmVcTable[16] = { 0 };
ulong	atmVcCnt = 0;
#define ATM_VC_SIZE		sizeof(atmVcTable)/sizeof(atmVcTable[0])

void BcmCoreAtmVcInit(void)
{
	atmVcCnt = 0;
	AdslMibByteClear(ATM_VC_SIZE, atmVcTable);
}

void BcmCoreAtmVcSet(void)
{
	dslCommandStruct	cmd;
	int					i;

	cmd.command = kDslAtmVcControlCmd;
	cmd.param.value = kDslAtmVcClear;
	BcmCoreCommandHandlerSync(&cmd);

	for (i = 0; i < atmVcCnt; i++) {
		cmd.command = kDslAtmVcControlCmd;
		cmd.param.value = kDslAtmVcAddEntry | atmVcTable[i];
		BcmCoreCommandHandlerSync(&cmd);
	}
}

void BcmAdslCoreAtmClearVcTable(void)
{
	dslCommandStruct	cmd;

	atmVcCnt = 0;
	AdslMibByteClear(ATM_VC_SIZE, atmVcTable);

	cmd.command = kDslAtmVcControlCmd;
	cmd.param.value = kDslAtmVcClear;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreAtmAddVc(int vpi, int vci)
{
	dslCommandStruct	cmd;
	int					i;
	ulong				vc;

	vc = (vpi << 16) | vci;
	for (i = 0; i < atmVcCnt; i++) {
	  if (vc == atmVcTable[i])
		break;
	}
	if ((i == atmVcCnt) && (atmVcCnt < ATM_VC_SIZE)) {
	  atmVcTable[atmVcCnt] = vc;
	  atmVcCnt++;
	}

	cmd.command = kDslAtmVcControlCmd;
	cmd.param.value = kDslAtmVcAddEntry | vc;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreAtmDeleteVc(int vpi, int vci)
{
	dslCommandStruct	cmd;
	int					i;
	ulong				vc;

	vc = (vpi << 16) | vci;
	for (i = 0; i < atmVcCnt; i++) {
	  if (vc == atmVcTable[i]) {
		atmVcTable[i] = atmVcTable[atmVcCnt];
		atmVcCnt--;
		atmVcTable[atmVcCnt] = 0;
		break;
	  }
	}

	cmd.command = kDslAtmVcControlCmd;
	cmd.param.value = kDslAtmVcDeleteEntry | vc;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreAtmSetMaxSdu(unsigned short maxsdu)
{
	dslCommandStruct	cmd;

	cmd.command = kDslAtmVcControlCmd;
	cmd.param.value = kDslAtmSetMaxSDU | maxsdu;
	BcmCoreCommandHandlerSync(&cmd);
}

#if 0
void AddVcTest(ulong vci)
{
	BcmAdslCoreAtmAddVc(vci >> 16, vci & 0xFFFF);
}

void DelVcTest(ulong vci)
{
	BcmAdslCoreAtmDeleteVc(vci >> 16, vci & 0xFFFF);
}
#endif

/*
**
**	DslDiags common functions
**
*/
#ifdef SUPPORT_DSL_BONDING
static	void	*diagStatDataPtr[MAX_DSL_LINE] = {NULL, NULL};
#else
static	void	*diagStatDataPtr[MAX_DSL_LINE] = {NULL};
#endif

void DiagWriteDataCont(ulong cmd, char *buf0, int len0, char *buf1, int len1)
{
	if (len0 > LOG_MAX_DATA_SIZE) {
		BcmAdslCoreDiagWriteStatusData(cmd, buf0, LOG_MAX_DATA_SIZE, NULL, 0);
		DiagWriteDataCont(cmd, buf0 + LOG_MAX_DATA_SIZE, len0 - LOG_MAX_DATA_SIZE, buf1, len1);
	}
	else if ((len0 + len1) > LOG_MAX_DATA_SIZE) {
		long	len2 = (LOG_MAX_DATA_SIZE - len0) & ~1;

		BcmAdslCoreDiagWriteStatusData(cmd, buf0, len0, buf1, len2);
		DiagWriteDataCont(cmd, buf1 + len2, len1 - len2, NULL, 0);
	}
	else
#ifdef SUPPORT_DSL_BONDING
		BcmAdslCoreDiagWriteStatusData((cmd & DIAG_LINE_MASK) | (statusInfoData-3), buf0, len0, buf1, len1);
#else
		BcmAdslCoreDiagWriteStatusData(statusInfoData-3, buf0, len0, buf1, len1);
#endif
}

void BcmAdslCoreDiagWriteStatus(dslStatusStruct *status, char *pBuf, int len)
{
	static	long	statStrBuf[4096/4];
	char			*statStr, *p1;
	ulong			cmd;
	long			n, n1;
	uchar			lineId;
	
	if (!BcmAdslDiagIsActive() && !BcmXdslDiagStatSaveLocalIsActive())
		return;

	BcmCoreDpcSyncEnter();

	statStr = pBuf;
	n = len;
	
	lineId = DSL_LINE_ID(status->code);

#ifdef SUPPORT_DSL_BONDING
	cmd = statusInfoData | (lineId << DIAG_LINE_SHIFT);
#else
	cmd = statusInfoData;
#endif
	p1 = NULL;
	n1 = 0;

	switch (DSL_STATUS_CODE(status->code)) {
	  case kDslPrintfStatus:
	  case kDslPrintfStatus1:
	  case kDslPrintfStatusSaveLocalOnly:
		statStr = (char *) statStrBuf;
		n = vsprintf (statStr, status->param.dslPrintfMsg.fmt, status->param.dslPrintfMsg.argPtr) + 1;
		BcmAdslCoreDiagScrambleString(statStr);
#ifdef SUPPORT_DSL_BONDING
		cmd = LOG_CMD_SCRAMBLED_STRING | (lineId << DIAG_LINE_SHIFT);
#else
		cmd = LOG_CMD_SCRAMBLED_STRING;
#endif
		break;

	  case kDslExceptionStatus:
		{
		ulong	*sp;
		sp = (ulong*) status->param.dslException.sp;
#ifdef FLATTEN_ADDR_ADJUST
		sp = (ulong *)LMEM_ADDR_TO_HOST(sp[28]);
#endif
		p1 = (void *) sp;
		n1 = 64 * sizeof(long);
		}
		break;
	  case kDslConnectInfoStatus:
		{
		ulong	bufLen = status->param.dslConnectInfo.value;

		  switch (status->param.dslConnectInfo.code) {
			case kDslChannelResponseLog:
			case kDslChannelResponseLinear:
				bufLen <<= 1;
				/* */
			case kDslChannelQuietLineNoise:
				p1 = status->param.dslConnectInfo.buffPtr;
				n1 = bufLen;
				break;
			case kDslRcvCarrierSNRInfo:
				if (NULL != diagStatDataPtr[lineId]) {
					n  = 3 * 4;
					p1 = diagStatDataPtr[lineId];
					n1 = bufLen << 1;
				}
				break;
			case kG992p2XmtToneOrderingInfo:
			case kG992p2RcvToneOrderingInfo:
			case kG994MessageExchangeRcvInfo:
			case kG994MessageExchangeXmtInfo:
			case kG992MessageExchangeRcvInfo:
			case kG992MessageExchangeXmtInfo:
				if (NULL != diagStatDataPtr[lineId]) {
					n  = 3 * 4;
					p1 = diagStatDataPtr[lineId];
					n1 = bufLen;
				}
				break;
			default:
				break;
		  }
		}
		break;
	  case kDslDspControlStatus:
		switch (status->param.dslConnectInfo.code) {
			case kDslG992RcvShowtimeUpdateGainPtr:
				p1 = status->param.dslConnectInfo.buffPtr;
				n1 = status->param.dslConnectInfo.value<<1;
				((dslStatusStruct *)pBuf)->param.dslConnectInfo.value <<=1;
				break;
			case kDslPLNPeakNoiseTablePtr:
			case kDslPerBinThldViolationTablePtr:
			case kDslImpulseNoiseDurationTablePtr:
			case kDslImpulseNoiseTimeTablePtr:
			case kDslInpBinTablePtr:
			case kDslItaBinTablePtr:  
			case kDslNLNoise:
			case kDslInitializationSNRMarginInfo:
			case kFireMonitoringCounters:
				p1 = status->param.dslConnectInfo.buffPtr;
				n1 = status->param.dslConnectInfo.value;
				break;
		}
		break;

	  case kDslShowtimeSNRMarginInfo:
		if (NULL != diagStatDataPtr[lineId]) {
			n  = sizeof(status->param.dslShowtimeSNRMarginInfo) + 4 - 4;
			p1 = diagStatDataPtr[lineId];
			n1 = status->param.dslShowtimeSNRMarginInfo.nCarriers << 1;
		}
		break;
	  case kDslReceivedEocCommand:
		if ( ((kDslClearEocSendFrame  == status->param.dslClearEocMsg.msgId) ||
			  (kDslClearEocRcvedFrame == status->param.dslClearEocMsg.msgId) ||
			  (kDslGeneralMsgStart <= status->param.dslClearEocMsg.msgId))
		  &&
			 (0 == (status->param.dslClearEocMsg.msgType & kDslClearEocMsgDataVolatileMask)))
		{
			n -= 4;
			((dslStatusStruct *)statStr)->param.dslClearEocMsg.msgType |= kDslClearEocMsgDataVolatileMask;
			n1 = status->param.dslClearEocMsg.msgType & kDslClearEocMsgLengthMask;
			p1 = status->param.dslClearEocMsg.dataPtr;
		}
		break;
	  case kAtmStatus:
		if (kAtmStatCounters == status->param.atmStatus.code) {
			n = 3*4;
			((dslStatusStruct *)pBuf)->param.atmStatus.code = kAtmStatCounters1;
			p1 = (void *) status->param.atmStatus.param.value;
			n1 = ((atmPhyCounters *) p1)->id;
			((dslStatusStruct *)pBuf)->param.atmStatus.param.value = n1;
		}
		break;
	}

	if(BcmXdslDiagStatSaveLocalIsActive())
		BcmXdslDiagStatSaveLocal(cmd, statStr, n, p1, n1);

	if (!BcmAdslDiagIsActive() || (kDslPrintfStatusSaveLocalOnly == status->code)) {
		BcmCoreDpcSyncExit();
		return;
	}

	if (n > LOG_MAX_DATA_SIZE) {
		BcmAdslCoreDiagWriteStatusData(cmd | DIAG_SPLIT_MSG, statStr, LOG_MAX_DATA_SIZE, NULL, 0);
#ifdef SUPPORT_DSL_BONDING
		DiagWriteDataCont((cmd & DIAG_LINE_MASK) | (statusInfoData-2), statStr + LOG_MAX_DATA_SIZE, n - LOG_MAX_DATA_SIZE, p1, n1);
#else
		DiagWriteDataCont(statusInfoData-2, statStr + LOG_MAX_DATA_SIZE, n - LOG_MAX_DATA_SIZE, p1, n1);
#endif
	}
	else if ((n + n1) > LOG_MAX_DATA_SIZE) {
		long	len1 = (LOG_MAX_DATA_SIZE - n) & ~1;
		BcmAdslCoreDiagWriteStatusData(cmd | DIAG_SPLIT_MSG, statStr, n, p1, len1);
#ifdef SUPPORT_DSL_BONDING
		DiagWriteDataCont((cmd & DIAG_LINE_MASK) | (statusInfoData-2), p1 + len1, n1 - len1, NULL, 0);
#else
		DiagWriteDataCont(statusInfoData-2, p1 + len1, n1 - len1, NULL, 0);
#endif
	}
	else
		BcmAdslCoreDiagWriteStatusData(cmd, statStr, n, p1, n1);

	BcmCoreDpcSyncExit();
}

void BcmAdslCoreWriteOvhMsg(void *gDslVars, char *hdr, dslFrame *pFrame)
{
	dslStatusStruct status;
	dslFrameBuffer	*pBuf, *pBufNext;
	uchar		*pData;
	int			dataLen, i = 0;
	ulong		cmd;
	Boolean		bFirstBuf = true;
	
	if (!BcmAdslDiagIsActive() && !BcmXdslDiagStatSaveLocalIsActive())
		return;
#ifdef SUPPORT_DSL_BONDING
	cmd = statusInfoData | (gLineId(gDslVars) << DIAG_LINE_SHIFT);
	status.code = kDslDspControlStatus | (gLineId(gDslVars) << DSL_LINE_SHIFT);
#else
	cmd = statusInfoData;
	status.code = kDslDspControlStatus;
#endif
	if( 'T' == hdr[0] )
		status.param.dslConnectInfo.code = kDslTxOvhMsg;
	else
		status.param.dslConnectInfo.code = kDslRxOvhMsg;
	status.param.dslConnectInfo.value = DslFrameGetLength(gDslVars, pFrame);
	pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);

	while (NULL != pBuf) {
		dataLen   = DslFrameBufferGetLength(gDslVars, pBuf);
		pData = DslFrameBufferGetAddress(gDslVars, pBuf);
		pBufNext = DslFrameGetNextBuffer(gDslVars, pBuf);
		if( ++i >= 20 && pBufNext) {
			if(BcmAdslDiagIsActive()) {
				BcmAdslCoreDiagWriteStatusData(cmd -3, pData, dataLen, NULL, 0);
				BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), " G.997 frame %s: pFr = 0x%X, len = %ld; too many buffer(> %d) in this frame",
					hdr, (int) pFrame, DslFrameGetLength(gDslVars, pFrame), i);
			}
			if(BcmXdslDiagStatSaveLocalIsActive()) {
				BcmXdslDiagLastSplitStatSaveLocal(pData, dataLen);
				BcmAdslCoreDiagWriteStatusString(gLineId(gDslVars), " G.997 frame %s: pFr = 0x%X, len = %ld; too many buffer(> %d) in this frame",
					hdr, (int) pFrame, DslFrameGetLength(gDslVars, pFrame), i);
			}
			break;
		}
		
		if( !bFirstBuf ) {
			if(pBufNext) {
				if(BcmAdslDiagIsActive())
					BcmAdslCoreDiagWriteStatusData(cmd -2, pData, dataLen, NULL, 0);
				if(BcmXdslDiagStatSaveLocalIsActive())
					BcmXdslDiagContSplitStatSaveLocal(pData, dataLen);
			}
			else {
				if(BcmAdslDiagIsActive())
					BcmAdslCoreDiagWriteStatusData(cmd -3, pData, dataLen, NULL, 0);
				if(BcmXdslDiagStatSaveLocalIsActive())
					BcmXdslDiagLastSplitStatSaveLocal(pData, dataLen);
			}
		}
		else {
			if(pBufNext) {
				if(BcmAdslDiagIsActive())
					BcmAdslCoreDiagWriteStatusData(cmd | DIAG_SPLIT_MSG,
						(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
						pData, dataLen);
				if(BcmXdslDiagStatSaveLocalIsActive())
					BcmXdslDiagFirstSplitStatSaveLocal(cmd, status.param.dslConnectInfo.value + sizeof(status.code) + sizeof(status.param.dslConnectInfo),
						(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
						pData, dataLen);
			}
			else {
				if(BcmAdslDiagIsActive())
					BcmAdslCoreDiagWriteStatusData(cmd,
						(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
						pData, dataLen);
				if(BcmXdslDiagStatSaveLocalIsActive())
					BcmXdslDiagStatSaveLocal(cmd,
						(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
						pData, dataLen);
			}
			bFirstBuf = FALSE;
		}
		pBuf = pBufNext;
	}	
}

#define DIAG_NOCMD_TO	(5000 / BCMOS_MSEC_PER_TICK)
#define DIAG_PING_TIME	(1000  / BCMOS_MSEC_PER_TICK)
#define DIAG_DISC_TIME	(10000 / BCMOS_MSEC_PER_TICK)

Private ulong DslGetCycleCountCP09Sel0(void)
{
	ulong cnt;
	__asm volatile("mfc0 %0, $9":"=d"(cnt));
	return(cnt); 
}

void BcmAdslCoreDiagSetSyncTime(ulong syncTime)
{
	BcmCoreDpcSyncEnter();
	gDiagSyncTimeLastCycleCount=DslGetCycleCountCP09Sel0();
	gDiagSyncTime = syncTime+1;
	BcmCoreDpcSyncExit();
	AdslDrvPrintf (TEXT("%s: syncTime=%lu ms %lu sec\n"), __FUNCTION__, syncTime, syncTime/1000);
}

ulong BcmAdslCoreDiagGetSyncTime(void)
{
	ulong	curCycleCount;
	
	BcmCoreDpcSyncEnter();
	curCycleCount = DslGetCycleCountCP09Sel0();
	gDiagSyncTime += ((curCycleCount - gDiagSyncTimeLastCycleCount + 100000)/200000);
	gDiagSyncTimeLastCycleCount = curCycleCount;
	BcmCoreDpcSyncExit();
	
	return gDiagSyncTime/1000;
}

void BcmAdslCoreDiagCmdNotify(void)
{
	bcmOsGetTime(&gDiagLastCmdTime);
	gDiagLastPingTime = gDiagLastCmdTime;
}

void BcmAdslCoreDiagConnectionCheck(void)
{
	char	buf[4];
	OS_TICKS	tick;
	ulong	syncTime;
	
	if(BcmAdslDiagIsActive()) {
		bcmOsGetTime(&tick);
		syncTime = BcmAdslCoreDiagGetSyncTime();	/* Also update syncTime */
		if ((tick - gDiagLastCmdTime) > DIAG_NOCMD_TO) {
			if ((tick - gDiagLastCmdTime) > DIAG_DISC_TIME) {
				AdslDrvPrintf (TEXT("DslDiags timeout disconnect: syncTime= %lu sec, to=%lu, currTick=%lu, gDiagLastCmdTime=%lu\n"),
						syncTime, (tick - gDiagLastCmdTime)*BCMOS_MSEC_PER_TICK, tick, gDiagLastCmdTime);
				BcmAdslDiagDisconnect();
			}
			else if ((tick - gDiagLastPingTime) > DIAG_PING_TIME) {
				AdslDrvPrintf (TEXT("DslDiags ping request: syncTime= %lu sec, currTick=%lu, gDiagLastPingTime=%lu gDiagLastCmdTime=%lu\n"),
						syncTime, tick, gDiagLastPingTime, gDiagLastCmdTime);
				gDiagLastPingTime = tick;
				BcmAdslCoreDiagWriteStatusData(LOG_CMD_PING_REQ, buf, 2, NULL, 0);
			}
		}
	}
}

void BcmAdslCoreDiagStatusSnooper(dslStatusStruct *status, char *pBuf, int len)
{
	unsigned char lineId;
	OS_TICKS	  tick;
	Bool bStatPtrSet = false;

	if (!BcmAdslDiagIsActive() && !BcmXdslDiagStatSaveLocalIsActive())
		return;
	
	bcmOsGetTime(&tick);
#if 0 /* TBD */
	if ((tick - gDiagLastCmdTime) > DIAG_NOCMD_TO) {
		if ((tick - gDiagLastCmdTime) > DIAG_DISC_TIME) {
		AdslDrvPrintf (TEXT("DslDiags timeout disconnect: to=%lu\n"), (tick - gDiagLastCmdTime)*BCMOS_MSEC_PER_TICK);
			BcmAdslDiagDisconnect();
		}
		else if ((tick - gDiagLastPingTime) > DIAG_PING_TIME) {
			gDiagLastPingTime = tick;
			BcmAdslCoreDiagWriteStatusData(LOG_CMD_PING_REQ, pBuf, 2, NULL, 0);
		}
	}
#endif
	BcmAdslCoreDiagWriteStatus(status, pBuf, len);
	
	lineId = DSL_LINE_ID(status->code);
	
	switch (DSL_STATUS_CODE(status->code)) {
#ifdef ADSLDRV_STATUS_PROFILING
		case kDslReceivedEocCommand:
			if ( kStatusBufferHistogram==status->param.dslClearEocMsg.msgId )
				BcmXdslCoreDrvProfileInfoPrint();
			break;
#endif
		case kDslDspControlStatus:
			if (kDslStatusBufferInfo == status->param.dslConnectInfo.code) {
				diagStatDataPtr[lineId] = status->param.dslConnectInfo.buffPtr;
				bStatPtrSet = true;
			}
			break;
		case kDslExceptionStatus:
			BcmAdslCoreDiagWriteStatusString(lineId, "Resetting ADSL MIPS\n");
			break;
		default:
			break;
	}
	if (!bStatPtrSet)
		diagStatDataPtr[lineId] = NULL;
}

void BcmAdslCoreDiagSaveStatusString(char *fmt, ...)
{
	dslStatusStruct 	status;
	va_list 			ap;

	va_start(ap, fmt);

	status.code = kDslPrintfStatusSaveLocalOnly;
	status.param.dslPrintfMsg.fmt = fmt;
	status.param.dslPrintfMsg.argNum = 0;
	status.param.dslPrintfMsg.argPtr = (void *)ap;
	va_end(ap);

	BcmAdslCoreDiagWriteStatus(&status, NULL, 0);
}

void BcmAdslCoreDiagWriteStatusString(unsigned char lineId, char *fmt, ...)
{
	dslStatusStruct 	status;
	va_list 			ap;
	
	va_start(ap, fmt);
	
	status.code = kDslPrintfStatus | (lineId << DSL_LINE_SHIFT);
	status.param.dslPrintfMsg.fmt = fmt;
	status.param.dslPrintfMsg.argNum = 0;
	status.param.dslPrintfMsg.argPtr = (void *)ap;
	va_end(ap);
	
	BcmAdslCoreDiagWriteStatus(&status, NULL, 0);
}

void BcmAdslCoreDiagWriteFile(unsigned char lineId, char *fname, void *ptr, ulong len) 
{
	dslStatusStruct	  status;

	status.code = kDslReceivedEocCommand | (lineId << DSL_LINE_SHIFT);
	status.param.dslClearEocMsg.msgId	= kDslGeneralMsgDbgFileName;
	status.param.dslClearEocMsg.msgType = strlen(fname) + 1;
	status.param.dslClearEocMsg.dataPtr = fname;
	BcmAdslCoreDiagWriteStatus(&status, (char *)&status, sizeof(status.code) + sizeof(status.param.dslClearEocMsg));

	status.param.dslClearEocMsg.msgId	= kDslGeneralMsgDbgWriteFile;
	status.param.dslClearEocMsg.msgType = len;
	status.param.dslClearEocMsg.dataPtr = ptr;
	BcmAdslCoreDiagWriteStatus(&status, (char *)&status, sizeof(status.code) + sizeof(status.param.dslClearEocMsg));
}

void BcmAdslCoreDiagOpenFile(unsigned char lineId, char *fname) 
{
	dslStatusStruct	  status;

	status.code = kDslReceivedEocCommand | (lineId << DSL_LINE_SHIFT);
	status.param.dslClearEocMsg.msgId	= kDslGeneralMsgDbgFileName;
	status.param.dslClearEocMsg.msgType = (strlen(fname) + 1) | kDslDbgFileNameDelete;
	status.param.dslClearEocMsg.dataPtr = fname;
	BcmAdslCoreDiagWriteStatus(&status, (char *)&status, sizeof(status.code) + sizeof(status.param.dslClearEocMsg));
}
/* common diag command handler */

static void BcmDiagPrintConfigCmd (char *hdr, adslCfgProfile *pCfg)
{
	ulong	cfg;

	AdslDrvPrintf (TEXT("DrvDiag: %s: CCFG=0x%lX ACFG=0x%lX"), hdr, pCfg->adslAnnexCParam, pCfg->adslAnnexAParam);
#ifndef G992_ANNEXC
	cfg = pCfg->adslAnnexAParam;
#else
	cfg = pCfg->adslAnnexCParam;
#endif
	if (cfg & kAdslCfgExtraData) {
		AdslDrvPrintf (TEXT(" TrM=0x%lX ShM=0x%lX MgTo=%ld"), pCfg->adslTrainingMarginQ4, pCfg->adslShowtimeMarginQ4, pCfg->adslLOMTimeThldSec);
		if (cfg & kAdslCfgDemodCapOn)
			AdslDrvPrintf (TEXT(", DemodMask=0x%lX DemodVal=0x%lX\n"), pCfg->adslDemodCapMask, pCfg->adslDemodCapValue);
		else
			AdslDrvPrintf (TEXT("\n"));
	}
	else
		AdslDrvPrintf (TEXT("\n"));
}

typedef struct _diagEyeCfgStruct {
	short		toneMain;
	short		tonePilot;
	long			reserved;
	long			reserved1;
	BOOL		enabled;
} diagEyeCfgStruct;

void BcmAdslCoreDiagCmdCommon(unsigned char lineId, int diagCmd, int len, void *pCmdData)
{
	dslCommandStruct	cmd;
	char		*pInpBins;
	char		*pItaBins;
	long		size;
	int		res, n;
	uchar		*pOid;
	uchar		*pObj;
	/* AdslDrvPrintf (TEXT("DrvDiagCmd: %d\n"), diagCmd); */
	
	switch (diagCmd) {
		case LOG_CMD_PING_REQ:
			/* AdslDrvPrintf (TEXT("DrvDiagCmd: PING\n")); */
			BcmAdslCoreDiagWriteStatusData(LOG_CMD_PING_RSP, pCmdData, 2, NULL, 0);
			break;
		case LOG_CMD_MIB_GET1:
		{
			char *pName = (char *)pCmdData;
			pOid = (uchar *)((int)pCmdData + strlen(pName) + 1);
			n = ((int)pCmdData + len) - (int)pOid;	/* oid length */
			if( n < 2 ) {
				BcmAdslCoreDiagWriteStatusString(lineId, "LOG_CMD_MIB_GET1: Invalid oid len(%d)\n", n);
				break;
			}
			res = BcmAdslCoreGetObjectValue (lineId, pOid, n, NULL, &size);
			if (kAdslMibStatusNoObject != res) {
				BcmAdslCoreDiagWriteStatusString(lineId, "LOG_CMD_MIB_GET1: filename=%s dataLen=%d 0x%X 0x%X 0x%X\n",
					pName, size,*(ulong*)res,*((ulong*)res+1),*((ulong*)res+2));
				BcmAdslCoreDiagWriteFile(lineId, pName, (void *)res, size);
			}
			break;
		}
		case LOG_CMD_MIB_GET:
			if (len != 0)
				memmove(pCmdData+2, pCmdData, len);
			((uchar *)pCmdData)[0] = len & 0xFF;
			((uchar *)pCmdData)[1] = (len >> 8) & 0xFF;
			pOid = pCmdData+2;
			size = LOG_MAX_DATA_SIZE - (2+len+2);  /* Make sure pCmdData size is at least LOG_MAX_DATA_SIZE => DoDiagCommand() */
			res = BcmAdslCoreGetObjectValue (lineId, (0 == len ? NULL : pOid), len, pOid+len+2, &size);
			if (kAdslMibStatusSuccess == res) {
				pOid[len+0] = size & 0xFF;
				pOid[len+1] = (size >> 8) & 0xFF;
				BcmAdslCoreDiagWriteStatusData(LOG_CMD_MIB_GET | (lineId << DIAG_LINE_SHIFT), pCmdData, 2+len+2+size, NULL, 0);
			}
			else if ((kAdslMibStatusBufferTooSmall == res) && (kOidAdslPrivate == pOid[0])) {
				int	i=0;
				pObj = (void *) BcmAdsl_GetObjectValue (lineId, (0 == len ? NULL : pOid), len, NULL, (long *)&size);
				
				n = LOG_MAX_DATA_SIZE - (2+len+2);	/* Available buffer size for MIB data */
				pOid[0] = kOidAdslPrivatePartial;
				pOid[len+0] = n & 0xFF;
				pOid[len+1] = (n >> 8) & 0xFF;
				
				BcmCoreDpcSyncEnter();
				while( size> n )
				{
					memcpy (pOid+len+2, pObj+i*n, n);
					BcmAdslCoreDiagWriteStatusData(LOG_CMD_MIB_GET | (lineId << DIAG_LINE_SHIFT), pCmdData, 2+len+2+n, NULL, 0);
					size -= n;
					i++;
				}
				
				if(size) {
					pOid[len+0] = size & 0xFF;
					pOid[len+1] = (size >> 8) & 0xFF;
					memcpy (pOid+len+2, pObj+i*n, size);
					BcmAdslCoreDiagWriteStatusData(LOG_CMD_MIB_GET | (lineId << DIAG_LINE_SHIFT), pCmdData, 2+len+2+size, NULL, 0);
				}
				
				pOid[len+0] = 0;
				pOid[len+1] = 0;
				BcmAdslCoreDiagWriteStatusData(LOG_CMD_MIB_GET | (lineId << DIAG_LINE_SHIFT), pCmdData, 2+len+2, NULL, 0);
				BcmCoreDpcSyncExit();
			}
			else {
				BcmAdslCoreDiagWriteStatusString(lineId, "LOG_CMD_MIB_GET Error %d: oidLen=%d, oid = %d %d %d %d %d", 
					res, len, pOid[0], pOid[1], pOid[2], pOid[3], pOid[4]);
				pOid[len+0] = 0;
				pOid[len+1] = 0;
				BcmAdslCoreDiagWriteStatusData(LOG_CMD_MIB_GET | (lineId << DIAG_LINE_SHIFT), pCmdData, 2+len+2, NULL, 0);
			}
			break;

		case LOG_CMD_EYE_CFG:
			{
			diagEyeCfgStruct *pEyeCfg = (diagEyeCfgStruct *)pCmdData;

#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
			BcmAdslCoreDiagWriteStatusString(lineId, "%s: LOG_CMD_EYE_CFG - toneMain=%d tonePilot=%d enabled=%d",
				__FUNCTION__, pEyeCfg->toneMain, pEyeCfg->tonePilot, pEyeCfg->enabled);
			BcmAdslCoreDiagSetBufDesc();
#endif
#ifdef SUPPORT_DSL_BONDING
			cmd.command = kDslDiagSetupCmd | (lineId << DSL_LINE_SHIFT);
#else
			cmd.command = kDslDiagSetupCmd;
#endif
			cmd.param.dslDiagSpec.setup = pEyeCfg->enabled;
			cmd.param.dslDiagSpec.eyeConstIndex1 = pEyeCfg->toneMain;
			cmd.param.dslDiagSpec.eyeConstIndex2 = pEyeCfg->tonePilot;
			cmd.param.dslDiagSpec.logTime = 0;
			BcmCoreCommandHandlerSync(&cmd);
			}
			break;

		case LOG_CMD_SWITCH_RJ11_PAIR:
			AdslDrvPrintf (TEXT("DrvDiag: LOG_CMD_SWITCH_RJ11_PAIR\n"));
			BcmAdslCoreNotify(lineId, ACEV_SWITCH_RJ11_PAIR);
			break;

		case LOG_CMD_CFG_PHY:
		{
			adslPhyCfg	*pCfg = pCmdData;

			AdslDrvPrintf (TEXT("CFG_PHY CMD: demodCapabilities=0x%08X  mask=0x%08X  demodCap=0x%08X\n"),
				(unsigned int)adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities,
				(unsigned int)pCfg->demodCapMask, (unsigned int)pCfg->demodCap);
			if(adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities == pCfg->demodCap) {
				AdslDrvPrintf (TEXT("CFG_PHY CMD: No change, do nothing\n"));
				break;
			}
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities |= 
				pCfg->demodCapMask & pCfg->demodCap;
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities &= 
				~(pCfg->demodCapMask & ~pCfg->demodCap);

			adslCoreCfgProfile[lineId].adslDemodCapValue=adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.demodCapabilities;
			adslCoreCfgProfile[lineId].adslDemodCapMask=adslCoreCfgProfile[lineId].adslDemodCapValue | pCfg->demodCapMask;
		}
			goto _cmd_reset;

		case LOG_CMD_CFG_PHY2:
		{
			adslPhyCfg	*pCfg = pCmdData;

			AdslDrvPrintf (TEXT("CFG_PHY2 CMD: subChannelInfop5=0x%08X  mask=0x%08X  demodCap=0x%08X\n"),
				(unsigned int)adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5,
				(unsigned int)pCfg->demodCapMask, (unsigned int)pCfg->demodCap);
			
			if(adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5 == pCfg->demodCap) {
				AdslDrvPrintf (TEXT("CFG_PHY2 CMD: No change, do nothing\n"));
				break;
			}

			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5 |= 
				pCfg->demodCapMask & pCfg->demodCap;
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5 &= 
				~(pCfg->demodCapMask & ~pCfg->demodCap);

			adslCoreCfgProfile[lineId].adslDemodCap2Value=adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.subChannelInfop5;
			adslCoreCfgProfile[lineId].adslDemodCap2Mask=adslCoreCfgProfile[lineId].adslDemodCap2Value | pCfg->demodCapMask;
		}
			goto _cmd_reset;
#ifdef G993
		case LOG_CMD_CFG_PHY3:
		{
			adslPhyCfg	*pCfg = pCmdData;
			#if 0
			ulong	vdslParam = 
				(adslCoreConnectionParam.param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA->profileSel & kVdslProfileMask1) |
				((adslCoreConnectionParam.param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA->maskUS0 << kVdslUS0MaskShift) & kVdslUS0Mask);
			#endif
			AdslDrvPrintf (TEXT("CFG_PHY3 CMD: ((maskUS0<<16)|profileSel) - Current=0x%08X  New=0x%08X\n"),
				(uint)adslCoreCfgProfile[lineId].vdslParam, (uint)pCfg->demodCap);
			
			if(adslCoreCfgProfile[lineId].vdslParam == pCfg->demodCap) {
				AdslDrvPrintf (TEXT("CFG_PHY3 CMD: No change, do nothing\n"));
				break;
			}
			
			adslCoreCfgProfile[lineId].vdslParam = pCfg->demodCap;
			
			if (!ADSL_PHY_SUPPORT(kAdslPhyVdsl30a))
				pCfg->demodCap &= ~kVdslProfile30a;
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA->profileSel = 
				pCfg->demodCap & kVdslProfileMask1;
			adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.carrierInfoG993p2AnnexA->maskUS0 =
				(pCfg->demodCap & kVdslUS0Mask) >> kVdslUS0MaskShift;
		}
#endif
_cmd_reset:

		case LOG_CMD_RESET:
			BcmAdslCoreConnectionReset(lineId);
			break;

		case LOG_CMD_TEST_DATA:
			adslCoreAlwaysReset = (0 != *(long*) pCmdData) ? AC_TRUE : AC_FALSE;
			break;

		case LOG_CMD_LOG_STOP:
			cmd.command = kDslDiagStopLogCmd;
			BcmCoreCommandHandlerSync(&cmd);
			break;

		case LOG_CMD_CONFIG_A:
#ifndef G992_ANNEXC
			{
			adslCfgProfile *pAdslCfg = (adslCfgProfile *) pCmdData;

			BcmDiagPrintConfigCmd  ("LOG_CMD_CONFIG_A", pCmdData);

			if ( (kAdslCfgModAny == (pAdslCfg->adslAnnexAParam & kAdslCfgModMask)) &&
				 (pAdslCfg->adsl2Param == (kAdsl2CfgReachExOn | kAdsl2CfgAnnexLUpWide | kAdsl2CfgAnnexLUpNarrow)) )
				pAdslCfg->adsl2Param = 0;
			BcmAdslCoreConfigure(lineId, (adslCfgProfile *) pCmdData);
			}
#else
			AdslDrvPrintf (TEXT("DrvDiag: LOG_CMD_CONFIG_A - Can't configure AnnexC modem for AnnexA\n"));
#endif
			break;

		case LOG_CMD_CONFIG_C:
#ifdef G992_ANNEXC
			BcmDiagPrintConfigCmd  ("LOG_CMD_CONFIG_C", pCmdData);
			BcmAdslCoreConfigure(lineId, (adslCfgProfile *) pCmdData);
#else
			AdslDrvPrintf (TEXT("DrvDiag: LOG_CMD_CONFIG_C - Can't configure AnnexA modem for AnnexC\n"));
#endif
			break;

		case LOG_CMD_BERT_EX:
			BcmAdslCoreBertStartEx(lineId, *(long*)pCmdData);
			break;

		case LOG_CMD_GDB:
		{
#ifdef DEBUG_GDB_STUB
			int i;
			printk("BcmAdslCoreDiagCmdCommon::LOG_CMD_GDB '");
			for (i = 0; i < len; i++)
			printk("%c", *((char *)pCmdData + i));
			printk("'\n");
#endif
			BcmCoreDpcSyncEnter();
			BcmAdslCoreGdbCmd(pCmdData, len);
			BcmCoreDpcSyncExit(); 
			break;
		}
		case (kDslAfelbTestCmd - kFirstDslCommandCode):
		case (kDslTestQuietCmd - kFirstDslCommandCode):
		case (kDslDiagStartBERT - kFirstDslCommandCode):
		case (kDslFilterSNRMarginCmd - kFirstDslCommandCode):
		case (kDslDyingGaspCmd - kFirstDslCommandCode):
			cmd.param.value = *(long*)pCmdData;

			/* fall through */

		case (kDslStartRetrainCmd - kFirstDslCommandCode):
		case (kDslIdleCmd - kFirstDslCommandCode):
		case (kDslIdleRcvCmd - kFirstDslCommandCode):
		case (kDslIdleXmtCmd - kFirstDslCommandCode):
		case (kDslLoopbackCmd - kFirstDslCommandCode):
		case (kDslDiagStopBERT - kFirstDslCommandCode):
		case (kDslDiagStopLogCmd - kFirstDslCommandCode):
		case (kDslPingCmd - kFirstDslCommandCode):
			cmd.command = (diagCmd + kFirstDslCommandCode) | (lineId << DSL_LINE_SHIFT);
			AdslDrvPrintf (TEXT("DrvDiag: cmd=%ld, val= %ld\n"), cmd.command, cmd.param.value);
			BcmCoreCommandHandlerSync(&cmd);

			if ((kDslDyingGaspCmd - kFirstDslCommandCode) == diagCmd) {
				bcmOsSleep (1000/BCMOS_MSEC_PER_TICK);
				cmd.command = kDslIdleCmd | (lineId << DSL_LINE_SHIFT);
				BcmCoreCommandHandlerSync(&cmd);
			}
			break;

		case (kDslTestCmd - kFirstDslCommandCode):
			cmd.command = kDslTestCmd | (lineId << DSL_LINE_SHIFT);
			memcpy(&cmd.param, pCmdData, sizeof(cmd.param.dslTestSpec));
			/* AdslDrvPrintf (TEXT("DrvDslTestCmd: testCmd=%ld\n"), cmd.param.dslTestSpec.type); */
			if (kDslTestToneSelection == cmd.param.dslTestSpec.type) {
				cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap = ((char*) pCmdData) +
					FLD_OFFSET(dslCommandStruct,param.dslTestSpec.param.toneSelectSpec.xmtMap) -
					FLD_OFFSET(dslCommandStruct,param);
				cmd.param.dslTestSpec.param.toneSelectSpec.rcvMap = 
					cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap +
					((cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones + 7) >> 3);
				BcmAdslCoreDiagSelectTones(
					lineId,
					cmd.param.dslTestSpec.param.toneSelectSpec.xmtStartTone,
					cmd.param.dslTestSpec.param.toneSelectSpec.xmtNumOfTones,
					cmd.param.dslTestSpec.param.toneSelectSpec.rcvStartTone,
					cmd.param.dslTestSpec.param.toneSelectSpec.rcvNumOfTones,
					cmd.param.dslTestSpec.param.toneSelectSpec.xmtMap,
					cmd.param.dslTestSpec.param.toneSelectSpec.rcvMap);
			}
			else if (kDslTestExecuteDelay== cmd.param.dslTestSpec.type) {
				BcmAdslCoreSetTestExecutionDelay(lineId, cmd.param.dslTestSpec.type,cmd.param.dslTestSpec.param.value);
			}
			else {
				BcmAdslCoreSetTestMode(lineId, cmd.param.dslTestSpec.type);
#ifdef ADSLDRV_STATUS_PROFILING
				if(101 == cmd.param.dslTestSpec.type)	/* Retrieve Histogram of status buffer and reset */
					BcmXdslCoreDrvProfileInfoClear();
#endif
			}
			break;

		case (kDslStartPhysicalLayerCmd - kFirstDslCommandCode):
			cmd.command = kDslStartPhysicalLayerCmd | (lineId << DSL_LINE_SHIFT);
			memcpy(&cmd.param, pCmdData, sizeof(cmd.param.dslModeSpec));
			BcmCoreCommandHandlerSync(&cmd);
			break;

		case (kDslSetXmtGainCmd - kFirstDslCommandCode):
			cmd.param.value = *(long*)pCmdData;
			BcmAdslCoreSetXmtGain (lineId, cmd.param.value);
			break;

		case (kDslAfeTestCmd - kFirstDslCommandCode):
			BcmAdslCoreAfeTestMsg(pCmdData);
			break;
			
		case (kDslAfeTestCmd1 - kFirstDslCommandCode):
			cmd.command = kDslAfeTestCmd1 | (lineId << DSL_LINE_SHIFT);
			memcpy(&cmd.param, pCmdData, sizeof(cmd.param.dslAfeTestSpec1));
			BcmCoreCommandHandlerSync(&cmd);
			break;
			
		case (kDslPLNControlCmd - kFirstDslCommandCode):
			cmd.command = kDslPLNControlCmd | (lineId << DSL_LINE_SHIFT);
			cmd.param.dslPlnSpec.plnCmd = ((long*)pCmdData)[0];
			if (kDslPLNControlStart == cmd.param.dslPlnSpec.plnCmd) {
				cmd.param.dslPlnSpec.mgnDescreaseLevelPerBin = ((long*)pCmdData)[1];
				cmd.param.dslPlnSpec.mgnDescreaseLevelBand   = ((long*)pCmdData)[2];
			}
			gSharedMemAllocFromUserContext=1;
			if (kDslPLNControlDefineInpBinTable == cmd.param.dslPlnSpec.plnCmd) {
				cmd.param.dslPlnSpec.nInpBin = ((ulong*)pCmdData)[3];
				if(NULL!=(pInpBins=AdslCoreSharedMemAlloc(2*cmd.param.dslPlnSpec.nInpBin))){
					memcpy(pInpBins,((char *)pCmdData) + FLD_OFFSET(dslCommandStruct, param.dslPlnSpec.itaBinPtr) - 0*FLD_OFFSET(dslCommandStruct, param),2*cmd.param.dslPlnSpec.nInpBin);
					cmd.param.dslPlnSpec.inpBinPtr = (ushort *)pInpBins;
				}
				AdslDrvPrintf (TEXT("cmd.param.dslPlnSpec.inpBinPtr: 0x%x, data= 0x%X 0x%X 0x%X\n"), 
					(unsigned int)cmd.param.dslPlnSpec.inpBinPtr,
					(unsigned int)cmd.param.dslPlnSpec.inpBinPtr[0],
					(unsigned int)cmd.param.dslPlnSpec.inpBinPtr[1],
					(unsigned int)cmd.param.dslPlnSpec.inpBinPtr[2]);
			}
			if (kDslPLNControlDefineItaBinTable == cmd.param.dslPlnSpec.plnCmd) {
				cmd.param.dslPlnSpec.nItaBin = ((long*)pCmdData)[5];
				if(NULL!=(pItaBins=AdslCoreSharedMemAlloc(2*cmd.param.dslPlnSpec.nItaBin))){
					memcpy(pItaBins,((char *)pCmdData) + FLD_OFFSET(dslCommandStruct, param.dslPlnSpec.itaBinPtr) - 0*FLD_OFFSET(dslCommandStruct, param),2*cmd.param.dslPlnSpec.nItaBin);
					cmd.param.dslPlnSpec.itaBinPtr = (ushort *)pItaBins;
				}
				AdslDrvPrintf (TEXT("DrvDslPLNControlCmd: itaBin=%d\n"), (int)cmd.param.dslPlnSpec.nItaBin);
			}
			BcmCoreCommandHandlerSync(&cmd);
			gSharedMemAllocFromUserContext=0;
			break;
#ifdef PHY_PROFILE_SUPPORT
		case (kDslProfileControlCmd - kFirstDslCommandCode):

			cmd.param.value = *(long*)pCmdData;
			cmd.command = diagCmd + kFirstDslCommandCode;
			
			if( kDslProfileEnable == cmd.param.value )
				BcmAdslCoreProfilingStart();
			else
				BcmAdslCoreProfilingStop();
			
			BcmCoreCommandHandlerSync(&cmd);
			break;
#endif
		case (kDslSendEocCommand - kFirstDslCommandCode):
			cmd.command = kDslSendEocCommand | (lineId << DSL_LINE_SHIFT);
			memcpy(&cmd.param, pCmdData, sizeof(cmd.param.dslClearEocMsg));
			size = cmd.param.dslClearEocMsg.msgType & 0xFFFF;
			cmd.param.dslClearEocMsg.msgType &= ~kDslClearEocMsgDataVolatileMask;
			gSharedMemAllocFromUserContext=1;
			cmd.param.dslClearEocMsg.dataPtr = AdslCoreSharedMemAlloc(size);
			memcpy(cmd.param.dslClearEocMsg.dataPtr,
					pCmdData+FLD_OFFSET(dslCommandStruct, param.dslClearEocMsg.dataPtr) - FLD_OFFSET(dslCommandStruct,param), size);
			
			BcmCoreCommandHandlerSync(&cmd);
			gSharedMemAllocFromUserContext=0;
			break;
		default:
			if (diagCmd < 200) {
				cmd.param.value = *(long*)pCmdData;
				cmd.command = (diagCmd + kFirstDslCommandCode) | (lineId << DSL_LINE_SHIFT);
				BcmCoreCommandHandlerSync(&cmd);
			}
			break;
	}	
}

/*
**
**	Support for AFE test
**
*/

#define 		UN_INIT_HDR_MARK	0x80

typedef struct {
	char		*pSdram;
	ulong		size;
	ulong		cnt;
	ulong		frameCnt;
} afeTestData;

afeTestData		afeParam = { NULL, 0, 0, UN_INIT_HDR_MARK };
afeTestData		afeImage = { NULL, 0, 0, UN_INIT_HDR_MARK };

char  *rdFileName = NULL;

void BcmAdslCoreIdle(int size)
{
	BcmAdslCoreDiagWriteStatusString(0, "ReadFile Done");
#ifdef PHY_BLOCK_TEST
	AdslDrvPrintf (TEXT("ReadFile Done: addr=0x%X size=%d\n"), (uint)afeParam.pSdram, (int)afeParam.size);
#endif
}
void (*bcmLoadBufferCompleteFunc)(int size) = BcmAdslCoreIdle;

void BcmAdslCoreSetLoadBufferCompleteFunc(void *funcPtr)
{
	bcmLoadBufferCompleteFunc = (NULL != funcPtr) ? funcPtr : BcmAdslCoreIdle;
}

void *BcmAdslCoreGetLoadBufferCompleteFunc(void)
{
	return (BcmAdslCoreIdle == bcmLoadBufferCompleteFunc) ? bcmLoadBufferCompleteFunc : NULL;
}

void BcmAdslCoreAfeTestInit(afeTestData *pAfe, void *pSdram, ulong size)
{
	pAfe->pSdram = pSdram;
	pAfe->size	= size;
	pAfe->cnt	= 0;
	pAfe->frameCnt = 0;
}

void BcmAdslCoreAfeTestStart(afeTestData *pAfe, afeTestData *pImage)
{
	dslCommandStruct	cmd;

	cmd.command = kDslAfeTestCmd;
	cmd.param.dslAfeTestSpec.type = kDslAfeTestPatternSend;
	cmd.param.dslAfeTestSpec.afeParamPtr = afeParam.pSdram;
	cmd.param.dslAfeTestSpec.afeParamSize = afeParam.size;
	cmd.param.dslAfeTestSpec.imagePtr = afeImage.pSdram;
	cmd.param.dslAfeTestSpec.imageSize = afeImage.size;
	BcmCoreCommandHandlerSync(&cmd);
}

void BcmAdslCoreAfeTestAck (afeTestData	*pAfeData, ulong frNum, ulong frRcv)
{
	dslStatusStruct		status;

	// AdslDrvPrintf (TEXT("BcmAdslCoreAfeTestAck. frNumA = %d frNumR = %d\n"), frNum, frRcv);
	status.code = kDslDataAvailStatus;
#if 1
	status.param.dslDataAvail.dataPtr = (void *) (frNum | (frRcv << 8) | DIAG_ACK_FRAME_RCV_PRESENT);
#else
	status.param.dslDataAvail.dataPtr = (void *) frNum;
#endif
	status.param.dslDataAvail.dataLen = DIAG_ACK_LEN_INDICATION;
	BcmAdslCoreDiagStatusSnooper(&status, (void *)&status, sizeof(status.code) + sizeof(status.param.dslDataAvail));
}

#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
void BcmAdslCoreResetPhy(int copyImage)
{
	void		*pSdramImageAddr;

	if(!copyImage) {
		BcmAdslCoreDiagWriteStatusString(0, "%s: Restting PHY without re-copying image\n", __FUNCTION__);
		BcmAdslCoreStop();
		BcmAdslCoreStart(DIAG_DATA_EYE, AC_FALSE);
	}
	else if(pBkupImage) {
		BcmAdslCoreStop();
		BcmAdslCoreDiagWriteStatusString(0, "%s: Re-copying backup image before resetting PHY\n", __FUNCTION__);
		memcpy((void*)HOST_LMEM_BASE, pBkupImage, bkupLmemSize);
		pSdramImageAddr = AdslCoreSetSdramImageAddr(((ulong*)HOST_LMEM_BASE)[2], bkupSdramSize);
		memcpy(pSdramImageAddr, pBkupImage + bkupLmemSize, bkupSdramSize);
		BcmAdslCoreStart(DIAG_DATA_EYE, AC_FALSE);
	}
	else {
		/* NOTE: We cannot re-copy image from the flash as reading from a file can not be done in bottom half */
		BcmAdslCoreDiagWriteStatusString(0, "%s: Backup image is not available, will re-copy PHY image from the flash\n", __FUNCTION__);
		adslCoreResetPending = AC_TRUE;
	}
}
#endif

#ifdef LMEM_ACCESS_WORKAROUND
extern void AdslCoreRunPhyMipsInSDRAM(void);
extern AC_BOOL AdslCoreIsPhyMipsRunning(void);
#endif

void BcmAdslCoreAfeTestMsg(void *pMsg)
{
	dslCommandStruct	*pCmd = (void *) (((char *) pMsg) - sizeof(pCmd->command));
	char				*pSdram;
	afeTestData		*pAfeData;
	ulong			frNum = 0, dataLen = 0, n;
	int				i;

#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
	char				*pBkupAddr=NULL;
#endif

	if (kDslAfeTestPhyRun == pCmd->param.dslAfeTestSpec.type) {
#ifdef PHY_BLOCK_TEST
		AdslDrvPrintf (TEXT("PHY RUN START\n"));
#endif
		BcmAdslCoreStart(DIAG_DATA_EYE, AC_FALSE);
#ifdef PHY_BLOCK_TEST
		if(BcmAdslCoreIsTestCommand() && (NULL != irqDpcId))
			bcmOsDpcEnqueue(irqDpcId);
#endif
		return;
	}
	else if (kDslAfeTestRdEndOfFile == pCmd->param.dslAfeTestSpec.type) {
#ifdef PHY_BLOCK_TEST
		BcmAdslCoreDebugReadEndOfFile();
#else
		if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
			BcmAdslCoreDebugReadEndOfFile();
#endif
		afeParam.cnt = 0;
		afeImage.cnt = 0;
		return;
	}

	if ( ((void *) -1 == pCmd->param.dslAfeTestSpec.afeParamPtr) &&
		 ((void *) -1 == pCmd->param.dslAfeTestSpec.imagePtr) ) {

		if (-1 == (pCmd->param.dslAfeTestSpec.afeParamSize & pCmd->param.dslAfeTestSpec.imageSize)) {
			BcmAdslCoreAfeTestStart(&afeParam, &afeImage);
			return;
		}
		
		switch (pCmd->param.dslAfeTestSpec.type) {
			case kDslAfeTestLoadImage:
			case kDslAfeTestLoadImageOnly:
				
				BcmAdslCoreAfeTestInit(&afeParam, (void *)HOST_LMEM_BASE, pCmd->param.dslAfeTestSpec.afeParamSize);
				BcmAdslCoreAfeTestInit(&afeImage, NULL, pCmd->param.dslAfeTestSpec.imageSize);
				BcmAdslCoreStop();
				if ((kDslAfeTestLoadImage == pCmd->param.dslAfeTestSpec.type) &&
					(0 == afeParam.size) && (0 == afeImage.size)) {
					BcmAdslCoreStart(DIAG_DATA_EYE, AC_TRUE);
					if (DIAG_DEBUG_CMD_LOG_DATA == diagDebugCmd.cmd) {
						BcmAdslCoreDiagStartLog(diagDebugCmd.param1, diagDebugCmd.param2);
						diagDebugCmd.cmd = 0;
					}
				}
#if 0 && defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
				if((0 != afeParam.size) && (0 != afeImage.size)) {
					ulong combineImageLen = afeParam.size+afeImage.size;
					if((NULL != pBkupImage) && (combineImageLen > (bkupLmemSize+bkupSdramSize))) {
						vfree(pBkupImage);
						pBkupImage = (char *)vmalloc(combineImageLen);
						if(NULL == pBkupImage) {
							bkupLmemSize = 0;
							bkupSdramSize = 0;
							BcmAdslCoreDiagWriteStatusString(0, "*** vmalloc(%d) for backing up downloading image failed\n", (int)combineImageLen);
						}
						else {
							bkupLmemSize = afeParam.size;
							bkupSdramSize = afeImage.size;
							BcmAdslCoreDiagWriteStatusString(0, "*** vmalloc(%d) for backing up downloading image sucessful\n", (int)combineImageLen);
						}
					}
					else {
						bkupLmemSize = afeParam.size;
						bkupSdramSize = afeImage.size;
						if(NULL == pBkupImage) {
							pBkupImage = (char *)vmalloc(combineImageLen);
							if(NULL == pBkupImage) {
								bkupLmemSize = 0;
								bkupSdramSize = 0;
								BcmAdslCoreDiagWriteStatusString(0, "*** vmalloc(%d) for backing up downloading image failed\n", (int)combineImageLen);
							}
							else {
								BcmAdslCoreDiagWriteStatusString(0, "*** vmalloc(%d) for backing up downloading image sucessful\n", (int)combineImageLen);
							}
						}
					}
				}
#endif
				break;
			case kDslAfeTestPatternSend:
				pSdram = (char*) AdslCoreGetSdramImageStart() + AdslCoreGetSdramImageSize();
				BcmAdslCoreAfeTestInit(&afeParam, pSdram, pCmd->param.dslAfeTestSpec.afeParamSize);
				pSdram += (pCmd->param.dslAfeTestSpec.afeParamSize + 0xF) & ~0xF;
				BcmAdslCoreAfeTestInit(&afeImage, pSdram, pCmd->param.dslAfeTestSpec.imageSize);
				break;

			case kDslAfeTestLoadBuffer:
				n = pCmd->param.dslAfeTestSpec.imageSize;	/* buffer address is here */
				pSdram = (((int) n & 0xF0000000) == 0x10000000) ? ADSL_ADDR_TO_HOST(n) : (void *) n;
				BcmAdslCoreAfeTestInit(&afeParam, pSdram, pCmd->param.dslAfeTestSpec.afeParamSize);
				BcmAdslCoreAfeTestInit(&afeImage, 0, 0);
				if (0 == afeParam.size)
					(*bcmLoadBufferCompleteFunc)(0);
				break;
		}
		return;
	}

	if ((0 == afeParam.size) && (0 == afeImage.size))
		return;

	if (pCmd->param.dslAfeTestSpec.afeParamSize != 0) {
#ifdef LMEM_ACCESS_WORKAROUND
		if( ((0x632680a0 == PERF->RevID) || (0x632680b0 == PERF->RevID)) &&
			(afeParam.cnt == 0) && (AC_FALSE == AdslCoreIsPhyMipsRunning()) &&
			(((uint)afeParam.pSdram & HOST_LMEM_BASE) == HOST_LMEM_BASE) )
			AdslCoreRunPhyMipsInSDRAM();
#endif
		pAfeData = &afeParam;
		frNum = (ulong) (pCmd->param.dslAfeTestSpec.afeParamPtr) & 0xFF;
		dataLen = pCmd->param.dslAfeTestSpec.afeParamSize;
#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
		pBkupAddr = pBkupImage;
#endif
	}
	else if (pCmd->param.dslAfeTestSpec.imageSize != 0) {
		pAfeData = &afeImage;
		if (NULL == pAfeData->pSdram) {
			pAfeData->pSdram = AdslCoreSetSdramImageAddr(((ulong*)HOST_LMEM_BASE)[2], pAfeData->size);
			if(NULL == pAfeData->pSdram) {
				if(0 == pAfeData->frameCnt) {
					BcmAdslCoreDiagWriteStatusString(0, "Can not load image! SDRAM image offset(0x%x) is below the Host reserved memory offset(0x%x)",
						(((uint*)HOST_LMEM_BASE)[2] & (ADSL_PHY_SDRAM_PAGE_SIZE-1)), ADSL_PHY_SDRAM_BIAS);
					AdslDrvPrintf (TEXT("Can not load image! SDRAM image offset(0x%x) is below the Host reserved memory offset(0x%x)\n"),
						(((uint*)HOST_LMEM_BASE)[2] & (ADSL_PHY_SDRAM_PAGE_SIZE-1)), ADSL_PHY_SDRAM_BIAS);
				}
				BcmAdslCoreAfeTestAck (pAfeData, pAfeData->frameCnt, pAfeData->frameCnt);
				pAfeData->frameCnt = (pAfeData->frameCnt + 1) & 0xFF;
				return;
			}
		}
		frNum = (ulong) (pCmd->param.dslAfeTestSpec.imagePtr) & 0xFF;
		dataLen = pCmd->param.dslAfeTestSpec.imageSize;
#if defined(__KERNEL__) && \
	(!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
		if(pBkupImage)
			pBkupAddr = pBkupImage+bkupLmemSize;
#endif
	}
	else
		pAfeData = NULL;

	if (NULL != pAfeData) {
		
		if (frNum != pAfeData->frameCnt) {
			BcmAdslCoreAfeTestAck (pAfeData, (pAfeData->frameCnt - 1) & 0xFF, frNum);
			return;
		}
		n = pAfeData->size - pAfeData->cnt;
		if (dataLen > n)
			dataLen = n;
		pSdram = pAfeData->pSdram + pAfeData->cnt;
		memcpy (
			pSdram, 
			((char *) pCmd) + sizeof(pCmd->command) + sizeof(pCmd->param.dslAfeTestSpec),
			dataLen);
#if defined(__KERNEL__) && (defined(CONFIG_BCM96368) || defined(CONFIG_BCM963268))
		if(pBkupAddr) {
			pBkupAddr += pAfeData->cnt;
			memcpy (
				pBkupAddr, 
				((char *) pCmd) + sizeof(pCmd->command) + sizeof(pCmd->param.dslAfeTestSpec),
				dataLen);
		}
#endif
		pAfeData->cnt += dataLen;
		pAfeData->frameCnt = (pAfeData->frameCnt + 1) & 0xFF;
		BcmAdslCoreAfeTestAck (pAfeData, frNum, frNum);
	}

	if ((afeParam.cnt == afeParam.size) && (afeImage.cnt == afeImage.size)) {
		switch (pCmd->param.dslAfeTestSpec.type) {
			case kDslAfeTestLoadImageOnly:
				BcmAdslCoreDiagWriteStatusString(0, "LoadImageOnly Done");
				AdslCoreSetXfaceOffset(HOST_LMEM_BASE, afeParam.size);
#ifdef PHY_BLOCK_TEST
				AdslDrvPrintf (TEXT("\nLMEM & SDRAM images load done\nLMEM addr=0x%X size=%d  SDRAM addr=0x%X size=%d\n"),
					(uint)afeParam.pSdram, (int)afeParam.size, (uint)afeImage.pSdram, (int)afeImage.size);
#endif
				adslCoreAlwaysReset = AC_FALSE;
				for(i = 0; i < MAX_DSL_LINE; i++)
					adslCoreConnectionMode[i] = AC_TRUE;
				break;
			case kDslAfeTestLoadImage:
				BcmAdslCoreDiagWriteStatusString(0, "LoadImage Done");
				AdslCoreSetXfaceOffset(HOST_LMEM_BASE, afeParam.size);
				adslCoreAlwaysReset = AC_FALSE;
				
				for(i = 0; i < MAX_DSL_LINE; i++) {
					adslCoreConnectionMode[i] = AC_TRUE;
#ifdef G992_ANNEXC
					pAdslCoreCfgProfile[i]->adslAnnexCParam &= ~(kAdslCfgDemodCapOn | kAdslCfgDemodCap2On);
#else
					pAdslCoreCfgProfile[i]->adslAnnexAParam &= ~(kAdslCfgDemodCapOn | kAdslCfgDemodCap2On);
#endif
				}
#if defined(PHY_BLOCK_TEST) && defined(PHY_BLOCK_TEST_SAR)
				adslNewImg = AC_TRUE;
#endif
#ifdef ADSLDRV_STATUS_PROFILING
				BcmXdslCoreDrvProfileInfoClear();
#endif
				BcmAdslCoreStart(DIAG_DATA_EYE, AC_FALSE);
				if (DIAG_DEBUG_CMD_LOG_DATA == diagDebugCmd.cmd) {
					BcmAdslCoreDiagStartLog(diagDebugCmd.param1, diagDebugCmd.param2);
					diagDebugCmd.cmd = 0;
				}
				break;
			case kDslAfeTestPatternSend:
				BcmAdslCoreAfeTestStart(&afeParam, &afeImage);
				break;
			case kDslAfeTestLoadBuffer:
				(*bcmLoadBufferCompleteFunc)(afeParam.size);
				break;
		}
		afeParam.cnt = 0;
		afeParam.frameCnt = UN_INIT_HDR_MARK;
		afeImage.cnt = 0;
		afeImage.frameCnt = UN_INIT_HDR_MARK;
	}
}

void BcmAdslCoreSendBuffer(ulong statusCode, uchar *bufPtr, ulong bufLen)
{
	ULONG				n, frCnt, dataCnt, dataSize;
	dslStatusStruct		*pStatus;
	char			testFrame[1000], *pData;

	pStatus = (void *) testFrame;
	dataSize = sizeof(testFrame) - sizeof(pStatus->code) - sizeof(pStatus->param.dslDataAvail) - 16;
	pData = testFrame + sizeof(pStatus->code) + sizeof(pStatus->param.dslDataAvail);

	dataCnt  = 0;
	frCnt	 = 0;
	while (dataCnt < bufLen) {
		n = bufLen - dataCnt;
		if (n > dataSize)
			n = dataSize;

		pStatus->code = statusCode;
		pStatus->param.dslDataAvail.dataPtr = (void *) frCnt;
		pStatus->param.dslDataAvail.dataLen	= n;
		memcpy (pData, bufPtr + dataCnt, n);
		BcmAdslCoreDiagStatusSnooper(pStatus, testFrame, (pData - testFrame) + n);

		dataCnt += n;
		frCnt = (frCnt + 1) & 0xFF;

		if (0 == (frCnt & 7))
			BcmAdslCoreDelay(40);
	}

	pStatus->code = statusCode;
	pStatus->param.dslDataAvail.dataPtr = (void *) frCnt;
	pStatus->param.dslDataAvail.dataLen	= 0;
	BcmAdslCoreDiagStatusSnooper(pStatus, testFrame, pData - testFrame);
}

#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
void BcmAdslCoreSendDmaBuffers(ulong statusCode, int bufNum)
{
	int					len, i;
	dslStatusStruct		*pStatus;
	static char			testFrame[LOG_MAX_DATA_SIZE], *pFrameData;
	LogProtoHeader		*pLogHdr;

	/* 
	**	Send the first 2 DMA buffers as raw/DMA data 
	**  If DMA'd block aren't sent in interrupt handler DslDiags needs to 
	**	see this to know the sampling rate 512 vs. 1024
	*/

	pLogHdr = ((LogProtoHeader *) BcmAdslCoreDiagGetDmaDataAddr(0)) - 1;
	BcmAdslCoreDiagWriteStatusData(
		pLogHdr->logCommmand, 
		(void *) (pLogHdr + 1),
		BcmAdslCoreDiagGetDmaDataSize(0), 
		NULL, 0);

	pLogHdr = ((LogProtoHeader *) BcmAdslCoreDiagGetDmaDataAddr(1)) - 1;
	BcmAdslCoreDiagWriteStatusData(
		pLogHdr->logCommmand, 
		(void *) (pLogHdr + 1),
		BcmAdslCoreDiagGetDmaDataSize(1), NULL, 0);

	/*	Send captured samples as statuses for loopback test */

	pStatus = (void *) testFrame;
	pFrameData = testFrame + sizeof(pStatus->code) + sizeof(pStatus->param.dslDataAvail);
	len	  = BcmAdslCoreDiagGetDmaDataSize(0);

	pStatus->code = kDslDataAvailStatus;
	pStatus->param.dslDataAvail.dataPtr = (void *) 0xFFF00000;
	pStatus->param.dslDataAvail.dataLen = len * bufNum;
	BcmAdslCoreDiagStatusSnooper(pStatus, testFrame, pFrameData - testFrame);

	for (i = 0; i < bufNum; i++) {
		pStatus->code = statusCode;
		pStatus->param.dslDataAvail.dataPtr = (void *) (i & 0xFF);
		pStatus->param.dslDataAvail.dataLen = len;
		len	  = BcmAdslCoreDiagGetDmaDataSize(i);
		memcpy (pFrameData, BcmAdslCoreDiagGetDmaDataAddr(i), len);
		BcmAdslCoreDiagStatusSnooper(pStatus, testFrame, (pFrameData - testFrame) + len);

		if (7 == (i & 7))
			BcmAdslCoreDelay(40);
	}

	pStatus->code = statusCode;
	pStatus->param.dslDataAvail.dataPtr = (void *) bufNum;
	pStatus->param.dslDataAvail.dataLen	= 0;
	BcmAdslCoreDiagStatusSnooper(pStatus, testFrame, pFrameData - testFrame);
}
#endif

void BcmAdslCoreAfeTestStatus(dslStatusStruct *status)
{
	/* Original kDslDataAvailStatus message has already gone to BcmAdslCoreDiagStatusSnooper */

	BcmAdslCoreSendBuffer(status->code, status->param.dslDataAvail.dataPtr, status->param.dslDataAvail.dataLen);
}

ulong BcmAdslCoreGetConfiguredMod(uchar lineId)
{
	return adslCoreConnectionParam[lineId].param.dslModeSpec.capabilities.modulations;
}

/*
**
**	Support for chip test (not standard PHY DSL interface
**
*/

#if 1 || defined(PHY_BLOCK_TEST)

/* Memory map for Monitor (System Calls) */

#define MEM_MONITOR_ID              0
#define MEM_MONITOR_PARAM_1         1
#define MEM_MONITOR_PARAM_2         2
#define MEM_MONITOR_PARAM_3         3
#define MEM_MONITOR_PARAM_4         4

#define GLOBAL_EVENT_ZERO_BSS          0  /* bssStart, bssEnd */
#define GLOBAL_EVENT_LOAD_IMAGE     2   /* imageId, imageStart */
#define GLOBAL_EVENT_DUMP_IMAGE     3   /* imageId, imageStart, imageEnd */
#define GLOBAL_EVENT_PR_MSG         4   /* num, msgFormat, msgData */
#define GLOBAL_EVENT_PROBE_IMAGE    5   /* imageId, imageStart, imageEnd */
#define GLOBAL_EVENT_READ_FILE      20  /* filename, bufferStart, size */
#define GLOBAL_EVENT_SIM_STOP       8   /* N/A */

Bool		testCmdInProgress = AC_FALSE;
Bool		testFileReadCmdInProgress = AC_FALSE;
Bool		testPlaybackStopRq = AC_FALSE;
OS_TICKS	tFileReadCmd, tTestCmd;

#ifdef PHY_BLOCK_TEST_SAR
void AdslDrvXtmLinkUp(void);
void AdslDrvXtmLinkDown(void);
#endif

/*
**	File cache control data
*/

// #define	READ_FILE_CACHE_SIZE		0x4000

void  BcmAdslCoreDebugReadFile(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen);
void  __BcmAdslCoreDebugReadFileCached(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen);
void  BcmAdslCoreDebugReadFileCached(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen);
#ifdef READ_FILE_CACHE_SIZE
void  (*bcmDebugReadFileFunc)(char *fName, ulong rcmd, uchar *pBuf, ulong bLen) = BcmAdslCoreDebugReadFileCached;
#else
void  (*bcmDebugReadFileFunc)(char *fName, ulong rcmd, uchar *pBuf, ulong bLen) = BcmAdslCoreDebugReadFile;
#endif

char	*dirFileName = NULL;
ulong	dirFileOffset = 0;
char	*cacheFileName = NULL;
uchar	*cacheBufPtr = NULL;
ulong	cacheSize = 0;
circBufferStruct	cacheCircBuf;

uchar	*rdBufPtr = NULL;
ulong	rdBufLen = 0;

void BcmAdslCoreDebugSendCommand(char *fileName, ushort cmd, ulong offset, ulong len, ulong bufAddr)
{
	DiagProtoFrame		*pDiagFrame;
	DiagTestData		*pTestCmd;
	char				testFrame[sizeof(LogProtoHeader)+sizeof(DiagTestData)];

	pDiagFrame	= (void *) testFrame;
	pTestCmd	= (void *) pDiagFrame->diagData;
	*(short *)pDiagFrame->diagHdr.logProtoId = *(short *) LOG_PROTO_ID;
	pDiagFrame->diagHdr.logPartyId	= LOG_PARTY_CLIENT;
	pDiagFrame->diagHdr.logCommmand = LOG_CMD_DEBUG;

	pTestCmd->cmd		= cmd;
	pTestCmd->cmdId		= 0;
	pTestCmd->offset	= offset;
	pTestCmd->len		= len;
	pTestCmd->bufPtr	= bufAddr;
	memcpy (pTestCmd->fileName, fileName, DIAG_TEST_FILENAME_LEN);
	BcmAdslCoreDiagWrite(testFrame, sizeof(LogProtoHeader)+sizeof(DiagTestData));
}

void BcmAdslCoreDebugWriteFile(char *fileName, char cmd, uchar *bufPtr, ulong bufLen)
{
	BcmAdslCoreDebugSendCommand(fileName, cmd, 0, bufLen, (ulong) bufPtr);
	BcmAdslCoreSendBuffer(kDslDataAvailStatus, bufPtr, bufLen);
}

void BcmAdslCoreDebugCompleteCommand(ulong rdSize)
{
	char	strBuf[16];
	ulong	monitorId, reqAddr, reqSize;
	volatile ulong	*pAdslLmem = (void *) HOST_LMEM_BASE;
	
	monitorId = pAdslLmem[MEM_MONITOR_ID];
	reqAddr = pAdslLmem[MEM_MONITOR_PARAM_2];
	reqSize =  pAdslLmem[MEM_MONITOR_PARAM_3];
	
	if ( (GLOBAL_EVENT_READ_FILE == monitorId) || (GLOBAL_EVENT_LOAD_IMAGE == monitorId) ) {
		pAdslLmem[MEM_MONITOR_PARAM_2] = rdSize;
		if(GLOBAL_EVENT_READ_FILE == monitorId ) {
			if( rdSize < reqSize ) {
				BcmAdslCoreDiagWriteStatusString(0, "READ_EOF: fn=%s addr=0x%X size=%d reqSize=%d dstAddr=0x%X\n",
					(NULL != rdFileName) ? rdFileName: "Unknown",
					(uint)afeParam.pSdram, (int)rdSize, (int)reqSize, (uint)reqAddr);
				dirFileName = NULL;
				dirFileOffset = 0;
				rdFileName = NULL;
				BcmAdslDiagStatSaveStop();
			}
		}
		else {
			BcmAdslCoreDiagWriteStatusString (0, "LOAD_DONE: fn=%s addr=0x%X size=%d\n",
				(NULL != rdFileName) ? rdFileName: "Unknown",
				(uint)afeParam.pSdram, (int)afeParam.size);
			dirFileName = NULL;
			dirFileOffset = 0;
			rdFileName = NULL;
		}
	}
	
	testCmdInProgress = AC_FALSE;
	adslCoreStarted = AC_TRUE;
	
	sprintf(strBuf, "0x%X", (uint)monitorId);
	BcmAdslCoreDiagSaveStatusString("CMD_COMPLETE: %s fn=%s rdSize=%d reqSize=%d reqAddr=0x%X\n",
		(GLOBAL_EVENT_READ_FILE==monitorId) ? "READ": (GLOBAL_EVENT_LOAD_IMAGE==monitorId) ? "LOAD": strBuf,
		(NULL != rdFileName) ? rdFileName: "Unknown",
		rdSize, reqSize, reqAddr);
	
	pAdslLmem[MEM_MONITOR_ID] ^= (ulong) -1;
}

void BcmAdslCoreDebugReadEndOfFile(void)
{
	if (testPlaybackStopRq)
		testPlaybackStopRq = AC_FALSE;
	BcmAdslCoreSetLoadBufferCompleteFunc(NULL);
	BcmCoreDpcSyncEnter();
	BcmAdslCoreDebugCompleteCommand(0);
	BcmCoreDpcSyncExit();
}

int __BcmAdslCoreDebugReadFileComplete(int rdSize, Bool bCompleteCmd)
{
	testFileReadCmdInProgress = AC_FALSE;
	if (testPlaybackStopRq) {
		testPlaybackStopRq = AC_FALSE;
		rdSize = 0;
	}

	if (rdFileName == dirFileName)
		dirFileOffset += rdSize;

	/* 
	** if read problem (rdSize == 0) leave command pending and be ready for
	** DslDiags retransmissions (i.e. leave complete function pointer
	*/

	if (rdSize != 0) {
		BcmAdslCoreSetLoadBufferCompleteFunc(NULL);
		if (bCompleteCmd) {
			BcmCoreDpcSyncEnter();
			BcmAdslCoreDebugCompleteCommand(rdSize);
			BcmCoreDpcSyncExit();
		}
	}
	return rdSize;
}

void BcmAdslCoreDebugReadFileComplete(int rdSize)
{
	__BcmAdslCoreDebugReadFileComplete(rdSize, true);
}

#ifdef READ_FILE_CACHE_SIZE

void BcmAdslCoreDebugReadFileCacheComplete(int rdSize)
{
	CircBufferWriteUpdateContig(&cacheCircBuf, rdSize);
	rdSize = __BcmAdslCoreDebugReadFileComplete(rdSize, false);

	if ((rdBufLen != 0) && (rdSize != 0))
		__BcmAdslCoreDebugReadFileCached(cacheFileName, DIAG_TEST_CMD_READ, rdBufPtr, rdBufLen);
}

#endif

void __BcmAdslCoreDebugReadFile(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen, void *fnComplPtr)
{
	ulong	offset = 0;

	if (NULL == dirFileName) {
		dirFileName = fileName;
		dirFileOffset = 0;
	}
	
	if (fileName == dirFileName)
		offset = dirFileOffset;
	rdFileName = fileName;
#ifndef PHY_BLOCK_TEST
	BcmAdslCoreDiagSaveStatusString("CMD_REQ: %s fn=%s offset=%d len=%d addr=0x%X\n",
		(readCmd == DIAG_TEST_CMD_READ) ? "READ": "LOAD",
		fileName, (int)offset , (int)bufLen, (uint)bufPtr);
#else
	AdslDrvPrintf (TEXT("%s: fn=%s offset=%d len=%d addr=0x%X\n"),
		(readCmd == DIAG_TEST_CMD_READ) ? "READ": "LOAD",
		fileName, (int)offset , (int)bufLen, (uint)bufPtr);
#endif

	bcmOsGetTime(&tFileReadCmd);
	testFileReadCmdInProgress = AC_TRUE;
	BcmAdslCoreSetLoadBufferCompleteFunc(fnComplPtr);
	BcmAdslCoreAfeTestInit(&afeParam, bufPtr, bufLen);
	BcmAdslCoreAfeTestInit(&afeImage, 0, 0);
	BcmAdslCoreDebugSendCommand(fileName, readCmd, offset, bufLen, (ulong) bufPtr);
}

void BcmAdslCoreDebugReadFile(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen)
{
	return __BcmAdslCoreDebugReadFile(fileName, readCmd, bufPtr, bufLen, BcmAdslCoreDebugReadFileComplete);
}

/*
**
**	File cache control data
**
*/

#ifdef READ_FILE_CACHE_SIZE

void *BcmAdslCoreDebugCacheAlloc(ulong *pBufSize)
{
	void	*pMem;
	ulong	bufSize = *pBufSize;

	do { 
#if defined(VXWORKS)
	    pMem = (void *) malloc(bufSize);
#elif defined(__KERNEL__)
		pMem = (void *) kmalloc(bufSize, GFP_KERNEL);
#endif
	} while ((NULL == pMem) && ((bufSize >> 1) > 0x2000));
	*pBufSize = bufSize;
	return pMem;
}

ulong BcmAdslCoreDebugReadCacheContig(uchar *bufPtr, ulong bufLen)
{
	ulong		n;

	if (0 == bufLen)
		return 0;
	if (0 == (n = CircBufferGetReadContig(&cacheCircBuf))) 
		return 0;

	if (n > bufLen)
		n = bufLen;

	memcpy (ADSL_ADDR_TO_HOST(bufPtr), CircBufferGetReadPtr(&cacheCircBuf), n);
	CircBufferReadUpdateContig(&cacheCircBuf, n);
	return n;
}

void __BcmAdslCoreDebugReadFileCached(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen)
{
	ulong		n;

	do {
	  n = BcmAdslCoreDebugReadCacheContig(bufPtr, bufLen);
	  bufPtr += n;
	  bufLen -= n;
	  if (0 == bufLen)
		  break;

	  n = BcmAdslCoreDebugReadCacheContig(bufPtr, bufLen);
	  bufPtr += n;
	  bufLen -= n;
	} while (0);

	rdBufPtr = bufPtr;
	rdBufLen = bufLen;

	if (!testFileReadCmdInProgress && ((n = CircBufferGetWriteContig(&cacheCircBuf)) != 0))
		__BcmAdslCoreDebugReadFile(fileName, readCmd, CircBufferGetWritePtr(&cacheCircBuf), n, BcmAdslCoreDebugReadFileCacheComplete);
	if (0 == bufLen)
		BcmAdslCoreDebugCompleteCommand(0);
}

void BcmAdslCoreDebugReadFileCached(char *fileName, ulong readCmd, uchar *bufPtr, ulong bufLen)
{
	if (NULL == cacheFileName) {
		cacheSize = READ_FILE_CACHE_SIZE;
		cacheBufPtr = BcmAdslCoreDebugCacheAlloc(&cacheSize);
		BcmAdslCoreDiagWriteStatusString(0, "AllocateCache: ptr=0x%X, size=%d", (ulong) cacheBufPtr, cacheSize);
		if (NULL == cacheBufPtr) {
			bcmDebugReadFileFunc = BcmAdslCoreDebugReadFile;
			return BcmAdslCoreDebugReadFile(fileName, readCmd, bufPtr, bufLen);
		}
		cacheFileName = fileName;
		CircBufferInit(&cacheCircBuf, cacheBufPtr, cacheSize);
	}
	else if (cacheFileName != fileName)
		return BcmAdslCoreDebugReadFile(fileName, readCmd, bufPtr, bufLen);

	__BcmAdslCoreDebugReadFileCached(fileName, readCmd, bufPtr, bufLen);
}
#endif

void BcmAdslCoreDebugTestComplete(void)
{
	BcmAdslCoreDebugSendCommand("", DIAG_TEST_CMD_TEST_COMPLETE, 0, 0, 0);
}

void BcmAdslCoreDebugPlaybackStop(void)
{
	testPlaybackStopRq = AC_TRUE;
}

void BcmAdslCoreDebugPlaybackResume(void)
{
	if (testCmdInProgress) {
		BcmAdslCoreSetLoadBufferCompleteFunc(NULL);
#ifdef READ_FILE_CACHE_SIZE
		__BcmAdslCoreDebugReadFileCached(cacheFileName, DIAG_TEST_CMD_READ, rdBufPtr, rdBufLen);
#else
		BcmAdslCoreDebugCompleteCommand(0);
#endif
	}
}

void BcmAdslCoreDebugReset(void)
{
	if (NULL != cacheBufPtr) {
#if defined(VXWORKS)
		free (cacheBufPtr);
#elif defined(__KERNEL__)
		kfree(cacheBufPtr);
#endif
		cacheBufPtr = NULL;
	}
	cacheFileName = NULL;
#ifdef READ_FILE_CACHE_SIZE
	bcmDebugReadFileFunc = BcmAdslCoreDebugReadFileCached;
#else
	bcmDebugReadFileFunc = BcmAdslCoreDebugReadFile;
#endif
	testCmdInProgress = AC_FALSE;
	dirFileName = NULL;
	dirFileOffset = 0;
	rdFileName = NULL;
	BcmAdslCoreDebugSendCommand("", DIAG_TEST_CMD_TEST_COMPLETE, 0, 0, 0);
}

void BcmAdslCoreDebugTimer(void)
{
	volatile ulong	*pAdslLmem = (void *) HOST_LMEM_BASE;
	char		*pName;
	OS_TICKS	tMs;

	if (testCmdInProgress && testFileReadCmdInProgress) {
		bcmOsGetTime(&tMs);
		tMs = (tMs - tFileReadCmd) * BCMOS_MSEC_PER_TICK;
		if (tMs > 5000) {
			BcmAdslCoreDiagWriteStatusString(0, "BcmAdslCoreDebugTimer: FileReadCmd timeout time=%d", tMs);
			pName = ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_1]);
			(*bcmDebugReadFileFunc)(
				pName,
				DIAG_TEST_CMD_READ,
				(uchar *) pAdslLmem[MEM_MONITOR_PARAM_2],
				pAdslLmem[MEM_MONITOR_PARAM_3]);
		}
	}
	else if (!testCmdInProgress && BcmAdslCoreIsTestCommand()) {
		bcmOsGetTime(&tMs);
		tMs = (tMs - tTestCmd) * BCMOS_MSEC_PER_TICK;
		if (tMs > 100) {
			BcmAdslCoreDiagWriteStatusString(0, "BcmAdslCoreDebugTimer: TestCmd=%d time=%d", pAdslLmem[MEM_MONITOR_ID], tMs);
#if defined(VXWORKS) || defined(TARG_OS_RTEMS)
			if (irqSemId != 0 )
				bcmOsSemGive (irqSemId);
#else
			if (irqDpcId != NULL)
				bcmOsDpcEnqueue(irqDpcId);
#endif
		}
	}

}

Bool BcmAdslCoreIsTestCommand(void)
{
	volatile ulong	*pAdslLmem = (void *) HOST_LMEM_BASE;

	if (testCmdInProgress)
		return AC_FALSE;

	return ((pAdslLmem[MEM_MONITOR_ID] & 0xFFFFFF00) == 0);
}

void BcmAdslCoreProcessTestCommand(void)
{
	volatile ulong	*pAdslLmem = (void *) HOST_LMEM_BASE;
	char	*pName;
#ifdef PHY_BLOCK_TEST
	uchar	cmd;
#endif

	if (!BcmAdslCoreIsTestCommand())
		return;
	bcmOsGetTime(&tTestCmd);
	
	adslCoreStarted = AC_FALSE;
	switch (pAdslLmem[MEM_MONITOR_ID]) {
		case GLOBAL_EVENT_LOAD_IMAGE:
			pName = ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_1]);
			testCmdInProgress = AC_TRUE;
			BcmAdslCoreDebugReadFile(
				pName,
				DIAG_TEST_CMD_LOAD,
				(uchar *) pAdslLmem[MEM_MONITOR_PARAM_2],
				pAdslLmem[MEM_MONITOR_PARAM_3]);
			testCmdInProgress = AC_TRUE;
			break;
		case GLOBAL_EVENT_READ_FILE:
			pName = ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_1]);
			testCmdInProgress = AC_TRUE;
			(*bcmDebugReadFileFunc)(
				pName,
				DIAG_TEST_CMD_READ,
				(uchar *) pAdslLmem[MEM_MONITOR_PARAM_2],
				pAdslLmem[MEM_MONITOR_PARAM_3]);
			testCmdInProgress = AC_TRUE;
			break;
#ifdef PHY_BLOCK_TEST
		case GLOBAL_EVENT_DUMP_IMAGE:
			cmd = DIAG_TEST_CMD_WRITE;
			goto _write_file;
		case GLOBAL_EVENT_PROBE_IMAGE:
			cmd = DIAG_TEST_CMD_APPEND;
_write_file:
			pName = ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_1]);
			
			BcmAdslCoreDebugWriteFile(
				pName,
				cmd,
				ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_2]),
				pAdslLmem[MEM_MONITOR_PARAM_3] - pAdslLmem[MEM_MONITOR_PARAM_2] + 1);
			// BcmAdslCoreDelay(40);
			BcmAdslCoreDebugCompleteCommand(0);
			break;

		case GLOBAL_EVENT_PR_MSG:
#ifdef PHY_BLOCK_TEST_SAR
			if (adslNewImg) {
				AdslDrvXtmLinkUp();
				adslNewImg = AC_FALSE;
			}
#endif
			pName = ADSL_ADDR_TO_HOST(pAdslLmem[MEM_MONITOR_PARAM_2]);
			BcmAdslCoreDiagWriteStatusString(
				0,
				pName,
				pAdslLmem[MEM_MONITOR_PARAM_3],
				pAdslLmem[MEM_MONITOR_PARAM_4]);
			BcmAdslCoreDebugCompleteCommand(0);
			break;

		case GLOBAL_EVENT_SIM_STOP:
#ifdef PHY_BLOCK_TEST_SAR
			AdslDrvXtmLinkDown();
#endif
			BcmAdslCoreDebugSendCommand("", DIAG_TEST_CMD_TEST_COMPLETE, 0, 0, 0);
			BcmAdslCoreDebugCompleteCommand(0);
			break;
#endif
		case GLOBAL_EVENT_ZERO_BSS:
			BcmAdslCoreDebugCompleteCommand(0);
			break;
		default:
			AdslDrvPrintf (TEXT("BcmAdslCoreProcessTestCommand: unknown cmd = 0x%lX\n"), pAdslLmem[MEM_MONITOR_ID]);
			break;
	}
	
	if (!testCmdInProgress)
		adslCoreStarted = AC_TRUE;
}

#endif /* PHY_BLOCK_TEST */

#ifdef PHY_PROFILE_SUPPORT

#ifdef SYS_RANDOM_GEN
#define PROF_TIMER_SEED_INIT	srand(jiffies)
#define PROF_RANDOM32_GEN 		rand()
#else
LOCAL uint32 profileTimerSeed = 0;
LOCAL uint32 random32(uint32 *seed);

LOCAL uint32 random32(uint32 *seed)	/* FIXME: will get a more sophiscated algorithm later */
{
	*seed = 16807*(*seed);
	*seed += (((*seed) >> 16) & 0xffff);
	return *seed;
}

#define PROF_TIMER_SEED_INIT	(profileTimerSeed = jiffies)
#define PROF_RANDOM32_GEN 		random32(&profileTimerSeed)
#endif

LOCAL void BcmAdslCoreProfileTimerFn(ulong arg);

LOCAL int profileStarted = 0;
LOCAL struct timer_list profileTimer;

LOCAL void BcmAdslCoreProfilingStart(void)
{
	init_timer(&profileTimer);
	profileTimer.function = BcmAdslCoreProfileTimerFn;
	profileTimer.expires = 2;	/* 10ms */
	profileTimer.data = 0;
	PROF_TIMER_SEED_INIT;
	add_timer(&profileTimer);
	profileStarted = 1;
}

void BcmAdslCoreProfilingStop(void)
{
	if(profileStarted) {
		del_timer(&profileTimer);
		profileStarted = 0;
	}
}


LOCAL void BcmAdslCoreProfileTimerFn(ulong arg)
{
	ulong 		cycleCnt0;
	ulong 		randomDelay;
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	
	if(!profileStarted)
		return;
	
	/* Wait some random time within 250us or 250* CyclesPerUs */
	randomDelay = PROF_RANDOM32_GEN;
	
#ifdef PROF_RES_IN_US
	randomDelay = randomDelay % 250;
	cycleCnt0 = BcmAdslCoreGetCycleCount();	
	while (BcmAdslCoreCycleTimeElapsedUs(BcmAdslCoreGetCycleCount(), cycleCnt0) < randomDelay);
#else
	randomDelay = randomDelay % (250 *  ((adslCoreCyclesPerMs+500)/1000));
	cycleCnt0 = BcmAdslCoreGetCycleCount();	
	while ((BcmAdslCoreGetCycleCount() - cycleCnt0) < randomDelay);
#endif
	
	/* Initiate PHY interupt */
#if 1
	pAdslEnum[ADSL_HOSTMESSAGE] = 1;
#else
	printk("%s: randomDelay - %d\n", __FUNCTION__, randomDelay);
#endif

	/* Re-schedule the timer */
	mod_timer(&profileTimer, 1 /* 5ms */);
}
#endif /* PHY_PROFILE_SUPPORT */

#ifdef CONFIG_VDSL_SUPPORTED

/* CTRL_CONFIG Register */
#define CTRL_CONFIG_ENABLE_INDIRECT_MODE    1
#define CTRL_CONFIG_MEMORY_WRITE           (0 << 13)
#define CTRL_CONFIG_MEMORY_READ            (1 << 13)
#define CTRL_CONFIG_FFT_DMA_FIFO_WR        (2 << 13)
#define CTRL_CONFIG_FFT_PROC_FIFO_WR       (3 << 13)
#define CTRL_CONFIG_IO_WRITE               (4 << 13)
#define CTRL_CONFIG_IO_READ                (5 << 13)

/* SPI related */
#define MSG_TYPE_HALF_DUPLEX_WRITE  1
#define MSG_TYPE_HALF_DUPLEX_READ   2

#define SPI_CMD_READ                1
#define SPI_CMD_WRITE               0

/* 6306 Control Interface registers */
#define     RBUS_6306_CTRL_CONTROL                 0x000
#define     RBUS_6306_CTRL_CONFIG                   0x004
#define     RBUS_6306_CTRL_CPU_ADDR              0x008
#define     RBUS_6306_CTRL_CPU_READ_DATA    0x00c
#define     RBUS_6306_CTRL_CHIP_ID1                0x038
#define     RBUS_6306_CTRL_CHIP_ID2                0x03C
#define     RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0  0x400
#define     RBUS_6306_CTRL_ANA_PLL_STATUS            0x47c

#if defined(CONFIG_BCM96368)
#define     RBUS_AFE_PRIMARY_BASE            0xB0F57000
#define     RBUS_AFE_BONDING_BASE            0xB0F58000
#define     RBUS_AFE_CONFIG                         0xc
#else
#define     RBUS_AFE_PRIMARY_BASE            0xB0757000
#define     RBUS_AFE_BONDING_BASE            0xB0758000
#define     RBUS_AFE_CONFIG                         0x8
#define     RBUS_AFE_BUS_READ_ADDR          0xf00
#define     RBUS_AFE_PRIMARY_INDIRECT   (RBUS_AFE_PRIMARY_BASE | RBUS_AFE_BUS_READ_ADDR)
#define     RBUS_AFE_BONDING_INDIRECT   (RBUS_AFE_BONDING_BASE | RBUS_AFE_BUS_READ_ADDR)
#endif

#define     RBUS_AFE_X_REG_CONTROL          0x030
#define     RBUS_AFE_X_REG_DATA                0x034

#define     POS_AFE_X_REG_CTRL_START       0x00
#define     POS_AFE_X_REG_CTRL_WRITE       0x02
#define     POS_AFE_X_REG_ADDR                  0x03
#define     POS_AFE_X_REG_SELECT               0x08

#define     RBUS_AFE_SPI_CONTROL1             0x800
#define     RBUS_AFE_SPI_CONTROL2             0x804
#define     RBUS_AFE_SPI_TAIL                      0x808
#define     RBUS_AFE_SPI_TX_MSG_FIFO       0x840
#define     RBUS_AFE_SPI_RX_MSG_FIFO       0x880

#define     vpApiWriteReg(x, y)     (*(unsigned int *)(x) = (y))
#if defined(CONFIG_BCM96368)
#define     vpApiReadReg(x)         (*(unsigned int *)(x))
#else
unsigned int  vpApiReadReg(unsigned int x)
{
   if((x & 0xfff) > 0x7ff)
      return (*(unsigned int *)(x));    /* Direct read; BusClk */
   else {
      /* Indirect read; SysClk */
      if ((x & 0xfffff000) == RBUS_AFE_PRIMARY_BASE) {
         vpApiWriteReg(RBUS_AFE_PRIMARY_INDIRECT, x);
         return (*(unsigned int *)(RBUS_AFE_PRIMARY_INDIRECT));
      }
      else {
         vpApiWriteReg(RBUS_AFE_BONDING_INDIRECT, x);
         return (*(unsigned int *)(RBUS_AFE_BONDING_INDIRECT));
      }
   }
}

#endif

static int  spiWriteInProgress = 0;

static void spiWaitForWriteDone(void);

int IsAfe6306ChipUsed(void)
{
    ulong afeIds[2];
    
    BcmXdslCoreGetAfeBoardId(&afeIds[0]);
    
    return(((afeIds[0] & AFE_CHIP_MASK) == (AFE_CHIP_6306 << AFE_CHIP_SHIFT)) ||
            ((afeIds[1] & AFE_CHIP_MASK) == (AFE_CHIP_6306 << AFE_CHIP_SHIFT)));
}

static void writeToSPITxFIFO(unsigned int *pData, int tailBytes, int prendCount, int numWords) 
{
    int i;
    unsigned int wdata, address, spiControl2;
    
    vsb6306VdslMode = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_CONFIG) & 1;
    
/*	PR2("writeToSPITxFIFO: tailBytes = %d, prendCount = %d\n", tailBytes, prendCount); */
/*	PR1("writeToSPITxFIFO: numWords = %d\n", numWords); */
	/* program SPI master */
	/* SPIMsgData address*/
	for(i=0; i< numWords; i++) {
		address = RBUS_AFE_SPI_TX_MSG_FIFO + i*4;
		vpApiWriteReg(RBUS_AFE_BONDING_BASE | address, pData[i]);
/*		PR2("writeToSPITxFIFO i = %d, data = 0x%08x\n", i, pData[i]); */
	}
	spiControl2 = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_CONTROL2);
	if (vsb6306VdslMode == CTRL_CONTROL_VDSL_MODE) {
		/* In case of VDSL divide the clock further */
		spiControl2 = (spiControl2 & 0xfffff800) | (5 << 8) | 0xff;
	} else {
		/* In case of ADSL don't divide the clock further */
		spiControl2 = (spiControl2 & 0xfffff800) | (6 << 8) | 0x00;
	}
	/* SPI control2 register */
	vpApiWriteReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_CONTROL2, spiControl2);
	/* configure tail register to make it exactly bytes 
	   which should be byte_count + prend_bytes */
	wdata = (tailBytes) << 16 ;
	vpApiWriteReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_TAIL, wdata);	
	/* SPI control1 register */
	wdata = (prendCount << 24) | (0x3 << 16);
	vpApiWriteReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_CONTROL1, wdata);

}

static void waitForSPICmdDone(void)
{
    unsigned int rdata;
    unsigned int command_done = 0x0;
    while (command_done == 0x0){
        rdata = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_CONTROL1);
        command_done = (rdata >> 8) & 0x00000001;
    };
    /* clear the interrupt and other bits */
    vpApiWriteReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_CONTROL1, 0xffff);
    /* clean SPI tail register */ 
    vpApiWriteReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_TAIL, 0x0);
}

__inline__ unsigned int reverseBits(unsigned int input, int numBits)
{
    unsigned int output = 0;
    int i;
    for (i = 0; i < numBits -1; i++) {
        output |= input & 0x1;
        output <<= 1;
        input >>= 1;
    }
    output |= input;
    return output;
}

static void spiWaitForWriteDone(void)
{
    if (spiWriteInProgress) {
        waitForSPICmdDone();
        spiWriteInProgress = 0;
    }
}
#ifdef CONFIG_BCM963268
#define RDATA0_MASK	0xfffff
#define RDATA1_MASK	0xfff
#define	RDATA0_SHIFT	12
#define	RDATA1_SHIFT	20
#else
#define RDATA0_MASK	0x1fffff
#define RDATA1_MASK	0x7ff
#define	RDATA0_SHIFT	11
#define	RDATA1_SHIFT	21
#endif

int spiTo6306Read(int address)
{
    unsigned int byte_count, prend_count;
    unsigned int spi_command0;
    unsigned int msgType, reverse_value;
    unsigned int rdata, rdata0, rdata1;
    unsigned int start_bit;
    unsigned int default_value_vector32;
    unsigned int default_value_vector8;
    unsigned int txdata[8];

    /* Wait for completion of previous SPI Write command */
    spiWaitForWriteDone();

    vsb6306VdslMode = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_CONFIG) & 1;

    if (vsb6306VdslMode == CTRL_CONTROL_VDSL_MODE) {
        default_value_vector32 = 0x7fffffff;   
        default_value_vector8 = 0xff; 
        start_bit = 0x0;
    } else {
        default_value_vector32 = 0x0; 
        default_value_vector8 = 0x0;
        start_bit = 0x1;
    }     

    /* PR1("spiTo6306Read: vsb6306VdslMode = %d\n", vsb6306VdslMode); */

    /* the address is only 16-bit */
    address &= 0xffff;    
    msgType = MSG_TYPE_HALF_DUPLEX_READ; 
    byte_count = 0x6; 
    prend_count = 0x3;
    reverse_value = 0x0;

    /* address need to reverse bits because 6306 is LSB first SPI is MSB first */
    /* reverse address bits */
    address >>= 2;
    reverse_value = reverseBits(address, 14);
    spi_command0 = (start_bit << 15) | ((reverse_value & 0x3fff) << 1); 

    txdata[0] = (msgType << 30 |  byte_count << 16 | spi_command0);
    txdata[1] = (SPI_CMD_READ << 31 ) | default_value_vector32;
    writeToSPITxFIFO(txdata, 5, prend_count, 2);
    
    waitForSPICmdDone();
    
    /* read back the data from receive fifo */
    rdata0 = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_SPI_RX_MSG_FIFO);
    rdata1 = vpApiReadReg(RBUS_AFE_BONDING_BASE | (RBUS_AFE_SPI_RX_MSG_FIFO + 4));

    /* The number of bits that is valid in rdata0 is a result of the hardware design and */
    /* provided by the HW designer based on the design and the waveform. If the design changes */
    /* this may change and need to be found out looking at the waveform */
    /* for rdata0, only [20:0] contain useful information */
    /* for rdata1, only [31:21] contain useful information */
    /* read back data should be {rdata0[20:0], rdata1[31:21]} */
    rdata = ((rdata0 & RDATA0_MASK) << RDATA0_SHIFT) | ((rdata1 >> RDATA1_SHIFT) & RDATA1_MASK);
    
    /* revers bit order due to 6306 LSB first SPI MSB first */
    reverse_value = reverseBits(rdata, 32);

    /* PR2("SPI to 6306 Read operation with address = %08x, data = %08x\n", address, reverse_value); */
    
    return reverse_value;
}

int spiTo6306IndirectRead(int address)
{
    int returnValue;
    unsigned int spi_address, spi_data;
    unsigned int msgType;
    unsigned int byte_count, prend_count;
    unsigned int spi_command0, spi_command1;
    unsigned int reverse_value, start_bit, default_value16, stop_bit;
    unsigned int txdata[8];
    unsigned int indirectLength, incommand, cmdConfig;
    int unit;

    /* Wait for completion of previous SPI Write command */
    spiWaitForWriteDone();
    
    vsb6306VdslMode = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_CONFIG) & 1;
    
    if (vsb6306VdslMode == CTRL_CONTROL_VDSL_MODE) {
        default_value16 = 0x7fff;
        start_bit = 0x0;
        stop_bit  = 0x1;
    } else {
        default_value16 = 0x0;
        start_bit = 0x1;
        stop_bit  = 0x0;
    }

    msgType = MSG_TYPE_HALF_DUPLEX_WRITE;
    byte_count = 9;
    prend_count = 0x2;

    /* write cpu_addr */
    spi_address = RBUS_6306_CTRL_CPU_ADDR >> 2;
    spi_data    = (address & 0xffff);
    reverse_value = reverseBits(spi_address, 14);
    txdata[0] = (msgType << 30 |  byte_count << 16 | (start_bit << 15) | ((reverse_value & 0x3fff) << 1));

    reverse_value = reverseBits(spi_data, 16);
    spi_command1 = (SPI_CMD_WRITE   << 31)| ((reverse_value&0xffff) << 15) | (stop_bit << 14) | (start_bit << 13);   

    /* Config */
    spi_address = RBUS_6306_CTRL_CONFIG >> 2;
    indirectLength = 0;
    unit = address & 0x000f0000;
    if (unit == 0x50000) {
        incommand = CTRL_CONFIG_IO_READ;
    } else
        incommand = CTRL_CONFIG_MEMORY_READ;

    cmdConfig = (incommand | indirectLength << 1 | CTRL_CONFIG_ENABLE_INDIRECT_MODE);

    reverse_value = reverseBits(spi_address, 14);
    txdata[1] = spi_command1 | ((reverse_value>>1) & 0x1fff);

    spi_command0 = (reverse_value << 31) | (SPI_CMD_WRITE << 29);
    spi_data  = cmdConfig & 0xffff;
    reverse_value = reverseBits(spi_data, 16);
    spi_command0 |= ((reverse_value << 13) | (default_value16 & 0x1fff));
    txdata[2] = spi_command0;
    writeToSPITxFIFO(txdata, (byte_count+prend_count), prend_count, 3);
    
    waitForSPICmdDone();

    /* Write the data */
    returnValue = spiTo6306Read(RBUS_6306_CTRL_CPU_READ_DATA);
    /* PR2("SPI to 6306 IndirectRead operation with address = %08x, data = %08x\n", address, returnValue); */
    /* PR2("incommand = %08x, indirectLength = %08x\n", incommand, indirectLength); */
    return returnValue;
}

void spiTo6306Write(int address, unsigned int data)
{
    unsigned int spi_address;
    unsigned int msgType;
    unsigned int byte_count, prend_count;
    unsigned int spi_command0, spi_command1, spi_command2;
    unsigned int reverse_value, start_bit, default_value16;
    unsigned int txdata[8];

    /* Wait for completion of previous SPI Write command */
    spiWaitForWriteDone();
    
    vsb6306VdslMode = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_CONFIG) & 1;
    
    if (vsb6306VdslMode == CTRL_CONTROL_VDSL_MODE) {
        default_value16 = 0x7fff;  
        start_bit = 0x0; 
    } else {
        default_value16 = 0x0;
        start_bit = 0x1;
    }

    /* both address and data are only 16 bit */
    address &= 0xffff;    
    data    &= 0xffff;

    msgType = MSG_TYPE_HALF_DUPLEX_WRITE;
    byte_count = 0x5; 
    spi_address = address >> 2;
    prend_count = 0x2;
    reverse_value = 0x0;

    /* PR2("SPI to 6306 Write operation with address = %08x, data = %08x\n", address, data); */

    /* both address and data need to revers bits because 6306 is LSB first SPI is MSB first */
    /* reverse address bits */
    reverse_value = reverseBits(spi_address, 14);
    
    /* add starting bit at the beginning, 
       then the address to form the first command for SPI interface*/
    spi_command0 = (start_bit << 15) | ((reverse_value & 0x3fff) << 1);
    
    /* reverse data bits */
    reverse_value = reverseBits(data, 16);

    /* read_write direction need to be after the address,
       Then the 14:0 of the data bits. the bit 15 data will
       be on the next transfer */
    spi_command1 = (SPI_CMD_WRITE   << 15)| (reverse_value >> 1 & 0x7fff);
    /* bit 15 data with default value following base on what mode 6306 is */
    spi_command2 =  ((reverse_value << 15) & 0x8000) | default_value16;

    txdata[0] = (msgType << 30 |  byte_count << 16 | spi_command0);
    txdata[1] = spi_command1 << 16 | spi_command2;
    writeToSPITxFIFO(txdata, (byte_count+prend_count), prend_count, 2);


    spiWriteInProgress = 1;
}


void spiTo6306IndirectWrite(int address, unsigned int data)
{
    unsigned int spi_address, spi_data;
    unsigned int msgType;
    unsigned int byte_count, prend_count;
    unsigned int spi_command0, spi_command1;
    unsigned int reverse_value, start_bit, default_value16, stop_bit;
    unsigned int txdata[8];
    unsigned int indirectLength, incommand, cmdConfig;
    int unit;

    /* PR2("spiTo6306IndirectWrite: addr = 0x%08x, data = 0x%08x\n", address, data); */

    /* Wait for completion of previous SPI Write command */
    spiWaitForWriteDone();
    
    vsb6306VdslMode = vpApiReadReg(RBUS_AFE_BONDING_BASE | RBUS_AFE_CONFIG) & 1;
    
    if (vsb6306VdslMode == CTRL_CONTROL_VDSL_MODE) {
        default_value16 = 0x7fff;  
        start_bit = 0x0; 
        stop_bit  = 0x1; 
    } else {
        default_value16 = 0x0; 
        start_bit = 0x1;
        stop_bit  = 0x0;
    }   

    msgType = MSG_TYPE_HALF_DUPLEX_WRITE;
    byte_count = 13; 
    prend_count = 0x2;

    /* write cpu_addr */
    spi_address = RBUS_6306_CTRL_CPU_ADDR >> 2;
    spi_data    = (address & 0xffff);
    reverse_value = reverseBits(spi_address, 14);
    txdata[0] = (msgType << 30 |  byte_count << 16 | (start_bit << 15) | ((reverse_value & 0x3fff) << 1));

    reverse_value = reverseBits(spi_data, 16);
    spi_command1 = (SPI_CMD_WRITE   << 31)| ((reverse_value&0xffff) << 15) | (stop_bit << 14) | (start_bit << 13);   

    /* Config */
    spi_address = RBUS_6306_CTRL_CONFIG >> 2;
    indirectLength = 0;

    unit = address & 0x000f0000;
    if (unit == 0) {
        incommand = CTRL_CONFIG_MEMORY_WRITE;
    } 
    else if (unit  == 0x50000) {
        incommand = CTRL_CONFIG_IO_WRITE;
    } else if (unit  == 0x60000) {
        incommand = CTRL_CONFIG_FFT_PROC_FIFO_WR;
    } else if (unit  == 0x70000) {
        incommand = CTRL_CONFIG_FFT_DMA_FIFO_WR;
    }
    else {
        incommand = CTRL_CONFIG_MEMORY_WRITE; /* Default */
    }
    cmdConfig = (incommand | indirectLength << 1 | CTRL_CONFIG_ENABLE_INDIRECT_MODE);

    reverse_value = reverseBits(spi_address, 14);
    txdata[1] = spi_command1 | ((reverse_value>>1) & 0x1fff);

    spi_command0 = (reverse_value << 31) | (SPI_CMD_WRITE << 29);
    spi_data  = cmdConfig & 0xffff;
    reverse_value = reverseBits(spi_data, 16);
    spi_command0 |= ((reverse_value << 13) | (stop_bit << 12) | (start_bit << 11));
    reverse_value = reverseBits(data, 32);
    txdata[2] = spi_command0 | ((reverse_value >> 21) & 0x7ff);
    txdata[3] = (reverse_value << 11) | (default_value16 & 0x7ff);
    writeToSPITxFIFO(txdata, (byte_count+prend_count), prend_count, 4);
    spiWriteInProgress = 1;
}

#ifdef CONFIG_BCM96368
static uint wrXRegCnt = 0;
static void SoftAfeUtilsWriteXReg(int regBase, int addr, int data, int select)
{
    int regVal, loopCnt=0;

    wrXRegCnt++;
    
    regVal = vpApiReadReg(regBase|RBUS_AFE_X_REG_CONTROL);
    while ((regVal & 0x1) && (++loopCnt < 100000)) {
        regVal = vpApiReadReg(regBase|RBUS_AFE_X_REG_CONTROL);
    }
    if(100000 == loopCnt) {
        AdslDrvPrintf(TEXT("**** wrXRegCnt = %d regAddr = 0x%x regVal = 0x%x ***\n"),
            wrXRegCnt, (uint)(regBase|RBUS_AFE_X_REG_CONTROL), regVal);
        while(1);
    }
    vpApiWriteReg(regBase|RBUS_AFE_X_REG_DATA, data);
    regVal = (1 << POS_AFE_X_REG_CTRL_START) | (1 << POS_AFE_X_REG_CTRL_WRITE) |
                (addr << POS_AFE_X_REG_ADDR) | (select << POS_AFE_X_REG_SELECT);
    vpApiWriteReg(regBase|RBUS_AFE_X_REG_CONTROL, regVal);
}

void PLLPowerUpSequence6306(void)
{
#ifdef CONFIG_BCM963268
    int anaPllReg0Offset = 0xc000;
#else
    int anaPllReg0Offset = 0;
#endif
    AdslDrvPrintf(TEXT("**** PLLPowerUpSequence6306 ****\n"));
    /* PLL power-up sequence */
    /* Power up:  i_config_pll_reg0<13> = 0 */
    spiTo6306Write(RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0, anaPllReg0Offset | 0);
    /* Reset on:  i_config_pll_reg0<12> =1  */
    spiTo6306Write(RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0, anaPllReg0Offset | (1<<12));
    /* Reset off: i_config_pll_reg0<12> = 0 */
    spiTo6306Write(RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0, anaPllReg0Offset | 0);
    /* Reset VCO tuning sequencer: i_config_pll_reg0<5> = 1 */
    spiTo6306Write(RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0, anaPllReg0Offset | (1<<5));
    /* Wait for all writes to be done */
    spiWaitForWriteDone();
    /* Wait 30 usec */
    BcmAdslCoreDelay(1);
    /* Release VCO tuning sequencer: i_config_pll_reg0<5> = 1 */
    spiTo6306Write(RBUS_6306_CTRL_ANA_CONFIG_PLL_REG0, anaPllReg0Offset | 0);
    spiWaitForWriteDone();
    /* Wait 20 msec */
    BcmAdslCoreDelay(20);
    /* Check PLL lock bit */
    while ((spiTo6306Read(RBUS_6306_CTRL_ANA_PLL_STATUS) & 0x40000) == 0);
}
#endif	/* CONFIG_BCM96368 */

void SetupReferenceClockTo6306(void)
{
    AdslDrvPrintf(TEXT("**** SetupReferenceClockTo6306 ****\n"));
#ifdef CONFIG_BCM96368
    /* procedure to provide correct reference clock (64MHz) to 6306 */
    SoftAfeUtilsWriteXReg(RBUS_AFE_PRIMARY_BASE, 20, 0x05, 1);
    BcmAdslCoreDelay(1);
    /* 8-bit control for PLL */
    SoftAfeUtilsWriteXReg(RBUS_AFE_PRIMARY_BASE, 21, 0x03, 1);
    BcmAdslCoreDelay(1);
    /* Power up PLL */
    SoftAfeUtilsWriteXReg(RBUS_AFE_PRIMARY_BASE, 19, 0, 1);
    BcmAdslCoreDelay(1);
    SoftAfeUtilsWriteXReg(RBUS_AFE_PRIMARY_BASE, 24, 0x1c, 1);
#else	/* CONFIG_BCM963268 */
    if(0 ==(MISC->miscPllLock_stat & MISC_LCPLL_LOCK))
        AdslDrvPrintf(TEXT("SetupReferenceClockTo6306: LCPLL is not lock!\n"));
    if( (0x632680a0 == PERF->RevID) || (0x632680b0 == PERF->RevID) )
        MISC->miscLcpll_ctrl |= MISC_CLK64_EXOUT_EN | MISC_CLK64_ENABLE;
    else
        MISC->miscLcpll_ctrl |= MISC_CLK64_EXOUT_EN;
#endif
}
#endif /* CONFIG_VDSL_SUPPORTED */
