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
//**************************************************************************
// File Name  : Adsl.c
//
// Description: This file contains API for ADSL PHY
//
//**************************************************************************

#ifdef _WIN32_WCE
#include <windows.h>
#include <types.h>
#include <ceddk.h>
#include <memory.h>
#include <linklist.h>
#include <nkintr.h>
#include <hwcomapi.h>
#include <devload.h>
#include <pm.h>
#elif defined(_CFE_)
#include "lib_types.h"
#include "lib_string.h"
#include "lib_printf.h"
#include "boardparms.h"
#elif defined(TARG_OS_RTEMS)
#include <alloc.h>
#include <xapi.h>
#include "types.h"
#include "bspcfg.h"
#define ulong unsigned long
#else
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include "boardparms.h"
#endif

#ifndef _WIN32_WCE
#include "bcmtypes.h"
#endif

#include "BcmOs.h"
#include "board.h"
#include "bcm_map.h"
#if !defined(TARG_OS_RTEMS)
#include "bcm_intr.h"
#endif

#include "bcmadsl.h"
#include "softdsl/AdslCoreDefs.h"
#include "BcmAdslCore.h"
#include "AdslCore.h"
#include "softdsl/SoftDsl.h"
#include "BcmAdslDiag.h"

#ifdef AEI_VDSL_TOGGLE_DATAPUMP
#ifdef SUPPORT_DSL_BONDING
#define MAX_DSL_FAIL_TRAIN_COUNT 10
int countDSLFailed[2] = {0,0};
int DSLLinkUp[2] = {0,0};
int toggle_reset = 0;
int datapumpAutodetect = 0;
extern void kerSysSendtoMonitorTask(int msgType, char *msgData, int msgDataLen);
#define MSG_NETLINK_BRCM_LINK_CHANGE_DATAPUMP 0x8000
extern unsigned char dslDatapump;
extern unsigned char nvram_dslDatapump;

struct timer_list trainingTimer[2];
#define MAX_TRAINING_WAIT_TIME 60
void switchDatapump(unsigned long data);
#endif
#endif

#if !defined(__KERNEL__)
#define KERNEL_VERSION(a,b,c) 0x7fffffff
#endif

#if !defined(TARG_OS_RTEMS)
#ifdef DSL_KTHREAD
void BcmXdslStatusPolling(void);
#else
LOCAL void      *g_TimerHandle;
LOCAL void BcmAdsl_Timer(void * arg);
#endif
LOCAL void BcmAdsl_Status(unsigned char lineId);
#else /* defined(TARG_OS_RTEMS) */
#define calloc(L,X)     xmalloc(L)
#define free(P)         xfree(P)

LOCAL OS_SEMID  g_StatusSemId;
LOCAL OS_TASKID g_StatusTid;

LOCAL void StatusTask(void);
extern void AdslLinkReset(void);
#endif


LOCAL void IdleNotifyCallback (unsigned char lineId, ADSL_LINK_STATE dslLinkState, ADSL_LINK_STATE dslPrevLinkState, UINT32 ulParm)
{
}

#define ADSL_RJ11_INNER_PAIR	0
#define ADSL_RJ11_OUTER_PAIR	1

LOCAL void SetRj11Pair( UINT16 usPairToEnable, UINT16 usPairToDisable );

LOCAL ADSL_FN_NOTIFY_CB g_pFnNotifyCallback = IdleNotifyCallback;
LOCAL UINT32 g_ulNotifyCallbackParm;
LOCAL ADSL_LINK_STATE g_LinkState[MAX_DSL_LINE];
LOCAL unsigned short g_GpioInnerPair = 0xffff;
LOCAL unsigned short g_GpioOuterPair = 0xffff;
LOCAL unsigned short g_BoardType;
LOCAL int			 g_RJ11Pair = ADSL_RJ11_INNER_PAIR;
#ifndef DYING_GASP_API
#if defined(__KERNEL__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
LOCAL irqreturn_t BcmDyingGaspIsr(int irq, void * dev_id);
#else
LOCAL irqreturn_t BcmDyingGaspIsr(int irq, void * dev_id, struct pt_regs *ptregs);
#endif
#else
LOCAL unsigned int BcmDyingGaspIsr( void );
#endif
#endif
int g_nAdslExit = 0;
int g_nAdslInitialized = 0;
UINT32 g_maxRateUS = 0;
UINT32 g_maxRateDS = 0;
//LGD_FOR_TR098
#ifdef SUPPORT_DSL_BONDING
unsigned long g_ShowtimeStartTicks[MAX_DSL_LINE] = {0, 0};
#else
unsigned long g_ShowtimeStartTicks[MAX_DSL_LINE] = {0};
#endif

extern void BcmAdslCoreDebugTimer(void);

#ifdef _WIN32_WCE

