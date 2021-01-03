/*
 * BSS Config related declarations and exported functions for
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
 * $Id: wlc_bsscfg.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */
#ifndef _WLC_BSSCFG_H_
#define _WLC_BSSCFG_H_

#include <wlc_types.h>

/* Check if a particular BSS config is AP or STA */
#if defined(AP) && !defined(STA)
#define BSSCFG_AP(cfg)		(1)
#define BSSCFG_STA(cfg)		(0)
#elif !defined(AP) && defined(STA)
#define BSSCFG_AP(cfg)		(0)
#define BSSCFG_STA(cfg)		(1)
#else
#define BSSCFG_AP(cfg)		((cfg)->_ap)
#define BSSCFG_STA(cfg)		(!(cfg)->_ap)
#endif /* defined(AP) && !defined(STA) */

#ifdef STA
#define BSSCFG_IBSS(cfg)	(/* BSSCFG_STA(cfg) && */!(cfg)->BSS)
#else
#define BSSCFG_IBSS(cfg)	(0)
#endif

/* forward declarations */
typedef struct wlc_bsscfg wlc_bsscfg_t;
struct scb;
struct wlc_info;
struct wlc_bsscfg;
struct wl_if;
#ifdef WLP2P
#include <proto/p2p.h>
#include <wlioctl.h>
#endif

#include <wlc_rate.h>

/* virtual interface */
struct wlc_if {
	uint8		type;		/* WLC_IFTYPE_BSS or WLC_IFTYPE_WDS */
	uint8		index;		/* assigned in wl_add_if(), index of the wlif if any,
					 * not necessarily corresponding to bsscfg._idx or
					 * AID2PVBMAP(scb).
					 */
	uint32		flags;		/* flags for the interface */
	struct wl_if	*wlif;		/* pointer to wlif */
	union {
		struct scb *scb;		/* pointer to scb if WLC_IFTYPE_WDS */
		struct wlc_bsscfg *bsscfg;	/* pointer to bsscfg if WLC_IFTYPE_BSS */
	} u;
};

#define NTXRATE			64	/* # tx MPDUs rate is reported for */

/* virtual interface types */
#define WLC_IFTYPE_BSS 1		/* virt interface for a bsscfg */
#define WLC_IFTYPE_WDS 2		/* virt interface for a wds */

/* flags for the interface */
#define WLC_IF_PKT_80211	0x01	/* this interface expects 80211 frames */

/* mac list */
#define MAXMACLIST	64		/* max # source MAC matches */

#define BCN_TEMPLATE_COUNT 2

/* Iterator for AP bss configs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_AP(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_AP(cfg))

/* Iterator for "up" AP bss configs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_UP_AP(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_AP(cfg) && cfg->up)

/* Iterator for "associated" STA bss configs:  (wlc_info_t *wlc, int idx, wlc_bsscfg_t *cfg) */
#define FOREACH_AS_STA(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]) && BSSCFG_STA(cfg) && cfg->associated)

/* As above for all non-NULL BSS configs */
#define FOREACH_BSS(wlc, idx, cfg) \
	for (idx = 0; (int) idx < WLC_MAXBSSCFG; idx++) \
		if ((cfg = (wlc)->bsscfg[idx]))

#define WLC_IS_CURRENT_BSSID(cfg, bssid) \
	(!bcmp((char*)(bssid), (char*)&((cfg)->BSSID), ETHER_ADDR_LEN))
#define WLC_IS_CURRENT_SSID(cfg, ssid, len) \
	(((len) == (cfg)->SSID_len) && \
	 !bcmp((char*)(ssid), (char*)&((cfg)->SSID), (len)))

#define AP_BSS_UP_COUNT(wlc) wlc_ap_bss_up_count(wlc)

/*
 * Software packet template (spt) structure; for beacons and (maybe) probe responses.
 */
#define WLC_SPT_COUNT_MAX 2
/* Turn on define to get stats on SPT */
/* #define WLC_SPT_DEBUG */

typedef struct wlc_spt
{
	uint32		in_use_bitmap;	/* Bitmap of entries in use by DMA */
	wlc_pkt_t	pkts[WLC_SPT_COUNT_MAX];	/* Pointer to array of pkts */
	int		latest_idx;	/* Most recently updated entry */
	int		dtim_count_offset; /* Where in latest_idx is dtim_count_offset */
	uint8		*tim_ie;	/* Pointer to start of TIM IE in current packet */
	bool		bcn_modified;	/* Ucode versions, TRUE: push out to shmem */

#if defined(WLC_SPT_DEBUG)
	uint32		tx_count;	/* Debug: Number of times tx'd */
	uint32		suppressed;	/* Debug: Number of times supressed */
#endif /* WLC_SPT_DEBUG */
} wlc_spt_t;

extern int wlc_spt_init(struct wlc_info *wlc, wlc_spt_t *spt, int count, int len);
extern void wlc_spt_deinit(struct wlc_info *wlc, wlc_spt_t *spt, int pkt_free_force);

/* In the case of 2 templates, return index of one not in use; -1 if both in use */
#define SPT_PAIR_AVAIL(spt) \
	(((spt)->in_use_bitmap == 0) || ((spt)->in_use_bitmap == 0x2) ? 0 : \
	((spt)->in_use_bitmap == 0x1) ? 1 : -1)

/* Is the given pkt index in use */
#define SPT_IN_USE(spt, idx) (((spt)->in_use_bitmap & (1 << (idx))) != 0)

