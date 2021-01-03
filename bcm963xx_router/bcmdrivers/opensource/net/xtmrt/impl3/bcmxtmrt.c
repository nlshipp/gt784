/*
<:copyright-gpl 
 Copyright 2007 Broadcom Corp. All Rights Reserved. 
 
 This program is free software; you can distribute it and/or modify it 
 under the terms of the GNU General Public License (Version 2) as 
 published by the Free Software Foundation. 
 
 This program is distributed in the hope it will be useful, but WITHOUT 
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 for more details. 
 
 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA. 
:>
*/
/**************************************************************************
 * File Name  : bcmxtmrt.c
 *
 * Description: This file implements BCM6368 ATM/PTM network device driver
 *              runtime processing - sending and receiving data.
 ***************************************************************************/

/* Defines. */
#define VERSION     "0.3"
#define VER_STR     "v" VERSION " " __DATE__ " " __TIME__


/* Includes. */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/ppp_channel.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmppp.h>
#include <linux/blog.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <bcmtypes.h>
#include <bcm_map_part.h>
#include <bcm_intr.h>
#include <board.h>
#include "bcmnet.h"
#include "bcmxtmcfg.h"
#include "bcmxtmrt.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/nbuff.h>
#include "bcmxtmrtimpl.h"
#include "bcmPktDma.h"
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
#include "fap_task.h"
#include "fap_dqm.h"
#include "fcache.h"
#endif

static UINT32 gs_ulLogPorts[]  = {0, 3, 1, 2};
#define PORT_PHYS_TO_LOG(PP) gs_ulLogPorts[PP]

/* Externs. */
extern unsigned long getMemorySize(void);
extern int kerSysGetMacAddress(UINT8 *pucaMacAddr, UINT32 ulId);

/* 32bit context is union of pointer to pdevCtrl and channel number */
#if (NUM_RXDMA_CHANNELS > 4)
#error "Overlaying channel and pDevCtrl into context param needs rework"
#else
#define BUILD_CONTEXT(pGi,channel) \
            (uint32)((uint32)(pGi) | ((uint32)(channel) & 0x3u))
#define CONTEXT_TO_CHANNEL(context)  (int)((context) & 0x3u)
#endif

/* 32-bit recycle context definition */
typedef union {
    struct {
        uint32 reserved     : 29;
        uint32 fapQuickFree :  1;
        uint32 channel      :  2;
    };
    uint32 u32;
} xtm_recycle_context_t;

#define RECYCLE_CONTEXT(_context)  ( (xtm_recycle_context_t *)(&(_context)) )
#define FKB_RECYCLE_CONTEXT(_pFkb) RECYCLE_CONTEXT((_pFkb)->recycle_context)

/* Prototypes. */
int __init bcmxtmrt_init( void );
static void bcmxtmrt_cleanup( void );
static int bcmxtmrt_open( struct net_device *dev );
       int bcmxtmrt_close( struct net_device *dev );
static void bcmxtmrt_timeout( struct net_device *dev );
static struct net_device_stats *bcmxtmrt_query(struct net_device *dev);
static void bcmxtmrt_clrStats(struct net_device *dev);
static int bcmxtmrt_ioctl(struct net_device *dev, struct ifreq *Req, int nCmd);
static int bcmxtmrt_ethtool_ioctl(PBCMXTMRT_DEV_CONTEXT pDevCtx,void *useraddr);
static int bcmxtmrt_atm_ioctl(struct socket *sock, unsigned int cmd,
    unsigned long arg);
static PBCMXTMRT_DEV_CONTEXT FindDevCtx( short vpi, int vci );
static int bcmxtmrt_atmdev_open(struct atm_vcc *pVcc);
static void bcmxtmrt_atmdev_close(struct atm_vcc *pVcc);
static int bcmxtmrt_atmdev_send(struct atm_vcc *pVcc, struct sk_buff *skb);
static int bcmxtmrt_pppoatm_send(struct ppp_channel *pChan,struct sk_buff *skb);
static int bcmxtmrt_xmit( pNBuff_t pNBuff, struct net_device *dev);
static void AddRfc2684Hdr(pNBuff_t *ppNBuff, struct sk_buff **ppNbuffSkb,
    UINT8 **ppData, int * pLen, UINT32 ulHdrType);
static void bcmxtmrt_recycle_skb_or_data(struct sk_buff *skb, unsigned context,
    UINT32 nFlag );
static void bcmxtmrt_recycle(pNBuff_t pNBuff, unsigned context, UINT32 nFlag);
static FN_HANDLER_RT bcmxtmrt_rxisr(int nIrq, void *pRxDma);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
static int bcmxtmrt_poll_napi(struct napi_struct *napi, int budget);
#else
static int bcmxtmrt_poll(struct net_device *dev, int *budget);
#endif
static UINT32 bcmxtmrt_rxtask( UINT32 ulBudget, UINT32 *pulMoreToDo );
static void ProcessRxCell(PBCMXTMRT_GLOBAL_INFO pGi, BcmXtm_RxDma *rxdma, UINT8 *pucData);
static void MirrorPacket( struct sk_buff *skb, char *intfName );
static void bcmxtmrt_timer( PBCMXTMRT_GLOBAL_INFO pGi );
static int DoGlobInitReq( PXTMRT_GLOBAL_INIT_PARMS pGip );
static int DoGlobUninitReq( void );
static int DoCreateDeviceReq( PXTMRT_CREATE_NETWORK_DEVICE pCnd );
static int DoRegCellHdlrReq( PXTMRT_CELL_HDLR pCh );
static int DoUnregCellHdlrReq( PXTMRT_CELL_HDLR pCh );
static int DoLinkStsChangedReq( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc );
static int DoLinkUp( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                     PXTMRT_LINK_STATUS_CHANGE pLsc,
                     UINT32 ulDevId);
static int DoLinkDownRx( UINT32 ulPortId );
static int DoLinkDownTx( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc );
static int DoSetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId );
static int DoUnsetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId );
static int DoSendCellReq( PBCMXTMRT_DEV_CONTEXT pDevCtx, PXTMRT_CELL pC );
static int DoDeleteDeviceReq( PBCMXTMRT_DEV_CONTEXT pDevCtx );
static int DoGetNetDevTxChannel( PXTMRT_NETDEV_TXCHANNEL pParm );
static int bcmxtmrt_add_proc_files( void );
static int bcmxtmrt_del_proc_files( void );
static int ProcDmaTxInfo(char *page, char **start, off_t off, int cnt, 
    int *eof, void *data);
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
void bcmxtmrt_update_hw_stats(void * flow_vp, unsigned int hits, unsigned int octets);
#endif

/* Globals. */
BCMXTMRT_GLOBAL_INFO g_GlobalInfo;
static struct atm_ioctl g_PppoAtmIoctlOps =
    {
        .ioctl    = bcmxtmrt_atm_ioctl,
    };
static struct ppp_channel_ops g_PppoAtmOps =
    {
        .start_xmit = bcmxtmrt_pppoatm_send
    };
static const struct atmdev_ops g_AtmDevOps =
    {
        .open       = bcmxtmrt_atmdev_open,
        .close      = bcmxtmrt_atmdev_close,
        .send       = bcmxtmrt_atmdev_send,
    };

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
static const struct header_ops bcmXtmRt_headerOps = {
    .parse = NULL
};

static const struct net_device_ops bcmXtmRt_netdevops = {

    .ndo_open           = bcmxtmrt_open,
    .ndo_stop           = bcmxtmrt_close,
    .ndo_start_xmit     = (HardStartXmitFuncP)bcmxtmrt_xmit,
    .ndo_do_ioctl       = bcmxtmrt_ioctl,
    .ndo_tx_timeout     = bcmxtmrt_timeout,
    .ndo_get_stats      = bcmxtmrt_query
 };
#endif

static int bcmxtmrt_in_init_dev = 0;

/***************************************************************************
 * Function Name: bcmxtmrt_init
 * Description  : Called when the driver is loaded.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int __init bcmxtmrt_init( void )
{
    UINT16 usChipId  = (PERF->RevID & 0xFFFE0000) >> 16;
    UINT16 usChipRev = (PERF->RevID & 0xFF);

    printk(CARDNAME ": Broadcom BCM%X%X ATM/PTM Network Device ", usChipId,
        usChipRev);
    printk(VER_STR "\n");

    g_pXtmGlobalInfo = &g_GlobalInfo;

    memset(&g_GlobalInfo, 0x00, sizeof(g_GlobalInfo));

    g_GlobalInfo.ulChipRev = PERF->RevID;
    register_atm_ioctl(&g_PppoAtmIoctlOps);
    g_GlobalInfo.pAtmDev = atm_dev_register("bcmxtmrt_atmdev", &g_AtmDevOps,
        -1, NULL);
    if( g_GlobalInfo.pAtmDev )
    {
        g_GlobalInfo.pAtmDev->ci_range.vpi_bits = 12;
        g_GlobalInfo.pAtmDev->ci_range.vci_bits = 16;
    }

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
    /* Allocate smaller number of xtm tx BDs in FAP builds. BDs are stored in PSM - Apr 2010 */
    g_GlobalInfo.ulNumExtBufs = XTM_NR_TX_BDS;   /* was NR_TX_BDS */
#else
    /* Allocate original number of xtm tx BDs in non-FAP builds. BDs are stored in DDR - Apr 2010 */
    g_GlobalInfo.ulNumExtBufs = NR_RX_BDS(getMemorySize());
#endif

    g_GlobalInfo.ulNumExtBufsRsrvd = g_GlobalInfo.ulNumExtBufs / 5;
    g_GlobalInfo.ulNumExtBufs90Pct = (g_GlobalInfo.ulNumExtBufs * 9) / 10;
    g_GlobalInfo.ulNumExtBufs50Pct = g_GlobalInfo.ulNumExtBufs / 2;

    bcmxtmrt_add_proc_files();

    return 0;
} /* bcmxtmrt_init */


/***************************************************************************
 * Function Name: bcmxtmrt_cleanup
 * Description  : Called when the driver is unloaded.
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_cleanup( void )
{
    bcmxtmrt_del_proc_files();
    deregister_atm_ioctl(&g_PppoAtmIoctlOps);
    if( g_GlobalInfo.pAtmDev )
    {
        atm_dev_deregister( g_GlobalInfo.pAtmDev );
        g_GlobalInfo.pAtmDev = NULL;
    }
} /* bcmxtmrt_cleanup */


/***************************************************************************
 * Function Name: bcmxtmrt_open
 * Description  : Called to make the device operational.  Called due to shell
 *                command, "ifconfig <device_name> up".
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_open( struct net_device *dev )
{
    int nRet = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
#else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
#endif

    BCM_XTM_DEBUG("bcmxtmrt_open\n");

    netif_start_queue(dev);

    if( pDevCtx->ulAdminStatus == ADMSTS_UP )
        pDevCtx->ulOpenState = XTMRT_DEV_OPENED;
    else
        nRet = -EIO;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    napi_enable(&pDevCtx->napi);
#endif

/* FAP enables RX in open as packets can/will arrive between
   link up and open which cause the interrupt to not be reset  */
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
    {
        int i;

        for (i = 0; i < MAX_RECEIVE_QUEUES; i++)
            bcmPktDma_XtmRxEnable(i);
    }
#endif

    return( nRet );
} /* bcmxtmrt_open */


/***************************************************************************
 * Function Name: bcmxtmrt_close
 * Description  : Called to stop the device.  Called due to shell command,
 *                "ifconfig <device_name> down".
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int bcmxtmrt_close( struct net_device *dev )
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
#else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
#endif

    if( pDevCtx->ulOpenState != XTMRT_DEV_CLOSED )
    {
        BCM_XTM_DEBUG("bcmxtmrt_close\n");

        pDevCtx->ulOpenState = XTMRT_DEV_CLOSED;
        netif_stop_queue(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        napi_disable(&pDevCtx->napi);
#endif
    }
        
    return 0;
} /* bcmxtmrt_close */


/***************************************************************************
 * Function Name: bcmxtmrt_timeout
 * Description  : Called when there is a transmit timeout. 
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_timeout( struct net_device *dev )
{
    dev->trans_start = jiffies;
    netif_wake_queue(dev);
} /* bcmxtmrt_timeout */


/***************************************************************************
 * Function Name: bcmxtmrt_query
 * Description  : Called to return device statistics. 
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static struct net_device_stats *bcmxtmrt_query(struct net_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
#else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
#endif
    struct net_device_stats *pStats = &pDevCtx->DevStats;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    if( pDevCtx->ucTxVcid != INVALID_VCID ) {
        pStats->tx_bytes += pGi->pulMibTxOctetCountBase[pDevCtx->ucTxVcid];
    }

    if ((pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
          &&
        (pDevCtx->ulHdrType == HT_PTM)) {
       pStats->rx_bytes   = pGi->ptmBondInfo.bStats.rxo ;
       pStats->rx_packets = (pGi->ptmBondInfo.bStats.rxp +
                             pGi->ptmBondInfo.bStats.rxpl) ;
    }
    else {
       for( i = 0; i < MAX_DEFAULT_MATCH_IDS; i++ )
       {
          if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
          {
             *pGi->pulMibRxMatch = i;
             pStats->rx_bytes = *pGi->pulMibRxOctetCount;
             *pGi->pulMibRxMatch = i;
             pStats->rx_packets = *pGi->pulMibRxPacketCount;

             /* By convension, VCID 0 collects statistics for packets that use
              * Packet CMF rules.
              */
             if( i == 0 )
             {
                UINT32 j;
                for( j = MAX_DEFAULT_MATCH_IDS + 1; j < MAX_MATCH_IDS; j++ )
                {
                   *pGi->pulMibRxMatch = j;
                   pStats->rx_bytes += *pGi->pulMibRxOctetCount;
                   *pGi->pulMibRxMatch = j;
                   pStats->rx_packets += *pGi->pulMibRxPacketCount;
                }
             } /* if (i) */
          }
       } /* for (i) */
    }

    return( pStats );
} /* bcmxtmrt_query */

/***************************************************************************
 * Function Name: bcmxtmrt_clrStats
 * Description  : Called to clear device statistics. 
 * Returns      : None
 ***************************************************************************/
static void bcmxtmrt_clrStats(struct net_device *dev)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
#else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
#endif
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    *pGi->pulMibRxCtrl |= pGi->ulMibRxClrOnRead;
    bcmxtmrt_query(dev);
    *pGi->pulMibRxCtrl &= ~pGi->ulMibRxClrOnRead;
    memset(&pDevCtx->DevStats, 0, sizeof(struct net_device_stats));
} /* bcmxtmrt_clrStats */

