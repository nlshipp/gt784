#ifndef __FAP4KE_MEMORY_H_INCLUDED__
#define __FAP4KE_MEMORY_H_INCLUDED__

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
 * File Name  : fap4ke_dspram.h
 *
 * Description: This file contains ...
 *
 *******************************************************************************
 */

#include "fap4ke_init.h"
#include "fap4ke_timers.h"
#include "fap4ke_irq.h"
#include "fap_dqm.h"
#include "fap4ke_packet.h"
#include "fap4ke_iopDma.h"
#include "fap4ke_mailBox.h"
#include "bcmPktDma_structs.h"
#include "fap4ke_dpe.h"
#include "fap4ke_xtmrt.h"

/***************************************************
 * 4ke Data Scratch Pad Ram (DSPRAM) Mappings
 ***************************************************/

// Uncomment to place BcmPktDma_LocalEthRxDma & BcmPktDma_LocalEthTxDma in PSM
//#define COMPARE_PSM_IUDMA_PERF

/* In the 6362 we will always keep the XTM Tx BDs in DDR because the upstream
   bandwidth is fairly small, and we do not have enough space in the PSM to 
   allocate the BDs required for 16 channels */
#define ENET_RX_BDS_IN_PSM
#define ENET_TX_BDS_IN_PSM
#define XTM_RX_BDS_IN_PSM
//#define XTM_TX_BDS_IN_PSM

// Uncomment to enable enet rx polling in the 4ke task loop
//#define ENABLE_ENET_RX_POLLING

// Uncomment to enable counters to examine the enet comms between FAP and Host - May 2010
//#define ENABLE_FAP_COMMS_DEBUG

#if defined(ENABLE_FAP_COMMS_DEBUG)
#define ENET_TX_HOST_2_FAP_REJECT		0
#define ENET_RXFREE_HOST_2_FAP_WAIT		1
#define ENET_RX_FAP_2_HOST_REJECT		2
#define ENET_TXFREE_FAP_2_HOST_WAIT		3
#define ENET_RX_NO_BDS					4
#endif

#define FAP_PSM_MANAGED_MEMORY_SIZE 17828	/* bytes available in PSM for enet & xtmrt BDs - Apr 2010 */

#define p4keDspram ( (fap4keDspram_alloc_t *)(DSPRAM_VBASE) )

#define p4keDspramGbl ( (fap4keDspram_global_t *)(&p4keDspram->global) )

/*
 * fap4keDspram_timers_t: all Timers must be declared here
 */
typedef struct {
    /* Timer Management */
    volatile int64 jiffies64;
    Dll_t timersList;

    /* CPU Utilization */
    fap4keTmr_cpuSample_t cpu;

    /* User-defined timers */
    fap4keTmr_timer_t keepAlive;
    fap4keTmr_timer_t flowStatsTimer;
} fap4keDspram_timers_t;

/*
 * fap4keDspram_tasks_t: all Tasks must be declared here
 */
typedef struct {
    /* Task Management */
    fap4keTsk_scheduler_t scheduler;

    /* User-defined Tasks */
    fap4keTsk_task_t enetRecv0;
    fap4keTsk_task_t enetRecv1;
    fap4keTsk_task_t xtmRecv0;
    fap4keTsk_task_t xtmRecv1;
} fap4keDspram_tasks_t;

/*
 * fap4keDspram_irq_t: Interrupt Management variables
 */
typedef struct {
    /* Interrupt Management */
    uint32 handlerCount;
    uint32 wait_pc;
    uint32 epc_jump;
    fap4keIrq_handlerInfo_t handlerInfo[FAP4KE_IRQ_HANDLERS_MAX];
} fap4keDspram_irq_t;

/*
 * fap4keDqm_handlerInfo_t: DQM queue handler structure
 */
typedef struct {
   uint32                   mask;
   uint32                   enable;
   fap4keTsk_taskPriority_t taskPriority;
   fap4keTsk_task_t         task;
} fap4keDqm_handlerInfo_t;

/*
 * dqmStats_s: DQM statistics
 */
typedef struct {
    uint32       count;
} dqmStats_s;

/*
 * fap4keDspram_dqm_t: DQM variables
 */ 
typedef struct{
    /* Queue Handlers */
    uint32 handlerCount;
    fap4keDqm_handlerInfo_t handlerInfo[DQM_MAX_HANDLER];
    dqmStats_s stats; 
} fap4keDspram_dqm_t;

