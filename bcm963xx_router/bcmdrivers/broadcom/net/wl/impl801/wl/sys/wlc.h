/*
 * Common (OS-independent) definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */

#ifndef _wlc_h_
#define _wlc_h_

#include <wlc_types.h>

#include <wl_dbg.h>
#include <wlioctl.h>
#ifdef WLC_HIGH
#include <wlc_event.h>
#endif
#include <wlc_pio.h>
#include <wlc_phy_hal.h>
#include <wlc_channel.h>
#ifdef WLC_SPLIT
#include <bcm_rpc.h>
#endif



/*
 * defines for join pref processing
 */
#define WLC_JOIN_PREF_LEN_FIXED		2 /* Length for the fixed portion of Join Pref TLV value */

#define JOIN_RSSI_BAND		WLC_BAND_5G	/* WLC_BAND_AUTO disables the feature */
#define JOIN_RSSI_DELTA		10		/* Positive value */
#define JOIN_PREF_IOV_LEN	8		/* 4 bytes each for RSSI Delta and mandatory RSSI */

/* Construct the join pref TLV based on rssi and band */
#define PREP_JOIN_PREF_RSSI_DELTA(_pref, _rssi, _band) \
	do {						\
		(_pref)[0] = WL_JOIN_PREF_RSSI_DELTA;	\
		(_pref)[1] = WLC_JOIN_PREF_LEN_FIXED;	\
		(_pref)[2] = _rssi;			\
		(_pref)[3] = _band;			\
	} while (0)

/* Construct the mandatory TLV for RSSI */
#define PREP_JOIN_PREF_RSSI(_pref) \
	do {						\
		(_pref)[0] = WL_JOIN_PREF_RSSI;		\
		(_pref)[1] = WLC_JOIN_PREF_LEN_FIXED;	\
		(_pref)[2] = 0;				\
		(_pref)[3] = 0;				\
	} while (0)

#define MA_WINDOW_SZ		8	/* moving average window size */
#define	HWRXOFF			38	/* chip rx buffer offset */
#define	INVCHANNEL		255	/* invalid channel */
#define	MAXCOREREV		31	/* max # supported core revisions (0 .. MAXCOREREV - 1) */
#define NSCBHASH                ((MAXSCB + 7)/8) /* # scb hash buckets */
#define	WLC_MAXFRAG		16	/* max fragments per SDU */
#define WLC_WEP_IV_SIZE		4	/* size of WEP1 and WEP128 IV */
#define WLC_WEP_ICV_SIZE	4	/* size of WEP ICV */
#define WLC_ASSOC_RECREATE_BI_TIMEOUT	10 /* bcn intervals to wait during and assoc_recreate */
#define WLC_ASSOC_VERIFY_TIMEOUT	1000 /* ms to wait to allow an AP to respond to class 3
					      * data if it needs to disassoc/deauth
					      */
#define WLC_ADVERTISED_LISTEN	10	/* listen interval specified in (re)assoc request */
#define WLC_FREQTRACK_THRESHOLD	5	/* seconds w/o beacons before we increase the
					 * frequency tracking bandwidth
					 */
#define WLC_FREQTRACK_DETECT_TIME	2	/* seconds in which we must detect
						 * beacons after setting the wide
						 * bandwidth
						 */
#define WLC_FREQTRACK_MIN_ATTEMPTS	3	/* minimum number of times we should
						 * try to get beacons using the wide freq
						 * tracking b/w
						 */
#define WLC_FREQTRACK_TIMEOUT	30	/* give up on wideband frequency tracking after
					 * 30 seconds
					 */
#ifdef BCMQT_CPU
#define WECA_ASSOC_TIMEOUT	1500	/* qt is slow */
#else
#define WECA_ASSOC_TIMEOUT	300	/* authentication or association timeout in ms */
#endif
#ifdef WLAFTERBURNER
#define	WLC_ABMINRATE		WLC_RATE_36M	/* default afterburner minimum rate value */
#endif /* WLAFTERBURNER */
#define WLC_MAXMODULES		22	/* max #  wlc_module_register() calls */
#define WLC_TXQ_DRAIN_TIMEOUT	3000	/* txq drain timeout in ms */
#define WLC_IE_WAIT_TIMEOUT	200	/* assoc. ie waiting timeout in ms */

/* network protection config */
#define	WLC_PROT_G_SPEC		1	/* SPEC g protection */
#define	WLC_PROT_G_OVR		2	/* SPEC g prot override */
#define	WLC_PROT_G_USER		3	/* gmode specified by user */
#define	WLC_PROT_OVERLAP	4	/* overlap */
#define	WLC_PROT_N_USER		10	/* nmode specified by user */
#define	WLC_PROT_N_CFG		11	/* n protection */
#define	WLC_PROT_N_CFG_OVR	12	/* n protection override */
#define	WLC_PROT_N_NONGF	13	/* non-GF protection */
#define	WLC_PROT_N_NONGF_OVR	14	/* non-GF protection override */
#define	WLC_PROT_N_PAM_OVR	15	/* n preamble override */
#define	WLC_PROT_N_OBSS		16	/* non-HT OBSS present */


/* network packet filter bit map matching NDIS_PACKET_TYPE_XXX */
#define WLC_ACCEPT_DIRECTED               0x0001
#define WLC_ACCEPT_MULTICAST              0x0002
#define WLC_ACCEPT_ALL_MULTICAST          0x0004
#define WLC_ACCEPT_BROADCAST              0x0008
#define WLC_ACCEPT_SOURCE_ROUTING         0x0010
#define WLC_ACCEPT_PROMISCUOUS            0x0020
#define WLC_ACCEPT_SMT                    0x0040
#define WLC_ACCEPT_ALL_LOCAL              0x0080

#define WLC_BITSCNT(x)	bcm_bitcount((uint8 *)&(x), sizeof(uint8))


/* freqtrack_override values */
typedef enum _freqtrack_override_t {
	FREQTRACK_AUTO = 0,
	FREQTRACK_ON,
	FREQTRACK_OFF
} freqtrack_override_t;

#define NUM_UCODE_FRAMES        4	/* Num ucode frames for TSSI estimation */

#define WLC_FRAMEBURST_MIN_RATE WLC_RATE_2M	/* frameburst mode is only enabled above 2Mbps */

#define WLC_CHANNEL_QA_NSAMP	2	/* size of channel_qa_sample array */
#define WLC_RM_RPI_INTERVAL	20	/* ms, time between RPI/Noise samples */

#define	WLC_RATEPROBE_RATE	(6 * 2)	/* 6Mbps ofdm rateprobe rate */

/* Maximum wait time for a MAC suspend */
#define	WLC_MAX_MAC_SUSPEND	83000	/* uS: 83mS is max packet time (64KB ampdu @ 6Mbps) */

/* Probe Response timeout - responses for probe requests older that this are tossed, zero to disable
 */
#define WLC_PRB_RESP_TIMEOUT	0 /* Disable probe response timeout */

#define WLC_VLAN_TAG_LEN	4	/* Duplicated to avoid including <proto/vlan.h> */

#define CCX_HEADROOM 0

#define TKIP_TAILROOM MAX(TKIP_MIC_SIZE, TKIP_EOM_SIZE) /* Tailroom needed for TKIP */

/* TKIP and CKIP are mutually exclusive so account only for max */
#define TXTAIL TKIP_TAILROOM


/* transmit buffer max headroom for protocol headers */
#define TXOFF (D11_TXH_LEN + D11_PHY_HDR_LEN + DOT11_A4_HDR_LEN + DOT11_QOS_LEN + \
	       DOT11_IV_MAX_LEN + DOT11_LLC_SNAP_HDR_LEN + WLC_VLAN_TAG_LEN + CCX_HEADROOM)

#ifdef WLC_HIGH
/* For managing scan result lists */
typedef struct wlc_bss_list {
	uint		count;
	bool		beacon;		/* set for beacon, cleared for probe response */
	wlc_bss_info_t*	ptrs[MAXBSS];
} wlc_bss_list_t;
#endif /* WLC_HIGH */

#define ISCAN_SUCCESS(wlc)  \
		(wlc->custom_iscan_results_state == WL_SCAN_RESULTS_SUCCESS)
#define ISCAN_PARTIAL(wlc)  \
		(wlc->custom_iscan_results_state == WL_SCAN_RESULTS_PARTIAL)
#define ISCAN_IN_PROGRESS(wlc)  \
		(wlc->custom_iscan_results_state == WL_SCAN_RESULTS_PENDING)
#define ISCAN_ABORTED(wlc)  \
		(wlc->custom_iscan_results_state == WL_SCAN_RESULTS_ABORTED)
#define ISCAN_RESULTS_OK(wlc) (ISCAN_SUCCESS(wlc) || ISCAN_PARTIAL(wlc))

#define	SW_TIMER_MAC_STAT_UPD		30	/* periodic MAC stats update */

#define CRSGLITCH_THRESHOLD_DEFAULT	4000 /* normalized per second count */
#define CCASTATS_THRESHOLD_DEFAULT	40 /* normalized percent stats 0 - 255 */
#define SAMPLE_PERIOD_DEFAULT		5 /* in second */
#define THRESHOLD_TIME_DEFAULT		2 /* number of sample periods */
#define LOCKOUT_PERIOD_DEFAULT		120 /* in second */
#define MAX_ACS_DEFAULT				5 /* number of ACS in a lockout period */
#define CHANIM_SCB_MAX_PROBE 		20

#define SAMPLE_PERIOD_MIN		1
#define THRESHOLD_TIME_MIN		1

/* index for accumulative count */
#define CHANIM_HOME_CHAN			0
#define CHANIM_OFF_CHAN 			1
#define CHANIM_ACCUM_CNT			2

/* wlc_chanim_update flags */
#define CHANIM_WD			0x1 /* on watchdog */
#define CHANIM_CHANSPEC 	0x2 /* on chanspec switch */

/* chanim flag defines */
#define CHANIM_DETECTED	0x1 	/* interference detected */
#define CHANIM_ACTON		0x2		/* ACT triggered */

#define WLC_CCASTATS_CAP(wlc)  D11REV_GE(((wlc_info_t*)(wlc))->pub->corerev, 15)

#ifdef WLCHANIM
#define WLC_CHANIM_ENAB(wlc)	1	/* WLCHANIM support, for run time control */
#define WLC_CHANIM_ACT(c_info)		(chanim_mark(c_info).flags & CHANIM_ACTON)
#define WLC_CHANIM_MODE_ACT(c_info) (chanim_config(c_info).mode >= CHANIM_ACT)
#define WLC_CHANIM_MODE_DETECT(c_info) (chanim_config(c_info).mode >= CHANIM_DETECT)
#define WLC_CHANIM_LOCKOUT(c_info) \
	(chanim_mark(c_info).flags & CHANIM_LOCKOUT)
#else /* WLCHANIM */
#define WLC_CHANIM_ENAB(a) 		0	/* NO WLCHANIM support */
#define WLC_CHANIM_ACT(a)		0	/* NO WLCHANIM support */
#define WLC_CHANIM_MODE_ACT(a) 0
#define WLC_CHANIM_MODE_DETECT(a) 0
#define WLC_CHANIM_LOCKOUT(a) 0
#endif /* WLCHANIM */

typedef struct iscan_ignore {
	uint8 bssid[ETHER_ADDR_LEN];
	uint16 ssid_sum;
	uint16 ssid_len;
	chanspec_t chanspec;	/* chan portion is chan on which bcn_prb was rx'd */
} iscan_ignore_t;

/* support 100 ignore list items */
#define WLC_ISCAN_IGNORE_LIST_SZ	1200
#define IGNORE_LIST_MATCH_SZ	OFFSETOF(iscan_ignore_t, chanspec)
#define WLC_ISCAN_IGNORE_MAX	(WLC_ISCAN_IGNORE_LIST_SZ / sizeof(iscan_ignore_t))

/* wlc_bss_info flag bit values */
#define WLC_BSS_54G		0x0001	/* BSS is a legacy 54g BSS */
#define WLC_BSS_RSSI_ON_CHANNEL	0x0002	/* RSSI measurement was from the same channel as BSS */
#define WLC_BSS_WME		0x0004	/* BSS is WME capable */
#define WLC_BSS_BRCM		0x0008	/* BSS is BRCM */
#define WLC_BSS_WPA		0x0010	/* BSS is WPA capable */
#define WLC_BSS_HT		0x0020	/* BSS is HT (MIMO) capable */
#define WLC_BSS_40MHZ		0x0040	/* BSS is 40MHZ capable */
#define WLC_BSS_WPA2		0x0080	/* BSS is WPA2 capable */
#define WLC_BSS_BEACON		0x0100	/* bss_info was derived from a beacon */
#define WLC_BSS_40INTOL		0x0200	/* BSS is forty intolerant */
#define WLC_BSS_SGI_20		0x0400	/* BSS supports 20MHz SGI */
#define WLC_BSS_SGI_40		0x0800	/* BSS supports 40MHz SGI */
#ifdef WLBTAMP
#define WLC_BSS_11E		0x1000	/* BSS is 802.11e capable (BT-AMP QoS) */
#endif /* WLBTAMP */
#define WLC_BSS_CACHE		0x2000	/* bss_info was collected from scan cache */
#ifdef BCMWAPI_WAI
#define WLC_BSS_WAPI		0x4000	/* BSS is WAPI capable */
#endif /* BCMWAPI_WAI */

/* pkt type used to indicate a roaming STA */
#define ROAM_PKT_TYPE	0x806	/* currently arp */
#define ARP_CMD_OFF		6 /* Unused */
#define ARP_RESP		2 /* Unused */

/* wlc_BSSinit actions */
#define WLC_BSS_JOIN		0 /* join action */
#define WLC_BSS_START		1 /* start action */

#define WLC_STA_RETRY_MAX_TIME	3600	/* maximum config value for sta_retry_time (1 hour) */

#define WLC_2050_ROAM_TRIGGER	(-70) /* Roam Trigger for 2050 */
#define WLC_2050_ROAM_DELTA	(20)  /* Roam Delta for 2050 */
#define WLC_2053_ROAM_TRIGGER	(-60) /* Roam Trigger for 2053 */
#define WLC_2053_ROAM_DELTA	(15)  /* Roam Delta for 2053 */
#define WLC_2055_ROAM_TRIGGER	(-75) /* Roam Trigger for 2055 */
#define WLC_2055_ROAM_DELTA	(20)  /* Roam Delta for 2055 */
#define WLC_2060WW_ROAM_TRIGGER	(-75) /* Roam Trigger for 2060ww */
#define WLC_2060WW_ROAM_DELTA	(15)  /* Roam Delta for 2060ww */
#define WLC_2G_ROAM_TRIGGER	(-75)	/* Roam trigger for all other radios */
#define WLC_2G_ROAM_DELTA	(20)	/* Roam delta for all other radios */
#define WLC_5G_ROAM_TRIGGER	(-75)	/* Roam trigger for all other radios */
#define WLC_5G_ROAM_DELTA	(15)	/* Roam delta for all other radios */
#define WLC_AUTO_ROAM_TRIGGER	(-75)	/* This value can dynamically change */
#define WLC_AUTO_ROAM_DELTA	(15)	/* Can also change under motion */
#define WLC_NEVER_ROAM_TRIGGER	(-150) /* Avoid Roaming by setting a large value */
#define WLC_NEVER_ROAM_DELTA	(150)  /* Avoid Roaming by setting a large value */

/* ROAM related defines */
#ifdef BCMQT_CPU
#define WLC_BCN_TIMEOUT		40	/* qt is slow */
#else
#define WLC_BCN_TIMEOUT		8	/* seconds w/o bcns until loss of connection */
#endif
#define WLC_ROAM_SCAN_PERIOD	10	/* roaming scan period in seconds */
#define WLC_FULLROAM_PERIOD 	70	/* full roam scan Period */
#define WLC_CACHE_INVALID_TIMER	60	/* entries are no longer valid after this # of secs */
#define WLC_UATBTT_TBTT_THRESH	10	/* beacon loss before checking unaligned tbtt */
#define WLC_ROAM_TBTT_THRESH	20	/* beacon loss before roaming attempt */

#define ROAM_FULLSCAN_NTIMES	3
#define ROAM_RSSITHRASHDIFF 	5

/* thresholds for consecutive bcns lost roams allowed */
#define ROAM_CONSEC_BCNS_LOST_THRESH	5

/* Motion detection related defines */
#define ROAM_MOTION_TIMEOUT		120	 /* 2 minutes? */
#define MOTION_RSSI_DELTA_DEFAULT	10	 /* in dBm */

/* AP environment */
#define AP_ENV_DETECT_NOT_USED	0 /* We aren't using AP environment detection */
#define AP_ENV_DENSE		1 /* "Corporate" or other AP dense environment */
#define AP_ENV_SPARSE		2 /* "Home" or other sparse environment */
#define AP_ENV_INDETERMINATE	3 /* AP environment hasn't been identified */

/* Roam delta suggested defaults, in dBm */
#define ROAM_DELTA_AGGRESSIVE	10
#define ROAM_DELTA_MODERATE	20
#define ROAM_DELTA_CONSERVATIVE	30
#define ROAM_DELTA_AUTO		15

/* Defaults? */
#define TXMIN_THRESH_DEFAULT	7 /* Roam scan after 7 packets at min tx rate */
#define TXFAIL_THRESH_DEFAULT	7 /* Start looking if we are stuck at the most basic rate */

/* motion detect */
#define MOTION_DETECT_DELTA_MOD	5 /* Drop the delta by 5 dB if we detect motion */
#define MOTION_DETECT_TRIG_MOD	10 /* Increase the trigger by 10 dB if we detect motion */
#define MOTION_DETECT_RSSI	-50 /* Don't activate the motion detection code above -50 dB */

#define ROAM_REASSOC_TIMEOUT	300 /* 5 minutes? */

#define WLC_MIN_CNTRY_ELT_SZ	6	/* Min size for 802.11d Country Info Element. */

