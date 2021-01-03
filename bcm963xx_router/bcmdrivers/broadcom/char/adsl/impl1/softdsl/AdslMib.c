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
 * AdslMib.c -- Adsl MIB data manager
 *
 * Description:
 *  This file contains functions for ADSL MIB (RFC 2662) data management
 *
 *
 * Copyright (c) 1993-1998 AltoCom, Inc. All rights reserved.
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.6 $
 *
 * $Id: AdslMib.c,v 1.6 2011/06/24 01:52:40 dkhoo Exp $
 *
 * $Log: AdslMib.c,v $
 * Revision 1.6  2011/06/24 01:52:40  dkhoo
 * [All]Per PLM, update DSL driver to 23j and bonding datapump to Av4bC035d
 *
 * Revision 1.22  2007/09/04 07:21:15  tonytran
 * PR31097: 1_28_rc8
 *
 * Revision 1.21  2006/04/15 11:54:24  ovandewi
 * fix trellis status snooping
 *
 * Revision 1.20  2006/04/05 05:06:46  dadityan
 * Fix for wron ATU-C vendor Id
 *
 * Revision 1.16  2004/06/04 18:56:01  ilyas
 * Added counter for ADSL2 framing and performance
 *
 * Revision 1.15  2004/04/12 23:34:52  ilyas
 * Merged the latest ADSL driver chnages for ADSL2+
 *
 * Revision 1.14  2004/03/03 20:14:05  ilyas
 * Merged changes for ADSL2+ from ADSL driver
 *
 * Revision 1.13  2003/10/17 21:02:12  ilyas
 * Added more data for ADSL2
 *
 * Revision 1.12  2003/10/14 00:55:27  ilyas
 * Added UAS, LOSS, SES error seconds counters.
 * Support for 512 tones (AnnexI)
 *
 * Revision 1.10  2003/07/18 19:07:15  ilyas
 * Merged with ADSL driver
 *
 * Revision 1.9  2002/11/13 21:32:49  ilyas
 * Added adjustK support for Centillium non-standard framing mode
 *
 * Revision 1.8  2002/10/31 20:27:13  ilyas
 * Merged with the latest changes for VxWorks/Linux driver
 *
 * Revision 1.7  2002/07/20 00:51:41  ilyas
 * Merged witchanges made for VxWorks/Linux driver.
 *
 * Revision 1.6  2002/02/01 06:42:48  ilyas
 * Ignore ASx chaanels for transmit coding parameters
 *
 * Revision 1.5  2002/01/13 22:25:40  ilyas
 * Added functions to get channels rate
 *
 * Revision 1.4  2002/01/03 06:03:36  ilyas
 * Handle byte moves tha are not multiple of 2
 *
 * Revision 1.3  2002/01/02 19:13:57  liang
 * Fix compiler warning.
 *
 * Revision 1.2  2001/12/22 02:37:30  ilyas
 * Changed memset,memcpy function to BlockByte functions
 *
 * Revision 1.1  2001/12/21 22:39:30  ilyas
 * Added support for ADSL MIB data objects (RFC2662)
 *
 *
 *****************************************************************************/

#include "SoftDsl.gh"
#include "AdslMib.h"
#include "BlockUtil.h"
#if defined(G993) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
/* Targets w/o G993 need SoftDslG993p2.h for kDsl993p2FramerAdslDs/kDsl993p2FramerAdslUs, NTR defines */
#include "SoftDslG993p2.h"
#endif

#define globalVar   gAdslMibVars

#define k15MinInSeconds             (15*60)
#define k1DayInSeconds              (24*60*60)

#define Q4ToTenth(num)              ((((num) * 10) + 8) >> 4)
#define Q8ToTenth(num)              ((((num) * 10) + 128) >> 8)
#define RestrictValue(n,l,r)        ((n) < (l) ? (l) : (n) > (r) ? (r) : (n))

#define NitroRate(rate)             ((((rate)*53)+48) / 49)
#define ActualRate(rate)    (globalVar.adslMib.adslFramingMode & kAtmHeaderCompression ? NitroRate(rate) : rate)

#define ReadCnt16(pData)  \
  (((ulong) ((uchar *)(pData))[0] << 8) + ((uchar *)(pData))[1])
#define ReadCnt32(pData)          \
  (((ulong) ((uchar *)(pData))[0] << 24) + \
  ((ulong) ((uchar *)(pData))[1] << 16) + \
  ((ulong) ((uchar *)(pData))[2] << 8)  + \
  ((uchar *)(pData))[3])

long	secElapsedInDay;

#define     MIN(a,b) (((a)<(b))?(a):(b))

#define     BITSWAP_REQ_TIMEOUT (10 * 1000)  /* 10 seconds in ms */

#ifdef CONFIG_VDSL_SUPPORTED
#ifdef SUPPORT_DSL_BONDING
#define MAX_TONE_NUM    (kVdslMibMaxToneNum + (kVdslMibMaxToneNum >> 1))    /* 3k per line for 4-band bonding */
#else
#define MAX_TONE_NUM    kVdslMibMaxToneNum
#endif
#else
#define MAX_TONE_NUM    kAdslMibMaxToneNum
#endif
static short                   gSnr[MAX_TONE_NUM];
static short                   gShowtimeMargin[MAX_TONE_NUM];
static uchar                   gBitAlloc[MAX_TONE_NUM];
static short                   gGain[MAX_TONE_NUM];
static ComplexShort      gChanCharLin[MAX_TONE_NUM];
static short                   gChanCharLog[MAX_TONE_NUM];
static short                   gQuietLineNoise[MAX_TONE_NUM];
static char                    gVendorTbl[][6] =
{
   {0x0, 0x0, '?', '?', '?', '?'},         /* Unknown */
   {0xB5, 0, 'B', 'D', 'C', 'M'},   /* Broadcom */
   {0xB5, 0, 'G', 'S', 'P', 'N'},   /* Globespan */
   {0xB5, 0, 'A', 'N', 'D', 'V'},   /* ADI */
   {0xB5, 0, 'T', 'S', 'T', 'C'},   /* TI */
   {0xB5, 0, 'C', 'E', 'N', 'T'},   /* Centillium */
   {0xB5, 0, 'A', 'L', 'C', 'B'},   /* Alcatel */
   {0xB5, 0, 'I', 'F', 'T', 'N'},   /* Infineon */
   {0xB5, 0, 'I', 'K', 'N', 'S'},   /* Ikanos */
   {0xB5, 0, 'C', 'T', 'N', 'W'},   /* Catena */
   {0xB5, 0, 'A', 'L', 'C', 'B'}, /* AlcatelLSpan */
   {0xB5, 0, 'C', 'X', 'S', 'Y'},   /* Conexant */
   {0xB5, 0, 'C', 'E', 'N', 'T'}    /* Centillium */
};
#define  VENDOR_TBL_SIZE   (sizeof(gVendorTbl)/sizeof(gVendorTbl[0]))

#define IncPerfCounterVar(perfEntry, varname) do {                      \
    (*(adslPerfDataEntry*)(perfEntry)).perfTotal.varname++;             \
    (*(adslPerfDataEntry*)(perfEntry)).perfSinceShowTime.varname++;     \
    (*(adslPerfDataEntry*)(perfEntry)).perfCurr15Min.varname++;         \
    (*(adslPerfDataEntry*)(perfEntry)).perfCurr1Day.varname++;          \
} while (0)

#define AddBlockCounterVar(chEntry, varname, inc) do {                  \
    (*(adslChanPerfDataEntry *)(chEntry)).perfTotal.varname += inc;     \
    (*(adslChanPerfDataEntry *)(chEntry)).perfCurr15Min.varname += inc; \
    (*(adslChanPerfDataEntry *)(chEntry)).perfCurr1Day.varname += inc;  \
} while (0)

#define IncFailureCounterVar(perfEntry, varname) do {                   \
    (*(adslPerfDataEntry*)(perfEntry)).failTotal.varname++;             \
    (*(adslPerfDataEntry*)(perfEntry)).failSinceShowTime.varname++;     \
    (*(adslPerfDataEntry*)(perfEntry)).failCurDay.varname++;            \
    (*(adslPerfDataEntry*)(perfEntry)).failCur15Min.varname++;          \
} while (0)

#define IncAtmRcvCounterVar(mib, pathId, varname, n) do {						\
    (*(adslMibInfo*)(mib)).atmStat2lp[pathId].rcvStat.varname += n;			    \
    (*(adslMibInfo*)(mib)).atmStatSincePowerOn2lp[pathId].rcvStat.varname += n; \
    (*(adslMibInfo*)(mib)).atmStatCurDay2lp[pathId].rcvStat.varname += n;       \
    (*(adslMibInfo*)(mib)).atmStatCur15Min2lp[pathId].rcvStat.varname += n;     \
} while (0)

#define IncAtmXmtCounterVar(mib, pathId, varname, n) do {						\
    (*(adslMibInfo*)(mib)).atmStat2lp[pathId].xmtStat.varname += n;			    \
    (*(adslMibInfo*)(mib)).atmStatSincePowerOn2lp[pathId].xmtStat.varname += n; \
    (*(adslMibInfo*)(mib)).atmStatCurDay2lp[pathId].xmtStat.varname += n;       \
    (*(adslMibInfo*)(mib)).atmStatCur15Min2lp[pathId].xmtStat.varname += n;     \
} while (0)

extern unsigned long BcmAdslCoreGetConfiguredMod(uchar lineId);
extern void AdslCoreIndicateLinkPowerStateL2(uchar lineId);
extern int sprintf( char *, const char *, ... );

Private int SM_DECL AdslMibNotifyIdle (void *gDslVars, ulong event)
{
    return 0;
}

Public Boolean AdslMibIsAdsl2Mod(void *gDslVars)
{
    int     modType  = AdslMibGetModulationType(gDslVars);

    return ((kAdslModAdsl2 == modType) || (kAdslModAdsl2p == modType) ||(kAdslModReAdsl2 == modType));
}

Public Boolean XdslMibIsVdsl2Mod(void *gDslVars)
{
    int     modType  = AdslMibGetModulationType(gDslVars);

    return (kVdslModVdsl2 == modType);
}

Public Boolean XdslMibIsXdsl2Mod(void *gDslVars)
{
    int     modType  = AdslMibGetModulationType(gDslVars);

    return ((kAdslModAdsl2 == modType) || (kAdslModAdsl2p == modType) ||
    (kAdslModReAdsl2 == modType) || (kVdslModVdsl2 == modType) );
}

Public Boolean XdslMibIsAtmConnectionType(void *gDslVars)
{
	unsigned char tmType;
	tmType = globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[0].tmType[0];
#if 0
#if defined(CONFIG_VDSL_SUPPORTED)
	if (XdslMibIsVdsl2Mod(gDslVars))
		tmType = globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[0].tmType[0];
	else
#endif
		tmType = globalVar.adslMib.adsl2Info2lp[0].xmtChanInfo.connectionType;
#endif
	return ((kXdslDataAtm == tmType) || (kXdslDataNitro == tmType));
}

Public Boolean XdslMibIsPtmConnectionType(void *gDslVars)
{
	unsigned char tmType;
	tmType = globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[0].tmType[0];
#if 0
#if defined(CONFIG_VDSL_SUPPORTED)
	if(XdslMibIsVdsl2Mod(gDslVars))
		tmType = globalVar.adslMib.vdslInfo[0].xmt2Info.tmType[0];
	else
#endif
		tmType = globalVar.adslMib.adsl2Info2lp[0].xmtChanInfo.connectionType;
#endif
	return(kXdslDataPtm == tmType);
}

#define kTwoThirdQ15      ((long)21845)   /* 2/3 in Q15 */
#define kFourFifthQ15     ((long)26214)   /* 4/5 in Q15 */
#define kTenLog10TwoQ12     ((long)12330)   /* 10*log10(2) in Q12 */
#define kTenLog10EQ12     ((long)17789)   /* 10*log10(e) in Q12 */
Public short
UtilQ0LinearToQ4dB (ulong x)
  {
  int   k;
  long  y, z, v2, v3, v4, u;
  /*long  v5;*/
  
  /* (1) speical cases */
  if (x <= 1) return 0;
  
  /* (2) normalize x */
  y = (long)x;
  k = 32;
  while (y > 0) { y <<= 1; k--;}
  
  /* (3) approximate ln(y) */
  z  = ((-y) >> 17) & 0x7FFF;           /* z  = (1-y) in Q15 */
  v2 = (z * z) >> 16;               /* v2 = z**2 / 2 in Q15 */
  v3 = (v2 * ((kTwoThirdQ15 * z) >> 15)) >> 15; /* v3 = z**3 / 3 in Q15 */
  v4 = (v2 * v2) >> 15;             /* v4 = z**4 / 4 in Q15 */
  /*v5 = (v4 * ((kFourFifthQ15 * z) >> 15)) >> 15;*/  /* v5 = z**5 / 5 in Q15 */
  
  u = z + v2 + v3 + v4;
  
  /* (4) calculate output */
  return ((kTenLog10TwoQ12 * k - ((kTenLog10EQ12 * u) >> 15) + 0x80) >> 8);
  
  }
#ifdef G993
Public short log2Int (unsigned long x)
  {
    short k=0;
    unsigned long y=1;
    while(y<x) {y<<=1;k++;}
    return (1<<k);
  }

Public void CreateNegBandPlan(void *gDslVars, bandPlanDescriptor* NegtBandPlan, bandPlanDescriptor32* NegtBandPlan32, bandPlanDescriptor* PhysBandPlan)
{
	int n,j=0,i=0;
	NegtBandPlan->reserved=NegtBandPlan32->reserved;
	NegtBandPlan->noOfToneGroups=0;
	for(n=0; n<PhysBandPlan->noOfToneGroups;n++)
	{
		for (; j<NegtBandPlan32->noOfToneGroups;j++)
		{
			if (NegtBandPlan32->toneGroups[j].startTone>=PhysBandPlan->toneGroups[n].startTone && NegtBandPlan32->toneGroups[j].startTone<=PhysBandPlan->toneGroups[n].endTone)
			{
				NegtBandPlan->noOfToneGroups++;
				NegtBandPlan->toneGroups[i].startTone=NegtBandPlan32->toneGroups[j].startTone;
				for (; j<NegtBandPlan32->noOfToneGroups;j++)
				{
					if (NegtBandPlan32->toneGroups[j].endTone<=PhysBandPlan->toneGroups[n].endTone)
						NegtBandPlan->toneGroups[i].endTone=NegtBandPlan32->toneGroups[j].endTone;
					else 
					    break;
				}
				__SoftDslPrintf(gDslVars, "NegtBP start=%d end=%d", 0,NegtBandPlan->toneGroups[i].startTone,NegtBandPlan->toneGroups[i].endTone);
				i++;
				break;
			}
			if (NegtBandPlan32->toneGroups[j].startTone>PhysBandPlan->toneGroups[n].endTone)
				break;
		}
	}
}

Public short calcOamGfactor(Bp993p2 *bp)
{
  int i;unsigned long lastTone; 
  i = bp->noOfToneGroups-1;
  lastTone=bp->toneGroups[i].endTone;
  return log2Int((lastTone+511)>>9);
}
#endif

Public void AdslMibByteMove (int size, void* srcPtr, void* dstPtr)
{
    int     len = size & ~1;
    uchar   *sPtr, *dPtr;

    if (len)
        BlockByteMove (len, srcPtr, dstPtr);

    sPtr = ((uchar*)srcPtr) + len;
    dPtr = ((uchar*)dstPtr) + len;
    srcPtr = ((uchar*)srcPtr) + size;
    while (sPtr != srcPtr)
        *dPtr++ = *sPtr++;
}

Public void AdslMibByteClear(int size, void* dstPtr)
{
    uchar   *dPtr = dstPtr;
    uchar   *dPtrEnd = dPtr + size;

    while (dPtr != dPtrEnd)
        *dPtr++ = 0;
}

Public int AdslMibStrCopy(char *srcPtr, char *dstPtr)
{
    char    *sPtr = srcPtr;

    do {
        *dstPtr = *srcPtr++;
    } while (*dstPtr++ != 0);

    return srcPtr - sPtr;
}

Public int AdslMibGetActiveChannel(void *gDslVars)
{
    return (globalVar.adslMib.adslConnection.chType);

}

Public int AdslMibGetModulationType(void *gDslVars)
{
    return globalVar.adslMib.adslConnection.modType & kAdslModMask;
}

#ifndef _M_IX86
Private int AdslMibGetFastBytesInFrame(G992CodingParams *param)
{
    int     n = 0;

#ifdef  G992P1_NEWFRAME
    if (param->NF >= 0x0200)    /* old PHY (A023a or earlier) */
        n = param->RSF - param->AS3BF;

    n +=  param->AS0BF;
    n += param->AS1BF;
    n += param->AS2BF;
    n += param->AS3BF;
    n += param->LS0CF;
    n += param->LS1BF;
    n += param->LS2BF;
#else
    n = 0;
#endif
    return n;
}

Private int AdslMibGetIntlBytesInFrame(G992CodingParams *param)
{
    int     n;

#ifdef  G992P1_NEWFRAME
    n =  param->AS0BI;
    n += param->AS1BI;
    n += param->AS2BI;
    n += param->AS3BI;
    n += param->LS0CI;
    n += param->LS1BI;
    n += param->LS2BI;
#else
    n = (param->K - 1);
#endif
    return n;
}

Public int AdslMibGetGetChannelRate(void *gDslVars, int dir, int channel)
{
  G992CodingParams *param;
  int               n=0;
  if ( (0 != channel ) && (1 != channel))
    return n;
  param = (kAdslRcvDir == dir) ? &globalVar.rcvParams : &globalVar.xmtParams;
#ifdef G992P3
#ifdef CONFIG_VDSL_SUPPORTED
  if(XdslMibIsVdsl2Mod(gDslVars))
    n = 1000 * ((kAdslRcvDir == dir) ? globalVar.adslMib.vdslInfo[channel].rcvRate : globalVar.adslMib.vdslInfo[channel].xmtRate);
  else
#endif /* CONFIG_VDSL_SUPPORTED */
  if (AdslMibIsAdsl2Mod(gDslVars)) {
#if (MAX_LP_NUM == 2)
    n = 1000 * ((kAdslRcvDir == dir) ? globalVar.adslMib.adsl2Info2lp[channel].rcvRate : globalVar.adslMib.adsl2Info2lp[channel].xmtRate);
#else
    if (kAdslIntlChannel == channel)
      n = 1000 * ((kAdslRcvDir == dir) ? globalVar.adslMib.adsl2Info.rcvRate : globalVar.adslMib.adsl2Info.xmtRate);
#endif /* #if (MAX_LP_NUM == 2) */
  }
  else
#endif /* G992P3 */
  {
    if (kAdslIntlChannel == channel)
      n = AdslMibGetIntlBytesInFrame(param);
    else 
      n = AdslMibGetFastBytesInFrame(param);
    
    n *= 4000 * 8;

    if (globalVar.adslMib.adslFramingMode & kAtmHeaderCompression)
      n = NitroRate(n);
  }

  return n;
}

Private void AdslMibSetChanEntry(G992CodingParams *param, adslChanEntry *pChFast, adslChanEntry *pChIntl)
{
    int     n;

    pChIntl->adslChanIntlDelay = 4 + (param->S*param->D >> 2) + ((param->S - 1) >> 2);
    pChFast->adslChanPrevTxRate = pChFast->adslChanCurrTxRate;
    pChIntl->adslChanPrevTxRate = pChIntl->adslChanCurrTxRate;
    n = AdslMibGetFastBytesInFrame(param);
    pChFast->adslChanCrcBlockLength = n * 68;
    pChFast->adslChanCurrTxRate = n * 4000 * 8;

    n = AdslMibGetIntlBytesInFrame(param);
    pChIntl->adslChanCrcBlockLength = n * 68;
    pChIntl->adslChanCurrTxRate = n * 4000 * 8;
}

Private void AdslMibSetConnectionInfo(void *gDslVars, G992CodingParams *param, long code, long val, adslConnectionInfo *pConInfo)
{
    adslDataConnectionInfo *pInfo;

    pInfo = (kG992p2XmtCodingParamsInfo == code) ? &pConInfo->xmtInfo : &pConInfo->rcvInfo;
    pInfo->K = param->K;
    pInfo->S = param->S;
    pInfo->R = param->R;
    pInfo->D = param->D;

    pConInfo->trellisCoding = (0 == val) ? kAdslTrellisOff : kAdslTrellisOn;
    if(!XdslMibIsXdsl2Mod(gDslVars))
        pConInfo->chType = AdslMibGetFastBytesInFrame(param) ? kAdslFastChannel : kAdslIntlChannel;
    if (kG992p2XmtCodingParamsInfo == code) 
        if (val != 0)
            pConInfo->trellisCoding2 |= kAdsl2TrellisTxEnabled;
        else
            pConInfo->trellisCoding2 &= ~kAdsl2TrellisTxEnabled;
    else
        if (val != 0)
            pConInfo->trellisCoding2 |= kAdsl2TrellisRxEnabled;
        else
            pConInfo->trellisCoding2 &= ~kAdsl2TrellisRxEnabled;

    /* Centillium NS framing for S = 1/2 */
    if (kG992p2RcvCodingParamsInfo == code) {
        if (((pInfo->K + pInfo->R) > 255) && (6 == globalVar.rsOption[0]))
            globalVar.adslMib.adslRxNonStdFramingAdjustK = 6;
        else 
            globalVar.adslMib.adslRxNonStdFramingAdjustK = 0;
    }
}

Private void Adsl2MibSetConnectionInfo(void *gDslVars, G992p3CodingParams *param, long code, long val, adsl2ConnectionInfo *pConInfo)
{
    adsl2DataConnectionInfo *pInfo;

    pInfo = (kG992p3XmtCodingParamsInfo == code) ? &pConInfo->xmt2Info : &pConInfo->rcv2Info;

    pInfo->Nlp  = param->Nlp;
    pInfo->Nbc  = param->Nbc;
    pInfo->MSGlp= param->MSGlp;
    pInfo->MSGc = param->MSGc;

    pInfo->L = param->L;
    pInfo->M = param->M;
    pInfo->T = param->T;
    pInfo->D = param->D;
    pInfo->R = param->R;
    pInfo->B = param->B;

    if(kG992p3RcvCodingParamsInfo == code )
        globalVar.adslMib.xdslConnection[(int)globalVar.pathId].chType = (pInfo->D > 1) ? kAdslIntlChannel: kAdslFastChannel;
}

Private void Adsl2MibSetInfoFromGdmt1(void *gDslVars, adsl2DataConnectionInfo *pInfo2, adslDataConnectionInfo *pInfo)
{
    if (pInfo2->L != 0)
        return;

    pInfo2->Nlp = 1;
    pInfo2->Nbc = 1;
    pInfo2->MSGlp= 0;
    pInfo2->MSGc = 1;

    pInfo2->D = pInfo->D;
    pInfo2->R = pInfo->R;
    pInfo2->M = pInfo->S;
    pInfo2->B = pInfo->K - 1;
    pInfo2->L = (pInfo->K + pInfo->R/pInfo->S) * 8;
    pInfo2->T = 1;
}

#if 0
Private void Adsl2MibSetInfoFromGdmt(void *gDslVars)
{
#ifdef CONFIG_VDSL_SUPPORTED
    Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info2lp[(int)globalVar.pathId].rcv2Info, &globalVar.adslMib.xdslConnection[(int)globalVar.pathId].rcvInfo);
    Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info2lp[(int)globalVar.pathId].xmt2Info, &globalVar.adslMib.xdslConnection[(int)globalVar.pathId].xmtInfo);
#else
    Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info.rcv2Info, &globalVar.adslMib.adslConnection.rcvInfo);
    Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info.xmt2Info, &globalVar.adslMib.adslConnection.xmtInfo);
#endif
}
#endif

#if 0
Private void Vdsl2MibSetConnectionInfo(void *gDslVars, FramerDeframerOptions *param, long code,  vdsl2ConnectionInfo *pConInfo)
{
  vdslMuxFramerParamType  *pInfo;
  adsl2DelayInp            *pDelayInp;
  adslConnectionInfo    *pXdslInfo = &globalVar.adslMib.xdslConnection[param->path];
  adslConnectionStat    *pXdslStat = &globalVar.adslMib.xdslStat[param->path];

  if(kDsl993p2FramerDeframerUs == code) {
    pInfo = &pConInfo->xmt2Info;
    pDelayInp = &pConInfo->xmt2DelayInp;
    if(param->codingType & kAdslTrellisOn)
      pXdslInfo->trellisCoding2 |= kAdsl2TrellisTxEnabled;
    else
      pXdslInfo->trellisCoding2 &= ~kAdsl2TrellisTxEnabled;
    if(param->fireEnabled & 0x2)
      pXdslStat->fireStat.status |= kFireUsEnabled;
    else
      pXdslStat->fireStat.status &= ~kFireUsEnabled;
  }
  else {
    pInfo = &pConInfo->rcv2Info;
    pDelayInp = &pConInfo->rcv2DelayInp;
    if(param->codingType & kAdslTrellisOn)
      pXdslInfo->trellisCoding2 |= kAdsl2TrellisRxEnabled;
    else
      pXdslInfo->trellisCoding2 &= ~kAdsl2TrellisRxEnabled;
    if(param->fireEnabled & 0x2)
      pXdslStat->fireStat.status |= kFireDsEnabled;
    else
      pXdslStat->fireStat.status &= ~kFireDsEnabled;
    globalVar.adslMib.xdslConnection[param->path].chType = (param->D > 1) ? kAdslIntlChannel: kAdslFastChannel;
  }
  
  pInfo->N = param->N;
  pInfo->D = param->D;
  pInfo->L = param->L;
  pInfo->B[0] = (unsigned char)param->B;
  pInfo->B[1] = (unsigned char)param->b1;
  pInfo->I = param->I;
  pInfo->M = param->M;
  pInfo->T = param->T;
  pInfo->G = param->G;
  pInfo->U = (unsigned char)param->U;
  pInfo->F = param->F;
  pInfo->R = param->R;
  
  pInfo->ovhType = param->ovhType;
  pInfo->ahifChanId[0] = param->ahifChanId[0];
  pInfo->ahifChanId[1] = param->ahifChanId[1] ;
  pInfo->tmType[0] = param->tmType[0];
  pInfo->tmType[1] = param->tmType[1];

  pInfo->pathId = param->path;
  
  pDelayInp->delay = param->delay;
  pDelayInp->inp = param->INP;
}
#endif

Private int GetAdsl2Inpq(xdslFramingInfo *p2, int q)
{
   return (0 == p2->L) ? 0 : (4*p2->R*p2->D*q)/p2->L;
}

Private int GetAdsl2Delayq(xdslFramingInfo *p2, int q)
{
   return (0 == p2->L) ? 0 : (q*8*(p2->M*(p2->B[0]+1)+p2->R)*p2->D/p2->L)/4;
}