/*
 * fap4keDspram_enet_t: ENET variables
 */ 
typedef struct{
    fap4keTsk_task_t *xmitFromHostTask;
    fap4keTsk_task_t *recvFreeFromHostTask;
    /* Channel Bitmap used to enable/disable Tx cleanup for each iuDMA channel */
    uint32 txCleanupChannelMap;
} fap4keDspram_enet_t;

/*
 * fap4keDspram_xtm_t: XTM variables
 */ 
typedef struct{
    fap4keTsk_task_t *xmitFromHostTask;
    fap4keTsk_task_t *recvFreeFromHostTask;
    /* Channel Bitmap used to enable/disable Tx cleanup for each iuDMA channel */
    uint32 txCleanupChannelMap;
    fap4keXtm_devMap_t devMap;
    fap4keXtm_qos_t qos;
} fap4keDspram_xtm_t;

/*
 * fap4keDspram_dpe_t: DPE variables
 */ 
typedef struct{
    fap4keDpe_handlerInfo_t handlerInfo[2];
} fap4keDspram_dpe_t;

/*
 * fap4keDspram_packet_t: Packet processing variables
 */ 
typedef struct {
    fap4kePkt_runtime_t runtime;
} fap4keDspram_packet_t;

/*
 * fap4keDspram_global_t: contains all global variables stored in DSPRAM
 */
typedef struct {
    /* Timers */
    fap4keDspram_timers_t timers;
    /* Tasks */
    fap4keDspram_tasks_t tasks;
    /* Interrupts */
    fap4keDspram_irq_t irq;
    /* DQM */
    fap4keDspram_dqm_t dqm;
    /* ENET */
    fap4keDspram_enet_t enet;
    /* XTM */
    fap4keDspram_xtm_t xtm;
    /* Packet */
    fap4keDspram_packet_t packet;
#ifndef COMPARE_PSM_IUDMA_PERF
    /* Eth Iudma */
    BcmPktDma_LocalEthRxDma EthRxDma[ENET_RX_CHANNELS_MAX];
    BcmPktDma_LocalEthTxDma EthTxDma[ENET_TX_CHANNELS_MAX];
    BcmPktDma_LocalEthRxDma *g_pEthRxDma[ENET_RX_CHANNELS_MAX];
    BcmPktDma_LocalEthTxDma *g_pEthTxDma[ENET_TX_CHANNELS_MAX];

    /* Xtm Iudma */
    BcmPktDma_LocalXtmRxDma XtmRxDma[XTM_RX_CHANNELS_MAX];
    BcmPktDma_LocalXtmTxDma XtmTxDma[XTM_TX_CHANNELS_MAX];
    BcmPktDma_LocalXtmRxDma *g_pXtmRxDma[XTM_RX_CHANNELS_MAX];
    BcmPktDma_LocalXtmTxDma *g_pXtmTxDma[XTM_TX_CHANNELS_MAX];
#endif
    /* DPE */
    fap4keDspram_dpe_t dpe;
} fap4keDspram_global_t;
 
/*
 * fap4keDspram_alloc_t: used to manage the overall 4ke DSPRAM allocation
 */
typedef struct {
    /* 4ke stack: Never write to this area!!! */
    volatile const uint8 stack4ke[FAP_INIT_4KE_STACK_SIZE];

    union {
        volatile const uint8 global_u8[DSPRAM_SIZE - FAP_INIT_4KE_STACK_SIZE];

        fap4keDspram_global_t global;
    };
} fap4keDspram_alloc_t;


/***************************************************
 * SDRAM Mappings
 ***************************************************/

/*
 * We need to avoid global allocations in SDRAM to be cached by the Host MIPS,
 * by aligning all SDRAM allocations to a Host D$ line size, and making the
 * allocations to be an integer multiple of the Host D$ line size
 */

#define FAP4KE_SDRAM_ALLOC_SIZE 4096 /* bytes */

typedef struct {
    fap4keTmr_timer_t testTimer;
} fap4keSdram_main_t;

typedef struct {
    uint16 printCount4ke;
    uint16 keepAliveCount4ke;
} fap4keSdram_mailBox_t;

