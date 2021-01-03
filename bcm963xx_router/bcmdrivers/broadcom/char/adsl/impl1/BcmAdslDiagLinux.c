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
 * $Revision: 1.4 $
 *
 * $Id: BcmAdslDiagLinux.c,v 1.4 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: BcmAdslDiagLinux.c,v $
 * Revision 1.4  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.3  2004/07/20 23:45:48  ilyas
 * Added driver version info, SoftDslPrintf support. Fixed G.997 related issues
 *
 * Revision 1.2  2004/06/10 00:20:33  ilyas
 * Added L2/L3 and SRA
 *
 * Revision 1.1  2004/04/14 21:11:59  ilyas
 * Inial CVS checkin
 *
 ****************************************************************************/

/* Includes. */
#include "BcmOs.h"

#include <linux/types.h>
#define	_SYS_TYPES_H

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>

#include <net/ip.h>
#include <net/route.h>
#include <net/arp.h>
#include <linux/version.h>

//#define TEST_CMD_BUF_RING

#ifdef TEST_CMD_BUF_RING
#include <linux/timer.h>
#include <linux/jiffies.h>
#endif
#include <linux/vmalloc.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include "bcm_common.h"
#include "bcm_map.h"
#include "bcm_intr.h"
#else
#include "6345_common.h"
#include "6345_map.h"
#include "6345_intr.h"
#endif
#include "board.h"

#include "bcmadsl.h"
#include "BcmAdslCore.h"
#include "AdslCore.h"
#include "AdslCoreMap.h"

#define EXCLUDE_CYGWIN32_TYPES
#include "softdsl/SoftDsl.h"

#include "BcmAdslDiag.h"
#include "DiagDef.h"

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
#include <bcmxtmcfg.h>
#include <asm/io.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#include <net/net_namespace.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
/* Note: Linux4.04L.02 and older don't have "netdev_ops->ndo_start_xmit" */
#define	DEV_TRANSMIT(x)	(x)->dev->netdev_ops->ndo_start_xmit (x, (x)->dev)
#else
#define	DEV_TRANSMIT(x)	dev_queue_xmit(x)
#endif

extern dslCommandStruct	adslCoreConnectionParam;
extern Bool	adslCoreInitialized;
extern void BcmAdslCoreReset(void);
extern void GdbStubInit(void);

void BcmAdslCoreDiagDataInit(void);
void BcmAdslCoreDiagCommand(void);


#define DMA_INTR_MASK		(ADSL_INT_RCV | ADSL_INT_RCV_FIFO_OF | ADSL_INT_RCV_DESC_UF | \
							ADSL_INT_DESC_PROTO_ERR |ADSL_INT_DATA_ERR |ADSL_INT_DESC_ERR)
#define DMA_ERROR_MASK	(ADSL_INT_RCV_FIFO_OF | ADSL_INT_RCV_DESC_UF | \
							ADSL_INT_DESC_PROTO_ERR |ADSL_INT_DATA_ERR |ADSL_INT_DESC_ERR)
#define PHY_INTR_ENABLE(x)	(x[ADSL_INTMASK_F] |= DMA_INTR_MASK); \
							(x[ADSL_INTMASK_I] |= ADSL_INT_HOST_MSG)
#define PHY_INTR_DISABLE(x)	(x[ADSL_INTMASK_F]  &= ~DMA_INTR_MASK); \
							(x[ADSL_INTMASK_I] &= ~ADSL_INT_HOST_MSG)
							
/* Local vars */

#ifdef DIAG_DBG
ulong			diagRxIntrCnt = 0;
ulong			diagSkipWrDmaBlkCnt =0;
#endif

ulong			diagMaxLpPerSrvCnt =0;
ulong			diagDmaErrorCnt =0;
ulong			diagDmaTotalDataLen = 0;

ulong			diagDataMap = 0;
ulong			diagLogTime = 0;
ulong			diagTotalBufNum = 0;

ulong			diagDmaIntrCnt = 0;
ulong			diagDmaBlockCnt = 0;
ulong			diagDmaOvrCnt = 0;
ulong			diagDmaSeqBlockNum = 0;
ulong			diagDmaSeqErrCnt = 0;
ulong			diagDmaBlockWrCnt = 0;
ulong			diagDmaBlockWrErrCnt = 0;
ulong			diagDmaBlockWrBusy = 0;
ulong			diagDmaLogBlockNum = 0;

int				diagEnableCnt = 0;
void *			diagDpcId = NULL;

/*
**
**	Socket diagnostic support
**
*/

#define	DIAG_SKB_USERS			0x3FFFFFFF

#define	UNCACHED(p)				((void *)((long)(p) | 0x20000000))
#define	CACHED(p)				((void *)((long)(p) & ~0x20000000))

#define kDiagDmaBlockSizeShift	11
#define kDiagDmaBlockSize		(1 << kDiagDmaBlockSizeShift)

#define DIAG_DESC_TBL_ALIGN_SIZE	0x1000
#define DIAG_DESC_TBL_MAX_SIZE		0x800
#define DIAG_DESC_TBL_SIZE(x)		((x) * sizeof(adslDmaDesc))
#define DIAG_DMA_BLK_SIZE			( sizeof(diagDmaBlock) )

static struct net_device	*dbgDev = NULL;
static struct net_device	*gdbDev = NULL;
static struct sk_buff		*skbModel = NULL;
static struct sk_buff		*skbModel2 = NULL;
struct sk_buff		*skbGdb = NULL;

typedef struct _diagIpHeader {
	uchar	ver_hl;			/* version & header length */
	uchar	tos;			/* type of service */
	ushort	len;			/* total length */
	ushort	id;				/* identification */
	ushort	off;			/* fragment offset field */
	uchar	ttl;			/* time to live */
	uchar	proto;			/* protocol */
	ushort	checksum;		/* checksum */
	ulong	srcAddr;
	ulong	dstAddr;		/* source and dest address */
} diagIpHeader;

typedef struct _diagUdpHeader {
	ushort	srcPort;		/* source port */
	ushort	dstPort;		/* destination port */
	ushort	len;			/* udp length */
	ushort	checksum;		/* udp checksum */
} diagUdpHeader;

#define DIAG_FRAME_PAD_SIZE		2
#define DIAG_DMA_MAX_DATA_SIZE	1200
#define DIAG_FRAME_HEADER_LEN	(sizeof(diagSockFrame) - DIAG_DMA_MAX_DATA_SIZE - DIAG_FRAME_PAD_SIZE)

typedef struct _diagSockFrame {
	uchar			pad[DIAG_FRAME_PAD_SIZE];
	struct ethhdr		eth;
	diagIpHeader		ipHdr;
	diagUdpHeader	udpHdr;
	LogProtoHeader	diagHdr;
	uchar			diagData[DIAG_DMA_MAX_DATA_SIZE];
} diagSockFrame;

typedef struct {
	struct sk_buff			skb;
	ulong				len;
	ulong				frameNum;
	ulong				mark;
	LogProtoHeader		diagHdrDma;
	diagSockFrame			dataFrame;
	struct skb_shared_info	skbShareInfo;
} diagDmaBlock;

uchar		*diagBuf, *diagBufUC;
diagDmaBlock	*diagStartBlock = NULL;
diagDmaBlock	*diagWriteBlock = NULL;
diagDmaBlock	*diagCurrBlock = NULL;
diagDmaBlock	*diagEndBlock  = NULL;
diagDmaBlock	*diagEyeBlock = NULL;

typedef struct {
	ulong		flags;
	ulong		addr;
} adslDmaDesc;

adslDmaDesc         *diagDescTbl = NULL;
adslDmaDesc         *diagDescTblUC = NULL;

void                        *diagDescMemStart = NULL;
ulong                       diagDescMemSize = 0;

static ushort DiagIpComputeChecksum(diagIpHeader *ipHdr)
{
	ushort	*pHdr = (ushort	*) ipHdr;
	ushort	*pHdrEnd = pHdr + 10;
	ulong	sum = 0;

	do {
		sum += pHdr[0];
		sum += pHdr[1];
		pHdr += 2;
	} while (pHdr != pHdrEnd);

	while (sum > 0xFFFF)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return sum ^ 0xFFFF;
}

static ushort DiagIpUpdateChecksum(int sum, ushort oldv, ushort newv)
{
	ushort	tmp;

	tmp = (newv - oldv);
	tmp -= (tmp >> 15);
	sum = (sum ^ 0xFFFF) + tmp;
	sum = (sum & 0xFFFF) + (sum >> 16);
	return sum ^ 0xFFFF;
}

static void DiagUpdateDataLen(diagSockFrame *diagFr, int dataLen)
{
	int		n;

	diagFr->udpHdr.len = dataLen + sizeof(LogProtoHeader) + sizeof(diagUdpHeader);
	n = diagFr->udpHdr.len + sizeof(diagIpHeader);

	diagFr->ipHdr.checksum = DiagIpUpdateChecksum(diagFr->ipHdr.checksum, diagFr->ipHdr.len, n);
	diagFr->ipHdr.len = n;
}

#if 0
static void DiagPrintData(struct sk_buff *skb)
{
	int		i;

	printk ("***SKB: dev=0x%X, hd=0x%X, dt=0x%X, tl=0x%X, end=0x%X, len=%d, users=%d\n", 
		(int)skb->dev,
		(int)skb->head, (int)skb->data, (int)skb->tail, (int)skb->end, skb->len, 
		atomic_read(&skb->users));

	for (i = 0; i < skb->len; i++)
		printk("%X ", skb->data[i]);
	printk("\n");
}
#endif
static int __GdbWriteData(struct sk_buff *skb, char *buf0, int len0)
{
	diagSockFrame		*dd;
	int					n;

	dd = (diagSockFrame *) skb->head;
	DiagUpdateDataLen(dd, len0 - sizeof(LogProtoHeader));

	memcpy (dd->diagData-sizeof(LogProtoHeader), buf0, len0);

	skb->data = skb->head + DIAG_FRAME_PAD_SIZE;
	skb->len  = DIAG_FRAME_HEADER_LEN + len0 - sizeof(LogProtoHeader);
	skb->tail = skb->data + skb->len;

	n = DEV_TRANSMIT(skb);

	return n;
}