#ifdef CONFIG_VDSL_SUPPORTED
Private void Xdsl2MibConvertVdslFramerParam(vdslMuxFramerParamType *pVdslFramingParam, xdslFramingInfo * pXdslParam)
{
   pVdslFramingParam->N = pXdslParam->N;
   pVdslFramingParam->D = pXdslParam->D;
   pVdslFramingParam->L = pXdslParam->L;
   pVdslFramingParam->B[0] = (unsigned char)pXdslParam->B[0];
   pVdslFramingParam->B[1] = (unsigned char)pXdslParam->B[1];
   pVdslFramingParam->I = pXdslParam->I;
   pVdslFramingParam->M = pXdslParam->M;
   pVdslFramingParam->T = pXdslParam->T;
   pVdslFramingParam->G = pXdslParam->G;
   pVdslFramingParam->U = pXdslParam->U;
   pVdslFramingParam->F = pXdslParam->F;
   pVdslFramingParam->R = pXdslParam->R;
   pVdslFramingParam->ovhType = pXdslParam->ovhType;
   pVdslFramingParam->ahifChanId[0] = pXdslParam->ahifChanId[0];
   pVdslFramingParam->ahifChanId[1] = pXdslParam->ahifChanId[1] ;
   pVdslFramingParam->tmType[0] = pXdslParam->tmType[0];
   pVdslFramingParam->tmType[1] = pXdslParam->tmType[1];
   pVdslFramingParam->pathId = pXdslParam->pathId;
}
#endif

Private void XdslMibConvertToAdslFramerParam(void *gDslVars, int dir,
   adslDataConnectionInfo *pAdslFramingParam,
   adsl2DataConnectionInfo   *pAdsl2FramingParam,
   xdslDirFramingInfo      *pDirFramingInfo,
   xdslFramingInfo         *pXdslFramingParam)
{
   pAdsl2FramingParam->Nlp = pDirFramingInfo->Nlp;
   pAdsl2FramingParam->Nbc = pDirFramingInfo->Nbc;
   pAdsl2FramingParam->MSGlp = pDirFramingInfo->MSGlp;
   pAdsl2FramingParam->MSGc = pXdslFramingParam->U*pXdslFramingParam->G - 6;

   pAdsl2FramingParam->L = pXdslFramingParam->L;
   pAdsl2FramingParam->M = pXdslFramingParam->M;
   pAdsl2FramingParam->T = pXdslFramingParam->T;
   pAdsl2FramingParam->D = pXdslFramingParam->D;
   pAdsl2FramingParam->R = pXdslFramingParam->R;
   pAdsl2FramingParam->B = pXdslFramingParam->B[0];
   pAdslFramingParam->K = pXdslFramingParam->B[0]+pXdslFramingParam->B[1]+1;

   if(!AdslMibIsAdsl2Mod(gDslVars)) {
      pAdslFramingParam->S = (uchar)pXdslFramingParam->S.num;
      /* Centillium NS framing for S = 1/2 */
      if (DS_DIRECTION == dir) {
        if (((pAdslFramingParam->K + pAdslFramingParam->R) > 255) && (6 == globalVar.rsOption[0]))
            globalVar.adslMib.adslRxNonStdFramingAdjustK = 6;
        else 
            globalVar.adslMib.adslRxNonStdFramingAdjustK = 0;
      }
   }
#if 0
    if (pInfo2->L != 0)
        return;

    pInfo2->Nlp = 1;
    pInfo2->Nbc = 1;
    pInfo2->MSGlp= 0;
    pInfo2->MSGc = 1;

    pInfo2->D = pInfo->D;
    pInfo2->R = pInfo->R;
    pInfo2->M = pInfo->S;
    pInfo2->B = pInfo->K - 1;
    pInfo2->L = (pInfo->K + pInfo->R/pInfo->S) * 8;
    pInfo2->T = 1;
#endif
}

Private void XdslMibConvertFromAdslFramerParam(void *gDslVars,
   adslDataConnectionInfo *pAdslFramingParam,
   adsl2DataConnectionInfo   *pAdsl2FramingParam,
   xdslDirFramingInfo      *pDirFramingInfo,
   xdslFramingInfo         *pXdslFramingParam)
{
   pDirFramingInfo->Nlp = pAdsl2FramingParam->Nlp;
   pDirFramingInfo->Nbc = pAdsl2FramingParam->Nbc;
   pDirFramingInfo->MSGlp = pAdsl2FramingParam->MSGlp;
   
   pXdslFramingParam->L = (ushort)pAdsl2FramingParam->L;
   pXdslFramingParam->M = (uchar)pAdsl2FramingParam->M;
   pXdslFramingParam->T = (uchar)pAdsl2FramingParam->T;
   pXdslFramingParam->D = pAdsl2FramingParam->D;
   pXdslFramingParam->R = (uchar)pAdsl2FramingParam->R;
   pXdslFramingParam->B[0] = (uchar)pAdsl2FramingParam->B;
   pXdslFramingParam->N = (pAdsl2FramingParam->B+1)*pAdsl2FramingParam->M + pAdsl2FramingParam->R;
   
   if(AdslMibIsAdsl2Mod(gDslVars)) {
      pXdslFramingParam->G = 1;
      pXdslFramingParam->U = pAdsl2FramingParam->MSGc + 6;
      pXdslFramingParam->S.num = 8*pXdslFramingParam->N;
      pXdslFramingParam->S.denom = pXdslFramingParam->L;
   }
   else {
      pXdslFramingParam->S.num = pAdslFramingParam->S;
      pXdslFramingParam->S.denom = 1;
   }
}

Private void Xdsl2MibConvertConnectionInfo(void *gDslVars, int pathId, int dir)
{
#ifdef CONFIG_VDSL_SUPPORTED
   vdslMuxFramerParamType   *pVdslFramingParam;
#endif
   adsl2DelayInp                  *pDelayInp;
   adslDataConnectionInfo     *pAdslFramingParam;
   adsl2DataConnectionInfo   *pAdsl2FramingParam;
   adsl2ChanInfo                 *pAdsl2ChanInfo;
   xdslConnectionInfo     *pXdslInfo = &globalVar.adslMib.xdslInfo;
   xdslDirFramingInfo      *pDirFramingInfo = &globalVar.adslMib.xdslInfo.dirInfo[dir];
   xdslFramingInfo         *pXdslFramingParam = &globalVar.adslMib.xdslInfo.dirInfo[dir].lpInfo[pathId];
   Boolean                    newStat = true;

#ifdef CONFIG_VDSL_SUPPORTED
   if(XdslMibIsVdsl2Mod(gDslVars)) {
      globalVar.adslMib.vdslInfo[0].vdsl2Mode = pXdslInfo->xdslMode;
      globalVar.adslMib.vdslInfo[0].pwrState = pXdslInfo->pwrState;
      globalVar.adslMib.vdslInfo[0].vdsl2Profile = pXdslInfo->vdsl2Profile;
      if(DS_DIRECTION == dir ) {
         globalVar.adslMib.vdslInfo[pathId].rcvRate = pXdslFramingParam->dataRate;
         pDelayInp = &globalVar.adslMib.vdslInfo[pathId].rcv2DelayInp;
         pVdslFramingParam = &globalVar.adslMib.vdslInfo[pathId].rcv2Info;
      }
      else {
         globalVar.adslMib.vdslInfo[pathId].xmtRate = pXdslFramingParam->dataRate;
         pDelayInp = &globalVar.adslMib.vdslInfo[pathId].xmt2DelayInp;
         pVdslFramingParam = &globalVar.adslMib.vdslInfo[pathId].xmt2Info;
      }
      pDelayInp->delay = pXdslFramingParam->delay;
      pDelayInp->inp = pXdslFramingParam->INP;
      Xdsl2MibConvertVdslFramerParam(pVdslFramingParam, pXdslFramingParam);
      return;
   }
   else
#endif
   {   /* ADSL */
      globalVar.adslMib.adsl2Info2lp[0].adsl2Mode = pXdslInfo->xdslMode;
      globalVar.adslMib.adsl2Info2lp[0].pwrState = pXdslInfo->pwrState;
      if(DS_DIRECTION == dir ) {
         globalVar.adslMib.adsl2Info2lp[pathId].rcvRate = pXdslFramingParam->dataRate;
         pDelayInp = &globalVar.adslMib.adsl2Info2lp[pathId].rcv2DelayInp;
         pAdsl2FramingParam = &globalVar.adslMib.adsl2Info2lp[pathId].rcv2Info;
         pAdslFramingParam = &globalVar.adslMib.xdslConnection[pathId].rcvInfo;
         pAdsl2ChanInfo = &globalVar.adslMib.adsl2Info2lp[pathId].rcvChanInfo;
      }
      else {
         globalVar.adslMib.adsl2Info2lp[pathId].xmtRate = pXdslFramingParam->dataRate;
         pDelayInp = &globalVar.adslMib.adsl2Info2lp[pathId].xmt2DelayInp;
         pAdsl2FramingParam = &globalVar.adslMib.adsl2Info2lp[pathId].xmt2Info;
         pAdslFramingParam = &globalVar.adslMib.xdslConnection[pathId].xmtInfo;
         pAdsl2ChanInfo = &globalVar.adslMib.adsl2Info2lp[pathId].xmtChanInfo;
      }
   }
   
   if(0 == pDirFramingInfo->NlpData)
      newStat = false;
   
   if(newStat) {
      /* Old delay/inp were sent and stored in Q8; new delay is sent from PHY as an integer and INP in Q1 */
      pDelayInp->delay = pXdslFramingParam->delay << 8;
      pDelayInp->inp = pXdslFramingParam->INP << 7;
      pAdsl2ChanInfo->ahifChanId = pXdslFramingParam->ahifChanId[0];
      pAdsl2ChanInfo->connectionType = pXdslFramingParam->tmType[0];
      XdslMibConvertToAdslFramerParam(gDslVars, dir, pAdslFramingParam, pAdsl2FramingParam, pDirFramingInfo, pXdslFramingParam);
   }
   else {
      /* For old framing parameters status */
      pXdslFramingParam->ahifChanId[0] = pAdsl2ChanInfo->ahifChanId;
      pXdslFramingParam->tmType[0] = pAdsl2ChanInfo->connectionType;
      XdslMibConvertFromAdslFramerParam(gDslVars, pAdslFramingParam, pAdsl2FramingParam, pDirFramingInfo, pXdslFramingParam);

      if(US_DIRECTION == dir) {
         /* PHY doesn't send delay/inp for US direction */
         /* Calculate and store in Q8, the same format as sent by PHY through kG992RcvDelay/kG992RcvInp */
         /* status for DS direction */
         pDelayInp->delay = (ushort)GetAdsl2Delayq(pXdslFramingParam, (1 << 8));
         pDelayInp->inp = (ushort)GetAdsl2Inpq(pXdslFramingParam, (1 << 8));
      }
      else {
         if(!AdslMibIsAdsl2Mod(gDslVars)) {
            /* ADSL1, check for S = 0.05 */
            if((pAdslFramingParam->K > 256) && (1 == pXdslFramingParam->S.num)) {
               pXdslFramingParam->B[0] = pAdslFramingParam->K/2;
               pXdslFramingParam->R /= 2;
               pXdslFramingParam->S.denom = 2;
            }
         }
         if (!XdslMibIsPhyRActive(gDslVars, DS_DIRECTION)) {
            /* In PhyR mode, PHY already send these values */
            pDelayInp->delay = (ushort)GetAdsl2Delayq(pXdslFramingParam, (1 << 8));
            pDelayInp->inp = (ushort)GetAdsl2Inpq(pXdslFramingParam, (1 << 8));
         }
      }

      /* Unified framing param store delay as integer and INP in Q1 as sent from new PHY */
      pXdslFramingParam->delay = pDelayInp->delay >> 8;
      pXdslFramingParam->INP = pDelayInp->inp >> 7;
   }
}

Private void Xdsl2MibSetConnectionInfo(void *gDslVars, FramerDeframerOptions *param, int dir, int msgLen)
{
   xdslDirFramingInfo   *pDirFramingInfo;
   xdslFramingInfo      *pFramingInfo;
   ulong                       ginpStat;
   adslConnectionInfo    *pXdslInfo;
   adslConnectionStat    *pXdslStat;
   uchar                       pathId = param->path;
   Boolean                    swapPath = (US_DIRECTION == dir) ? globalVar.swapTxPath: globalVar.swapRxPath;
   long                         dataRate;
   
   if(swapPath)
      pathId =param->path ^ 1;

   pXdslInfo = &globalVar.adslMib.xdslConnection[pathId];
   pXdslStat = &globalVar.adslMib.xdslStat[pathId];
   pDirFramingInfo = &globalVar.adslMib.xdslInfo.dirInfo[dir];
   
   if(US_DIRECTION == dir) {
      ginpStat= kGinpUsEnabled;
      if(param->codingType & kAdslTrellisOn)
         pXdslInfo->trellisCoding2 |= kAdsl2TrellisTxEnabled;
      else
         pXdslInfo->trellisCoding2 &= ~kAdsl2TrellisTxEnabled;
      if(param->fireEnabled & 0x2)
         pXdslStat->fireStat.status |= kFireUsEnabled;
   }
   else { /* kDsl993p2FramerDeframerDs */
      ginpStat= kGinpDsEnabled;
      if(param->codingType & kAdslTrellisOn)
         pXdslInfo->trellisCoding2 |= kAdsl2TrellisRxEnabled;
      else
         pXdslInfo->trellisCoding2 &= ~kAdsl2TrellisRxEnabled;
      if(param->fireEnabled & 0x2)
         pXdslStat->fireStat.status |= kFireDsEnabled;
      pXdslInfo->chType = (param->D > 1) ? kAdslIntlChannel: kAdslFastChannel;
   }
   
   pFramingInfo = &pDirFramingInfo->lpInfo[pathId];
   dataRate = pFramingInfo->dataRate;
   AdslMibByteClear(sizeof(xdslFramingInfo), pFramingInfo);
   pFramingInfo->dataRate = dataRate;
   
   pFramingInfo->N = param->N;
   pFramingInfo->B[0] = (unsigned char)param->B;
   pFramingInfo->B[1] = (unsigned char)param->b1;
   pFramingInfo->I = param->I;
   pFramingInfo->G = param->G;
   pFramingInfo->U = param->U;
   pFramingInfo->F = param->F;
   pFramingInfo->ovhType = param->ovhType;
   pFramingInfo->ahifChanId[0] = param->ahifChanId[0];
   pFramingInfo->ahifChanId[1] = param->ahifChanId[1] ;
   pFramingInfo->tmType[0] = param->tmType[0];
   pFramingInfo->tmType[1] = param->tmType[1];
   pFramingInfo->pathId = param->path;
   pFramingInfo->L = param->L;
   pFramingInfo->M = param->M;
   pFramingInfo->T = param->T;
   pFramingInfo->D = param->D;
   pFramingInfo->R = param->R;
   pFramingInfo->delay = param->delay;
   pFramingInfo->INP = param->INP;
   pFramingInfo->S.num = param->S.num;
   pFramingInfo->S.denom = param->S.denom;
   
   if(msgLen >= GINP_FRAMER_STRUCT_SIZE) {
      if((param->path) && (0 != (param->ginpFraming&7))) {
         pXdslStat->ginpStat.status |= ginpStat;
         pFramingInfo->ginpLookBack = param->ginpFraming >> 3;
         pFramingInfo->rtxMode = 17 + (param->ginpFraming&7);
      }
      pFramingInfo->INPrein = param->INPrein;
      pFramingInfo->rrcBits = param->phyRrrcBits;
      pFramingInfo->rxQueue = param->fireRxQueue;
      pFramingInfo->txQueue = param->fireTxQueue;
      pFramingInfo->Q = param->Q;
      pFramingInfo->V = param->V;
      if(msgLen >= GINP_FRAMER_ETR_STRUCT_SIZE)
         pFramingInfo->etrRate = param->ETR_kbps;
      else
         pFramingInfo->etrRate = 0;
      if(msgLen >= GINP_FRAMER_INPSHINE_STRUCT_SIZE)
        pFramingInfo->INP = param->INPshine;
   }
   
   if((0==pXdslStat->ginpStat.status) && (pXdslStat->fireStat.status))
      pFramingInfo->rtxMode = 1;
   
   if( XdslMibIs2lpActive(gDslVars, dir))
      pDirFramingInfo->Nlp = 2;
   else
      pDirFramingInfo->Nlp = 1;
   
   pDirFramingInfo->NlpData = 1;
   pDirFramingInfo->Nbc = 1;
   pDirFramingInfo->MSGlp= 0;
}

Private ulong AdslMibShowtimeSFErrors(ulong *curCnts, ulong *oldCnts)
{
    return (curCnts[kG992ShowtimeSuperFramesRcvdWrong] - oldCnts[kG992ShowtimeSuperFramesRcvdWrong]);
}

Private ulong AdslMibShowtimeRSErrors(ulong *curCnts, ulong *oldCnts)
{
    return (curCnts[kG992ShowtimeRSCodewordsRcvedCorrectable] - oldCnts[kG992ShowtimeRSCodewordsRcvedCorrectable]);
}

Private Boolean AdslMibShowtimeDataError(ulong *curCnts, ulong *oldCnts)
{
    return 
        (curCnts[kG992ShowtimeRSCodewordsRcvedUncorrectable] != 
         oldCnts[kG992ShowtimeRSCodewordsRcvedUncorrectable])   ||
         (AdslMibShowtimeSFErrors(curCnts, oldCnts) != 0);
}

#define AdslMibES(state,cnt)                                        \
do {                                                                \
    if (!state) {                                                   \
        state = true;                                               \
        IncPerfCounterVar(&globalVar.adslMib.adslPerfData, cnt);    \
    }                                                               \
} while (0)

Private void AdslMibConnectionStatUpdate (void *gDslVars, ulong *cntOld, ulong *cntNew, int pathId)
{
    adslConnectionStat *s1, *s1Tx;
    adslConnectionStat *s2, *s2Tx;
    int n;

    s1 = &globalVar.adslMib.xdslStat[pathId];
    s2 = &globalVar.adslMib.xdslStatSincePowerOn[pathId];

    n = cntNew[kG992ShowtimeSuperFramesRcvd] - cntOld[kG992ShowtimeSuperFramesRcvd];
    s1->rcvStat.cntSF += n;
    s2->rcvStat.cntSF += n;

    n = cntNew[kG992ShowtimeSuperFramesRcvdWrong] - cntOld[kG992ShowtimeSuperFramesRcvdWrong];
    s1->rcvStat.cntSFErr += n;
    s2->rcvStat.cntSFErr += n;

    n = cntNew[kG992ShowtimeRSCodewordsRcved] - cntOld[kG992ShowtimeRSCodewordsRcved];
    s1->rcvStat.cntRS += n;
    s2->rcvStat.cntRS += n;

    n = cntNew[kG992ShowtimeRSCodewordsRcvedCorrectable] - cntOld[kG992ShowtimeRSCodewordsRcvedCorrectable];
    s1->rcvStat.cntRSCor += n;
    s2->rcvStat.cntRSCor += n;

    n = cntNew[kG992ShowtimeRSCodewordsRcvedUncorrectable] - cntOld[kG992ShowtimeRSCodewordsRcvedUncorrectable];
    s1->rcvStat.cntRSUncor += n;
    s2->rcvStat.cntRSUncor += n;
    if(!XdslMibIsXdsl2Mod(gDslVars)) {
        /* for ADSL2 and above, these are done in AdslMibUpdateTxStat() */
        s1Tx = &globalVar.adslMib.xdslStat[0];
        s2Tx = &globalVar.adslMib.xdslStatSincePowerOn[0];
        n = cntNew[kG992ShowtimeNumOfFEBE] - cntOld[kG992ShowtimeNumOfFEBE];
        s1Tx->xmtStat.cntSFErr += n;
        s2Tx->xmtStat.cntSFErr += n;
        n = cntNew[kG992ShowtimeNumOfFECC] - cntOld[kG992ShowtimeNumOfFECC];
        s1Tx->xmtStat.cntRSCor += n;
        s2Tx->xmtStat.cntRSCor += n;

        if ((globalVar.adslMib.adslConnection.xmtInfo.R != 0) && globalVar.adslMib.adslConnection.xmtInfo.S)
            s1Tx->xmtStat.cntRS = (s1->xmtStat.cntSF * 68) / globalVar.adslMib.adslConnection.xmtInfo.S;
        else
            s1Tx->xmtStat.cntRS = 0;
    }

#ifdef DSL_REPORT_ALL_COUNTERS
    n = cntNew[kG992ShowtimeNumOfHEC] - cntOld[kG992ShowtimeNumOfHEC];
	IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntHEC, n);

    n = cntNew[kG992ShowtimeNumOfOCD] - cntOld[kG992ShowtimeNumOfOCD];
	IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntOCD, n);

    n = cntNew[kG992ShowtimeNumOfLCD] - cntOld[kG992ShowtimeNumOfLCD];
	IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntLCD, n);

    if (n != 0)
        AdslMibES(globalVar.currSecondLCD, adslLCDS);
    if(!XdslMibIsXdsl2Mod(gDslVars)) {
        /* for ADSL2 and above, these are done in AdslMibUpdateTxStat() */
        n = cntNew[kG992ShowtimeNumOfFHEC] - cntOld[kG992ShowtimeNumOfFHEC];
		IncAtmXmtCounterVar(&globalVar.adslMib, 0, cntHEC, n);
    }
    n = cntNew[kG992ShowtimeNumOfFOCD] - cntOld[kG992ShowtimeNumOfFOCD];
	IncAtmXmtCounterVar(&globalVar.adslMib, 0, cntOCD, n);

    n = cntNew[kG992ShowtimeNumOfFLCD] - cntOld[kG992ShowtimeNumOfFLCD];
	IncAtmXmtCounterVar(&globalVar.adslMib, 0, cntLCD, n);
#endif
}

Private void AdslMibUpdateShowtimeErrors(void *gDslVars, ulong nErr, int pathId)
{

    int Limit = 18;
    AdslMibES(globalVar.currSecondErrored, adslESs);

    if(XdslMibIsXdsl2Mod(gDslVars) && (adslCorePhyDesc.mjVerNum > 6) &&
        !XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
        if(globalVar.PERpDS[pathId] < (15 << 8))
            Limit=((18*15) << 8)/globalVar.PERpDS[pathId];
        else if(globalVar.PERpDS[pathId] > (20 << 8))
            Limit=((18*20) << 8)/globalVar.PERpDS[pathId];
    }
    if (nErr > Limit)
        AdslMibES(globalVar.currSecondSES, adslSES);
}

Private void AdslMibUpdateLOM(void *gDslVars)
{
    AdslMibES(globalVar.currSecondLOM, adslLOMS);
}

Private void AdslMibUpdateLOS(void *gDslVars)
{
    AdslMibES(globalVar.currSecondErrored, adslESs);
    AdslMibES(globalVar.currSecondLOS, adslLOSS);
    AdslMibES(globalVar.currSecondSES, adslSES);
}

Private void AdslMibUpdateLOF(void *gDslVars)
{
    AdslMibES(globalVar.currSecondErrored, adslESs);
    AdslMibES(globalVar.currSecondSES, adslSES);
}

Private void AdslMibUpdateShowtimeRSErrors(void *gDslVars, ulong nErr)
{
    AdslMibES(globalVar.currSecondFEC, adslFECs);
}

Private void DiffTxPerfData(
	adslPerfCounters *pPerf0, 
	adslPerfCounters *pPerf1, 
	adslPerfCounters *pPerfRes)
{
	ulong n;
	n=pPerf0->adslESs - pPerf1->adslESs;
	pPerfRes->adslESs  = n<=0x1000000?n:0;
	n=pPerf0->adslSES - pPerf1->adslSES;
	pPerfRes->adslSES  = n<=0x1000000?n:0;
	n= pPerf0->adslFECs- pPerf1->adslFECs;
	pPerfRes->adslFECs = n<=0x1000000?n:0;
	n= pPerf0->adslLOSS- pPerf1->adslLOSS;
	pPerfRes->adslLOSS = n<=0x1000000?n:0;
	n=pPerf0->adslUAS - pPerf1->adslUAS;
	pPerfRes->adslUAS  = n<=0x1000000?n:0;
}


Private void AddTxPerfData(
	adslPerfCounters *pPerf0, 
	adslPerfCounters *pPerf1, 
	adslPerfCounters *pPerfRes)
{
	pPerfRes->adslESs  = pPerf0->adslESs + pPerf1->adslESs;
	pPerfRes->adslSES  = pPerf0->adslSES + pPerf1->adslSES;
	pPerfRes->adslFECs = pPerf0->adslFECs+ pPerf1->adslFECs;
	pPerfRes->adslLOSS = pPerf0->adslLOSS+ pPerf1->adslLOSS;
	pPerfRes->adslUAS  = pPerf0->adslUAS + pPerf1->adslUAS;
}

