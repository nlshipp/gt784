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
 * AdslMibObj.c -- Adsl MIB object access functions
 *
 * Description:
 *	This file contains functions for access to ADSL MIB (RFC 2662) data
 *
 *
 * Copyright (c) 1993-1998 AltoCom, Inc. All rights reserved.
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslMibObj.c,v 1.3 2011/05/11 08:28:58 ydu Exp $
 *
 * $Log: AdslMibObj.c,v $
 * Revision 1.3  2011/05/11 08:28:58  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.15  2007/09/04 07:21:15  tonytran
 * PR31097: 1_28_rc8
 *
 * Revision 1.14  2007/01/11 09:14:04  tonytran
 * Fixed the set phy cfg and bitswap counter problems; Removed FAST_DEC_DISABLE from phy cfg
 *
 * Revision 1.13  2006/09/13 22:07:06  dadityan
 * Added Mib Oid for Adsl Phy Cfg
 *
 * Revision 1.8  2004/06/04 18:56:01  ilyas
 * Added counter for ADSL2 framing and performance
 *
 * Revision 1.7  2003/10/17 21:02:12  ilyas
 * Added more data for ADSL2
 *
 * Revision 1.6  2003/10/14 00:55:27  ilyas
 * Added UAS, LOSS, SES error seconds counters.
 * Support for 512 tones (AnnexI)
 *
 * Revision 1.5  2002/10/31 20:27:13  ilyas
 * Merged with the latest changes for VxWorks/Linux driver
 *
 * Revision 1.4  2002/07/20 00:51:41  ilyas
 * Merged witchanges made for VxWorks/Linux driver.
 *
 * Revision 1.3  2002/01/03 06:03:36  ilyas
 * Handle byte moves tha are not multiple of 2
 *
 * Revision 1.2  2002/01/02 19:13:19  liang
 * Change memcpy to BlockByteMove.
 *
 * Revision 1.1  2001/12/21 22:39:30  ilyas
 * Added support for ADSL MIB data objects (RFC2662)
 *
 *
 *****************************************************************************/

#include "SoftDsl.gh"

#include "AdslMib.h"
#include "AdslMibOid.h"
#include "BlockUtil.h"

#define	globalVar	gAdslMibVars
#ifndef _M_IX86
extern adslCfgProfile	adslCoreCfgProfile;
#endif
#ifdef G993
#define RestrictValue(n,l,r)        ((n) < (l) ? (l) : (n) > (r) ? (r) : (n))
#endif
/*
**
**		ATM TC (Transmission Convergence aka PHY) MIB data oblects
**
*/

extern void *AdslCoreSharedMemAlloc(long size);
extern void *memcpy( void *, const void *, unsigned int );
extern Boolean gSharedMemAllocFromUserContext;

Private void * MibGetAtmTcObjPtr(void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	atmPhyDataEntrty	*pAtmData;
	void				*pObj = NULL;

	if ((objIdLen < 2) || (objId[0] != kOidAtmMibObjects) || (objId[1] != kOidAtmTcTable))
		return NULL;

	pAtmData = (kAdslIntlChannel == AdslMibGetActiveChannel(gDslVars) ?
				&globalVar.adslMib.adslChanIntlAtmPhyData : &globalVar.adslMib.adslChanFastAtmPhyData);
	if (objIdLen == 2) {
		*objLen = sizeof(atmPhyDataEntrty);
		return pAtmData;
	}
	if (objId[2] != kOidAtmTcEntry)
		return NULL;
		
	if (objIdLen == 3) {
		*objLen = sizeof(atmPhyDataEntrty);
		return pAtmData;
	}

	if (objId[3] > kOidAtmAlarmState)
		return NULL;

	if (kOidAtmOcdEvents == objId[3]) {
		*objLen = sizeof(pAtmData->atmInterfaceOCDEvents);
		pObj = &pAtmData->atmInterfaceOCDEvents;
	}
	else if (kOidAtmAlarmState == objId[3]) {
		*objLen = sizeof(pAtmData->atmInterfaceTCAlarmState);
		pObj = &pAtmData->atmInterfaceTCAlarmState;
	}

	return pObj;
}


/*
**
**		ADSL Line MIB data oblects
**
*/

Private void * MibGetAdslLineTableObjPtr (void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;	

	if (0 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslLine);
		return &globalVar.adslMib.adslLine;
	}
	if (objId[1] != kOidAdslLineEntry)
		return NULL;
	if (1 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslLine);
		return &globalVar.adslMib.adslLine;
	}

	switch (objId[2]) {
		case kOidAdslLineCoding:
			*objLen = sizeof(globalVar.adslMib.adslLine.adslLineCoding);
			pObj = &globalVar.adslMib.adslLine.adslLineCoding;
			break;
		case kOidAdslLineType:
			*objLen = sizeof(globalVar.adslMib.adslLine.adslLineType);
			pObj = &globalVar.adslMib.adslLine.adslLineType;
			break;
		case kOidAdslLineSpecific:
		case kOidAdslLineConfProfile:
		case kOidAdslLineAlarmConfProfile:
		default:
			pObj = NULL;
			break;
	}

	return pObj;
}