/***************************************************************************
 * Function Name: bcmxtmrt_ioctl
 * Description  : Driver IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_ioctl(struct net_device *dev, struct ifreq *Req, int nCmd)
{
 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
 #else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
 #endif
    int *data=(int*)Req->ifr_data;
    int status;
    MirrorCfg mirrorCfg;
    int nRet = 0;

    switch (nCmd)
    {
    case SIOCGLINKSTATE:
        if( pDevCtx->ulLinkState == LINK_UP )
            status = LINKSTATE_UP;
        else
            status = LINKSTATE_DOWN;
        if (copy_to_user((void*)data, (void*)&status, sizeof(int)))
            nRet = -EFAULT;
        break;

    case SIOCSCLEARMIBCNTR:
        bcmxtmrt_clrStats(dev);
        break;

    case SIOCMIBINFO:
        if (copy_to_user((void*)data, (void*)&pDevCtx->MibInfo,
            sizeof(pDevCtx->MibInfo)))
        {
            nRet = -EFAULT;
        }
        break;

    case SIOCPORTMIRROR:
        if(copy_from_user((void*)&mirrorCfg,data,sizeof(MirrorCfg)))
            nRet=-EFAULT;
        else
        {
            if( mirrorCfg.nDirection == MIRROR_DIR_IN )
            {
                if( mirrorCfg.nStatus == MIRROR_ENABLED )
                    strcpy(pDevCtx->szMirrorIntfIn, mirrorCfg.szMirrorInterface);
                else
                    memset(pDevCtx->szMirrorIntfIn, 0x00, MIRROR_INTF_SIZE);
            }
            else /* MIRROR_DIR_OUT */
            {
                if( mirrorCfg.nStatus == MIRROR_ENABLED )
                    strcpy(pDevCtx->szMirrorIntfOut, mirrorCfg.szMirrorInterface);
                else
                    memset(pDevCtx->szMirrorIntfOut, 0x00, MIRROR_INTF_SIZE);
            }
        }
        break;

    case SIOCETHTOOL:
        nRet = bcmxtmrt_ethtool_ioctl(pDevCtx, (void *) Req->ifr_data);
        break;

    default:
        nRet = -EOPNOTSUPP;    
        break;
    }

    return( nRet );
} /* bcmxtmrt_ioctl */


/***************************************************************************
 * Function Name: bcmxtmrt_ethtool_ioctl
 * Description  : Driver ethtool IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_ethtool_ioctl(PBCMXTMRT_DEV_CONTEXT pDevCtx, void *useraddr)
{
    struct ethtool_drvinfo info;
    struct ethtool_cmd ecmd;
    unsigned long ethcmd;
    int nRet = 0;

    if( copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)) == 0 )
    {
        switch (ethcmd)
        {
        case ETHTOOL_GDRVINFO:
            info.cmd = ETHTOOL_GDRVINFO;
            strncpy(info.driver, CARDNAME, sizeof(info.driver)-1);
            strncpy(info.version, VERSION, sizeof(info.version)-1);
            if (copy_to_user(useraddr, &info, sizeof(info)))
                nRet = -EFAULT;
            break;

        case ETHTOOL_GSET:
            ecmd.cmd = ETHTOOL_GSET;
            ecmd.speed = pDevCtx->MibInfo.ulIfSpeed / (1024 * 1024);
            if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
                nRet = -EFAULT;
            break;

        default:
            nRet = -EOPNOTSUPP;    
            break;
        }
    }
    else
       nRet = -EFAULT;

    return( nRet );
}

/***************************************************************************
 * Function Name: bcmxtmrt_atm_ioctl
 * Description  : Driver ethtool IOCTL entry point.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atm_ioctl(struct socket *sock, unsigned int cmd,
    unsigned long arg)
{
    struct atm_vcc *pAtmVcc = ATM_SD(sock);
    void __user *argp = (void __user *)arg;
    atm_backend_t b;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    int nRet = -ENOIOCTLCMD;

    switch( cmd )
    {
    case ATM_SETBACKEND:
        if( get_user(b, (atm_backend_t __user *) argp) == 0 )
        {
            switch (b)
            {
            case ATM_BACKEND_PPP_BCM:
                if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci))!=NULL &&
                    pDevCtx->Chan.private == NULL )
                {
                    pDevCtx->Chan.private = pDevCtx->pDev;
                    pDevCtx->Chan.ops = &g_PppoAtmOps;
                    pDevCtx->Chan.mtu = 1500; /* TBD. Calc value. */
                    pAtmVcc->user_back = pDevCtx;
                    if( ppp_register_channel(&pDevCtx->Chan) == 0 )
                        nRet = 0;
                    else
                        nRet = -EFAULT;
                }
                else
                    nRet = (pDevCtx) ? 0 : -EFAULT;
                break;

            case ATM_BACKEND_PPP_BCM_DISCONN:
                /* This is a patch for PPP reconnection.
                 * ppp daemon wants us to send out an LCP termination request
                 * to let the BRAS ppp server terminate the old ppp connection.
                 */
                if((pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL)
                {
                    struct sk_buff *skb;
                    int size = 6;
                    int eff  = (size+3) & ~3; /* align to word boundary */

                    while (!(skb = alloc_skb(eff, GFP_KERNEL)))
                        schedule();

                    skb->dev = NULL; /* for paths shared with net_device interfaces */
                    skb_put(skb, size);

                    skb->data[0] = 0xc0;  /* PPP_LCP == 0xc021 */
                    skb->data[1] = 0x21;
                    skb->data[2] = 0x05;  /* TERMREQ == 5 */
                    skb->data[3] = 0x02;  /* id == 2 */
                    skb->data[4] = 0x00;  /* HEADERLEN == 4 */
                    skb->data[5] = 0x04;

                    if (eff > size)
                        memset(skb->data+size,0,eff-size);

                    nRet = bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
                }
                else
                    nRet = -EFAULT;
                break;

            case ATM_BACKEND_PPP_BCM_CLOSE_DEV:
                if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL)
                {
                    bcmxtmrt_pppoatm_send(&pDevCtx->Chan, NULL);
                    ppp_unregister_channel(&pDevCtx->Chan);
                    pDevCtx->Chan.private = NULL;
                }
                nRet = 0;
                break;

            default:
                break;
            }
        }
        else
            nRet = -EFAULT;
        break;

    case PPPIOCGCHAN:
        if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL )
        {
            nRet = put_user(ppp_channel_index(&pDevCtx->Chan),
                (int __user *) argp) ? -EFAULT : 0;
        }
        else
            nRet = -EFAULT;
        break;

    case PPPIOCGUNIT:
        if( (pDevCtx = FindDevCtx(pAtmVcc->vpi, pAtmVcc->vci)) != NULL )
        {
            nRet = put_user(ppp_unit_number(&pDevCtx->Chan),
                (int __user *) argp) ? -EFAULT : 0;
        }
        else
            nRet = -EFAULT;
        break;
    default:
        break;
    }

    return( nRet );
} /* bcmxtmrt_atm_ioctl */


/***************************************************************************
 * Function Name: FindDevCtx
 * Description  : Finds a device context structure for a VCC.
 * Returns      : Pointer to a device context structure or NULL.
 ***************************************************************************/
static PBCMXTMRT_DEV_CONTEXT FindDevCtx( short vpi, int vci )
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = NULL;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        if( (pDevCtx = pGi->pDevCtxs[i]) != NULL )
        {
            if( pDevCtx->Addr.u.Vcc.usVpi == vpi &&
                pDevCtx->Addr.u.Vcc.usVci == vci )
            {
                break;
            }

            pDevCtx = NULL;
        }
    }

    return( pDevCtx );
} /* FindDevCtx */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_open
 * Description  : ATM device open
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atmdev_open(struct atm_vcc *pVcc)
{
    set_bit(ATM_VF_READY,&pVcc->flags);
    return( 0 );
} /* bcmxtmrt_atmdev_open */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_close
 * Description  : ATM device open
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static void bcmxtmrt_atmdev_close(struct atm_vcc *pVcc)
{
    clear_bit(ATM_VF_READY,&pVcc->flags);
    clear_bit(ATM_VF_ADDR,&pVcc->flags);
} /* bcmxtmrt_atmdev_close */


/***************************************************************************
 * Function Name: bcmxtmrt_atmdev_send
 * Description  : send data
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_atmdev_send(struct atm_vcc *pVcc, struct sk_buff *skb)
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = FindDevCtx( pVcc->vpi, pVcc->vci );
    int nRet;

    if( pDevCtx )
        nRet = bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
    else
        nRet = -EIO;

    return( nRet );
} /* bcmxtmrt_atmdev_send */



/***************************************************************************
 * Function Name: bcmxtmrt_pppoatm_send
 * Description  : Called by the PPP driver to send data.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_pppoatm_send(struct ppp_channel *pChan, struct sk_buff *skb)
{
    bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), (struct net_device *) pChan->private );
    return(1);
} /* bcmxtmrt_pppoatm_send */


/***************************************************************************
 * Function Name: QueuePacket
 * Description  : Determines whether to queue a packet for transmission based
 *                on the number of total external (ie Ethernet) buffers and
 *                buffers already queued.
 *                For all ATM cells (ASM, OAM which are locally originated and 
 *                mgmt based), we allow them to get queued as they are critical
 *                & low frequency based.
 *                For ex., if we drop sucessive ASM cels during congestion (the whole bonding
 *                layer will be reset end to end). So, the criteria here should
 *                be applied more for data packets than for mgmt cells.
 * Returns      : 1 to queue packet, 0 to drop packet
 ***************************************************************************/
inline int QueuePacket( PBCMXTMRT_GLOBAL_INFO pGi, PTXQINFO pTqi, UINT32 isAtmCell )
{
    int nRet = 0; /* default to drop packet */

    if( pGi->ulNumTxQs == 1 )
    {
        /* One total transmit queue.  Allow up to 90% of external buffers to
         * be queued on this transmit queue.
         */
        if(( pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufs90Pct )
              ||
           (isAtmCell))
        {
            nRet = 1; /* queue packet */
            pGi->ulDbgQ1++;
        }
        else
            pGi->ulDbgD1++;
    }
    else
    {
        if(pGi->ulNumExtBufs - pGi->ulNumTxBufsQdAll > pGi->ulNumExtBufsRsrvd)
        {
            /* The available number of external buffers is greater than the
             * reserved value.  Allow up to 50% of external buffers to be
             * queued on this transmit queue.
             */
            if(( pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufs50Pct )
                 ||
                (isAtmCell))
            {
                nRet = 1; /* queue packet */
                pGi->ulDbgQ2++;
            }
            else
                pGi->ulDbgD2++;
        }
        else
        {
            /* Divide the reserved number of external buffers evenly among all
             * of the transmit queues.
             */
            if((pTqi->ulNumTxBufsQdOne < pGi->ulNumExtBufsRsrvd / pGi->ulNumTxQs)
                 ||
                (isAtmCell))
            {
                nRet = 1; /* queue packet */
                pGi->ulDbgQ3++;
            }
            else
                pGi->ulDbgD3++;
        }
    }

    return( nRet );
} /* QueuePacket */


/***************************************************************************
 * Function Name: bcmxtmrt_xmit
 * Description  : Check for transmitted packets to free and, if skb is
 *                non-NULL, transmit a packet. Transmit may be invoked for
 *                a packet originating from the network stack or from a
 *                packet received from another interface.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_xmit( pNBuff_t pNBuff, struct net_device *dev )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    PBCMXTMRT_DEV_CONTEXT pDevCtx = netdev_priv(dev);
#else
    PBCMXTMRT_DEV_CONTEXT pDevCtx = dev->priv;
#endif
    DmaDesc  dmaDesc;

    spin_lock_bh(&pGi->xtmlock_tx);

    if( pDevCtx->ulLinkState == LINK_UP )
    {
        BcmPktDma_XtmTxDma *txdma = NULL;
        UINT8 * pData;
        UINT32 i, channel;
        unsigned len, uMark, uPriority;
        struct sk_buff * pNBuffSkb; /* If pNBuff is sk_buff: protocol access */

        /* Free packets that have been transmitted. */
        for (i=0; i<pDevCtx->ulTxQInfosSize; i++)
        {
            uint32               txSource;
            uint32               txAddr;
            uint32               rxChannel;
            pNBuff_t             nbuff_reclaim_p;

            txdma = pDevCtx->txdma[i];
            while (bcmPktDma_XtmFreeXmitBufGet(txdma->ulDmaIndex,(uint32 *)&nbuff_reclaim_p,
                                               &txSource, &txAddr, &rxChannel) == TRUE)
            {
                if (nbuff_reclaim_p != PNBUFF_NULL) 
                {
                    spin_unlock_bh(&pGi->xtmlock_tx);
                    BCM_XTM_TX_DEBUG("Host bcmPktDma_XtmFreeXmitBufGet TRUE! (xmit) key 0x%x\n", (int)nbuff_reclaim_p);
                    nbuff_free(nbuff_reclaim_p);
                    spin_lock_bh(&pGi->xtmlock_tx);
                }
            }
        }

        if( nbuff_get_params(pNBuff, &pData, &len, &uMark, &uPriority)
            == (void*)NULL )
        {
            goto unlock_done_xmit;
        }

        if (IS_SKBUFF_PTR(pNBuff))
        {
            pNBuffSkb = PNBUFF_2_SKBUFF(pNBuff);

            /* Give the highest possible priority to ARP packets */
            if (pNBuffSkb->protocol == __constant_htons(ETH_P_ARP))
               uMark |= 0x7;
        }
        else
        {
            pNBuffSkb = NULL;
        }