int GdbWriteData(char *buf0, int len0)
{
  struct sk_buff *skb;
#ifdef DEBUG_GDB_STUB
  int i;
#endif

  if (NULL == gdbDev)
    return 0;

#ifdef DEBUG_GDB_STUB
  printk("GdbWriteData::'");
  for (i = 0; i < len0; i++) 
    printk("%c", buf0[i]);
  printk("' len=%d\n", len0);
#endif
  skb = alloc_skb (DIAG_FRAME_PAD_SIZE + DIAG_FRAME_HEADER_LEN + len0 + 16, GFP_ATOMIC);
  if (NULL == skb)
    return -ENOMEM;

  if(skbGdb != NULL){
    skb->dev = gdbDev;
    skb->protocol = eth_type_trans (skb, gdbDev);
    skb->data = skb->head + DIAG_FRAME_PAD_SIZE;
    memcpy(skb->data, skbGdb->data, DIAG_FRAME_HEADER_LEN);
    return __GdbWriteData(skb, buf0, len0);
  }
  return -ENOMEM;
}

static int __DiagWriteData(struct sk_buff *skbDiag, ulong cmd, char *buf0, int len0, char *buf1, int len1)
{
	diagSockFrame		*dd;
	int					n;
#ifdef SUPPORT_DSL_BONDING
	ulong				lineId;
#endif

	dd = (diagSockFrame *) skbDiag->head;
	DiagUpdateDataLen(dd, len0+len1);

	if (cmd & DIAG_MSG_NUM)
		n = diagDmaLogBlockNum++;
	else {
		n = *(short *) LOG_PROTO_ID;
		if (cmd & DIAG_SPLIT_MSG)
			n++;
	}

	*(short *)dd->diagHdr.logProtoId = n;
#ifdef SUPPORT_DSL_BONDING
	lineId = (cmd & DIAG_LINE_MASK) >> DIAG_LINE_SHIFT;
	dd->diagHdr.logPartyId = LOG_PARTY_CLIENT | (lineId << DIAG_PARTY_LINEID_SHIFT);
#else
	dd->diagHdr.logPartyId = LOG_PARTY_CLIENT;
#endif
	dd->diagHdr.logCommmand = cmd & 0xFF;
	memcpy (dd->diagData, buf0, len0);
	if (NULL != buf1)
		memcpy (dd->diagData+len0, buf1, len1);

	skbDiag->data = skbDiag->head + DIAG_FRAME_PAD_SIZE;
	skbDiag->len  = DIAG_FRAME_HEADER_LEN + len0 + len1;
	skbDiag->tail = skbDiag->data + skbDiag->len;
	
#if (defined(VECTORING) && defined(SUPPORT_VECTORINGD))
	/* Assumming VECTORING is built based on new Linux where this function is available in skbuff.h */
	/* This is to prevent "protocol x is buggy" prints from the kernel */
	skb_set_network_header(skbDiag, sizeof(struct ethhdr));
#endif
	/* DiagPrintData(skbDiag); */
	n = DEV_TRANSMIT(skbDiag);

	return n;
}

int DiagWriteData(struct net_device * dev, ulong cmd, char *buf0, int len0, char *buf1, int len1)
{
	struct sk_buff	 *skb,*skb2;
	int n=0;

	if (NULL == dbgDev)
		return 0;

	skb = alloc_skb (DIAG_FRAME_PAD_SIZE + DIAG_FRAME_HEADER_LEN + len0 + len1 + 16, GFP_ATOMIC);

	if (NULL == skb)
		return -ENOMEM;
	
	if(skbModel != NULL) {
		skb->dev = dev;
		skb->protocol = eth_type_trans (skb, dev);
		skb->data = skb->head + DIAG_FRAME_PAD_SIZE;
		memcpy(skb->data, skbModel->data, DIAG_FRAME_HEADER_LEN);
		n = __DiagWriteData(skb, cmd, buf0, len0, buf1, len1);
	}
	
	if(skbModel2 != NULL) {
		skb2 = alloc_skb (DIAG_FRAME_PAD_SIZE + DIAG_FRAME_HEADER_LEN + len0 + len1 + 16, GFP_ATOMIC);
		if (NULL == skb2)
			return -ENOMEM;

		skb2->dev = dev;
		skb2->protocol = eth_type_trans (skb2, dev);
		skb2->data = skb2->head + DIAG_FRAME_PAD_SIZE;
		memcpy(skb2->data, skbModel2->data, DIAG_FRAME_HEADER_LEN);
		n = __DiagWriteData(skb2, cmd, buf0, len0, buf1, len1);
	}
	
	return n;
}

int BcmAdslCoreDiagWriteStatusData(ulong cmd, char *buf0, int len0, char *buf1, int len1)
{
	return DiagWriteData(dbgDev, cmd, buf0, len0, buf1, len1);
}

int BcmAdslCoreDiagWrite(void *pBuf, int len)
{
	ulong			cmd;
	DiagProtoFrame	*pDiagFrame = (DiagProtoFrame *) pBuf;
#ifdef SUPPORT_DSL_BONDING
	uchar			lineId = (pDiagFrame->diagHdr.logPartyId & DIAG_PARTY_LINEID_MASK) >> DIAG_PARTY_LINEID_SHIFT;
	cmd = (lineId << DIAG_LINE_SHIFT) | pDiagFrame->diagHdr.logCommmand;
#else
	cmd = pDiagFrame->diagHdr.logCommmand;
#endif
	return DiagWriteData(dbgDev, cmd, pDiagFrame->diagData, len - sizeof(LogProtoHeader), NULL, 0);
}

#define DiagWriteMibData(dev,buf,len)		DiagWriteData(dev,LOG_CMD_MIB_GET,buf,len,NULL,0)
#define DiagWriteStatusString(dev,str)		DiagWriteData(dev,LOG_CMD_SCRAMBLED_STRING,str,strlen(str)+1,NULL,0)

#if 0
#include "softdsl/StatusParser.h"
LOCAL void BcmAdslCoreDiagWriteStatusOrig(dslStatusStruct *status)
{
	static	char	statStr[4096];
	static	char	statStrAnnex[] = "\n";
	long			n, len;
	char			ch1 = 0, ch2 = 0, *pStr;

	if (NULL == dbgDev)
		return;

	StatusParser (status, statStr);
	if (statStr[0] == 0)
		return;

	strcat(statStr, statStrAnnex);
	len = strlen(statStr);
	pStr = statStr;

	while (len > (LOG_MAX_DATA_SIZE-1)) {
		n = LOG_MAX_DATA_SIZE-1;
		ch1 = pStr[n-1];
		ch2 = pStr[n];
		pStr[n-1] = 1;
		pStr[n] = 0;

		DiagWriteData(dbgDev, LOG_CMD_STRING_DATA, pStr, n + 1, NULL, 0);

		pStr[n-1] = ch1;
		pStr[n] = ch2;
		pStr += (n-1);
		len -= (n - 1);
	}
	DiagWriteData(dbgDev, LOG_CMD_STRING_DATA, pStr, len + 1, NULL, 0);
}
#endif

void BcmAdslCoreDiagWriteLog(logDataCode logData, ...)
{
	static	char	logDataBuf[512];
	char			*logDataPtr = logDataBuf;
	long			n, i, datum, *pCmdData;
	va_list			ap;

	if ((NULL == dbgDev) || (0 == (diagDataMap & DIAG_DATA_LOG)))
		return;

	va_start(ap, logData);

	switch	(logData) {
		case	commandInfoData:
			logDataPtr += sprintf(logDataPtr, "%d:\t", (int)logData);	
			pCmdData = (void *) va_arg(ap, long);
			n = va_arg(ap, long);
			for (i = 0; i < n ; i++)
				logDataPtr += sprintf(logDataPtr, "%ld ", pCmdData[i]);	
			logDataPtr += sprintf(logDataPtr, "\n");	
			break;
		case (inputSignalData - 2):
			datum = va_arg(ap, long);
			*logDataPtr++ = (char) datum;	
			break;
		default:
			break;
	}

	if (logDataPtr != logDataBuf)
		DiagWriteData(dbgDev, logData | DIAG_MSG_NUM, logDataBuf, (logDataPtr - logDataBuf), NULL, 0);
	va_end(ap);
}

static void DiagUpdateSkbForDmaBlock(diagDmaBlock *db, int len)
{
	DiagUpdateDataLen(&db->dataFrame, len);
	*(ulong*)&db->dataFrame.diagHdr = *(ulong*)&db->diagHdrDma;

	db->skb.data = db->skb.head + DIAG_FRAME_PAD_SIZE;
	db->skb.len  = DIAG_FRAME_HEADER_LEN + len;
	db->skb.tail = db->skb.data + DIAG_FRAME_HEADER_LEN + len;

	/* DiagPrintData(skbDiag); */
}

static char *ConvertToDottedIpAddr(ulong ipAddr, char *buf)
{
	if(NULL != buf) {
		sprintf(buf,"%d.%d.%d.%d",
			(int)((ipAddr>>24)&0xFF),
			(int)((ipAddr>>16)&0xFF),
			(int)((ipAddr>>8)&0xFF),
			(int)(ipAddr&0xFF));
	}
	return buf;
}

static struct in_ifaddr * BcmAdslCoreGetIntf(struct net_device	*dev, char *pDevName, ulong ipAddr)
{
	struct in_ifaddr		*ifa, **ifap;
	struct in_device	*in_dev;
	char			buf[16], tmpBuf[16];

	/* get device IP address */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
	in_dev = __in_dev_get_rtnl(dev);
#else
	in_dev = __in_dev_get(dev);
#endif
	if (NULL == in_dev)
		return NULL;

	for (ifap=&in_dev->ifa_list; (ifa=*ifap) != NULL; ifap=&ifa->ifa_next) {
		printk ("\tifa_label = %s, ipAddr = %s, mask = %s\n",
			ifa->ifa_label, ConvertToDottedIpAddr(ifa->ifa_local, buf), ConvertToDottedIpAddr(ifa->ifa_mask,tmpBuf));
		sprintf(tmpBuf, "%s:0", pDevName);	/* Construct the secondary interface name */
		if ((0 == strcmp(pDevName, ifa->ifa_label)) ||
			(0 == strcmp(tmpBuf, ifa->ifa_label))) {
			if ((ifa->ifa_local & ifa->ifa_mask) == (ipAddr & ifa->ifa_mask)) {
				printk("\tMatched network address found!\n");
				return ifa;
			}
		}
	}
	return NULL;
}

