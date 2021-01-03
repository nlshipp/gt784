/*
<:copyright-broadcom 
 
 Copyright (c) 2007 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          5300 California Avenue
          Irvine, California 92617 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***************************************************************************
 * File Name  : p8021ag.c
 *
 * Description: This file contains Linux character device driver entry points
 *              for IEEE P802.1ag support.
 ***************************************************************************/


/* Includes. */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <asm/uaccess.h>
#include <p8021agdrv.h>
#include "p8021ag.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
#include <net/net_namespace.h>
#endif

//#define DEBUG_8021AG
#if defined(DEBUG_8021AG)
#define DPRINTK(format,args...) printk(format,##args)
#define DUMPADDR(addr,len) dumpaddr(addr,len)
extern void dumpaddr(unsigned char *, int);
#else
#define DPRINTK(format,args...)
#define DUMPADDR(B,L)
#endif

/* Defines. */
#define P8021AG_VERSION                     "1.0"
#define DEFAULT_MD_LEVEL                    2
#define LTM_TTL_VALUE                       64

/* Typedefs. */
typedef void (*FN_IOCTL) (unsigned long arg);

typedef struct p8021ag_info
{
    struct net_device *dev;
    unsigned long ulMdLevel;
    unsigned long ulCurrLbmTransId;
    unsigned long ulNextLbmTransId;
    unsigned long ulCurrLtmTransId;
    unsigned long ulNextLtmTransId;
    unsigned short usLbmVlanId;
    unsigned short usLtmVlanId;
    int nLbmStatus;
    int nLtmStatus;
    unsigned char ucLtrList[LTM_TTL_VALUE][ETH_ALEN];
} P8021AG_INFO, *PP8021AG_INFO;


/* Prototypes. */
static int __init p8021ag_init( void );
static void __exit p8021ag_cleanup( void );
static int p8021ag_open( struct inode *inode, struct file *filp );
static int p8021ag_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg );
static void DoStart( unsigned long arg );
static void DoStop( unsigned long arg );
static void DoSetMdLevel( unsigned long arg );
static void DoSendLoopback( unsigned long arg );
static void DoSendLinktrace( unsigned long arg );
static int SendLbm( PP8021AG_INFO pInfo, PP8021AG_LOOPBACK pLbParms );
static int SendLtm( PP8021AG_INFO pInfo, PP8021AG_LINKTRACE pLtParms );
static int p8021ag_receive( struct sk_buff *skb );
static void ProcessLbm( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo );
static void ProcessLbr( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo );
static void ProcessLtm( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo );
static void ProcessLtr( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo );
static unsigned char *FindTlv( unsigned char ucTlv, unsigned char *pucStart );


/* Globals. */
static struct file_operations g_p8021ag_fops =
{
    .ioctl  = p8021ag_ioctl,
    .open   = p8021ag_open,
};

static P8021AG_INFO g_P8021agInfo;

/***************************************************************************
 * Function Name: p8021ag_init
 * Description  : Initial function that is called at system startup that
 *                registers this device.
 * Returns      : None.
 ***************************************************************************/
static int __init p8021ag_init( void )
{
    printk( "p8021ag: p8021ag_init entry\n" );
    register_chrdev( P8021AG_MAJOR, "p8021ag", &g_p8021ag_fops );
    memset(&g_P8021agInfo, 0x00, sizeof(g_P8021agInfo));
    g_P8021agInfo.ulMdLevel = DEFAULT_MD_LEVEL;
    p8021ag_hook = NULL;
    return( 0 );
} /* p8021ag_init */


/***************************************************************************
 * Function Name: p8021ag_cleanup
 * Description  : Final function that is called when the module is unloaded.
 * Returns      : None.
 ***************************************************************************/
static void __exit p8021ag_cleanup( void )
{
    printk( "p8021ag: p8021ag_cleanup entry\n" );
    unregister_chrdev( P8021AG_MAJOR, "p8021ag" );
    DoStop( 0 );
    p8021ag_hook = NULL;
} /* p8021ag_cleanup */