#define SPT_LATEST_PKT(spt) ((spt)->pkts[(spt)->latest_idx])

/* MBSS debug counters */
typedef struct wlc_mbss_cnt {
	uint32		prq_directed_entries; /* Directed PRQ entries seen */
	uint32		prb_resp_alloc_fail;  /* Failed allocation on probe response */
	uint32		prb_resp_tx_fail;     /* Failed to TX probe response */
	uint32		prb_resp_retrx_fail;  /* Failed to TX probe response */
	uint32		prb_resp_retrx;       /* Retransmit suppressed probe resp */
	uint32		prb_resp_ttl_expy;    /* Probe responses suppressed due to TTL expry */
	uint32		bcn_tx_failed;	      /* Failed to TX beacon frame */

	uint32		mc_fifo_max;	/* Max number of BC/MC pkts in DMA queues */
	uint32		bcmc_count;	/* Total number of pkts sent thru BC/MC fifo */

	/* Timing and other measurements for PS transitions */
	uint32		ps_trans;	/* Total number of transitions started */
} wlc_mbss_cnt_t;

/* Set the DTIM count in the latest beacon */
#define BCN_TIM_DTIM_COUNT_OFFSET (TLV_BODY_OFF + DOT11_MNG_TIM_DTIM_COUNT) /* 2 */
#define BCN_TIM_BITMAPCTRL_OFFSET (TLV_BODY_OFF + DOT11_MNG_TIM_BITMAP_CTL) /* 4 */
#define BCN_TIM_DTIM_COUNT_SET(tim, value) (tim)[BCN_TIM_DTIM_COUNT_OFFSET] = (value)
#define BCN_TIM_BCMC_FLAG_SET(tim)         (tim)[BCN_TIM_BITMAPCTRL_OFFSET] |= 0x1
#define BCN_TIM_BCMC_FLAG_RESET(tim)       (tim)[BCN_TIM_BITMAPCTRL_OFFSET] &= 0xfe

/* Vendor IE definitions */
#define VNDR_IE_EL_HDR_LEN	(sizeof(void *))
#define VNDR_IE_MAX_TOTLEN	256	/* Space for vendor IEs in Beacons and Probe Responses */

typedef struct vndr_ie_listel {
	struct vndr_ie_listel *next_el;
	vndr_ie_info_t vndr_ie_infoel;
} vndr_ie_listel_t;

/* association states */
typedef struct wlc_assoc {
	struct wl_timer *timer;		/* timer for auth/assoc request timeout */
	uint	type;			/* roam, association, or recreation */
	uint	state;			/* current state in assoc process */
	uint	flags;			/* control flags for assoc */

	bool	preserved;		/* Whether association was preserved */
	uint	recreate_bi_timeout;	/* bcn intervals to wait to detect our former AP
					 * when performing an assoc_recreate
					 */
	uint	verify_timeout;		/* ms to wait to allow an AP to respond to class 3
					 * data if it needs to disassoc/deauth
					 */
	uint8	retry_max;		/* max. retries */
	uint8	ess_retries;		/* number of ESS retries */
	uint8	bss_retries;		/* number of BSS retries */

	uint16	capability;		/* next (re)association request overrides */
	uint16	listen;
	struct ether_addr bssid;
	uint8	*ie;
	uint	ie_len;

	struct dot11_assoc_req *req;	/* last (re)association request */
	uint	req_len;
	bool	req_is_reassoc;		/* is a reassoc */
	struct dot11_assoc_resp *resp;	/* last (re)association response */
	uint	resp_len;
} wlc_assoc_t;

/* roam states */
#define ROAM_CACHELIST_SIZE 	4
typedef struct wlc_roam {
	uint	bcn_timeout;		/* seconds w/o bcns until loss of connection */
	bool	assocroam;		/* roam to preferred assoc band in oid bcast scan */
	bool	off;			/* disable roaming */
	uint8	time_since_bcn;		/* second count since our last beacon from AP */
	uint8	tbtt_since_bcn;		/* tbtt count since our last beacon from AP */
	uint8	uatbtt_tbtt_thresh;	/* tbtts w/o bcns until unaligned tbtt checking */
	uint8	tbtt_thresh;		/* tbtts w/o bcns until roaming attempt */
	uint8	minrate_txfail_cnt;	/* tx fail count at min rate */
	uint8	minrate_txpass_cnt;	/* number of consecutive frames at the min rate */
	uint	minrate_txfail_thresh;	/* min rate tx fail threshold for roam */
	uint	minrate_txpass_thresh;	/* roamscan threshold for being stuck at min rate */
	uint	txpass_cnt;		/* turn off roaming if we get a better tx rate */
	uint	txpass_thresh;		/* turn off roaming after x many packets */
	uint32	tsf_h;			/* TSF high bits (to detect retrograde TSF) */
	uint32	tsf_l;			/* TSF low bits (to detect retrograde TSF) */
	uint	scan_block;		/* roam scan frequency mitigator */
	uint	ratebased_roam_block;	/* limits mintxrate/txfail roaming frequency */
	uint	partialscan_period;	/* user-specified roam scan period */
	uint	fullscan_period; 	/* time between full roamscans */
	uint	reason;			/* cache so we can report w/WLC_E_ROAM event */
	bool	active;			/* RSSI based roam in progress */
	bool	cache_valid;		/* RSSI roam cache is valid */
	uint	time_since_upd;		/* How long since our update? */
	uint	fullscan_count;		/* Count of full rssiroams */
	int	prev_rssi;		/* Prior RSSI, used to invalidate cache */
	uint	cache_numentries;	/* # of rssiroam APs in cache */
	bool	thrash_block_active;	/* Some/all of our APs are unavaiable to reassoc */
	bool	motion;			/* We are currently moving */
	int	RSSIref;		/* trigger for motion detection */
	uint16	motion_dur;		/* How long have we been moving? */
	uint16	motion_timeout;		/* Time left using modifed values */
	uint8	motion_rssi_delta;	/* Motion detect RSSI delta */
	bool	roam_scan_piggyback;	/* Use periodic broadcast scans as roam scans */
	bool	piggyback_enab;		/* Turn on/off roam scan piggybacking */
	struct {			/* Roam cache info */
		chanspec_t chanspec;
		struct ether_addr BSSID;
		uint16 time_left_to_next_assoc;
	} cached_ap[ROAM_CACHELIST_SIZE];
	uint8	ap_environment;		/* Auto-detect the AP density of the environment */
	bool	bcns_lost;		/* indicate if the STA can see the AP */
	uint8	consec_roam_bcns_lost;	/* counter to keep track of consecutive calls
					 * to wlc_roam_bcns_lost function
					 */
} wlc_roam_t;