//spin_unlock_bh(&pGi->xtmlock_tx);
//BCM_XTM_TX_DEBUG("XTM TX: pNBuff<0x%08x> pNBuffSkb<0x%08x> pData<0x%08x>\n", (int)pNBuff,(int)pNBuffSkb, (int)pData);
//DUMP_PKT(pData, 32);
//spin_lock_bh(&pGi->xtmlock_tx);

        if( pDevCtx->ulTxQInfosSize )
        {
            /* Find a transmit queue to send on. */
            UINT32 ulPort = 0;
            UINT32 ulPtmPriority = PTM_FLOW_PRI_LOW;
            UINT32 isAtmCell ;
            UINT32 ulTxAvailable = 0;


            isAtmCell = ( pNBuffSkb &&
                  ((pDevCtx->Addr.ulTrafficType & TRAFFIC_TYPE_ATM_MASK) == TRAFFIC_TYPE_ATM)  &&
                  (pNBuffSkb->protocol & ~FSTAT_CT_MASK) == SKB_PROTO_ATM_CELL
                  );

            if (isAtmCell)       /* Necessary for ASM/OAM cells */
               uMark |= 0x7;

#ifdef CONFIG_NETFILTER
            /* bit 2-0 of the 32-bit nfmark is the subpriority (0 to 7) set by ebtables.
             * bit 3   of the 32-bit nfmark is the DSL latency, 0=PATH0, 1=PATH1
             * bit 4   of the 32-bit nfmark is the PTM priority, 0=LOW,  1=HIGH
             */
            uPriority = uMark & 0x07;

            ulPort = (uMark >> 3) & 0x1;  //DSL latency

            if ((pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM) ||
                (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM_BONDED))
               ulPtmPriority = (uMark >> 4) & 0x1;

            txdma = pDevCtx->pTxPriorities[ulPtmPriority][ulPort][uPriority];
        
            /* If a transmit queue was not found, use the existing highest priority queue
             * that had been configured with the default Ptm priority and DSL latency (port).
             */
            if (txdma == NULL && uPriority > 1)
            {
               UINT32 ulPtmPriorityDflt;
               UINT32 ulPortDflt;

               if (pDevCtx->pTxPriorities[0][0][0] == pDevCtx->txdma[0])
               {
                  ulPtmPriorityDflt = 0;
                  ulPortDflt        = 0;
               }
               else if (pDevCtx->pTxPriorities[0][1][0] == pDevCtx->txdma[0])
               {
                  ulPtmPriorityDflt = 0;
                  ulPortDflt        = 1;
               }
               else if (pDevCtx->pTxPriorities[1][0][0] == pDevCtx->txdma[0])
               {
                  ulPtmPriorityDflt = 1;
                  ulPortDflt        = 0;
               }
               else
               {
                  ulPtmPriorityDflt = 1;
                  ulPortDflt        = 1;
               }
               for (i = uPriority - 1; txdma == NULL && i >= 1; i--)
                  txdma = pDevCtx->pTxPriorities[ulPtmPriorityDflt][ulPortDflt][i];
            }
#endif

            /* If a transmit queue was not found, use the first active one. */
            if (NULL == txdma || txdma->txEnabled == 0)
               txdma = pDevCtx->txdma[0];

            if (txdma && txdma->txEnabled == 1)
            {
                channel = txdma->ulDmaIndex;
                ulTxAvailable = bcmPktDma_XtmXmitAvailable(channel);
            }
            else
                ulTxAvailable = 0;

            if (ulTxAvailable && QueuePacket(pGi, txdma, isAtmCell))
            {
                UINT32 ulRfc2684_type; /* Not needed as CMF "F in software" */
                UINT32 ulHdrType = pDevCtx->ulHdrType;

                if( pDevCtx->szMirrorIntfOut[0] != '\0' &&
                    pNBuffSkb && !isAtmCell &&
                    (ulHdrType ==  HT_PTM ||
                     ulHdrType ==  HT_LLC_SNAP_ETHERNET ||
                     ulHdrType ==  HT_VC_MUX_ETHERNET) )
                {
#ifdef CONFIG_BLOG     
                    blog_skip( pNBuffSkb );/* Mirroring not supported for FKB */
#endif
                    MirrorPacket( pNBuffSkb, pDevCtx->szMirrorIntfOut );
                }

                if( (pDevCtx->ulFlags & CNI_HW_ADD_HEADER) == 0 &&
                     HT_LEN(ulHdrType) != 0 && !isAtmCell )
                {
                    ulRfc2684_type = HT_TYPE(ulHdrType);
                }
                else
                    ulRfc2684_type = RFC2684_NONE;

#ifdef CONFIG_BLOG
                blog_emit( pNBuff, dev, pDevCtx->ulEncapType,
                           channel, ulRfc2684_type );
#endif

                if ( ulRfc2684_type )
                {
                    AddRfc2684Hdr(&pNBuff, &pNBuffSkb, &pData, &len, ulHdrType);
                }

                if( len < ETH_ZLEN && !isAtmCell &&
                    (ulHdrType == HT_PTM ||
                     ulHdrType == HT_LLC_SNAP_ETHERNET ||
                     ulHdrType == HT_VC_MUX_ETHERNET) )
                {
                    len = ETH_ZLEN;
                }

                if ((pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
                    &&
                    (ulHdrType == HT_PTM))
                  bcmxtmrt_ptmbond_add_hdr (pDevCtx, ulPtmPriority, &pNBuff, &pNBuffSkb, &pData, &len) ;

#if defined(CONFIG_BCM96368)
                /* Ported from xtmrt/impl1 Apr 2010 */
                if(kerSysGetSdramWidth() == MEMC_16BIT_BUS)
                {
                    pNBuff = nbuff_align_data(pNBuff, &pData, len, NBUFF_ALIGN_MASK_8);
                    if(pNBuff == NULL)
                    {
                        pDevCtx->DevStats.tx_errors++;
                        goto unlock_done_xmit;
                    }
                }
#endif
                nbuff_flush(pNBuff, pData, len);

                dmaDesc.status = DMA_SOP | DMA_EOP | pDevCtx->ucTxVcid;

                if (( pDevCtx->Addr.ulTrafficType & TRAFFIC_TYPE_ATM_MASK ) == TRAFFIC_TYPE_ATM )
                {
                    if (isAtmCell)
                    {
                        dmaDesc.status |= pNBuffSkb->protocol & FSTAT_CT_MASK;
                        if( (pDevCtx->ulFlags & CNI_USE_ALT_FSTAT) != 0 )
                        {
                            dmaDesc.status |= FSTAT_MODE_COMMON;
                            dmaDesc.status &= ~(FSTAT_COMMON_INS_HDR_EN |
                                              FSTAT_COMMON_HDR_INDEX_MASK);
                        }
                    }
                    else
                    {
                        dmaDesc.status |= FSTAT_CT_AAL5;
                        if( (pDevCtx->ulFlags & CNI_USE_ALT_FSTAT) != 0 )
                        {
                            dmaDesc.status |= FSTAT_MODE_COMMON;
                            if(HT_LEN(ulHdrType) != 0 &&
                               (pDevCtx->ulFlags & CNI_HW_ADD_HEADER) != 0)
                            {
                                dmaDesc.status |= FSTAT_COMMON_INS_HDR_EN |
                                                  ((HT_TYPE(ulHdrType) - 1) <<
                                                  FSTAT_COMMON_HDR_INDEX_SHIFT);
                            }
                            else
                            {
                                dmaDesc.status &= ~(FSTAT_COMMON_INS_HDR_EN |
                                                   FSTAT_COMMON_HDR_INDEX_MASK);
                            }
                        }
                    }
                }
                else
                    dmaDesc.status |= FSTAT_CT_PTM | FSTAT_PTM_ENET_FCS |
                                      FSTAT_PTM_CRC;

                dmaDesc.status |= DMA_OWN;

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
                if(IS_FKBUFF_PTR(pNBuff))
                {
                    FkBuff_t *pFkb = PNBUFF_2_FKBUFF(pNBuff);

                    /* We can only use the recycle context if this is our buffer */
                    if(pFkb->recycle_hook == (RecycleFuncP)bcmxtmrt_recycle)
                    {
                        uint8 *pData = PFKBUFF_TO_PDATA(pFkb, RXBUF_HEAD_RESERVE);

                        BCM_XTM_TX_DEBUG("fapQuickFree: pFkb 0x%08X, pData 0x%08X\n", (uint32_t)pFkb, (uint32_t)pData);

                        bcmPktDma_XtmXmit(channel, pData, len, HOST_VIA_DQM_4KE_FREE,
                                          dmaDesc.status, (uint32)pNBuff,
                                          FKB_RECYCLE_CONTEXT(pFkb)->channel);

                        FKB_RECYCLE_CONTEXT(pFkb)->fapQuickFree = 1;

                        spin_unlock_bh(&pGi->xtmlock_tx);
                        nbuff_free(pNBuff);
                        spin_lock_bh(&pGi->xtmlock_tx);

                        FKB_RECYCLE_CONTEXT(pFkb)->fapQuickFree = 0;

                        goto tx_continue;
                    }
                }

                bcmPktDma_XtmXmit(channel, pData, len, HOST_VIA_DQM, dmaDesc.status, (uint32)pNBuff, 0);

tx_continue:
#else
                bcmPktDma_XtmXmit(channel, pData, len, HOST_VIA_LINUX, dmaDesc.status, (uint32)pNBuff, 0);
#endif

                /* Transmitted bytes are counted in hardware. */
                pDevCtx->DevStats.tx_packets++;
                pDevCtx->DevStats.tx_bytes += len;
                if (!isAtmCell) pDevCtx->DevStats.tx_bytes += ETH_CRC_LEN;
                pDevCtx->pDev->trans_start = jiffies;
            }
            else
            {
                /* Transmit queue is full.  Free the socket buffer.  Don't call
                 * netif_stop_queue because this device may use more than one
                 * queue.
                 */
                nbuff_flushfree(pNBuff);                
                pDevCtx->DevStats.tx_errors++;
            }
        }
        else
        {
            nbuff_flushfree(pNBuff);            
            pDevCtx->DevStats.tx_dropped++;
        }
    }
    else
    {
        if( pNBuff )
        {
            nbuff_flushfree(pNBuff);
            pDevCtx->DevStats.tx_dropped++;
        }
    }

unlock_done_xmit:
    spin_unlock_bh(&pGi->xtmlock_tx);

    return 0;
} /* bcmxtmrt_xmit */

/***************************************************************************
 * Function Name: AddRfc2684Hdr
 * Description  : Adds the RFC2684 header to an ATM packet before transmitting
 *                it.
 * Returns      : None.
 ***************************************************************************/
static void AddRfc2684Hdr(pNBuff_t *ppNBuff, struct sk_buff **ppNBuffSkb,
                          UINT8 **ppData, int * pLen, UINT32 ulHdrType)
{
    UINT8 ucHdrs[][16] =
        {{},
         {0xAA, 0xAA, 0x03, 0x00, 0x80, 0xC2, 0x00, 0x07, 0x00, 0x00},
         {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x08, 0x00},
         {0xFE, 0xFE, 0x03, 0xCF},
         {0x00, 0x00}};
    int minheadroom = HT_LEN(ulHdrType);

    if ( *ppNBuffSkb )
    {
        struct sk_buff *skb = *ppNBuffSkb;
        int headroom = skb_headroom(skb);

        if (headroom < minheadroom)
        {
            struct sk_buff *skb2 = skb_realloc_headroom(skb, minheadroom);

            dev_kfree_skb_any(skb);
            skb = (skb2 == NULL) ? NULL : skb2;
        }
        if( skb )
        {
            *ppData = skb_push(skb, minheadroom);
            *pLen = skb->len;
            u16cpy(*ppData, ucHdrs[HT_TYPE(ulHdrType)], minheadroom);
        }
        // else ?
        *ppNBuffSkb = skb;
        *ppNBuff = SKBUFF_2_PNBUFF(skb);
    }
    else // if( IS_FKBUFF_PTR(*ppNBuff) )
    {
        struct fkbuff *fkb = PNBUFF_2_FKBUFF(*ppNBuff);
        int headroom = fkb_headroom(fkb);
        if (headroom >= minheadroom)
        {
            *ppData = fkb_push(fkb, minheadroom);
            *pLen += minheadroom;
            u16cpy(*ppData, ucHdrs[HT_TYPE(ulHdrType)], minheadroom);
        }
        else
            printk(CARDNAME ": FKB not enough headroom.\n");
    }

} /* AddRfc2684Hdr */


/***************************************************************************
 * Function Name: AssignRxBuffer
 * Description  : Put a data buffer back on to the receive BD ring. 
 * Returns      : None.
 ***************************************************************************/
void AssignRxBuffer(int channel, UINT8 *pucData)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    BcmXtm_RxDma *rxdma = pGi->rxdma[channel];

    spin_lock_bh(&pGi->xtmlock_rx);

    /* Free the data buffer to a specific Rx Queue (ie channel) */
    bcmPktDma_XtmFreeRecvBuf(rxdma->channel, (unsigned char *)pucData);

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
    /* Delay is only needed when the free is actually being done by the FAP */
    if(bcmxtmrt_in_init_dev)
       udelay(20);
#endif

    spin_unlock_bh(&pGi->xtmlock_rx);
}

/***************************************************************************
 * Function Name: FlushAssignRxBuffer
 * Description  : Flush then assign RxBdInfo to the receive BD ring. 
 * Returns      : None.
 ***************************************************************************/
void FlushAssignRxBuffer(int channel, UINT8 *pucData, UINT8 *pucEnd)
{
    cache_flush_region(pucData, pucEnd);
    AssignRxBuffer(channel, pucData);
}

/***************************************************************************
 * Function Name: bcmxtmrt_recycle_skb_or_data
 * Description  : Put socket buffer header back onto the free list or a data
 *                buffer back on to the BD ring. 
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_recycle_skb_or_data(struct sk_buff *skb, unsigned context,
    UINT32 nFlag )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    int channel = RECYCLE_CONTEXT(context)->channel;

    if( nFlag & SKB_RECYCLE )
    {
        BcmXtm_RxDma *rxdma = pGi->rxdma[channel];       

        spin_lock_bh(&pGi->xtmlock_rx);
        skb->next_free = rxdma->freeSkbList;
        rxdma->freeSkbList = skb;
        spin_unlock_bh(&pGi->xtmlock_rx);
    }
    else
    {

        UINT8 *pucData, *pucEnd;

        pucData = skb->head + RXBUF_HEAD_RESERVE;
#ifdef XTM_CACHE_SMARTFLUSH
        pucEnd = (UINT8*)(skb_shinfo(skb)) + sizeof(struct skb_shared_info);
#else
        pucEnd = pucData + RXBUF_SIZE;
#endif
        FlushAssignRxBuffer(channel, pucData, pucEnd);
    }
} /* bcmxtmrt_recycle_skb_or_data */

/***************************************************************************
 * Function Name: _bcmxtmrt_recycle_fkb
 * Description  : Put fkb buffer back on to the BD ring.
 * Returns      : None.
 ***************************************************************************/
static inline void _bcmxtmrt_recycle_fkb(struct fkbuff *pFkb,
                                              unsigned context)
{
    /* don't free the data if it's being done in FAP */
    if(!FKB_RECYCLE_CONTEXT(pFkb)->fapQuickFree)
    {
        UINT8 *pucData = (UINT8*) PFKBUFF_TO_PDATA(pFkb, RXBUF_HEAD_RESERVE);
        int channel = FKB_RECYCLE_CONTEXT(pFkb)->channel;
        AssignRxBuffer(channel, pucData); /* No cache flush */
    }
} /* _bcmxtmrt_recycle_fkb */

/***************************************************************************
 * Function Name: bcmxtmrt_recycle
 * Description  : Recycle a fkb or skb or skb->data
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_recycle(pNBuff_t pNBuff, unsigned context, UINT32 flags)
{
    if (IS_FKBUFF_PTR(pNBuff))
        _bcmxtmrt_recycle_fkb( PNBUFF_2_FKBUFF(pNBuff), context );
    else // if (IS_SKBUFF_PTR(pNBuff))
        bcmxtmrt_recycle_skb_or_data( PNBUFF_2_SKBUFF(pNBuff), context, flags );
}

/***************************************************************************
 * Function Name: bcmxtmrt_rxisr
 * Description  : Hardware interrupt that is called when a packet is received
 *                on one of the receive queues.
 * Returns      : IRQ_HANDLED
 ***************************************************************************/
static FN_HANDLER_RT bcmxtmrt_rxisr(int nIrq, void *pRxDma)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    UINT32 i;
    UINT32 ulScheduled = 0;
    int    channel;

    channel = CONTEXT_TO_CHANNEL((uint32)pRxDma);

    spin_lock(&pGi->xtmlock_rx_regs);

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        if( (pDevCtx = pGi->pDevCtxs[i]) != NULL &&
            pDevCtx->ulOpenState == XTMRT_DEV_OPENED )
        {
            /* Device is open.  Schedule the poll function. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            napi_schedule(&pDevCtx->napi);
#else
            netif_rx_schedule(pDevCtx->pDev);
#endif

            bcmPktDma_XtmClrRxIrq(channel);
            pGi->ulIntEnableMask |= 1 << channel;
            ulScheduled = 1;
        }
    }

    if( ulScheduled == 0 && pGi->ulDrvState == XTMRT_RUNNING )
    {
        /* Device is not open.  Reenable interrupt. */
        bcmPktDma_XtmClrRxIrq(channel);
#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
        BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + channel);
#endif
    }

    spin_unlock(&pGi->xtmlock_rx_regs);

    return( IRQ_HANDLED );
} /* bcmxtmrt_rxisr */

/***************************************************************************
 * Function Name: bcmxtmrt_poll
 * Description  : Hardware interrupt that is called when a packet is received
 *                on one of the receive queues.
 * Returns      : IRQ_HANDLED
 ***************************************************************************/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)

