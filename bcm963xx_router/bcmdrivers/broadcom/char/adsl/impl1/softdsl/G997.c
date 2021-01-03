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
 * G997.c -- dslpacket main module
 *
 * Description:
 *	This file contains root functions of G997
 *
 *
 * Copyright (c) 1993-1998 AltoCom, Inc. All rights reserved.
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: G997.c,v 1.3 2011/05/11 08:28:58 ydu Exp $
 *
 * $Log: G997.c,v $
 * Revision 1.3  2011/05/11 08:28:58  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.10  2004/07/21 01:39:41  ilyas
 * Reset entire G.997 state on retrain. Timeout in G.997 if no ACK
 *
 * Revision 1.9  2004/04/27 00:57:57  ilyas
 * Remove debug prints
 *
 * Revision 1.8  2004/04/27 00:27:16  ilyas
 * Implemented double buffering to ensure G.997 HDLC frame is continuous
 *
 * Revision 1.7  2004/04/12 23:34:52  ilyas
 * Merged the latest ADSL driver chnages for ADSL2+
 *
 * Revision 1.6  2003/08/04 15:38:38  yongbing
 * Undo the last check-in which will disable G.997 function
 *
 * Revision 1.5  2003/07/26 00:33:40  yjchen
 * temporarily disable G997StatusSnooper to avoid crash
 *
 * Revision 1.4  2003/07/18 18:56:59  ilyas
 * Added support for shared TX buffer (for ADSL driver)
 *
 * Revision 1.3  2002/07/20 00:51:41  ilyas
 * Merged witchanges made for VxWorks/Linux driver.
 *
 * Revision 1.2  2002/01/11 06:48:27  ilyas
 * Added command handler pointer
 *
 * Revision 1.1  2001/12/13 02:28:27  ilyas
 * Added common framer (DslPacket and G997) and G997 module
 *
 *
 *
 *****************************************************************************/

#include "SoftDsl.gh"

#include "DslFramer.h"
#include "G997.h"
#include "BlockUtil.h"
#include "AdslMibDef.h"

#define	globalVar	gG997Vars

Private void G997EocSend (void *gDslVars);
Private void G997ClearEocSendComplete (void *gDslVars, int msgType, uchar *dataPtr);

/*	G997 HDLC interface functions */

Private Boolean	G997HdlcRxGetPtr(void *gDslVars, dslFramerBufDesc *pBufDesc)
{
	return DslFramerRxGetPtr(gDslVars, &globalVar.dslFramer, pBufDesc);
}

Private void G997HdlcRxDone(void *gDslVars, dslFramerBufDesc *pBufDesc)
{
	DslFramerRxDone(gDslVars, &globalVar.dslFramer, pBufDesc);
}

Private Boolean	G997HdlcTxGetPtr(void *gDslVars, dslFramerBufDesc *pBufDesc)
{
	return DslFramerTxGetPtr(gDslVars, &globalVar.dslFramer, pBufDesc);
}

Private void G997HdlcTxDone(void *gDslVars, dslFramerBufDesc *pBufDesc)
{
	DslFramerTxDone (gDslVars, &globalVar.dslFramer, pBufDesc);
}

Public void G997Reset(void *gDslVars)
{
	HdlcByteReset(gDslVars, &globalVar.hdlcByte);
	DslFramerTxFlush(gDslVars, &globalVar.dslFramer);
	DslFramerRxFlush(gDslVars, &globalVar.dslFramer);
	globalVar.txIdle	= true;
	globalVar.timeCmdOut = 0;
	globalVar.txMsgNum	= 0;
	globalVar.rxMsgNum	= 0;
}

/*
**
**		G997 interface functions
**
*/

Public Boolean  G997Init(
			void					*gDslVars, 
			bitMap					setup, 
			ulong					rxBufNum,
			ulong					rxBufSize,
			ulong					rxPacketNum,
			upperLayerFunctions		*pUpperLayerFunctions,
			dslCommandHandlerType	g997PhyCommandHandler)
{
	Boolean					flag;

	globalVar.setup				= setup;
	G997SetTxBuffer(gDslVars, kG997MsgBufSize, globalVar.txMsgBuf);

	flag = DslFramerInit(
			gDslVars,
			&globalVar.dslFramer,
			0,
			kDslG997Status,
			FLD_OFFSET (dslStatusStruct, param.g997Status),
			pUpperLayerFunctions->rxIndicateHandlerPtr,
			pUpperLayerFunctions->txCompleteHandlerPtr,
			pUpperLayerFunctions->statusHandlerPtr,
			rxBufNum,
			rxBufSize,
			rxPacketNum);
	if (!flag)
		return false;

	flag = HdlcByteInit(
				gDslVars,
				&globalVar.hdlcByte,
				kHdlcTxIdleStop | kHdlcCrc16,
				G997HdlcRxGetPtr,
				G997HdlcRxDone,
				G997HdlcTxGetPtr,
				G997HdlcTxDone);
	if (!flag)
		DslFramerClose(gDslVars, &globalVar.dslFramer);

	globalVar.commandHandler = g997PhyCommandHandler;
	globalVar.rxMsgNum	= 0;
	globalVar.txMsgNum	= 0;
	globalVar.txIdle	= true;
	globalVar.timeMs	= 0;
	globalVar.timeCmdOut = 0;
	return flag;
}