Private void * MibGetAdslPhysTableObjPtr (void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;	

	if (0 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslPhys);
		return &globalVar.adslMib.adslPhys;
	}
	if (objId[0] != kOidAdslPhysEntry)
		return NULL;
	if (1 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslPhys);
		return &globalVar.adslMib.adslPhys;
	}

	switch (objId[1]) {
		case kOidAdslPhysInvSerialNumber:
		case kOidAdslPhysInvVendorID:
		case kOidAdslPhysInvVersionNumber:
			pObj = NULL;
			break;
			
		case kOidAdslPhysCurrSnrMgn:
			*objLen = sizeof(globalVar.adslMib.adslPhys.adslCurrSnrMgn);
			pObj = &globalVar.adslMib.adslPhys.adslCurrSnrMgn;
			break;
		case kOidAdslPhysCurrAtn:
			*objLen = sizeof(globalVar.adslMib.adslPhys.adslCurrAtn);
			pObj = &globalVar.adslMib.adslPhys.adslCurrAtn;
			break;
		case kOidAdslPhysCurrStatus:
			*objLen = sizeof(globalVar.adslMib.adslPhys.adslCurrStatus);
			pObj = &globalVar.adslMib.adslPhys.adslCurrStatus;
			break;
		case kOidAdslPhysCurrOutputPwr:
			*objLen = sizeof(globalVar.adslMib.adslPhys.adslCurrOutputPwr);
			pObj = &globalVar.adslMib.adslPhys.adslCurrOutputPwr;
			break;
		case kOidAdslPhysCurrAttainableRate:
			*objLen = sizeof(globalVar.adslMib.adslPhys.adslCurrAttainableRate);
			pObj = &globalVar.adslMib.adslPhys.adslCurrAttainableRate;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslPerfTableCounterPtr (adslPerfCounters *pPerf, uchar cntId, ulong *objLen)
{
	void	*pObj = NULL;

	switch (cntId) {
		case  kOidAdslPerfLofs:
			*objLen = sizeof(pPerf->adslLofs);
			pObj = &pPerf->adslLofs;
			break;
		case  kOidAdslPerfLoss:
			*objLen = sizeof(pPerf->adslLoss);
			pObj = &pPerf->adslLoss;
			break;
		case  kOidAdslPerfLprs:
			*objLen = sizeof(pPerf->adslLprs);
			pObj = &pPerf->adslLprs;
			break;
		case  kOidAdslPerfESs:
			*objLen = sizeof(pPerf->adslESs);
			pObj = &pPerf->adslESs;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslPerfTableObjPtr (void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;

	if (0 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslPerfData);
		return &globalVar.adslMib.adslPerfData;
	}
	if (objId[0] != kOidAdslPerfDataEntry)
		return NULL;
	if (1 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslPerfData);
		return &globalVar.adslMib.adslPerfData;
	}

	switch (objId[1]) {
		case  kOidAdslPerfLofs:
		case  kOidAdslPerfLoss:
		case  kOidAdslPerfLprs:
		case  kOidAdslPerfESs:
			pObj = MibGetAdslPerfTableCounterPtr (&globalVar.adslMib.adslPerfData.perfTotal, objId[1], objLen);
			break;
		case  kOidAdslPerfValidIntervals:
			*objLen = sizeof(globalVar.adslMib.adslPerfData.adslPerfValidIntervals);
			pObj = &globalVar.adslMib.adslPerfData.adslPerfValidIntervals;
			break;	
		case  kOidAdslPerfInvalidIntervals:
			*objLen = sizeof(globalVar.adslMib.adslPerfData.adslPerfInvalidIntervals);
			pObj = &globalVar.adslMib.adslPerfData.adslPerfInvalidIntervals;
			break;	
		case  kOidAdslPerfCurr15MinTimeElapsed: 	
			*objLen = sizeof(globalVar.adslMib.adslPerfData.adslPerfCurr15MinTimeElapsed);
			pObj = &globalVar.adslMib.adslPerfData.adslPerfCurr15MinTimeElapsed;
			break;
		case  kOidAdslPerfCurr15MinLofs:
		case  kOidAdslPerfCurr15MinLoss:
		case  kOidAdslPerfCurr15MinLprs:
		case  kOidAdslPerfCurr15MinESs:
			pObj = MibGetAdslPerfTableCounterPtr (&globalVar.adslMib.adslPerfData.perfCurr15Min, objId[1]-kOidAdslPerfCurr15MinLofs+1, objLen);
			break;	
		case  kOidAdslPerfCurr1DayTimeElapsed:
			*objLen = sizeof(globalVar.adslMib.adslPerfData.adslPerfCurr1DayTimeElapsed);
			pObj = &globalVar.adslMib.adslPerfData.adslPerfCurr1DayTimeElapsed;
			break;
		case  kOidAdslPerfCurr1DayLofs:
		case  kOidAdslPerfCurr1DayLoss:
		case  kOidAdslPerfCurr1DayLprs:
		case  kOidAdslPerfCurr1DayESs:
			pObj = MibGetAdslPerfTableCounterPtr (&globalVar.adslMib.adslPerfData.perfCurr1Day, objId[1]-kOidAdslPerfCurr1DayLofs+1, objLen);
			break;	
		case  kOidAdslPerfPrev1DayMoniSecs:
			*objLen = sizeof(globalVar.adslMib.adslPerfData.adslAturPerfPrev1DayMoniSecs);
			pObj = &globalVar.adslMib.adslPerfData.adslAturPerfPrev1DayMoniSecs;
			break;
		case  kOidAdslPerfPrev1DayLofs:
		case  kOidAdslPerfPrev1DayLoss:
		case  kOidAdslPerfPrev1DayLprs:
		case  kOidAdslPerfPrev1DayESs:
			pObj = MibGetAdslPerfTableCounterPtr (&globalVar.adslMib.adslPerfData.perfPrev1Day, objId[1]-kOidAdslPerfPrev1DayLofs+1, objLen);
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslPerfIntervalObjPtr (void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;	
	uchar	ind;

	if (0 == objIdLen) {
		*objLen = sizeof(globalVar.adslMib.adslPerfIntervals);
		return &globalVar.adslMib.adslPerfIntervals;
	}
	if ((objId[0] != kOidAdslPerfIntervalEntry) || (objIdLen < 4))
		return NULL;
	ind = objId[3];
	if ((ind == 0) || (ind > globalVar.adslMib.adslPerfData.adslPerfValidIntervals))
		return NULL;

	switch (objId[1]) {
		case kOidAdslIntervalNumber:
			*objLen = sizeof(int);
			globalVar.scratchData = ind;
			pObj = &globalVar.scratchData;
			break;
		case kOidAdslIntervalLofs:
		case kOidAdslIntervalLoss:
		case kOidAdslIntervalLprs:
		case kOidAdslIntervalESs:
			pObj = MibGetAdslPerfTableCounterPtr(&globalVar.adslMib.adslPerfIntervals[ind-1], objId[1]-kOidAdslIntervalLofs+1, objLen);
			break;

		case kOidAdslIntervalValidData:
			*objLen = sizeof(int);
			globalVar.scratchData = true;
			pObj = &globalVar.scratchData;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslObjPtr (void *gDslVars, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;	

	if ((objIdLen < 3) || (objId[0] != kOidAdslLine) || (objId[1] != kOidAdslMibObjects))
		return NULL;

	switch (objId[2]) {
		case kOidAdslLineTable:
			pObj = MibGetAdslLineTableObjPtr (gDslVars, objId+3, objIdLen-3, objLen);
			break;
		case kOidAdslAturPhysTable:
			pObj = MibGetAdslPhysTableObjPtr (gDslVars, objId+3, objIdLen-3, objLen);
			break;
		case kOidAdslAtucPhysTable:
			pObj	= &globalVar.adslMib.adslAtucPhys;
#if defined(CONFIG_BCM96368) || defined(CHIP_6368) || defined(CONFIG_BCM963268) || defined(CHIP_63268)
			*objLen = sizeof(globalVar.adslMib.xdslAtucPhys);
#else
			*objLen = sizeof(globalVar.adslMib.adslAtucPhys);
#endif
			break;
		case kOidAdslAturPerfDataTable:
			pObj = MibGetAdslPerfTableObjPtr (gDslVars, objId+3, objIdLen-3, objLen);
			break;
		case kOidAdslAturPerfIntervalTable:
			pObj = MibGetAdslPerfIntervalObjPtr (gDslVars, objId+3, objIdLen-3, objLen);
			break;
		default:
			pObj = NULL;
			break;
	}

	return pObj;
}

/*
**
**		ADSL Channel MIB data oblects
**
*/

Private void * MibGetAdslChanTableObjPtr(void *gDslVars, uchar chId, uchar *objId, int objIdLen, ulong *objLen)
{
	void			*pObj = NULL;
	adslChanEntry	*pChan;

	pChan = (kAdslIntlChannel == chId ? &globalVar.adslMib.adslChanIntl: &globalVar.adslMib.adslChanFast);

	if (0 == objIdLen) {
		*objLen = sizeof(adslChanEntry);
		return pChan;
	}
	if (objId[0] != kOidAdslChanEntry)
		return NULL;
	if (1 == objIdLen) {
		*objLen = sizeof(adslChanEntry);
		return pChan;
	}

	switch (objId[1]) {
		case kOidAdslChanInterleaveDelay:
			*objLen = sizeof(pChan->adslChanIntlDelay);
			pObj = &pChan->adslChanIntlDelay;
			break;
		case kOidAdslChanCurrTxRate:
			*objLen = sizeof(pChan->adslChanCurrTxRate);
			pObj = &pChan->adslChanCurrTxRate;
			break;
		case kOidAdslChanPrevTxRate:
			*objLen = sizeof(pChan->adslChanPrevTxRate);
			pObj = &pChan->adslChanPrevTxRate;
			break;
		case kOidAdslChanCrcBlockLength:
			*objLen = sizeof(pChan->adslChanCrcBlockLength);
			pObj = &pChan->adslChanCrcBlockLength;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslChanPerfTableCounterPtr (adslChanCounters *pPerf, uchar cntId, ulong *objLen)
{
	void	*pObj = NULL;

	switch (cntId) {
		case kOidAdslChanReceivedBlks:
			*objLen = sizeof(pPerf->adslChanReceivedBlks);
			pObj = &pPerf->adslChanReceivedBlks;
			break;
		case kOidAdslChanTransmittedBlks:
			*objLen = sizeof(pPerf->adslChanTransmittedBlks);
			pObj = &pPerf->adslChanTransmittedBlks;
			break;
		case kOidAdslChanCorrectedBlks:
			*objLen = sizeof(pPerf->adslChanCorrectedBlks);
			pObj = &pPerf->adslChanCorrectedBlks;
			break;
		case kOidAdslChanUncorrectBlks:
			*objLen = sizeof(pPerf->adslChanUncorrectBlks);
			pObj = &pPerf->adslChanUncorrectBlks;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslChanPerfTableObjPtr(void *gDslVars, uchar chId, uchar *objId, int objIdLen, ulong *objLen)
{
	void					*pObj = NULL;
	adslChanPerfDataEntry	*pChanPerf;

	pChanPerf = (kAdslIntlChannel == chId ? &globalVar.adslMib.adslChanIntlPerfData : &globalVar.adslMib.adslChanFastPerfData);

	if (0 == objIdLen) {
		*objLen = sizeof(adslChanPerfDataEntry);
		return pChanPerf;
	}
	if (objId[0] != kOidAdslChanPerfEntry)
		return NULL;
	if (1 == objIdLen) {
		*objLen = sizeof(adslChanPerfDataEntry);
		return pChanPerf;
	}

	switch (objId[1]) {
		case kOidAdslChanReceivedBlks:
		case kOidAdslChanTransmittedBlks:
		case kOidAdslChanCorrectedBlks:
		case kOidAdslChanUncorrectBlks:
			pObj = MibGetAdslChanPerfTableCounterPtr (&pChanPerf->perfTotal, objId[1], objLen);
			break;

		case kOidAdslChanPerfValidIntervals:
			*objLen = sizeof(pChanPerf->adslChanPerfValidIntervals);
			pObj = &pChanPerf->adslChanPerfValidIntervals;
			break;	
		case kOidAdslChanPerfInvalidIntervals:
			*objLen = sizeof(pChanPerf->adslChanPerfInvalidIntervals);
			pObj = &pChanPerf->adslChanPerfInvalidIntervals;
			break;	
		case kOidAdslChanPerfCurr15MinTimeElapsed:
			*objLen = sizeof(pChanPerf->adslPerfCurr15MinTimeElapsed);
			pObj = &pChanPerf->adslPerfCurr15MinTimeElapsed;
			break;	

		case kOidAdslChanPerfCurr15MinReceivedBlks:
		case kOidAdslChanPerfCurr15MinTransmittedBlks:
		case kOidAdslChanPerfCurr15MinCorrectedBlks:
		case kOidAdslChanPerfCurr15MinUncorrectBlks:
			pObj = MibGetAdslChanPerfTableCounterPtr (&pChanPerf->perfCurr15Min, objId[1]-kOidAdslChanPerfCurr15MinReceivedBlks+1, objLen);
			break;

		case kOidAdslChanPerfCurr1DayTimeElapsed:
			*objLen = sizeof(pChanPerf->adslPerfCurr1DayTimeElapsed);
			pObj = &pChanPerf->adslPerfCurr1DayTimeElapsed;
			break;	

		case kOidAdslChanPerfCurr1DayReceivedBlks:
		case kOidAdslChanPerfCurr1DayTransmittedBlks:
		case kOidAdslChanPerfCurr1DayCorrectedBlks:
		case kOidAdslChanPerfCurr1DayUncorrectBlks:
			pObj = MibGetAdslChanPerfTableCounterPtr (&pChanPerf->perfCurr1Day, objId[1]-kOidAdslChanPerfCurr1DayReceivedBlks+1, objLen);
			break;

		case kOidAdslChanPerfPrev1DayMoniSecs:
			*objLen = sizeof(pChanPerf->adslPerfCurr1DayTimeElapsed);
			pObj = &pChanPerf->adslPerfCurr1DayTimeElapsed;
			break;	

		case kOidAdslChanPerfPrev1DayReceivedBlks:
		case kOidAdslChanPerfPrev1DayTransmittedBlks:
		case kOidAdslChanPerfPrev1DayCorrectedBlks:
		case kOidAdslChanPerfPrev1DayUncorrectBlks:
			pObj = MibGetAdslChanPerfTableCounterPtr (&pChanPerf->perfPrev1Day, objId[1]-kOidAdslChanPerfPrev1DayReceivedBlks+1, objLen);
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslChanPerfIntervalObjPtr(void *gDslVars, uchar chId, uchar *objId, int objIdLen, ulong *objLen)
{
	void				*pObj = NULL;
	adslChanCounters	*pIntervals;
	uchar				ind, nInt;

	if (kAdslIntlChannel == chId) {
		pIntervals = globalVar.adslMib.adslChanIntlPerfIntervals;
		nInt = globalVar.adslMib.adslChanIntlPerfData.adslChanPerfValidIntervals;
	}
	else {
		pIntervals = globalVar.adslMib.adslChanFastPerfIntervals;
		nInt = globalVar.adslMib.adslChanFastPerfData.adslChanPerfValidIntervals;
	}

	if (0 == objIdLen) {
		*objLen = sizeof(adslChanCounters) * nInt;
		return pIntervals;
	}
	if ((objId[0] != kOidAdslChanIntervalEntry) || (objIdLen < 4))
		return NULL;
	ind = objId[3];
	if ((ind == 0) || (ind > nInt))
		return NULL;

	switch (objId[1]) {
		case kOidAdslChanIntervalNumber:
			*objLen = sizeof(int);
			globalVar.scratchData = ind;
			pObj = &globalVar.scratchData;
			break;
		case kOidAdslChanIntervalReceivedBlks:
		case kOidAdslChanIntervalTransmittedBlks:
		case kOidAdslChanIntervalCorrectedBlks:
		case kOidAdslChanIntervalUncorrectBlks:
			pObj = MibGetAdslChanPerfTableCounterPtr(pIntervals+ind-1, objId[1]-kOidAdslChanIntervalReceivedBlks+1, objLen);
			break;
		case kOidAdslChanIntervalValidData:
			*objLen = sizeof(int);
			globalVar.scratchData = true;
			pObj = &globalVar.scratchData;
			break;
		default:
			pObj = NULL;
			break;
	}
	return pObj;
}

Private void * MibGetAdslChanObjPtr (void *gDslVars, uchar chId, uchar *objId, int objIdLen, ulong *objLen)
{
	void	*pObj = NULL;

	if ((objIdLen < 3) || (objId[0] != kOidAdslLine) || (objId[1] != kOidAdslMibObjects))
		return NULL;

	if (chId != AdslMibGetActiveChannel(gDslVars))
		return NULL;

	switch (objId[2]) {
		case kOidAdslAturChanTable:
			pObj = MibGetAdslChanTableObjPtr (gDslVars, chId, objId+3, objIdLen-3, objLen);
			break;
		case kOidAdslAturChanPerfTable:
			pObj = MibGetAdslChanPerfTableObjPtr (gDslVars, chId, objId+3, objIdLen-3, objLen);
			break;
		case kOidAdslAturChanIntervalTable:
			pObj = MibGetAdslChanPerfIntervalObjPtr (gDslVars, chId, objId+3, objIdLen-3, objLen);
			break;
		default:
			pObj = NULL;
			break;
	}

	return pObj;
}
#ifdef G993
Private void perToneToPerToneGrpArray(void	*gDslVars, uchar *dstPtr, uchar *srcPtr, bandPlanDescriptor *bp, short gFactor, int elemSize)
{
	int n,gpNum;
	if (globalVar.actualgFactorForToneGroupObjects==false)
		gFactor=8;
	globalVar.actualgFactorForToneGroupObjects=false;
	BlockByteClear(elemSize*512, (void*)dstPtr);
	for(n=0;n<bp->noOfToneGroups;n++)
		for(gpNum=bp->toneGroups[n].startTone/gFactor;gpNum<=bp->toneGroups[n].endTone/gFactor;gpNum++)
			memcpy(&dstPtr[gpNum*elemSize],&srcPtr[gpNum*gFactor*elemSize],elemSize);
}

Private void usPerBandObjectInReportFormat(void	*gDslVars, short *dstPtr, uchar objId, bandPlanDescriptor *bp,int elemSize)
{
	int n,i=0;short specialVal,val;
	BlockByteClear(sizeof(short)*MAX_NUM_BANDS, (void*)dstPtr);
	switch(objId)
	{
		case kOidAdslPrivLATNusperband:
			specialVal=1023;
			if(bp->toneGroups[0].startTone>256)
				memcpy(&dstPtr[i++],&specialVal,sizeof(short));
			for (n=0;n<5;n++)
			{
				if (n<bp->noOfToneGroups)
					val=globalVar.adslMib.perbandDataUs[n].adslCurrAtn;
				else
					val=specialVal;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
		case kOidAdslPrivSATNusperband:
			specialVal=1023;
			if(bp->toneGroups[0].startTone>256)
				memcpy(&dstPtr[i++],&specialVal,sizeof(short));
			for (n=0;n<5;n++)
			{
				if (n<bp->noOfToneGroups)
					val=globalVar.adslMib.perbandDataUs[n].adslSignalAttn;
				else
					val=specialVal;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
		case kOidAdslPrivSNRMusperband:
			specialVal=-512;
			if(bp->toneGroups[0].startTone>256)
				memcpy(&dstPtr[i++],&specialVal,sizeof(short));
			for (n=0;n<5;n++)
			{
				if (n<bp->noOfToneGroups)
					val=globalVar.adslMib.perbandDataUs[n].adslCurrSnrMgn;
				else
					val=specialVal;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
	}
}

Private void dsPerBandObjectInReportFormat(void	*gDslVars, short *dstPtr, uchar objId, bandPlanDescriptor *bp,int elemSize)
{
	short val,n,i=0;
	switch(objId)
	{
		case kOidAdslPrivLATNdsperband:
			for(n=0;n<5;n++) 
			{
				if(n<globalVar.adslMib.dsNegBandPlanDiscovery.noOfToneGroups) {
					val=globalVar.adslMib.perbandDataDs[n].adslCurrAtn;
					val=RestrictValue(val, 0, 1023);
				}
				else
					val=1023;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
		case kOidAdslPrivSATNdsperband:
			for(n=0;n<5;n++) 
			{
				if(n<globalVar.adslMib.dsNegBandPlanDiscovery.noOfToneGroups) {
					val=globalVar.adslMib.perbandDataDs[n].adslSignalAttn;
					val=RestrictValue(val, 0, 1023);
				}
				else
					val=1023;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
		case kOidAdslPrivSNRMdsperband:
			for(n=0;n<5;n++)
			{
				if(n<globalVar.adslMib.dsNegBandPlan.noOfToneGroups) {
					val=globalVar.adslMib.perbandDataDs[n].adslCurrSnrMgn;
					val=RestrictValue(val, -512, 512);
					if(val==512)
						val=-512;
				}
				else
					val=-512;
				memcpy(&dstPtr[i++],&val,sizeof(short));
			}
			break;
	}
}
#endif
#ifndef _M_IX86
Public int AdslMibSetObjectValue(
				void	*gDslVars, 
				uchar	*objId, 
				int		objIdLen,
				uchar	*dataBuf,
				ulong	*dataBufLen)
{
	int			i,len;
	ushort			*pInterval;
	dslCommandStruct	cmd;
	len = (dataBufLen)? *dataBufLen: 0;
#if 0
	__SoftDslPrintf(gDslVars, "objId=%X %X %X %X ,objIdLen= %d, dataBufLen=%u dataBuf= %X %X", 0,
			objId[0],objId[1],objId[2],objId[3],objIdLen,*dataBufLen,dataBuf[0],dataBuf[1]);
#endif
	switch (objId[0])
	{
		case kOidAdslPrivate:
			switch (objId[1]) {
				case kOidAdslPrivExtraInfo:
					switch (objId[2])
					{
#ifdef NTR_SUPPORT
						case kOidAdslPrivSetNtrCfg:
							if(len > sizeof(dslNtrCfg))
								len = sizeof(dslNtrCfg);
							AdslMibByteMove(len, dataBuf,(void*)&globalVar.adslMib.ntrCfg);
							break;
#endif
#ifdef G993
						case kOidAdslPrivSetFlagActualGFactor:
								
								if(*dataBuf==0)
									globalVar.actualgFactorForToneGroupObjects=false;
								else globalVar.actualgFactorForToneGroupObjects=true;
								break;
#endif
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
						case kOIdAdslPrivIncrementErrorSamplesReadPtr:
#if 0
							printk("ReadIdx: %d\n",globalVar.adslMib.vectData.esCtrl.readIdx);
#endif
							if (globalVar.adslMib.vectData.esCtrl.readIdx == MAX_NUM_VECTOR_SYMB-1)
								globalVar.adslMib.vectData.esCtrl.readIdx = 0;
							else
								globalVar.adslMib.vectData.esCtrl.readIdx++;
							break;
#endif
						case kOidAdslPrivSetSnrClampShape:
							cmd.command = kDslSendEocCommand;
							cmd.param.dslClearEocMsg.msgId = kDslSetSnrClampingMask;
							cmd.param.dslClearEocMsg.msgType = len;
							gSharedMemAllocFromUserContext=1;
							cmd.param.dslClearEocMsg.dataPtr = (char *) AdslCoreSharedMemAlloc(len);
							memcpy(cmd.param.dslClearEocMsg.dataPtr,&dataBuf[0],len);
							(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
							gSharedMemAllocFromUserContext=0;
							break;
						case kOidAdslPrivNonLinThldNumBins:
							globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins=dataBuf[0];
							globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins<<=8;
							globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins|=dataBuf[1];
							if (globalVar.adslMib.adslNonLinData.NonLinNumAffectedBins>globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins)
								globalVar.adslMib.adslNonLinData.NonLinearityFlag=1;
							else
								globalVar.adslMib.adslNonLinData.NonLinearityFlag=0;
							break;
						case kOidAdslPrivPLNDurationBins:
							if(len > sizeof(globalVar.PLNDurationBins))
								len = sizeof(globalVar.PLNDurationBins);
							
							for(i=0;i<kPlnNumberOfDurationBins;i++)
								globalVar.PLNDurationBins[i]=0;
							AdslMibByteMove(len, dataBuf,globalVar.PLNDurationBins);
#if 0
							__SoftDslPrintf(gDslVars, "PLNDurationBins=%X %X %X %X", 0,
									globalVar.PLNDurationBins[0],globalVar.PLNDurationBins[1],
									globalVar.PLNDurationBins[2],globalVar.PLNDurationBins[3]);
#endif
							gSharedMemAllocFromUserContext=1;
							if(NULL!=(pInterval=(ushort *)AdslCoreSharedMemAlloc(len))){
								memcpy(pInterval,&dataBuf[0],len);
								cmd.command=kDslPLNControlCmd;
								cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineInpBinTable;
								cmd.param.dslPlnSpec.nInpBin= globalVar.adslMib.adslPLNData.PLNNbDurBins;
								cmd.param.dslPlnSpec.inpBinPtr=pInterval;
								(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
								gSharedMemAllocFromUserContext=0;
							}
							else
								__SoftDslPrintf(gDslVars, "No Shared Mem", 0);
#if 0 
							__SoftDslPrintf(gDslVars, "PLNDurationBins=%d cmd.param.dslPlnSpec.nInpBin=%d", 0,
									globalVar.PLNDurationBins,cmd.param.dslPlnSpec.nInpBin);
#endif
							break;
						case kOidAdslPrivPLNIntrArvlBins:
							if(len > sizeof(globalVar.PLNIntrArvlBins))
								len = sizeof(globalVar.PLNIntrArvlBins);
							
							for(i=0;i<kPlnNumberOfInterArrivalBins;i++)
								globalVar.PLNIntrArvlBins[i]=0;
							AdslMibByteMove(len, dataBuf, globalVar.PLNIntrArvlBins);
#if 0
							__SoftDslPrintf(gDslVars, "PLNIntrArvlBins=%X %X %X %X", 0,
									globalVar.PLNIntrArvlBins[0],globalVar.PLNIntrArvlBins[1],
									globalVar.PLNIntrArvlBins[2],globalVar.PLNIntrArvlBins[3]);
#endif
							gSharedMemAllocFromUserContext=1;
							if(NULL!=(pInterval=(ushort *)AdslCoreSharedMemAlloc(len))){
								memcpy(pInterval,&dataBuf[0],len);
#if 0
								__SoftDslPrintf(gDslVars, "pInterval=%X %X %X %X", 0,
										pInterval[0],pInterval[1],pInterval[2],pInterval[3]);
#endif
								cmd.command=kDslPLNControlCmd;
								cmd.param.dslPlnSpec.plnCmd=kDslPLNControlDefineItaBinTable;
								cmd.param.dslPlnSpec.nItaBin=globalVar.adslMib.adslPLNData.PLNNbIntArrBins;
								cmd.param.dslPlnSpec.itaBinPtr= pInterval;
								(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
								gSharedMemAllocFromUserContext=0;
							}
#if 0
							__SoftDslPrintf(gDslVars,  "cmd.param.dslPlnSpec.nItaBin=%d", 0, cmd.param.dslPlnSpec.nItaBin);
#endif
							break;
						case kOidAdslExtraPLNData:
							switch (objId[3]){
								case kOidAdslExtraPLNDataThldBB:
									globalVar.adslMib.adslPLNData.PLNThldBB=dataBuf[0];
									globalVar.adslMib.adslPLNData.PLNThldBB<<=8;
									globalVar.adslMib.adslPLNData.PLNThldBB|=dataBuf[1];
									break;
								case kOidAdslExtraPLNDataThldPerTone:
									globalVar.adslMib.adslPLNData.PLNThldPerTone=dataBuf[0];
									globalVar.adslMib.adslPLNData.PLNThldPerTone<<=8;
									globalVar.adslMib.adslPLNData.PLNThldPerTone|=dataBuf[1];
									break;
								case kOidAdslExtraPLNDataPLNState:
									if(*dataBuf==2){
#if 0
										__SoftDslPrintf(gDslVars, "Start Requested\n", 0);
#endif
										globalVar.adslMib.adslPLNData.PLNState=1;
									}
									else if(*dataBuf==3){
#if 0
										__SoftDslPrintf(gDslVars, "Stop Requested\n", 0);
#endif
										globalVar.adslMib.adslPLNData.PLNState=0;
									}
									cmd.command=kDslPLNControlCmd;
									cmd.param.value=kDslPLNControlStop;
									(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
									if(globalVar.adslMib.adslPLNData.PLNState==1) {
										cmd.command=kDslPLNControlCmd;
										cmd.param.dslPlnSpec.plnCmd=kDslPLNControlStart;
										cmd.param.dslPlnSpec.mgnDescreaseLevelPerBin= globalVar.adslMib.adslPLNData.PLNThldPerTone;
										cmd.param.dslPlnSpec.mgnDescreaseLevelBand=globalVar.adslMib.adslPLNData.PLNThldBB;
										(*globalVar.cmdHandlerPtr)(gDslVars, &cmd);
									}
									break;
								case kOidAdslExtraPLNDataNbDurBins:
									if(*dataBuf>kPlnNumberOfDurationBins)
										*dataBuf=kPlnNumberOfDurationBins;
									globalVar.adslMib.adslPLNData.PLNNbDurBins=dataBuf[0];
#if 0
									__SoftDslPrintf(gDslVars, "PLNNbDurBins = %d", 0, globalVar.adslMib.adslPLNData.PLNNbDurBins);
#endif
									break;
								case kOidAdslExtraPLNDataNbIntArrBins:
									if(*dataBuf>kPlnNumberOfInterArrivalBins)
										*dataBuf=kPlnNumberOfInterArrivalBins;
									globalVar.adslMib.adslPLNData.PLNNbIntArrBins=dataBuf[0];
#if 0
									__SoftDslPrintf(gDslVars, "PLNNbIntrArrBins = %d", 0, globalVar.adslMib.adslPLNData.PLNNbIntArrBins);
#endif
									break;
								case kOidAdslExtraPLNDataUpdate:
									if(*dataBuf==1)
									{
#if 0
										__SoftDslPrintf(gDslVars, "Update Requested , sending update commands\n", 0);
#endif
										globalVar.adslMib.adslPLNData.PLNUpdateData=0;
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
									}
									break;
							}	
							break;
					}
					break;
			}
			break;
	}
	return kAdslMibStatusSuccess;
}
#endif

Public int	AdslMibGetObjectValue (
				void	*gDslVars, 
				uchar	*objId, 
				int		objIdLen,
				uchar	*dataBuf,
				ulong	*dataBufLen)
{
	uchar		*pObj;
	ulong		objLen, bufLen;
#ifdef G993
	void *pSharedMem = NULL;
#endif

	bufLen = *dataBufLen;
	pObj = NULL;
	if ((NULL == objId) || (0 == objIdLen)) {
		pObj = (void *) &globalVar.adslMib;
		objLen = sizeof (globalVar.adslMib);
	}
	else {
		switch (objId[0]) {
			case kOidAdsl:
				pObj = MibGetAdslObjPtr (gDslVars, objId+1, objIdLen-1, &objLen);
				break;
			case kOidAdslInterleave:
				pObj = MibGetAdslChanObjPtr (gDslVars, kAdslIntlChannel, objId+1, objIdLen-1, &objLen);
				break;
			case kOidAdslFast:
				pObj = MibGetAdslChanObjPtr (gDslVars, kAdslFastChannel, objId+1, objIdLen-1, &objLen);
				break;
			case kOidAtm:
				pObj = MibGetAtmTcObjPtr (gDslVars, objId+1, objIdLen-1, &objLen);
				break;
#ifndef _M_IX86
			case kOidAdslPhyCfg:
				pObj=(void *) &adslCoreCfgProfile;
				objLen=sizeof(adslCfgProfile);
				break;
#endif				
			case kOidAdslPrivate:
				switch (objId[1]) {
					case kOidAdslPrivSNR:
						pObj = (void *) globalVar.snr;
						objLen = sizeof (globalVar.snr[0])*globalVar.nTones;
#ifndef _M_IX86
						__SoftDslPrintf(gDslVars, "kOidAdslPrivSNR Len=%d", 0, globalVar.nTones);
#endif
						break;
					case kOidAdslPrivBitAlloc:
						pObj = (void *) globalVar.bitAlloc;
						objLen = sizeof (globalVar.bitAlloc[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivGain:
						pObj = (void *) globalVar.gain;
						objLen = sizeof (globalVar.gain[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivShowtimeMargin:
						pObj = (void *) globalVar.showtimeMargin;
						objLen = sizeof (globalVar.showtimeMargin[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivChanCharLin:
						pObj = (void *) globalVar.chanCharLin;
						objLen = sizeof (globalVar.chanCharLin[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivChanCharLog:
						pObj = (void *) globalVar.chanCharLog;
						objLen = sizeof (globalVar.chanCharLog[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivQuietLineNoise:
						pObj = (void *) globalVar.quietLineNoise;
						objLen = sizeof (globalVar.quietLineNoise[0]) * globalVar.nTones;
						break;
					case kOidAdslPrivPLNDurationBins:
						pObj = (void *) &globalVar.PLNDurationBins;
						objLen = sizeof (globalVar.PLNDurationBins[0])*kPlnNumberOfDurationBins;
						break;
					case kOidAdslPrivPLNIntrArvlBins:
						pObj = (void *) &globalVar.PLNIntrArvlBins;
						objLen = sizeof (globalVar.PLNIntrArvlBins[0])*kPlnNumberOfInterArrivalBins;
						break;
#ifdef ADSL_MIBOBJ_PLN
					case kOidAdslPrivPLNValueps :
						pObj = (void *) &globalVar.PLNValueps;
						objLen = sizeof (globalVar.PLNValueps[0])*(kAdslMibToneNum*2-32);
						break;
					case kOidAdslPrivPLNThldCntps  :
						pObj = (void *) &globalVar.PLNThldCntps;
						objLen = sizeof (globalVar.PLNThldCntps[0])*(kAdslMibToneNum*2-32);
						break;
#endif
					case kOidAdslPrivPLNDurationHist :
						pObj = (void *) &globalVar.PLNDurationHist;
						objLen = sizeof (globalVar.PLNDurationHist[0])*kPlnNumberOfDurationBins;
						break;
					case kOidAdslPrivPLNIntrArvlHist:
						pObj = (void *) &globalVar.PLNIntrArvlHist;
						objLen = sizeof (globalVar.PLNIntrArvlHist[0])*kPlnNumberOfInterArrivalBins;
						break;
					case kOidAdslPrivNLDistNoise:
						pObj = (void *) &globalVar.distNoisedB;
						objLen = sizeof (globalVar.distNoisedB[0])*kAdslMibToneNum*2;
						break;
#ifdef G993
					case kOidAdslPrivChanCharLinDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,pSharedMem,(uchar *) (&globalVar.chanCharLin[0]), &globalVar.adslMib.dsNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds,sizeof(ComplexShort));
						pObj=pSharedMem;
						objLen=512*sizeof(ComplexShort);
						break;
					case kOidAdslPrivChanCharLinUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,pSharedMem,(uchar *) (&globalVar.chanCharLin[0]), &globalVar.adslMib.usNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus,sizeof(ComplexShort));
						pObj=pSharedMem;
						objLen=512*sizeof(ComplexShort);
						break;
					case kOidAdslPrivChanCharLogDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						if(globalVar.dsBpHlogForReport==NULL)
							break;
						perToneToPerToneGrpArray(gDslVars,pSharedMem,(uchar *) (&globalVar.chanCharLog[0]), globalVar.dsBpHlogForReport, globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivChanCharLogUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.chanCharLog[0]), &globalVar.adslMib.usNegBandPlanDiscovery, globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivQuietLineNoiseDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						if(globalVar.dsBpQLNForReport==NULL)
							break;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.quietLineNoise[0]), globalVar.dsBpQLNForReport, globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivQuietLineNoiseUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.quietLineNoise[0]), &globalVar.adslMib.usNegBandPlanDiscovery, globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivSNRDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						if (globalVar.dsBpSNRForReport==NULL)
							break;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.snr[0]), globalVar.dsBpSNRForReport, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivSNRUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.snr[0]), &globalVar.adslMib.usNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivLATNdsperband:
						pSharedMem=globalVar.scratchObject;
						dsPerBandObjectInReportFormat(gDslVars,(short *)pSharedMem,kOidAdslPrivLATNdsperband,&globalVar.adslMib.dsNegBandPlanDiscovery,sizeof(short));
						pObj=pSharedMem;
						objLen=5*sizeof(short);
						break;
					case kOidAdslPrivSATNdsperband:
						pSharedMem=globalVar.scratchObject;
						dsPerBandObjectInReportFormat(gDslVars,(short *)pSharedMem,kOidAdslPrivSATNdsperband,&globalVar.adslMib.dsNegBandPlanDiscovery,sizeof(short));
						pObj=pSharedMem;
						objLen=5*sizeof(short);
						break;
					case kOidAdslPrivLATNusperband:
						pSharedMem=globalVar.scratchObject;
						usPerBandObjectInReportFormat(gDslVars,(short *)pSharedMem,kOidAdslPrivLATNusperband,&globalVar.adslMib.usNegBandPlanDiscovery,sizeof(short));
						pObj=pSharedMem;
						objLen=5*sizeof(short);
						break;
					case kOidAdslPrivSATNusperband:
						pSharedMem=globalVar.scratchObject;
						usPerBandObjectInReportFormat(gDslVars,(short*)pSharedMem,kOidAdslPrivSATNusperband,&globalVar.adslMib.usNegBandPlanDiscovery,sizeof(short));
						pObj=pSharedMem;
						objLen=5*sizeof(short);
						break;
					case kOidAdslPrivBitAllocDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *) pSharedMem,(uchar *) (&globalVar.bitAlloc[0]), &globalVar.adslMib.dsNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds,sizeof(uchar));
						pObj=pSharedMem;
						objLen=512*sizeof(uchar);
						break;
					case kOidAdslPrivBitAllocUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.bitAlloc[0]), &globalVar.adslMib.usNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus,sizeof(uchar));
						pObj=pSharedMem;
						objLen=512*sizeof(uchar);
						break;
					case kOidAdslPrivGainDsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.gain[0]), &globalVar.adslMib.dsNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
					case kOidAdslPrivGainUsPerToneGroup:
						pSharedMem=globalVar.scratchObject;
						perToneToPerToneGrpArray(gDslVars,(uchar *)pSharedMem,(uchar *) (&globalVar.gain[0]), &globalVar.adslMib.usNegBandPlan, globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus,sizeof(short));
						pObj=pSharedMem;
						objLen=512*sizeof(short);
						break;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
					case kOIdAdslPrivGetVectErrorSamples:
					case kOIdAdslPrivViewVectErrorSamples:
						objLen = sizeof(VectorErrorSample);
						pObj = (uchar*)&globalVar.vectorErrorSampleArray.samples[globalVar.adslMib.vectData.esCtrl.readIdx];
						break;
					case kOIdAdslPrivGetVceMacAddress:
						objLen = sizeof(VceMacAddress);
#if 0
						gSharedMemAllocFromUserContext = 1;
						pSharedMem=AdslCoreSharedMemAlloc(sizeof(VceMacAddress));
#else
						pSharedMem=globalVar.scratchObject;
#endif
						BlockByteMove(sizeof(VceMacAddress),(unsigned char *)&globalVar.adslMib.vectData.macAddress,(uchar *)pSharedMem);
						pObj = pSharedMem;
						break;
#endif
#endif
					case kOidAdslPrivExtraInfo:
						switch (objId[2]) {
							case kOidAdslExtraConnectionInfo:
#if 1
								pObj = (void *) &globalVar.adslMib.xdslConnection;
								objLen = sizeof (globalVar.adslMib.xdslConnection);
#else
								pObj = (void *) &globalVar.adslMib.adslConnection;
								objLen = sizeof (globalVar.adslMib.adslConnection);
#endif							
								break;
							case kOidAdslExtraConnectionStat:
#if 1
								pObj = (void *) &globalVar.adslMib.xdslStat;
								objLen = sizeof (globalVar.adslMib.xdslStat);
#else
								pObj = (void *) &globalVar.adslMib.adslStat;
								objLen = sizeof (globalVar.adslMib.adslStat);
#endif							
								break;
							case kOidAdslExtraFramingMode:
								pObj = (void *) &globalVar.adslMib.adslFramingMode;
								objLen = sizeof (globalVar.adslMib.adslFramingMode);
								break;
							case kOidAdslExtraTrainingState:
								pObj = (void *) &globalVar.adslMib.adslTrainingState;
								objLen = sizeof (globalVar.adslMib.adslTrainingState);
								break;
							case kOidAdslExtraNonStdFramingAdjustK:
								pObj = (void *) &globalVar.adslMib.adslRxNonStdFramingAdjustK;
								objLen = sizeof (globalVar.adslMib.adslRxNonStdFramingAdjustK);
								break;
							case kOidAdslExtraAtmStat:
#if 1
								pObj = (void *) &globalVar.adslMib.atmStat2lp;
								objLen = sizeof (globalVar.adslMib.atmStat2lp);
#else
								pObj = (void *) &globalVar.adslMib.atmStat;
								objLen = sizeof (globalVar.adslMib.atmStat);
#endif							
								break;
							case kOidAdslExtraDiagModeData:
								pObj = (void *) &globalVar.adslMib.adslDiag;
								objLen = sizeof (globalVar.adslMib.adslDiag);
								break;
							case kOidAdslExtraAdsl2Info:
#if 1
								pObj = (void *) &globalVar.adslMib.adsl2Info2lp;
								objLen = sizeof (globalVar.adslMib.adsl2Info2lp);
#else
								pObj = (void *) &globalVar.adslMib.adsl2Info;
								objLen = sizeof (globalVar.adslMib.adsl2Info);
#endif							
								break;
							case kOidAdslExtraTxPerfCounterInfo:
								pObj = (void *) &globalVar.adslMib.adslTxPerfTotal;
								objLen = sizeof (globalVar.adslMib.adslTxPerfTotal);
								break;
							case kOidAdslExtraNLInfo:
								pObj = (void *) &globalVar.adslMib.adslNonLinData;
								objLen= sizeof(globalVar.adslMib.adslNonLinData);
								break;
							case kOidAdslExtraPLNInfo:
								pObj = (void *) &globalVar.adslMib.adslPLNData;
								objLen= sizeof(globalVar.adslMib.adslPLNData);
								break;
#ifdef G993
							case kOidAdslPrivBandPlanUSNeg:
								pObj = (void *) &globalVar.adslMib.usNegBandPlan;
								objLen= sizeof(globalVar.adslMib.usNegBandPlan);
								break;
							case kOidAdslPrivBandPlanUSPhy:
								pObj = (void *) &globalVar.adslMib.usPhyBandPlan;
								objLen= sizeof(globalVar.adslMib.usPhyBandPlan);
								break;
							case kOidAdslPrivBandPlanDSNeg:
								pObj = (void *) &globalVar.adslMib.dsNegBandPlan;
								objLen= sizeof(globalVar.adslMib.dsNegBandPlan);
								break;
							case kOidAdslPrivBandPlanDSPhy:
								pObj = (void *) &globalVar.adslMib.dsPhyBandPlan;
								objLen= sizeof(globalVar.adslMib.dsPhyBandPlan);
								break;
							case kOidAdslPrivBandPlanUSNegDiscovery:
								pObj = (void *) &globalVar.adslMib.usNegBandPlanDiscovery;
								objLen= sizeof(globalVar.adslMib.usNegBandPlanDiscovery);
								break;
							case kOidAdslPrivBandPlanUSPhyDiscovery:
								pObj = (void *) &globalVar.adslMib.usPhyBandPlanDiscovery;
								objLen= sizeof(globalVar.adslMib.usPhyBandPlanDiscovery);
								break;
							case kOidAdslPrivBandPlanDSNegDiscovery:
								pObj = (void *) &globalVar.adslMib.dsNegBandPlanDiscovery;
								objLen= sizeof(globalVar.adslMib.dsNegBandPlanDiscovery);
								break;
							case kOidAdslPrivBandPlanDSPhyDiscovery:
								pObj = (void *) &globalVar.adslMib.dsPhyBandPlanDiscovery;
								objLen= sizeof(globalVar.adslMib.dsPhyBandPlanDiscovery);
								break;
#endif						
								
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM963268)
							case kOidAdslExtraDiagModeDataVdsl:
								pObj = (void *) &globalVar.adslMib.vdslDiag;
								objLen = sizeof (globalVar.adslMib.vdslDiag);
								break;
							case  kOidAdslExtraVdsl2Info:
								pObj = (void *) &globalVar.adslMib.vdslInfo;
								objLen = sizeof (globalVar.adslMib.vdslInfo);
								break;
#endif	
							case kOidAdslExtraXdslInfo:
								pObj = (void *) &globalVar.adslMib.xdslInfo;
								objLen = sizeof (globalVar.adslMib.xdslInfo);
								break;
						}
						break;
				}
				break;
		}
	}
	if (NULL == pObj)
		return kAdslMibStatusNoObject;

	*dataBufLen = objLen;
	if (NULL == dataBuf)
		return (int)pObj;
	if (objLen > bufLen)
		return kAdslMibStatusBufferTooSmall;

	AdslMibByteMove(objLen, pObj, dataBuf);

	return kAdslMibStatusSuccess;
}