static int bcmxtmrt_poll_napi(struct napi_struct* napi, int budget)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMask;
    UINT32 i;
    UINT32 work_done;
    UINT32 ret_done;
    UINT32 flags;
    UINT32 more_to_do;

    spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
    ulMask = pGi->ulIntEnableMask;
    pGi->ulIntEnableMask = 0;
    spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);

    work_done = bcmxtmrt_rxtask(budget, &more_to_do);
    ret_done = work_done & XTM_POLL_DONE;
    work_done &= ~XTM_POLL_DONE;

    /* JU: You may not call napi_complete if work_done == budget...
       this causes the framework to crash (specifically, you get
       napi->poll_list.next=0x00100100).  So, in this case
       you have to just return work_done.  */
    if (work_done == budget || ret_done != XTM_POLL_DONE)
    {
        /* We have either exhausted our budget or there are
           more packets on the DMA (or both) */
        spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
        pGi->ulIntEnableMask |= ulMask;
        spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);        
        return work_done;
    }

    /* We are done */
    napi_complete(napi);

    /* Renable interrupts. */
    if( pGi->ulDrvState == XTMRT_RUNNING )
    for( i = 0; ulMask && i < MAX_RECEIVE_QUEUES; i++, ulMask >>= 1 )
        if( (ulMask & 0x01) == 0x01 )
        {
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
            bcmPktDma_XtmClrRxIrq(i);
#else
            BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);
#endif
        }

    return work_done;
}
#else
static int bcmxtmrt_poll(struct net_device * dev, int * budget)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMask;
    UINT32 i;
    UINT32 work_to_do = min(dev->quota, *budget);
    UINT32 work_done;
    UINT32 ret_done;
    UINT32 more_to_do = 0;
    UINT32 flags;

    spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
    ulMask = pGi->ulIntEnableMask;
    pGi->ulIntEnableMask = 0;
    spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);

    work_done = bcmxtmrt_rxtask(work_to_do, &more_to_do);
    ret_done = work_done & XTM_POLL_DONE;
    work_done &= ~XTM_POLL_DONE;

    *budget -= work_done;
    dev->quota -= work_done;

    /* JU I think it should be (work_done >= work_to_do).... */
    if (work_done < work_to_do && ret_done != XTM_POLL_DONE)
    {
        /* Did as much as could, but we are not done yet */
        spin_lock_irqsave(&pGi->xtmlock_rx_regs, flags);
        pGi->ulIntEnableMask |= ulMask;
        spin_unlock_irqrestore(&pGi->xtmlock_rx_regs,flags);
        return 1;
    }

    /* We are done */
    netif_rx_complete(dev);

    /* Renable interrupts. */
    if( pGi->ulDrvState == XTMRT_RUNNING )
    for( i = 0; ulMask && i < MAX_RECEIVE_QUEUES; i++, ulMask >>= 1 )
        if( (ulMask & 0x01) == 0x01 )
        {
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
            bcmPktDma_XtmClrRxIrq(i);
#else
            BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);
#endif
        }

    return 0;
} /* bcmxtmrt_poll */
#endif

/***************************************************************************
 * Function Name: bcmxtmrt_rxtask
 * Description  : Linux Tasklet that processes received packets.
 * Returns      : None.
 ***************************************************************************/
static UINT32 bcmxtmrt_rxtask( UINT32 ulBudget, UINT32 *pulMoreToDo )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 ulMoreToReceive;
    UINT32 i;
    BcmXtm_RxDma       *rxdma;
    DmaDesc dmaDesc;
    UINT8 *pBuf, *pucData;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    int  len;

    UINT32 ulRxPktGood = 0;
    UINT32 ulRxPktProcessed = 0;
    UINT32 ulRxPktMax = ulBudget + (ulBudget / 2);

    /* Receive packets from every receive queue in a round robin order until
     * there are no more packets to receive.
     */
    do
    {
        ulMoreToReceive = 0;

        for  (i = 0;  i < MAX_RECEIVE_QUEUES; i++)
        {
            UINT32   ulCell;
            
            rxdma = pGi->rxdma[i];

            if( ulBudget == 0 )
            {
                *pulMoreToDo = 1;
                break;
            }

            spin_lock_bh(&pGi->xtmlock_rx);


            dmaDesc.word0 = bcmPktDma_XtmRecv(i, (unsigned char **)&pucData, &len);
            if( dmaDesc.status & DMA_OWN )
            {
                ulRxPktGood |= XTM_POLL_DONE;
                spin_unlock_bh(&pGi->xtmlock_rx);

                continue;   /* next RxBdInfos */
            }
            pBuf = pucData ;

            ulRxPktProcessed++;

            pDevCtx = pGi->pDevCtxsByMatchId[dmaDesc.status & FSTAT_MATCH_ID_MASK];
            ulCell  = (dmaDesc.status & FSTAT_PACKET_CELL_MASK) == FSTAT_CELL;

            /* error status, or packet with no pDev */
            if(((dmaDesc.status & FSTAT_ERROR) != 0) ||
               ((dmaDesc.status & (DMA_SOP|DMA_EOP)) != (DMA_SOP|DMA_EOP)) ||
                ((!ulCell) && (pDevCtx == NULL)))   /* packet */
            {
                spin_unlock_bh(&pGi->xtmlock_rx);

                AssignRxBuffer(i, pBuf);
                if( pDevCtx )
                    pDevCtx->DevStats.rx_errors++;
            }
            else if ( !ulCell ) /* process packet, pDev != NULL */
            {
                FkBuff_t * pFkb;
                UINT16 usLength = dmaDesc.length;
                int delLen = 0, trailerDelLen = 0;

                ulRxPktGood++;
                ulBudget--;

                if( (pDevCtx->ulFlags & LSC_RAW_ENET_MODE) != 0 )
                    usLength -= 4; /* ETH CRC Len */

                if ( pDevCtx->ulHdrType == HT_PTM &&
                    (pDevCtx->ulFlags & CNI_HW_REMOVE_TRAILER) == 0 )
                {
                   if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) {
                      if (usLength > XTMRT_PTM_CRC_SIZE) {
                        usLength -= XTMRT_PTM_CRC_SIZE;
                                trailerDelLen = XTMRT_PTM_CRC_SIZE ;
                             }
                   }
                   else {
                      if (usLength > (ETH_FCS_LEN+XTMRT_PTM_CRC_SIZE)) {
                        usLength -= (ETH_FCS_LEN+XTMRT_PTM_CRC_SIZE);
                                trailerDelLen = (ETH_FCS_LEN+XTMRT_PTM_CRC_SIZE) ;
                             }
                   }
                }

                if( (pDevCtx->ulFlags & CNI_HW_REMOVE_HEADER) == 0 )
                {
                   delLen = HT_LEN(pDevCtx->ulHdrType);

                   /* For PTM flow, this will not take effect and hence so, for
                    * bonding flow as well. So we do not need checks here to not
                    * make it happen.
                    */
                    if( delLen > 0)
                    {
                        pucData += delLen;
                        usLength -= delLen;
                    }
                }

                if( usLength < ETH_ZLEN )
                    usLength = ETH_ZLEN;

                pFkb = fkb_qinit(pBuf, RXBUF_HEAD_RESERVE, pucData, usLength,
                                 (uint32_t)rxdma);

                if ((pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
                      &&
                    (pDevCtx->ulHdrType == HT_PTM)) {
                    spin_unlock_bh (&pGi->xtmlock_rx) ;
                    spin_lock_bh (&pGi->xtmlock_rx_bond) ;
                    bcmxtmrt_ptmbond_receive_rx_fragment (pDevCtx, pFkb, dmaDesc.status) ;
                    spin_unlock_bh (&pGi->xtmlock_rx_bond);
                }
                else {
                    bcmxtmrt_process_rx_pkt (pDevCtx, rxdma, pFkb, dmaDesc.status, delLen, trailerDelLen) ;
                    //bcmxtmrt_ptm_receive_and_drop (pDevCtx, pFkb, dmaDesc.status) ; 
                                         /* Used for SAR-phy Lpbk high-rate test */
                }
            }
            else                /* process cell */
            {
                spin_unlock_bh(&pGi->xtmlock_rx);
                ProcessRxCell(pGi, rxdma, pucData);
            }

            if( ulRxPktProcessed >= ulRxPktMax )
                break;
            else
                ulMoreToReceive = 1; /* more packets to receive on Rx queue? */

        } /* For loop */

    } while( ulMoreToReceive );

    return( ulRxPktGood );

} /* bcmxtmrt_rxtask */


/***************************************************************************
 * Function Name: bcmxtmrt_process_rx_pkt
 * Description  : Processes a received packet.
 *                Responsible to send the packet up to the blog and network stack.
 * Returns      : Status as the packet thro BLOG/NORMAL path.
 ***************************************************************************/

UINT32 bcmxtmrt_process_rx_pkt ( PBCMXTMRT_DEV_CONTEXT pDevCtx, BcmXtm_RxDma *rxdma,
                                 FkBuff_t *pFkb, UINT16 bufStatus, int delLen, int trailerDelLen )
{
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
   struct sk_buff *skb ;
   UINT8  *pucData  = pFkb->data ;
   UINT32 ulHdrType = pDevCtx->ulHdrType ;
   UINT32 ulRfc2684_type = RFC2684_NONE; /* blog.h: Rfc2684_t */
   UINT8  *pBuf = PFKBUFF_TO_PDATA(pFkb, RXBUF_HEAD_RESERVE) ;
   UINT32 retStatus;

   pDevCtx->pDev->last_rx = jiffies;

   pDevCtx->DevStats.rx_bytes += pFkb->len ;

   if( (ulHdrType ==  HT_PTM || ulHdrType ==  HT_LLC_SNAP_ETHERNET || 
        ulHdrType ==  HT_VC_MUX_ETHERNET) 
                     &&
                    ((pucData[0] & 0x01) == 0x01) )
                {
                    pDevCtx->DevStats.multicast++;
                }

   if( (pDevCtx->ulFlags & CNI_HW_REMOVE_HEADER) == 0 )
   {  /* cannot be an AtmCell, also do not use delLen (bonding), recompute */
      if( HT_LEN(ulHdrType) > 0)
         ulRfc2684_type = HT_TYPE(ulHdrType); /* blog.h: Rfc2684_t */
   }

   //dumpaddr(pFkb->data, 16) ;

#ifdef CONFIG_BLOG
   spin_unlock_bh(&pGi->xtmlock_rx);
   if( blog_finit( pFkb, pDevCtx->pDev, pDevCtx->ulEncapType,
               (bufStatus & FSTAT_MATCH_ID_MASK), ulRfc2684_type ) == PKT_DONE )
   {
      retStatus = PACKET_BLOG ;
   }
   else {
      spin_lock_bh(&pGi->xtmlock_rx);
#else
   {
#endif

        if( rxdma->freeSkbList == NULL )
        {
           spin_unlock_bh(&pGi->xtmlock_rx);

           fkb_release(pFkb);  /* releases allocated blog */
           FlushAssignRxBuffer(rxdma->channel, pBuf, pBuf);
           pDevCtx->DevStats.rx_dropped++;
           return PACKET_NORMAL;
        }

        /* Get an skb to return to the network stack. */
        skb = rxdma->freeSkbList;
        rxdma->freeSkbList = rxdma->freeSkbList->next_free;

        spin_unlock_bh(&pGi->xtmlock_rx);
        BCM_XTM_RX_DEBUG("XTM RX SKB: skb<0x%08x> pBuf<0x%08x> len<%d>\n", (int)skb, (int)pBuf, pFkb->len);

        {
            uint32 recycle_context = 0;

            RECYCLE_CONTEXT(recycle_context)->channel = rxdma->channel;

            skb_headerinit( RXBUF_HEAD_RESERVE,
#ifdef XTM_CACHE_SMARTFLUSH
                            pFkb->len + delLen + trailerDelLen + SAR_DMA_MAX_BURST_LENGTH,
#else
                            RXBUF_SIZE,
#endif
                            skb, pBuf,
                            (RecycleFuncP)bcmxtmrt_recycle_skb_or_data,
                            recycle_context, (void*)fkb_blog(pFkb));
        }

        if ( delLen )
             __skb_pull(skb, delLen);

        __skb_trim(skb, pFkb->len);
        skb->dev = pDevCtx->pDev ;

        if( pDevCtx->szMirrorIntfIn[0] != '\0' &&
            (ulHdrType ==  HT_PTM ||
             ulHdrType ==  HT_LLC_SNAP_ETHERNET ||
             ulHdrType ==  HT_VC_MUX_ETHERNET) )
        {
#ifdef CONFIG_BLOG
            blog_skip(skb);
#endif // CONFIG_BLOG

            MirrorPacket( skb, pDevCtx->szMirrorIntfIn );
        }

        switch( ulHdrType )
        {
            case HT_LLC_SNAP_ROUTE_IP:
            case HT_VC_MUX_IPOA:
                /* IPoA */
                skb->protocol = htons(ETH_P_IP);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
                skb_reset_mac_header(skb);
#else
                skb->mac.raw = skb->data;
#endif
                /* Give the received packet to the network stack. */
                netif_receive_skb(skb);
                break;

        case HT_LLC_ENCAPS_PPP:
        case HT_VC_MUX_PPPOA:
                /*PPPoA*/
                ppp_input(&pDevCtx->Chan, skb);
                break;

        default:
                /* bridge, MER, PPPoE */
                skb->protocol = eth_type_trans(skb,pDevCtx->pDev);

                /* Give the received packet to the network stack. */
                netif_receive_skb(skb);
                break;
        }

        retStatus = PACKET_NORMAL;
    }
    return (retStatus) ;
}

/***************************************************************************
 * Function Name: ProcessRxCell
 * Description  : Processes a received cell.
 * Returns      : None.
 ***************************************************************************/
static void ProcessRxCell(PBCMXTMRT_GLOBAL_INFO pGi, BcmXtm_RxDma *rxdma,
    UINT8 *pucData )
{
    const UINT16 usOamF4VciSeg = 3;
    const UINT16 usOamF4VciEnd = 4;
    UINT8 ucCts[] = {0, 0, 0, 0, CTYPE_OAM_F5_SEGMENT, CTYPE_OAM_F5_END_TO_END,
        0, 0, CTYPE_ASM_P0, CTYPE_ASM_P1, CTYPE_ASM_P2, CTYPE_ASM_P3,
        CTYPE_OAM_F4_SEGMENT, CTYPE_OAM_F4_END_TO_END};
    XTMRT_CELL Cell;
    UINT8 ucCHdr = *pucData;
    UINT8 *pucAtmHdr = pucData + sizeof(char);
    UINT8 ucLogPort;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;

    /* Fill in the XTMRT_CELL structure */
    Cell.ConnAddr.u.Vcc.usVpi = (((UINT16) pucAtmHdr[0] << 8) +
        ((UINT16) pucAtmHdr[1])) >> 4;
    Cell.ConnAddr.u.Vcc.usVci = (UINT16)
        (((UINT32) (pucAtmHdr[1] & 0x0f) << 16) +
         ((UINT32) pucAtmHdr[2] << 8) +
         ((UINT32) pucAtmHdr[3])) >> 4;

    if ((Cell.ConnAddr.u.Vcc.usVpi == XTMRT_ATM_BOND_ASM_VPI)
             &&
        (Cell.ConnAddr.u.Vcc.usVci == XTMRT_ATM_BOND_ASM_VCI)) {

       pDevCtx = pGi->pDevCtxs[0];
    }
    else {
       Cell.ConnAddr.ulTrafficType = TRAFFIC_TYPE_ATM;
       ucLogPort = PORT_PHYS_TO_LOG((ucCHdr & CHDR_PORT_MASK) >> CHDR_PORT_SHIFT);
       Cell.ConnAddr.u.Vcc.ulPortMask = PORT_TO_PORTID(ucLogPort);

    if( Cell.ConnAddr.u.Vcc.usVci == usOamF4VciSeg )
    {
        ucCHdr = CHDR_CT_OAM_F4_SEG;
        pDevCtx = pGi->pDevCtxs[0];
    }
    else
        if( Cell.ConnAddr.u.Vcc.usVci == usOamF4VciEnd )
        {
            ucCHdr = CHDR_CT_OAM_F4_E2E;
            pDevCtx = pGi->pDevCtxs[0];
        }
        else
        {
            pDevCtx = FindDevCtx( (short) Cell.ConnAddr.u.Vcc.usVpi,
                (int) Cell.ConnAddr.u.Vcc.usVci);
        }
    } /* End of else */

    Cell.ucCircuitType = ucCts[(ucCHdr & CHDR_CT_MASK) >> CHDR_CT_SHIFT];

    if( (ucCHdr & CHDR_ERROR) == 0 )
    {
        memcpy(Cell.ucData, pucData + sizeof(char), sizeof(Cell.ucData));

        /* Call the registered OAM or ASM callback function. */
        switch( ucCHdr & CHDR_CT_MASK )
        {
        case CHDR_CT_OAM_F5_SEG:
        case CHDR_CT_OAM_F5_E2E:
        case CHDR_CT_OAM_F4_SEG:
        case CHDR_CT_OAM_F4_E2E:
            if( pGi->pfnOamHandler && pDevCtx )
            {
                (*pGi->pfnOamHandler) ((XTMRT_HANDLE)pDevCtx,
                    XTMRTCB_CMD_CELL_RECEIVED, &Cell,
                    pGi->pOamContext);
            }
            break;

        case CHDR_CT_ASM_P0:
        case CHDR_CT_ASM_P1:
        case CHDR_CT_ASM_P2:
        case CHDR_CT_ASM_P3:
            if( pGi->pfnAsmHandler && pDevCtx )
            {
                (*pGi->pfnAsmHandler) ((XTMRT_HANDLE)pDevCtx,
                    XTMRTCB_CMD_CELL_RECEIVED, &Cell,
                    pGi->pAsmContext);
            }
            break;

        default:
            break;
        }
    }
    else
        if( pDevCtx )
            pDevCtx->DevStats.rx_errors++;

    /* Put the buffer back onto the BD ring. */
    FlushAssignRxBuffer(rxdma->channel, pucData, pucData + RXBUF_SIZE);

} /* ProcessRxCell */

/***************************************************************************
 * Function Name: MirrorPacket
 * Description  : This function sends a sent or received packet to a LAN port.
 *                The purpose is to allow packets sent and received on the WAN
 *                to be captured by a protocol analyzer on the Lan for debugging
 *                purposes.
 * Returns      : None.
 ***************************************************************************/
static void MirrorPacket( struct sk_buff *skb, char *intfName )
{
    struct sk_buff *skbClone;
    struct net_device *netDev;

    if( (skbClone = skb_clone(skb, GFP_ATOMIC)) != NULL )
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        if( (netDev = __dev_get_by_name(&init_net, intfName)) != NULL )
#else
        if( (netDev = __dev_get_by_name(intfName)) != NULL )
#endif
        {
            unsigned long flags;

            // blog_xfer(skb, skbClone);
            skbClone->dev = netDev;
            skbClone->protocol = htons(ETH_P_802_3);
            local_irq_save(flags);
            local_irq_enable();
            dev_queue_xmit(skbClone) ;
            local_irq_restore(flags);
        }
        else
            dev_kfree_skb(skbClone);
    }
} /* MirrorPacket */

