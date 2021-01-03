/*
 * Linux-specific portion of
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wl_linux.c,v 1.7 2011/04/14 23:21:47 dkhoo Exp $
 */

#define LINUX_PORT

#define __UNDEF_NO_VERSION__

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#include <linux/module.h>
#endif

#ifdef WL_UMK
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#error "No support for Kernel Rev < 2.6.0"
#endif
#endif /* WL_UMK */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/ethtool.h>
#include <linux/completion.h>
#if defined(LINUX_HYBRID) || defined(WL_UMK)
#include <linux/pci_ids.h>
#define WLC_MAXBSSCFG		1	/* single BSS configs */
#else /* LINUX_HYBRID || WL_UMK */
#include <bcmdevs.h>
#endif /* LINUX_HYBRID || WL_UMK */

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>

#if !defined(LINUX_HYBRID) && !defined(WL_UMK)
#include <wlc_cfg.h>
#endif

#include <proto/802.1d.h>

#include <epivers.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <bcmutils.h>
#include <pcicfg.h>
#include <wlioctl.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 4, 5)
#error "No support for Kernel Rev <= 2.4.5, As the older kernel revs doesn't support Tasklets"
#endif

#ifdef WL_UMK
typedef struct wl_cdev wl_cdev_t;
#endif

#include <wlc_pub.h>
#include <wl_dbg.h>
#ifdef WL_MONITOR
#include <wlc_ethereal.h>
#include <proto/ieee80211_radiotap.h>
#endif


#ifdef BCMJTAG
#include <bcmjtag.h>
#endif

#ifdef DSLCPE
/* Todo: remove CONFIG_NET_RADIO in linux configuration */
#undef CONFIG_NET_RADIO
#include <wl_linux_dslcpe.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
#include <linux/wireless.h>
#endif
#ifdef DSLCPE_DELAY
bool wlc_is_dpc_deferred(void *wlc);
#endif /* DSLCPE_DELAY */
extern void wlc_shutdown_handler(wlc_info_t* wlc);
extern void wlc_reset_cnt(wlc_info_t* wlc);
#endif /* DSLCPE */

/* Linux wireless extension support */
#undef USE_IW
#if defined(CONFIG_WIRELESS_EXT) && defined(WLLXIW)
#define USE_IW
#endif
#ifdef USE_IW
#include <wl_iw.h>
struct iw_statistics *wl_get_wireless_stats(struct net_device *dev);
#endif /* USE_IW */


#include <wl_export.h>
#ifdef TOE
#include <wl_toe.h>
#endif

#ifdef ARPOE
#include <wl_arpoe.h>
#endif

#ifdef BCMDBUS
#include "dbus.h"
/* BMAC_NOTES: Remove, but just in case your Linux system has this defined */
#undef CONFIG_PCI
#endif


#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif /* HNDCTF */

#ifdef WLC_HIGH_ONLY
#include "bcm_rpc_tp.h"
#include "bcm_rpc.h"
#include "bcm_xdr.h"
#include "wlc_rpc.h"
#endif

#if defined(DSLCPE)
#include <board.h>      /* must be included before nbuff.h */
#if defined(DSLCPE_BLOG)
#include <linux/nbuff.h>  /* must be included before bcmutils.h */
#include <linux/blog.h>
#endif /* DSLCPE_BLOG */
#endif /* DSLCPE */

#include <wl_linux.h>
#ifdef WL_UMK
#include <linux/cdev.h>
#include <wl_chardev.h>
#include <wl_cdev.h>
#endif /* WL_UMK */
#ifdef BCMASSERT_LOG
#include <bcm_assert_log.h>
#endif

#ifndef WL_UMK
static void wl_timer(ulong data);
static void _wl_timer(wl_timer_t *t);
#endif

#ifdef WL_MONITOR
static int wl_monitor_start(struct sk_buff *skb, struct net_device *dev);
#endif

#ifdef WLC_HIGH_ONLY
static void wl_rpc_down(void *wlh);
static void wl_rpcq_free(wl_info_t *wl);
static void wl_rpcq_dispatch(struct wl_task *task);
static void wl_rpc_dispatch_schedule(void *ctx, struct rpc_buf* buf);
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY)
static void wl_rpc_txdone(void *ctx, struct rpc_buf* buf);
#endif
#define RPCQ_LOCK(_wl, _flags) spin_lock_irqsave(&(_wl)->rpcq_lock, (_flags))
#define RPCQ_UNLOCK(_wl, _flags)  spin_unlock_irqrestore(&(_wl)->rpcq_lock, (_flags))
#endif /* WLC_HIGH_ONLY */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
static void wl_start_txqwork(struct wl_task *task);
static void wl_txq_free(wl_info_t *wl);
#define TXQ_LOCK(_wl, _flags) spin_lock_irqsave(&(_wl)->txq_lock, (_flags))
#define TXQ_UNLOCK(_wl, _flags)  spin_unlock_irqrestore(&(_wl)->txq_lock, (_flags))

static void wl_set_multicast_list_workitem(struct work_struct *work);

#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */

#ifdef WLC_HIGH_ONLY
static void wl_timer_task(wl_task_t *task);
#else

#endif /* WLC_HIGH_ONLY */

#ifndef WL_UMK
static int wl_linux_watchdog(void *ctx);
static
#endif
int wl_found = 0;

#ifdef LINUX_CRYPTO
struct ieee80211_tkip_data {
#define TKIP_KEY_LEN 32
	u8 key[TKIP_KEY_LEN];
	int key_set;

	u32 tx_iv32;
	u16 tx_iv16;
	u16 tx_ttak[5];
	int tx_phase1_done;

	u32 rx_iv32;
	u16 rx_iv16;
	u16 rx_ttak[5];
	int rx_phase1_done;
	u32 rx_iv32_new;
	u16 rx_iv16_new;

	u32 dot11RSNAStatsTKIPReplays;
	u32 dot11RSNAStatsTKIPICVErrors;
	u32 dot11RSNAStatsTKIPLocalMICFailures;

	int key_idx;

	struct crypto_tfm *tfm_arc4;
	struct crypto_tfm *tfm_michael;

	/* scratch buffers for virt_to_page() (crypto API) */
	u8 rx_hdr[16], tx_hdr[16];
};
#endif /* LINUX_CRYPTO */

#define	WL_INFO(dev)		((wl_info_t*)(WL_DEV_IF(dev)->wl))	/* points to wl */


/* local prototypes */
static int wl_open(struct net_device *dev);
static int wl_close(struct net_device *dev);
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
static int wl_start(pNBuff_t pNBuff, struct net_device *dev);
#else
static int wl_start(struct sk_buff *skb, struct net_device *dev);
#endif /* DSLCPE_BLOG && DSLCPE */
static int wl_start_int(wl_info_t *wl, wl_if_t *wlif, struct sk_buff *skb);

static struct net_device_stats *wl_get_stats(struct net_device *dev);
static int wl_set_mac_address(struct net_device *dev, void *addr);
static void wl_set_multicast_list(struct net_device *dev);
static void _wl_set_multicast_list(struct net_device *dev);
static int wl_ethtool(wl_info_t *wl, void *uaddr, wl_if_t *wlif);
#ifndef WL_UMK
#ifdef NAPI_POLL
static int wl_poll(struct net_device *dev, int *budget);
#else /* NAPI_POLL */
#ifdef DSLCPE_NAPI 
static int wl_poll(struct napi_struct *napi, int budget);
static int wl_dpc(ulong data);
#else
static void wl_dpc(ulong data);
#endif /* DSLCPE_NAPI */
#endif /* NAPI_POLL */
#endif /* WL_UMK */
static void wl_link_up(wl_info_t *wl, char * ifname);
static void wl_link_down(wl_info_t *wl, char *ifname);
#if defined(BCMSUP_PSK) && defined(STA)
static void wl_mic_error(wl_info_t *wl, wlc_bsscfg_t *cfg,
	struct ether_addr *ea, bool group, bool flush_txq);
#endif
#if defined(AP) || defined(DSLCPE_DELAY) || defined(WLC_HIGH_ONLY) || \
	defined(WL_MONITOR) || defined(WL_CONFIG_RFKILL_INPUT)
static int wl_schedule_task(wl_info_t *wl, void (*fn)(struct wl_task *), void *context);
#endif
#if defined(CONFIG_PROC_FS)
static int wl_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data);
#endif /* defined(CONFIG_PROC_FS) */
#if defined(BCMDBG) && !defined(WL_UMK)
static int wl_dump(wl_info_t *wl, struct bcmstrbuf *b);
#endif /* BCMDBG && !WL_UMK */
static struct wl_if *wl_alloc_if(wl_info_t *wl, int iftype, uint unit, struct wlc_if* wlc_if);
static void wl_free_if(wl_info_t *wl, wl_if_t *wlif);
static void wl_get_driver_info(struct net_device *dev, struct ethtool_drvinfo *info);

#if defined(WL_CONFIG_RFKILL_INPUT)
#include <linux/rfkill.h>
static int wl_init_rfkill(wl_info_t *wl);
static void wl_uninit_rfkill(wl_info_t *wl);
static void wl_force_kill(wl_task_t *task);
#endif


#ifdef BCMJTAG
static void *wl_jtag_probe(uint16 venid, uint16 devid, void *regsva, void *param);
static void wl_jtag_detach(void *wl);
static void wl_jtag_poll(void *wl);
#endif


#if defined(WL_UMK)
MODULE_LICENSE("GPL");
#else /* !(BCMINTERNAL && WLC_HIGH_ONLY) && !WL_UMK */
MODULE_LICENSE("Proprietary");
#endif 

#if	defined(CONFIG_PCI) && !defined(BCMJTAG)
/* recognized PCI IDs */