Private void AdslMibUpdateIntervalCounters (void *gDslVars, ulong sec)
{
	adslMibInfo		*pMib = &globalVar.adslMib;
    long    secElapsed, i, n;

    secElapsed  = sec + globalVar.adslMib.adslPerfData.adslPerfCurr15MinTimeElapsed;
    if (secElapsed >= k15MinInSeconds) {
        n = globalVar.adslMib.adslPerfData.adslPerfValidIntervals;
        if (n < kAdslMibPerfIntervals)
            n++;
        for (i = (n-1); i > 0; i--)
            AdslMibByteMove(
                sizeof(adslPerfCounters), 
                &globalVar.adslMib.adslPerfIntervals[i-1], 
                &globalVar.adslMib.adslPerfIntervals[i]);
        AdslMibByteMove(
            sizeof(adslPerfCounters), 
            &globalVar.adslMib.adslPerfData.perfCurr15Min, 
            &globalVar.adslMib.adslPerfIntervals[0]);
        BlockByteClear(sizeof(adslPerfCounters), (void*)&globalVar.adslMib.adslPerfData.perfCurr15Min);
        globalVar.adslMib.adslPerfData.adslPerfValidIntervals = n;
		/* ATM/failure Cur15 */
        BlockByteClear(sizeof(globalVar.adslMib.atmStatCur15Min2lp), (void*)&globalVar.adslMib.atmStatCur15Min2lp);
        BlockByteClear(sizeof(adslFailureCounters), (void*)&globalVar.adslMib.adslPerfData.failCur15Min);
		/* TX Cur15 */
		BlockByteMove(sizeof(adslPerfCounters), (void*)&pMib->adslTxPerfCur15Min, (void*)&pMib->adslTxPerfLast15Min);
		BlockByteClear(sizeof(adslPerfCounters), (void*)&pMib->adslTxPerfCur15Min);

        n = globalVar.adslMib.adslChanIntlPerfData.adslChanPerfValidIntervals;
        if (n < kAdslMibChanPerfIntervals)
            n++;
        for (i = (n-1); i > 0; i--) {
            AdslMibByteMove(
                sizeof(adslChanCounters),
                &globalVar.adslMib.adslChanIntlPerfIntervals[i-1], 
                &globalVar.adslMib.adslChanIntlPerfIntervals[i]);
            AdslMibByteMove(
                sizeof(adslChanCounters),
                &globalVar.adslMib.adslChanFastPerfIntervals[i-1], 
                &globalVar.adslMib.adslChanFastPerfIntervals[i]);
        }
        AdslMibByteMove(
            sizeof(adslChanCounters),
            &globalVar.adslMib.adslChanIntlPerfData.perfCurr15Min, 
            &globalVar.adslMib.adslChanIntlPerfIntervals[0]);
        AdslMibByteMove(
            sizeof(adslChanCounters),
            &globalVar.adslMib.adslChanFastPerfData.perfCurr15Min, 
            &globalVar.adslMib.adslChanFastPerfIntervals[0]);

        BlockByteClear(sizeof(adslChanCounters), (void*)&globalVar.adslMib.adslChanIntlPerfData.perfCurr15Min);
        BlockByteClear(sizeof(adslChanCounters), (void*)&globalVar.adslMib.adslChanFastPerfData.perfCurr15Min);
        globalVar.adslMib.adslChanIntlPerfData.adslChanPerfValidIntervals = n;
        globalVar.adslMib.adslChanFastPerfData.adslChanPerfValidIntervals = n;

        secElapsed -= k15MinInSeconds;
    }
    globalVar.adslMib.adslPerfData.adslPerfCurr15MinTimeElapsed = secElapsed;
    globalVar.adslMib.adslChanFastPerfData.adslPerfCurr15MinTimeElapsed = secElapsed;
    globalVar.adslMib.adslChanIntlPerfData.adslPerfCurr15MinTimeElapsed = secElapsed;

    secElapsed = sec + globalVar.adslMib.adslPerfData.adslPerfCurr1DayTimeElapsed;
    if (secElapsed >= k1DayInSeconds) {
        AdslMibByteMove(
            sizeof(adslPerfCounters), 
            &globalVar.adslMib.adslPerfData.perfCurr1Day, 
            &globalVar.adslMib.adslPerfData.perfPrev1Day);
        BlockByteClear(sizeof(adslPerfCounters), (void*)&globalVar.adslMib.adslPerfData.perfCurr1Day);
        AdslMibByteMove(
            sizeof(adslFailureCounters), 
            &globalVar.adslMib.adslPerfData.failCurDay, 
            &globalVar.adslMib.adslPerfData.failPrevDay);
        BlockByteClear(sizeof(adslFailureCounters), (void*)&globalVar.adslMib.adslPerfData.failCurDay);
		/* ATM 1Day */
        AdslMibByteMove(
            sizeof(globalVar.adslMib.atmStatCurDay2lp), 
            &globalVar.adslMib.atmStatCurDay2lp, 
            &globalVar.adslMib.atmStatPrevDay2lp);
        BlockByteClear(sizeof(globalVar.adslMib.atmStatCurDay2lp), (void*)&globalVar.adslMib.atmStatCurDay2lp);
		/* TX 1Day */
		BlockByteMove(sizeof(adslPerfCounters), (void*)&pMib->adslTxPerfCur1Day, (void*)&pMib->adslTxPerfLast1Day);
		BlockByteClear(sizeof(adslPerfCounters), (void*)&pMib->adslTxPerfCur1Day);

        AdslMibByteMove(
            sizeof(adslChanCounters),
            &globalVar.adslMib.adslChanIntlPerfData.perfCurr1Day, 
            &globalVar.adslMib.adslChanIntlPerfData.perfPrev1Day);
        AdslMibByteMove(
            sizeof(adslChanCounters),
            &globalVar.adslMib.adslChanFastPerfData.perfCurr1Day,
            &globalVar.adslMib.adslChanFastPerfData.perfPrev1Day);
        BlockByteClear(sizeof(adslChanCounters), (void*)&globalVar.adslMib.adslChanIntlPerfData.perfCurr1Day);
        BlockByteClear(sizeof(adslChanCounters), (void*)&globalVar.adslMib.adslChanFastPerfData.perfCurr1Day);

        globalVar.adslMib.adslPerfData.adslAturPerfPrev1DayMoniSecs = k1DayInSeconds;
        secElapsed -= k1DayInSeconds;
    }
    globalVar.adslMib.adslPerfData.adslPerfCurr1DayTimeElapsed = secElapsed;
    globalVar.adslMib.adslChanFastPerfData.adslPerfCurr1DayTimeElapsed = secElapsed;
    globalVar.adslMib.adslChanIntlPerfData.adslPerfCurr1DayTimeElapsed = secElapsed;
  secElapsedInDay=secElapsed;
}


Public void AdslMibClearData(void *gDslVars)
{
    BlockByteClear (sizeof(adslMibVarsStruct), (void*)&globalVar);

    globalVar.notifyHandlerPtr = AdslMibNotifyIdle;

    globalVar.adslMib.adslLine.adslLineCoding = kAdslLineCodingDMT;
    globalVar.adslMib.adslLine.adslLineType  = kAdslLineTypeFastOrIntl;

    globalVar.adslMib.adslPhys.adslCurrOutputPwr = 130;
    globalVar.adslMib.adslChanIntlAtmPhyData.atmInterfaceOCDEvents = 0;
    globalVar.adslMib.adslChanIntlAtmPhyData.atmInterfaceTCAlarmState = kAtmPhyStateLcdFailure;
    globalVar.adslMib.adslChanFastAtmPhyData.atmInterfaceOCDEvents = 0;
    globalVar.adslMib.adslChanFastAtmPhyData.atmInterfaceTCAlarmState = kAtmPhyStateLcdFailure;
    globalVar.adslMib.adslFramingMode = 3;
    globalVar.showtimeMarginThld = -1;
}
Public void AdslMibResetConectionStatCounters(void *gDslVars)
{
#ifdef CONFIG_VDSL_SUPPORTED
    adslBondingStat bondingStat;
#endif
    ulong n = globalVar.adslMib.xdslStat[0].fireStat.status;
    ulong n0 = globalVar.adslMib.xdslStat[0].ginpStat.status;
    ulong n1 = globalVar.adslMib.xdslStat[MAX_LP_NUM-1].ginpStat.status;
#ifdef CONFIG_VDSL_SUPPORTED
    BlockByteMove(sizeof(adslBondingStat), (uchar *)&globalVar.adslMib.xdslStat[0].bondingStat, (uchar *)&bondingStat);  /* No dual latency in bonding */
#endif
    BlockByteClear (sizeof(globalVar.adslMib.xdslStat), (void *) &globalVar.adslMib.xdslStat);
    
    globalVar.adslMib.xdslStat[0].fireStat.status=n;
    globalVar.adslMib.xdslStat[0].ginpStat.status=n0;
    globalVar.adslMib.xdslStat[MAX_LP_NUM-1].ginpStat.status=n1;
#ifdef CONFIG_VDSL_SUPPORTED
    BlockByteMove(sizeof(adslBondingStat), (uchar *)&bondingStat, (uchar *)&globalVar.adslMib.xdslStat[0].bondingStat);
#endif
    BlockByteClear (sizeof(globalVar.adslMib.atmStat2lp), (void *) &globalVar.adslMib.atmStat2lp);
    BlockByteClear (sizeof(globalVar.adslMib.adslPerfData), (void *) &globalVar.adslMib.adslPerfData);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfTotal), (void *) &globalVar.adslMib.adslTxPerfTotal);
    BlockByteClear (sizeof(globalVar.adslMib.adslChanIntlPerfData), (void *) &globalVar.adslMib.adslChanIntlPerfData);
    BlockByteClear (sizeof(globalVar.adslMib.adslChanFastPerfData), (void *) &globalVar.adslMib.adslChanFastPerfData);
    BlockByteClear (sizeof(globalVar.adslMib.adslPerfIntervals), (void *) &globalVar.adslMib.adslPerfIntervals);
    BlockByteClear (sizeof(globalVar.adslMib.xdslChanPerfIntervals), (void *) &globalVar.adslMib.xdslChanPerfIntervals);
    BlockByteClear (sizeof(globalVar.adslMib.xdslStatSincePowerOn), (void *) &globalVar.adslMib.xdslStatSincePowerOn);
    BlockByteClear (sizeof(globalVar.adslMib.atmStatSincePowerOn2lp), (void *) &globalVar.adslMib.atmStatSincePowerOn2lp);
    BlockByteClear (sizeof(globalVar.adslMib.atmStatCurDay2lp), (void *) &globalVar.adslMib.atmStatCurDay2lp);
    BlockByteClear (sizeof(globalVar.adslMib.atmStatPrevDay2lp), (void *) &globalVar.adslMib.atmStatPrevDay2lp);
    BlockByteClear (sizeof(globalVar.adslMib.atmStatCur15Min2lp), (void *) &globalVar.adslMib.atmStatCur15Min2lp);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfTotal), (void *) &globalVar.adslMib.adslTxPerfTotal);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfCur15Min), (void *) &globalVar.adslMib.adslTxPerfCur15Min);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfCur1Day), (void *) &globalVar.adslMib.adslTxPerfCur1Day);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfLast15Min), (void *) &globalVar.adslMib.adslTxPerfLast15Min);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfLast1Day), (void *) &globalVar.adslMib.adslTxPerfLast1Day);
    BlockByteClear (sizeof(globalVar.adslMib.adslTxPerfSinceShowTime), (void *) &globalVar.adslMib.adslTxPerfSinceShowTime);
    globalVar.txShowtimeTime=0;
    globalVar.secElapsedShTm=0;
}

#define kAdslConnected  (kAdslXmtActive | kAdslRcvActive) 

Public Boolean AdslMibIsLinkActive(void *gDslVars)
{
    return (kAdslConnected == (globalVar.linkStatus & kAdslConnected));
}

#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
Public Boolean AdslMibGetErrorSampleStatus(void *gDslVars)
{
  return (globalVar.adslMib.vectData.esCtrl.writeIdx != globalVar.adslMib.vectData.esCtrl.readIdx);
}
#endif
Public int AdslMibTrainingState (void *gDslVars)
{
    return (globalVar.adslMib.adslTrainingState);
}

Public int AdslMibPowerState(void *gDslVars)
{
    return (globalVar.adslMib.xdslInfo.pwrState);
#if 0
    return (globalVar.adslMib.adsl2Info.pwrState);
#endif
}

Public void AdslMibSetNotifyHandler(void *gDslVars, adslMibNotifyHandlerType notifyHandlerPtr)
{
    globalVar.notifyHandlerPtr = notifyHandlerPtr ? notifyHandlerPtr : AdslMibNotifyIdle;
}

Private void AdslMibNotify(void *gDslVars, int event)
{
    globalVar.notifyHandlerPtr (gDslVars, event);
}

Public Boolean XdslMibIsPhyRActive(void *gDslVars, unsigned char dir)
{
    Boolean res = false;
#ifdef CONFIG_VDSL_SUPPORTED
    if(XdslMibIsVdsl2Mod(gDslVars)) {
        if(US_DIRECTION == dir)
            res = (globalVar.adslMib.xdslStat[0].fireStat.status & kFireUsEnabled)? true: false;
        else if(DS_DIRECTION == dir)
            res = (globalVar.adslMib.xdslStat[0].fireStat.status & kFireDsEnabled)? true: false;
    }
    else
#endif
    if(DS_DIRECTION == dir)   /* No US PhyR for ADSL2/2+ */
        res = (globalVar.adslMib.xdslStat[0].fireStat.status & kFireDsEnabled)? true:false;
#if 0
    __SoftDslPrintf(gDslVars, "%s: dir = %s, res = %d", 0, __FUNCTION__, (US_DIRECTION == dir)? "Tx": "Rx", res);
#endif
    return res;
}

Public Boolean XdslMibIsDataCarryingPath(void *gDslVars, unsigned char pathId, unsigned char dir)
{
    unsigned char ahifChanId = globalVar.adslMib.xdslInfo.dirInfo[dir].lpInfo[pathId].ahifChanId[0];
    return ((unsigned char)-1 != ahifChanId)? 1:0;
}

Public Boolean XdslMibIsGinpActive(void *gDslVars, unsigned char dir)
{
    Boolean res = false;
#ifdef CONFIG_VDSL_SUPPORTED
    if(XdslMibIsVdsl2Mod(gDslVars)) {
        if(US_DIRECTION == dir)
            res = (globalVar.adslMib.xdslStat[0].ginpStat.status & kGinpUsEnabled)? true: false;
        else if(DS_DIRECTION == dir)
            res = (globalVar.adslMib.xdslStat[0].ginpStat.status & kGinpDsEnabled)? true: false;
    }
    else
#endif
    if(DS_DIRECTION == dir) /* No US G.inp for ADSL2/2+ */
        res = (globalVar.adslMib.xdslStat[0].ginpStat.status & kGinpDsEnabled)? true:false;
#if 0
    __SoftDslPrintf(gDslVars, "%s: dir = %s, res = %d", 0, __FUNCTION__, (US_DIRECTION == dir)? "Tx": "Rx", res);
#endif
    return res;
}

Public Boolean XdslMibIs2lpActive(void *gDslVars, unsigned char dir)
{
    Boolean res = false;

    if(DS_DIRECTION == dir)
        res = globalVar.adslMib.lp2Active;
    else if(US_DIRECTION == dir)
        res = globalVar.adslMib.lp2TxActive;
    
    return res;
}