static struct net_device * 
BcmAdslCoreInitNetDev(PADSL_DIAG pAdslDiag, int port, struct sk_buff **ppskb, char *devname)
{
	char				*diagDevNames[] = {"eth0", "br1", "br0"};
	struct net_device	*dev = NULL;
	struct in_ifaddr	*ifa;
	struct rtable		rtbl;
	diagSockFrame		*dd;
	int			n, dstPort;
	struct sk_buff	*pskb = *ppskb;
	char			buf[16], tmpBuf[16];

	dstPort = ((ulong) pAdslDiag->diagMap) >> 16;
	if (0 == dstPort)
		dstPort = port;
	
	printk ("Drv%sCmd: CONNECT map=0x%X, logTime=%d, gwIpAddr=%s, dstPort=%d\n",
		devname, pAdslDiag->diagMap&0xffff, pAdslDiag->logTime,
		ConvertToDottedIpAddr(pAdslDiag->gwIpAddr,buf), dstPort);
	
	/* find DIAG interface device */
	ifa = NULL;
	printk ("Searching for interface that has matching network address w/ srvIpaddr %s\n", ConvertToDottedIpAddr(pAdslDiag->srvIpAddr, buf));
	for (n = 0; n < sizeof(diagDevNames)/sizeof(diagDevNames[0]); n++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
		dev = __dev_get_by_name(&init_net, diagDevNames[n]);
#else
		dev = __dev_get_by_name(diagDevNames[n]);
#endif
		if (NULL == dev)
			continue;
		printk ("\tdev = %s(0x%X) hwAddr=%X:%X:%X:%X:%X:%X\n",
			diagDevNames[n], (int) dev,
			dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
			dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
		if(NULL != (ifa = BcmAdslCoreGetIntf(dev, diagDevNames[n], (ulong)pAdslDiag->srvIpAddr)))
			break;
	}

	if (NULL == ifa) {
		if(0 == pAdslDiag->gwIpAddr) {
			printk ("No interface with matching network address with the srvIpAddr. No gateway specified, Diag support is impossible\n");
			return NULL;
		}
		printk ("Searching for interface that has matching network address w/ gwIpAddr %s\n",
			ConvertToDottedIpAddr(pAdslDiag->gwIpAddr, buf));
		for (n = 0; n < sizeof(diagDevNames)/sizeof(diagDevNames[0]); n++) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
			dev = __dev_get_by_name(&init_net, diagDevNames[n]);
#else
			dev = __dev_get_by_name(diagDevNames[n]);
#endif
			if (NULL == dev)
				continue;
			if(NULL != (ifa = BcmAdslCoreGetIntf(dev, diagDevNames[n], (ulong)pAdslDiag->gwIpAddr)))
				break;
		}
	}
	
	if (NULL == ifa) {
		printk ("No interface with matching network address with the gwIpAddr. Diag support is impossible\n");
		return NULL;
	}

	/* get remote MAC address for Diag srvIpAddr */
	if (NULL == pskb) {
		pskb = alloc_skb (DIAG_FRAME_HEADER_LEN + 32, GFP_ATOMIC);
		if (pskb == NULL)
			return NULL;
	}
	pskb->dev = dev;
	pskb->protocol = eth_type_trans (pskb, dev);
	dd = (diagSockFrame *) pskb->head;
	dd->eth.h_proto = htons(ETH_P_IP);

	memcpy(dd->eth.h_source, dev->dev_addr, ETH_ALEN);
	memset(dd->eth.h_dest, 0, ETH_ALEN);

	pskb->dst = (void *) &rtbl;

	if ((ifa->ifa_local & ifa->ifa_mask) == (pAdslDiag->srvIpAddr & ifa->ifa_mask)) {
		printk ("Diag server is on the same subnet\n");
		rtbl.rt_gateway = pAdslDiag->srvIpAddr;
		n = arp_find(dd->eth.h_dest, pskb);
	}
	else {
		printk ("Diag server is outside subnet, using local IP address %s and gateway IP address %s\n",
			ConvertToDottedIpAddr(ifa->ifa_local, buf),
			ConvertToDottedIpAddr(pAdslDiag->gwIpAddr, tmpBuf));
		rtbl.rt_gateway = pAdslDiag->gwIpAddr;
		n = arp_find(dd->eth.h_dest, pskb);
	}

	if (n != 0) {
		pskb = NULL;
		return NULL;
	}
	printk ("dstMACAddr = %X:%X:%X:%X:%X:%X\n", 
		dd->eth.h_dest[0], dd->eth.h_dest[1], dd->eth.h_dest[2],
		dd->eth.h_dest[3], dd->eth.h_dest[4], dd->eth.h_dest[5]);

	/* check dd->eth.h_dest[0..5] != 0 (TBD) */
	dd->ipHdr.ver_hl = 0x45;
	dd->ipHdr.tos = 0;
	dd->ipHdr.len = 0;			/* changes for frames */
	dd->ipHdr.id = 0x2000;
	dd->ipHdr.off = 0;
	dd->ipHdr.ttl =128;
	dd->ipHdr.proto = 0x11;		/* always UDP */
	dd->ipHdr.checksum = 0;		/* changes for frames */
	dd->ipHdr.srcAddr = ifa->ifa_local;
	dd->ipHdr.dstAddr = pAdslDiag->srvIpAddr;
	dd->ipHdr.checksum = DiagIpComputeChecksum(&dd->ipHdr);

	dd->udpHdr.srcPort = port;
	dd->udpHdr.dstPort = dstPort;
	dd->udpHdr.len = 0;			/* changes for frames */
	dd->udpHdr.checksum = 0;	/* not used */

	/* to prevent skb from deallocation */

	pskb->data = pskb->head + DIAG_FRAME_PAD_SIZE;
	atomic_set(&pskb->users, DIAG_SKB_USERS);

	*ppskb = pskb;

	return dev;
}

struct net_device * BcmAdslCoreGuiInit(PADSL_DIAG pAdslDiag)
{
	struct net_device	*dev;

	dev = BcmAdslCoreInitNetDev(pAdslDiag, LOG_FILE_PORT2, &skbModel2, "Gui");

	diagDataMap = pAdslDiag->diagMap & 0xFFFF;
	diagLogTime = pAdslDiag->logTime;
	diagEnableCnt = 1;

	return dev;
}

#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
void BcmAdslDiagSendHdr(void)
{
	dslCommandStruct	cmd;

	if (!BcmAdslDiagIsActive())
		return;
	
	if(NULL != skbModel) {
		cmd.command = kDslDiagFrameHdrCmd;
		cmd.param.dslStatusBufSpec.pBuf = SDRAM_ADDR_TO_ADSL(skbModel->data);
		cmd.param.dslStatusBufSpec.bufSize = DIAG_FRAME_HEADER_LEN;
		BcmAdslCoreDiagWriteStatusString(0, "Diag Hdr Addr: %X, Size %d\n",
			(uint)cmd.param.dslStatusBufSpec.pBuf, (int)cmd.param.dslStatusBufSpec.bufSize);
		dma_cache_wback_inv((ulong)skbModel->data, DIAG_FRAME_HEADER_LEN);
		BcmCoreCommandHandlerSync(&cmd);
		BcmAdslCoreDiagWriteStatusString (0, "Sending DslDiags Hdr to PHY\n");
	}
}

void BcmAdslCoreDiagSetBufDesc(void)
{		
	dslCommandStruct	cmd;
	
	if (!BcmAdslDiagIsActive())
		return;
	
	cmd.command = kDslDiagSetupBufDesc;
	cmd.param.dslDiagBufDesc.descBufAddr = (ulong)diagDescTblUC;
	cmd.param.dslDiagBufDesc.bufCnt = diagTotalBufNum;
	BcmAdslCoreDiagWriteStatusString(0, "Diag Desc Addr: %X, nBuf %d\n",
		(uint)diagDescTblUC, (int)diagTotalBufNum);
	BcmCoreCommandHandlerSync(&cmd);
}

static int setupRawDataFlag = 7;

static void BcmAdslCoreDiagSetupRawData(int flag)
{
	XTM_INTERFACE_LINK_INFO linkInfo;
	BCMXTM_STATUS			res;

	setupRawDataFlag = flag;
	
	if(flag&1)
	{
		linkInfo.ulLinkUsRate =  (unsigned int) skbModel->dev;
		linkInfo.ulLinkState = LINK_START_TEQ_DATA;
	}
	else
	{
		linkInfo.ulLinkState = LINK_STOP_TEQ_DATA;
	}
	res = BcmXtm_SetInterfaceLinkInfo(PORT_TO_PORTID((flag>>1) & 0x3), &linkInfo);
	BcmAdslCoreDiagWriteStatusString(0, "%s: %s TEQ data chanId=%d res=%d\n",
		__FUNCTION__,
		(flag&1)? "Start": "Stop", (flag>>1) & 0x3, res);
}

#endif

struct net_device * BcmAdslCoreDiagInit(PADSL_DIAG pAdslDiag)
{
	struct net_device	*dev;

	dev = BcmAdslCoreInitNetDev(pAdslDiag, LOG_FILE_PORT, &skbModel, "Diag");

	diagDataMap = pAdslDiag->diagMap & 0xFFFF;
	diagLogTime = pAdslDiag->logTime;
	diagEnableCnt = 1;
	
	return dev;
}

struct net_device *BcmAdslCoreGdbInit(PADSL_DIAG pAdslDiag)
{
  GdbStubInit();
  return BcmAdslCoreInitNetDev(pAdslDiag, GDB_PORT, &skbGdb, "Gdb");
}
void BcmAdslCoreDiagDataFlush(void)
{
	int				i, n = 0;
	diagDmaBlock	*diagPtr;

	do {
		diagPtr = (void *) diagStartBlock;
		diagPtr = CACHED(diagPtr);
		
		for (i = 0; i < diagTotalBufNum; i++) {
			n = atomic_read(&diagPtr->skb.users);
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
			if (( 0 == *(ushort*)UNCACHED(&diagPtr->dataFrame.pad) ) && (DIAG_SKB_USERS == n))
#else
			if (( 0 != diagPtr->dataFrame.pad[0] ) && (DIAG_SKB_USERS == n))
#endif
				break;

			diagPtr = (void *) (((char *) diagPtr)  + DIAG_DMA_BLK_SIZE);
		}

		if (i >= diagTotalBufNum)
			break;
		
#ifdef DIAG_DBG
		printk ("DiagDataFlush waiting for block %d to be sent. users = 0x%X\n", i, n);
#endif
	} while (1);
}