/***************************************************************************
 * Function Name: bcmxtmrt_timer
 * Description  : Periodic timer that calls the send function to free packets
 *                that have been transmitted.
 * Returns      : None.
 ***************************************************************************/
static void bcmxtmrt_timer( PBCMXTMRT_GLOBAL_INFO pGi )
{
    UINT32 i;
    UINT32 ulHdrTypeIsPtm = FALSE;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;

    /* Free transmitted buffers. */
    for( i = 0; i < MAX_DEV_CTXS; i++ ) {
        if( (pDevCtx = pGi->pDevCtxs[i]) ) {
            if( pDevCtx->ulTxQInfosSize )
            {
                bcmxtmrt_xmit( PNBUFF_NULL, pGi->pDevCtxs[i]->pDev );
            }
            if (pGi->pDevCtxs[i]->ulHdrType == HT_PTM)
               ulHdrTypeIsPtm = TRUE ;
        }
    }

    if ((pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
          &&
        (ulHdrTypeIsPtm == TRUE)) {
       if (pGi->ptmBondInfo.tickCount >= MAX_TICK_COUNT) {
          spin_lock_bh(&pGi->xtmlock_rx_bond);
       bcmxtmrt_ptmbond_tick (&pGi->ptmBondInfo) ;
          spin_unlock_bh(&pGi->xtmlock_rx_bond);
          pGi->ptmBondInfo.tickCount = 0 ;
       }
       else
          pGi->ptmBondInfo.tickCount++ ;
    }

    /* Restart the timer. */
    pGi->Timer.expires = jiffies + SAR_TIMEOUT;
    add_timer(&pGi->Timer);
} /* bcmxtmrt_timer */


/***************************************************************************
 * Function Name: bcmxtmrt_request
 * Description  : Request from the bcmxtmcfg driver.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int bcmxtmrt_request( XTMRT_HANDLE hDev, UINT32 ulCommand, void *pParm )
{
    PBCMXTMRT_DEV_CONTEXT pDevCtx = (PBCMXTMRT_DEV_CONTEXT) hDev;
    int nRet = 0;

    switch( ulCommand )
    {
    case XTMRT_CMD_GLOBAL_INITIALIZATION:
        nRet = DoGlobInitReq( (PXTMRT_GLOBAL_INIT_PARMS) pParm );
        break;

    case XTMRT_CMD_GLOBAL_UNINITIALIZATION:
        nRet = DoGlobUninitReq();
        break;

    case XTMRT_CMD_CREATE_DEVICE:
        nRet = DoCreateDeviceReq( (PXTMRT_CREATE_NETWORK_DEVICE) pParm );
        break;

    case XTMRT_CMD_GET_DEVICE_STATE:
        *(UINT32 *) pParm = pDevCtx->ulOpenState;
        break;

    case XTMRT_CMD_SET_ADMIN_STATUS:
        pDevCtx->ulAdminStatus = (UINT32) pParm;
        break;

    case XTMRT_CMD_REGISTER_CELL_HANDLER:
        nRet = DoRegCellHdlrReq( (PXTMRT_CELL_HDLR) pParm );
        break;

    case XTMRT_CMD_UNREGISTER_CELL_HANDLER:
        nRet = DoUnregCellHdlrReq( (PXTMRT_CELL_HDLR) pParm );
        break;

    case XTMRT_CMD_LINK_STATUS_CHANGED:
        nRet = DoLinkStsChangedReq(pDevCtx, (PXTMRT_LINK_STATUS_CHANGE)pParm);
        break;

    case XTMRT_CMD_SEND_CELL:
        nRet = DoSendCellReq( pDevCtx, (PXTMRT_CELL) pParm );
        break;

    case XTMRT_CMD_DELETE_DEVICE:
        nRet = DoDeleteDeviceReq( pDevCtx );
        break;

    case XTMRT_CMD_SET_TX_QUEUE:
        nRet = DoSetTxQueue( pDevCtx, (PXTMRT_TRANSMIT_QUEUE_ID) pParm );
        break;

    case XTMRT_CMD_UNSET_TX_QUEUE:
        nRet = DoUnsetTxQueue( pDevCtx, (PXTMRT_TRANSMIT_QUEUE_ID) pParm );
        break;

    case XTMRT_CMD_GET_NETDEV_TXCHANNEL:
        nRet = DoGetNetDevTxChannel( (PXTMRT_NETDEV_TXCHANNEL) pParm );
        break;

    default:
        nRet = -EINVAL;
        break;
    }

    return( nRet );
} /* bcmxtmrt_request */


/***************************************************************************
 * Function Name: DoGlobInitReq
 * Description  : Processes an XTMRT_CMD_GLOBAL_INITIALIZATION command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGlobInitReq( PXTMRT_GLOBAL_INIT_PARMS pGip )
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i, j = 0, k, ulSize;

    BcmXtm_RxDma         *rxdma;
    volatile DmaStateRam *StateRam;

    if( pGi->ulDrvState != XTMRT_UNINITIALIZED )
        return -EPERM;

    bcmxtmrt_in_init_dev = 1;

    bcmLog_setLogLevel(BCM_LOG_ID_XTM, BCM_LOG_LEVEL_ERROR);

    spin_lock_init(&pGi->xtmlock_tx);
    spin_lock_init(&pGi->xtmlock_rx);
    spin_lock_init(&pGi->xtmlock_rx_regs);

    /* Save MIB counter/Cam registers. */
    pGi->pulMibTxOctetCountBase = pGip->pulMibTxOctetCountBase;
    pGi->ulMibRxClrOnRead = pGip->ulMibRxClrOnRead;
    pGi->pulMibRxCtrl = pGip->pulMibRxCtrl;
    pGi->pulMibRxMatch = pGip->pulMibRxMatch;
    pGi->pulMibRxOctetCount = pGip->pulMibRxOctetCount;
    pGi->pulMibRxPacketCount = pGip->pulMibRxPacketCount;
    pGi->pulRxCamBase = pGip->pulRxCamBase;

    /* allocate rxdma channel structures */
    for (i = 0; i < MAX_RECEIVE_QUEUES; i++) 
    {
        pGi->rxdma[i] = (BcmXtm_RxDma *) (kzalloc(
                               sizeof(BcmXtm_RxDma), GFP_KERNEL));

        if (pGi->rxdma[i] == NULL) 
        {
            printk("Unable to allocate memory for rx dma channel structs \n");
            for(j = 0; j < i; j++)
                kfree(pGi->rxdma[j]); 
            return -ENXIO;
        }
    }

    /* alloc space for the rx buffer descriptors */
    for (i = 0; i < MAX_RECEIVE_QUEUES; i++) 
    {
        rxdma = pGi->rxdma[i]; 

        if( pGip->ulReceiveQueueSizes[i] == 0 ) continue; 

        rxdma->pktDmaRxInfo.rxBdsBase = bcmPktDma_XtmAllocRxBds(i, pGip->ulReceiveQueueSizes[i]);

        if (rxdma->pktDmaRxInfo.rxBdsBase == NULL) 
        {
            printk("Unable to allocate memory for Rx Descriptors \n");
            for(j = 0; j < MAX_RECEIVE_QUEUES; j++)
            {
#if !defined(XTM_RX_BDS_IN_PSM)
                if(pGi->rxdma[j]->pktDmaRxInfo.rxBdsBase)
                    kfree((void *)pGi->rxdma[j]->pktDmaRxInfo.rxBdsBase);
#endif

                kfree(pGi->rxdma[j]); 
            }
            return -ENOMEM;
        }
#if defined(XTM_RX_BDS_IN_PSM)
        rxdma->pktDmaRxInfo.rxBds = rxdma->pktDmaRxInfo.rxBdsBase;
#else
        /* Align rx BDs on 16-byte boundary - Apr 2010 */
        rxdma->pktDmaRxInfo.rxBds = (volatile DmaDesc *)(((int)rxdma->pktDmaRxInfo.rxBdsBase + 0xF) & ~0xF);
        rxdma->pktDmaRxInfo.rxBds = (volatile DmaDesc *)CACHE_TO_NONCACHE(rxdma->pktDmaRxInfo.rxBds);
#endif
        rxdma->pktDmaRxInfo.numRxBds = pGip->ulReceiveQueueSizes[i];

        printk("XTM Init: %d rx BDs at 0x%x\n", rxdma->pktDmaRxInfo.numRxBds,
            (unsigned int)rxdma->pktDmaRxInfo.rxBds);

        rxdma->rxIrq = bcmPktDma_XtmSelectRxIrq(i);
        rxdma->channel = i;
    }


    /*
     * clear RXDMA state RAM
     */
    pGi->dmaCtrl = (DmaRegs *) SAR_DMA_BASE;
    StateRam = (DmaStateRam *)&pGi->dmaCtrl->stram.s;
    memset((char *) &StateRam[SAR_RX_DMA_BASE_CHAN], 0x00,
            sizeof(DmaStateRam) * NR_SAR_RX_DMA_CHANS);

    /* setup the RX DMA channels */
    for (i = 0; i < MAX_RECEIVE_QUEUES; i++) 
    {
        BcmXtm_RxDma *rxdma;

        rxdma = pGi->rxdma[i];

        rxdma->pktDmaRxInfo.rxDma = &pGi->dmaCtrl->chcfg[SAR_RX_DMA_BASE_CHAN + i];
        rxdma->pktDmaRxInfo.rxDma->cfg = 0; 

        if( pGip->ulReceiveQueueSizes[i] == 0 ) continue; 

        rxdma->pktDmaRxInfo.rxDma->maxBurst = SAR_DMA_MAX_BURST_LENGTH;
        rxdma->pktDmaRxInfo.rxDma->intMask = 0;   /* mask all ints */
        rxdma->pktDmaRxInfo.rxDma->intStat = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
        rxdma->pktDmaRxInfo.rxDma->intMask = DMA_DONE | DMA_NO_DESC | DMA_BUFF_DONE;
        pGi->dmaCtrl->stram.s[SAR_RX_DMA_BASE_CHAN + i].baseDescPtr = 
            (uint32)VIRT_TO_PHY((uint32 *)rxdma->pktDmaRxInfo.rxBds);

        /* register the RX ISR */
        if (rxdma->rxIrq)
        {
            BcmHalInterruptDisable(rxdma->rxIrq);
            BcmHalMapInterrupt(bcmxtmrt_rxisr,
                              BUILD_CONTEXT(pGi,i), rxdma->rxIrq);
        }

       bcmPktDma_XtmInitRxChan(i, rxdma->pktDmaRxInfo.numRxBds, (BcmPktDma_LocalXtmRxDma *)&pGi->rxdma[i]->pktDmaRxInfo);
    }

    /* Allocate receive socket buffers and data buffers. */
    for (i = 0; i < MAX_RECEIVE_QUEUES; i++) 
    {
        const UINT32 ulRxAllocSize = SKB_ALIGNED_SIZE + RXBUF_ALLOC_SIZE;
        const UINT32 ulBlockSize = (64 * 1024);
        const UINT32 ulBufsPerBlock = ulBlockSize / ulRxAllocSize;
        UINT32 BufsToAlloc, ulAllocAmt;
        unsigned char *data, *pFkBuf;
        struct sk_buff *pSkbuff;
        uint32 context = 0;

        rxdma = pGi->rxdma[i];
        j = 0;

        rxdma->pktDmaRxInfo.rxAssignedBds = 0;
        rxdma->pktDmaRxInfo.rxHeadIndex = rxdma->pktDmaRxInfo.rxTailIndex = 0;

        BufsToAlloc = rxdma->pktDmaRxInfo.numRxBds;

        RECYCLE_CONTEXT(context)->channel = rxdma->channel;

        while (BufsToAlloc)
        {
            ulAllocAmt = (ulBufsPerBlock < BufsToAlloc) ? ulBufsPerBlock : BufsToAlloc;

            ulSize = ulAllocAmt * ulRxAllocSize;
            ulSize = (ulSize + 0x0f) & ~0x0f;

            if( (j >= MAX_BUFMEM_BLOCKS) ||
                ((data = kmalloc(ulSize, GFP_KERNEL)) == NULL) )
            {
                /* release all allocated receive buffers */
                printk(KERN_NOTICE CARDNAME": Low memory.\n");
                for (k = 0; k < MAX_BUFMEM_BLOCKS; k++) 
                {
                    if (rxdma->buf_pool[k]) {
                        kfree(rxdma->buf_pool[k]);
                        rxdma->buf_pool[k] = NULL;
                    }
                }
                for(k = 0; k < MAX_RECEIVE_QUEUES; k++)
                {
#if !defined(XTM_RX_BDS_IN_PSM)
                    if(pGi->rxdma[k]->pktDmaRxInfo.rxBdsBase)
                        kfree((void *)pGi->rxdma[k]->pktDmaRxInfo.rxBdsBase);
#endif

                    kfree(pGi->rxdma[k]); 
                }
                return -ENOMEM;
            }
 
            rxdma->buf_pool[j++] = data;
            memset(data, 0x00, ulSize);
            cache_flush_len(data, ulSize);

            data = (UINT8 *) (((UINT32) data + 0x0f) & ~0x0f);
            for (k = 0, pFkBuf = data; k < ulAllocAmt; k++) 
            {
                /* Place a FkBuff_t object at the head of pFkBuf */
                fkb_preinit(pFkBuf, (RecycleFuncP)bcmxtmrt_recycle, context);
                FlushAssignRxBuffer(i, 
                                 PFKBUFF_TO_PDATA(pFkBuf, RXBUF_HEAD_RESERVE), 
                                   (uint8_t*)pFkBuf + RXBUF_ALLOC_SIZE);

                /* skbuff allocation as in impl1 - Apr 2010 */
                pSkbuff = (struct sk_buff *)(pFkBuf + RXBUF_ALLOC_SIZE);
                pSkbuff->next_free = rxdma->freeSkbList;
                rxdma->freeSkbList = pSkbuff;
                pFkBuf += ulRxAllocSize;
            }
            BufsToAlloc -= ulAllocAmt;
        }
    }


    pGi->bondConfig.uConfig = pGip->bondConfig.uConfig ;
    if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE) 
    {
       bcmxtmrt_ptmbond_initialize (PTM_BOND_INITIALIZE_GLOBAL) ;
       spin_lock_init(&pGi->xtmlock_rx_bond);
    }

    /* Initialize a timer function to free transmit buffers. */
    init_timer(&pGi->Timer);
    pGi->Timer.data = (unsigned long) pGi;
    pGi->Timer.function = (void *) bcmxtmrt_timer;

    /* This was not done before. Is done in impl1 - Apr 2010 */
    pGi->dmaCtrl->controller_cfg |= DMA_MASTER_EN;

    pGi->ulDrvState = XTMRT_INITIALIZED;

    bcmxtmrt_in_init_dev = 0;

    return 0;
} /* DoGlobInitReq */