#ifndef DSLCPE
static
#endif /* DSLCPE */
struct pci_device_id wl_id_table[] = {
#ifdef PCOEM_LINUXSTA
	{ PCI_VENDOR_ID_BROADCOM, 0x4311, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4311 2G */
	{ PCI_VENDOR_ID_BROADCOM, 0x4312, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4311 DUAL */
	{ PCI_VENDOR_ID_BROADCOM, 0x4313, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4311 5G */
	{ PCI_VENDOR_ID_BROADCOM, 0x4315, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4312 2G */
	{ PCI_VENDOR_ID_BROADCOM, 0x4328, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4321 DUAL */
	{ PCI_VENDOR_ID_BROADCOM, 0x4329, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4321 2G */
	{ PCI_VENDOR_ID_BROADCOM, 0x432a, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4321 5G */
	{ PCI_VENDOR_ID_BROADCOM, 0x432b, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4322 DUAL */
	{ PCI_VENDOR_ID_BROADCOM, 0x432c, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4322 2G */
	{ PCI_VENDOR_ID_BROADCOM, 0x432d, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 4322 5G */
	{ PCI_VENDOR_ID_BROADCOM, 0x4353, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 43224 DUAL */
	{ PCI_VENDOR_ID_BROADCOM, 0x4357, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, /* 43225 2G */
#else
	{ PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID, PCI_ANY_ID,
	PCI_CLASS_NETWORK_OTHER << 8, 0xffff00, 0 },
#endif /* PCOEM_LINUXSTA */
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, wl_id_table);
#endif	



#ifdef BCMDBG
#ifdef DSLCPE
int msglevel = 0xdeadbeef;
#else
static int msglevel = 0xdeadbeef;
#endif
module_param(msglevel, int, 0);
static int msglevel2 = 0xdeadbeef;
module_param(msglevel2, int, 0);
#endif /* BCMDBG */

static int oneonly = 0;
module_param(oneonly, int, 0);

static int piomode = 0;
module_param(piomode, int, 0);

static int instance_base = 0;	/* Starting instance number */
module_param(instance_base, int, 0);

#if defined(BCMDBG) && !defined(WL_UMK)
static struct ether_addr local_ea;
static char *macaddr = NULL;
module_param(macaddr, charp, S_IRUGO);
#endif

#if defined(BCMJTAG) || defined(BCMSLTGT)
static int nompc = 1;
#else
static int nompc = 0;
#endif
module_param(nompc, int, 0);

#ifdef DSLCPE
#if defined(CONFIG_BRCM_IKOS)
void wl_emu_selfinit(wl_info_t *wl);
#endif
static char name[IFNAMSIZ] = "wl%d";
#else
static char name[IFNAMSIZ] = "eth%d";
#endif /* DSLCPE */
module_param_string(name, name, IFNAMSIZ, 0);

/* BCMSLTGT: slow target */
#if defined(BCMJTAG) || defined(BCMSLTGT)
/* host and target have different clock speeds */
static uint htclkratio = 2000;
module_param(htclkratio, int, 0);
#endif	/* defined(BCMJTAG) || defined(BCMSLTGT) */

#ifdef WL_MONITOR
struct wl_radiotap_header {
	struct ieee80211_radiotap_header	ieee_radiotap;
	uint32       tsft_h;
	uint32       tsft_l;
	uint8        flags;
	uint8        rate;
	uint16       channel_freq;
	uint16       channel_flags;
	uint8        signal;
	uint8        noise;
	uint8        antenna;
} __attribute__((__packed__));

#define WL_RADIOTAP_PRESENT				\
	((1 << IEEE80211_RADIOTAP_TSFT) |		\
	 (1 << IEEE80211_RADIOTAP_RATE) |		\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |		\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |	\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |	\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |		\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

/* include/linux/if_arp.h
 *   #define ARPHRD_IEEE80211_PRISM 802    IEEE 802.11 + Prism2 header
 *   #define ARPHRD_IEEE80211_RADIOTAP 803  IEEE 802.11 + radiotap header
 * include/net/ieee80211_radiotap.h
 *  radiotap structure
 */

#ifndef ARPHRD_IEEE80211_RADIOTAP
#define ARPHRD_IEEE80211_RADIOTAP 803
#endif
#endif	 /* WL_MONITOR */

#ifndef	SRCBASE
#define	SRCBASE "."
#endif

#if defined(DSLCPE) && defined(DSLCPE_DELAY)
typedef struct fn_info {
	wl_info_t *wl;
	void (*fn)(void *);
	void *context;
} fn_info_t;

static void
_wl_schedule_fn(wl_task_t *task)
{
	fn_info_t *fi = (fn_info_t *) task->context;
	wl_info_t *wl = fi->wl;

	atomic_dec(&wl->callbacks);

	WL_LOCK(wl);
	fi->fn(fi->context);
	WL_UNLOCK(wl);
	MFREE(wl->osh, task, sizeof(wl_task_t));
	MFREE(wl->osh, fi, sizeof(fn_info_t));
	schedule();
}

void
wl_schedule_fn(wl_info_t *wl, void (*fn)(void *), void *context)
{
	fn_info_t *fi;

	if ((fi = MALLOC(wl->osh, sizeof(fn_info_t))) != NULL) {

		fi->wl = wl;
		fi->fn = fn;
		fi->context = context;

		wl_schedule_task(wl, _wl_schedule_fn, fi);
	}
}
#endif /* DSLCPE_DELAY */

#if WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
static struct ethtool_ops wl_ethtool_ops =
#else
static const struct ethtool_ops wl_ethtool_ops =
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) */
{
	.get_drvinfo = wl_get_driver_info
};
#endif /* WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */

#ifdef AEI_VDSL_CUSTOMER_NCS
static char * pMirrorIntfNames[] =
{
    "eth0",
    "eth1",
    "eth2",
    "eth3",
    "eth4", /* for HPNA */
    "wl0",
    "wl0.1",
    "wl0.2",
    "wl0.3",
    NULL
};

static void UpdateMirrorFlag(uint16 *mirFlags, char *intfName, uint8 enable)
{
    uint8 i;
    uint16 flag;
    
    for (i = 0; pMirrorIntfNames[i]; i++)
    {
        if (strcmp(pMirrorIntfNames[i], intfName) == 0)
        {
            flag = 0x0001 << i;

            if (enable)
                (*mirFlags) |= flag;
            else
                (*mirFlags) &= ~flag;

            break;
        }
    }
}

static void MirrorPacket( struct sk_buff *skb, char *intfName )
{   
    struct sk_buff *skbCopy;
    struct net_device *netDev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
    if( (netDev = __dev_get_by_name(&init_net, intfName)) != NULL )
#else
    if( (netDev = __dev_get_by_name(intfName)) != NULL )
#endif
    {
        if( (skbCopy = skb_copy(skb, GFP_ATOMIC)) != NULL )
        {
            unsigned long flags;
    
            skbCopy->dev = netDev;
            skbCopy->blog_p = BLOG_NULL;
            if (strstr(intfName, "wl"))
                skbCopy->protocol = htons(ETH_P_MIRROR_WLAN);
            else
                skbCopy->protocol = htons(ETH_P_MIRROR);
            local_irq_save(flags);
            local_irq_enable();
            dev_queue_xmit(skbCopy) ;
            local_irq_restore(flags);
        }
    }
} /* MirrorPacket */

static void MultiMirrorPacket( struct sk_buff *skb, uint16 mirFlags)
{   
    int i;
    uint32 flag;

    if (mirFlags == 0)
        return;

    for (i = 0; pMirrorIntfNames[i]; i++)
    {
        flag = 0x0001 << i;

        if (!(mirFlags & flag))
            continue;

        MirrorPacket( skb, pMirrorIntfNames[i] );
    }
}
#endif /* AEI_VDSL_CUSTOMER_NCS */

#if defined(WL_USE_NETDEV_OPS)
static const struct net_device_ops wl_netdev_ops =
{
	.ndo_open = wl_open,
	.ndo_stop = wl_close,
#ifdef DSLCPE_BLOG
	.ndo_start_xmit	= (HardStartXmitFuncP)wl_start,	
#else
	.ndo_start_xmit = wl_start,
#endif /* DSLCPE_NAPI */
	.ndo_get_stats = wl_get_stats,
	.ndo_set_mac_address = wl_set_mac_address,
	.ndo_set_multicast_list = wl_set_multicast_list,
	.ndo_do_ioctl = wl_ioctl
};
#ifdef WL_MONITOR
static const struct net_device_ops wl_netdev_monitor_ops =
{
	.ndo_start_xmit = wl_monitor_start,
	.ndo_get_stats = wl_get_stats,
	.ndo_do_ioctl = wl_ioctl
};
#endif /* WL_MONITOR */
#endif /* WL_USE_NETDEV_OPS */

static
void wl_if_setup(struct net_device *dev)
{
#if defined(WL_USE_NETDEV_OPS)
	dev->netdev_ops = &wl_netdev_ops;
#else
	dev->open = wl_open;
	dev->stop = wl_close;
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	dev->hard_start_xmit = (HardStartXmitFuncP)wl_start;
#else
	dev->hard_start_xmit = wl_start;
#endif /* DSLCPE_BLOG && DSLCPE */
	dev->get_stats = wl_get_stats;
	dev->set_mac_address = wl_set_mac_address;
	dev->set_multicast_list = wl_set_multicast_list;
	dev->do_ioctl = wl_ioctl;
#endif /* WL_USE_NETDEV_OPS */
#ifdef NAPI_POLL
	dev->poll = wl_poll;
	dev->weight = 64;
#endif /* NAPI_POLL */

#ifdef USE_IW
#if WIRELESS_EXT < 19
	dev->get_wireless_stats = wl_get_wireless_stats;
#endif /* WIRELESS_EXT < 19 */
#if WIRELESS_EXT > 12
	dev->wireless_handlers = (struct iw_handler_def *) &wl_iw_handler_def;
#endif /* WIRELESS_EXT > 12 */
#endif /* USE_IW */
#if WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	dev->ethtool_ops = &wl_ethtool_ops;
#endif /* WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */

#ifdef DSLCPE_NAPI
	{
	wl_if_t *wlif;
	wlif = (wl_if_t *)netdev_priv(dev);
	netif_napi_add(dev, &wlif->napi, wl_poll, RXBND);

	/* enable napi right away, don't wait for open */
	napi_enable(&((wl_if_t *)netdev_priv(dev))->napi);
	}
#endif /* DSLCPE_NAPI */
}

#ifdef HNDCTF
static void
wl_ctf_detach(ctf_t *ci, void *arg)
{
	wl_info_t *wl = (wl_info_t *)arg;

	wl->cih = NULL;

#ifdef CTFPOOL
	/* free the buffers in fast pool */
	osl_ctfpool_cleanup(wl->osh);
#endif /* CTFPOOL */

	return;
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static int
wl_dump_ctf(wl_info_t *wl, struct bcmstrbuf *b)
{
	ctf_dump(wl->cih, b);
	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */
#endif /* HNDCTF */

/**
 * attach to the WL device.
 *
 * Attach to the WL device identified by vendor and device parameters.
 * regs is a host accessible memory address pointing to WL device registers.
 *
 * wl_attach is not defined as static because in the case where no bus
 * is defined, wl_attach will never be called, and thus, gcc will issue
 * a warning that this function is defined but not used if we declare
 * it as static.
 */
static wl_info_t *
wl_attach(uint16 vendor, uint16 device, ulong regs, uint bustype, void *btparam, uint irq)
{
	struct net_device *dev;
	wl_if_t *wlif;
	wl_info_t *wl;
#if defined(CONFIG_PROC_FS)
	char tmp[128];
#endif
	osl_t *osh;
	int unit;
#ifdef HNDCTF
	char name[IFNAMSIZ];
#endif /* HNDCTF */
#ifdef WLC_HIGH_ONLY
	DEVICE_SPEED device_speed = INVALID_SPEED;
#endif


	unit = wl_found + instance_base;

	if (unit < 0) {
		WL_ERROR(("wl%d: unit number overflow, exiting\n", unit));
		return NULL;
	}

	if (oneonly && (unit != instance_base)) {
		WL_ERROR(("wl%d: wl_attach: oneonly is set, exiting\n", unit));
		return NULL;
	}

	/* Requires pkttag feature */
	osh = osl_attach(btparam, bustype, TRUE);
	ASSERT(osh);

	/* allocate private info */
	if ((wl = (wl_info_t*) MALLOC(osh, sizeof(wl_info_t))) == NULL) {
		WL_ERROR(("wl%d: malloc wl_info_t, out of memory, malloced %d bytes\n", unit,
			MALLOCED(osh)));
		osl_detach(osh);
		return NULL;
	}
	bzero(wl, sizeof(wl_info_t));

	wl->osh = osh;
	atomic_set(&wl->callbacks, 0);

#ifdef WLC_HIGH_ONLY
	wl->rpc_th = bcm_rpc_tp_attach(osh, NULL);
	if (wl->rpc_th == NULL) {
		WL_ERROR(("wl%d: %s: bcm_rpc_tp_attach failed! \n", unit, __FUNCTION__));
		goto fail;
	}

	wl->rpc = bcm_rpc_attach(NULL, osh, wl->rpc_th, &device);
	if (wl->rpc == NULL) {
		WL_ERROR(("wl%d: %s: bcm_rpc_attach failed! \n", unit, __FUNCTION__));
		goto fail;
	}
#endif /* WLC_HIGH_ONLY */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
	/* init tx work queue for wl_start/send pkt; no need to destroy workitem  */
	MY_INIT_WORK(&wl->txq_task.work, (work_func_t)wl_start_txqwork);
	wl->txq_task.context = wl;
#if defined(DSLCPE_TX_DELAY)
	wl->txq_head = wl->txq_tail = NULL;
	wl->enable_tx_delay=FALSE;
#ifdef DSLCPE_TXRX_BOUND	
	wl->tx_bound = 10;
#endif /* DSLCPE_TXRX_BOUND */
#endif /* DSLCPE_TX_DELAY */

	/* init work queue for wl_set_multicast_list(); no need to destroy workitem  */
	MY_INIT_WORK(&wl->multicast_task.work, (work_func_t)wl_set_multicast_list_workitem);
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */

	wlif = wl_alloc_if(wl, WL_IFTYPE_BSS, unit, NULL);
	if (!wlif) {
		WL_ERROR(("wl%d: wl_alloc_if failed\n", unit));
		MFREE(osh, wl, sizeof(wl_info_t));
		osl_detach(osh);
		return NULL;
	}

	dev = wlif->dev;
	wl->dev = dev;
	wl_if_setup(dev);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	wlif = netdev_priv(dev);
#endif

	/* map chip registers (47xx: and sprom) */
	dev->base_addr = regs;

	WL_TRACE(("wl%d: Bus: ", unit));
	if (bustype == PCMCIA_BUS) {
		/* Disregard command overwrite */
		wl->piomode = TRUE;
		WL_TRACE(("PCMCIA\n"));
	} else if (bustype == PCI_BUS) {
		/* piomode can be overwritten by command argument */
		wl->piomode = piomode;
		WL_TRACE(("PCI/%s\n", wl->piomode ? "PIO" : "DMA"));
	}
#ifdef BCMJTAG
	else if (bustype == JTAG_BUS) {
		/* Disregard command option overwrite */
		wl->piomode = TRUE;
		WL_TRACE(("JTAG\n"));
	}
#endif	/* BCMJTAG */
	else if (bustype == RPC_BUS) {
		/* Do nothing */
	} else {
		bustype = PCI_BUS;
		WL_TRACE(("force to PCI\n"));
	}
	wl->bcm_bustype = bustype;

#ifdef BCMJTAG
	if (wl->bcm_bustype == JTAG_BUS)
		wl->regsva = (void *)dev->base_addr;
	else
#endif
#ifdef WLC_HIGH_ONLY
	if (wl->bcm_bustype == RPC_BUS) {
		wl->regsva = (void *)0;
		btparam = wl->rpc;
	} else
#endif
#ifndef WL_UMK
	if ((wl->regsva = ioremap_nocache(dev->base_addr, PCI_BAR0_WINSZ)) == NULL) {
		WL_ERROR(("wl%d: ioremap() failed\n", unit));
		goto fail;
	}
#else
	wl->regsva = (void *)regs;
#endif /* WL_UMK */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
#ifdef WLC_HIGH_ONLY
	spin_lock_init(&wl->rpcq_lock);
#endif /* WLC_HIGH_ONLY */
	spin_lock_init(&wl->txq_lock);

#ifdef WLC_HIGH_ONLY
	init_MUTEX(&wl->sem);
#endif /* WLC_HIGH_ONLY */
#else
	spin_lock_init(&wl->lock);
	spin_lock_init(&wl->isr_lock);
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */

#if defined(DSLCPE_DELAY)
	wl->oshsh.lock = &wl->lock;
	wl->oshsh.wl = wl;
	osl_oshsh_init(wl->osh, &wl->oshsh);
#endif /* DSLCPE && DSLCPE_DELAY */

#ifdef BCMASSERT_LOG
	bcm_assertlog_init();
#endif
	/* common load-time initialization */
#ifndef WL_UMK
	{
	int err;
	if (!(wl->wlc = wlc_attach((void *) wl, vendor, device, unit, wl->piomode,
		osh, wl->regsva, wl->bcm_bustype, btparam, &err))) {
		printf("%s: %s driver failed with code %d\n", dev->name, EPI_VERSION_STR, err);
		goto fail;
	}
	}
	wl->pub = wlc_pub(wl->wlc);
#else
	/* allocate pub info */
	if ((wl->pub = (wlc_pub_t*) MALLOC(osh, sizeof(wlc_pub_t))) == NULL) {
		WL_ERROR(("wl%d: malloc wlc_pub_t, out of memory, malloced %d bytes\n", unit,
			MALLOCED(osh)));
		goto fail;
	}
	bzero(wl->pub, sizeof(wlc_pub_t));
	wl->pub->unit = unit;
	wl->wlc = (wlc_info_t *)wl->pub;
	/* Always default WME enable */
	wl->pub->_wme = ON;
#endif /* WL_UMK */

#ifdef WLC_HIGH_ONLY
	REGOPSSET(osh, (osl_rreg_fn_t)wlc_reg_read, (osl_wreg_fn_t)wlc_reg_write, wl->wlc);
	device_speed = bcm_rpc_tp_get_device_speed(wl->rpc_th);
	if ((device_speed != FULL_SPEED) && (device_speed != LOW_SPEED)) {
		WL_TRACE(("%s: HIGH SPEED USB DEVICE\n", __FUNCTION__));
#ifdef BMAC_ENABLE_LINUX_HOST_RPCAGG
		wlc_iovar_setint(wl->wlc, "rpc_agg", BCM_RPC_TP_DNGL_AGG_DPC);
		if ((device == BCM43236_CHIP_ID) ||
		    (device == BCM43235_CHIP_ID) ||
		    (device == BCM43238_CHIP_ID)) {
			/* 43236 allow large host->dongle aggregation */
			wlc_iovar_setint(wl->wlc, "rpc_agg",
				BCM_RPC_TP_HOST_AGG_AMPDU | BCM_RPC_TP_DNGL_AGG_DPC);
		}
#endif /* BMAC_ENABLE_LINUX_HOST_RPCAGG */
	} else {
		WL_ERROR(("%s: LOW SPEED USB DEVICE\n", __FUNCTION__));
		/* for slow bus like 12Mbps, it hurts to run ampdu */
		WL_TRACE(("%s: LEGACY USB DEVICE\n", __FUNCTION__));
		wlc_iovar_setint(wl->wlc, "ampdu_manual_mode", 1);
		wlc_iovar_setint(wl->wlc, "ampdu_rx_factor", AMPDU_RX_FACTOR_8K);
		wlc_iovar_setint(wl->wlc, "ampdu_ba_wsize", 8);
	}

	wl->rpc_dispatch_ctx.rpc = wl->rpc;
	wl->rpc_dispatch_ctx.wlc = wl->wlc;
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY)
	bcm_rpc_rxcb_init(wl->rpc, wl, wl_rpc_dispatch_schedule, wl, wl_rpc_down,
	                  NULL, wl_rpc_txdone);
#else
	bcm_rpc_rxcb_init(wl->rpc, wl, wl_rpc_dispatch_schedule, wl, wl_rpc_down,
	                  NULL, NULL);
#endif
#endif /* WLC_HIGH_ONLY */

#ifndef WL_UMK
	if (nompc) {
		if (wlc_iovar_setint(wl->wlc, "mpc", 0)) {
			WL_ERROR(("wl%d: Error setting MPC variable to 0\n", unit));
		}
	}
#endif /* WL_UMK */

#if defined(CONFIG_PROC_FS)
	/* create /proc/net/wl<unit> */
	sprintf(tmp, "net/wl%d", wl->pub->unit);
	create_proc_read_entry(tmp, 0, 0, wl_read_proc, (void*)wl);
#endif /* defined(CONFIG_PROC_FS) */

#ifndef WL_UMK
#ifdef BCMDBG
	if (macaddr != NULL) {  /* user command line override */
		int err;

		WL_ERROR(("wl%d: setting MAC ADDRESS %s\n", unit, macaddr));
		bcm_ether_atoe(macaddr, &local_ea);

		err = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, &local_ea,
			ETHER_ADDR_LEN, IOV_SET, NULL);
		if (err)
			WL_ERROR(("wl%d: Error setting MAC ADDRESS\n", unit));
	}
#endif	/* BCMDBG */

	bcopy(&wl->pub->cur_etheraddr, dev->dev_addr, ETHER_ADDR_LEN);

#ifndef NAPI_POLL
	/* setup the bottom half handler */
	tasklet_init(&wl->tasklet, wl_dpc, (ulong)wl);
#endif /* NAPI_POLL */
#endif /* WL_UMK */

#ifdef TOE
	/* allocate the toe info struct */
	if ((wl->toei = wl_toe_attach(wl->wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_toe_attach failed\n", unit));
		goto fail;
	}
#endif

#ifdef ARPOE
	/* allocate the arp info struct */
	if ((wl->arpi = wl_arp_attach(wl->wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_arp_attach failed\n", unit));
		goto fail;
	}
#endif

#ifdef HNDCTF
	sprintf(name, "wl%d", unit);
	wl->cih = ctf_attach(osh, name, &wl_msg_level, wl_ctf_detach, wl);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
	wlc_dump_register(wl->pub, "ctf", (dump_fn_t)wl_dump_ctf, (void *)wl);
#endif /* BCMDBG || BCMDBG_DUMP */
#endif /* HNDCTF */

#ifdef CTFPOOL
	/* create packet pool with specified number of buffers */ 
#ifdef WLC_HIGH_ONLY
	if (CTF_ENAB(wl->cih) && (num_physpages >= 8192) &&
	    (osl_ctfpool_init(osh, CTFPOOLSZ, DBUS_RX_BUFFER_SIZE_RPC+BCMEXTRAHDROOM) < 0)) {
#else
	if (CTF_ENAB(wl->cih) && (num_physpages >= 8192) &&
	    (osl_ctfpool_init(osh, CTFPOOLSZ, RXBUFSZ+BCMEXTRAHDROOM) < 0)) {
#endif /* WLC_HIGH_ONLY */
		WL_ERROR(("wl%d: wlc_attach: osl_ctfpool_init failed\n", unit));
		goto fail;
	}
#endif /* CTFPOOL */

#ifdef WLC_LOW
	/* register our interrupt handler */
#ifdef BCMJTAG
	if (wl->bcm_bustype != JTAG_BUS)
#endif	/* BCMJTAG */
	{
#ifndef WL_UMK
		if (request_irq(irq, wl_isr, IRQF_SHARED, dev->name, wl)) {
			WL_ERROR(("wl%d: request_irq() failed\n", unit));
			goto fail;
		}
		dev->irq = irq;
#endif /* !WL_UMK */
	}
#ifdef WL_UMK
	wlif->pci_dev = (struct pci_dev *)btparam;
	return wl;
#endif
#endif /* WLC_LOW */

	if (wl->bcm_bustype == PCI_BUS) {
		struct pci_dev *pci_dev = (struct pci_dev *)btparam;
		if (pci_dev != NULL)
			SET_NETDEV_DEV(dev, &pci_dev->dev);
	}

	if (register_netdev(dev)) {
		WL_ERROR(("wl%d: register_netdev() failed\n", unit));
		goto fail;
	}
	wlif->dev_registed = TRUE;

#ifdef HNDCTF
	if ((ctf_dev_register(wl->cih, dev, FALSE) != BCME_OK) ||
	    (ctf_enable(wl->cih, dev, TRUE) != BCME_OK)) {
		WL_ERROR(("wl%d: ctf_dev_register() failed\n", unit));
		goto fail;
	}
#endif /* HNDCTF */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#ifdef LINUX_CRYPTO
	/* load the tkip module */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	wl->tkipmodops = lib80211_get_crypto_ops("TKIP");
	if (wl->tkipmodops == NULL) {
		request_module("lib80211_crypt_tkip");
		wl->tkipmodops = lib80211_get_crypto_ops("TKIP");
	}
#else
	wl->tkipmodops = ieee80211_get_crypto_ops("TKIP");
	if (wl->tkipmodops == NULL) {
		request_module("ieee80211_crypt_tkip");
		wl->tkipmodops = ieee80211_get_crypto_ops("TKIP");
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
#endif /* LINUX_CRYPTO */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
#ifdef USE_IW
	wlif->iw.wlinfo = (void *)wl;
#endif

#if defined(WL_CONFIG_RFKILL_INPUT)
	if (wl_init_rfkill(wl) < 0)
		WL_ERROR(("%s: init_rfkill_failure\n", __FUNCTION__));
#endif

#if defined(PCOEM_LINUXSTA) && !defined(WL_UMK)
	/* set led default duty-cycle */
	if (wlc_iovar_setint(wl->wlc, "leddc", 0xa0000)) {
		WL_ERROR(("wl%d: Error setting led duty-cycle\n", unit));
	}
	if (wlc_set(wl->wlc, WLC_SET_PM, PM_FAST)) {
		WL_ERROR(("wl%d: Error setting PM variable to FAST PS\n", unit));
	}
	/* turn off vlan by default */
	if (wlc_iovar_setint(wl->wlc, "vlan_mode", OFF)) {
		WL_ERROR(("wl%d: Error setting vlan mode OFF\n", unit));
	}
	/* default infra_mode is Infrastructure */
	if (wlc_set(wl->wlc, WLC_SET_INFRA, 1)) {
		WL_ERROR(("wl%d: Error setting infra_mode to infrastructure\n", unit));
	}
#endif /* defined(PCOEM_LINUXSTA) && !defined(WL_UMK) */

#ifndef WL_UMK
	/* register module */
	wlc_module_register(wl->pub, NULL, "linux", wl, NULL, wl_linux_watchdog, NULL);

#ifdef BCMDBG
	wlc_dump_register(wl->pub, "wl", (dump_fn_t)wl_dump, (void *)wl);
#endif
#endif /* WL_UMK */
	/* print hello string */
	printf("%s: Broadcom BCM%04x 802.11 Wireless Controller " EPI_VERSION_STR,
		dev->name, device);

#ifdef BCMDBG
	printf(" (Compiled in " SRCBASE " at " __TIME__ " on " __DATE__ ")");
#endif /* BCMDBG */
	printf("\n");

	wl_found++;

#if defined(DSLCPE) && defined(CONFIG_BRCM_IKOS)
	wl_emu_selfinit(wl);
#endif /* defined(DSLCPE) && defined(CONFIG_BRCM_IKOS) */
			
	return wl;

fail:
	wl_free(wl);
	return NULL;
}

#ifdef BCMDBUS
static void *
wl_dbus_probe_cb(void *arg, const char *desc, uint32 bustype, uint32 hdrlen)
{
	wl_info_t *wl;
	WL_ERROR(("%s: \n", __FUNCTION__));

	if (!(wl = wl_attach(BCM_DNGL_VID, BCM_DNGL_BDC_PID, (ulong)NULL, RPC_BUS, NULL, 0))) {
		WL_ERROR(("%s: wl_attach failed\n", __FUNCTION__));
	}

	/* This is later passed to wl_dbus_disconnect_cb */
	return wl;
}

static void
wl_dbus_disconnect_cb(void *arg)
{
	wl_info_t *wl = arg;

	WL_ERROR(("%s: \n", __FUNCTION__));

	if (wl) {
#ifdef WLC_HIGH_ONLY
		wlc_device_removed(wl->wlc);
		wlc_bmac_dngl_reboot(wl->rpc);
		bcm_rpc_down(wl->rpc);
#endif
		WL_LOCK(wl);
		wl_down(wl);
		WL_UNLOCK(wl);
		wl_free(wl);
	}
}
#endif /* BCMDBUS */

#if defined(CONFIG_PROC_FS)
static int
wl_read_proc(char *buffer, char **start, off_t offset, int length, int *eof, void *data)
{
	wl_info_t *wl;
	int len;
	off_t pos;
	off_t begin;

	len = pos = begin = 0;

	wl = (wl_info_t*) data;

	WL_LOCK(wl);
	/* pass space delimited variables for dumping */
#if defined(BCMDBG) && !defined(WL_UMK)
	wlc_iovar_dump(wl->wlc, "all", strlen("all") + 1, buffer, PAGE_SIZE);
	len = strlen(buffer);
#endif /* BCMDBG && !WL_UMK */
	WL_UNLOCK(wl);
	pos = begin + len;

	if (pos < offset) {
		len = 0;
		begin = pos;
	}

	*eof = 1;

	*start = buffer + (offset - begin);
	len -= (offset - begin);

	if (len > length)
		len = length;

	return (len);
}
#endif /* defined(CONFIG_PROC_FS) */

/* For now, JTAG, SDIO, and PCI are mutually exclusive.  When this changes, remove
 * #if !defined(BCMJTAG) && !defined(BCMSDIO) ... #endif conditionals.
 */
#if !defined(BCMJTAG)
#ifdef CONFIG_PCI
#ifdef DSLCPE
void __devexit wl_remove(struct pci_dev *pdev);
#else
static void __devexit wl_remove(struct pci_dev *pdev);
#endif /* DSLCPE */


/**
 * determines if a device is a WL device, and if so, attaches it.
 *
 * This function determines if a device pointed to by pdev is a WL device,
 * and if so, performs a wl_attach() on it.
 *
 */
int __devinit
wl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int rc;
	wl_info_t *wl;
#ifdef LINUXSTA_PS
	uint32 val;
#endif

#if defined(DSLCPE)&&defined(CONFIG_BRCM_IKOS)
        wl_msg_level=wl_msg_level2=0xffffffff;
#endif

	WL_TRACE(("%s: bus %d slot %d func %d irq %d\n", __FUNCTION__,
	          pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn), pdev->irq));

	if ((pdev->vendor != PCI_VENDOR_ID_BROADCOM) ||
	    (((pdev->device & 0xff00) != 0x4300) &&
	     ((pdev->device & 0xff00) != 0x4700) &&
	     ((pdev->device < 43000) || (pdev->device > 43999)))) {
		WL_TRACE(("%s: unsupported vendor %x device %x\n", __FUNCTION__,
		          pdev->vendor, pdev->device));
		return (-ENODEV);
	}

	rc = pci_enable_device(pdev);
	if (rc) {
		WL_ERROR(("%s: Cannot enable device %d-%d_%d\n", __FUNCTION__,
		          pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn)));
		return (-ENODEV);
	}
	pci_set_master(pdev);

#ifdef LINUXSTA_PS
	/*
	 * Disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state.
	 * Code taken from ipw2100 driver
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);
#endif /* LINUXSTA_PS */

	wl = wl_attach(pdev->vendor, pdev->device, pci_resource_start(pdev, 0), PCI_BUS, pdev,
		pdev->irq);
	if (!wl)
		return -ENODEV;

	pci_set_drvdata(pdev, wl);

#ifdef WL_UMK
	/* Initialize char driver that interfaces to user space */
	if (wl_cdev_init(wl) < 0) {
		WL_ERROR(("wl%d: %s: wl_dev_init failed\n", wl->pub->unit, __FUNCTION__));
		wl_free(wl);
		return -ENODEV;
	}
#endif
	return 0;
}

#ifdef LINUXSTA_PS
static int
wl_suspend(struct pci_dev *pdev, DRV_SUSPEND_STATE_TYPE state)
{
	wl_info_t *wl = (wl_info_t *) pci_get_drvdata(pdev);

	WL_TRACE(("wl: wl_suspend\n"));

	wl = (wl_info_t *) pci_get_drvdata(pdev);
	if (!wl) {
		WL_ERROR(("wl: wl_suspend: pci_get_drvdata failed\n"));
		return -ENODEV;
	}

#ifdef WL_UMK
	if ((_wl_ioctl(wl, WLC_DOWN, NULL, 0)) < 0)
		WL_ERROR(("wl%d: %s: WLC_DOWN Failed\n", wl->pub->unit, __FUNCTION__));
#endif
	WL_LOCK(wl);
	WL_APSTA_UPDN(("wl%d (%s): wl_suspend() -> wl_down()\n", wl->pub->unit, wl->dev->name));
	wl_down(wl);
	wl->pub->hw_up = FALSE;
	WL_UNLOCK(wl);
	PCI_SAVE_STATE(pdev, wl->pci_psstate);
	pci_disable_device(pdev);
	return pci_set_power_state(pdev, PCI_D3hot);
}

static int
wl_resume(struct pci_dev *pdev)
{
	wl_info_t *wl = (wl_info_t *) pci_get_drvdata(pdev);
	int err = 0;
	uint32 val;

	WL_TRACE(("wl: wl_resume\n"));

	if (!wl) {
		WL_ERROR(("wl: wl_resume: pci_get_drvdata failed\n"));
	        return -ENODEV;
	}

	err = pci_set_power_state(pdev, PCI_D0);
	if (err)
		return err;

	PCI_RESTORE_STATE(pdev, wl->pci_psstate);

	err = pci_enable_device(pdev);
	if (err)
		return err;

	pci_set_master(pdev);
	/*
	 * Suspend/Resume resets the PCI configuration space, so we have to
	 * re-disable the RETRY_TIMEOUT register (0x41) to keep
	 * PCI Tx retries from interfering with C3 CPU state
	 * Code taken from ipw2100 driver
	 */
	pci_read_config_dword(pdev, 0x40, &val);
	if ((val & 0x0000ff00) != 0)
		pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);

#ifdef WL_UMK
	if ((err = _wl_ioctl(wl, WLC_UP, NULL, 0)) < 0)
		WL_ERROR(("wl%d: %s: WLC_UP Failed\n", wl->pub->unit, __FUNCTION__));
#endif
	WL_LOCK(wl);
	WL_APSTA_UPDN(("wl%d: (%s): wl_resume() -> wl_up()\n", wl->pub->unit, wl->dev->name));
	err = wl_up(wl);
	WL_UNLOCK(wl);

	return (err);
}
#endif /* LINUXSTA_PS */

#ifdef DSLCPE
void __devexit
#else
static void __devexit
#endif /* DSLCPE */
wl_remove(struct pci_dev *pdev)
{
	wl_info_t *wl = (wl_info_t *) pci_get_drvdata(pdev);

	if (!wl) {
		WL_ERROR(("wl: wl_remove: pci_get_drvdata failed\n"));
		return;
	}
#ifdef WL_UMK
	WL_ERROR(("wl_remove: XXX: Bypassing chipmatch\n"));
#else
	if (!wlc_chipmatch(pdev->vendor, pdev->device)) {
		WL_ERROR(("wl: wl_remove: wlc_chipmatch failed\n"));
		return;
	}
#endif /* WL_UMK */

	WL_LOCK(wl);
	WL_APSTA_UPDN(("wl%d (%s): wl_remove() -> wl_down()\n", wl->pub->unit, wl->dev->name));
	wl_down(wl);
	WL_UNLOCK(wl);
#ifdef WL_UMK
	wl_cdev_cleanup(wl);
#endif
#ifdef DSLCPE_DGASP
	kerSysDeregisterDyingGaspHandler(wl_netdev_get(wl)->name);
#endif /* DSLCPE_DGASP */

	wl_free(wl);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}

#ifndef DSLCPE
static struct pci_driver wl_pci_driver = {
	name:		"wl",
	probe:		wl_pci_probe,
#ifdef LINUXSTA_PS
	suspend:	wl_suspend,
	resume:		wl_resume,
#endif /* LINUXSTA_PS */
	remove:		__devexit_p(wl_remove),
	id_table:	wl_id_table,
	};
#endif /* DSLCPE */
#endif	/* CONFIG_PCI */
#endif  /* !defined(BCMJTAG) */

#ifdef BCMJTAG
static bcmjtag_driver_t wl_jtag_driver = {
		wl_jtag_probe,
		wl_jtag_detach,
		wl_jtag_poll,
		};
#endif	/* BCMJTAG */


#if defined(WL_UMK) && defined(USERMODE_INTS)
static void
send_signal(wl_info_t *wl)
{
	if (wl->pid) {
		WL_INFORM(("Sending signal %d to pid %d\n", WL_USER_SIG + 2, wl->pid));
		kill_proc(wl->pid, WL_USER_SIG + 2, 1);
	}
}
#endif /* WL_UMK && USERMODE_INTS */

#ifndef DSLCPE
/**
 * This is the main entry point for the WL driver.
 *
 * This function determines if a device pointed to by pdev is a WL device,
 * and if so, performs a wl_attach() on it.
 *
 */
static int __init
wl_module_init(void)
{
	int error = -ENODEV;

#ifdef BCMDBG
	if (msglevel != 0xdeadbeef)
		wl_msg_level = msglevel;
	else {
		char *var = getvar(NULL, "wl_msglevel");
		if (var)
			wl_msg_level = bcm_strtoul(var, NULL, 0);
	}
	printf("%s: msglevel set to 0x%x\n", __FUNCTION__, wl_msg_level);
	if (msglevel2 != 0xdeadbeef)
		wl_msg_level2 = msglevel2;
	else {
		char *var = getvar(NULL, "wl_msglevel2");
		if (var)
			wl_msg_level2 = bcm_strtoul(var, NULL, 0);
	}
	printf("%s: msglevel2 set to 0x%x\n", __FUNCTION__, wl_msg_level2);
#endif /* BCMDBG */

#ifdef BCMJTAG
	if (!(error = bcmjtag_register(&wl_jtag_driver)))
		return (0);
#endif	/* BCMJTAG */


#if !defined(BCMJTAG)
#ifdef CONFIG_PCI
	if (!(error = pci_module_init(&wl_pci_driver)))
		return (0);

#endif	/* CONFIG_PCI */
#endif  

#ifdef BCMDBUS
	/* BMAC_NOTE: define hardcode number, why NODEVICE is ok ? */
	error = dbus_register(BCM_DNGL_VID, 0, wl_dbus_probe_cb, wl_dbus_disconnect_cb,
		NULL, NULL, NULL);
	if (error == DBUS_ERR_NODEVICE) {
		error = DBUS_OK;
	}
#endif /* BCMDBUS */

	return (error);
}

/**
 * This function unloads the WL driver from the system.
 *
 * This function unconditionally unloads the WL driver module from the
 * system.
 *
 */
static void __exit
wl_module_exit(void)
{
#ifdef BCMJTAG
	bcmjtag_unregister();
#endif	/* BCMJTAG */


#if !defined(BCMJTAG)
#ifdef CONFIG_PCI
	pci_unregister_driver(&wl_pci_driver);

#endif	/* CONFIG_PCI */
#endif  

#ifdef BCMDBUS
	dbus_deregister();
#endif /* BCMDBUS */
}

module_init(wl_module_init);
module_exit(wl_module_exit);
#endif /* #ifndef DSLCPE */

#ifdef DSLCPE
/* Wait for all callbacks complete */
void wl_wait_timer_done(void *wl)
{	while (atomic_read(&((wl_info_t *)wl)->callbacks) > 0)
		yield();
}
#endif /* DSLCPE */

/**
 * This function frees the WL per-device resources.
 *
 * This function frees resources owned by the WL device pointed to
 * by the wl parameter.
 *
 */
void
wl_free(wl_info_t *wl)
{
	wl_timer_t *t, *next;
	osl_t *osh;

	WL_TRACE(("wl: wl_free\n"));
#ifdef BCMJTAG
	if (wl->bcm_bustype != JTAG_BUS)
#endif	/* BCMJTAG */
	{
		if (wl->dev && wl->dev->irq)
			free_irq(wl->dev->irq, wl);
	}

#if defined(WL_CONFIG_RFKILL_INPUT)
	wl_uninit_rfkill(wl);
#endif

#ifdef DSLCPE_NAPI
	if (NAPI_ENAB(wl->pub))
	{
		clear_bit(__LINK_STATE_START, &wl->dev->state);
		napi_disable(&((wl_if_t *)netdev_priv(wl->dev))->napi);
	}
#endif /* DSLCPE_NAPI */

	if (wl->dev) {
		wl_free_if(wl, WL_DEV_IF(wl->dev));
		wl->dev = NULL;
	}

#ifdef TOE
	wl_toe_detach(wl->toei);
#endif

#ifdef ARPOE
	wl_arp_detach(wl->arpi);
#endif

#ifndef WL_UMK
#ifndef NAPI_POLL
	/* kill dpc */
	tasklet_kill(&wl->tasklet);
#endif /* NAPI_POLL */

	if (wl->pub) {
		wlc_module_unregister(wl->pub, "linux", wl);
	}
#endif /* WL_UMK */

	/* free common resources */
	if (wl->wlc) {
#if defined(CONFIG_PROC_FS)
		char tmp[128];
		/* remove /proc/net/wl<unit> */
		sprintf(tmp, "net/wl%d", wl->pub->unit);
		remove_proc_entry(tmp, 0);
#endif /* defined(CONFIG_PROC_FS) */
#ifdef WL_UMK
		MFREE(wl->osh, wl->pub, sizeof(wlc_pub_t));
#else
		wlc_detach(wl->wlc);
#endif
		wl->wlc = NULL;
		wl->pub = NULL;
	}

	/* virtual interface deletion is deferred so we cannot spinwait */

#ifndef DSLCPE
	/* DSLCPE: Moved to wlc_detach to avoid rmmod crash */
	/* wait for all pending callbacks to complete */
	while (atomic_read(&wl->callbacks) > 0)
		schedule();
#endif /* DSLCPE */
	/* free timers */
	for (t = wl->timers; t; t = next) {
		next = t->next;
#ifdef BCMDBG
		if (t->name)
			MFREE(wl->osh, t->name, strlen(t->name) + 1);
#endif
		MFREE(wl->osh, t, sizeof(wl_timer_t));
	}

	/* free monitor */
	if (wl->monitor_dev) {
		wl_free_if(wl, WL_DEV_IF(wl->monitor_dev));
		wl->monitor_dev = NULL;
	}

	osh = wl->osh;
#ifndef WL_UMK
	/*
	 * unregister_netdev() calls get_stats() which may read chip registers
	 * so we cannot unmap the chip registers until after calling unregister_netdev() .
	 */
	if (wl->regsva && BUSTYPE(wl->bcm_bustype) != SDIO_BUS &&
	    BUSTYPE(wl->bcm_bustype) != JTAG_BUS) {
		iounmap((void*)wl->regsva);
	}
#endif /* !WL_UMK */
	wl->regsva = NULL;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#ifdef LINUX_CRYPTO
	/* un register the TKIP module...if any */
	if (wl->tkipmodops != NULL) {
		if (wl->tkip_ucast_data) {
			wl->tkipmodops->deinit(wl->tkip_ucast_data);
			wl->tkip_ucast_data = NULL;
		}
		if (wl->tkip_bcast_data) {
			wl->tkipmodops->deinit(wl->tkip_bcast_data);
			wl->tkip_bcast_data = NULL;
		}
	}
#endif /* LINUX_CRYPTO */
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */

#ifdef WLC_HIGH_ONLY

	wl_rpcq_free(wl);
#endif /* WLC_HIGH_ONLY */
#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
	wl_txq_free(wl);
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */

#ifdef WLC_HIGH_ONLY
	if (wl->rpc) {
		bcm_rpc_detach(wl->rpc);
		wl->rpc = NULL;
	}

	if (wl->rpc_th) {
		bcm_rpc_tp_detach(wl->rpc_th);
		wl->rpc_th = NULL;
	}
#endif /* WLC_HIGH_ONLY */

#ifdef BCMASSERT_LOG
	bcm_assertlog_deinit();
#endif

#ifdef CTFPOOL
	/* free the buffers in fast pool */
	osl_ctfpool_cleanup(wl->osh);
#endif /* CTFPOOL */

#ifdef HNDCTF
	/* free ctf resources */
	if (wl->cih)
		ctf_detach(wl->cih);
#endif /* HNDCTF */

	MFREE(osh, wl, sizeof(wl_info_t));

	if (MALLOCED(osh)) {
		printf("Memory leak of bytes %d\n", MALLOCED(osh));
#ifdef BCMDBG_MEM
		MALLOC_DUMP(osh, NULL);
#else
		ASSERT(0);
#endif
	}

	osl_detach(osh);
}

static int
wl_open(struct net_device *dev)
{
	wl_info_t *wl;
	int error = 0;

	if (!dev)
		return -ENETDOWN;

	wl = WL_INFO(dev);

	WL_TRACE(("wl%d: wl_open\n", wl->pub->unit));

	WL_LOCK(wl);
	WL_APSTA_UPDN(("wl%d: (%s): wl_open() -> wl_up()\n",
	               wl->pub->unit, wl->dev->name));
	/* Since this is resume, reset hw to known state */
	error = wl_up(wl);
#ifndef WL_UMK
	if (!error) {
		error = wlc_set(wl->wlc, WLC_SET_PROMISC, (dev->flags & IFF_PROMISC));
	}
#ifdef DSLCPE
#ifdef DSLCPE_IDLE_PWRSAVE
	wlc_idle_pwrsave_start_timer(wl->wlc);
#endif	
	error = wl_dslcpe_open(dev);
#endif /* DSLCPE */
#endif /* WL_UMK */
	WL_UNLOCK(wl);

#ifdef WL_UMK
	/* dmamem_open() */
	cap_raise(current->cap_effective, CAP_SYS_RAWIO);
#endif /* WL_UMK */
	if (!error)
		OLD_MOD_INC_USE_COUNT;

	return (error? -ENODEV: 0);
}

static int
wl_close(struct net_device *dev)
{
	wl_info_t *wl;

	if (!dev)
		return -ENETDOWN;

	wl = WL_INFO(dev);

	WL_TRACE(("wl%d: wl_close\n", wl->pub->unit));

	WL_LOCK(wl);
	WL_APSTA_UPDN(("wl%d (%s): wl_close() -> wl_down()\n",
		wl->pub->unit, wl->dev->name));
	wl_down(wl);
#ifdef DSLCPE
#ifdef DSLCPE_IDLE_PWRSAVE
	wlc_idle_pwrsave_stop_timer(wl->wlc);
#endif
	wl_dslcpe_close(dev);
#endif /* DSLCPE */
	WL_UNLOCK(wl);

	OLD_MOD_DEC_USE_COUNT;

	return (0);
}

#if defined(WLC_LOW) && !defined(DSLCPE_TX_DELAY)
/* transmit a packet */
static int BCMFASTPATH
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
wl_start(pNBuff_t pNBuff, struct net_device *dev)
#else
wl_start(struct sk_buff *skb, struct net_device *dev)
#endif /* DSLCPE_BLOG && DSLCPE */
{
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	struct sk_buff * skb;
#endif /* DSLCPE_BLOG && DSLCPE */
	if (!dev)
		return -ENETDOWN;

#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	skb = nbuff_xlate(pNBuff);
	if ( ((wl_info_t *)WL_INFO(dev))->pub->fcache ) { 
		blog_emit( skb, dev, TYPE_ETH, 0, BLOG_WLANPHY ); 
	}
#endif /* DSLCPE_BLOG && DSLCPE */

#ifdef AEI_VDSL_CUSTOMER_NCS
	wl_if_t *wlif;

	wlif = WL_DEV_IF(dev);

	if (wlif && wlif->usMirrorOutFlags != 0 &&
	    __constant_ntohs(skb->protocol) != ETHER_TYPE_BRCM)
		MultiMirrorPacket( skb, wlif->usMirrorOutFlags );
#endif

	return wl_start_int(WL_INFO(dev), WL_DEV_IF(dev), skb);
}
#endif	/* WLC_LOW */

static int BCMFASTPATH
wl_start_int(wl_info_t *wl, wl_if_t *wlif, struct sk_buff *skb)
{
	void *pkt;

	WL_TRACE(("wl%d: wl_start: len %d summed %d\n", wl->pub->unit, skb->len, skb->ip_summed));

	WL_LOCK(wl);

	/* Convert the packet. Mainly attach a pkttag */
	if ((pkt = PKTFRMNATIVE(wl->osh, skb)) == NULL) {
		WL_ERROR(("wl%d: PKTFRMNATIVE failed!\n", wl->pub->unit));
		WLCNTINCR(wl->pub->_cnt->txnobuf);
		PKTFREE(wl->osh, skb, TRUE);
		WL_UNLOCK(wl);
		return 0;
	}

#ifdef ARPOE
	/* Arp agent */
	if (ARPOE_ENAB(wl->pub)) {
		if (wl_arp_send_proc(wl->arpi, pkt) == ARP_REPLY_HOST) {
			PKTFREE(wl->osh, pkt, TRUE);
			WL_UNLOCK(wl);
			return 0;
		}
	}
#endif

#ifdef DSLCPE
	PKT_PREALLOCINC(wl->osh, pkt);
#endif /* DSLCPE */
#ifdef TOE
	/* Apply TOE */
	if (TOE_ENAB(wl->pub))
		wl_toe_send_proc(wl->toei, pkt);
#endif

	/* Fix the priority if WME is enabled */
	if (WME_ENAB(wl->pub) && (PKTPRIO(pkt) == 0))
		pktsetprio(pkt, FALSE);

#ifdef WL_UMK
	WL_UNLOCK(wl);
	wl_cdev_sendpkt(wlif, (struct sk_buff *)pkt);
#else
	wlc_sendpkt(wl->wlc, pkt, wlif->wlcif);

	WL_UNLOCK(wl);
#endif

	return (0);
}

void
wl_txflowcontrol(wl_info_t *wl, bool state, int prio)
{
	wl_if_t *wlif;

	ASSERT(prio == ALLPRIO);
	for (wlif = wl->if_list; wlif != NULL; wlif = wlif->next) {
		if (state == ON)
#ifdef DSLCPE			
			;		
#else			
			netif_stop_queue(wlif->dev);
#endif			
		else
			netif_wake_queue(wlif->dev);
	}
}

#if defined(AP) || defined(DSLCPE_DELAY) || defined(WLC_HIGH_ONLY) || \
	defined(WL_MONITOR) || defined(WL_CONFIG_RFKILL_INPUT)
/* Schedule a completion handler to run at safe time */
static int
wl_schedule_task(wl_info_t *wl, void (*fn)(struct wl_task *task), void *context)
{
	wl_task_t *task;

	WL_TRACE(("wl%d: wl_schedule_task\n", wl->pub->unit));

	if (!(task = MALLOC(wl->osh, sizeof(wl_task_t)))) {
		WL_ERROR(("wl%d: wl_schedule_task: out of memory, malloced %d bytes\n",
			wl->pub->unit, MALLOCED(wl->osh)));
		return -ENOMEM;
	}

	MY_INIT_WORK(&task->work, (work_func_t)fn);
	task->context = context;

	if (!schedule_work(&task->work)) {
		WL_ERROR(("wl%d: schedule_work() failed\n", wl->pub->unit));
		MFREE(wl->osh, task, sizeof(wl_task_t));
		return -ENOMEM;
	}

	atomic_inc(&wl->callbacks);

	return 0;
}
#endif /* defined(AP) || defined(DSLCPE_DELAY) || defined(WLC_HIGH_ONLY) || defined(WL_MONITOR) */

static struct wl_if *
wl_alloc_if(wl_info_t *wl, int iftype, uint subunit, struct wlc_if* wlcif)
{
	struct net_device *dev;
	wl_if_t *wlif;
	wl_if_t *p;
#ifdef DSLCPE
	unsigned char locked=0;
#endif /* DSLCPE */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29))
	if (!(wlif = MALLOC(wl->osh, sizeof(wl_if_t)))) {
		WL_ERROR(("wl%d: wl_alloc_if: out of memory, malloced %d bytes\n",
		          (wl->pub)?wl->pub->unit:subunit, MALLOCED(wl->osh)));
		return NULL;
	}
	bzero(wlif, sizeof(wl_if_t));
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22))
	if (!(dev = MALLOC(wl->osh, sizeof(struct net_device)))) {
		MFREE(wl->osh, wlif, sizeof(wl_if_t));
		WL_ERROR(("wl%d: wl_alloc_if: out of memory, malloced %d bytes\n",
		          (wl->pub)?wl->pub->unit:subunit, MALLOCED(wl->osh)));
		return NULL;
	}
	bzero(dev, sizeof(struct net_device));
	ether_setup(dev);
	strncpy(dev->name, name, IFNAMSIZ);
#else
#ifdef DSLCPE
	if(softirq_count()) {
		WL_UNLOCK(wl);
		locked=1;
	}
#endif /* DSLCPE */
	/* Allocate net device, including space for private structure */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
	dev = alloc_netdev(sizeof(wl_if_t), name, ether_setup);
	wlif = netdev_priv(dev);
#ifdef DSLCPE
	if(locked)
		WL_LOCK(wl);
#endif /* DSLCPE */
	if (!dev) {
#else
	dev = alloc_netdev(0, name, ether_setup);
#ifdef DSLCPE
	if(locked)
		WL_LOCK(wl);
#endif /* DSLCPE */
	if (!dev) {
		MFREE(wl->osh, wlif, sizeof(wl_if_t));
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
		WL_ERROR(("wl%d: wl_alloc_if: out of memory, alloc_netdev\n",
			(wl->pub)?wl->pub->unit:subunit));
		return NULL;
	}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) */

	wlif->dev = dev;
	wlif->wl = wl;
	wlif->wlcif = wlcif;
	wlif->subunit = subunit;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 29)
	dev->priv = wlif;
#endif

	/* match current flow control state */
	if (iftype != WL_IFTYPE_MON && wl->dev && netif_queue_stopped(wl->dev))
		netif_stop_queue(dev);

	/* add the interface to the interface linked list */
	if (wl->if_list == NULL)
		wl->if_list = wlif;
	else {
		p = wl->if_list;
		while (p->next != NULL)
			p = p->next;
		p->next = wlif;
	}
	return wlif;
}

static void
wl_free_if(wl_info_t *wl, wl_if_t *wlif)
{
	wl_if_t *p;

	/* check if register_netdev was successful */
	if (wlif->dev_registed) {
#ifdef HNDCTF
		if (wl->cih)
			ctf_dev_unregister(wl->cih, wlif->dev);
#endif /* HNDCTF */
		unregister_netdev(wlif->dev);
	}

	/* remove the interface from the interface linked list */
	p = wl->if_list;
	if (p == wlif)
		wl->if_list = p->next;
	else {
		while (p != NULL && p->next != wlif)
			p = p->next;
		if (p != NULL)
			p->next = p->next->next;
	}

	if (wlif->dev) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22))
		MFREE(wl->osh, wlif->dev, sizeof(struct net_device));
#else
		free_netdev(wlif->dev);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29))
		/* wlif is part of dev, we will not have wlif reference after this point */
		return;
#endif
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) */
	}
	MFREE(wl->osh, wlif, sizeof(wl_if_t));
}

#ifdef AP
static wlc_bsscfg_t *
wl_bsscfg_find(wl_if_t *wlif)
{
	wl_info_t *wl = wlif->wl;
	return wlc_bsscfg_find_by_wlcif(wl->wlc, wlif->wlcif);
}

/* Create a virtual interface. Call only from safe time! can't call register_netdev with WL_LOCK */

static void
_wl_add_if(wl_task_t *task)
{
	wl_if_t *wlif = (wl_if_t *) task->context;
	wl_info_t *wl = wlif->wl;
	struct net_device *dev = wlif->dev;
	wlc_bsscfg_t *cfg;
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	dev->hard_start_xmit = (HardStartXmitFuncP) wl_start;
#else
	dev->hard_start_xmit = wl_start;
#endif /* DSLCPE_BLOG && DSLCPE */
	dev->do_ioctl = wl_ioctl;
	dev->set_mac_address = wl_set_mac_address;
#if WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	dev->ethtool_ops = &wl_ethtool_ops;
#endif /* WIRELESS_EXT >= 19 || LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29) */
	dev->get_stats = wl_get_stats;

	cfg = wl_bsscfg_find(wlif);
	ASSERT(cfg != NULL);

#ifdef NAPI_POLL
	dev->poll = wl_poll;
	dev->weight = 64;
#endif /* NAPI_POLL */

	bcopy(&cfg->cur_etheraddr, dev->dev_addr, ETHER_ADDR_LEN);

	if (register_netdev(dev)) {
		WL_ERROR(("wl%d: wl_add_if: register_netdev() failed for \"%s\"\n",
			wl->pub->unit, dev->name));
		goto done;
	}
	wlif->dev_registed = TRUE;

#ifdef HNDCTF
	if ((ctf_dev_register(wl->cih, dev, FALSE) != BCME_OK) ||
	    (ctf_enable(wl->cih, dev, TRUE) != BCME_OK)) {
		ctf_dev_unregister(wl->cih, dev);
		WL_ERROR(("wl%d: ctf_dev_register() failed\n", wl->pub->unit));
		goto done;
	}
#endif /* HNDCTF */

done:
	MFREE(wl->osh, task, sizeof(wl_task_t));
	atomic_dec(&wl->callbacks);
}

/* Schedule _wl_add_if() to be run at safe time. */
struct wl_if *
wl_add_if(wl_info_t *wl, struct wlc_if* wlcif, uint unit, struct ether_addr *remote)
{
	wl_if_t *wlif;
	char *devname;
	int iftype;

	if (remote) {
		iftype = WL_IFTYPE_WDS;
		devname = "wds";
	} else {
		iftype = WL_IFTYPE_BSS;
		devname = "wl";
	}

	wlif = wl_alloc_if(wl, iftype, unit, wlcif);

	if (!wlif) {
		WL_ERROR(("wl%d: wl_add_if: failed to create %s interface %d\n", wl->pub->unit,
			(remote)?"WDS":"BSS", unit));
		return NULL;
	}

	sprintf(wlif->dev->name, "%s%d.%d", devname, wl->pub->unit, wlif->subunit);

	if (wl_schedule_task(wl, _wl_add_if, wlif)) {
		MFREE(wl->osh, wlif, sizeof(wl_if_t) + sizeof(struct net_device));
		return NULL;
	}

	return wlif;
}

/* Remove a virtual interface. Call only from safe time! */
static void
_wl_del_if(wl_task_t *task)
{
	wl_if_t *wlif = (wl_if_t *) task->context;
	wl_info_t *wl = wlif->wl;

	wl_free_if(wl, wlif);

	MFREE(wl->osh, task, sizeof(wl_task_t));
	atomic_dec(&wl->callbacks);
}

/* Schedule _wl_del_if() to be run at safe time. */
void
wl_del_if(wl_info_t *wl, wl_if_t *wlif)
{
	ASSERT(wlif != NULL);
	ASSERT(wlif->wl == wl);

	wlif->wlcif = NULL;

	if (wl_schedule_task(wl, _wl_del_if, wlif)) {
		WL_ERROR(("wl%d: wl_del_if: schedule_task() failed\n", wl->pub->unit));
		return;
	}
}
#endif /* AP */

#ifndef WL_UMK
/* Return pointer to interface name */
char *
wl_ifname(wl_info_t *wl, wl_if_t *wlif)
{
	if (wlif)
		return wlif->dev->name;
	else
		return wl->dev->name;
}

void
wl_init(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_init\n", wl->pub->unit));

	wl_reset(wl);

	wlc_init(wl->wlc);
}

uint
wl_reset(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_reset\n", wl->pub->unit));

	wlc_reset(wl->wlc);

	/* dpc will not be rescheduled */
	wl->resched = 0;

	return (0);
}
#endif /* WL_UMK */

/*
 * These are interrupt on/off entry points. Disable interrupts
 * during interrupt state transition.
 */
void BCMFASTPATH
wl_intrson(wl_info_t *wl)
{
#if defined(WLC_LOW) && !defined(WL_UMK)
	unsigned long flags;

	INT_LOCK(wl, flags);
	wlc_intrson(wl->wlc);
	INT_UNLOCK(wl, flags);
#endif /* WLC_LOW && !WL_UMK */
}

bool
wl_alloc_dma_resources(wl_info_t *wl, uint addrwidth)
{
	return TRUE;
}

uint32 BCMFASTPATH
wl_intrsoff(wl_info_t *wl)
{
#if defined(WLC_LOW) && !defined(WL_UMK)
	unsigned long flags;
	uint32 status;

	INT_LOCK(wl, flags);
	status = wlc_intrsoff(wl->wlc);
	INT_UNLOCK(wl, flags);
	return status;
#else
	return 0;
#endif /* WLC_LOW && !WL_UMK */
}

void
wl_intrsrestore(wl_info_t *wl, uint32 macintmask)
{
#if defined(WLC_LOW) && !defined(WL_UMK)
	unsigned long flags;

	INT_LOCK(wl, flags);
	wlc_intrsrestore(wl->wlc, macintmask);
	INT_UNLOCK(wl, flags);
#endif /* WLC_LOW && !WL_UMK */
}

int
wl_up(wl_info_t *wl)
{
	int error = 0;

	WL_TRACE(("wl%d: wl_up\n", wl->pub->unit));

#ifdef DSLCPE_NAPI
	if (NAPI_ENAB(wl->pub))
		set_bit(__LINK_STATE_START, &wl->dev->state);
#endif
	if (wl->pub->up)
		return (0);

#ifdef WLC_HIGH_ONLY
	if (bcm_rpc_is_asleep(wl->rpc)) {
		if (!bcm_rpc_resume(wl->rpc)) {
			WL_ERROR(("wl%d: wl_up fail to resume RPC\n", wl->pub->unit));
			return -1;
		}
	}
#endif
#ifndef WL_UMK
	error = wlc_up(wl->wlc);

	/* wake (not just start) all interfaces */
	if (!error)
#endif
		wl_txflowcontrol(wl, OFF, ALLPRIO);

	return (error);
}

void
wl_down(wl_info_t *wl)
{
	wl_if_t *wlif;
#ifndef WL_UMK
	uint callbacks, ret_val = 0;
#endif

	WL_TRACE(("wl%d: wl_down\n", wl->pub->unit));

	for (wlif = wl->if_list; wlif != NULL; wlif = wlif->next) {
		netif_down(wlif->dev);
		netif_stop_queue(wlif->dev);
	}

#ifndef WL_UMK

	/* call common down function */
	ret_val = wlc_down(wl->wlc);
	callbacks = atomic_read(&wl->callbacks) - ret_val;

	/* wait for down callbacks to complete */
	WL_UNLOCK(wl);

#ifndef WLC_HIGH_ONLY
	/* For HIGH_only driver, it's important to actually schedule other work,
	 * not just spin wait since everything runs at schedule level
	 */
	SPINWAIT((atomic_read(&wl->callbacks) > callbacks), 100 * 1000);
#else
	bcm_rpc_sleep(wl->rpc);
#endif /* WLC_HIGH_ONLY */

	WL_LOCK(wl);
#endif /* !WL_UMK */
}

#ifdef WL_UMK
static int wl_toe_get(wl_info_t *wl, uint32 *toe_ol) { return 0; }
static int wl_toe_set(wl_info_t *wl, uint32 toe_ol) { return 0; }
#else
/* Retrieve current toe component enables, which are kept as a bitmap in toe_ol iovar */
static int
wl_toe_get(wl_info_t *wl, uint32 *toe_ol)
{
	if (wlc_iovar_getint(wl->wlc, "toe_ol", toe_ol) != 0)
		return -EOPNOTSUPP;

	return 0;
}

/* Set current toe component enables in toe_ol iovar, and set toe global enable iovar */
static int
wl_toe_set(wl_info_t *wl, uint32 toe_ol)
{
	if (wlc_iovar_setint(wl->wlc, "toe_ol", toe_ol) != 0)
		return -EOPNOTSUPP;

	/* Enable toe globally only if any components are enabled. */

	if (wlc_iovar_setint(wl->wlc, "toe", (toe_ol != 0)) != 0)
		return -EOPNOTSUPP;

	return 0;
}
#endif /* WL_UMK */

static void
wl_get_driver_info(struct net_device *dev, struct ethtool_drvinfo *info)
{
	wl_info_t *wl = WL_INFO(dev);
	bzero(info, sizeof(struct ethtool_drvinfo));
	sprintf(info->driver, "wl%d", wl->pub->unit);
	strcpy(info->version, EPI_VERSION_STR);
}

static int
wl_ethtool(wl_info_t *wl, void *uaddr, wl_if_t *wlif)
{
	struct ethtool_drvinfo info;
	struct ethtool_value edata;
	uint32 cmd;
	uint32 toe_cmpnt = 0, csum_dir;
	int ret;

	WL_TRACE(("wl%d: %s\n", wl->pub->unit, __FUNCTION__));
	if (copy_from_user(&cmd, uaddr, sizeof(uint32)))
		return (-EFAULT);

	switch (cmd) {
	case ETHTOOL_GDRVINFO:
		wl_get_driver_info(wl->dev, &info);
		info.cmd = cmd;
		if (copy_to_user(uaddr, &info, sizeof(info)))
			return (-EFAULT);
		break;

	/* Get toe offload components */
	case ETHTOOL_GRXCSUM:
	case ETHTOOL_GTXCSUM:
		if ((ret = wl_toe_get(wl, &toe_cmpnt)) < 0)
			return ret;

		csum_dir = (cmd == ETHTOOL_GTXCSUM) ? TOE_TX_CSUM_OL : TOE_RX_CSUM_OL;

		edata.cmd = cmd;
		edata.data = (toe_cmpnt & csum_dir) ? 1 : 0;

		if (copy_to_user(uaddr, &edata, sizeof(edata)))
			return (-EFAULT);
		break;

	/* Set toe offload components */
	case ETHTOOL_SRXCSUM:
	case ETHTOOL_STXCSUM:
		if (copy_from_user(&edata, uaddr, sizeof(edata)))
			return (-EFAULT);

		/* Read the current settings, update and write back */
		if ((ret = wl_toe_get(wl, &toe_cmpnt)) < 0)
			return ret;

		csum_dir = (cmd == ETHTOOL_STXCSUM) ? TOE_TX_CSUM_OL : TOE_RX_CSUM_OL;

		if (edata.data != 0)
			toe_cmpnt |= csum_dir;
		else
			toe_cmpnt &= ~csum_dir;

		if ((ret = wl_toe_set(wl, toe_cmpnt)) < 0)
			return ret;

		/* If setting TX checksum mode, tell Linux the new mode */
		if (cmd == ETHTOOL_STXCSUM) {
#ifdef DSLCPE
			if (!wl->dev)
				return -ENETDOWN;
#endif /* DSLCPE */
			if (edata.data)
				wl->dev->features |= NETIF_F_IP_CSUM;
			else
				wl->dev->features &= ~NETIF_F_IP_CSUM;
		}

		break;

	default:
		return (-EOPNOTSUPP);

	}

	return (0);
}

int
wl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	wl_info_t *wl;
	wl_if_t *wlif;
	void *buf = NULL;
	wl_ioctl_t ioc;
	int bcmerror;
#ifdef AEI_VDSL_CUSTOMER_NCS
	MirrorCfg mirrorCfg;
#endif

	if (!dev)
		return -ENETDOWN;

	wl = WL_INFO(dev);
	wlif = WL_DEV_IF(dev);
	if (wlif == NULL || wl == NULL)
		return -ENETDOWN;

	bcmerror = 0;

	WL_TRACE(("wl%d: wl_ioctl: cmd 0x%x\n", wl->pub->unit, cmd));

#ifdef CONFIG_PREEMPT
	if (preempt_count())
		WL_ERROR(("wl%d: wl_ioctl: cmd = 0x%x, preempt_count=%d\n",
			wl->pub->unit, cmd, preempt_count()));
#endif
#ifdef WL_UMK
	if (wl->pid == 0)
		return -ENETDOWN;
#endif

#ifdef USE_IW
	/* linux wireless extensions */
	if ((cmd >= SIOCIWFIRST) && (cmd <= SIOCIWLAST)) {
		/* may recurse, do NOT lock */
		return wl_iw_ioctl(dev, ifr, cmd);
	}
#endif /* USE_IW */

	if (cmd == SIOCETHTOOL)
		return (wl_ethtool(wl, (void*)ifr->ifr_data, wlif));

	switch (cmd) {
		case SIOCDEVPRIVATE :
			break;
#ifdef AEI_VDSL_CUSTOMER_NCS
		case SIOCPORTMIRROR:
		{
			if (copy_from_user((void *)&mirrorCfg, (int *)ifr->ifr_data, sizeof(MirrorCfg)))
				return -EFAULT;
			else
			{
				if ( mirrorCfg.nDirection == MIRROR_DIR_IN )
				{
					UpdateMirrorFlag(&(wlif->usMirrorInFlags),
						mirrorCfg.szMirrorInterface, mirrorCfg.nStatus);
				}
				else /* MIRROR_DIR_OUT */
				{
					UpdateMirrorFlag(&(wlif->usMirrorOutFlags),
						mirrorCfg.szMirrorInterface, mirrorCfg.nStatus);
				}
			}
			return 0;
		}
#endif /* AEI_VDSL_CUSTOMER_NCS */
		default:
			bcmerror = BCME_UNSUPPORTED;
			goto done2;
	}

	if (copy_from_user(&ioc, ifr->ifr_data, sizeof(wl_ioctl_t))) {
		bcmerror = BCME_BADADDR;
		goto done2;
	}

	/* optimization for direct ioctl calls from kernel */
	if (segment_eq(get_fs(), KERNEL_DS))
		buf = ioc.buf;

	else if (ioc.buf) {
		if (!(buf = (void *) MALLOC(wl->osh, MAX(ioc.len, WLC_IOCTL_MAXLEN)))) {
			bcmerror = BCME_NORESOURCE;
			goto done2;
		}

		if (copy_from_user(buf, ioc.buf, ioc.len)) {
			bcmerror = BCME_BADADDR;
			goto done1;
		}
	}

#ifndef WL_UMK
	WL_LOCK(wl);
#endif
	if (!capable(CAP_NET_ADMIN)) {
		bcmerror = BCME_EPERM;
	} else {
#ifndef WL_UMK
		bcmerror = wlc_ioctl(wl->wlc, ioc.cmd, buf, ioc.len, wlif->wlcif);
#else
		bcmerror = _wl_ioctl(wl, ioc.cmd, buf, ioc.len);
#endif
	}
#ifndef WL_UMK
	WL_UNLOCK(wl);
#endif

done1:
	if (ioc.buf && (ioc.buf != buf)) {
		if (copy_to_user(ioc.buf, buf, ioc.len))
			bcmerror = BCME_BADADDR;
		MFREE(wl->osh, buf, MAX(ioc.len, WLC_IOCTL_MAXLEN));
	}

done2:
	ASSERT(VALID_BCMERROR(bcmerror));
	if (bcmerror != 0)
		wl->pub->bcmerror = bcmerror;
	return (OSL_ERROR(bcmerror));
}

static struct net_device_stats*
wl_get_stats(struct net_device *dev)
{
#ifndef WL_UMK
	struct net_device_stats *stats_watchdog = NULL;
#endif
	struct net_device_stats *stats = NULL;
	wl_info_t *wl;

	if (!dev)
		return NULL;

	if ((wl = WL_INFO(dev)) == NULL)
		return NULL;

#ifndef WL_UMK
	/* At rmmod we will return NULL for calls to wl_get_stats
	 * if pub is already freed. Which will be the case
	 * during module_exit, where pub is deallocated and
	 * marked as NULL.
	 */
	if (!(wl->pub))
		return NULL;

	WL_TRACE(("wl%d: wl_get_stats\n", wl->pub->unit));

	stats = &wl->stats;
	ASSERT(wl->stats_id < 2);
	stats_watchdog = &wl->stats_watchdog[wl->stats_id];

	memcpy(stats, stats_watchdog, sizeof(struct net_device_stats));
#else
	if ((stats = &wl->stats) == NULL)
		return NULL;

	stats->rx_packets = WLCNTVAL(wl->pub->_cnt->rxframe);
	stats->tx_packets = WLCNTVAL(wl->pub->_cnt->txframe);
	stats->rx_bytes = WLCNTVAL(wl->pub->_cnt->rxbyte);
	stats->tx_bytes = WLCNTVAL(wl->pub->_cnt->txbyte);
	stats->rx_errors = WLCNTVAL(wl->pub->_cnt->rxerror);
	stats->tx_errors = WLCNTVAL(wl->pub->_cnt->txerror);
	stats->collisions = 0;

	stats->rx_length_errors = 0;
	stats->rx_over_errors = WLCNTVAL(wl->pub->_cnt->rxoflo);
	stats->rx_crc_errors = WLCNTVAL(wl->pub->_cnt->rxcrc);
	stats->rx_frame_errors = 0;
	stats->rx_fifo_errors = WLCNTVAL(wl->pub->_cnt->rxoflo);
	stats->rx_missed_errors = 0;

	stats->tx_fifo_errors = WLCNTVAL(wl->pub->_cnt->txuflo);
#endif /* WL_UMK */
	return (stats);
}

#ifdef USE_IW
struct iw_statistics *
wl_get_wireless_stats(struct net_device *dev)
{
	int res = 0;
	wl_info_t *wl;
	wl_if_t *wlif;
	struct iw_statistics *wstats = NULL;
#ifndef WL_UMK
	struct iw_statistics *wstats_watchdog = NULL;
#endif
	int phy_noise, rssi;

	if (!dev)
		return NULL;

	wl = WL_INFO(dev);
	wlif = WL_DEV_IF(dev);

	WL_TRACE(("wl%d: wl_get_wireless_stats\n", wl->pub->unit));

#ifndef WL_UMK
	wstats = &wl->wstats;
	ASSERT(wl->stats_id < 2);
	wstats_watchdog = &wl->wstats_watchdog[wl->stats_id];

	phy_noise = wl->phy_noise;
#if WIRELESS_EXT > 11
	wstats->discard.nwid = 0;
	wstats->discard.code = wstats_watchdog->discard.code;
	wstats->discard.fragment = wstats_watchdog->discard.fragment;
	wstats->discard.retries = wstats_watchdog->discard.retries;
	wstats->discard.misc = wstats_watchdog->discard.misc;

	wstats->miss.beacon = 0;
#endif /* WIRELESS_EXT > 11 */
#else
	wstats = &wlif->iw.wstats;
	res = _wl_ioctl(wl, WLC_GET_PHY_NOISE, &phy_noise, sizeof(phy_noise));
	if (res) {
		WL_ERROR(("wl%d: %s: WLC_GET_PHY_NOISE failed (%d)\n",
			wl->pub->unit, __FUNCTION__, res));
		return NULL;
	}
#endif /* WL_UMK */

	/* RSSI measurement is somewhat meaningless for AP in this context */
	if (AP_ENAB(wl->pub))
		rssi = 0;
	else {
		scb_val_t scb;
#ifndef WL_UMK
		res = wlc_ioctl(wl->wlc, WLC_GET_RSSI, &scb, sizeof(scb), wlif->wlcif);
#else
		res = _wl_ioctl(wl, WLC_GET_RSSI, &scb, sizeof(scb));
#endif
		if (res) {
			WL_ERROR(("wl%d: %s: WLC_GET_RSSI failed (%d)\n",
				wl->pub->unit, __FUNCTION__, res));
			return NULL;
		}
		rssi = scb.val;
	}

	if (rssi <= WLC_RSSI_NO_SIGNAL)
		wstats->qual.qual = 0;
	else if (rssi <= WLC_RSSI_VERY_LOW)
		wstats->qual.qual = 1;
	else if (rssi <= WLC_RSSI_LOW)
		wstats->qual.qual = 2;
	else if (rssi <= WLC_RSSI_GOOD)
		wstats->qual.qual = 3;
	else if (rssi <= WLC_RSSI_VERY_GOOD)
		wstats->qual.qual = 4;
	else
		wstats->qual.qual = 5;

	/* Wraps to 0 if RSSI is 0 */
	wstats->qual.level = 0x100 + rssi;
	wstats->qual.noise = 0x100 + phy_noise;
#if WIRELESS_EXT > 18
	wstats->qual.updated |= (IW_QUAL_ALL_UPDATED | IW_QUAL_DBM);
#else
	wstats->qual.updated |= 7;
#endif /* WIRELESS_EXT > 18 */

#ifndef WL_UMK
	return wstats;
#else
#if WIRELESS_EXT > 11
	wstats->discard.nwid = 0;
	wstats->discard.code = WLCNTVAL(wl->pub->_cnt->rxundec);
	wstats->discard.fragment = WLCNTVAL(wl->pub->_cnt->rxfragerr);
	wstats->discard.retries = WLCNTVAL(wl->pub->_cnt->txfail);
	wstats->discard.misc = WLCNTVAL(wl->pub->_cnt->rxrunt) + WLCNTVAL(wl->pub->_cnt->rxgiant);

	wstats->miss.beacon = 0;
#endif /* WIRELESS_EXT > 11 */

	if (res == 0)
		return wstats;
	else
		return NULL;
#endif /* WL_UMK */
}
#endif /* USE_IW */

static int
wl_set_mac_address(struct net_device *dev, void *addr)
{
	int err = 0;
	wl_info_t *wl;
	struct sockaddr *sa = (struct sockaddr *) addr;

	if (!dev)
		return -ENETDOWN;

	wl = WL_INFO(dev);

	WL_TRACE(("wl%d: wl_set_mac_address\n", wl->pub->unit));

#ifndef WL_UMK
	WL_LOCK(wl);

	bcopy(sa->sa_data, dev->dev_addr, ETHER_ADDR_LEN);
	err = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, sa->sa_data, ETHER_ADDR_LEN,
		IOV_SET, (WL_DEV_IF(dev))->wlcif);
	WL_UNLOCK(wl);
#else
	bcopy(sa->sa_data, dev->dev_addr, ETHER_ADDR_LEN);
	err = _wl_iovar(wl, WLC_SET_VAR, "cur_etheraddr", &sa->sa_data, ETHER_ADDR_LEN);
#endif	/* !WL_UMK */
	if (err)
		WL_ERROR(("wl%d: wl_set_mac_address: error setting MAC addr override\n",
			wl->pub->unit));
	return err;
}

static void
wl_set_multicast_list(struct net_device *dev)
{
#ifndef DSLCPE_TX_DELAY
#ifndef WLC_HIGH_ONLY
	_wl_set_multicast_list(dev);
#else
	wl_info_t *wl = WL_INFO(dev);

	wl->multicast_task.context = dev;

	if (schedule_work(&wl->multicast_task.work)) {
		/* work item may already be on the work queue, so only inc callbacks if
		 * we actually schedule a new item
		 */
		atomic_inc(&wl->callbacks);
#endif /* WLC_HIGH_ONLY */
#else /* DSLCPE_TX_DELAY */
	/* set_multicase_list function should be used for rx, and this function is called only when multicast
	 MAC address is set. In order to sync w/ original workqueue, this function is put
	under TX_DELAY. Could be move to under RX_DELAY later.
	*/
	wl_info_t *wl = WL_INFO(dev);
	if (!wl->enable_tx_delay) {
		_wl_set_multicast_list(dev);
	}
	else {
		wl->multicast_task.context = dev;

		if (schedule_work(&wl->multicast_task.work)) {
			/* work item may already be on the work queue, so only inc callbacks if
			 * we actually schedule a new item
			 */
			atomic_inc(&wl->callbacks);
		}
	}
#endif /* DSLCPE_TX_DELAY */
}

static void
_wl_set_multicast_list(struct net_device *dev)
{
#ifdef WL_UMK
	return;
#else
	wl_info_t *wl;
	struct dev_mc_list *mclist;
	int i;

	if (!dev)
		return;
	wl = WL_INFO(dev);

	WL_TRACE(("wl%d: wl_set_multicast_list\n", wl->pub->unit));

	WL_LOCK(wl);

	if (wl->pub->up) {
		wl->pub->allmulti = (dev->flags & IFF_ALLMULTI)? TRUE: FALSE;

		/* copy the list of multicasts into our private table */
		for (i = 0, mclist = dev->mc_list; mclist && (i < dev->mc_count);
			i++, mclist = mclist->next) {
			if (i >= MAXMULTILIST) {
				wl->pub->allmulti = TRUE;
				i = 0;
				break;
			}
			wl->pub->multicast[i] = *((struct ether_addr*) mclist->dmi_addr);
		}
		wl->pub->nmulticast = i;
		wlc_set(wl->wlc, WLC_SET_PROMISC, (dev->flags & IFF_PROMISC));
	}

	WL_UNLOCK(wl);
#endif /* WL_UMK */
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
irqreturn_t BCMFASTPATH
wl_isr(int irq, void *dev_id)
#else
irqreturn_t BCMFASTPATH
wl_isr(int irq, void *dev_id, struct pt_regs *ptregs)
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20) */
{
#if defined(WLC_LOW) && !defined(WL_UMK)
	wl_info_t *wl;
	bool ours, wantdpc;
	unsigned long flags;

	wl = (wl_info_t*) dev_id;

	WL_ISRLOCK(wl, flags);

	/* call common first level interrupt handler */
	if ((ours = wlc_isr(wl->wlc, &wantdpc))) {
		/* if more to do... */
		if (wantdpc) {

			/* ...and call the second level interrupt handler */
			/* schedule dpc */
			ASSERT(wl->resched == FALSE);
#ifdef NAPI_POLL
			/* allow the device to be added to the cpu polling
			 * list if we are up
			 */
			if (netif_rx_schedule_prep(wl->dev)) {
				/* tell the network core that we have packets
				 * to send up.
				 */
				__netif_rx_schedule(wl->dev);
			} else {
				WL_ERROR(("wl%d: wl_isr: intr while in poll!\n",
				          wl->pub->unit));
				wl_intrson(wl);
			}
#else /* NAPI_POLL */
#ifdef DSLCPE_NAPI
			if (NAPI_ENAB(wl->pub)) {
				napi_schedule(&((wl_if_t *)netdev_priv(wl->dev))->napi);
			}
			else
#endif /* DSLCPE_NAPI */

			tasklet_schedule(&wl->tasklet);
#endif /* NAPI_POLL */
		}
	}

	WL_ISRUNLOCK(wl, flags);

	return IRQ_RETVAL(ours);
#else
	return IRQ_RETVAL(0);
#endif /* WLC_LOW && !WL_UMK */
}

#ifndef WL_UMK
#ifdef NAPI_POLL
static int BCMFASTPATH
wl_poll(struct net_device *dev, int *budget)
#else /* NAPI_POLL */
#ifdef DSLCPE_NAPI
static int wl_poll(struct napi_struct *napi, int budget)
{
    int work_done;
    wl_info_t *wl;
	wl_if_t *wlif;

    
	wlif = (wl_if_t *)container_of(napi, wl_if_t, napi);
	wl = wlif->wl;

    
    wl->pub->napi_budget = budget;

    work_done = wl_dpc((ulong)wl);

    return work_done;
}
#endif
#ifdef DSLCPE_NAPI
static int BCMFASTPATH
#else
static void BCMFASTPATH
#endif
wl_dpc(ulong data)
#endif /* NAPI_POLL */
{
#ifdef WLC_LOW
#ifdef NAPI_POLL
	wl_info_t *wl = WL_INFO(dev);

	wl->pub->rxbnd = min(RXBND, *budget);
	ASSERT(wl->pub->rxbnd <= dev->quota);
#else /* NAPI_POLL */
#ifdef DSLCPE_NAPI
	int work_done = 0;
	int work_todo = 0;
	wl_if_t *wlif;
#endif //DSLCPE_NAPI
	wl_info_t *wl = (wl_info_t *)data;

#ifdef DSLCPE_NAPI
	wlif = (wl_if_t *)netdev_priv(wl->dev);
#endif
	WL_LOCK(wl);
#endif /* NAPI_POLL */

	/* call the common second level interrupt handler */
	if (wl->pub->up) {
#if defined(DSLCPE_DELAY)
		if (wlc_is_dpc_deferred(wl->wlc)) {
			wl->resched = 1;
#ifdef DSLCPE_NAPI
			if (NAPI_ENAB(wl->pub)) {
				napi_complete(&wlif->napi);
			}
#endif /* DSLCPE_NAPI */
			goto done;
		}
#endif /* defined(DSLCPE) && defined(DSLCPE_DELAY) */
		if (wl->resched) {
			unsigned long flags;

			INT_LOCK(wl, flags);
			wlc_intrsupd(wl->wlc);
			INT_UNLOCK(wl, flags);
		}

#ifdef DSLCPE_NAPI
		if (NAPI_ENAB(wl->pub))
			work_todo = wl->pub->napi_budget;
		wl->resched = wlc_dpc(wl->wlc, TRUE, work_todo, &work_done);
#else
		wl->resched = wlc_dpc(wl->wlc, TRUE);
#endif /* DSLCPE_NAPI */
	}

	/* wlc_dpc() may bring the driver down */
	if (!wl->pub->up)
		goto done;

#ifndef NAPI_POLL
#ifdef DSLCPE_NAPI
	if (NAPI_ENAB(wl->pub)) {
		wl->pub->napi_budget -= work_done;
	}
#endif /* DSLCPE_NAPI */
	/* re-schedule dpc */
	if (wl->resched)
	{
#ifdef DSLCPE_NAPI
		if (NAPI_ENAB(wl->pub)) {
			goto done;
		} else
#endif /* DSLCPE_NAPI */
		tasklet_schedule(&wl->tasklet);
	}
	else {
		/* re-enable interrupts */
#ifdef DSLCPE_NAPI
		if (NAPI_ENAB(wl->pub)) {
			napi_complete(&wlif->napi);
		}
#endif /* DSLCPE_NAPI */
		wl_intrson(wl);
	}
#endif /* NAPI_POLL */

done:
#ifdef NAPI_POLL
	WL_TRACE(("wl%d: wl_poll: rxbnd %d quota %d budget %d processed %d\n",
	          wl->pub->unit, wl->pub->rxbnd, dev->quota,
	          *budget, wl->pub->processed));

	ASSERT(wl->pub->processed <= wl->pub->rxbnd);

	/* update number of frames processed */
	*budget -= wl->pub->processed;
	dev->quota -= wl->pub->processed;

	/* we got packets but no budget */
	if (wl->resched)
		/* indicate that we are not done, don't enable
		 * interrupts yet. linux network core will call
		 * us again.
		 */
		return 1;

	netif_rx_complete(dev);

	/* enable interrupts now */
	wl_intrson(wl);

	/* indicate that we are done */
	return 0;
#else /* NAPI_POLL */
	WL_UNLOCK(wl);
#ifdef DSLCPE_NAPI
	return work_done;
#else
	return;
#endif /* DSLCPE_NAPI */
#endif /* NAPI_POLL */
#endif /* WLC_LOW */
}
#endif /* WL_UMK */

static inline int32
wl_ctf_forward(wl_info_t *wl, struct sk_buff *skb)
{
#ifdef HNDCTF
	/* use slow path if ctf is disabled */
	if (!CTF_ENAB(wl->cih))
		return (BCME_ERROR);

	/* try cut thru first */
	if (ctf_forward(wl->cih, skb) != BCME_ERROR)
		return (BCME_OK);

	/* clear skipct flag before sending up */
	PKTCLRSKIPCT(wl->osh, skb);
#endif /* HNDCTF */

#ifdef CTFPOOL
	/* allocate and add a new skb to the pkt pool */
	if (PKTISFAST(wl->osh, skb))
		osl_ctfpool_add(wl->osh);

	/* clear fast buf flag before sending up */
	PKTCLRFAST(wl->osh, skb);

	/* re-init the hijacked field */
	CTFPOOLPTR(wl->osh, skb) = NULL;
#endif /* CTFPOOL */

	return (BCME_ERROR);
}

/*
 * The last parameter was added for the build. Caller of
 * this function should pass 1 for now.
 */
void BCMFASTPATH
wl_sendup(wl_info_t *wl, wl_if_t *wlif, void *p, int numpkt)
{
#ifdef WL_UMK
	int ret;
	struct sk_buff *skb = (struct sk_buff *)p;
#else
	struct sk_buff *skb;
#endif

#ifdef AEI_VDSL_CUSTOMER_NCS
        struct sk_buff *tmp_skb = (struct sk_buff *)p;
        struct ethhdr *eth;
        wl_if_t *main_wlif;

        eth = (struct ethhdr *)tmp_skb->data;
        main_wlif = WL_DEV_IF(wl->dev);

        if (wlif)
        {
                if (wlif->dev && wlif->usMirrorInFlags != 0 &&
                    eth->h_proto != ETHER_TYPE_BRCM)
                        MultiMirrorPacket( tmp_skb , wlif->usMirrorInFlags );
        }
        else
        {
                if (main_wlif && main_wlif->usMirrorInFlags != 0 &&
                    eth->h_proto != ETHER_TYPE_BRCM)
                        MultiMirrorPacket( tmp_skb , main_wlif->usMirrorInFlags );
        }
#endif /* AEI_VDSL_CUSTOMER_NCS */

	WL_TRACE(("wl%d: wl_sendup: %d bytes\n", wl->pub->unit, PKTLEN(wl->osh, p)));

#ifdef ARPOE
	/* Arp agent */
	if (ARPOE_ENAB(wl->pub)) {
		int err = wl_arp_recv_proc(wl->arpi, p);
		/* stop the arp-req pkt going to the host, offload has already handled it. */
		if ((err == ARP_REQ_SINK) || (err ==  ARP_REPLY_PEER)) {
			PKTFREE(wl->pub->osh, p, FALSE);
			return;
		}
	}
#endif

#ifdef TOE
	/* Apply TOE */
	if (TOE_ENAB(wl->pub))
		(void)wl_toe_recv_proc(wl->toei, p);
#endif

	/* route packet to the appropriate interface */
	if (wlif) {
		/* drop if the interface is not up yet */
		if (!netif_device_present(wlif->dev)) {
			WL_ERROR(("wl%d: wl_sendup: interface not ready\n", wl->pub->unit));
#ifndef WL_UMK
			PKTFREE(wl->osh, p, FALSE);
#else
			kfree_skb(skb);
#endif
			return;
		}
		/* Convert the packet, mainly detach the pkttag */
		skb = PKTTONATIVE(wl->osh, p);
		skb->dev = wlif->dev;
	} else {
		/* Convert the packet, mainly detach the pkttag */
		skb = PKTTONATIVE(wl->osh, p);
		skb->dev = wl->dev;
	}

#ifdef HNDCTF
	/* try cut thru' before sending up */
	if (wl_ctf_forward(wl, skb) != BCME_ERROR)
		return;
#endif /* HNDCTF */

#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	if ((wl->pub->fcache) && 
		(PKT_DONE==blog_sinit(skb, skb->dev, TYPE_ETH, 0, BLOG_WLANPHY))) {
	 	/* Doesnot need go to IP stack*/
		return;
	}
#endif

	skb->protocol = eth_type_trans(skb, skb->dev);
#if !defined(BCMWAPI_WPI) && !defined(WL_UMK)
	if (!ISALIGNED((uintptr)skb->data, 4)) {
		WL_ERROR(("Unaligned assert. skb %p. skb->data %p.\n", skb, skb->data));
		if (wlif) {
			WL_ERROR(("wl_sendup: dev name is %s (wlif) \n", wlif->dev->name));
			WL_ERROR(("wl_sendup: hard header len  %d (wlif) \n",
				wlif->dev->hard_header_len));
		}
		WL_ERROR(("wl_sendup: dev name is %s (wl) \n", wl->dev->name));
		WL_ERROR(("wl_sendup: hard header len %d (wl) \n", wl->dev->hard_header_len));
		ASSERT(ISALIGNED((uintptr)skb->data, 4));
	}
#endif /* !BCMWAPI_WPI && !WL_UMK */

	/* send it up */
	WL_APSTA_RX(("wl%d: wl_sendup(): pkt %p summed %d on interface %p (%s)\n",
		wl->pub->unit, p, skb->ip_summed, wlif, skb->dev->name));

#ifdef WL_UMK
	ret = netif_rx_ni(skb);
	switch (ret) {
	case NET_RX_CN_LOW:
	case NET_RX_CN_MOD:
	case NET_RX_CN_HIGH:
		WL_ERROR(("Sendup: Congestion\n"));
		break;
	case NET_RX_DROP:
		WL_ERROR(("Sendup: Packet dropped\n"));
		break;
	default:
		break;
	}
#else
#ifdef NAPI_POLL
	netif_receive_skb(skb);
#else /* NAPI_POLL */
#ifdef DSLCPE_NAPI
	if (NAPI_ENAB(wl->pub))
		netif_receive_skb(skb);
	else
#endif
	netif_rx(skb);
#endif /* NAPI_POLL */
#endif /* WL_UMK */
}

#ifndef WL_UMK
void
wl_dump_ver(wl_info_t *wl, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "wl%d: %s %s version %s\n", wl->pub->unit,
		__DATE__, __TIME__, EPI_VERSION_STR);
}
#endif /* WL_UMK */

#if defined(BCMDBG) && !defined(WL_UMK)
static int
wl_dump(wl_info_t *wl, struct bcmstrbuf *b)
{
	wl_if_t *p;
	int i;

	wl_dump_ver(wl, b);

	bcm_bprintf(b, "name %s dev %p tbusy %d callbacks %d malloced %d\n",
	       wl->dev->name, wl->dev, (uint)netif_queue_stopped(wl->dev),
	       atomic_read(&wl->callbacks), MALLOCED(wl->osh));

	/* list all interfaces, skipping the primary one since it is printed above */
	p = wl->if_list;
	if (p)
		p = p->next;
	for (i = 0; p != NULL; p = p->next, i++) {
		if ((i % 4) == 0) {
			if (i != 0)
				bcm_bprintf(b, "\n");
			bcm_bprintf(b, "Interfaces:");
		}
		bcm_bprintf(b, " name %s dev %p", p->dev->name, p->dev);
	}
	if (i)
		bcm_bprintf(b, "\n");

	return 0;
}
#endif	/* BCMDBG && !WL_UMK */

static void
wl_link_up(wl_info_t *wl, char *ifname)
{
#ifndef WL_UMK
	WL_ERROR(("wl%d: link up (%s)\n", wl->pub->unit, ifname));
#endif
}

static void
wl_link_down(wl_info_t *wl, char *ifname)
{
#ifndef WL_UMK
	WL_ERROR(("wl%d: link down (%s)\n", wl->pub->unit, ifname));
#endif
}

void
wl_event(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
#ifdef USE_IW
	wl_iw_event(wl->dev, &(e->event), e->data);
#endif /* USE_IW */

	switch (e->event.event_type) {
	case WLC_E_LINK:
	case WLC_E_NDIS_LINK:
		if (e->event.flags&WLC_EVENT_MSG_LINK)
			wl_link_up(wl, ifname);
		else
			wl_link_down(wl, ifname);
		break;
#if defined(BCMSUP_PSK) && defined(STA)
	case WLC_E_MIC_ERROR: {
		int err = 0;
		wlc_bsscfg_t *cfg = wlc_bsscfg_find(wl->wlc, e->bsscfgidx, &err);
		if (cfg == NULL)
			break;
		wl_mic_error(wl, cfg, e->addr,
			e->event.flags&WLC_EVENT_MSG_GROUP,
			e->event.flags&WLC_EVENT_MSG_FLUSHTXQ);
		break;
	}
#endif
#if defined(WL_CONFIG_RFKILL_INPUT)
	case WLC_E_RADIO: {
		mbool i;
		if (wlc_get(wl->wlc, WLC_GET_RADIO, &i) < 0)
			WL_ERROR(("%s: WLC_GET_RADIO failed\n", __FUNCTION__));
		if (wl->last_phyind == (mbool)(i & WL_RADIO_HW_DISABLE))
			break;

		wl->last_phyind = (mbool)(i & WL_RADIO_HW_DISABLE);

		WL_ERROR(("wl%d: Radio hardware state changed to %d\n", wl->pub->unit, i));
		(void) wl_schedule_task(wl, wl_force_kill, wl);
		break;
	}
#else
	case WLC_E_RADIO:
		break;
#endif /* WL_CONFIG_RFKILL_INPUT */
	}
}

void
wl_event_sync(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
}

#ifdef WLC_HIGH_ONLY

static void
wl_rpc_down(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)(wlh);

	wlc_device_removed(wl->wlc);

	wl_rpcq_free(wl);
}
#endif /* WLC__HIGH_ONLY */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
/* enqueue pkt to local queue, schedule a task to run, return this context */
static int
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
wl_start(pNBuff_t pNBuff, struct net_device *dev)
#else
wl_start(struct sk_buff *skb, struct net_device *dev)
#endif /* DSLCPE_BLOG && DSLCPE */
{
	wl_if_t *wlif;
	wl_info_t *wl;
	ulong flags;
#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	struct sk_buff *skb; 
#endif /* DSLCPE_BLOG && DSLCPE */

	wlif = WL_DEV_IF(dev);
	wl = wlif->wl;

#if defined(DSLCPE_BLOG) && defined(DSLCPE)
	skb = nbuff_xlate(pNBuff);
	if ( wl->pub->fcache ) { 
		blog_emit( skb, dev, TYPE_ETH, 0, BLOG_WLANPHY );
	}
#endif /* DSLCPE_BLOG && DSLCPE */

#if defined(DSLCPE_TX_DELAY) && defined(WLC_LOW)
	if ( !wl->enable_tx_delay)
		return wl_start_int(wl, wlif, skb);
#endif /* DSLCPE_TX_DELAY && WLC_LOW*/

	skb->prev = NULL;

	/* Lock the queue as tasklet could be running at this time */
	TXQ_LOCK(wl, flags);
	if (wl->txq_head == NULL)
		wl->txq_head = skb;
	else {
		wl->txq_tail->prev = skb;
	}
	wl->txq_tail = skb;

	if (wl->txq_dispatched == FALSE) {
		wl->txq_dispatched = TRUE;

		if (schedule_work(&wl->txq_task.work)) {
			atomic_inc(&wl->callbacks);
		} else {
			WL_ERROR(("wl%d: wl_start/schedule_work failed\n", wl->pub->unit));
		}
	}

	TXQ_UNLOCK(wl, flags);

	return (0);
}

static void
wl_start_txqwork(struct wl_task *task)
{
	wl_info_t *wl = (wl_info_t *)task->context;
	struct sk_buff *skb;
	ulong flags;
	uint count = 0;

	WL_TRACE(("wl%d: wl_start_txqwork\n", wl->pub->unit));

	/* First remove an entry then go for execution */
	TXQ_LOCK(wl, flags);
	while (wl->txq_head) {
		skb = wl->txq_head;
		wl->txq_head = skb->prev;
		skb->prev = NULL;
		if (wl->txq_head == NULL)
			wl->txq_tail = NULL;
		TXQ_UNLOCK(wl, flags);

		/* it has WL_LOCK/WL_UNLOCK inside */
		wl_start_int(wl, wl->if_list, skb);

		/* bounded our execution, reshedule ourself next */
#ifdef DSLCPE_TXRX_BOUND
		if (++count >= wl->tx_bound)
#else
		if (++count >= 10)
#endif
			break;

		TXQ_LOCK(wl, flags);
	}

#ifdef DSLCPE_TXRX_BOUND
	if (count >= wl->tx_bound) {
#else
	if (count >= 10) {
#endif
		if (!schedule_work(&wl->txq_task.work)) {
			WL_ERROR(("wl%d: wl_start/schedule_work failed\n", wl->pub->unit));
			atomic_dec(&wl->callbacks);
		}
	} else {
		wl->txq_dispatched = FALSE;
		TXQ_UNLOCK(wl, flags);
		atomic_dec(&wl->callbacks);
	}


	return;
}


static void
wl_txq_free(wl_info_t *wl)
{
	struct sk_buff *skb;

	if (wl->txq_head == NULL) {
		ASSERT(wl->txq_tail == NULL);
		return;
	}

	while (wl->txq_head) {
		skb = wl->txq_head;
		wl->txq_head = skb->prev;
		PKTFREE(wl->osh, skb, TRUE);
	}

	wl->txq_tail = NULL;
}
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */

#ifdef WLC_HIGH_ONLY
static void
wl_rpcq_free(wl_info_t *wl)
{
	rpc_buf_t *buf;

	if (wl->rpcq_head == NULL) {
		ASSERT(wl->rpcq_tail == NULL);
		return;
	}

	while (wl->rpcq_head) {
		buf = wl->rpcq_head;
		wl->rpcq_head = bcm_rpc_buf_next_get(wl->rpc_th, buf);
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		PKTFREE(wl->osh, buf, FALSE);
#else
		bcm_rpc_buf_free(wl->rpc_dispatch_ctx.rpc, buf);
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */
	}

	wl->rpcq_tail = NULL;
}

static void
wl_rpcq_dispatch(struct wl_task *task)
{
	wl_info_t * wl = (wl_info_t *)task->context;
	rpc_buf_t *buf;
	ulong flags;

	/* First remove an entry then go for execution */
	RPCQ_LOCK(wl, flags);
	while (wl->rpcq_head) {
		buf = wl->rpcq_head;
		wl->rpcq_head = bcm_rpc_buf_next_get(wl->rpc_th, buf);

		if (wl->rpcq_head == NULL)
			wl->rpcq_tail = NULL;
		RPCQ_UNLOCK(wl, flags);

		WL_LOCK(wl);
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY)
		/* In bcm_rpc_buf_recv_high(), we made pkts native before
		 * pushing into rpcq.
		 */
		PKTFRMNATIVE(wl->osh, buf);
#endif /* BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY */
		wlc_rpc_high_dispatch(&wl->rpc_dispatch_ctx, buf);
		WL_UNLOCK(wl);

		RPCQ_LOCK(wl, flags);
	}

	wl->rpcq_dispatched = FALSE;
	MFREE(wl->osh, task, sizeof(wl_task_t));
	RPCQ_UNLOCK(wl, flags);

	atomic_dec(&wl->callbacks);
}

static void
wl_rpcq_add(wl_info_t *wl, rpc_buf_t *buf)
{
	ulong flags;

	bcm_rpc_buf_next_set(wl->rpc_th, buf, NULL);

	/* Lock the queue as tasklet could be running at this time */
	RPCQ_LOCK(wl, flags);
	if (wl->rpcq_head == NULL)
		wl->rpcq_head = buf;
	else
		bcm_rpc_buf_next_set(wl->rpc_th, wl->rpcq_tail, buf);

	wl->rpcq_tail = buf;

	if (wl->rpcq_dispatched == FALSE) {
		wl->rpcq_dispatched = TRUE;
		wl_schedule_task(wl, wl_rpcq_dispatch, wl);
	}

	RPCQ_UNLOCK(wl, flags);
}

#if defined(BCMDBG) || defined(BCMDBG_ERR)
static const struct name_entry rpc_name_tbl[] = RPC_ID_TABLE;
#endif /* BCMDBG */

/* dongle-side rpc dispatch routine */
static void
wl_rpc_dispatch_schedule(void *ctx, struct rpc_buf* buf)
{
	bcm_xdr_buf_t b;
	wl_info_t *wl = (wl_info_t *)ctx;
	wlc_rpc_id_t rpc_id;
	int err;

	bcm_xdr_buf_init(&b, bcm_rpc_buf_data(wl->rpc_th, buf),
	                 bcm_rpc_buf_len_get(wl->rpc_th, buf));

	err = bcm_xdr_unpack_uint32(&b, &rpc_id);
	ASSERT(!err);
	WL_TRACE(("%s: Dispatch id %s\n", __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, rpc_id)));

