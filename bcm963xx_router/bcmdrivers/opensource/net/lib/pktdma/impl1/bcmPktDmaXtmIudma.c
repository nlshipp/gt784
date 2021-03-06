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
/*
 *******************************************************************************
 * File Name  : bcmPktDmaXtmIudma.c
 *
 * Description: This file contains the Packet DMA Implementation for the iuDMA
 *              channels of the XTM Controller.
 * Note       : This bcmPktDma code is tied to impl1 of the Xtm Driver 
 *
 *******************************************************************************
 */

#ifndef CONFIG_BCM96816

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
#include <bcm_intr.h>
#endif

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
#include "bcm_log.h"
#include "fap4ke_local.h"
#include "fap_task.h"
#include "fap_dqm.h"
#include "fap4ke_mailBox.h"
#include "fap4ke_timers.h"
#include "bcmPktDmaHooks.h"
#endif

#include "bcmPktDma.h"

/* fap4ke_local redfines memset to the 4ke lib one - not what we want */
#if defined memset
#undef memset
#endif

#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
/* Define in DDR */
BcmPktDma_LocalXtmRxDma *g_pXtmRxDma[MAX_RECEIVE_QUEUES];
BcmPktDma_LocalXtmTxDma *g_pXtmTxDma[MAX_TRANSMIT_QUEUES];
#endif
BcmPktDma_LocalXtmTxDma *g_pXtmSwTxDma[MAX_TRANSMIT_QUEUES];

/* Binding with XTMRT */
PBCMXTMRT_GLOBAL_INFO g_pXtmGlobalInfo = (PBCMXTMRT_GLOBAL_INFO)NULL; 


int bcmPktDma_XtmInitRxChan_Iudma(uint32 channel,
                                  uint32 bufDescrs,
                                  BcmPktDma_LocalXtmRxDma *pXtmRxDma)
{
    g_pXtmRxDma[channel] = (BcmPktDma_LocalXtmRxDma *)pXtmRxDma;
    g_pXtmRxDma[channel]->numRxBds = bufDescrs;
    g_pXtmRxDma[channel]->rxAssignedBds = 0;
    g_pXtmRxDma[channel]->rxHeadIndex = 0;
    g_pXtmRxDma[channel]->rxTailIndex = 0;
    g_pXtmRxDma[channel]->xtmrxchannel_isr_enable = 1;
    g_pXtmRxDma[channel]->rxEnabled = 0;
    
#if 0
    /* Code not necessary. Copies from A to A */
    g_pXtmRxDma[channel]->rxBds = (volatile DmaDesc *)(pXtmRxDma->rxBds);
    g_pXtmRxDma[channel]->rxDma = (volatile DmaChannelCfg *)(pXtmRxDma->rxDma);
#endif
       
    return 1;

}

