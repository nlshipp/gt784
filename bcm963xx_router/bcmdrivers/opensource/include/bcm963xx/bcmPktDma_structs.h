#ifndef __PKTDMASTRUCTS_H_INCLUDED__
#define __PKTDMASTRUCTS_H_INCLUDED__

#include <bcm_map.h>
#include "bcmPktDma_defines.h"

/* NOTE: Keep this in sync with BcmPktDma_EthRxDma in bcmenet.h */
typedef struct BcmPktDma_LocalEthRxDma
{
   int      enetrxchannel_isr_enable;
   volatile DmaChannelCfg *rxDma;
   volatile DmaDesc *rxBdsBase;   /* save non-aligned address for free */
   volatile DmaDesc *rxBds;
   int      rxAssignedBds;
   int      rxHeadIndex;
   int      rxTailIndex;
   int      numRxBds;
   volatile int  rxEnabled;
} BcmPktDma_LocalEthRxDma;

typedef struct {
    uint32 key;
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
    uint32 address;
    uint16 source;
    uint16 rxChannel;
#endif
} BcmPktDma_txRecycle_t;

/* NOTE: Keep this in sync with BcmPktDma_EthTxDma in bcmenet.h */
typedef struct BcmPktDma_LocalEthTxDma
{
   int      txFreeBds; /* # of free transmit bds */
   int      txHeadIndex;
   int      txTailIndex;
   volatile DmaChannelCfg *txDma;    /* location of transmit DMA register set */
#if (defined(CONFIG_BCM96816) && defined(DBL_DESC))
    volatile DmaDesc16 *txBdsBase;   /* save non-aligned address for free */
    volatile DmaDesc16 *txBds;       /* Memory location of tx Dma BD ring */
#else
    volatile DmaDesc *txBdsBase;   /* save non-aligned address for free */
    volatile DmaDesc *txBds;       /* Memory location of tx Dma BD ring */
#endif
   BcmPktDma_txRecycle_t *txRecycle;
   volatile int txEnabled;
} BcmPktDma_LocalEthTxDma;

/* NOTE: Keep this in sync with BcmPktDma_XtmRxDma in bcmxtmrtimpl.h */
typedef struct BcmPktDma_LocalXtmRxDma
{
   int                     xtmrxchannel_isr_enable;
   volatile DmaChannelCfg *rxDma;
   volatile DmaDesc       *rxBdsBase;   /* save non-aligned address for free */
   volatile DmaDesc       *rxBds;
   int                     rxAssignedBds;
   int                     rxHeadIndex;
   int                     rxTailIndex;
   int                     numRxBds;
   volatile int            rxEnabled;
} BcmPktDma_LocalXtmRxDma;

/* NOTE: Keep this in sync with BcmPktDma_XtmTxDma in bcmxtmrtimpl.h */
typedef struct BcmPktDma_LocalXtmTxDma
{
    UINT32 ulPort;
    UINT32 ulPtmPriority;
    UINT32 ulSubPriority;
    UINT32 ulQueueSize;
    UINT32 ulDmaIndex;
    UINT32 ulNumTxBufsQdOne;

    int                     txFreeBds; 
    int                     txHeadIndex;
    int                     txTailIndex;
    volatile DmaChannelCfg *txDma;
    volatile DmaDesc       *txBdsBase;   /* save non-aligned address for free */
    volatile DmaDesc       *txBds;
    BcmPktDma_txRecycle_t  *txRecycle;
    volatile int            txEnabled;

    /* Field added for xtmrt dmaStatus field generation for xtm flows - Apr 2010 */
    uint32                  baseDmaStatus;

    /* Field added for SW Scheduler in BCM6368. Only Manipulted/controlled by
     * SW Scheduler that runs in BCM6368 PTM bonded mode.
     */
    int                     txSchedHeadIndex ;
    /* For SW WFQ implementation */
    UINT16 ulAlg;
    UINT16 ulWeightValue;
} BcmPktDma_LocalXtmTxDma;


#endif