typedef struct {
    /* used in QSM memory management */
    uint32 availableMemory;
    uint32 nextAddress;

    /* test code */
    fap4keTmr_timer_t dqmTestTimer;
    uint32 inc;
} fap4keSdram_dqm_t;

typedef union {
    uint8 u8[FAP4KE_SDRAM_ALLOC_SIZE];

    struct {
        uint8 resv1[16];
        fap4keSdram_mailBox_t mailBox;
        fap4keSdram_main_t main;
        fap4keSdram_dqm_t dqm;
        uint8 localPrintBuf[FAP_MAILBOX_PRINTBUF_SIZE];
        uint8 resv2[16];
    } alloc;
} __attribute__((aligned(16))) fap4keSdram_alloc_t;

/* declared in fap4ke_main.c */
extern fap4keSdram_alloc_t fap4keSdram_g;

#define p4keSdram ( (fap4keSdram_alloc_t *)(mCacheToNonCacheVirtAddr(&fap4keSdram_g)) )


/***************************************************
 * Packet Shared Memory (PSM) Mappings
 ***************************************************/

#define p4kePsm ( (volatile fap4kePsm_alloc_t *)(FAP_PSM_BASE) )

#define p4kePsmGbl ( (fap4kePsm_global_t *)(&p4kePsm->global) )

#define pHostPsm ( (volatile fap4kePsm_alloc_t *)(FAP_HOST_PSM_BASE) )

#define pHostPsmGbl ( (fap4kePsm_global_t *)(&pHostPsm->global) )

/*
 * fap4kePsm_packet_t: Packet Manager Variables, including flow definitions
 */
typedef struct {
    fap4kePkt_tables_t tables;
} fap4kePsm_packet_t;

/*
 * fap4kePsm_timers_t: Timers variables in PSM
 */
typedef struct {
    fap4keTmr_cpuHistory_t cpu;
} fap4kePsm_timers_t;


//#define CC_FAP4KE_PMON

#if defined(CC_FAP4KE_PMON)
/*
 * fap4kePsm_pmon_t: Performance Monitoring variables in PSM
 */
typedef enum {
    FAP4KE_PMON_ID_NOP50 = 0,
    FAP4KE_PMON_ID_REG_WR,
    FAP4KE_PMON_ID_REG_RD,
    FAP4KE_PMON_ID_ENET_RX_BEGIN,
    FAP4KE_PMON_ID_ENET_RX_HEADER,
    FAP4KE_PMON_ID_ENET_RX_CLASSIFY,
    FAP4KE_PMON_ID_ENET_RX_XMIT,
    FAP4KE_PMON_ID_ENET_RX_END,
    FAP4KE_PMON_ID_XTM_RX_BEGIN,
    FAP4KE_PMON_ID_XTM_RX_HEADER,
    FAP4KE_PMON_ID_XTM_RX_CLASSIFY,
    FAP4KE_PMON_ID_XTM_RX_XMIT,
    FAP4KE_PMON_ID_XTM_RX_END,
    FAP4KE_PMON_ID_DMA_RX_START,
    FAP4KE_PMON_ID_DMA_RX_FINISH,
    FAP4KE_PMON_ID_CLASSIFY,
    FAP4KE_PMON_ID_SW_MOD,
    FAP4KE_PMON_ID_DMA_TX_START,
    FAP4KE_PMON_ID_DMA_TX_FINISH,
    FAP4KE_PMON_ID_FFE1,
    FAP4KE_PMON_ID_FFE2,
    FAP4KE_PMON_ID_FFE3,
    FAP4KE_PMON_ID_IUDMA_XMIT,
    FAP4KE_PMON_ID_IUDMA_RECV,
    FAP4KE_PMON_ID_IUDMA_FREEXMITBUFGET,
    FAP4KE_PMON_ID_IUDMA_FREERECVBUF,
    FAP4KE_PMON_ID_MAX
} fap4kePsm_pmonId_t;

#undef FAP_DECL
#define FAP_DECL(x) #x,