/* Double check that unsupported cores are not enabled */
#if CONF_MSK(D11CONF, 0x4f) || CONF_GE(D11CONF, MAXCOREREV)
#error "Configuration for D11CONF includes unsupported versions."
#endif /* Bad versions */

#define	VALID_COREREV(corerev)	CONF_HAS(D11CONF, corerev)

/* values for shortslot_override */
#define WLC_SHORTSLOT_AUTO	-1 /* Driver will manage Shortslot setting */
#define WLC_SHORTSLOT_OFF	0  /* Turn off short slot */
#define WLC_SHORTSLOT_ON	1  /* Turn on short slot */

/* value for short/long and mixmode/greenfield preamble */

#define WLC_LONG_PREAMBLE	(0)
#define WLC_SHORT_PREAMBLE	(1 << 0)
#define WLC_GF_PREAMBLE		(1 << 1)
#define WLC_MM_PREAMBLE		(1 << 2)
#define WLC_IS_MIMO_PREAMBLE(_pre) (((_pre) == WLC_GF_PREAMBLE) || ((_pre) == WLC_MM_PREAMBLE))

/* values for barker_preamble */
#define WLC_BARKER_SHORT_ALLOWED	0 /* Short pre-amble allowed */
#define WLC_BARKER_LONG_ONLY		1 /* No short pre-amble allowed */

#define	WLC_IBSS_BCN_TIMEOUT	4 /* Timeout to indicate that various types of IBSS beacons have
				   * gone away
				   */


#define	EHINIT(eh, src, dst, type) \
	bcopy((uchar*)dst, (eh)->ether_dhost, ETHER_ADDR_LEN); \
	bcopy((uchar*)src, (eh)->ether_shost, ETHER_ADDR_LEN); \
	(eh)->ether_type = hton16(type);

/* assoc->state values */
#define AS_IDLE			0 /* Idle state */
#define AS_SCAN			1 /* Scan state */
#define AS_JOIN_START		2 /* Join start state */
#define AS_WAIT_IE		3 /* waiting for association ie */
#define AS_WAIT_IE_TIMEOUT	4 /* ie waiting timeout */
#define AS_WAIT_TX_DRAIN	5 /* Waiting to tx drain out state */
#define AS_SENT_AUTH_1		6 /* Sent Authentication 1 state */
#define AS_SENT_AUTH_2		7 /* Sent Authentication 2 state */
#define AS_SENT_AUTH_3		8 /* Sent Authentication 3 state */
#define AS_SENT_ASSOC		9 /* Sent Association state */
#define AS_REASSOC_RETRY        10 /* Sent Assoc on DOT11_SC_REASSOC_FAIL */
#define AS_WAIT_RCV_BCN		11 /* Waiting to receive beacon state */
#define AS_SYNC_RCV_BCN		12 /* Waiting Re-sync beacon state */
#define AS_WAIT_DISASSOC	13 /* Waiting to disassociate state */
#define AS_WAIT_PASSHASH	14 /* calculating passhash state */
#define AS_RECREATE_WAIT_RCV_BCN 15 /* wait for former BSS/IBSS bcn in assoc_recreate */
#define AS_ASSOC_VERIFY		16 /* allow former AP time to disassoc/deauth before PS mode */
#define AS_LOSS_ASSOC		17 /* Loss association */
#define AS_LAST_STATE		17

/* assoc->type values */
#define AS_ASSOCIATION		1 /* Join association */
#define AS_ROAM			2 /* Roaming association */
#define AS_RECREATE		3 /* Re-Creating a prior association */

/* assoc->flags */
#define AS_F_CACHED_RESULTS	0x0001	/* Assoc used cached results */
#define AS_F_CACHED_CHANNELS	0x0002	/* Assoc used cached results to create a channel list */
#define AS_F_SPEEDY_RECREATE	0x0004	/* Speedy association recreation */
#define AS_F_CACHED_ROAM	0x0008	/* Assoc used cached results when directed roaming */

/* A fifo is full. Clear precedences related to that FIFO */
#define WLC_TX_FIFO_CLEAR(wlc, fifo) ((wlc)->tx_prec_map &= ~(wlc)->fifo2prec_map[fifo])

/* Fifo is NOT full. Enable precedences for that FIFO */
#define WLC_TX_FIFO_ENAB(wlc, fifo)  ((wlc)->tx_prec_map |= (wlc)->fifo2prec_map[fifo])

/* TxFrameID */
/* seq and frag bits: SEQNUM_SHIFT, FRAGNUM_MASK (802.11.h) */
/* rate epoch bits: TXFID_RATE_SHIFT, TXFID_RATE_MASK ((wlc_rate.c) */
#define TXFID_QUEUE_MASK	0x0007 /* Bits 0-2 */
#define TXFID_SEQ_MASK		0x7FE0 /* Bits 5-14 */
#define TXFID_SEQ_SHIFT		5      /* Number of bit shifts */
#define	TXFID_RATE_PROBE_MASK	0x8000 /* Bit 15 for rate probe */
#define TXFID_RATE_MASK		0x0018 /* Mask for bits 3 and 4 */
#define TXFID_RATE_SHIFT	3      /* Shift 3 bits for rate mask */

/* promote boardrev */
#define BOARDREV_PROMOTABLE	0xFF	/* from */
#define BOARDREV_PROMOTED	1	/* to */

/* power-save mode definitions */
#define PSQ_PKTS_BCMC		50	/* max # of enq'd PS bc/mc pkts */
#ifndef PSQ_PKTS_LO
#define PSQ_PKTS_LO		5	/* max # PS pkts scb can enq */
#endif
#ifndef PSQ_PKTS_HI
#define	PSQ_PKTS_HI		125
#endif

#ifdef STA
/* if wpa is in use then portopen is true when the group key is plumbed otherwise it is always true
 */
#define WLC_PORTOPEN(cfg) \
	(((cfg)->WPA_auth != WPA_AUTH_DISABLED && WSEC_ENABLED((cfg)->wsec)) ? \
	(cfg)->wsec_portopen : TRUE)

#define WLC_DPT_PM_PENDING(dpt) FALSE

#ifdef WLBTAMP
extern bool wlc_bta_active(bta_info_t *bta);
/* if there is any BT-AMP physical links then we are not allowed to go in PS mode */
#define BTA_ACTIVE(wlc)	wlc_bta_active(wlc->bta)
extern bool wlc_bta_inprog(bta_info_t *bta);
/* if there is any BT-AMP physical link creations then we are not allowed to go in MPC mode */
#define BTA_IN_PROGRESS(wlc)	wlc_bta_inprog(wlc->bta)
#else
#define BTA_ACTIVE(wlc)	FALSE
#define BTA_IN_PROGRESS(wlc)	FALSE
#endif

#define PS_ALLOWED(wlc)	wlc_ps_allowed(wlc)
#define STAY_AWAKE(wlc) wlc_stay_awake(wlc)
#else
#define PS_ALLOWED(wlc)	FALSE
#define STAY_AWAKE(wlc)	TRUE
#endif /* STA */

/* 802.1D Priority to TX FIFO number for wme */
extern const uint8 prio2fifo[];

/* rm request types */
#define WLC_RM_TYPE_NONE		0 /* No Radio measurement type */
#define WLC_RM_TYPE_BASIC		1 /* Basic Radio measurement type */
#define WLC_RM_TYPE_CCA			2 /* CCA Radio measurement type */
#define WLC_RM_TYPE_RPI			3 /* RPI Radio measurement type */

/* rm request flags */
#define WLC_RM_FLAG_PARALLEL		(1<<0) /* Flag for setting for Parallel bit */
#define WLC_RM_FLAG_LATE		(1<<1) /* Flag for LATE bit */
#define WLC_RM_FLAG_INCAPABLE		(1<<2) /* Flag for Incapable bit */
#define WLC_RM_FLAG_REFUSED		(1<<3) /* Flag for REFUSED */

/* rm report type */
#define WLC_RM_CLASS_NONE		0 /* No report */
#define WLC_RM_CLASS_IOCTL		1 /* IOCTL type of report */
#define WLC_RM_CLASS_11H		2 /* Report for 11H */
#define WLC_RM_CLASS_CCX		0x10 /* Report for CCX */

#define DATA_BLOCK_SCAN		(1 << 0)
#define DATA_BLOCK_QUIET	(1 << 1)
#define DATA_BLOCK_JOIN		(1 << 2)
#define DATA_BLOCK_PS		(1 << 3)
#define DATA_BLOCK_TX_SUPR	(1 << 4)

/* Ucode MCTL_WAKE override bits */
#define WLC_WAKE_OVERRIDE_CLKCTL	0x01
#define WLC_WAKE_OVERRIDE_PHYREG	0x02
#define WLC_WAKE_OVERRIDE_MACSUSPEND	0x04
#define WLC_WAKE_OVERRIDE_TXFIFO	0x08
#define WLC_WAKE_OVERRIDE_FORCEFAST	0x10

/* stuff pulled in from wlc.c */

/* Interrupt bit error summary.  Don't include I_RU: we refill DMA at other
 * times; and if we run out, constant I_RU interrupts may cause lockup.  We
 * will still get error counts from rx0ovfl.
 */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RO | I_XU)
/* default software intmasks */
#define	DEF_RXINTMASK	(I_RI)	/* enable rx int on rxfifo only */
#define	DEF_MACINTMASK	(MI_TXSTOP | MI_TBTT | MI_ATIMWINEND | MI_PMQ | \
			 MI_PHYTXERR | MI_DMAINT | MI_TFS | MI_BG_NOISE | \
			 MI_CCA | MI_TO | MI_GP0 | MI_RFDISABLE | MI_PWRUP | \
			 MI_BT_RFACT_STUCK | MI_BT_PRED_REQ)

#define	RETRY_SHORT_DEF			7	/* Default Short retry Limit */
#define	RETRY_SHORT_MAX			255	/* Maximum Short retry Limit */
#define	RETRY_LONG_DEF			4	/* Default Long retry count */
#define	RETRY_SHORT_FB			3	/* Short retry count for fallback rate */
#define	RETRY_LONG_FB			2	/* Long retry count for fallback rate */

#define	MAXTXPKTS		6		/* max # pkts pending */
#ifdef WLAFTERBURNER
#define	MAXTXPKTS_AB		8		/* max # pkts pending if afterburner enabled */
#endif /* WLAFTERBURNER */
#define MAXTXPKTS_AMPDUMAC	64

/* frameburst */
#define	MAXTXFRAMEBURST		8		/* vanilla xpress mode: max frames/burst */
#ifdef WLAFTERBURNER
#define	MAXTXFRAMEBURST_AB	16		/* afterburner mode: max frames/burst */
#endif /* WLAFTERBURNER */
#define	MAXFRAMEBURST_TXOP	10000		/* Frameburst TXOP in usec */

/* Per-AC retry limit register definitions; uses bcmdefs.h bitfield macros */
#define EDCF_SHORT_S            0
#define EDCF_SFB_S              4
#define EDCF_LONG_S             8
#define EDCF_LFB_S              12
#define EDCF_SHORT_M            BITFIELD_MASK(4)
#define EDCF_SFB_M              BITFIELD_MASK(4)
#define EDCF_LONG_M             BITFIELD_MASK(4)
#define EDCF_LFB_M              BITFIELD_MASK(4)

#define WME_RETRY_SHORT_GET(val, ac)        GFIELD(val, EDCF_SHORT)
#define WME_RETRY_SFB_GET(val, ac)          GFIELD(val, EDCF_SFB)
#define WME_RETRY_LONG_GET(val, ac)         GFIELD(val, EDCF_LONG)
#define WME_RETRY_LFB_GET(val, ac)          GFIELD(val, EDCF_LFB)

#define WLC_WME_RETRY_SHORT_GET(wlc, ac)    GFIELD(wlc->wme_retries[ac], EDCF_SHORT)
#define WLC_WME_RETRY_SFB_GET(wlc, ac)      GFIELD(wlc->wme_retries[ac], EDCF_SFB)
#define WLC_WME_RETRY_LONG_GET(wlc, ac)     GFIELD(wlc->wme_retries[ac], EDCF_LONG)
#define WLC_WME_RETRY_LFB_GET(wlc, ac)      GFIELD(wlc->wme_retries[ac], EDCF_LFB)

#define WLC_WME_RETRY_SHORT_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_SHORT, val)
#define WLC_WME_RETRY_SFB_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_SFB, val)
#define WLC_WME_RETRY_LONG_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_LONG, val)
#define WLC_WME_RETRY_LFB_SET(wlc, ac, val) \
	wlc->wme_retries[ac] = SFIELD(wlc->wme_retries[ac], EDCF_LFB, val)

/* move WLC_NOISE_REQUEST_xxx  */
#define WLC_NOISE_REQUEST_SCAN	0x1
#define WLC_NOISE_REQUEST_CQ	0x2
#define WLC_NOISE_REQUEST_RM	0x4

#define WLC_RSSI_WINDOW_SZ	16	/* rssi window size */

/* PLL requests */
#define WLC_PLLREQ_SHARED	0x1	/* pll is shared on old chips */
#define WLC_PLLREQ_RADIO_MON	0x2	/* hold pll for radio monitor register checking */
#define WLC_PLLREQ_FLIP		0x4	/* hold/release pll for some short operation */

/* Do we support this rate? */
#define VALID_RATE(wlc, rspec) wlc_valid_rate(wlc, rspec, WLC_BAND_AUTO, FALSE)
#define VALID_RATE_DBG(wlc, rspec) wlc_valid_rate(wlc, rspec, WLC_BAND_AUTO, TRUE)

#define WLC_MIMOPS_RETRY_SEND	1
#define WLC_MIMOPS_RETRY_NOACK	2

/* Check if any join/roam is being processed */
#ifdef STA
#define AS_IN_PROGRESS(wlc)	((wlc)->assoc_req[0] != NULL)
#endif

#ifdef WLSCANCACHE
/* Time in ms longer than which a cached scan result is considered stale for the purpose
 * of an association attempt. Should be a value short enough to ensure that a target AP
 * would not have reconfigured since we collected the scan result.
 */
#ifndef BCMWL_ASSOC_CACHE_TOLERANCE
#define BCMWL_ASSOC_CACHE_TOLERANCE	8000	/* 8 sec old scan results should be re-scanned */
#endif /* BCMWL_ASSOC_CACHE_TOLERANCE */
#endif /* WLSCANCACHE */
/*
 * Macros to check if AP or STA is active.
 * AP Active means more than just configured: driver and BSS are "up";
 * that is, we are beaconing/responding as an AP (aps_associated).
 * STA Active similarly means the driver is up and a configured STA BSS
 * is up: either associated (stas_associated) or trying.
 *
 * Macro definitions vary as per AP/STA ifdefs, allowing references to
 * ifdef'd structure fields and constant values (0) for optimization.
 * Make sure to enclose blocks of code such that any routines they
 * reference can also be unused and optimized out by the linker.
 */
/* NOTE: References structure fields defined in wlc.h */
#if !defined(AP) && !defined(STA)
#define AP_ACTIVE(wlc)	(0)
#define STA_ACTIVE(wlc)	(0)
#elif defined(AP) && !defined(STA)
#define AP_ACTIVE(wlc)	((wlc)->aps_associated)
#define STA_ACTIVE(wlc) (0)
#elif defined(STA) && !defined(AP)
#define AP_ACTIVE(wlc)	(0)
#define STA_ACTIVE(wlc)	((wlc)->stas_associated > 0 || AS_IN_PROGRESS(wlc))
#else /* implies AP && STA defined */
#define AP_ACTIVE(wlc)	((wlc)->aps_associated)
#define STA_ACTIVE(wlc)	((wlc)->stas_associated > 0 || AS_IN_PROGRESS(wlc))
#endif /* defined(AP) && !defined(STA) */

/*
 * Detect Card removed.
 * Even checking an sbconfig register read will not false trigger when the core is in reset.
 * it breaks CF address mechanism. Accessing gphy phyversion will cause SB error if aphy
 * is in reset on 4306B0-DB. Need a simple accessible reg with fixed 0/1 pattern
 * (some platforms return all 0).
 * If clocks are present, call the sb routine which will figure out if the device is removed.
 */
#ifdef WLC_HIGH_ONLY
#define DEVICEREMOVED(wlc)	(!wlc->device_present)
#else
#define DEVICEREMOVED(wlc)      \
	((wlc->hw->clk) ?   \
	((R_REG(wlc->hw->osh, &wlc->hw->regs->maccontrol) & \
	(MCTL_PSM_JMP_0 | MCTL_IHR_EN)) != MCTL_IHR_EN) : \
	(si_deviceremoved(wlc->hw->sih)))
#endif /* WLC_HIGH_ONLY */

#define	WLC_ACTION_ASSOC		1 /* Association request for MAC */
#define	WLC_ACTION_ROAM			2 /* Roaming request for MAC */
#define	WLC_ACTION_SCAN			3 /* Scan request for MAC */
#define	WLC_ACTION_QUIET		4 /* Quiet request for MAC */
#define	WLC_ACTION_RM			5 /* Radio Measure request for MAC */
#define	WLC_ACTION_ISCAN		6 /* Incremental Scan request for MAC */
#define	WLC_ACTION_RECREATE_ROAM	7 /* Roaming request for MAC */
#define WLC_ACTION_REASSOC		8 /* Reassociation request */
#define WLC_ACTION_RECREATE		9 /* Association recreate request */
#define WLC_ACTION_ESCAN        10
#define WLC_ACTION_ACTFRAME        	11 /* Action frame off home channel */	

/*
 * Simple macro to change a channel number to a chanspec. Useable until the channels we
 * support overlap in the A and B bands.
 */
#define WLC_CHAN2SPEC(chan)	((chan) <= CH_MAX_2G_CHANNEL ? \
	(uint16)((chan) | WL_CHANSPEC_BAND_2G) : (uint16)((chan) | WL_CHANSPEC_BAND_5G))