Public Boolean G997SetTxBuffer(void *gDslVars, ulong len, void *bufPtr)
{
	if ((0 == len) || (NULL == bufPtr))
		return false;
	
	__SoftDslPrintf(gDslVars, "%s: line%d - bufPtr=0x%08x len=%d\n", 0,  __FUNCTION__, gLineId(gDslVars), (unsigned int)bufPtr, len);
	
	globalVar.txMsgBufLen = len;
	globalVar.txMsgBufPtr = bufPtr;

	if ((ADSL_PHY_SUPPORT(kAdslPhyAdsl2)) || (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2))) {
		globalVar.txMsgLen		= len >> 1;
		globalVar.txMsgBufs		= 2;
	}
	else {
		globalVar.txMsgLen		= len;
		globalVar.txMsgBufs		= 1;
	}
	globalVar.txMsgBufNum	= 0;

	return true;
}

Public void G997Close(void *gDslVars)
{
	DslFramerClose (gDslVars, &globalVar.dslFramer);
}
	
Public void G997Timer(void *gDslVars, long timeMs)
{
	globalVar.timeMs += timeMs;

	if (!AdslMibIsLinkActive(gDslVars)) {
		globalVar.txIdle = true;
	}

	if (globalVar.timeCmdOut && ((globalVar.timeMs - globalVar.timeCmdOut) > 5000)) {
		unsigned int	rdPtr, wrPtr, sePtr;
		volatile AdslXfaceData	*pAdslX = (AdslXfaceData *) ADSL_LMEM_XFACE_DATA;
		__SoftDslPrintf(gDslVars, "G997Timer: CMD pending too long. txIdle=%d", 0, globalVar.txIdle);

		rdPtr = (unsigned int)pAdslX->sbCmd.pRead;
		wrPtr = (unsigned int)pAdslX->sbCmd.pWrite;
		sePtr =(unsigned int)pAdslX->sbCmd.pStretchEnd;
		__SoftDslPrintf(gDslVars, "***pRead=%x pWrite=%x pEnd=%x pStretchEnd=%x***\n", 0,
			rdPtr, wrPtr, (unsigned int)pAdslX->sbCmd.pEnd, sePtr);
		G997ClearEocSendComplete (gDslVars, (globalVar.txMsgNum << 16), (void *) 1);
	}
}
	
Public Boolean G997CommandHandler(void *gDslVars, dslCommandStruct *cmd)
{
	return false;
}
	
Public int G997SendFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
	DslFramerSendFrame (gDslVars, &globalVar.dslFramer, pFrame);
	if (globalVar.txIdle)
		G997EocSend (gDslVars);
	return 1;
}

Public int G997ReturnFrame(void *gDslVars, void *pVc, ulong mid, dslFrame *pFrame)
{
	DslFramerReturnFrame (gDslVars, &globalVar.dslFramer, pFrame);
	return 1;
}

Public void * G997GetFramePoolHandler(void *gDslVars)
{
	return DslFramerGetFramePoolHandler (&globalVar.dslFramer);
}

/*
**
**	Clear EOC mesage parsing functions
**
*/

Private void G997IncBufNum(void *gDslVars)
{
	ulong	bufNum = globalVar.txMsgBufNum + 1;

	globalVar.txMsgBufNum = (bufNum == globalVar.txMsgBufs) ? 0 : bufNum;
}