#define FAP4KE_PMON_ID_NAME \
    {                                                   \
        FAP_DECL(FAP4KE_PMON_ID_NOP50)                  \
        FAP_DECL(FAP4KE_PMON_ID_REG_WR)                 \
        FAP_DECL(FAP4KE_PMON_ID_REG_RD)                 \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RX_BEGIN)          \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RX_HEADER)         \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RX_CLASSIFY)       \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RX_XMIT)           \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RX_END)            \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_BEGIN)           \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_HEADER)          \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_CLASSIFY)        \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_XMIT)            \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_END)             \
        FAP_DECL(FAP4KE_PMON_ID_DMA_RX_START)           \
        FAP_DECL(FAP4KE_PMON_ID_DMA_RX_FINISH)          \
        FAP_DECL(FAP4KE_PMON_ID_CLASSIFY)               \
        FAP_DECL(FAP4KE_PMON_ID_SW_MOD)                 \
        FAP_DECL(FAP4KE_PMON_ID_DMA_TX_START)           \
        FAP_DECL(FAP4KE_PMON_ID_DMA_TX_FINISH)          \
        FAP_DECL(FAP4KE_PMON_ID_FFE1)                   \
        FAP_DECL(FAP4KE_PMON_ID_FFE2)                   \
        FAP_DECL(FAP4KE_PMON_ID_FFE3)                   \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_XMIT)             \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_RECV)             \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_FREEXMITBUFGET)   \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_FREERECVBUF)      \
    }

typedef struct {
    uint32 globalIrqs;
    uint32 halfCycles[FAP4KE_PMON_ID_MAX];
    uint32 instncomplete[FAP4KE_PMON_ID_MAX];
    uint32 icachehit[FAP4KE_PMON_ID_MAX];
    uint32 icachemiss[FAP4KE_PMON_ID_MAX];
    uint32 interrupts[FAP4KE_PMON_ID_MAX];
} fap4kePsm_pmon_t;
#endif /* CC_FAP4KE_PMON */

typedef struct {
/* Stats for 4ke enet rx (iuDMA interrupt) */
    uint32 rxCount;             /* snapshot */
    uint32 rxHighWm;            /* peak value */
    int32 txCount;
    uint32 rxTotal;
    uint32 rxDropped;
    uint32 rxNoBd;
    uint32 rxAssignedBdsMin;    /* peak value */
    uint32 txFreeBdsMin;        /* peak value */
/* Stats for Host enet rx (Q7) */
    uint32 Q7budget;            /* snapshot */
    uint32 Q7rxCount;           /* snapshot */
    uint32 Q7rxHighWm;          /* peak value */
    uint32 Q7rxTotal;
/* Stats for 4ke enet rx free (Q12) */
    uint32 Q12rxCount;          /* snapshot */
    uint32 Q12rxHighWm;         /* peak value */
    uint32 Q12rxTotal;
/* Stats for 4ke enet tx (Q11) */
    uint32 Q11txCount;          /* snapshot */
    uint32 Q11txHighWm;         /* peak value */
    uint32 Q11txTotal;
/* Stats for Host enet tx free (Q13) */
    uint32 Q13txCount;          /* snapshot */
    uint32 Q13txHighWm;         /* peak value */
    uint32 Q13txTotal;
} fap4kePsm_stats_t;

//#define CC_FAP4KE_TRACE

#if defined(CC_FAP4KE_TRACE)
#define FAP4KE_TRACE_HISTORY_SIZE 300

#undef FAP4KE_DECL
#define FAP4KE_DECL(x) #x,

#define FAP4KE_TRACE_TYPE_NAME       \
    {                                \
        FAP4KE_DECL(RX_BEGIN)        \
        FAP4KE_DECL(RX_PACKET)       \
        FAP4KE_DECL(RX_END)          \
        FAP4KE_DECL(TX_BEGIN)        \
        FAP4KE_DECL(TX_FREE)         \
        FAP4KE_DECL(TX_PACKET)       \
        FAP4KE_DECL(TX_END)          \
        FAP4KE_DECL(IRQ_BEGIN)       \
        FAP4KE_DECL(IRQ_CALL_START)  \
        FAP4KE_DECL(IRQ_CALL_END)    \
        FAP4KE_DECL(IRQ_END)         \
        FAP4KE_DECL(TASK)            \
        FAP4KE_DECL(WAIT_START)      \
        FAP4KE_DECL(WAIT_END)        \
    }

