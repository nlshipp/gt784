/*
 * wl_linux.c exported functions and definitions
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wl_linux.h,v 1.2 2010/08/12 09:22:57 ydu Exp $
 */

#ifndef _wl_linux_h_
#define _wl_linux_h_


/* BMAC Note: High-only driver is no longer working in softirq context as it needs to block and
 * sleep so perimeter lock has to be a semaphore instead of spinlock. This requires timers to be
 * submitted to workqueue instead of being on kernel timer
 */
typedef struct wl_timer {
	struct timer_list timer;
	struct wl_info *wl;
	void (*fn)(void *);
	void* arg; /* argument to fn */
	uint ms;
	bool periodic;
	bool set;
	struct wl_timer *next;
#ifdef BCMDBG
	char* name; /* Description of the timer */
#endif
} wl_timer_t;

/* contortion to call functions at safe time */
/* In 2.6.20 kernels work functions get passed a pointer to the struct work, so things
 * will continue to work as long as the work structure is the first component of the task structure.
 */
typedef struct wl_task {
	struct work_struct work;
	void *context;
#ifdef DSLCPE_WORKQUEUE_TASK
	struct wl_info *wl;
	void (*fn)(void *);
	bool set;
#endif /* DSLCPE_WORKQUEUE_TASK */
} wl_task_t;

#if defined(DSLCPE) && defined(DSLCPE_DELAYTASK)
/* wl delayed task data structure */
typedef struct wl_delayed_task {
	struct delayed_work work;
	void *context;
	struct wl_info *wl;
	void (*fn)(void *);
	bool set;
} wl_delayed_task_t;
#endif /* DSLCPE && DSLCPE_DELAYTASK */

#define WL_IFTYPE_BSS	1 /* iftype subunit for BSS */
#define WL_IFTYPE_WDS	2 /* iftype subunit for WDS */
#define WL_IFTYPE_MON	3 /* iftype subunit for MONITOR */

#ifndef WL_UMK
struct wl_if {
#else
struct wl_if {
#endif
#ifdef USE_IW
	wl_iw_t		iw;		/* wireless extensions state (must be first) */
#endif /* USE_IW */
	struct wl_if *next;
	struct wl_info *wl;		/* back pointer to main wl_info_t */
	struct net_device *dev;		/* virtual netdevice */
	struct wlc_if *wlcif;		/* wlc interface handle */
	uint subunit;			/* WDS/BSS unit */
	bool dev_registed;		/* netdev registed done */
#ifdef WL_UMK
	struct pci_dev *pci_dev;
#endif
#if defined(DSLCPE_NAPI) && (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30))
    struct napi_struct napi;
#endif
#ifdef AEI_VDSL_CUSTOMER_NCS
	uint16 usMirrorInFlags;
	uint16 usMirrorOutFlags;
#endif
#ifndef WL_UMK
};
#else
};
#endif

struct rfkill_stuff {
	struct rfkill *rfkill;
	char rfkill_name[32];
	char registered;
};

struct wl_info {
	wlc_pub_t	*pub;		/* pointer to public wlc state */
	void		*wlc;		/* pointer to private common os-independent data */
	osl_t		*osh;		/* pointer to os handler */
#ifdef HNDCTF
	ctf_t		*cih;		/* ctf instance handle */
#endif /* HNDCTF */
	struct net_device *dev;		/* backpoint to device */
#ifdef WL_UMK
	wl_cdev_t	*char_dev;	/* allcoated in wl_cdev_init */
	int		pid;		/* process id */
#endif
#ifdef WLC_HIGH_ONLY
	struct semaphore sem;		/* use semaphore to allow sleep */
#else
	spinlock_t	lock;		/* per-device perimeter lock */
	spinlock_t	isr_lock;	/* per-device ISR synchronization lock */
#endif
	uint		bcm_bustype;	/* bus type */
	bool		piomode;	/* set from insmod argument */
	void *regsva;			/* opaque chip registers virtual address */
	struct net_device_stats stats;	/* stat counter reporting structure */
	wl_if_t *if_list;		/* list of all interfaces */
	atomic_t callbacks;		/* # outstanding callback functions */
	struct wl_timer *timers;	/* timer cleanup queue */
#ifndef NAPI_POLL
	struct tasklet_struct tasklet;	/* dpc tasklet */
#endif /* NAPI_POLL */
	struct net_device *monitor_dev;	/* monitor pseudo device */
	uint		monitor_type;	/* monitor pseudo device */
	bool		resched;	/* dpc needs to be and is rescheduled */
#ifdef TOE
	wl_toe_info_t	*toei;		/* pointer to toe specific information */
#endif
#ifdef ARPOE
	wl_arp_info_t	*arpi;		/* pointer to arp agent offload info */
#endif
#if defined(DSLCPE_DELAY)
	shared_osl_t	oshsh;		/* shared info for osh */
#endif
#ifdef LINUXSTA_PS
	uint32		pci_psstate[16];	/* pci ps-state save/restore */
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 29)
	struct lib80211_crypto_ops *tkipmodops;