int bcmPktDma_XtmInitTxChan_Iudma(uint32 channel,
                                  uint32 bufDescrs,
                                  BcmPktDma_LocalXtmTxDma *pXtmTxDma,
                                  uint32 dmaType)
{
   BcmPktDma_LocalXtmTxDma **ppXtmTxDma ;

   ppXtmTxDma = (dmaType == XTM_HW_DMA) ? g_pXtmTxDma : g_pXtmSwTxDma ;

    //printk("bcmPktDma_XtmInitTxChan_Iudma ch: %ld bufs: %ld txdma: %p\n", 
    //        channel, bufDescrs, pXtmTxDma);

   ppXtmTxDma[channel] = (BcmPktDma_LocalXtmTxDma *)pXtmTxDma;

   ppXtmTxDma[channel]->txFreeBds = bufDescrs;
   ppXtmTxDma[channel]->txHeadIndex = 0;
   ppXtmTxDma[channel]->txTailIndex = 0;
   ppXtmTxDma[channel]->txEnabled = 0;
   if (dmaType == XTM_SW_DMA)
      ppXtmTxDma[channel]->txSchedHeadIndex = 0;
    
    return 1;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmSelectRxIrq
 Purpose: Return IRQ number to be used for bcmPkt Rx on a specific channel
-------------------------------------------------------------------------- */
int	bcmPktDma_XtmSelectRxIrq_Iudma(int channel)
{
    return (SAR_RX_INT_ID_BASE + channel);
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmRecv
 Purpose: Receive a packet on a specific channel, 
          returning the associated DMA desc
-------------------------------------------------------------------------- */
uint32 bcmPktDma_XtmRecv_Iudma(int channel, unsigned char **pBuf, int * pLen)
{
    BcmPktDma_LocalXtmRxDma *rxdma;
    DmaDesc                  dmaDesc;

    FAP4KE_IUDMA_PMON_DECLARE();
    FAP4KE_IUDMA_PMON_BEGIN(FAP4KE_PMON_ID_IUDMA_RECV);

    dmaDesc.word0 = 0;
    rxdma = g_pXtmRxDma[channel];

    if (rxdma->rxAssignedBds != 0)
    {
        /* Get the status from Rx BD */
        dmaDesc.word0 = rxdma->rxBds[rxdma->rxHeadIndex].word0;

        /* If no more rx packets, we are done for this channel */
        if ((dmaDesc.status & DMA_OWN) == 0)
        {
            *pBuf = (unsigned char *)
                   (phys_to_virt(rxdma->rxBds[rxdma->rxHeadIndex].address));
            *pLen = (int) dmaDesc.length;

            /* Wrap around the rxHeadIndex */
            if (++rxdma->rxHeadIndex == rxdma->numRxBds) 
            {
                rxdma->rxHeadIndex = 0;
            }
            rxdma->rxAssignedBds--;
        }
    } 
    else   /* out of buffers! */
       return (uint32)0xFFFF; 

    //printk("XtmRecv_Iudma end ch: %d head: %d tail: %d assigned: %d\n", channel, rxdma->rxHeadIndex, rxdma->rxTailIndex, rxdma->rxAssignedBds);

    FAP4KE_IUDMA_PMON_END(FAP4KE_PMON_ID_IUDMA_RECV);

    return dmaDesc.word0;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmXmitAvailable
 Purpose: Determine if there are free resources for the xmit
   Notes: channel in XTM mode refers to a specific TXQINFO struct of a 
          specific XTM Context
-------------------------------------------------------------------------- */
int bcmPktDma_XtmXmitAvailable_Iudma(int channel, uint32 dmaType)
{
    BcmPktDma_LocalXtmTxDma *txdma;
    BcmPktDma_LocalXtmTxDma **ppXtmTxDma ;

    ppXtmTxDma = (dmaType == XTM_HW_DMA) ? g_pXtmTxDma : g_pXtmSwTxDma ;

    
    txdma = ppXtmTxDma[channel];
    
    if (txdma->txFreeBds != 0)  return 1;
    
    return 0;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmTxEnable_Iudma
 Purpose: Coordinate with FAP for tx enable
  Return: 1 on success; 0 otherwise
-------------------------------------------------------------------------- */
int bcmPktDma_XtmTxEnable_Iudma( int channel, PDEV_PARAMS unused, uint32 dmaType )
{
    BcmPktDma_LocalXtmTxDma *txdma;
    BcmPktDma_LocalXtmTxDma **ppXtmTxDma ;

    ppXtmTxDma = (dmaType == XTM_HW_DMA) ? g_pXtmTxDma : g_pXtmSwTxDma ;

    //printk("bcmPktDma_XtmTxEnable_Iudma ch: %d\n", channel);

    txdma = ppXtmTxDma[channel];
    txdma->txEnabled = 1;

    /* The other SW entity which reads from this DMA in case of SW_DMA,
     * will always look at this bit to start processing anything from
     * the DMA queue.
     */
    if (dmaType == XTM_SW_DMA)
       txdma->txDma->cfg = DMA_ENABLE ;
    return 1;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmTxDisable_Iudma
 Purpose: Coordinate with FAP for tx disable
  Return: 1 on success; 0 otherwise
-------------------------------------------------------------------------- */
int bcmPktDma_XtmTxDisable_Iudma( int channel, uint32 dmaType, void (*func) (uint32 param1,
         BcmPktDma_XtmTxDma *txswdma), uint32 param1)
{
    int j;
    BcmPktDma_LocalXtmTxDma *txdma;
    BcmPktDma_LocalXtmTxDma **ppXtmTxDma ;

    ppXtmTxDma = (dmaType == XTM_HW_DMA) ? g_pXtmTxDma : g_pXtmSwTxDma ;

    txdma = ppXtmTxDma[channel];
    txdma->txEnabled = 0;

    /* Changing txEnabled to 0 prevents any more packets
     * from being queued on a transmit DMA channel.  Allow all currenlty
     * queued transmit packets to be transmitted before disabling the DMA.
     */

    if (dmaType == XTM_HW_DMA) {

       for (j = 0; j < 2000 && (txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE; j++)
       {
#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
          udelay(500);
#else
          {
             /* Increase wait from .1 sec to .5 sec to handle longer XTM packets - May 2010 */
             uint32 prevJiffies = fap4keTmr_jiffies + (FAPTMR_HZ) / 2; /* .5 sec */

             while(!fap4keTmr_isTimeAfter(fap4keTmr_jiffies, prevJiffies)); 

             if((txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE) 
             {
                return 0;    /* return so caller can handle the failure */
             }
          }
#endif
       }

       if ((txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE)
       {
          /* This should not happen. */
          txdma->txDma->cfg = DMA_PKT_HALT;
#if !defined(CONFIG_BCM_FAP) && !defined(CONFIG_BCM_FAP_MODULE)
          udelay(500);
#else
          /* This will not happen in the FAP/FAP_MODULE case */
#endif
          txdma->txDma->cfg = 0;
          if ((txdma->txDma->cfg & DMA_ENABLE) == DMA_ENABLE)
             return 0;    /* return so caller can handle the failure */
       }
    } /* if DMA is HW Type */
    else {

       /* No blocking wait for SW DMAs */
       (*func)(param1, txdma) ;
    }

    return 1;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmRxEnable
    Purpose: Enable rx DMA for the given channel.  
    Return: 1 on success; 0 otherwise
-------------------------------------------------------------------------- */

int bcmPktDma_XtmRxEnable_Iudma( int channel )
{
    BcmPktDma_LocalXtmRxDma *rxdma = g_pXtmRxDma[channel];

    //printk("bcmPktDma_XtmRxEnable_Iudma channel: %d\n", channel);

    rxdma->rxDma->cfg |= DMA_ENABLE;
    rxdma->rxEnabled = 1;

    return 1;
}

/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmRxDisable
 Purpose: Disable rx interrupts for the given channel
  Return: 1 on success; 0 otherwise
-------------------------------------------------------------------------- */

int bcmPktDma_XtmRxDisable_Iudma( int channel )
{
    BcmPktDma_LocalXtmRxDma  *rxdma;
    int                       i;

    //printk("bcmPktDma_XtmRxDisable_Iudma channel: %d\n", channel);

    rxdma = g_pXtmRxDma[channel];
    rxdma->rxEnabled = 0;

    rxdma->rxDma->cfg &= ~DMA_ENABLE;
    for (i = 0; rxdma->rxDma->cfg & DMA_ENABLE; i++) 
    {
        rxdma->rxDma->cfg &= ~DMA_ENABLE;
    
        if (i >= 100)
        {
            //printk("Failed to disable RX DMA?\n");
            return 0;
        }

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
        /* The delay only works when called from Linux */
        udelay(20);
#else
        {
            uint32 prevJiffies = fap4keTmr_jiffies;
                                                                                    
            while(!fap4keTmr_isTimeAfter(fap4keTmr_jiffies, prevJiffies)); 

            if((rxdma->rxDma->cfg & DMA_ENABLE) == DMA_ENABLE) 
            {
                return 0;    /* return so caller can handle the failure */
            }
        }
#endif
    }

    return 1;
}


/* --------------------------------------------------------------------------
    Name: bcmPktDma_XtmForceFreeXmitBufGet
 Purpose: Gets a TX buffer to free by caller, ignoring DMA_OWN status
-------------------------------------------------------------------------- */
BOOL bcmPktDma_XtmForceFreeXmitBufGet_Iudma(int channel, uint32 *pKey,
                                            uint32 *pTxSource, uint32 *pTxAddr,
                                            uint32 *pRxChannel, uint32 dmaType,
                                            uint32 noGlobalBufAccount)
{
    BOOL ret = FALSE;
    int  bdIndex;
    BcmPktDma_LocalXtmTxDma *txdma;
    BcmPktDma_LocalXtmTxDma **ppXtmTxDma ;

    ppXtmTxDma = (dmaType == XTM_HW_DMA) ? g_pXtmTxDma : g_pXtmSwTxDma ;

    txdma = ppXtmTxDma[channel];
    bdIndex = txdma->txHeadIndex;
    *pKey = 0;
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
    /* TxSource & TxAddr not required in non-FAP applications */
    *pTxSource = 0;
    *pTxAddr   = 0;
    *pRxChannel = 0;
#endif

    /* Reclaim transmitted buffers */
    if (txdma->txFreeBds < txdma->ulQueueSize)
    {
       /* Dont check for DMA_OWN status */
        {
           *pKey = txdma->txRecycle[bdIndex].key;
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
           *pTxSource = txdma->txRecycle[bdIndex].source;
           *pTxAddr = txdma->txRecycle[bdIndex].address;
           *pRxChannel = txdma->txRecycle[bdIndex].rxChannel;
#endif

           if (++txdma->txHeadIndex == txdma->ulQueueSize)
               txdma->txHeadIndex = 0;

           txdma->txFreeBds++;
           txdma->ulNumTxBufsQdOne--;
#if !defined(CONFIG_BCM96816) && !defined(CONFIG_BCM96362)
           // FIXME - Which chip uses more then one TX queue?
           if (!noGlobalBufAccount)
           g_pXtmGlobalInfo->ulNumTxBufsQdAll--;
#endif

           ret = TRUE;
        }
    }

    return ret;
}


/* Return non-aligned, cache-based pointer to caller - Apr 2010 */
DmaDesc *bcmPktDma_XtmAllocTxBds(int channel, int numBds)
{
    /* Allocate space for pKeyPtr, pTxSource and pTxAddress as well as BDs. */
    int size = sizeof(DmaDesc) + sizeof(BcmPktDma_txRecycle_t);

#if defined(XTM_TX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
{
    uint8 * pMem;
    static uint8 * txBdAllocation[MAX_TRANSMIT_QUEUES] = {NULL};
    static int txNumBds[MAX_TRANSMIT_QUEUES] = {0};

    /* Restore previous BD allocation pointer if any */
    pMem = txBdAllocation[channel];

    if (pMem) 
    {
        if(txNumBds[channel] != numBds)
        {
            printk("ERROR: Tried to allocate a different number of txBDs (was %d, attempted %d)\n",
                    txNumBds[channel], numBds);
            printk("       Xtm tx BD allocation rejected!!\n");
            return( NULL );
}
        memset(pMem, 0, numBds * size);
        return((DmaDesc *)pMem);   /* tx bd ring + pKeyPtr, pTxSource and pTxAddress */
    }

    /* Try to allocate Tx Descriptors in PSM. Use Host-side addressing here. */
    /* fapDrv_psmAlloc guarantees 8 byte alignment. */
    pMem = bcmPktDma_psmAlloc(numBds * size);
    if(pMem != FAP4KE_OUT_OF_PSM)
    {
        memset(pMem, 0, numBds * size);
        txBdAllocation[channel] = pMem;
        txNumBds[channel] = numBds;
        return((DmaDesc *)pMem);   /* tx bd ring + pKeyPtr, pTxSource and pTxAddress */
}

    printk("ERROR: Out of PSM. Xtm tx BD allocation rejected!!\n");
    return(NULL);
}
#else  /* !defined(XTM_TX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)) */
{
    void * p;

    /* Allocate Tx Descriptors in DDR */
    /* Leave room for alignment by caller - Apr 2010 */
    p = kmalloc(numBds * size + 0x10, GFP_ATOMIC) ;
    if (p !=NULL) {
        memset(p, 0, numBds * size + 0x10);
        cache_flush_len(p, numBds * size + 0x10);
    }
    return( (DmaDesc *)p );   /* tx bd ring + pKeyPtr, pTxSource and pTxAddress */
}
#endif   /* defined(XTM_TX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)) */
}

/* Return non-aligned, cache-based pointer to caller - Apr 2010 */
DmaDesc *bcmPktDma_XtmAllocRxBds(int channel, int numBds)
{
#if defined(XTM_RX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
{
    uint8 * pMem;
    static uint8 * rxBdAllocation[MAX_RECEIVE_QUEUES] = {NULL};
    static int rxNumBds[MAX_RECEIVE_QUEUES] = {0};

    /* Restore previous BD allocation pointer if any */
    pMem = rxBdAllocation[channel];

    if (pMem) 
    {
        if(rxNumBds[channel] != numBds)
        {
            printk("ERROR: Tried to allocate a different number of rxBDs (was %d, attempted %d)\n",
                    rxNumBds[channel], numBds);
            printk("       Xtm rx BD allocation rejected!!\n");
            return( NULL );
        }
        memset(pMem, 0, numBds * sizeof(DmaDesc));
        return((DmaDesc *)pMem);   /* rx bd ring */
    }

    /* Try to allocate Rx Descriptors in PSM. Use Host-side addressing here. */
    /* fapDrv_psmAlloc guarantees 8 byte alignment. */
    pMem = bcmPktDma_psmAlloc(numBds * sizeof(DmaDesc));
    if(pMem != FAP4KE_OUT_OF_PSM)
    {
        memset(pMem, 0, numBds * sizeof(DmaDesc));
        rxBdAllocation[channel] = pMem;
        rxNumBds[channel] = numBds;
        return((DmaDesc *)pMem);   /* rx bd ring */
    }

    printk("ERROR: Out of PSM. Xtm rx BD allocation rejected!!\n");
    return( NULL );  
}
#else   /* !defined(XTM_RX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)) */
{
    void * p;

    /* Allocate Rx Descriptors in DDR */
    /* Leave room for alignment by caller - Apr 2010 */
    p = kmalloc(numBds * sizeof(DmaDesc) + 0x10, GFP_ATOMIC) ;
    if (p != NULL) {
        memset(p, 0, numBds * sizeof(DmaDesc) + 0x10);
        cache_flush_len(p, numBds * sizeof(DmaDesc) + 0x10);
    }
    return((DmaDesc *)p);   /* rx bd ring */
}
#endif   /* defined(XTM_RX_BDS_IN_PSM) && (defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)) */
}


EXPORT_SYMBOL(g_pXtmGlobalInfo);

EXPORT_SYMBOL(bcmPktDma_XtmInitRxChan_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmInitTxChan_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmSelectRxIrq_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmClrRxIrq_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmRecvAvailable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmRecv_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmXmitAvailable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmXmit_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmTxEnable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmTxDisable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmRxEnable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmRxDisable_Iudma);
EXPORT_SYMBOL(bcmPktDma_XtmForceFreeXmitBufGet_Iudma);

#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
EXPORT_SYMBOL(g_pXtmRxDma);
EXPORT_SYMBOL(g_pXtmTxDma);
EXPORT_SYMBOL(g_pXtmSwTxDma);
#endif

#endif  /* #ifndef CONFIG_BCM96816 */