/***************************************************************************
 * Function Name: p8021ag_open
 * Description  : Called when an application opens this device.
 * Returns      : 0 - success
 ***************************************************************************/
static int p8021ag_open( struct inode *inode, struct file *filp )
{
    return( 0 );
} /* p8021ag_open */


/***************************************************************************
 * Function Name: p8021ag_ioctl
 * Description  : Main entry point for an application send issue ATM API
 *                requests.
 * Returns      : 0 - success or error
 ***************************************************************************/
static int p8021ag_ioctl( struct inode *inode, struct file *flip,
    unsigned int command, unsigned long arg )
{
    int ret = 0;
    unsigned int cmdnr = _IOC_NR(command);
    FN_IOCTL IoctlFuncs[] = {DoStart, DoStop, DoSetMdLevel, DoSendLoopback,
        DoSendLinktrace, NULL};
    int nrcmds = (sizeof(IoctlFuncs) / sizeof(FN_IOCTL)) - 1;

    DPRINTK("p8021ag: ioctl cmdnr=%u\n", cmdnr);
    if( cmdnr >= 0 && cmdnr < nrcmds && IoctlFuncs[cmdnr] != NULL )
        (*IoctlFuncs[cmdnr]) (arg);
    else
        ret = -EINVAL;

    return( ret );
} /* p8021ag_ioctl */


/***************************************************************************
 * Function Name: DoStart
 * Description  : Start 802.1ag processing.
 * Returns      : None.
 ***************************************************************************/
static void DoStart( unsigned long arg )
{
    P8021AG_START_STOP KArg;

    DPRINTK("p8021ag: DoStart entry\n");
    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        PP8021AG_INFO pInfo = &g_P8021agInfo;

        if( p8021ag_hook == NULL )
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
            if( (pInfo->dev = __dev_get_by_name(&init_net, KArg.szDeviceName)) != NULL )
#else
            if( (pInfo->dev = __dev_get_by_name(KArg.szDeviceName)) != NULL )
#endif
            {
                pInfo->ulCurrLbmTransId = pInfo->ulNextLbmTransId = 1;
                pInfo->ulCurrLtmTransId = pInfo->ulNextLtmTransId = 1;

                /* assign 802.1ag hook function */
                p8021ag_hook = p8021ag_receive;

                KArg.nResult = 0;
            }
            else
            {
                /* The instance for the passed device name was not found. */
                KArg.nResult = -ENODEV;
            }
        }
        else
        {
            /* Only one handler can be registered at a time. */
            KArg.nResult = -EBUSY;
        }

        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
    }

    DPRINTK("p8021ag: DoStart %s exit, result=%d\n", KArg.szDeviceName, KArg.nResult);
} /* DoStart */


/***************************************************************************
 * Function Name: DoStop
 * Description  : Stop 802.1ag processing.
 * Returns      : None.
 ***************************************************************************/
static void DoStop( unsigned long arg )
{
    P8021AG_START_STOP KArg;

    DPRINTK("p8021ag: DoStop entry\n");
    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        PP8021AG_INFO pInfo = &g_P8021agInfo;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
        if( pInfo->dev == __dev_get_by_name(&init_net, KArg.szDeviceName))
#else
        if( pInfo->dev == __dev_get_by_name(KArg.szDeviceName))
#endif
        {
            p8021ag_hook = NULL;
            KArg.nResult = 0;
        }
        else
        {
            /* Only one handler can be registered at a time. */
            KArg.nResult = -EBUSY;
        }

        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
    }

    DPRINTK("p8021ag: DoStop exit, result=%d\n", KArg.nResult);
} /* DoStop */


/***************************************************************************
 * Function Name: DoSetMdLevel
 * Description  : Set the Maintenance Domain level.
 * Returns      : None.
 ***************************************************************************/
static void DoSetMdLevel( unsigned long arg )
{
    DPRINTK("p8021ag: DoSetMdLevel - level=%lu\n", arg);
    g_P8021agInfo.ulMdLevel = arg;
} /* DoSetMdLevel */