typedef struct wlc_link_qual {
	/* RSSI moving average */
	int	*rssi_pkt_window;	/* RSSI moving average window */
	int	rssi_pkt_index;		/* RSSI moving average window index */
	int	rssi_pkt_tot;		/* RSSI moving average total */
	uint16  rssi_pkt_count;		/* RSSI moving average count (maximum MA_WINDOW_SZ) */
	uint16  rssi_pkt_win_sz;	/* RSSI moving average window size (maximum MA_WINDOW_SZ) */
	int	rssi;			/* RSSI moving average */

	/* SNR moving average */
	int	snr;			/* SNR moving average */
	int32 	snr_ma;			/* SNR moving average total */
	uint8	snr_window[MA_WINDOW_SZ];	/* SNR moving average window */
	int	snr_index;		/* SNR moving average window index */
	int8 	snr_no_of_valid_values;	/* number of valid values in the window */

	/* RSSI event notification */
	wl_rssi_event_t *rssi_event;	/* RSSI event notification configuration. */
	uint8 rssi_level;		/* current rssi based on notification configuration */
	struct wl_timer *rssi_event_timer;	/* timer to limit event notifications */
	bool is_rssi_event_timer_active;	/* flag to indicate timer active */
} wlc_link_qual_t;

#ifdef NOT_YET
/* PM states */
typedef struct wlc_pm_st {
	bool	PMawakebcn;		/* bcn recvd during current waking state */
	bool	PMpending;		/* waiting for tx status with PM indicated set */
	bool	priorPMstate;		/* Detecting PM state transitions */
	bool	PSpoll;			/* whether there is an outstanding PS-Poll frame */
} wlc_pm_st_t;
#endif /* NOT_YET */

/* join targets sorting preference */
#define MAXJOINPREFS		5	/* max # user-supplied join prefs */
#define MAXWPACFGS		16	/* max # wpa configs */
typedef struct {
	struct {
		uint8 type;		/* type */
		uint8 start;		/* offset */
		uint8 bits;		/* width */
		uint8 reserved;
	} field[MAXJOINPREFS];		/* preference field, least significant first */
	uint fields;			/* # preference fields */
	uint prfbmp;			/* preference types bitmap */
	struct {
		uint8 akm[WPA_SUITE_LEN];	/* akm oui */
		uint8 ucipher[WPA_SUITE_LEN];	/* unicast cipher oui */
		uint8 mcipher[WPA_SUITE_LEN];	/* multicast cipher oui */
	} wpa[MAXWPACFGS];		/* wpa configs, least favorable first */
	uint wpas;			/* # wpa configs */
	uint8 band;			/* 802.11 band pref */
} wlc_join_pref_t;

#ifdef SMF_STATS
typedef struct wlc_smfs_elem {
	struct wlc_smfs_elem *next;
	wl_smfs_elem_t smfs_elem;
} wlc_smfs_elem_t;

typedef struct wlc_smf_stats {
	wl_smf_stats_t smfs_main;
	uint32 count_excl; /* counts for those sc/rc code excluded from the interested group */
	wlc_smfs_elem_t *stats;
} wlc_smf_stats_t;

typedef struct wlc_smfs_info {
	uint32 enable;
	wlc_smf_stats_t smf_stats[SMFS_TYPE_MAX];
} wlc_smfs_info_t;
#endif /* SMF_STATS */

#ifdef WLP2P
/* number of schedules */
#define WLC_P2P_MAX_ABS_SCHED	2	/* two concurrent Absence schedules */
#define WLC_P2P_MAX_SCHED	(WLC_P2P_MAX_ABS_SCHED + 1)	/* + one presence schedule */

typedef struct wlc_p2p_sched {
	uint8 idx;			/* current descriptor index in 'desc' */
	uint8 cnt;			/* the number of descriptors in 'desc' */
	uint8 option;			/* see WL_P2P_SCHED_OPTION_XXXX */
	wl_p2p_sched_desc_t *desc;	/* len = cnt * sizeof(wl_p2p_sched_desc_t) */
} wlc_p2p_sched_t;

/* bsscfg */
#define WLC_P2P_BSSCFG_GGO	NULL

