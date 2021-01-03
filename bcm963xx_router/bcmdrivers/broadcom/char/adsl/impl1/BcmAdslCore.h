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
/***************************************************************************
****************************************************************************
** File Name  : BcmAdslCore.h
**
** Description: This file contains the definitions, structures and function
**              prototypes for Bcm Core ADSL PHY interface
**
***************************************************************************/
#if !defined(_BcmAdslCore_H)
#define _BcmAdslCore_H

#include "AdslMibDef.h"

/* 
**	Internal ADSL driver events handled by the ADSL driver
**  defined not to intersect with ADSL_LINK_STATE events (in vcmadsl.h)
*/

#define	ADSL_SWITCH_RJ11_PAIR	0x80000001

/***************************************************************************
** Function Prototypes
***************************************************************************/

void BcmAdslCoreInit(void);
void BcmAdslCoreUninit(void);
Bool BcmAdslCoreCheckBoard(void);
void BcmAdslCoreConnectionStart(unsigned char lineId);
void BcmAdslCoreConnectionStop(unsigned char lineId);
void BcmAdslCoreConnectionReset(unsigned char lineId);

void BcmAdslCoreGetConnectionInfo(unsigned char lineId, PADSL_CONNECTION_INFO pConnectionInfo);
void BcmAdslCoreDiagCmd(unsigned char lineId, PADSL_DIAG pAdslDiag);
void BcmAdslCoreDiagCmdCommon(unsigned char lineId, int diagCmd, int len, void *pCmdData);
void BcmAdslCoreDiagCmdNotify(void);
void BcmAdslCoreDiagSetSyncTime(unsigned long syncTime);
void BcmAdslCoreDiagConnectionCheck(void);
unsigned long BcmAdslCoreDiagGetSyncTime(void);
char * BcmAdslCoreDiagScrambleString(char *s);
int  BcmAdslCoreSetObjectValue(unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen);
int  BcmAdslCoreGetObjectValue(unsigned char lineId, char *objId, int objIdLen, char *dataBuf, long *dataBufLen);
void BcmAdslCoreStartBERT(unsigned char lineId, unsigned long totalBits);
void BcmAdslCoreStopBERT(unsigned char lineId);
void BcmAdslCoreBertStartEx(unsigned char lineId, unsigned long bertSec);
void BcmAdslCoreBertStopEx(unsigned char lineId);
#ifndef DYING_GASP_API
Bool BcmAdslCoreCheckPowerLoss(void);
#endif
void BcmAdslCoreSendDyingGasp(int powerCtl);
void BcmAdslCoreConfigure(unsigned char lineId, adslCfgProfile *pAdslCfg);
void BcmAdslCoreGetVersion(adslVersionInfo *pAdslVer);
void BcmAdslCoreSetTestMode(unsigned char lineId, int testMode);
void BcmAdslCoreSetTestExecutionDelay(unsigned char lineId, int testMode, unsigned long value);
void BcmAdslCoreSelectTones(
	unsigned char lineId,
	int		xmtStartTone, 
	int		xmtNumTones, 
	int		rcvStartTone,
	int		rcvNumTones, 
	char	*xmtToneMap,
	char	*rcvToneMap);
void BcmAdslCoreDiagSelectTones(
	unsigned char lineId,
	int		xmtStartTone, 
	int		xmtNumTones, 
	int		rcvStartTone,
	int		rcvNumTones, 
	char	*xmtToneMap,
	char	*rcvToneMap);
void BcmAdslCoreSetAdslDiagMode(unsigned char lineId, int diagMode);
int BcmAdslCoreGetConstellationPoints (int toneId, ADSL_CONSTELLATION_POINT *pointBuf, int numPoints);
int BcmAdslCoreGetOemParameter (int paramId, void *buf, int len);
int BcmAdslCoreSetOemParameter (int paramId, void *buf, int len);
int BcmAdslCoreSetXmtGain(unsigned char lineId, int gain);
int  BcmAdslCoreGetSelfTestMode(void);
void BcmAdslCoreSetSelfTestMode(int stMode);
int  BcmAdslCoreGetSelfTestResults(void);
void BcmAdslCoreAfeTestMsg(void *pMsg);
void BcmAdslCoreDebugCmd(unsigned char lineId, void *pMsg);
void BcmAdslCoreGdbCmd(void *pCmd, int cmdLen);
#if defined(__KERNEL__) && (!defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338))
void BcmAdslCoreResetPhy(int copyImage);
#endif
ADSL_LINK_STATE BcmAdslCoreGetEvent (unsigned char lineId);
Bool BcmAdslCoreSetSDRAMBaseAddr(void *pAddr);
Bool BcmAdslCoreSetVcEntry (int gfc, int port, int vpi, int vci, int pti_clp);
Bool BcmAdslCoreSetGfc2VcMapping(Bool bOn);
Bool BcmAdslCoreSetAtmLoopbackMode(void);
void BcmAdslCoreResetStatCounters(unsigned char lineId);

Bool BcmAdslCoreG997SendData(unsigned char lineId, void *buf, int len);

void *BcmAdslCoreG997FrameGet(unsigned char lineId, int *pLen);
void *BcmAdslCoreG997FrameGetNext(unsigned char lineId, int *pLen);
void BcmAdslCoreG997FrameFinished(unsigned char lineId);

void BcmAdslCoreAtmSetPortId(int path, int portId);
void BcmAdslCoreAtmClearVcTable(void);
void BcmAdslCoreAtmAddVc(int vpi, int vci);
void BcmAdslCoreAtmDeleteVc(int vpi, int vci);
void BcmAdslCoreAtmSetMaxSdu(unsigned short maxsdu);

void BcmAdsl_Notify(unsigned char lineId);
void BcmAdsl_ConfigureRj11Pair(long pair);
void BcmAdslCoreDelay(unsigned long timeMs);
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
void BcmXdslCoreSendAfeInfo(int toPhy);
#endif
void BcmXdslNotifyRateChange(unsigned char lineId);


#define	BcmCoreCommandHandlerSync(cmd)	do {	\
	BcmCoreDpcSyncEnter();						\
	AdslCoreCommandHandler(cmd);				\
	BcmCoreDpcSyncExit();						\
} while (0)

#endif /* _BcmAdslCore_H */