Public Boolean  AdslMibInit(void *gDslVars, dslCommandHandlerType commandHandler)
{
    int i;
    
    AdslMibClearData(gDslVars);
    globalVar.nTones = kAdslMibToneNum;
    globalVar.cmdHandlerPtr = commandHandler;
#ifdef G992_ANNEXC
    globalVar.nTones = kAdslMibToneNum*2;
#endif
    AdslMibSetNotifyHandler(gDslVars, NULL);
    globalVar.xdslInitIdleStateTime = 0;
    globalVar.txUpdateStatFlag = 0;
    globalVar.txCntReceived = 0;
    globalVar.secElapsedShTm = 0;
    globalVar.pathId = 0;
    globalVar.pathActive = 0;
    globalVar.swapRxPath = false;
    globalVar.swapTxPath = false;
    globalVar.minEFTRReported = false;
    globalVar.adslMib.lp2Active = false;
    globalVar.adslMib.lp2TxActive = false;
    globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins=kDslNonLinearityDefaultThreshold;
    globalVar.adslMib.adslNonLinData.NonLinNumAffectedBins=-1;
    globalVar.adslMib.adslTrainingState = kAdslTrainingIdle;
    globalVar.adslMib.xdslInitializationCause = kXdslInitConfigSuccess;
    globalVar.lastInitState = kXdslLastInitStateStart;
    
    for(i=0; i<MAX_LP_NUM; i++) {
        globalVar.per2lp[i]=1;
        globalVar.PERpDS[i]=1;
    }
    
#ifdef CONFIG_VDSL_SUPPORTED
    globalVar.waitBandPlan=0;
    globalVar.waitfirstQLNstatusLD=0;
    globalVar.waitfirstHLOGstatusLD=0;
    globalVar.waitfirstSNRstatusLD=0;
    globalVar.waitfirstSNRstatusMedley=0;
    globalVar.bandPlanType=0;

    globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds =
    globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus =
    globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds =
    globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus = 1;
#endif

    globalVar.snr = &gSnr[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.showtimeMargin = &gShowtimeMargin[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.bitAlloc = &gBitAlloc[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.gain = &gGain[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.chanCharLin = &gChanCharLin[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.chanCharLog = &gChanCharLog[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
    globalVar.quietLineNoise = &gQuietLineNoise[0] + gLineId(gDslVars) * (MAX_TONE_NUM/2);
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
    globalVar.adslMib.vectSM.state = VECT_DISABLED;
#endif

    return true;
}

Public int PHYSupportedNTones(void *gDslVars)
{
#ifdef  G993
  if ( (ADSL_PHY_SUPPORT(kAdslPhyVdslG993p2)) && (BcmAdslCoreGetConfiguredMod(gLineId(gDslVars)) & kG993p2AnnexA) ) {
    if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
      return MAX_TONE_NUM/2;
    else
      return  kVdslMibMaxToneNum;
  }
#endif
  if (ADSL_PHY_SUPPORT(kAdslPhyAdsl2p) || ADSL_PHY_SUPPORT(kAdslPhyAdsl2))
    return  kAdslMibToneNum*2;
  return kAdslMibToneNum;
}

Public void AdslMibTimer(void *gDslVars, long timeMs)
{
    long    sec;
    int path0, path1;
    path0 = XdslMibGetPath0MappedPathId(gDslVars, US_DIRECTION);
    path1 = XdslMibGetPath1MappedPathId(gDslVars, US_DIRECTION);

    globalVar.txShowtimeTime+=timeMs;
    if(!XdslMibIsGinpActive(gDslVars, US_DIRECTION))
        globalVar.adslMib.xdslStatSincePowerOn[path0].xmtStat.cntSF +=  (timeMs << 8)/globalVar.per2lp[path0];
    else if(!XdslMibIsDataCarryingPath(gDslVars, path0, US_DIRECTION)) /* OHM Channel */
        globalVar.adslMib.xdslStatSincePowerOn[path0].xmtStat.cntSF +=  (timeMs << 8)/globalVar.per2lp[path0];
    if(globalVar.txShowtimeTime>3000 && globalVar.txUpdateStatFlag==1 )
    {
        sec = globalVar.txShowtimeTime/1000;
        if(sec > globalVar.secElapsedShTm+1)
        {
            globalVar.secElapsedShTm=sec;
            globalVar.adslMib.adslPerfData.adslSinceLinkTimeElapsed = sec;
            if(XdslMibIsXdsl2Mod(gDslVars))
            {
                globalVar.adslMib.xdslStat[path0].xmtStat.cntRS=globalVar.txShowtimeTime*globalVar.rsRate2lp[path0]/1000;
                if(!XdslMibIsGinpActive(gDslVars, US_DIRECTION))
                    globalVar.adslMib.xdslStat[path0].xmtStat.cntSF = (globalVar.txShowtimeTime << 8)/globalVar.per2lp[path0];
                else if (!XdslMibIsDataCarryingPath(gDslVars, path0, US_DIRECTION)) /* OHM Channel */
                    globalVar.adslMib.xdslStat[path0].xmtStat.cntSF = (globalVar.txShowtimeTime << 8)/globalVar.per2lp[path0];
                
                if(globalVar.adslMib.lp2TxActive)
                {
                    globalVar.adslMib.xdslStat[path1].xmtStat.cntRS=globalVar.txShowtimeTime*globalVar.rsRate2lp[path1]/1000;
                    if(!XdslMibIsGinpActive(gDslVars, US_DIRECTION))
                        globalVar.adslMib.xdslStat[path1].xmtStat.cntSF= (globalVar.txShowtimeTime << 8)/globalVar.per2lp[path1];
                    else if (!XdslMibIsDataCarryingPath(gDslVars, path1, US_DIRECTION)) /* OHM Channel */
                        globalVar.adslMib.xdslStat[path1].xmtStat.cntSF= (globalVar.txShowtimeTime << 8)/globalVar.per2lp[path1];
                }
            }
            else
            {
                globalVar.adslMib.adslStat.xmtStat.cntSF=globalVar.txShowtimeTime/17;
            }
        }
    }

    timeMs += globalVar.timeMs;
    sec = timeMs / 1000;
    globalVar.timeSec += sec;
    globalVar.timeMs  = timeMs - sec * 1000;

    if (sec != 0) {
        globalVar.currSecondErrored = false;
        globalVar.currSecondLOS     = false;
        globalVar.currSecondSES     = false;
        globalVar.currSecondFEC     = false;
        globalVar.currSecondLCD     = false;
        globalVar.currSecondLOM     = false;
        if (AdslMibIsLinkActive(gDslVars)) {
            if(globalVar.adslMib.adslPhys.adslCurrStatus & kAdslPhysStatusLOM)
                AdslMibUpdateLOM(gDslVars);
            if (globalVar.adslMib.adslPhys.adslCurrStatus & kAdslPhysStatusLOS) {
                AdslMibUpdateLOS(gDslVars);
            }
            IncPerfCounterVar(&globalVar.adslMib.adslPerfData, adslAS);
        }
        else {
            int i;  /* When link is down, the frequency of status arrival sometimes more than 1 sec */
            for(i = 0; i < sec; i++)
                IncPerfCounterVar(&globalVar.adslMib.adslPerfData, adslUAS);
            globalVar.adslMib.adslTxPerfTotal.adslUAS += sec;
            globalVar.adslMib.adslTxPerfCur15Min.adslUAS += sec;
            globalVar.adslMib.adslTxPerfCur1Day.adslUAS += sec;
        }

        AdslMibUpdateIntervalCounters (gDslVars, sec);
    }

    if(kAdslTrainingIdle == globalVar.adslMib.adslTrainingState) {
        if(0 == globalVar.xdslInitIdleStateTime)
            globalVar.xdslInitIdleStateTime = globalVar.txShowtimeTime;
        else if((globalVar.txShowtimeTime - globalVar.xdslInitIdleStateTime) > NO_PEER_DETECT_TIMEOUT)
            globalVar.adslMib.xdslInitializationCause = kXdslInitConfigNoXTUDetected;
    }
    else
        globalVar.xdslInitIdleStateTime = 0;

    if( (0 != globalVar.bitSwapReqTime[DS_DIRECTION]) &&
        ((globalVar.txShowtimeTime - globalVar.bitSwapReqTime[DS_DIRECTION]) > BITSWAP_REQ_TIMEOUT) )
        globalVar.bitSwapReqTime[DS_DIRECTION] = 0;
    
    if( (0 != globalVar.bitSwapReqTime[US_DIRECTION]) &&
        ((globalVar.txShowtimeTime - globalVar.bitSwapReqTime[US_DIRECTION]) > BITSWAP_REQ_TIMEOUT) )
        globalVar.bitSwapReqTime[US_DIRECTION] = 0;
}

Private void AdslMibSetBitAndGain(void *gDslVars, int tblIdx, void *buf, int bufBgSize, Boolean bShared)
{
    int     i;
    uchar   *bufPtr = buf;
    short valGi;
    short sub=UtilQ0LinearToQ4dB(512);

    if ((tblIdx + bufBgSize) > kAdslMibMaxToneNum)
        bufBgSize = kAdslMibMaxToneNum - tblIdx;

    for (i = 0; i < bufBgSize; i++) {
        if (!bShared || (0 == (globalVar.bitAlloc[tblIdx + i] | globalVar.gain[tblIdx + i]))) {
            globalVar.bitAlloc[tblIdx + i] = bufPtr[i << 1] & 0xF;
            valGi=(bufPtr[i << 1] >> 4) | ((ulong)bufPtr[(i << 1) + 1] << 4);
            globalVar.gain[tblIdx + i] =(short)(2*(UtilQ0LinearToQ4dB(valGi)-sub));
        }
    }
}

Private void AdslMibSetGain(void *gDslVars, int tblIdx, void *buf, int bufBgSize, Boolean bShared)
{
    int     i;
    short   *bufPtr = buf;
    if ((tblIdx + bufBgSize) > kAdslMibMaxToneNum)
        bufBgSize = kAdslMibMaxToneNum - tblIdx;
    for (i = 0; i < bufBgSize; i++) {
        if (!bShared || (0 ==  globalVar.gain[tblIdx + i])) 
            globalVar.gain[tblIdx + i] = bufPtr[i]>>3;
    }
}

#define AdslMibParseBitGain(p,inx,b,g)  do {        \
    inx = p->ind;                                   \
    b   = p->gb & 0xF;                              \
    g   = ((long) p->gain << 4) | (p->gb >> 4);     \
} while (0)

Private void *AdslMibUpdateBitGain(void *gDslVars, void *pOlrEntry, Boolean bAdsl2)
{
    int     index, gain;
    uchar   bits;

    if (bAdsl2) {
        dslOLRCarrParam2p   *p = pOlrEntry;
        AdslMibParseBitGain(p, index, bits, gain);
        pOlrEntry = p + 1;
    }
    else {
        dslOLRCarrParam     *p = pOlrEntry;
        AdslMibParseBitGain(p, index, bits, gain);
        pOlrEntry = p + 1;
    }

    globalVar.bitAlloc[index] = bits;
    globalVar.gain[index]     = gain;

    return pOlrEntry;
}


Private void AdslMibSetBitAllocation(void *gDslVars, int tblIdx, void *buf, int bufSize, Boolean bShared)
{
    int     i;
    uchar   *bufPtr = buf;


    if ((tblIdx + bufSize) > kAdslMibMaxToneNum)
        bufSize = kAdslMibMaxToneNum - tblIdx;

    for (i = 0; i < bufSize; i++) {
        if (!bShared || (0 == (globalVar.bitAlloc[tblIdx + i]))) {
            if(bufPtr[i]!=0xFF)
                globalVar.bitAlloc[tblIdx + i] = bufPtr[i];
            else
                globalVar.bitAlloc[tblIdx + i] = 0;
        }
    }
}

Private void AdslMibSetModulationType(void *gDslVars, int mod)
{
    globalVar.adslMib.adslConnection.modType = 
        (globalVar.adslMib.adslConnection.modType & ~kAdslModMask) | mod;
}

Private int AdslMibGetAdsl2Mode(void *gDslVars, ulong mask)
{
    return globalVar.adslMib.xdslInfo.xdslMode & mask;
#if 0
    return globalVar.adslMib.adsl2Info.adsl2Mode & mask;
#endif
}

Private void AdslMibSetAdsl2Mode(void *gDslVars, ulong mask, int mod)
{
    globalVar.adslMib.xdslInfo.xdslMode = (globalVar.adslMib.xdslInfo.xdslMode & ~mask) | mod;
#if 0
    globalVar.adslMib.adsl2Info.adsl2Mode = 
        (globalVar.adslMib.adsl2Info.adsl2Mode & ~mask) | mod;
#endif
}

#ifdef CONFIG_VDSL_SUPPORTED
Private void XdslMibSetXdslMode(void *gDslVars, ulong mask, int mod)
{
    globalVar.adslMib.xdslInfo.xdslMode = (globalVar.adslMib.xdslInfo.xdslMode & ~mask) | mod;
#if 0
    globalVar.adslMib.vdslInfo[0].vdsl2Mode =
        (globalVar.adslMib.vdslInfo[0].vdsl2Mode & ~mask) | mod;
#endif
}

#define XdslMibSetAnnexType(gv,mod)    XdslMibSetXdslMode(gv,kXdslModeAnnexMask,mod)
#endif

#define AdslMibGetAnnexMType(gv)        AdslMibGetAdsl2Mode(gv,kAdsl2ModeAnnexMask)
#define AdslMibSetAnnexMType(gv,mod)    AdslMibSetAdsl2Mode(gv,kAdsl2ModeAnnexMask,mod)

#define AdslMibGetAnnexLUpType(gv)      AdslMibGetAdsl2Mode(gv,kAdsl2ModeAnnexLUpMask)
#define AdslMibSetAnnexLUpType(gv,mod)  AdslMibSetAdsl2Mode(gv,kAdsl2ModeAnnexLUpMask,mod)
#define AdslMibGetAnnexLDnType(gv)      AdslMibGetAdsl2Mode(gv,kAdsl2ModeAnnexLDnMask)
#define AdslMibSetAnnexLDnType(gv,mod)  AdslMibSetAdsl2Mode(gv,kAdsl2ModeAnnexLDnMask,mod)

Private Boolean AdslMibTone32_64(void *gDslVars)
{
#ifdef G992P1_ANNEX_B
    if (ADSL_PHY_SUPPORT(kAdslPhyAnnexB))
        return true;
#endif
    if (AdslMibGetAnnexMType(gDslVars) != 0)
        return true;

    return false;
}

#if 0   /* Not in used */
Private int AdslMibGetBitmapMode(void *gDslVars)
{
    return globalVar.adslMib.adslConnection.modType & kAdslBitmapMask;
}

Private void AdslMibSetBitmapMode(void *gDslVars, int mod)
{
    globalVar.adslMib.adslConnection.modType = 
        (globalVar.adslMib.adslConnection.modType & ~kAdslBitmapMask) | mod;
}

Private int AdslMibGetUpstreamMode(void *gDslVars)
{
    return globalVar.adslMib.adslConnection.modType & kAdslUpstreamModeMask;
}

Private void AdslMibSetUpstreamMode(void *gDslVars, int mod)
{
    globalVar.adslMib.adslConnection.modType = 
        (globalVar.adslMib.adslConnection.modType & ~kAdslUpstreamModeMask) | mod;
}
#endif

/********* ADSL bandplans *******
Modulation:       US              DS
G.lite:               0-31           32-255
ADSL1:
ADSL2:
  AnnexA:          0-31           32-255
  AnnexM:          0-63           64-255
  AnnexB:          32-63          64-255
  AnnexL(w/n):     0-31           32-255
ADSL2+:
  AnnexA:          0-31           32-511
  AnnexB:          32-63          64-511
  AnnexM:          0-63           32-511
********************************/
Private long AdslMibGetTotalNumOfLoadedTones(void *gDslVars, unsigned char dir)
{
    long totalNumOfLoadedTones;
    
    if( US_DIRECTION == dir ) {
        if(AdslMibIsAdsl2Mod(gDslVars) && ADSL_PHY_SUPPORT(kAdslPhyAnnexA) && (0 != AdslMibGetAnnexMType(gDslVars))) {
            totalNumOfLoadedTones = 64;   /* AnnexM: 63-0+1 */
        }
        else {
            totalNumOfLoadedTones = 32;   /* AnnexA: 31-0+1; AnnexB:63-32+1; AnnexL:31-0+1 */
        }
    }
    else {
        if(kAdslModAdsl2p == AdslMibGetModulationType(gDslVars)) {
            if(ADSL_PHY_SUPPORT(kAdslPhyAnnexA))
                totalNumOfLoadedTones = 480;  /* 511-32+1 */
            else
                totalNumOfLoadedTones = 448;  /* AnnexB: 511-64+1 */
        }
        else {
            if(ADSL_PHY_SUPPORT(kAdslPhyAnnexA) && (0 == AdslMibGetAnnexMType(gDslVars)))
                totalNumOfLoadedTones = 224;  /* 255-32+1 */
            else
                totalNumOfLoadedTones = 192;   /* AnnexM/B: 255-64+1 */
        }
    }

    return totalNumOfLoadedTones;
}

Public void AdslMibUpdateACTPSD(void *gDslVars, long ACTATP, unsigned char dir)
{    
    long *pActPsd, nLoadedTones;
    
    if(!XdslMibIsVdsl2Mod(gDslVars)) {
#if defined(CONFIG_VDSL_SUPPORTED)
        pActPsd = (US_DIRECTION==dir)? &globalVar.adslMib.xdslPhys.actualPSD: &globalVar.adslMib.xdslAtucPhys.actualPSD;
#else
        pActPsd = (US_DIRECTION==dir)? &globalVar.adslMib.adslPhys.actualPSD: &globalVar.adslMib.adslAtucPhys.actualPSD;
#endif
        nLoadedTones = AdslMibGetTotalNumOfLoadedTones(gDslVars, dir);
        /* ACTPSD(dBm/Hz) = ACTATP(dBm) - (10log10(tone_sapcing) + 10log10(number_of_loaded tones)) */
        *pActPsd = ACTATP - (UtilQ0LinearToQ4dB(4312) + UtilQ0LinearToQ4dB(nLoadedTones));
        *pActPsd = Q4ToTenth(*pActPsd);
        __SoftDslPrintf(gDslVars, "%s ACTPSD: %d/10 dBm/Hz, ACTATP: %d/10 dBm, nLoadedTones: %d\n", 0,
            (US_DIRECTION==dir)? "US": "DS", *pActPsd, Q4ToTenth(ACTATP), nLoadedTones);
    }
}

Private void XdslMibSetActivePath(void *gDslVars, unsigned char dir, int pathId)
{
    if(US_DIRECTION == dir )
      globalVar.pathActive |= (1 << (2+pathId));
    else if(DS_DIRECTION == dir )
      globalVar.pathActive |= (1 << pathId);
}

#if 0
Private Boolean XdslMibIsPathActive(void *gDslVars, unsigned char dir, int pathId)
{
    Boolean res= false;
    
    if(US_DIRECTION == dir )
      res = ( 0 != (globalVar.pathActive & (1 << (2+pathId))) );
    else if(DS_DIRECTION == dir )
      res = ( 0 != (globalVar.pathActive & (1 << pathId)) );
    return res;
}
#endif

Private Boolean XdslMibIsAllPathActive(void *gDslVars, unsigned char dir)
{
    ulong mask = 0;
    
    if(US_DIRECTION == dir ) {
       if(globalVar.adslMib.lp2TxActive)
           mask = 3 << 2;
       else
           mask = 1 << 2;
    }
    else if(DS_DIRECTION == dir ) {
        if(globalVar.adslMib.lp2Active)
            mask = 3;
        else
            mask = 1;
    }
    else
        return false;

    return ( (globalVar.pathActive & mask) == mask );

}

Private void AdslMibCalcAdslPERp(void *gDslVars, int pathId, int dir)
{
    ulong per, rsRate, den, UG;
    
    xdslFramingInfo   *p = &globalVar.adslMib.xdslInfo.dirInfo[dir].lpInfo[pathId];
    UG = p->U*p->G;
    den = (p->M*p->L);
    per =(den) ? (((2*p->T*((ulong)p->M*(p->B[0]+1)+p->R)*UG) << 8)/den) : 1;
    if(0 == per)
        per = 1;

    den = (8*p->N);
    rsRate = (den) ? ((ulong)p->L*4000)/den : 0;

    if(US_DIRECTION == dir) {
        globalVar.rsRate2lp[pathId] = rsRate;
        globalVar.per2lp[pathId] = per;
    }
    else
        globalVar.PERpDS[pathId] = per;
}

#ifdef CONFIG_VDSL_SUPPORTED
Private void AdslMibCalcVdslPERp(void *gDslVars, int pathId, int dir)
{
   ulong                per, rsRate, den;
   xdslFramingInfo   *pXdslFramingParam = &globalVar.adslMib.xdslInfo.dirInfo[dir].lpInfo[pathId];
   
   den = (8*pXdslFramingParam->N);
   rsRate = (den) ? (pXdslFramingParam->L * 4000)/den : 0;

   den = (pXdslFramingParam->M*pXdslFramingParam->L);
   per = (den) ? (((ulong)(2*pXdslFramingParam->T * pXdslFramingParam->U * pXdslFramingParam->N) << 8)/den) : 1; /* Q8 */
   if(0 == per)
   per = 1;

   if(US_DIRECTION == dir) {
     globalVar.rsRate2lp[pathId] = rsRate;
     globalVar.per2lp[pathId] = per;
   }
   else
     globalVar.PERpDS[pathId] = per;
}
#endif

void AdslMibInitTxStat(
  void          *gDslVars, 
  adslConnectionDataStat  *adslTxData, 
  adslPerfCounters    *adslTxPerf, 
  atmConnectionDataStat *atmTxData)
{
  adslMibInfo				*pMib = &globalVar.adslMib;

  BlockByteClear(sizeof(adslPerfCounters), (void*)&pMib->adslTxPerfSinceShowTime);
  BlockByteMove(sizeof(adslPerfCounters), (void *)adslTxPerf, (void*)&pMib->adslTxPerfLast);
}

Public int XdslMibGetCurrentMappedPathId(void *gDslVars, unsigned char dir)
{
    int pathId = globalVar.pathId;
    
    if(US_DIRECTION == dir ) {
      if(true == globalVar.swapTxPath)
          pathId ^= 1;
    }
    else if(DS_DIRECTION == dir ) {
        if(true == globalVar.swapRxPath)
            pathId ^= 1;
    }
    
    return pathId;
}

Public int XdslMibGetPath0MappedPathId(void *gDslVars, unsigned char dir)
{
    int pathId = 0;
    
    if(US_DIRECTION == dir ) {
      if(true == globalVar.swapTxPath)
          pathId = 1;
    }
    else if(DS_DIRECTION == dir ) {
        if(true == globalVar.swapRxPath)
            pathId = 1;
    }
    
    return pathId;
}
Public int XdslMibGetPath1MappedPathId(void *gDslVars, unsigned char dir)
{
    int pathId = 1;
    
    if(US_DIRECTION == dir ) {
      if(true == globalVar.swapTxPath)
          pathId = 0;
    }
    else if(DS_DIRECTION == dir ) {
        if(true == globalVar.swapRxPath)
            pathId = 0;
    }
    
    return pathId;
}

Public void XdslMibGinpEFTRminReported(void *gDslVars)
{
    globalVar.minEFTRReported = true;
}

Public void XdslMibNotifyBitSwapReq(void *gDslVars, unsigned char dir)
{
    globalVar.bitSwapReqTime[dir] = globalVar.txShowtimeTime;
}

Public void XdslMibNotifyBitSwapRej(void *gDslVars, unsigned char dir)
{
    globalVar.bitSwapReqTime[dir] = 0;
}

Public void AdslMibUpdateTxStat(
  void          *gDslVars, 
  adslConnectionDataStat  *adslTxData, 
  adslPerfCounters    *adslTxPerf, 
  atmConnectionDataStat *atmTxData)
{
  adslMibInfo       *pMib = &globalVar.adslMib;
  adslPerfCounters    txPerfTemp;
  ulong       n, updateTimeDiff;
  int       i;
  adslChanPerfDataEntry     *pChanPerfData;
  
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
  pChanPerfData = (kAdslIntlChannel == globalVar.adslMib.adslConnection.chType) ? &globalVar.adslMib.adslChanIntlPerfData : &globalVar.adslMib.adslChanFastPerfData;
#else
  if(!globalVar.adslMib.lp2Active && !globalVar.adslMib.lp2TxActive )
    pChanPerfData = (kAdslIntlChannel == globalVar.adslMib.adslConnection.chType) ? &globalVar.adslMib.adslChanIntlPerfData : &globalVar.adslMib.adslChanFastPerfData;
  else
    pChanPerfData = &globalVar.adslMib.xdslChanPerfData[0];
#endif

  if(!globalVar.txCntReceived) {
    globalVar.txCntReceived++;
    globalVar.txCountersReportedTime = globalVar.txShowtimeTime;
    AdslMibInitTxStat(gDslVars, adslTxData, adslTxPerf, atmTxData);
    for(i=0; i<MAX_LP_NUM; i++) {
      if( (0==i) || XdslMibIs2lpActive(gDslVars, US_DIRECTION) ) {
        globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFEBE] = adslTxData[i].cntSFErr;
        globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFECC] = adslTxData[i].cntRSCor;
        globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFHEC] = atmTxData[i].cntHEC;
        globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellTotal]=atmTxData[i].cntCellTotal;
        globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellData]= atmTxData[i].cntCellData;
        globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfBitErrs]= atmTxData[i].cntBitErrs;
      }
    }
  }
  updateTimeDiff = globalVar.txShowtimeTime - globalVar.txCountersReportedTime;
  for(i=0; i<MAX_LP_NUM; i++) {
    if( (0 ==i ) || XdslMibIs2lpActive(gDslVars, US_DIRECTION) ) {
      n = adslTxData[i].cntSFErr - globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFEBE];
#ifdef BOGUS_CRC_DEBUG
    __SoftDslPrintf(gDslVars, "diffCRC: %ld < ((%ld << 8)/%ld): %ld;  txShowtimeTime=%ld, cnSFErr=%ld\n", 0,
                         n, updateTimeDiff, globalVar.per2lp[i], ((updateTimeDiff<<8)/globalVar.per2lp[i]), globalVar.txShowtimeTime, adslTxData[i].cntSFErr);
#endif
      if ( n < ((updateTimeDiff<<8)/globalVar.per2lp[i]) )   /* per2lp was stored in Q8 */
      {
        pMib->xdslStat[i].xmtStat.cntSFErr += n;
        pMib->xdslStatSincePowerOn[i].xmtStat.cntSFErr += n;
        pChanPerfData[i].perfTotal.adslChanTxCRC += n;
        pChanPerfData[i].perfCurr15Min.adslChanTxCRC += n;
        pChanPerfData[i].perfCurr1Day.adslChanTxCRC  += n;
      }
      n = adslTxData[i].cntRSCor - globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFECC];
      if(n < ((updateTimeDiff*globalVar.rsRate2lp[i])/1000))   /* rsRate is per sec */
      {
        pMib->xdslStat[i].xmtStat.cntRSCor += n;
        pMib->xdslStatSincePowerOn[i].xmtStat.cntRSCor += n;
        pChanPerfData[i].perfTotal.adslChanTxFEC += n;
        pChanPerfData[i].perfCurr15Min.adslChanTxFEC += n;
        pChanPerfData[i].perfCurr1Day.adslChanTxFEC  += n;
      }

      if( XdslMibIsAtmConnectionType(gDslVars) ) {
     #ifdef DSL_REPORT_ALL_COUNTERS
        n = atmTxData[i].cntHEC - globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFHEC];
        if(n<=0x1000000)
        {
		  IncAtmXmtCounterVar(&globalVar.adslMib, 0, cntHEC, n);
        }
     #endif
        n = atmTxData[i].cntCellTotal - globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellTotal];
        if(n<=0x1000000)
		  IncAtmXmtCounterVar(&globalVar.adslMib, i, cntCellTotal, n);
        n = atmTxData[i].cntCellData - globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellData];
        if(n<=0x1000000)
		  IncAtmXmtCounterVar(&globalVar.adslMib, i, cntCellData, n);
        n = atmTxData[i].cntBitErrs - globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfBitErrs];
        if(n<=0x1000000)
		  IncAtmXmtCounterVar(&globalVar.adslMib, i, cntBitErrs, n);
      }
    }
  }
  
  for(i=0; i<MAX_LP_NUM; i++) {
    if( (0==i) || XdslMibIs2lpActive(gDslVars, US_DIRECTION) ) {
      globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFEBE] = adslTxData[i].cntSFErr;
      globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFECC] = adslTxData[i].cntRSCor;
      globalVar.shtCounters2lp[i][kG992ShowtimeNumOfFHEC] = atmTxData[i].cntHEC;
      globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellTotal] = atmTxData[i].cntCellTotal;
      globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfCellData]= atmTxData[i].cntCellData;
      globalVar.shtExtCounters2lp[i][kG992ShowtimeNumOfBitErrs]= atmTxData[i].cntBitErrs;
    }
  }
  
  BlockByteMove(sizeof(adslPerfCounters), (void *)adslTxPerf, (void*)&txPerfTemp);
  DiffTxPerfData(adslTxPerf, &pMib->adslTxPerfLast, adslTxPerf);
  AddTxPerfData(adslTxPerf, &pMib->adslTxPerfTotal, &pMib->adslTxPerfTotal);
  AddTxPerfData(adslTxPerf, &pMib->adslTxPerfCur15Min, &pMib->adslTxPerfCur15Min);
  AddTxPerfData(adslTxPerf, &pMib->adslTxPerfCur1Day, &pMib->adslTxPerfCur1Day);
  AddTxPerfData(adslTxPerf, &pMib->adslTxPerfSinceShowTime, &pMib->adslTxPerfSinceShowTime);
  
  BlockByteMove(sizeof(adslPerfCounters), (void *)&txPerfTemp, (void*)&pMib->adslTxPerfLast);
  globalVar.txCountersReportedTime = globalVar.txShowtimeTime;
}

Public int AdslMibStatusSnooper (void *gDslVars, dslStatusStruct *status)
{
  atmPhyDataEntrty          *pAtmData;
  adslChanPerfDataEntry     *pChanPerfData;
  long                      val, n, nb;
  uchar                     *pVendorId, *pSysVendorId;
  int                       pathId = globalVar.pathId;
  int                       retCode = 0;
  int                       tmpInt;
  
  switch (DSL_STATUS_CODE(status->code)) {
    case kDslOLRResponseStatus:
        val = status->param.value & 0xffff;
        if((kOLRDeferType1 == val) || (kOLRRejectType2 == val) || (kOLRRejectType3 == val))
            globalVar.adslMib.adslStat.bitswapStat.xmtCntRej++;
        break;
    case kDslDspControlStatus:
      switch(status->param.dslConnectInfo.code)
      {
        case kDslATURClockErrorInfo:
           globalVar.adslMib.transceiverClkError = status->param.dslConnectInfo.value;
           break;
        case kFireMonitoringCounters:
        {
          unsigned long *pLong = (unsigned long *)status->param.dslConnectInfo.buffPtr;
          adslFireStat *pFireStat = &globalVar.adslMib.adslStat.fireStat;
          val = status->param.dslConnectInfo.value;
          pFireStat->reXmtRSCodewordsRcved = pLong[kFireReXmtRSCodewordsRcved];
          pFireStat->reXmtUncorrectedRSCodewords = pLong[kFireReXmtUncorrectedRSCodewords];
          pFireStat->reXmtCorrectedRSCodewords = pLong[kFireReXmtCorrectedRSCodewords];
          break;
        }
        case kDslG992RcvShowtimeUpdateGainPtr:
        {
          ushort   *buffPtr = status->param.dslConnectInfo.buffPtr;
          val = status->param.dslConnectInfo.value;
          if (AdslMibTone32_64(gDslVars)) {
            AdslMibSetGain(gDslVars, 32, buffPtr + 32, 32, true);
            AdslMibSetGain(gDslVars, 64, buffPtr + 64, val - 32, false);
          }
          else
            AdslMibSetGain(gDslVars, 32, buffPtr+32, val - 32, false);
#ifdef G992_ANNEXC
          if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars)) {
            AdslMibSetGain(gDslVars, kAdslMibToneNum, buffPtr+kAdslMibToneNum, 256, false);
            AdslMibSetGain(gDslVars, 2*kAdslMibToneNum+32, buffPtr+2*kAdslMibToneNum+32, 224+256, false);
          }
          else
            AdslMibSetGain(gDslVars, kAdslMibToneNum+32, buffPtr+kAdslMibToneNum+32, 224, false);
#endif
        }
          break;
        case kDslNLNoise:
          n = (status->param.dslConnectInfo.value <kAdslMibMaxToneNum ? status->param.dslConnectInfo.value : kAdslMibMaxToneNum);
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.distNoisedB);
          break;
        #ifdef ADSL_MIBOBJ_PLN
        case kDslPLNPeakNoiseTablePtr:
          n=status->param.dslConnectInfo.value;
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNValueps);
          break;
        case kDslPerBinThldViolationTablePtr:
          n=status->param.dslConnectInfo.value;
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNThldCntps);
          break;
        #endif
        case kDslImpulseNoiseDurationTablePtr:
        {
            int i;
            short *pShort = (short *)status->param.dslConnectInfo.buffPtr;
            n=status->param.dslConnectInfo.value;
            for(i=0;i<n/sizeof(short);i++)
               globalVar.PLNDurationHist[i]=pShort[i];
            break;
        }
        case kDslImpulseNoiseDurationTableLongPtr:
            n=status->param.dslConnectInfo.value;
            AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNDurationHist);
            break;
        case kDslImpulseNoiseTimeTablePtr:
        {
            int i;
            short *pShort = (short *)status->param.dslConnectInfo.buffPtr;
            n=status->param.dslConnectInfo.value;
            for(i=0;i<n/sizeof(short);i++)
               globalVar.PLNIntrArvlHist[i]=pShort[i];
            if(globalVar.adslMib.adslPLNData.PLNUpdateData==0)
               globalVar.adslMib.adslPLNData.PLNUpdateData=1;
            break;
        }
        case kDslImpulseNoiseTimeTableLongPtr:
        	n=status->param.dslConnectInfo.value;
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNIntrArvlHist);
          break;
        case kDslINMControTotalSymbolCount:
          globalVar.adslMib.adslPLNData.PLNBBCounter=status->param.dslConnectInfo.value;
          break;
        case kDslPLNMarginPerBin:
          globalVar.adslMib.adslPLNData.PLNThldPerTone=status->param.dslConnectInfo.value;
          break;
        case kDslPLNMarginBroadband:
          globalVar.adslMib.adslPLNData.PLNThldBB=status->param.dslConnectInfo.value;
          break;
        case kDslPerBinMsrCounter:
          globalVar.adslMib.adslPLNData.PLNPerToneCounter=status->param.dslConnectInfo.value;
          break;
        case kDslBroadbandMsrCounter:
          globalVar.adslMib.adslPLNData.PLNBBCounter=status->param.dslConnectInfo.value;
          break;
        case kDslInpBinTablePtr:
          n=status->param.dslConnectInfo.value;
          globalVar.adslMib.adslPLNData.PLNNbDurBins=n/2;
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNDurationBins);
          break;
        case kDslItaBinTablePtr:
          n=status->param.dslConnectInfo.value;
          globalVar.adslMib.adslPLNData.PLNNbIntArrBins=n/2;
          AdslMibByteMove(n,status->param.dslConnectInfo.buffPtr,globalVar.PLNIntrArvlBins);
          break;
        case kDslPlnState:
          globalVar.adslMib.adslPLNData.PLNState=status->param.dslConnectInfo.value;
          break;
      }
      break; /* kDslDspControlStatus */
    case kDslTrainingStatus:
      val = status->param.dslTrainingInfo.value;
      switch (status->param.dslTrainingInfo.code) {
         case kG992LDLastStateDs:
            globalVar.adslMib.adslDiag.ldLastStateDS = (unsigned short)val;
            break;
         case kG992LDLastStateUs:
            globalVar.adslMib.adslDiag.ldLastStateUS = (unsigned short)val;
            break;
         case kG992ControlAllRateOptionsFailedErr:
         case kDslG992BitAndGainVerifyFailed:
            globalVar.lastInitState = kXdslLastInitStateNotFeasible;
            globalVar.adslMib.xdslInitializationCause = kXdslInitConfigNotFeasible;
            break;
#ifdef CONFIG_VDSL_SUPPORTED
        case kDsl993p2Profile:
            globalVar.adslMib.xdslInfo.vdsl2Profile = (ushort)(val & 0xFFFF);
            //globalVar.adslMib.vdslInfo[0].vdsl2Profile = (ushort)(val & 0xFFFF);
          break;
        case kDsl993p2Annex:
            if( VDSL2_ANNEX_A == val )
                XdslMibSetAnnexType(gDslVars, (kAdslTypeAnnexA << kXdslModeAnnexShift));
            else if( VDSL2_ANNEX_B == val )
                XdslMibSetAnnexType(gDslVars, (kAdslTypeAnnexB << kXdslModeAnnexShift));
            else if( VDSL2_ANNEX_C == val )
                XdslMibSetAnnexType(gDslVars, (kAdslTypeAnnexC << kXdslModeAnnexShift));
          break;
#endif
        case kDslG992LatencyPathId:
          globalVar.pathId = ((unsigned char)val < MAX_LP_NUM)? (unsigned char)val: 0;
          break;
        case kDslG992RxLatencyPathCount:
          globalVar.adslMib.lp2Active = (2 == val);
          break;
        case kDslG992TxLatencyPathCount:
          globalVar.adslMib.lp2TxActive = (2 == val);
          break;
        case kG992RcvDelay:
          globalVar.adslMib.adsl2Info2lp[pathId].rcv2DelayInp.delay = (unsigned short)val;
          break;
        case kG992RcvInp:
          globalVar.adslMib.adsl2Info2lp[pathId].rcv2DelayInp.inp = (unsigned short)val;
          break;
        case kG992FireState:
          globalVar.adslMib.xdslStat[0].fireStat.status = val;
          break;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
        case kDslVectoringEnabled:
          globalVar.adslMib.vectSM.state = VECT_WAIT_FOR_CONFIG;
          BlockByteClear(sizeof(VceMacAddress),(char *)&globalVar.adslMib.vectData.macAddress);
          globalVar.adslMib.vectData.esCtrl.writeIdx = 0;
          globalVar.adslMib.vectData.esCtrl.readIdx  = 0;
          globalVar.adslMib.vectData.esCtrl.totalCount = 0;
          break;
        case kDslVectoringState:
          globalVar.adslMib.vectSM.state = val;
          break;
#ifdef G993P5_OVERHEAD_MESSAGING
        case kDslVectoringLineId:
          globalVar.adslMib.vectSM.lineId = val;
          break;
#endif
#endif
        case kDslBondingState:
          globalVar.adslMib.xdslStat[pathId].bondingStat.status = val;
          break;
        case kDslStartedG994p1:
        case kG994p1EventToneDetected:
        case kDslStartedT1p413HS:
        case kDslT1p413ReturntoStartup:
            BlockByteClear(sizeof(globalVar.adslMib.xdslInfo), (void*)&globalVar.adslMib.xdslInfo);
#if defined(CONFIG_VDSL_SUPPORTED)
            globalVar.dsBpHlogForReport=NULL;
            globalVar.dsBpQLNForReport=NULL;
            globalVar.dsBpSNRForReport=NULL;
            BlockByteClear(sizeof(globalVar.adslMib.vdslInfo), (void*)&globalVar.adslMib.vdslInfo[0]);
#endif
            BlockByteClear(sizeof(globalVar.adslMib.adsl2Info2lp), (void*)&globalVar.adslMib.adsl2Info2lp[0]);
            globalVar.pathId = 0;
            globalVar.swapRxPath = false;
            globalVar.swapTxPath = false;
            globalVar.minEFTRReported = false;
            globalVar.adslMib.lp2Active = false;
            globalVar.adslMib.lp2TxActive = false;
            globalVar.adslMib.xdslStat[0].fireStat.status=0;
            globalVar.adslMib.xdslStat[0].ginpStat.status=0;
            globalVar.adslMib.xdslStat[0].bondingStat.status=0;
            globalVar.adslMib.adslRxNonStdFramingAdjustK = 0;
            globalVar.adslMib.adslTrainingState = kAdslTrainingIdle;
            globalVar.vendorIdReceived = false;
            globalVar.rsOptionValid = false;
            globalVar.rsOption[0] = 0;
            globalVar.nMsgCnt = 0;
            globalVar.txUpdateStatFlag=0;
            n = globalVar.linkStatus;
            globalVar.linkStatus = 0;
            globalVar.pathActive = 0;
            globalVar.adslMib.adslNonLinData.NonLinNumAffectedBins=-1;
            globalVar.adslMib.adslNonLinData.NonLinearityFlag=0;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
            globalVar.adslMib.vectSM.state = VECT_DISABLED;
#endif
#ifdef CO_G994_NSIF
			globalVar.adslMib.adslAtucPhys.nsifLen = 0;
#endif
            if (n != 0)
                AdslMibNotify(gDslVars, kAdslEventLinkChange);
            break;
#ifdef CO_G994_NSIF
        case kDslG994p1RcvNonStandardInfo:
			{
			int  AdslCoreGetOemParameter (int paramId, void *buf, int len);

			globalVar.adslMib.adslAtucPhys.nsifLen = AdslCoreGetOemParameter(
				kDslOemG994RcvNSInfo, globalVar.adslMib.adslAtucPhys.adslNsif, sizeof(globalVar.adslMib.adslAtucPhys.adslNsif));
			}
            break;
#endif
        case kDslG992p2RcvVerifiedBitAndGain:
            val = Q4ToTenth(val);
            globalVar.adslMib.adslPhys.adslCurrSnrMgn = RestrictValue(val, -640, 640);
            break;
        case kDslG992p2TxShowtimeActive:
            pathId = XdslMibGetCurrentMappedPathId(gDslVars, US_DIRECTION);

            globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[pathId].dataRate = val;
            if (globalVar.adslMib.adslFramingMode & kAtmHeaderCompression)
               globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[pathId].dataRate = NitroRate(val);

            Xdsl2MibConvertConnectionInfo(gDslVars, pathId, US_DIRECTION);
#ifdef CONFIG_VDSL_SUPPORTED
            if(XdslMibIsVdsl2Mod(gDslVars))
                AdslMibCalcVdslPERp(gDslVars, pathId, US_DIRECTION);
            else
#endif
                AdslMibCalcAdslPERp(gDslVars, pathId, US_DIRECTION);

            globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].aggrDataRate =
            globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[0].dataRate + globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[1].dataRate;

            if(XdslMibIsGinpActive(gDslVars, US_DIRECTION)) {
                /* For Ginp, INPrein is not computed in PHY for non-RTX latency path, and INPreinLp0 = INPLp0, so assign it here */
                tmpInt = XdslMibGetPath0MappedPathId(gDslVars, US_DIRECTION);
                globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[tmpInt].INPrein = globalVar.adslMib.xdslInfo.dirInfo[US_DIRECTION].lpInfo[tmpInt].INP;
            }

            if (AdslMibIsLinkActive(gDslVars)) {
                AdslMibNotify(gDslVars, kAdslEventRateChange);
                break;
            }
            
            globalVar.txCntReceived=0;
            globalVar.txShowtimeTime=0;
            globalVar.secElapsedShTm=0;
            globalVar.adslMib.adslPerfData.adslSinceLinkTimeElapsed = 0;
            
            XdslMibSetActivePath(gDslVars, US_DIRECTION, pathId);
            
            if(XdslMibIsAllPathActive(gDslVars, US_DIRECTION)) {
                globalVar.txCntReceived=0;
                globalVar.txShowtimeTime=0;
                globalVar.secElapsedShTm=0;
                globalVar.txUpdateStatFlag=1;
                globalVar.linkStatus |= kAdslXmtActive;
            }
            
            BlockByteClear(sizeof(globalVar.shtCounters2lp), (void*)&globalVar.shtCounters2lp[0]);
            BlockByteClear(sizeof(globalVar.shtExtCounters2lp), (void*)&globalVar.shtExtCounters2lp[0]);
            BlockByteClear(sizeof(globalVar.ginpCounters), (void*)&globalVar.ginpCounters);