typedef struct wlc_p2p_info {
	bool enable;			/* enable/disable p2p */
	bool ps;			/* GO is in PS state */
	uint16 flags;			/* see 'flags' below */
	/* schedules - see 'sched' index below */
	wlc_p2p_sched_t sched[WLC_P2P_MAX_SCHED];
	/* current OppPS and CTWindow */
	bool ops;
	uint8 ctw;
	/* current absence schedule */
	uint8 id;			/* NoA SE index in beacons */
	uint8 action;			/* see WL_P2P_SCHED_ACTION_XXXX */
	wl_p2p_sched_desc_t cur;
	/* d11 info - see 'M_P2P_BSS_BLK' in d11.h */
	uint8 d11cbi;			/* d11 SHM per BSS control block index */
	/* network parms */
	uint32 tbtt;			/* next tbtt in local TSF time. */
	int32 tsfo;			/* TSF offset (local - remote) */
	/* back pointer to bsscfg, NULL for global GO */
	wlc_bsscfg_t *bsscfg;
} wlc_p2p_info_t;

/* p2p info 'flags' */
#define WLC_P2P_INFO_FLAG_CUR	0x01	/* 'cur' is valid */
#define WLC_P2P_INFO_FLAG_OPS	0x02	/* 'ops' is valid */
#define WLC_P2P_INFO_FLAG_ID	0x04	/* 'id' is valid */
#define WLC_P2P_INFO_FLAG_NET	0x08	/* network parms valid (Client assoc'd or GO is up) */

/* p2p info 'sched' index */
#define WLC_P2P_SCHED_ABS	0	/* scheduled Absence */
#define WLC_P2P_SCHED_REQ_ABS	1	/* requested Absence */
#define WLC_P2P_SCHED_REQ_PSC	2	/* requested Presence */

/* dedicate the last entry in M_ADDR_BMP_BLK for non P2P bsscfgs */
#define WLC_P2P_NON_P2P_ABI	(M_ADDR_BMP_BLK_SZ - 1)
#endif /* WLP2P */

/* BSS configuration state */
struct wlc_bsscfg {
	struct wlc_info	*wlc;		/* wlc to which this bsscfg belongs to. */
	bool		up;		/* is this configuration up operational */
	bool		enable;		/* is this configuration enabled */
	bool		_ap;		/* is this configuration an AP */
	bool		associated;	/* is BSS in ASSOCIATED state */
	struct wlc_if	*wlcif;		/* virtual interface, NULL for primary bsscfg */
	void		*sup;		/* pointer to supplicant state */
	int8		sup_type;	/* type of supplicant */
	bool		sup_enable_wpa;	/* supplicant WPA on/off */
	bool		BSS;		/* infraustructure or adhac */
	bool		dtim_programmed;
	void		*authenticator;	/* pointer to authenticator state */
	bool		sup_auth_pending;	/* flag for auth timeout */
	uint8		SSID_len;	/* the length of SSID */
	uint8		SSID[DOT11_MAX_SSID_LEN];	/* SSID string */
	bool		closednet_nobcnssid;	/* hide ssid info in beacon */
	bool		closednet_nobcprbresp;	/* Don't respond to broadcast probe requests */
	bool		ap_isolate;	/* true if isolating associated STA devices */
	struct scb *bcmc_scb[MAXBANDS]; /* one bcmc_scb per band */
	int8		_idx;		/* the index of this bsscfg,
					 * assigned at wlc_bsscfg_alloc()
					 */
	/* MAC filter */
	uint		nmac;		/* # of entries on maclist array */
	int		macmode;	/* allow/deny stations on maclist array */
	struct ether_addr *maclist;	/* list of source MAC addrs to match */

