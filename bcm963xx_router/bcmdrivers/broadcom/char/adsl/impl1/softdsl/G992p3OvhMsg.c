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
/**********************************************************************
 *
 * G992p3OvhMsg.c -- G992p3 overhead channel message processing module
 *
 * Description:
 *  This file contains mian functions of G992p3 overhead channel message 
 *  processing
 *
 *
 * Copyright (c) 1999-2003 BroadCom, Inc. All rights reserved.
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.8 $
 *
 * $Id: G992p3OvhMsg.c,v 1.8 2011/07/07 09:10:53 lzhang Exp $
 *
 * $Log: G992p3OvhMsg.c,v $
 * Revision 1.8  2011/07/07 09:10:53  lzhang
 * ATU-R system vendor ID field reports "ACTI"  which is not unique to this product.
 *
 * Revision 1.7  2011/06/24 01:52:40  dkhoo
 * [All]Per PLM, update DSL driver to 23j and bonding datapump to Av4bC035d
 *
 * Revision 1.11  2004/09/11 03:52:24  ilyas
 * Added support for overhead message segmentation
 *
 * Revision 1.10  2004/07/21 01:39:41  ilyas
 * Reset entire G.997 state on retrain. Timeout in G.997 if no ACK
 *
 * Revision 1.9  2004/07/07 01:27:20  ilyas
 * Fixed OHC message stuck problem on L2 entry/exit
 *
 * Revision 1.8  2004/06/10 00:13:31  ilyas
 * Added L2/L3 and SRA
 *
 * Revision 1.7  2004/05/07 02:58:36  ilyas
 * Commented out debug print
 *
 * Revision 1.6  2004/05/06 20:02:39  ilyas
 * Fixed include path for Linux build
 *
 * Revision 1.5  2004/05/06 03:14:44  ilyas
 * Added power management commands. Fixed multibuffer message handling
 *
 * Revision 1.4  2004/04/27 00:27:16  ilyas
 * Implemented double buffering to ensure G.997 HDLC frame is continuous
 *
 * Revision 1.3  2004/04/12 23:34:52  ilyas
 * Merged the latest ADSL driver chnages for ADSL2+
 *
 * Revision 1.2  2003/10/14 00:56:35  ilyas
 * Added stubs if G992P3 is not defined
 *
 * Revision 1.1  2003/07/18 19:39:18  ilyas
 * Initial G.992.3 overhead channel message implementation (from ADSL driver)
 *
 *
 *****************************************************************************/

#include "SoftDsl.gh"

#include "DslFramer.h"
#include "G992p3OvhMsg.h"
#include "AdslMibDef.h"
#include "BlockUtil.h"

#include "../AdslCore.h"

#undef  G992P3_POLL_OVH_MSG
#define G992P3_POLL_OVH_MSG
#undef  G992P3_OLR_TEST
#undef  G992P3_DBG_PRINT
#undef  G993_DBG_PRINT
#undef  DEBUG_PRINT_RCV_SEG
#define CLIP_LARGE_SNR
#define WORKAROUND_2BYTES_US_ATTNDR_RSP

#define globalVar gG992p3OvhMsgVars

#define     MIN(a,b) (((a)<(b))?(a):(b))

/* G.992.3 overhead message definitions */

#define kG992p3AddrField          0
#define kG992p3PriorityMask         0x03
#define kG992p3PriorityHigh         0
#define kG992p3PriorityNormal       0x01
#define kG992p3PriorityLow          0x02

#define kG992p3CtrlField          1
#define kG992p3MsgNumMask         0x01
#define kG992p3CmdRspMask         0x02
#define kG992p3Cmd              0
#define kG992p3Rsp              0x02

#define kG992p3DataField          2
#define kG992p3CmdCode            kG992p3DataField
#define kG992p3CmdSubCode         (kG992p3CmdCode + 1)
#define kG992p3CmdId            (kG992p3CmdCode + 2)


/* G.992.3 overhead message commands */

#define kG992p3OvhMsgCmdNone        0
#define kG992p3OvhMsgCmdOLR         0x01
#define   kG992p3OvhMsgCmdOLRReq1     0x01
#define   kG992p3OvhMsgCmdOLRReq2     0x02
#define   kG992p3OvhMsgCmdOLRReq3     0x03
#define   kG992p3OvhMsgCmdOLRReq4     0x04
#define   kG992p3OvhMsgCmdOLRReq5     0x05
#define   kG992p3OvhMsgCmdOLRReq6     0x06
#define   kG992p3OvhMsgCmdOLRReq7     0x07

#define   kG992p3OvhMsgCmdOLRRspDefer1    0x81
#define   kG992p3OvhMsgCmdOLRRspRej2    0x82
#define   kG992p3OvhMsgCmdOLRRspRej3    0x83

#define kG992p3OvhMsgCmdEOC         0x41
#define   kG992p3OvhMsgCmdEOCSelfTest   0x01
#define   kG992p3OvhMsgCmdEOCUpdTestParam 0x02
#define   kG992p3OvhMsgCmdEOCStartTxCorCRC  0x03
#define   kG992p3OvhMsgCmdEOCStopTxCorCRC 0x04
#define   kG992p3OvhMsgCmdEOCStartRxCorCRC  0x05
#define   kG992p3OvhMsgCmdEOCStopRxCorCRC 0x06
#define   kG992p3OvhMsgCmdEOCStartRtxTestMode 0x07
#define   kG992p3OvhMsgCmdEOCStopRtxTestMode 0x08
#define   kG992p3OvhMsgCmdEOCAck      0x80

#define kG992p3OvhMsgCmdTime        0x42

#define kG992p3OvhMsgCmdCtrlRead      0x04

#define kG992p3OvhMsgCmdInventory     0x43
#define   kG992p3OvhMsgCmdInvId       0x01
#define   kG992p3OvhMsgCmdInvAuxId      0x02
#define   kG992p3OvhMsgCmdInvSelfTestRes  0x03
#define   kG992p3OvhMsgCmdInvPmdCap     0x04
#define   kG992p3OvhMsgCmdInvPmsTcCap   0x05
#define   kG992p3OvhMsgCmdInvTpsTcCap   0x06

#define kG992p3OvhMsgCmdCntRead       0x05
#define  kG992p3OvhMsgCmdCntReadId      0x01
#define  kG992p3OvhMsgCmdCntReadId1   0x0F      /* kG992p3OvhMsgCmdCntReadId + FIRE counters */

#define kG992p3OvhMsgCmdPower       0x07
#define kG992p3OvhMsgCmdClearEOC      0x08
#define   kG992p3OvhMsgCmdClearEOCMsg   0x01
#define   kG992p3OvhMsgCmdClearEOCAck   0x80
#define kG992p3OvhMsgCmdNonStdFac     0x3F

#define kG992p3OvhMsgCmdPMDRead       0x81
#define   kG992p3OvhMsgCmdPMDSingleRead   0x01
#define   kG992p3OvhMsgCmdPMDMultiRead    0x04
#define   kG992p3OvhMsgCmdPMDMultiNext    0x03
#define   kG992p3OvhMsgCmdPMDBlockRead    0x05
#define   kG992p3OvhMsgCmdPMDVectorBlockRead  0x06
#define   kG992p3OvhMsgCmdPMDReadNACK   0x80
#define   kG992p3OvhMsgCmdPMDSingleRdRsp  0x81
#define   kG992p3OvhMsgCmdPMDMultiRdRsp   0x82
#define   kG992p3OvhMsgCmdPMDBlockReadRsp 0x84
#define   kG992p3OvhMsgCmdPMDVectorBlockReadRsp 0x86
#define     kG992p3OvhMsgCmdPMDChanRspLog 0x01
#define     kG992p3OvhMsgCmdPMDQLineNoise 0x03
#define     kG992p3OvhMsgCmdPMDSnr      0x04
#define     kG992p3OvhMsgCmdPMDPeriod   0x05

#define     kG992p3OvhMsgCmdPMDLnAttn   0x21
#define     kG992p3OvhMsgCmdPMDSigAttn    0x22
#define     kG992p3OvhMsgCmdPMDSnrMgn   0x23
#define     kG992p3OvhMsgCmdPMDAttnDR   0x24
#define     kG992p3OvhMsgCmdPMDNeXmtPower 0x25
#define     kG992p3OvhMsgCmdPMDFeXmtPower 0x26
#define     kG992p3OvhMsgCmdPMDSingleReadINPId         0x41
#define     kG992p3OvhMsgCmdPMDSingleReadINPReinId   0x42
#define     kG992p3OvhMsgCmdPMDSingleReadETRId         0x43
#define     kG992p3OvhMsgCmdPMDSingleReadDelayId      0x44
#define     kDslPLNControlReadPerBinPeakNoise   0x81
#define     kDslPLNControlReadPerBinThresholdCount   0x82
#define     kDslPLNControlReadImpulseDurationStat   0x83
#define     kDslPLNControlReadImpulseInterArrivalTimeStat   0x84


#define     kDslPLNControlStart                      1
#define     kDslPLNControlStop                       2
#define     kDslPLNControlPLNStatus                  0x85

#define     kDslINMControlCommand   0x89
#define     kDslReadINMCounters    2
#define     kDslSetINMParams    3
#define     kDslReadINMParams    4

#define     kG992p3OvhMsgINMControlACK    0x80
#define     kG992p3OvhMsgINMControlNACK    0x81
#define     kG992p3OvhMsgINMINPEQModeNACC    0x81
#define     kG992p3OvhMsgINMINPEQModeACC    0x80

#define kG992p3OvhMsgCmdNonStdFacLow    0xBF

#define kG992p3OvhSegMsgAckHi       0xF0
#define kG992p3OvhSegMsgAckNormal     0xF1
#define kG992p3OvhSegMsgAckLow        0xF2

#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
#define VECTORMGR_ACK_RESPONSE             0x80
#define VECTORMGR_NACK_RESPONSE            0x81

#define kG993p2OvhMsgCmdVectoring          0x0F
#define kG993p2OvhMsgCmdErrorFeedback      0x18
#define kG993p2OvhMshCmdPilotSeqUpdate     0x11

#ifdef G993P5_OVERHEAD_MESSAGING

/* High priority vectoring command */
#define VECTORMGR_HIGH_CMD_ID              0x18
#define VECTORMGR_START_DUMP_CMD_ID        0x01

#define VECTORMGR_START_DUMP_MSG_LENGTH    (4+7+5*8)

/* Normal priority vectoring command */
#define VECTORMGR_LOW_CMD_ID               0x0F
#define VECTORMGR_SET_MODE_CMD_ID          0x04
#define VECTORMGR_SET_MODE_MSG_LENGTH      (4+sizeof(VectorMode))

#else   /* G993P5_OVERHEAD_MESSAGING */

#define VECTORMGR_SET_PILOT_CMD_ID         0x01
#define VECTORMGR_SET_PILOT_MSG_LENGTH     (4+2+128+sizeof(FourBandsDescriptor)+sizeof(VceMacAddress))

#define VECTORMGR_START_DUMP_CMD_ID        0x02
#define VECTORMGR_START_DUMP_MSG_LENGTH    (4+sizeof(StartVectorDump))

#define VECTORMGR_SET_MODE_CMD_ID          0x04
#define VECTORMGR_SET_MODE_MSG_LENGTH      (4+sizeof(VectorMode))
#endif /* G993P5_OVERHEAD_MESSAGING */

typedef union VectorMgrEocBufferCmd VectorMgrEocBufferCmd;
union VectorMgrEocBufferCmd
{
#ifndef G993P5_OVERHEAD_MESSAGING
  uchar setPilotCmd[VECTORMGR_SET_PILOT_MSG_LENGTH];
#endif /* G993P5_OVERHEAD_MESSAGING */
  uchar startDumpCmd[VECTORMGR_START_DUMP_MSG_LENGTH];
  uchar setModeCmd[VECTORMGR_SET_MODE_MSG_LENGTH];
};
typedef union VectorMgrEocBufferResponse VectorMgrEocBufferResponse;
union VectorMgrEocBufferResponse
{
#ifdef G993P5_OVERHEAD_MESSAGING
  uchar ack[4+4];
#else
  uchar ack[4];
#endif
};

#define VECTORMGR_REQUEST_MSG_LENGTH       (sizeof(VectorMgrEocBufferCmd))
#define VECTORMGR_REPLY_MSG_LENGTH         (sizeof(VectorMgrEocBufferResponse))

#endif /* (defined(VECTORING) && defined(SUPPORT_VECTORINGD)) */

/* frame flags */

#define   kG992p3OvhSegMsgMaxLen      1000

#define kFrameBusy              0x01
#define kFrameNextSegPending        0x02
#define kFrameSetCmdTime          0x04
#define kFrameSetSegTime          0x08
#define kFrameCmdAckPending         0x10
#define kFrameClearEoc            0x20
#define kFramePollCmd           0x40

#define G992p3GetFrameInfoPtr(gv,f)     (((ulong*) DslFrameGetLinkFieldAddress(gv,f)) + 2)

#define G992p3GetFrameInfoVal(gv,f)     *G992p3GetFrameInfoPtr(gv,f)
#define G992p3SetFrameInfoVal(gv,f,val)   *G992p3GetFrameInfoPtr(gv,f) = val;

#define G992p3FrameIsBitSet(gv,f,mask)    (*G992p3GetFrameInfoPtr(gv,f) & mask)
#define G992p3FrameBitSet(gv,f,mask)    (*G992p3GetFrameInfoPtr(gv,f) |= mask)
#define G992p3FrameBitClear(gv,f,mask)    (*G992p3GetFrameInfoPtr(gv,f) &= ~mask)

#define G992p3IsFrameBusy(gv,f)       G992p3FrameIsBitSet(gv,f,kFrameBusy)
#define G992p3TakeFrame(gv,f)       G992p3FrameBitSet(gv,f,kFrameBusy)
#define G992p3ReleaseFrame(gv,f)      G992p3FrameBitClear(gv,f,kFrameBusy)

/* globalVar.txFlags */

#define kTxCmdWaitingAck          0x0010
#define kTxCmdL3WaitingAck          0x0020
#define kTxCmdL3RspWait           0x0040
#define kTxCmdL0WaitingAck          0x0100

#define G992p3OvhMsgXmtRspBusy()      G992p3IsFrameBusy(gDslVars, &globalVar.txRspFrame)
#define G992p3OvhMsgXmtPwrRspBusy()     G992p3IsFrameBusy(gDslVars, &globalVar.txPwrRspFrame)
#define G992p3OvhMsgXmtCmdBusy()      G992p3IsFrameBusy(gDslVars, &globalVar.txCmdFrame)
#define G992p3OvhMsgXmtOLRCmdBusy()     G992p3IsFrameBusy(gDslVars, &globalVar.txOLRCmdFrame)

/* globalVar.setup flag */
#define kG992p3ClearEocWorkaround            0x80000000
#define kG992p3ClearEocCXSYWorkaround        0x40000000
#define kG992p3ClearEocCXSYCounterWorkaround 0x20000000
#define kG992p3ClearEocGSPNXmtPwrDSNoUpdate  0x10000000

#define RestrictValue(n,l,r)        ((n) < (l) ? (l) : (n) > (r) ? (r) : (n))

#ifdef AEI_VDSL_CUSTOMER_TELUS
#ifdef AEI_VDSL_CUSTOMER_V1000F	
char g992p3OvhMsgVendorId[kDslVendorIDRegisterLength+1] = "\xB5""\x00""V1000F""\x00\x00";
#else
char g992p3OvhMsgVendorId[kDslVendorIDRegisterLength+1] = "\xB5""\x00""ACTI""\x00\x00";
#endif
#else
char g992p3OvhMsgVendorId[kDslVendorIDRegisterLength+1] = "\xB5""\x00""BDCM""\x00\x00";
#endif


/* Counter read/write macros */

#define WriteCnt16(pData,cnt) do {        \
  ((uchar *)(pData))[0] = ((cnt) >> 8) & 0xFF;  \
  ((uchar *)(pData))[1] = (cnt) & 0xFF;     \
} while (0)

#define WriteCnt32(pData,cnt) do {        \
  ((uchar *)(pData))[0] = (cnt) >> 24;      \
  ((uchar *)(pData))[1] = ((cnt) >> 16) & 0xFF; \
  ((uchar *)(pData))[2] = ((cnt) >> 8) & 0xFF;  \
  ((uchar *)(pData))[3] = (cnt) & 0xFF;     \
} while (0)

#define ReadCnt16(pData)  \
  (((ulong) ((uchar *)(pData))[0] << 8) + ((uchar *)(pData))[1])

#define ReadCnt32(pData)          \
  (((ulong) ((uchar *)(pData))[0] << 24) + \
  ((ulong) ((uchar *)(pData))[1] << 16) + \
  ((ulong) ((uchar *)(pData))[2] << 8)  + \
  ((uchar *)(pData))[3])

#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
#ifndef G993P5_OVERHEAD_MESSAGING
#define ReadLittleEndian16(pData) \
  (((ulong) ((uchar *)(pData))[1] << 8) + ((uchar *)(pData))[0])
#endif
#endif

#ifdef G992P3

#define kPollCmdActive            0x00000001
#define kPollCmdOnce            0x00000002
#define kPollCmdStayActive          0x00000004


#define kSnrfReadCmdId      0
#define kQlnReadCmdId     1
#define kHlogReadCmdId      2
#define kAttnReadCmdId      3
#define kMgntCntReadCmdId   4
#define kSnrmReadCmdId      5
#define kMaxRateReadCmdId   6
#define kXmtPowerReadCmdId    7
#define kSatnReadCmdId      8
#define kVendorIdCmdId      9
#define kPMDSingleReadCmd   10
#define kPMDVectorBlockReadCmd   11
#define kPMDBlockReadCmd   12
#define kINPReadCmdId            13
#define kINPReinReadCmdId      14

#define kPMDVectorPollPeriod		20000

//static Boolean  gPollingRateAdjusted  = false;
#define gPollingRateAdjusted(x) (((dslVarsStruct *)(x))->pollingRateAdjusted)

#ifdef SUPPORT_DSL_BONDING
uchar mgntCntReadCmdLine0[] = { kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdCntRead, kG992p3OvhMsgCmdCntReadId};
uchar mgntCntReadCmdLine1[] = { kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdCntRead, kG992p3OvhMsgCmdCntReadId};
uchar *mgntCntReadCmd[] = {mgntCntReadCmdLine0, mgntCntReadCmdLine1};
#if 0
uchar mgntCntReadCmd[2][4] = {{ kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdCntRead, kG992p3OvhMsgCmdCntReadId},
						{ kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdCntRead, kG992p3OvhMsgCmdCntReadId}};
#endif
#else
uchar mgntCntReadCmd[] = { kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdCntRead, kG992p3OvhMsgCmdCntReadId};
#endif
uchar attnReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDLnAttn };
uchar satnReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDSigAttn };
uchar snrmReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead , kG992p3OvhMsgCmdPMDSnrMgn};
uchar maxRateReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDAttnDR };
uchar xmtPowerReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDNeXmtPower };
uchar snrfReadCmd[]  = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDSnr };
uchar qlnReadCmd[]  = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDQLineNoise };
uchar hlogReadCmd[]  = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDChanRspLog };
uchar	VendorIdCmd[]  = { kG992p3PriorityNormal, 0,
	kG992p3OvhMsgCmdInventory, kG992p3OvhMsgCmdInvId };
uchar	PMDSingleReadCmd[] = { kG992p3PriorityLow, 0,
	kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead };
#ifdef SUPPORT_DSL_BONDING
uchar PMDVectorBlockReadCmdLine0[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDVectorBlockRead,kG992p3OvhMsgCmdPMDChanRspLog,0x00,0x00,0x00,0x3F};
uchar PMDVectorBlockReadCmdLine1[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDVectorBlockRead,kG992p3OvhMsgCmdPMDChanRspLog,0x00,0x00,0x00,0x3F};
uchar PMDBlockReadCmdLine0[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDBlockRead,0x00,0x00,0x00,0x1F};
uchar PMDBlockReadCmdLine1[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDBlockRead,0x00,0x00,0x00,0x1F};
uchar *PMDVectorBlockReadCmd[] = {PMDVectorBlockReadCmdLine0, PMDVectorBlockReadCmdLine1};
uchar *PMDBlockReadCmd[] = {PMDBlockReadCmdLine0, PMDBlockReadCmdLine1};
#if 0
uchar PMDVectorBlockReadCmd[2][9] = {{ kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDVectorBlockRead,kG992p3OvhMsgCmdPMDChanRspLog,0x00,0x00,0x00,0x3F},
							{ kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDVectorBlockRead,kG992p3OvhMsgCmdPMDChanRspLog,0x00,0x00,0x00,0x3F}};
uchar PMDBlockReadCmd[2][9] = {{ kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDBlockRead,0x00,0x00,0x00,0x1F},
							{ kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDBlockRead,0x00,0x00,0x00,0x1F}};
#endif
#else
uchar PMDVectorBlockReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDVectorBlockRead,kG992p3OvhMsgCmdPMDChanRspLog,0x00,0x00,0x00,0x3F};
uchar PMDBlockReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDBlockRead,0x00,0x00,0x00,0x1F};
#endif

uchar inpReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDSingleReadINPId };
uchar inpReinReadCmd[] = { kG992p3PriorityLow, 0, 
  kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDSingleReadINPReinId };

Private void G992p3OvhMsgCompleteClearEocFrame(void *gDslVars, dslFrame *pFrame);
Private void G992p3OvhMsgPollCmdRateChange(void *gDslVars);
Private Boolean G992p3OvhMsgRcvProcessCmd(void *gDslVars, dslFrameBuffer *pBuf, uchar *pData, ulong cmdLen, dslFrame *pFrame);

extern void *memcpy( void *, const void *, unsigned int );
extern void *memset( void *, unsigned int, unsigned int );
extern int G997ReturnFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame);

Private short log2Int (unsigned long x)
{
    short k=0;
    unsigned long y=1;
    while(y<x) {y<<=1;k++;}
    return k;
}

#ifdef G992P3_OLR_TEST

extern dslVarsStruct  acDslVars;

void OLRRequestTest(void)
{
  void *gDslVars = &acDslVars;
  int             i;
  dslStatusStruct       status;
  static dslOLRCarrParam2p  carTbl[8];


  __SoftDslPrintf(NULL, "OLRRequestTest:", 0);
  status.code = kDslOLRRequestStatus;
  status.param.dslOLRRequest.msgType = kOLRRequestType4;
  status.param.dslOLRRequest.nCarrs  = 8;
  for (i = 0; i < 4; i++) {
    status.param.dslOLRRequest.L[i] = (i << 8) | (i+2);
    status.param.dslOLRRequest.B[i] = i + 1;
  }
  if (status.param.dslOLRRequest.msgType < kOLRRequestType4) {
    dslOLRCarrParam   *pCar = (void *) carTbl;
    
    for (i = 0; i < 8; i++) {
    pCar[i].ind  = 600 + i;
    pCar[i].gain = 16 + i;
    pCar[i].gb   = (i << 4) | (i + 1);
    }
  }
  else {
    dslOLRCarrParam2p   *pCar = (void *) carTbl;

    for (i = 0; i < 8; i++) {
    pCar[i].ind  = 600 + i;
    pCar[i].gain = 16 + i;
    pCar[i].gb   = (i << 4) | (i + 1);
    }
  }

  status.param.dslOLRRequest.carrParamPtr = (void *) carTbl;
  G992p3OvhMsgStatusSnooper(gDslVars, &status);
}

void OLRResponseTest(void)
{
  void *gDslVars = &acDslVars;
  dslStatusStruct   status;

  status.code = kDslOLRResponseCmd;
  status.param.value = kOLRRejectType3;
  G992p3OvhMsgStatusSnooper(gDslVars, &status);
}

void PwrTestMsgPrint(char *hdr, dslPwrMessage *pMsg)
{
  __SoftDslPrintf(NULL, "%s: type=0x%X, val=0x%X, len=%d ptr=0x%X", 0, hdr, pMsg->msgType,
    pMsg->param.value, pMsg->param.msg.msgLen, pMsg->param.msg.msgData);
}

void PwrTestCmd1(void)
{
  void *gDslVars = &acDslVars;
  dslStatusStruct       status;

  status.code = kDslPwrMgrCmd;
  status.param.dslPwrMsg.msgType = kPwrSimpleRequest;
  status.param.dslPwrMsg.param.value = 0x19;
  PwrTestMsgPrint("PwrTestCmd1", &status.param.dslPwrMsg);
  G992p3OvhMsgStatusSnooper(gDslVars, &status);
}

void PwrTestCmd2(void)
{
  void *gDslVars = &acDslVars;
  dslStatusStruct       status;
  uchar       *p;
  int         i;

  status.code = kDslPwrMgrCmd;
  status.param.dslPwrMsg.msgType = kPwrL2Grant;
  status.param.dslPwrMsg.param.msg.msgLen = 0x40;
  status.param.dslPwrMsg.param.msg.msgData = p = AdslCoreSharedMemAlloc(0x100);
  for (i = 0; i < status.param.dslPwrMsg.param.msg.msgLen; i++)
    p[i] = i & 0xFF;

  PwrTestMsgPrint("PwrTestCmd2", &status.param.dslPwrMsg);
  G992p3OvhMsgStatusSnooper(gDslVars, &status);
}

void OvhMsgSnr(void)
{
  void *gDslVars = &acDslVars;
  char snrCmd[]  = { kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDSnr };
  // char snrCmd[]  = { kG992p3PriorityLow, 0, kG992p3OvhMsgCmdPMDRead, kG992p3OvhMsgCmdPMDSingleRead, kG992p3OvhMsgCmdPMDChanRspLog };
  dslFrameBuffer  frBuf;

  DslFrameNativeBufferSetLength(&frBuf, sizeof(snrCmd));
  DslFrameNativeBufferSetAddress(&frBuf, snrCmd);
  snrCmd[kG992p3CtrlField] = globalVar.rxCmdMsgNum ^ 1;
  G992p3OvhMsgRcvProcessCmd(gDslVars, &frBuf, snrCmd, sizeof(snrCmd), NULL);
}
#endif /* G992P3_OLR_TEST */

static int FrameDataCopy(void *gDslVars, dslFrameBuffer *pBuf, int firstBufOffset, ulong destLen, uchar *pDest)
{
	uchar     *pData, *pDest0 = pDest, *pDestEnd = pDest + destLen;
	int       len, n;

	len   = DslFrameBufferGetLength(gDslVars, pBuf);
	pData = DslFrameBufferGetAddress(gDslVars, pBuf);

	n = len - firstBufOffset;
	if (n > 0) {
		memcpy (pDest, pData+firstBufOffset, n);
		pDest += n;
	}

	pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
	while (NULL != pBuf) {
		len   = DslFrameBufferGetLength(gDslVars, pBuf);
		pData = DslFrameBufferGetAddress(gDslVars, pBuf);
		if (len > (pDestEnd - pDest))
			len = pDestEnd - pDest;
		memcpy (pDest, pData, len);
		pDest += len;
		if (pDest == pDestEnd)
			break;
		pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
	}
	
	return pDest - pDest0;
}

#ifdef DEBUG_PRINT_RCV_SEG
extern int BcmAdslCoreDiagWriteStatusData(ulong cmd, char *buf0, int len0, char *buf1, int len1);

static void G992p3OvhMsgPrintAssembledSeg(uchar *pData, int dataLen)
{
	dslStatusStruct	status;
	ulong			cmd = statusInfoData;
	int 				dataOffset, packetDataLimit = 1200;
	
	status.code = kDslDspControlStatus;
	status.param.dslConnectInfo.code = kDslRxOvhMsg;
	status.param.dslConnectInfo.value = dataLen;
	
	if(dataLen <= packetDataLimit) {
		BcmAdslCoreDiagWriteStatusData(cmd,
					(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
					pData, dataLen);
	}
	else {
		BcmAdslCoreDiagWriteStatusData(cmd | 0x80000000,
					(char *)&status, sizeof(status.code) + sizeof(status.param.dslConnectInfo),
					pData, packetDataLimit);	/* DIAG_SPLIT_MSG */
		dataLen -= packetDataLimit;
		dataOffset = packetDataLimit;
		while(dataLen) {
			if(dataLen > packetDataLimit) {
				BcmAdslCoreDiagWriteStatusData(cmd -2, pData+dataOffset, dataLen, NULL, 0);
				dataLen -= packetDataLimit;
				dataOffset += packetDataLimit;
			}
			else {
				BcmAdslCoreDiagWriteStatusData(cmd -3,	pData+dataOffset, dataLen, NULL, 0);
				dataLen = 0;
			}
		}
	}
}
#endif

static uchar G992p3OvhMsgGetSC(void *gDslVars, dslFrameBuffer *pBuf, ulong frameLen)
{
	uchar	*pData;
	int		len;
	ulong	totalLen=0;
	if(NULL == pBuf )
		return((uchar)-1);
	
	while (NULL != pBuf) {
		len   = DslFrameBufferGetLength(gDslVars, pBuf);
		pData = DslFrameBufferGetAddress(gDslVars, pBuf);
		pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
		totalLen += len;
		if(totalLen > frameLen) {
			len -= (totalLen - frameLen);
			break;
		}
		else if (totalLen == frameLen)
			break;
	}
	return(*(pData+len-1));
}

static void G992p3OvhMsgFlushFrameList(void *gDslVars, DListHeader *pFrameList)
{
	void 	*p;
	dslFrame	*pFrame;
	
	p = DListFirst(pFrameList);
	while (DListValid(pFrameList, p)){
		pFrame = (dslFrame *) DslFrameGetFrameAddressFromLink(gDslVars, p);
		DListRemove(p);
		G992p3OvhMsgReturnFrame(gDslVars, NULL, 0, pFrame);
		G997ReturnFrame(gDslVars, NULL, 0, pFrame);
		p = DListFirst(pFrameList);
	}
}

static ulong GetMultiFrameLen(void *gDslVars, DListHeader *pFrameList)
{
	void		*p;
	dslFrame	*pFrame;
	ulong	len=0;

	p = DListFirst(pFrameList);
	while ( DListValid(pFrameList, p) ) {
		pFrame = (dslFrame *) DslFrameGetFrameAddressFromLink(gDslVars, p);
		len += pFrame->totalLength;
		p = DListNext(p);
	}
	
	return len;
}

static int MultiFrameDataCopy(void *gDslVars, DListHeader *pFrameList, int firstBufDataOffset, ulong destLen, uchar *pDest)
{
	void				*p;
	dslFrame			*pFrame;
	dslFrameBuffer	*pBuf;
	uchar			*pData, *pDest0 = pDest, *pDestEnd = pDest + destLen;
	int				len, n, firstFrame=1;

	p = DListFirst(pFrameList);
	if(!DListValid(pFrameList, p))
		return(pDest - pDest0);
	
	do {		
		pFrame = (dslFrame *) DslFrameGetFrameAddressFromLink(gDslVars, p);
		pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
		
		if(firstFrame) {
			firstFrame=0;
			len   = DslFrameBufferGetLength(gDslVars, pBuf);
			pData = DslFrameBufferGetAddress(gDslVars, pBuf);
			n = len - firstBufDataOffset;
			if (n > 0) {
				memcpy (pDest, pData+firstBufDataOffset, n);
				pDest += n;
			}
			pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
		}
		
		while (NULL != pBuf) {
			len   = DslFrameBufferGetLength(gDslVars, pBuf);
			pData = DslFrameBufferGetAddress(gDslVars, pBuf);
			if (len > (pDestEnd - pDest))
				len = pDestEnd - pDest;

			memcpy (pDest, pData, len);
			pDest += len;
			if (pDest == pDestEnd)
				break;

			pBuf = DslFrameGetNextBuffer(gDslVars, pBuf);
		}
		
		p = DListNext(p);
	}while (DListValid(pFrameList, p));
	
	return pDest - pDest0;
}

/*
**
**    G992p3OvhMsg interface functions
**
*/

Private void G992p3OvhMsgPollCmdRateChange(void *gDslVars)
{
  g992p3PollCmdStruct *pCmd;
  
  if(gPollingRateAdjusted(gDslVars))
    return;
#ifndef G993
  pCmd = globalVar.pollCmd + kSnrfReadCmdId;
  pCmd->cmdFlags &= ~kPollCmdActive;
  pCmd = globalVar.pollCmd + kQlnReadCmdId;
  pCmd->cmdFlags &= ~kPollCmdActive;  
  pCmd = globalVar.pollCmd + kHlogReadCmdId;
  pCmd->cmdFlags &= ~kPollCmdActive;
  
  pCmd = globalVar.pollCmd + kAttnReadCmdId;
  pCmd->cmdFlags &= ~kPollCmdActive;
  pCmd = globalVar.pollCmd + kSatnReadCmdId;
  pCmd->cmdFlags &= ~kPollCmdActive;
#endif

  pCmd = globalVar.pollCmd + kMgntCntReadCmdId;
  pCmd->tmPeriod = 15000; /* 15 Secs */ 
#ifndef G993
  pCmd = globalVar.pollCmd + kSnrmReadCmdId;
  pCmd->tmPeriod = 20000; /* 20 Secs */   
  pCmd = globalVar.pollCmd + kMaxRateReadCmdId;
  pCmd->tmPeriod = 60000; /* 1 min. */
  pCmd = globalVar.pollCmd + kXmtPowerReadCmdId;
  pCmd->tmPeriod = 60000; /* 1 min. */  
#endif  

  gPollingRateAdjusted(gDslVars) = true;
}

Private void G992p3OvhMsgInitPollCmd(void *gDslVars, int id, uchar *cmd, ulong len, ulong period, ulong flags)
{
  g992p3PollCmdStruct   *pCmd = globalVar.pollCmd + id;

  pCmd->cmd = cmd;
  pCmd->len = len;
  pCmd->tmPeriod = period;
  pCmd->tmLastSent = globalVar.timeMs - period;
  pCmd->cmdFlags = flags;
}

extern void AdslCoreG997FrameBufferFlush(void *pDslVars);

#if defined(CONFIG_VDSL_SUPPORTED)
Private void G992p3OvhMsgInitHlogVecCmd (void *gDslVars)
{
	int nxtCmd=0;
	uchar *pCmd;
	int cmdSize;
#ifdef SUPPORT_DSL_BONDING
	pCmd = PMDVectorBlockReadCmd[gLineId(gDslVars)];
	cmdSize = sizeof(PMDVectorBlockReadCmdLine0);
#else
	pCmd = &PMDVectorBlockReadCmd[0];
	cmdSize = sizeof(PMDVectorBlockReadCmd);
#endif
	pCmd[4]=kG992p3OvhMsgCmdPMDChanRspLog;
	WriteCnt16((pCmd+5),globalVar.HlogVecCmd.toneGroups[nxtCmd].startToneGp);
	WriteCnt16((pCmd+7),globalVar.HlogVecCmd.toneGroups[nxtCmd].stopToneGp);
	globalVar.HlogVecCmd.lastCmdNm=nxtCmd;
	G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pCmd, cmdSize, 1000, kPollCmdActive);
	globalVar.pollingSnrBlRdCmd=0;
}