#ifndef ADSL_DRV_NO_STAT_RESET
            n = globalVar.adslMib.xdslStat[0].fireStat.status;
            nb = globalVar.adslMib.xdslStat[0].bondingStat.status;
            /* Need to preserve Ginp counters which was stored on path 0 */
            BlockByteMove(sizeof(globalVar.adslMib.xdslStat[0].ginpStat),
                                    (uchar *)&globalVar.adslMib.xdslStat[0].ginpStat,
                                    (uchar *)&globalVar.adslMib.xdslStat[1].ginpStat);
            BlockByteClear(sizeof(globalVar.adslMib.xdslStat[0]), (void*)&globalVar.adslMib.xdslStat[0]);
            BlockByteMove(sizeof(globalVar.adslMib.xdslStat[0].ginpStat),
                                    (uchar *)&globalVar.adslMib.xdslStat[1].ginpStat,
                                    (uchar *)&globalVar.adslMib.xdslStat[0].ginpStat);
            globalVar.adslMib.xdslStat[0].fireStat.status = n;
            globalVar.adslMib.xdslStat[0].bondingStat.status = nb;
            n = globalVar.adslMib.xdslStat[1].fireStat.status;
            nb = globalVar.adslMib.xdslStat[1].bondingStat.status;
            BlockByteClear(sizeof(globalVar.adslMib.xdslStat[1]), (void*)&globalVar.adslMib.xdslStat[1]);
            globalVar.adslMib.xdslStat[1].fireStat.status = n;
            globalVar.adslMib.xdslStat[1].bondingStat.status = nb;
            BlockByteClear(sizeof(globalVar.adslMib.atmStat2lp), (void*)&globalVar.adslMib.atmStat2lp[0]);

            BlockByteClear(
                sizeof(globalVar.adslMib.adslPerfData.perfSinceShowTime), 
                (void*)&globalVar.adslMib.adslPerfData.perfSinceShowTime
                );

            BlockByteClear(
                sizeof(globalVar.adslMib.adslPerfData.failSinceShowTime), 
                (void*)&globalVar.adslMib.adslPerfData.failSinceShowTime
                );
#endif
            globalVar.adslMib.adslTrainingState = kAdslTrainingConnected;
            globalVar.timeConStarted = 0;
            
            if (AdslMibIsLinkActive(gDslVars)) {
                globalVar.lastInitState = kXdslLastInitStateShowtime;
                globalVar.adslMib.xdslInitializationCause = kXdslInitConfigSuccess;
                globalVar.adslMib.adslPhys.adslCurrStatus = kAdslPhysStatusNoDefect;
                globalVar.adslMib.adslAtucPhys.adslCurrStatus = kAdslPhysStatusNoDefect;
                AdslMibNotify(gDslVars, kAdslEventLinkChange);
            }
            break;
        case kDslG992p2RxShowtimeActive:
            pathId = XdslMibGetCurrentMappedPathId(gDslVars, DS_DIRECTION);
            
            globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[pathId].dataRate = val;
            if (globalVar.adslMib.adslFramingMode & kAtmHeaderCompression)
               globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[pathId].dataRate = NitroRate(val);
            
            Xdsl2MibConvertConnectionInfo(gDslVars, pathId, DS_DIRECTION);
#ifdef CONFIG_VDSL_SUPPORTED
            if(XdslMibIsVdsl2Mod(gDslVars))
               AdslMibCalcVdslPERp(gDslVars, pathId, DS_DIRECTION);
            else
#endif
               AdslMibCalcAdslPERp(gDslVars, pathId, DS_DIRECTION);

            globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].aggrDataRate =
            globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[0].dataRate + globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[1].dataRate;

            if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION)) {
                /* For Ginp, INPrein is not computed in PHY for non-RTX latency path, and INPreinLp0 = INPLp0, so assign it here */
                tmpInt = XdslMibGetPath0MappedPathId(gDslVars, DS_DIRECTION);
                globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[tmpInt].INPrein = globalVar.adslMib.xdslInfo.dirInfo[DS_DIRECTION].lpInfo[tmpInt].INP;
            }
            
            if (AdslMibIsLinkActive(gDslVars)) {
                AdslMibNotify(gDslVars, kAdslEventRateChange);
                break;
            }
            
            XdslMibSetActivePath(gDslVars, DS_DIRECTION, pathId);
            
            if(XdslMibIsAllPathActive(gDslVars, DS_DIRECTION))
                globalVar.linkStatus |= kAdslRcvActive;
            
            if (AdslMibIsLinkActive(gDslVars)) {
                globalVar.lastInitState = kXdslLastInitStateShowtime;
                globalVar.adslMib.xdslInitializationCause = kXdslInitConfigSuccess;
                globalVar.adslMib.adslPhys.adslCurrStatus = kAdslPhysStatusNoDefect;
                AdslMibNotify(gDslVars, kAdslEventLinkChange);
            }
            
            if(XdslMibIsGinpActive(gDslVars, DS_DIRECTION) && (0 == pathId))
                globalVar.adslMib.xdslStat[0].ginpStat.cntDS.minEFTR = val; /* Initialize minEFTR to NDR */
            
            break;
        case kDslRetrainReason:
            globalVar.txUpdateStatFlag=0;
            if(globalVar.adslMib.adslTrainingState == kAdslTrainingConnected)
                globalVar.adslMib.adslPerfData.lastShowtimeDropReason=val;
            if(globalVar.adslMib.adslTrainingState > kAdslTrainingG994)
                globalVar.adslMib.adslPerfData.lastRetrainReason=val;
            break;
        case kDslFinishedG994p1:
            if((ulong)-1 == globalVar.adslMib.adslPhys.adslLDCompleted)
                globalVar.adslMib.adslPhys.adslLDCompleted = (ulong)-2;
            BlockByteClear(sizeof(globalVar.adslMib.adslPhys.adslVendorID), (uchar*)&globalVar.adslMib.adslPhys.adslVendorID[0]);
            globalVar.adslMib.adslPhys.adslVendorID[0] = 0xb5;
            globalVar.adslMib.adslPhys.adslVendorID[1] = 0x00;
            globalVar.adslMib.adslPhys.adslVendorID[2] = 'B';
            globalVar.adslMib.adslPhys.adslVendorID[3] = 'D';
            globalVar.adslMib.adslPhys.adslVendorID[4] = 'C';
            globalVar.adslMib.adslPhys.adslVendorID[5] = 'M';
            globalVar.adslMib.adslPhys.adslVendorID[6] = 0x00;
            globalVar.adslMib.adslPhys.adslVendorID[7] = 0x00;
            
            switch (val) {
                case kG992p2AnnexAB:
                case kG992p2AnnexC:
                    AdslMibSetModulationType(gDslVars, kAdslModGlite);
                    break;
#ifdef G992P1_ANNEX_I
                case (kG992p1AnnexI>>4):
                    AdslMibSetModulationType(gDslVars, kAdslModAnnexI);
                    break;
#endif
#ifdef G992P3
                case kG992p3AnnexA:
                case kG992p3AnnexB:
                    AdslMibSetModulationType(gDslVars, kAdslModAdsl2);
                    break;
#endif
#ifdef G992P5
                case kG992p5AnnexA:
                case kG992p5AnnexB:
                    AdslMibSetModulationType(gDslVars, kAdslModAdsl2p);
                    break;
#endif
#ifdef G993
               case kG993p2AnnexA:
                   globalVar.waitBandPlan=1;
                   AdslMibSetModulationType(gDslVars, kVdslModVdsl2);
                   break;
#endif
                case kG992p1AnnexA:
                case kG992p1AnnexB:
                case kG992p1AnnexC:
                default:
                    AdslMibSetModulationType(gDslVars, kAdslModGdmt);
                    break;
            }
            globalVar.nTones = kAdslMibToneNum;
#ifdef G992P5
            if (kAdslModAdsl2p == AdslMibGetModulationType(gDslVars))
                globalVar.nTones = kAdslMibToneNum * 2;
#endif
#ifdef G992_ANNEXC
            globalVar.nTones = kAdslMibToneNum * 
                ((kAdslModAnnexI == AdslMibGetModulationType(gDslVars)) ? 4 : 2);
#endif
#ifdef G993
            if (kVdslModVdsl2 == AdslMibGetModulationType(gDslVars)) {
                if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
                    globalVar.nTones = MAX_TONE_NUM/2;
                else
                    globalVar.nTones = kVdslMibToneNum;
                 globalVar.adslMib.adslTrainingState = kAdslTrainingG993Started;
            }
            else
#endif
            globalVar.adslMib.adslTrainingState = kAdslTrainingG992Started;

            if (kVdslModVdsl2 != AdslMibGetModulationType(gDslVars)) {
                 /* Will get updated when framing parameters statuses arrived on the 6368/6362 in ADSL mode */
                globalVar.adslMib.adsl2Info2lp[0].rcvChanInfo.ahifChanId = 0;
                globalVar.adslMib.adsl2Info2lp[0].rcvChanInfo.connectionType = kXdslDataAtm;
                globalVar.adslMib.adsl2Info2lp[0].xmtChanInfo.ahifChanId = 0;
                globalVar.adslMib.adsl2Info2lp[0].xmtChanInfo.connectionType = kXdslDataAtm;
                if( 2 == MAX_LP_NUM) {
                    globalVar.adslMib.adsl2Info2lp[1].rcvChanInfo.ahifChanId = 2;
                    globalVar.adslMib.adsl2Info2lp[1].rcvChanInfo.connectionType = kXdslDataAtm;
                    globalVar.adslMib.adsl2Info2lp[1].xmtChanInfo.ahifChanId = 2;
                    globalVar.adslMib.adsl2Info2lp[1].xmtChanInfo.connectionType = kXdslDataAtm;
                }
            }
#ifdef CONFIG_VDSL_SUPPORTED
            globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds =
            globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus =
            globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds =
            globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus = 1;
#endif
            break;
        case kDslG992p3AnnexLMode:
            if ((val != 0) && (kAdslModAdsl2 == AdslMibGetModulationType(gDslVars))) {
                AdslMibSetModulationType(gDslVars, kAdslModReAdsl2);
                if (kG994p1G992p3AnnexLUpNarrowband == (val & 0xFF))
                    AdslMibSetAnnexLUpType(gDslVars, kAdsl2ModeAnnexLUpNarrow);
                else
                    AdslMibSetAnnexLUpType(gDslVars, kAdsl2ModeAnnexLUpWide);
                if (kG994p1G992p3AnnexLDownNonoverlap == ((val >> 8) & 0xFF))
                    AdslMibSetAnnexLDnType(gDslVars, kAdsl2ModeAnnexLDnNonOvlap);
                else
                    AdslMibSetAnnexLDnType(gDslVars, kAdsl2ModeAnnexLDnOvlap);
            }
            break;
        case kG992EnableAnnexM:
            AdslMibSetAnnexMType(gDslVars, val+1);
            break;
        case kDslFinishedT1p413:
            AdslMibSetModulationType(gDslVars, kAdslModT1413);
            globalVar.adslMib.adslTrainingState = kAdslTrainingG992Started;
            BlockByteClear(sizeof(globalVar.adslMib.adslPhys.adslVendorID), &globalVar.adslMib.adslPhys.adslVendorID[0]);
            globalVar.adslMib.adslPhys.adslVendorID[0] = 0x54;
            globalVar.adslMib.adslPhys.adslVendorID[1] = 0x4d;
            break;
        case kG992DataRcvDetectLOS:
            globalVar.adslMib.adslPhys.adslCurrStatus |= kAdslPhysStatusLOS;
            globalVar.adslMib.adslPhys.adslCurrStatus &= ~(kAdslPhysStatusLOF | kAdslPhysStatusLOM);
            IncPerfCounterVar(&globalVar.adslMib.adslPerfData, adslLoss);
            AdslMibUpdateLOS(gDslVars);
            break;
        case kG992DataRcvDetectLOSRecovery:
            globalVar.adslMib.adslPhys.adslCurrStatus &= ~kAdslPhysStatusLOS;
            break;
        case kG992DecoderDetectRemoteLOS:
            globalVar.adslMib.adslAtucPhys.adslCurrStatus |= kAdslPhysStatusLOS;
            break;
        case kG992DecoderDetectRemoteLOSRecovery:
            globalVar.adslMib.adslAtucPhys.adslCurrStatus &= ~kAdslPhysStatusLOS;
            break;
        case kG992DecoderDetectRDI:
            if (0 == (globalVar.adslMib.adslPhys.adslCurrStatus & kAdslPhysStatusLOS))
                globalVar.adslMib.adslPhys.adslCurrStatus |= kAdslPhysStatusLOF;
            IncPerfCounterVar(&globalVar.adslMib.adslPerfData, adslLofs);
            AdslMibUpdateLOF(gDslVars);
            break;
        case kG992DecoderDetectRDIRecovery:
            globalVar.adslMib.adslPhys.adslCurrStatus &= ~kAdslPhysStatusLOF;
            break;
        case kG992LDCompleted:
            globalVar.adslMib.adslPhys.adslLDCompleted = (val != 0)? 2: -1;
            break;
        case kDslG994p1StartupFinished:
            globalVar.adslMib.adslTrainingState = kAdslTrainingG994;
            break;
        case kDslG992p2Phase3Started:
            globalVar.adslMib.adslTrainingState = kAdslTrainingG992ChanAnalysis;
            break;
        case kDslG992p2Phase4Started:
            globalVar.adslMib.adslTrainingState = kAdslTrainingG992Exchange;
            break;
        case kDslG992p2ReceivedBitGainTable:
            globalVar.nMsgCnt = 0;
            /* fall through */
        case kDslG992p2ReceivedMsgLD:
        case kDslG992p2ReceivedRates1:
        case kDslG992p2ReceivedMsg1:
        case kDslG992p2ReceivedRatesRA:
        case kDslG992p2ReceivedMsgRA:
        case kDslG992p2ReceivedRates2:
        case kDslG992p2ReceivedMsg2:
            globalVar.g992MsgType = status->param.dslTrainingInfo.code;
            break;
        case kDslG992Timeout:
            IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslInitTo);
            break;
        default:
            break;
      }
      break; /* kDslTrainingStatus */
    
    case kDslConnectInfoStatus:
      val = status->param.dslConnectInfo.value;
      switch (status->param.dslConnectInfo.code) {
        case kDslNLdbEcho:
            globalVar.adslMib.adslNonLinData.NonLinDbEcho = (short)val;
            break;
        case kDslG992VendorID:
            val = (val < VENDOR_TBL_SIZE) ? val: 0;
#ifdef CONFIG_VDSL_SUPPORTED
            pVendorId = &globalVar.adslMib.xdslAtucPhys.adslVendorID[0];
            pSysVendorId = &globalVar.adslMib.xdslAtucPhys.adslSysVendorID[0];
#else
            pVendorId = &globalVar.adslMib.adslAtucPhys.adslVendorID[0];
            pSysVendorId = &globalVar.adslMib.adslAtucPhys.adslSysVendorID[0];
#endif
            AdslMibByteMove(6, (void *)&gVendorTbl[val][0], pVendorId);
            AdslMibByteMove(kAdslPhysVendorIdLen, pVendorId, pSysVendorId);
            __SoftDslPrintf(gDslVars, "kDslG992VendorID: %c%c%c%c\n",
                         0, pVendorId[2],pVendorId[3],pVendorId[4],pVendorId[5]);
            break;
#ifdef G994P1
        case    kG994MessageExchangeRcvInfo:
        {
            uchar   *msgPtr = ((unsigned char*)status->param.dslConnectInfo.buffPtr);
            ushort  v;
            if ((msgPtr != NULL) && ((msgPtr[0] == 2) || (msgPtr[0] == 3)) &&   /* CL or CLR message */
                (val >= (2 + kAdslPhysVendorIdLen)) && !globalVar.vendorIdReceived) {
                globalVar.vendorIdReceived=true;
#ifdef CONFIG_VDSL_SUPPORTED
                AdslMibByteMove(kAdslPhysVendorIdLen, msgPtr+2, globalVar.adslMib.xdslAtucPhys.adslVendorID);
                AdslMibByteMove(kAdslPhysVendorIdLen, msgPtr+2, globalVar.adslMib.xdslAtucPhys.adslSysVendorID);
                v=((uchar)globalVar.adslMib.xdslAtucPhys.adslVendorID[6]<<8)+(uchar)globalVar.adslMib.xdslAtucPhys.adslVendorID[7];
                sprintf(globalVar.adslMib.xdslAtucPhys.adslVersionNumber,"0x%04x",v);
#else
                AdslMibByteMove(kAdslPhysVendorIdLen, msgPtr+2, globalVar.adslMib.adslAtucPhys.adslVendorID);
                AdslMibByteMove(kAdslPhysVendorIdLen, msgPtr+2, globalVar.adslMib.adslAtucPhys.adslSysVendorID);
                v=((uchar)globalVar.adslMib.adslAtucPhys.adslVendorID[6]<<8)+(uchar)globalVar.adslMib.adslAtucPhys.adslVendorID[7];
                sprintf(globalVar.adslMib.adslAtucPhys.adslVersionNumber,"0x%04x",v);
#endif
            }
        }
            break;
#endif
        case kG992p2XmtCodingParamsInfo:
            {
            G992CodingParams *codingParam = (G992CodingParams*) status->param.dslConnectInfo.buffPtr;

            codingParam->AS0BF = codingParam->AS1BF = codingParam->AS2BF = codingParam->AS3BF = codingParam->AEXAF = 0;
            codingParam->AS0BI = codingParam->AS1BI = codingParam->AS2BI = codingParam->AS3BI = codingParam->AEXAI = 0;
#if 0
            AdslMibByteMove(sizeof(G992CodingParams), codingParam, &globalVar.xmtParams);
            AdslMibSetChanEntry(codingParam, &globalVar.adslMib.adslChanFast, &globalVar.adslMib.adslChanIntl);
            AdslMibSetConnectionInfo(gDslVars, codingParam, status->param.dslConnectInfo.code, val, &globalVar.adslMib.adslConnection);
            Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info.xmt2Info, &globalVar.adslMib.adslConnection.xmtInfo);
#else
            AdslMibByteMove(sizeof(G992CodingParams), codingParam, &globalVar.xmtParams2lp[pathId]);
            AdslMibSetChanEntry(codingParam, &globalVar.adslMib.adslChanFast, &globalVar.adslMib.adslChanIntl);
            AdslMibSetConnectionInfo(gDslVars, codingParam, status->param.dslConnectInfo.code, val, &globalVar.adslMib.xdslConnection[pathId]);
            Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info2lp[pathId].xmt2Info, &globalVar.adslMib.xdslConnection[pathId].xmtInfo);
#endif
            }
            break;
        case kG992p2RcvCodingParamsInfo:
            {
            G992CodingParams *codingParam = (G992CodingParams*) status->param.dslConnectInfo.buffPtr;
#if 0
            AdslMibByteMove(sizeof(G992CodingParams), codingParam, &globalVar.rcvParams);
            AdslMibSetChanEntry(codingParam, &globalVar.adslMib.adslChanFast, &globalVar.adslMib.adslChanIntl);
            AdslMibSetConnectionInfo(gDslVars, codingParam, status->param.dslConnectInfo.code, val, &globalVar.adslMib.adslConnection);
            Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info.rcv2Info, &globalVar.adslMib.adslConnection.rcvInfo);
#else
            AdslMibByteMove(sizeof(G992CodingParams), codingParam, &globalVar.rcvParams2lp[pathId]);
            AdslMibSetChanEntry(codingParam, &globalVar.adslMib.adslChanFast, &globalVar.adslMib.adslChanIntl);
            AdslMibSetConnectionInfo(gDslVars, codingParam, status->param.dslConnectInfo.code, val, &globalVar.adslMib.xdslConnection[pathId]);
            Adsl2MibSetInfoFromGdmt1(gDslVars, &globalVar.adslMib.adsl2Info2lp[pathId].rcv2Info, &globalVar.adslMib.xdslConnection[pathId].rcvInfo);
#endif
            }
            break;

        case kG992p3XmtCodingParamsInfo:
        case kG992p3RcvCodingParamsInfo:
            {
            G992p3CodingParams *codingParam = (G992p3CodingParams *) status->param.dslConnectInfo.buffPtr;

            Adsl2MibSetConnectionInfo(gDslVars, codingParam, status->param.dslConnectInfo.code, val, &globalVar.adslMib.adsl2Info);
            }
            break;

        case kG992p3PwrStateInfo:
            globalVar.adslMib.adsl2Info.pwrState = val;	/* For DslDiag backward compatible */
            globalVar.adslMib.xdslInfo.pwrState = val;
            if( 2 == val )
                AdslCoreIndicateLinkPowerStateL2(gLineId(gDslVars));
#ifdef DEBUG_L2_RET_L0
            printk("%s: L%d\n", __FUNCTION__, val);
#endif
            break;

        case kDslATUAvgLoopAttenuationInfo:
            val = Q4ToTenth(val);
            globalVar.adslMib.adslPhys.adslCurrAtn = RestrictValue(val, 0, 1630);
            globalVar.adslMib.adslPhys.adslSignalAttn = globalVar.adslMib.adslPhys.adslCurrAtn;
            break;

        case kDslSignalAttenuation:
            val = Q4ToTenth(val);
            globalVar.adslMib.adslDiag.signalAttn = val;
            globalVar.adslMib.adslPhys.adslSignalAttn = val;
            break;

        case kDslAttainableNetDataRate:
            globalVar.adslMib.adslDiag.attnDataRate = val;
            globalVar.adslMib.adslPhys.adslCurrAttainableRate = ActualRate(val);
            break;

        case kDslHLinScale:
            globalVar.adslMib.adslDiag.hlinScaleFactor = val;
            globalVar.adslMib.adslPhys.adslHlinScaleFactor = val;
            break;

        case kDslATURcvPowerInfo:
            globalVar.rcvPower = val;
            break;

        case kDslMaxReceivableBitRateInfo:
            globalVar.adslMib.adslPhys.adslCurrAttainableRate = ActualRate(val * 1000);
            break;

        case kDslRcvCarrierSNRInfo:
            n = kAdslMibToneNum;
#ifdef G992_ANNEXC
            if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars))
                n = kAdslMibToneNum << 1;