	/* security */
	uint32		wsec;		/* wireless security bitvec */
#ifdef DSLCPE_WDSSEC
	uint32		wdswsec;	/* wireless security bitvec for WDS */
	bool		wdswsec_enable; /* enable use of wireless security bitvec for WDS */
#endif
	int16		auth;		/* 802.11 authentication: Open, Shared Key, WPA */
	int16		openshared;	/* try Open auth first, then Shared Key */
	bool		wsec_restrict;	/* drop unencrypted packets if wsec is enabled */
	bool		eap_restrict;	/* restrict data until 802.1X auth succeeds */
	uint16		WPA_auth;	/* WPA: authenticated key management */
	bool		wpa2_preauth;	/* default is TRUE, wpa_cap sets value */
#ifdef BCMWAPI_WAI
	bool		wai_restrict;	/* restrict data until WAI auth succeeds */
	bool		wai_preauth;	/* default is FALSE, wapi_cap sets value */
#endif /* BCMWAPI_WAI */
	bool		wsec_portopen;	/* indicates keys are plumbed */
	wsec_iv_t	wpa_none_txiv;	/* global txiv for WPA_NONE, tkip and aes */
	int		wsec_index;	/* 0-3: default tx key, -1: not set */
	wsec_key_t	*bss_def_keys[WLC_DEFAULT_KEYS];	/* default key storage */

#ifdef MBSS
	wlc_pkt_t	probe_template;	/* Probe response master packet, including PLCP */
	bool		prb_modified;	/* Ucode version: push to shm if true */
	wlc_spt_t	*bcn_template;	/* Beacon DMA template */
	uint32		maxassoc;	/* Max associations for this bss */
	int8		_ucidx;		/* the uCode index of this bsscfg,
					 * assigned at wlc_bsscfg_up()
					 */
	uint32		mc_fifo_pkts;	/* Current number of BC/MC pkts sent to DMA queues */
	uint32		prb_ttl_us;     /* Probe rsp time to live since req. If 0, disabled */
#ifdef WLCNT
	wlc_mbss_cnt_t *cnt;		/* MBSS debug counters */
#endif
#if defined(BCMDBG_MBSS_PROFILE)
	uint32		ps_start_us;	/* When last PS (off) transition started */
	uint32		max_ps_off_us;	/* Max delay time for out-of-PS transitions */
	uint32		tot_ps_off_us;	/* Total time delay for out-of-PS transitions */
	uint32		ps_off_count;	/* Number of deferred out-of-PS transitions completed */
	bool		bcn_tx_done;	/* TX done on sw beacon */
#endif /* BCMDBG_MBSS_PROFILE */
#endif /* MBSS */
	/* TKIP countermeasures */
	bool		tkip_countermeasures;	/* flags TKIP no-assoc period */
	uint32		tk_cm_dt;	/* detect timer */
	uint32		tk_cm_bt;	/* blocking timer */
	uint32		tk_cm_bt_tmstmp;    /* Timestamp when TKIP BT is activated */
	bool		tk_cm_activate;	/* activate countermeasures after EAPOL-Key sent */
#ifdef AP
	uint8		aidmap[AIDMAPSZ];	/* aid map */
#endif
#ifdef WMF
	bool		wmf_enable;	/* WMF is enabled or not */
	struct wlc_wmf_instance	*wmf_instance; /* WMF instance handle */
#endif
#ifdef MCAST_REGEN
	bool		mcast_regen_enable;	/* Multicast Regeneration is enabled or not */
#endif
	vndr_ie_listel_t	*vndr_ie_listp;	/* dynamic list of Vendor IEs */
	struct ether_addr	BSSID;		/* BSSID (associated) */
	struct ether_addr	cur_etheraddr;	/* h/w address */
	uint16                  bcmc_fid;	/* the last BCMC FID queued to TX_BCMC_FIFO */
	uint16                  bcmc_fid_shm;	/* the last BCMC FID written to shared mem */

	uint32		flags;		/* WLC_BSSCFG flags; see below */
#ifdef STA
	/* Association parameters. Used to limit the scan in join process. Saved before
	 * starting a join process and freed after finishing the join process regardless
	 * if the join is succeeded or failed.
	 */
	wl_assoc_params_t	*assoc_params;
	uint16			assoc_params_len;
#endif
	struct ether_addr	prev_BSSID;	/* MAC addr of last associated AP (BSS) */

#if defined(MACOSX)
	bool	sendup_mgmt;	/* sendup mgmt per packet filter setting */
#endif

	/* for Win7 */
	uint8		*bcn;		/* AP beacon */
	uint		bcn_len;	/* AP beacon length */
	bool		ar_disassoc;	/* disassociated in associated recreation */

	int		auth_atmptd;		/* auth type (open/shared) attempted */

	pmkid_cand_t	pmkid_cand[MAXPMKID];	/* PMKID candidate list */
	uint		npmkid_cand;		/* num PMKID candidates */
	pmkid_t		pmkid[MAXPMKID];	/* PMKID cache */
	uint		npmkid;			/* num cached PMKIDs */

	wlc_bss_info_t	*target_bss;		/* BSS parms during tran. to ASSOCIATED state */
	wlc_bss_info_t	*current_bss;		/* BSS parms in ASSOCIATED state */

	wlc_assoc_t	*assoc;
	wlc_roam_t	*roam;
	wlc_link_qual_t	*link;

	/* PM states */
	bool		PMawakebcn;		/* bcn recvd during current waking state */
	bool		PMpending;		/* waiting for tx status with PM indicated set */
	bool		priorPMstate;		/* Detecting PM state transitions */
	bool		PSpoll;			/* whether there is an outstanding PS-Poll frame */


	/* join targets sorting preference */
	wlc_join_pref_t *join_pref;

	/* Give RSSI score of APs in preferred band a boost
	 * to make them fare better instead of always preferring
	 * the band. This is maintained separately from regular
	 * join pref as no order can be imposed on this preference
	 */
	struct {
		uint8 band;
		uint8 rssi;
	} join_pref_rssi_delta;

#ifdef WLP2P
	uint8		rcmta_ra_idx;		/* RCMTA idx for RA to be used for P2P */
	uint8		rcmta_bssid_idx;	/* RCMTA idx for BSSID to be used for P2P */
#endif
	/* BSSID entry in RCMTA, use the wsec key management infrastructure to
	 * manage the RCMTA entries.
	 */
	wsec_key_t	*rcmta;

#ifdef SMF_STATS
	wlc_smfs_info_t *smfs_info;
#endif /* SMF_STATS */

	int8	PLCPHdr_override;	/* 802.11b Preamble Type override */

	/* 'unique' ID of this bsscfg, assigned at bsscfg allocation */
	uint16		ID;

#ifdef WLP2P
	wlc_p2p_info_t	*p2p;
#endif
	uint		txrspecidx;		/* index into tx rate circular buffer */
	ratespec_t     	txrspec[NTXRATE][2];	/* circular buffer of prev MPDUs tx rates */

#ifdef WL_BSSCFG_TX_SUPR
	struct pktq	*psq;		/* PS-defer queue */
	bool tx_start_pending;
#endif
};