/***************************************************************************
 * Function Name: DoSendLoopback
 * Description  : Send an 802.1ag Loopback Message (LBM) and waits for a
 *                Loopback Reply (LBR).  That status of the operation is
 *                returned to the caller.
 * Returns      : None.
 ***************************************************************************/
static void DoSendLoopback( unsigned long arg )
{
    P8021AG_LOOPBACK KArg;

    DPRINTK("p8021ag: DoSendLoopback entry\n");

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        PP8021AG_INFO pInfo = &g_P8021agInfo;

        if( pInfo->nLbmStatus == 0 )
        {
            /* Send an Lbm and wait for the Lbr. */
            KArg.nResult = SendLbm( pInfo, &KArg );
        }
        else
        {
            /* A loopback operation is inprogress or the 802.1ag receive handler
             * is not registered (start was not called).
             */
            KArg.nResult = (pInfo->nLbmStatus) ? -EBUSY : -EPERM;
        }

        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
    }

    DPRINTK("p8021ag: DoSendLoopback exit, result=%d\n", KArg.nResult);
} /* DoSendLoopback */


/***************************************************************************
 * Function Name: DoSendLinktrace
 * Description  : Send an 802.1ag Linktrace Message (LTM) and wait for a
 *                Linktrace Reply (LTR).  The status of the operation and
 *                reply information is returned to the caller.
 * Returns      : None.
 ***************************************************************************/
static void DoSendLinktrace( unsigned long arg )
{
    P8021AG_LINKTRACE KArg;

    DPRINTK("p8021ag: DoSendLinktrace entry\n");

    if( copy_from_user( &KArg, (void *) arg, sizeof(KArg) ) == 0 )
    {
        PP8021AG_INFO pInfo = &g_P8021agInfo;

        if( !pInfo->nLtmStatus )
        {
            /* Send an Ltm and wait for the Ltr. */
            KArg.nResult = SendLtm( pInfo, &KArg );
        }
        else
        {
            /* A linktrace operation is inprogress or the 802.1ag receive handler
             * is not registered (start was not called).
             */
            KArg.nResult = (pInfo->nLtmStatus) ? -EBUSY : -EPERM;
        }

        copy_to_user( (void *) arg, &KArg, sizeof(KArg) );
    }

    DPRINTK("p8021ag: DoSendLinktrace exit, result=%d\n", KArg.nResult);
} /* DoSendLinktrace */


/***************************************************************************
 * Function Name: SendLbm
 * Description  : Send an 802.1ag Loopback Message (LBM) and waits for a
 *                Loopback Reply (LBR).
 * Returns      : 0 if Lbr received, error if not
 ***************************************************************************/