#define WLC_DOT11_BSSTYPE(infra) ((infra) == 0 ? DOT11_BSSTYPE_INDEPENDENT : \
	DOT11_BSSTYPE_INFRASTRUCTURE)

/* wsec macros */
#define UCAST_NONE(prsn)	(((prsn)->ucount == 1) && \
	((prsn)->unicast[0] == WPA_CIPHER_NONE))
#define UCAST_AES(prsn)		(wlc_rsn_ucast_lookup(prsn, WPA_CIPHER_AES_CCM))
#define UCAST_TKIP(prsn)	(wlc_rsn_ucast_lookup(prsn, WPA_CIPHER_TKIP))

#define MCAST_NONE(prsn)	((prsn)->multicast == WPA_CIPHER_NONE)
#define MCAST_AES(prsn)		((prsn)->multicast == WPA_CIPHER_AES_CCM)
#define MCAST_TKIP(prsn)	((prsn)->multicast == WPA_CIPHER_TKIP)
#define MCAST_WEP(prsn)		((prsn)->multicast == WPA_CIPHER_WEP_40 || \
	 (prsn)->multicast == WPA_CIPHER_WEP_104)

#define WLCWLUNIT(wlc)		((wlc)->pub->unit)

/* generic buffer length macro */
#define BUFLEN(start, end)	((end >= start) ? (int)(end - start) : 0)

#define BUFLEN_CHECK_AND_RETURN(len, maxlen, ret) \
{ \
	if ((int)(len) > (int)(maxlen)) {				\
		WL_ERROR(("%s, line %d, BUFLEN_CHECK_AND_RETURN: len = %d, maxlen = %d\n", \
			  __FUNCTION__, __LINE__, (int)(len), (int)(maxlen))); \
		ASSERT(0);						\
		return (ret);				\
	}								\
}

#define BUFLEN_CHECK_AND_RETURN_VOID(len, maxlen) \
{ \
	if ((int)(len) > (int)(maxlen)) {				\
		WL_ERROR(("%s, line %d, BUFLEN_CHECK_AND_RETURN_VOID: len = %d, maxlen = %d\n", \
			  __FUNCTION__, __LINE__, (int)(len), (int)(maxlen))); \
		ASSERT(0);						\
		return;							\
	}								\
}

typedef	struct wlc_protection {
	bool	_g;		/* use g spec protection, driver internal */
	int8	g_override;	/* override for use of g spec protection */
	uint8	gmode_user;	/* user config gmode, operating band->gmode is different */
	int8	overlap;	/* Overlap BSS/IBSS protection for both 11g and 11n */
	int8	nmode_user;	/* user config nmode, operating pub->nmode is different */
	int8	n_cfg;		/* use OFDM protection on MIMO frames */
	int8	n_cfg_override;	/* override for use of N protection */
	bool	nongf;		/* non-GF present protection */
	int8	nongf_override;	/* override for use of GF protection */
	int8	n_pam_override;	/* override for preamble: MM or GF */
	bool	n_obss;		/* indicated OBSS Non-HT STA present */

	uint	longpre_detect_timeout;	/* #sec until long preamble bcns gone */
	uint	barker_detect_timeout;	/* #sec until bcns signaling Barker long preamble */
					/* only is gone */
	uint	ofdm_ibss_timeout;	/* #sec until ofdm IBSS beacons gone */
	uint	ofdm_ovlp_timeout;	/* #sec until ofdm overlapping BSS bcns gone */
	uint	nonerp_ibss_timeout;	/* #sec until nonerp IBSS beacons gone */
	uint	nonerp_ovlp_timeout;	/* #sec until nonerp overlapping BSS bcns gone */
	uint	g_ibss_timeout;		/* #sec until bcns signaling Use_Protection gone */
	uint	n_ibss_timeout;		/* #sec until bcns signaling Use_OFDM_Protection gone */
	uint	ht20in40_ovlp_timeout;	/* #sec until 20MHz overlapping OPMODE gone */
	uint	ht20in40_ibss_timeout;	/* #sec until 20MHz-only HT station bcns gone */
	uint	non_gf_ibss_timeout;	/* #sec until non-GF bcns gone */
} wlc_protection_t;

enum txcore_index {
	CCK_IDX = 0,	/* CCK txcore index */
	OFDM_IDX,
	NSTS1_IDX,
	NSTS2_IDX,
	NSTS3_IDX,
	NSTS4_IDX,
	MAX_CORE_IDX
};

/* anything affects the single/dual streams/antenna operation */
typedef struct wlc_stf {
	uint8	hw_txchain;		/* HW txchain bitmap cfg */
	uint8	txchain;		/* txchain bitmap being used */
	uint8	txstreams;		/* number of txchains being used */

	uint8	hw_rxchain;		/* HW rxchain bitmap cfg */
	uint8	rxchain;		/* rxchain bitmap being used */
	uint8	rxstreams;		/* number of rxchains being used */

	uint8 	ant_rx_ovr;		/* rx antenna override */
	int8	txant;			/* userTx antenna setting */
	uint16	phytxant;		/* phyTx antenna setting in txheader */

	uint8	ss_opmode;		/* singlestream Operational mode, 0:siso; 1:cdd */
	bool	ss_algosel_auto;	/* if TRUE, use wlc->stf->ss_algo_channel; */
					/* else use wlc->band->stf->ss_mode_band; */
	uint16	ss_algo_channel;	/* ss based on per-channel algo: 0: SISO, 1: CDD 2: STBC */
	uint8   no_cddstbc;		/* stf override, 1: no CDD (or STBC) allowed */

	uint8   rxchain_restore_delay;	/* delay time to restore default rxchain */

	int8	ldpc;			/* ON/OFF ldpc RX supported */
	int8	ldpc_tx;		/* AUTO/ON/OFF ldpc TX supported */
	uint8	txcore[MAX_CORE_IDX][2];	/* bitmap of selected core for each Nsts */
	int8	spatial_policy;
	uint16	shmem_base;

	bool	tempsense_disable;	/* disable periodic tempsense check */
	uint	tempsense_period;	/* period to poll tempsense */
	uint 	tempsense_lasttime;

	uint	ipaon;			/* IPA on/off, assume it's the same for 2g and 5g */
} wlc_stf_t;

#define WLC_STF_SS_STBC_TX(wlc, scb) \
	(((wlc)->stf->txstreams > 1) && (((wlc)->band->band_stf_stbc_tx == ON) || \
	 (SCB_STBC_CAP((scb)) &&					\
	  (wlc)->band->band_stf_stbc_tx == AUTO &&			\
	  isset(&((wlc)->stf->ss_algo_channel), PHY_TXC1_MODE_STBC))))

#define WLC_STBC_CAP_PHY(wlc) ((WLCISNPHY(wlc->band) && NREV_GE(wlc->band->phyrev, 3)) || \
	WLCISHTPHY(wlc->band))

#define WLC_SGI_CAP_PHY(wlc) ((WLCISNPHY(wlc->band) && NREV_GE(wlc->band->phyrev, 3)) || \
	WLCISHTPHY(wlc->band) || WLCISLCNPHY(wlc->band))

#define WLC_LDPC_CAP_PHY(wlc) (WLCISHTPHY(wlc->band))

/* 802.11h Quiet Period */
typedef struct wlc_quiet {
	int duration; 		/* TU's */
	int period;
	int offset; 		/* TU's offset from TBTT in Count field */
	uint32 _state;
	int countdown;		/* Beacons from now when quiet period will start */
	uint32 start_prep_tsf;	/* When to start preparing to be quiet(ie shutdown FIFOs)
							 */
	uint32 start_tsf;	/* When to start being quiet */
	uint32 end_tsf;	/* When we can start xmiting again */
	/* dual purpose: wait for offset with beacon & wait for duration */
	struct wl_timer *timer;
} wlc_quiet_t;

typedef struct wlc_rm_req {
	int8	type;		/* type of measurement */
	int8	flags;
	int	token;		/* token for this particular measurement */
	chanspec_t chanspec;	/* channel for the measurement */
	uint32	tsf_h;		/* TSF high 32-bits of Measurement Request start time */
	uint32	tsf_l;		/* TSF low 32-bits */
	int	dur;		/* TUs */
} wlc_rm_req_t;

typedef struct wlc_rm_req_state {
	int	report_class;	/* type of report to generate */
	bool	broadcast;	/* was the request DA broadcast */
	int	token;		/* overall token for measurement set */
	uint	step;		/* current state of RM state machine */
	chanspec_t	chanspec_return;	/* channel to return to after the measurements */
	bool	ps_pending;	/* true if we need to wait for PS to be announced before
				 * off-channel measurement
				 */
	int	dur;		/* TUs, min duration of current parallel set measurements */
	uint32	actual_start_h;	/* TSF high 32-bits of actual start time */
	uint32	actual_start_l;	/* TSF low 32-bits */
	int	cur_req;	/* index of current measure request */
	int	req_count;	/* number of measure requests */
	wlc_rm_req_t*	req;	/* array of requests */
	/* CCA measurements */
	bool	cca_active;	/* true if measurement in progress */
	int	cca_dur;	/* TU, specified duration */
	int	cca_idle;	/* idle carrier time reported by ucode */
	uint8	cca_busy;	/* busy fraction */
	/* RPI measurements */
	bool	rpi_active;	/* true if measurement in progress */
	bool	rpi_end;	/* signal last sample */
	int	rpi_dur;	/* TU, specified duration */
	int	rpi_sample_num;	/* number of samples collected */
	uint16	rpi[WL_RPI_REP_BIN_NUM];	/* rpi/rssi measurement values */
	int	rssi_sample_num; /* count of samples in averaging total */
	int	rssi_sample;	/* current sample averaging total */
	void *		cb;	/* completion callback fn: may be NULL */
	void *		cb_arg;	/* single arg to callback function */
} wlc_rm_req_state_t;

/*
 * Multiband 101:
 *
 * A "band" is a radio frequency/channel range.
 * We support the 802.11b band (2.4Ghz) using the B or G phy and the 2050 radio.
 * We support the 802.11a band (5  Ghz) using the A      phy and the 2060 radio.
 *
 * Some chips and boards support a single band (uniband) and some
 * support both B/G and A bands (multiband).
 *
 * Versions of the dot11 io core <= 4 support a single phytype/band.
 * Versions of the dot11 io core >= 5 support multiple phytypes.
 * The sbtmstatelow.gmode flag is used to switch between phytypes.
 *
 * 4306B0 (corerev4) includes two dot11 cores, one with a Gphy and one with an Aphy.
 * 4306C0 and later chips (>= corerev5) include a single dot11 core having
 * some combination of A, B, and/or G cores .
 * The driver multiband design abstracts the core portion
 * (such as dma engines and pio fifos) from the band portion (phy+radio state).
 *
 * For convenience, band 0 is always the B/G phy, and, if it exists,
 * band 1 is the A phy.
 */


/*
 * core state (mac)
 */
typedef struct wlccore {
#ifdef WLC_LOW
	uint		coreidx;		/* # sb enumerated core */

	/* fifo */
	uint		*txavail[NFIFO];	/* # tx descriptors available */
	int16		txpktpend[NFIFO];	/* tx admission control */
#endif /* WLC_LOW */

#ifdef WLC_HIGH
	macstat_t	*macstat_snapshot;	/* mac hw prev read values */
#endif
} wlccore_t;

/*
 * band state (phy+ana+radio)
 */
typedef struct wlcband {
	int		bandtype;		/* WLC_BAND_2G, WLC_BAND_5G */
	uint		bandunit;		/* bandstate[] index */

	uint16		phytype;		/* phytype */
	uint16		phyrev;
	uint16		radioid;
	uint16		radiorev;
	wlc_phy_t	*pi;			/* pointer to phy specific information */
	bool		abgphy_encore;

#ifdef WLC_HIGH
	uint8		gmode;			/* currently active gmode (see wlioctl.h) */

	struct scb	*hwrs_scb;		/* permanent scb for hw rateset */

	wlc_rateset_t	defrateset;		/* band-specific copy of default_bss.rateset */

	ratespec_t 	rspec_override;		/* 802.11 rate override */
	ratespec_t	mrspec_override;	/* multicast rate override */
	uint8		band_stf_ss_mode;	/* Configured STF type, 0:siso; 1:cdd */
	int8		band_stf_stbc_tx;	/* STBC TX 0:off; 1:force on; -1:auto */
	wlc_rateset_t	hw_rateset;		/* rates supported by chip (phy-specific) */
	uint8		basic_rate[WLC_MAXRATE + 1]; /* basic rates indexed by rate */
	bool		mimo_cap_40;		/* 40 MHz cap enabled on this band */
	int		roam_trigger;		/* "good enough" threshold for current AP */
	uint		roam_delta;		/* "signif better" thresh for new AP candidates */
	int		roam_trigger_init_def;	/* roam trigger default value in intialization */
	int		roam_trigger_def;	/* roam trigger default value */
	uint		roam_delta_def;		/* roam delta default value */
	int8		antgain;		/* antenna gain from srom */

	uint16		CWmin;	/* The minimum size of contention window, in unit of aSlotTime */
	uint16		CWmax;	/* The maximum size of contention window, in unit of aSlotTime */
	uint16		bcntsfoff;		/* beacon tsf offset */
#endif /* WLC_HIGH */
} wlcband_t;

/* generic function callback takes just one arg */
typedef void (*cb_fn_t)(void *);

#ifdef WLC_HIGH
/* generic function callback takes a context arg and status arg */
typedef void (*scancb_fn_t)(void *, int, wlc_bsscfg_t *cfg);
#endif

/* tx completion callback takes 3 args */
typedef void (*pkcb_fn_t)(wlc_info_t *wlc, uint txstatus, void *arg);

typedef struct pkt_cb {
	pkcb_fn_t fn;		/* function to call when tx frame completes */
	void *arg;		/* void arg for fn */
	uint8 nextidx;		/* index of next call back if threading */
	bool entered;		/* recursion check */
} pkt_cb_t;

#define WLC_MAX_BRCM_ELT	32	/* Max size of BRCM proprietary elt */

typedef struct wlc_dfs_cac {
	uint	cactime_preism;		/* configured preism cac time in second */
	uint	cactime_postism;	/* configured postism cac time in second */
	uint32	nop_sec;		/* Non-Operation Period in second */
	int	ism_monitor;		/* 0 for off, non-zero to force ISM to monitor-only mode */
	chanspec_t chanspec_forced;		/* next dfs channel is forced to this one */
	wl_dfs_status_t	status;		/* data structure for handling dfs_status iovar */
	uint	cactime;      /* holds cac time in WLC_DFS_RADAR_CHECK_INTERVAL for current state */
	/* use of duration
	 * 1. used as a down-counter where timer expiry is needed.
	 * 2. (cactime - duration) is time elapsed at current state
	 */
	uint	duration;
	chanspec_t chanspec_next;		/* next dfs channel */
	bool	timer_running;
} wlc_dfs_cac_t;

	/* module control blocks */
typedef	struct modulecb {
	char name[32];               /* module name : NULL indicates empty array member */
	const bcm_iovar_t *iovars;	/* iovar table */
	void *hdl;			/* handle passed when handler 'doiovar' is called */
	watchdog_fn_t watchdog_fn;	/* watchdog handler */
	iovar_fn_t iovar_fn;		/* iovar handler */
	down_fn_t down_fn;		/* down handler. Note: the int returned
					 * by the down function is a count of the
					 * number of timers that could not be
					 * freed.
					 */
} modulecb_t;

	/* dump control blocks */
typedef	struct dumpcb_s {
	const char *name;		/* dump name */
	dump_fn_t dump_fn;		/* 'wl dump' handler */
	void *dump_fn_arg;
	struct dumpcb_s *next;
} dumpcb_t;

/* Allowed transmit features for scb */
typedef enum scb_txmod {
	TXMOD_START, /* Node to designate start of the tx path */
	TXMOD_DPT,
	TXMOD_CRAM,
	TXMOD_APPS,
	TXMOD_AMSDU,
	TXMOD_BA,
	TXMOD_AMPDU,
	TXMOD_TRANSMIT,	/* Node to designate enqueue to wlc->txq */
	TXMOD_LAST
} scb_txmod_t;

/* PM=2 receive duration duty cycle states */
typedef enum _pm2rd_state_t {
	PM2RD_IDLE,			/* In an idle DTIM period with no data to receive and
	                     * no receive duty cycle active.  ie. the last
						 * received beacon had a cleared TIM bit.
						 */
	PM2RD_WAIT_BCN,		/* In the OFF part of the receive duty cycle.
	                     * In PS mode, waiting for next beacon.
					     */
	PM2RD_WAIT_TMO,		/* In the ON part of the receive duty cycle.
	                     * Out of PS mode, waiting for pm2_rcv timeout.
					     */
	PM2RD_WAIT_RTS_ACK	/* Transitioning from the ON part to the OFF part of
	                     * the receive duty cycle.
						 * Started entering PS mode, waiting for a
						 * PM-indicated ACK from AP to complete entering
						 * PS mode.
						 */
} pm2rd_state_t;

/* Function that does transmit packet feature processing. prec is precedence of the packet */
typedef void (*txmod_tx_fn_t)(void *ctx, struct scb *scb, void *pkt, uint prec);

/* Callback for txmod when it gets deactivated by other txmod */
typedef void (*txmod_deactivate_fn_t)(void *ctx, struct scb *scb);

/* Callback for txmod when it gets activated */
typedef void (*txmod_activate_fn_t)(void *ctx, struct scb *scb);

/* Callback for txmod to return packets held by this txmod */
typedef uint (*txmod_pktcnt_fn_t)(void *ctx);

/* Function vector to make it easy to initialize txmod
 * Note: txmod_info itself is not modified to avoid adding one more level of indirection
 * during transmit of a packet
 */
typedef struct txmod_fns {
	txmod_tx_fn_t		tx_fn;			/* Process the packet */
	txmod_pktcnt_fn_t	pktcnt_fn;		/* Return the packet count */
	txmod_deactivate_fn_t	deactivate_notify_fn;	/* Handle the deactivation of the feature */
	txmod_activate_fn_t	activate_notify_fn;	/* Handle the activation of the feature */
} txmod_fns_t;