/***************************************************************************
 * Function Name: DoGlobUninitReq
 * Description  : Processes an XTMRT_CMD_GLOBAL_UNINITIALIZATION command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGlobUninitReq( void )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i, j;

    if( pGi->ulDrvState == XTMRT_UNINITIALIZED )
    {
        nRet = -EPERM;
    }
    else
    {
        pGi->ulDrvState = XTMRT_UNINITIALIZED;

        for( i = 0; i < MAX_RECEIVE_QUEUES; i++ )
        {
#if !defined(CONFIG_BCM_FAP) && !defined (CONFIG_BCM_FAP_MODULE)
            BcmHalInterruptDisable(SAR_RX_INT_ID_BASE + i);
#endif

#if !defined(XTM_RX_BDS_IN_PSM)
            if (pGi->rxdma[i]->pktDmaRxInfo.rxBdsBase)
                kfree((void *)pGi->rxdma[i]->pktDmaRxInfo.rxBdsBase);            
#endif

            /* Free space for receive socket buffers and data buffers */ 
            for (j = 0; j < MAX_BUFMEM_BLOCKS; j++)
            {
                if (pGi->rxdma[i]->buf_pool[j])
                {
                    kfree(pGi->rxdma[i]->buf_pool[j]);
                    pGi->rxdma[i]->buf_pool[j] = NULL;
                }
            }

            if (pGi->rxdma[i])
                kfree((void *)(pGi->rxdma[i]));
        }

        del_timer_sync(&pGi->Timer);

    }
    return( nRet );

} /* DoGlobUninitReq */


/***************************************************************************
 * Function Name: DoCreateDeviceReq
 * Description  : Processes an XTMRT_CMD_CREATE_DEVICE command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoCreateDeviceReq( PXTMRT_CREATE_NETWORK_DEVICE pCnd )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx = NULL;
    struct net_device *dev = NULL;
    int i;
    UINT32 unit = 0;
    UINT32 macId = 0;

    BCM_XTM_DEBUG("DoCreateDeviceReq\n");

    if( pGi->ulDrvState != XTMRT_UNINITIALIZED &&
        (dev = alloc_netdev( sizeof(BCMXTMRT_DEV_CONTEXT),
         pCnd->szNetworkDeviceName, ether_setup )) != NULL )
    {
        dev_alloc_name(dev, dev->name);
        SET_MODULE_OWNER(dev);

 #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        pDevCtx = (PBCMXTMRT_DEV_CONTEXT) netdev_priv(dev);
 #else
        pDevCtx = (PBCMXTMRT_DEV_CONTEXT) dev->priv;
 #endif
        memset(pDevCtx, 0x00, sizeof(BCMXTMRT_DEV_CONTEXT));
        memcpy(&pDevCtx->Addr, &pCnd->ConnAddr, sizeof(XTM_ADDR));
        if(( pCnd->ConnAddr.ulTrafficType & TRAFFIC_TYPE_ATM_MASK ) == TRAFFIC_TYPE_ATM )
            pDevCtx->ulHdrType = pCnd->ulHeaderType;
        else
            pDevCtx->ulHdrType = HT_PTM;
        pDevCtx->ulFlags = pCnd->ulFlags;
        pDevCtx->pDev = dev;
        pDevCtx->ulAdminStatus = ADMSTS_UP;
        pDevCtx->ucTxVcid = INVALID_VCID;

        /* Read and display the MAC address. */
        dev->dev_addr[0] = 0xff;

        /* format the mac id */
        i = strcspn(dev->name, "0123456789");
        if (i > 0)
           unit = simple_strtoul(&(dev->name[i]), (char **)NULL, 10);

        if (pDevCtx->ulHdrType == HT_PTM)
           macId = MAC_ADDRESS_PTM;
        else
           macId = MAC_ADDRESS_ATM;
        /* set unit number to bit 20-27 */
        macId |= ((unit & 0xff) << 20);

        kerSysGetMacAddress(dev->dev_addr, macId);

        if( (dev->dev_addr[0] & 0x01) == 0x01 )
        {
            printk( KERN_ERR CARDNAME": Unable to read MAC address from "
                "persistent storage.  Using default address.\n" );
            memcpy( dev->dev_addr, "\x02\x10\x18\x02\x00\x01", 6 );
        }

        printk( CARDNAME": MAC address: %2.2x %2.2x %2.2x %2.2x %2.2x "
            "%2.2x\n", dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
            dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5] );
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        dev->netdev_ops = &bcmXtmRt_netdevops;
#else
        /* Setup the callback functions. */
        dev->open               = bcmxtmrt_open;
        dev->stop               = bcmxtmrt_close;
        dev->hard_start_xmit    = (HardStartXmitFuncP) bcmxtmrt_xmit;
        dev->tx_timeout         = bcmxtmrt_timeout;
        dev->set_multicast_list = NULL;
        dev->do_ioctl           = &bcmxtmrt_ioctl;
        dev->poll               = bcmxtmrt_poll;
        dev->weight             = 64;
        dev->get_stats          = bcmxtmrt_query;
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
        dev->clr_stats          = bcmxtmrt_clrStats;
#endif
        dev->watchdog_timeo     = SAR_TIMEOUT;

        /* identify as a WAN interface to block WAN-WAN traffic */
        dev->priv_flags |= IFF_WANDEV;

        switch( pDevCtx->ulHdrType )
        {
        case HT_LLC_SNAP_ROUTE_IP:
        case HT_VC_MUX_IPOA:
            pDevCtx->ulEncapType = TYPE_IP;     /* IPoA */

            /* Since IPoA does not need a Ethernet header,
             * set the pointers below to NULL. Refer to kernel rt2684.c.
             */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            dev->header_ops = &bcmXtmRt_headerOps;
#else
            dev->hard_header = NULL;
            dev->rebuild_header = NULL;
            dev->set_mac_address = NULL;
            dev->hard_header_parse = NULL;
            dev->hard_header_cache = NULL;
            dev->header_cache_update = NULL;
            dev->change_mtu = NULL;
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)

            dev->type = ARPHRD_PPP;
            dev->hard_header_len = HT_LEN_LLC_SNAP_ROUTE_IP;
            dev->mtu = RFC1626_MTU;
            dev->addr_len = 0;
            dev->tx_queue_len = 100;
            dev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
            break;

        case HT_LLC_ENCAPS_PPP:
        case HT_VC_MUX_PPPOA:
            pDevCtx->ulEncapType = TYPE_PPP;    /*PPPoA*/
            break;

        default:
            pDevCtx->ulEncapType = TYPE_ETH;    /* bridge, MER, PPPoE, PTM */
            dev->flags = IFF_BROADCAST | IFF_MULTICAST;
            break;
        }
        
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        netif_napi_add(dev, &pDevCtx->napi, bcmxtmrt_poll_napi, 64);
#endif      
        /* Don't reset or enable the device yet. "Open" does that. */
        printk("[%s.%d]: register_netdev\n", __func__, __LINE__);
        nRet = register_netdev(dev);
        printk("[%s.%d]: register_netdev done\n", __func__, __LINE__);
        if (nRet == 0) 
        {
            UINT32 i;
            for( i = 0; i < MAX_DEV_CTXS; i++ )
                if( pGi->pDevCtxs[i] == NULL )
                {
                    pGi->pDevCtxs[i] = pDevCtx;

                    bcmPktDma_XtmCreateDevice(i, pDevCtx->ulEncapType);

                    break;
                }

            pCnd->hDev = (XTMRT_HANDLE) pDevCtx;
        }
        else
        {
            printk(KERN_ERR CARDNAME": register_netdev failed\n");
            free_netdev(dev);
        }

        if( nRet != 0 )
            kfree(pDevCtx);
    }
    else
    {
        printk(KERN_ERR CARDNAME": alloc_netdev failed\n");
        nRet = -ENOMEM;
    }

    return( nRet );
} /* DoCreateDeviceReq */


/***************************************************************************
 * Function Name: DoRegCellHdlrReq
 * Description  : Processes an XTMRT_CMD_REGISTER_CELL_HANDLER command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoRegCellHdlrReq( PXTMRT_CELL_HDLR pCh )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    switch( pCh->ulCellHandlerType )
    {
    case CELL_HDLR_OAM:
        if( pGi->pfnOamHandler == NULL )
        {
            pGi->pfnOamHandler = pCh->pfnCellHandler;
            pGi->pOamContext = pCh->pContext;
        }
        else
            nRet = -EEXIST;
        break;

    case CELL_HDLR_ASM:
        if( pGi->pfnAsmHandler == NULL )
        {
            pGi->pfnAsmHandler = pCh->pfnCellHandler;
            pGi->pAsmContext = pCh->pContext;
        }
        else
            nRet = -EEXIST;
        break;
    }

    return( nRet );
} /* DoRegCellHdlrReq */


/***************************************************************************
 * Function Name: DoUnregCellHdlrReq
 * Description  : Processes an XTMRT_CMD_UNREGISTER_CELL_HANDLER command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoUnregCellHdlrReq( PXTMRT_CELL_HDLR pCh )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    switch( pCh->ulCellHandlerType )
    {
    case CELL_HDLR_OAM:
        if( pGi->pfnOamHandler == pCh->pfnCellHandler )
        {
            pGi->pfnOamHandler = NULL;
            pGi->pOamContext = NULL;
        }
        else
            nRet = -EPERM;
        break;

    case CELL_HDLR_ASM:
        if( pGi->pfnAsmHandler == pCh->pfnCellHandler )
        {
            pGi->pfnAsmHandler = NULL;
            pGi->pAsmContext = NULL;
        }
        else
            nRet = -EPERM;
        break;
    }

    return( nRet );
} /* DoUnregCellHdlrReq */


/***************************************************************************
 * Function Name: DoLinkStsChangedReq
 * Description  : Processes an XTMRT_CMD_LINK_STATUS_CHANGED command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkStsChangedReq( PBCMXTMRT_DEV_CONTEXT pDevCtx,
     PXTMRT_LINK_STATUS_CHANGE pLsc )
{
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    int nRet = -EPERM;

    local_bh_disable();

    if( pDevCtx )
    {
        UINT32 i;

#if 0  /* debug code */
    {
    int j;
    printk("ulLinkState: %ld ulLinkUsRate: %ld ulLinkDsRate: %ld ulLinkDataMask: %ld ulTransmitQueueIdsSize: %ld ucTxVcid: %d ulRxVcidsSize: %ld\n\n",
           pLsc->ulLinkState, pLsc->ulLinkUsRate, pLsc->ulLinkDsRate, pLsc->ulLinkDataMask, pLsc->ulTransmitQueueIdsSize, pLsc->ucTxVcid, pLsc->ulRxVcidsSize);

    for (j = 0; j < MAX_TRANSMIT_QUEUES; j++)
        printk("%d: ulPortId: %ld PtmPriority: %ld WeightAlg: %ld WeightValue: %ld SubPriority: %ld QueueSize: %ld QueueIndex: %ld BondingPortId: %ld\n",
        j, pLsc->TransitQueueIds[j].ulPortId, pLsc->TransitQueueIds[j].ulPtmPriority, pLsc->TransitQueueIds[j].ulWeightAlg, pLsc->TransitQueueIds[j].ulWeightValue,
        pLsc->TransitQueueIds[j].ulSubPriority, pLsc->TransitQueueIds[j].ulQueueSize, pLsc->TransitQueueIds[j].ulQueueIndex, pLsc->TransitQueueIds[j].ulBondingPortId);
    }
#endif

        for( i = 0; i < MAX_DEV_CTXS; i++ )
        {
            if( pGi->pDevCtxs[i] == pDevCtx )
            {
                pDevCtx->ulFlags |= pLsc->ulLinkState & LSC_RAW_ENET_MODE;
                pLsc->ulLinkState &= ~LSC_RAW_ENET_MODE;
                pDevCtx->MibInfo.ulIfLastChange = (jiffies * 100) / HZ;
                pDevCtx->MibInfo.ulIfSpeed = pLsc->ulLinkUsRate;

                if( pLsc->ulLinkState == LINK_UP ) 
                {
                    nRet = DoLinkUp( pDevCtx, pLsc , i);
                }
                else 
                {
                    spin_lock(&pGi->xtmlock_tx);
                    nRet = DoLinkDownTx( pDevCtx, pLsc );
                    spin_unlock(&pGi->xtmlock_tx);
                }

                break;
            }
        }

      if ((pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
            &&
          (pDevCtx->ulHdrType == HT_PTM)) {

            spin_lock (&pGi->xtmlock_rx_bond) ;
            bcmxtmrt_ptmbond_handle_port_status_change (&pGi->ptmBondInfo, pLsc->ulLinkState,
                                                     pDevCtx->ulPortDataMask) ;
            spin_unlock (&pGi->xtmlock_rx_bond) ;
        }
    }
    else
    {
        /* No device context indicates that the link is down.  Do global link
         * down processing.  pLsc is really an unsigned long containing the
         * port id.
         */
      spin_lock(&pGi->xtmlock_rx);
      nRet = DoLinkDownRx( (UINT32) pLsc );
      spin_unlock(&pGi->xtmlock_rx);
    }

    local_bh_enable();