static int SendLbm( PP8021AG_INFO pInfo, PP8021AG_LOOPBACK pLbParms )
{
    const long lLbrToMs = 3000;
    const long lLbrIntervalMs = 100;

    int nRet = pInfo->nLbmStatus = -ETIME; /* default value */
    struct sk_buff *skb = alloc_skb(ETH_ZLEN, GFP_KERNEL);

    if( skb )
    {
        long i;
        unsigned long flags;
        unsigned char *p = skb->data;
        PCFM_COMMON_HDR pCmnHdr;

        memset(skb->data, 0x00, ETH_ZLEN);

        /* Create Lbm packet.  Add VLAN tagged MAC header. */
        memcpy(p, pLbParms->ucMacAddr, ETH_ALEN);
        p += ETH_ALEN;
        memcpy(p, pInfo->dev->dev_addr, ETH_ALEN);
        p += ETH_ALEN;
        *(unsigned short *) p = ETH_P_8021Q;
        p += sizeof(short);
        *(unsigned short *) p = pInfo->usLbmVlanId = pLbParms->usVlanId;
        p += sizeof(short);
        *(unsigned short *) p = ETH_P_8021AG;
        p += sizeof(short);

        /* Add CFM common header. */
        pCmnHdr = (PCFM_COMMON_HDR) p;
        pCmnHdr->ucMdLevelVersion = CFM_MD_LEVEL(pInfo->ulMdLevel);
        pCmnHdr->ucOpCode = CFM_LBM;
        pCmnHdr->ucFlags = 0;
        pCmnHdr->ucFirstTlvOffset = CFM_FTO_LBM;

        /* Add transaction id. */
        pInfo->ulCurrLbmTransId = pInfo->ulNextLbmTransId++;
        p += sizeof(CFM_COMMON_HDR);
        memcpy(p, &pInfo->ulCurrLbmTransId, sizeof(pInfo->ulCurrLbmTransId));

        /* Add End TLV. */
        p += pCmnHdr->ucFirstTlvOffset;
        *p++ = '\0';
        *p++ = '\0';
        *p++ = '\0';

        /* Fix up skb. */
        skb->tail = p;
        if( (skb->len = skb->tail - skb->data) < ETH_ZLEN )
            skb->len = ETH_ZLEN;
        skb->dev = pInfo->dev;

        DPRINTK("p8021ag: SendLbm - skb->data=0x%8.8lx, skb->len=%d\n",
            (unsigned long) skb->data, (int) skb->len);
        DUMPADDR(skb->data, skb->len);

        /* Queue the LBM to send. */
        local_irq_save(flags);
        local_irq_enable();
        dev_queue_xmit(skb) ;
        local_irq_restore(flags);

        /* Wait for a LBR. */
        for(i = 0; i < lLbrToMs && pInfo->nLbmStatus != 0; i += lLbrIntervalMs)
        {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(1 + ((HZ * lLbrIntervalMs) / 1000));  
        }

        nRet = pInfo->nLbmStatus;
    }

    pInfo->nLbmStatus = 0;
    DPRINTK("p8021ag: SendLbm - ret=%d\n", nRet);
    return( nRet );
} /* SendLbm */


/***************************************************************************
 * Function Name: SendLtm
 * Description  : Send an 802.1ag Loopback Message (LTM) and waits for a
 *                Loopback Reply (LTR).
 * Returns      : 0 if Ltr received, error if not
 ***************************************************************************/