	/* Handle few emergency ones */
	switch (rpc_id) {
	default:
		wl_rpcq_add(wl, buf);
		break;
	}
}

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY)
static void
wl_rpc_txdone(void *ctx, struct rpc_buf* buf)
{
	bcm_xdr_buf_t b;
	wlc_rpc_id_t rpc_id;
	int err;
	wl_info_t *wl = (wl_info_t *)ctx;

	bcm_xdr_buf_init(&b, bcm_rpc_buf_data(wl->rpc_th, buf),
	                 bcm_rpc_buf_len_get(wl->rpc_th, buf));
	err = bcm_xdr_unpack_uint32(&b, &rpc_id);
	ASSERT(!err);
	WL_MBSS(("%s:  id %s\n", __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, rpc_id)));

	switch (rpc_id) {
	case WLRPC_WLC_BMAC_TXFIFO_ID :
		/* pull back the original packet to begin of body */
		bcm_rpc_buf_pull(wl->rpc_th, buf, WLC_RPCTX_PARAMS);
		break;
	default:
		bcm_rpc_tp_buf_free(wl->rpc_th, buf);
		break;
	}
}
#endif /* defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) */

#endif /* WLC_HIGH_ONLY */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
static void
wl_set_multicast_list_workitem(struct work_struct *work)
{
	wl_task_t *task = (wl_task_t *)work;
	struct net_device *dev = (struct net_device*)task->context;
	wl_info_t *wl;

	wl = WL_INFO(dev);

	atomic_dec(&wl->callbacks);

	_wl_set_multicast_list(dev);
}
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */
 