/* Structure to store registered functions for a Txmod */
typedef struct txmod_info {
	txmod_tx_fn_t		tx_fn;			/* Process the packet */
	txmod_deactivate_fn_t	deactivate_notify_fn;	/* Handle the deactivation of the feature */
	txmod_activate_fn_t	activate_notify_fn;	/* Handle the activation of the feature */
	txmod_pktcnt_fn_t	pktcnt_fn;		/* Return the packet count */
	void *ctx; 					/* Opaque handle to be passed */
} txmod_info_t;

#define OBSS_CHANVEC_SIZE	CEIL(CH_MAX_2G_CHANNEL + 1, NBBY)
typedef struct obss_info {
	bool		coex_enab;		/* 20/40 MHz BSS Management enabled */
	bool		coex_permit;	/* 20/40 operation permitted */
	uint8		coex_det;		/* 40 MHz Intolerant device detected */
	uint8		coex_ovrd;		/* 40 MHz Intolerant device detected */
	bool		switch_bw_deferred;	/* defer the switch */
	uint8		coex_bits_buffered;	/* buffer coexistence bits due to scan */
	uint8		num_chan;		/* num of 2G channels */
	uint16		scan_countdown;		/* timer for schedule next OBSS scan */
	uint8		chanvec[OBSS_CHANVEC_SIZE]; /* bitvec of channels in 2G */
	uint8		coex_map[CH_MAX_2G_CHANNEL + 1]; /* channel map of coexist in 2G */
	uint32		fid_time;		/* time when 40MHz intolerant device detected */
	uint32		coex_te_mask;	/* mask for trigger events */
	obss_params_t	ap_params;		/* AP Overlapping BSS scan parameters */
	obss_params_t	sta_params;		/* STA Overlapping BSS scan parameters */
} obss_info_t;

#define WLC_INTOL40_DET(wlc) (((wlc)->obss->coex_det & WL_COEX_40MHZ_INTOLERANT) != 0)
#define WLC_WIDTH20_DET(wlc) (((wlc)->obss->coex_det & WL_COEX_WIDTH20) != 0)
#define WLC_INTOL40_OVRD(wlc) (((wlc)->obss->coex_ovrd & WL_COEX_40MHZ_INTOLERANT) != 0)
#define WLC_WIDTH20_OVRD(wlc) (((wlc)->obss->coex_ovrd & WL_COEX_WIDTH20) != 0)
#define WLC_COEX_STATE_BITS(bit) (bit & (WL_COEX_40MHZ_INTOLERANT | WL_COEX_WIDTH20))

#define COEX_MASK_TEA	0x1
#define COEX_MASK_TEB	0x2
#define COEX_MASK_TEC	0x4
#define COEX_MASK_TED	0x8

/* coex bw downgrade reason code */
#define COEX_UPGRADE_TIMER 	0
#define COEX_DGRADE_TEA		1
#define COEX_DGRADE_40INTOL 2
#define COEX_DGRADE_TEBC	3
#define COEX_DGRADE_TED		4

#ifdef WLC_LOW
typedef struct wlc_hwband {
	int		bandtype;		/* WLC_BAND_2G, WLC_BAND_5G */
	uint		bandunit;		/* bandstate[] index */
	uint16		mhfs[MHFMAX];		/* MHF array shadow */
	uint8		bandhw_stf_ss_mode;	/* HW configured STF type, 0:siso; 1:cdd */
	uint16		CWmin;
	uint16		CWmax;
	uint32          core_flags;

	uint16		phytype;		/* phytype */
	uint16		phyrev;
	uint16		radioid;
	uint16		radiorev;
	wlc_phy_t	*pi;			/* pointer to phy specific information */
	bool		abgphy_encore;
}  wlc_hwband_t;
#endif /* WLC_LOW */

#if defined(DMA_TX_FREE)
#define WLC_TXSTATUS_SZ 128
#define WLC_TXSTATUS_VEC_SZ	(WLC_TXSTATUS_SZ/NBBY)
typedef struct wlc_txstatus_flags {
	uint	head;
	uint	tail;
	uint8	vec[WLC_TXSTATUS_VEC_SZ];
}  wlc_txstatus_flags_t;
#endif /* DMA_TX_FREE */

struct wlc_hw_info {
#ifdef WLC_SPLIT
	rpc_info_t	*rpc;			/* Handle to RPC module */
#endif
	osl_t		*osh;			/* pointer to os handle */
	bool		_piomode;		/* true if pio mode */
	wlc_info_t	*wlc;

	/* fifo */
	hnddma_t	*di[NFIFO];		/* hnddma handles, per fifo */
	pio_t		*pio[NFIFO];		/* pio handlers, per fifo */

#ifdef WLC_LOW
	uint		unit;			/* device instance number */

	/* version info */
	uint16		vendorid;		/* PCI vendor id */
	uint16		deviceid;		/* PCI device id */
	uint		corerev;		/* core revision */
	uint8		sromrev;		/* version # of the srom */
	uint16		boardrev;		/* version # of particular board */
	uint32		boardflags;		/* Board specific flags from srom */
	uint32		boardflags2;		/* More board flags if sromrev >= 4 */

	uint32		machwcap;		/* MAC capabilities (corerev >= 13) */
	uint32		machwcap_backup;	/* backup of machwcap (corerev >= 13) */
	uint16		ucode_dbgsel;		/* dbgsel for ucode debug(config gpio) */
	bool		ucode_loaded;		/* TRUE after ucode downloaded */
	bool		_p2p_hw;

	si_t		*sih;			/* SB handle (cookie for siutils calls) */
	char		*vars;			/* "environment" name=value */
	uint		vars_size;		/* size of vars, free vars on detach */
	d11regs_t	*regs;			/* pointer to device registers */
	void		*physhim;		/* phy shim layer handler */
	void		*phy_sh;		/* pointer to shared phy state */
	wlc_hwband_t	*band;			/* pointer to active per-band state */
	wlc_hwband_t	*bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	uint16		bmac_phytxant;		/* cache of high phytxant state */
	bool		shortslot;		/* currently using 11g ShortSlot timing */
	uint16		SRL;			/* 802.11 dot11ShortRetryLimit */
	uint16		LRL;			/* 802.11 dot11LongRetryLimit */
	uint16		SFBL;			/* Short Frame Rate Fallback Limit */
	uint16		LFBL;			/* Long Frame Rate Fallback Limit */

	bool		up;			/* d11 hardware up and running */
	uint		now;			/* # elapsed seconds */
	uint		_nbands;		/* # bands supported */
	chanspec_t	chanspec;		/* bmac chanspec shadow */

	uint		*txavail[NFIFO];	/* # tx descriptors available */
	uint16		*xmtfifo_sz;		/* fifo size in 256B for each xmt fifo */

	mbool		pllreq;			/* pll requests to keep PLL on */

	uint8		suspended_fifos;	/* Which TX fifo to remain awake for */
	uint32		maccontrol;		/* Cached value of maccontrol */
	uint		mac_suspend_depth;	/* current depth of mac_suspend levels */
	uint32		wake_override;		/* Various conditions to force MAC to WAKE mode */
	uint32		mute_override;		/* Prevent ucode from sending beacons */
	struct		ether_addr etheraddr;	/* currently configured ethernet address */
	uint32		led_gpio_mask;		/* LED GPIO Mask */
	uint32		btc_gpio_mask;		/* Resolved GPIO Mask */
	uint32		btc_gpio_out;		/* Resolved GPIO out pins */
	int		btc_mode;		/* Bluetooth Coexistence mode */
	int		btc_wire;		/* BTC: 2-wire or 3-wire */

	bool		noreset;		/* true= do not reset hw, used by WLC_OUT */
	bool		forcefastclk;		/* true if the h/w is forcing the use of fast clk */
	bool		clk;			/* core is out of reset and has clock */
	bool		sbclk;			/* sb has clock */
	bmac_pmq_t *bmac_pmq;  /*  bmac PM states derived from ucode PMQ */
	bool		phyclk;			/* phy is out of reset and has clock */
	bool		dma_lpbk;		/* core is in DMA loopback */

#ifdef WLLED
	bmac_led_info_t	*ledh;			/* pointer to led specific information */
#endif

#ifdef WLC_LOW_ONLY
	struct wl_timer *wdtimer;		/* timer for watchdog routine */
	struct ether_addr orig_etheraddr;	/* original hw ethernet address */
	uint16		rpc_dngl_agg;		/* rpc agg control for dongle */
#ifdef DMA_TX_FREE
	wlc_txstatus_flags_t	txstatus_ampdu_flags[NFIFO];
#endif
#ifdef WLEXTLOG
	wlc_extlog_info_t	*extlog;	/* external log handle */
#endif
	uint32		mem_required_def;	/* memory required to replenish RX DMA ring */
	uint32		mem_required_lower;	/* memory required with lower RX bound */
	uint32		mem_required_least;	/* minimum memory requirement to handle RX */

#endif	/* WLC_LOW_ONLY */
#endif /* WLC_LOW */

	uint8		hw_stf_ss_opmode;		/* STF single stream operation mode */

	uint8		antsel_type;		/* Type of boardlevel mimo antenna switch-logic
						 * 0 = N/A, 1 = 2x4 board, 2 = 2x3 CB2 board
						 */
	uint32          antsel_avail;           /* put antsel_info_t here if more info is needed */
	uint16          btc_flags;   /* BTC configurations */
	uint8       btcx_prot_rssi_thresh; /* rssi threshold for forcing protection */
	uint8       btcx_ampdutx_rssi_thresh; /* rssi threshold to turn off ampdutx */
	uint8       btcx_ampdurx_rssi_thresh; /* rssi threshold to turn off ampdurx */

};

#if defined(DELTASTATS)
#define DELTA_STATS_NUM_INTERVAL	2	/* number of stats intervals stored */

/* structure to store info for delta statistics feature */
typedef struct delta_stats_info {
	uint32 interval;		/* configured delta statistics interval (secs) */
	uint32 seconds;			/* counts seconds to next interval */
	uint32 current_index;	/* index into stats of the current statistics */
	wl_delta_stats_t stats[DELTA_STATS_NUM_INTERVAL];	/* statistics for each interval */
} delta_stats_info_t;
#endif

/* structure for chanim support */
#ifdef WLCHANIM
/* normalized count struct */
typedef struct chanim_stats {
	struct chanim_stats *next;
	uint32 glitchcnt;               /* normalized as per second count */
	uint8 ccastats[CCASTATS_MAX]; 	/* normalized as 0-255 */
	chanspec_t chanspec;
} chanim_stats_t;

/* accumulative  count structure */
typedef struct {
	uint32 glitch_cnt;
	uint32 ccastats_us[CCASTATS_MAX]; /* in microsecond */
	chanspec_t chanspec;
	uint stats_ms; /* accumulative time, in millisecond */
} chanim_accum_t;

/* basic count struct */
typedef struct {
	uint16 glitch_cnt;
	uint32 ccastats_cnt[CCASTATS_MAX];
	uint timestamp;
} chanim_cnt_t;


/* configurable parameters */
typedef struct {
	uint8 mode;		/* configurable chanim mode */
	uint8 sample_period;	/* in seconds, time to do a sampling measurement */
	uint8 threshold_time;	/* number of sample period to trigger an action */
	uint8 max_acs;			/* the maximum acs scans for one lockout period */
	uint32 lockout_period;	/* in seconds, time for one lockout period */
	uint32 crsglitch_thres;
	uint32 ccastats_thres;
	uint scb_max_probe; 	/* when triggered by intf, how many times to probe */
} chanim_config_t;

/* transient counters/stamps */
typedef struct {
	uint32 detecttime;
	uint32 flags;
	uint8 record_idx; /* where the next acs record should locate */
	uint scb_max_probe; /* original number of scb probe to conduct */
	uint scb_timeout; /* the storage for the original scb timeout that is swapped */
} chanim_mark_t;

typedef struct {
	chanim_stats_t *stats; /* normalized stats */
	chanim_accum_t accum_cnt[CHANIM_ACCUM_CNT]; /* accumulative cnt */
	chanim_cnt_t last_cnt; /* count from last read */	
	chanim_mark_t mark;
	chanim_config_t config;
	chanim_acs_record_t record[CHANIM_ACS_RECORD];
} chanim_info_t;

#define chanim_mark(c_info)	(c_info)->mark
#define chanim_config(c_info) (c_info)->config
#define chanim_act_delay(c_info) \
	(chanim_config(c_info).sample_period * chanim_config(c_info).threshold_time)

#endif /* WLCHANIM */

typedef struct {
	uint32 txdur;	/* num usecs tx'ing */
	uint32 ibss;	/* num usecs rx'ing cur bss */
	uint32 obss;	/* num usecs rx'ing other data */
	uint32 noctg;	/* 802.11 of unknown type */
	uint32 nopkt;	/* non 802.11 */
	uint32 usecs;	/* usecs in this interval */
	uint32 PM;	/* usecs MAC spent in doze mode for PM */
} cca_ucode_counts_t;

#ifdef CCA_STATS
#define CCA_POOL_MAX		300
#define CCA_FREE_BUF		0xffff

typedef uint16 cca_idx_t;
typedef struct {
    chanspec_t chanspec;
    cca_idx_t  secs[MAX_CCA_SECS];
} cca_congest_channel_t;

typedef struct {
	cca_ucode_counts_t last_cca_stats;	/* Previously read values, for computing deltas */
	cca_congest_channel_t     chan_stats[MAX_CCA_CHANNELS];
	int             cca_second;		/* which second bucket we are using */
	int             cca_second_max;		/* num of seconds to track */
	int		alloc_fail;
	cca_congest_t	cca_pool[CCA_POOL_MAX];
} cca_info_t;

#define CCA_POOL_DATA(cca, chanspec, second) \
	(&(cca->cca_pool[cca->chan_stats[chanspec].secs[second]]))
#define CCA_POOL_IDX(cca, chanspec, second) \
	(cca->chan_stats[chanspec].secs[second])

void cca_stats_upd(wlc_info_t *wlc, int calculate);
#endif /* CCA_STATS */

/* Source Address based duplicate detection (Avoid heavy SCBs) */
#define SA_SEQCTL_SIZE 10

struct sa_seqctl {
	struct ether_addr sa;
	uint16 seqctl_nonqos;
};

/*
 * Principal common (os-independent) software data structure.
 */
struct wlc_info {
	wlc_pub_t	*pub;			/* pointer to wlc public state */
	osl_t		*osh;			/* pointer to os handle */
	void		*wl;			/* pointer to os-specific private state */
	d11regs_t	*regs;			/* pointer to device registers */

	wlc_hw_info_t	*hw;			/* HW related state used primarily by BMAC */
#ifdef WLC_SPLIT
	rpc_info_t	*rpc;			/* Handle to RPC module */
#endif

	/* clock */
	int		clkreq_override;	/* setting for clkreq for PCIE : Auto, 0, 1 */
	uint16		fastpwrup_dly;		/* time in us needed to bring up d11 fast clock */

	/* interrupt */
	uint32		macintstatus;		/* bit channel between isr and dpc */
	uint32		macintmask;		/* sw runtime master macintmask value */
	uint32		defmacintmask;		/* default "on" macintmask value */

	/* up and down */
	bool		device_present;		/* (removable) device is present */

	bool		clk;			/* core is out of reset and has clock */

	/* multiband */
	wlccore_t	*core;			/* pointer to active io core */
	wlcband_t	*band;			/* pointer to active per-band state */
	wlccore_t	*corestate;		/* per-core state (one per hw core) */
	wlcband_t	*bandstate[MAXBANDS];		/* per-band state (one per phy/radio) */

	bool		war16165;		/* PCI slow clock 16165 war flag */
	bool		btc_stuck_war50943;	/* WAR to handle stuck BT_RFACT */
	bool		btc_stuck_detected;	/* stuck BT_RFACT has been detected */
	uint16		bt_shm_addr;

	bool		tx_suspended;		/* data fifos need to remain suspended */

	uint		txpend16165war;
	int8		phy_noise_list[MAXCHANNEL];	/* noise right after tx */

	/* packet queue */
	uint		qvalid;			/* DirFrmQValid and BcMcFrmQValid */

	/* BMAC_NOTE: wlc_phy code requires this */
	uint32		radar;			/* radar info: just on or off for now */

	/* Regulatory power limits */
	int8		txpwr_local_max;	/* regulatory local txpwr max */
	uint8		txpwr_local_constraint;	/* local power contraint in dB */


#ifdef WLC_HIGH

#ifdef WLC_HIGH_ONLY
	rpctx_info_t	*rpctx;			/* RPC TX module */
	bool		reset_bmac_pending;	/* bmac reset is in progressing */
	uint32		rpc_agg;		/* host agg: bit 16-31, bmac agg: bit 0-15 */
	uint32		rpc_msglevel;		/* host rpc: bit 16-31, bmac rpc: bit 0-15 */
#ifdef DNGL_WD_KEEP_ALIVE
    bool        dngl_wd_keep_alive;   /* enable or disable dngl keep alive timer */
#endif
#endif /* WLC_HIGH_ONLY */

