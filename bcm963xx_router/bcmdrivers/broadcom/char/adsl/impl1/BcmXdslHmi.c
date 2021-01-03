
#include "BcmOs.h"

#include <linux/types.h>
#define	_SYS_TYPES_H

#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/inetdevice.h>
#include <linux/udp.h>

#include <net/ip.h>
#include <net/route.h>
#include <net/arp.h>
#include <linux/version.h>

#include "BcmXdslHmi.h"

#define HMI_DEBUG
#define HMI_ANSWER_IN_DRV

#define HMI_Q_SIZE	3	/* Max number of HMI questions pending */

#define	HMI_UNCACHED(p)	((uint32)(p) | 0x20000000)
#define	HMI_CACHED(p)		((uint32)(p) & ~0x20000000)

typedef void * elementType;
typedef struct queue_tag queueType;
typedef struct _hmiElementType hmiElementType;

struct queue_tag {
	int queueSize;
	int front;
	int rear;
	int itemCnt;
	elementType *array;
};

struct _hmiElementType {
	uint32			timeStamp;
	uint32			requestRef;
	struct sk_buff	*skb;
};

struct _udpFrameHdr {
	struct ethhdr		eth;
	struct iphdr		ipHdr;
	struct udphdr		udpHdr;
}__attribute__((packed));

static int hmiQuestionWaitingAnswer(void);
static void hmiUpdateSkbForAnswer(struct sk_buff *skb, int len);
static ushort hmiIpComputeChecksum(struct iphdr *ipHdr);
static ushort hmiIpUpdateChecksum(int sum, ushort oldVal, ushort newVal);
static void hmiUpdateDataLen(UdpFrameHdr *pUdpFrameHdr, int dataLen);
static int hmiUpdateMacAddr(struct sk_buff *skb, UdpFrameHdr *pUdpFrameHdr);

static queueType *createQ(int size);
static int isQFull(queueType *q);
static int isQEmpty(queueType *q);
static int peekQ(queueType *q, elementType *x);
static int insertItem(queueType *q, elementType x);
static int removeItem(queueType *q, elementType *x);
static void deleteQ(queueType *q);

static struct net_device 			*ethDev = NULL;
static queueType					*hmiQuestionQ = NULL;
static UdpFrameHdr 				udpFrameHdrModel;
#define hmiQuestionQFull()		isQFull(hmiQuestionQ)
#define hmiQuestionQInsert(x)		insertItem(hmiQuestionQ, (elementType)x)
#define hmiQRemoveItem(x)		removeItem(hmiQuestionQ, (elementType *)x)

#ifdef HMI_DEBUG
static void dumpUdpFrame(UdpFrameHdr *pUdpFrameHdr);
static void dumpUdpFrame(UdpFrameHdr *pUdpFrameHdr)
{
	HmiMsgHdr *pHmiMsgHdr = (HmiMsgHdr *)((uint32)pUdpFrameHdr+UDP_FRAME_HDR_LEN);
#if 0
	printk("==>ETH:\n");
	printk ("   h_dest = %X:%X:%X:%X:%X:%X\n", 
		pUdpFrameHdr->eth.h_dest[0], pUdpFrameHdr->eth.h_dest[1], pUdpFrameHdr->eth.h_dest[2],
		pUdpFrameHdr->eth.h_dest[3], pUdpFrameHdr->eth.h_dest[4], pUdpFrameHdr->eth.h_dest[5]);	
	printk ("   h_source = %X:%X:%X:%X:%X:%X\n", 
		pUdpFrameHdr->eth.h_source[0], pUdpFrameHdr->eth.h_source[1], pUdpFrameHdr->eth.h_source[2],
		pUdpFrameHdr->eth.h_source[3], pUdpFrameHdr->eth.h_source[4], pUdpFrameHdr->eth.h_source[5]);	
	printk ("   h_proto = %X\n", pUdpFrameHdr->eth.h_proto);

	printk("==>IP saddr: %X,  daddr: %X. tot_len: %d\n",
		pUdpFrameHdr->ipHdr.saddr, pUdpFrameHdr->ipHdr.daddr, pUdpFrameHdr->ipHdr.tot_len);
	
	printk("==>UDP source: %d,  dest: %d,  len: %d, check: %X\n",
		pUdpFrameHdr->udpHdr.source, pUdpFrameHdr->udpHdr.dest,
		pUdpFrameHdr->udpHdr.len, pUdpFrameHdr->udpHdr.check);
#endif

	bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);
	printk("<== srcRef: %08x, reqRef: %08x, appId: %04x, len: %d, cmdId: %04x\n",
		pHmiMsgHdr->sourceRef,
		pHmiMsgHdr->requestRef,
		pHmiMsgHdr->applicationId,
		pHmiMsgHdr->length,
		pHmiMsgHdr->commandId);
	bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);
	
}
#endif