Private void G992p3OvhMsgPollPMDPerTonePollCmdControl(void *gDslVars,int lastRspType)
{
	g992p3PollCmdStruct	*pCmd = &globalVar.pollCmd[globalVar.cmdNum];
	int	nxtCmd;
	uchar	*pRdCmd;
	int		rdCmdSize;
	
	__SoftDslPrintf(gDslVars,"In PollPMDPerTonePollCmdControl lastRspType=%d",0,lastRspType);
	
	switch(pCmd->cmd[3])
	{
		case kG992p3OvhMsgCmdPMDBlockRead:
		{
#ifdef SUPPORT_DSL_BONDING
			pRdCmd = PMDBlockReadCmd[gLineId(gDslVars)];
			rdCmdSize = sizeof(PMDBlockReadCmdLine0);
#else
			pRdCmd = &PMDBlockReadCmd[0];
			rdCmdSize = sizeof(PMDBlockReadCmd);
#endif
			if(globalVar.pollingSnrBlRdCmd==0)
			{
				if(globalVar.HlogQlnBlRdCmd.lastCmdNm==globalVar.HlogQlnBlRdCmd.numCmds-1)
				{
					nxtCmd=0;
					WriteCnt16((pRdCmd+4),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].startToneGp);
					WriteCnt16((pRdCmd+6),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].stopToneGp);
					globalVar.SnrBlRdCmd.lastCmdNm=nxtCmd;
					G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
					globalVar.pollingSnrBlRdCmd=1;
				}
				else
				{
					nxtCmd=globalVar.HlogQlnBlRdCmd.lastCmdNm+1;
					WriteCnt16((pRdCmd+4),globalVar.HlogQlnBlRdCmd.toneGroups[nxtCmd].startToneGp);
					WriteCnt16((pRdCmd+6),globalVar.HlogQlnBlRdCmd.toneGroups[nxtCmd].stopToneGp);
					globalVar.HlogQlnBlRdCmd.lastCmdNm=nxtCmd;
					G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
				}
			}
			else
			{
				if(globalVar.SnrBlRdCmd.lastCmdNm==globalVar.SnrBlRdCmd.numCmds-1)
				{
					nxtCmd=0;
					WriteCnt16((pRdCmd+4),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].startToneGp);
					WriteCnt16((pRdCmd+6),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].stopToneGp);
					globalVar.SnrBlRdCmd.lastCmdNm=nxtCmd;
					G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, pRdCmd, rdCmdSize, kPMDVectorPollPeriod, kPollCmdActive);
					pCmd = globalVar.pollCmd + kPMDBlockReadCmd;
					pCmd->tmLastSent = globalVar.timeMs;
				}
				else
				{
					nxtCmd=globalVar.SnrBlRdCmd.lastCmdNm+1;
					WriteCnt16((pRdCmd+4),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].startToneGp);
					WriteCnt16((pRdCmd+6),globalVar.SnrBlRdCmd.toneGroups[nxtCmd].stopToneGp);
					globalVar.SnrBlRdCmd.lastCmdNm=nxtCmd;
					G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
				}
			}
			break;
		}
		case kG992p3OvhMsgCmdPMDVectorBlockRead:
		{
			if(lastRspType==0)
			{
#ifdef SUPPORT_DSL_BONDING
				pRdCmd = PMDBlockReadCmd[gLineId(gDslVars)];
				rdCmdSize = sizeof(PMDBlockReadCmdLine0);	/* cmd size are identical for both lines */
#else
				pRdCmd = &PMDBlockReadCmd[0];
				rdCmdSize = sizeof(PMDBlockReadCmd);
#endif
				nxtCmd=0;
				WriteCnt16((pRdCmd+4),globalVar.HlogQlnBlRdCmd.toneGroups[nxtCmd].startToneGp);
				WriteCnt16((pRdCmd+6),globalVar.HlogQlnBlRdCmd.toneGroups[nxtCmd].stopToneGp);
				globalVar.HlogQlnBlRdCmd.lastCmdNm=nxtCmd;
				G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
				break;
			}
#ifdef SUPPORT_DSL_BONDING
			pRdCmd = PMDVectorBlockReadCmd[gLineId(gDslVars)];
			rdCmdSize = sizeof(PMDVectorBlockReadCmdLine0);	/* cmd size are identical for both lines */
#else
			pRdCmd = &PMDVectorBlockReadCmd[0];
			rdCmdSize = sizeof(PMDVectorBlockReadCmd);
#endif
			switch(pCmd->cmd[4])
			{
				case kG992p3OvhMsgCmdPMDChanRspLog:
				{
					if(globalVar.HlogVecCmd.lastCmdNm==globalVar.HlogVecCmd.numCmds-1)
					{
						nxtCmd=0;
						pRdCmd[4]=kG992p3OvhMsgCmdPMDQLineNoise;
						WriteCnt16((pRdCmd+5),globalVar.QLNVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.QLNVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.QLNVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
					}
					else
					{
						nxtCmd=globalVar.HlogVecCmd.lastCmdNm+1;
						WriteCnt16((pRdCmd+5),globalVar.HlogVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.HlogVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.HlogVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
					}
					break;
				}
				case kG992p3OvhMsgCmdPMDQLineNoise:
				{
					if(globalVar.QLNVecCmd.lastCmdNm==globalVar.QLNVecCmd.numCmds-1)
					{
						nxtCmd=0;
						pRdCmd[4]=kG992p3OvhMsgCmdPMDSnr;
						WriteCnt16((pRdCmd+5),globalVar.SnrVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.SnrVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.SnrVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, kPMDVectorPollPeriod, kPollCmdActive|kPollCmdStayActive);
					}
					else
					{
						nxtCmd=globalVar.QLNVecCmd.lastCmdNm+1;
						WriteCnt16((pRdCmd+5),globalVar.QLNVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.QLNVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.QLNVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
					}
					break;
				}
				case kG992p3OvhMsgCmdPMDSnr:
				{
					if(globalVar.SnrVecCmd.lastCmdNm==globalVar.SnrVecCmd.numCmds-1)
					{
						nxtCmd=0;
						WriteCnt16((pRdCmd+5),globalVar.SnrVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.SnrVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.SnrVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, kPMDVectorPollPeriod, kPollCmdActive);
						pCmd = globalVar.pollCmd + kPMDVectorBlockReadCmd;
						pCmd->tmLastSent = globalVar.timeMs;
					}
					else
					{
						nxtCmd=globalVar.SnrVecCmd.lastCmdNm+1;
						WriteCnt16((pRdCmd+5),globalVar.SnrVecCmd.toneGroups[nxtCmd].startToneGp);
						WriteCnt16((pRdCmd+7),globalVar.SnrVecCmd.toneGroups[nxtCmd].stopToneGp);
						globalVar.SnrVecCmd.lastCmdNm=nxtCmd;
						G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, pRdCmd, rdCmdSize, 1000, kPollCmdActive);
					}
					break;
				}
			}
			break;
		}
		default:
			break;
	}
}

Private void CreateVectorReadCmdSeq(void *gDslVars,VDSLPMDVectorReadCmdToneGrp* CmdGrp, int MaxGrpSize,bandPlanDescriptor *bandplan, int gFactor)
{
	unsigned short n=0,bp=0,nextBand=1,MaxInc;
	MaxInc=MaxGrpSize-1;
	if(0 == gFactor)
		gFactor = 1;
	do{
		if(nextBand==1)
			CmdGrp->toneGroups[n].startToneGp=(bandplan->toneGroups[bp].startTone)/gFactor;
		else
			CmdGrp->toneGroups[n].startToneGp=CmdGrp->toneGroups[n-1].stopToneGp+1;
		if((CmdGrp->toneGroups[n].startToneGp+MaxInc)*gFactor < bandplan->toneGroups[bp].endTone+1)
		{
			CmdGrp->toneGroups[n].stopToneGp=CmdGrp->toneGroups[n].startToneGp+MaxInc;
			nextBand=0;
		}
		else
		{
			CmdGrp->toneGroups[n].stopToneGp=(bandplan->toneGroups[bp].endTone)/gFactor;
			nextBand=1;
			bp++;
		}
#if 0
		__SoftDslPrintf(gDslVars,"Cmd num %d StartGp=%d StopGp=%d",0,n,CmdGrp->toneGroups[n].startToneGp,CmdGrp->toneGroups[n].stopToneGp);
#endif
		n++;
	}while(bp<bandplan->noOfToneGroups);
	CmdGrp->numCmds=n;
	CmdGrp->lastCmdNm=-1;
}
#endif

Public void G992p3OvhMsgSetRateChangeFlag(void *gDslVars)
{
	globalVar.rateChangeFlag=1;
}

Public void G992p3OvhMsgReset(void *gDslVars)
{
	ulong	*pFrFlags;
	dslFrame	*pFrame, **pFr;
	int		i;
	adslMibInfo	*pMib;
	ulong	mibLen;
	
	pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);

	globalVar.rxCmdMsgNum = 1;
	globalVar.rxRspMsgNum = 0;
	globalVar.txCmdMsgNum[kG992p3PriorityHigh] = 1;
	globalVar.txCmdMsgNum[kG992p3PriorityNormal] = 1;
	globalVar.txCmdMsgNum[kG992p3PriorityLow] = 1;
	globalVar.txCmdMsgNum[3] = 1;
	globalVar.txRspMsgNum = 1;
	globalVar.txFlags   = 0;
	globalVar.txL0Rq    = false;
	globalVar.txL3Rq    = false;

	globalVar.timeCmdOut  = globalVar.timeMs;
	globalVar.cmdTryCnt   = 0;
	globalVar.cmdNum    = (ulong) -1;
#if defined(CONFIG_VDSL_SUPPORTED)
	globalVar.pollingSnrBlRdCmd=0;
#endif
	AdslMibByteClear(sizeof(globalVar.pollCmd), (void *)globalVar.pollCmd);

#ifdef SUPPORT_DSL_BONDING
	G992p3OvhMsgInitPollCmd(gDslVars, kMgntCntReadCmdId, mgntCntReadCmd[gLineId(gDslVars)], sizeof(mgntCntReadCmdLine0), 1000, kPollCmdActive);
#else
	G992p3OvhMsgInitPollCmd(gDslVars, kMgntCntReadCmdId, mgntCntReadCmd, sizeof(mgntCntReadCmd), 1000, kPollCmdActive);
#endif

#if defined(CONFIG_VDSL_SUPPORTED)
	if( XdslMibIsVdsl2Mod(gDslVars) ) {
		G992p3OvhMsgInitPollCmd(gDslVars, kSnrfReadCmdId, snrfReadCmd,sizeof(snrfReadCmd), 10000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kQlnReadCmdId, qlnReadCmd,sizeof(qlnReadCmd), 10000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kHlogReadCmdId, hlogReadCmd,sizeof(hlogReadCmd), 10000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kAttnReadCmdId, attnReadCmd, sizeof(attnReadCmd), 5000,0);
		G992p3OvhMsgInitPollCmd(gDslVars, kSnrmReadCmdId, snrmReadCmd, sizeof(snrmReadCmd), 3000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kMaxRateReadCmdId, maxRateReadCmd, sizeof(maxRateReadCmd), 8000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kXmtPowerReadCmdId, xmtPowerReadCmd, sizeof(xmtPowerReadCmd), 5000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kSatnReadCmdId, satnReadCmd, sizeof(satnReadCmd), 10000, 0);
		G992p3OvhMsgInitPollCmd(gDslVars, kVendorIdCmdId, VendorIdCmd, sizeof(VendorIdCmd), 10000, kPollCmdActive | kPollCmdOnce);
		G992p3OvhMsgInitPollCmd(gDslVars, kPMDSingleReadCmd, PMDSingleReadCmd, sizeof(PMDSingleReadCmd), 3000, kPollCmdActive);
#ifdef SUPPORT_DSL_BONDING
		G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, PMDVectorBlockReadCmd[gLineId(gDslVars)],
								sizeof(PMDVectorBlockReadCmdLine0), 1000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, PMDBlockReadCmd[gLineId(gDslVars)],
								sizeof(PMDBlockReadCmdLine0), 1000, 0);
#else
		G992p3OvhMsgInitPollCmd(gDslVars, kPMDVectorBlockReadCmd, PMDVectorBlockReadCmd, sizeof(PMDVectorBlockReadCmd), 1000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kPMDBlockReadCmd, PMDBlockReadCmd, sizeof(PMDBlockReadCmd), 1000, 0);
#endif
		globalVar.pollCmdNum = 13;
	}
	else
#endif
	{
		G992p3OvhMsgInitPollCmd(gDslVars, kSnrfReadCmdId, snrfReadCmd,sizeof(snrfReadCmd), 10000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kQlnReadCmdId, qlnReadCmd,sizeof(qlnReadCmd), 10000, kPollCmdActive | kPollCmdOnce);
		G992p3OvhMsgInitPollCmd(gDslVars, kHlogReadCmdId, hlogReadCmd,sizeof(hlogReadCmd), 10000, kPollCmdActive | kPollCmdOnce);
		G992p3OvhMsgInitPollCmd(gDslVars, kAttnReadCmdId, attnReadCmd, sizeof(attnReadCmd), 5000,  kPollCmdActive | kPollCmdOnce);
		G992p3OvhMsgInitPollCmd(gDslVars, kSnrmReadCmdId, snrmReadCmd, sizeof(snrmReadCmd), 3000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kMaxRateReadCmdId, maxRateReadCmd, sizeof(maxRateReadCmd), 8000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kXmtPowerReadCmdId, xmtPowerReadCmd, sizeof(xmtPowerReadCmd), 5000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kSatnReadCmdId, satnReadCmd, sizeof(satnReadCmd), 10000, kPollCmdActive);
		G992p3OvhMsgInitPollCmd(gDslVars, kVendorIdCmdId, VendorIdCmd, sizeof(VendorIdCmd), 10000, kPollCmdActive | kPollCmdOnce);
		if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
			G992p3OvhMsgInitPollCmd(gDslVars, kINPReadCmdId, inpReadCmd, sizeof(inpReadCmd), 10000, kPollCmdActive);
			G992p3OvhMsgInitPollCmd(gDslVars, kINPReinReadCmdId, inpReinReadCmd, sizeof(inpReinReadCmd), 10000, kPollCmdActive);
			globalVar.pollCmdNum = 15;
		}
		else
		globalVar.pollCmdNum = 10;
	}

	pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.txRspFrame);
	*pFrFlags = 0;
	pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.txCmdFrame);
	*pFrFlags = 0;
	pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.txOLRCmdFrame);
	*pFrFlags = 0;
	pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.txPwrRspFrame);
	*pFrFlags = 0;
	globalVar.txSegFrameCtl.segFrame = NULL;

	gPollingRateAdjusted(gDslVars) = false;
	globalVar.setup &= ~(kG992p3ClearEocWorkaround | kG992p3ClearEocCXSYWorkaround |kG992p3ClearEocCXSYCounterWorkaround | kG992p3ClearEocGSPNXmtPwrDSNoUpdate);

	AdslCoreG997FrameBufferFlush(gDslVars);

	while(CircBufferGetReadAvail(&globalVar.txClEocFrameCB) > 0) {
		pFr = CircBufferGetReadPtr(&globalVar.txClEocFrameCB);
		pFrame = *pFr;
		CircBufferReadUpdate(&globalVar.txClEocFrameCB, sizeof(dslFrame*));
		G992p3OvhMsgCompleteClearEocFrame(gDslVars, pFrame);
	}

	globalVar.olrSegmentSerialNum=0;
	G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.g992P3OlrFrameList);
	for(i=0; i < 3; i++) {
		globalVar.segCmd[i].segId = 0;
		pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.segCmd[i].txRspFrame);
		*pFrFlags = 0;
		G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.segCmd[i].segFrameList);
		globalVar.segRsp[i].segId = 0;
		pFrFlags = G992p3GetFrameInfoPtr(globalVar, &globalVar.segRsp[i].txRspFrame);
		*pFrFlags = 0;
		G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.segRsp[i].segFrameList);
	}
	/* Move from RxShowtimeActive */
#if defined(CONFIG_VDSL_SUPPORTED) && \
	(defined(USE_CXSY_OVH_MSG_WORKAROUND) || defined(USE_CXSY_OVH_MSG_DISABLE_POLLING_CMD) ||defined(USE_CXSY_OVH_MSG_COUNTER_WORKAROUND))
	if (XdslMibIsVdsl2Mod(gDslVars) &&
		pMib->xdslAtucPhys.adslVendorID[2] == 'C' &&
		pMib->xdslAtucPhys.adslVendorID[3] == 'X' &&
		pMib->xdslAtucPhys.adslVendorID[4] == 'S' &&
		pMib->xdslAtucPhys.adslVendorID[5] == 'Y' ) {
#if defined(USE_CXSY_OVH_MSG_DISABLE_POLLING_CMD)
		/* Disable polling Management counter, PMD Test Parameter and Inventory commands */
		globalVar.pollCmd[kMgntCntReadCmdId].cmdFlags = 0;
		globalVar.pollCmd[kPMDSingleReadCmd].cmdFlags = 0;
		globalVar.pollCmd[kPMDVectorBlockReadCmd].cmdFlags = 0;
		globalVar.pollCmd[kPMDBlockReadCmd].cmdFlags = 0;
		globalVar.pollCmd[kVendorIdCmdId].cmdFlags = 0;
#endif
#if defined(USE_CXSY_OVH_MSG_COUNTER_WORKAROUND)
		/* Report CRC's and FEC's counters since showtime iso since power on */
		globalVar.setup |= kG992p3ClearEocCXSYCounterWorkaround;
#endif
#if defined(USE_CXSY_OVH_MSG_WORKAROUND)
		globalVar.setup |= kG992p3ClearEocCXSYWorkaround;
#endif
	}
#endif	/* CONFIG_VDSL_SUPPORTED ....*/

	if( pMib->adslAtucPhys.adslVendorID[2] == 'G' &&
		pMib->adslAtucPhys.adslVendorID[3] == 'S' &&
		pMib->adslAtucPhys.adslVendorID[4] == 'P' &&
		pMib->adslAtucPhys.adslVendorID[5] == 'N' ) {
		globalVar.setup |= kG992p3ClearEocGSPNXmtPwrDSNoUpdate;
		G992p3OvhMsgPollCmdRateChange(gDslVars);
		__SoftDslPrintf(gDslVars, "CPE is connecting GSPN CO",0);
	}
	/* Move from TxShowtimeActive */
	globalVar.cmdPollingStamp = globalVar.timeMs;
#if defined(CONFIG_VDSL_SUPPORTED)
	if( XdslMibIsVdsl2Mod(gDslVars) && AdslMibIsLinkActive(gDslVars) &&
		(globalVar.pollCmd[kPMDVectorBlockReadCmd].cmdFlags==kPollCmdActive) ) {
		CreateVectorReadCmdSeq(gDslVars,&globalVar.HlogVecCmd,19,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
		CreateVectorReadCmdSeq(gDslVars,&globalVar.QLNVecCmd,38,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
		CreateVectorReadCmdSeq(gDslVars,&globalVar.SnrVecCmd,38,&(pMib->usNegBandPlan),pMib->gFactors.Gfactor_MEDLEYSETus);
		CreateVectorReadCmdSeq(gDslVars,&globalVar.HlogQlnBlRdCmd,32,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
		CreateVectorReadCmdSeq(gDslVars,&globalVar.SnrBlRdCmd,32,&(pMib->usNegBandPlan),pMib->gFactors.Gfactor_MEDLEYSETus);
		G992p3OvhMsgInitHlogVecCmd(gDslVars);
	}
#endif
}

Public Boolean  G992p3OvhMsgInit(
      void          *gDslVars, 
      bitMap          setup, 
      dslFrameHandlerType   rxReturnFramePtr,
      dslFrameHandlerType   txSendFramePtr,
      dslFrameHandlerType   txSendCompletePtr,
      dslCommandHandlerType commandHandler,
      dslStatusHandlerType  statusHandler)
{
	int i;
	globalVar.setup       = setup;
	globalVar.txSendFramePtr  = txSendFramePtr;
	globalVar.txSendCompletePtr = txSendCompletePtr;
	globalVar.cmdHandlerPtr   = commandHandler;
	globalVar.statusHandlerPtr  = statusHandler;

	DslFrameInit (gDslVars, &globalVar.txRspFrame);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txRspFrBuf, globalVar.txRspBuf);

	DslFrameBufferSetAddress (gDslVars, &globalVar.txPwrRspFrBuf0, globalVar.txPwrRspBuf0);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txPwrRspFrBuf0a, globalVar.txPwrRspBuf0+2);


	DslFrameInit (gDslVars, &globalVar.txCmdFrame);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txCmdFrBuf0, globalVar.txCmdBuf);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txCmdFrBuf0a, globalVar.txCmdBuf+4);
	DslFrameBufferSetLength (gDslVars, &globalVar.txCmdFrBuf0, 0);
	globalVar.lastTxCmdFrame = &globalVar.txCmdFrame;

	DslFrameInit (gDslVars, &globalVar.txOLRCmdFrame);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txOLRCmdFrBuf0, globalVar.txOLRCmdBuf);
	DslFrameBufferSetAddress (gDslVars, &globalVar.txOLRCmdFrBuf0a, globalVar.txOLRCmdBuf+4);
	DslFrameBufferSetLength (gDslVars, &globalVar.txOLRCmdFrBuf0, 0);

	CircBufferInit (&globalVar.txClEocFrameCB, globalVar.txClEocFrame, sizeof(globalVar.txClEocFrame));
	
	globalVar.PLNMessageReadRspFlag=0;
	globalVar.PLNPerBinMeasurementCounterRcvFlag=0;
	globalVar.PLNBroadbandMsrCounterRcvFlag=0;
	globalVar.kG992p3OvhMsgCmdPLN=0x10;
	globalVar.timeMs = 0;
	globalVar.PLNElapsedTimeMSec=0;
	globalVar.PLNActiveFlag=0;
	globalVar.INMMessageReadCntrRspFlag=0;
	globalVar.INMMessageReadParamsRspFlag=0;
	globalVar.INMWaitForParamsFlag=0;
	
	DListInit(&globalVar.g992P3OlrFrameList);
	globalVar.olrSegmentSerialNum=0;
	for(i=0; i < 3; i++) {
		globalVar.segCmd[i].segId = 0;
		DListInit(&globalVar.segCmd[i].segFrameList);
		DslFrameInit (gDslVars, &globalVar.segCmd[i].txRspFrame);
		DslFrameBufferSetAddress (gDslVars, &globalVar.segCmd[i].txRspFrBuf, globalVar.segCmd[i].txRspBuf);
		globalVar.segRsp[i].segId = 0;
		DListInit(&globalVar.segRsp[i].segFrameList);
		DslFrameInit (gDslVars, &globalVar.segRsp[i].txRspFrame);
		DslFrameBufferSetAddress (gDslVars, &globalVar.segRsp[i].txRspFrBuf, globalVar.segRsp[i].txRspBuf);
	}
	G992p3OvhMsgReset(gDslVars);
	return true;
}

Public void G992p3OvhMsgClose(void *gDslVars)
{
}
  
Public Boolean G992p3OvhMsgCommandHandler(void *gDslVars, dslCommandStruct *cmd)
{
  return false;
}

Private Boolean G992p3OvhMsgXmtSendFrame(void *gDslVars, dslFrame *pFrame)
{
  if (G992p3IsFrameBusy(gDslVars, pFrame))
    return false;

  G992p3TakeFrame(gDslVars, pFrame);
  pFrame->bufCnt = kG992p3OvhMsgFrameBufCnt;
  (*globalVar.txSendFramePtr)(gDslVars, NULL, 0, pFrame);
  return true;
}

Private Boolean G992p3OvhMsgXmtSendCurrSeg(void *gDslVars, g992p3SegFrameCtlStruct *pSegCtl)
{
  if (G992p3OvhMsgXmtSendFrame(gDslVars, pSegCtl->segFrame)) {
    pSegCtl->tryCnt++;
    pSegCtl->timeSegOut = globalVar.timeMs;
    G992p3FrameBitSet(gDslVars, pSegCtl->segFrame, kFrameSetSegTime);
    return true;
  }

  pSegCtl->timeSegOut = globalVar.timeMs - 800;
  return false;
}

Private void G992p3OvhMsgXmtSendNextSeg(void *gDslVars, g992p3SegFrameCtlStruct *pSegCtl)
{
  int       n, len, frLen;
  dslFrameBuffer  *pBuf, *pFirstBuf;
  uchar     *pData0, *pData1;
  dslFrame    *pFrame = pSegCtl->segFrame;

  if (G992p3IsFrameBusy(gDslVars, pFrame))
    return;
  G992p3FrameBitClear(gDslVars, pFrame, kFrameNextSegPending);

  if (0 == pSegCtl->segId) {
    pSegCtl->segFrame = NULL;
    return;
  }

  pSegCtl->segId--;
  pFirstBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
  pData0    = DslFrameBufferGetAddress (gDslVars, pFirstBuf);
  if ((0 == pSegCtl->segId) || (pSegCtl->segId == (pSegCtl->segTotal-1)))
    n = 0x80 | (pSegCtl->segId << 3);
  else
    n = (pSegCtl->segId << 3);
  pData0[kG992p3CtrlField] &= 0x3;
  pData0[kG992p3CtrlField] |= n;

  DslFrameInit (gDslVars, pFrame);
  DslFrameEnqueBufferAtBack (gDslVars, pFrame, pFirstBuf);

  frLen = 0;
  len   = DslFrameBufferGetLength (gDslVars, pSegCtl->segFrBufCur);
  if ((0 == DslFrameBufferGetLength (gDslVars, &pSegCtl->segFrBuf)) && (len < (kG992p3OvhSegMsgMaxLen-10))) {
    frLen = len;
    pBuf = DslFrameGetNextBuffer(gDslVars, pSegCtl->segFrBufCur);
    DslFrameEnqueBufferAtBack (gDslVars, pFrame, pSegCtl->segFrBufCur);
    pSegCtl->segFrBufCur = pBuf;
    pData0  = DslFrameBufferGetAddress (gDslVars, pBuf);
    DslFrameBufferSetAddress (gDslVars, &pSegCtl->segFrBuf, pData0);
  }

  pData0  = DslFrameBufferGetAddress (gDslVars, pSegCtl->segFrBufCur);
  len     = DslFrameBufferGetLength (gDslVars, pSegCtl->segFrBufCur);
  pData1  = DslFrameBufferGetAddress (gDslVars, &pSegCtl->segFrBuf);
  pData1 += DslFrameBufferGetLength (gDslVars, &pSegCtl->segFrBuf);
  len    -= (pData1 - pData0);
#ifdef G992P3_DBG_PRINT
  __SoftDslPrintf(gDslVars, "G992p3OvhMsgXmtSendNextSeg: pData0=0x%X, pData1=0x%X, len=%d, frLen=%d\n", 0, 
    pData0, pData1, len, frLen);
#endif
  if (len <= 0) {
    if (NULL == (pSegCtl->segFrBufCur = DslFrameGetNextBuffer(gDslVars, pSegCtl->segFrBufCur))) {
      pSegCtl->segFrame = NULL;
      return;
    }
    pData1  = DslFrameBufferGetAddress (gDslVars, pSegCtl->segFrBufCur);
    len     = DslFrameBufferGetLength (gDslVars, pSegCtl->segFrBufCur);
  }
  DslFrameBufferSetAddress (gDslVars, &pSegCtl->segFrBuf, pData1);

  if ((frLen+len) > kG992p3OvhSegMsgMaxLen)
    len = kG992p3OvhSegMsgMaxLen - frLen;
  DslFrameBufferSetLength (gDslVars, &pSegCtl->segFrBuf, len);

  DslFrameEnqueBufferAtBack (gDslVars, pFrame, &pSegCtl->segFrBuf);
  pSegCtl->tryCnt = 0;
  if (G992p3OvhMsgXmtSendCurrSeg(gDslVars, pSegCtl) && (0 == pSegCtl->segId)) {
    if (G992p3FrameIsBitSet(gDslVars, pFrame, kFrameCmdAckPending)) {
      G992p3FrameBitClear(gDslVars, pFrame, kFrameCmdAckPending);
      globalVar.txFlags |= kTxCmdWaitingAck;
    }
    pSegCtl->segFrame = NULL;
  }
}

Private void G992p3OvhMsgXmtSendSegFrame(void *gDslVars, g992p3SegFrameCtlStruct *pSegCtl, dslFrame *pFrame)
{
  int       len0, len;
  dslFrameBuffer  *pBuf;
  uchar     *pData;

  pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
  len0 = DslFrameBufferGetLength(gDslVars, pBuf);
  len  = DslFrameGetLength(gDslVars, pFrame);
  pSegCtl->segTotal = (len + (len0 + kG992p3OvhSegMsgMaxLen - 1)) / (len0 + kG992p3OvhSegMsgMaxLen);
  if (pSegCtl->segTotal > 8)
    return;
  pSegCtl->segId = pSegCtl->segTotal;

  pData = DslFrameBufferGetAddress (gDslVars, pBuf);
  pSegCtl->segFrBufCur = DslFrameGetNextBuffer(gDslVars, pBuf);

  pData = DslFrameBufferGetAddress (gDslVars, pSegCtl->segFrBufCur);
  DslFrameBufferSetAddress (gDslVars, &pSegCtl->segFrBuf, pData);
  DslFrameBufferSetLength (gDslVars, &pSegCtl->segFrBuf, 0);

  globalVar.txSegFrameCtl.segFrame = pFrame;
  G992p3OvhMsgXmtSendNextSeg(gDslVars, pSegCtl);
}

Private void G992p3OvhMsgXmtSendLongFrame(void *gDslVars, dslFrame *pFrame)
{
#ifdef USE_CXSY_OVH_MSG_WORKAROUND
  uchar* pData;
  dslFrameBuffer  *pBuf;
  pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
  pData=DslFrameGetNextBuffer(gDslVars, pBuf);
  if( (globalVar.setup & kG992p3ClearEocCXSYWorkaround) != 0 ) 
    pData[kG992p3CtrlField] = (pData[kG992p3CtrlField]&0xFE) | ((pData[kG992p3CtrlField]&0x02)>>1);
#endif
  if (DslFrameGetLength(gDslVars, pFrame) <= 1024) {
    G992p3OvhMsgXmtSendFrame(gDslVars, pFrame);
    return;
  }

  if (NULL != globalVar.txSegFrameCtl.segFrame) {
    __SoftDslPrintf(gDslVars, "G992p3OvhMsgXmtSendLongFrame: txSegFrameCtl BUSY, fr=0x%X\n", 0, (int) pFrame);
    return;
  }

  G992p3OvhMsgXmtSendSegFrame(gDslVars, &globalVar.txSegFrameCtl, pFrame);
}