	/* sub-module handler */
	wlc_seq_cmds_info_t	*seq_cmds_info;	/* pointer to sequence commands info */
#ifdef WLLED
	led_info_t	*ledh;			/* pointer to led specific information */
#endif
#ifdef CRAM
	cram_info_t	*crami;			/* cram module handler */
#endif
#ifdef WMF
	wmf_info_t	*wmfi;			/* wmf module handler */
#endif
#ifdef WLAMSDU
	amsdu_info_t	*ami;			/* amsdu module handler */
#endif
#ifdef WLBA
	ba_info_t	*bastate;		/* pointer to ba module state */
#endif
#ifdef WLAMPDU
	ampdu_info_t	*ampdu;			/* ampdu module handler */
#endif
#ifdef WOWL
	wowl_info_t	*wowl;			/* WOWL module handler */
#endif
#ifdef BCMAUTH_PSK
	wlc_auth_info_t	*authi;			/* authenticator module handler shim */
#endif
#ifdef WLBDD
	bdd_info_t	*bdd;			/* bdd module handler */
#endif
#ifdef WLP2P
	p2p_info_t	*p2p;			/* p2p module handler */
#endif
#ifdef WLBTAMP
	bta_info_t	*bta;			/* bta module handler */
#endif
#ifdef WLMFG
	mfg_info_t	*mfg;			/* mfg module handler */
#endif
#ifdef WLEXTLOG
	wlc_extlog_info_t *extlog;		/* extlog handler */
#endif
	antsel_info_t	*asi;			/* antsel module handler */
	wlc_cm_info_t	*cmi;			/* channel manager module handler */
	ratesel_info_t	*rsi;			/* ratesel module handler */
	wlc_cac_t	*cac;			/* CAC handler */
	wlc_scan_info_t	*scan;			/* ptr to scan state info */
	wlc_plt_pub_t	*plt;			/* plt(Production line Test) module handler */


#ifdef WET
	void		*weth;			/* pointer to wet specific information */
#endif
	void		*btparam;		/* bus type specific cookie */

	uint		vars_size;		/* size of vars, free vars on detach */
	int		btc_wire;		/* BTC: 2-wire or 3-wire */

	uint16		vendorid;		/* PCI vendor id */
	uint16		deviceid;		/* PCI device id */
	uint		ucode_rev;		/* microcode revision */

	uint32		machwcap;		/* MAC capabilities, BMAC shadow */

	struct ether_addr perm_etheraddr;	/* original sprom local ethernet address */

	bool		bandlocked;		/* disable auto multi-band switching */
	bool		bandinit_pending;	/* track band init in auto band */

	bool		radio_monitor;		/* radio timer is running */
	bool		down_override;		/* true=down */
	bool		going_down;		/* down path intermediate variable */

#ifdef WLSCANCACHE
	bool		_assoc_cache_assist;	/* enable use of scan cache in assoc attempts */
#endif
	bool		mpc;			/* enable minimum power consumption */
	bool		mpc_out;		/* disable radio_mpc_disable for out */
	bool		mpc_scan;		/* disable radio_mpc_disable for scan */
	bool		mpc_join;		/* disable radio_mpc_disable for join */
	bool		mpc_oidscan;		/* disable radio_mpc_disable for oid scan */
	bool		mpc_oidjoin;		/* disable radio_mpc_disable for oid join */
	bool		mpc_oidnettype;		/* disable radio_mpc_disable for oid
						 * network_type_in_use
						 */
	uint8		mpc_dlycnt;		/* # of watchdog cnt before turn disable radio */
	uint8		mpc_offcnt;		/* # of watchdog cnt that radio is disabled */
	uint8		mpc_delay_off;		/* delay radio disable by # of watchdog cnt */

	/* timer */
	struct wl_timer *wdtimer;		/* timer for watchdog routine */
	uint		fast_timer;		/* Periodic timeout for 'fast' timer */
	uint		slow_timer;		/* Periodic timeout for 'slow' timer */
	uint		glacial_timer;		/* Periodic timeout for 'glacial' timer */
	uint		phycal_mlo;		/* last time measurelow calibration was done */
	uint		phycal_txpower;		/* last time txpower calibration was done */

	struct wl_timer *radio_timer;		/* timer for hw radio button monitor routine */
	struct wl_timer *pspoll_timer;		/* periodic pspoll timer */


#if defined(DSLCPE) && defined(DSLCPE_DELAY)
	struct wl_timer *deferred_dpc_timer;	/* timer to restart dpc deferred during  */
						/* long delay */
#endif
#if defined(DSLCPE) && defined(DSLCPE_IDLE_PWRSAVE)
	struct {
		struct wl_timer *idle_pwrsave_timer;	/* timer to togger for power save */
		uint 	old_now;			/* saved now value */
	} idle_pwrsave;
	
#endif /* defined(DSLCPE) && defined(DSLCPE_IDLE_PWRSAVE) */
#ifdef DSLCPE_RX_DELAY
	struct wl_task *bmac_rx_task;	/* work queue for wlc_bmac_recv() */
	bool bmac_rx_task_dispatched;
	bool enable_rx_delay;
#endif
#ifdef DSLCPE_TXRX_BOUND
	unsigned int rx_bound; /* max # packets for one tx handling */
#endif /* DSLCPE_TXRX_BOUND */
	/* promiscuous */
	uint32		monitor;		/* monitor (MPDU sniffing) mode */
	bool		bcnmisc_ibss;		/* bcns promisc mode override for IBSS */
	bool		bcnmisc_scan;		/* bcns promisc mode override for scan */
	bool		bcnmisc_monitor;	/* bcns promisc mode override for monitor */

	bool		channel_qa_active;	/* true if chan qual measurement in progress */

	uint8		bcn_wait_prd;		/* max waiting period (for beacon) in 1024TU */

	/* driver feature */
	bool		frameburst;		/* enable per-packet framebursting */
#ifdef WLAFTERBURNER
	bool		afterburner;		/* true if currently in afterburner mode */
	uint8		ab_wds_timeout;		/* afterburner capability of wds network */
	uint8		guard_count;	/* ab <--> n mode switchover guard time, secs */
	bool		ampdu_saved;	/* saved value for ab <--> n switchover */
	bool		amsdu_tx_saved;	/* saved value for ab <--> n switchover */
	int		wme_saved;		/* saved value for ab <--> n switchover */
#endif /* WLAFTERBURNER */
#ifdef WLAMSDU
	bool		_amsdu_rx;		/* true if currently amsdu deagg is enabled */
#endif
	bool		_amsdu_noack;		/* enable AMSDU no ack mode */
	bool		_rifs;			/* enable per-packet rifs */
	int32		rifs_advert;		/* RIFS mode advertisement */
	int32		rifs_mode;		/* RIFS mode in the HT Info IE */
	int8		sgi_tx;			/* sgi tx */
	bool		wet;			/* true if wireless ethernet bridging mode */
	bool		mac_spoof;		/* Original wireless ethernet, MAC Clone/Spoof */


	bool		txc;			/* true if tx header cache is currently enabled */
	bool		txc_policy;		/* 0:off 1:auto */
	bool		txc_sticky;		/* invalidate txc every second or not */
	uint32		txcgen;			/* tx header cache generation number */

	/* AP-STA synchronization, power save */
	bool		check_for_unaligned_tbtt; /* check unaligned tbtt flag */
	bool		PM_override;		/* no power-save flag, override PM(user input) */
	bool		PMenabled;		/* current power-management state (CAM or PS) */
	bool		PMpending;		/* waiting for tx status with PM indicated set */
	bool		PMblocked;		/* block any PSPolling in PS mode, used to buffer
						 * AP traffic, also used to indicate in progress
						 * of scan, rm, etc. off home channel activity.
						 */
	bool		PSpoll;			/* whether there is an outstanding PS-Poll frame */
	uint8		PM;			/* power-management mode (CAM, PS or FASTPS) */
	bool		PMawakebcn;		/* bcn recvd during current waking state */

	bool		WME_PM_blocked;		/* Can STA go to PM when in WME Auto mode */
	bool		wake;			/* host-specified PS-mode sleep state */
#ifdef DEBUG_TBTT
	uint32		prev_TBTT;		/* TSF when last TBTT indicated */
	bool		bad_TBTT;		/* lost the race and iterated */
#endif
	uint8		pspoll_prd;		/* pspoll interval in milliseconds */
	uint8		bcn_li_bcn;		/* beacon listen interval in # beacons */
	uint8		bcn_li_dtim;		/* beacon listen interval in # dtims */

	bool		WDarmed;		/* watchdog timer is armed */
	uint32		WDlast;			/* last time wlc_watchdog() was called */

	/* PM2 Receive Throttle Duty Cycle */
	uint16		pm2_rcv_percent;	/* Duty cycle ON % in each bcn interval */
	uint16		pm2_rcv_ticks;		/* Duty cycle ON time in 10 ms ticks */
	pm2rd_state_t	pm2_rcv_state;	/* Duty cycle state */
	uint16		pm2_rcv_tmr;		/* Duty cycle ON time countdown timer */

	/* PM2 Return to Sleep */
	uint16		pm2_sleep_ret_ticks; /* return to sleep time in 10 ms ticks */
	uint16		pm2_sleep_ret_tmr;	 /* return to sleep countdown timer */
	bool		pm2_keep_awake;		 /* keep PM2 awake due to tx/rx activity */

	/* WME */
	bool		wme_noack;		/* enable WME no-acknowledge mode */
	ac_bitmap_t	wme_dp;			/* Discard (oldest first) policy per AC */
	bool		wme_apsd;		/* enable Advanced Power Save Delivery */
	bool		wme_apsd_assoc;		/* an associated STA is using APSD */
	ac_bitmap_t	wme_admctl;		/* bit i set if AC i under admission control */
	uint16		edcf_txop[AC_COUNT];	/* current txop for each ac */
	wme_param_ie_t	wme_param_ie;		/* WME parameter info element, which on STA
						 * contains parameters in use locally, and on
						 * AP contains parameters advertised to STA
						 * in beacons and assoc responses.
						 */
	bool		wme_prec_queuing;	/* enable/disable non-wme STA prec queuing */
#if defined(WME_PER_AC_TX_PARAMS)
	uint16		wme_max_rate[AC_COUNT]; /* In units of 512 Kbps */
#endif
	uint16		wme_retries[AC_COUNT];  /* per-AC retry limits */
	uint32		wme_maxbw_ac[AC_COUNT];	/* Max bandwidth per allowed per ac */

#ifdef WLAFTERBURNER
	int		afterburner_override;	/* -1:auto 0:off 1:forced on */
	uint8		abminrate;		/* afterburner min rate threshold */
#endif /* WLAFTERBURNER */

	int		vlan_mode;		/* OK to use 802.1Q Tags (ON, OFF, AUTO) */
	uint16		tx_prec_map;		/* Precedence map based on HW FIFO space */
	uint16		fifo2prec_map[NFIFO];	/* pointer to fifo2_prec map based on WME */

	bool		wpa_msgs;		/* enable/disable wpa indications via wlc_wpa_msg */

	/* BSS Configurations */
	wlc_bsscfg_t	*bsscfg[WLC_MAXBSSCFG];	/* set of BSS configurations, idx 0 is default and
						 * always valid
						 */
	wlc_bsscfg_t	*cfg;			/* the primary bsscfg (can be AP or STA) */
#ifdef MBSS
	struct ether_addr vether_base;		/* Base virtual MAC addr when user
						 * doesn't provide one
						 */
	uint8		cur_dtim_count;		/* current DTIM count */
	int8		hw2sw_idx[WLC_MAXBSSCFG]; /* Map from uCode index to software index */
	uint16		prq_base;		/* Base address of PRQ in shm */
	uint16		prq_rd_ptr;		/* Cached read pointer for PRQ */
	uint32		last_tbtt_us;		/* Timestamp of TBTT time */
	int8		beacon_bssidx;		/* Track start config to rotate order of beacons */
	uint8		mbss_ucidx_mask;	/* used for extracting ucidx from macaddr */
	uint32		max_ap_bss;		/* max ap bss supported by driver */
#endif /* MBSS */
	uint8		stas_associated;	/* count of ASSOCIATED STA bsscfgs */
	uint8		stas_connected;		/* # ASSOCIATED STA bsscfgs with valid BSSID */
	uint8		aps_associated;		/* count of UP AP bsscfgs */
	uint8		ibss_bsscfgs;		/* count of IBSS bsscfgs */
	uint8		block_datafifo;		/* prohibit posting frames to data fifos */
	bool		bcmcfifo_drain;		/* TX_BCMC_FIFO is set to drain */

	/* tx queue */
	struct pktq	txq;			/* common tx pkt Q */

	/* event */
	wlc_eventq_t 	*eventq;		/* event queue for deferred processing */

	/* security */
	wsec_key_t*	wsec_keys[WSEC_MAX_KEYS];	/* dynamic key storage */
	wsec_key_t*	wsec_def_keys[WLC_DEFAULT_KEYS];	/* default key storage */
	bool		wsec_swkeys;		/* indicates that all keys should be
						 * treated as sw keys (used for debugging)
						 */

	uint32		lifetime[AC_COUNT];	/* MSDU lifetime per AC in us */
	modulecb_t	*modulecb;
	dumpcb_t	*dumpcb_head;
	txmod_info_t	*txmod_fns;

	uint8		mimoft;			/* SIGN or 11N */
	uint8		mimo_band_bwcap;	/* bw cap per band type */
	bool		mimo_mixedmode;		/* mimo preamble type */
#ifdef WLAMPDU
	bool		ampdu_rts;		/* use RTS for every AMPDU */
#endif
	int8		txburst_limit_override; /* tx burst limit override */
	uint16		txburst_limit;		/* tx burst limit value */
	int8		cck_40txbw;		/* 11N, cck tx b/w override when in 40MHZ mode */
	int8		ofdm_40txbw;		/* 11N, ofdm tx b/w override when in 40MHZ mode */
	int8		mimo_40txbw;		/* 11N, mimo tx b/w override when in 40MHZ mode */
	ht_cap_ie_t	ht_cap;			/* HT CAP IE being advertised by this node */
	ht_add_ie_t	ht_add;			/* HT ADD IE being used by this node */
	obss_info_t	*obss;			/* OBSS Coexistance info */

	/* scb data */
	scb_module_t	*scbstate;		/* pointer to scb module state */

	uint		seckeys;		/* 54 key table shm address */
	uint		tkmickeys;		/* 12 TKIP MIC key table shm address */

	/* channel quality measure */
	int		channel_quality;	/* quality metric(0-3) of last measured channel, or
						 * -1 if in progress
						 */
	uint8		channel_qa_channel;	/* channel number of channel being evaluated */
	int8		channel_qa_sample[WLC_CHANNEL_QA_NSAMP]; /* rssi samples of background
								  * noise
								  */
	uint		channel_qa_sample_num;	/* count of samples in channel_qa_sample array */

	wlc_bss_list_t	*scan_results;
	int32		scanresults_minrssi;	/* RSSI threshold under which beacon/probe responses
						* are tossed due to weak signal
						*/
	wlc_bss_list_t	*custom_scan_results;	/* results from ioctl scan */
	uint	custom_scan_results_state;	/* see WL_SCAN_RESULTS_* states in wlioctl.h */
	uint	custom_iscan_results_state;	/* see WL_SCAN_RESULTS_* states in wlioctl.h */
#ifdef STA
	iscan_ignore_t	*iscan_ignore_list;	/* networks to ignore on subsequent iscans */
	uint	iscan_ignore_last;		/* iscan_ignore_list count from prev partial scan */
	uint	iscan_ignore_count;		/* cur number of elements in iscan_ignore_list */
	chanspec_t iscan_chanspec_last;		/* resume chan after prev partial scan */
	struct wl_timer *iscan_timer;		/* stops iscan after iscan_duration ms */
#endif /* STA */

	wlc_bss_info_t	*default_bss;		/* configured BSS parameters */

	uint16		AID;			/* association ID */
	uint16		counter;		/* per-sdu monotonically increasing counter */
	uint16		mc_fid_counter;		/* BC/MC FIFO frame ID counter */


	bool		ibss_allowed;		/* FALSE, all IBSS will be ignored during a scan
						 * and the driver will not allow the creation of
						 * an IBSS network
						 */
	bool 		ibss_coalesce_allowed;

	/* country, spect management */
	uint8		spect_state; 		/* Keep various 11h states */
	bool		awaiting_cntry_info;	/* still waiting for country ie in 802.11d mode */
	char		country_default[WLC_CNTRY_BUF_SZ];	/* saved country for leaving 802.11d
								 * auto-country mode
								 */
	char		autocountry_default[WLC_CNTRY_BUF_SZ];	/* initial country for 802.11d
								 * auto-country mode
								 */

	bool		country_list_extended;	/* JP-variants are reported through
						 * WLC_GET_COUNTRY_LIST if TRUE
						 */
#ifdef BCMDBG
	bcm_tlv_t	*country_ie_override;	/* debug override of announced Country IE */
#endif

	dot11_quiet_t 	quiet_cmd;
	wl_chan_switch_t	csa;
	struct wl_timer *csa_timer;		/* time to switch channel after last beacon */
	uint8 		pwr_constraint;
#if defined(WL_AP_TPC) && defined(WL11H)
	uint8		ap_tpc;
	uint8		ap_tpc_interval;
#endif

	uint16		prb_resp_timeout;	/* do not send prb resp if request older than this,
						 * 0 = disable
						 */

	wlc_rateset_t	sup_rates_override;	/* use only these rates in 11g supported rates if
						 * specifed
						 */

	int16	rssi_win_rfchain[WL_RSSI_ANT_MAX][WLC_RSSI_WINDOW_SZ]; /* rssi per antenna */
	uint8	rssi_win_rfchain_idx;

	chanspec_t	home_chanspec;		/* shared home chanspec */

	/* PHY parameters */
	chanspec_t	chanspec;		/* target operational channel */
	uint16		usr_fragthresh;	/* user configured fragmentation threshold */
	uint16		fragthresh[NFIFO];	/* per-fifo fragmentation thresholds */
	uint16		RTSThresh;		/* 802.11 dot11RTSThreshold */
	uint16		SRL;			/* 802.11 dot11ShortRetryLimit */
	uint16		LRL;			/* 802.11 dot11LongRetryLimit */
	uint16		SFBL;			/* Short Frame Rate Fallback Limit */
	uint16		LFBL;			/* Long Frame Rate Fallback Limit */