static int SendLtm( PP8021AG_INFO pInfo, PP8021AG_LINKTRACE pLtParms )
{
    const long lLtrToSecs = 3;
    const unsigned char LtmMacAddrs[][ETH_ALEN] = {LTM_MAC_L0, LTM_MAC_L1,
        LTM_MAC_L2, LTM_MAC_L3, LTM_MAC_L4, LTM_MAC_L5, LTM_MAC_L6, LTM_MAC_L7};

    int nRet = pInfo->nLtmStatus = -ETIME; /* default value */
    struct sk_buff *skb = alloc_skb(ETH_ZLEN, GFP_KERNEL);

    if( skb )
    {
        long i;
        unsigned long flags;
        unsigned char *p = skb->data;
        PCFM_COMMON_HDR pCmnHdr;

        memset(skb->data, 0x00, ETH_ZLEN);

        /* Create Ltm packet.  Add VLAN tagged MAC header. */
        memcpy(p, LtmMacAddrs[pInfo->ulMdLevel], ETH_ALEN);
        p += ETH_ALEN;
        memcpy(p, pInfo->dev->dev_addr, ETH_ALEN);
        p += ETH_ALEN;
        *(unsigned short *) p = ETH_P_8021Q;
        p += sizeof(short);
        *(unsigned short *) p = pInfo->usLtmVlanId = pLtParms->usVlanId;
        p += sizeof(short);
        *(unsigned short *) p = ETH_P_8021AG;
        p += sizeof(short);

        /* Add CFM common header. */
        pCmnHdr = (PCFM_COMMON_HDR) p;
        pCmnHdr->ucMdLevelVersion = CFM_MD_LEVEL(pInfo->ulMdLevel);
        pCmnHdr->ucOpCode = CFM_LTM;
        pCmnHdr->ucFlags = 0;
        pCmnHdr->ucFirstTlvOffset = CFM_FTO_LTM;

        /* Add transaction id. */
        pInfo->ulCurrLtmTransId = pInfo->ulNextLtmTransId++;
        p += sizeof(CFM_COMMON_HDR);
        memcpy(p, &pInfo->ulCurrLtmTransId, sizeof(pInfo->ulCurrLtmTransId));
        p += sizeof(long);

        /* Add Time To Live (TTL). */
        *p++ = 64;

        /* Add original and target MAC addresses. */
        memcpy(p, pInfo->dev->dev_addr, ETH_ALEN);
        p += ETH_ALEN;
        memcpy(p, pLtParms->ucMacAddr, ETH_ALEN);
        p += ETH_ALEN;

        /* Add LTM Egress Identifier TLV. */
        p = (unsigned char *) pCmnHdr + sizeof(CFM_COMMON_HDR) +
            pCmnHdr->ucFirstTlvOffset;
        *p = TLV_TYPE_LTM_EGRESS_ID;
        TLV_SET_LEN(p, TLV_LEN_LTM_EGRESS_ID);
		p += TLV_START_DATA;
        *p++ = '\0';
        *p++ = '\0';
        memcpy(p, pInfo->dev->dev_addr, ETH_ALEN);
        p += ETH_ALEN;

        /* Add End TLV. */
        *p++ = '\0';
        *p++ = '\0';
        *p++ = '\0';

        /* Fix up skb. */
        skb->tail = p;
        if( (skb->len = skb->tail - skb->data) < ETH_ZLEN )
            skb->len = ETH_ZLEN;
        skb->dev = pInfo->dev;

        /* Initialize the LTR list. */
        memset(pInfo->ucLtrList, 0x00, sizeof(pInfo->ucLtrList));

        DPRINTK("p8021ag: SendLtm - skb->data=0x%8.8lx, skb->len=%d\n",
            (unsigned long) skb->data, (int) skb->len);
        DUMPADDR(skb->data, skb->len);

        /* Queue the LTM to send. */
        local_irq_save(flags);
        local_irq_enable();
        dev_queue_xmit(skb) ;
        local_irq_restore(flags);

        /* Wait for LTRs. */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ * lLtrToSecs);  

        /* Return LTR list from highest TTL to lowest TTL. */
        for( i = LTM_TTL_VALUE - 1; i >= 0 &&
             pLtParms->nMacAddrListLen < MAX_LINKTRACE_REPLIES; i-- )
        {
            if(memcmp(pInfo->ucLtrList[i],"\x00\x00\x00\x00\x00\x00",ETH_ALEN))
            {
                memcpy(pLtParms->ucMacAddrList[pLtParms->nMacAddrListLen++],
                    pInfo->ucLtrList[i], ETH_ALEN);
            }
        }

        nRet = 0;
    }

    pInfo->nLtmStatus = 0;
    DPRINTK("p8021ag: SendLtm - ret=%d, mac list size=%d\n", nRet,
        pLtParms->nMacAddrListLen);
    return( nRet );
} /* SendLtm */


/***************************************************************************
 * Function Name: p8021ag_receive
 * Description  : Process a received 802.1ag packet.
 * Returns      : 0 successful, -1 failed
 ***************************************************************************/
static int p8021ag_receive( struct sk_buff *skb )
{
    PP8021AG_INFO pInfo = &g_P8021agInfo;
    PCFM_COMMON_HDR pCmnHdr;

    if( skb->dev != pInfo->dev )
        return -1;
	
    if( *(unsigned short *) (skb->data - 2) == ETH_P_8021Q )
        pCmnHdr = (PCFM_COMMON_HDR) (skb->data + VLAN_HLEN);
    else
        pCmnHdr = (PCFM_COMMON_HDR) skb->data;

    skb_push(skb, ETH_HLEN);

    DPRINTK("p8021ag: p8021ag_receive entry, dev=0x%8.8lx, "
        "dev->name=%s\n", (unsigned long) skb->dev, skb->dev->name);
    DUMPADDR(skb->data, skb->len);

    switch( pCmnHdr->ucOpCode )
    {
    case CFM_LBM:
        ProcessLbm(pCmnHdr, skb, pInfo);
        break;

    case CFM_LBR:
        ProcessLbr(pCmnHdr, skb, pInfo);
        break;

    case CFM_LTM:
        ProcessLtm(pCmnHdr, skb, pInfo);
        break;

    case CFM_LTR:
        ProcessLtr(pCmnHdr, skb, pInfo);
        break;

    default:
        DPRINTK("p8021ag: Unsupported CFM opcode %u.  Free skb.\n",
            pCmnHdr->ucOpCode);
	    kfree_skb(skb);
    }

    return(0);
} /* p8021ag_receive */


