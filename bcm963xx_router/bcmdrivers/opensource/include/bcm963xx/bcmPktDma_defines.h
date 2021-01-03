#ifndef __PKTDMA_DEFINES_H_INCLUDED__
#define __PKTDMA_DEFINES_H_INCLUDED__

#define ENET_RX_CHANNELS_MAX  4
#define ENET_TX_CHANNELS_MAX  4
#define XTM_RX_CHANNELS_MAX   4
#define XTM_TX_CHANNELS_MAX   16

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
#define NR_RX_BDS_MAX           400
#define NR_RX_BDS(x)            NR_RX_BDS_MAX
#define NR_TX_BDS               180
#define XTM_NR_TX_BDS           NR_RX_BDS_MAX
#elif defined(CONFIG_BCM96816)
/* Keep 4_06 branch bcmnet.h in sync with Devel branch - Apr 2010 */
#define NR_RX_BDS_MAX           3000
#define NR_RX_BDS(x)            NR_RX_BDS_MAX
#define NR_TX_BDS               2500
#else
#define NR_RX_BDS_MAX           400
#define NR_RX_BDS(x)            NR_RX_BDS_MAX
#define NR_TX_BDS               180
#endif

#endif /* __PKTDMA_DEFINES_H_INCLUDED__ */