int BcmAdslCoreHmiInit(void)
{
	char				*ethDevNames[] = { "eth0", "br0" };
	struct net_device	*dev;
	struct in_device	*in_dev;
	struct in_ifaddr	**ifap;
	int				i;
	struct in_ifaddr	*ifa = NULL;	
	UdpFrameHdr		*fh = &udpFrameHdrModel;

	memset((void *)fh, 0, sizeof(UdpFrameHdr));

	for (i = 0; i < sizeof(ethDevNames)/sizeof(ethDevNames[0]); i++) {
		dev = __dev_get_by_name(ethDevNames[i]);
		if (NULL == dev)
			continue;

		printk ("dev = %s(0x%X) hwAddr=%X:%X:%X:%X:%X:%X\n", 
			ethDevNames[i], (int) dev,
			dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
			dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

		/* get device IP address */
		in_dev = __in_dev_get(dev);
		if (NULL == in_dev)
			continue;

		for (ifap=&in_dev->ifa_list; (ifa=*ifap) != NULL; ifap=&ifa->ifa_next) {
			//printk ("ifa_label = %s, ipAddr = 0x%X mask = 0x%X\n", ifa->ifa_label, ifa->ifa_local, ifa->ifa_mask);
			if (strcmp(ethDevNames[i], ifa->ifa_label) == 0)
				break;
		}

		if ( NULL != ifa )
			break;
	}

	if ( NULL == ifa )
		return -1;

	fh->eth.h_proto 	= htons(ETH_P_IP);
	memcpy(fh->eth.h_source, dev->dev_addr, ETH_ALEN);
	memset(fh->eth.h_dest, 0, ETH_ALEN);

	fh->ipHdr.version 	= 4;
	fh->ipHdr.ihl		= 5;
	fh->ipHdr.tos 		= 0;
	fh->ipHdr.tot_len	= sizeof(struct iphdr)+sizeof(struct udphdr)+HMI_HDR_LEN;	/* changes for frames */
	fh->ipHdr.id 		= 0x2000;
	fh->ipHdr.frag_off	= 0;
	fh->ipHdr.ttl 		= 128;
	fh->ipHdr.protocol	= 0x11;	/* always UDP */
	fh->ipHdr.check 	= 0;		/* changes for frames */
	fh->ipHdr.saddr 	= ifa->ifa_local;
	fh->ipHdr.daddr 	= 0;		/* changes for frames */
	fh->ipHdr.check 	= hmiIpComputeChecksum(&fh->ipHdr);

	fh->udpHdr.source = HMI_SERVER_PORT;
	fh->udpHdr.dest 	= 0;										/* changes for frames */
	fh->udpHdr.len 	= sizeof(struct udphdr)+HMI_HDR_LEN;	/* changes for frames */
	fh->udpHdr.check 	= 0;		/* not used */

	hmiQuestionQ = createQ(HMI_Q_SIZE);
	if(!hmiQuestionQ)
		return -1;
	
	ethDev = dev;
	
	return 0;
}

void BcmAdslHmiMsgProcess(void *pArg)
{
	ADSLDRV_HMI		KArg;
	struct sk_buff	*skb;
	HmiMsgHdr 		*pHmiMsgHdr;
	UdpFrameHdr 		*pUdpFrameHdr;
	hmiElementType	*pElem;
	
	if( NULL == ethDev ) {
		if( -1 == BcmAdslCoreHmiInit() )
			return;
	}
	
	if(hmiQuestionQFull())
		return;
	
	skb = dev_alloc_skb(UDP_FRAME_HDR_LEN + MAX_HMI_PACKET_SIZE);
	if(!skb)
		return;
	
	pUdpFrameHdr = (UdpFrameHdr *)HMI_UNCACHED(skb->data);
	pHmiMsgHdr = (HmiMsgHdr *)HMI_UNCACHED(HMI_HDR_OFFSET(skb));

	if (copy_from_user( (void *)&KArg,  pArg, sizeof(KArg) ) != 0)
		goto getout;
	if ( (0 == KArg.pHmiMsg) || (KArg.hmiMsgLen > MAX_HMI_PACKET_SIZE) )
	        goto getout;
	if (copy_from_user((void *)pHmiMsgHdr, (void *)KArg.pHmiMsg, KArg.hmiMsgLen) != 0)
		goto getout;

	skb->dev = ethDev;
	skb->protocol = eth_type_trans (skb, ethDev);
	skb->len = UDP_FRAME_HDR_LEN+HMI_HDR_LEN;	/* Will adjust on the reply if it has data */
	skb->tail = skb->data+skb->len;
	
	if(udpFrameHdrModel.ipHdr.daddr != KArg.remoteIpAddr) {
		udpFrameHdrModel.ipHdr.daddr = KArg.remoteIpAddr;
		if( 0 != hmiUpdateMacAddr(skb, &udpFrameHdrModel) )
			goto getout;
		/* NOTE: Maybe able to do 2 hmiIpUpdateChecksum() */
		udpFrameHdrModel.ipHdr.check = 0;
		udpFrameHdrModel.ipHdr.check = hmiIpComputeChecksum(&(udpFrameHdrModel.ipHdr));
	}
	
	memcpy((void *)pUdpFrameHdr, (void *)&udpFrameHdrModel, UDP_FRAME_HDR_LEN);	
	pUdpFrameHdr->udpHdr.dest = KArg.remotePort;

	pElem = calloc(1,sizeof(hmiElementType));
	if(!pElem)
		goto getout;

	pElem->requestRef = BYTESWAP32(pHmiMsgHdr->requestRef);
	pElem->skb = skb;
	bcmOsGetTime(&pElem->timeStamp);
	
#ifdef HMI_DEBUG
	bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);	
	printk("==> srcRef: %08x, reqRef: %08x, appId: %04x, len: %04x, cmdId: %04x\n",
		pHmiMsgHdr->sourceRef,
		pHmiMsgHdr->requestRef,
		pHmiMsgHdr->applicationId,
		pHmiMsgHdr->length,
		pHmiMsgHdr->commandId);
	bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);	