typedef enum {
    FAP4KE_TRACE_RX_BEGIN,
    FAP4KE_TRACE_RX_PACKET,
    FAP4KE_TRACE_RX_END,

    FAP4KE_TRACE_TX_BEGIN,
    FAP4KE_TRACE_TX_FREE,
    FAP4KE_TRACE_TX_PACKET,
    FAP4KE_TRACE_TX_END,

    FAP4KE_TRACE_IRQ_BEGIN,
    FAP4KE_TRACE_IRQ_CALL_START,
    FAP4KE_TRACE_IRQ_CALL_END,
    FAP4KE_TRACE_IRQ_END,

    FAP4KE_TRACE_TASK,
    FAP4KE_TRACE_WAIT_START,
    FAP4KE_TRACE_WAIT_END,

    FAP4KE_TRACE_MAX
} fap4keTrace_id_t;

typedef enum {
    FAP4KE_TRACE_TYPE_DEC,
    FAP4KE_TRACE_TYPE_HEX,
    FAP4KE_TRACE_TYPE_STR,
    FAP4KE_TRACE_TYPE_MAX
} fap4keTrace_type_t;

typedef struct {
    fap4keTrace_id_t id;
    uint32_t cycles;
    uint32_t arg;
    fap4keTrace_type_t type;
} fap4keTrace_record_t;

typedef struct {
    uint32_t write;
    uint32_t count;
    fap4keTrace_record_t record[FAP4KE_TRACE_HISTORY_SIZE];
} fap4keTrace_history_t;

/*
 * fap4kePsm_trace_t: 4ke Trace variables in PSM
 */
typedef struct {
    uint32_t enable;
    fap4keTrace_history_t history;
} fap4kePsm_trace_t;

#define p4keTrace ( &p4kePsmGbl->trace )
#define pHostTrace ( &pHostPsmGbl->trace )
#endif /* CC_FAP4KE_TRACE */

/*
 * fap4kePsm_global_t: contains all global variables stored in the PSM
 */
typedef struct {
    fap4kePsm_packet_t packet;
    fap4kePsm_timers_t timers;
#if defined(CC_FAP4KE_PMON)
    fap4kePsm_pmon_t pmon;
#endif
    fap4kePsm_stats_t stats;
    fap4kePsm_stats_t xtmStats;
#if defined(COMPARE_PSM_IUDMA_PERF)
    /* Eth Iudma */
    BcmPktDma_LocalEthRxDma EthRxDma[ENET_RX_CHANNELS_MAX];
    BcmPktDma_LocalEthTxDma EthTxDma[ENET_TX_CHANNELS_MAX];
    BcmPktDma_LocalEthRxDma *g_pEthRxDma[ENET_RX_CHANNELS_MAX];
    BcmPktDma_LocalEthTxDma *g_pEthTxDma[ENET_TX_CHANNELS_MAX];

    /* Xtm Iudma */
    BcmPktDma_LocalXtmRxDma XtmRxDma[XTM_RX_CHANNELS_MAX];
    BcmPktDma_LocalXtmTxDma XtmTxDma[XTM_TX_CHANNELS_MAX];
    BcmPktDma_LocalXtmRxDma *g_pXtmRxDma[XTM_RX_CHANNELS_MAX];
    BcmPktDma_LocalXtmTxDma *g_pXtmTxDma[XTM_TX_CHANNELS_MAX];
#endif

    /* Global flag to coordinate XTM tx disable between 4ke and Host - May 2010 */
    uint8 XtmTxDownFlags[XTM_TX_CHANNELS_MAX];

    /* ManagedMemory replaces TxKeys, txSources, txAddresses, enet & xtm rx/tx BDs - Apr 2010 */
    uint8   ManagedMemory[FAP_PSM_MANAGED_MEMORY_SIZE];
    uint8 * pManagedMemory;

#if defined(CC_FAP4KE_TRACE)
    fap4kePsm_trace_t trace;
#endif

#if defined(ENABLE_FAP_COMMS_DEBUG)
	int debug_ctrs[5];
#endif

    int blockHalt;
} fap4kePsm_global_t;

typedef union {
    uint8 u8[FAP_PSM_SIZE];
    uint32 u32[FAP_PSM_SIZE_32];

    fap4kePsm_global_t global;
} fap4kePsm_alloc_t;

/***************************************************
 * Miscellaneous
 ***************************************************/

fapRet fap4keHw_dspramCheck(void);
fapRet fap4keHw_psmCheck(void);

#endif  /* defined(__FAP4KE_MEMORY_H_INCLUDED__) */