Private void G992p3OvhMsgXmtSendRsp(void *gDslVars, int rspLen)
{
#ifdef USE_CXSY_OVH_MSG_WORKAROUND
  if( (globalVar.setup & kG992p3ClearEocCXSYWorkaround) != 0 )
    globalVar.txRspBuf[kG992p3CtrlField] |= 0x01;
#endif
  if (rspLen != -1)
    DslFrameBufferSetLength (gDslVars, &globalVar.txRspFrBuf, rspLen);
  DslFrameInit (gDslVars, &globalVar.txRspFrame);
  if (rspLen < 1020) {
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txRspFrame, &globalVar.txRspFrBuf);
    G992p3OvhMsgXmtSendFrame(gDslVars, &globalVar.txRspFrame);
#ifdef G992P3_DBG_PRINT
    __SoftDslPrintf(gDslVars,"G992p3OvhMsgXmtSendRsp 0 : txRspFrame=0x%X, txRspFrBuf=0x%X , rspLen %d, bufcnt %d\n", 
            0, &globalVar.txRspFrame, &globalVar.txRspFrBuf, rspLen, globalVar.txRspFrame.bufCnt);
#endif
  }
  else {
    DslFrameBufferSetLength (gDslVars, &globalVar.txRspFrBuf, 4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txRspFrame, &globalVar.txRspFrBuf);

    DslFrameBufferSetAddress (gDslVars, &globalVar.txRspFrBuf1, globalVar.txRspBuf + 4);
    DslFrameBufferSetLength (gDslVars, &globalVar.txRspFrBuf1, rspLen - 4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txRspFrame, &globalVar.txRspFrBuf1);
    G992p3OvhMsgXmtSendLongFrame(gDslVars, &globalVar.txRspFrame);
#ifdef G992P3_DBG_PRINT
  __SoftDslPrintf(gDslVars,"G992p3OvhMsgXmtSendRsp 1 : txRspFrame=0x%X, txRspFrBuf=0x%X , rspLen %d, bufcnt %d\n", 
          0, &globalVar.txRspFrame, &globalVar.txRspFrBuf, rspLen, globalVar.txRspFrame.bufCnt);
#endif
  }
}

Private void G992p3OvhMsgRcvSegXmtSendRsp(void *gDslVars, g992p3RcvSegFrameCtlStruct *pSegFrameCtl)
{
#ifdef G992P3_DBG_PRINT
  __SoftDslPrintf(gDslVars,"G992p3OvhMsgRcvSegXmtSendRsp: pSegFrameCtl=0x%X, txRspFrame=0x%X , txRspFrBuf=0x%X, bufcnt %d\n", 
            0, pSegFrameCtl, &pSegFrameCtl->txRspFrame, &pSegFrameCtl->txRspFrBuf, pSegFrameCtl->txRspFrame.bufCnt);
#endif
  DslFrameBufferSetLength (gDslVars, &pSegFrameCtl->txRspFrBuf, kG992p3OvhMsgMaxSegRspSize);
  DslFrameInit (gDslVars, &pSegFrameCtl->txRspFrame);
  DslFrameEnqueBufferAtBack (gDslVars, &pSegFrameCtl->txRspFrame, &pSegFrameCtl->txRspFrBuf);
  G992p3OvhMsgXmtSendFrame(gDslVars, &pSegFrameCtl->txRspFrame);
}

Private void G992p3OvhMsgXmtTransferOLRCmd(void * gDslVars, dslFrame * pFrame)
{
  G992p3OvhMsgXmtSendLongFrame(gDslVars, pFrame);

  G992p3FrameBitSet(gDslVars, pFrame, kFrameSetCmdTime);
}

Private void G992p3OvhMsgXmtTransferCmd(void *gDslVars, dslFrame *pFrame)
{
  G992p3OvhMsgXmtSendLongFrame(gDslVars, pFrame);

  G992p3FrameBitSet(gDslVars, pFrame, kFrameSetCmdTime);
  globalVar.timeCmdOut = globalVar.timeMs;
}

Private uchar G992p3OvhMsgGetCmdNum(void *gDslVars, uchar *msgHdr, Boolean bNewCmd)
{
  int   n = msgHdr[kG992p3AddrField] & kG992p3PriorityMask;
  
  if( (globalVar.setup & kG992p3ClearEocWorkaround) != 0 )
    n = 0;

  if (bNewCmd)
    globalVar.txCmdMsgNum[n] ^= 1;
  return globalVar.txCmdMsgNum[n];
}

Private void G992p3OvhMsgXmtResendCmd(void *gDslVars)
{
  if (!G992p3OvhMsgXmtCmdBusy()) {
    if( (globalVar.setup & kG992p3ClearEocWorkaround) != 0 ) {
      dslFrameBuffer      *pBuf;
      uchar         *pData;

      pBuf  = DslFrameGetFirstBuffer(gDslVars, globalVar.lastTxCmdFrame);
      pData = DslFrameBufferGetAddress(gDslVars, pBuf);
      pData[kG992p3CtrlField] = kG992p3Cmd | G992p3OvhMsgGetCmdNum(gDslVars, pData, true);
    }
    
    globalVar.cmdTryCnt++;
    __SoftDslPrintf(gDslVars, "G992p3OvhMsgXmtResendCmd: cnt=%d, frame=0x%X", 0, globalVar.cmdTryCnt, globalVar.lastTxCmdFrame);
    G992p3OvhMsgXmtTransferCmd(gDslVars, globalVar.lastTxCmdFrame);
  }
}

Private Boolean G992p3OvhMsgXmtSendCmd(
  void  *gDslVars, 
  uchar *pData0, 
  ulong cmdLen0, 
  uchar *pData1, 
  ulong cmdLen1, 
  Boolean bNewCmd,
  Boolean bWaitRsp
  )
{
  int   i;

  if (G992p3OvhMsgXmtCmdBusy())
    return false;

  if (bWaitRsp) {
    globalVar.txFlags |= kTxCmdWaitingAck;
    if (G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc))
      G992p3OvhMsgCompleteClearEocFrame(gDslVars, globalVar.lastTxCmdFrame);
    globalVar.lastTxCmdFrame = &globalVar.txCmdFrame;
    if (bNewCmd)
      globalVar.cmdTryCnt = 1;
  }
  else
    globalVar.txFlags &= ~kTxCmdWaitingAck;
  G992p3FrameBitClear(gDslVars, globalVar.lastTxCmdFrame, kFramePollCmd);

  globalVar.txCmdBuf[kG992p3AddrField] = pData0[kG992p3AddrField];
  globalVar.txCmdBuf[kG992p3CtrlField] = kG992p3Cmd | G992p3OvhMsgGetCmdNum(gDslVars, pData0, bNewCmd);
  for (i = kG992p3CmdCode; i < cmdLen0; i++)
    globalVar.txCmdBuf[i] = pData0[i];

  DslFrameInit (gDslVars, &globalVar.txCmdFrame);

  if ((cmdLen0 + cmdLen1) <= 1024) {
    DslFrameBufferSetLength (gDslVars, &globalVar.txCmdFrBuf0, cmdLen0);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txCmdFrame, &globalVar.txCmdFrBuf0);
  }
  else {
    DslFrameBufferSetLength (gDslVars, &globalVar.txCmdFrBuf0, 4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txCmdFrame, &globalVar.txCmdFrBuf0);
    DslFrameBufferSetLength (gDslVars, &globalVar.txCmdFrBuf0a, cmdLen0-4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txCmdFrame, &globalVar.txCmdFrBuf0a);
    if (bWaitRsp)
      G992p3FrameBitClear(gDslVars, &globalVar.txCmdFrame, kFrameCmdAckPending);
    globalVar.txFlags &= ~kTxCmdWaitingAck;
  }

  if (cmdLen1 != 0) {
    DslFrameBufferSetAddress (gDslVars, &globalVar.txCmdFrBuf1, pData1);
    DslFrameBufferSetLength (gDslVars, &globalVar.txCmdFrBuf1, cmdLen1);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txCmdFrame, &globalVar.txCmdFrBuf1);
  }

  G992p3OvhMsgXmtTransferCmd(gDslVars, &globalVar.txCmdFrame);
  return true;
}

Private Boolean G992p3OvhMsgXmtSendOLRCmd(
  void  *gDslVars, 
  uchar *pData0, 
  ulong cmdLen0, 
  uchar *pData1, 
  ulong cmdLen1, 
  Boolean bNewCmd
  )
{
  int   i;

  if (G992p3OvhMsgXmtOLRCmdBusy())
    return false;

  globalVar.txOLRCmdBuf[kG992p3AddrField] = pData0[kG992p3AddrField];
  globalVar.txOLRCmdBuf[kG992p3CtrlField] = kG992p3Cmd | G992p3OvhMsgGetCmdNum(gDslVars, pData0, bNewCmd);
  for (i = kG992p3CmdCode; i < cmdLen0; i++)
    globalVar.txOLRCmdBuf[i] = pData0[i];

  DslFrameInit (gDslVars, &globalVar.txOLRCmdFrame);

  if ((cmdLen0 + cmdLen1) <= 1024) {
    DslFrameBufferSetLength (gDslVars, &globalVar.txOLRCmdFrBuf0, cmdLen0);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txOLRCmdFrame, &globalVar.txOLRCmdFrBuf0);
  }
  else {
    DslFrameBufferSetLength (gDslVars, &globalVar.txOLRCmdFrBuf0, 4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txOLRCmdFrame, &globalVar.txOLRCmdFrBuf0);
    DslFrameBufferSetLength (gDslVars, &globalVar.txOLRCmdFrBuf0a, cmdLen0-4);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txOLRCmdFrame, &globalVar.txOLRCmdFrBuf0a);
  }

  if (cmdLen1 != 0) {
    DslFrameBufferSetAddress (gDslVars, &globalVar.txOLRCmdFrBuf1, pData1);
    DslFrameBufferSetLength (gDslVars, &globalVar.txOLRCmdFrBuf1, cmdLen1);
    DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txOLRCmdFrame, &globalVar.txOLRCmdFrBuf1);
  }

  G992p3OvhMsgXmtTransferOLRCmd(gDslVars, &globalVar.txOLRCmdFrame);
  return true;
}

Private void G992p3OvhMsgSendClearEocCmd(void *gDslVars, dslFrame *pFrame)
{
  dslFrameBuffer      *pBuf;
  uchar         *eocHdr;

  pBuf   = DslFrameGetFirstBuffer(gDslVars, pFrame);
  eocHdr = DslFrameBufferGetAddress(gDslVars, pBuf);

  eocHdr[kG992p3AddrField] = kG992p3PriorityNormal;
  eocHdr[kG992p3CtrlField] = kG992p3Cmd | G992p3OvhMsgGetCmdNum(gDslVars, eocHdr, true);
  eocHdr[kG992p3CmdCode] = kG992p3OvhMsgCmdClearEOC;
  eocHdr[kG992p3CmdSubCode] = 1;

  globalVar.txFlags |= kTxCmdWaitingAck;
  globalVar.lastTxCmdFrame = pFrame;
  globalVar.cmdTryCnt = 1;
  G992p3FrameBitSet(gDslVars, pFrame, kFrameClearEoc);
  G992p3OvhMsgXmtTransferCmd(gDslVars, pFrame);
}

Private void G992p3OvhMsgCompleteClearEocFrame(void *gDslVars, dslFrame *pFrame)
{
  G992p3FrameBitClear(gDslVars, pFrame, kFrameClearEoc);
  (*globalVar.txSendCompletePtr)(gDslVars, NULL, 0, pFrame);
  globalVar.lastTxCmdFrame = &globalVar.txCmdFrame;
  __SoftDslPrintf(gDslVars, "G992p3OvhMsgCompleteClearEocFrame: pFrame=0x%X", 0, pFrame);
}

Public Boolean G992p3OvhMsgSendClearEocFrame(void *gDslVars, dslFrame *pFrame)
{
  dslFrame **pFr;

  if (CircBufferGetWriteAvail(&globalVar.txClEocFrameCB) < sizeof(dslFrame*))
    return false;

  pFr = CircBufferGetWritePtr(&globalVar.txClEocFrameCB);
  *pFr = pFrame;
  CircBufferWriteUpdate(&globalVar.txClEocFrameCB, sizeof(dslFrame*));
  return true;
}

Public void G992p3OvhMsgTimer(void *gDslVars, long timeMs)
{
	dslStatusStruct	status;
	int				i;
	
	if (!ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
		return;

	globalVar.timeMs += timeMs;
	globalVar.cmdTime += timeMs;
	if(globalVar.PLNActiveFlag==1)
		globalVar.PLNElapsedTimeMSec+=timeMs;
	if (!AdslMibIsLinkActive(gDslVars))
		return;

	/* Handle pending transition to L3 */
	if ((globalVar.txFlags & kTxCmdL3RspWait) && 
		((globalVar.timeMs - globalVar.timeRspOut) > 1000)) {
		status.code = kDslConnectInfoStatus;
		status.param.dslConnectInfo.code  = kG992p3PwrStateInfo;
		status.param.dslConnectInfo.value = 3;
		(*globalVar.statusHandlerPtr)(gDslVars, &status);
		globalVar.txFlags &= ~kTxCmdL3RspWait;
	}

	/* Handle rcv segment timeout */
	if( globalVar.olrSegmentSerialNum && ((globalVar.timeMs - globalVar.lastOlrSegmentRcvTime) > 1000) ) {
		G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.g992P3OlrFrameList);
		__SoftDslPrintf(gDslVars, "G992p3OvhMsg Timeout: waiting for OLR segment number=%d", 0, globalVar.olrSegmentSerialNum);
		globalVar.olrSegmentSerialNum=0;
	}
	
	for(i=0; i < 3; i++) {
		if( globalVar.segCmd[i].segId && ((globalVar.timeMs -globalVar.segCmd[i].lastSegRcvTime) > 1000) ) {
			G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.segCmd[i].segFrameList);
			__SoftDslPrintf(gDslVars, "G992p3OvhMsg Timeout: CMD PRI%d waiting for segment number=%d", 0, i, globalVar.segCmd[i].segId-1);
			globalVar.segCmd[i].segId = 0;
		}
		if( globalVar.segRsp[i].segId && ((globalVar.timeMs -globalVar.segRsp[i].lastSegRcvTime) > 1000) ) {
			G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.segRsp[i].segFrameList);
			__SoftDslPrintf(gDslVars, "G992p3OvhMsg Timeout: RSP PRI%d waiting for segment number=%d", 0, i, globalVar.segRsp[i].segId-1);
			globalVar.segRsp[i].segId = 0;
		}
	}
	
  if (NULL != globalVar.txSegFrameCtl.segFrame) {
    if (G992p3FrameIsBitSet(gDslVars, globalVar.txSegFrameCtl.segFrame, kFrameNextSegPending))
    G992p3OvhMsgXmtSendNextSeg(gDslVars, &globalVar.txSegFrameCtl);
    else if ((globalVar.timeMs - globalVar.txSegFrameCtl.timeSegOut) > 1000) {
    if (globalVar.txSegFrameCtl.tryCnt >= 3)
      globalVar.txSegFrameCtl.segFrame = NULL;
    else
      G992p3OvhMsgXmtSendCurrSeg(gDslVars, &globalVar.txSegFrameCtl);
    }
    return;
  }

  if (globalVar.txFlags & kTxCmdWaitingAck) {
    int tmp=0;
    
    if( ((globalVar.setup & kG992p3ClearEocWorkaround) != 0) &&
         !G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc) &&
         (CircBufferGetReadAvail(&globalVar.txClEocFrameCB) > 0) )  {
      tmp=globalVar.timeCmdOut;
      globalVar.timeCmdOut = 0;
      globalVar.cmdTryCnt = 3;
    }
    
    if ((globalVar.timeMs - globalVar.timeCmdOut) > 1000) {
      if (globalVar.cmdTryCnt >= 3) {
        if(!tmp)
          __SoftDslPrintf(gDslVars, "G992p3OvhMsg Timeout: cnt=%d, frame=0x%X", 0, globalVar.cmdTryCnt, globalVar.lastTxCmdFrame);
        else {
          __SoftDslPrintf(gDslVars, "G992p3OvhMsg Terminate restransmit of frame=0x%X", 0, globalVar.lastTxCmdFrame);
          globalVar.timeCmdOut = tmp;         
        }
        
        globalVar.cmdTryCnt = 0;
        globalVar.txFlags &= ~kTxCmdWaitingAck;
        if (G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc))
          G992p3OvhMsgCompleteClearEocFrame(gDslVars, globalVar.lastTxCmdFrame);

        if (globalVar.txFlags & kTxCmdL3WaitingAck) {
          status.code = kDslConnectInfoStatus;
          status.param.dslConnectInfo.code  = kG992p3PwrStateInfo;
          status.param.dslConnectInfo.value = 3;
          (*globalVar.statusHandlerPtr)(gDslVars, &status);
          globalVar.txFlags &= ~kTxCmdL3WaitingAck;
        }
          
        globalVar.txFlags &= ~kTxCmdL0WaitingAck;

        if (G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFramePollCmd)
          && ((ulong) -1 != globalVar.cmdNum && !(globalVar.pollCmd[globalVar.cmdNum].cmdFlags & kPollCmdStayActive))) {
          __SoftDslPrintf(gDslVars, "G992p3OvhMsg (TO) Disable command num=%d", 0, globalVar.cmdNum);
          globalVar.pollCmd[globalVar.cmdNum].cmdFlags &= ~kPollCmdActive;
#if defined(CONFIG_VDSL_SUPPORTED)
          if (globalVar.cmdNum==kPMDVectorBlockReadCmd)
          	G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,0);
#endif
        }
      }
      else 
      {
        G992p3OvhMsgXmtResendCmd(gDslVars);
      }
    }

    return;
  }

  if (!G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc) &&
    (CircBufferGetReadAvail(&globalVar.txClEocFrameCB) > 0)) {
    dslFrame *pFrame, **pFr;

    pFr = CircBufferGetReadPtr(&globalVar.txClEocFrameCB);
    pFrame = *pFr;
    CircBufferReadUpdate(&globalVar.txClEocFrameCB, sizeof(dslFrame*));
    G992p3OvhMsgSendClearEocCmd(gDslVars, pFrame);
    return;
  }

#ifdef G992P3_POLL_OVH_MSG
  if (((globalVar.timeMs - globalVar.cmdPollingStamp) > 5000) && ((globalVar.timeMs - globalVar.timeCmdOut) > 200)) {
    g992p3PollCmdStruct   *pCmd;
    ulong         lastCmd, cmd;

    cmd = globalVar.cmdNum;
    if (++cmd >= globalVar.pollCmdNum)
      cmd = 0;
    lastCmd = cmd;
    pCmd = &globalVar.pollCmd[cmd];

    while (!((pCmd->cmdFlags & kPollCmdActive) && ((globalVar.timeMs - pCmd->tmLastSent) > pCmd->tmPeriod))) {
      if (++cmd >= globalVar.pollCmdNum)
        cmd = 0;
      if (cmd == lastCmd) {
        cmd = (ulong) -1;
        break;
      }
      pCmd = &globalVar.pollCmd[cmd];
    }
    globalVar.cmdNum = cmd;
    if (((ulong) -1 != cmd) && G992p3OvhMsgXmtSendCmd(gDslVars, pCmd->cmd,pCmd->len, NULL,0, true,true)) {
      G992p3FrameBitSet(gDslVars, globalVar.lastTxCmdFrame, kFramePollCmd);
      pCmd->tmLastSent = globalVar.timeMs;
    }
  }
#endif
}
  
Public int G992p3OvhMsgSendCompleteFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
  Boolean   bSeg;

  if (!ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
    return 0;
#ifdef G992P3_DBG_PRINT
  __SoftDslPrintf(gDslVars, " G992p3OvhMsgSendCompleteFrame(%s): txFlags=0x%X, pVc=0x%X mid=0x%X pFr=0x%X\n", 0, 
     (pFrame == &globalVar.txCmdFrame) ? "CMD" : 
    ((pFrame == &globalVar.txRspFrame) ? "RSP" : 
    ((pFrame == &globalVar.txPwrRspFrame) ? "RSPPWR" : 
    "UNKNOWN")),
    globalVar.txFlags, (int) pVc, mid, (int) pFrame);
#endif
  G992p3ReleaseFrame (gDslVars, pFrame);

  if ((bSeg = G992p3FrameIsBitSet(gDslVars, pFrame, kFrameSetSegTime))) {
    G992p3FrameBitClear(gDslVars, pFrame, kFrameSetSegTime);
    globalVar.txSegFrameCtl.timeSegOut = globalVar.timeMs;
  }
  if (G992p3FrameIsBitSet(gDslVars, pFrame, kFrameSetCmdTime)) {
    if (!bSeg)
      G992p3FrameBitClear(gDslVars, pFrame, kFrameSetCmdTime);
    globalVar.timeCmdOut = globalVar.timeMs;
  }

  return 1;
}

#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
static int compactedToneIndex(int realToneIndex,FourBandsDescriptor *toneDescriptor)
{
  int compactedIx;
  int nrOfTones=0;
  int k=0;
  
  while ((realToneIndex>toneDescriptor->toneGroups[k].endTone)&&
         (k<toneDescriptor->noOfToneGroups)) 
  {
    nrOfTones+=toneDescriptor->toneGroups[k].endTone-toneDescriptor->toneGroups[k].startTone+1;
    k++;
  }


  if ((k==toneDescriptor->noOfToneGroups)||
      (realToneIndex>toneDescriptor->toneGroups[k].endTone)||
      (realToneIndex<toneDescriptor->toneGroups[k].startTone)) 
    return -1;
  else
  {
    compactedIx=nrOfTones+realToneIndex-toneDescriptor->toneGroups[k].startTone;
    ASSERT(compactedIx>=0);
    return (compactedIx);
  }
}

static int genConversionInfo(void *gDslVars,FourBandsDescriptor *rxband, FourBandsDescriptor *vectband, ConversionInfo *info)
{
  FourBandsDescriptor mergedband;
  int k1=0,k2=0,kmerge=-1,k;
  int S1,E1,S2,E2;

  __SoftDslPrintf(gDslVars,"DRV VECT: RXBAND  : nGroup=%2d   Firstband = %d-%d  ", 0, rxband->noOfToneGroups, rxband->toneGroups[0].startTone,  rxband->toneGroups[0].endTone);
  __SoftDslPrintf(gDslVars,"DRV VECT: VECTBAND: nGroup=%2d   Firstband = %d-%d  ", 0, rxband->noOfToneGroups, vectband->toneGroups[0].startTone,vectband->toneGroups[0].endTone);

  /* First merged the bandplan */
  while((k1<rxband->noOfToneGroups) && (k2<vectband->noOfToneGroups))
  {
    S1 = rxband->toneGroups[k1].startTone;
    E1 = rxband->toneGroups[k1].endTone;
	vectband->toneGroups[k2].startTone = (unsigned short)vectband->toneGroups[k2].startTone;
    vectband->toneGroups[k2].endTone   = (unsigned short)vectband->toneGroups[k2].endTone;
    S2 = vectband->toneGroups[k2].startTone;
    E2 = vectband->toneGroups[k2].endTone;

    if(S1<=S2)
    {
      k1++;
      if(S2<=E1)
      {
        k2++;
        kmerge++;
        mergedband.toneGroups[kmerge].startTone=S2;
        mergedband.toneGroups[kmerge].endTone=(E1<=E2) ? E1 : E2;
      }
    }
    else
    {
      k2++;
      if(S1<=E2)
      {
        k1++;
        kmerge++;
        mergedband.toneGroups[kmerge].startTone=S1;
        mergedband.toneGroups[kmerge].endTone = (E1<=E2) ? E1 : E2;
      }
    }
  }

  if(kmerge==-1)
  {
    return 1;
  }
  
  mergedband.noOfToneGroups = kmerge+1;
  info->nBands = mergedband.noOfToneGroups;  
  for(k=0;k<info->nBands;k++)
  {
    int startTone = mergedband.toneGroups[k].startTone;
    int endTone   = mergedband.toneGroups[k].endTone;
    info->infoBand[k].startRx   = compactedToneIndex(startTone,rxband);
#ifndef G993P5_OVERHEAD_MESSAGING
    info->infoBand[k].startVect = compactedToneIndex(startTone,vectband);\
#else
    info->infoBand[k].nSkipped  = startTone-vectband->toneGroups[k].startTone; /* in future implementation should always be 0 */
#endif
    info->infoBand[k].nSamples = endTone-startTone+1;
#ifdef G993P5_OVERHEAD_MESSAGING
    info->infoBand[k].nTonesInBand = vectband->toneGroups[k].endTone-vectband->toneGroups[k].startTone+1;
    __SoftDslPrintf(gDslVars,"DRV VECT: ConvInfo startRx: %d, nSkipped: %d",0,info->infoBand[k].startRx,info->infoBand[k].nSkipped);
    __SoftDslPrintf(gDslVars,"DRV VECT: ConvInfo nSamples: %d, nTonesInBand: %d",0,info->infoBand[k].nSamples,info->infoBand[k].nTonesInBand);
#endif
  }
  
  return 0;
}
#ifdef G993P5_OVERHEAD_MESSAGING
#define VDSL_MSG_SUCCESS                  0
#define VDSL_MSG_UNKNOWN_MESSAGE          1
#define VDSL_MSG_HDLC_MSG_TOO_SHORT       2
#define VDSL_MSG_HDLC_MSG_TOO_LONG        3
#define VDSL_MSG_DECODE_ERROR             4
#define VDSL_MSG_INVALID_FIELD_VALUE      5
#define VDSL_MSG_MSG_NOT_SUPPORTED        6

#define TONE_GROUP_DESCRIPTOR_BYTES_PER_TONE_GROUP     3

/* Useful macros */
#define GET_BOTTOM_NIBBLE(x)                ( ( unsigned char ) ( (x) & 0xf ) )
#define GET_TOP_NIBBLE(x)                   ( ( unsigned char ) ( (x) >> 4 ) )

Private void assignTwoUint16sFrom24Bits ( unsigned short *pTarget1, unsigned short *pTarget2, const unsigned char *p3Bytes )
{
    *pTarget1 = p3Bytes[2] | ( GET_BOTTOM_NIBBLE ( p3Bytes[1] ) << 8 );
    *pTarget2 = GET_TOP_NIBBLE ( p3Bytes[1] ) | ( p3Bytes[0] << 4 );
}

Private unsigned short decodeToneGroupDescriptor ( const unsigned char ** ppHdlcMessage,
                                                   short *pRemainingHdlcMessageLength,
                                                   VdslMsgToneGroupDescriptor *pToneGroupDescriptor,
                                                   unsigned int maxNoToneGroups)
{
    int i;
    const unsigned char * pHdlcMessage = *ppHdlcMessage;
    unsigned short toneGroupLength;

    /* The ToneGroup descriptor is described as follows (called Bands descriptor in G.993.1 06/2004 page 76):
     *
     * Octet                   Content of field
     * 1                       Number of bands to be described
     * 2-4                     Bits 0-11:  Start tone index of band 1
     *                         Bits 12-23: Ending tone index of band 1
     * 5-7 (if applicable)     Bits 0-11:  Start tone index of band 2
     *                         Bits 12-23: Ending tone index of band 2
     * etc.                    etc.
     */

    pToneGroupDescriptor->noOfToneGroups = *pHdlcMessage++;

    if ( pToneGroupDescriptor->noOfToneGroups > maxNoToneGroups )
    {
        /* Too many tone groups */
        return VDSL_MSG_DECODE_ERROR;
    }


    /* Calculate the tone group descriptor length */
    toneGroupLength = 1 + ( pToneGroupDescriptor->noOfToneGroups * TONE_GROUP_DESCRIPTOR_BYTES_PER_TONE_GROUP );

    /* Check that there are enough bytes remaining in the HDLC message */
    if ( *pRemainingHdlcMessageLength < toneGroupLength )
    {
        /* Not enough bytes in message - return an error */
        return VDSL_MSG_HDLC_MSG_TOO_SHORT;
    }

    /* Loop through the HDLC message extracting the start and end tone indices */
    for ( i = 0; i < pToneGroupDescriptor->noOfToneGroups; i++ )
    {
        assignTwoUint16sFrom24Bits ( &pToneGroupDescriptor->toneGroups[i].startTone,
                                     &pToneGroupDescriptor->toneGroups[i].endTone,
                                     pHdlcMessage );

        pHdlcMessage += TONE_GROUP_DESCRIPTOR_BYTES_PER_TONE_GROUP;
    }

    /* Used up some HDLC bytes */
    ( *pRemainingHdlcMessageLength ) -= toneGroupLength;
    *ppHdlcMessage = pHdlcMessage;

    return VDSL_MSG_SUCCESS;
}

void decodeErrorReportConfig(void *gDslVars,const unsigned char **ppHdlcMessage, VectBackChannelParams *pVectBackChannel)
{
  const unsigned char *pHdlcMessage = *ppHdlcMessage;
  int N_band  = pHdlcMessage[0]>>4;
  int padding = pHdlcMessage[0]&(1<<3);
  int format  = pHdlcMessage[0]&0x3;
  int F_sub,L_w,B_min,B_max;
  
  /* default the back channel is inactive */
  pVectBackChannel->lastReportedBand=-1;

  pHdlcMessage++;

  /* Decode the first band */
  F_sub   = pHdlcMessage[0]>>4;
  L_w     = pHdlcMessage[0]&0xF;
  B_min   = pHdlcMessage[1]>>4;
  B_max   = pHdlcMessage[1]&0xF;

  /* Check that the config is supported */
  if(
     (N_band>0)&&(N_band<=4) &&                              /* No more than 4 bands is supported */
     (padding==(1<<3)) &&                                    /* Padding must be set to 1          */
     (B_min==0) && (B_max==11) &&                          /* Only supported Bmin and Bmax */
     (((format==0)&&(L_w==8))||((format==1)&&(L_w==6)))      /* Only two supported formats */ 
     )
  {
    int lastReportedBand=0;
    int k;
    pVectBackChannel->floatFormat = format;
    pVectBackChannel->log2M       = F_sub;
    pVectBackChannel->nBands      = N_band;
    
    /* Check that all bands have the same format and which band is the last reported one */
    for(k=1;k<N_band;k++)
    {
      int L_new = pHdlcMessage[2*k]&0xF;
      if(L_new!=0)
      {
        /* Compare the format and check that there is no other bands with Lw=0
           before that */
        if((pHdlcMessage[0]!=pHdlcMessage[2*k])||(pHdlcMessage[1]!=pHdlcMessage[2*k+1])||(lastReportedBand!=(k-1)))
        {
          /* Format has changed disable the back channel or a band has been
             switch off in the middle of the band */
          lastReportedBand=-1;
          continue;
        }
        lastReportedBand++;
      }
    }
    pVectBackChannel->lastReportedBand=lastReportedBand;    
  }
  
  pHdlcMessage+=2*N_band;
  
  __SoftDslPrintf(gDslVars,"DRV VECT: Back channel log2M: %d, isFloat: %d",0,pVectBackChannel->log2M,pVectBackChannel->floatFormat);
  __SoftDslPrintf(gDslVars,"DRV VECT: Back channel lastReportedBand: %d",0,pVectBackChannel->lastReportedBand);
  
  *ppHdlcMessage = pHdlcMessage;
}

Private void printVectSM(void *gDslVars, VectoringStateMachine *vectSM)
{
  __SoftDslPrintf(gDslVars,"DRV VECT:state       :%d",0,vectSM->state       );
  __SoftDslPrintf(gDslVars,"DRV VECT:syncCounter :%d",0,vectSM->syncCounter );
  __SoftDslPrintf(gDslVars,"DRV VECT:startSync   :%d",0,vectSM->startSync   );
  __SoftDslPrintf(gDslVars,"DRV VECT:log2M       :%d",0,vectSM->log2M       );
  __SoftDslPrintf(gDslVars,"DRV VECT:lineId      :%d",0,vectSM->lineId      );
  __SoftDslPrintf(gDslVars,"DRV VECT:conv.nBands :%d",0,vectSM->convInfo.nBands);
  /* ConversionInfo      convInfo; */
}