#endif

	/* Forward the question to the PHY here */

	hmiQuestionQInsert(pElem);
	
#ifdef HMI_ANSWER_IN_DRV
	BcmAdslHmiAnswerProcess();
#endif
	
	return;
getout:
	kfree_skb(skb);
}

void BcmAdslHmiAnswerProcess(void)
{
	hmiElementType	*pElem;
	struct sk_buff	*skb;
	HmiMsgHdr 		*pHmiMsgHdr;
#ifdef HMI_DEBUG	
	UdpFrameHdr 		*pUdpFrameHdr;
#endif
	int 				rc;
	
	if( NULL == ethDev )
		return;	
	
	/* If a question is waiting for an answer then read answer from PHY then send back to client */
	/* otherwise discard answer from PHY */
	if( hmiQuestionWaitingAnswer() ) {
		hmiQRemoveItem(&pElem);
		
		skb = pElem->skb;
		pHmiMsgHdr = (HmiMsgHdr *)HMI_UNCACHED(HMI_HDR_OFFSET(skb));
#ifdef HMI_ANSWER_IN_DRV		
		/* Contruct a fake reply */
		bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);
		hmiMsgConstructReply(pHmiMsgHdr);
		bigEndianByteSwap((uint8 *)pHmiMsgHdr, HmiMsgHdrLayout);
#else
		/* copy HMI msg/answer from PHY to pHmiMsgHdr */
		/* We can double check that requestRef from PHY matches with pElem->requestRef */
#endif
		if(pHmiMsgHdr->length) /* The answer has payload */
			hmiUpdateSkbForAnswer(skb, BYTESWAP16(pHmiMsgHdr->length));
#ifdef HMI_DEBUG
		pUdpFrameHdr = (UdpFrameHdr *)HMI_UNCACHED(skb->data);
		dumpUdpFrame(pUdpFrameHdr);
#endif		
		
		/* Send skb to the ethernet interface */
		rc = dev_queue_xmit(skb);
		if( 0 != rc )
			printk("%s: dev_queue_xmit() failed! rc %d\n", __FUNCTION__, rc);

		kfree(pElem);
	}	
	
}