#endif
#ifdef G992P5
            if (kAdslModAdsl2p == AdslMibGetModulationType(gDslVars))
                n = kAdslMibToneNum << 1;
#endif
#ifdef G993
            if( kVdslModVdsl2 == AdslMibGetModulationType(gDslVars) ) {
                int k,n,i;short *SnrBuff;
                SnrBuff=(short *)status->param.dslConnectInfo.buffPtr;
                k=0;
                for(n=0;n<globalVar.adslMib.dsPhyBandPlan.noOfToneGroups;n++) {
                    if(globalVar.adslMib.dsPhyBandPlan.toneGroups[n].endTone< globalVar.nTones)
                        for(i=globalVar.adslMib.dsPhyBandPlan.toneGroups[n].startTone;i<=globalVar.adslMib.dsPhyBandPlan.toneGroups[n].endTone;i++)
                            globalVar.snr[i]=SnrBuff[k++];
                }
            }
            else
#endif
            AdslMibByteMove(
                val * sizeof(globalVar.snr[0]),
                status->param.dslConnectInfo.buffPtr, 
                globalVar.snr + globalVar.nMsgCnt * n);
          /*  if (val > n)
                val = n;*/
#ifdef G992_ANNEXC
            globalVar.nMsgCnt ^= 1;
#endif
            break;
        case kG992p2XmtToneOrderingInfo:
            {
            uchar   *buffPtr = status->param.dslConnectInfo.buffPtr;

            if (AdslMibTone32_64(gDslVars)) {
                AdslMibSetBitAllocation(gDslVars, 0, buffPtr + 0, 32, false);
                AdslMibSetBitAllocation(gDslVars, 32, buffPtr + 32, 32, true);
            }
            else
                AdslMibSetBitAllocation(gDslVars, 0, buffPtr, val, false);
            
            if(AdslMibIsLinkActive(gDslVars)) {
                if ( XdslMibIsXdsl2Mod(gDslVars) ) {
                    if(globalVar.bitSwapReqTime[US_DIRECTION]) {
                        globalVar.adslMib.adslStat.bitswapStat.xmtCnt++;
                        globalVar.bitSwapReqTime[US_DIRECTION] = 0;
                    }
                }
#if 0
                else {
                    globalVar.adslMib.adslStat.bitswapStat.xmtCnt++;
                    globalVar.adslMib.adslStat.bitswapStat.xmtCntReq++;
                }
#endif
            }
#ifdef G992_ANNEXC
            n = kAdslMibToneNum;
            if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars))
                n <<= 1;
            AdslMibSetBitAllocation(gDslVars, n, buffPtr + 32, 32, false);
#endif
            }
            break;

        case kG992p2RcvToneOrderingInfo:
            {
            uchar   *buffPtr = status->param.dslConnectInfo.buffPtr;

            if (AdslMibTone32_64(gDslVars)) {
                AdslMibSetBitAllocation(gDslVars, 32, buffPtr + 32, 32, true);
                AdslMibSetBitAllocation(gDslVars, 64, buffPtr + 64, val - 32, false);
            }
            else
                AdslMibSetBitAllocation(gDslVars, 32, buffPtr+32, val - 32, false);
            
            if(AdslMibIsLinkActive(gDslVars)) {
                if ( XdslMibIsXdsl2Mod(gDslVars) ) {
                    if(globalVar.bitSwapReqTime[DS_DIRECTION]) {
                        globalVar.adslMib.adslStat.bitswapStat.rcvCnt++;
                        globalVar.bitSwapReqTime[DS_DIRECTION] = 0;
                    }
                }
#if 0
                else {
                    globalVar.adslMib.adslStat.bitswapStat.rcvCnt++;
                    globalVar.adslMib.adslStat.bitswapStat.rcvCntReq++;
                }
#endif
            }
#ifdef G992_ANNEXC
            if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars)) {
                AdslMibSetBitAllocation(gDslVars, kAdslMibToneNum, buffPtr+kAdslMibToneNum, 256, false);
                AdslMibSetBitAllocation(gDslVars, 2*kAdslMibToneNum+32, buffPtr+2*kAdslMibToneNum+32, 224+256, false);
            }
            else
                AdslMibSetBitAllocation(gDslVars, kAdslMibToneNum+32, buffPtr+kAdslMibToneNum+32, 224, false);
#endif
            }
            break;
            
        case kG992MessageExchangeRcvInfo:
            if (kDslG992p2ReceivedMsg2 == globalVar.g992MsgType) {
                uchar   *msgPtr = (uchar *)status->param.dslConnectInfo.buffPtr;
                int     n;

                n = (*msgPtr) | ((*(msgPtr+1) & 1) << 8);
                globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate = ActualRate(n * 4000);
                globalVar.adslMib.adslAtucPhys.adslCurrSnrMgn = (*(msgPtr+2) & 0x1F) * 10;
                globalVar.adslMib.adslAtucPhys.adslCurrAtn = (*(msgPtr+3) >> 2) * 5;
            }
            else if (kDslG992p2ReceivedBitGainTable == globalVar.g992MsgType) {
                short   *buffPtr = status->param.dslConnectInfo.buffPtr;
                int     n;

                globalVar.bitAlloc[0] = 0;
                globalVar.gain[0] = 0;
                val = status->param.dslConnectInfo.value >> 1;
                n   = 0;
#ifdef G992P3
                if (XdslMibIsXdsl2Mod(gDslVars)) {
                    uchar   *p = (uchar *) buffPtr;
                    int     rate;

                    val -= 7;
                    n    = 7;
                    globalVar.adslMib.adslAtucPhys.adslCurrAtn = (((ulong) p[1] & 0x3) << 8) | p[0];
                    globalVar.adslMib.adslAtucPhys.adslCurrSnrMgn = ((long) p[5] << 8) | p[4];
                    rate = ((ulong) p[9] << 24) | ((ulong) p[8] << 16) | ((ulong) p[7] << 8) | p[6];
                    globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate = ActualRate(rate);

                    rate = ((long) p[11] << 8) | p[10];
                    globalVar.adslMib.adslPhys.adslCurrOutputPwr = (rate << (32-10)) >> (32-10);
                }
#endif

#ifdef G992_ANNEXC
                if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars)) {
                    AdslMibSetBitAndGain(gDslVars, 1+globalVar.nMsgCnt*2*kAdslMibToneNum, buffPtr + 0, 31, false);
                }
                else {
                    AdslMibSetBitAndGain(gDslVars, 1, buffPtr + 0, 31, false);
                    globalVar.bitAlloc[kAdslMibToneNum] = 0;
                    globalVar.gain[kAdslMibToneNum] = 0;
                    AdslMibSetBitAndGain(gDslVars, kAdslMibToneNum + 1, buffPtr + 31, 31, false);
                }
#else
                AdslMibSetBitAndGain(gDslVars, 1, buffPtr + n, val, false);
#endif
            }
            else if (kDslG992p2ReceivedRatesRA == globalVar.g992MsgType) {
                uchar   *msgPtr = (uchar *)status->param.dslConnectInfo.buffPtr;
                
                for (n = 0; n < 4; n++)
                    globalVar.rsOption[1+n] = msgPtr[n*30 + 21] & 0x3F;
                globalVar.rsOptionValid = true;
            }
            else if (kDslG992p2ReceivedMsgLD == globalVar.g992MsgType) {
                uchar   *p = (uchar *)status->param.dslConnectInfo.buffPtr;
                char    *ps = (char *) p;
                ulong   msgLen;
                int     i, j;

                msgLen = status->param.dslConnectInfo.value;
                switch (*p) {
                  case 0x11:
                    {
                    ulong   n;

                    globalVar.adslMib.adslAtucPhys.adslHlinScaleFactor = ((int) p[3] << 8) + p[2];
                    globalVar.adslMib.adslAtucPhys.adslCurrAtn = ((int) p[5] << 8) + p[4];
                    globalVar.adslMib.adslAtucPhys.adslSignalAttn = ((int) p[7] << 8) + p[6];
                    globalVar.adslMib.adslAtucPhys.adslCurrSnrMgn = ((int) ps[9] << 8) + p[8];
                    globalVar.adslMib.adslPhys.adslCurrOutputPwr = ((int) ps[15] << 8) + p[14];
                    n = ((ulong) p[13] << 24) | ((ulong) p[12] << 16) | ((ulong) p[11] << 8) | p[10];
                    globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate = n;
                    }
                    break;

                  case 0x22:
                    j = 0;
                    for (i = 2; i < msgLen; i += 4) {
                        globalVar.chanCharLin[j].x = ((int) ps[i+1] << 8) + p[i+0];
                        globalVar.chanCharLin[j].y = ((int) ps[i+3] << 8) + p[i+2];
                        j++;
                    }
                    break;
                  case 0x33:
                    j = 0;
                    for (i = 2; i < msgLen; i += 2) {
                        int     n;

                        n = ((int) (p[i+1] & 0x3) << 12) + (p[i] << 4);
                        n = -(n/10) + 6*16;
                        globalVar.chanCharLog[j] = n;
                        j++;
                    }
                    break;
                  case 0x44:
                    for (i = 2; i < msgLen; i++)
                        globalVar.quietLineNoise[i-2] = -(((short) p[i]) << 3) - 23*16;
                    break;
                  case 0x55:
                    for (i = 2; i < msgLen; i++)
                        globalVar.snr[i-2] = (((short) p[i]) << 3) - 32*16;
                    break;

                }
            }
#if 1 /* only add this for T1 case */
            else if ((kAdslModT1413 == globalVar.adslMib.adslConnection.modType) &&
                        (kDslG992p2ReceivedMsg1 == globalVar.g992MsgType)){
                uchar   vendorVer;
                ushort  vendorId;
                uchar   *msgPtr = (uchar *)status->param.dslConnectInfo.buffPtr;
                globalVar.vendorIdReceived=true;
                 vendorId = ((ushort)msgPtr[3] >> 4) + ((ushort)msgPtr[4] << 4) + (((ushort)msgPtr[5] & 0x0F) << 12);
                vendorVer = (msgPtr[2]  >> 2) & 0x1F;
#ifdef CONFIG_VDSL_SUPPORTED
                pVendorId = &globalVar.adslMib.xdslAtucPhys.adslVendorID[0];
                pSysVendorId = &globalVar.adslMib.xdslAtucPhys.adslSysVendorID[0];
                sprintf(globalVar.adslMib.xdslAtucPhys.adslVersionNumber,"0x%04x",vendorVer);
#else
                pVendorId = &globalVar.adslMib.adslAtucPhys.adslVendorID[0];
                pSysVendorId = &globalVar.adslMib.adslAtucPhys.adslSysVendorID[0];
                sprintf(globalVar.adslMib.adslAtucPhys.adslVersionNumber,"0x%04x",vendorVer);
#endif
                pVendorId[0] = 0xB5;
                pVendorId[1] = 0x00;
                pVendorId[6] = 0x00;
                pVendorId[7] = vendorVer;
                switch (vendorId)
                {
                    case 0x4D54:
                        AdslMibByteMove(4, "BDCM", pVendorId+2);
                        break;
                    case 0x001C:
                        AdslMibByteMove(4, "ANDV", pVendorId+2);
                        break;
                    case 0x0039:
                        AdslMibByteMove(4, "GSPN", pVendorId+2);
                        break;
                    case 0x0022:
                        AdslMibByteMove(4, "ALCB", pVendorId+2);
                        break;
                    case 0x0004:
                        AdslMibByteMove(4, "TSTC", pVendorId+2);
                        break;
                    case 0x0050:
                    case 0xB6DB:
                        AdslMibByteMove(4, "CENT", pVendorId+2);
                        break;
#if 0   /* Don't know the vendor ID for these */
                    case ????:
                        AdslMibByteMove(4, "IFTN", pVendorId+2);
                        break;
                    case ????:
                        AdslMibByteMove(4, "IKNS", pVendorId+2);
                        break;
#endif
                    default:
                        AdslMibByteMove(4, "UNKN", pVendorId+2);
                        __SoftDslPrintf(gDslVars, "kDslG992p2ReceivedMsg1: Unknown chipset vendorId =%x version=0x%4x\n",
                            0, vendorId, vendorVer);
                        break;
                }
                AdslMibByteMove(kAdslPhysVendorIdLen, pVendorId, pSysVendorId);
                __SoftDslPrintf(gDslVars, "kDslG992p2ReceivedMsg1: vendorId =%x, %c%c%c%c:0x%04x\n",
                            0, vendorId, pVendorId[2],pVendorId[3],pVendorId[4],pVendorId[5], vendorVer);
            }
#endif
            break;

        case kG992MessageExchangeXmtInfo:
            if ((1 == status->param.dslConnectInfo.value) && globalVar.rsOptionValid) {
                static  uchar optNum[16] = { 0, 1, 2, 0, 3, 0,0,0, 4,  0,0,0,0,0,0,0 };
                uchar   *msgPtr = (uchar *)status->param.dslConnectInfo.buffPtr;

                globalVar.rsOption[0] = globalVar.rsOption[optNum[*msgPtr & 0xF]];
                break;
            }

            if (kDslG992p2ReceivedBitGainTable == globalVar.g992MsgType) {
                short * buffPtr = status->param.dslConnectInfo.buffPtr;
                int     n;

                val = status->param.dslConnectInfo.value >> 1;
                n   = 0;
#ifdef G992P3
                if (XdslMibIsXdsl2Mod(gDslVars)) {
                    val -= 7;
                    n    = 7;
                }
#endif

#ifdef G992_ANNEXC
                if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars)) {
                    AdslMibSetBitAndGain(gDslVars, 32+globalVar.nMsgCnt*2*kAdslMibToneNum, buffPtr + 31, 224+256, false);
                }
                else {
                    AdslMibSetBitAndGain(gDslVars, 32, buffPtr + 31, 224, false);
                    AdslMibSetBitAndGain(gDslVars, kAdslMibToneNum + 32, buffPtr + 255 + 31, 224, false);
                }
#else
                if (AdslMibTone32_64(gDslVars)) {
                    AdslMibSetBitAndGain(gDslVars, 32, buffPtr + 31 + n, 32, true);
                    AdslMibSetBitAndGain(gDslVars, 64, buffPtr + 63 + n, val - 32, false);
                }
                else
#endif
                    AdslMibSetBitAndGain(gDslVars, 32, buffPtr + 31 + n, val > 300 ? 480 : 224, false);
            }
            break;

        case kG992ShowtimeMonitoringStatus:
            {
            ulong   *counters = (ulong*) (status->param.dslConnectInfo.buffPtr);
            pathId = XdslMibGetCurrentMappedPathId(gDslVars, DS_DIRECTION);
#if defined(CONFIG_BCM96358) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
            pChanPerfData = (kAdslIntlChannel == globalVar.adslMib.adslConnection.chType) ?
                &globalVar.adslMib.adslChanIntlPerfData : &globalVar.adslMib.adslChanFastPerfData;
#else
            if(!globalVar.adslMib.lp2Active && !globalVar.adslMib.lp2TxActive ) {
                pChanPerfData = (kAdslIntlChannel == globalVar.adslMib.adslConnection.chType) ?
                   &globalVar.adslMib.adslChanIntlPerfData : &globalVar.adslMib.adslChanFastPerfData;
            }
            else
                pChanPerfData = &globalVar.adslMib.xdslChanPerfData[pathId];
#endif
            if (XdslMibIsDataCarryingPath(gDslVars, pathId, DS_DIRECTION)) {
               if (AdslMibShowtimeDataError(counters, (ulong *)&globalVar.shtCounters2lp[pathId])) {
                   ulong   nErr = AdslMibShowtimeSFErrors(counters, (ulong *)&globalVar.shtCounters2lp[pathId]);
                   AdslMibUpdateShowtimeErrors(gDslVars, nErr, pathId);
               }
               n = AdslMibShowtimeRSErrors(counters, (ulong *)&globalVar.shtCounters2lp[pathId]);
               if (n != 0)
                   AdslMibUpdateShowtimeRSErrors(gDslVars, n);
            }
            val = counters[kG992ShowtimeSuperFramesRcvd] - globalVar.shtCounters2lp[pathId][kG992ShowtimeSuperFramesRcvd];
            AddBlockCounterVar(pChanPerfData, adslChanReceivedBlks, val);
            if(XdslMibIsXdsl2Mod(gDslVars))
              val=val*globalVar.PERpDS[pathId]/globalVar.per2lp[pathId];
            AddBlockCounterVar(pChanPerfData, adslChanTransmittedBlks, val);  /* TBD */
            
            val = counters[kG992ShowtimeSuperFramesRcvdWrong] - globalVar.shtCounters2lp[pathId][kG992ShowtimeSuperFramesRcvdWrong];
            AddBlockCounterVar(pChanPerfData, adslChanUncorrectBlks, val);

            val = counters[kG992ShowtimeRSCodewordsRcvedCorrectable] - globalVar.shtCounters2lp[pathId][kG992ShowtimeRSCodewordsRcvedCorrectable];
            AddBlockCounterVar(pChanPerfData, adslChanCorrectedBlks, val);

            if(XdslMibIsXdsl2Mod(gDslVars)) {
                int pathIdUS = XdslMibGetCurrentMappedPathId(gDslVars, US_DIRECTION);
                counters[kG992ShowtimeNumOfFEBE] = globalVar.shtCounters2lp[pathIdUS][kG992ShowtimeNumOfFEBE];
                counters[kG992ShowtimeNumOfFECC] = globalVar.shtCounters2lp[pathIdUS][kG992ShowtimeNumOfFECC];
                counters[kG992ShowtimeNumOfFHEC] = globalVar.shtCounters2lp[pathIdUS][kG992ShowtimeNumOfFHEC];
            }
            
            AdslMibConnectionStatUpdate (gDslVars, (ulong *)&globalVar.shtCounters2lp[pathId], counters, pathId);
            AdslMibByteMove(sizeof(globalVar.shtCounters), counters, &globalVar.shtCounters2lp[pathId]);
            }
            break;
#if 1
        case kG992BitswapState:
            globalVar.adslMib.adslStat.bitswapStat.status = val;
            __SoftDslPrintf(gDslVars, "kG992BitswapState status=%d",0,globalVar.adslMib.adslStat.bitswapStat.status);
            break;
        case kG992AocBitswapTxStarted:
            __SoftDslPrintf(gDslVars, "kG992AocBitswapTxStarted xmtCntReq=%d",0,globalVar.adslMib.adslStat.bitswapStat.xmtCntReq);
            globalVar.adslMib.adslStat.bitswapStat.xmtCntReq++;
            break;
        case kG992AocBitswapRxStarted:
            __SoftDslPrintf(gDslVars, "kG992AocBitswapRxStarted rcvCntReq=%d",0,globalVar.adslMib.adslStat.bitswapStat.rcvCntReq);
            globalVar.adslMib.adslStat.bitswapStat.rcvCntReq++;
            break;
        case kG992AocBitswapTxCompleted:
            __SoftDslPrintf(gDslVars, "kG992AocBitswapTxCompleted xmtCnt=%d",0,globalVar.adslMib.adslStat.bitswapStat.xmtCnt);
            globalVar.adslMib.adslStat.bitswapStat.xmtCnt++;
            break;
        case kG992AocBitswapRxCompleted:
            __SoftDslPrintf(gDslVars, "kG992AocBitswapRxCompleted rcvCnt=%d",0,globalVar.adslMib.adslStat.bitswapStat.rcvCnt);
            globalVar.adslMib.adslStat.bitswapStat.rcvCnt++;
            break;
        case kG992AocBitswapTxDenied:
            if(0 == val) {  /* 0 - rejected */
                __SoftDslPrintf(gDslVars, "kG992AocBitswapTxDenied xmtCntRej=%d",0,globalVar.adslMib.adslStat.bitswapStat.xmtCntRej);
                globalVar.adslMib.adslStat.bitswapStat.xmtCntRej++;
            }
            break;
        case kG992AocBitswapRxDenied:
            if(0 == val) {
                __SoftDslPrintf(gDslVars, "kG992AocBitswapRxDenied rcvCntRej=%d",0,globalVar.adslMib.adslStat.bitswapStat.rcvCntRej);
                globalVar.adslMib.adslStat.bitswapStat.rcvCntRej++;
            }
            break;
#endif
        case kDslChannelResponseLog:
            n = val << 1;
            if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
                nb = MAX_TONE_NUM/2;
            else
                nb = MAX_TONE_NUM;
            nb = nb * sizeof(globalVar.chanCharLog[0]);
            if (n > nb)
                n = nb;
            AdslMibByteMove(n, status->param.dslConnectInfo.buffPtr, globalVar.chanCharLog);
            break;

        case kDslChannelResponseLinear:
            n = val << 1;
            if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
                nb = MAX_TONE_NUM/2;
            else
                nb = MAX_TONE_NUM;
            nb = nb * sizeof(globalVar.chanCharLin[0]);
            if (n > nb)
                n = nb;
            AdslMibByteMove(n, status->param.dslConnectInfo.buffPtr, globalVar.chanCharLin);
            break;

        case kDslChannelQuietLineNoise:
            {
            int     i;
            uchar   *pNoiseBuf;

            n = (val < globalVar.nTones ? val : globalVar.nTones);
            pNoiseBuf = (uchar *) status->param.dslConnectInfo.buffPtr;

            for (i = 0; i < n; i++)
                globalVar.quietLineNoise[i] = (-23 << 4) - (pNoiseBuf[i] << 3);
            }
            break;

        case kDslNLMaxCritNoise:
            globalVar.adslMib.adslNonLinData.maxCriticalDistNoise= val;
            break;

        case kDslNLAffectedBits:
            globalVar.adslMib.adslNonLinData.distAffectedBits=val;
            break;

        case kDslNLAffectedBins:
        		globalVar.adslMib.adslNonLinData.NonLinNumAffectedBins=val;
        		if(val>=globalVar.adslMib.adslNonLinData.NonLinThldNumAffectedBins)
        			globalVar.adslMib.adslNonLinData.NonLinearityFlag=1;
        		else globalVar.adslMib.adslNonLinData.NonLinearityFlag=0;
        		break;
        		
        case    kDslATURXmtPowerInfo:
            globalVar.adslMib.adslPhys.adslCurrOutputPwr = Q4ToTenth(val);
            AdslMibUpdateACTPSD(gDslVars, val, US_DIRECTION);
            break;

        case    kDslATUCXmtPowerInfo:
#ifndef USE_LOCAL_DS_POWER
            if(!AdslMibIsLinkActive(gDslVars) || !XdslMibIsXdsl2Mod(gDslVars)) {
                globalVar.adslMib.adslAtucPhys.adslCurrOutputPwr = Q4ToTenth(val);
                AdslMibUpdateACTPSD(gDslVars, val, DS_DIRECTION);
            }
#else
            globalVar.adslMib.adslAtucPhys.adslCurrOutputPwr = Q4ToTenth(val);
            AdslMibUpdateACTPSD(gDslVars, val, DS_DIRECTION);
#endif
            break;
#ifdef USE_LOCAL_DS_POWER
        case kDslATUCShowtimeXmtPowerInfo:
            globalVar.adslMib.adslAtucPhys.adslCurrOutputPwr = Q4ToTenth(val);
            AdslMibUpdateACTPSD(gDslVars, val, DS_DIRECTION);
            break;
#endif
        case    kDslFramingModeInfo:
            globalVar.adslMib.adslFramingMode = 
                (globalVar.adslMib.adslFramingMode & ~kAdslFramingModeMask) | (val & kAdslFramingModeMask);
            break;

        case    kDslATUHardwareAGCObtained:
            globalVar.adslMib.afeRxPgaGainQ1 = val >> 3;
            break;

        default:
            break;
    }
    break;  /* kDslConnectInfoStatus */
    
    case kDslShowtimeSNRMarginInfo:
       if (status->param.dslShowtimeSNRMarginInfo.avgSNRMargin >= globalVar.showtimeMarginThld)
           globalVar.adslMib.adslPhys.adslCurrStatus &= ~kAdslPhysStatusLOM;
       else if (0 == (globalVar.adslMib.adslPhys.adslCurrStatus & kAdslPhysStatusLOS))
           globalVar.adslMib.adslPhys.adslCurrStatus |= kAdslPhysStatusLOM;
       globalVar.adslMib.adslPhys.adslCurrSnrMgn = Q4ToTenth(status->param.dslShowtimeSNRMarginInfo.avgSNRMargin);
#ifdef G993
      if (AdslMibGetModulationType(gDslVars)==kVdslModVdsl2)
      {
           int k,n,i;short *SnrBuff;
           SnrBuff=(short *)status->param.dslShowtimeSNRMarginInfo.buffPtr; 
           k=0;
           for(n=0;n<globalVar.adslMib.dsPhyBandPlan.noOfToneGroups;n++)
           {
               if(globalVar.adslMib.dsPhyBandPlan.toneGroups[n].endTone < globalVar.nTones)
                   for(i=globalVar.adslMib.dsPhyBandPlan.toneGroups[n].startTone;i<=globalVar.adslMib.dsPhyBandPlan.toneGroups[n].endTone;i++)
                       globalVar.showtimeMargin[i]=SnrBuff[k++];
           }
      }
      else