Private int createVectorReply(void *gDslVars, uchar* buffer, int buffer_length)
{
  ulong	mibLen;
  /* LineMgr *lineMgr    = getLineContext()->lineMgr; */
  adslMibInfo *pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
  VectoringStateMachine *vectSM = &(pMib->vectSM);
  VectData              *lineMgr = &(pMib->vectData);
  int nack = true;
  int facilityCmd = buffer[2];
  int cmdId = buffer[3];
  int reportDisableByCo = false;

  if((facilityCmd==VECTORMGR_HIGH_CMD_ID) && (cmdId==VECTORMGR_START_DUMP_CMD_ID))
  {
    /* Decode the message */
    VectBackChannelParams       vectBackChannel;
    VdslVectoredBandsDescriptor vectBands;
    const unsigned char *payload =  &buffer[4];
    short remainingLength = buffer_length-4;
    unsigned short firstSSC;
    PilotSequence  *pilotSequence = &lineMgr->phyData.pilotSequence;

    nack=false;
   
    firstSSC = (payload[0]<<8) + payload[1];
    payload+=2;

    if(vectSM->state==VECT_WAIT_FOR_CONFIG)
    {
      /* Initialize the first sync */
      pilotSequence->firstSync = firstSSC;

      __SoftDslPrintf(gDslVars,"DRV VECT: init firstSync=%d",0, firstSSC);
      /* Instruction below must be done in foreground */
      vectSM->syncCounter = (/* PHY does this: vectSM->syncCounter+*/pilotSequence->firstSync)&(PILOT_SEQUENCE_LEN-1);
      lineMgr->phyData.syncOffset  = (lineMgr->phyData.syncOffset+pilotSequence->firstSync)&(PILOT_SEQUENCE_LEN-1);
    } 
    
    if(payload[0]==0)
    {
      nack = true;
      reportDisableByCo = true;
      __SoftDslPrintf(gDslVars,"DRV VECT: report disabled by CO=0x%x",0, payload[0]);
    }
    else
    {
      if((payload[0]!=1)||(payload[1]!=0)||(payload[2]!=0))
      {
          __SoftDslPrintf(gDslVars,"DRV VECT: payload=0x%x 0x%x 0x%x: %d ",0, payload[0], payload[1], payload[2]);
          nack = true;
      }
      
      payload+=3; /* Skip m, k as not implemented */
      
      remainingLength-=5;
      
      if(!nack)
      {
        /* Decode the bandplan */
        decodeToneGroupDescriptor(&payload, 
                                  &remainingLength, 
                                  (VdslMsgToneGroupDescriptor*) &vectBands,
                                  8);
        
        if(vectBands.noOfTonesGroups>4)
        {
            __SoftDslPrintf(gDslVars,"DRV VECT: Too many vectoring bands: %d ",0, vectBands.noOfTonesGroups);
            nack=true;
        }
        else
        {
          /* Convert to internal format */
          ConversionInfo *info = &vectSM->convInfo;
          nack = genConversionInfo(gDslVars, (FourBandsDescriptor*)&pMib->dsPhyBandPlan,
                                   (FourBandsDescriptor*)&vectBands,
                                   info);      
          {
              /* Sanity check of the bandplan */
              int nTones=0,k;
              for(k=0;k<info->nBands;k++)
                  nTones+=info->infoBand[k].nTonesInBand;
              if(nTones>(MAX_VECT_TONES<<MIN_LOG2_SUB_SYNC_RX))
              {
                  __SoftDslPrintf(gDslVars,"DRV VECT: Too many tones in the vectoring bandplan: nTones=%d  MAX_VECT_TONES=%d MIN_LOG2_SUB_SYNC_RX=%d",0, nTones, MAX_VECT_TONES, MIN_LOG2_SUB_SYNC_RX);
                  nack=true;
              }
          }
        }
        
        if(!nack)
        {
          /* decode the configuration */
          decodeErrorReportConfig(gDslVars,&payload,&vectBackChannel);
          if(vectBackChannel.lastReportedBand==-1)
          {
              __SoftDslPrintf(gDslVars,"DRV VECT: lastReportedBand==-1",0);
              nack = true;
          }
        }
      }
    }

    if(nack)
      vectBackChannel.lastReportedBand=-1;
    
    /* Configure the vectoring state machine based on those */
    vectSM->startSync = 0xFFFF;
#if 0
    vectSM->lineId    = LineContext_getLineMgr()->pilotSequence.lineIdDs;
#endif
    vectSM->lastReportedBand = vectBackChannel.lastReportedBand;

    if(vectBackChannel.lastReportedBand != -1)
      vectSM->log2M    = vectBackChannel.log2M|(vectBackChannel.floatFormat<<7);

    printVectSM(gDslVars,vectSM);

    vectSM->state = VECT_WAIT_FOR_TRIGGER;
	{
	    uchar *pSharedMem;
	    int n = sizeof(VectoringStateMachine);
        dslCommandStruct		cmd;
	    pSharedMem = (uchar *)AdslCoreSharedMemAlloc(n);	/* AdslCoreSharedMemAlloc always return a valid address */
	    memcpy(pSharedMem,vectSM, n);
	    cmd.command = kDslSendEocCommand;
	    cmd.param.dslClearEocMsg.msgId = kDslVectoringStartDumpCmd;
	    cmd.param.dslClearEocMsg.msgType =n & kDslClearEocMsgLengthMask;
	    cmd.param.dslClearEocMsg.dataPtr = (char *)pSharedMem;
	    (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
	}

  }
 
  if((facilityCmd==VECTORMGR_LOW_CMD_ID)&&(cmdId==VECTORMGR_SET_MODE_CMD_ID))
  {
#if 0 
    if(buffer_length==4+sizeof(VectorMode))
    {

      TestConfigMessage * testConfig = CoreMgr_getTestConf();
      VectorMode *vectorMode    =  (VectorMode*)&buffer->data[4];
      int        disableBitSwap = (testConfig->disableBitSwap&0xFE);
      if ((vectorMode->disableRxBitSwap<=1) || (vectorMode->disableVN<=1))
      {
        /* Foreground section */
        lineMgr->transceiver.snrMode = (vectorMode->disableVN) ? 0 : lineMgr->transceiver.negotiatedSnrMode;
        testConfig->disableBitSwap   = disableBitSwap | vectorMode->disableRxBitSwap;
        /* end foreground section */
        nack  = false;
      }
    }
#else
        if(buffer_length==4+sizeof(VectorMode))
        {
          VectorMode *vectorMode    =  (VectorMode*)&buffer[4];
          unsigned char disableRxBitSwap = lineMgr->vectorMode.disableRxBitSwap;
		  unsigned char disableVN      = lineMgr->vectorMode.disableVN;
          
		  __SoftDslPrintf(gDslVars,"DRV VECT(VECTORMGR_SET_MODE_CMD_ID): disableBS=0x%x disableVN=0x%x, MSG: disableBS=0x%x disableVN=0x%x direction=%d", 0,
						  disableRxBitSwap, disableVN, vectorMode->disableRxBitSwap, vectorMode->disableVN, vectorMode->direction);       

          if (vectorMode->disableRxBitSwap<=1)
			{
			  /* valid request */
				  dslCommandStruct		cmd;
				  cmd.command = kDslBitswapControlCmd;
				  if (vectorMode->disableRxBitSwap)
				  cmd.param.value = kDslDisableRxOLR;
				  else
				  cmd.param.value = kDslEnableRxOLR;
				  (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				  lineMgr->vectorMode.disableRxBitSwap = vectorMode->disableRxBitSwap;
			}
		   if (vectorMode->disableVN<=1)
          {
			  /* valid request */
				  dslCommandStruct		cmd;
				  cmd.command = kDslBitswapControlCmd;
				  if (vectorMode->disableVN)
					  cmd.param.value = kDslBitswapDisableVN;
				  else
					  cmd.param.value = kDslBitswapEnableVN;
				  (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				  lineMgr->vectorMode.disableVN = vectorMode->disableVN;
			}
			
            nack  = false;
          }
#endif
  }

  if(nack)
  {
    buffer[3] = VECTORMGR_NACK_RESPONSE;
    if((facilityCmd==VECTORMGR_HIGH_CMD_ID) && (cmdId==VECTORMGR_START_DUMP_CMD_ID))
    { 
      buffer[4] = reportDisableByCo ? 0x02 : 0x01;
      buffer_length  = 5;
    }
    else
      buffer_length  = 4;
  }
  else
  {
    buffer[3] = VECTORMGR_ACK_RESPONSE;
    if((facilityCmd==VECTORMGR_HIGH_CMD_ID) && (cmdId==VECTORMGR_START_DUMP_CMD_ID))
    {
      buffer[4] = 0x00;
      buffer[5] = 0x00;
      buffer[6] = 0xC0;
      buffer[7] = 0x00;
      buffer_length  = 8;
    }
    else
      buffer_length  = 4;
  }

  return buffer_length; 
}
#else /* G993P5_OVERHEAD_MESSAGING */
Private int createVectorReply(void *gDslVars, uchar* buffer, int buffer_length)
{
  ulong	mibLen;
  /* LineMgr *lineMgr    = getLineContext()->lineMgr; */
  adslMibInfo *pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
  VectoringStateMachine *vectSM = &(pMib->vectSM);
  VectData              *lineMgr = &(pMib->vectData);
  int nack = true;

  switch(buffer[3])
  {
      case VECTORMGR_SET_PILOT_CMD_ID:
      {
		  int longSize = 4+sizeof(PilotSequence)+sizeof(FourBandsDescriptor)+sizeof(VceMacAddress);
		  int shortSize = 4+2+sizeof(FourBandsDescriptor)+sizeof(VceMacAddress);

        if ((buffer_length==longSize) || (buffer_length==shortSize))
        {
          ConversionInfo *info = &vectSM->convInfo;
		  PilotSequence  *pilotSequence = &lineMgr->phyData.pilotSequence;
		  int offsetForBandPlan = (buffer_length==longSize)?sizeof(PilotSequence):2;
          
		  __SoftDslPrintf(gDslVars,"DRV VECT(VECTORMGR_SET_PILOT_CMD_ID):     Buf: 0x%x 0x%x 0x%x 0x%x bp: 0x%x 0x%x 0x%x 0x%x", 0,  buffer[4], buffer[5], buffer[6], buffer[7], buffer[4+sizeof(PilotSequence)], buffer[4+sizeof(PilotSequence)+1], buffer[4+sizeof(PilotSequence)+2], buffer[4+sizeof(PilotSequence)+3]);
          nack = genConversionInfo(gDslVars,(FourBandsDescriptor*)&pMib->dsNegBandPlan, (FourBandsDescriptor*)&buffer[4+offsetForBandPlan], info);
          if(!nack)
          {
	        __SoftDslPrintf(gDslVars,"DRV VECT:     Check common bands: %d+%d = %d <> %d", 0, info->infoBand[info->nBands-1].startVect,  info->infoBand[info->nBands-1].nSamples, (info->infoBand[info->nBands-1].startVect+info->infoBand[info->nBands-1].nSamples), MAX_VECT_TONES);
            if((info->infoBand[info->nBands-1].startVect+
                info->infoBand[info->nBands-1].nSamples)>(MAX_VECT_TONES<<MIN_LOG2_SUB_SYNC_RX))
            {
              /* Sanity check of the bandplan */
              __SoftDslPrintf(gDslVars,"DRV VECT: Too many tones in the vectoring bandplan, startvect=%d nsamples=%d (%d>%d)",0,
							  info->infoBand[info->nBands-1].startVect, info->infoBand[info->nBands-1].nSamples,
							  info->infoBand[info->nBands-1].startVect+ info->infoBand[info->nBands-1].nSamples, MAX_VECT_TONES);
              nack=true;
            }
            else
            {
              /* Initialize the first sync and the pilot sequence */
			  if (buffer_length==longSize)
			  memcpy(&pilotSequence->bitsPattern,&buffer[6],sizeof(PilotSequence)-2);
			  else
				  memset(&pilotSequence->bitsPattern,0,sizeof(PilotSequence)-2);

			  pilotSequence->firstSync = (unsigned short)ReadLittleEndian16(&buffer[4]);

			  vectSM->syncCounter = (pilotSequence->firstSync)&(PILOT_SEQUENCE_LEN-1);
			  lineMgr->phyData.syncOffset  = (lineMgr->phyData.syncOffset+pilotSequence->firstSync)&(PILOT_SEQUENCE_LEN-1);

			  /* copying vectoring band plan (PHY will regen conversionInfo from physical band plan */
			  if (buffer_length==longSize)
			  memcpy(&lineMgr->phyData.vectoringBandPlan,&buffer[4+sizeof(PilotSequence)],sizeof(FourBandsDescriptor));
			  else
				  memcpy(&lineMgr->phyData.vectoringBandPlan,&buffer[6],sizeof(FourBandsDescriptor));

              vectSM->state = VECT_FULL;
			  {
				  uchar *pSharedMem;
				  int n = sizeof(VectDataPhy);
                  dslCommandStruct		cmd;
				  pSharedMem = (uchar *)AdslCoreSharedMemAlloc(n);	/* AdslCoreSharedMemAlloc always return a valid address */
				  memcpy(pSharedMem,&lineMgr->phyData, n);
				  cmd.command = kDslSendEocCommand;
				  cmd.param.dslClearEocMsg.msgId = kDslVectoringSetPilotCmd;
				  cmd.param.dslClearEocMsg.msgType =n & kDslClearEocMsgLengthMask;
				  cmd.param.dslClearEocMsg.dataPtr = (char *)pSharedMem;
				  (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
			  }
              /* Copy the VCE MAC address */
			  {
				  int offset;
				  if (buffer_length==longSize)
					  offset = 4+sizeof(PilotSequence)+sizeof(FourBandsDescriptor);
				  else
					  offset = 4+2+sizeof(FourBandsDescriptor);
				  memcpy(&lineMgr->macAddress,&buffer[offset],sizeof(VceMacAddress)-2);
				  offset += sizeof(VceMacAddress)-2;
				  lineMgr->macAddress.vlan = ReadLittleEndian16(&buffer[offset]);
			  }
            }
          }
		  else
			  __SoftDslPrintf(gDslVars,"DRV VECT: Invalid vectoring bandplan. No common bands",0);
        }
		else
			__SoftDslPrintf(gDslVars,"DRV VECT: Unexpected length: %d vs %d or %d",0, buffer_length, longSize, shortSize );
      }
      break;
      
      case VECTORMGR_START_DUMP_CMD_ID:
      {
        if(buffer_length==4+sizeof(StartVectorDump))
        {
          StartVectorDump *startVectorDump = (StartVectorDump*)&buffer[4];
#if 1
          unsigned short numTones   = ReadLittleEndian16(&buffer[12]);
          unsigned short numSymbol  = ReadLittleEndian16(&buffer[8]);
          unsigned short startSync  = ReadLittleEndian16(&buffer[6]);
          unsigned short startTone  = ReadLittleEndian16(&buffer[10]);
          unsigned short lineId     = ReadLittleEndian16(&buffer[14]);
#endif
          unsigned char log2M      = startVectorDump->log2M;
          unsigned char reservedA = startVectorDump->reservedA;
          unsigned char  reservedB = startVectorDump->reservedB; 
          unsigned char direction = startVectorDump->direction;
	      __SoftDslPrintf(gDslVars,"DRV VECT(VECTORMGR_START_DUMP_CMD_ID):   lineId    =%d, numSymbol = %d, startSync = %d, startTone = %d, numTones = %d, log2M=%d direction = %d",0, lineId, numSymbol, startSync, startTone, numTones, log2M, direction);

          if((vectSM->state!=VECT_WAIT_FOR_CONFIG)&&
             ((startTone&0x7)==0)&&
             (numTones<=(MAX_VECT_TONES)) && (log2M>0))
          {
            vectSM->startSync = startSync;
            vectSM->startTone = startTone;
            vectSM->numTones  = numTones;
            vectSM->numSymbol = numSymbol;
            vectSM->log2M     = log2M;
            vectSM->lineId    = lineId;
            vectSM->state     = VECT_WAIT_FOR_TRIGGER;

			{
			    uchar *pSharedMem;
			    int n = sizeof(VectoringStateMachine);
                dslCommandStruct		cmd;
			    pSharedMem = (uchar *)AdslCoreSharedMemAlloc(n);	/* AdslCoreSharedMemAlloc always return a valid address */
			    memcpy(pSharedMem,vectSM, n);
			    cmd.command = kDslSendEocCommand;
			    cmd.param.dslClearEocMsg.msgId = kDslVectoringStartDumpCmd;
			    cmd.param.dslClearEocMsg.msgType =n & kDslClearEocMsgLengthMask;
			    cmd.param.dslClearEocMsg.dataPtr = (char *)pSharedMem;
			    (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
			}
            /* Create reply */
            nack = false;
          }
		  else
		  {
		    __SoftDslPrintf(gDslVars,"DRV VECT(VECTORMGR_START_DUMP_CMD_ID): Command not passed, state=%d, lineId=%d numTones=%d log2M=%d startSync=%d numSymbols=%d reservedA=%d reservedB=%d direction=%d",0, vectSM->state, lineId, numTones,log2M,startSync,numSymbol,reservedA,reservedB,direction);
		  }
        }
      }      
      break;

  case VECTORMGR_SET_MODE_CMD_ID:
      {
        if(buffer_length==4+sizeof(VectorMode))
        {
          VectorMode *vectorMode    =  (VectorMode*)&buffer[4];
          unsigned char disableRxBitSwap = lineMgr->vectorMode.disableRxBitSwap;
		  unsigned char disableVN      = lineMgr->vectorMode.disableVN;
          
		  __SoftDslPrintf(gDslVars,"DRV VECT(VECTORMGR_SET_MODE_CMD_ID): disableBS=0x%x disableVN=0x%x, MSG: disableBS=0x%x disableVN=0x%x direction=%d", 0,
						  disableRxBitSwap, disableVN, vectorMode->disableRxBitSwap, vectorMode->disableVN, vectorMode->direction);       

          if (vectorMode->disableRxBitSwap<=1)
			{
			  /* valid request */
				  dslCommandStruct		cmd;
				  cmd.command = kDslBitswapControlCmd;
				  if (vectorMode->disableRxBitSwap)
				  cmd.param.value = kDslDisableRxOLR;
				  else
				  cmd.param.value = kDslEnableRxOLR;
				  (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				  lineMgr->vectorMode.disableRxBitSwap = vectorMode->disableRxBitSwap;
			}
		   if (vectorMode->disableVN<=1)
          {
			  /* valid request */
				  dslCommandStruct		cmd;
				  cmd.command = kDslBitswapControlCmd;
				  if (vectorMode->disableVN)
					  cmd.param.value = kDslBitswapDisableVN;
				  else
					  cmd.param.value = kDslBitswapEnableVN;
				  (*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				  lineMgr->vectorMode.disableVN = vectorMode->disableVN;
			}
			
            nack  = false;
          }
        }
      break;

      
      default:
        break;
  }

  if(nack)
	  return 1;
  else
	  return 0;
}
#endif /* G993P5_OVERHEAD_MESSAGING */
#endif

#define	CHECK_CMD_RSP(cmdCode)	if (((ulong) -1 == globalVar.cmdNum) ||((cmdCode) != pCmd->cmd[kG992p3CmdCode])) return true;

Private Boolean G992p3OvhMsgRcvProcessRsp(void *gDslVars, dslFrameBuffer *pBuf, uchar *pData, ulong rspLen)
{
	ulong	mibLen;
	adslMibInfo	*pMib;
	g992p3PollCmdStruct	*pCmd = &globalVar.pollCmd[globalVar.cmdNum];
	char		oidStr[] = { kOidAdslPrivate, 0 };
	short	*pToneData;
	uchar	*pMsg, *pMsgEnd, rspId;
	int	n, path0, path1;
	dslCommandStruct	dslCmd;
	
	path0 = XdslMibGetPath0MappedPathId(gDslVars, US_DIRECTION);
	path1 = XdslMibGetPath1MappedPathId(gDslVars, US_DIRECTION);
	
#ifdef G992P3_DBG_PRINT
	__SoftDslPrintf(gDslVars, "G992p3OvhMsgRcvProcessRsp: code=0x%X, subCode=0x%X rspLen=%d\n", 0,pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], rspLen);
#endif
	switch (pData[kG992p3CmdCode]) {
		case kG992p3OvhMsgCmdCntRead:
			CHECK_CMD_RSP(pData[kG992p3CmdCode]);
			if (!(pCmd->cmdFlags & kPollCmdOnce) && !(pCmd->cmdFlags & kPollCmdStayActive))
				pCmd->cmdFlags |= kPollCmdStayActive;
			
			mibLen = sizeof(adslMibInfo);
			pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
			if (((kG992p3OvhMsgCmdCntReadId | 0x80) == pData[kG992p3CmdSubCode]) ||
				((pMib->adslStat.fireStat.status & kFireUsEnabled) && ((kG992p3OvhMsgCmdCntReadId1| 0x80) == pData[kG992p3CmdSubCode]))) {
				adslConnectionDataStat		adslTxData[MAX_LP_NUM];
				adslPerfCounters		adslTxPerf;
				atmConnectionDataStat		atmTxData[MAX_LP_NUM];
				int				i = 0;
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
				if ( (48 != rspLen) && !(pMib->adslStat.fireStat.status & kFireUsEnabled) )
					break;
#elif defined(CONFIG_VDSL_SUPPORTED)
				if ( !XdslMibIsVdsl2Mod(gDslVars) && XdslMibIsAtmConnectionType(gDslVars) && (48 != rspLen) &&
					!((pMib->adslStat.fireStat.status & kFireUsEnabled) || XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) )
					break;
#else
				if ( XdslMibIsAtmConnectionType(gDslVars) && (48 != rspLen) &&
					!((pMib->adslStat.fireStat.status & kFireUsEnabled) || XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) )
					break;
#endif
				pMsg = pData + kG992p3CmdSubCode + 1;

				AdslMibByteClear(sizeof(adslTxData), (void *)&adslTxData[0]);
				AdslMibByteClear(sizeof(adslTxPerf), (void *)&adslTxPerf);
				AdslMibByteClear(sizeof(atmTxData), (void *)&atmTxData[0]);

				adslTxData[path0].cntRSCor = ReadCnt32(pMsg+i); i += 4;
				
				if(XdslMibIs2lpActive(gDslVars, US_DIRECTION)) {
					adslTxData[path1].cntRSCor = ReadCnt32(pMsg+i); i += 4;	/* US dual latency/G.inp is active */
				}
				
				adslTxData[path0].cntSFErr = ReadCnt32(pMsg+i); i += 4;
				
				if(XdslMibIs2lpActive(gDslVars, US_DIRECTION)) {
					adslTxData[path1].cntSFErr = ReadCnt32(pMsg+i); i += 4;
				}
				if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION) || XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
						pMib->adslStat.ginpStat.cntDS.rtx_tx = ReadCnt32(pMsg+i); i += 4;
					}
					if(XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
						pMib->adslStat.ginpStat.cntUS.rtx_c = ReadCnt32(pMsg+i); i += 4;
						pMib->adslStat.ginpStat.cntUS.rtx_uc = ReadCnt32(pMsg+i); i += 4;
					}
				}
				
				adslTxPerf.adslFECs  = ReadCnt32(pMsg+i); i += 4;
				adslTxPerf.adslESs   = ReadCnt32(pMsg+i); i += 4;
				adslTxPerf.adslSES   = ReadCnt32(pMsg+i); i += 4;
				adslTxPerf.adslLOSS  = ReadCnt32(pMsg+i); i += 4;
				adslTxPerf.adslUAS   = ReadCnt32(pMsg+i); i += 4;
				
				if(XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
					pMib->adslStat.ginpStat.cntUS.LEFTRS = ReadCnt32(pMsg+i); i += 4;
					pMib->adslStat.ginpStat.cntUS.errFreeBits = ReadCnt32(pMsg+i); i += 4;
					pMib->adslStat.ginpStat.cntUS.minEFTR = ReadCnt32(pMsg+i); i += 4;
				}
				if( XdslMibIsAtmConnectionType(gDslVars) ) {
					atmTxData[0].cntHEC    = ReadCnt32(pMsg+i); i += 4;
					atmTxData[0].cntCellTotal = ReadCnt32(pMsg+i); i += 4;
					atmTxData[0].cntCellData  = ReadCnt32(pMsg+i); i += 4;
					atmTxData[0].cntBitErrs   = ReadCnt32(pMsg+i); i += 4;
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION) &&
						!XdslMibIsGinpActive(gDslVars, US_DIRECTION) &&
						!XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
						/* Only applicable for real dual latency case. With G.inp enabled,
						TPS-TC counters are only on Bearer# 0; assuming it's only on path 0 */
						atmTxData[1].cntHEC    = ReadCnt32(pMsg+i); i += 4;
						atmTxData[1].cntCellTotal = ReadCnt32(pMsg+i); i += 4;
						atmTxData[1].cntCellData  = ReadCnt32(pMsg+i); i += 4;
						atmTxData[1].cntBitErrs   = ReadCnt32(pMsg+i); i += 4;
					}
				}
				
				AdslMibUpdateTxStat(gDslVars, &adslTxData[0], &adslTxPerf, &atmTxData[0]);
				if( (kG992p3OvhMsgCmdCntReadId1| 0x80) == pData[kG992p3CmdSubCode] ) {
					pMib->adslStat.fireStat.reXmtRSCodewordsRcvedUS = ReadCnt32(pMsg+i); i += 4;
					pMib->adslStat.fireStat.reXmtUncorrectedRSCodewordsUS= ReadCnt32(pMsg+i); i += 4;
					pMib->adslStat.fireStat.reXmtCorrectedRSCodewordsUS= ReadCnt32(pMsg+i); i += 4;
				}
			}
			break;
		case kG992p3OvhMsgCmdInventory:
			CHECK_CMD_RSP(pData[kG992p3CmdCode]);
			if (pCmd->cmdFlags & kPollCmdOnce) {
				pCmd->cmdFlags &= ~kPollCmdActive;
				__SoftDslPrintf(gDslVars, "G992p3OvhMsg (ONCE) Disable command num=%d %x %x", 0, globalVar.cmdNum,pCmd->cmd[kG992p3CmdCode+1],pData[kG992p3CmdCode+1]);
			}
			pMsg = pData + kG992p3CmdSubCode +1;
			pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
			switch(pData[kG992p3CmdSubCode]){
				case 0x81:{
#if defined(CONFIG_VDSL_SUPPORTED)
					memcpy(pMib->xdslAtucPhys.adslSysVendorID,pMsg,8);
					pMsg+=8;
					memcpy(pMib->xdslAtucPhys.adslSysVersionNumber,pMsg,16);
					pMsg+=16; 
					memcpy(pMib->xdslAtucPhys.adslSerialNumber,pMsg,32);
					pMsg+=32;
#else
					memcpy(pMib->adslAtucPhys.adslSysVendorID,pMsg,8);
					pMsg+=8;
					memcpy(pMib->adslAtucPhys.adslSysVersionNumber,pMsg,16);
					pMsg+=16; 
					memcpy(pMib->adslAtucPhys.adslSerialNumber,pMsg,32);
					pMsg+=32;
#endif
				}
				break;
			}
			break;
		
		case kG992p3OvhMsgCmdPMDRead:
			CHECK_CMD_RSP(pData[kG992p3CmdCode]);
			if (kG992p3OvhMsgCmdPMDReadNACK == pData[kG992p3CmdSubCode] && !(pCmd->cmdFlags & kPollCmdStayActive)) {
				pCmd->cmdFlags &= ~kPollCmdActive;
				if (globalVar.cmdNum==kPMDVectorBlockReadCmd)
#ifdef CONFIG_BCM6368
					G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,0);
#endif
				__SoftDslPrintf(gDslVars, "G992p3OvhMsg (NACK) Disable command num=%d", 0, globalVar.cmdNum);
				break;
			}
			
			if (pCmd->cmdFlags & kPollCmdOnce) {
				pCmd->cmdFlags &= ~kPollCmdActive;
				__SoftDslPrintf(gDslVars, "G992p3OvhMsg (ONCE) Disable command num=%d", 0, globalVar.cmdNum);
			}
			
			if (!(pCmd->cmdFlags & kPollCmdOnce) && !(pCmd->cmdFlags & kPollCmdStayActive))
				pCmd->cmdFlags |= kPollCmdStayActive;
			pMsg = pData + kG992p3CmdSubCode + 1;
			mibLen = sizeof(adslMibInfo);
			pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
#ifdef G993
			if( XdslMibIsVdsl2Mod(gDslVars) ) {
				switch(pData[kG992p3CmdSubCode]){
				case kG992p3OvhMsgCmdPMDBlockReadRsp:
				{
					int startSCG,stopSCG,i,g,n,j,k,diff=0;
					startSCG=ReadCnt16(&(pCmd->cmd[4]));
					stopSCG=ReadCnt16(&(pCmd->cmd[6]));
					if(globalVar.pollingSnrBlRdCmd==0) 
					{
					__SoftDslPrintf(gDslVars, "Parsing Block Read Rsp Hlog",0);
					pMsg+=2;
					oidStr[1] = kOidAdslPrivChanCharLog;
					g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSus;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pToneData+=startSCG*g;
					n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
					n = -(n/10) + 6*16;
					j=0;
					for(k=0;k<pMib->usNegBandPlanDiscovery.noOfToneGroups;k++)
					{
						if(startSCG*g > pMib->usNegBandPlanDiscovery.toneGroups[k].endTone)
							continue;
						diff=pMib->usNegBandPlanDiscovery.toneGroups[k].startTone-startSCG*g;
						if(diff>=0 && diff<g)
						{
							j=diff;
							pToneData+=j;
						}
						break;
						
					}
					__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlanDiscovery.toneGroups[k].startTone,diff);
					for (;j<g;j++)
						*pToneData++ = n;
					pMsg += 2;
					for (i=startSCG+1;i<stopSCG;i++)
					{
						n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
						n = -(n/10) + 6*16;
						for (j=0;j<g;j++)
							*pToneData++ = n;
						pMsg += 2;
					}
					if(startSCG!=stopSCG)
					{
						__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlanDiscovery.toneGroups[k].endTone);
						diff=(stopSCG+1)*g-1-pMib->usNegBandPlanDiscovery.toneGroups[k].endTone;
						if(diff<0)
							diff=0;
						n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
						n = -(n/10) + 6*16;
						for (j=0;j<g-diff;j++)
							*pToneData++ = n;
						pMsg += 2;
					}
					
					__SoftDslPrintf(gDslVars, "Parsing Block Read Rsp QLN",0);
					pMsg+=2;
					oidStr[1] = kOidAdslPrivQuietLineNoise;
					g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSus;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pToneData+=startSCG*g;
					if (*pMsg != 255)
						n = -(((short) pMsg[0]) << 3) - 23*16;
					else
						n = -160*16;
					j=0;
					for(k=0;k<pMib->usNegBandPlanDiscovery.noOfToneGroups;k++)
					{
						if(startSCG*g > pMib->usNegBandPlanDiscovery.toneGroups[k].endTone)
							continue;
						diff=pMib->usNegBandPlanDiscovery.toneGroups[k].startTone-startSCG*g;
						if(diff>=0 && diff<g)
						{
							j=diff;
							pToneData+=j;
						}
						break;
					}
					__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlanDiscovery.toneGroups[k].startTone,diff);
					for (;j<g;j++)
						*pToneData++ = n;
					pMsg++;
					for (i=startSCG+1;i<stopSCG;i++)
					{
						if (*pMsg != 255)
							n = -(((short) pMsg[0]) << 3) - 23*16;
						else
							n = -160*16;
						for (j=0;j<g;j++)
							*pToneData++ = n;
						pMsg++;
					}
					if(startSCG!=stopSCG) 
					{
						__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlanDiscovery.toneGroups[k].endTone);
						diff=(stopSCG+1)*g-1-pMib->usNegBandPlanDiscovery.toneGroups[k].endTone;
						if(diff<0)
							diff=0;
						if (*pMsg != 255)
							n = -(((short) pMsg[0]) << 3) - 23*16;
						else
							n = -160*16;
						for (j=0;j<g-diff;j++)
							*pToneData++ = n;
						pMsg++;
					}
					G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,1);
					break;
					}
					__SoftDslPrintf(gDslVars, "Parsing Block Read Rsp SNR",0);
					pMsg+=2+2*(stopSCG-startSCG+1)+2+(stopSCG-startSCG+1);
					
					pMsg+=2;
					oidStr[1] = kOidAdslPrivSNR;
					g=pMib->gFactors.Gfactor_MEDLEYSETus;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pToneData+=startSCG*g;
					if (*pMsg != 255)
						n= (((short) pMsg[0]) << 3) - 32*16;
					else
						n = 0;
					j=0;
					for(k=0;k<pMib->usNegBandPlan.noOfToneGroups;k++)
					{
						if(startSCG*g > pMib->usNegBandPlan.toneGroups[k].endTone)
							continue;
						diff=pMib->usNegBandPlan.toneGroups[k].startTone-startSCG*g;
						if(diff>=0 && diff<g)
						{
							j=diff;
							pToneData+=j;
						}
						break;
					}
					__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlan.toneGroups[k].startTone,diff);
					for (;j<g;j++)
						*pToneData++ = n;
					pMsg++;
					for (i=startSCG+1;i<stopSCG;i++)
					{
						if (*pMsg != 255)
							n = (((short) pMsg[0]) << 3) - 32*16;
						else
							n = 0;
						for (j=0;j<g;j++)
							*pToneData++ = n;
						pMsg++;
					}
					if(startSCG!=stopSCG) 
					{
						__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlan.toneGroups[k].endTone);
						diff=(stopSCG+1)*g-1-pMib->usNegBandPlan.toneGroups[k].endTone;
						if(diff<0)
							diff=0;
						if (*pMsg != 255)
							n = (((short) pMsg[0]) << 3) - 32*16;
						else
							n = 0;
						for (j=0;j<g-diff;j++)
							*pToneData++ = n;
						pMsg++;
					}
				}
				G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,1);
				break;
				case kG992p3OvhMsgCmdPMDVectorBlockReadRsp:
				{
					switch(pCmd->cmd[4])
					{
						case kG992p3OvhMsgCmdPMDChanRspLog:
						{
							int startSCG,stopSCG,i,g,n,j,k,diff=0;
							__SoftDslPrintf(gDslVars, "Parsing Vector Read Rsp Hlog",0);
							startSCG=ReadCnt16(&(pCmd->cmd[5]));
							stopSCG=ReadCnt16(&(pCmd->cmd[7]));
							pMsg+=2;
							oidStr[1] = kOidAdslPrivChanCharLog;
							g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSus;
							pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
							pToneData+=startSCG*g;
							n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
							n = -(n/10) + 6*16;
							j=0;
							for(k=0;k<pMib->usNegBandPlanDiscovery.noOfToneGroups;k++)
							{
								if(startSCG*g > pMib->usNegBandPlanDiscovery.toneGroups[k].endTone)
									continue;
								diff=pMib->usNegBandPlanDiscovery.toneGroups[k].startTone-startSCG*g;
								if(diff>=0 && diff<g)
								{
									j=diff;
									pToneData+=j;
								}
								break;
							}
							__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlanDiscovery.toneGroups[k].startTone,diff);
							for (;j<g;j++)
								*pToneData++ = n;
							pMsg += 2;
							for (i=startSCG+1;i<stopSCG;i++)
							{
								n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
								n = -(n/10) + 6*16;
								for (j=0;j<g;j++)
									*pToneData++ = n;
								pMsg += 2;
							}
							if(startSCG!=stopSCG)
							{
								__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlanDiscovery.toneGroups[k].endTone);
								diff=(stopSCG+1)*g-1-pMib->usNegBandPlanDiscovery.toneGroups[k].endTone;
								if(diff<0)
									diff=0;
								n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
								n = -(n/10) + 6*16;
								for (j=0;j<g-diff;j++)
									*pToneData++ = n;
							}
						}
						G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,1);
						break;
						case kG992p3OvhMsgCmdPMDQLineNoise:
						{
							int startSCG,stopSCG,i,g,n,j,k,diff=0;
							__SoftDslPrintf(gDslVars, "Parsing Vector Read Rsp QLN",0);
							startSCG=ReadCnt16(&(pCmd->cmd[5]));
							stopSCG=ReadCnt16(&(pCmd->cmd[7]));
							pMsg+=2;
							oidStr[1] = kOidAdslPrivQuietLineNoise;
							g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSus;
							pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
							pToneData+=startSCG*g;
							if (*pMsg != 255)
								n = -(((short) pMsg[0]) << 3) - 23*16;
							else
								n = -160*16;
							j=0;
							for(k=0;k<pMib->usNegBandPlanDiscovery.noOfToneGroups;k++)
							{
								if(startSCG*g > pMib->usNegBandPlanDiscovery.toneGroups[k].endTone)
									continue;
								diff=pMib->usNegBandPlanDiscovery.toneGroups[k].startTone-startSCG*g;
								if(diff>=0 && diff<g)
								{
									j=diff;
									pToneData+=j;
								}
								break;
							}
							__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlanDiscovery.toneGroups[k].startTone,diff);
							for (;j<g;j++)
								*pToneData++ = n;
							pMsg++;
							for (i=startSCG+1;i<stopSCG;i++)
							{
								if (*pMsg != 255)
									n = -(((short) pMsg[0]) << 3) - 23*16;
								else
									n = -160*16;
								for (j=0;j<g;j++)
									*pToneData++ = n;
								pMsg++;
							}
							if(startSCG!=stopSCG)
							{
								__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlanDiscovery.toneGroups[k].endTone);
								diff=(stopSCG+1)*g-1-pMib->usNegBandPlanDiscovery.toneGroups[k].endTone;
								if(diff<0)
									diff=0;
								if (*pMsg != 255)
									n = -(((short) pMsg[0]) << 3) - 23*16;
								else
									n = -160*16;
								for (j=0;j<g-diff;j++)
									*pToneData++ = n;
							}
						}
						G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,1);
						break;
						case kG992p3OvhMsgCmdPMDSnr:
						{
							int startSCG,stopSCG,i,g,n,j,k,diff=0;
							__SoftDslPrintf(gDslVars, "Parsing Vector Read Rsp SNR",0);
							startSCG=ReadCnt16(&(pCmd->cmd[5]));
							stopSCG=ReadCnt16(&(pCmd->cmd[7]));
							n=ReadCnt16(pMsg);
							pMsg+=2;
							oidStr[1] = kOidAdslPrivSNR;
							g=pMib->gFactors.Gfactor_MEDLEYSETus;
							pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
							pToneData+=startSCG*g;
							if (*pMsg != 255)
								n= (((short) pMsg[0]) << 3) - 32*16;
							else
								n = 0;
							j=0;
							for(k=0;k<pMib->usNegBandPlan.noOfToneGroups;k++)
							{
								if(startSCG*g > pMib->usNegBandPlan.toneGroups[k].endTone)
									continue;
								diff=pMib->usNegBandPlan.toneGroups[k].startTone-startSCG*g;
								if(diff>=0 && diff<g)
								{
									j=diff;
									pToneData+=j;
								}
								break;
							}
							__SoftDslPrintf(gDslVars, "startSCG=%d j=%d BPstartTone=%d diff=%d",0,startSCG,j,pMib->usNegBandPlan.toneGroups[k].startTone,diff);
							for (;j<g;j++)
								*pToneData++ = n;
							pMsg++;
							for (i=startSCG+1;i<stopSCG;i++)
							{
								if (*pMsg != 255)
									n = (((short) pMsg[0]) << 3) - 32*16;
								else
									n = 0;
								for (j=0;j<g;j++)
									*pToneData++ = n;
								pMsg++;
							}
							if(startSCG!=stopSCG) 
							{
								__SoftDslPrintf(gDslVars, "stopSCG=%d endTone=%d ",0,stopSCG,pMib->usNegBandPlan.toneGroups[k].endTone);
								diff=(stopSCG+1)*g-1-pMib->usNegBandPlan.toneGroups[k].endTone;
								if(diff<0)
									diff=0;
								if (*pMsg != 255)
									n = (((short) pMsg[0]) << 3) - 32*16;
								else
									n = 0;
								for (j=0;j<g-diff;j++)
									*pToneData++ = n;
							}
						}
						G992p3OvhMsgPollPMDPerTonePollCmdControl(gDslVars,1);
						break;
					}
				}
				break;
				case kG992p3OvhMsgCmdPMDSingleRdRsp:
					{
						int US0used;
						__SoftDslPrintf(gDslVars, "%s: kG992p3OvhMsgCmdPMDSingleRdRsp, rspLen = %d",0, __FUNCTION__, rspLen);

						if(pMib->usNegBandPlanDiscovery.toneGroups[0].startTone<256)
							US0used=1;
						else
							US0used=0;
						if(US0used==0)
							pMsg+=2;
						for(n=0;n<pMib->usNegBandPlanDiscovery.noOfToneGroups;n++) {
							pMib->perbandDataUs[n].adslCurrAtn= ReadCnt16(pMsg);
#if 0
							__SoftDslPrintf(gDslVars, "US LATN[%d]: %u/10", 0,n, pMib->perbandDataUs[n].adslCurrAtn);
#endif
							pMsg+=2;
						}
						pMsg+=2*(4+US0used-pMib->usNegBandPlanDiscovery.noOfToneGroups);
						if(US0used==0)
							pMsg+=2;
						for(n=0;n<pMib->usNegBandPlanDiscovery.noOfToneGroups;n++) {
							pMib->perbandDataUs[n].adslSignalAttn= ReadCnt16(pMsg);
#if 0
						__SoftDslPrintf(gDslVars, "US SATN[%d]: %u/10", 0,n,pMib->perbandDataUs[n].adslSignalAttn);
#endif
						pMsg+=2;
						}
						pMsg+=2*(4+US0used-pMib->usNegBandPlanDiscovery.noOfToneGroups);
						pMib->adslAtucPhys.adslCurrSnrMgn = ReadCnt16(pMsg);
						pMsg+=2;
#if 0
						__SoftDslPrintf(gDslVars, "US SNRM: %d/10", 0, pMib->adslAtucPhys.adslCurrSnrMgn);
#endif
						if(US0used==0)
							pMsg+=2;
						for(n=0;n<pMib->usNegBandPlanDiscovery.noOfToneGroups;n++) {
							pMib->perbandDataUs[n].adslCurrSnrMgn= ReadCnt16(pMsg);
#if 0
							__SoftDslPrintf(gDslVars, "US SNRM[%d]: %u/10", 0,n,pMib->perbandDataUs[n].adslCurrSnrMgn);
#endif
							pMsg+=2;
						}
						pMsg+=2*(4+US0used-pMib->usNegBandPlanDiscovery.noOfToneGroups);   
						pMib->adslAtucPhys.adslCurrAttainableRate=ReadCnt32(pMsg);
#if 0
						__SoftDslPrintf(gDslVars, "US ATTN RATE: %u/1000 kbps", 0,pMib->adslAtucPhys.adslCurrAttainableRate);
#endif
						pMsg+=4;
#ifndef USE_LOCAL_DS_POWER
						pMib->adslAtucPhys.adslCurrOutputPwr=ReadCnt16(pMsg);
#endif
#if 0
						__SoftDslPrintf(gDslVars, "US NE PWR: %d/10 dBm", 0,pMib->adslAtucPhys.adslCurrOutputPwr);
#endif
						pMsg+=2;
						pMsg+=2;
						if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
#if 1
							path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
							/* Store INP/INPrein in Q1 */
							pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].INP = (ushort)(ReadCnt16(pMsg)/5); pMsg+=2;
							pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].INPrein = (*pMsg)/5; pMsg+=1;