#else
	struct ieee80211_crypto_ops *tkipmodops;	/* external tkip module ops */
#endif
	struct ieee80211_tkip_data  *tkip_ucast_data;
	struct ieee80211_tkip_data  *tkip_bcast_data;
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 14) */
	/* RPC, handle, lock, txq, workitem */
#ifdef WLC_HIGH_ONLY
	rpc_info_t 	*rpc;		/* RPC handle */
	rpc_tp_info_t	*rpc_th;	/* RPC transport handle */
	wlc_rpc_ctx_t	rpc_dispatch_ctx;

	bool	   rpcq_dispatched;	/* Avoid scheduling multiple tasks */
	spinlock_t rpcq_lock;		/* Lock for the queue */
	rpc_buf_t *rpcq_head;		/* RPC Q */
	rpc_buf_t *rpcq_tail;		/* Points to the last buf */
#endif /* WLC_HIGH_ONLY */

#if defined(WLC_HIGH_ONLY) || defined(DSLCPE_TX_DELAY)
	bool	   txq_dispatched;	/* Avoid scheduling multiple tasks */
	spinlock_t txq_lock;		/* Lock for the queue */
	struct sk_buff *txq_head;	/* TX Q */
	struct sk_buff *txq_tail;	/* Points to the last buf */

	wl_task_t	txq_task;	/* work queue for wl_start() */
	wl_task_t	multicast_task;	/* work queue for wl_set_multicast_list() */
#ifdef DSLCPE_TX_DELAY	
	bool enable_tx_delay;
#ifdef DSLCPE_TXRX_BOUND
	unsigned int tx_bound; /* max # packets for one tx handling */
#endif /* DSLCPE_TXRX_BOUND */
#endif /* DSLCPE_TX_DELAY */
#endif /* WLC_HIGH_ONLY || DSLCPE_TX_DELAY */
	uint	stats_id;		/* the current set of stats */
	/* ping-pong stats counters updated by Linux watchdog */
	struct net_device_stats stats_watchdog[2];
#ifdef USE_IW
	struct iw_statistics wstats_watchdog[2];
	struct iw_statistics wstats;
	int		phy_noise;
#endif /* USE_IW */
#if defined(WL_CONFIG_RFKILL_INPUT)
	struct rfkill_stuff wl_rfkill;
	mbool last_phyind;
#endif /* defined(WL_CONFIG_RFKILL_INPUT) */
};


#ifdef WLC_HIGH_ONLY
#define WL_LOCK(wl)	down(&(wl)->sem)
#define WL_UNLOCK(wl)	up(&(wl)->sem)

#define WL_ISRLOCK(wl)
#define WL_ISRUNLOCK(wl)
#else
/* perimeter lock */
#define WL_LOCK(wl)	spin_lock_bh(&(wl)->lock)
#define WL_UNLOCK(wl)	spin_unlock_bh(&(wl)->lock)

/* locking from inside wl_isr */
#define WL_ISRLOCK(wl, flags) do {spin_lock(&(wl)->isr_lock); (void)(flags);} while (0)
#define WL_ISRUNLOCK(wl, flags) do {spin_unlock(&(wl)->isr_lock); (void)(flags);} while (0)

/* locking under WL_LOCK() to synchronize with wl_isr */
#define INT_LOCK(wl, flags)	spin_lock_irqsave(&(wl)->isr_lock, flags)
#define INT_UNLOCK(wl, flags)	spin_unlock_irqrestore(&(wl)->isr_lock, flags)
#endif	/* WLC_HIGH_ONLY */


#ifdef WL_UMK
extern int wl_found;
#endif

/* handle forward declaration */
typedef struct wl_info wl_info_t;

#ifndef PCI_D0
#define PCI_D0		0
#endif

#ifndef PCI_D3hot
#define PCI_D3hot	3
#endif

/* exported functions */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
extern irqreturn_t wl_isr(int irq, void *dev_id);
#else
extern irqreturn_t wl_isr(int irq, void *dev_id, struct pt_regs *ptregs);
#endif

extern int __devinit wl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
extern void wl_free(wl_info_t *wl);
extern int  wl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
extern struct net_device * wl_netdev_get(wl_info_t *wl);

#ifdef BCM_WL_EMULATOR
extern wl_info_t *  wl_wlcreate(osl_t *osh, void *pdev);
#endif

#endif /* _wl_linux_h_ */
