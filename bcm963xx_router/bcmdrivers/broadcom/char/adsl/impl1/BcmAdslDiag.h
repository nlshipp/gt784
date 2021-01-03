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
 * BcmAdslDiag.h -- Internal definitions for ADSL diagnostic
 *
 * Description:
 *	Internal definitions for ADSL core driver
 *
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: BcmAdslDiag.h,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: BcmAdslDiag.h,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

#define			DIAG_SPLIT_MSG		0x80000000
#define			DIAG_MSG_NUM		0x40000000
#define			DIAG_LINE_MASK		0x1C000000
#define			DIAG_LINE_SHIFT		26

int	 BcmAdslDiagTaskInit(void);
void BcmAdslDiagTaskUninit(void);
int  BcmAdslDiagInit(int diagDataMap);
void BcmAdslCoreDiagDmaInit(void);
void * BcmAdslCoreDiagGetDmaDataAddr(int descNum);
int	 BcmAdslCoreDiagGetDmaDataSize(int descNum);
int	 BcmAdslCoreDiagGetDmaBlockNum(void);
void BcmAdslDiagReset(int diagDataMap);
int BcmAdslCoreDiagWriteStatusData(ulong cmd, char *buf0, int len0, char *buf1, int len1);
void BcmAdslCoreDiagStatusSnooper(dslStatusStruct *status, char *pBuf, int len);
void BcmAdslCoreDiagSaveStatusString(char *fmt, ...);
void BcmAdslCoreDiagWriteStatusString(unsigned char lineId, char *fmt, ...);
int  BcmAdslCoreDiagWrite(void *pBuf, int len);
void BcmAdslCoreDiagWriteLog(long logData, ...);
Bool BcmAdslCoreDiagIntrHandler(void);
void BcmAdslCoreDiagIsrTask(void);
void BcmAdslCoreLogWriteConnectionParam(dslCommandStruct *pDslCmd);
int  BcmAdslDiagGetConstellationPoints (int toneId, void *pointBuf, int numPoints);
int	 BcmAdslDiagDisable(void);
int	 BcmAdslDiagEnable(void);
void BcmAdslDiagDisconnect(void);
Bool BcmAdslDiagIsActive(void);
void BcmAdslCoreDiagStartLog(unsigned long map, unsigned long time);
void BcmAdslCoreWriteOvhMsg(void *gDslVars, char *hdr, dslFrame *pFrame);
void BcmAdslCoreDiagSetBufDesc(void);
void BcmAdslCoreDiagOpenFile(unsigned char lineId, char *fname) ;
void BcmAdslCoreDiagWriteFile(unsigned char lineId, char *fname, void *ptr, ulong len) ;
void BcmAdslCoreDiagDataLogNotify(int set);
void BcmXdslCoreDiagStatSaveDisableOnRetrainSet(void);
void BcmXdslCoreDiagSendPhyInfo(void);
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
void BcmAdslDiagSendHdr(void);
Bool BcmCoreDiagXdslEyeDataAvail(void);
Bool BcmCoreDiagLogActive(ulong map);
void BcmCoreDiagEyeDataFrameSend(void);

#endif