#if defined(WLC_HIGH_ONLY)
static void
wl_timer_task(wl_task_t *task)
{
	wl_timer_t *t = (wl_timer_t *)task->context;

	_wl_timer(t);
	MFREE(t->wl->osh, task, sizeof(wl_task_t));

	/* This dec is for the task_schedule. The timer related
	 * callback is decremented in _wl_timer
	 */
	atomic_dec(&t->wl->callbacks);
}
#endif /* WLC_HIGH_ONLY */

#ifndef WL_UMK
static void
wl_timer(ulong data)
{
#ifndef WLC_HIGH_ONLY
	_wl_timer((wl_timer_t*)data);
#else
	wl_timer_t *t = (wl_timer_t *)data;
	wl_schedule_task(t->wl, wl_timer_task, t);
#endif /* WLC_HIGH_ONLY */
}

static void
_wl_timer(wl_timer_t *t)
{
	WL_LOCK(t->wl);

	if (t->set) {
#if defined(DSLCPE_DELAY)
		bool run_timer = (IN_LONG_DELAY(t->wl->osh) ? FALSE: TRUE);
#endif
		if (t->periodic) {
#if defined(BCMJTAG) || defined(BCMSLTGT)
			t->timer.expires = jiffies + t->ms*HZ/1000*htclkratio;
#else
			t->timer.expires = jiffies + t->ms*HZ/1000;
#endif
			atomic_inc(&t->wl->callbacks);
			add_timer(&t->timer);
			t->set = TRUE;
		} else
			t->set = FALSE;

#if defined(DSLCPE_DELAY)
		if (!run_timer) {
			if (!t->periodic) {
				wl_add_timer(t->wl, t, t->ms, t->periodic);
			}
		}
		else
#endif
		t->fn(t->arg);
	}

	atomic_dec(&t->wl->callbacks);

	WL_UNLOCK(t->wl);
}