Private void G997EocSend (void *gDslVars)
{
	dslCommandStruct	cmd;
	int					len;
	uchar				*bufPtr;

	bufPtr = globalVar.txMsgBufPtr + globalVar.txMsgBufNum * globalVar.txMsgLen;
	globalVar.txIdle = false;
	len = HdlcByteTx(gDslVars, &globalVar.hdlcByte, globalVar.txMsgLen, bufPtr);
	if (len > 0) {
		G997IncBufNum(gDslVars);
		cmd.command						 = kDslSendEocCommand;
		cmd.param.dslClearEocMsg.msgId   = kDslClearEocSendFrame;
		cmd.param.dslClearEocMsg.msgType = (globalVar.txMsgNum << 16) | len;
		cmd.param.dslClearEocMsg.dataPtr = bufPtr;
		if ((*globalVar.commandHandler)(gDslVars, &cmd)) {
			globalVar.timeCmdOut = globalVar.timeMs;
			if (0 == globalVar.timeCmdOut)
				globalVar.timeCmdOut = (ulong) -1;
		}
		else {
			__SoftDslPrintf(gDslVars, "G997EocSend: Failed to write CMD, type=0x%X", 0, cmd.param.dslClearEocMsg.msgType);
			globalVar.txIdle = true;
		}
	}
	else
		globalVar.txIdle = true;
}

Private void G997ClearEocSendComplete (void *gDslVars, int msgType, uchar *dataPtr)
{
	uchar n, numAck, sendNext=1;

	n = (uchar)((msgType & kDslClearEocMsgNumMask) >> 16);
	if (dataPtr != NULL) {	/* frame accepted */
		globalVar.txMsgNum = n + 1;
	}
	else {					/* frame rejected */
		numAck = (uchar)(msgType & 0x000000FF);
		__SoftDslPrintf(gDslVars, "G997ClearEocSendComplete: FRAME REJECTED, txMsgNum=%d numRq=%d numAck=%d\n",
			0, globalVar.txMsgNum, n, numAck);
		if( n == numAck )
			sendNext=0;
		globalVar.txMsgNum = n;
	}

	globalVar.timeCmdOut = 0;
	n = HdlcByteTxIdle(gDslVars, &globalVar.hdlcByte) && DslFramerTxIdle(gDslVars, &globalVar.dslFramer);
	if (n)
		globalVar.txIdle = true;
	else if(sendNext)
		G997EocSend (gDslVars);
	else
		globalVar.timeCmdOut = globalVar.timeMs;
}

Private void G997ClearEocIndicateRcv (void *gDslVars, int msgType, uchar *dataPtr)
{
	int		n, len;

	n = (msgType & kDslClearEocMsgNumMask) >> 16;
	if (n != globalVar.rxMsgNum) {
		DslFramerRxFlushFrame (gDslVars, &globalVar.dslFramer, kDslFramerRxFrameErrPhy);
		HdlcByteRxFlush(gDslVars, &globalVar.hdlcByte);
	}
	globalVar.rxMsgNum = n + 1;
	len = msgType & kDslClearEocMsgLengthMask;

	HdlcByteRx(gDslVars, &globalVar.hdlcByte, len, dataPtr);
}

Public void G997StatusSnooper (void *gDslVars, dslStatusStruct *status)
{
	if (0 == (globalVar.dslFramer.setup & kDslFramerInitialized))
		return;
	
	switch (DSL_STATUS_CODE(status->code)) {
		case kDslEscapeToG994p1Status:
			G997Reset(gDslVars);
			break;

		case	kDslReceivedEocCommand:
			switch (status->param.dslClearEocMsg.msgId) {
				case kDslClearEocSendComplete:
					/* 
					** New PHY will indicate early completion with kDslClearEocSendComplete2
					** and will set kDslClearEocMsgExtraSendComplete for ADSL driver to ignore 
					** kDslClearEocSendComplete message.
					** Old PHY will only use kDslClearEocSendComplete w/o kDslClearEocMsgExtraSendComplete
					*/
					if (status->param.dslClearEocMsg.msgType & kDslClearEocMsgExtraSendComplete)
						break;
					/* proceed if kDslClearEocMsgExtraSendComplete not set */
				case kDslClearEocSendComplete2:
					G997ClearEocSendComplete (gDslVars, status->param.dslClearEocMsg.msgType, status->param.dslClearEocMsg.dataPtr);
					break;
				case kDslClearEocRcvedFrame:
					G997ClearEocIndicateRcv (
						gDslVars,
						status->param.dslClearEocMsg.msgType,
						status->param.dslClearEocMsg.dataPtr);
					break;
			}
			break;

		case kIllegalCommand:
			if (kDslSendEocCommand == status->param.value)
				G997ClearEocSendComplete (gDslVars, globalVar.txMsgNum << 16, NULL);
			break;
		default:
			break;
	}
}