void BcmAdslCoreDiagCommand(void)
{
	dslCommandStruct	cmd;
	
	cmd.command = kDslDiagSetupCmd;
	cmd.param.dslDiagSpec.setup = 0;
	if (diagDataMap & DIAG_DATA_EYE)
		cmd.param.dslDiagSpec.setup |= kDslDiagEnableEyeData;
	if (diagDataMap & DIAG_DATA_LOG) {
		cmd.param.dslDiagSpec.setup |= kDslDiagEnableLogData;
		diagDmaLogBlockNum = 0;
		BcmAdslCoreDiagWriteLog(inputSignalData - 2, AC_TRUE);
		diagDmaLogBlockNum = 0;
	}
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
	if(kDslDiagEnableEyeData & cmd.param.dslDiagSpec.setup)
		BcmAdslCoreDiagSetBufDesc();
#endif
	cmd.param.dslDiagSpec.eyeConstIndex1 = 63; 
	cmd.param.dslDiagSpec.eyeConstIndex2 = 64;
	cmd.param.dslDiagSpec.logTime = diagLogTime;
	BcmCoreCommandHandlerSync(&cmd);
}

LOCAL void __BcmAdslCoreDiagDmaInit(void)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;

#ifdef DIAG_DBG
	diagRxIntrCnt = 0;
	diagSkipWrDmaBlkCnt = 0;
#endif
	diagDmaErrorCnt = 0;
	diagDmaIntrCnt = 0;

	diagDmaSeqBlockNum = 0;
	diagDmaSeqErrCnt = 0;
	diagDmaBlockWrBusy = 0;
#endif
	diagDmaTotalDataLen = 0;
	diagMaxLpPerSrvCnt = 0;	
	diagDmaBlockCnt = 0;
	diagDmaOvrCnt = 0;

	diagDmaBlockWrCnt = 0;
	diagDmaBlockWrErrCnt = 0;

	diagStartBlock = (void*)diagBufUC;
	diagCurrBlock = diagStartBlock;
	diagWriteBlock = diagStartBlock;
	diagEyeBlock = diagStartBlock;
	diagEndBlock  = (void *) (((char*) diagCurrBlock) + diagTotalBufNum*DIAG_DMA_BLK_SIZE);
	
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	pAdslEnum[ADSL_INTMASK_F] = 0;
	pAdslEnum[RCV_PTR_FAST] = 0;
	pAdslEnum[RCV_CTL_FAST] = 0;
	
	pAdslEnum[RCV_ADDR_FAST] = (ulong) diagDescTbl & 0x1FFFFFFF;
	pAdslEnum[RCV_CTL_FAST] = 1 | ((FLD_OFFSET(diagDmaBlock, dataFrame.diagData) - FLD_OFFSET(diagDmaBlock, len)) << 1);
	pAdslEnum[RCV_PTR_FAST] = diagTotalBufNum << 3;
	pAdslEnum[ADSL_INTMASK_F] |= DMA_INTR_MASK;
#endif	
}

void BcmAdslCoreDiagDmaInit(void)
{
	if (0 == diagDataMap) {
		diagDataMap = DIAG_DATA_EYE;
		diagLogTime = 0;
		BcmAdslCoreDiagDataInit();
	}
	else
		__BcmAdslCoreDiagDmaInit();
}

void BcmAdslCoreDiagStartLog(ulong map, ulong time)
{
	dslCommandStruct	cmd;

	diagDataMap = 0;
	if (map & kDslDiagEnableEyeData)
		diagDataMap |= DIAG_DATA_EYE;
	if (map & (kDslDiagEnableLogData | kDslDiagEnableDebugData)) {
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
		BcmAdslCoreDiagSetupRawData(setupRawDataFlag | 1);
#endif
		diagDataMap |= DIAG_DATA_LOG;
		diagDmaLogBlockNum = 0;
		BcmAdslCoreDiagWriteLog(inputSignalData - 2, AC_TRUE);
		diagDmaLogBlockNum = 0;
		BcmAdslCoreDiagDataLogNotify(1);
	}
	else
		BcmAdslCoreDiagDataLogNotify(0);
	
	diagLogTime = time;
	BcmAdslCoreDiagDataInit();
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	printk("%s: map=0x%X, time=%d, diagTotalBufNum=%d\n", __FUNCTION__, (int)map, (int)time, (int)diagTotalBufNum);
#else
	printk("%s: map=0x%08X, time=%d, diagTotalBufNum=%d diagBufAddr=0x%08X\n",
		__FUNCTION__, (uint)map, (int)time, (int)diagTotalBufNum, (uint)diagDescTblUC);
#endif

	cmd.command = kDslDiagSetupCmd;
	cmd.param.dslDiagSpec.setup = map;
	cmd.param.dslDiagSpec.eyeConstIndex1 = 0;
	cmd.param.dslDiagSpec.eyeConstIndex2 = 0;
	cmd.param.dslDiagSpec.logTime = time;
	BcmCoreCommandHandlerSync(&cmd);

}

LOCAL void BcmAdslCoreDiagDmaUninit(void)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;

	pAdslEnum[ADSL_INTMASK_F] = 0;
	pAdslEnum[RCV_PTR_FAST] = 0;
	pAdslEnum[RCV_CTL_FAST] = 0;
#endif
	BcmAdslCoreDiagDataFlush();
}

#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
void * BcmAdslCoreDiagGetDmaDataAddr(int descNum)
{
	diagDmaBlock	*diagDmaPtr;

	diagDmaPtr = (void *) ((diagDescTblUC[descNum].addr - FLD_OFFSET(diagDmaBlock, len)) | 0xA0000000);
	return &diagDmaPtr->dataFrame.diagData;
}

int BcmAdslCoreDiagGetDmaDataSize(int descNum)
{
	diagDmaBlock	*diagDmaPtr;

	diagDmaPtr = (void *) ((diagDescTblUC[descNum].addr - FLD_OFFSET(diagDmaBlock, len)) | 0xA0000000);
	return diagDmaPtr->len;
}
#endif	/* 48 platforms */

int	 BcmAdslCoreDiagGetDmaBlockNum(void)
{
	return diagTotalBufNum;
}


ulong  BcmAdslCoreDiagBufRequired(ulong map)
{
	ulong	nBuf = 0;

	if (map & DIAG_DATA_EYE)
		nBuf = 28;

	if (map & DIAG_DATA_LOG)
#if (LINUX_FW_VERSION >= 307) && (defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358))
		nBuf= 256;
#else
		nBuf= 28;
#endif
	
	return nBuf;
}

void *BcmAdslCoreDiagAllocDmaMem(ulong *pBufNum)
{
	void	*pTbl;
	int		nBuf = *pBufNum;
	
	pTbl = (void *) kmalloc(nBuf * DIAG_DMA_BLK_SIZE + (DIAG_DESC_TBL_ALIGN_SIZE + DIAG_DESC_TBL_SIZE(nBuf)), GFP_ATOMIC|__GFP_NOWARN);
	
	while ((NULL == pTbl) && (nBuf > 0)) {
		nBuf--;
		pTbl = (void *) kmalloc(nBuf * DIAG_DMA_BLK_SIZE + (DIAG_DESC_TBL_ALIGN_SIZE + DIAG_DESC_TBL_SIZE(nBuf)), GFP_ATOMIC|__GFP_NOWARN);
	}
	diagDescMemStart = pTbl;
	if (NULL == pTbl) {
		*pBufNum = 0;
		return NULL;
	}

	pTbl = (void *) (((ulong) pTbl + 0xFFF) & ~0xFFF);
	if (diagDescMemStart == pTbl)
		nBuf += 2;

	diagDescMemSize  = nBuf * DIAG_DMA_BLK_SIZE + DIAG_DESC_TBL_SIZE(nBuf);

	BcmAdslCoreDiagWriteStatusString (0, "diagDmaBlkSz=%d, diagTotalBuf=%d, diagDescTbl=0x%X(0x%X), size=%d\n",
		sizeof(diagDmaBlock), (int)nBuf, (int) pTbl, (int) diagDescMemStart, (int) diagDescMemSize);

	*pBufNum = nBuf;
	return pTbl;
}