#if 0 /* debug code */
    printk("\n");
    printk("GLOBAL: ulNumTxQs %ld\n", pGi->ulNumTxQs);

    if (pDevCtx != NULL)
    {
        printk("DEV PTR: %p VPI: %d VCI: %d\n", pDevCtx, pDevCtx->Addr.u.Vcc.usVpi, pDevCtx->Addr.u.Vcc.usVci);
        printk("DEV ulLinkState: %ld ulPortDataMask: %ld ulOpenState: %ld ulAdminStatus: %ld \n", 
                pDevCtx->ulLinkState, pDevCtx->ulPortDataMask, pDevCtx->ulOpenState, pDevCtx->ulAdminStatus);
        printk("DEV ulHdrType: %ld ulEncapType: %ld ulFlags: %ld ucTxVcid: %d ulTxQInfosSize: %ld\n", 
                pDevCtx->ulHdrType, pDevCtx->ulEncapType, pDevCtx->ulFlags, pDevCtx->ucTxVcid, pDevCtx->ulTxQInfosSize);
    }
    else
        printk("StsChangedReq called with NULL pDevCtx\n");
#endif

    return( nRet );
} /* DoLinkStsChangedReq */


/***************************************************************************
 * Function Name: DoLinkUp
 * Description  : Processes a "link up" condition.
 *                In bonding case, successive links may be coming UP one after
 *                another, accordingly the processing differs. 
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkUp( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                     PXTMRT_LINK_STATUS_CHANGE pLsc,
                     UINT32 ulDevId)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId;
    BcmXtm_RxDma       *rxdma;
    int i;

    BCM_XTM_DEBUG("DoLinkUp\n");

    if (pDevCtx->ulLinkState != pLsc->ulLinkState) 
    {
        /* Initialize transmit DMA channel information. */
        pDevCtx->ucTxVcid = pLsc->ucTxVcid;
        pDevCtx->ulLinkState = pLsc->ulLinkState;
        pDevCtx->ulTxQInfosSize = 0;

        /* Use each Rx vcid as an index into an array of bcmxtmrt devices
         * context structures.
         */
        for (i = 0; i < pLsc->ulRxVcidsSize ; i++) {
            pGi->pDevCtxsByMatchId[pLsc->ucRxVcids[i]] = pDevCtx;
            bcmPktDma_XtmLinkUp(ulDevId, pLsc->ucRxVcids[i]);
        }

        for(i = 0, pTxQId = pLsc->TransitQueueIds; 
                   i < pLsc->ulTransmitQueueIdsSize; i++, pTxQId++)
        {
            if (DoSetTxQueue(pDevCtx, pTxQId) != 0)    
            {
                pDevCtx->ulTxQInfosSize = 0;
                return -ENOMEM;
            }
        }

        /* If it is not already there, put the driver into a "ready to send and
         * receive state".
         */
        if (pGi->ulDrvState == XTMRT_INITIALIZED)
        {
             pGi->ulDrvState = XTMRT_RUNNING;

            /* Enable receive interrupts and start a timer. */
            for (i = 0; i < MAX_RECEIVE_QUEUES; i++)
            {
                rxdma = pGi->rxdma[i];

                if (rxdma->pktDmaRxInfo.rxBds)
                {
                    /* fap RX interrupts are enabled in open */
#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
                    bcmPktDma_XtmRxEnable(i);
                    BcmHalInterruptEnable(SAR_RX_INT_ID_BASE + i);
#endif
                }
            }

            pGi->Timer.expires = jiffies + SAR_TIMEOUT;
            add_timer(&pGi->Timer);

            if (pDevCtx->ulOpenState == XTMRT_DEV_OPENED)
                netif_start_queue(pDevCtx->pDev);
        }
    }
    pDevCtx->ulPortDataMask = pLsc->ulLinkDataMask ;

    return 0;
} /* DoLinkUp */


/***************************************************************************
 * Function Name: DoLinkDownRx
 * Description  : Processes a "link down" condition for receive only.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkDownRx( UINT32 ulPortId )
{
    int nRet = 0;
    BcmXtm_RxDma       *rxdma;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i, ulStopRunning;

    BCM_XTM_DEBUG("DoLinkDownRx\n");

    /* If all links are down, put the driver into an "initialized" state. */
    for( i = 0, ulStopRunning = 1; i < MAX_DEV_CTXS; i++ )
    {
        if( pGi->pDevCtxs[i] )
        {
            PBCMXTMRT_DEV_CONTEXT pDevCtx = pGi->pDevCtxs[i];
            UINT32 ulDevPortId = pDevCtx->ulPortDataMask ;
            if( (ulDevPortId & ~ulPortId) != 0 )
            {
                /* At least one link that uses a different port is up.
                 * For Ex., in bonding case, one of the links can be up
                 */
                ulStopRunning = 0;
                break;
            }
        }
    }

    if( ulStopRunning )
    {
        pGi->ulDrvState = XTMRT_INITIALIZED;

        /* Disable receive interrupts and stop the timer. */
        for (i = 0; i < MAX_RECEIVE_QUEUES; i++)
        {
            rxdma = pGi->rxdma[i];
            if (rxdma->pktDmaRxInfo.rxBds)
            {
                bcmPktDma_XtmRxDisable(i);
#if !defined(CONFIG_BCM_FAP) && !defined (CONFIG_BCM_FAP_MODULE)
                BcmHalInterruptDisable(SAR_RX_INT_ID_BASE + i);
#endif
            }
        }

        /* Stop the timer. */
        del_timer_sync(&pGi->Timer);
    }

    return( nRet );
} /* DoLinkDownRx */

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
/******************************************************************************
* Function: xtmDmaStatus                                                      *
*                                                                             *
* Description: Dumps information about the status of the XTM IUDMA channel    *
******************************************************************************/  
void xtmDmaStatus(int channel, BcmPktDma_XtmTxDma *txdma)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    BcmPktDma_LocalXtmRxDma *rxdma;

    rxdma = (BcmPktDma_LocalXtmRxDma *)&pGi->rxdma[channel]->pktDmaRxInfo; 

    printk("XTM IUDMA INFO CH %d\n", channel);

    printk("RXDMA STATUS: HeadIndex: %d TailIndex: %d numRxBds: %d rxAssignedBds: %d\n",
                  rxdma->rxHeadIndex, rxdma->rxTailIndex,
                  rxdma->numRxBds, rxdma->rxAssignedBds);

    printk("RXDMA CFG: cfg: 0x%lx intStat: 0x%lx intMask: 0x%lx\n",
                     rxdma->rxDma->cfg,
                     rxdma->rxDma->intStat,
                     rxdma->rxDma->intMask);

    printk("TXDMA STATUS: HeadIndex: %d TailIndex: %d txFreeBds: %d\n",
                  txdma->txHeadIndex, 
                  txdma->txTailIndex,
                  txdma->txFreeBds);

    printk("TXDMA CFG: cfg: 0x%lx intStat: 0x%lx intMask: 0x%lx\n",
                     txdma->txDma->cfg,
                     txdma->txDma->intStat,
                     txdma->txDma->intMask);

}
#endif

/***************************************************************************
 * Function Name: ShutdownTxQueue
 * Description  : Shutdown and clean up a transmit queue by waiting for it to
 *                empty, clearing state ram and free memory allocated for it.
 * Returns      : None.
 ***************************************************************************/
static void ShutdownTxQueue(volatile BcmPktDma_XtmTxDma *txdma,
    volatile DmaStateRam *pStRam)
{
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 j, ulIdx, txAddr;
    UINT32 txSource, rxChannel;
    pNBuff_t nbuff_reclaim_p;

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
      /* Add a variable to confirm that the 4ke tx disable is complete - May 2010 */
    pHostPsmGbl->XtmTxDownFlags[txdma->ulDmaIndex] = 0;
#endif

    bcmPktDma_XtmTxDisable(txdma->ulDmaIndex);

    /* Wait until iuDMA is really disabled (esp. when the disable is done by FAP) */
    for (j = 0; (j < 2000) && ((txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE); j++)
    {
        /* Increase delay as XTM tx of large packets is slow - May 2010 */
        udelay(5000);
    }
    if((txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE)
        printk("XTM Tx ch %d NOT disabled\n", (int)txdma->ulDmaIndex);
    else
    {
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
        volatile fap4kePsm_global_t * pPsmGbl = (volatile fap4kePsm_global_t *)pHostPsmGbl;

        /* More delay required to ensure FAP has completed the XTM Force Free of the tx BDs */
        for (j = 0; (j < 2000) && (pPsmGbl->XtmTxDownFlags[txdma->ulDmaIndex] == 0); j++)
        {
            udelay(5000);
        }
        if(j < 2000)
#endif
        printk("HOST XTM Tx ch %d down success\n", (int)txdma->ulDmaIndex);
    }

    ulIdx = SAR_TX_DMA_BASE_CHAN + txdma->ulDmaIndex;

    pStRam[ulIdx].baseDescPtr = 0;
    pStRam[ulIdx].state_data = 0;
    pStRam[ulIdx].desc_len_status = 0;
    pStRam[ulIdx].desc_base_bufptr = 0;

    /* Free transmitted packets. */
    for (j = 0; j < txdma->ulQueueSize; j++)
    {
        if (bcmPktDma_XtmFreeXmitBufGet(txdma->ulDmaIndex,
            (uint32 *)&nbuff_reclaim_p, &txSource, &txAddr, &rxChannel))
        {
            if (nbuff_reclaim_p != PNBUFF_NULL) 
            {
                spin_unlock_bh(&pGi->xtmlock_tx);
                nbuff_free(nbuff_reclaim_p);
                spin_lock_bh(&pGi->xtmlock_tx);
            }
        }
    }

    /* Added to match impl1 - May 2010 */
    txdma->txFreeBds = txdma->ulQueueSize = 0;
    txdma->ulNumTxBufsQdOne = 0;

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
    //xtmDmaStatus(txdma->ulDmaIndex, txdma);
#endif

#if !defined(XTM_TX_BDS_IN_PSM)
    /* remove the tx bd ring */
    if (txdma->txBdsBase)
    {
        kfree((void*)txdma->txBdsBase);
        txdma->txBdsBase = txdma->txBds = NULL;
    }
#endif
}

/***************************************************************************
 * Function Name: DoLinkDownTx
 * Description  : Processes a "link down" condition for transmit only.
 *                In bonding case, one of the links could still be UP, in which
 *                case only the link data status is updated.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoLinkDownTx( PBCMXTMRT_DEV_CONTEXT pDevCtx,
                         PXTMRT_LINK_STATUS_CHANGE pLsc )
{
    int nRet = 0;
    volatile DmaStateRam  *pStRam;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    volatile BcmPktDma_XtmTxDma  *txdma;
    UINT32 i, ulTxQs;

    BCM_XTM_DEBUG("DoLinkDownTx\n");

    if (pLsc->ulLinkDataMask == 0) {
       /* Disable transmit DMA. */
       pDevCtx->ulLinkState = LINK_DOWN;

       pStRam = pGi->dmaCtrl->stram.s;
       for (i = 0; i < pDevCtx->ulTxQInfosSize; i++)
       {
          txdma = pDevCtx->txdma[i];
          ShutdownTxQueue(txdma, pStRam);
       }

       /* Zero transmit related data structures. */
       pDevCtx->ulTxQInfosSize = 0;

       /* Free memory used for txdma info - Apr 2010 */
       for (i = 0; i < pDevCtx->ulTxQInfosSize; i++)
       {
           if(pDevCtx->txdma[i])
           {
              kfree((void*)pDevCtx->txdma[i]);
              pDevCtx->txdma[i] = NULL;
           }
       }

       /* Zero out list of priorities - Apr 2010 */
       memset(pDevCtx->pTxPriorities, 0x00, sizeof(pDevCtx->pTxPriorities));
       pDevCtx->ucTxVcid = INVALID_VCID;

       pGi->ulNumTxBufsQdAll = 0;

       /* Zero receive vcids. */
       for( i = 0; i < MAX_MATCH_IDS; i++ )
          if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
             pGi->pDevCtxsByMatchId[i] = NULL;

       /* Count the total number of transmit queues used across all device
        * interfaces.
        */
       for( i = 0, ulTxQs = 0; i < MAX_DEV_CTXS; i++ )
           if( pGi->pDevCtxs[i] )
               ulTxQs += pGi->pDevCtxs[i]->ulTxQInfosSize;
       pGi->ulNumTxQs = ulTxQs;
    }

    pDevCtx->ulPortDataMask = pLsc->ulLinkDataMask;

    return( nRet );
} /* DoLinkDownTx */


/***************************************************************************
 * Function Name: DoSetTxQueue
 * Description  : Allocate memory for and initialize a transmit queue.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoSetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId )
{
    UINT32 ulQueueSize, ulPort;
    BcmPktDma_XtmTxDma  *txdma;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;

    BCM_XTM_DEBUG("DoSetTxQueue\n");

    local_bh_enable();  // needed to avoid kernel error 
    pDevCtx->txdma[pDevCtx->ulTxQInfosSize] = txdma =
       (BcmPktDma_XtmTxDma *) (kzalloc(sizeof(BcmPktDma_XtmTxDma), GFP_KERNEL));
    local_bh_disable();

    if (pDevCtx->txdma[pDevCtx->ulTxQInfosSize] == NULL) 
    {
        printk("Unable to allocate memory for tx dma info\n");
        return -ENOMEM;
    }
    /* Increment channels per dev context */
    pDevCtx->ulTxQInfosSize++;

    /* Set every transmit queue size to the number of external buffers.
     * The QueuePacket function will control how many packets are queued.
     */
    ulQueueSize = pGi->ulNumExtBufs;

    local_bh_enable();  // needed to avoid kernel error - Apr 2010 
    /* allocate and assign tx buffer descriptors */
    txdma->txBdsBase = bcmPktDma_XtmAllocTxBds(pTxQId->ulQueueIndex, ulQueueSize);
    local_bh_disable();

    if (txdma->txBdsBase == NULL) 
    {
            printk("Unable to allocate memory for Tx Descriptors \n");
            return -ENOMEM;
    }
#if defined(XTM_TX_BDS_IN_PSM)
    txdma->txBds = txdma->txBdsBase;
#else
    /* Align to 16 byte boundary - Apr 2010 */
    txdma->txBds = (volatile DmaDesc *)(((int)txdma->txBdsBase + 0xF) & ~0xF);
    txdma->txBds = (volatile DmaDesc *)CACHE_TO_NONCACHE(txdma->txBds);
#endif

    /* pKeyPtr, pTxSource, pTxAddress now relative to txBds - Apr 2010 */
    txdma->txRecycle = (BcmPktDma_txRecycle_t *)((uint32)txdma->txBds + (pGi->ulNumExtBufs * sizeof(DmaDesc)));
#if !defined(XTM_TX_BDS_IN_PSM)
    txdma->txRecycle = (BcmPktDma_txRecycle_t *)NONCACHE_TO_CACHE(txdma->txRecycle);