#else
							ushort usVal;
							usVal = (ushort)(ReadCnt16(pMsg)/10); pMsg += 2;
							__SoftDslPrintf(gDslVars, "From CO -> DS: INP = %d, INPRein = %d",0, usVal, *pMsg/10); pMsg+=1;
#endif
						}
						if(XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
#if 1
							path1 = XdslMibGetPath1MappedPathId(gDslVars, US_DIRECTION);
							n= ReadCnt32(pMsg); pMsg+=4;	/* etrRate */
							pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].delay = *pMsg; pMsg+=1;
							if(n != pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].etrRate) {
								pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].etrRate = n;
								dslCmd.command = kDslRTXSetUsEtr;
								dslCmd.param.value = pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].etrRate;
								(*globalVar.cmdHandlerPtr)(gDslVars, &dslCmd);
							}
#else
							US0used = ReadCnt32(pMsg); pMsg+=4;
							__SoftDslPrintf(gDslVars, "From CO -> US: etrRate = %d, delay = %d",0, US0used, *pMsg); pMsg+=1;
#endif
						}
						break;
					}
				default : break;
				}
				break;
			}
#endif
			if (kG992p3OvhMsgCmdPMDSingleRdRsp != pData[kG992p3CmdSubCode])
				break;
			rspId = pCmd->cmd[kG992p3CmdCode+2];
#if 0 && defined(G992P3_DBG_PRINT)
			__SoftDslPrintf(gDslVars, "kG992p3OvhMsgCmdPMDSingleRdRsp: rspId = 0x%X, rspLen=%d", 0, rspId, rspLen);
#endif
			switch (rspId) {
				case kG992p3OvhMsgCmdPMDLnAttn:
					pMib->adslAtucPhys.adslCurrAtn = ReadCnt16(pMsg);
					__SoftDslPrintf(gDslVars, "US LATN: %d/10", 0, pMib->adslAtucPhys.adslCurrAtn);
					break;
				case kG992p3OvhMsgCmdPMDSigAttn:
					pMib->adslAtucPhys.adslSignalAttn = ReadCnt16(pMsg);
					__SoftDslPrintf(gDslVars, "US SATN: %d/10", 0, pMib->adslAtucPhys.adslSignalAttn);
					break;
				case kG992p3OvhMsgCmdPMDSnrMgn:
					pMib->adslAtucPhys.adslCurrSnrMgn = ReadCnt16(pMsg);
					__SoftDslPrintf(gDslVars, "US SNRM: %d/10", 0, pMib->adslAtucPhys.adslCurrSnrMgn);
					break;
				case kG992p3OvhMsgCmdPMDAttnDR:
#ifdef WORKAROUND_2BYTES_US_ATTNDR_RSP
					if(rspLen == 6)
						pMib->adslAtucPhys.adslCurrAttainableRate = ReadCnt16(pMsg) * 4000;
					else
#endif
					pMib->adslAtucPhys.adslCurrAttainableRate = ReadCnt32(pMsg);
					__SoftDslPrintf(gDslVars, "US ATTNDR: %d Kbps", 0, pMib->adslAtucPhys.adslCurrAttainableRate/1000);
					break;
#ifndef USE_LOCAL_DS_POWER
				case kG992p3OvhMsgCmdPMDNeXmtPower:
					if( 0 == (globalVar.setup & kG992p3ClearEocGSPNXmtPwrDSNoUpdate) ) {
						n=ReadCnt16(pMsg);
						pMib->adslAtucPhys.adslCurrOutputPwr = (n << (32-10)) >> (32-10);
						AdslMibUpdateACTPSD(gDslVars, (pMib->adslAtucPhys.adslCurrOutputPwr << 4)/10, DS_DIRECTION);
						__SoftDslPrintf(gDslVars, "DS XmtPwr: %d/10", 0, pMib->adslAtucPhys.adslCurrOutputPwr);
					}
					break;
#endif
				case kG992p3OvhMsgCmdPMDSnr:
					mibLen = 0;
					oidStr[1] = kOidAdslPrivSNR;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pMsg += 2;
					pMsgEnd = pData + rspLen;
					while (pMsg != pMsgEnd) {
						if (*pMsg != 255)
							*pToneData = (((short) pMsg[0]) << 3) - 32*16;
						else
							*pToneData = 0;
						pToneData++;
						pMsg++;
					}
					break;
				case kG992p3OvhMsgCmdPMDQLineNoise:
					mibLen = 0;
					oidStr[1] = kOidAdslPrivQuietLineNoise;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pMsg += 2;
					pMsgEnd = pData + rspLen;
					while (pMsg != pMsgEnd) {
						if (*pMsg != 255)
							*pToneData = -(((short) pMsg[0]) << 3) - 23*16;
						else
							*pToneData = -150*16;
						pToneData++;
						pMsg++;
					}
					break;
				case kG992p3OvhMsgCmdPMDChanRspLog:
					mibLen = 0;
					oidStr[1] = kOidAdslPrivChanCharLog;
					pToneData = (void *) AdslMibGetObjectValue(gDslVars, oidStr,sizeof(oidStr), NULL, &mibLen);
					pMsg += 2;
					pMsgEnd = pData + ((rspLen + 1) & ~0x1);
					while (pMsg != pMsgEnd) {
						n = ((int) (pMsg[0] & 0x3) << 12) + (pMsg[1] << 4);
						n = -(n/10) + 6*16;
						*pToneData++ = n;
						pMsg += 2;
					}
					break;
				case kG992p3OvhMsgCmdPMDSingleReadINPId:
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
#if 1
						path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
						pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].INP = (ushort)(ReadCnt16(pMsg)/5); pMsg+=2;
#else
					__SoftDslPrintf(gDslVars, "From CO -> DS: INP = %d", 0, ReadCnt16(pMsg)/10); pMsg+=2;
#endif
					}
					break;
				case kG992p3OvhMsgCmdPMDSingleReadINPReinId:
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
#if 1
						path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
						pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].INPrein = *pMsg/5; pMsg+=1;
#else
					__SoftDslPrintf(gDslVars, "From CO -> DS: INPRein = %d", 0, *pMsg/10); pMsg+=1;
#endif
					}
					break;
				default:
					break;
				}
			break;
		case kG992p3OvhMsgCmdPower:
			if (globalVar.txFlags & kTxCmdL3WaitingAck) {
				dslStatusStruct   status;

				status.code = kDslConnectInfoStatus;
				status.param.dslConnectInfo.code = kG992p3PwrStateInfo;
				if (kPwrGrant == pData[kG992p3CmdSubCode])
					status.param.dslConnectInfo.value = 3;
				else 
					status.param.dslConnectInfo.value = kPwrReject;
				(*globalVar.statusHandlerPtr)(gDslVars, &status);
				globalVar.txFlags &= ~kTxCmdL3WaitingAck;
			}
			globalVar.txFlags &= ~kTxCmdL0WaitingAck;
			break;
		case kG992p3OvhMsgCmdClearEOC:
			if( G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc) ) {
				globalVar.txFlags &= ~kTxCmdWaitingAck;
				G992p3OvhMsgCompleteClearEocFrame(gDslVars, globalVar.lastTxCmdFrame);
			}
			break;
		case kG992p3OvhMsgCmdOLR:
			if((kOLRDeferType1 == pData[kG992p3CmdSubCode]) ||
				(kOLRRejectType2 == pData[kG992p3CmdSubCode]) ||
				(kOLRRejectType3 == pData[kG992p3CmdSubCode])) {
				mibLen = sizeof(adslMibInfo);
				pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
				pMib->adslStat.bitswapStat.rcvCntRej++;
				XdslMibNotifyBitSwapRej(gDslVars, DS_DIRECTION);
				__SoftDslPrintf(gDslVars, "%s: OLR Reject Type %d Reason %d rcvCntRej %ld\n", 0,
					__FUNCTION__, pData[kG992p3CmdSubCode], pData[kG992p3CmdSubCode+1], pMib->adslStat.bitswapStat.rcvCntRej);
			}
			break;
		default:
			return true;	/* Prevent unknown/un-handled responses from changing the polling/re-transmitting state machine */
	}

	if(!(G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc)) &&
		!(globalVar.txFlags & kTxCmdL0WaitingAck) &&
		!(globalVar.txFlags & kTxCmdL3WaitingAck)) {
		globalVar.txFlags &= ~kTxCmdWaitingAck;
	}

	return true;
}

Private void G992p3CopyOemData(uchar *pDest, ulong dstLen, uchar *pSrc, ulong srcLen)
{
  if (dstLen <= srcLen)
    AdslMibByteMove (dstLen, pSrc, pDest);
  else {
    AdslMibByteMove (srcLen, pSrc, pDest);
    AdslMibByteClear(dstLen - srcLen, pDest + srcLen);
  }
}

void G992p3GetOemData(void *gDslVars, int paramId, uchar *pDest, ulong dstLen, uchar *pSrc, ulong srcLen)
{
  dslStatusStruct     status;
  int           len;
  
  status.code = kDslGetOemParameter;
  status.param.dslOemParameter.paramId = paramId;
  status.param.dslOemParameter.dataPtr = NULL;
  status.param.dslOemParameter.dataLen = 0;
  (*globalVar.statusHandlerPtr)(gDslVars, &status);
  if (NULL != status.param.dslOemParameter.dataPtr)
    G992p3CopyOemData(pDest, dstLen, status.param.dslOemParameter.dataPtr, status.param.dslOemParameter.dataLen);
  else if (NULL == pSrc)
    AdslMibByteClear(dstLen, pDest);
  else if (0 == srcLen) {
    len = AdslMibStrCopy(pSrc, pDest);
    if (len < dstLen)
      AdslMibByteClear(dstLen - len, pDest + len);
  }
  else  // pSrc != NULL, srcLen != 0
    G992p3CopyOemData(pDest, dstLen, pSrc, srcLen);
}
#if defined(CONFIG_VDSL_SUPPORTED)
static void G992p3OvhMsgProcessBlockReadCommand(void *gDslVars,uchar *pData, int *rLen,Boolean* bRsp)
{
    int startSCG,stopSCG,i,g,j;
    uchar		*pMsg;
    unsigned long hlog,hlogLen,snr,snrLen,qln,qlnLen,mibLen;
    short *pHlogInfo,*pQlnInfo,*pSnrInfo;
    adslMibInfo	*pMib;
    uchar hLogOidStr[] = { kOidAdslPrivate, kOidAdslPrivChanCharLog};
    uchar qlnOidStr[] = { kOidAdslPrivate, kOidAdslPrivQuietLineNoise};
    uchar snrOidStr[] = { kOidAdslPrivate, kOidAdslPrivSNR };
    pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
    startSCG=ReadCnt16(pData+kG992p3CmdSubCode+1);
    stopSCG=ReadCnt16(pData+kG992p3CmdSubCode+3);
    if(startSCG>511 || stopSCG>511) {
        globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
        *rLen=4;
        *bRsp = true;
        return;
    }
    pHlogInfo = (void*) AdslMibGetObjectValue (gDslVars, hLogOidStr, sizeof(hLogOidStr), NULL, &hlogLen);
    pQlnInfo = (void*) AdslMibGetObjectValue (gDslVars, qlnOidStr, sizeof(qlnOidStr), NULL, &qlnLen);
    pSnrInfo = (void*) AdslMibGetObjectValue (gDslVars, snrOidStr, sizeof(snrOidStr), NULL, &snrLen);
    globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgCmdPMDBlockReadRsp;
    *rLen=4;
    pMsg = &globalVar.txRspBuf[*rLen];
    WriteCnt16(pMsg, 256);
    pMsg+=2;
    g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSds;
    i=startSCG;
    for (j=0;j<pMib->dsNegBandPlanDiscovery.noOfToneGroups;j++)
    {
        for(;i<=stopSCG;i++) {
            if(i*g >  pMib->dsNegBandPlanDiscovery.toneGroups[j].endTone )
                break;
            if(i*g <  pMib->dsNegBandPlanDiscovery.toneGroups[j].startTone )
                hlog=0x3FF;
            else 
            { 
                hlog = (ulong) (((6*16-pHlogInfo[i*g])*5) >> 3);
                if (hlog > 0x3FC)
                        hlog=0x3FF;
            }
            WriteCnt16(pMsg,hlog);
            pMsg+=2;
        }
    }
    for(;i<=stopSCG;i++) {
        WriteCnt16(pMsg,0x3FF);
        pMsg+=2;
    }
    *rLen+=2+2*(stopSCG-startSCG+1);
    WriteCnt16(pMsg, 256);
    pMsg+=2;
    i=startSCG;
    for (j=0;j<pMib->dsNegBandPlanDiscovery.noOfToneGroups;j++)
    {   
        for(;i<=stopSCG;i++) {
            if(i*g >  pMib->dsNegBandPlanDiscovery.toneGroups[j].endTone )
                break;
            if(i*g <  pMib->dsNegBandPlanDiscovery.toneGroups[j].startTone )
                pMsg[i-startSCG]=255;
            else 
            { 
                qln = (ulong) ((-pQlnInfo[i*g] - 23*16) >> 3);
                pMsg[i-startSCG]=(qln > 254) ? 255 : (uchar)qln;
            }
        }
    }
    for(;i<=stopSCG;i++) 
        pMsg[i-startSCG]=255;

    *rLen+=2+stopSCG-startSCG+1;
    pMsg+=stopSCG-startSCG+1;
    g=pMib->gFactors.Gfactor_MEDLEYSETds;
    WriteCnt16(pMsg, 256);
    pMsg+=2;
    i=startSCG;
    for (j=0;j<pMib->dsNegBandPlan.noOfToneGroups;j++)
    {
        for(;i<=stopSCG;i++) {
            if(i*g >  pMib->dsNegBandPlan.toneGroups[j].endTone )
                break;
            if(i*g <  pMib->dsNegBandPlan.toneGroups[j].startTone )
                pMsg[i-startSCG]=255;
            else 
            { 
                snr = (ulong) ((pSnrInfo[i*g] + 32*16) >> 3);
                pMsg[i-startSCG]=(snr > 254) ? 255 : (uchar)snr;
            }
        }
    }
    for(;i<=stopSCG;i++) 
        pMsg[i-startSCG]=255;
    *rLen+=2+stopSCG-startSCG+1;
    *bRsp=true;
    return;
}
#endif
Private Boolean G992p3OvhMsgRcvProcessCmd(void *gDslVars, dslFrameBuffer *pBuf, uchar *pData, ulong cmdLen, dslFrame *pFrame)
{
	uchar		*pMsg;
	Boolean		bRsp;
	int		n, i, rspLen;
	ulong		mibLen;
	adslMibInfo	*pMib;
	adslChanPerfDataEntry *pChanPerfData;
	dslCommandStruct		cmd;
	int				modType = AdslMibGetModulationType(gDslVars);
	int				timeSec,timeSec1,timeMin,timeMin1,timeHr,timeHr1;
	int				path0, path1;
	
	path0 = XdslMibGetPath0MappedPathId(gDslVars, DS_DIRECTION);
	path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
	
	if ((pData[kG992p3CmdCode] != kG992p3OvhMsgCmdOLR) && G992p3OvhMsgXmtRspBusy())
		return false;
	
	globalVar.rxCmdMsgNum = pData[kG992p3CtrlField] & kG992p3MsgNumMask;
	bRsp = false;
	rspLen = 0;
	globalVar.PLNMessageReadRspFlag=0;
#ifdef G992P3_DBG_PRINT
	__SoftDslPrintf(gDslVars, "G992p3OvhMsgRcvProcessCmd: pFrame = 0x%x, pBuf = 0x%x, pData %x %x %x, cmdLen = %d",
		0, (long)pFrame, (long)pBuf, pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], pData[kG992p3CmdSubCode+1], (int)cmdLen);
#endif
	switch (pData[kG992p3CmdCode]) {
		case kG992p3OvhMsgCmdTime:
			if (pData[kG992p3CmdSubCode]==0x01) {
				globalVar.txRspBuf[kG992p3CmdCode] = pData[kG992p3CmdCode];
				globalVar.cmdTime=(pData[4]-'0')*36000+(pData[5]-'0')*3600+(pData[7]-'0')*600+(pData[8]-'0')*60+(pData[10]-'0')*10+pData[11]-'0';
				globalVar.cmdTime*=1000;
				globalVar.txRspBuf[kG992p3CmdSubCode]=0x80;
				rspLen=4; 
				timeSec1=(globalVar.cmdTime+500)/1000;
				timeMin1=timeSec1/60;
				timeHr1=timeMin1/60;
				timeSec1%=60;
				timeMin1%=60;
				__SoftDslPrintf(gDslVars, "G992p3Ovh CMD TIME Set as Hr%d Min %d Sec %d", 0, timeHr1,timeMin1,timeSec1);
			}
			else if(pData[kG992p3CmdSubCode]==0x02) {
				globalVar.txRspBuf[kG992p3CmdCode] = pData[kG992p3CmdCode];
				globalVar.txRspBuf[kG992p3CmdSubCode]=0x82;
				timeSec=(globalVar.cmdTime+500)/1000;
				timeMin=timeSec/60;
				timeHr=timeMin/60;
				timeSec%=60;
				timeMin%=60;
				globalVar.txRspBuf[4]=timeHr/10+'0';
				globalVar.txRspBuf[5]=timeHr%10+'0';
				globalVar.txRspBuf[6]=':';
				globalVar.txRspBuf[7]=timeMin/10+'0';
				globalVar.txRspBuf[8]=timeMin%10+'0';
				globalVar.txRspBuf[9]=':';
				globalVar.txRspBuf[10]=timeSec/10+'0';
				globalVar.txRspBuf[11]=timeSec%10+'0';
				rspLen=12;
				__SoftDslPrintf(gDslVars, "G992p3Ovh CMD TIME Get called Hr%d Min %d Sec %d", 0, timeHr,timeMin,timeSec);
			}
			bRsp = true;
			break;
			
		case kG992p3OvhMsgCmdCntRead:
			mibLen = sizeof(adslMibInfo);
			pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
			if ( (kG992p3OvhMsgCmdCntReadId == pData[kG992p3CmdSubCode]) ||
				((pMib->adslStat.fireStat.status & kFireDsEnabled) && (kG992p3OvhMsgCmdCntReadId1== pData[kG992p3CmdSubCode])) ) {
				i = 0;
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
				pChanPerfData = (kAdslIntlChannel == pMib->adslConnection.chType) ?
					&pMib->adslChanIntlPerfData : &pMib->adslChanFastPerfData;
#else
				if(!(XdslMibIs2lpActive(gDslVars, DS_DIRECTION) || XdslMibIs2lpActive(gDslVars, US_DIRECTION)) ) {
					pChanPerfData = (kAdslIntlChannel == pMib->adslConnection.chType) ?
						&pMib->adslChanIntlPerfData : &pMib->adslChanFastPerfData;
				}
				else
					pChanPerfData = pMib->xdslChanPerfData;
#endif
				globalVar.txRspBuf[kG992p3CmdCode] = pData[kG992p3CmdCode];
				globalVar.txRspBuf[kG992p3CmdSubCode] = pData[kG992p3CmdSubCode] ^ 0x80;
				pMsg = &globalVar.txRspBuf[kG992p3CmdSubCode+1];
#if defined(USE_CXSY_OVH_MSG_COUNTER_WORKAROUND)
				/* Report CRC's and FEC's counters since showtime iso since power on */
				if( 0 != (globalVar.setup & kG992p3ClearEocCXSYCounterWorkaround))
				{
					WriteCnt32(pMsg+i, pMib->xdslStat[path0].rcvStat.cntRSCor); i+= 4;
					
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) {
						WriteCnt32(pMsg+i, pMib->xdslStat[path1].rcvStat.cntRSCor); i+= 4;
					}
					
					WriteCnt32(pMsg+i, pMib->xdslStat[path0].rcvStat.cntSFErr); i+= 4;
					
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) {
						WriteCnt32(pMsg+i, pMib->xdslStat[path1].rcvStat.cntSFErr); i+= 4;
					}
				}
				else 
#endif
				{
					WriteCnt32(pMsg+i, pChanPerfData[path0].perfTotal.adslChanCorrectedBlks); i+= 4;
					
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) {
						WriteCnt32(pMsg+i, pChanPerfData[path1].perfTotal.adslChanCorrectedBlks); i+= 4;
					}
					
					WriteCnt32(pMsg+i, pChanPerfData[path0].perfTotal.adslChanUncorrectBlks); i+= 4;
					
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION)) {
						WriteCnt32(pMsg+i, pChanPerfData[path1].perfTotal.adslChanUncorrectBlks); i+= 4;
					}
				}
				if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION) || XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
					if(XdslMibIsVdsl2Mod(gDslVars) && XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
						WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntUS.rtx_tx); i+= 4;
					}
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
						WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntDS.rtx_c); i+= 4;
						WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntDS.rtx_uc); i+= 4;
					}
				}
				
				WriteCnt32(pMsg+i,  pMib->adslPerfData.perfTotal.adslFECs); i+= 4;
				WriteCnt32(pMsg+i, pMib->adslPerfData.perfTotal.adslESs); i+= 4;
				WriteCnt32(pMsg+i, pMib->adslPerfData.perfTotal.adslSES); i+= 4;
				WriteCnt32(pMsg+i, pMib->adslPerfData.perfTotal.adslLOSS); i+= 4;
				WriteCnt32(pMsg+i, pMib->adslPerfData.perfTotal.adslUAS); i+= 4;
				
				if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
					WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntDS.LEFTRS); i+= 4;
					WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntDS.errFreeBits); i+= 4;
					WriteCnt32(pMsg+i, pMib->adslStat.ginpStat.cntDS.minEFTR); i+= 4;
					XdslMibGinpEFTRminReported(gDslVars);
				}
				
				if(XdslMibIsAtmConnectionType(gDslVars)) {
					WriteCnt32(pMsg+i, pMib->atmStat2lp[0].rcvStat.cntHEC); i+= 4;
					WriteCnt32(pMsg+i, pMib->atmStat2lp[0].rcvStat.cntCellTotal); i+= 4;
					WriteCnt32(pMsg+i, pMib->atmStat2lp[0].rcvStat.cntCellData); i+= 4;
					WriteCnt32(pMsg+i, pMib->atmStat2lp[0].rcvStat.cntBitErrs); i+= 4;
					if(XdslMibIs2lpActive(gDslVars, DS_DIRECTION) && !XdslMibIsGinpActive(gDslVars, DS_DIRECTION) &&
						!XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {	/* Real dual latency case */
						WriteCnt32(pMsg+i, pMib->atmStat2lp[1].rcvStat.cntHEC); i+= 4;
						WriteCnt32(pMsg+i, pMib->atmStat2lp[1].rcvStat.cntCellTotal); i+= 4;
						WriteCnt32(pMsg+i, pMib->atmStat2lp[1].rcvStat.cntCellData); i+= 4;
						WriteCnt32(pMsg+i, pMib->atmStat2lp[1].rcvStat.cntBitErrs); i+= 4;
					}
				}
				
				if ( kG992p3OvhMsgCmdCntReadId1 != pData[kG992p3CmdSubCode] )
					rspLen = (pMsg+i) - globalVar.txRspBuf;
				else {
					WriteCnt32(pMsg+i, pMib->adslStat.fireStat.reXmtRSCodewordsRcved); i+= 4;
					WriteCnt32(pMsg+i, pMib->adslStat.fireStat.reXmtUncorrectedRSCodewords); i+= 4;
					WriteCnt32(pMsg+i, pMib->adslStat.fireStat.reXmtCorrectedRSCodewords); i+= 4;
					rspLen = (pMsg+i) - globalVar.txRspBuf;
				}
				bRsp = true;
			}
			break;
			
		case kG992p3OvhMsgCmdEOC:
#ifdef G993_DBG_PRINT
			__SoftDslPrintf(gDslVars, "kG992p3OvhMsgCmdEOC subcode=%x",0,pData[kG992p3CmdSubCode]);
#endif
			switch (pData[kG992p3CmdSubCode]) {
			  case kG992p3OvhMsgCmdEOCSelfTest:
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdEOC;
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdEOCSelfTest;
				globalVar.txRspBuf[kG992p3CmdSubCode+1] = 3;  /* min time in sec. before requesting results */
				rspLen = 5;
				bRsp = true;
				break;
			  case kG992p3OvhMsgCmdEOCUpdTestParam:
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdEOC;
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdEOCAck;
				rspLen = 4;
				bRsp = true;
				break;
			  case kG992p3OvhMsgCmdEOCStartRtxTestMode:
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdEOC;
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdEOCAck;
				rspLen = 4;
				bRsp = true;
				cmd.command=kDslRTXTestModeCmd;
				cmd.param.value=kDslRTXTestModeStart;
				(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				break;
			  case kG992p3OvhMsgCmdEOCStopRtxTestMode:
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdEOC;
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdEOCAck;
				rspLen = 4;
				bRsp = true;
				cmd.command=kDslRTXTestModeCmd;
				cmd.param.value=kDslRTXTestModeStop;
				(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				break;
			  default:
				break;
			}
			break;
			
		case kG992p3OvhMsgCmdClearEOC:
#ifdef G993_DBG_PRINT
			__SoftDslPrintf(gDslVars, "kG992p3OvhMsgCmdClearEOC subcode=%x",0,pData[kG992p3CmdSubCode]);
#endif
			if (kG992p3OvhMsgCmdClearEOCMsg == pData[kG992p3CmdSubCode]) {
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdClearEOC;
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdClearEOCAck;
				rspLen = 4;
				bRsp = true;
			}
			else if (kG992p3OvhMsgCmdClearEOCAck == pData[kG992p3CmdSubCode]) { /* workaround for Huawei DSLAM */
				globalVar.setup |= kG992p3ClearEocWorkaround;
				G992p3OvhMsgPollCmdRateChange(gDslVars);
				G992p3OvhMsgRcvProcessRsp(gDslVars, pBuf, pData, cmdLen);
				__SoftDslPrintf(gDslVars, "G992p3Ovh using Clear EOC Workaround",0);
			}
			break;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
		case kG993p2OvhMsgCmdVectoring:
#ifdef G993P5_OVERHEAD_MESSAGING
        case kG993p2OvhMsgCmdErrorFeedback:
#endif
#ifdef G993_DBG_PRINT
			__SoftDslPrintf(gDslVars,"G993p2: VECTORING CMD received, subcode=%x",0,pData[kG992p3CmdSubCode]);
#endif
			{
#ifndef G993P5_OVERHEAD_MESSAGING
				int	isNack = createVectorReply(gDslVars,pData,cmdLen);
				bRsp = true;
				rspLen = 4;
				globalVar.txRspBuf[kG992p3CmdCode]    = kG993p2OvhMsgCmdVectoring;
				if (isNack)
					globalVar.txRspBuf[kG992p3CmdSubCode] = VECTORMGR_NACK_RESPONSE;
				else
					globalVar.txRspBuf[kG992p3CmdSubCode] = VECTORMGR_ACK_RESPONSE;
#else
				rspLen = createVectorReply(gDslVars,pData,cmdLen);
				bRsp = true;
                __SoftDslPrintf(gDslVars,"G993p2: VECTORING CMD answers with %d bytes",0,rspLen);
                if (rspLen)
                    BlockByteMove(rspLen,pData,globalVar.txRspBuf);
#endif
			}
			break;
#endif
		case kG992p3OvhMsgCmdInventory:
#ifdef G993_DBG_PRINT
			__SoftDslPrintf(gDslVars, "kG992p3OvhMsgCmdInventory subcode=%x",0,pData[kG992p3CmdSubCode]);
#endif
			pMsg = &globalVar.txRspBuf[kG992p3CmdSubCode+1];
			switch (pData[kG992p3CmdSubCode]) {
				case kG992p3OvhMsgCmdInvId:
					{
					adslPhyInfo   *pPhyInfo = AdslCoreGetPhyInfo();

					G992p3GetOemData(gDslVars, kDslOemEocVendorId, pMsg, 8, g992p3OvhMsgVendorId, kDslVendorIDRegisterLength);
					G992p3GetOemData(gDslVars, kDslOemEocVersion, pMsg+8, 16, pPhyInfo->pVerStr, 0);
					G992p3GetOemData(gDslVars, kDslOemEocSerNum, pMsg+24, 32, NULL, 0);
					rspLen = kDslVendorIDRegisterLength + 16 + 32;
					bRsp = true;
					}
					break;
				case kG992p3OvhMsgCmdInvAuxId:
					AdslMibByteMove (kDslVendorIDRegisterLength, g992p3OvhMsgVendorId, pMsg);
					rspLen = kDslVendorIDRegisterLength;
					bRsp = true;
					break;
				case kG992p3OvhMsgCmdInvSelfTestRes:
					pMsg[0] = 0;
					pMsg[1] = 0;
					pMsg[2] = 0;
					pMsg[3] = 0;
					rspLen = 4;
					bRsp = true;
					break;
				case kG992p3OvhMsgCmdInvPmdCap:
				case kG992p3OvhMsgCmdInvPmsTcCap:
				case kG992p3OvhMsgCmdInvTpsTcCap:
				default:
					bRsp = false;
					break;
			}
			if (bRsp) {
				globalVar.txRspBuf[kG992p3CmdCode] = pData[kG992p3CmdCode];
				globalVar.txRspBuf[kG992p3CmdSubCode] = pData[kG992p3CmdSubCode] | 0x80;
				rspLen += 4;
			}
			break;

		case kDslINMControlCommand:
				globalVar.txRspBuf[kG992p3CmdCode] = kDslINMControlCommand;
				switch(pData[kG992p3CmdSubCode]) {
					case kDslSetINMParams:
						{
							unsigned short INMIATO,INMIATS,INMCC,INM_INPEQ_MODE,n;int i;
							globalVar.PLNActiveFlag=1;
							globalVar.PLNNoiseMarginPerBinDecLevel=0xFFFF;
							globalVar.PLNNoiseMarginBroadbandDecLevel=0x1FED;
							INMIATS=pData[4];
							INMIATS=INMIATS>>4;
							INMIATO=pData[4]&0x01;
							INMIATO<<=8;
							INMIATO=INMIATO|pData[5];
							INMCC=pData[6];
							INM_INPEQ_MODE=pData[7];
							__SoftDslPrintf(gDslVars, "SetINMParams INMIATO=%d, INMIATS=%d , INMCC=%d INM_MODE=%d",0,INMIATO,INMIATS,INMCC,INM_INPEQ_MODE);
							if ( (cmdLen < 8) || (INM_INPEQ_MODE > 3) || (INMCC > 64) || (INMIATO < 3) || (INMIATS > 7) || ((INM_INPEQ_MODE == 0 ) && (INMCC !=0 )) )
							{
								globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgINMControlNACK;
								rspLen = 4;
								bRsp = true;
								break;
							}
							globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgINMControlACK;
							globalVar.INMCC=INMCC;
							globalVar.INM_INPEQ_MODE=INM_INPEQ_MODE;
							globalVar.INMIATS=INMIATS;
							globalVar.INMIATO=INMIATO;
							globalVar.PLNnItaBin=8;
							n=2*globalVar.PLNnItaBin;
							if (NULL!= (globalVar.pPLNitaBinIntervalPtr=AdslCoreSharedMemAlloc(n)))
							{
								INMIATS=(1<<INMIATS);
								n=2;
								globalVar.pPLNitaBinIntervalPtr[0]=2;
								for (i=0;i<7;i++)
								{
									n=INMIATO+i*INMIATS;
									globalVar.pPLNitaBinIntervalPtr[i+1]=n;
								}
							}
							globalVar.PLNnInpBin=17;
							n=2*globalVar.PLNnInpBin;
							if(NULL!=(globalVar.pPLNinpBinIntervalPtr=AdslCoreSharedMemAlloc(n)) )
							{
								for (i=0;i<17;i++)
								{
									globalVar.pPLNinpBinIntervalPtr[i]=i+1;
								}
							}
							globalVar.pPLNPeakNoiseTable=NULL;
							globalVar.pPLNThresholdCountTable=NULL;

							globalVar.PLNPerBinPeakNoiseMsrCounter=0;
							globalVar.PLNPerBinThldCounter=0;
							globalVar.PLNImpulseDurStatCounter=0;
							globalVar.PLNImpulseInterArrStatCounter=0;
							globalVar.PLNElapsedTimeMSec=0;
							cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlStop;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);

							cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineInpBinTable;
							cmd.param.dslPlnSpec.nInpBin= globalVar.PLNnInpBin;
							cmd.param.dslPlnSpec.inpBinPtr=globalVar.pPLNinpBinIntervalPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						
							cmd.command=kDslPLNControlCmd;
							cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineItaBinTable;
							cmd.param.dslPlnSpec.nItaBin=globalVar.PLNnItaBin;
							cmd.param.dslPlnSpec.itaBinPtr= globalVar.pPLNitaBinIntervalPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							
							cmd.command=kDslPLNControlCmd;
							cmd.param.dslPlnSpec.plnCmd=kDslINMControlParams;
							cmd.param.dslPlnSpec.inmContinueConfig=globalVar.INMCC;
							cmd.param.dslPlnSpec.inmInpEqMode= globalVar.INM_INPEQ_MODE;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
							if (XdslMibIsVdsl2Mod(gDslVars) || ADSL_PHY_SUPPORT(kAdslPhyE14InmAdsl))
							{
								cmd.command=kDslPLNControlCmd;
								cmd.param.dslPlnSpec.plnCmd=kDslINMConfigParams;
								cmd.param.dslPlnSpec.inmContinueConfig=globalVar.INMCC;
								cmd.param.dslPlnSpec.inmInpEqMode= globalVar.INM_INPEQ_MODE;
								cmd.param.dslPlnSpec.inmIATO=globalVar.INMIATO;
								cmd.param.dslPlnSpec.inmIATS= globalVar.INMIATS;
								(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
								globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
								globalVar.INMWaitForParamsFlag=1;
								bRsp=false;
								break;
							}
#endif
							cmd.command=kDslPLNControlCmd;
							cmd.param.dslPlnSpec.plnCmd=kDslPLNControlStart;
							cmd.param.dslPlnSpec.mgnDescreaseLevelPerBin= globalVar.PLNNoiseMarginPerBinDecLevel;
							cmd.param.dslPlnSpec.mgnDescreaseLevelBand=globalVar.PLNNoiseMarginBroadbandDecLevel;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd); 
							globalVar.txRspBuf[kG992p3CmdSubCode]=0x80;
							globalVar.txRspBuf[4]=kG992p3OvhMsgINMINPEQModeACC;
							rspLen=5;
							bRsp=true;
							break;
						}
					case kDslReadINMCounters:
						{
							pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
							pMib->adslPLNData.PLNUpdateData=0;
							cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlPeakNoiseGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							 cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlThldViolationGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlImpulseNoiseEventGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd; 
							cmd.param.value= kDslPLNControlGetStatus;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd; 
							cmd.param.value=kDslPLNControlImpulseNoiseTimeGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
							bRsp=false;
							globalVar.INMMessageReadCntrRspFlag=1;
							break;
						}
					case kDslReadINMParams:
						{
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
							if (XdslMibIsVdsl2Mod(gDslVars) || ADSL_PHY_SUPPORT(kAdslPhyE14InmAdsl))
							{
								globalVar.txRspBuf[kG992p3CmdSubCode]= kDslReadINMParams | 0x80;
								globalVar.txRspBuf[4]=globalVar.INMIATS << 4;
								globalVar.txRspBuf[4]|=globalVar.INMIATO>>8;
								globalVar.txRspBuf[5]=globalVar.INMIATO&0x00FF;
								globalVar.txRspBuf[6]=globalVar.INMCC;
								globalVar.txRspBuf[7]=globalVar.INM_INPEQ_MODE;
								rspLen=8;
								bRsp=true;
								break;
							}
#endif
							pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
							pMib->adslPLNData.PLNUpdateData=0;
							cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlPeakNoiseGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							 cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlThldViolationGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd;
							cmd.param.value=kDslPLNControlImpulseNoiseEventGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd; 
							cmd.param.value= kDslPLNControlGetStatus;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							cmd.command=kDslPLNControlCmd; 
							cmd.param.value=kDslPLNControlImpulseNoiseTimeGetPtr;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1; 
							bRsp=false;
							globalVar.INMMessageReadParamsRspFlag=1;
							break;
						}
					default:
						{
							globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
							rspLen = 4;
							bRsp = true;
							break;
						}
					}
				break;
		case kG992p3OvhMsgCmdPMDRead:
#ifdef G993_DBG_PRINT
			__SoftDslPrintf(gDslVars, "kG992p3OvhMsgCmdPMDRead subcode=%x",0,pData[kG992p3CmdSubCode]);
#endif
#ifdef G993
			if(XdslMibIsVdsl2Mod(gDslVars)) {
				globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdPMDRead;
				switch(pData[kG992p3CmdSubCode]) {
					default: 
#ifdef G993_DBG_PRINT
						__SoftDslPrintf(gDslVars, "G992p3OvhMsg Not Single Read",0);
#endif
						globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
						rspLen = 4;
						bRsp = true;
						break;
					case kG992p3OvhMsgCmdPMDMultiRead:
					{
						if(cmdLen==8)
						{
							G992p3OvhMsgProcessBlockReadCommand(gDslVars,pData,&rspLen,&bRsp);
							break;
						}
						globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
						rspLen = 4;
						bRsp=true;
						break;
					}
					case kG992p3OvhMsgCmdPMDBlockRead:
					{
						G992p3OvhMsgProcessBlockReadCommand(gDslVars,pData,&rspLen,&bRsp);
						break;
#if 0
						int startSCG,stopSCG,i,g;
						unsigned long hlog,hlogLen,snr,snrLen,qln,qlnLen;
						short *pHlogInfo,*pQlnInfo,*pSnrInfo;
						uchar hLogOidStr[] = { kOidAdslPrivate, kOidAdslPrivChanCharLog};
						uchar qlnOidStr[] = { kOidAdslPrivate, kOidAdslPrivQuietLineNoise};
						uchar snrOidStr[] = { kOidAdslPrivate, kOidAdslPrivSNR };
						pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
						startSCG=ReadCnt16(pData+kG992p3CmdSubCode+1);
						stopSCG=ReadCnt16(pData+kG992p3CmdSubCode+3);
						if(startSCG>511 || stopSCG>511) {
							globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
							rspLen = 4;
							bRsp = true;
							break;
						}
						pHlogInfo = (void*) AdslMibGetObjectValue (gDslVars, hLogOidStr, sizeof(hLogOidStr), NULL, &hlogLen);
						pQlnInfo = (void*) AdslMibGetObjectValue (gDslVars, qlnOidStr, sizeof(qlnOidStr), NULL, &qlnLen);
						pSnrInfo = (void*) AdslMibGetObjectValue (gDslVars, snrOidStr, sizeof(snrOidStr), NULL, &snrLen);
						globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgCmdPMDBlockReadRsp;
						rspLen=4;
						pMsg = &globalVar.txRspBuf[rspLen];
						WriteCnt16(pMsg, 256);
						pMsg+=2;
						g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSds;
						for(i=startSCG;i<=stopSCG;i++) 	{
							hlog = (ulong) (((6*16-pHlogInfo[i*g])*5) >> 3);
							if (hlog > 0x3FE) 
								hlog=0x3FF;
							WriteCnt16(pMsg,hlog);
							pMsg+=2;
						}
						rspLen+=2+2*(stopSCG-startSCG+1);
						WriteCnt16(pMsg, 256);
						pMsg+=2;
						for(i=startSCG;i<=stopSCG;i++) {
							qln = (ulong) ((-pQlnInfo[i*g] - 23*16) >> 3);
							pMsg[i-startSCG]=(qln > 254) ? 255 : qln;
						}
						rspLen+=2+stopSCG-startSCG+1;
						pMsg+=stopSCG-startSCG+1;
						g=pMib->gFactors.Gfactor_MEDLEYSETds;
						WriteCnt16(pMsg, 256);
						pMsg+=2;
						for(i=startSCG;i<=stopSCG;i++) {
							snr = (ulong) ((pSnrInfo[i*g] + 32*16) >> 3);
							pMsg[i-startSCG]=(snr > 254) ? 255 : snr;
						}
						rspLen+=2+stopSCG-startSCG+1;
						
						bRsp=true;
						break;
#endif
					}
                    case kG992p3OvhMsgCmdPMDVectorBlockRead:
                        pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
                        switch(pData[kG992p3CmdSubCode+1])
                        {
                            case kG992p3OvhMsgCmdPMDChanRspLog:
                            {
                                int startSCG,stopSCG,i,g,j;
                                unsigned long hlog,hlogLen;
                                short *pHlogInfo;
                                uchar hLogOidStr[] = { kOidAdslPrivate, kOidAdslPrivChanCharLog};
                                pHlogInfo = (void*) AdslMibGetObjectValue (gDslVars, hLogOidStr, sizeof(hLogOidStr), NULL, &hlogLen);
                                startSCG=ReadCnt16(pData+kG992p3CmdSubCode+2);
                                stopSCG=ReadCnt16(pData+kG992p3CmdSubCode+4);
                                if(startSCG>511 || stopSCG>511) {
                                    globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
                                    rspLen = 4;
                                    bRsp = true;
                                    break;
                                }
                                globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgCmdPMDVectorBlockReadRsp;
                                rspLen=4;
                                pMsg = &globalVar.txRspBuf[rspLen];
                                WriteCnt16(pMsg, 256);
                                rspLen+=2;
                                pMsg+=2;
                                g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSds;
                                i=startSCG;
                                for (j=0;j<pMib->dsNegBandPlanDiscovery.noOfToneGroups;j++)
                                {   
                                    for(;i<=stopSCG;i++) {
                                        if(i*g >  pMib->dsNegBandPlanDiscovery.toneGroups[j].endTone )
                                            break;
                                        if(i*g <  pMib->dsNegBandPlanDiscovery.toneGroups[j].startTone )
                                            hlog=0x3FF;
                                        else 
                                        { 
                                            hlog = (ulong) (((6*16-pHlogInfo[i*g])*5) >> 3);
                                            if (hlog > 0x3FC)
                                                hlog=0x3FF;
                                        }
                                        WriteCnt16(pMsg,hlog);
                                        pMsg+=2;
                                    }
                                }
                                for(;i<=stopSCG;i++) {
                                    WriteCnt16(pMsg,0x3FF);
                                    pMsg+=2;
                                }
                                rspLen+=2*(stopSCG-startSCG+1);

                                bRsp=true;
                                break;
                            }
                            case kG992p3OvhMsgCmdPMDQLineNoise:
                            {
                                int startSCG,stopSCG,i,g,j;
                                unsigned long qln,qlnLen;
                                short *pQlnInfo;
                                uchar qlnOidStr[] = { kOidAdslPrivate, kOidAdslPrivQuietLineNoise};
                                pQlnInfo = (void*) AdslMibGetObjectValue (gDslVars, qlnOidStr, sizeof(qlnOidStr), NULL, &qlnLen);
                                startSCG=ReadCnt16(pData+kG992p3CmdSubCode+2);
                                stopSCG=ReadCnt16(pData+kG992p3CmdSubCode+4);
                                if(startSCG>511 || stopSCG>511) {
                                    globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
                                    rspLen = 4;
                                    bRsp = true;
                                    break;
                                }
                                globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgCmdPMDVectorBlockReadRsp;
                                rspLen=4;
                                pMsg = &globalVar.txRspBuf[rspLen];
                                WriteCnt16(pMsg, 256);
                                rspLen+=2;
                                pMsg+=2;
                                g=pMib->gFactors.Gfactor_SUPPORTERCARRIERSds;
                                i=startSCG;
                                for (j=0;j<pMib->dsNegBandPlanDiscovery.noOfToneGroups;j++)
                                {   
                                    for(;i<=stopSCG;i++) {
                                        if(i*g >  pMib->dsNegBandPlanDiscovery.toneGroups[j].endTone )
                                            break;
                                        if(i*g <  pMib->dsNegBandPlanDiscovery.toneGroups[j].startTone )
                                            pMsg[i-startSCG]=255;
                                        else 
                                        { 
                                            qln = (ulong) ((-pQlnInfo[i*g] - 23*16) >> 3);
                                            pMsg[i-startSCG]=(qln > 254) ? 255 : (uchar)qln;
                                        }
                                    }
                                }
                                for(;i<=stopSCG;i++) 
                                    pMsg[i-startSCG]=255;
                                rspLen+=stopSCG-startSCG+1;
                                bRsp=true;
                                break;
                            }
                            case kG992p3OvhMsgCmdPMDSnr:
                            {
                                int startSCG,stopSCG,i,g,j;
                                short *pSnrInfo;
                                ulong snrLen,snr;
                                uchar snrOidStr[] = { kOidAdslPrivate, kOidAdslPrivSNR };
                                pSnrInfo = (void*) AdslMibGetObjectValue (gDslVars, snrOidStr, sizeof(snrOidStr), NULL, &snrLen);
                                startSCG=ReadCnt16(pData+kG992p3CmdSubCode+2);
                                stopSCG=ReadCnt16(pData+kG992p3CmdSubCode+4);
                                if(startSCG>511 || stopSCG>511) {
                                    globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
                                    rspLen = 4;
                                    bRsp = true;
                                    break;
                                }
                                globalVar.txRspBuf[kG992p3CmdSubCode]=kG992p3OvhMsgCmdPMDVectorBlockReadRsp;
                                rspLen=4;
                                pMsg = &globalVar.txRspBuf[rspLen];
                                WriteCnt16(pMsg, 256);
                                rspLen+=2;
                                pMsg+=2;
                                g=pMib->gFactors.Gfactor_MEDLEYSETds;
                                
                                i=startSCG;
                                for (j=0;j<pMib->dsNegBandPlan.noOfToneGroups;j++)
                                {   
                                    for(;i<=stopSCG;i++) {
                                        if(i*g >  pMib->dsNegBandPlan.toneGroups[j].endTone )
                                            break;
                                        if(i*g <  pMib->dsNegBandPlan.toneGroups[j].startTone )
                                            pMsg[i-startSCG]=255;
                                        else 
                                        { 
                                            snr = (ulong) ((pSnrInfo[i*g] + 32*16) >> 3);
                                            pMsg[i-startSCG]=(snr > 254) ? 255 : (uchar)snr;
                                        }
                                    }
                                }
                                for(;i<=stopSCG;i++) 
                                    pMsg[i-startSCG]=255;
                                rspLen+=stopSCG-startSCG+1;
                                
                                bRsp=true;
                                break;
                            }
                            default:
                                globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
                                rspLen = 4;
                                bRsp = true;
                                break;
                        }
                        break;
                        case kG992p3OvhMsgCmdPMDSingleRead:
					{
						short	val;
						globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDSingleRdRsp;
						rspLen=4;
						pMsg = &globalVar.txRspBuf[kG992p3CmdSubCode+1];
						mibLen = sizeof(adslMibInfo);
						pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
						for(n=0;n<5;n++) {
							if(n<pMib->dsNegBandPlanDiscovery.noOfToneGroups) {
								val=pMib->perbandDataDs[n].adslCurrAtn;
								val=RestrictValue(val, 0, 1023);
							}
							else
								val=1023;
							WriteCnt16(pMsg+n*2, val);
#if 0
							__SoftDslPrintf(gDslVars, "DS LATN[%d] Reported: %d/10 rspLen=%d ", 0,n, val,rspLen);
#endif
							rspLen+=2;
						}
						pMsg = &globalVar.txRspBuf[rspLen];
						for(n=0;n<5;n++) {
							if(n<pMib->dsNegBandPlan.noOfToneGroups) {
								val=pMib->perbandDataDs[n].adslSignalAttn;
								val=RestrictValue(val, 0, 1023);
							}
							else
								val=1023;
							WriteCnt16(pMsg+n*2, val);
#if 0
							__SoftDslPrintf(gDslVars, "DS SATN[%d] Reported: %d/10, rspLen=%d", 0,n, val,rspLen);
#endif
							rspLen+=2;
						}
						pMsg = &globalVar.txRspBuf[rspLen];
						val=pMib->adslPhys.adslCurrSnrMgn;
#ifdef CLIP_LARGE_SNR
						val=RestrictValue(val, -512, 511);
#else
						val=RestrictValue(val, -512, 512);
						if(val==512)
							val=-512;
#endif
						WriteCnt16(pMsg, val);
						rspLen+=2;  
						for(n=0;n<5;n++) {
							if(n<pMib->dsNegBandPlan.noOfToneGroups) {
								val=pMib->perbandDataDs[n].adslCurrSnrMgn;
#ifdef CLIP_LARGE_SNR
								val=RestrictValue(val, -512, 511);
#else
								val=RestrictValue(val, -512, 512);
								if(val==512)
									val=-512;
#endif
							}
							else
								val=-512;
							WriteCnt16(pMsg+2+n*2, val);
#if 0
							__SoftDslPrintf(gDslVars, "DS SNRM[%d] Reported: %d/10", 0,n, val);
#endif
							rspLen+=2;
						}
						pMsg = &globalVar.txRspBuf[rspLen];
						WriteCnt32(pMsg, pMib->adslPhys.adslCurrAttainableRate);
						rspLen+=4;
						pMsg = &globalVar.txRspBuf[rspLen];
						val=pMib->adslPhys.adslCurrOutputPwr;
						if(val<-310 || val>310 || val==0)
							val=-512; 
						WriteCnt16(pMsg, val);
#if 0
						__SoftDslPrintf(gDslVars, "NE Tx Pwr In MIB = %d Reported: %d/10", 0, pMib->adslPhys.adslCurrOutputPwr,val);
#endif
						rspLen+=2;
						pMsg = &globalVar.txRspBuf[rspLen];
						val=pMib->adslAtucPhys.adslCurrOutputPwr;
						if(val<-310 || val>310 || val==0)
							val=-512;
						WriteCnt16(pMsg,val);
#if 0
						__SoftDslPrintf(gDslVars, "FE Tx Pwr Reported: In MIB =%d Reported %d/10", 0,pMib->adslAtucPhys.adslCurrOutputPwr, val);
#endif
						rspLen+=2;
						if(XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
							pMsg = &globalVar.txRspBuf[rspLen];
							path0= XdslMibGetPath0MappedPathId(gDslVars, US_DIRECTION);
							path1 = XdslMibGetPath1MappedPathId(gDslVars, US_DIRECTION);
							n = pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].INP * 5;
							if(n > 2047)
								n = 2047;
							WriteCnt16(pMsg, n); rspLen+=2;
							n = MIN(pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path0].INPrein, pMib->xdslInfo.dirInfo[US_DIRECTION].lpInfo[path1].INPrein);
							globalVar.txRspBuf[rspLen] = (uchar)(n * 5); rspLen+=1;
						}
						if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
							pMsg = &globalVar.txRspBuf[rspLen];
							path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
							WriteCnt32(pMsg, pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].etrRate); rspLen+=4;
							globalVar.txRspBuf[rspLen] = (uchar)pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].delay; rspLen+=1;
						}
						bRsp = true;
						break;
					}
				}
			}
#endif
			globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdPMDRead;
#ifdef G993
			if(XdslMibIsVdsl2Mod(gDslVars))
				break;