	/* network config */
	bool	shortpreamble;		/* currently operating with CCK ShortPreambles */
	bool	shortslot;		/* currently using 11g ShortSlot timing */
	bool	erp_ie_use_protection;	/* currently advertising Use_Protection */
	bool	erp_ie_nonerp;		/* currently advertising NonERP_Present */
	int8	barker_preamble;	/* current Barker Preamble Mode */
	int8	shortslot_override;	/* 11g ShortSlot override */
	bool	include_legacy_erp;	/* include Legacy ERP info elt ID 47 as well as g ID 42 */
	bool	barker_overlap_control; /* TRUE: be aware of overlapping BSSs for barker */
	bool	ignore_bcns;		/* override: ignore non shortslot bcns in a 11g network */
	bool	interference_mode_crs;	/* aphy crs state for interference mitigation mode */
	bool	legacy_probe;		/* restricts probe requests to CCK rates */

	bool	nonerp_assoc;		/* NonERP STAs associated */
	bool	ofdm_assoc;		/* OFDM STAs associated */
#ifdef WLAFTERBURNER
	bool	nonabcap_assoc;		/* nonAfterburnerCapable STAs associated */
#endif /* WLAFTERBURNER */
	bool	longslot_assoc;		/* Long Slot timing only STAs associated */
	bool	longpre_assoc;		/* bss/ibss: cck long preamble only STAs associated */
	bool	non_gf_assoc;		/* non GF STAs associated */
	bool	ht20in40_assoc;		/* 20MHz only HT STAs associated in a 40MHz */
	bool	non11n_apsd_assoc;	/* an associated STA is non-11n APSD */
	bool	ht40intolerant_assoc;	/* an associated STA that is 40 intolerant */
	wlc_protection_t *protection;

	wlc_stf_t *stf;

	char		brcm_ie[WLC_MAX_BRCM_ELT];	/* brcm proprietary element */

	pkt_cb_t	*pkt_callback; /* tx completion callback handlers */

	uint32		txretried;		/* tx retried number in one msdu */

	uint8		nickname[32];		/* Set by vx/linux ioctls but currently unused */

	ratespec_t	bcn_rspec;		/* save bcn ratespec purpose */

#ifdef STA
	bool		IBSS_join_only;		/* don't start IBSS if not found */


	wlc_bsscfg_t	*assoc_req[WLC_MAXBSSCFG]; /* join/roam requests */

	/* association */
	uint		sta_retry_time;		/* time to retry on initial assoc failure */

	wlc_bss_list_t	*join_targets;
	uint		join_targets_last;	/* index of last target tried (next: --last) */

	/* 802.11h Quiet Period */
	wlc_quiet_t *quiet;

	/* 802.11h Channel Switch Announcement */
	struct {
		uint32	secs;		/* seconds until channel switch */
		chanspec_t	chanspec;	/* target chanspec */
	} channel_sw;

#ifdef WLRM
	/* Radio Measurement support */
	wlc_rm_req_state_t *rm_state;		/* radio measurement state */
	wl_rm_rep_elt_t	*rm_ioctl_rep;		/* saved measure reports for ioctl rm requests */
	int		rm_ioctl_rep_len;	/* length of rm_ioctl_rep block */
	struct wl_timer *rm_timer;		/* 11h radio measurement timer */
	struct wl_timer *rm_rpi_timer;		/* RPI sample timer */
#endif /* WLRM */
	/* Demodulator frequency tracking */
	bool		freqtrack;		/* Have we increased the frequency
						 * tracking bandwidth of the
						 * demodulator?
						 */
	uint		freqtrack_starttime;	/* Start time(in seconds) of the last
						 * frequency tracking attempt
						 */
	int8		freqtrack_attempts;	/* Number of times we tried to acquire
						 * beacons by increasing the freq
						 * tracking b/w
						 */
	int8		freqtrack_override;	/* Override setting from registry */

	uint8		apsd_sta_qosinfo;	/* Application-requested APSD configuration */
	bool		apsd_sta_usp;		/* Unscheduled Service Period in progress on STA */
	struct wl_timer *apsd_trigger_timer;	/* timer for wme apsd trigger frames */
	uint32 		apsd_trigger_timeout;	/* timeout value for apsd_trigger_timer (in ms)
						 * 0 == disable
						 */
	ac_bitmap_t	apsd_trigger_ac;	/* Permissible Acess Category in which APSD Null
						 * Trigger frames can be send
						 */
	bool		apsd_auto_trigger;	/* Enable/Disable APSD frame after indication in
						 * Beacon's PVB
						 */
	mbool		clkreqdisab;		/* disable clkreq for TX/RX/MAC Wake */
#endif	/* STA */

#ifdef AP
	bool		cs_scan_ini;		/* Channel Scan started flag */
	chanspec_t	chanspec_selected;	/* chan# selected by WLC_START_CHANNEL_SEL */
	chanspec_t	*scan_chanspec_list;
	int		scan_chanspec_list_size;
	int		scan_chanspec_count;
	uint		scb_timeout;		/* inactivity timeout for associated STAs */
	uint		scb_activity_time;	/* skip probe if activity during this time */
	bool		reprobe_scb;		/* to let watchdog know there are scbs to probe */
	uint		scb_max_probe;		/* max number of probes to be conducted */
#endif	/* AP */

#ifdef WLCHANIM
	chanim_info_t *chanim_info;
#endif	/* WLCHANIM */

	bool		scbrssi;		/* per-scb rssi values */

	apps_wlc_psinfo_t *psinfo;              /* Power save nodes related state */
	wlc_ap_info_t *ap;

	uint8	htphy_membership;	/* HT PHY membership */

	/* Global txmaxmsdulifetime, global rxmaxmsdulifetime */
	uint32		txmsdulifetime;
	uint16		rxmsdulifetime;

#ifdef BRCMAPIVTW
	bool		brcm_ap_iv_tw;
	int8		brcm_ap_iv_tw_override;
#endif

#if defined(DELTASTATS)
	delta_stats_info_t *delta_stats;
#endif /* DELTASTATS */

	uint8		noise_req;			/* to manage phy noise sample requests */
	int		phynoise_chan_scan;	/* sampling target channel for scan */
	bool		_regulatory_domain;	/* 802.11d enabled? */
	uint		_spect_management;	/* 802.11h dot11SpectrumManagementRequired value */

#ifdef WL11N
	uint8	mimops_PM;
	uint8	mimops_ActionPM;
	uint8  	mimops_ActionRetry;
	bool    mimops_ActionPending;
#endif /* WL11N */
	uint16	rfaware_lifetime;	/* RF awareness lifetime (us/1024, not ms) */
	uint32	exptime_cnt;		/* number of expired pkts since start_exptime */
	uint32	last_exptime;		/* sysuptime for last timer expiration */

	uint8	txpwr_percent;		/* power output percentage */

	uint8	ht_wsec_restriction;	/* the restriction of HT with TKIP or WEP */

#endif /* WLC_HIGH */

#ifdef ADV_PS_POLL
	bool        send_pspoll_after_tx;   /* send pspoll frame after last TX frame, to check
						 * any buffered frames in AP, during PM = 1,
						 * (or send ps poll in advance after last tx)
						 */
	bool		adv_ps_poll;		/* enable/disable 'send_pspoll_after_tx' */
#endif

#if defined(RWL_WIFI) || defined(WIFI_REFLECTOR) || defined(WIFI_ACT_FRAME)
	struct dot11_action_wifi_vendor_specific *rwl_first_action_node, *rwl_last_action_node;
	struct ether_addr rwl_ea;
#endif /* RWL_WIFI */

	uint		bcn_thresh;		/* bcn intervals w/o bcn --> bcn lost event */
	uint		bcn_interval_cnt;	/* count of bcn intervals since last bcn */

	uint16      tx_duty_cycle_ofdm; /* maximum allowed duty cycle for OFDM */
	uint16      tx_duty_cycle_cck; /* maximum allowed duty cycle for CCK */
	/* pointer to LMAC realted data structure */
	lmac_info_t	*lmac_info;

	bool	nav_reset_war_disable; /* WAR to reset NAV on 0 duration ACK */

	int		sup_wpa2_eapver; /* for choosing eapol version in M2 */
	bool		sup_m3sec_ok; /* to selectively allow incorrect bit in M3 */
	int		txc_scb_handle;	/* txc scb cubby offset */

	/* parameters for delaying radio shutdown after sending NULL PM=1 */
	uint16		pm2_radio_shutoff_dly;	/* configurable radio shutoff delay */
	bool		pm2_radio_shutoff_pending;	/* flag indicating radio shutoff pending */
	struct wl_timer *pm2_radio_shutoff_dly_timer;	/* timer to delay radio shutoff */

#ifdef CCA_STATS
	cca_info_t *cca_info;
#endif

	struct sa_seqctl ctl_dup_det[SA_SEQCTL_SIZE]; /* Small table for duplicate detections
							* for cases where SCB does not exist
							*/
	uint8 ctl_dup_det_idx;
	uint16	next_bsscfg_ID;
#ifdef BCMWAPI_WPI
	uint	wapimickeys;            /* 8 WAPI MIC key table shm address */
	/* WAPI HW support variables */
	bool	hw_wapi_capable;
	bool	hw_wapi_enabled;
#endif /* BCMWAPI_WPI */

};


typedef struct _btc_flags_ucode {
	uint8 idx;
	uint16 mask;
} btc_flags_ucode_t;

#define BTC_FLAGS_SIZE 9
#define BTC_FLAGS_MHF3_START 1
#define BTC_FLAGS_MHF3_END   6

extern btc_flags_ucode_t btc_ucode_flags[BTC_FLAGS_SIZE];

/* antsel module specific state */
struct antsel_info {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_pub_t *pub;			/* pointer to public fn */
	uint8	antsel_type;		/* Type of boardlevel mimo antenna switch-logic
					 * 0 = N/A, 1 = 2x4 board, 2 = 2x3 CB2 board
					 */
	uint8 antsel_antswitch;		/* board level antenna switch type */
	bool antsel_avail;		/* Ant selection availability (SROM based) */
	wlc_antselcfg_t antcfg_11n;	/* antenna configuration */
	wlc_antselcfg_t antcfg_cur;	/* current antenna config (auto) */
};

#define WLC_HT_WEP_RESTRICT	0x01 	/* restrict HT with WEP */
#define WLC_HT_TKIP_RESTRICT	0x02 	/* restrict HT with TKIP */

#define WLC_RFAWARE_LIFETIME_DEFAULT	800	/* default rfaware_lifetime unit: 256us */

#define WLC_EXPTIME_END_TIME	10000	/* exit exptime state if we get frames without expiration
					 * after last expired frame (ms)
					 */

/* BCMECICOEX support */
#ifdef BCMECICOEX
#define BCMECICOEX_ENAB(wlc) \
	(D11REV_GE(((wlc_info_t*)(wlc))->pub->corerev, 15) && \
	(((wlc_info_t*)(wlc))->pub->sih->cccaps & CC_CAP_ECI) && \
	(((wlc_info_t*)(wlc))->machwcap & MCAP_BTCX))
#define BCMECICOEX_ENAB_BMAC(wlc_hw) \
	(D11REV_GE(((wlc_hw_info_t*)(wlc_hw))->corerev, 15) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps & CC_CAP_ECI) && \
	!(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	(((wlc_hw_info_t*)(wlc_hw))->machwcap & MCAP_BTCX))
#define BCMSECICOEX_ENAB_BMAC(wlc_hw) \
	((((wlc_hw_info_t*)(wlc_hw))->boardflags & BFL_BTC2WIRE) && \
	!(((wlc_hw_info_t*)(wlc_hw))->boardflags2 & BFL2_BTC3WIRE) && \
	(((wlc_hw_info_t*)(wlc_hw))->sih->cccaps_ext & CC_CAP_EXT_SECI_PRESENT) && \
	(((wlc_hw_info_t*)(wlc_hw))->machwcap & MCAP_BTCX))
#else /* BCMECICOEX */
#define BCMECICOEX_ENAB(wlc)            0
#define BCMECICOEX_ENAB_BMAC(wlc_hw)	0
#define BCMSECICOEX_ENAB_BMAC(wlc_hw)	0
#endif /* BCMECICOEX */

#define WLC_BSSCFG_IDX_INVALID (-1)

/*
 * Conversion between HW and SW BSS indexes.  HW is (currently) based on lower
 * bits of BSSID/MAC address.  SW is based on allocation function.
 * BSS does not need to be up, so caller should check if required.  No error checking.
 * For the reverse map, use WLC_BSSCFG_UCIDX(cfg)
 */
#if defined(MBSS)
#define WLC_BSSCFG_HW2SW_IDX(wlc, uc_idx) ((int)((wlc)->hw2sw_idx[(uc_idx)]))
#else /* !MBSS */
#define WLC_BSSCFG_HW2SW_IDX(wlc, uc_idx) 0
#endif /* MBSS */

/*
 * Under MBSS, a pre-TBTT interrupt is generated.  The driver puts beacons in
 * the ATIM fifo at that time and tells uCode about pending BC/MC packets.
 * The delay is settable thru uCode.  MBSS_PRE_TBTT_DEFAULT_us is the default
 * setting for this value.
 * If the driver experiences significant latency, it must avoid setting up
 * beacons or changing the SHM FID registers.  The "max latency" setting
 * indicates the maximum permissible time between the TBTT interrupt and the
 * DPC to respond to the interrupt before the driver must abort the TBTT
 * beacon operations.
 */
#define MBSS_PRE_TBTT_DEFAULT_us 5000		/* 5 milliseconds! */
#define MBSS_PRE_TBTT_MAX_LATENCY_us 4000

/* Default pre tbtt time for non mbss case */
#define	PRE_TBTT_DEFAULT_us		2

/* Time in usec for PHY enable */
#define	PHY_ENABLE_MAX_LATENCY_us	3000
#define	PHY_DISABLE_MAX_LATENCY_us	3000

/* Association use of Scan Cache */
#ifdef WLSCANCACHE
#define ASSOC_CACHE_ASSIST_ENAB(wlc)	((wlc)->_assoc_cache_assist)
#else
#define ASSOC_CACHE_ASSIST_ENAB(wlc)	(0)
#endif

/* quiet->state */
#define WAITING_FOR_FLUSH_COMPLETE 	(1 << 0)
#define WAITING_FOR_INTERVAL		(1 << 1)
#define WAITING_FOR_OFFSET		(1 << 2)
#define WAITING_FOR_DURATION		(1 << 3)
#define SILENCE				(1 << 4)

/* Regulatory Domain -- 11H Support */
#ifdef WL11D
#define WL11D_ENAB(wlc) ((wlc)->_regulatory_domain)
#else
#define WL11D_ENAB(wlc) 0
#endif /* WL11D */

/* Spectrum Management -- 11H Support */
#ifdef WL11H
#define QUIET_STATE(wlc)	((wlc)->quiet->_state)
#define WL11H_ENAB(wlc)		((wlc)->_spect_management)
#else
#define QUIET_STATE(wlc)	0
#define WL11H_ENAB(wlc)		0
#endif /* WL11H */

/* spect_state */
#define NEED_TO_UPDATE_BCN	(1 << 0)	/* Need to decrement counter in outgoing beacon */
#define NEED_TO_SWITCH_CHANNEL	(1 << 1)	/* A channel switch is pending */
#define RADAR_SIM		(1 << 2)	/* Simulate radar detection...for testing */

#define	CHANNEL_BANDUNIT(wlc, ch) (((ch) <= CH_MAX_2G_CHANNEL) ? BAND_2G_INDEX : BAND_5G_INDEX)
#define	CHANNEL_BAND(wlc, ch) (((ch) <= CH_MAX_2G_CHANNEL) ? WLC_BAND_2G : WLC_BAND_5G)

#define	OTHERBANDUNIT(wlc)	((uint)((wlc)->band->bandunit? BAND_2G_INDEX : BAND_5G_INDEX))

#define VALID_BAND(wlc, band)	((band == WLC_BAND_AUTO) || (band == WLC_BAND_2G) || \
				 (band == WLC_BAND_5G))

#define IS_MBAND_UNLOCKED(wlc) \
	((NBANDS(wlc) > 1) && !(wlc)->bandlocked)

#ifdef WLC_LOW
#define WLC_BAND_PI_RADIO_CHANSPEC wlc_phy_chanspec_get(wlc->band->pi)
#else
#define WLC_BAND_PI_RADIO_CHANSPEC (wlc->chanspec)
#endif

/* increment counter */
#ifdef BCMDBG
#define WLCNINC(cntr)		((cntr) ++)	/* by 1 */
#define WLCNINCN(cntr, n)	((cntr) += (n))	/* by n */
#else
#define WLCNINC(cntr)
#define WLCNINCN(cntr, n)
#endif /* BCMDBG */

#define AID2PVBMAP(aid)	(aid & 0x3fff)
#define AID2AIDMAP(aid)	((aid & 0x3fff) - 1)
/* set the 2 MSBs of the AID */
#define AIDMAP2AID(pos)	((pos + 1) | 0xc000)

/* sum the individual fifo tx pending packet counts */
#if defined(WLC_HIGH_ONLY)
#include <wlc_rpctx.h>
#define TXPKTPENDTOT(wlc)		(wlc_rpctx_txpktpend((wlc)->rpctx, 0, TRUE))
#define TXPKTPENDGET(wlc, fifo)		(wlc_rpctx_txpktpend((wlc)->rpctx, (fifo), FALSE))
#define TXPKTPENDINC(wlc, fifo, val)	(wlc_rpctx_txpktpendinc((wlc)->rpctx, (fifo), (val)))
#define TXPKTPENDDEC(wlc, fifo, val)	(wlc_rpctx_txpktpenddec((wlc)->rpctx, (fifo), (val)))
#define TXPKTPENDCLR(wlc, fifo)		(wlc_rpctx_txpktpendclr((wlc)->rpctx, (fifo)))
#define TXAVAIL(wlc, fifo)		(wlc_rpctx_txavail((wlc)->rpctx, (fifo)))
#define GETNEXTTXP(wlc, _queue)		(wlc_rpctx_getnexttxp((wlc)->rpctx, (_queue)))