/* wlc_bsscfg_t flags */
#define WLC_BSSCFG_PRESERVE     0x1		/* preserve STA association on driver down/up */
#define WLC_BSSCFG_WME_DISABLE	0x2		/* Do not advertise WME for this BSS */
#define WLC_BSSCFG_PS_OFF_TRANS	0x4		/* BSS is in transition to PS-OFF */
#define WLC_BSSCFG_SW_BCN	0x8		/* The BSS is generating beacons in SW */
#define WLC_BSSCFG_SW_PRB	0x10		/* The BSS is generating probe responses in SW */
#define WLC_BSSCFG_HW_BCN	0x20		/* The BSS is generating beacons in HW */
#define WLC_BSSCFG_HW_PRB	0x40		/* The BSS is generating probe responses in HW */
#define WLC_BSSCFG_DPT		0x80		/* The BSS is for DPT link */
#define WLC_BSSCFG_MBSS16	0x100		/* The BSS creates bcn/prbresp template for ucode */
#define WLC_BSSCFG_BTA		0x200		/* The BSS is for BTA link */
#define WLC_BSSCFG_NOBCMC	0x400		/* The BSS has no broadcast/multicast traffic */
#define WLC_BSSCFG_NOIF		0x800		/* The BSS has no OS presentation */
#define WLC_BSSCFG_11N_DISABLE	0x1000		/* Do not advertise .11n IEs for this BSS */
#define WLC_BSSCFG_P2P		0x2000		/* The BSS is for p2p link */
#define WLC_BSSCFG_11H_DISABLE	0x4000		/* Do not follow .11h rules for this BSS */
#define WLC_BSSCFG_NATIVEIF	0x8000		/* The BSS uses native OS if */
#define WLC_BSSCFG_P2P_DISC	0x10000		/* The BSS is for p2p discovery */
#define WLC_BSSCFG_TX_SUPR	0x20000		/* The BSS is in absence mode */
#define WLC_BSSCFG_SRADAR_ENAB	0x40000		/* follow specail radar rules for soft/ext ap */
#define WLC_BSSCFG_P2P_IE	0x80000		/* Include P2P IEs */


/* Init strings for flags */
#define WLC_BSSCFG_FLAGS_STR_INIT \
	"PRESERVE_ASSOC", \
	"WME_DISABLE", \
	"PS_OFF_TRANS", \
	"SW_BCN", \
	"SW_PRB", \
	"HW_BCN", \
	"HW_PRB", \
	"DPT", \
	"MBSS16", \
	"BTA", \
	"NOBCMC", \
	"NOIF", \
	"11N_DISABLE", \
	"P2P", \
	"11H_DISABLE", \
	"NATIVEIF", \
	"P2P_DISC", \
	"TX_SUPR", \
	"SRADAR", \
	"P2P_IE"

/* Flags that should not be cleared on AP bsscfg up */
#define WLC_BSSCFG_PERSIST_FLAGS (\
		WLC_BSSCFG_WME_DISABLE | \
		WLC_BSSCFG_PRESERVE | \
		WLC_BSSCFG_BTA | \
		WLC_BSSCFG_NOBCMC | \
		WLC_BSSCFG_NOIF | \
		WLC_BSSCFG_11N_DISABLE | \
		WLC_BSSCFG_P2P | \
		WLC_BSSCFG_11H_DISABLE | \
		WLC_BSSCFG_P2P_DISC | \
		WLC_BSSCFG_NATIVEIF | \
		WLC_BSSCFG_SRADAR_ENAB | \
	0)

#define WLC_BSSCFG(wlc, idx) \
	(((idx) < WLC_MAXBSSCFG && (idx) >= 0) ? ((wlc)->bsscfg[idx]) : NULL)

#define HWBCN_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_HW_BCN) != 0)
#define HWPRB_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_HW_PRB) != 0)

#define BSSCFG_HAS_NOIF(cfg)	(((cfg)->flags & WLC_BSSCFG_NOIF) != 0)
#define BSSCFG_HAS_NATIVEIF(cfg)	(((cfg)->flags & WLC_BSSCFG_NATIVEIF) != 0)

#define BSSCFG_DPT_ENAB(cfg)	FALSE

#ifdef WLBDD
#define BSSCFG_BDD_ENAB(cfg)	((cfg)->flags & WLC_BSSCFG_DPT)
#else
#define BSSCFG_BDD_ENAB(cfg)	FALSE
#endif

#ifdef WMF
#define WMF_ENAB(cfg)	((cfg)->wmf_enable)
#else
#define WMF_ENAB(cfg)	FALSE
#endif

#ifdef MCAST_REGEN
#define MCAST_REGEN_ENAB(cfg)	((cfg)->mcast_regen_enable)
#else
#define MCAST_REGEN_ENAB(cfg)	FALSE
#endif

#ifdef SMF_STATS
#define SMFS_ENAB(cfg) ((cfg)->smfs_info->enable)
#else
#define SMFS_ENAB(cfg) FALSE
#endif /* SMF_STATS */

typedef struct wlc_prq_info_s {
	shm_mbss_prq_entry_t source;   /* To get ta addr and timestamp directly */
	bool is_directed;         /* Non-broadcast (has bsscfg associated with it) */
	bool directed_ssid;       /* Was request directed to an SSID? */
	bool directed_bssid;      /* Was request directed to a BSSID? */
	wlc_bsscfg_t *bsscfg;     /* The BSS Config associated with the request (if not bcast) */
	shm_mbss_prq_ft_t frame_type;  /* 0: cck; 1: ofdm; 2: mimo; 3 rsvd */
	bool up_band;             /* Upper or lower sideband of 40 MHz for MIMO phys */
	uint8 plcp0;              /* First byte of PLCP */
} wlc_prq_info_t;