#endif
      {
         val = status->param.dslShowtimeSNRMarginInfo.nCarriers;
         n = kAdslMibToneNum;
#ifdef G992_ANNEXC
         if (kAdslModAnnexI == AdslMibGetModulationType(gDslVars))
             n = kAdslMibToneNum << 1;
#endif
#ifdef G992P5
         if (kAdslModAdsl2p == AdslMibGetModulationType(gDslVars))
             n = kAdslMibToneNum << 1;
#endif
         if (val > n)
             val = n;
         if (val != 0)
             AdslMibByteMove(
                 val * sizeof(globalVar.showtimeMargin[0]),
                 status->param.dslShowtimeSNRMarginInfo.buffPtr, 
                 globalVar.showtimeMargin);
      }
      break;
    
    case kDslEscapeToG994p1Status:
      
      if(globalVar.adslMib.adslTrainingState != kAdslTrainingIdle) {
         if(kXdslLastInitStateStart == globalVar.lastInitState)
            globalVar.adslMib.xdslInitializationCause = kXdslInitConfigCommProblem;
      }
      globalVar.lastInitState = kXdslLastInitStateStart;

      globalVar.nTones=PHYSupportedNTones(gDslVars);
      switch (globalVar.adslMib.adslTrainingState) {
        case kAdslTrainingConnected:
          IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslRetr);
          n = globalVar.adslMib.adslPhys.adslCurrStatus;
          if (n & kAdslPhysStatusLOS)
          {
			  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslRetrLos);
          }
          else if (n & kAdslPhysStatusLOF)
          {
			  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslRetrLof);
          }
          else if (n & kAdslPhysStatusLOM)
          {
			  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslRetrLom);
          }
          else if (n & kAdslPhysStatusLPR)
          {
			  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslRetrLpr);
          }
          break;

        case kAdslTrainingG992Started:
        case kAdslTrainingG992ChanAnalysis:
        case kAdslTrainingG992Exchange:
        case kAdslTrainingG993Started:
        case kAdslTrainingG993ChanAnalysis:
        case kAdslTrainingG993Exchange:
		  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslInitErr);
          break;
        case kAdslTrainingG994:
		  IncFailureCounterVar(&globalVar.adslMib.adslPerfData, adslLineSearch);
          break;  
      }
      if (0 == globalVar.timeConStarted)
          globalVar.timeConStarted = globalVar.timeSec;
      globalVar.adslMib.adslTrainingState = kAdslTrainingIdle;
      globalVar.adslMib.adslRxNonStdFramingAdjustK = 0;
      globalVar.rsOptionValid = false;
      globalVar.rsOption[0] = 0;

      globalVar.nMsgCnt = 0;
      n = globalVar.linkStatus;
      globalVar.linkStatus = 0;
      globalVar.pathActive = 0;
      if (n != 0)
          AdslMibNotify(gDslVars, kAdslEventLinkChange);
      break; /* kDslEscapeToG994p1Status */

    case kDslOLRBitGainUpdateStatus:
        {
        void        *p = status->param.dslOLRRequest.carrParamPtr;
        Boolean     bAdsl2 = (status->param.dslOLRRequest.msgType >= kOLRRequestType4);

        for (n = 0; n < status->param.dslOLRRequest.nCarrs; n++)
            p = AdslMibUpdateBitGain(gDslVars, p, bAdsl2);  
        }   
        break;

    case kAtmStatus:
      pAtmData = (kAdslIntlChannel == AdslMibGetActiveChannel(gDslVars) ?
                  &globalVar.adslMib.adslChanIntlAtmPhyData : &globalVar.adslMib.adslChanFastAtmPhyData);
      switch (status->param.atmStatus.code) {
          case kAtmStatRxHunt:
          case kAtmStatRxPreSync:
              if (kAtmPhyStateNoAlarm == pAtmData->atmInterfaceTCAlarmState)
                  pAtmData->atmInterfaceOCDEvents++;
              pAtmData->atmInterfaceTCAlarmState = kAtmPhyStateLcdFailure;
              break;
          case kAtmStatRxSync:
              pAtmData->atmInterfaceTCAlarmState = kAtmPhyStateNoAlarm;
              break;
          case kAtmStatBertResult:
              globalVar.adslMib.adslBertRes.bertTotalBits = 
                  status->param.atmStatus.param.bertInfo.totalBits;
              globalVar.adslMib.adslBertRes.bertErrBits = 
                  status->param.atmStatus.param.bertInfo.errBits;
              break;
          case kAtmStatHdrCompr:
              if (status->param.atmStatus.param.value)
                  globalVar.adslMib.adslFramingMode |= kAtmHeaderCompression;
              else
                  globalVar.adslMib.adslFramingMode &= ~kAtmHeaderCompression;
              break;
          case kAtmStatCounters:
              {
              int n;
              atmPhyCounters  *p = (void *) status->param.atmStatus.param.value;
              pathId = XdslMibGetCurrentMappedPathId(gDslVars, DS_DIRECTION);

              n = p->rxCellTotal - globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellTotalRcved];
			  IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntCellTotal, n);
              n = p->rxCellData - globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellDataRcved];
			  IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntCellData, n);
              n = p->rxCellDrop - globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellDropRcved];
			  IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntCellDrop, n);
              n = p->bertBitErrors - globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfBitErrsRcved];
			  IncAtmRcvCounterVar(&globalVar.adslMib, pathId, cntBitErrs, n);
              globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellTotalRcved]= p->rxCellTotal;
              globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellDataRcved]= p->rxCellData;
              globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfCellDropRcved]= p->rxCellDrop;
              globalVar.shtExtCounters2lp[pathId][kG992ShowtimeNumOfBitErrsRcved]= p->bertBitErrors;
              }
              break;

          default:
              break;
      }
      break; /* kAtmStatus */
    case kDslReceivedEocCommand:
      switch(status->param.dslClearEocMsg.msgId) {
        case kGinpMonitoringCounters:
        {
          xdslGinpStat *pGinpStat = &globalVar.adslMib.adslStat.ginpStat;
          GinpCounters *pGinpCounter = (GinpCounters *)status->param.dslConnectInfo.buffPtr;
          n = status->param.dslClearEocMsg.msgType & 0xFFFF;
          pGinpStat->cntUS.rtx_tx += (pGinpCounter->rtx_tx -globalVar.ginpCounters.rtx_tx);
          pGinpStat->cntDS.rtx_c += (pGinpCounter->rtx_c- globalVar.ginpCounters.rtx_c);
          pGinpStat->cntDS.rtx_uc += (pGinpCounter->rtx_uc - globalVar.ginpCounters.rtx_uc);
          pGinpStat->cntDS.LEFTRS += pGinpCounter->LEFTRS;
          pGinpStat->cntDS.errFreeBits += (pGinpCounter->errFreeBits - globalVar.ginpCounters.errFreeBits);
          if(globalVar.minEFTRReported) {
            pGinpStat->cntDS.minEFTR = pGinpCounter->minEFTR;
            globalVar.minEFTRReported = false;
          }
          else
            pGinpStat->cntDS.minEFTR = MIN(pGinpStat->cntDS.minEFTR, pGinpCounter->minEFTR);
          globalVar.ginpCounters.rtx_tx = pGinpCounter->rtx_tx;
          globalVar.ginpCounters.rtx_c = pGinpCounter->rtx_c;
          globalVar.ginpCounters.rtx_uc = pGinpCounter->rtx_uc;
          globalVar.ginpCounters.errFreeBits = pGinpCounter->errFreeBits;
          if((n > sizeof(ginpCounters)) && (0 != pGinpCounter->SEFTR))
            AdslMibES(globalVar.currSecondSES, adslSES);
          break;
        }
        case kDsl993p2FramerDeframerUs:
        case kDsl993p2FramerDeframerDs:
        {
          int dir;
          Boolean *pSwapPath;
          long msgId = status->param.dslClearEocMsg.msgId;
          FramerDeframerOptions *pFramerParam = (FramerDeframerOptions *) status->param.dslClearEocMsg.dataPtr;
          
          if(kDsl993p2FramerDeframerUs == msgId) {
            pSwapPath = &globalVar.swapTxPath;
            dir = US_DIRECTION;
          }
          else {
            dir = DS_DIRECTION;
            pSwapPath = &globalVar.swapRxPath;
         }
          n = status->param.dslClearEocMsg.msgType & 0xFFFF;
          pathId = (pFramerParam->path < MAX_LP_NUM) ? pFramerParam->path: 0;
          if((n >= GINP_FRAMER_STRUCT_SIZE) && (0 == pathId) && ((uchar)-1 == pFramerParam->ahifChanId[0]))
            *pSwapPath = true;
          
          Xdsl2MibSetConnectionInfo(gDslVars, pFramerParam, dir, n);
          if(*pSwapPath)
            pathId ^= 1;
          Xdsl2MibConvertConnectionInfo(gDslVars, pathId, dir);
 #ifdef CONFIG_VDSL_SUPPORTED
          if(XdslMibIsVdsl2Mod(gDslVars))
              AdslMibCalcVdslPERp(gDslVars, pathId, dir);
          else
 #endif
              AdslMibCalcAdslPERp(gDslVars, pathId, dir);
          break;
        }
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
        case kDsl993p2FramerAdslUs:
        case kDsl993p2FramerAdslDs:
        {
          adsl2ChanInfo *pChanInfo;
          FramerDeframerOptions *pFramerParam = (FramerDeframerOptions *) status->param.dslClearEocMsg.dataPtr;
          long msgId = status->param.dslClearEocMsg.msgId;
          pathId = (pFramerParam->path < MAX_LP_NUM) ? pFramerParam->path: 0;

          if(kDsl993p2FramerAdslUs == msgId)
              pChanInfo = &globalVar.adslMib.adsl2Info2lp[pathId].xmtChanInfo;
          else
              pChanInfo = &globalVar.adslMib.adsl2Info2lp[pathId].rcvChanInfo;
          pChanInfo->ahifChanId = pFramerParam->ahifChanId[0];
          pChanInfo->connectionType = pFramerParam->tmType[0];
          break;
        }
#ifdef NTR_SUPPORT
      case kDslNtrCounters:
      {
         dslNtrData* pNtr =(dslNtrData*) status->param.dslClearEocMsg.dataPtr;
         int   ntrCntLen = status->param.dslClearEocMsg.msgType & 0xFFFF;

         if (ntrCntLen > sizeof(dslNtrData))
            ntrCntLen = sizeof(dslNtrData);
         AdslMibByteMove (ntrCntLen, pNtr, &globalVar.adslMib.ntrCnt);
      }
         break;
#endif
#ifdef CONFIG_VDSL_SUPPORTED
      case kDsl993p2BandPlanDsDump:
      {
        unsigned short *pBuf;
        Bp993p2 *bp;
        if(globalVar.waitBandPlan)
        {
          int i;
          char vendorId[kAdslPhysVendorIdLen];
          char adslVersionNumber[kAdslPhysVersionNumLen];

          ulong ldStatus = globalVar.adslMib.xdslPhys.adslLDCompleted;
          if( 2 != ldStatus )
            globalVar.adslMib.xdslPhys.adslLDCompleted = ldStatus;
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.usNegBandPlan);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.dsNegBandPlan);
          BlockByteClear(sizeof(bandPlanDescriptor32), (void*)&globalVar.adslMib.usNegBandPlan32);
          BlockByteClear(sizeof(bandPlanDescriptor32), (void*)&globalVar.adslMib.dsNegBandPlan32);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.usPhyBandPlan);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.dsPhyBandPlan);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.usNegBandPlanDiscovery);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.dsNegBandPlanDiscovery);
          BlockByteClear(sizeof(bandPlanDescriptor32), (void*)&globalVar.adslMib.usNegBandPlanDiscovery32);
          BlockByteClear(sizeof(bandPlanDescriptor32), (void*)&globalVar.adslMib.dsNegBandPlanDiscovery32);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.usPhyBandPlanDiscovery);
          BlockByteClear(sizeof(bandPlanDescriptor), (void*)&globalVar.adslMib.dsPhyBandPlanDiscovery);
          BlockByteMove(kAdslPhysVendorIdLen, &globalVar.adslMib.xdslAtucPhys.adslVendorID[0], vendorId);
          BlockByteMove(kAdslPhysVersionNumLen, &globalVar.adslMib.xdslAtucPhys.adslVersionNumber[0], adslVersionNumber);
          BlockByteClear(sizeof(globalVar.adslMib.xdslAtucPhys), (void*)&globalVar.adslMib.xdslAtucPhys);
          BlockByteMove(kAdslPhysVendorIdLen, vendorId, &globalVar.adslMib.xdslAtucPhys.adslVendorID[0]);
          BlockByteMove(kAdslPhysVersionNumLen, adslVersionNumber, &globalVar.adslMib.xdslAtucPhys.adslVersionNumber[0]);

          BlockByteClear(sizeof(globalVar.adslMib.vdslDiag), (void*)&globalVar.adslMib.vdslDiag[0]);
          BlockByteClear(sizeof(globalVar.adslMib.perbandDataUs), (void*)&globalVar.adslMib.perbandDataUs[0]);
          BlockByteClear(sizeof(globalVar.adslMib.perbandDataDs), (void*)&globalVar.adslMib.perbandDataDs[0]);
          if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
            i = MAX_TONE_NUM/2;
          else
            i = MAX_TONE_NUM;
          BlockByteClear(i, (void*)globalVar.snr);
          BlockByteClear(i, (void*)globalVar.showtimeMargin);
          BlockByteClear(i, (void*)globalVar.bitAlloc);
          BlockByteClear(i, (void*)globalVar.gain);
          BlockByteClear(i, (void*)globalVar.chanCharLin);
          for(i=0; i<globalVar.nTones; i++)
          {
            globalVar.quietLineNoise[i]=(-160)<<4;
            globalVar.chanCharLog[i]=(-96)<<4;
          }
          globalVar.waitfirstQLNstatusLD=1;
          globalVar.waitfirstHLOGstatusLD=1;
          globalVar.waitfirstSNRstatusLD=1;
          globalVar.waitBandPlan=0;
        }
        pBuf =(unsigned short *)(status->param.dslClearEocMsg.dataPtr); 
        bp=(Bp993p2*)pBuf;
        if(globalVar.bandPlanType==MedleyPhase)
        {
          if(bp->reserved==PhyBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.dsPhyBandPlan), bp, &globalVar.adslMib.dsPhyBandPlan);
            globalVar.bpSNR=&globalVar.adslMib.dsPhyBandPlan;
            globalVar.waitfirstSNRstatusMedley=1;
          }
          else if (bp->reserved==NegBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.dsNegBandPlan32), bp, &globalVar.adslMib.dsNegBandPlan32);
            globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds=calcOamGfactor(bp);
            __SoftDslPrintf(gDslVars, "Gfactor_MEDLEYSETds=%d",0,globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds);
          }
          if (globalVar.adslMib.dsPhyBandPlan.noOfToneGroups>0 && globalVar.adslMib.dsNegBandPlan32.noOfToneGroups>0)
            CreateNegBandPlan(gDslVars,&globalVar.adslMib.dsNegBandPlan, &globalVar.adslMib.dsNegBandPlan32, &globalVar.adslMib.dsPhyBandPlan);
        }
        else
        {
          if(bp->reserved==PhyBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.dsPhyBandPlanDiscovery), bp, &globalVar.adslMib.dsPhyBandPlanDiscovery);
            globalVar.bpSNR=&globalVar.adslMib.dsPhyBandPlanDiscovery;
          }
          else if (bp->reserved==NegBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.dsNegBandPlanDiscovery32), bp, &globalVar.adslMib.dsNegBandPlanDiscovery32);
            globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds=calcOamGfactor(bp);
            __SoftDslPrintf(gDslVars, "Gfactor_SUPPORTERCARRIERSds=%d",0,globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds);
          }
          if (globalVar.adslMib.dsPhyBandPlanDiscovery.noOfToneGroups>0 && globalVar.adslMib.dsNegBandPlanDiscovery32.noOfToneGroups>0)
            CreateNegBandPlan(gDslVars,&globalVar.adslMib.dsNegBandPlanDiscovery, &globalVar.adslMib.dsNegBandPlanDiscovery32, &globalVar.adslMib.dsPhyBandPlanDiscovery);
          }
      }
        break;
      case kDsl993p2BandPlanUsDump:
      {
        unsigned short *pBuf;
        Bp993p2 *bp;
        pBuf =(unsigned short *)(status->param.dslClearEocMsg.dataPtr); 
        bp=(Bp993p2*)pBuf;
        if(globalVar.bandPlanType==MedleyPhase)
        {
          if(bp->reserved==PhyBandPlan)
            AdslMibByteMove(sizeof(globalVar.adslMib.usPhyBandPlan), bp, &globalVar.adslMib.usPhyBandPlan);
          else if (bp->reserved==NegBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.usNegBandPlan32), bp, &globalVar.adslMib.usNegBandPlan32);
            globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus=calcOamGfactor(bp);
            __SoftDslPrintf(gDslVars, "Gfactor_MEDLEYSETus=%d",0,globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus);
          }
          if (globalVar.adslMib.usPhyBandPlan.noOfToneGroups>0 && globalVar.adslMib.usNegBandPlan32.noOfToneGroups>0)
            CreateNegBandPlan(gDslVars,&globalVar.adslMib.usNegBandPlan, &globalVar.adslMib.usNegBandPlan32, &globalVar.adslMib.usPhyBandPlan);
        }
        else
        {
          if(bp->reserved==PhyBandPlan)
            AdslMibByteMove(sizeof(globalVar.adslMib.usPhyBandPlanDiscovery), bp, &globalVar.adslMib.usPhyBandPlanDiscovery);
          else if (bp->reserved==NegBandPlan)
          {
            AdslMibByteMove(sizeof(globalVar.adslMib.usNegBandPlanDiscovery32), bp, &globalVar.adslMib.usNegBandPlanDiscovery32);
            globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus=calcOamGfactor(bp);
            __SoftDslPrintf(gDslVars, "Gfactor_SUPPORTERCARRIERSus=%d",0,globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus);
          }
          if (globalVar.adslMib.usPhyBandPlanDiscovery.noOfToneGroups>0 && globalVar.adslMib.usNegBandPlanDiscovery32.noOfToneGroups>0)
            CreateNegBandPlan(gDslVars,&globalVar.adslMib.usNegBandPlanDiscovery, &globalVar.adslMib.usNegBandPlanDiscovery32, &globalVar.adslMib.usPhyBandPlanDiscovery);
        }
      }
        break;
      case kDsl993p2QlnRaw:
      {
        int k,n,i,j;short *QLNBuff;
        QLNBuff=(short *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.dsBpQLNForReport=&globalVar.adslMib.dsPhyBandPlanDiscovery;
        k=0;
        if(globalVar.waitfirstQLNstatusLD==1)
            globalVar.waitfirstQLNstatusLD=0;
        if (globalVar.adslMib.xdslInfo.vdsl2Profile!=kVdslProfile30a)
            j = 582;
        else
            j = 630;
        for(n=0;n<globalVar.adslMib.dsPhyBandPlanDiscovery.noOfToneGroups;n++)
        {
            if(globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].endTone < globalVar.nTones)
                for(i=globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].startTone;i<=globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].endTone;i++)
                    globalVar.quietLineNoise[i]=(short)(QLNBuff[k++]/16-j); //convert dBm per tone to dBm per Hz
        }
      }
        break;
      case kDsl993p2HlogRaw:
      {
          int k,n,i;short *HlogBuff;
          HlogBuff=(short *)(status->param.dslClearEocMsg.dataPtr); 
          k=0;
          globalVar.dsBpHlogForReport=&globalVar.adslMib.dsPhyBandPlanDiscovery;
          for(n=0;n<globalVar.adslMib.dsPhyBandPlanDiscovery.noOfToneGroups;n++)
            if(globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].endTone < globalVar.nTones)
                for(i=globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].startTone;i<=globalVar.adslMib.dsPhyBandPlanDiscovery.toneGroups[n].endTone;i++)
                  globalVar.chanCharLog[i]=HlogBuff[k++]/16;
      }
        break;
      case kDsl993p2SnrRaw:
      {
          int k,n,i;short *SnrBuff;
          globalVar.dsBpSNRForReport=globalVar.bpSNR;
          if(globalVar.waitfirstSNRstatusMedley==1)
          {
            if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
                n = MAX_TONE_NUM/2;
            else
                n = MAX_TONE_NUM;
            BlockByteClear(n, (void*)globalVar.snr);
            globalVar.waitfirstSNRstatusMedley=0;
          }
          SnrBuff=(short *)(status->param.dslClearEocMsg.dataPtr);
          k=0;
          for(n=0;n<globalVar.bpSNR->noOfToneGroups;n++)
          {
            if(globalVar.bpSNR->toneGroups[n].endTone < globalVar.nTones)
                for(i=globalVar.bpSNR->toneGroups[n].startTone;i<=globalVar.bpSNR->toneGroups[n].endTone;i++)
                  globalVar.snr[i]=SnrBuff[k++]/16;
          }
      }
        break;
      case kDsl993p2NeBi:
      {
            int i,k,n; uchar *pBuf;
            pBuf =(uchar *)(status->param.dslClearEocMsg.dataPtr); 
            k=0;
            for(n=0;n<globalVar.adslMib.dsNegBandPlan32.noOfToneGroups;n++)
            {
                if(globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                    for(i=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone;i++)
                        if(pBuf[k]== 0xFF)
                        {
                            globalVar.bitAlloc[i]=0;
                            k++;
                        }
                        else
                            globalVar.bitAlloc[i]=pBuf[k++];
            }
      }
        break;
      case kDsl993p2FeBi:
      {
        int i,k,n; uchar *pBuf;
        pBuf =(uchar *)(status->param.dslClearEocMsg.dataPtr); 
        k=0;
        for(n=0;n<globalVar.adslMib.usNegBandPlan32.noOfToneGroups;n++)
        {
            if(globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
              for(i=globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone;i++)
              {
                  if(pBuf[k]==0xFF)
                  {
                      globalVar.bitAlloc[i]=0;
                      k++;
                  }
                  else
                    globalVar.bitAlloc[i]=pBuf[k++];
              }
        }
      }
        break;
      case kDsl993p2NeGi:
      {
        int k,n,i;short *GiBuff;
        GiBuff=(short *)(status->param.dslClearEocMsg.dataPtr); 
        k=0;
        if( XdslMibIsVdsl2Mod(gDslVars) ) {
            for(n=0;n<globalVar.adslMib.dsNegBandPlan32.noOfToneGroups;n++) {
                if(globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                    for(i=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone;i++)
                      globalVar.gain[i]=GiBuff[k++]/16;
            }
        }
        else {
            int msgLen = (status->param.dslClearEocMsg.msgType & 0xFFFF) / sizeof(short);
            if(msgLen > globalVar.nTones)
                msgLen = globalVar.nTones;
            if (AdslMibTone32_64(gDslVars))
                n=64;
            else n=32;
            for(;n<msgLen;n++)
                globalVar.gain[n]=GiBuff[n]/16;
        }
      }
      break;
      case kDsl993p2FeGi :
      {
        int k,n,i; short tmp, temp;
        short sub=UtilQ0LinearToQ4dB(512);
        k=0;i=0;
        for(n=0;n<globalVar.adslMib.usNegBandPlan32.noOfToneGroups;n++)
        {
            if(globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
            {
                k=globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone-globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone+1;
                AdslMibByteMove(2*k,status->param.dslClearEocMsg.dataPtr+2*i,&globalVar.gain[globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone]);
                i+=k;
            }
        }
        k=0;
        for(n=0;n<globalVar.adslMib.usNegBandPlan32.noOfToneGroups;n++)
        {
            if(globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
            {
                for(i=globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone;i++)
                {
                    tmp=globalVar.gain[i];
                    temp=UtilQ0LinearToQ4dB(tmp)-sub;
                    temp*=2;
                    globalVar.gain[i]=((short)(temp));
                }
            }
        }
      }
        break;
      case kDsl993p2SNRM:
      {
        short* buff;short val;
        buff=(short *)(status->param.dslClearEocMsg.dataPtr);
        val = Q4ToTenth(buff[0]);
        globalVar.adslMib.adslPhys.adslCurrSnrMgn= val;
#if 0
        __SoftDslPrintf(gDslVars, " Avg SNRM =%d",0,val);
#endif
      }
        break;
      case kDsl993p2SNRMpb:
      {
        short *buff;int n;short val;
        buff=(short *)(status->param.dslClearEocMsg.dataPtr);
        for(n=0;n<globalVar.adslMib.dsNegBandPlan.noOfToneGroups;n++)
        {
          val=Q4ToTenth(buff[n]);
          globalVar.adslMib.perbandDataDs[n].adslCurrSnrMgn=val;
#if 0      
          __SoftDslPrintf(gDslVars, " PB SNRMPB =%d",0,val);
#endif
        }
      }
        break;
      case kDsl993p2SATNpbRaw:
      {
        short *buff;int n;short val;
        buff=(short *)(status->param.dslClearEocMsg.dataPtr);
        for(n=0;n<globalVar.adslMib.dsNegBandPlan.noOfToneGroups;n++)
        {
          val=Q8ToTenth(buff[n]);
          globalVar.adslMib.perbandDataDs[n].adslSignalAttn=val;
#if 0
          __SoftDslPrintf(gDslVars, " PB SATN Attn =%d",0,val);
#endif
        }
      }
        break;
      case kDsl993p2LnAttnRaw:
      {
          short *buff;int n;short val;
        buff=(short *)(status->param.dslClearEocMsg.dataPtr);
         for(n=0;n<globalVar.adslMib.dsPhyBandPlanDiscovery.noOfToneGroups;n++)
        {
          val=Q8ToTenth(buff[n]);
          globalVar.adslMib.perbandDataDs[n].adslCurrAtn=val;
          globalVar.adslMib.perbandDataDs[n].adslSignalAttn=val;
#if 0
          __SoftDslPrintf(gDslVars, " PB LATN Attn =%d",0,val);
#endif
        }
      }
        break;
      case kDsl993p2dsATTNDR:
      {
        long *val;
        val=(long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.adslPhys.adslCurrAttainableRate=val[0]*1000;
#if 0
        __SoftDslPrintf(gDslVars, " DS Attn Data Rate=%u",0,globalVar.adslMib.adslPhys.adslCurrAttainableRate);
#endif
      }
        break;
      case kDsl993p2BpType:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.bandPlanType= val[0];
#if 0
        __SoftDslPrintf(gDslVars, "BandPlanType Received=%u",bandPlanType);
#endif
        break;
      }
#if 0
      case kDsl993p2FramerDeframerUs:
      case kDsl993p2FramerDeframerDs:
      {
//        vdsl2ConnectionInfo   *pVdslInfo;
        long msgId = status->param.dslClearEocMsg.msgId;
        FramerDeframerOptions *pFramerParam = (FramerDeframerOptions *) status->param.dslClearEocMsg.dataPtr;
        
        n = status->param.dslClearEocMsg.msgType & 0xFFFF;
        pathId = (pFramerParam->path < MAX_LP_NUM) ? pFramerParam->path: 0;
//        pVdslInfo = &globalVar.adslMib.vdslInfo[pathId];
//        Vdsl2MibSetConnectionInfo(gDslVars, pFramerParam, msgId, pVdslInfo);
        Xdsl2MibSetConnectionInfo(gDslVars, pFramerParam, msgId);
        nb = (kDsl993p2FramerDeframerUs == msgId) ? kGinpUsEnabled: kGinpDsEnabled;
        if((n >= sizeof(FramerDeframerOptions)) && pathId) {
            if(0 != (pFramerParam->ginpFraming&7)) {
                globalVar.adslMib.xdslStat[0].ginpStat.status |= (ulong)nb;
                // TO DO: extract G.inp framing parameters
            }
            else {
                globalVar.adslMib.xdslStat[0].ginpStat.status &= ~((ulong)nb);
                // TO DO: clear G.inp framing parameters
            }
        }
        if(XdslMibIsDataCarryingPath(gDslVars, (unsigned char)pathId,
            (kDsl993p2FramerDeframerUs == msgId) ? US_DIRECTION: DS_DIRECTION)) {
#ifdef CONFIG_VDSL_SUPPORTED
            if(XdslMibIsVdsl2Mod(gDslVars))
              AdslMibCalcVdslPERp(gDslVars, pathId, msgId);
            else
#endif
              AdslMibCalcAdslPERp(gDslVars, pathId, msgId);
        }
        break;
      }
#endif
      case kDsl993p2MaxRate:
      {
        ulong *pUlong = (ulong *) status->param.dslClearEocMsg.dataPtr;
        
        globalVar.adslMib.adslPhys.adslCurrAttainableRate = ActualRate(*pUlong) * 1000;
        globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate = ActualRate(*(pUlong+1)) * 1000;
      }
        break;
      case kDsl993p2PowerNeTxTot:
      {
        short *val =(unsigned short *)(status->param.dslClearEocMsg.dataPtr);
        val[0] = Q8ToTenth(val[0]);
        globalVar.adslMib.xdslPhys.adslCurrOutputPwr=val[0];
#if 0
        __SoftDslPrintf(gDslVars, " Ne Tx Tot =%d",0,val[0]);
#endif
      }
      break;
      case kDsl993p2PowerFeTxTot:
      {
      	short *val =(unsigned short *)(status->param.dslClearEocMsg.dataPtr);
      	val[0] = Q8ToTenth(val[0]);
      	globalVar.adslMib.xdslAtucPhys.adslCurrOutputPwr=val[0];
#if 0
        __SoftDslPrintf(gDslVars, " Fe Tx Tot =%d",0,val[0]);
#endif
      }
        break;
      case kDslActualPSDUs:
      {
        long *val =(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        if (val[0]<0)
        val[0] = -Q8ToTenth(-val[0]);
        else 
        val[0] = Q8ToTenth(val[0]);
        globalVar.adslMib.xdslPhys.actualPSD=val[0];
#if 0
        __SoftDslPrintf(gDslVars, " US Actual PSD =%d",0,val[0]);
#endif
      }
        break;
      case kDslActualPSDDs:
      {
        long *val =(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        if (val[0]<0)
          val[0] = -Q8ToTenth(-val[0]);
        else 
          val[0] = Q8ToTenth(val[0]);
        globalVar.adslMib.xdslAtucPhys.actualPSD=val[0];
#if 0
        __SoftDslPrintf(gDslVars, " DS Actual PSD =%d",0,val[0]);
#endif
      }
        break;
      case kDslSNRModeUs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslPhys.SNRmode= val[0];
#if 0
        __SoftDslPrintf(gDslVars, "SNRModeUs Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslSNRModeDs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslAtucPhys.SNRmode= val[0];
#if 0
        __SoftDslPrintf(gDslVars, "SNRModeDs Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslActualCE:
      {
      	unsigned long *val;
      	val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
      	globalVar.adslMib.xdslPhys.actualCE= val[0];
#if 0
      	__SoftDslPrintf(gDslVars, "ActualCE Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslUPBOkle:
      {
        unsigned short *val;
        val=(unsigned short *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslPhys.UPBOkle= val[0];
#if 0
        __SoftDslPrintf(gDslVars, "UPBOkle Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslQLNmtUs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslAtucPhys.QLNMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "QLNMTUS Received=%u",0,val[0]);
#endif
          break;
      }
      case kDslQLNmtDs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslPhys.QLNMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "QLNMTDS Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslSNRmtUs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslAtucPhys.SNRMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "SNRMTUS Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslSNRmtDs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslPhys.SNRMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "SNRMTDS Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslHLOGmtUs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslAtucPhys.HLOGMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "HLOGMTUS Received=%u",0,val[0]);
#endif
        break;
      }
      case kDslHLOGmtDs:
      {
        unsigned long *val;
        val=(unsigned long *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.xdslPhys.HLOGMT=val[0];
#if 0
        __SoftDslPrintf(gDslVars, "HLOGMTDS Received=%u",0,val[0]);
#endif
        break;
      }
      case kDsl993p2PowerNeTxPb:
      {
        short val;
        short *pBuf =(unsigned short *)(status->param.dslClearEocMsg.dataPtr); 
        for(n=0;n<globalVar.adslMib.usPhyBandPlanDiscovery.noOfToneGroups;n++)
        {
          val=Q8ToTenth(pBuf[n]);
          globalVar.adslMib.xdslPhys.perBandCurrOutputPwr[n]=val;
#if 0
          __SoftDslPrintf(gDslVars, " PB NE Cur Output Pwr =%d",0,val);
#endif
        }
      }
        break;
      case kDsl993p2PowerFeTxPb:
      {
        short val;
        short *pBuf =(unsigned short *)(status->param.dslClearEocMsg.dataPtr); 
        for(n=0;n<globalVar.adslMib.dsPhyBandPlanDiscovery.noOfToneGroups;n++)
        {
          val=Q8ToTenth(pBuf[n]);
          globalVar.adslMib.xdslAtucPhys.perBandCurrOutputPwr[n]=val;
#if 0
          __SoftDslPrintf(gDslVars, " PB FE Cur Output Pwr =%d",0,val);
#endif
        }
      }
        break;
      case kDsl993p2FeQlnLD:
      {
          int n,i,grpIndex;uchar *pMsg;short val=0;
          int g=globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus;
          if(0 == g)
              g=1;
          if(globalVar.waitfirstQLNstatusLD==1)
          {
              for(i=0; i<globalVar.nTones; i++)
                  globalVar.quietLineNoise[i]=(-160)<<4;
              globalVar.waitfirstQLNstatusLD=0;
          }
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
#if 0
          __SoftDslPrintf(gDslVars, "kDsl993p2FeQLNLD Received",0);
#endif
          for(n=0;n<globalVar.adslMib.usNegBandPlanDiscovery32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].endTone;i++)
                  {
                      if(i/g > grpIndex)
                      {
                          grpIndex=i/g;
                          if(pMsg[grpIndex]!=255)
                              val = -(((short) pMsg[grpIndex]) << 3) - 23*16;
                          else 
                              val = -160*16;
                      }
                      globalVar.quietLineNoise[i]=val;
                  }
          }
      }
        break;
      case kDsl993p2FeHlogLD:
      {
          int n,i,grpIndex;uchar *pMsg;short val=0;
          int g=globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSus;
          if(0 == g)
              g=1;
          if(globalVar.waitfirstHLOGstatusLD==1)
          {
              for(i=0;i<globalVar.nTones;i++)
                  globalVar.chanCharLog[i]=(-96)<<4;
              globalVar.waitfirstHLOGstatusLD=0;
          }
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.usNegBandPlanDiscovery32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].endTone< globalVar.nTones)
                  for(i=globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlanDiscovery32.toneGroups[n].endTone;i++)
                  {
                      if(i/g>grpIndex)
                      {
                          grpIndex=i/g;
                          val = ((int) (pMsg[grpIndex*2] & 0x3) << 8) + (pMsg[grpIndex*2+1]);
#if 0
                          if(val==1023)
                              val=(-96)<<4;
                          else
#endif
                              val = -(val*16/10) + 6*16;
                      }
                      globalVar.chanCharLog[i]=val;
                  }
          }
      }
        break;
      case kDsl993p2NeHlogLD:
      {
          int n,i,grpIndex;uchar *pMsg;short val=0;
          int g=globalVar.adslMib.gFactors.Gfactor_SUPPORTERCARRIERSds;
          globalVar.dsBpHlogForReport=&globalVar.adslMib.dsNegBandPlanDiscovery;
          if(0==g)
              g=1;
          if(globalVar.waitfirstHLOGstatusLD==1)
          {
              for(i=0;i<globalVar.nTones;i++)
                  globalVar.chanCharLog[i]=(-96)<<4;
              globalVar.waitfirstHLOGstatusLD=0;
          }
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.dsNegBandPlanDiscovery32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.dsNegBandPlanDiscovery32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.dsNegBandPlanDiscovery32.toneGroups[n].startTone;i<=globalVar.adslMib.dsNegBandPlanDiscovery32.toneGroups[n].endTone;i++)
                  {
                      if(i/g > grpIndex)
                      {
                          grpIndex=i/g;
                          val = ((int) (pMsg[grpIndex*2] & 0x3) << 8) + (pMsg[grpIndex*2+1]);
#if 0
                          if(val==1023)
                              val=(-96)<<4;
                          else
#endif
                              val = -(val*16/10) + 6*16;
                      }
                      globalVar.chanCharLog[i]=val;
                  }
          }
      }
        break;
      case kDsl993p2FePbLatnLD:
      {
          int n;uchar *pMsg;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          if(globalVar.adslMib.usNegBandPlanDiscovery.toneGroups[0].startTone>256)
              pMsg+=2;
          for(n=0;n<globalVar.adslMib.usNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataUs[n].adslCurrAtn=ReadCnt16(pMsg);
              pMsg+=2;
#if 0
          __SoftDslPrintf(gDslVars, "US PB LATN Attn[%d] =%d",0,n,globalVar.adslMib.perbandDataUs[n].adslCurrAtn);
#endif
          }
      }
        break;
      case kDsl993p2NePbLatnLD:
      {
          int n;uchar *pMsg;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr);
          for(n=0;n<globalVar.adslMib.dsNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataDs[n].adslCurrAtn=ReadCnt16(pMsg);
              globalVar.adslMib.vdslDiag[n].loopAttn=globalVar.adslMib.perbandDataDs[n].adslCurrAtn;
              pMsg+=2;
#if 0
              __SoftDslPrintf(gDslVars, "DS PB LATN Attn[%d] =%d",0,n,globalVar.adslMib.perbandDataDs[n].adslCurrAtn);
#endif
          }
      }
        break;
      case kDsl993p2FePbSatnLD:
      {
          int n;uchar *pMsg;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr);
          if(globalVar.adslMib.usNegBandPlanDiscovery.toneGroups[0].startTone>256)
              pMsg+=2;
          for(n=0;n<globalVar.adslMib.usNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataUs[n].adslSignalAttn=ReadCnt16(pMsg);
#if 0
              __SoftDslPrintf(gDslVars, "US PB SATN Attn[%d] =%d",0,n,globalVar.adslMib.perbandDataUs[n].adslSignalAttn);
#endif
              pMsg+=2;
          }
      }
        break;
      case kDsl993p2NePbSatnLD:
      {
          int n;uchar *pMsg;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.dsNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataDs[n].adslSignalAttn=ReadCnt16(pMsg);
              globalVar.adslMib.vdslDiag[n].signalAttn=globalVar.adslMib.perbandDataDs[n].adslSignalAttn;
#if 0
              __SoftDslPrintf(gDslVars, "DS PB SATN Attn[%d] =%d",0,n,globalVar.adslMib.perbandDataDs[n].adslSignalAttn);
#endif
              pMsg+=2;
          }
      }
          break;
      case kDsl993p2FePbSnrLD:
      {
          uchar *pMsg;
          pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
          globalVar.adslMib.adslAtucPhys.adslCurrSnrMgn = ReadCnt16(pMsg);
#if 0
          __SoftDslPrintf(gDslVars, " Avg SNRM =%d",0,globalVar.adslMib.adslAtucPhys.adslCurrSnrMgn);
#endif
          pMsg+=2;
          if(globalVar.adslMib.usNegBandPlanDiscovery.toneGroups[0].startTone>256)
          pMsg+=2;
          for(n=0;n<globalVar.adslMib.usNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataUs[n].adslCurrSnrMgn=ReadCnt16(pMsg);
#if 0
              __SoftDslPrintf(gDslVars, "US PB SNRM [%d] =%d",0,n,globalVar.adslMib.perbandDataUs[n].adslCurrSnrMgn);
#endif
              pMsg+=2;
          }
      }
        break;
      case kDsl993p2NePbSnrLD:
      {
          uchar *pMsg;
          globalVar.dsBpSNRForReport=&globalVar.adslMib.dsNegBandPlanDiscovery;
          pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
          globalVar.adslMib.adslPhys.adslCurrSnrMgn = ReadCnt16(pMsg);
#if 0
          __SoftDslPrintf(gDslVars, " Avg SNRM =%d",0,globalVar.adslMib.adslPhys.adslCurrSnrMgn);
#endif
          pMsg+=2;
          for(n=0;n<globalVar.adslMib.dsNegBandPlanDiscovery.noOfToneGroups;n++)
          {
              globalVar.adslMib.perbandDataDs[n].adslCurrSnrMgn=ReadCnt16(pMsg);
              globalVar.adslMib.vdslDiag[n].snrMargin=globalVar.adslMib.perbandDataDs[n].adslCurrSnrMgn;
#if 0
              __SoftDslPrintf(gDslVars, "DS PB SNRM [%d] =%d",0,n,globalVar.adslMib.perbandDataDs[n].adslCurrSnrMgn);
#endif
              pMsg+=2;
          }
      }
        break;
      case kDsl993p2FeAttnLD:
      {
          uchar *pMsg;
          pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
          globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate=ReadCnt32(pMsg);
#if 0
          __SoftDslPrintf(gDslVars, "US ATTN RATE: %u/1000 kbps", 0,globalVar.adslMib.adslAtucPhys.adslCurrAttainableRate);
#endif
      }
        break;
      case kDsl993p2NeAttnLD:
      {
          uchar *pMsg;
          pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
          globalVar.adslMib.adslPhys.adslCurrAttainableRate=ReadCnt32(pMsg);
#if 0
          __SoftDslPrintf(gDslVars, "DS ATTN RATE: %u/1000 kbps", 0,globalVar.adslMib.adslPhys.adslCurrAttainableRate);
#endif
      }
        break;
      case kDsl993p2FeTxPwrLD:
      {
        uchar *pMsg;
        pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.adslAtucPhys.adslCurrOutputPwr=ReadCnt16(pMsg);
#if 0
        __SoftDslPrintf(gDslVars, "FE TX PWR: %d/10 dBm", 0,globalVar.adslMib.adslAtucPhys.adslCurrOutputPwr);
#endif
      }
        break;
      case kDsl993p2NeTxPwrLD:
      {
        uchar *pMsg;
        pMsg=(uchar *)(status->param.dslClearEocMsg.dataPtr);
        globalVar.adslMib.adslPhys.adslCurrOutputPwr=ReadCnt16(pMsg);
#if 0
        __SoftDslPrintf(gDslVars, "NE TX PWR: %d/10 dBm", 0,globalVar.adslMib.adslPhys.adslCurrOutputPwr);
#endif
      }
        break;
      case kDsl993p2FeSnrLD:
      {
          int n,i,grpIndex;uchar *pMsg;short val=0;
          int g=globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus;
          if(0==g)
              g=1;
          if(globalVar.waitfirstSNRstatusLD==1)
          {
              if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
                  n = MAX_TONE_NUM/2;
              else
                  n = MAX_TONE_NUM;
              BlockByteClear(n, (void*)globalVar.snr);
              globalVar.waitfirstSNRstatusLD=0;
          }
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.usNegBandPlan32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone;i++)
                  {
                      if(i/g>grpIndex)
                      {
                          grpIndex=i/g;
                          if(pMsg[grpIndex]!=255)
                              val= (((short) pMsg[grpIndex]) << 3) - 32*16;
                          else
                              val = 0;
                      }
                      globalVar.snr[i]=val;
                  }
          }
      }
        break;
      case kDsl993p2NeSnrLD:
      {
          int n,i,grpIndex;uchar *pMsg;short val=0;
          int g=globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds;
          if(0==g)
              g=1;
          if(globalVar.waitfirstSNRstatusLD==1)
          {
          if(ADSL_PHY_SUPPORT(kAdslPhyBonding))
              n = MAX_TONE_NUM/2;
          else
              n = MAX_TONE_NUM;
          BlockByteClear(n, (void*)globalVar.snr);
          globalVar.waitfirstSNRstatusLD=0;
          }
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.dsNegBandPlan32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone;i++)
                  {
                      if(i/g>grpIndex)
                      {
                          grpIndex=i/g;
                          if(pMsg[grpIndex]!=255)
                              val= (((short) pMsg[grpIndex]) << 3) - 32*16;
                          else 
                              val = 0;
                      }
                      globalVar.snr[i]=val;
                  }
          }
      }
        break;
      case kDsl993p2NeHlinLD:
      {
          int n,i,k=0,grpIndex;uchar *pMsg;
          int g=globalVar.adslMib.gFactors.Gfactor_MEDLEYSETds;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.dsNegBandPlan32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.dsNegBandPlan32.toneGroups[n].endTone;i++)
                  {
                      if(i/g>grpIndex)
                      {
                          grpIndex=i/g;
                          if(k==0 && i>globalVar.adslMib.dsNegBandPlan32.toneGroups[n].startTone+10)
                          {
                              globalVar.adslMib.adslPhys.adslHlinScaleFactor = ReadCnt16(pMsg+6*grpIndex);
                              globalVar.adslMib.adslDiag.hlinScaleFactor = globalVar.adslMib.adslPhys.adslHlinScaleFactor;
                              k=1;
                          }
                      }
                      globalVar.chanCharLin[i].x = ReadCnt16(pMsg+6*grpIndex+2);
                      globalVar.chanCharLin[i].y = ReadCnt16(pMsg+6*grpIndex+4);
                  }
          }