#else
#define	TXPKTPENDTOT(wlc) ((wlc)->core->txpktpend[0] + (wlc)->core->txpktpend[1] + \
	(wlc)->core->txpktpend[2] + (wlc)->core->txpktpend[3])
#define TXPKTPENDGET(wlc, fifo)		((wlc)->core->txpktpend[(fifo)])
#define TXPKTPENDINC(wlc, fifo, val)	((wlc)->core->txpktpend[(fifo)] += (val))
#define TXPKTPENDDEC(wlc, fifo, val)	((wlc)->core->txpktpend[(fifo)] -= (val))
#define TXPKTPENDCLR(wlc, fifo)		((wlc)->core->txpktpend[(fifo)] = 0)
#define TXAVAIL(wlc, fifo)		(*(wlc)->core->txavail[(fifo)])
#define GETNEXTTXP(wlc, _queue)								\
	((!PIO_ENAB((wlc)->pub))?							\
		dma_getnexttxp((wlc)->hw->di[(_queue)], HNDDMA_RANGE_TRANSMITTED) :	\
		wlc_pio_getnexttxp((wlc)->hw->pio[(_queue)]))
#endif /* WLC_HIGH_ONLY */

#define WLC_IS_MATCH_SSID(wlc, ssid1, ssid2, len1, len2) \
	((len1 == len2) && !bcmp(ssid1, ssid2, len1))

/* API shared by both WLC_HIGH and WLC_LOW driver */
extern void wlc_high_dpc(wlc_info_t *wlc, uint32 macintstatus);
extern void wlc_fatal_error(wlc_info_t *wlc);
extern void wlc_bmac_rpc_watchdog(wlc_info_t *wlc);
extern void wlc_recv(wlc_info_t *wlc, void *p);
extern bool wlc_dotxstatus(wlc_info_t *wlc, tx_status_t *txs, uint32 frm_tx2);
extern void wlc_recover_pkts(wlc_info_t *wlc, uint queue);
extern uint16 wlc_wme_get_frame_medium_time(wlc_info_t *wlc, ratespec_t ratespec,
	uint8 preamble_type, uint mac_len);
extern void wlc_txfifo(wlc_info_t *wlc, uint fifo, void *p, bool commit,
	int8 txpktpend);
extern void wlc_txfifo_complete(wlc_info_t *wlc, uint fifo, int8 txpktpend);
extern void wlc_info_init(wlc_info_t* wlc, int unit);
extern uint16 wlc_rcvfifo_limit_get(wlc_info_t *wlc);
extern void wlc_print_txstatus(tx_status_t* txs);
extern int  wlc_xmtfifo_sz_get(wlc_info_t *wlc, uint fifo, uint *blocks);
extern void wlc_write_template_ram(wlc_info_t *wlc, int offset, int len, void *buf);
extern void wlc_write_hw_bcntemplates(wlc_info_t *wlc, void *bcn, int len, bool both);
#if defined(BCMDBG) || defined(BCMDBG_DUMP)
extern void wlc_get_rcmta(wlc_info_t *wlc, int idx, struct ether_addr *addr);
#endif
extern void wlc_set_rcmta(wlc_info_t *wlc, int idx, const struct ether_addr *addr);
#ifdef WLP2P
extern void wlc_set_rcmta_type(wlc_info_t *wlc, int idx, uint16 mask, uint16 val);
#endif
extern void wlc_set_addrmatch(wlc_info_t *wlc, int match_reg_offset,
	const struct ether_addr *addr);
extern void wlc_mute(wlc_info_t *wlc, bool on, mbool flags);
extern void wlc_read_tsf(wlc_info_t* wlc, uint32* tsf_l_ptr, uint32* tsf_h_ptr);
extern void wlc_set_cwmin(wlc_info_t *wlc, uint16 newmin);
extern void wlc_set_cwmax(wlc_info_t *wlc, uint16 newmax);
extern void wlc_fifoerrors(wlc_info_t *wlc);
extern void wlc_rssi_init(wlc_info_t *wlc, int rssi);
extern int  wlc_get_rssi_from_pkt(wlc_info_t *wlc, d11rxhdr_t *rxh);
extern void wlc_noise_sample_request(wlc_info_t *wlc, uint8 request, uint8 channel);
extern void wlc_noise_cb(wlc_info_t *wlc, uint8 channel, int8 noise_dbm);
extern void wlc_channel_qa_sample_req(wlc_info_t *wlc);
extern void wlc_pllreq(wlc_info_t *wlc, bool set, mbool req_bit);
extern void wlc_update_phy_mode(wlc_info_t *wlc, uint32 phy_mode);
extern void wlc_reset_bmac_done(wlc_info_t *wlc);
extern void wlc_protection_upd(wlc_info_t *wlc, uint idx, int val);
extern void wlc_print_bytes(const char *name, uchar *bytes, int len);
extern void wlc_hwtimer_gptimer_set(wlc_info_t *wlc, uint us);
extern void wlc_hwtimer_gptimer_abort(wlc_info_t *wlc);
extern uint *wlc_scb_txcptr_to_invalidate_txc(wlc_info_t *wlc, struct scb *scb);

#if defined(BCMDBG) || defined(WLMSG_PRHDRS)
extern void wlc_print_rxh(d11rxhdr_t* rxh);
extern void wlc_print_hdrs(wlc_info_t *wlc, const char *prefix, uint8 *frame,
	d11txh_t *txh, d11rxhdr_t *rxh, uint len);
extern void wlc_print_txdesc(d11txh_t *txh);
#endif
#if defined(BCMDBG) || defined(WLMSG_PRHDRS) || defined(WLMSG_PRPKT) || \
	defined(WLMSG_ASSOC)
extern void wlc_print_dot11_mac_hdr(uint8* buf, int len);
#endif

#ifdef STA
#ifdef WLRM
extern void wlc_rm_cca_complete(wlc_info_t *wlc, uint32 cca_idle_us);
extern bool wlc_rm_rpi_sample(wlc_info_t *wlc, int8 rssi);
#endif
#endif /* STA */

#ifdef MBSS
extern bool wlc_ucodembss_hwcap(wlc_info_t *wlc);
#else
#define wlc_ucodembss_hwcap(a) 0
#endif

#ifdef WLC_LOW
extern void wlc_setxband(wlc_hw_info_t *wlc_hw, uint bandunit);
extern void wlc_coredisable(wlc_hw_info_t* wlc_hw);
extern void wlc_rxfifo_setpio(wlc_hw_info_t *wlc_hw);
#endif

/* API in HIGH only or monolithic driver */
#ifdef WLC_HIGH
extern void wlc_rssi_reset_ma(wlc_bsscfg_t *cfg, int nval);
extern int  wlc_rssi_update_ma(wlc_bsscfg_t *cfg, int nval);
extern void wlc_rssi_event_update(wlc_bsscfg_t *cfg);
extern void wlc_enable_btc_ps_protection(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool up);
extern void wlc_link(wlc_info_t *wlc, bool up, struct ether_addr *addr, wlc_bsscfg_t *bsscfg,
	uint reason);
extern int wlc_scan_request_ex(wlc_info_t *wlc, int bss_type, const struct ether_addr* bssid,
                               int nssid, wlc_ssid_t *ssids, int scan_type, int nprobes,
                               int active_time, int passive_time, int home_time,
                               const chanspec_t* chanspec_list, int chanspec_num,
                               chanspec_t chanspec_start, bool save_prb,
                               scancb_fn_t fn, void* arg,
                               int macreq, wlc_bsscfg_t *cfg);
#define wlc_scan_request(wlc, bss_type, bssid, nssid, ssid, scan_type, nprobes, \
		active_time, passive_time, home_time, chanspec_list, chanspec_num, \
		save_prb, fn, arg) \
	wlc_scan_request_ex(wlc, bss_type, bssid, nssid, ssid, scan_type, nprobes, \
		active_time, passive_time, home_time, chanspec_list, chanspec_num, 0, \
		save_prb, fn, arg, WLC_ACTION_SCAN, NULL)
extern void wlc_set_ssid(wlc_info_t *wlc, uchar SSID[], int len);

#ifdef WLSCANCACHE
extern bool wlc_assoc_cache_validate_timestamps(wlc_info_t *wlc, wlc_bss_list_t *bss_list);
#endif /* WLSCANCACHE */

#ifdef WLAFTERBURNER
extern void wlc_afterburner(wlc_info_t *wlc, bool state);
#endif /* WLAFTERBURNER */
extern bool wlc_valid_rate(wlc_info_t *wlc, ratespec_t rate, int band, bool verbose);
extern void wlc_do_chanswitch(wlc_info_t *wlc, chanspec_t newchspec);
extern void *wlc_senddisassoc(wlc_info_t *wlc, const struct ether_addr *da,
	const struct ether_addr *bssid, const struct ether_addr *sa,
	struct scb *scb, uint16 reason_code);
extern void *wlc_sendassocreq(wlc_info_t *wlc, wlc_bss_info_t *bi, struct scb *scb, bool reassoc);
extern void wlc_BSSinit(wlc_info_t *wlc, wlc_bss_info_t *bi, wlc_bsscfg_t *cfg,
                         char *bcn, int len, int start);
extern uint8* wlc_copy_info_elt(uint8 *cp, const void* data);
extern uint8 *wlc_copy_info_elt_safe(uint8 *buf, int buflen, const void* data);
extern bool wlc_update_n_protection(wlc_info_t *wlc);
extern bool wlc_update_gbss_modes(wlc_info_t *wlc);
extern void wlc_qosinfo_update(struct scb *scb, uint8 qosinfo);
extern bool wlc_is_wme_ie(wlc_info_t *wlc, uint8 *ie, uint8 **tlvs, uint *tlvs_len);
extern void wlc_process_brcm_ie(wlc_info_t *wlc, struct scb *scb, brcm_ie_t *brcm_ie);
extern ht_cap_ie_t *wlc_read_ht_cap_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len);
extern ht_add_ie_t *wlc_read_ht_add_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len);
extern ht_cap_ie_t *wlc_read_ht_cap_ies(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len);
extern ht_add_ie_t *wlc_read_ht_add_ies(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len);
extern void wlc_process_coex_mgmt_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb);
extern void wlc_ht_upd_coex_state(wlc_info_t *wlc, uint8 detected);
extern void wlc_ht_coex_filter_scanresult(wlc_info_t *wlc);
extern void wlc_ht_coex_exclusion_range(wlc_info_t *wlc, uint8 *ch_min,
                                        uint8 *ch_max, uint8* ch_ctl);
#ifdef WL11N
extern void wlc_ht_obss_scanparams_hostorder(wlc_info_t *wlc, obss_params_t *param,
	bool host_order);
#else
#define wlc_ht_obss_scanparams_hostorder(a, b, c)  do {} while (0)
#endif /* WL11N */

obss_params_t *wlc_ht_read_obss_scanparams_ie(wlc_info_t *wlc, uint8 *tlv, int tlv_len);

extern void wlc_write_ht_add_ie(wlc_bsscfg_t *cfg, ht_add_ie_t *add_ie);
extern void wlc_write_ht_cap_ie(wlc_bsscfg_t *cfg, ht_cap_ie_t *cap_ie,
	uint8* sup_mcs, bool is2G);
extern uint8* wlc_write_brcm_ht_cap_ie(wlc_info_t *wlc, uint8 *cp, int buflen, ht_cap_ie_t *cap_ie);
extern uint8* wlc_write_brcm_ht_add_ie(wlc_info_t *wlc, uint8 *cp, int buflen, ht_add_ie_t *add_ie);
extern void wlc_ht_update_scbstate(wlc_info_t *wlc, struct scb *scb,
	ht_cap_ie_t *cap_ie, ht_add_ie_t *add_ie, obss_params_t *obss_ie);
extern int wlc_parse_rates(wlc_info_t *wlc, uchar *tlvs, uint tlvs_len, wlc_bss_info_t *bi,
	wlc_rateset_t *rates);
extern void *wlc_sendauth(wlc_bsscfg_t *cfg, struct ether_addr *ea, struct ether_addr *bssid,
	struct scb *scb, int auth_alg, int auth_seq, int auth_status, wsec_key_t *scb_key,
	uint8 *challenge_text, bool short_preamble);
extern void wlc_scb_disassoc_cleanup(wlc_info_t *wlc, struct scb *scb);
extern void wlc_cwmin_gphy_update(wlc_info_t *wlc, wlc_rateset_t *rs, bool associated);
extern void wlc_wme_acp_get_all(wlc_info_t *wlc, wme_param_ie_t *wlp, edcf_acparam_t *acparams);
extern int wlc_wme_acp_set(wlc_info_t *wlc, wme_param_ie_t *wlp, edcf_acparam_t *acparams);
extern uchar wlc_assocscb_getcnt(wlc_info_t *wlc);
extern uchar wlc_bss_assocscb_getcnt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern uint8 *wlc_write_csa_body(wl_chan_switch_t *csa, uint8 *cp);
extern uint8 *wlc_write_ext_csa_ie(wl_chan_switch_t *csa, uint8 *cp);
extern uint8 *wlc_write_csa_ie(wl_chan_switch_t *csa, uint8 *cp, int buflen);
extern uint8 *wlc_write_extch_ie(chanspec_t chspec, uint8 *cp, int buflen);
extern int wlc_abo(wlc_info_t *wlc, int val);
extern void wlc_ap_upd(wlc_info_t* wlc,  wlc_bsscfg_t *bsscfg);
extern int wlc_minimal_up(wlc_info_t* wlc);
extern int wlc_minimal_down(wlc_info_t* wlc);

#ifdef WLCHANIM
extern bool wlc_chanim_interfered(wlc_info_t *wlc, chanspec_t chanspec);
extern void wlc_chanim_upd_act(wlc_info_t *wlc);
extern void wlc_chanim_upd_acs_record(chanim_info_t *c_info, chanspec_t home_chspc,
	chanspec_t selected, uint8 trigger);
extern void wlc_chanim_action(wlc_info_t *wlc);
#else /* WLCHANIM */
#define wlc_chanim_interfered(a, b) 0
#define wlc_chanim_upd_act(a) do {} while (0)
#define wlc_chanim_upd_acs_record(a, b, c, d) do {} while (0)
#define wlc_chanim_action(a) do {} while (0)
#endif /* WLCHANIM */

/* helper functions */
extern bool wlc_rateset_isofdm(uint count, uint8 *rates);
extern void wlc_remove_all_keys(wlc_info_t *wlc);
extern void wlc_tx_suspend(wlc_info_t *wlc);
extern bool wlc_tx_suspended(wlc_info_t *wlc);
extern void wlc_tx_resume(wlc_info_t *wlc);
extern void wlc_rateset_show(wlc_info_t *wlc, wlc_rateset_t *rs, struct ether_addr *ea);
extern void wlc_shm_ssid_upd(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

extern void wlc_rateprobe(wlc_info_t *wlc, struct ether_addr *ea, ratespec_t rate_override);
extern void *wlc_senddeauth(wlc_info_t *wlc, struct ether_addr *da, struct ether_addr *bssid,
	struct ether_addr *sa, struct scb *scb, uint16 reason_code);
extern void wlc_mac_event(wlc_info_t* wlc, uint msg, const struct ether_addr* addr,
	uint result, uint status, uint auth_type, void *data, int datalen);
extern void wlc_bss_mac_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type,
	void *data, int datalen);
extern void wlc_bss_mac_rxframe_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg, uint msg,
	const struct ether_addr* addr, uint result, uint status, uint auth_type,
	void *data, int datalen, wl_event_rx_frame_data_t *rxframe_data);
extern void wlc_prep_event_rx_frame_data(wlc_info_t *wlc, d11rxhdr_t *rxh, uint8 *plcp,
                                         wl_event_rx_frame_data_t *rxframe_data);
#ifdef BCMWAPI_WAI
extern void wlc_wapi_station_event(wlc_info_t* wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *addr, void *ie, uint8 *gsn, uint16 msg_type);
#endif /* BCMWAPI_WAI */

#if !defined(WLNOEIND)
extern void wlc_bss_eapol_event(wlc_info_t *wlc, wlc_bsscfg_t * bsscfg,
                                const struct ether_addr *ea, uint8 *data, uint32 len);
#ifdef BCMWAPI_WAI
extern void wlc_bss_wai_event(wlc_info_t *wlc, wlc_bsscfg_t * bsscfg,
	const struct ether_addr *ea, uint8 *data, uint32 len);
#endif /* BCMWAPI_WAI */
#endif /* !defined(WLNOEIND) */

extern void wlc_ibss_disable(wlc_bsscfg_t *cfg);
extern void wlc_ibss_enable(wlc_bsscfg_t *cfg);

extern int wlc_pkt_callback_register(wlc_info_t *wlc, pkcb_fn_t fn, void *arg, void *pkt);
extern void *wlc_sendnulldata(wlc_info_t *wlc, struct ether_addr *da, uint rate_override,
                              uint32 pktflags, int prio);
extern int wlc_wsec(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 val);
extern bcm_tlv_t *wlc_find_vendor_ie(void *tlvs, int tlvs_len, const uint8 *voui, uint8 *type,
	int type_len);
extern int wlc_set_gmode(wlc_info_t *wlc, uint8 gmode, bool config);

extern void wlc_mac_bcn_promisc(wlc_info_t *wlc);
extern void wlc_mac_promisc(wlc_info_t *wlc);
extern void wlc_sendprobe(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint8 ssid[], int ssid_len, const struct ether_addr *da, const struct ether_addr *bssid,
	ratespec_t rate_override, uint8 *extra_ie, int extra_ie_len);
extern void wlc_send_q(wlc_info_t *wlc, struct pktq *q);
extern int wlc_prep_sdu(wlc_info_t *wlc, void **sdu, int *nsdu, uint *fifo);
extern int wlc_prep_pdu(wlc_info_t *wlc, void *pdu, uint *fifo);
extern void wlc_send_action_err(wlc_info_t *wlc, struct dot11_management_header *hdr,
	uint8 *body, int body_len);