wl_timer_t *
wl_init_timer(wl_info_t *wl, void (*fn)(void *arg), void *arg, const char *name)
{
	wl_timer_t *t;

	if (!(t = MALLOC(wl->osh, sizeof(wl_timer_t)))) {
		WL_ERROR(("wl%d: wl_init_timer: out of memory, malloced %d bytes\n", wl->pub->unit,
			MALLOCED(wl->osh)));
		return 0;
	}

	bzero(t, sizeof(wl_timer_t));

	init_timer(&t->timer);
	t->timer.data = (ulong) t;
	t->timer.function = wl_timer;
	t->wl = wl;
	t->fn = fn;
	t->arg = arg;
	t->next = wl->timers;
	wl->timers = t;

#ifdef BCMDBG
	if ((t->name = MALLOC(wl->osh, strlen(name) + 1)))
		strcpy(t->name, name);
#endif

	return t;
}

/* BMAC_NOTE: Add timer adds only the kernel timer since it's going to be more accurate
 * as well as it's easier to make it periodic
 */
void
wl_add_timer(wl_info_t *wl, wl_timer_t *t, uint ms, int periodic)
{
	ASSERT(!t->set);

	t->ms = ms;
	t->periodic = (bool) periodic;
	t->set = TRUE;
#if defined(BCMJTAG) || defined(BCMSLTGT)
	t->timer.expires = jiffies + ms*HZ/1000*htclkratio;
#else
	t->timer.expires = jiffies + ms*HZ/1000;
#endif /* defined(BCMJTAG) || defined(BCMSLTGT) */

	atomic_inc(&wl->callbacks);
	add_timer(&t->timer);
}