#if 0
          __SoftDslPrintf(gDslVars, "DS HlinScaleFactor: %d",0,globalVar.adslMib.adslPhys.adslHlinScaleFactor);
#endif
      }
        break;
      case kDsl993p2FeHlinLD:
      {
          int n,i,k=0,grpIndex;uchar *pMsg;
          int g=globalVar.adslMib.gFactors.Gfactor_MEDLEYSETus;
          pMsg=(uchar*)(status->param.dslClearEocMsg.dataPtr); 
          for(n=0;n<globalVar.adslMib.usNegBandPlan32.noOfToneGroups;n++)
          {
              grpIndex=-1;
              if(globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone < globalVar.nTones)
                  for(i=globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone;i<=globalVar.adslMib.usNegBandPlan32.toneGroups[n].endTone;i++)
                  {
                      if(i/g>grpIndex)
                      {
                          grpIndex=i/g;
                          if(k==0 && i>globalVar.adslMib.usNegBandPlan32.toneGroups[n].startTone+10)
                          {
                              globalVar.adslMib.adslAtucPhys.adslHlinScaleFactor = ReadCnt16(pMsg+6*grpIndex);
                              k=1;
                          }
                      }
                      globalVar.chanCharLin[i].x = ReadCnt16(pMsg+6*grpIndex+2);
                      globalVar.chanCharLin[i].y = ReadCnt16(pMsg+6*grpIndex+4);
                  }
          }
#if 0
          __SoftDslPrintf(gDslVars, "US HlinScaleFactor: %d",0,globalVar.adslMib.adslAtucPhys.adslHlinScaleFactor);
#endif
      }
        break;
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
#ifdef G993P5_OVERHEAD_MESSAGING 
      case kVectoringMacAddress:
      {
#if 1
          __SoftDslPrintf(gDslVars, "DRV MIB: kVectoringMacAddress: 0x%x",0,*((unsigned int*)status->param.dslClearEocMsg.dataPtr));
#endif
          BlockByteMove(6,(char *)status->param.dslClearEocMsg.dataPtr,(char *)&globalVar.adslMib.vectData.macAddress.macAddress[0]);
          globalVar.adslMib.vectData.macAddress.addressType=0;
      }
      break;
#endif /* G993P5_OVERHEAD_MESSAGING */
      case kDslVectoringErrorSamples:
      {
        VectorErrorSample *pMsg=(VectorErrorSample*)(status->param.dslClearEocMsg.dataPtr);
        unsigned short writeIdx = globalVar.adslMib.vectData.esCtrl.writeIdx;
        BlockByteMove(sizeof(VectorErrorSample),(char *)pMsg,(char *)&globalVar.vectorErrorSampleArray.samples[writeIdx]);
#ifndef G993P5_OVERHEAD_MESSAGING
        if (globalVar.vectorErrorSampleArray.samples[writeIdx].nbSamples==0) {
          __SoftDslPrintf(gDslVars, "MIB VECT: Errored vectorErrorSample: nbSamples=%d, lineId=%d, syncCounter=%d, startIdx=%d",0, 
          globalVar.vectorErrorSampleArray.samples[writeIdx].nbSamples,
          globalVar.vectorErrorSampleArray.samples[writeIdx].lineId,
          globalVar.vectorErrorSampleArray.samples[writeIdx].syncCounter,
          globalVar.vectorErrorSampleArray.samples[writeIdx].startIdx);
#else
        if (globalVar.vectorErrorSampleArray.samples[writeIdx].nERBbytes==0) {
          __SoftDslPrintf(gDslVars, "MIB VECT: Errored vectorErrorSample: nERBbytes=%d, lineId=%d, syncCounter=%d, startIdx=%d",0, 
          globalVar.vectorErrorSampleArray.samples[writeIdx].nERBbytes,
          globalVar.vectorErrorSampleArray.samples[writeIdx].lineId,
          globalVar.vectorErrorSampleArray.samples[writeIdx].syncCounter);
#endif
        }
        else
        {
#ifndef G993P5_OVERHEAD_MESSAGING
          __SoftDslPrintf(gDslVars, "MIB VECT: vectorErrorSample: nbSamples=%d, lineId=%d, syncCounter=%d, totalCount=%d writeIdx=%d readIdx=%d",0, 
          globalVar.vectorErrorSampleArray.samples[writeIdx].nbSamples,
          globalVar.vectorErrorSampleArray.samples[writeIdx].lineId,
          globalVar.vectorErrorSampleArray.samples[writeIdx].syncCounter,
          globalVar.adslMib.vectData.esCtrl.totalCount,
          globalVar.adslMib.vectData.esCtrl.writeIdx, globalVar.adslMib.vectData.esCtrl.readIdx);
#else
          __SoftDslPrintf(gDslVars, "MIB VECT: vectorErrorSample: nERBbytes=%d, lineId=%d, syncCounter=%d, totalCount=%d writeIdx=%d readIdx=%d",0, 
          globalVar.vectorErrorSampleArray.samples[writeIdx].nERBbytes,
          globalVar.vectorErrorSampleArray.samples[writeIdx].lineId,
          globalVar.vectorErrorSampleArray.samples[writeIdx].syncCounter,
          globalVar.adslMib.vectData.esCtrl.totalCount,
          globalVar.adslMib.vectData.esCtrl.writeIdx, globalVar.adslMib.vectData.esCtrl.readIdx);
#endif

          globalVar.adslMib.vectData.esCtrl.writeIdx = (writeIdx+1)&(MAX_NUM_VECTOR_SYMB-1);
          globalVar.adslMib.vectData.esCtrl.totalCount++;
          retCode = 1;
        }
      }
        break;
#endif /* (defined(VECTORING) && defined(SUPPORT_VECTORINGD)) */
#endif /* CONFIG_VDSL_SUPPORTED */
#endif /*!defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358) */
      }
      break; /* kDslReceivedEocCommand */
    default:
      break;
  }
  return retCode;
}

Public void AdslMibClearBertResults(void *gDslVars)
{
    globalVar.adslMib.adslBertRes.bertTotalBits = 0;
    globalVar.adslMib.adslBertRes.bertErrBits = 0;
}

Public void AdslMibBertStartEx(void *gDslVars, ulong bertSec)
{
    AdslMibClearBertResults(gDslVars);

    globalVar.adslMib.adslBertStatus.bertTotalBits.cntHi = 0;
    globalVar.adslMib.adslBertStatus.bertTotalBits.cntLo = 0;
    globalVar.adslMib.adslBertStatus.bertErrBits.cntHi = 0;
    globalVar.adslMib.adslBertStatus.bertErrBits.cntLo = 0;

    globalVar.adslMib.adslBertStatus.bertSecTotal = bertSec;
    globalVar.adslMib.adslBertStatus.bertSecElapsed = 1;
    globalVar.adslMib.adslBertStatus.bertSecCur = (ulong ) -1;
}

Public void AdslMibBertStopEx(void *gDslVars)
{
    globalVar.adslMib.adslBertStatus.bertSecCur = 0;
}

Private void AdslMibAdd64 (cnt64 *pCnt64, ulong num)
{
    ulong   n;

    n = pCnt64->cntLo + num;
    if ((n < pCnt64->cntLo) || (n < num))
        pCnt64->cntHi++;

    pCnt64->cntLo = n;
}

Public ulong AdslMibBertContinueEx(void *gDslVars, ulong totalBits, ulong errBits)
{
    if (0 == globalVar.adslMib.adslBertStatus.bertSecCur)
        return 0;

    AdslMibAdd64(&globalVar.adslMib.adslBertStatus.bertTotalBits, totalBits);
    AdslMibAdd64(&globalVar.adslMib.adslBertStatus.bertErrBits, errBits);

    globalVar.adslMib.adslBertStatus.bertSecElapsed += globalVar.adslMib.adslBertStatus.bertSecCur;
    if (globalVar.adslMib.adslBertStatus.bertSecElapsed >= globalVar.adslMib.adslBertStatus.bertSecTotal) {
        globalVar.adslMib.adslBertStatus.bertSecCur = 0;
        return 0;
    }
        
    if (AdslMibIsLinkActive(gDslVars)) {
        ulong    nBits, nSec, nSecLeft;
        
        nBits = AdslMibGetGetChannelRate(gDslVars, kAdslRcvDir, kAdslIntlChannel);
        nBits += AdslMibGetGetChannelRate(gDslVars, kAdslRcvDir, kAdslFastChannel);
        if(nBits <= 0x05555555)  /* Avoid overflow */
          nBits = (nBits * 48) / 53;
        else
          nBits = (nBits / 53) * 48;
        nSec = 0xFFFFFFF0 / nBits;
        nSecLeft = globalVar.adslMib.adslBertStatus.bertSecTotal - globalVar.adslMib.adslBertStatus.bertSecElapsed;
        if (nSec > nSecLeft)
            nSec = nSecLeft;
        if (nSec > 20)
            nSec = 20;
        
        globalVar.adslMib.adslBertStatus.bertSecCur = nSec;
        return nSec * nBits;
    }
    else {
        globalVar.adslMib.adslBertStatus.bertSecCur = 1;
        return 8000000;
    }
}

Public void AdslMibSetLPR(void *gDslVars)
{
    globalVar.adslMib.adslPhys.adslCurrStatus |= kAdslPhysStatusLPR;
}

Public void AdslMibSetShowtimeMargin(void *gDslVars, long showtimeMargin)
{
    globalVar.showtimeMarginThld = showtimeMargin;
}

Public void AdslMibSetNumTones (void *gDslVars, int numTones)
{
    globalVar.nTones=numTones;
}

Public void  *AdslMibGetData (void *gDslVars, int dataId, void *pAdslMibData)
{
    switch (dataId) {
        case kAdslMibDataAll:
            if (NULL != pAdslMibData)
                AdslMibByteMove (sizeof(adslMibVarsStruct), &globalVar, pAdslMibData);
            else
                pAdslMibData = &globalVar;
            break;

        default:
            pAdslMibData = NULL;
    }   
    return pAdslMibData;
}

#endif /* _M_IX86 */
