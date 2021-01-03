/****************************************************************************
 *
 * BcmCoreDiagNone.c -- Bcm ADSL core diagnostic over socket stubs
 *
 * Description:
 *	This file contains BCM ADSL core driver diagnostic stubs
 *
 *
 * Copyright (c) 2000-2001  Broadcom Corporation
 * All Rights Reserved
 * No portions of this material may be reproduced in any form without the
 * written permission of:
 *          Broadcom Corporation
 *          16215 Alton Parkway
 *          Irvine, California 92619
 * All information contained in this document is Broadcom Corporation
 * company private, proprietary, and trade secret.
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: BcmAdslDiagNone.c,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: BcmAdslDiagNone.c,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

/* Only for references from BcmCoreTest.c (TBD) */

int				diagSock = -1;
unsigned long	adslCoreDiagMap = 0;
int				diagEnableCnt = 0;

int BcmAdslDiagTaskInit(void)
{
	return 0;
}

void BcmAdslDiagTaskUninit(void)
{
}

int BcmAdslDiagInit(int diagDataMap)
{
	return 0;
}

int BcmAdslCoreDiagIntrHandler(void)
{
	return 0;
}

void BcmAdslCoreDiagIsrTask(void)
{
}

void BcmAdslCoreDiagWriteStatusData(unsigned long cmd, char *buf0, int len0,
    char *buf1, int len1)
{
}

void BcmAdslCoreDiagStartLog(unsigned long map, unsigned long time)
{
}

int BcmAdslDiagIsActive(void)
{
	return (0);
}

void BcmAdslDiagReset(int diagDataMap)
{
}

int  BcmAdslDiagGetConstellationPoints (int toneId, void *pointBuf, int numPoints)
{
	return 0;
}

int BcmAdslDiagDisable(void)
{
	diagEnableCnt--;
	return diagEnableCnt;
}

int BcmAdslDiagEnable(void)
{
	diagEnableCnt++;
	return diagEnableCnt;
}

void BcmAdslCoreDiagCmd(char *pAdslDiag)
{
}

int BcmAdslCoreDiagWrite(void *pBuf, int len)
{
    return( 0 );
}

void * BcmAdslCoreDiagGetDmaDataAddr(int descNum)
{
    return( (char *) 0 );
}

int BcmAdslCoreDiagGetDmaDataSize(int descNum)
{
    return( 0 );
}

int	 BcmAdslCoreDiagGetDmaBlockNum(void)
{
	return( 0 );
}

void BcmAdslCoreDiagDmaInit(void)
{
}

typedef     long logDataCode;
void BcmAdslCoreDiagWriteLog(logDataCode logData, ...)
{
}