/* return TRUE if timer successfully deleted, FALSE if still pending */
bool
wl_del_timer(wl_info_t *wl, wl_timer_t *t)
{
	if (t->set) {
		t->set = FALSE;
		if (!del_timer(&t->timer)) {
#ifdef BCMDBG
			WL_INFORM(("wl%d: Failed to delete timer %s\n", wl->pub->unit, t->name));
#endif
			return FALSE;
		}
		atomic_dec(&wl->callbacks);
	}

	return TRUE;
}

void
wl_free_timer(wl_info_t *wl, wl_timer_t *t)
{
	wl_timer_t *tmp;

	/* delete the timer in case it is active */
	wl_del_timer(wl, t);

	if (wl->timers == t) {
		wl->timers = wl->timers->next;
#ifdef BCMDBG
		if (t->name)
			MFREE(wl->osh, t->name, strlen(t->name) + 1);
#endif
		MFREE(wl->osh, t, sizeof(wl_timer_t));
		return;

	}

	tmp = wl->timers;
	while (tmp) {
		if (tmp->next == t) {
			tmp->next = t->next;
#ifdef BCMDBG
			if (t->name)
				MFREE(wl->osh, t->name, strlen(t->name) + 1);
#endif
			MFREE(wl->osh, t, sizeof(wl_timer_t));
			return;
		}
		tmp = tmp->next;
	}

}
#endif /* WL_UMK */