/***************************************************************************
 * Function Name: ProcessLbm
 * Description  : Process a received 802.1ag Loopback Message.  It send a
 *                Loopback response.
 * Returns      : None.
 ***************************************************************************/
static void ProcessLbm( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo )
{
    /* Only process if the destination MAC address matches the device's MAC
     * address.
     */
    if( !memcmp(skb->data, skb->dev->dev_addr, ETH_ALEN) )
    {
        unsigned long flags;
        unsigned char macaddr[ETH_ALEN];

        /* Create and send a Loopback Response.  Swap destination and source
         * MAC addresses.
         */
        memcpy(macaddr, skb->data, ETH_ALEN);
        memcpy(skb->data, skb->data + ETH_ALEN, ETH_ALEN);
        memcpy(skb->data + ETH_ALEN, macaddr, ETH_ALEN);

        pCmnHdr->ucOpCode = CFM_LBR;

        DPRINTK("p8021ag: ProcessLbm - send LBR, skb->data=0x%8.8lx, "
            "skb->len=%d\n", (unsigned long) skb->data, (int) skb->len);
        DUMPADDR(skb->data, skb->len);

        /* Queue the LBR to send on the interface on which it was received. */
        local_irq_save(flags);
        local_irq_enable();
        dev_queue_xmit(skb) ;
        local_irq_restore(flags);
    }
    else
    {
        DPRINTK("p8021ag: Discarding received LBM\n");
	    kfree_skb(skb);
    }
} /* ProcessLbm */


/***************************************************************************
 * Function Name: ProcessLbr
 * Description  : Process a received 802.1ag Loopback Response.  If the LTR
 *                has the pending transaction id, it sets a status that the
 *                LBR for a pending LBM has been received.
 * Returns      : None.
 ***************************************************************************/
static void ProcessLbr( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo )
{
    /* Only process if the destination MAC address matches the device's MAC
     * address, the VLAN id matches the the LBM VLAN id, the transaction id
     * matches the LBM transaction id and the MD level is the configured MD
     * level for this device.
     */
    if( !memcmp(skb->data, skb->dev->dev_addr, ETH_ALEN) &&
        *(unsigned short *) (skb->data + ETH_HLEN) == pInfo->usLbmVlanId &&
        *(unsigned long *) (pCmnHdr + 1) == pInfo->ulCurrLbmTransId &&
        (pCmnHdr->ucMdLevelVersion & CFM_MD_LEVEL_MASK) ==
            CFM_MD_LEVEL(pInfo->ulMdLevel) )
    {
        pInfo->nLbmStatus = 0;
    }
    else
        DPRINTK("p8021ag: ProcessLbr - no match\n");
    kfree_skb(skb);
} /* ProcessLbr */


/***************************************************************************
 * Function Name: ProcessLtm
 * Description  : Process a received 802.1ag Linktrace Message.  It sends a
 *                Linktrace response.
 *
 *                This function only supports a small subset of Linktrace
 *                Message processing.  It expects the Target MAC address to
 *                its interfaces MAC address and the level to be its level.
 *                Otherwise the LTM is discarded. It does not forward the LTM.
 *
 * Returns      : None.
 ***************************************************************************/