#endif

			if(globalVar.kG992p3OvhMsgCmdPLN==pData[kG992p3CmdSubCode]) {
				__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ,pData[2]=%d pData[3]=%d pData[4]=%d",0,pData[2],pData[3],pData[4]);
				globalVar.txRspBuf[kG992p3CmdSubCode]= globalVar.kG992p3OvhMsgCmdPLN | 0x80;
				bRsp=true;
				switch(pData[kG992p3CmdId]) {
					case kDslPLNControlStart :
						globalVar.PLNActiveFlag=1;
						globalVar.PLNNoiseMarginPerBinDecLevel=pData[7];
						globalVar.PLNNoiseMarginPerBinDecLevel<<=8;
						globalVar.PLNNoiseMarginPerBinDecLevel=globalVar.PLNNoiseMarginPerBinDecLevel|pData[8];
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN Start,PLNNoiseMarginDecLevel CMDLEN=%u 0x%x 0x%x 0x%x %u",0,globalVar.PLNNoiseMarginPerBinDecLevel,pData[7],pData[8],globalVar.PLNNoiseMarginPerBinDecLevel,cmdLen);
						globalVar.PLNNoiseMarginBroadbandDecLevel=pData[5];
						globalVar.PLNNoiseMarginBroadbandDecLevel<<=8;
						globalVar.PLNNoiseMarginBroadbandDecLevel=globalVar.PLNNoiseMarginBroadbandDecLevel|pData[6];
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN Start,PLNNoiseMarginBroadbandDecLevel=%u 0x%x 0x%x 0x%x",0,globalVar.PLNNoiseMarginBroadbandDecLevel,pData[5],pData[6],globalVar.PLNNoiseMarginBroadbandDecLevel);
						if(cmdLen>12) {
							globalVar.PLNnInpBin=pData[9];
							n=2*globalVar.PLNnInpBin;
							if(NULL!=(globalVar.pPLNinpBinIntervalPtr=AdslCoreSharedMemAlloc(n))) {
								__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN .pPLNinpBinIntervalPtr %X n= %d",0,globalVar.pPLNinpBinIntervalPtr,n);
								memcpy(globalVar.pPLNinpBinIntervalPtr,&pData[10],n);
							}
							
							globalVar.PLNnItaBin=pData[10+2*globalVar.PLNnInpBin];
							n=2*globalVar.PLNnItaBin;
							if(NULL!=(globalVar.pPLNitaBinIntervalPtr=AdslCoreSharedMemAlloc(n))) {
								__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN .pPLNitaBinIntervalPtr %X n= %d",0,globalVar.pPLNitaBinIntervalPtr,n);
								memcpy(globalVar.pPLNitaBinIntervalPtr,&pData[10+2*globalVar.PLNnInpBin+1],n);
							}
						}
						
						globalVar.pPLNPeakNoiseTable=NULL;
						globalVar.pPLNThresholdCountTable=NULL;

						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN  Start",0);
						globalVar.PLNPerBinPeakNoiseMsrCounter=0;
						globalVar.PLNPerBinThldCounter=0;
						globalVar.PLNImpulseDurStatCounter=0;
						globalVar.PLNImpulseInterArrStatCounter=0;
						globalVar.PLNElapsedTimeMSec=0;

						cmd.command=kDslPLNControlCmd;
						if(cmdLen<13) {
							globalVar.PLNnInpBin=0;
							globalVar.PLNnItaBin=0; 
						}
						
						cmd.command=kDslPLNControlCmd;
						cmd.param.value=kDslPLNControlStop;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);

						cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineInpBinTable;
						cmd.param.dslPlnSpec.nInpBin= globalVar.PLNnInpBin;
						cmd.param.dslPlnSpec.inpBinPtr=globalVar.pPLNinpBinIntervalPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						
						cmd.command=kDslPLNControlCmd;
						cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineItaBinTable;
						cmd.param.dslPlnSpec.nItaBin=globalVar.PLNnItaBin;
						cmd.param.dslPlnSpec.itaBinPtr= globalVar.pPLNitaBinIntervalPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);

						cmd.command=kDslPLNControlCmd;
						cmd.param.dslPlnSpec.plnCmd=kDslPLNControlStart;
						cmd.param.dslPlnSpec.mgnDescreaseLevelPerBin= globalVar.PLNNoiseMarginPerBinDecLevel;
						cmd.param.dslPlnSpec.mgnDescreaseLevelBand=globalVar.PLNNoiseMarginBroadbandDecLevel;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd); 

						globalVar.txRspBuf[4]=kDslPLNControlStart;
						rspLen++;
						break;
					case kDslPLNControlStop:
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN Read Stop",0);
						cmd.command=kDslPLNControlCmd;
						cmd.param.value=kDslPLNControlStop;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						globalVar.txRspBuf[4]=kDslPLNControlStop;
						globalVar.PLNActiveFlag=0;
						rspLen++;
						break;
					case kDslPLNControlReadPerBinPeakNoise:   
						cmd.command=kDslPLNControlCmd;
						cmd.param.value=kDslPLNControlPeakNoiseGetPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						bRsp=false;
						globalVar.PLNMessageReadRspFlag=1;
						globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadPerBinPeakNoise",0);
						break;
					case kDslPLNControlReadPerBinThresholdCount:
						cmd.command=kDslPLNControlCmd;
						cmd.param.value=kDslPLNControlThldViolationGetPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						bRsp=false;
						globalVar.PLNMessageReadRspFlag=1;
						globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadPerBinThresholdCount",0);
						break;
					case kDslPLNControlReadImpulseDurationStat:
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadImpulseDurationStat",0);
						cmd.command=kDslPLNControlCmd;
						cmd.param.value=kDslPLNControlImpulseNoiseEventGetPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						globalVar.PLNMessageReadRspFlag=1;
						globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
						bRsp=false;  
						break;
					case kDslPLNControlReadImpulseInterArrivalTimeStat:
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadImpulseInterArrivalTimeStat",0);
						cmd.command=kDslPLNControlCmd; 
						cmd.param.value=kDslPLNControlImpulseNoiseTimeGetPtr;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						globalVar.PLNMessageReadRspFlag=1;
						globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
						bRsp=false;
						break;
					case  kDslPLNControlPLNStatus :
						__SoftDslPrintf(gDslVars, "G992p3Ovh CMD get PLN Status",0);
						cmd.command=kDslPLNControlCmd; 
						cmd.param.value= kDslPLNControlGetStatus;
						(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
						globalVar.PLNMessageReadRspFlag=1;
						globalVar.txRspMsgNumStored=globalVar.txRspMsgNum^1;
						bRsp=false;
						break;
				}
				rspLen+=4;
				break;
			}

			if(kG992p3OvhMsgCmdPMDSingleRead != pData[kG992p3CmdSubCode]) {
#ifdef G993_DBG_PRINT
				__SoftDslPrintf(gDslVars, "G992p3OvhMsg Not Single Read",0);
#endif
				globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
				rspLen = 4;
				bRsp = true;
				break;
			}

			globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDSingleRdRsp;
			pMsg = &globalVar.txRspBuf[kG992p3CmdSubCode+1];
			rspLen = 0;
			mibLen = sizeof(adslMibInfo);
			pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
			switch (pData[kG992p3CmdId]) { 
				case kG992p3OvhMsgCmdPMDPeriod:
					  WriteCnt16(pMsg+0, 0x100);
					  rspLen = 2;
					  break;
				case kG992p3OvhMsgCmdPMDChanRspLog:
				{
					ulong chLogLen, val;
					short *pChLog;
					uchar chLogOidStr[] = { kOidAdslPrivate, kOidAdslPrivChanCharLog };
					
					WriteCnt16(pMsg+0, 0x100);
					pMsg+=2;
					pChLog = (void*) AdslMibGetObjectValue (gDslVars, chLogOidStr, sizeof(chLogOidStr), NULL, &chLogLen);
					for (i = 0; i < (chLogLen >> 1); i++) {
						val = (ulong) (((6*16 - pChLog[i]) * 5) >> 3);
						if (val > 0x3FC)
							val = 0x3FF;
						WriteCnt16(pMsg, val);
						pMsg += 2;
					}
					rspLen = chLogLen + 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDQLineNoise:
				{
					ulong qlnLen, qln;
					short *pQlnInfo;
					uchar qlnOidStr[] = { kOidAdslPrivate, kOidAdslPrivQuietLineNoise };
					
					WriteCnt16(pMsg+0, 0x100);
					pMsg+=2;
					pQlnInfo = (void*) AdslMibGetObjectValue (gDslVars, qlnOidStr, sizeof(qlnOidStr), NULL, &qlnLen);
					qlnLen >>= 1;
					for (i = 0; i < qlnLen; i++) {
						qln = (ulong) ((-pQlnInfo[i] - 23*16) >> 3);
						pMsg[i] = (qln > 254) ? 255 : qln;
					}
					rspLen = qlnLen + 2;
				}
					break;     
				case kG992p3OvhMsgCmdPMDSnr:
				{
					ulong snrLen, snr;
					short *pSnrInfo;
					uchar snrOidStr[] = { kOidAdslPrivate, kOidAdslPrivSNR };

					WriteCnt16(pMsg+0, 0x100);
					pMsg+=2;
					pSnrInfo = (void*) AdslMibGetObjectValue (gDslVars, snrOidStr, sizeof(snrOidStr), NULL, &snrLen);
					snrLen >>= 1;
					for (i = 0; i < snrLen; i++) {
					snr = (ulong) ((pSnrInfo[i] + 32*16) >> 3);
					pMsg[i] = (snr > 254) ? 255 : snr;
					}
					rspLen = snrLen + 2;
				}
					break;

				case kG992p3OvhMsgCmdPMDLnAttn:
				{
					long val;
					val=pMib->adslPhys.adslCurrAtn;
					val=RestrictValue(val, 0, 1023);
					WriteCnt16(pMsg+0, val);
					rspLen = 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDSigAttn:
				{
					long val;
					val=pMib->adslPhys.adslSignalAttn;
					val=RestrictValue(val, 0, 1023);
					WriteCnt16(pMsg+0, val);
					rspLen = 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDSnrMgn:
				{
					long val;
					val=pMib->adslPhys.adslCurrSnrMgn;
#ifdef CLIP_LARGE_SNR
					val=RestrictValue(val, -512, 511);
#else
					val=RestrictValue(val, -512, 512);
					if(val==512)
						val=-512;
#endif
					WriteCnt16(pMsg+0, val);
					rspLen = 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDAttnDR:
					WriteCnt32(pMsg+0, pMib->adslPhys.adslCurrAttainableRate);
					rspLen = 4;
					break;
				case kG992p3OvhMsgCmdPMDNeXmtPower:
				{
					long val;
					val=pMib->adslPhys.adslCurrOutputPwr;
					if(val<-310 || val >310)
						val=-512;
					WriteCnt16(pMsg+0,val);
					rspLen = 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDFeXmtPower:
				{
					long val;
					val=pMib->adslAtucPhys.adslCurrOutputPwr;
					if(val<-310 || val >310)
						val=-512;
					WriteCnt16(pMsg+0,val);
					rspLen = 2;
				}
					break;
				case kG992p3OvhMsgCmdPMDSingleReadETRId:
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
						path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
						WriteCnt32(pMsg, pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].etrRate); rspLen+=4;
					}
					else
						globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
					break;
				case kG992p3OvhMsgCmdPMDSingleReadDelayId:
					if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
						path1 = XdslMibGetPath1MappedPathId(gDslVars, DS_DIRECTION);
						pMsg[rspLen] = (uchar)pMib->xdslInfo.dirInfo[DS_DIRECTION].lpInfo[path1].delay; rspLen+=1;
					}
					else
						globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
					break;
				default:
					globalVar.txRspBuf[kG992p3CmdSubCode] = kG992p3OvhMsgCmdPMDReadNACK;
					break;
			}
			rspLen += 4;
			
			bRsp = true;
			break;
		
		case kG992p3OvhMsgCmdOLR:
		{
			uchar *pSharedMem;
			int bufOffset;
			mibLen = sizeof(adslMibInfo);
#if defined(CONFIG_VDSL_SUPPORTED)
			if( (kVdslModVdsl2 == modType) &&
				((kG992p3OvhMsgCmdOLRReq6 == pData[kG992p3CmdSubCode]) ||
				(kG992p3OvhMsgCmdOLRReq7 == pData[kG992p3CmdSubCode])) ) {
				bufOffset =4;
				n = pFrame->totalLength - bufOffset;
				pSharedMem = (uchar *)AdslCoreSharedMemAlloc(n);	/* AdslCoreSharedMemAlloc always return a valid address */
				bufOffset = FrameDataCopy(gDslVars, pBuf, bufOffset, n, pSharedMem);
				cmd.command = kDslSendEocCommand;
				if(kG992p3OvhMsgCmdOLRReq6 == pData[kG992p3CmdSubCode])
					cmd.param.dslClearEocMsg.msgId = kDslVdslSraReqRecvd;
				else
					cmd.param.dslClearEocMsg.msgId = kDslVdslSosReqRecvd;
				cmd.param.dslClearEocMsg.msgType = n & kDslClearEocMsgLengthMask;
				cmd.param.dslClearEocMsg.dataPtr = (char *)pSharedMem;
				(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
				pMib->adslStat.bitswapStat.xmtCntReq++;
				XdslMibNotifyBitSwapReq(gDslVars, US_DIRECTION);
				__SoftDslPrintf(gDslVars, "Drv: VDSL %s Request received, msgPtr=%p, msgLen=%d frameCopyLen=%d\n", 0,
								(kG992p3OvhMsgCmdOLRReq6 == pData[kG992p3CmdSubCode])? "SRA": "SOS",
								pSharedMem, n, bufOffset);
				break;
			}
#endif
			cmd.command = kDslOLRRequestCmd;
			cmd.param.dslOLRRequest.msgType = pData[kG992p3CmdSubCode];
			pMsg = pData+kG992p3CmdSubCode+1;
			switch (cmd.param.dslOLRRequest.msgType) {
				case kG992p3OvhMsgCmdOLRReq2:
				case kG992p3OvhMsgCmdOLRReq3:
				case kG992p3OvhMsgCmdOLRReq5:
				case kG992p3OvhMsgCmdOLRReq6:
					cmd.param.dslOLRRequest.L[0] = ReadCnt16(pMsg);
					cmd.param.dslOLRRequest.B[0] = pMsg[2];
					pMsg += 3;
					/* fall through */
				case kG992p3OvhMsgCmdOLRReq1:
				case kG992p3OvhMsgCmdOLRReq4:
					if ( (kAdslModAdsl2p == modType) || (kVdslModVdsl2 == modType) ){
						cmd.param.dslOLRRequest.nCarrs = ReadCnt16(pMsg);
						pMsg +=  2;
						cmd.param.dslOLRRequest.carrParamPtr = pMsg;
					}
					else {
						cmd.param.dslOLRRequest.nCarrs  = pMsg[0];
						pMsg += 1;
						cmd.param.dslOLRRequest.carrParamPtr = pMsg;
					}

					if ((cmd.param.dslOLRRequest.nCarrs != 0) && (cmdLen > (ulong)(pMsg-pData))) {
						int pri;

						n = cmd.param.dslOLRRequest.nCarrs * (((kAdslModAdsl2p == modType) || (kVdslModVdsl2 == modType)) ? 
							sizeof(dslOLRCarrParam2p) : sizeof(dslOLRCarrParam));
						if (kVdslModVdsl2 == modType)
							n++;  /* Include segment terminating byte for VDSL */
						if (NULL != (pSharedMem = AdslCoreSharedMemAlloc(n))) {
							bufOffset = (int)cmd.param.dslOLRRequest.carrParamPtr - (int)pData;
							pri = pData[kG992p3AddrField] & kG992p3PriorityMask;
							
							if(globalVar.olrSegmentSerialNum || globalVar.segCmd[pri].segId) {
								if(globalVar.olrSegmentSerialNum)
									bufOffset = MultiFrameDataCopy(gDslVars, &globalVar.g992P3OlrFrameList, bufOffset, n, pSharedMem);
								else
									bufOffset = MultiFrameDataCopy(gDslVars, &globalVar.segCmd[pri].segFrameList, bufOffset, n, pSharedMem);
							}
							else
								bufOffset = FrameDataCopy(gDslVars, pBuf, bufOffset, n, pSharedMem);
							
							cmd.param.dslOLRRequest.carrParamPtr = pSharedMem;
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
							pMib->adslStat.bitswapStat.xmtCntReq++;
							XdslMibNotifyBitSwapReq(gDslVars, US_DIRECTION);
							__SoftDslPrintf(gDslVars, "%s: OLR subCmd=%x, shareMemLen=%d frameCopyLen=%d\n",
								0, __FUNCTION__, pData[kG992p3CmdSubCode], n, bufOffset);
						}
					}
					break;
			}
			break;
		}
		case kG992p3OvhMsgCmdPower:
			cmd.command = kDslPwrMgrCmd;
			cmd.param.dslPwrMsg.msgType = pData[kG992p3CmdSubCode];
			switch (cmd.param.dslPwrMsg.msgType) {
				case kPwrL2TrimRequest:
					cmd.param.dslPwrMsg.param.value = pData[kG992p3CmdSubCode+1];
					(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
					break;
				case kPwrL2Request:
				{
					uchar *pSharedMem=NULL;
					int cmdPri, firstBufOffset = kG992p3CmdSubCode+1;

					cmdPri = pData[kG992p3AddrField] & kG992p3PriorityMask;

					if( globalVar.segCmd[cmdPri].segId ) {
						cmdLen = GetMultiFrameLen(gDslVars, &globalVar.segCmd[cmdPri].segFrameList);
						if(cmdLen)
							pSharedMem = AdslCoreSharedMemAlloc(cmdLen);
					}
					else
						pSharedMem = AdslCoreSharedMemAlloc(cmdLen);
					
					if (NULL == pSharedMem )
						break;
					
					if( globalVar.segCmd[cmdPri].segId )
						cmdLen = MultiFrameDataCopy(gDslVars, &globalVar.segCmd[cmdPri].segFrameList, firstBufOffset, cmdLen-firstBufOffset, pSharedMem);
					else
						cmdLen = FrameDataCopy(gDslVars, pBuf, firstBufOffset, cmdLen-firstBufOffset, pSharedMem);
					cmd.param.dslPwrMsg.param.msg.msgLen = cmdLen;
					cmd.param.dslPwrMsg.param.msg.msgData = pSharedMem;
					(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
				}
					break;
				case kPwrSimpleRequest:
				{
					globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdPower;
					if (3 == pData[kG992p3CmdSubCode+1]) {
						globalVar.txFlags |= kTxCmdL3RspWait;
						globalVar.timeRspOut = globalVar.timeMs;
						globalVar.txRspBuf[kG992p3CmdSubCode] = kPwrGrant;
						rspLen = 4;
					}
					else {
						globalVar.txRspBuf[kG992p3CmdSubCode] = kPwrReject;
						globalVar.txRspBuf[kG992p3CmdSubCode+1] = kPwrInvalid;
						rspLen = 5;
					}
						bRsp = true;
				}
					break;
			}
			break;

		default:
			break;
	}

	if (bRsp) {
		globalVar.txRspMsgNum ^= 1;
		globalVar.txRspBuf[kG992p3AddrField] = pData[kG992p3AddrField];
		globalVar.txRspBuf[kG992p3CtrlField] = kG992p3Rsp | globalVar.txRspMsgNum;
		if( (globalVar.setup & kG992p3ClearEocWorkaround) != 0 ) {
			globalVar.txRspBuf[kG992p3CtrlField] = kG992p3Rsp | G992p3OvhMsgGetCmdNum(gDslVars, pData, true);
			globalVar.txRspMsgNum ^= 1;
		}
		G992p3OvhMsgXmtSendRsp(gDslVars, rspLen);
		globalVar.cmdPollingStamp = 0;
	}

	return bRsp;
}

Boolean G992p3OvhMsgRcvProcessSegAck(void *gDslVars, dslFrameBuffer *pBuf, uchar *pData, ulong cmdLen)
{
#if 1 || defined(G992P3_DBG_PRINT)
  __SoftDslPrintf(gDslVars, "G992p3OvhMsgRcvProcessSegAck: code=0x%X, subCode=0x%X, segId=%d, 0x%X, 0x%X\n", 0,
    pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], pData[kG992p3CmdSubCode+1],
    pData[kG992p3CmdSubCode+2], pData[kG992p3CmdSubCode+3]);
#endif

  if (NULL == globalVar.txSegFrameCtl.segFrame)
    return false;

  if (globalVar.txSegFrameCtl.segId == pData[kG992p3CmdSubCode+1]) {
    if (G992p3IsFrameBusy(gDslVars, globalVar.txSegFrameCtl.segFrame)) {
      G992p3FrameBitSet(gDslVars, globalVar.txSegFrameCtl.segFrame, kFrameNextSegPending);
      __SoftDslPrintf(gDslVars, "G992p3OvhMsgRcvProcessSegAck: Mark pending\n", 0);
    }
    else
      G992p3OvhMsgXmtSendNextSeg(gDslVars, &globalVar.txSegFrameCtl);
    return true;
  }
  return false;
}

static int G992p3OvhMsgRcvProcessSeg(
	void *gDslVars,
	dslFrame *pFrame,
	dslFrameBuffer *pBuf,
	uchar *pData,
	int len,
	uchar segFlagAndId)
{
	uchar ctlField, flag, segId, cmdOrRsp, cmdOrRspPri;
	g992p3RcvSegFrameCtlStruct *pSegFrameCtl;
	
	flag = (segFlagAndId  >> 6 ) & 3;
	segId = (segFlagAndId >> 3) & 7;
	cmdOrRsp = pData[kG992p3CtrlField] & kG992p3CmdRspMask;
	cmdOrRspPri = pData[kG992p3AddrField] & kG992p3PriorityMask;

	if(pData[kG992p3CtrlField] & 0x4) {
		__SoftDslPrintf(gDslVars, "%s: Received invalid segmented message\n", 0, __FUNCTION__);
		return 1;
	}

#ifdef DEBUG_PRINT_RCV_SEG
	__SoftDslPrintf(gDslVars, "%s: pFrame=0x%x pBuf=0x%x, pData=0x%x, len=%d, CmdOrRsp = %s, CmdCode = %x\n",
		0, __FUNCTION__, (long)pFrame, (long)pBuf, (long)pData, len, (kG992p3Cmd == cmdOrRsp )? "CMD":"RSP", pData[kG992p3CmdCode]);
#endif
	if( kG992p3Cmd == cmdOrRsp )
		pSegFrameCtl = &globalVar.segCmd[cmdOrRspPri];
	else
		pSegFrameCtl = &globalVar.segRsp[cmdOrRspPri];
	
	if( 2 == flag ) {	/* Either the first or last segment */
		if(segId >= 1) {	/* First segment */
#ifdef DEBUG_PRINT_RCV_SEG
			__SoftDslPrintf(gDslVars, "%s: Received first segment, cmd=%x subCmd=%x, segId=%d pSegFrameCtl->segId=%d\n",
				0, __FUNCTION__, pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], segId, pSegFrameCtl->segId);
#endif
			if (G992p3IsFrameBusy(gDslVars, &pSegFrameCtl->txRspFrame))
				return 1;
			
			if( pSegFrameCtl->segId != 0 )
				G992p3OvhMsgFlushFrameList(gDslVars, &pSegFrameCtl->segFrameList);
			pSegFrameCtl->pFirstSegData = pData;
			pSegFrameCtl->pFirstSegBuf = pBuf;
			DListInit(&pSegFrameCtl->segFrameList);
			DListInsertTail(&pSegFrameCtl->segFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
		}
		else if ( (0 == segId) && (1 == pSegFrameCtl->segId) ) {	/* Last segment */
			int dataLen;
#ifdef DEBUG_PRINT_RCV_SEG
			__SoftDslPrintf(gDslVars, "%s: Received last segment, cmd=%x subcmd=%x len=%d\n",
				0, __FUNCTION__, pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], len);
#endif
			DslFrameBufferSetLength(gDslVars, pBuf, len-4);
			DslFrameBufferSetAddress(gDslVars, pBuf, pData+4);
			G992p3SetFrameInfoVal(gDslVars, pFrame, 4);
			DListInsertTail(&pSegFrameCtl->segFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
			pData = pSegFrameCtl->pFirstSegData;
			len = GetMultiFrameLen(gDslVars, &pSegFrameCtl->segFrameList);

			if( kG992p3Cmd == cmdOrRsp ) {
				dataLen = DslFrameBufferGetLength(gDslVars, pSegFrameCtl->pFirstSegBuf);
				switch(pData[kG992p3CmdCode]) {
					case kG992p3OvhMsgCmdOLR:
						break;
					case kG992p3OvhMsgCmdPower:
						if( kPwrL2Request == pData[kG992p3CmdSubCode] )
							break;
					default:
						pData = AdslCoreSharedMemAlloc(len);
						dataLen = MultiFrameDataCopy(gDslVars, &pSegFrameCtl->segFrameList, 0, len, pData);
						break;
				}
				G992p3OvhMsgRcvProcessCmd(gDslVars, pSegFrameCtl->pFirstSegBuf, pData, dataLen, pFrame);
			}
			else {
				pData = AdslCoreSharedMemAlloc(len);
				dataLen = MultiFrameDataCopy(gDslVars, &pSegFrameCtl->segFrameList, 0, len, pData);
				G992p3OvhMsgRcvProcessRsp(gDslVars, pSegFrameCtl->pFirstSegBuf, pData, dataLen);
#if 0 && defined(DEBUG_PRINT_RCV_SEG)
				G992p3OvhMsgPrintAssembledSeg(pData, dataLen);
#endif
			}
#ifdef DEBUG_PRINT_RCV_SEG
			__SoftDslPrintf(gDslVars, "%s: Processed %s multiFrameLen=%d dataLen=%d\n",
				0, __FUNCTION__, ( kG992p3Cmd == cmdOrRsp )? "CMD":"RSP", len, dataLen);
#endif
			G992p3OvhMsgFlushFrameList(gDslVars, &pSegFrameCtl->segFrameList);
			pSegFrameCtl->segId =0;
			
			return 2;
		}
		else {
			__SoftDslPrintf(gDslVars, "%s: Received invalid first/last segment message, segId=%d, expected segId=%d\n",
				0, __FUNCTION__, segId, pSegFrameCtl->segId-1);
			return 1;
		}
	}
	else {		/* Intermediate segment */
		if( (0 == flag) && (pSegFrameCtl->segId == (segId + 1)) ) {
			if (G992p3IsFrameBusy(gDslVars, &pSegFrameCtl->txRspFrame))
				return 1;
			DslFrameBufferSetLength(gDslVars, pBuf, len-4);
			DslFrameBufferSetAddress(gDslVars, pBuf, pData+4);
			G992p3SetFrameInfoVal(gDslVars, pFrame, 4);
			DListInsertTail(&pSegFrameCtl->segFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
		}
		else {
			__SoftDslPrintf(gDslVars, "%s: Received invalid intermediate segment message, flag=%d, segId=%d, expected segId=%d\n",
				0, __FUNCTION__, flag, segId, pSegFrameCtl->segId-1);
			return 1;
		}
	}

	pSegFrameCtl->segId = segId;
	pSegFrameCtl->lastSegRcvTime = globalVar.timeMs;

	if( kG992p3Cmd == cmdOrRsp ) {
		globalVar.txRspMsgNum ^= 1;
		ctlField = kG992p3Rsp | globalVar.txRspMsgNum;
	}
	else 
		ctlField = kG992p3Cmd | G992p3OvhMsgGetCmdNum(gDslVars, pData, true);
	
	pSegFrameCtl->txRspBuf[kG992p3AddrField] = pData[kG992p3AddrField];
	pSegFrameCtl->txRspBuf[kG992p3CtrlField] = ctlField;
	pSegFrameCtl->txRspBuf[kG992p3CmdCode] = 0xF0 | cmdOrRspPri;
	pSegFrameCtl->txRspBuf[kG992p3CmdSubCode] = 0x01;
	pSegFrameCtl->txRspBuf[kG992p3CmdSubCode+1] = segId;
	pSegFrameCtl->txRspBuf[kG992p3CmdSubCode+2] = pData[kG992p3CmdCode];
	pSegFrameCtl->txRspBuf[kG992p3CmdSubCode+3] = pData[kG992p3CmdSubCode];
	G992p3OvhMsgRcvSegXmtSendRsp(gDslVars, pSegFrameCtl);
	
	return 2;
}

static int G992p3OvhMsgRcvProcessSegOLRCmd(
	void *gDslVars,
	dslFrame *pFrame,
	dslFrameBuffer *pBuf,
	uchar *pData,
	int len,
	uchar SC)
{	
	ushort prevCarrsNum;
	uchar *pMsg = pData+kG992p3CmdSubCode+1;
	ushort nCarrs = ReadCnt16(pMsg);

	if ( 0xC0 == (0xC0 & SC) ) {		/* Last segment */
		if(globalVar.olrSegmentSerialNum == (SC&0x3F)) {
			prevCarrsNum = ReadCnt16(globalVar.pFirstOlrData+kG992p3CmdSubCode+1);
			WriteCnt16((globalVar.pFirstOlrData+kG992p3CmdSubCode+1),(prevCarrsNum+nCarrs));
			DslFrameBufferSetLength(gDslVars, pBuf, len-6);
			DslFrameBufferSetAddress(gDslVars, pBuf, pData+6);
			G992p3SetFrameInfoVal(gDslVars, pFrame, 6);
			DListInsertTail(&globalVar.g992P3OlrFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
			G992p3OvhMsgRcvProcessCmd(gDslVars, globalVar.pFirstOlrBuf, globalVar.pFirstOlrData,
								DslFrameBufferGetLength(gDslVars, globalVar.pFirstOlrBuf), pFrame);
			globalVar.olrSegmentSerialNum=0;
			G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.g992P3OlrFrameList);
			__SoftDslPrintf(gDslVars, "%s: Received last segmented OLR cmd, nCarrs=%d segId=%d\n", 0, __FUNCTION__, nCarrs, (0x3F&SC));
		}
		else {
			__SoftDslPrintf(gDslVars, "%s: Received last segmented OLR cmd with segId=%d, expected segId=%d\n",
				0, __FUNCTION__, (0x3F&SC), globalVar.olrSegmentSerialNum);
			return 1;
		}
	}
	else {
		dslFrameBuffer *pLastBuf;
		if ( 0x00 == SC ) {		/* First segment */
			globalVar.pFirstOlrData=pData;
			globalVar.pFirstOlrBuf=pBuf;
			if(globalVar.olrSegmentSerialNum)
				G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.g992P3OlrFrameList);
			DListInit(&globalVar.g992P3OlrFrameList);
			DListInsertTail(&globalVar.g992P3OlrFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
			globalVar.olrSegmentSerialNum=1;
			__SoftDslPrintf(gDslVars, "%s: Received first segmented OLR cmd, nCarrs=%d\n", 0, __FUNCTION__, nCarrs);
		}
		else {					/* Intermediate segments */
			if(globalVar.olrSegmentSerialNum == (SC&0x3F)) {
				globalVar.olrSegmentSerialNum++;
				prevCarrsNum = ReadCnt16(globalVar.pFirstOlrData+kG992p3CmdSubCode+1);
				WriteCnt16((globalVar.pFirstOlrData+kG992p3CmdSubCode+1),(prevCarrsNum+nCarrs));
				DslFrameBufferSetLength(gDslVars, pBuf, len-6);
				DslFrameBufferSetAddress(gDslVars, pBuf, pData+6);
				G992p3SetFrameInfoVal(gDslVars, pFrame, 6);
				DListInsertTail(&globalVar.g992P3OlrFrameList, DslFrameGetLinkFieldAddress(gDslVars, pFrame));
			}
			else {
				__SoftDslPrintf(gDslVars, "%s: Segmented OLR cmd with SerialNumcode=%d, expected SerialNum=%d\n",
					0, __FUNCTION__, (0x3F&SC), globalVar.olrSegmentSerialNum);
				return 1;
			}
		}
		
		pLastBuf = DslFrameGetLastBuffer(gDslVars, pFrame);
		DslFrameBufferSetLength(gDslVars, pLastBuf, DslFrameBufferGetLength(gDslVars, pLastBuf)-1);
		globalVar.lastOlrSegmentRcvTime=globalVar.timeMs;
		
		globalVar.txRspMsgNum ^= 1;
		globalVar.txRspBuf[kG992p3AddrField] = pData[kG992p3AddrField];
		globalVar.txRspBuf[kG992p3CtrlField] = kG992p3Rsp | globalVar.txRspMsgNum;
		globalVar.txRspBuf[kG992p3CmdCode] = pData[kG992p3CmdCode];
		globalVar.txRspBuf[kG992p3CmdSubCode] = 0x8B;
		globalVar.txRspBuf[kG992p3CmdSubCode+1] = SC;
		G992p3OvhMsgXmtSendRsp(gDslVars, 5);
	}
	
	return 2;
}


Public int G992p3OvhMsgIndicateRcvFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
	dslFrameBuffer	*pBuf;
	uchar			*pData, cmd, segFlagAndId;
	int				len;
	Boolean			bRes;

	if (!ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
	return 0;

	G992p3SetFrameInfoVal(gDslVars, pFrame, 0);
	pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
	if (NULL == pBuf)
	return 1;

	len = DslFrameBufferGetLength(gDslVars, pBuf);
	pData = DslFrameBufferGetAddress(gDslVars, pBuf);
	if ((len < 4) || (NULL == pData))
	return 1;
	
	segFlagAndId = pData[kG992p3CtrlField] & 0xF8;
	cmd = pData[kG992p3CmdCode];
	
#ifdef G992P3_DBG_PRINT
	__SoftDslPrintf(gDslVars, "%s: pFrame = 0x%x, pBuf = 0x%x, cmd = %d, frameLen=%d\n",
				0, __FUNCTION__, pFrame, pBuf, cmd, pFrame->totalLength);
#endif

	if ((kG992p3OvhSegMsgAckHi == cmd) ||(kG992p3OvhSegMsgAckNormal == cmd) ||
		(kG992p3OvhSegMsgAckLow == cmd))
		bRes = G992p3OvhMsgRcvProcessSegAck(gDslVars, pBuf, pData, len);
	else if (segFlagAndId)			/* Segmented OH Messages */
		return G992p3OvhMsgRcvProcessSeg(gDslVars,pFrame,pBuf,pData,len,segFlagAndId);
	else if (kG992p3Cmd == (pData[kG992p3CtrlField] & kG992p3CmdRspMask)) {
		if(XdslMibIsVdsl2Mod(gDslVars) && (kG992p3OvhMsgCmdOLR==cmd) &&
			(kG992p3OvhMsgCmdOLRReq4==pData[kG992p3CmdSubCode])) {
			uchar SC;
			__SoftDslPrintf(gDslVars, "%s: Received OLR type 1 cmd frameLen=%d\n",
				0, __FUNCTION__, pFrame->totalLength);
			SC = G992p3OvhMsgGetSC(gDslVars, pBuf, pFrame->totalLength);
			if( 0xC0 == SC ) {		/* Not a segmented OLR command */
				if(globalVar.olrSegmentSerialNum) {
					G992p3OvhMsgFlushFrameList(gDslVars, &globalVar.g992P3OlrFrameList);
					globalVar.olrSegmentSerialNum=0;
				}
				bRes = G992p3OvhMsgRcvProcessCmd(gDslVars, pBuf, pData, len, pFrame);
			}
			else 
				return G992p3OvhMsgRcvProcessSegOLRCmd(gDslVars,pFrame,pBuf,pData,len,SC);
		}
		else {
			if(pFrame->totalLength > len) {		/* Need to flatten buffers */
				pData = AdslCoreSharedMemAlloc(pFrame->totalLength);
				len = FrameDataCopy(gDslVars, pBuf, 0, pFrame->totalLength, pData);
			}
			bRes = G992p3OvhMsgRcvProcessCmd(gDslVars, pBuf, pData, len, pFrame);
		}
	}
	else {
		if(pFrame->totalLength > len) {		/* Need to flatten buffers */
			__SoftDslPrintf(gDslVars, "%s: Flatten buffers for cmd=%x subCmd=%x, totalLen=%d 1stBufLen=%d\n",
				0, __FUNCTION__, pData[kG992p3CmdCode], pData[kG992p3CmdSubCode], pFrame->totalLength, len);
			pData = AdslCoreSharedMemAlloc(pFrame->totalLength);
			len = FrameDataCopy(gDslVars, pBuf, 0, pFrame->totalLength, pData);
		}
		bRes = G992p3OvhMsgRcvProcessRsp(gDslVars, pBuf, pData, len);
	}
	
	if ((kG992p3OvhMsgCmdClearEOC == pData[kG992p3CmdCode]) &&
		(kG992p3OvhMsgCmdClearEOCMsg == pData[kG992p3CmdSubCode]))
	{
		DslFrameBufferSetLength(gDslVars, pBuf, len-4);
		DslFrameBufferSetAddress(gDslVars, pBuf, pData+4);
		G992p3SetFrameInfoVal(gDslVars, pFrame, 4);
		return 0;
	}
	
	return 1;
}

Public void G992p3OvhMsgReturnFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
  dslFrameBuffer    *pBuf;
  uchar       *pData;
  int         len;
  ulong       val = G992p3GetFrameInfoVal(gDslVars, pFrame);

  // __SoftDslPrintf(NULL, "ClearEOC frame returned: pFr=0x%X val=%d\n", 0, pFrame, val);
  if (0 == val)
    return;

  pBuf = DslFrameGetFirstBuffer(gDslVars, pFrame);
  if (NULL == pBuf)
    return;

  len   = DslFrameBufferGetLength(gDslVars, pBuf);
  pData = DslFrameBufferGetAddress(gDslVars, pBuf);
  // __SoftDslPrintf(NULL, "ClearEOC frame returned: len=%d pData=0x%X\n", 0, len, pData);

  DslFrameBufferSetLength(gDslVars, pBuf, len + val);
  DslFrameBufferSetAddress(gDslVars, pBuf, pData - val);
  G992p3SetFrameInfoVal(gDslVars, pFrame, 0);
}

Public void G992p3OvhMsgSetL3(void *gDslVars)
{
  if (0 == (globalVar.txFlags & kTxCmdL3WaitingAck))
    globalVar.txL3Rq = true;
}

Public void G992p3OvhMsgSetL0(void *gDslVars)
{
  if ((globalVar.txFlags & kTxCmdL0WaitingAck) || (0 == AdslMibPowerState(gDslVars)))
    return;

  globalVar.txL0Rq = true;
}
Public void INMReadParamsResponse(void *gDslVars)
{
	int rspLen=8;
	short *pPLNIntrArvlBins;
	ulong dataLen;
	ushort INMIATO,INMIATS;
	uchar OidStr[] = { kOidAdslPrivate, kOidAdslPrivPLNIntrArvlBins};
	
	globalVar.INMMessageReadParamsRspFlag=0;
	globalVar.txRspBuf[kG992p3AddrField] =kG992p3PriorityLow ;
	globalVar.txRspBuf[kG992p3CtrlField] =kG992p3Rsp | globalVar.txRspMsgNumStored;
	globalVar.txRspBuf[kG992p3CmdCode] = kDslINMControlCommand;
	globalVar.txRspBuf[kG992p3CmdSubCode]= kDslReadINMParams | 0x80;
		
	pPLNIntrArvlBins = (short *)AdslMibGetObjectValue(gDslVars,OidStr,sizeof(OidStr),NULL,&dataLen);
	INMIATO=pPLNIntrArvlBins[1];
	INMIATS=log2Int(pPLNIntrArvlBins[2]-pPLNIntrArvlBins[1]);
	globalVar.txRspBuf[4]=INMIATS<<4;
	globalVar.txRspBuf[4]|=INMIATO>>8;
	globalVar.txRspBuf[5]=INMIATO&0x00FF;
	globalVar.txRspBuf[6]=globalVar.INMCC;
	globalVar.txRspBuf[7]=globalVar.INM_INPEQ_MODE;
	G992p3OvhMsgXmtSendRsp(gDslVars, rspLen);
}
Public void INMSetParamsResponse(void *gDslVars,int newConfig)
{
	int rspLen=5;
	globalVar.txRspBuf[kG992p3AddrField] =kG992p3PriorityLow ;
	globalVar.txRspBuf[kG992p3CtrlField] =kG992p3Rsp | globalVar.txRspMsgNumStored;
	globalVar.txRspBuf[kG992p3CmdCode] = kDslINMControlCommand;
	globalVar.txRspBuf[kG992p3CmdSubCode]= kG992p3OvhMsgINMControlACK;
	if(newConfig==1)
		globalVar.txRspBuf[4]=kG992p3OvhMsgINMINPEQModeACC;
	else
		globalVar.txRspBuf[4]=kG992p3OvhMsgINMINPEQModeNACC;
	G992p3OvhMsgXmtSendRsp(gDslVars, rspLen);
	globalVar.INMWaitForParamsFlag=0;
}
	
Public void INMReadCntrResponse(void *gDslVars)
{
	int rspLen=109,i;
	uchar *pMsg;
	ulong dataLen,val;
	adslMibInfo *pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &dataLen);
	uchar OidStr[] = { kOidAdslPrivate, kOidAdslPrivPLNDurationHist };
	ulong *pPLNDurationHist, *pPLNIntrArvlHist;
	globalVar.txRspBuf[kG992p3AddrField] =kG992p3PriorityLow ;
	globalVar.txRspBuf[kG992p3CtrlField] =kG992p3Rsp | globalVar.txRspMsgNumStored;
	globalVar.txRspBuf[kG992p3CmdCode] = kDslINMControlCommand;
	globalVar.txRspBuf[kG992p3CmdSubCode]= kDslReadINMCounters | 0x80;
	pPLNDurationHist = (ulong *)AdslMibGetObjectValue(gDslVars,OidStr,sizeof(OidStr),NULL,&dataLen);
	pMsg=&globalVar.txRspBuf[kG992p3CmdSubCode+1];
	for(i=0;i<17;i++)
	{
		val=(ulong)pPLNDurationHist[i];
		WriteCnt32(pMsg,val);
		pMsg+=4;
	}
	OidStr[1]=kOidAdslPrivPLNIntrArvlHist;
	pPLNIntrArvlHist = (ulong *)AdslMibGetObjectValue(gDslVars,OidStr,sizeof(OidStr),NULL,&dataLen);
	for(i=0;i<8;i++)
	{
		val=(ulong)pPLNIntrArvlHist[i];
		WriteCnt32(pMsg,val);
		pMsg+=4;
	}
	WriteCnt32(pMsg,pMib->adslPLNData.PLNBBCounter);
	pMsg+=4;
	pMsg[0]=globalVar.INMDF;
	G992p3OvhMsgXmtSendRsp(gDslVars, rspLen);
	globalVar.INMMessageReadCntrRspFlag=0;
}
	
	
Public void PLNReadResponses(void *gDslVars, char PLNTableName)
{
  int rspLen=4;
  uchar *pMsg;
  ulong zero=0;
  globalVar.txRspBuf[kG992p3AddrField] =kG992p3PriorityLow ;
  globalVar.txRspBuf[kG992p3CtrlField] =kG992p3Rsp | globalVar.txRspMsgNumStored;
  globalVar.txRspBuf[kG992p3CmdCode] = kG992p3OvhMsgCmdPMDRead;
  globalVar.txRspBuf[kG992p3CmdSubCode]= globalVar.kG992p3OvhMsgCmdPLN | 0x80;
  globalVar.PLNMessageReadRspFlag=0;
  switch(PLNTableName)
          {      
    case kDslPLNPeakNoiseTablePtr:
      {
              int j;
        globalVar.txRspBuf[4]=kDslPLNControlReadPerBinPeakNoise;
        rspLen++;
        __SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadPerBinPeakNoise Ptr Has returned",0);
        if(globalVar.pPLNPeakNoiseTable!=NULL){
          pMsg= &globalVar.txRspBuf[5];
          if(globalVar.PLNPerBinMeasurementCounterRcvFlag==1){
            WriteCnt32(pMsg,globalVar.PLNUpdtCount);
            globalVar.PLNPerBinPeakNoiseMsrCounter=globalVar.PLNUpdtCount;
          }
          else WriteCnt32(pMsg,zero);
          rspLen+=4;
          for (j=0;j<globalVar.PLNPeakNoiseTableLen;j++)
            globalVar.txRspBuf[j+9]=*(globalVar.pPLNPeakNoiseTable + j);
          rspLen+=globalVar.PLNPeakNoiseTableLen;
        }
        globalVar.PLNPerBinMeasurementCounterRcvFlag=0;
      }
      break;
    case kDslPerBinThldViolationTablePtr:{
              int j;
        globalVar.txRspBuf[4]=kDslPLNControlReadPerBinThresholdCount;
        rspLen++;
        __SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadPerBinThresholdCount",0);
       
        if(globalVar.pPLNThresholdCountTable!=NULL){
          pMsg = &globalVar.txRspBuf[5];
          WriteCnt16(pMsg,globalVar.PLNNoiseMarginPerBinDecLevel);
          rspLen+=2;
          pMsg= &globalVar.txRspBuf[7];
          if(globalVar.PLNPerBinMeasurementCounterRcvFlag==1){
            WriteCnt32(pMsg,globalVar.PLNUpdtCount);
            globalVar.PLNPerBinThldCounter=globalVar.PLNUpdtCount;
          }
          else WriteCnt32(pMsg,zero);
          rspLen+=4;
          for (j=0;j<globalVar.PLNThresholdCountTableLen;j++)
            globalVar.txRspBuf[j+11]=*(globalVar.pPLNThresholdCountTable + j);
          rspLen+=globalVar.PLNThresholdCountTableLen;
        }
        globalVar.PLNPerBinMeasurementCounterRcvFlag=0;
    }
    break;
    case kDslImpulseNoiseDurationTablePtr:
    case kDslImpulseNoiseDurationTableLongPtr:{
            int j;ulong *pPLNDurationHist;ulong dataLen;ushort val;
      uchar OidStr[] = { kOidAdslPrivate, kOidAdslPrivPLNDurationHist };
      pPLNDurationHist = (ulong *)AdslMibGetObjectValue(gDslVars,OidStr,sizeof(OidStr),NULL,&dataLen);
      globalVar.txRspBuf[4]=kDslPLNControlReadImpulseDurationStat;
      rspLen++;
      __SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadImpulseDurationStat",0);
      if(pPLNDurationHist!=NULL){
        pMsg = &globalVar.txRspBuf[5]; 
        WriteCnt16(pMsg,globalVar.PLNNoiseMarginBroadbandDecLevel);
        rspLen+=2;
        pMsg = &globalVar.txRspBuf[7];
        if(globalVar.PLNBroadbandMsrCounterRcvFlag==1){
          WriteCnt32(pMsg,globalVar.PLNUpdtCount);
          globalVar.PLNImpulseDurStatCounter=globalVar.PLNUpdtCount;
        }
        else WriteCnt32(pMsg,zero);
        rspLen+=4;
        pMsg = &globalVar.txRspBuf[11];
        for (j=0;j<kPlnNumberOfDurationBins;j++)
        {
        	val=pPLNDurationHist[j];
        	WriteCnt16(pMsg,val);
          pMsg+=2;
        }
        rspLen+=2*kPlnNumberOfDurationBins;
        globalVar.PLNBroadbandMsrCounterRcvFlag=0;
      }
    }
    break;
    case kDslImpulseNoiseTimeTablePtr:
    case kDslImpulseNoiseTimeTableLongPtr:{
              int j;ulong *pPLNIntrArvlHist;ushort val;ulong dataLen;
      uchar OidStr[] = { kOidAdslPrivate, kOidAdslPrivPLNIntrArvlHist};
      pPLNIntrArvlHist = (ulong *)AdslMibGetObjectValue(gDslVars,OidStr,sizeof(OidStr),NULL,&dataLen);
      globalVar.txRspBuf[4]=kDslPLNControlReadImpulseInterArrivalTimeStat;
      rspLen++;
      __SoftDslPrintf(gDslVars, "G992p3Ovh CMD PLN ReadImpulseInterArrivalTimeStat",0);
      if(pPLNIntrArvlHist!=NULL){
        pMsg = &globalVar.txRspBuf[5];
        WriteCnt16(pMsg,globalVar.PLNNoiseMarginBroadbandDecLevel);
        rspLen+=2;
        pMsg = &globalVar.txRspBuf[7];
        if(globalVar.PLNBroadbandMsrCounterRcvFlag==1){
          WriteCnt32(pMsg,globalVar.PLNUpdtCount);
          globalVar.PLNImpulseInterArrStatCounter=globalVar.PLNUpdtCount;
        }
        else WriteCnt32(pMsg,zero);
        rspLen+=4;
        pMsg = &globalVar.txRspBuf[11];
        for (j=0;j<kPlnNumberOfInterArrivalBins;j++)
        {
        	val=(ushort)pPLNIntrArvlHist[j];
					WriteCnt16(pMsg,val);
          pMsg+=2;
        }
        rspLen+=2*kPlnNumberOfInterArrivalBins;
        globalVar.PLNBroadbandMsrCounterRcvFlag=0;
      }
    }
    break;
    case  kDslItaBinTablePtr:{
              int j,k;
        globalVar.txRspBuf[4]=kDslPLNControlPLNStatus;
        rspLen++;
        if(globalVar.PLNActiveFlag==0)
          globalVar.txRspBuf[5]=0x00;
        else globalVar.txRspBuf[5]=0x01;
        rspLen++;
        pMsg = &globalVar.txRspBuf[6];
        if(globalVar.PLNBroadbandMsrCounterRcvFlag==1){
          WriteCnt32(pMsg,globalVar.PLNUpdtCount);
        }
        else WriteCnt32(pMsg,zero);
        rspLen+=4;
        globalVar.txRspBuf[10]=globalVar.PLNinpBinTableLen/2;
        rspLen++;
        for (j=0;j<globalVar.PLNinpBinTableLen;j++){
          globalVar.txRspBuf[j+11]=*(globalVar.pPLNinpBinTablePtr + j);
        }
        rspLen+=globalVar.PLNinpBinTableLen;
        globalVar.txRspBuf[11+globalVar.PLNinpBinTableLen]=globalVar.PLNitaBinTableLen/2;
        rspLen++;
        k=12+globalVar.PLNinpBinTableLen;
        for (j=0;j<globalVar.PLNitaBinTableLen;j++){
          globalVar.txRspBuf[j+k]=*(globalVar.pPLNitaBinTablePtr + j);
#if 0
          __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Status InpBin Table index %d value %X addr %X",0,j,*(globalVar.pPLNitaBinTablePtr+j),globalVar.pPLNitaBinTablePtr+j);
#endif
        }
        rspLen+=globalVar.PLNitaBinTableLen;
    }
    break;
    }
  G992p3OvhMsgXmtSendRsp(gDslVars, rspLen);
}

Public void G992p3OvhMsgStatusSnooper (void *gDslVars, dslStatusStruct *status)
{
  static uchar l3RequestCmd[] = { kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdPower, kPwrSimpleRequest, 3};
  static uchar l0RequestCmd[] = { kG992p3PriorityNormal, 0, kG992p3OvhMsgCmdPower, kPwrSimpleRequest, 0};
  uchar olrCmd[2+3+2*4+4+1];
  int   olrLen, olrLen1;
  adslMibInfo   *pMib;
  ulong            mibLen;

  switch (DSL_STATUS_CODE(status->code))
    {
    case kDslDspControlStatus:
      switch(status->param.dslConnectInfo.code)
        {
        case kDslPLNPeakNoiseTablePtr:
    globalVar.PLNPeakNoiseTableLen=status->param.dslConnectInfo.value;
    globalVar.pPLNPeakNoiseTable=status->param.dslConnectInfo.buffPtr;
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslPLNPeakNoiseTablePtr );
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Peak Noise Table Ptr update Addr %X ",0,status->param.dslConnectInfo.buffPtr);
    break;
        case kDslPerBinThldViolationTablePtr:
    globalVar.PLNThresholdCountTableLen=status->param.dslConnectInfo.value;
    globalVar.pPLNThresholdCountTable=status->param.dslConnectInfo.buffPtr;
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslPerBinThldViolationTablePtr); 
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Threshold Violation Count Table Ptr update Addr %X",0,status->param.dslConnectInfo.buffPtr);
    break;   
        case kDslImpulseNoiseDurationTableLongPtr:
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslImpulseNoiseDurationTableLongPtr);
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Impulse Duration Table Long Ptr update Addr %X",0,status->param.dslConnectInfo.buffPtr);
    break;
        case kDslImpulseNoiseDurationTablePtr:
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslImpulseNoiseDurationTablePtr);
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Impulse Duration Table Ptr update Addr %X",0,status->param.dslConnectInfo.buffPtr);
    break;
        case kDslImpulseNoiseTimeTablePtr:
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslImpulseNoiseTimeTablePtr);
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN InterArrival Table Ptr update Addr %X",0,status->param.dslConnectInfo.buffPtr);
    if(globalVar.INMMessageReadParamsRspFlag==1)
      INMReadParamsResponse(gDslVars);
    if(globalVar.INMMessageReadCntrRspFlag==1)
      INMReadCntrResponse(gDslVars);
    break;
        case kDslImpulseNoiseTimeTableLongPtr:
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslImpulseNoiseTimeTableLongPtr);
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN InterArrival Table Long Ptr update Addr %X",0,status->param.dslConnectInfo.buffPtr);
    if(!XdslMibIsVdsl2Mod(gDslVars) && !(ADSL_PHY_SUPPORT(kAdslPhyE14InmAdsl)))
    {
    	if(globalVar.INMMessageReadParamsRspFlag==1)
      	INMReadParamsResponse(gDslVars);
    	if(globalVar.INMMessageReadCntrRspFlag==1)
      	INMReadCntrResponse(gDslVars);
    }
    break;   
        case kDslINMControTotalSymbolCount:
    if(globalVar.INMMessageReadCntrRspFlag==1)
      INMReadCntrResponse(gDslVars);
    break;
        case kDslPLNMarginPerBin:
    globalVar.PLNNoiseMarginPerBinDecLevel=status->param.dslConnectInfo.value;
    break;
        case kDslPLNMarginBroadband:
    globalVar.PLNNoiseMarginBroadbandDecLevel=status->param.dslConnectInfo.value;
    break;
        case kDslPerBinMsrCounter:
    globalVar.PLNPerBinMeasurementCounterRcvFlag=1;
    globalVar.PLNUpdtCount=status->param.dslConnectInfo.value;
    break;
        case kDslBroadbandMsrCounter:
    globalVar.PLNBroadbandMsrCounterRcvFlag=1;
    globalVar.PLNUpdtCount=status->param.dslConnectInfo.value;
    break;
        case kDslInpBinTablePtr:
    globalVar.PLNinpBinTableLen=status->param.dslConnectInfo.value;
    globalVar.PLNnInpBin=globalVar.PLNinpBinTableLen/2;
    globalVar.pPLNinpBinTablePtr=status->param.dslConnectInfo.buffPtr;
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Status InpBin Table Ptr update Addr %X Length %d ",0,status->param.dslConnectInfo.buffPtr,globalVar.PLNinpBinTableLen);
    break;
        case kDslItaBinTablePtr:
    globalVar.PLNitaBinTableLen=status->param.dslConnectInfo.value;
    globalVar.PLNnItaBin=globalVar.PLNitaBinTableLen/2;
    globalVar.pPLNitaBinTablePtr=status->param.dslConnectInfo.buffPtr;
    if(globalVar.PLNMessageReadRspFlag==1)
      PLNReadResponses(gDslVars,kDslItaBinTablePtr);
    __SoftDslPrintf(gDslVars, "G992p3Ovh PLN Status Ita Table Ptr update Addr %X Length %d ",0,status->param.dslConnectInfo.buffPtr,globalVar.PLNitaBinTableLen);
    break;
        case kDslPlnState:
    globalVar.PLNActiveFlag=status->param.dslConnectInfo.value;
    break;