#if defined(DSLCPE_WORKQUEUE_TASK)
/* Following defines low priority task wrapper functions */
void wl_workqueue_task_fn( struct wl_task *task)
{
	WL_LOCK(task->wl);

	task->set = FALSE;
	atomic_dec(&task->wl->callbacks);
	task->fn(task->context);
	WL_UNLOCK(task->wl);
}

struct wl_task *wl_init_workqueue_task(struct wl_info *wl, void (*fn)(void *arg), void *context)
{
	struct wl_task *task;
	
	if (!(task = MALLOC(wl->osh, sizeof(wl_task_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wl->pub->unit,
			__FUNCTION__,  MALLOCED(wl->osh)));
		return NULL;
	}

	bzero(task, sizeof(wl_task_t));

	INIT_WORK((struct work_struct *)task, (work_func_t)wl_workqueue_task_fn);
	task->context = context;
	task->fn = fn;
	task->wl = wl;
	task->set = FALSE;
	return task;
}

void wl_schedule_workqueue_task(struct wl_task *task)
{
	task->set = TRUE;
	schedule_work((struct work_struct *)task);
	atomic_inc(&task->wl->callbacks);
}


bool wl_cancel_workqueue_task(struct wl_task *task)
{
	if ( task && task->set ) {
		flush_scheduled_work();
		task->set = FALSE;
		atomic_dec(&task->wl->callbacks);
		return TRUE;
	}
	return TRUE;
}

bool wl_free_workqueue_task(struct wl_info *wl, struct wl_task *task)
{
	if ( wl_cancel_workqueue_task(task) ) {
		MFREE(wl->osh, task, sizeof(struct wl_task));
		return TRUE;
	}
	else {
		WL_ERROR(("wl%d: %s: Err: Couldnot free task!\n", wl->pub->unit, __FUNCTION__));
		return FALSE;
	}
}
#endif  /* DSLCPE_WORKQUEUE_TASK */

#if defined(DSLCPE) && defined(DSLCPE_DELAYTASK)
/* Following defines delayed task wrapper functions */

/* delayed task wrapper */
void wl_delayed_task_fn( struct wl_delayed_task *task)
{
	WL_LOCK(task->wl);

	task->set = FALSE;
	atomic_dec(&task->wl->callbacks);
	task->fn(task->context);
	WL_UNLOCK(task->wl);
}

struct wl_delayed_task *wl_init_delayed_task(struct wl_info *wl, void (*fn)(void *arg), void *context)
{
	struct wl_delayed_task *task;
	
	if (!(task = MALLOC(wl->osh, sizeof(wl_delayed_task_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wl->pub->unit,
			__FUNCTION__,  MALLOCED(wl->osh)));
		return NULL;
	}

	bzero(task, sizeof(wl_delayed_task_t));

	INIT_DELAYED_WORK((struct delayed_work *)task, (work_func_t)wl_delayed_task_fn);
	task->context = context;
	task->fn = fn;
	task->wl = wl;
	task->set = FALSE;
	return task;
}

void wl_schedule_delayed_task(struct wl_delayed_task *task, unsigned long delay)
{
	task->set = TRUE;
	schedule_delayed_work((struct delayed_work *)task, delay);
	atomic_inc(&task->wl->callbacks);
}


bool wl_cancel_delayed_task(struct wl_delayed_task *task)
{
	if ( task && task->set ) {
		if (cancel_delayed_work((struct delayed_work *)task)) {
			task->set = FALSE;
			atomic_dec(&task->wl->callbacks);
			return TRUE;
		}
		else {
			WL_ERROR(("wl%d: %s: Err: Couldnot cancel delayed task!\n", task->wl->pub->unit, __FUNCTION__));
			return FALSE;
		}
	}
	return TRUE;
}

bool wl_free_delayed_task(struct wl_info *wl, struct wl_delayed_task *task)
{
	if ( wl_cancel_delayed_task(task) ) {
		MFREE(wl->osh, task, sizeof(wl_delayed_task_t));
		return TRUE;
	}
	else {
		WL_ERROR(("wl%d: %s: Err: Couldnot free delayed task!\n", wl->pub->unit, __FUNCTION__));
		return FALSE;
	}
}
#endif /* DSLCPE && DSLCPE_DELAYTASK */

#if defined(BCMSUP_PSK) && defined(STA)
static void
wl_mic_error(wl_info_t *wl, wlc_bsscfg_t *cfg, struct ether_addr *ea, bool group, bool flush_txq)
{
	WL_WSEC(("wl%d: mic error using %s key\n", wl->pub->unit,
		(group) ? "group" : "pairwise"));

	if (wlc_sup_mic_error(cfg, group))
		return;
}
#endif /* defined(BCMSUP_PSK) && defined(STA) */

void
wl_monitor(wl_info_t *wl, wl_rxsts_t *rxsts, void *p)
{
#ifdef WL_MONITOR
	struct sk_buff *oskb = (struct sk_buff *)p;
	struct sk_buff *skb;
	uchar *pdata;
	uint len;

	len = 0;
	skb = NULL;
	WL_TRACE(("wl%d: wl_monitor\n", wl->pub->unit));

	if (!wl->monitor_dev)
		return;

	if (wl->monitor_type == ARPHRD_IEEE80211_PRISM) {
		p80211msg_t *phdr;

		len = sizeof(p80211msg_t) + oskb->len - D11_PHY_HDR_LEN;
		if ((skb = dev_alloc_skb(len)) == NULL)
			return;

		skb_put(skb, len);
		phdr = (p80211msg_t*)skb->data;
		/* Initialize the message members */
		phdr->msgcode = WL_MON_FRAME;
		phdr->msglen = sizeof(p80211msg_t);
		strcpy(phdr->devname, wl->dev->name);

		phdr->hosttime.did = WL_MON_FRAME_HOSTTIME;
		phdr->hosttime.status = P80211ITEM_OK;
		phdr->hosttime.len = 4;
		phdr->hosttime.data = jiffies;

		phdr->channel.did = WL_MON_FRAME_CHANNEL;
		phdr->channel.status = P80211ITEM_NO_VALUE;
		phdr->channel.len = 4;
		phdr->channel.data = 0;

		phdr->signal.did = WL_MON_FRAME_SIGNAL;
		phdr->signal.status = P80211ITEM_OK;
		phdr->signal.len = 4;
		/* two sets of preamble values are defined in wlc_ethereal and wlc_pub.h
		 *   and this assumet their values are matched. Otherwise,
		 * we have to go through conversion, which requires rspec since datarate
		 * is just kbps now
		 */
		phdr->signal.data = rxsts->preamble;

		phdr->noise.did = WL_MON_FRAME_NOISE;
		phdr->noise.status = P80211ITEM_NO_VALUE;
		phdr->noise.len = 4;
		phdr->noise.data = 0;

		phdr->rate.did = WL_MON_FRAME_RATE;
		phdr->rate.status = P80211ITEM_OK;
		phdr->rate.len = 4;
		phdr->rate.data = rxsts->datarate;

		phdr->istx.did = WL_MON_FRAME_ISTX;
		phdr->istx.status = P80211ITEM_NO_VALUE;
		phdr->istx.len = 4;
		phdr->istx.data = 0;

		phdr->mactime.did = WL_MON_FRAME_MACTIME;
		phdr->mactime.status = P80211ITEM_OK;
		phdr->mactime.len = 4;
		phdr->mactime.data = rxsts->mactime;

		phdr->rssi.did = WL_MON_FRAME_RSSI;
		phdr->rssi.status = P80211ITEM_OK;
		phdr->rssi.len = 4;
		phdr->rssi.data = rxsts->signal;		/* to dbm */

		phdr->sq.did = WL_MON_FRAME_SQ;
		phdr->sq.status = P80211ITEM_OK;
		phdr->sq.len = 4;
		phdr->sq.data = rxsts->sq;

		phdr->frmlen.did = WL_MON_FRAME_FRMLEN;
		phdr->frmlen.status = P80211ITEM_OK;
		phdr->frmlen.status = P80211ITEM_OK;
		phdr->frmlen.len = 4;
		phdr->frmlen.data = rxsts->pktlength;

		pdata = skb->data + sizeof(p80211msg_t);
		bcopy(oskb->data + D11_PHY_HDR_LEN, pdata, oskb->len - D11_PHY_HDR_LEN);

	} else if (wl->monitor_type == ARPHRD_IEEE80211_RADIOTAP) {

		int channel_frequency;
		uint16 channel_flags;
		struct wl_radiotap_header *bsd_header;
		struct dot11_header * mac_header;
		uint16 fc;

		len = sizeof(struct wl_radiotap_header) + oskb->len - D11_PHY_HDR_LEN;
		if ((skb = dev_alloc_skb(len)) == NULL)
			return;

		skb_put(skb, len);

		bsd_header = (struct wl_radiotap_header *)(skb->data);

		/* Initialize the message members */
		bsd_header->ieee_radiotap.it_version = 0;
		bsd_header->ieee_radiotap.it_pad = 0;
		bsd_header->ieee_radiotap.it_len = htol16(sizeof(struct wl_radiotap_header));
		bsd_header->ieee_radiotap.it_present = htol32(WL_RADIOTAP_PRESENT);

		bsd_header->tsft_l = htol32(rxsts->mactime);

		if (rxsts->channel <= CH_MAX_2G_CHANNEL) {
			channel_flags = IEEE80211_CHAN_2GHZ;
			channel_frequency = wf_channel2mhz(rxsts->channel, WF_CHAN_FACTOR_2_4_G);
		} else {
			channel_flags = IEEE80211_CHAN_5GHZ;
			channel_frequency = wf_channel2mhz(rxsts->channel, WF_CHAN_FACTOR_5_G);
		}

		bsd_header->channel_freq = htol16(channel_frequency);
		bsd_header->channel_flags = htol16(channel_flags);

		if (rxsts->preamble == WL_RXS_PREAMBLE_SHORT)
			bsd_header->flags |= IEEE80211_RADIOTAP_F_SHORTPRE;

		mac_header = (struct dot11_header *)(oskb->data + D11_PHY_HDR_LEN);

		fc = ntoh16(mac_header->fc);
		if (fc & (FC_WEP >> FC_WEP_SHIFT))
			bsd_header->flags |= IEEE80211_RADIOTAP_F_WEP;

		if (fc & (FC_MOREFRAG >> FC_MOREFRAG_SHIFT))
			bsd_header->flags |= IEEE80211_RADIOTAP_F_FRAG;

		if (rxsts->pkterror & WL_RXS_CRC_ERROR)
			bsd_header->flags |= IEEE80211_RADIOTAP_F_BADFCS;

		bsd_header->flags |= IEEE80211_RADIOTAP_F_FCS;

		bsd_header->rate = rxsts->datarate;
		bsd_header->signal = (int8)rxsts->signal;
		bsd_header->noise = (int8)rxsts->noise;
		bsd_header->antenna = rxsts->antenna;

		pdata = skb->data + sizeof(struct wl_radiotap_header);
		bcopy(oskb->data + D11_PHY_HDR_LEN, pdata, oskb->len - D11_PHY_HDR_LEN);
	}

	skb->dev = wl->monitor_dev;
	skb->dev->last_rx = jiffies;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 22)
	skb_reset_mac_header(skb);
#else
	skb->mac.raw = skb->data;
#endif
	skb->ip_summed = CHECKSUM_NONE;
	skb->pkt_type = PACKET_OTHERHOST;
	skb->protocol = htons(ETH_P_80211_RAW);

#ifdef NAPI_POLL
	netif_receive_skb(skb);
#else /* NAPI_POLL */
	netif_rx(skb);
#endif /* NAPI_POLL */
#endif /* WL_MONITOR */
}

#ifdef WL_MONITOR
static int
wl_monitor_start(struct sk_buff *skb, struct net_device *dev)
{
#ifdef DSLCPE
	dev_kfree_skb_any(skb);
#else
	wl_info_t *wl;

	wl = WL_DEV_IF(dev)->wl;
	PKTFREE(wl->osh, skb, FALSE);
#endif /* DSLCPE */
	return 0;
}

/* Create a virtual interface. Call only from safe time! can't call register_netdev with WL_LOCK */
static void
wl_add_monitor(wl_task_t *task)
{
	wl_info_t *wl = (wl_info_t *) task->context;
	struct net_device *dev;
	wl_if_t *wlif;

	ASSERT(wl);
	WL_LOCK(wl);
	WL_TRACE(("wl%d: wl_add_monitor\n", wl->pub->unit));

	if (wl->monitor_dev)
		goto done;

	wlif = wl_alloc_if(wl, WL_IFTYPE_MON, wl->pub->unit, NULL);
	if (!wlif) {
		WL_ERROR(("wl%d: wl_add_monitor: alloc wlif failed\n", wl->pub->unit));
		goto done;
	}

	/* link it all up */
	dev = wlif->dev;
	wl->monitor_dev = dev;

	/* override some fields */
	bcopy(wl->dev->dev_addr, dev->dev_addr, ETHER_ADDR_LEN);
	dev->flags = wl->dev->flags;
	dev->type = wl->monitor_type;
	if (wl->monitor_type == ARPHRD_IEEE80211_PRISM) {
		sprintf(dev->name, "prism%d", wl->pub->unit);
	} else {
		sprintf(dev->name, "radiotap%d", wl->pub->unit);
	}

	/* initialize dev fn pointers */
#if defined(WL_USE_NETDEV_OPS)
	dev->netdev_ops = &wl_netdev_monitor_ops;
#else
	dev->hard_start_xmit = wl_monitor_start;
	dev->do_ioctl = wl_ioctl;
	dev->get_stats = wl_get_stats;
#endif /* WL_USE_NETDEV_OPS */
#ifdef NAPI_POLL
	dev->poll = wl_poll;
	dev->weight = 64;
#endif /* NAPI_POLL */

	WL_UNLOCK(wl);
	if (register_netdev(dev)) {
		WL_ERROR(("wl%d: wl_add_monitor, register_netdev failed for \"%s\"\n",
			wl->pub->unit, wl->monitor_dev->name));
		wl->monitor_dev = NULL;
		wl_free_if(wl, wlif);
	} else
		wlif->dev_registed = TRUE;
	WL_LOCK(wl);
done:
	MFREE(wl->osh, task, sizeof(wl_task_t));
	atomic_dec(&wl->callbacks);
	WL_UNLOCK(wl);
}

static void
wl_del_monitor(wl_task_t *task)
{
	wl_info_t *wl = (wl_info_t *) task->context;
	struct net_device *dev;
	wl_if_t *wlif = NULL;

	ASSERT(wl);
	WL_LOCK(wl);
	WL_TRACE(("wl%d: wl_del_monitor\n", wl->pub->unit));
	dev = wl->monitor_dev;
	if (!dev)
		goto done;
	wlif = WL_DEV_IF(dev);
	ASSERT(wlif);
	wl->monitor_dev = NULL;
	WL_UNLOCK(wl);
	wl_free_if(wl, wlif);
	WL_LOCK(wl);
done:
	MFREE(wl->osh, task, sizeof(wl_task_t));
	WL_UNLOCK(wl);
	atomic_dec(&wl->callbacks);
}
#endif /* WL_MONITOR */

/*
 * Create a dedicated monitor interface since libpcap caches the
 * packet type when it opens the device. The protocol type in the skb
 * is dropped somewhere in libpcap, and every received frame is tagged
 * with the DLT/ARPHRD type that's read by libpcap when the device is
 * opened.
 *
 * If libpcap was fixed to handle per-packet link types, we might not
 * need to create a pseudo device at all, wl_set_monitor() would be
 * unnecessary, and wlc->monitor could just get set in wlc_ioctl().
 */
void
wl_set_monitor(wl_info_t *wl, int val)
{
#ifdef WL_MONITOR
	WL_TRACE(("wl%d: wl_set_monitor: val %d\n", wl->pub->unit, val));

	if (val && !wl->monitor_dev) {
		if (val == 1)
			wl->monitor_type = ARPHRD_IEEE80211_PRISM;
		else if (val == 2)
			wl->monitor_type = ARPHRD_IEEE80211_RADIOTAP;
		else {
			WL_ERROR(("monitor type %d not supported\n", val));
			ASSERT(0);
		}

		(void) wl_schedule_task(wl, wl_add_monitor, wl);
	} else if (!val && wl->monitor_dev) {
		(void) wl_schedule_task(wl, wl_del_monitor, wl);
	}
#endif /* WL_MONITOR */
}

#if LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 15)
const char *
print_tainted()
{
	return "";
}
#endif	/* LINUX_VERSION_CODE == KERNEL_VERSION(2, 6, 15) */


#ifdef BCMJTAG
/* attach to d11 core thru jtag */
/* venid and devid are pci vendor id and pci device id */
static void *
wl_jtag_probe(uint16 venid, uint16 devid, void *regsva, void *param)
{
	wl_info_t *wl;

	if (!wlc_chipmatch(venid, devid)) {
		WL_ERROR(("wl_jtag_probe: wlc_chipmatch failed\n"));
		return NULL;
	}

	if (!(wl = wl_attach(venid, devid, (ulong)regsva, JTAG_BUS, param, 0))) {
		WL_ERROR(("wl_jtag_probe: wl_attach failed\n"));
		return NULL;
	}

	return wl;
}

/* detach from d11 core */
static void
wl_jtag_detach(void *wl)
{
	WL_LOCK((wl_info_t *)wl);
	wl_down((wl_info_t *)wl);
	WL_UNLOCK((wl_info_t *)wl);
	wl_free((wl_info_t *)wl);
}

/* poll d11 core */
static void
wl_jtag_poll(void *wl)
{
	WL_ISR(0, (wl_info_t *)wl, NULL);
}
#endif /* BCMJTAG */


struct net_device *
wl_netdev_get(wl_info_t *wl)
{
	return wl->dev;
}

#ifdef BCM_WL_EMULATOR

/* create an empty wl_info structure to be used by the emulator */
wl_info_t *
wl_wlcreate(osl_t *osh, void *pdev)
{
	wl_info_t *wl;

	/* allocate private info */
	if ((wl = (wl_info_t*) MALLOC(osh, sizeof(wl_info_t))) == NULL) {
		WL_ERROR(("wl%d: malloc wl_info_t, out of memory, malloced %d bytes\n", 0,
		          MALLOCED(osh)));
		osl_detach(osh);
		return NULL;
	}
	bzero(wl, sizeof(wl_info_t));
	wl->dev = pdev;
	wl->osh = osh;
	return wl;
}

void * wl_getdev(void *w)
{
	wl_info_t *wl = (wl_info_t *)w;
	return wl->dev;
}
#endif /* BCM_WL_EMULATOR */

/* Linux: no chaining */
int
wl_set_pktlen(osl_t *osh, void *p, int len)
{
	PKTSETLEN(osh, p, len);
	return len;
}
/* Linux: no chaining */
void *
wl_get_pktbuffer(osl_t *osh, int len)
{
	return (PKTGET(osh, len, FALSE));
}

/* Linux version: no chains */
uint
wl_buf_to_pktcopy(osl_t *osh, void *p, uchar *buf, int len, uint offset)
{
	if (PKTLEN(osh, p) < len + offset)
		return 0;
	bcopy(buf, (char *)PKTDATA(osh, p) + offset, len);
	return len;
}

#ifdef LINUX_CRYPTO
int
wl_tkip_miccheck(wl_info_t *wl, void *p, int hdr_len, bool group_key, int key_index)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	struct sk_buff *skb = (struct sk_buff *)p;
	skb->dev = wl->dev;