static void ProcessLtm( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo )
{
    unsigned char *p = (unsigned char *) pCmnHdr;

    /* Only process if the target MAC address matches the device's MAC
     * address.
     */
    if( !memcmp((char *) pCmnHdr + LTM_TARG_MAC_ADDR_OFS, skb->dev->dev_addr, ETH_ALEN) && 
		(pCmnHdr->ucMdLevelVersion & CFM_MD_LEVEL_MASK) == CFM_MD_LEVEL(pInfo->ulMdLevel) &&
		pCmnHdr->ucFirstTlvOffset == CFM_FTO_LTM && //check first TLV offset
		*((unsigned char *) pCmnHdr + sizeof(CFM_COMMON_HDR) + pCmnHdr->ucFirstTlvOffset) == TLV_TYPE_LTM_EGRESS_ID && 
		p[LTM_TTL_OFS] > 0)
    {
        unsigned long flags;
        unsigned char *pTlv, *p2, *pucLtm;
        int nLen = skb->len - sizeof(CFM_COMMON_HDR) + pCmnHdr->ucFirstTlvOffset;

        if((pucLtm = (unsigned char *)kmalloc(ETH_DATA_LEN, GFP_KERNEL))!=NULL)
        {
            /* Save the Linktrace TLVs into a local variable. */
            memcpy(pucLtm, p+sizeof(CFM_COMMON_HDR)+pCmnHdr->ucFirstTlvOffset,
                (nLen < ETH_DATA_LEN) ? nLen : ETH_DATA_LEN);

            /* Create a Linktrace Response. */
            memcpy(skb->data, (char *)pCmnHdr+LTM_ORIG_MAC_ADDR_OFS, ETH_ALEN);
            memcpy(skb->data + ETH_ALEN, skb->dev->dev_addr, ETH_ALEN);

            /* Leave the VLAN and MAC type fields the same as the LTM. */
            pCmnHdr->ucMdLevelVersion = CFM_MD_LEVEL(pInfo->ulMdLevel);
            pCmnHdr->ucOpCode = CFM_LTR;
            pCmnHdr->ucFlags &=
                ~(CFM_FLAG_LTR_FWD_YES | CFM_FLAG_LTR_TERMINAL_MEP);
            pCmnHdr->ucFirstTlvOffset = CFM_FTO_LTR;
            p[LTR_TTL_OFS]--;
            p[LTR_RELAY_OFS] = RLY_HIT;

            p += LTR_HDR_PLUS_FIRST_TLV_OFS;

            /* Create LTR Eress Identification TLV. */
            *p = TLV_TYPE_LTR_EGRESS_ID;
            TLV_SET_LEN(p, TLV_LEN_LTR_EGRESS_ID);

            /*Copy LTM Egress Id TLV to LTR Egress Id TLV Last Egress Id field*/
            if( (pTlv = FindTlv( TLV_TYPE_LTM_EGRESS_ID, pucLtm)) != NULL )
                memcpy(p+TLV_START_DATA,pTlv+TLV_START_DATA,TLV_GET_LEN(pTlv));
            else
                memset(p + TLV_START_DATA, 0x00, TLV_GET_LEN(pTlv));
            p += TLV_START_DATA + TLV_GET_LEN(pTlv);

            /* Set Next Egress Id to the same value as Last Egress Id. */
            memcpy(p, pTlv + TLV_START_DATA, TLV_GET_LEN(pTlv));
            p += TLV_GET_LEN(pTlv);

            /* TBD. A Reply Ingress TLV might be needed. */

            /* Copy most other LTM TLVs. */
            p2 = pucLtm;
            while( *p2 != TLV_TYPE_END && p2 < pucLtm + 1024 )
            {
                unsigned short usLen = TLV_GET_LEN(p2);

                if(*p2 != TLV_TYPE_SENDER_ID && *p2 != TLV_TYPE_LTM_EGRESS_ID)
                {
                    memcpy( p, p2, TLV_START_DATA + usLen);
                    p  += TLV_START_DATA + usLen;
                }

                p2 += TLV_START_DATA + usLen;
            }

            /* Add End TLV. */
            *p++ = '\0';
            *p++ = '\0';
            *p++ = '\0';

            /* Fix up skb. */
            skb->tail = p;
            if( (skb->len = skb->tail - skb->data) < ETH_ZLEN )
                skb->len = ETH_ZLEN;

            DPRINTK("p8021ag: ProcessLtm - send LTR, skb->data=0x%8.8lx, "
                "skb->len=%d\n", (unsigned long) skb->data, (int) skb->len);
            DUMPADDR(skb->data, skb->len);

            /*Queue the LTR to send on the interface on which it was received*/
            local_irq_save(flags);
            local_irq_enable();
            dev_queue_xmit(skb) ;
            local_irq_restore(flags);

            kfree(pucLtm);
        }
        else
        {
            printk("p8021ag: Memory allocation error processing a received "
                "LTM\n");
	        kfree_skb(skb);
        }
    }
    else
    {
        printk("p8021ag: Discarding received LTM\n");
	    kfree_skb(skb);
    }
} /* ProcessLtm */