void BcmAdslCoreDiagDataInit(void)
{
	int			i;
	ushort			ipId;
	struct sk_buff		*skb;
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	ulong			nReq;
#endif

	if (0 == diagDataMap)
		return;
	
	BcmCoreDpcSyncEnter();
	if (NULL == diagDescTbl) {
		diagTotalBufNum = BcmAdslCoreDiagBufRequired(diagDataMap);
		diagDescTbl = BcmAdslCoreDiagAllocDmaMem(&diagTotalBufNum);
		if (NULL == diagDescTbl) {
			diagDataMap = 0;
			BcmCoreDpcSyncExit();
			return;
		}
	}
	else {
		BcmAdslCoreDiagDmaUninit();
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
		nReq = BcmAdslCoreDiagBufRequired(diagDataMap);
		if (nReq > diagTotalBufNum) {
			kfree(diagDescMemStart);
			diagTotalBufNum = nReq;
			diagDescTbl = BcmAdslCoreDiagAllocDmaMem(&diagTotalBufNum);
			if (NULL == diagDescTbl) {
				diagDataMap = 0;
				BcmCoreDpcSyncExit();
				return;
			}
		}
#endif
	}

	diagDescTblUC = UNCACHED(diagDescTbl);
	diagBuf = (uchar *)diagDescTbl + DIAG_DESC_TBL_SIZE(diagTotalBufNum);
	diagBufUC = UNCACHED(diagBuf);
	diagCurrBlock = (void*) diagBuf;

	for (i = 0; i < diagTotalBufNum; i++) {
		diagCurrBlock = (void*) diagBuf;
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
		diagDescTblUC[i].flags = DIAG_DMA_BLK_SIZE - FLD_OFFSET(diagDmaBlock, len) - sizeof(struct skb_shared_info);
		diagDescTblUC[i].addr  = ((ulong)diagBuf & 0x1FFFFFFF) + FLD_OFFSET(diagDmaBlock, len);
#else
		diagCurrBlock->diagHdrDma.logProtoId[0] = '*';
		diagCurrBlock->diagHdrDma.logProtoId[1] = 'L';
		diagCurrBlock->diagHdrDma.logPartyId = 1;
		diagCurrBlock->diagHdrDma.logCommmand = eyeData;
		
		diagCurrBlock->dataFrame.diagHdr.logProtoId[0] = '*';
		diagCurrBlock->dataFrame.diagHdr.logProtoId[1] = 'L';
		diagCurrBlock->dataFrame.diagHdr.logPartyId = 1; 	/* DIAG_PARTY_CLIENT */
		diagCurrBlock->dataFrame.diagHdr.logCommmand = eyeData;
		diagDescTbl[i].flags = (ulong)UNCACHED(&diagCurrBlock->dataFrame);
		diagDescTbl[i].addr = (ulong)UNCACHED(&diagCurrBlock->dataFrame.diagData);
		diagDescTblUC[i].flags = (ulong)UNCACHED(&diagCurrBlock->dataFrame);
		diagDescTblUC[i].addr = (ulong)UNCACHED(&diagCurrBlock->dataFrame.diagData);
#endif
		
		skb = &diagCurrBlock->skb;
		
		memset(skb, 0, sizeof(*skb));

		/* memcpy(skb, skbModel, sizeof(struct sk_buff)); */

		skb->head = (void *)&diagCurrBlock->dataFrame;
		skb->data = (void *)&diagCurrBlock->dataFrame.eth;
		skb->tail = skb->data;
		skb->end  = (void *)&diagCurrBlock->skbShareInfo;
		skb->len = 0;
		skb->data_len = 0;

		skb->truesize = (skb->end - skb->head) + sizeof(struct sk_buff);
		skb->cloned = 0;
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
		atomic_set(&skb->users, DIAG_SKB_USERS-1);
#else
		atomic_set(&skb->users, DIAG_SKB_USERS);
#endif
		atomic_set(&(skb_shinfo(skb)->dataref), 1);
		skb_shinfo(skb)->nr_frags = 0;
		skb_shinfo(skb)->frag_list = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
		skb_shinfo(skb)->gso_size = 0;
		skb_shinfo(skb)->gso_segs = 0;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		skb_shinfo(skb)->tso_size = 0;
		skb_shinfo(skb)->tso_segs = 0;
#endif
		skb->dev = dbgDev;
		if (NULL != dbgDev)
			skb->protocol = eth_type_trans (skb, skb->dev);
		skb->ip_summed = CHECKSUM_NONE;

		if (NULL != skbModel)
			memcpy(skb->data, skbModel->data, DIAG_FRAME_HEADER_LEN);
		ipId = diagCurrBlock->dataFrame.ipHdr.id;
		diagCurrBlock->dataFrame.ipHdr.id = 0x4000 + i;
		diagCurrBlock->dataFrame.ipHdr.checksum = DiagIpUpdateChecksum(
												diagCurrBlock->dataFrame.ipHdr.checksum,
												ipId,
												diagCurrBlock->dataFrame.ipHdr.id);
		
		diagCurrBlock->len = 0;
		diagCurrBlock->frameNum = 0;
		diagCurrBlock->mark   = 0;
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
		diagCurrBlock->dataFrame.pad[0] = 0;
#else
		*(ushort *)(&diagCurrBlock->dataFrame.pad) = 0;
		*(ushort *)UNCACHED(&diagCurrBlock->dataFrame.pad) = 0;
#endif
		diagBuf		+= DIAG_DMA_BLK_SIZE;
		diagBufUC	+= DIAG_DMA_BLK_SIZE;
	}
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	diagDescTblUC[diagTotalBufNum-1].flags |= 0x10000000;
#endif
	diagBuf = (uchar *)diagDescTbl + DIAG_DESC_TBL_SIZE(diagTotalBufNum);
	diagBufUC = UNCACHED(diagBuf);
	
	__BcmAdslCoreDiagDmaInit();
	BcmCoreDpcSyncExit();
	
	return;
}

#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
int BcmAdslCoreDiagWriteBlocks(void)
{
	diagDmaBlock	*diagPtr;
	int			reschedule = 0;

	if(NULL == dbgDev)
		return reschedule;

	while (diagWriteBlock != diagCurrBlock) {
		diagPtr = CACHED(diagWriteBlock);
		if ((diagPtr->dataFrame.pad[0] != 0) && (DIAG_SKB_USERS == atomic_read(&diagPtr->skb.users))) {
#ifdef DIAG_DBG
			diagSkipWrDmaBlkCnt++;
#endif
			reschedule = 1;
			break;
		}
		
		diagPtr->dataFrame.pad[0] = 0;
		atomic_set(&diagPtr->skb.users, DIAG_SKB_USERS);
		diagPtr->skb.dev = dbgDev;
		
		reschedule = DEV_TRANSMIT(&diagPtr->skb);
		
		if ( 0 != reschedule) {
			diagDmaBlockWrErrCnt++;
#ifdef DIAG_DBG
			printk("%s: Error=%d, packetID=0x%X\n", __FUNCTION__, n, diagPtr->dataFrame.ipHdr.id);
#endif
			break;
		}
		
		diagPtr->dataFrame.pad[0] = 1;
		diagDmaBlockWrCnt++;
		
		if (diagWriteBlock->diagHdrDma.logCommmand != kDslLogComplete)
			diagDmaTotalDataLen += diagWriteBlock->len;
		else
			BcmAdslCoreDiagDataLogNotify(0);
		
		diagWriteBlock = (void *) (((char*) diagWriteBlock) + DIAG_DMA_BLK_SIZE);
		if (diagWriteBlock == diagEndBlock)
			diagWriteBlock = diagStartBlock;

	}

	return reschedule;
}

LOCAL int BcmCoreDiagDpc(void * arg)
{
	int		descIndex;
	int		res;
	int 		loopCnt = 0;
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	diagDmaBlock	*diagDmaPtr, *diagPtr;

	pAdslEnum[ADSL_INTSTATUS_F] = pAdslEnum[ADSL_INTSTATUS_F];

	descIndex =  (pAdslEnum[RCV_STATUS_FAST] & 0xFFF) >> 3;
	diagDmaPtr = (diagDmaBlock*) ((diagDescTblUC[descIndex].addr - FLD_OFFSET(diagDmaBlock, len)) | 0xA0000000);
	
	while (diagCurrBlock != diagDmaPtr) {
		diagDmaBlockCnt++;
		diagPtr = CACHED(diagCurrBlock);
		if ((diagPtr->dataFrame.pad[0] != 0) && (DIAG_SKB_USERS == atomic_read(&diagPtr->skb.users)))
			diagDmaOvrCnt++;

		if (diagCurrBlock->frameNum != diagDmaSeqBlockNum) {
#ifdef DIAG_DBG
			BcmAdslCoreDiagWriteStatusString (0, "blkNum=%ld, diagP=%X, dmaP=0x%X, len=%ld,anum=%ld,mark=0x%lX,lHdr=0x%lX,enum=%ld\n", 
				diagDmaBlockCnt, (int)diagCurrBlock,
				(int)diagDmaPtr, diagCurrBlock->len,
				diagCurrBlock->frameNum, diagCurrBlock->mark,
				*(ulong*)&diagCurrBlock->diagHdrDma,diagDmaSeqBlockNum);
#endif
			diagDmaSeqErrCnt++;
			diagDmaSeqBlockNum = diagCurrBlock->frameNum;
		}
		
		diagDmaSeqBlockNum++;

		/* update log frame number */
		if (diagCurrBlock->diagHdrDma.logCommmand != eyeData)
			*(ushort *)diagCurrBlock->diagHdrDma.logProtoId = (ushort) diagDmaLogBlockNum++;
		
		/* update skb for this DMA block */
		diagPtr->diagHdrDma = diagCurrBlock->diagHdrDma;
		DiagUpdateSkbForDmaBlock(diagPtr, diagCurrBlock->len);

		diagCurrBlock->mark = 0;
		diagCurrBlock = (void *) (((char*) diagCurrBlock) + DIAG_DMA_BLK_SIZE);
		
		if (diagCurrBlock == diagEndBlock)
			diagCurrBlock = diagStartBlock;

		if (diagCurrBlock == diagWriteBlock)
			diagDmaOvrCnt++;

		res = BcmAdslCoreDiagWriteBlocks();

		loopCnt++;

		descIndex =  (pAdslEnum[RCV_STATUS_FAST] & 0xFFF) >> 3;
		diagDmaPtr = (diagDmaBlock*) ((diagDescTblUC[descIndex].addr - FLD_OFFSET(diagDmaBlock, len)) | 0xA0000000);
	}

	res = BcmAdslCoreDiagWriteBlocks();

	if( (0 != res) && (NULL != diagDpcId) )
		bcmOsDpcEnqueue(diagDpcId);
	else
		PHY_INTR_ENABLE(pAdslEnum);

	if(loopCnt > diagMaxLpPerSrvCnt)
		diagMaxLpPerSrvCnt=loopCnt;

	return 0;
}

Bool BcmAdslCoreDiagIntrHandler(void)
{
	ulong		intStatus;
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;

	PHY_INTR_DISABLE(pAdslEnum);
	
	intStatus = pAdslEnum[ADSL_INTSTATUS_F];
	pAdslEnum[ADSL_INTSTATUS_F] = intStatus;
	
	if ((0 == diagDataMap) || (diagEnableCnt <= 0)) {
		PHY_INTR_ENABLE(pAdslEnum);
		return 0;
	}
	
	if (intStatus & DMA_INTR_MASK) {
		diagDmaIntrCnt++;
		
		if(intStatus & DMA_ERROR_MASK)
			diagDmaErrorCnt++;
		
		if(intStatus & ADSL_INT_RCV) {
#ifdef DIAG_DBG
			diagRxIntrCnt++;
#endif
			if( NULL != diagDpcId ) {
				bcmOsDpcEnqueue(diagDpcId);
				return 0;
			}
		}
	}
	
	PHY_INTR_ENABLE(pAdslEnum);
	
	return 0;
}

#else /* 68 arch. chips */

Bool BcmCoreDiagXdslEyeDataAvail(void)
{
	return(BcmCoreDiagLogActive(DIAG_DATA_EYE) &&
		diagCurrBlock && (*(ushort *)&diagCurrBlock->dataFrame.pad &1));
}

Bool BcmCoreDiagLogActive(ulong map)
{
	return(0 != (diagDataMap & map));
}

void BcmCoreDiagEyeDataFrameSend(void)
{
	int len, n, loop=0;
	diagDmaBlock *diagPtr;
	ushort pad;

	if((NULL == dbgDev) || (NULL == diagCurrBlock))
		return;
	
	pad = *(ushort *)&diagCurrBlock->dataFrame.pad;
	
	while( 0 != (pad & 1) ) {
		loop++;
		diagPtr = CACHED(diagCurrBlock);

		if(DIAG_SKB_USERS != atomic_read(&diagPtr->skb.users)) {
			len = pad >> 1;
			DiagUpdateSkbForDmaBlock(diagPtr, len);
			atomic_set(&diagPtr->skb.users, DIAG_SKB_USERS);
			diagPtr->skb.dev = dbgDev;
			*(ushort *)&diagCurrBlock->dataFrame.pad = 0;
			
			n = DEV_TRANSMIT(&diagPtr->skb);
			
			if ( 0 != n)
				diagDmaBlockWrErrCnt++;
			else
				diagDmaBlockWrCnt++;
		}
		else {
			diagDmaOvrCnt++;
		}
		
		diagCurrBlock = (diagDmaBlock *) ((char *)diagCurrBlock + DIAG_DMA_BLK_SIZE);
		if (diagCurrBlock >= diagEndBlock)
			diagCurrBlock = diagStartBlock;
		
		diagDmaBlockCnt++;
		pad = *(ushort *)&diagCurrBlock->dataFrame.pad;
	}
	
	if(loop > diagMaxLpPerSrvCnt)
		diagMaxLpPerSrvCnt = loop;
}