extern int wlc_prep_80211sdu(wlc_info_t *wlc, void **pkts, int *npkts, uint *fifop);

extern void *wlc_frame_get_mgmt(wlc_info_t *wlc, uint16 fc, const struct ether_addr *da,
	const struct ether_addr *sa, const struct ether_addr *bssid, uint body_len,
	uint8 **pbody);
extern void *wlc_frame_get_ctl(wlc_info_t *wlc, uint len);
extern bool wlc_sendmgmt(wlc_info_t *wlc, void *p, struct scb *scb, bool preamble,
	wsec_key_t *key, ratespec_t rate_or);
extern bool wlc_sendctl(wlc_info_t *wlc, void *p, struct scb *scb, uint fifo,
	ratespec_t rate_or, bool enq_only);
extern void wlc_recvdata_ordered(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f);
extern uint16 wlc_calc_lsig_len(wlc_info_t *wlc, ratespec_t ratespec, uint mac_len);
extern ratespec_t wlc_rspec_to_rts_rspec(wlc_info_t *wlc, ratespec_t rspec, bool use_rspec,
	uint16 mimo_ctlchbw);
extern uint16 wlc_compute_rtscts_dur(wlc_info_t *wlc, bool cts_only, ratespec_t rts_rate,
	ratespec_t frame_rate, uint8 rts_preamble_type, uint8 frame_preamble_type,
	uint frame_len, bool ba);

extern void wlc_btcx_update_predictor(wlc_info_t *wlc, d11regs_t *regs);
extern void wlc_tbtt(wlc_info_t *wlc, d11regs_t *regs);

extern void wlc_btc_stuck_war50943(wlc_info_t *wlc, bool enable);

extern bool wlc_dump_ucode_fatal(wlc_info_t *wlc);

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
extern void wlc_dump_ie(wlc_info_t *wlc, bcm_tlv_t *ie, struct bcmstrbuf *b);
#endif

extern bool wlc_ps_check(wlc_info_t *wlc);
extern void wlc_reprate_init(wlc_info_t *wlc);
extern void wlc_bsscfg_reprate_init(wlc_bsscfg_t *bsscfg);
extern void wlc_exptime_start(wlc_info_t *wlc);
extern void wlc_exptime_check_end(wlc_info_t *wlc);
extern int wlc_rfaware_lifetime_set(wlc_info_t *wlc, uint16 lifetime);

#ifdef STA
extern void wlc_update_pmstate(wlc_info_t *wlc, tx_status_t *txs, struct scb *scb);
extern void wlc_set_pmstate(wlc_info_t *wlc, bool state);
extern void wlc_pm2_sleep_ret_timer_start(wlc_info_t *wlc);
extern uint8 wlc_stas_connected(wlc_info_t *wlc);
extern bool wlc_bssid_is_current(wlc_bsscfg_t *cfg, struct ether_addr *bssid);
extern bool wlc_bss_connected(wlc_bsscfg_t *cfg);
extern bool wlc_portopen(wlc_bsscfg_t *cfg);
extern void wlc_uint64_add(uint32* high, uint32* low, uint32 inc_high, uint32 inc_low);
extern void wlc_uint64_sub(uint32* a_high, uint32* a_low, uint32 b_high, uint32 b_low);
extern bool wlc_uint64_lt(uint32 a_high, uint32 a_low, uint32 b_high, uint32 b_low);
extern void wlc_rm_meas_end(wlc_info_t* wlc);
extern void wlc_rm_start(wlc_info_t* wlc);
extern void wlc_rm_stop(wlc_info_t* wlc);
extern int wlc_rm_abort(wlc_info_t* wlc);
extern uint32 wlc_calc_tbtt_offset(uint32 bi, uint32 tsf_h, uint32 tsf_l);

extern void wlc_start_quiet2(wlc_info_t *wlc);
extern void wlc_pm2_timeout(wlc_info_t *wlc);
#endif	/* STA */

/* Shared memory access */
extern void wlc_write_shm(wlc_info_t *wlc, uint offset, uint16 v);
extern uint16 wlc_read_shm(wlc_info_t *wlc, uint offset);
extern void wlc_set_shm(wlc_info_t *wlc, uint offset, uint16 v, int len);
extern void wlc_copyto_shm(wlc_info_t *wlc, uint offset, const void* buf, int len);
extern void wlc_copyfrom_shm(wlc_info_t *wlc, uint offset, void* buf, int len);

extern bool wlc_down_for_mpc(wlc_info_t *wlc);

extern void wlc_update_beacon(wlc_info_t *wlc);
extern void wlc_bss_update_beacon(wlc_info_t *wlc, struct wlc_bsscfg *bsscfg);

extern void wlc_update_probe_resp(wlc_info_t *wlc, bool suspend);
extern void wlc_bss_update_probe_resp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend);
extern void wlc_bcn_prb_body(wlc_info_t *wlc, uint type, wlc_bsscfg_t *cfg,
	uint8 *bcn, int *len);
extern int wlc_parse_bcn_prb(wlc_info_t *wlc, d11rxhdr_t *rxh,
	struct ether_addr *bssid, bool beacon, void *body, uint body_len,
	wlc_bss_info_t *bi);

#ifdef STA
extern void wlc_radio_mpc_upd(wlc_info_t* wlc);
#endif /* STA */
extern bool wlc_prec_enq(wlc_info_t *wlc, struct pktq *q, void *pkt, int prec);
extern bool wlc_prec_enq_head(wlc_info_t *wlc, struct pktq *q, void *pkt, int prec, bool head);
extern void wlc_pkt_callback(void *ctx, void *pkt, unsigned int tx_status);
extern void wlc_pkt_callback_append(wlc_info_t *wlc, void *new_pkt, void *pkt);

extern uint8* wlc_write_info_elt(uint8 *cp, int id, int len, const void* data);
extern uint8 *wlc_write_info_elt_safe(uint8 *buf, int buflen, int id, int len, const void* data);

extern void wlc_rateset_elts(wlc_info_t *wlc, wlc_bsscfg_t *cfg, const wlc_rateset_t *rates,
	wlc_rateset_t *sup_rates, wlc_rateset_t *ext_rates);
extern uint16 wlc_phytxctl1_calc(wlc_info_t *wlc, ratespec_t rspec);
extern void wlc_compute_plcp(wlc_info_t *wlc, ratespec_t rate, uint length, uint8 *plcp);
extern void wlc_prb_resp_plcp_hdrs(wlc_info_t *wlc, wlc_prq_info_t *info, int length, uint8 *plcp,
                                   d11txh_t *txh, uint8 *d11_hdr);
extern uint wlc_calc_frame_time(wlc_info_t *wlc, ratespec_t ratespec,
	uint8 preamble_type, uint mac_len);
extern void wlc_processpmq(wlc_info_t *wlc);

#define WLC_CHAN_PHYTYPE(x)     (((x) & RXS_CHAN_PHYTYPE_MASK) >> RXS_CHAN_PHYTYPE_SHIFT)
#define WLC_CHAN_CHANNEL(x)     (((x) & RXS_CHAN_ID_MASK) >> RXS_CHAN_ID_SHIFT)

#define WLC_RX_CHANNEL(rxh)	(WLC_CHAN_CHANNEL((rxh)->RxChan))

#ifdef WIFI_ACT_FRAME
int wlc_tx_action_frame_now(wlc_info_t *wlc, wlc_bsscfg_t *cfg, void *pkt);
extern int wlc_send_action_frame(wlc_info_t *wlc, wlc_bsscfg_t *cfg, const struct ether_addr *bssid,
	void *action_frame);
extern int
wlc_send_action_frame_off_channel(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg,
	uint32 channel, int32 dwell_time, struct ether_addr *bssid, void *action_frame);
#endif

extern void wlc_set_chanspec(wlc_info_t *wlc, chanspec_t chanspec);

extern bool wlc_timers_init(wlc_info_t* wlc, int unit);

extern const bcm_iovar_t wlc_iovars[];

extern bool wlc_update_brcm_ie(wlc_info_t *wlc);
extern void wlc_txq_scb_free_notify(void *context, struct scb *remove);
extern void wlc_recvdata(wlc_info_t *wlc, osl_t *osh, d11rxhdr_t *rxh, void *p);
extern void wlc_recvdata_sendup(wlc_info_t *wlc, struct scb *scb, bool wds, struct ether_addr *da,
                                void *p);
extern bool wlc_sendsdu(wlc_info_t *wlc, void *sdu);

extern void wlc_ether_8023hdr(wlc_info_t *wlc, osl_t *osh, struct ether_header *eh, void *p);
extern void wlc_8023_etherhdr(wlc_info_t *wlc, osl_t *osh, void *p);
extern void* wlc_hdr_proc(wlc_info_t *wlc, void *sdu, struct scb *scb);

extern uint16 wlc_sdu_etype(wlc_info_t *wlc, void *sdu);
extern uint8* wlc_sdu_data(wlc_info_t *wlc, void *sdu);

extern void wlc_txc_upd(struct wlc_info *wlc);
extern int wlc_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
extern int wlc_bssiovar_mkbuf(char *iovar, int bssidx, void *param,
	int paramlen, void *bufptr, int buflen, int *perr);
extern void wlc_if_event(wlc_info_t *wlc, uint8 what, struct wlc_if *wlcif);

#ifdef WL11D
extern void wlc_11d_scan_complete(wlc_info_t *wlc, int status);
#endif

extern bool wlc_valid_beacon_phytxctl(wlc_info_t *wlc);
struct d11init;
extern void wlc_wakeucode_init(wlc_info_t *wlc, const uint32 ucode[], const uint nbytes,
                               const struct d11init *inits, const struct d11init *binits);

extern void * wlc_prep_gtkmsg2_hdrs(wlc_info_t *wlc, void *sdu, struct scb *scb);

#ifdef STA
#if defined(BCMDBG) || defined(WLMSG_PRPKT)
extern void wlc_print_ies(wlc_info_t *wlc, uint8 *ies, uint ies_len);
#endif
#endif	/* STA */

#ifdef WL11N
extern int wlc_set_nmode(wlc_info_t *wlc, int32 nmode);
extern void wlc_ht_mimops_cap_update(wlc_info_t *wlc, uint8 mimops_mode);
extern void wlc_mimops_action_ht_send(wlc_info_t *wlc, uint8 mimops_mode);
extern void *wlc_send_action_ht_mimops(wlc_info_t *wlc, uint8 mimops_mode);
#endif

extern void wlc_txmod_fn_register(wlc_info_t *wlc, scb_txmod_t feature_id, void *ctx,
                                  txmod_fns_t fns);
extern void wlc_txmod_config(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);
extern void wlc_txmod_unconfig(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);

extern int wlc_txq_scb_init(void *ctx, struct scb *scb);

extern void wlc_custom_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg);
extern void wlc_switch_shortslot(wlc_info_t *wlc, bool shortslot);
extern void wlc_set_bssid(wlc_bsscfg_t *cfg);
extern void wlc_clear_bssid(wlc_bsscfg_t *cfg);
extern void wlc_edcf_setparams(wlc_bsscfg_t *cfg, bool suspend);
extern int wlc_bcn_tsf_later(wlc_bsscfg_t *cfg, d11rxhdr_t *rxh, void *bcn_plcp);
extern void wlc_tsf_adopt_bcn(wlc_bsscfg_t *cfg, struct dot11_bcn_prb *bcn);
extern bool wlc_InTIM(wlc_info_t *wlc, void *body, uint plen, bool *bcmc, uint *dtim_count);
extern bool wlc_sendpspoll(wlc_info_t *wlc, struct ether_addr *bssid);
extern void wlc_tbtt_align(wlc_bsscfg_t *cfg, d11rxhdr_t *rxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn);

extern int wlc_sta_info(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                        const struct ether_addr *ea, void *buf, int len);
extern void wlc_print_plcp(char *str, uchar *plcp);
extern void wlc_set_ratetable(wlc_info_t *wlc);
extern int wlc_set_mac(wlc_bsscfg_t *cfg);
extern void wlc_beacon_phytxctl_txant_upd(wlc_info_t *wlc, ratespec_t bcn_rate);
extern void wlc_beacon_phytxctl(wlc_info_t *wlc, ratespec_t bcn_rspec);
extern void wlc_mod_prb_rsp_rate_table(wlc_info_t *wlc, uint frame_len);
extern ratespec_t wlc_lowest_basic_rspec(wlc_info_t *wlc, wlc_rateset_t *rs);
extern void wlc_write_hwbcntemplate(wlc_info_t *wlc, void *bcn, int len, bool both);
extern uint16 wlc_compute_bcntsfoff(wlc_info_t *wlc, ratespec_t rspec,
	bool short_preamble, bool phydelay);
extern void wlc_radio_disable(wlc_info_t *wlc);
extern bool wlc_nonerp_find(wlc_info_t *wlc, void *body, uint body_len, uint8 **erp, int *len);
extern bool wlc_erp_find(wlc_info_t *wlc, void *body, uint body_len, uint8 **erp, int *len);
extern void wlc_bcn_li_upd(wlc_info_t *wlc);
extern chanspec_t wlc_create_chspec(struct wlc_info *wlc, uint8 channel);
extern uint16 wlc_get_btcx_txmode(struct wlc_info *wlc);
extern bool wlc_set_btcx_txmode(struct wlc_info *wlc, uint16 bits, uint16 mask);
extern uint16 wlc_get_btcx_txop(wlc_info_t *wlc);
extern bool wlc_set_btcx_txop(wlc_info_t *wlc, uint16 txop);

#if defined(WLTEST)
extern void* wlc_tx_testframe(wlc_info_t *wlc, struct ether_addr *da,
	struct ether_addr *sa, ratespec_t rate_override, int length);
#endif
extern uint16 wlc_pretbtt_calc(wlc_bsscfg_t *cfg);
extern void wlc_pretbtt_set(wlc_bsscfg_t *cfg);
extern void wlc_macctrl_init(wlc_bsscfg_t *cfg);
extern uint32 wlc_recover_tsf32(wlc_info_t *wlc, wlc_d11rxhdr_t *rxhdr);
extern int wlc_get_revision_info(wlc_info_t *wlc, void *buf, uint len);
extern void wlc_out(wlc_info_t *wlc);
extern int wlc_bandlock(struct wlc_info *, int val);
extern int wlc_set_rate_override(wlc_info_t *wlc, int band, int32 rate, bool mcast);
extern void wlc_pcie_war_ovr_update(wlc_info_t *wlc, uint8 aspm);
#if defined(STA) && defined(AP)
extern bool wlc_apup_allowed(wlc_info_t *wlc);
#endif /* STA && AP */

#if defined(WL11N) || defined(WLTEST)
extern int wlc_set_nrate_override(wlc_info_t *wlc, wlcband_t *cur_band, int32 int_val);
#endif /* defined(WL11N) || defined(WLTEST) */

#if defined(BCMTSTAMPEDLOGS)
extern void wlc_log(wlc_info_t *wlc, const char* str, uint32 p1, uint32 p2);
#else
#define wlc_log(wlc, str, p1, p2)	{ }
#endif /* defined(BCMTSTAMPEDLOGS) */

extern void wlc_lifetime_set(wlc_info_t *wlc, void *sdu, uint32 lifetime);

extern void wlc_set_home_chanspec(wlc_info_t *wlc, chanspec_t chanspec);

#ifdef STA
extern void wlc_bss_clear_bssid(wlc_bsscfg_t *cfg);
#if defined(WL_PM2_RCV_DUR_LIMIT)
extern void wlc_pm2_rcv_reset(wlc_info_t *wlc);
#else
#define wlc_pm2_rcv_reset(wlc)
#endif
extern void wlc_freqtrack_reset(wlc_info_t *wlc);
extern void wlc_watchdog_upd(wlc_info_t *wlc, bool tbtt);
extern void wlc_pspoll_timer_upd(wlc_info_t *wlc, bool allow);
extern void wlc_apsd_trigger_upd(wlc_info_t *wlc, bool allow);
extern bool wlc_ps_allowed(wlc_info_t *wlc);
extern bool wlc_stay_awake(wlc_info_t *wlc);
extern bool wlc_rsn_ucast_lookup(struct rsn_parms *rsn, uint8 auth);
extern void wlc_handle_ap_lost(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif /* STA */

#ifdef WME
extern void wlc_wme_initparams_sta(wlc_info_t *wlc, wme_param_ie_t *pe);
#endif

extern void wlc_protection_g_sync(wlc_info_t *wlc);
#ifdef WL11N
extern void wlc_protection_ncfg_sync(wlc_info_t *wlc);
extern void wlc_ht_obss_scan_reset(wlc_info_t *wlc);
#else
#define wlc_ht_obss_scan_reset(a)	do {} while (0)
#endif

extern bool wlc_csa_quiet(wlc_info_t *wlc, uint8 *tag, uint tag_len);

#if defined(WL11D)
extern int wlc_compatible_country(wlc_info_t *wlc, const char *country_abbrev);
#endif
#if defined(WL11H) || defined(WL11D)
extern void wlc_adopt_country_ie(wlc_info_t *wlc, const bcm_tlv_t *country_ie);
extern int wlc_parse_country_ie(wlc_info_t *wlc, const bcm_tlv_t *ie, char *country,
	chanvec_t *valid_channels, int8 *tx_pwr);
#endif /* WL11H || WL11D */

extern void wlc_bss_list_free(wlc_info_t *wlc, wlc_bss_list_t *bss_list);
extern void wlc_bss_list_xfer(wlc_bss_list_t *from, wlc_bss_list_t *to);

extern bool wlc_ifpkt_chk_cb(void *p, int idx);

extern void wlc_process_event(wlc_info_t *wlc, wlc_event_t *e);

#endif /* WLC_HIGH:End */

extern uint8 wlc_local_constraint_qdbm(wlc_info_t *wlc);
extern void wlc_send_tpc_request(wlc_info_t *wlc, struct ether_addr *da);
#endif	/* _wlc_h_ */