extern wlc_bsscfg_t *wlc_bsscfg_primary(struct wlc_info *wlc);
extern int wlc_bsscfg_primary_init(struct wlc_info *wlc);
extern void wlc_bsscfg_primary_cleanup(struct wlc_info *wlc);
extern wlc_bsscfg_t *wlc_bsscfg_alloc(struct wlc_info *wlc, int idx, uint flags,
                                      struct ether_addr *ea, bool ap);
extern int wlc_bsscfg_ap_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern int wlc_bsscfg_sta_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern int wlc_bsscfg_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern int wlc_bsscfg_reinit(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool ap);
extern int wlc_bsscfg_reinit_ext(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool ap);
extern int wlc_bsscfg_bta_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_dpt_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_bdd_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
#ifdef WLP2P
extern int wlc_bsscfg_p2p_init(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
#endif
extern void wlc_bsscfg_free(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_disablemulti(struct wlc_info *wlc);
extern int wlc_bsscfg_disable(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_down(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_up(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_enable(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_get_free_idx(struct wlc_info *wlc);
extern wlc_bsscfg_t *wlc_bsscfg_find(struct wlc_info *wlc, int idx, int *perr);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_hwaddr(struct wlc_info *wlc, struct ether_addr *hwaddr);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_bssid(struct wlc_info *wlc, struct ether_addr *bssid);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_target_bssid(struct wlc_info *wlc,
	struct ether_addr *bssid);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_ssid(struct wlc_info *wlc, uint8 *ssid, int ssid_len);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_wlcif(struct wlc_info *wlc, wlc_if_t *wlcif);
extern wlc_bsscfg_t *wlc_bsscfg_find_by_ID(struct wlc_info *wlc, uint16 id);
extern int wlc_ap_bss_up_count(struct wlc_info *wlc);
#ifdef STA
extern int wlc_bsscfg_assoc_params_set(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg,
	wl_assoc_params_t *assoc_params, int assoc_params_len);
extern void wlc_bsscfg_assoc_params_reset(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);
#define wlc_bsscfg_assoc_params(bsscfg)	(bsscfg)->assoc_params
#endif
extern void wlc_bsscfg_SSID_set(wlc_bsscfg_t *bsscfg, uint8 *SSID, int len);
extern void wlc_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm);
extern void wlc_bsscfg_ID_assign(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg);

#define WLC_BCMCSCB_GET(wlc, bsscfg) \
	(((bsscfg)->flags & WLC_BSSCFG_NOBCMC) ? \
	 NULL : \
	 (bsscfg)->bcmc_scb[CHSPEC_WLCBANDUNIT((wlc)->home_chanspec)])
#define WLC_BSSCFG_IDX(bsscfg) ((bsscfg)->_idx)

extern int wlc_vndr_ie_getlen(wlc_bsscfg_t *bsscfg, uint32 pktflag, int *totie);
typedef bool (*vndr_ie_write_filter_fn_t)(wlc_bsscfg_t *bsscfg, uint type, vndr_ie_t *ie);
extern uint8 *wlc_vndr_ie_write_ext(wlc_bsscfg_t *bsscfg, vndr_ie_write_filter_fn_t filter,
	uint type, uint8 *cp, int buflen, uint32 pktflag);
#define wlc_vndr_ie_write(cfg, cp, buflen, pktflag) \
		wlc_vndr_ie_write_ext(cfg, NULL, -1, cp, buflen, pktflag)
#ifdef WLP2P
extern uint8 *wlc_p2p_vndr_ie_write(wlc_bsscfg_t *bsscfg, uint type,
	uint8 *cp, int buflen, uint32 pktflag);
#endif
extern vndr_ie_listel_t *wlc_vndr_ie_add_elem(wlc_bsscfg_t *bsscfg, uint32 pktflag,
	vndr_ie_t *vndr_iep);
extern vndr_ie_listel_t *wlc_vndr_ie_mod_elem(wlc_bsscfg_t *bsscfg, vndr_ie_listel_t *old_listel,
	uint32 pktflag, vndr_ie_t *vndr_iep);
extern int wlc_vndr_ie_add(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len);
extern int wlc_vndr_ie_del(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len);
extern int wlc_vndr_ie_get(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len);
extern void wlc_vndr_ie_free(wlc_bsscfg_t *bsscfg);

#if defined(AP)
extern uint16 wlc_bsscfg_newaid(wlc_bsscfg_t *cfg);
#endif /* AP */

#ifdef SMF_STATS
extern int wlc_smfstats_update(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, uint8 smfs_type,
	uint16 code);
extern int wlc_bsscfg_smfsinit(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
extern int wlc_bsscfg_get_smfs(wlc_bsscfg_t *cfg, int idx, char* buf, int len);
extern int wlc_bsscfg_clear_smfs(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
#else
#define wlc_smfstats_update(a, b, c, d) do {} while (0)
#define wlc_bsscfg_smfsinit(a, b) do {} while (0)
#define wlc_bsscfg_get_smfs(a, b, c, d) do {} while (0)
#define wlc_bsscfg_clear_smfs(a, b) do {} while (0)
#endif

#define WLC_BSSCFG_AUTH(cfg) ((cfg)->auth)

#define WLC_BSSCFG_WEPKEY(cfg, idx) \
	(((idx) < WLC_DEFAULT_KEYS && (int)(idx) >= 0) ? \
	((cfg)->bss_def_keys[idx]) : 0)

/* Extend WME_ENAB to per-BSS */
#define BSS_WME_ENAB(wlc, cfg) \
	(WME_ENAB((wlc)->pub) && !((cfg)->flags & WLC_BSSCFG_WME_DISABLE))

#ifdef WLBTAMP
/* Extend BTA_ENAB to per-BSS */
#define BSS_BTA_ENAB(wlc, cfg) \
	(BTA_ENAB((wlc)->pub) && (cfg) && ((cfg)->flags & WLC_BSSCFG_BTA))
#endif

/* Extend N_ENAB to per-BSS */
#define BSS_N_ENAB(wlc, cfg) \
	(N_ENAB((wlc)->pub) && !((cfg)->flags & WLC_BSSCFG_11N_DISABLE))

/* Extend WL11H_ENAB to per-BSS */
#define BSS_WL11H_ENAB(wlc, cfg) \
	(WL11H_ENAB(wlc) && !((cfg)->flags & WLC_BSSCFG_11H_DISABLE))

/* Extend P2P_ENAB to per-BSS */
#ifdef WLP2P
#define BSS_P2P_ENAB(wlc, cfg) \
	(P2P_ENAB((wlc)->pub) && ((cfg)->flags & WLC_BSSCFG_P2P))
#define BSS_P2P_DISC_ENAB(wlc, cfg) \
	(P2P_ENAB((wlc)->pub) && ((cfg)->flags & WLC_BSSCFG_P2P_DISC))
#else
#define BSS_P2P_ENAB(wlc, cfg) (FALSE)
#define BSS_P2P_DISC_ENAB(wlc, cfg) (FALSE)
#endif

#ifdef WL_BSSCFG_TX_SUPR
#define BSS_TX_SUPR(cfg) ((cfg)->flags & WLC_BSSCFG_TX_SUPR)
#else
#define BSS_TX_SUPR(cfg) 0
#endif

/* Macros related to Multi-BSS. */
#if defined(MBSS)
#define SOFTBCN_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_SW_BCN) != 0)
#define SOFTPRB_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_SW_PRB) != 0)
#define UCTPL_MBSS_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_MBSS16) != 0)
/* Define as all bits less than and including the msb shall be one's */
#define EADDR_TO_UC_IDX(eaddr, mask)		((eaddr).octet[5] & (mask))
#define WLC_BSSCFG_UCIDX(bsscfg)	((bsscfg)->_ucidx)
#define MBSS_BCN_ENAB(cfg)		(SOFTBCN_ENAB(cfg) || UCTPL_MBSS_ENAB(cfg))
#define MBSS_PRB_ENAB(cfg)		(SOFTPRB_ENAB(cfg) || UCTPL_MBSS_ENAB(cfg))

/*
 * BC/MC FID macros.  Only valid under MBSS
 *
 *    BCMC_FID_SHM_COMMIT  Committing FID to SHM; move driver's value to bcmc_fid_shm
 *    BCMC_FID_QUEUED	   Are any packets enqueued on the BC/MC fifo?
 */

extern void bcmc_fid_shm_commit(wlc_bsscfg_t *bsscfg);
#define BCMC_FID_SHM_COMMIT(bsscfg) bcmc_fid_shm_commit(bsscfg)

#define BCMC_PKTS_QUEUED(bsscfg) \
	(((bsscfg)->bcmc_fid_shm != INVALIDFID) || ((bsscfg)->bcmc_fid != INVALIDFID))

extern int wlc_write_mbss_basemac(struct wlc_info *wlc, const struct ether_addr *addr);

#else

#define SOFTBCN_ENAB(pub)    (0)
#define SOFTPRB_ENAB(pub)    (0)
#define WLC_BSSCFG_UCIDX(bsscfg) 0
#define wlc_write_mbss_basemac(wlc, addr) (0)

#define BCMC_FID_SHM_COMMIT(bsscfg)
#define BCMC_PKTS_QUEUED(bsscfg) 0
#define MBSS_BCN_ENAB(cfg)       0
#define MBSS_PRB_ENAB(cfg)       0
#define UCTPL_MBSS_ENAB(cfg)         0
#endif /* defined(MBSS) */

#define BSS_11H_SRADAR_ENAB(wlc, cfg)	(WL11H_ENAB(wlc) && BSSCFG_SRADAR_ENAB(cfg))
#define BSSCFG_SRADAR_ENAB(cfg)	((cfg)->flags & WLC_BSSCFG_SRADAR_ENAB)

#define BSSCFG_SAFEMODE(cfg)	0

#ifdef WL_BSSCFG_TX_SUPR
extern void wlc_bsscfg_tx_stop(wlc_bsscfg_t *bsscfg);
extern void wlc_bsscfg_tx_start(wlc_bsscfg_t *bsscfg);
extern bool wlc_bsscfg_txq_enq(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, void *sdu, uint prec);
extern void wlc_bsscfg_tx_check(struct wlc_info *wlc);
#else
#define wlc_bsscfg_tx_stop(a) do { } while (0);
#define wlc_bsscfg_tx_start(a) do { } while (0);
#define wlc_bsscfg_txq_enq(a, b, c, d) 0
#define wlc_bsscfg_tx_check(a) do { } while (0);
#endif
#endif	/* _WLC_BSSCFG_H_ */