#endif /* defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358) */

void BcmAdslCoreDiagIsrTask(void)
{
}

int BcmAdslDiagGetConstellationPoints (int toneId, void *pointBuf, int numPoints)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	volatile ulong	*pAdslEnum = (ulong *) XDSL_ENUM_BASE;
	diagDmaBlock	*diagDmaBlockPtr;
	ulong			*pSrc, *pDst;
	int				i;
#endif

	if (0 == diagDataMap) {
		diagDataMap = DIAG_DATA_EYE;
		diagLogTime = 0;
		BcmAdslCoreDiagDataInit();
		BcmAdslCoreDiagCommand();
		return 0;
	}
	
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	diagDmaBlockPtr = (diagDmaBlock*) ((diagDescTblUC[ (pAdslEnum[RCV_STATUS_FAST] & 0xFFF) >> 3].addr - FLD_OFFSET(diagDmaBlock, len)) | 0xA0000000);
	while (diagEyeBlock != diagDmaBlockPtr) {
		if (eyeData == diagEyeBlock->diagHdrDma.logCommmand)
			break;

		diagEyeBlock = (void *) (((char*) diagEyeBlock) + DIAG_DMA_BLK_SIZE);
		if (diagEyeBlock == diagEndBlock)
			diagEyeBlock = diagStartBlock;
	}
	if (diagEyeBlock == diagDmaBlockPtr)
		return 0;

	if (numPoints > (diagEyeBlock->len >> 3))
		numPoints = diagEyeBlock->len >> 3;

	pSrc = ((ulong *)diagEyeBlock->dataFrame.diagData) + (toneId != 0 ? 1 : 0);
	pDst = (ulong *) pointBuf;
	for (i = 0; i < numPoints; i++) {
		*pDst++ = *pSrc;
		pSrc += 2;
	}

	diagEyeBlock = (void *) (((char*) diagEyeBlock) + DIAG_DMA_BLK_SIZE);
	if (diagEyeBlock == diagEndBlock)
		diagEyeBlock = diagStartBlock;
#endif
	return numPoints;
}

#ifdef PHY_PROFILE_SUPPORT
extern void BcmAdslCoreProfilingStop(void);
#endif

#ifdef TEST_CMD_BUF_RING

LOCAL uint32 sendCmdTimerSeed = 0;
LOCAL uint32 __random32(uint32 *seed);
LOCAL uint32 __random32(uint32 *seed) /* FIXME: will get a more sophiscated algorithm later */
{
	*seed = 16807*(*seed);
	*seed += (((*seed) >> 16) & 0xffff);
	return *seed;
}

#define PROF_TIMER_SEED_INIT	(sendCmdTimerSeed = jiffies)
#define PROF_RANDOM32_GEN		__random32(&sendCmdTimerSeed)

LOCAL ulong sndCnt = 0;
LOCAL ulong failCnt = 0, prevFailSndCnt = 0, continousFailcnt=0;

LOCAL int sendCmdStarted = 0;
LOCAL struct timer_list sendCmdTimer;

LOCAL void BcmAdslCoreProfileSendCmdFn(ulong arg);

LOCAL VOID	*pData = NULL;
LOCAL ulong	gInterval = 10;

LOCAL void BcmAdslCoreTestCmdStart(ulong interval)
{
	BcmAdslCoreDiagWriteStatusString(0, "*** %s: interval = %d ***\n", __FUNCTION__, (int)interval);
	printk("*** %s: interval = %d ***\n", __FUNCTION__, (int)interval);
	
	if(NULL == (pData = kmalloc(128, GFP_KERNEL))) {
		printk("*** %s: Failed kmalloc() ***\n", __FUNCTION__);
		return;
	}
	
	gInterval = interval;
	init_timer(&sendCmdTimer);
	sendCmdTimer.function = BcmAdslCoreProfileSendCmdFn;
	sendCmdTimer.data = interval;
	sendCmdTimer.expires = jiffies + 2; /* 10ms */
	PROF_TIMER_SEED_INIT;
	sendCmdStarted = 1;
	sndCnt = 0;
	failCnt = 0;
	add_timer(&sendCmdTimer);
}

void BcmAdslCoreTestCmdStop(void)
{
	BcmAdslCoreDiagWriteStatusString(0, "*** %s ***\n", __FUNCTION__);
	printk("*** %s ***\n", __FUNCTION__);

	if(sendCmdStarted) {
		del_timer(&sendCmdTimer);
		sendCmdStarted = 0;
	}
	
	if(pData) {
		kfree(pData);
		pData = NULL;
	}
}

extern AC_BOOL AdslCoreCommandWrite(dslCommandStruct *cmdPtr);

LOCAL void BcmAdslCoreProfileSendCmdFn(ulong arg)
{
	ulong				msgLen;
	dslCommandStruct		cmd;
	int					len, i;	
	volatile AdslXfaceData	*pAdslX = (AdslXfaceData *) ADSL_LMEM_XFACE_DATA;
	AC_BOOL			res=TRUE;
	
	if(!sendCmdStarted)
		return;
	
	if(0 == sndCnt) {
		printk("*** %s: arg = %d pData=%x***\n", __FUNCTION__, (int)arg, (uint) pData);
		printk("***pStart=%x pRead=%x pWrite=%x pEnd=%x pStretchEnd=%x pExtraEnd=%x\n",
			(uint)pAdslX->sbCmd.pStart, (uint)pAdslX->sbCmd.pRead, (uint)pAdslX->sbCmd.pWrite,
			(uint)pAdslX->sbCmd.pEnd, (uint)pAdslX->sbCmd.pStretchEnd, (uint)pAdslX->sbCmd.pExtraEnd);
		BcmAdslCoreDiagWriteStatusString(0, "*** %s: arg = %d pData=%x***\n", __FUNCTION__, (int)arg, (uint) pData);
		BcmAdslCoreDiagWriteStatusString(0, "***pStart=%x pRead=%x pWrite=%x pEnd=%x pStretchEnd=%x pExtraEnd=%x\n",
			(uint)pAdslX->sbCmd.pStart, (uint)pAdslX->sbCmd.pRead, (uint)pAdslX->sbCmd.pWrite,
			(uint)pAdslX->sbCmd.pEnd, (uint)pAdslX->sbCmd.pStretchEnd, (uint)pAdslX->sbCmd.pExtraEnd);
	}
	
	/* Send Cmd  */
	cmd.command = kDslSendEocCommand;
	cmd.param.dslClearEocMsg.msgId = 370;
	cmd.param.dslClearEocMsg.dataPtr = pData;
	
	for(i=0; i < 14; i++) {
		msgLen = PROF_RANDOM32_GEN;
		msgLen = msgLen % 113;
		if( 0 == msgLen )
			msgLen = 4;		
		len = msgLen + sizeof(cmd.param.dslClearEocMsg);

		cmd.param.dslClearEocMsg.msgType = msgLen;
		cmd.param.dslClearEocMsg.msgType |= kDslClearEocMsgDataVolatile;
		
		if(0 == (sndCnt%2000))
		{
//			printk("*** Sending cmdNum %d, len %d\n", (int)sndCnt, len);
			BcmAdslCoreDiagWriteStatusString(0, "*** Sending cmdNum %d, len %d\n", (int)sndCnt, len);
		}
		
		res = AdslCoreCommandWrite(&cmd);
		
		sndCnt++;
		
		if(FALSE == res) {
			if((prevFailSndCnt + 1) == sndCnt) {
				if(++continousFailcnt > 40 ) {
					printk("***FAILED!!!***\n\n");
					printk("***pStart=%x pRead=%x pWrite=%x pEnd=%x pStretchEnd=%x pExtraEnd=%x\n",
						(uint)pAdslX->sbCmd.pStart, (uint)pAdslX->sbCmd.pRead, (uint)pAdslX->sbCmd.pWrite,
						(uint)pAdslX->sbCmd.pEnd, (uint)pAdslX->sbCmd.pStretchEnd, (uint)pAdslX->sbCmd.pExtraEnd);
					BcmAdslCoreDiagWriteStatusString(0, "***FAILED!!!***\n\n");
					BcmAdslCoreDiagWriteStatusString(0, "***pStart=%x pRead=%x pWrite=%x pEnd=%x pStretchEnd=%x pExtraEnd=%x\n",
						(uint)pAdslX->sbCmd.pStart, (uint)pAdslX->sbCmd.pRead, (uint)pAdslX->sbCmd.pWrite,
						(uint)pAdslX->sbCmd.pEnd, (uint)pAdslX->sbCmd.pStretchEnd, (uint)pAdslX->sbCmd.pExtraEnd);
					sendCmdStarted = 0;
					BcmAdslCoreTestCmdStop();
					return;
				}
			}
			prevFailSndCnt = sndCnt;
			if(0 == (failCnt%2000))
			{
				uint	rdPtr, wrPtr, sePtr;
				rdPtr = (uint)pAdslX->sbCmd.pRead;
				wrPtr = (uint)pAdslX->sbCmd.pWrite;
				sePtr =(uint)pAdslX->sbCmd.pStretchEnd;
#if 0
				printk("***Write len=%d failed(%d/%d): pRead=%x pWrite=%x pStretchEnd=%x\n",
					len, (int)failCnt, (int)sndCnt,
					rdPtr, wrPtr, sePtr);
#endif
				BcmAdslCoreDiagWriteStatusString(0, "***Write len=%d failed(%d/%d): pRead=%x pWrite=%x pStretchEnd=%x\n",
					len, (int)failCnt+1, (int)sndCnt,
					rdPtr, wrPtr, sePtr);
			}
			failCnt++;
		}
		else {
			prevFailSndCnt = 0;
			continousFailcnt = 0;
		}
	}
			
	/* Re-schedule the timer */
	sendCmdTimer.expires = jiffies + gInterval;
	add_timer(&sendCmdTimer);

}
#endif /* TEST_CMD_BUF_RING */

char	savedStatFileName[] = "savedStatusFile.bin";
char	successStr[] = "SUCCESS";
char	failStr[] = "FAIL";

