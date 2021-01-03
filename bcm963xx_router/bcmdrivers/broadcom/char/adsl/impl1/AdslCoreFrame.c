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
 * AdslCoreFrame.c -- Frame allcation/freeing functions
 *
 * Description:
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslCoreFrame.c,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslCoreFrame.c,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 *
 *****************************************************************************/

#include "softdsl/SoftDsl.h"
#include "softdsl/BlankList.h"

#include "AdslCoreFrame.h"

#if defined(__KERNEL__) || defined(TARG_OS_RTEMS) || defined(_CFE_)
#include "BcmOs.h"
#else
#include <stdlib.h>
#endif

Public void * AdslCoreFrameAllocMemForFrames(ulong frameNum)
{
	void	**pFreeFrameList;
	uchar	*frPtr;
	ulong	i;

	/* allocate memory to hold the head and frameNum dslFrame buffers */

	pFreeFrameList = (void *) calloc (1, sizeof (void *) + sizeof(dslFrame) * frameNum);
	if (NULL == pFreeFrameList)
		return NULL;

	/* make a list of free dslFrame buffers */

	*pFreeFrameList = NULL;
	frPtr = (uchar *) (pFreeFrameList + 1) + sizeof(dslFrame) * (frameNum - 1);
	for (i = 0; i < frameNum; i++) {
		BlankListAdd(pFreeFrameList, (void*)frPtr);
		frPtr -= sizeof(dslFrame);
	}
	return pFreeFrameList;
}

Public void AdslCoreFrameFreeMemForFrames(void *hMem)
{
	free (hMem);
}

Public dslFrame * AdslCoreFrameAllocFrame(void *handle)
{
	dslFrame	*pFr;

	pFr = BlankListGet(handle);
	return pFr;
}

Public void AdslCoreFrameFreeFrame(void *handle, dslFrame *pFrame)
{
	BlankListAdd(handle, pFrame);
}

Public void * AdslCoreFrameAllocMemForBuffers(void **ppMemPool, ulong bufNum, ulong memSize)
{
	void	**pFreeBufList;
	uchar	*frPtr;
	ulong	i;
	
	/* allocate memory to hold the head and bufNum dslFrame buffers */

	pFreeBufList = (void *) calloc (1, sizeof (void *) + sizeof(dslFrameBuffer) * bufNum + memSize);
	if (NULL == pFreeBufList)
		return NULL;

	/* make a list of free dslFrame buffers */

	*pFreeBufList = NULL;
	frPtr = (uchar *) (pFreeBufList + 1) + sizeof(dslFrameBuffer) * (bufNum - 1);
	for (i = 0; i < bufNum; i++) {
		BlankListAdd(pFreeBufList, (void*)frPtr);
		frPtr -= sizeof(dslFrameBuffer);
	}

	*ppMemPool = (void *) pFreeBufList;
	return (uchar *) (pFreeBufList + 1) + sizeof(dslFrameBuffer) * bufNum;
}

Public void AdslCoreFrameFreeMemForBuffers(void *hMem, ulong memSize, void *pMemPool)
{
	free (pMemPool);
}

Public dslFrameBuffer * AdslCoreFrameAllocBuffer(void *handle, void *pMem, ulong length)
{
	dslFrameBuffer	*pBuf;

	pBuf = BlankListGet(handle);
	pBuf->pData  = pMem;
	pBuf->length = length;

	return pBuf;
}

Public void AdslCoreFrameFreeBuffer(void *handle, dslFrameBuffer *pBuf)
{
	BlankListAdd(handle, pBuf);
}

Public ulong AdslCoreFrame2Id(void *handle, dslFrame *pFrame)
{
	return ((uchar *)pFrame - (uchar *)handle - sizeof (void *)) / sizeof(dslFrame);
}

Public void * AdslCoreFrameId2Frame(void *handle, ulong frameId)
{
	return (uchar *)handle + sizeof(void *) + sizeof(dslFrame)*frameId;
}