#endif
    printk("XTM Init: %d tx BDs at 0x%x\n", (int)pGi->ulNumExtBufs, (unsigned int)txdma->txBds);

    ulPort = PORTID_TO_PORT(pTxQId->ulPortId);
    if ((ulPort < MAX_PHY_PORTS) && (txdma->ulSubPriority < MAX_SUB_PRIORITIES))
    {
            volatile DmaStateRam *pStRam = pGi->dmaCtrl->stram.s;
            UINT32 ulPtmPriority = PTM_FLOW_PRI_LOW, i, ulTxQs;

            txdma->ulPort = ulPort;
            txdma->ulPtmPriority = pTxQId->ulPtmPriority;
            txdma->ulSubPriority = pTxQId->ulSubPriority;
            txdma->ulQueueSize = ulQueueSize;
            txdma->ulDmaIndex = pTxQId->ulPrimaryShaperIndex;
            txdma->txEnabled = 1;
            txdma->ulNumTxBufsQdOne = 0;
            txdma->txDma = &pGi->dmaCtrl->chcfg[SAR_TX_DMA_BASE_CHAN + txdma->ulDmaIndex];
            txdma->txDma->cfg = 0;
            txdma->txDma->maxBurst = SAR_DMA_MAX_BURST_LENGTH;
            txdma->txDma->intMask = 0;   /* mask all ints */

            memset((UINT8 *)&pStRam[SAR_TX_DMA_BASE_CHAN + txdma->ulDmaIndex],
                0x00, sizeof(DmaStateRam));
            pStRam[SAR_TX_DMA_BASE_CHAN + txdma->ulDmaIndex].baseDescPtr =
                (UINT32) VIRT_TO_PHY(txdma->txBds);

            txdma->txBds[txdma->ulQueueSize - 1].status |= DMA_WRAP;
            txdma->txFreeBds = txdma->ulQueueSize;
            txdma->txHeadIndex = txdma->txTailIndex = 0;

            if (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM)
               ulPtmPriority = (txdma->ulPtmPriority == PTM_PRI_HIGH)? PTM_FLOW_PRI_HIGH : PTM_FLOW_PRI_LOW;
            pDevCtx->pTxPriorities[ulPtmPriority][ulPort][txdma->ulSubPriority] = txdma;

            /* Count the total number of transmit queues used across all device
             * interfaces.
             */
            for( i = 0, ulTxQs = 0; i < MAX_DEV_CTXS; i++ )
            {
                if( pGi->pDevCtxs[i] )
                    ulTxQs += pGi->pDevCtxs[i]->ulTxQInfosSize;
            }
            pGi->ulNumTxQs = ulTxQs;

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
            /* Shadow copies used by the 4ke for accelerated XTM - Apr 2010 */
            pDevCtx->devParams.ucTxVcid  = pDevCtx->ucTxVcid;
            pDevCtx->devParams.ulFlags   = pDevCtx->ulFlags;
            pDevCtx->devParams.ulHdrType = pDevCtx->ulHdrType;
#endif
    }

    bcmPktDma_XtmInitTxChan(txdma->ulDmaIndex, txdma->ulQueueSize, (BcmPktDma_LocalXtmTxDma *)txdma);

#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
    bcmPktDma_XtmTxEnable(txdma->ulDmaIndex, NULL);
#else
    /* For FAP builds, pass params needed for accelerated XTM - Apr 2010 */
    bcmPktDma_XtmTxEnable(txdma->ulDmaIndex, &pDevCtx->devParams);
#endif

    return 0;
} /* DoSetTxQueue */


/***************************************************************************
 * Function Name: DoUnsetTxQueue
 * Description  : Frees memory for a transmit queue.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoUnsetTxQueue( PBCMXTMRT_DEV_CONTEXT pDevCtx,
    PXTMRT_TRANSMIT_QUEUE_ID pTxQId )
{
    int nRet = 0;
    UINT32 i, j;
    BcmPktDma_XtmTxDma  *txdma;

    BCM_XTM_DEBUG("DoUnsetTxQueue\n");

    for (i = 0; i < pDevCtx->ulTxQInfosSize; i++)
    {
        txdma = pDevCtx->txdma[i];

        if( txdma && pTxQId->ulQueueIndex == txdma->ulDmaIndex )
        {
            UINT32 ulPort = PORTID_TO_PORT(pTxQId->ulPortId);
            PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
            volatile DmaStateRam *pStRam = pGi->dmaCtrl->stram.s;
            UINT32 ulPtmPriority = PTM_FLOW_PRI_LOW, ulTxQs;

            ShutdownTxQueue(txdma, pStRam);

            if ((pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM) ||
                (pDevCtx->Addr.ulTrafficType == TRAFFIC_TYPE_PTM_BONDED))
               ulPtmPriority = (txdma->ulPtmPriority == PTM_PRI_HIGH)? PTM_FLOW_PRI_HIGH : PTM_FLOW_PRI_LOW;
            pDevCtx->pTxPriorities[ulPtmPriority][ulPort][txdma->ulSubPriority] = NULL;

            /* Shift remaining array elements down by one element. */
            memmove(&pDevCtx->txdma[i], &pDevCtx->txdma[i + 1],
                (pDevCtx->ulTxQInfosSize - i - 1) * sizeof(txdma));
            pDevCtx->ulTxQInfosSize--;

            kfree((void*)txdma);

            /* Count the total number of transmit queues used across all device
             * interfaces.
             */
            for( j = 0, ulTxQs = 0; j < MAX_DEV_CTXS; j++ )
                if( pGi->pDevCtxs[j] )
                    ulTxQs += pGi->pDevCtxs[j]->ulTxQInfosSize;
            pGi->ulNumTxQs = ulTxQs;

            break;
        }
    }

    return( nRet );
} /* DoUnsetTxQueue */


/***************************************************************************
 * Function Name: DoSendCellReq
 * Description  : Processes an XTMRT_CMD_SEND_CELL command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoSendCellReq( PBCMXTMRT_DEV_CONTEXT pDevCtx, PXTMRT_CELL pC )
{
    int nRet = 0;

    if( pDevCtx->ulLinkState == LINK_UP )
    {
        struct sk_buff *skb = dev_alloc_skb(CELL_PAYLOAD_SIZE);

        if( skb )
        {
            UINT32 i;
            UINT32 ulPort = PORTID_TO_PORT(pC->ConnAddr.u.Conn.ulPortMask) ;
            UINT32 ulPtmPriority = PTM_FLOW_PRI_LOW;

            /* A network device instance can potentially have transmit queues
             * on different ports. Find a transmit queue for the port specified
             * in the cell structure.  The cell structure should only specify
             * one port.
             */
            for( i = 0; i < MAX_SUB_PRIORITIES; i++ )
            {
                if( pDevCtx->pTxPriorities[ulPtmPriority][ulPort][i] )
                {
                    skb->mark = i;
                    break;
                }
            }

            skb->dev = pDevCtx->pDev;
            __skb_put(skb, CELL_PAYLOAD_SIZE);
            memcpy(skb->data, pC->ucData, CELL_PAYLOAD_SIZE);

            switch( pC->ucCircuitType )
            {
            case CTYPE_OAM_F5_SEGMENT:
                skb->protocol = FSTAT_CT_OAM_F5_SEG;
                break;

            case CTYPE_OAM_F5_END_TO_END:
                skb->protocol = FSTAT_CT_OAM_F5_E2E;
                break;

            case CTYPE_OAM_F4_SEGMENT:
                skb->protocol = FSTAT_CT_OAM_F4_SEG;
                break;

            case CTYPE_OAM_F4_END_TO_END:
                skb->protocol = FSTAT_CT_OAM_F4_E2E;
                break;

            case CTYPE_ASM_P0:
                skb->protocol = FSTAT_CT_ASM_P0;
                break;

            case CTYPE_ASM_P1:
                skb->protocol = FSTAT_CT_ASM_P1;
                break;

            case CTYPE_ASM_P2:
                skb->protocol = FSTAT_CT_ASM_P2;
                break;

            case CTYPE_ASM_P3:
                skb->protocol = FSTAT_CT_ASM_P3;
                break;
            }

            skb->protocol |= SKB_PROTO_ATM_CELL;

            bcmxtmrt_xmit( SKBUFF_2_PNBUFF(skb), pDevCtx->pDev );
        }
        else
            nRet = -ENOMEM;
    }
    else
        nRet = -EPERM;

    return( nRet );
} /* DoSendCellReq */


/***************************************************************************
 * Function Name: DoDeleteDeviceReq
 * Description  : Processes an XTMRT_CMD_DELETE_DEVICE command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoDeleteDeviceReq( PBCMXTMRT_DEV_CONTEXT pDevCtx )
{
    int nRet = -EPERM;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    UINT32 i;

    BCM_XTM_DEBUG("DoDeleteDeviceReq\n");

    for( i = 0; i < MAX_DEV_CTXS; i++ )
        if( pGi->pDevCtxs[i] == pDevCtx )
        {
            pGi->pDevCtxs[i] = NULL;

            kerSysReleaseMacAddress( pDevCtx->pDev->dev_addr );

            unregister_netdev( pDevCtx->pDev );
            free_netdev( pDevCtx->pDev );

            nRet = 0;
            break;
        }

    for( i = 0; i < MAX_MATCH_IDS; i++ )
        if( pGi->pDevCtxsByMatchId[i] == pDevCtx )
            pGi->pDevCtxsByMatchId[i] = NULL;

    return( nRet );
} /* DoDeleteDeviceReq */


/***************************************************************************
 * Function Name: DoGetNetDevTxChannel
 * Description  : Processes an XTMRT_CMD_GET_NETDEV_TXCHANNEL command.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int DoGetNetDevTxChannel( PXTMRT_NETDEV_TXCHANNEL pParm )
{
    int nRet = 0;
    PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;
    BcmPktDma_XtmTxDma   *txdma;
    UINT32 i, j;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        pDevCtx = pGi->pDevCtxs[i];
        if ( pDevCtx != (PBCMXTMRT_DEV_CONTEXT) NULL )
        {
            if ( pDevCtx->ulOpenState == XTMRT_DEV_OPENED )
            {
                for (j = 0; j < pDevCtx->ulTxQInfosSize; j++)
                {
                    txdma = pDevCtx->txdma[j];

                    if ( txdma->ulDmaIndex == pParm->txChannel )
                    {
                        pParm->pDev = (void*)pDevCtx->pDev;
                        return nRet;
                    }
                }
            }
        }
    }

    return -EEXIST;
} /* DoGetNetDevTxChannel */


/***************************************************************************
 * Function Name: bcmxtmrt_add_proc_files
 * Description  : Adds proc file system directories and entries.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_add_proc_files( void )
{
    proc_mkdir ("driver/xtm", NULL);

    create_proc_read_entry("driver/xtm/txdmainfo",  0, NULL, ProcDmaTxInfo,  0);
    create_proc_read_entry("driver/xtm/rxbondctrs", 0, NULL, ProcRxBondCtrs, 0);
    create_proc_read_entry("driver/xtm/txbondinfo", 0, NULL, ProcTxBondInfo, 0);

#ifdef PTM_BONDING_DEBUG
    create_proc_read_entry("driver/xtm/rxbondseq0", 0, NULL, ProcRxBondSeq0, 0);
    create_proc_read_entry("driver/xtm/rxbondseq1", 0, NULL, ProcRxBondSeq1, 0);
#endif
    return(0);
} /* bcmxtmrt_add_proc_files */


/***************************************************************************
 * Function Name: bcmxtmrt_del_proc_files
 * Description  : Deletes proc file system directories and entries.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int bcmxtmrt_del_proc_files( void )
{
    remove_proc_entry("driver/xtm/txbondinfo", NULL);
    remove_proc_entry("driver/xtm/rxbondctrs", NULL);
    remove_proc_entry("driver/xtm/txdmainfo", NULL);
    remove_proc_entry("driver/xtm", NULL);

    return(0);
} /* bcmxtmrt_del_proc_files */


/***************************************************************************
 * Function Name: ProcDmaTxInfo
 * Description  : Displays information about transmit DMA channels for all
 *                network interfaces.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
static int ProcDmaTxInfo(char *page, char **start, off_t off, int cnt, 
    int *eof, void *data)
{
    PBCMXTMRT_GLOBAL_INFO  pGi = &g_GlobalInfo;
    PBCMXTMRT_DEV_CONTEXT  pDevCtx;
    BcmPktDma_XtmTxDma    *txdma;
    UINT32 i, j;
    int sz = 0;

    for( i = 0; i < MAX_DEV_CTXS; i++ )
    {
        pDevCtx = pGi->pDevCtxs[i];
        if ( pDevCtx != (PBCMXTMRT_DEV_CONTEXT) NULL )
        {
            for (j = 0; j < pDevCtx->ulTxQInfosSize; j++)
            {
                txdma = pDevCtx->txdma[j];

                sz += sprintf(page + sz, "dev: %s, tx_chan_size: %lu, tx_chan"
                    "_filled: %lu\n", pDevCtx->pDev->name, txdma->ulQueueSize,
                    txdma->ulNumTxBufsQdOne);
            }
        }
    }

    sz += sprintf(page + sz, "\next_buf_size: %lu, reserve_buf_size: %lu, tx_"
        "total_filled: %lu\n\n", pGi->ulNumExtBufs, pGi->ulNumExtBufsRsrvd,
        pGi->ulNumTxBufsQdAll);

    sz += sprintf(page + sz, "queue_condition: %lu %lu %lu, drop_condition: "
        "%lu %lu %lu\n\n", pGi->ulDbgQ1, pGi->ulDbgQ2, pGi->ulDbgQ3,
        pGi->ulDbgD1, pGi->ulDbgD2, pGi->ulDbgD3);

    *eof = 1;
    return( sz );
} /* ProcDmaTxInfo */

/***************************************************************************
 * Function Name: bcmxtmrt_update_hw_stats
 * Description  : Update the XTMRT transmit counters if appropriate for the
 *                flow. This flows through to the 'ifconfig' counts
 * Returns      : None
 ***************************************************************************/
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
void bcmxtmrt_update_hw_stats(void * flow_vp, unsigned int hits, unsigned int octets)
{
    Flow_t * flow_p = (Flow_t *) flow_vp;
    PBCMXTMRT_DEV_CONTEXT pDevCtx;

    /* If the flow uses XTMRT for tx, update the DevStats counters - Apr 2010 */
    if(flow_p->blog_p->tx.dev_p->destructor == 
           (void (*)(struct net_device *dev))bcmxtmrt_close)
    {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
        pDevCtx = netdev_priv(flow_p->blog_p->tx.dev_p);
#else
        pDevCtx = flow_p->blog_p->tx.dev_p->priv;
#endif
        /* Adjust xmit packet and octet counts for flows that tx to XTM */
        /* since the driver does not see the transmissions to count them - Apr 2010 */
        pDevCtx->DevStats.tx_packets += hits;
        pDevCtx->DevStats.tx_bytes   += octets;
    }
}
#endif

/***************************************************************************
 * MACRO to call driver initialization and cleanup functions.
 ***************************************************************************/
module_init(bcmxtmrt_init);
module_exit(bcmxtmrt_cleanup);
MODULE_LICENSE("Proprietary");

EXPORT_SYMBOL(bcmxtmrt_request);
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
EXPORT_SYMBOL(bcmxtmrt_close);
EXPORT_SYMBOL(bcmxtmrt_update_hw_stats);
#endif