static Boolean diagLockSession = 0;
static Boolean diagRegressionLock = 0;

void BcmAdslDiagDisconnect(void)
{
	diagDataMap = 0;
	diagLockSession = 0;
	diagRegressionLock = 0;
	BcmAdslCoreDiagCommand();
#ifdef PHY_PROFILE_SUPPORT
	BcmAdslCoreProfilingStop();
#endif
	BcmAdslCoreDiagDataLogNotify(0);
	dbgDev = NULL;
}

void BcmAdslCoreDiagCmd(unsigned char lineId, PADSL_DIAG pAdslDiag)
{
	ulong	origDiagMap, map;
	int		dstPort;
	diagSockFrame	*dd;
	struct net_device	*pDev = NULL;
	char		buf[16];
		
	BcmAdslCoreDiagCmdNotify();
	dstPort = ((ulong) pAdslDiag->diagMap) >> 16;
	map = (ulong) pAdslDiag->diagMap & 0xFFFF;
	
	switch (pAdslDiag->diagCmd) {
		case LOG_CMD_CONNECT:
			origDiagMap = diagDataMap;
			printk("CONNECT request from srvIpAddr=%s, dstPort=%d, diagMap=0x%08X\n",
				ConvertToDottedIpAddr(pAdslDiag->srvIpAddr, buf), dstPort, (uint)map);
			
			BcmCoreDpcSyncEnter();
			
			if (pAdslDiag->diagMap & DIAG_DATA_GUI_ID) {
				pDev = BcmAdslCoreGuiInit(pAdslDiag);
				if(NULL != pDev)
					dbgDev = pDev;
			}
			else if (pAdslDiag->diagMap & DIAG_DATA_GDB_ID) {
				pDev = BcmAdslCoreGdbInit(pAdslDiag);
				if(NULL != pDev)
					gdbDev = pDev;
			}
			else {
				
				if(NULL != skbModel)
					dd = (diagSockFrame *) skbModel->head;
				else
					dd = NULL;
				
				if ( (0 == diagLockSession) || ((NULL != dd) && (pAdslDiag->srvIpAddr == dd->ipHdr.dstAddr)) ) {
					diagLockSession = 1;
					pAdslDiag->diagMap = (int)((ulong)pAdslDiag->diagMap & ~((ulong)DIAG_LOCK_SESSION));
					
					printk("Connecting to srvIpAddr %s\n", ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf));
					BcmAdslCoreDiagWriteStatusString(lineId, "Connecting to srvIpAddr %s\n",
						ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf));
					if(dd && dbgDev) {
						if(dd->ipHdr.dstAddr != pAdslDiag->srvIpAddr) {
						printk("Disconnecting from srvIpAddr %s\n", ConvertToDottedIpAddr(dd->ipHdr.dstAddr,buf));
						BcmAdslCoreDiagWriteStatusString(lineId, "Disconnecting from srvIpAddr %s\n",
							ConvertToDottedIpAddr(dd->ipHdr.dstAddr,buf));
						}
					}
					
					pDev = BcmAdslCoreDiagInit(pAdslDiag);
					if(NULL != pDev)
						dbgDev = pDev;
					
					printk("%s: pDev=0x%X skbModel=0x%X\n", __FUNCTION__, (uint)pDev, (uint)skbModel);
					BcmAdslCoreDiagWriteStatusString(lineId, "%s: pDev=0x%X skbModel=0x%X\n",
						__FUNCTION__, (ulong)pDev, (ulong)skbModel);
				}
				else {
					static struct sk_buff *skbModelNew = NULL;
					struct sk_buff *skbModelTmp = skbModel;
					
					printk("Reject connection request from srvIpAddr %s\n", ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf));
					printk("Connection is being locked by srvIpAddr %s\n", (dd) ? ConvertToDottedIpAddr(dd->ipHdr.dstAddr,buf): "Unknown" );
					BcmAdslCoreDiagWriteStatusString(lineId, "Reject connection request from srvIpAddr %s\n",
						ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf));

					printk("Temporarily connect to rejected srvIpAddr %s to deliver unlock instructions\n",
						ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf));
					pDev = dbgDev;
					dbgDev = BcmAdslCoreInitNetDev(pAdslDiag, LOG_FILE_PORT, &skbModelNew, "Diag");
					skbModel = skbModelNew;
					BcmAdslCoreDiagWriteStatusString(lineId, "Connection request is rejected!!!\n");
					BcmAdslCoreDiagWriteStatusString(lineId, "Connection is being locked by srvIpAddr %s\n",
						(dd) ? ConvertToDottedIpAddr(dd->ipHdr.dstAddr,buf): "Unkown");
					if(!diagRegressionLock)
						BcmAdslCoreDiagWriteStatusString(lineId, "To establish a new connection, unlock the current connection(File Menu)\n");
					else
						BcmAdslCoreDiagWriteStatusString(lineId, "The target is being locked for regression testing, please contact the owner\n");
					skbModel = skbModelTmp;
					dbgDev = pDev;
					
					BcmCoreDpcSyncExit();
					return;
				}
			}
			
			BcmCoreDpcSyncExit();
			
			if (adslCoreInitialized && (NULL != dbgDev)) {
				adslVersionInfo		verInfo;
				adslMibInfo		*pAdslMib;
				
				if (diagDataMap & DIAG_DATA_LOG) {
					BcmAdslCoreReset();
					BcmAdslCoreDiagDataLogNotify(1);
				}
				else {
					BcmAdslCoreDiagDataInit();
					if (0 == origDiagMap) {
						BcmAdslCoreDiagCommand();
					}
				}
				
				BcmAdslCoreGetVersion(&verInfo);
				BcmAdslCoreDiagWriteStatusString(lineId, "ADSL version info: PHY=%s, Drv=%s. Built on "__DATE__" " __TIME__,
					verInfo.phyVerStr, verInfo.drvVerStr);
				
				map = sizeof(adslMibInfo);
				pAdslMib = (void *)BcmAdslCoreGetObjectValue (lineId, NULL, 0, NULL, (long *)&map);
				if(NULL != pAdslMib) {
					BcmAdslCoreDiagWriteStatusString(lineId, "MACAddress_AFEID: %02X:%02X:%02X:%02X:%02X:%02X, 0x%08X\n",
						dbgDev->dev_addr[0], dbgDev->dev_addr[1], dbgDev->dev_addr[2],
						dbgDev->dev_addr[3], dbgDev->dev_addr[4], dbgDev->dev_addr[5],
						pAdslMib->afeId[lineId]);
				}
				
				BcmXdslCoreDiagSendPhyInfo();
#if !defined(CONFIG_BCM96338) && !defined(CONFIG_BCM96348) && !defined(CONFIG_BCM96358)
				BcmXdslCoreSendAfeInfo(0);	/* Send AFE info to DslDiags */
				BcmAdslDiagSendHdr();
#endif
			}
			break;
			
		case LOG_CMD_DISCONNECT:
			if(NULL != skbModel)
				dd = (diagSockFrame *) skbModel->head;
			else
				dd = NULL;
			printk ("DISCONNECT Request(%s) from srvIpAddr=%s diagLockSession=%d regressionLock=%d\n",
				(map & DIAG_FORCE_DISCONNECT) ? "forced": "not-forced",
				ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf), diagLockSession, diagRegressionLock);
			
			if( (NULL != dd) && (pAdslDiag->srvIpAddr != dd->ipHdr.dstAddr) )
				BcmAdslCoreDiagWriteStatusString(lineId, "DISCONNECT Request(%s) from srvIpAddr = %s diagLockSession=%d regressionLock=%d\n",
					(map & DIAG_FORCE_DISCONNECT) ? "forced": "not-forced",
					ConvertToDottedIpAddr(pAdslDiag->srvIpAddr,buf), diagLockSession, diagRegressionLock);
			
			if( 0 == (map & DIAG_FORCE_DISCONNECT) ) {
				if( diagLockSession && (NULL != dd) ){
					if ( pAdslDiag->srvIpAddr != dd->ipHdr.dstAddr )
						break;	/* Reject DISCONNECT */
				}
			}
			else if( diagRegressionLock && (NULL != dd) ){
				if(pAdslDiag->srvIpAddr != dd->ipHdr.dstAddr)
					break;	/* Reject DISCONNECT */
			}
			
			BcmAdslDiagDisconnect();
			break;

		case LOG_CMD_DEBUG:
		{
			DiagDebugData	*pDbgCmd = (void *)pAdslDiag->diagMap;
			switch (pDbgCmd->cmd) {
				case DIAG_DEBUG_CMD_CLEAR_STAT:
					diagDmaIntrCnt	= 0;
					diagDmaBlockCnt = 0;
					diagDmaOvrCnt = 0;
					diagDmaSeqErrCnt = 0;
					diagDmaBlockWrCnt = 0;
					diagDmaBlockWrErrCnt = 0;
					diagDmaBlockWrBusy = 0;
					break;
				case DIAG_DEBUG_CMD_PRINT_STAT:
					BcmAdslCoreDiagWriteStatusString(lineId,
						"DiagLinux Statistics:\n"
						"   diagIntrCnt		= %d\n"
						"   dmaBlockCnt	= %d\n"
						"   dmaSeqNum	= %d\n"
						"   ethWrCnt		= %d\n"
						"   dmaOvrCnt		= %d\n"
						"   dmaSqErrCnt	= %d\n"
						"   ethErrWrCnt	= %d\n"
						"   ethBusyCnt	= %d\n"
#ifdef DIAG_DBG
						"   dmaRxIntrCnt		= %d\n"
						"   dmaSkipWrBlkCnt	= %d\n"
#endif
						"   dmaMaxLpPerSrvCnt	= %d\n"
						"   dmaErrorCnt		= %d\n"
						"   dmaTotalDataLen	= %lu\n",
						diagDmaIntrCnt,
						diagDmaBlockCnt,
						diagDmaSeqBlockNum,
						diagDmaBlockWrCnt,
						diagDmaOvrCnt,
						diagDmaSeqErrCnt,
						diagDmaBlockWrErrCnt,
						diagDmaBlockWrBusy,
#ifdef DIAG_DBG
						diagRxIntrCnt,
						diagSkipWrDmaBlkCnt,
#endif
						diagMaxLpPerSrvCnt,
						diagDmaErrorCnt,
						diagDmaTotalDataLen);
					break;
				case DIAG_DEBUG_CMD_STAT_SAVE_LOCAL:
				{
					static void 		*pMem = NULL;
					BCMADSL_STATUS	res;
					
					if( 1 == pDbgCmd->param1 ) {	/* Init logging statuses */
						res = BcmAdsl_DiagStatSaveStop();
						if(pMem)
							vfree(pMem);
						
						pMem = vmalloc(2 * pDbgCmd->param2);
						if( NULL == pMem ) {
							printk("%s - %s vmalloc(%ld)\n",
								"BcmAdsl_DiagStatSaveInit", failStr, pDbgCmd->param2);
							break;
						}
						res = BcmAdsl_DiagStatSaveInit( pMem, 2 * pDbgCmd->param2);
						printk("%s - %s pMem=0x%p len=%ld\n",
							"BcmAdsl_DiagStatSaveInit",
							( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr,
							pMem, pDbgCmd->param2);
					}
					else if( 2 == pDbgCmd->param1 ) { /* Enable logging statuses */
						res = BcmAdsl_DiagStatSaveStart();
						printk("%s - %s\n",
							"BcmAdsl_DiagStatSaveStart", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
					}
					else if( 3 == pDbgCmd->param1 ) {	/* Disable logging statuses */
						res = BcmAdsl_DiagStatSaveStop();
						printk("%s - %s\n",
							"BcmAdsl_DiagStatSaveStop", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
					}
					else if( 4 == pDbgCmd->param1 ) {	/* Write statuses to DslDiag */
						ADSL_SAVEDSTATUS_INFO savedStatInfo;
						int len, dataCnt, dataSize = 7*LOG_MAX_DATA_SIZE;
						
						BcmAdsl_DiagStatSaveStop();
						res = BcmAdsl_DiagStatSaveGet(&savedStatInfo);
						BcmAdslCoreDiagWriteStatusString(lineId, "%s - %s\n",
							"BcmAdsl_DiagStatSaveGet", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
						printk("%s - %s\n",
							"BcmAdsl_DiagStatSaveGet", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
						if( BCMADSL_STATUS_SUCCESS == res ) {
							BcmAdslCoreDiagWriteStatusString(lineId, "nStatus=%d pAddr=0x%X len=%d pAddr1=0x%X len1=%d\n",
								savedStatInfo.nStatus, (uint)savedStatInfo.pAddr, savedStatInfo.len,
								(uint)savedStatInfo.pAddr1, savedStatInfo.len1);
							printk("nStatus=%d pAddr=0x%X len=%d pAddr1=0x%X len1=%d\n",
								savedStatInfo.nStatus, (uint)savedStatInfo.pAddr, savedStatInfo.len,
								(uint)savedStatInfo.pAddr1, savedStatInfo.len1);
							
							BcmAdslCoreDiagOpenFile(lineId, savedStatFileName);
							dataCnt = 0;
							while (dataCnt < savedStatInfo.len) {
								len = savedStatInfo.len - dataCnt;
								if (len > dataSize)
									len = dataSize;
								BcmAdslCoreDiagWriteFile(lineId, savedStatFileName, savedStatInfo.pAddr+dataCnt, len);
								dataCnt += len;
								BcmAdslCoreDelay(40);
							}
							if( savedStatInfo.len1 > 0 ) {
								dataCnt = 0;
								while (dataCnt < savedStatInfo.len1) {
									len = savedStatInfo.len1 - dataCnt;
									if (len > dataSize)
										len = dataSize;
									BcmAdslCoreDiagWriteFile(lineId, savedStatFileName, savedStatInfo.pAddr1+dataCnt, len);
									dataCnt += len;
									BcmAdslCoreDelay(40);
								}
							}
						}
					}
					else if( 5 == pDbgCmd->param1 ) {	/* Enable logging statuses continously */
						res = BcmAdsl_DiagStatSaveContinous();
						printk("%s - %s\n",
							"BcmAdsl_DiagStatSaveContinous", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
					}
					else if( (6 == pDbgCmd->param1) || (9 == pDbgCmd->param1) ) {	/* start logging statuses continously/until retrain */
						BcmAdsl_DiagStatSaveStop();
						if(pMem)
							vfree(pMem);
						pMem = vmalloc(pDbgCmd->param2*2);
						if( NULL == pMem ) {
							printk("%s - %s vmalloc(%ld)\n",
								"BcmAdsl_DiagStatSaveInit", failStr,pDbgCmd->param2);
							break;
						}
						res = BcmAdsl_DiagStatSaveInit( pMem, 2 * pDbgCmd->param2);
						if( BCMADSL_STATUS_SUCCESS != res ) {
							printk("%s - %s pMem=0x%p len=%lu\n",
								"BcmAdsl_DiagStatSaveInit", failStr, pMem, pDbgCmd->param2);
							break;
						}
						BcmAdsl_DiagStatSaveContinous();
						if(9 == pDbgCmd->param1)
							BcmXdslCoreDiagStatSaveDisableOnRetrainSet();
						BcmAdsl_DiagStatSaveStart();
						if(9 == pDbgCmd->param1)
							printk("** Logging until retrain is started(0x%lx) ** \n", pDbgCmd->param2);
						else
							printk("** Logging continously started(0x%lx) ** \n", pDbgCmd->param2);
					}
					else if( 7 == pDbgCmd->param1 ) {	/* Stop logging statuses */
						res = BcmAdsl_DiagStatSaveUnInit();
						BcmAdslCoreDiagWriteStatusString(lineId, "%s - %s\n",
							"BcmAdsl_DiagStatSaveUnInit", ( BCMADSL_STATUS_SUCCESS == res ) ? successStr: failStr);
						if(pMem)
							vfree(pMem);
						pMem = NULL;
					}
					else if( 8 == pDbgCmd->param1 ) {	/* start logging until buffer is full */
						BcmAdsl_DiagStatSaveStop();
						if(pMem)
							vfree(pMem);
						pMem = vmalloc(pDbgCmd->param2*2);
						if( NULL == pMem ) {
							printk("%s - %s vmalloc(%ld)\n",
								"BcmAdsl_DiagStatSaveInit", failStr,pDbgCmd->param2);
							break;
						}
						res = BcmAdsl_DiagStatSaveInit( pMem, 2 * pDbgCmd->param2);
						if( BCMADSL_STATUS_SUCCESS != res ) {
							BcmAdslCoreDiagWriteStatusString(lineId, "%s - %s pMem=0x%p len=%ld\n",
								"BcmAdsl_DiagStatSaveInit", failStr, pMem, pDbgCmd->param2);
							break;
						}
						BcmAdsl_DiagStatSaveStart();
						printk("** Logging until buffer is full started ** \n");
					}
					else if( 10 == pDbgCmd->param1 ) {	/* Pause logging */
						ADSL_SAVEDSTATUS_INFO savedStatInfo;
						
						BcmAdsl_DiagStatSaveStop();
						printk("** Logging paused ** \n");
						res = BcmAdsl_DiagStatSaveGet(&savedStatInfo);
						if( BCMADSL_STATUS_SUCCESS == res ) {
							BcmAdslCoreDiagWriteStatusString(lineId, "	nStatus=%d pAddr=0x%X len=%d pAddr1=0x%X len1=%d\n",
								savedStatInfo.nStatus, (uint)savedStatInfo.pAddr, savedStatInfo.len,
								(uint)savedStatInfo.pAddr1, savedStatInfo.len1);
							printk("	nStatus=%d pAddr=0x%X len=%d pAddr1=0x%X len1=%d\n",
								savedStatInfo.nStatus, (uint)savedStatInfo.pAddr, savedStatInfo.len,
								(uint)savedStatInfo.pAddr1, savedStatInfo.len1);
						}
					}
					break;
				}
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM963268)
#ifdef TEST_CMD_BUF_RING
				case DIAG_DEBUG_CMD_START_BUF_TEST:
					BcmAdslCoreTestCmdStart(pDbgCmd->param1);
					break;
				case DIAG_DEBUG_CMD_STOP_BUF_TEST:
					BcmAdslCoreTestCmdStop();
					break;
#endif
				case DIAG_DEBUG_CMD_SET_RAW_DATA_MODE:
					BcmAdslCoreDiagSetupRawData(pDbgCmd->param1);
					break;
#endif
				case DIAG_DEBUG_CMD_SET_REGRESSION_LOCK:
					diagRegressionLock = 1;
					break;
				case DIAG_DEBUG_CMD_CLR_REGRESSION_LOCK:
					diagRegressionLock = 0;
					diagLockSession = 0;
					break;
			}
			BcmAdslCoreDebugCmd(lineId, pDbgCmd);
		}
			break;
		default:
			BcmAdslCoreDiagCmdCommon(lineId, pAdslDiag->diagCmd, pAdslDiag->logTime, (void*) pAdslDiag->diagMap);
			break;
	}
}

/***************************************************************************
** Function Name: BcmAdslDiagDisable/BcmAdslDiagEnable
** Description  : This function enables/disables diag interrupt processing
** Returns      : None.
***************************************************************************/
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

Bool BcmAdslDiagIsActive(void)
{
	return (NULL != dbgDev);
}

/***************************************************************************
** Function Name: BcmAdslDiagReset
** Description  : This function resets diag support after ADSL MIPS reset
** Returns      : None.
***************************************************************************/
void BcmAdslDiagReset(int map)
{
	BcmAdslDiagDisable();
	diagDataMap &= map & (DIAG_DATA_LOG | DIAG_DATA_EYE);
	if (diagDataMap & (DIAG_DATA_LOG | DIAG_DATA_EYE)) {
		diagDmaSeqBlockNum = 0;
		BcmAdslCoreDiagDataInit();
	}
	BcmAdslDiagEnable();
	if (diagDataMap & (DIAG_DATA_LOG | DIAG_DATA_EYE))
		BcmAdslCoreDiagCommand();
}

/***************************************************************************
** Function Name: BcmAdslDiagInit
** Description  : This function intializes diag support on Host and ADSL MIPS
** Returns      : None.
***************************************************************************/
int BcmAdslDiagInit(int map)
{
#if defined(CONFIG_BCM96338) || defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
	diagDpcId = bcmOsDpcCreate(BcmCoreDiagDpc, NULL);
#endif
	BcmAdslCoreDiagDataInit();
	BcmAdslCoreDiagCommand();
	return 0;
}



/**************************************************************************
** Function Name: BcmAdslDiagTaskInit
** Description  : This function intializes ADSL driver Diag task
** Returns      : None.
**************************************************************************/
int BcmAdslDiagTaskInit(void)
{
	return 0;
}

/**************************************************************************
** Function Name: BcmAdslDiagTaskUninit
** Description  : This function unintializes ADSL driver Diag task
** Returns      : None.
**************************************************************************/
void BcmAdslDiagTaskUninit(void)
{
}