static int hmiQuestionWaitingAnswer(void)
{
	hmiElementType	*pElem;
	if( -1 == peekQ(hmiQuestionQ, (elementType*)&pElem) )
		return 0;
	
	return (pElem->timeStamp != 0);
}

queueType *createQ(int size)
{
	queueType *q;
	
	if(size <= 0)
		return NULL;
	q = calloc(1,sizeof(queueType));
	if(!q)
		return NULL;
	q->array = calloc(1,sizeof(elementType)*size);
	if(!q->array)
	{
		kfree(q);
		return NULL;
	}
	
	q->queueSize = size;
	q->itemCnt = 0;
	q->front = 0;
	q->rear = -1;
	
	return q;
}

static int isQFull(queueType *q)
{
	return q->itemCnt == q->queueSize;
}

static int isQEmpty(queueType *q)
{
	return q->itemCnt == 0;
}

static int peekQ(queueType *q, elementType*x)
{	
	if(isQEmpty(q))
		return -1;
	
	*x = q->array[q->front];
	
	return 0;
}

static int insertItem(queueType *q, elementType x)
{
	
	if(isQFull(q))
		return -1;
	
	if(++q->rear == q->queueSize)
		q->rear = 0;
	
	q->array[q->rear] = x;
	
	q->itemCnt++;
	
	return 0;		
}

static int removeItem(queueType *q, elementType*x)
{
	
	if(isQEmpty(q))
		return -1;
	
	*x = q->array[q->front];
	if(++q->front == q->queueSize)
		q->front = 0;
	q->itemCnt--;

	return 0;
}

static void deleteQ(queueType *q)
{
	hmiElementType	*pElem;
	
	if( q )
	{
		while( -1 != removeItem(q, &pElem) ) {			
			kfree_skb(pElem->skb);
			kfree(pElem);
		}
		kfree(q->array);
		kfree(q);	
	}
}

static int hmiUpdateMacAddr(struct sk_buff *skb, UdpFrameHdr *pUdpFrameHdr)
{
	struct in_device	*in_dev;
	struct in_ifaddr	**ifap;
	struct in_ifaddr	*ifa;
	struct rtable		rtbl;	
	int				rc;
	
	/* get device IP address */
	in_dev = __in_dev_get(ethDev);
	if (NULL == in_dev)
		return -1;

	for ( ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL; ifap = &ifa->ifa_next ) {
		printk ("ifa_label = %s, ipAddr = 0x%X mask = 0x%X\n", ifa->ifa_label, ifa->ifa_local, ifa->ifa_mask);
		if (strcmp(ethDev->name, ifa->ifa_label) == 0)
			break;
	}

	if (NULL == ifa)
		return -1;
	
	rtbl.rt_gateway = pUdpFrameHdr->ipHdr.daddr;
	skb->dst = (void *) &rtbl;	
	rc = arp_find(pUdpFrameHdr->eth.h_dest, skb);
	skb->dst = NULL;
	
	return rc;
	
}

static void hmiUpdateSkbForAnswer(struct sk_buff *skb, int len)
{
	UdpFrameHdr *pUdpFrameHdr = (UdpFrameHdr *)HMI_UNCACHED(skb->data);

	hmiUpdateDataLen(pUdpFrameHdr, len);

	skb->len  += len;
	skb->tail += len;
}

static void hmiUpdateDataLen(UdpFrameHdr *pUdpFrameHdr, int dataLen)
{
	int n;
	n = pUdpFrameHdr->ipHdr.tot_len + dataLen;
	pUdpFrameHdr->udpHdr.len += dataLen;
	
	pUdpFrameHdr->ipHdr.check = hmiIpUpdateChecksum(pUdpFrameHdr->ipHdr.check, pUdpFrameHdr->ipHdr.tot_len, n);
	pUdpFrameHdr->ipHdr.tot_len = n;
}

static ushort hmiIpComputeChecksum(struct iphdr *ipHdr)
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

static ushort hmiIpUpdateChecksum(int sum, ushort oldVal, ushort newVal)
{
	ushort	tmp;

	tmp = (newVal - oldVal);
	tmp -= (tmp >> 15);
	sum = (sum ^ 0xFFFF) + tmp;
	sum = (sum & 0xFFFF) + (sum >> 16);
	return sum ^ 0xFFFF;
}