/***************************************************************************
 * Function Name: ProcessLtr
 * Description  : Process a received 802.1ag Loopback Response.  If the LTR
 *                has the pending transaction id, it sets a status that the
 *                LTR for a pending LBM has been received.
 * Returns      : None.
 ***************************************************************************/
static void ProcessLtr( PCFM_COMMON_HDR pCmnHdr, struct sk_buff *skb,
    PP8021AG_INFO pInfo )
{
    unsigned char *p = (unsigned char *) pCmnHdr;

    /* Only process if the destination MAC address matches the device's MAC
     * address, the VLAN id matches the the LBM VLAN id, the transaction id
     * matches the LBM transaction id, the MD level is the configured MD level
     * for this device and the TTL value is within the specified range.
     */
    if( !memcmp(skb->data, skb->dev->dev_addr, ETH_ALEN) &&
        *(unsigned short *) (skb->data + ETH_HLEN) == pInfo->usLtmVlanId &&
        *(unsigned long *) (pCmnHdr + 1) == pInfo->ulCurrLtmTransId &&
        (pCmnHdr->ucMdLevelVersion & CFM_MD_LEVEL_MASK) == CFM_MD_LEVEL(pInfo->ulMdLevel) &&
		pCmnHdr->ucFirstTlvOffset == CFM_FTO_LTR && //check first TLV offset
		*((unsigned char *) pCmnHdr + sizeof(CFM_COMMON_HDR) + pCmnHdr->ucFirstTlvOffset) == TLV_TYPE_LTR_EGRESS_ID &&         
        p[LTR_TTL_OFS] < LTM_TTL_VALUE )
    {
        /* Copy source MAC address to LTR list at array index of the TTL. */
        memcpy(pInfo->ucLtrList[p[LTR_TTL_OFS]], skb->data+ETH_ALEN, ETH_ALEN);
    }
    else
        DPRINTK("p8021ag: ProcessLtr - no match\n");
    kfree_skb(skb);
} /* ProcessLtr */


/***************************************************************************
 * Function Name: FindTlv
 * Description  : Finds a TLV structure.
 * Returns      : Pointer to the TLV structure or NULL if not found.
 ***************************************************************************/
static unsigned char *FindTlv( unsigned char ucTlv, unsigned char *pucStart )
{
    unsigned short usLen;
    unsigned char *pucSanityEnd = pucStart + 1024;
    unsigned char *pucRet = NULL;

    while( !pucRet && *pucStart != TLV_TYPE_END && pucStart < pucSanityEnd )
    {
        if( *pucStart == ucTlv )
            pucRet = pucStart;
        else
        {
            usLen = TLV_GET_LEN(pucStart);
            pucStart += TLV_START_DATA + usLen;
        }
    }

    return( pucRet );
} /* FindTlv */


module_init(p8021ag_init);
module_exit(p8021ag_cleanup);
MODULE_LICENSE("Proprietary");
MODULE_VERSION(P8021AG_VERSION);