	if (wl->tkipmodops) {
		if (group_key && wl->tkip_bcast_data)
			return (wl->tkipmodops->decrypt_msdu(skb, key_index, hdr_len,
				wl->tkip_bcast_data));
		else if (!group_key && wl->tkip_ucast_data)
			return (wl->tkipmodops->decrypt_msdu(skb, key_index, hdr_len,
				wl->tkip_ucast_data));
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
	WL_ERROR(("%s: No tkip mod ops\n", __FUNCTION__));
	return -1;

}

int
wl_tkip_micadd(wl_info_t *wl, void *p, int hdr_len)
{
	int error = -1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	struct sk_buff *skb = (struct sk_buff *)p;
	skb->dev = wl->dev;

	if (wl->tkipmodops) {
		if (wl->tkip_ucast_data)
			error = wl->tkipmodops->encrypt_msdu(skb, hdr_len, wl->tkip_ucast_data);
		if (error)
			WL_ERROR(("Error encrypting MSDU %d\n", error));
	}
	else
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
		WL_ERROR(("%s: No tkip mod ops\n", __FUNCTION__));
	return error;
}

int
wl_tkip_encrypt(wl_info_t *wl, void *p, int hdr_len)
{
	int error = -1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	struct sk_buff *skb = (struct sk_buff *)p;
	skb->dev = wl->dev;

	if (wl->tkipmodops) {
		if (wl->tkip_ucast_data)
			error = wl->tkipmodops->encrypt_mpdu(skb, hdr_len, wl->tkip_ucast_data);
		if (error) {
			WL_ERROR(("Error encrypting MPDU %d\n", error));
		}
	}
	else
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
		WL_ERROR(("%s: No tkip mod ops\n", __FUNCTION__));
	return error;

}

int
wl_tkip_decrypt(wl_info_t *wl, void *p, int hdr_len, bool group_key)
{
	int err = -1;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	struct sk_buff *skb = (struct sk_buff *)p;
	skb->dev = wl->dev;

	if (wl->tkipmodops) {
		if (group_key && wl->tkip_bcast_data)
			err = wl->tkipmodops->decrypt_mpdu(skb, hdr_len, wl->tkip_bcast_data);
		else if (!group_key && wl->tkip_ucast_data)
			err = wl->tkipmodops->decrypt_mpdu(skb, hdr_len, wl->tkip_ucast_data);
	}
	else
		WL_ERROR(("%s: No tkip mod ops\n", __FUNCTION__));

#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */

	/* Error */
	return err;
}


int
wl_tkip_keyset(wl_info_t *wl, wsec_key_t *key)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	bool group_key = FALSE;
	uchar	rxseq[IW_ENCODE_SEQ_MAX_SIZE];

	if (key->len != 0) {
		WL_WSEC(("%s: Key Length is Not zero\n", __FUNCTION__));
		if (key->algo != CRYPTO_ALGO_TKIP) {
			WL_WSEC(("%s: Algo is Not TKIP %d\n", __FUNCTION__, key->algo));
			return 0;
		}
		WL_WSEC(("%s: Trying to set a key in TKIP Mod\n", __FUNCTION__));
	}
	else
		WL_WSEC(("%s: Trying to Remove a Key from TKIP Mod\n", __FUNCTION__));

	if (ETHER_ISNULLADDR(&key->ea) || ETHER_ISBCAST(&key->ea)) {
		group_key = TRUE;
		WL_WSEC(("Group Key index %d\n", key->id));
	}
	else
		WL_WSEC(("Unicast Key index %d\n", key->id));

	if (wl->tkipmodops) {
		uint8 keybuf[8];
		if (group_key) {
			if (key->len) {
				if (!wl->tkip_bcast_data) {
					WL_WSEC(("Init TKIP Bcast Module\n"));
					WL_UNLOCK(wl);
					wl->tkip_bcast_data = wl->tkipmodops->init(key->id);
					WL_LOCK(wl);
				}
				if (wl->tkip_bcast_data) {
					WL_WSEC(("TKIP SET BROADCAST KEY******************\n"));
					bzero(rxseq, IW_ENCODE_SEQ_MAX_SIZE);
					bcopy(&key->rxiv, rxseq, 6);
					bcopy(&key->data[24], keybuf, sizeof(keybuf));
					bcopy(&key->data[16], &key->data[24], sizeof(keybuf));
					bcopy(keybuf, &key->data[16], sizeof(keybuf));
					wl->tkipmodops->set_key(&key->data, key->len,
						(uint8 *)&key->rxiv, wl->tkip_bcast_data);
				}
			}
			else {
				if (wl->tkip_bcast_data) {
					WL_WSEC(("Deinit TKIP Bcast Module\n"));
					wl->tkipmodops->deinit(wl->tkip_bcast_data);
					wl->tkip_bcast_data = NULL;
				}
			}
		}
		else {
			if (key->len) {
				if (!wl->tkip_ucast_data) {
					WL_WSEC(("Init TKIP Ucast Module\n"));
					WL_UNLOCK(wl);
					wl->tkip_ucast_data = wl->tkipmodops->init(key->id);
					WL_LOCK(wl);
				}
				if (wl->tkip_ucast_data) {
					WL_WSEC(("TKIP SET UNICAST KEY******************\n"));
					bzero(rxseq, IW_ENCODE_SEQ_MAX_SIZE);
					bcopy(&key->rxiv, rxseq, 6);
					bcopy(&key->data[24], keybuf, sizeof(keybuf));
					bcopy(&key->data[16], &key->data[24], sizeof(keybuf));
					bcopy(keybuf, &key->data[16], sizeof(keybuf));
					wl->tkipmodops->set_key(&key->data, key->len,
						(uint8 *)&key->rxiv, wl->tkip_ucast_data);
				}
			}
			else {
				if (wl->tkip_ucast_data) {
					WL_WSEC(("Deinit TKIP Ucast Module\n"));
					wl->tkipmodops->deinit(wl->tkip_ucast_data);
					wl->tkip_ucast_data = NULL;
				}
			}
		}
	}
	else
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
		WL_WSEC(("%s: No tkip mod ops\n", __FUNCTION__));
	return 0;
}

void
wl_tkip_printstats(wl_info_t *wl, bool group_key)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
	char debug_buf[512];
	if (wl->tkipmodops) {
		if (group_key && wl->tkip_bcast_data)
			wl->tkipmodops->print_stats(debug_buf, wl->tkip_bcast_data);
		else if (!group_key && wl->tkip_ucast_data)
			wl->tkipmodops->print_stats(debug_buf, wl->tkip_ucast_data);
		else
			return;
		printk("%s: TKIP stats from module: %s\n", debug_buf, group_key?"Bcast":"Ucast");
	}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
}

#endif /* LINUX_CRYPTO */


#ifndef WL_UMK

#if defined(WL_CONFIG_RFKILL_INPUT)
/* Rfkill support */
static int
wl_sw_toggle_radio(void *data, enum rfkill_state state)
{
	wl_info_t *wl = data;
	mbool cur_state;
	uint32 radioval = WL_RADIO_SW_DISABLE << 16;

	if (wlc_get(wl->wlc, WLC_GET_RADIO, &cur_state) < 0) {
		WL_ERROR(("%s: WLC_GET_RADIO failed\n", __FUNCTION__));
	}

	switch (state) {
	case RFKILL_STATE_SOFT_BLOCKED:
		/* Turn radio off */
		if (!mboolisset(cur_state, WL_RADIO_SW_DISABLE)) {
			WL_ERROR(("%s: => Disable radio\n", __FUNCTION__));
		}
		return 0;
	case RFKILL_STATE_UNBLOCKED:
		/* Turn radio on */
		if (mboolisset(cur_state, WL_RADIO_SW_DISABLE)) {
			radioval |= WL_RADIO_SW_DISABLE;
			WL_ERROR(("%s: => Enable radio\n", __FUNCTION__));
		}
		return 0;
	default:
		return -EINVAL;
	}
	if (wlc_set(wl->wlc, WLC_SET_RADIO, radioval) < 0)
			WL_ERROR(("%s: SET_RADIO failed\n", __FUNCTION__));
}

static int
wl_init_rfkill(wl_info_t *wl)
{
	int status;
	wl->wl_rfkill.rfkill = rfkill_allocate(&wl->dev->dev, RFKILL_TYPE_WLAN);
	if (!wl->wl_rfkill.rfkill) {
		WL_ERROR(("%s: RFKILL: Failed to allocate rfkill\n", __FUNCTION__));
		return -ENOMEM;
	}

	snprintf(wl->wl_rfkill.rfkill_name, sizeof(wl->wl_rfkill.rfkill_name),
		"brcmwl-%d", wl->pub->unit);

	if (wlc_get(wl->wlc, WLC_GET_RADIO, &status) < 0)
		WL_ERROR(("%s: WLC_GET_RADIO failed\n", __FUNCTION__));

	wl->wl_rfkill.rfkill->name = wl->wl_rfkill.rfkill_name;
	wl->wl_rfkill.rfkill->data = wl;
	wl->wl_rfkill.rfkill->toggle_radio = wl_sw_toggle_radio;

	if (status & WL_RADIO_HW_DISABLE)
		wl->wl_rfkill.rfkill->state = RFKILL_STATE_HARD_BLOCKED;
	else
		wl->wl_rfkill.rfkill->state = RFKILL_STATE_UNBLOCKED;

	wl->wl_rfkill.rfkill->user_claim_unsupported = 1;

	if (rfkill_register(wl->wl_rfkill.rfkill)) {
		WL_ERROR(("%s: rfkill_register failed! \n", __FUNCTION__));
		rfkill_free(wl->wl_rfkill.rfkill);
	}
	wl->wl_rfkill.registered = TRUE;
	return 0;
}

static void
wl_uninit_rfkill(wl_info_t *wl)
{
	if (wl->wl_rfkill.registered) {
		rfkill_unregister(wl->wl_rfkill.rfkill);
		wl->wl_rfkill.registered = FALSE;
		wl->wl_rfkill.rfkill = NULL;
	}
}

static void
wl_force_kill(wl_task_t *task)
{
	wl_info_t *wl = (wl_info_t *) task->context;

	ASSERT(wl);
	if (wl->last_phyind)
		rfkill_force_state(wl->wl_rfkill.rfkill, RFKILL_STATE_HARD_BLOCKED);
	else
		rfkill_force_state(wl->wl_rfkill.rfkill, RFKILL_STATE_UNBLOCKED);

	MFREE(wl->osh, task, sizeof(wl_task_t));
	atomic_dec(&wl->callbacks);
}
#endif /* WL_CONFIG_RFKILL_INPUT */

static int
wl_linux_watchdog(void *ctx)
{
	wl_info_t *wl = (wl_info_t *) ctx;
	struct net_device_stats *stats = NULL;
	uint id;
#ifdef USE_IW
	struct iw_statistics *wstats = NULL;
	int phy_noise;
#endif
	/* refresh stats */
	if (wl->pub->up) {
		ASSERT(wl->stats_id < 2);

		id = 1 - wl->stats_id;

		stats = &wl->stats_watchdog[id];
#ifdef USE_IW
		wstats = &wl->wstats_watchdog[id];
#endif
		stats->rx_packets = WLCNTVAL(wl->pub->_cnt->rxframe);
		stats->tx_packets = WLCNTVAL(wl->pub->_cnt->txframe);
		stats->rx_bytes = WLCNTVAL(wl->pub->_cnt->rxbyte);
		stats->tx_bytes = WLCNTVAL(wl->pub->_cnt->txbyte);
		stats->rx_errors = WLCNTVAL(wl->pub->_cnt->rxerror);
		stats->tx_errors = WLCNTVAL(wl->pub->_cnt->txerror);
		stats->collisions = 0;

		stats->rx_length_errors = 0;
		stats->rx_over_errors = WLCNTVAL(wl->pub->_cnt->rxoflo);
		stats->rx_crc_errors = WLCNTVAL(wl->pub->_cnt->rxcrc);
		stats->rx_frame_errors = 0;
		stats->rx_fifo_errors = WLCNTVAL(wl->pub->_cnt->rxoflo);
		stats->rx_missed_errors = 0;

		stats->tx_fifo_errors = WLCNTVAL(wl->pub->_cnt->txuflo);
#ifdef USE_IW
#if WIRELESS_EXT > 11
		wstats->discard.nwid = 0;
		wstats->discard.code = WLCNTVAL(wl->pub->_cnt->rxundec);
		wstats->discard.fragment = WLCNTVAL(wl->pub->_cnt->rxfragerr);
		wstats->discard.retries = WLCNTVAL(wl->pub->_cnt->txfail);
		wstats->discard.misc = WLCNTVAL(wl->pub->_cnt->rxrunt) +
			WLCNTVAL(wl->pub->_cnt->rxgiant);

		wstats->miss.beacon = 0;
#endif /* WIRELESS_EXT > 11 */
#endif /* USE_IW */

		wl->stats_id = id;

#ifdef USE_IW
		if (!wlc_get(wl->wlc, WLC_GET_PHY_NOISE, &phy_noise))
			wl->phy_noise = phy_noise;
#endif /* USE_IW */
	}

#ifdef CTFPOOL
	/* allocate and add a new skbs to the pkt pool */
	if (CTF_ENAB(wl->cih))
		osl_ctfpool_replenish(wl->osh, CTFPOOL_REFILL_THRESH);
#endif /* CTFPOOL */

	return 0;
}
#endif /* WL_UMK */

#ifdef DSLCPE
void wl_shutdown_handler(wl_info_t *wl)
{
	wlc_shutdown_handler(wl->wlc);
}

void wl_reset_cnt(struct net_device *dev)
{
	wl_info_t *wl;
	wl_if_t *wlif;

	wlif = WL_DEV_IF(dev);
	if (wlif) {
		wl = wlif->wl;
		if (wl) {
			wlc_reset_cnt(wl->wlc);
			wl_linux_watchdog((void *)wl);
		}
	}
}

void wl_deferred_dpc(void *data)
{
#ifdef DSLCPE_NAPI
	wl_info_t *wl;
	wl = (wl_info_t*) data;

	if (NAPI_ENAB(wl->pub))
		napi_schedule(&((wl_if_t *)(netdev_priv(wl->dev)))->napi);
	else
#endif
	wl_dpc((ulong)data);
}

#ifdef DSLCPE_TX_DELAY
bool wl_get_enable_tx_delay(void *wl)
{
	return ((wl_info_t *)wl)->enable_tx_delay;
}
void wl_set_enable_tx_delay(void *wl, bool enable)
{
	((wl_info_t *)wl)->enable_tx_delay = enable;
}
#ifdef DSLCPE_TXRX_BOUND
/* iovar support for tx bound () max # packets for one tx handling */
unsigned int wl_get_tx_bound(void *wl) {
	return ((wl_info_t *)wl)->tx_bound;
}
void wl_set_tx_bound(void *wl, unsigned int bound)
{
	((wl_info_t *)wl)->tx_bound = bound;
}
#endif /* DSLCPE_TXRX_BOUND */
#endif /* DSLCPE_TX_DELAY */

#if defined(CONFIG_BRCM_IKOS)
void wl_emu_selfinit(wl_info_t *wl)
{
	#include <d11.h>
	#include <wlc_rate.h>
	#include <wlc.h>
	extern void wlc_emu_pkt_create(wlc_info_t *wlc);
	extern void wlc_emu_dump_phy(wlc_info_t *wlc) ;
	chanspec_t ch = 1;
	uint16	   retry = 1;
	wlc_ssid_t myssid={strlen("6362_emu"),"6362_emu"};

	//band lock to b
	wlc_bandlock(wl->wlc, (uint)WLC_BAND_2G);

	
	wlc_ioctl(wl->wlc, WLC_SET_CHANNEL,
		&ch, sizeof(chanspec_t), NULL);
				
	//set infra
	if (wlc_set(wl->wlc, WLC_SET_INFRA, 1)) {
		printk("wl: error setting infra 1\n");
	}	
	//set ap
	if (wlc_set(wl->wlc, WLC_SET_AP, 1)) {
		printk("wl: wl_attach: error setting ap 1\n");
	}	
	//set ssid
	if (wlc_ioctl(wl->wlc, WLC_SET_SSID, &myssid, sizeof(wlc_ssid_t), NULL)) {
		printk("wl_attach: error setting ssid %s\n", myssid.SSID);
	}		
			
	//UP
	WL_LOCK(wl);
	wl_up(wl);
	WL_UNLOCK(wl);
	
	//set LRL, SRL = 1
	//wlc_ioctl(wl->wlc, WLC_SET_SRL, &retry, sizeof(retry), NULL);
	//wlc_ioctl(wl->wlc, WLC_SET_LRL, &retry, sizeof(retry), NULL);
	
	//fix ucode to channel 1 , wl default is 1 
	{
 	  int uch=wlc_read_shm(wl->wlc, (uint16)160);
			  
	  if(!uch) {
	  	printk("shm offset 160 = 0x%x, forcing to 1\n",uch);
	    	wlc_write_shm(wl->wlc, 160, (uint16)ch );		
	  }
	
	}
	//turn on promiscuous mode
	wlc_mctrl(wl->wlc, MCTL_PROMISC | MCTL_KEEPCONTROL, MCTL_PROMISC | MCTL_KEEPCONTROL);
	wlc_emu_dump_phy(wl->wlc);		
}
#endif /* CONFIG_BRCM_IKOS */
#endif /* DSLCPE */