#if !defined(CONFIG_BCM96358) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96338)
    case kDslINMConfig:
        {
        	INMParamStruct *inmParams;
        	inmParams=(INMParamStruct*)status->param.dslConnectInfo.buffPtr;
        	globalVar.INMCC=inmParams->clusterContinuationParameter;
        	globalVar.INM_INPEQ_MODE=inmParams->equivalentInpMode;
        	globalVar.INMIATO=inmParams->interArrivalTimeOffset;
        	globalVar.INMIATS=inmParams->interArrivalTimeStep;
        	globalVar.INMDF=inmParams->defaultParametersFlag;
        	if (globalVar.INMWaitForParamsFlag==1)
        		INMSetParamsResponse(gDslVars,inmParams->newConfig);
        }
      break;
#endif
        }
    break;
#if 0
    case kDslEscapeToG994p1Status:
      G992p3OvhMsgReset(gDslVars);
      break;
#endif
    case kDslTrainingStatus:
      switch (status->param.dslTrainingInfo.code) {
        case kG992FireState:
          if( status->param.dslTrainingInfo.value & kFireUsEnabled )
#ifdef SUPPORT_DSL_BONDING
            mgntCntReadCmd[gLineId(gDslVars)][3]=kG992p3OvhMsgCmdCntReadId1;
#else
            mgntCntReadCmd[3]=kG992p3OvhMsgCmdCntReadId1;
#endif
          break;
        case kDslStartedG994p1:
        case kG994p1EventToneDetected:
        case kDslStartedT1p413HS:
        case kDslT1p413ReturntoStartup:
#ifdef SUPPORT_DSL_BONDING
          mgntCntReadCmd[gLineId(gDslVars)][3]=kG992p3OvhMsgCmdCntReadId;
#else
          mgntCntReadCmd[3]=kG992p3OvhMsgCmdCntReadId;
#endif
          break;
        case kDslG992p2TxShowtimeActive:
          if(globalVar.rateChangeFlag){
            globalVar.rateChangeFlag=0;
            break;
          }
          
          if (AdslMibIsLinkActive(gDslVars))
            G992p3OvhMsgReset(gDslVars);
#if 0
          globalVar.cmdPollingStamp = globalVar.timeMs;
          G992p3OvhMsgReset(gDslVars);
#ifdef CONFIG_BCM96368
          if(XdslMibIsVdsl2Mod(gDslVars) && (globalVar.pollCmd[kPMDVectorBlockReadCmd].cmdFlags==kPollCmdActive)){
            pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
            CreateVectorReadCmdSeq(gDslVars,&globalVar.HlogVecCmd,19,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
            CreateVectorReadCmdSeq(gDslVars,&globalVar.QLNVecCmd,38,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
            CreateVectorReadCmdSeq(gDslVars,&globalVar.SnrVecCmd,38,&(pMib->usNegBandPlan),pMib->gFactors.Gfactor_MEDLEYSETus);
            CreateVectorReadCmdSeq(gDslVars,&globalVar.HlogQlnBlRdCmd,32,&(pMib->usNegBandPlanDiscovery),pMib->gFactors.Gfactor_SUPPORTERCARRIERSus);
            CreateVectorReadCmdSeq(gDslVars,&globalVar.SnrBlRdCmd,32,&(pMib->usNegBandPlan),pMib->gFactors.Gfactor_MEDLEYSETus);
            G992p3OvhMsgInitHlogVecCmd(gDslVars);
          }
#endif
#endif
          break;
        case kDslG992p2RxShowtimeActive:
          globalVar.cmdPollingStamp = globalVar.timeMs;
          if(globalVar.rateChangeFlag){
            globalVar.rateChangeFlag=0;
            break;
          }
          
          if (AdslMibIsLinkActive(gDslVars))
            G992p3OvhMsgReset(gDslVars);
          break;
        case kG992SetPLNMessageBase:
          globalVar.kG992p3OvhMsgCmdPLN=status->param.dslTrainingInfo.value;
          break;
        case kDslPwrStateRequest:
          G992p3OvhMsgSetL0(gDslVars);
          break;
      }
      break;
    case kDslReceivedEocCommand:
        if( (kDslVdslSraReqSnd == status->param.dslClearEocMsg.msgId) ||
            (kDslVdslSosReqSnd == status->param.dslClearEocMsg.msgId) ){
            olrCmd[kG992p3AddrField] = kG992p3PriorityHigh;
            olrCmd[kG992p3CmdCode] = kG992p3OvhMsgCmdOLR;
            if(kDslVdslSraReqSnd == status->param.dslClearEocMsg.msgId)
               olrCmd[kG992p3CmdSubCode] = kG992p3OvhMsgCmdOLRReq6;
            else
               olrCmd[kG992p3CmdSubCode] = kG992p3OvhMsgCmdOLRReq7;
            olrLen = kG992p3CmdSubCode+1;
            if(G992p3OvhMsgXmtSendOLRCmd(gDslVars,
                                        olrCmd, (ulong)olrLen,
                                        (uchar *)status->param.dslClearEocMsg.dataPtr,
                                        (ulong)status->param.dslClearEocMsg.msgType & kDslClearEocMsgLengthMask,
                                        true)) {
                mibLen = (ulong)sizeof(adslMibInfo);
                pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
                pMib->adslStat.bitswapStat.rcvCntReq++;
                XdslMibNotifyBitSwapReq(gDslVars, DS_DIRECTION);
                __SoftDslPrintf(gDslVars, "Drv: VDSL %s Request sent, msgPtr=%p msgLen=%d", 0,
                                            (kDslVdslSraReqSnd == status->param.dslClearEocMsg.msgId)? "SRA": "SOS",
                                            status->param.dslClearEocMsg.dataPtr,
                                            status->param.dslClearEocMsg.msgType & kDslClearEocMsgLengthMask);
            }
        }
        break;
    case kDslOLRRequestStatus:
    {
      uchar   *pOlrCmd;
      
      olrCmd[kG992p3AddrField] = kG992p3PriorityHigh;
      olrCmd[kG992p3CmdCode]   = kG992p3OvhMsgCmdOLR;
      olrCmd[kG992p3CmdCode+1] = status->param.dslOLRRequest.msgType;
      olrLen = kG992p3CmdCode+2;
      if ((status->param.dslOLRRequest.msgType != kOLRRequestType1) &&
        (status->param.dslOLRRequest.msgType != kOLRRequestType4)) {
        WriteCnt16(olrCmd+kG992p3CmdCode+2, status->param.dslOLRRequest.L[0]);
        olrCmd[kG992p3CmdCode+4] = status->param.dslOLRRequest.B[0];
        olrLen += 3;
      }
      pOlrCmd = olrCmd + olrLen;
      if (status->param.dslOLRRequest.msgType < kOLRRequestType4) {  /* G.992.3 request */
        pOlrCmd[0] = status->param.dslOLRRequest.nCarrs;
        olrLen1 = 3*status->param.dslOLRRequest.nCarrs;
        olrLen++;
      }
      else {
        WriteCnt16(pOlrCmd, status->param.dslOLRRequest.nCarrs);
        olrLen1 = 4*status->param.dslOLRRequest.nCarrs;
        olrLen += 2;
      }
      
      if(XdslMibIsVdsl2Mod(gDslVars))
        olrLen1++;  /* Include the segment terminating byte(0xC0) */
      
      if(G992p3OvhMsgXmtSendOLRCmd(gDslVars,
                                    olrCmd, olrLen,
                                    status->param.dslOLRRequest.carrParamPtr, olrLen1,
                                    true)) {
          mibLen = (ulong)sizeof(adslMibInfo);
          pMib = (void *) AdslMibGetObjectValue (gDslVars, NULL, 0, NULL, &mibLen);
          pMib->adslStat.bitswapStat.rcvCntReq++;
          XdslMibNotifyBitSwapReq(gDslVars, DS_DIRECTION);
      }
      break;
    }
    case kDslOLRResponseStatus:
      if (!G992p3OvhMsgXmtRspBusy()) {
        globalVar.txRspBuf[kG992p3AddrField] = kG992p3PriorityHigh;
        globalVar.txRspMsgNum ^= 1;
        globalVar.txRspBuf[kG992p3CtrlField] = kG992p3Rsp | globalVar.txRspMsgNum;
        globalVar.txRspBuf[kG992p3CmdCode]   = kG992p3OvhMsgCmdOLR;
        globalVar.txRspBuf[kG992p3CmdCode+1] = status->param.value & 0xFFFF; /* reject code */
        globalVar.txRspBuf[kG992p3CmdCode+2] = status->param.value >> 16;  /* reason code */
        G992p3OvhMsgXmtSendRsp(gDslVars, kG992p3CmdCode+3);
      }
      break;
      
    case kDslPwrMgrStatus:
      if (G992p3OvhMsgXmtPwrRspBusy())
        break;
      
      DslFrameInit (gDslVars, &globalVar.txPwrRspFrame);
      globalVar.txPwrRspBuf0[kG992p3AddrField] = kG992p3PriorityNormal;
      globalVar.txRspMsgNum ^= 1;
      globalVar.txPwrRspBuf0[kG992p3CtrlField] = kG992p3Rsp | globalVar.txRspMsgNum;
      globalVar.txPwrRspBuf0[kG992p3CmdCode]   = kG992p3OvhMsgCmdPower;
      globalVar.txPwrRspBuf0[kG992p3CmdSubCode]= status->param.dslPwrMsg.msgType;
      if ((kPwrL2Grant == status->param.dslPwrMsg.msgType) ||
    (kPwrL2Grant2p == status->param.dslPwrMsg.msgType)) {
#ifdef G992P3_DBG_PRINT
        __SoftDslPrintf(gDslVars, "kDslPwrMgrStatus: msgLen=%d\n", 0, status->param.dslPwrMsg.param.msg.msgLen);
#endif
        DslFrameBufferSetLength (gDslVars, &globalVar.txPwrRspFrBuf0, 4);
        DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txPwrRspFrame, &globalVar.txPwrRspFrBuf0);
        
        DslFrameBufferSetAddress (gDslVars, &globalVar.txPwrRspFrBuf1, status->param.dslPwrMsg.param.msg.msgData);
        DslFrameBufferSetLength (gDslVars, &globalVar.txPwrRspFrBuf1, status->param.dslPwrMsg.param.msg.msgLen);
        DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txPwrRspFrame, &globalVar.txPwrRspFrBuf1);
        G992p3OvhMsgXmtSendLongFrame(gDslVars, &globalVar.txPwrRspFrame);
      }
      else {
        globalVar.txPwrRspBuf0[kG992p3CmdSubCode+1]= status->param.dslPwrMsg.param.value;
        olrLen = kG992p3CmdSubCode + 2;
        DslFrameBufferSetLength (gDslVars, &globalVar.txPwrRspFrBuf0, olrLen);
        DslFrameEnqueBufferAtBack (gDslVars, &globalVar.txPwrRspFrame, &globalVar.txPwrRspFrBuf0);
        G992p3OvhMsgXmtSendFrame(gDslVars, &globalVar.txPwrRspFrame);
      }
      break;

    default:
      break;
    }
  
  if ( globalVar.txL3Rq && !G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc) ) { 
    if(G992p3OvhMsgXmtSendCmd(gDslVars, l3RequestCmd,sizeof(l3RequestCmd), NULL,0, true, true)) {
      globalVar.txL3Rq = false;
      globalVar.txFlags |= kTxCmdL3WaitingAck;
    }
  }
  
  if (0 == AdslMibPowerState(gDslVars))
    globalVar.txFlags &= ~kTxCmdL0WaitingAck;
  else  if ( globalVar.txL0Rq && !G992p3FrameIsBitSet(gDslVars, globalVar.lastTxCmdFrame, kFrameClearEoc) ) {
    if(G992p3OvhMsgXmtSendCmd(gDslVars, l0RequestCmd,sizeof(l0RequestCmd), NULL,0, true, true)) {
      globalVar.txL0Rq = false;
      globalVar.txFlags |= kTxCmdL0WaitingAck;
    }
  }
}

#else /* G992P3 */

Public Boolean  G992p3OvhMsgInit(
      void          *gDslVars, 
      bitMap          setup, 
      dslFrameHandlerType   rxReturnFramePtr,
      dslFrameHandlerType   txSendFramePtr,
      dslFrameHandlerType   txSendCompletePtr,
      dslCommandHandlerType commandHandler,
      dslStatusHandlerType  statusHandler)
{
  return true;
}

Public int G992p3OvhMsgIndicateRcvFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
  return 0;
}

Public void G992p3OvhMsgReturnFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
}

Public int G992p3OvhMsgSendCompleteFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
  return 0;
}

Public void G992p3OvhMsgClose(void *gDslVars)
{
}
  
Public void G992p3OvhMsgTimer(void *gDslVars, long timeQ24ms)
{
}
  
Public Boolean G992p3OvhMsgCommandHandler(void *gDslVars, dslCommandStruct *cmd)
{
  return false;
}

Public void G992p3OvhMsgStatusSnooper (void *gDslVars, dslStatusStruct *status)
{
}

Public void G992p3OvhMsgSetL3(void *gDslVars)
{
}

Public void G992p3OvhMsgSetL0(void *gDslVars)
{
}

#endif