BCMADSL_STATUS BcmAdsl_Initialize(ADSL_FN_NOTIFY_CB pFnNotifyCb, UINT32 ulParm, adslCfgProfile *pAdslCfg)
{
    DEBUGMSG (DBG_MSG, (TEXT("BcmAdsl_Initialize=0x%08X, &g_nAdslExit=0x%08X\n"), (int)BcmAdsl_Initialize, (int) &g_nAdslExit));

    BcmOsInitialize ();

    g_pFnNotifyCallback = (pFnNotifyCb != NULL) ? pFnNotifyCb : IdleNotifyCallback;
    g_ulNotifyCallbackParm = ulParm;

    //BcmAdslCoreSetSDRAMBaseAddr((void *) (((ulong) kerSysGetSdramSize() - 0x40000) | 0xA0000000));
    BcmAdslCoreSetSDRAMBaseAddr((void *) ((0x800000 - 0x40000) | 0xA0000000));
    BcmAdslCoreConfigure(pAdslCfg);

    BcmAdslCoreInit();
    g_LinkState = BCM_ADSL_LINK_DOWN;
    g_nAdslExit = 0;
    g_nAdslInitialized = 1;
    g_TimerHandle = bcmOsTimerCreate(BcmAdsl_Timer, NULL);
    if (NULL != g_TimerHandle)
        bcmOsTimerStart(g_TimerHandle, 1000);
    
    return BCMADSL_STATUS_SUCCESS;
}

#elif !defined(TARG_OS_RTEMS)
//**************************************************************************
// Function Name: BcmAdsl_Initialize
// Description  : Initializes ADSL PHY.
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_Initialize(unsigned char lineId, ADSL_FN_NOTIFY_CB pFnNotifyCb, UINT32 ulParm, adslCfgProfile *pAdslCfg)
{
	int	i;
#if defined(AEI_VDSL_TOGGLE_DATAPUMP) && defined(SUPPORT_DSL_BONDING)
	//printk("=============init_timer trainingTimer================\n");
	init_timer(&trainingTimer[0]);
	trainingTimer[0].function = switchDatapump;
	init_timer(&trainingTimer[1]);
	trainingTimer[1].function = switchDatapump;

#endif	
	printk("BcmAdsl_Initialize=0x%08X, g_pFnNotifyCallback=0x%08X\n", (int)BcmAdsl_Initialize, (int) &g_pFnNotifyCallback);

	if (g_nAdslInitialized != 0) {
		BcmAdslCoreConfigure(lineId, pAdslCfg);
		return BCMADSL_STATUS_SUCCESS;
	}

	BcmOsInitialize ();
#ifndef DYING_GASP_API
	{
	unsigned long	ulIntr;
#if defined(CONFIG_BCM963x8)
#if defined(__KERNEL__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	if( BpGetDyingGaspExtIntr( &ulIntr ) == BP_SUCCESS ) 
#endif
	{
		BcmHalMapInterrupt((void *)BcmDyingGaspIsr, 0, INTERRUPT_ID_DG);
		BcmHalInterruptEnable( INTERRUPT_ID_DG );
	}
#endif
	}
#endif

	if( BpGetRj11InnerOuterPairGpios(&g_GpioInnerPair, &g_GpioOuterPair) != BP_SUCCESS ) {
		g_GpioInnerPair = 0xffff;
		g_GpioOuterPair = 0xffff;
	}
#ifndef BP_GET_EXT_AFE_DEFINED
	else {
		g_GpioInnerPair = GPIO_NUM_TO_MASK(g_GpioInnerPair);
		g_GpioOuterPair = GPIO_NUM_TO_MASK(g_GpioOuterPair);
	}
#endif
	
	g_BoardType = 0;
	g_pFnNotifyCallback = (pFnNotifyCb != NULL) ? pFnNotifyCb : IdleNotifyCallback;
	g_ulNotifyCallbackParm = ulParm;
	
	BcmAdslCoreSetSDRAMBaseAddr((void *) (((unsigned long) kerSysGetSdramSize() - 0x40000) | 0xA0000000));
	
	for(i = 0; i < MAX_DSL_LINE; i++)
		BcmAdslCoreConfigure((unsigned char)i, pAdslCfg);
	BcmAdslCoreInit();
	
	for(i = 0; i < MAX_DSL_LINE; i++)
		g_LinkState[i] = BCM_ADSL_LINK_DOWN;
	g_nAdslExit = 0;
	g_nAdslInitialized = 1;
#ifndef DSL_KTHREAD
	g_TimerHandle = bcmOsTimerCreate(BcmAdsl_Timer, NULL);
	if (NULL != g_TimerHandle)
		bcmOsTimerStart(g_TimerHandle, 1000);
#endif

	return BCMADSL_STATUS_SUCCESS;
}
#else /* defined(TARG_OS_RTEMS) */
BCMADSL_STATUS BcmAdsl_Initialize(ADSL_FN_NOTIFY_CB pFnNotifyCb, UINT32 ulParm, adslCfgProfile *pAdslCfg)
{
    typedef void (*FN_HDLR) (unsigned long);
    BcmOsInitialize ();

#if 0
/* The interrupt handling of the Dying gasp is controlled external */
    BcmHalMapInterrupt((FN_HDLR)BcmDyingGaspIsr, 0, INTERRUPT_ID_DYING_GASP);
    BcmHalInterruptEnable( INTERRUPT_ID_DYING_GASP );
#if defined(BOARD_bcm96348) || defined(BOARD_bcm96338)
    BcmHalMapInterrupt((FN_HDLR)BcmDyingGaspIsr, 0, INTERRUPT_ID_DG);
    BcmHalInterruptEnable( INTERRUPT_ID_DG );
#endif
#endif

    g_pFnNotifyCallback = (pFnNotifyCb != NULL) ? pFnNotifyCb : IdleNotifyCallback;
    g_BoardType = 0;
    g_ulNotifyCallbackParm = ulParm;
    bcmOsSemCreate(NULL, &g_StatusSemId);

    /* kerSysGetSdramSize subtracts the size reserved for the ADSL MIPS */
    BcmAdslCoreSetSDRAMBaseAddr((void *)
        ((unsigned long) kerSysGetSdramSize() | 0xA0000000));
    BcmAdslCoreConfigure(pAdslCfg);

    BcmAdslCoreInit();
    g_LinkState = BCM_ADSL_LINK_DOWN;
    g_nAdslExit = 0;
	g_nAdslInitialized = 1;
    bcmOsTaskCreate("ADSL", 20*1024, 50, StatusTask, 0, &g_StatusTid);
    
    return BCMADSL_STATUS_SUCCESS;
}
#endif

//**************************************************************************
// Function Name: BcmAdsl_Check
// Description  : Checks presense of ADSL phy which is always present on
//                the BCM63xx.
// Returns      : STS_SUCCESS if ADSL board found
//**************************************************************************
BCMADSL_STATUS BcmAdsl_Check(void)
{
    return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_Uninitialize
// Description  : Uninitializes ADSL PHY.
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_Uninitialize(unsigned char lineId)
{
	int	i;
	
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	g_nAdslExit = 1;
	for(i = 0; i < MAX_DSL_LINE; i++)
		BcmAdsl_Notify((unsigned char)i);
	BcmAdslCoreUninit();
	BcmOsUninitialize();
	g_nAdslInitialized = 0;
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_ConnectionStart
// Description  : Start ADSL connections.
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_ConnectionStart(unsigned char lineId)
{
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreConnectionStart(lineId);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_ConnectionStop
// Description  : Stop ADSL connections
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_ConnectionStop(unsigned char lineId)
{
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreConnectionStop(lineId);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_GetPhyAddresses
// Description  : Return addresses of Utopia ports for ADSL PHY
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_GetPhyAddresses(PADSL_CHANNEL_ADDR pChannelAddr)
{
    pChannelAddr->usFastChannelAddr = 1;
    pChannelAddr->usInterleavedChannelAddr = 2;

    return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SetPhyAddresses
// Description  : Configure addresses of Utopia ports
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetPhyAddresses(PADSL_CHANNEL_ADDR pChannelAddr)
{
    return BCMADSL_STATUS_ERROR;
}

//**************************************************************************
// Function Name: BcmAdsl_GetConnectionInfo
// Description  : Return ADSL connection info
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_GetConnectionInfo(unsigned char lineId, PADSL_CONNECTION_INFO pConnectionInfo)
{
    BcmAdslCoreGetConnectionInfo(lineId, pConnectionInfo);

    return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_DiagCommand
// Description  : Process ADSL diagnostic command
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagCommand(unsigned char lineId, PADSL_DIAG pAdslDiag)
{
    BcmAdslCoreDiagCmd(lineId, pAdslDiag);
    return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SetObjectValue
// Description  : Set ADS MIB object
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
int BcmAdsl_SetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	return BcmAdslCoreSetObjectValue (lineId, objId, objIdLen, dataBuf, dataBufLen);
}
//**************************************************************************
// Function Name: BcmAdsl_GetObjectValue
// Description  : Get ADS MIB object
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
int BcmAdsl_GetObjectValue (unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen)
{
	return BcmAdslCoreGetObjectValue (lineId, objId, objIdLen, dataBuf, dataBufLen);
}

//**************************************************************************
// Function Name: BcmAdsl_StartBERT
// Description  : Start BERT test
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_StartBERT(unsigned char lineId, unsigned long totalBits)
{
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreStartBERT(lineId, totalBits);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_StopBERT
// Description  : Stops BERT test
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_StopBERT(unsigned char lineId)
{
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreStopBERT(lineId);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_BertStartEx
// Description  : Start BERT test
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_BertStartEx(unsigned char lineId, unsigned long bertSec)
{
	if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreBertStartEx(lineId, bertSec);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_BertStopEx
// Description  : Stops BERT test
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_BertStopEx(unsigned char lineId)
{
    if (0 == g_nAdslInitialized)
		return BCMADSL_STATUS_ERROR;
	BcmAdslCoreBertStopEx(lineId);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SendDyingGasp
// Description  : Sends DyingGasp EOC message
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SendDyingGasp(int powerCtl)
{
	BcmAdslCoreSendDyingGasp(powerCtl);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_Configure
// Description  : Changes ADSL current configuration
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_Configure(unsigned char lineId, adslCfgProfile *pAdslCfg)
{
	BcmAdslCoreConfigure(lineId, pAdslCfg);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_GetVersion
// Description  : Changes ADSL version information
// Returns      : STS_SUCCESS 
//**************************************************************************
BCMADSL_STATUS BcmAdsl_GetVersion(adslVersionInfo *pAdslVer)
{
	BcmAdslCoreGetVersion(pAdslVer);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_Get/Set OEM Parameter
// Description  : Gets or sets ADSL OEM parameter
// Returns      : # of bytes used
//**************************************************************************
int BcmAdsl_GetOemParameter (int paramId, void *buf, int len)
{
	return BcmAdslCoreGetOemParameter (paramId, buf, len);
}

int BcmAdsl_SetOemParameter (int paramId, void *buf, int len)
{
	return BcmAdslCoreSetOemParameter (paramId, buf, len);
}

int BcmAdsl_SetXmtGain(unsigned char lineId, int gain)
{
	return BcmAdslCoreSetXmtGain(lineId, gain);
}

BCMADSL_STATUS BcmAdsl_SetSDRAMBaseAddr(void *pAddr)
{
	BcmAdslCoreSetSDRAMBaseAddr(pAddr);
	return BCMADSL_STATUS_SUCCESS;
}

BCMADSL_STATUS BcmAdsl_ResetStatCounters(unsigned char lineId)
{
	BcmAdslCoreResetStatCounters(lineId);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SetGfc2VcMapping
// Description  : Enables/disables GFC to VC mapping for ATM transmitter
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetGfc2VcMapping(int bOn)
{
	return BcmAdslCoreSetGfc2VcMapping(bOn) ? BCMADSL_STATUS_SUCCESS : BCMADSL_STATUS_ERROR;
}

//**************************************************************************
// Function Name: BcmAdsl_SetVcEntry
// Description  : Maps a port/vpi/vci to a GFC index.
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetVcEntry (int gfc, int port, int vpi, int vci)
{
	return BcmAdslCoreSetVcEntry (gfc, port, vpi, vci, 0) ?
		BCMADSL_STATUS_SUCCESS : BCMADSL_STATUS_ERROR;
}

//**************************************************************************
// Function Name: BcmAdsl_SetVcEntryEx
// Description  : Maps a port/vpi/vci to a GFC index.
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetVcEntryEx (int gfc, int port, int vpi, int vci, int pti_clp)
{
	return BcmAdslCoreSetVcEntry (gfc, port, vpi, vci, pti_clp) ?
		BCMADSL_STATUS_SUCCESS : BCMADSL_STATUS_ERROR;
}

//**************************************************************************
// Function Name: BcmAdsl_SetAtmLoopbackMode
// Description  : Sets ADSL to loopback ATM cells
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetAtmLoopbackMode()
{
	return BcmAdslCoreSetAtmLoopbackMode() ? 
		BCMADSL_STATUS_SUCCESS : BCMADSL_STATUS_ERROR;
}

//**************************************************************************
// Function Name: BcmAdsl_SetTestMode
// Description  : Sets ADSL special test mode
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetTestMode(unsigned char lineId, ADSL_TEST_MODE testMode)
{
	BcmAdslCoreSetTestMode(lineId, testMode);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SelectTones
// Description  : Test function to set tones used by the ADSL modem
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SelectTones(
	unsigned char lineId,
	int		xmtStartTone,
	int		xmtNumTones,
	int		rcvStartTone,
	int		rcvNumTones,
	char	*xmtToneMap,
	char	*rcvToneMap)
{
	BcmAdslCoreSelectTones(lineId, xmtStartTone,xmtNumTones, rcvStartTone,rcvNumTones, xmtToneMap,rcvToneMap);
	return BCMADSL_STATUS_SUCCESS;
}

//**************************************************************************
// Function Name: BcmAdsl_SetDiagMode
// Description  : Test function to set tones used by the ADSL modem
// Returns      : STS_SUCCESS if successful or error status.
//**************************************************************************
BCMADSL_STATUS BcmAdsl_SetDiagMode(unsigned char lineId, int diagMode)
{
	BcmAdslCoreSetAdslDiagMode(lineId, diagMode);
	return BCMADSL_STATUS_SUCCESS;
}

/***************************************************************************
 * Function Name: BcmAdsl_GetConstellationPoints
 * Description  : Gets constellation point for selected tone
 * Returns      : number of points rettrieved 
 ***************************************************************************/
int BcmAdsl_GetConstellationPoints (int toneId, ADSL_CONSTELLATION_POINT *pointBuf, int numPoints)
{
	return BcmAdslCoreGetConstellationPoints (toneId, pointBuf, numPoints);
}



BCMADSL_STATUS BcmAdsl_G997SendData(unsigned char lineId, void *buf, int len)
{
    return BcmAdslCoreG997SendData(lineId, buf, len) ? 
        BCMADSL_STATUS_SUCCESS : BCMADSL_STATUS_ERROR;
}

void *BcmAdsl_G997FrameGet(unsigned char lineId, int *pLen)
{
    return BcmAdslCoreG997FrameGet(lineId, pLen);
}

void *BcmAdsl_G997FrameGetNext(unsigned char lineId, int *pLen)
{
    return BcmAdslCoreG997FrameGetNext(lineId, pLen);
}

void  BcmAdsl_G997FrameFinished(unsigned char lineId)
{
    BcmAdslCoreG997FrameFinished(lineId);
}

//**************************************************************************
// 
//		ATM EOP workaround functions
// 
//**************************************************************************

void BcmAdsl_AtmSetPortId(int path, int portId)
{
	BcmAdslCoreAtmSetPortId(path,portId);
}

void BcmAdsl_AtmClearVcTable(void)
{
	BcmAdslCoreAtmClearVcTable();
}

void BcmAdsl_AtmAddVc(int vpi, int vci)
{
	BcmAdslCoreAtmAddVc(vpi, vci);
}

void BcmAdsl_AtmDeleteVc(vpi, vci)
{
	BcmAdslCoreAtmDeleteVc(vpi, vci);
}

void BcmAdsl_AtmSetMaxSdu(unsigned short maxsdu)
{
	BcmAdslCoreAtmSetMaxSdu(maxsdu);
}

//**************************************************************************
// Function Name: BcmAdsl_Status
// Description  : Runs in a separate thread of execution. Wakes up every
//                second to check status of DSL PHY
// Returns      : None.
//**************************************************************************


void BcmAdsl_G997GetAndSendFrameData(unsigned char lineId)
{
	int len=0;
	char* frameReadPtr;
	char* frameBuffer;
	int adv;
	int bufLen;
	
	if(NULL == (frameReadPtr=(char*)BcmAdsl_G997FrameGet(lineId, &bufLen)))
		return;
	
	while(frameReadPtr!=NULL) {
		len=len+bufLen;
		frameReadPtr=(char*)BcmAdsl_G997FrameGetNext(lineId, &bufLen);
	}
	
	BcmAdslCoreDiagWriteStatusString(lineId, "Line %d: G997 got Frame totalLen=%d", lineId, len);
	frameBuffer=(char*)calloc(len,1);
	
	if(frameBuffer==NULL) {
		BcmAdslCoreDiagWriteStatusString(lineId, "Line %d: G997 Memory Allocation failed", lineId);
		return;
	}
	
	adv=0;
	frameReadPtr=(char*)BcmAdsl_G997FrameGet(lineId, &bufLen);
	while(frameReadPtr!=NULL) {
		memcpy((frameBuffer+adv),frameReadPtr,bufLen);
		adv+=bufLen;
		frameReadPtr=(char*)BcmAdsl_G997FrameGetNext(lineId, &bufLen);
	}
	
	BcmAdsl_G997FrameFinished(lineId);
	
	if( BCMADSL_STATUS_SUCCESS != BcmAdsl_G997SendData(lineId, frameBuffer,len)) 
		free(frameBuffer);
	
	return;
}

int ClearEOCLoopBackEnabled=0;
void BcmAdsl_Status(unsigned char lineId)
{
	ADSL_CONNECTION_INFO ConnectionInfo;
	ADSL_LINK_STATE      adslEvent;
	BcmCoreDpcSyncEnter();
	BcmAdslCoreGetConnectionInfo(lineId, &ConnectionInfo);

	if (ConnectionInfo.LinkState != g_LinkState[lineId]) {
#if defined(AEI_VDSL_TOGGLE_DATAPUMP) && defined(SUPPORT_DSL_BONDING)
        if(datapumpAutodetect)
        {
		    if (ConnectionInfo.LinkState==BCM_ADSL_TRAINING_G994)
		    {
			    //printk("=============mod_timer trainingTimer================\n");
			    mod_timer(&trainingTimer[lineId%2],jiffies +  HZ * MAX_TRAINING_WAIT_TIME);
		    }
		    else {
			    //printk("=============del_timer trainingTimer================\n");
			    del_timer(&trainingTimer[lineId%2]);
		    }
        }
#endif

		switch (ConnectionInfo.LinkState) {
			case BCM_ADSL_LINK_UP:
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
#ifdef SUPPORT_DSL_BONDING
				DSLLinkUp[lineId] = 1;
				countDSLFailed[lineId] = 0;
				if (nvram_dslDatapump != dslDatapump) {
					printk("@@@@@@@@@@@@@@ nvram datapump value changing from %d to %d @@@@@@@@@@@@@@@@@@@@\n",nvram_dslDatapump,dslDatapump);
					/* Don't really need MSG_NETLINK_BRCM_LINK_CHANGE_DATAPUM anymore after having driver directly do 
					   the datapump switching. Can use this message to tell ssk what datapump was successfully chosen.
					   Since just need to pass a number, can use the length field and have ssk read that.
					   Leave the message out but modify ssk to receive datapump id anyway in case needs this message
					*/
					//kerSysSendtoMonitorTask (MSG_NETLINK_BRCM_LINK_CHANGE_DATAPUMP,NULL,dslDatapump);
					nvram_dslDatapump = dslDatapump;
					restoreDatapump(dslDatapump);
				}
#endif
#endif
				//LGD_FOR_TR098
				bcmOsGetTime(&g_ShowtimeStartTicks[lineId]);
				
				if( XdslCoreIsXdsl2Mod(lineId) ) {
					BCMOS_EVENT_LOG((KERN_CRIT \
						TEXT("Line %d: %s link up, Bearer 0, us=%lu, ds=%lu\n"), \
						lineId, \
						XdslCoreIsVdsl2Mod(lineId) ? "VDSL2": "ADSL", \
						ConnectionInfo.ulInterleavedUpStreamRate / 1000, \
						ConnectionInfo.ulInterleavedDnStreamRate / 1000));
					if( XdslCoreIs2lpActive(lineId, US_DIRECTION) || XdslCoreIs2lpActive(lineId, DS_DIRECTION))
						BCMOS_EVENT_LOG((KERN_CRIT \
							TEXT("Line %d: %s link up, Bearer 1, us=%lu, ds=%lu\n"), \
							lineId, \
							XdslCoreIsVdsl2Mod(lineId) ? "VDSL2": "ADSL", \
							ConnectionInfo.ulFastUpStreamRate / 1000, \
							ConnectionInfo.ulFastDnStreamRate / 1000));
				}
				else if( ConnectionInfo.ulInterleavedUpStreamRate ) {
					BCMOS_EVENT_LOG((KERN_CRIT \
						TEXT("Line %d: ADSL link up, interleaved, us=%lu, ds=%lu\n"), \
						lineId, \
						ConnectionInfo.ulInterleavedUpStreamRate / 1000, \
						ConnectionInfo.ulInterleavedDnStreamRate / 1000));
				}
				else {
					BCMOS_EVENT_LOG((KERN_CRIT \
						TEXT("Line %d: ADSL link up, fast, us=%lu, ds=%lu\n"), \
						lineId, \
						ConnectionInfo.ulFastUpStreamRate / 1000, \
						ConnectionInfo.ulFastDnStreamRate / 1000));
				}
				break;

			case BCM_ADSL_TRAINING_G994:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: xDSL G.994 training\n"), lineId));
				break;

			case BCM_ADSL_TRAINING_G992_STARTED:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: ADSL G.992 started\n"), lineId));
				break;

			case BCM_ADSL_TRAINING_G992_CHANNEL_ANALYSIS:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: ADSL G.992 channel analysis\n"), lineId));
				break;

			case BCM_ADSL_TRAINING_G992_EXCHANGE:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: ADSL G.992 message exchange\n"), lineId));
				break;
			case BCM_ADSL_TRAINING_G993_STARTED:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: VDSL G.993 started\n"), lineId));
				break;

			case BCM_ADSL_TRAINING_G993_CHANNEL_ANALYSIS:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: VDSL G.993 channel analysis\n"), lineId));
				break;

			case BCM_ADSL_TRAINING_G993_EXCHANGE:
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: VDSL G.993 message exchange\n"), lineId));
				break;

			case BCM_ADSL_LINK_DOWN:
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
#ifdef SUPPORT_DSL_BONDING
			countDSLFailed[lineId]++;
			DSLLinkUp[lineId]=0;
			/* if continuos failure and no link has come up */
			if (datapumpAutodetect && countDSLFailed[lineId]>MAX_DSL_FAIL_TRAIN_COUNT && DSLLinkUp[lineId] != 1 && DSLLinkUp[(lineId+1)%2] != 1)
			{
				countDSLFailed[lineId] = 0;
				countDSLFailed[(lineId+1)%2] = 0;
				toggle_reset = 1;
			}
#endif
#endif
				BcmAdslCoreBertStopEx(lineId);
#ifdef AEI_VDSL_TOGGLE_DATAPUMP
#ifdef SUPPORT_DSL_BONDING
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: %s link down failure count %d \n"), lineId, XdslCoreIsVdsl2Mod(lineId) ? "VDSL2": "ADSL",countDSLFailed[lineId]));
#endif
#else
				BCMOS_EVENT_LOG((KERN_CRIT TEXT("Line %d: %s link down\n"), lineId, XdslCoreIsVdsl2Mod(lineId) ? "VDSL2": "ADSL"));
#endif
				break;

			default:
				break;
		}
		
		(*g_pFnNotifyCallback)(lineId, ConnectionInfo.LinkState, g_LinkState[lineId], g_ulNotifyCallbackParm);
		g_LinkState[lineId] = ConnectionInfo.LinkState;
	}
	BcmCoreDpcSyncExit();
	/* call extended info to see if the callback needs to be called (TBD) */

	while ((adslEvent = BcmAdslCoreGetEvent(lineId)) != -1) {
		if (ADSL_SWITCH_RJ11_PAIR == adslEvent) {
			g_RJ11Pair ^= 1;
#if !defined(BOARD_bcm96345) || defined(BOARDVAR_bant_a)
			if ( (g_GpioInnerPair != 0xffff) && (g_GpioOuterPair != 0xffff) ) {
				if (ADSL_RJ11_INNER_PAIR == g_RJ11Pair)
					SetRj11Pair( g_GpioInnerPair, g_GpioOuterPair );
				else
					SetRj11Pair( g_GpioOuterPair, g_GpioInnerPair );
			}
#else
			{
			BCMOS_DECLARE_IRQFLAGS(flags);

			BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);
			if (ADSL_RJ11_INNER_PAIR == g_RJ11Pair) {
				// Switch to inner (GPIO is low)
				GPIO->GPIODir |= GPIO_AUTOSENSE_CTRL; // define as output
				GPIO->GPIOio &= ~GPIO_AUTOSENSE_CTRL;
			}
			else {
				// Switch to outer (GPIO is high)
				GPIO->GPIODir |= GPIO_AUTOSENSE_CTRL; // define as output
				GPIO->GPIOio |= GPIO_AUTOSENSE_CTRL;
			}
			BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);
			}
#endif
		}
		else {
			if ((BCM_ADSL_LINK_UP != adslEvent) && (BCM_ADSL_LINK_DOWN != adslEvent)) {
				if(BCM_ADSL_G997_FRAME_RECEIVED == adslEvent && ClearEOCLoopBackEnabled ){
					/* TO DO: Add ClearEOC for bonding */
					BcmAdslCoreDiagWriteStatusString(lineId, "G997: Frame Received event");
					BcmAdsl_G997GetAndSendFrameData(lineId);
				}
				else if(BCM_ADSL_G997_FRAME_SENT == adslEvent && ClearEOCLoopBackEnabled)
					BcmAdslCoreDiagWriteStatusString(lineId, "G997: Frame Sent Event");
				else {
					(*g_pFnNotifyCallback)(lineId, adslEvent, g_LinkState[lineId], g_ulNotifyCallbackParm);
				}
			}
		}
	}
	
}

void BcmAdsl_Notify(unsigned char lineId)
{
#if !defined(TARG_OS_RTEMS)
	if( g_nAdslExit == 0 )
		BcmAdsl_Status(lineId);
#else
	bcmOsSemGive (g_StatusSemId);
#endif
}

void BcmXdslNotifyRateChange(unsigned char lineId)
{
	ADSL_CONNECTION_INFO ConnectionInfo;
	
	BcmAdslCoreGetConnectionInfo(lineId, &ConnectionInfo);

	if((BCM_ADSL_LINK_UP == g_LinkState[lineId]) &&
		(ConnectionInfo.LinkState == g_LinkState[lineId])) {
		if( ConnectionInfo.ulInterleavedUpStreamRate ) {
			BCMOS_EVENT_LOG((KERN_CRIT \
				TEXT("Line %d: Rate Change, us=%lu, ds=%lu\n"), \
				lineId, \
				ConnectionInfo.ulInterleavedUpStreamRate / 1000, \
				ConnectionInfo.ulInterleavedDnStreamRate / 1000));
		}
		else {
			BCMOS_EVENT_LOG((KERN_CRIT \
				TEXT("Line %d: Rate Change, us=%lu, ds=%lu\n"), \
				lineId, \
				ConnectionInfo.ulFastUpStreamRate / 1000, \
				ConnectionInfo.ulFastDnStreamRate / 1000));
		}
		(*g_pFnNotifyCallback)(lineId, ConnectionInfo.LinkState, g_LinkState[lineId], g_ulNotifyCallbackParm);
	}
}

#if !defined(TARG_OS_RTEMS)
#ifdef DSL_KTHREAD
void BcmXdslStatusPolling(void)
{
	if( !g_nAdslInitialized )
		return;

#ifdef PHY_BLOCK_TEST
	BcmAdslCoreDebugTimer();
#else
	if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
		BcmAdslCoreDebugTimer();
#endif

	BcmAdslCoreDiagConnectionCheck();

	BcmAdsl_Status(0);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		BcmAdsl_Status(1);
#endif
}/* BcmXdslStatusPolling() */
#else /* !DSL_KTHREAD */
LOCAL void BcmAdsl_Timer(void * arg)
{
	if( g_nAdslExit == 1 )
		return;

#ifdef PHY_BLOCK_TEST
	BcmAdslCoreDebugTimer();
#else
	if (ADSL_PHY_SUPPORT(kAdslPhyPlayback))
		BcmAdslCoreDebugTimer();
#endif

	BcmAdslCoreDiagConnectionCheck();

	BcmAdsl_Status(0);
#ifdef SUPPORT_DSL_BONDING
	if (ADSL_PHY_SUPPORT(kAdslPhyBonding))
		BcmAdsl_Status(1);
#endif
	bcmOsTimerStart(g_TimerHandle, 1000);
}/* BcmAdsl_Timer() */
#endif /* !DSL_KTHREAD */
#else
LOCAL void StatusTask()
{
	while (TRUE) {
		/* Sleep 1 second */
		bcmOsSemTake(g_StatusSemId, 1000 / BCMOS_MSEC_PER_TICK);
		
		if( g_nAdslExit == 1 )
			break;
		
		BcmAdsl_Status();
	}
}
#endif

//**************************************************************************
// Function Name: SetRj11Pair
// Description  : Sets the RJ11 wires to use either the inner pair or outer
//                pair for ADSL.
// Returns      : None.
//**************************************************************************
LOCAL void SetRj11Pair( UINT16 usPairToEnable, UINT16 usPairToDisable )
{
#ifndef BP_GET_EXT_AFE_DEFINED
	int i;
	BCMOS_DECLARE_IRQFLAGS(flags);

	BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);

	GPIO->GPIODir |= usPairToEnable | usPairToDisable;

	/* Put the "other" pair into a disabled state */
	GPIO->GPIOio &= ~usPairToDisable;

	BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);

	/* Enable the pair two times just to make sure. */
	for( i = 0; i < 2; i++ )
	{
		BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);
		GPIO->GPIOio |= usPairToEnable;
		BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);

		bcmOsDelay(10);

		BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);
		GPIO->GPIOio &= ~usPairToEnable;
		BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);

		bcmOsDelay(10);
	}
#else
	kerSysSetGpio(usPairToDisable, kGpioInactive);
	kerSysSetGpio(usPairToEnable, kGpioActive);
#endif
} /* SetRj11Pair */

//**************************************************************************
// Function Name: BcmAdsl_ConfigureRj11Pair
// Description  : Configures RJ11 pair setting according to ADSL profile
//                pair for ADSL.
// Returns      : None.
//**************************************************************************
#if !defined(BOARD_bcm96345) || defined(BOARDVAR_bant_a)
void BcmAdsl_ConfigureRj11Pair(long pair)
{
	if ( (g_GpioInnerPair == 0xffff) || (g_GpioOuterPair == 0xffff) )
		return;

	if (kAdslCfgLineInnerPair == (pair & kAdslCfgLinePairMask)) {
		g_RJ11Pair = ADSL_RJ11_INNER_PAIR;
		SetRj11Pair( g_GpioInnerPair, g_GpioOuterPair );
	}
	else {
		g_RJ11Pair = ADSL_RJ11_OUTER_PAIR;
		SetRj11Pair( g_GpioOuterPair, g_GpioInnerPair );
	}
} /* BcmAdsl_ConfigureRj11Pair */
#else
void BcmAdsl_ConfigureRj11Pair(long pair)
{
	BCMOS_DECLARE_IRQFLAGS(flags);

	BCMOS_SPIN_LOCK_IRQ(&bcm_gpio_spinlock, flags);

	if (kAdslCfgLineInnerPair == (pair & kAdslCfgLinePairMask)) {
		g_RJ11Pair = ADSL_RJ11_INNER_PAIR;
		// Switch to inner (GPIO is low)
		GPIO->GPIODir |= GPIO_AUTOSENSE_CTRL; // define as output
		GPIO->GPIOio &= ~GPIO_AUTOSENSE_CTRL;
	}
	else {
		g_RJ11Pair = ADSL_RJ11_OUTER_PAIR;
		// Swith to outer (GPIO is high)
		GPIO->GPIODir |= GPIO_AUTOSENSE_CTRL; // define as output
		GPIO->GPIOio |= GPIO_AUTOSENSE_CTRL;
	}

	BCMOS_SPIN_UNLOCK_IRQ(&bcm_gpio_spinlock, flags);
} /* BcmAdsl_ConfigureRj11Pair */
#endif

#ifndef DYING_GASP_API

//**************************************************************************
// Function Name: BcmDyingGaspIsr
// Description  : Handles power off to the board.
// Returns      : 1
//**************************************************************************
#if defined(__KERNEL__) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
LOCAL irqreturn_t BcmDyingGaspIsr(int irq, void * dev_id);
#else
LOCAL irqreturn_t BcmDyingGaspIsr(int irq, void * dev_id, struct pt_regs *ptregs)
#endif
{
	if (BcmAdslCoreCheckPowerLoss())
		BcmAdslCoreSendDyingGasp(1);
    return( IRQ_HANDLED );
} /* BcmDyingGaspIsr */
#else
LOCAL unsigned int BcmDyingGaspIsr( void )
{
	if (BcmAdslCoreCheckPowerLoss())
		BcmAdslCoreSendDyingGasp(1);
    return( 1 );
} /* BcmDyingGaspIsr */
#endif

#else /* DYING_GASP_API */

//**************************************************************************
// Function Name: BcmAdsl_DyingGaspHandler
// Description  : Handles power off to the board.
// Returns      : none
//**************************************************************************
void BcmAdsl_DyingGaspHandler(void *context)
{
	BcmAdslCoreSendDyingGasp(1);	
	
}

#endif

extern BCMADSL_STATUS BcmAdslDiagStatSaveInit(void *pAddr, int len);
extern BCMADSL_STATUS BcmAdslDiagStatSaveContinous(void);
extern BCMADSL_STATUS BcmAdslDiagStatSaveStart(void);
extern BCMADSL_STATUS BcmAdslDiagStatSaveStop(void);
extern BCMADSL_STATUS BcmAdslDiagStatSaveUnInit(void);
extern BCMADSL_STATUS BcmAdslDiagStatSaveGet(PADSL_SAVEDSTATUS_INFO pSavedStatInfo);


//**************************************************************************
// Function Name: BcmAdsl_DiagStatSaveInit
// Description : Initialize the pAddr for saving statuses locally in the target.
// Returns      : BCMADSL_STATUS_SUCCESS or BCMADSL_STATUS_ERROR
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveInit(void *pAddr, int len)
{
	return BcmAdslDiagStatSaveInit(pAddr, len);
}
//**************************************************************************
// Function Name: BcmAdslDiagStatSaveContinous
// Description : Enable saving statuses continously.
// Returns      : BCMADSL_STATUS_SUCCESS or BCMADSL_STATUS_ERROR
// NOTE         : This has to be called after BcmAdslDiagStatSaveInit() and before BcmAdslDiagStatSaveStart()
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveContinous(void)
{
	return BcmAdslDiagStatSaveContinous();
}
//**************************************************************************
// Function Name: BcmAdsl_DiagStatSaveStart
// Description : Start saving statuses.
// Returns      : BCMADSL_STATUS_SUCCESS or BCMADSL_STATUS_ERROR
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveStart(void)
{
	return BcmAdslDiagStatSaveStart();
}
//**************************************************************************
// Function Name: BcmAdsl_DiagStatSaveStop
// Description : Stop saving statuses.
// Returns      : BCMADSL_STATUS_SUCCESS or BCMADSL_STATUS_ERROR
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveStop(void)
{
	return BcmAdslDiagStatSaveStop();
}
//**************************************************************************
// Function Name: BcmAdsl_DiagStatSaveUnInit
// Description : Unitialize saving statuses.
// Returns		: BCMADSL_STATUS_SUCCESS or BCMADSL_STATUS_ERROR
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveUnInit(void)
{
	return BcmAdslDiagStatSaveUnInit();
}
//**************************************************************************
// Function Name: BcmAdsl_DiagStatSaveGet
// Description : Get the current saved statuses.
// Returns      : BCMADSL_STATUS_SUCCESS with valid pSavedStatInfo or
//                    BCMADSL_STATUS_ERROR
//**************************************************************************
BCMADSL_STATUS BcmAdsl_DiagStatSaveGet(PADSL_SAVEDSTATUS_INFO pSavedStatInfo)
{
	return BcmAdslDiagStatSaveGet(pSavedStatInfo);
}


//**************************************************************************
// Function Name: BcmAdsl_SetMaxRates
// Description  : Sets the maximum upstream and downstream rates (ADSL2 and ADSL2+)
// Returns      : none
//**************************************************************************
void BcmAdsl_SetMaxRates(UINT32 maxRateUS, UINT32 maxRateDS, adslCfgProfile *pAdslCfg)
{
   g_maxRateUS = maxRateUS;
   g_maxRateDS = maxRateDS;
   if (g_nAdslInitialized != 0)
   {
      /* If the PHY has already been initialized, reconfigure and reset it */
      BcmAdslCoreConfigure(0, pAdslCfg);
      BcmAdslCoreSetGfc2VcMapping(0);
      BcmAdslCoreConnectionStart(0);
   }
}


#if defined(AEI_VDSL_TOGGLE_DATAPUMP) && defined(SUPPORT_DSL_BONDING)
void switchDatapump(unsigned long data)
{
    //printk("==============firing trainingTimer================\n");	    
    toggle_reset = 1;
    return;
}
#endif

