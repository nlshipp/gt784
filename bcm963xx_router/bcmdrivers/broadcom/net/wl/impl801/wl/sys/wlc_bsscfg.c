/*
 * BSS Configuration routines for
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
 * $Id: wlc_bsscfg.c,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#if defined(BCMSUP_PSK)
#include <proto/eapol.h>
#endif
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wlc_scb.h>
#if defined(BCMSUP_PSK)
#include <wlc_sup.h>
#endif
#if defined(BCMAUTH_PSK)
#include <wlc_auth.h>
#endif
#include <wl_export.h>
#include <wlc_channel.h>
#include <wlc_ap.h>
#ifdef WMF
#include <wlc_wmf.h>
#endif
#ifdef WIN7
#include <wlc_scan.h>
#endif /* WIN7 */
#include <wlc_alloc.h>
#include <wlc_assoc.h>
#ifdef WLP2P
#include <wlc_p2p.h>
#endif

#ifdef SMF_STATS
/* the status/reason codes of interest */
uint16 const smfs_sc_table[] = {
	DOT11_SC_SUCCESS,
	DOT11_SC_FAILURE,
	DOT11_SC_CAP_MISMATCH,
	DOT11_SC_REASSOC_FAIL,
	DOT11_SC_ASSOC_FAIL,
	DOT11_SC_AUTH_MISMATCH,
	DOT11_SC_AUTH_SEQ,
	DOT11_SC_AUTH_CHALLENGE_FAIL,
	DOT11_SC_AUTH_TIMEOUT,
	DOT11_SC_ASSOC_BUSY_FAIL,
	DOT11_SC_ASSOC_RATE_MISMATCH,
	DOT11_SC_ASSOC_SHORT_REQUIRED,
	DOT11_SC_ASSOC_SHORTSLOT_REQUIRED
};

uint16 const smfs_rc_table[] = {
	DOT11_RC_RESERVED,
	DOT11_RC_UNSPECIFIED,
	DOT11_RC_AUTH_INVAL,
	DOT11_RC_DEAUTH_LEAVING,
	DOT11_RC_INACTIVITY,
	DOT11_RC_BUSY,
	DOT11_RC_INVAL_CLASS_2,
	DOT11_RC_INVAL_CLASS_3,
	DOT11_RC_DISASSOC_LEAVING,
	DOT11_RC_NOT_AUTH,
	DOT11_RC_BAD_PC
};

#define MAX_SCRC_EXCLUDED	16
#endif /* SMF_STATS */

/* Local Functions */

#ifdef MBSS
static int wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif

static int _wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	struct ether_addr *ea, uint flags, bool ap);
#if defined(AP) || defined(STA)
static void _wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif

static int wlc_bsscfg_bcmcscbinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint bandindex);

#ifdef AP
static void wlc_bsscfg_ap_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif
#ifdef STA
static void wlc_bsscfg_sta_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
#endif

#ifdef SMF_STATS
static int wlc_bsscfg_smfsfree(struct wlc_info *wlc, wlc_bsscfg_t *cfg);
#endif /* SMF_STATS */

/* Count the number of bsscfgs (any type) that are enabled */
static int
wlc_bsscfg_ena_cnt(wlc_info_t *wlc)
{
	int idx, count;

	for (count = idx = 0; idx < WLC_MAXBSSCFG; idx++)
		if (wlc->bsscfg[idx] && wlc->bsscfg[idx]->enable)
			count++;
	return count;
}

/* Return the number of AP bsscfgs that are UP */
int
wlc_ap_bss_up_count(wlc_info_t *wlc)
{
	uint16 i, apbss_up = 0;
	wlc_bsscfg_t *bsscfg;

	FOREACH_UP_AP(wlc, i, bsscfg) {
		apbss_up++;
	}

	return apbss_up;
}

/*
 * Allocate and set up a software packet template
 * @param count The number of packets to use; must be <= WLC_SPT_COUNT_MAX
 * @param len The length of the packets to be allocated
 *
 * Returns 0 on success, < 0 on error.
 */

int
wlc_spt_init(wlc_info_t *wlc, wlc_spt_t *spt, int count, int len)
{
	int idx;

	if (count > WLC_SPT_COUNT_MAX) {
		return -1;
	}

	ASSERT(spt != NULL);
	bzero(spt, sizeof(*spt));

	for (idx = 0; idx < count; idx++) {
		if ((spt->pkts[idx] = PKTGET(wlc->osh, len, TRUE)) == NULL) {
			wlc_spt_deinit(wlc, spt, TRUE);
			return -1;
		}
	}

	spt->latest_idx = -1;

	return 0;
}

/*
 * Clean up a software template object;
 * if pkt_free_force is TRUE, will not check if the pkt is in use before freeing.
 * Note that if "in use", the assumption is that some other routine owns
 * the packet and will free appropriately.
 */

void
wlc_spt_deinit(wlc_info_t *wlc, wlc_spt_t *spt, int pkt_free_force)
{
	int idx;

	if (spt != NULL) {
		for (idx = 0; idx < WLC_SPT_COUNT_MAX; idx++) {
			if (spt->pkts[idx] != NULL) {
				if (pkt_free_force || !SPT_IN_USE(spt, idx)) {
					PKTFREE(wlc->osh, spt->pkts[idx], TRUE);
				} else {
					WLPKTFLAG_BSS_DOWN_SET(WLPKTTAG(spt->pkts[idx]), TRUE);
				}
			}
		}
		bzero(spt, sizeof(*spt));
	}
}

#if defined(MBSS)
static void
mbss_ucode_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bool cur_val, new_val;

	/* Assumes MBSS_EN has same value in all cores */
	cur_val = ((wlc_mhf_get(wlc, MHF1, WLC_BAND_AUTO) & MHF1_MBSS_EN) != 0);
	new_val = (MBSS_ENAB(wlc->pub) != 0);

	if (cur_val != new_val) {
		wlc_suspend_mac_and_wait(wlc);
		/* enable MBSS in uCode */
		WL_MBSS(("%s MBSS mode\n", new_val ? "Enabling" : "Disabling"));
		(void)wlc_mhf(wlc, MHF1, MHF1_MBSS_EN, new_val ? MHF1_MBSS_EN : 0, WLC_BAND_ALL);
		wlc_enable_mac(wlc);
	}
}

/* BCMC_FID_SHM_COMMIT - Committing FID to SHM; move driver's value to bcmc_fid_shm */
void
bcmc_fid_shm_commit(wlc_bsscfg_t *bsscfg)
{
	bsscfg->bcmc_fid_shm = bsscfg->bcmc_fid;
	bsscfg->bcmc_fid = INVALIDFID;
}

/* BCMC_FID_INIT - Set driver and shm FID to invalid */
#define BCMC_FID_INIT(bsscfg) do { \
		(bsscfg)->bcmc_fid = INVALIDFID; \
		(bsscfg)->bcmc_fid_shm = INVALIDFID; \
	} while (0)

static int
mbss_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int result = 0;
	int idx;
	wlc_bsscfg_t *bsscfg;

	/* Assumes MBSS is enabled for this BSS config herein */

	/* Set pre TBTT interrupt timer to 10 ms for now; will be shorter */
	wlc_write_shm(wlc, SHM_MBSS_PRE_TBTT, wlc->ap->pre_tbtt_us);

	/* if the BSS configs hasn't been given a user defined address or
	 * the address is duplicated, we'll generate our own.
	 */
	FOREACH_BSS(wlc, idx, bsscfg) {
		if (bsscfg == cfg)
			continue;
		if (bcmp(&bsscfg->cur_etheraddr, &cfg->cur_etheraddr, ETHER_ADDR_LEN) == 0)
			break;
	}
	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr) || idx < WLC_MAXBSSCFG) {
		result = wlc_bsscfg_macgen(wlc, cfg);
		if (result) {
			WL_ERROR(("wl%d.%d: mbss_bsscfg_up: unable to generate MAC address\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
			return result;
		}
	}

	/* Set the uCode index of this config */
	cfg->_ucidx = EADDR_TO_UC_IDX(cfg->cur_etheraddr, wlc->mbss_ucidx_mask);
	ASSERT(cfg->_ucidx <= wlc->mbss_ucidx_mask);
	wlc->hw2sw_idx[cfg->_ucidx] = WLC_BSSCFG_IDX(cfg);

	/* Allocate DMA space for beacon software template */
	result = wlc_spt_init(wlc, cfg->bcn_template, BCN_TEMPLATE_COUNT, BCN_TMPL_LEN);
	if (result < 0) {
		WL_ERROR(("wl%d.%d: mbss_bsscfg_up: unable to allocate beacon templates",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		return result;
	}
	/* Set the BSSCFG index in the packet tag for beacons */
	for (idx = 0; idx < BCN_TEMPLATE_COUNT; idx++) {
		WLPKTTAGBSSCFGSET(cfg->bcn_template->pkts[idx], WLC_BSSCFG_IDX(cfg));
	}

	/* Make sure that our SSID is in the correct uCode
	 * SSID slot in shared memory
	 */
	wlc_shm_ssid_upd(wlc, cfg);

	BCMC_FID_INIT(cfg);

	if (!MBSS_ENAB16(wlc->pub)) {
		cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
		cfg->flags |= (WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
	} else {
		cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB |
		                WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
		cfg->flags |= (WLC_BSSCFG_MBSS16);
	}

	return result;
}
#endif /* MBSS */

/* Clear non-persistant flags; by default, HW beaconing and probe resp */
#define WLC_BSSCFG_FLAGS_INIT(cfg) do { \
		(cfg)->flags &= WLC_BSSCFG_PERSIST_FLAGS; \
		(cfg)->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB); \
	} while (0)

int
wlc_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ret = BCME_OK;

	ASSERT(cfg != NULL);
	ASSERT(cfg->enable);

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_up(%s): stas/aps/associated %d/%d/%d\n",
	          wlc->pub->unit, (BSSCFG_AP(cfg) ? "AP" : "STA"),
	          wlc->stas_associated, wlc->aps_associated, wlc->pub->associated));

#ifdef AP
	if (BSSCFG_AP(cfg)) {
		char * bcn;
#ifdef STA
		bool mpc_out = wlc->mpc_out;
		bool was_up = wlc->pub->up;
		bool cfg_up = cfg->up;
#endif
		bcn = (char *) MALLOC(wlc->osh, BCN_TMPL_LEN);
		if (!bcn) {
			cfg->up = FALSE;
			return BCME_NOMEM;
		}

		if (WIN7_OS(wlc->pub) && cfg->bcn) {
			/* free old AP beacon info for restarting ap. note that when
			 * cfg->up cleared(below) bsscfg down does not free bcn memory
			 */
			MFREE(wlc->pub->osh, cfg->bcn, cfg->bcn_len);
			cfg->bcn = NULL;
			cfg->bcn_len = 0;
		}

		/* No SSID configured yet... */
		if (cfg->SSID_len == 0) {
			cfg->up = FALSE;
			if (bcn)
				MFREE(wlc->osh, (void *)bcn, BCN_TMPL_LEN);
			return BCME_ERROR;
		}

#ifdef WLAFTERBURNER
		if (APSTA_ENAB(wlc->pub) && wlc->afterburner) {
			WL_ERROR(("wl%d: Unable to bring up the AP bsscfg since ab is enabled\n",
				wlc->pub->unit));
			cfg->up = FALSE;
			if (bcn)
				MFREE(wlc->osh, (void *)bcn, BCN_TMPL_LEN);
			return BCME_ERROR;
		}
#endif /* WLAFTERBURNER */

#ifdef STA
		/* defer to any STA association in progress */
		if (APSTA_ENAB(wlc->pub) && !wlc_apup_allowed(wlc)) {
			WL_APSTA_UPDN(("wl%d: wlc_bsscfg_up: defer AP UP, STA associating: "
				       "stas/aps/associated %d/%d/%d, assoc_state/type %d/%d",
				       wlc->pub->unit, wlc->stas_associated, wlc->aps_associated,
				       wlc->pub->associated, cfg->assoc->state, cfg->assoc->type));
			cfg->up = FALSE;
			if (bcn)
				MFREE(wlc->osh, (void *)bcn, BCN_TMPL_LEN);
			return BCME_OK;
		}

		wlc->mpc_out = TRUE;
		wlc_radio_mpc_upd(wlc);
#endif /* STA */

		/* AP mode operation must have the driver up before bringing
		 * up a configuration
		 */
		if (!wlc->pub->up)
			goto end;

#ifdef STA
		if (!was_up && !cfg_up && cfg->up)
			goto end;
#endif

		/* Init (non-persistant) flags */
		WLC_BSSCFG_FLAGS_INIT(cfg);

#ifdef MBSS
		if (MBSS_ENAB(wlc->pub)) {
			if ((ret = mbss_bsscfg_up(wlc, cfg)) < 0)
				goto end;

			wlc_write_mbss_basemac(wlc, &wlc->vether_base);
		}
		mbss_ucode_set(wlc, cfg);
#endif /* MBSS */

		cfg->up = TRUE;

		wlc_ap_up(wlc->ap, cfg);

		/* for softap and extap, following special radar rules */
		/* return bad channel error if radar channel */
		/* when no station associated */
		/* won't allow soft/ext ap to be started on radar channel */
		if (BSS_11H_SRADAR_ENAB(wlc, cfg) &&
		    wlc_radar_chanspec(wlc->cmi, cfg->target_bss->chanspec) &&
		    !wlc->stas_associated) {
			WL_ERROR(("no assoc STA and starting soft or ext AP on radar channel %d\n",
			  CHSPEC_CHANNEL(cfg->target_bss->chanspec)));
			return BCME_BADCHAN;
		}

		wlc_bss_up(wlc->ap, cfg, bcn, BCN_TMPL_LEN);

		wlc_bss_update_beacon(wlc, cfg);

		if (WIN7_OS(wlc->pub)) {
			uint8 *buf;
			int len = BCN_TMPL_LEN;
			int ch = CHSPEC_CHANNEL(wlc_get_home_chanspec(cfg));
			/* indicate AP starting with channel info */
			wlc_bss_mac_event(wlc, cfg, WLC_E_AP_STARTED, NULL,
				WLC_E_STATUS_SUCCESS, 0, 0, &ch, sizeof(ch));
			/* save current AP beacon info */
			if ((buf = MALLOC(wlc->pub->osh, BCN_TMPL_LEN)) != NULL) {
				wlc_bcn_prb_body(wlc, FC_BEACON, cfg, buf, &len);
				if ((cfg->bcn = MALLOC(wlc->pub->osh, len)) != NULL) {
					bcopy(buf, cfg->bcn, len);
					cfg->bcn_len = len;
				}
				MFREE(wlc->pub->osh, buf, BCN_TMPL_LEN);
			}
		}

	end:

		if (cfg->up)
			WL_INFORM(("wl%d: BSS %d is up\n", wlc->pub->unit, cfg->_idx));
		if (bcn)
			MFREE(wlc->osh, (void *)bcn, BCN_TMPL_LEN);
#ifdef STA
		wlc->mpc_out = mpc_out;
		wlc_radio_mpc_upd(wlc);

		wlc_set_pmstate(wlc, wlc->PMenabled);
		wlc_watchdog_upd(wlc, PS_ALLOWED(wlc));
#endif
	}
#endif /* AP */

#ifdef STA
	if (BSSCFG_STA(cfg)) {
		cfg->up = TRUE;
	}
#endif

	/* presence of multiple active bsscfgs changes defkeyvalid, so update on bsscfg up/down */
	if (wlc->pub->up) {
		uint16 defkeyvalid = wlc_key_defkeyflag(wlc);

		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, defkeyvalid, WLC_BAND_ALL);
		WL_WSEC(("wl%d: %s: updating MHF1_DEFKEYVALID to %d\n",
		         wlc->pub->unit, __FUNCTION__, defkeyvalid != 0));
	}

	return ret;
}

/* Enable: always try to force up */
int
wlc_bsscfg_enable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_enable %p currently %s\n",
	          wlc->pub->unit, bsscfg, (bsscfg->enable ? "ENABLED" : "DISABLED")));

	ASSERT(bsscfg != NULL);

	if (!MBSS_ENAB(wlc->pub)) {
		/* block simultaneous multiple AP connection */
		if (BSSCFG_AP(bsscfg) && AP_ACTIVE(wlc)) {
			WL_ERROR(("wl%d: Cannot enable multiple AP bsscfg\n", wlc->pub->unit));
			return BCME_ERROR;
		}

		/* block simultaneous IBSS and AP connection */
		if (BSSCFG_AP(bsscfg) && wlc->ibss_bsscfgs) {
			WL_ERROR(("wl%d: Cannot enable AP bsscfg with a IBSS\n", wlc->pub->unit));
			return BCME_ERROR;
		}
	}

	bsscfg->enable = TRUE;

	if (BSSCFG_AP(bsscfg)) {
#ifdef MBSS
		/* make sure we don't exceed max */
		if (MBSS_ENAB16(wlc->pub) &&
		    ((uint32)AP_BSS_UP_COUNT(wlc) >= wlc->max_ap_bss)) {
			bsscfg->enable = FALSE;
			WL_ERROR(("wl%d: max %d ap bss allowed\n",
			          wlc->pub->unit, wlc->max_ap_bss));
			return BCME_ERROR;
		}
#endif /* MBSS */

		return wlc_bsscfg_up(wlc, bsscfg);
	}

	/* wlc_bsscfg_up() will be called for STA assoication code:
	 * - for IBSS, in wlc_join_start_ibss() and in wlc_join_BSS()
	 * - for BSS, in wlc_assoc_complete()
	 */
	/*
	 * if (BSSCFG_STA(bsscfg)) {
	 *	return BCME_OK;
	 * }
	 */

	return BCME_OK;
}

int
wlc_bsscfg_down(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int callbacks = 0;

	ASSERT(cfg != NULL);

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_down %p currently %s %s; stas/aps/associated %d/%d/%d\n",
	          wlc->pub->unit, cfg, (cfg->up ? "UP" : "DOWN"), (BSSCFG_AP(cfg) ? "AP" : "STA"),
	          wlc->stas_associated, wlc->aps_associated, wlc->pub->associated));

	if (!cfg->up) {
		/* Are we in the process of an association? */
#ifdef STA
		if ((BSSCFG_STA(cfg) && cfg->assoc->state != AS_IDLE))
			wlc_assoc_abort(cfg);
#endif /* STA */
		return BCME_OK;
	}

#ifdef AP
	if (BSSCFG_AP(cfg)) {

		/* bring down this config */
		cfg->up = FALSE;

		callbacks += wlc_ap_down(wlc->ap, cfg);

#ifdef MBSS
		{
		uint i, clear_len = FALSE;
		wlc_bsscfg_t *bsscfg;
		uint8 ssidlen = cfg->SSID_len;

		wlc_spt_deinit(wlc, cfg->bcn_template, FALSE);

		if (cfg->probe_template != NULL) {
			PKTFREE(wlc->osh, cfg->probe_template, TRUE);
			cfg->probe_template = NULL;
		}

		/* If we clear ssid length of all bsscfgs while doing
		 * a wl down the ucode can get into a state where it 
		 * will keep searching  for non-zero ssid length thereby
		 * causing mac_suspend_and_wait messages. To avoid that
		 * we will prevent clearing the ssid len of at least one BSS
		 */
		FOREACH_BSS(wlc, i, bsscfg) {
			if (bsscfg->up) {
				clear_len = TRUE;
				break;
			}
		}
		if (clear_len) {
			cfg->SSID_len = 0;

			/* update uCode shared memory */
			wlc_shm_ssid_upd(wlc, cfg);
			cfg->SSID_len = ssidlen;

			/* Clear the HW index */
			wlc->hw2sw_idx[cfg->_ucidx] = WLC_BSSCFG_IDX_INVALID;
		}
		}
#endif /* MBSS */

#ifdef BCMAUTH_PSK
		if (cfg->authenticator != NULL)
			wlc_authenticator_down(cfg->authenticator);
#endif

		if (!AP_ACTIVE(wlc) && wlc->pub->up) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_mctrl(wlc, MCTL_AP, 0);
			wlc_enable_mac(wlc);
#ifdef STA
			if (APSTA_ENAB(wlc->pub)) {
				int idx;
				wlc_bsscfg_t *bc;
				FOREACH_AS_STA(wlc, idx, bc) {
					if (bc != wlc->cfg)
						continue;
					WL_APSTA_UPDN(("wl%d: wlc_bsscfg_down: last AP down,"
					               "sync STA: assoc_state %d type %d\n",
					               wlc->pub->unit, bc->assoc->state,
					               bc->assoc->type));
					if (bc->assoc->state == AS_IDLE) {
						ASSERT(bc->assoc->type == AS_IDLE);
						wlc_assoc_change_state(bc, AS_SYNC_RCV_BCN);
					}
				}
			}
#endif /* STA */
		}

#ifdef STA
		wlc_radio_mpc_upd(wlc);

		wlc_set_pmstate(wlc, wlc->PMenabled);
		wlc_watchdog_upd(wlc, PS_ALLOWED(wlc));
#endif

		if (WIN7_OS(wlc->pub) && cfg->bcn) {
			/* free beacon info */
			MFREE(wlc->osh, cfg->bcn, cfg->bcn_len);
			cfg->bcn = NULL;
			cfg->bcn_len = 0;
		}
	}
#endif /* AP */

#ifdef STA
	if (BSSCFG_STA(cfg)) {
#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
		if (cfg->sup != NULL)
			callbacks += wlc_sup_down(cfg->sup);
#endif
		/* abort any assocaitions or roams in progress */
		callbacks += wlc_assoc_abort(cfg);
		cfg->up = FALSE;
	}
#endif /* STA */

#ifdef WL_BSSCFG_TX_SUPR
	pktq_flush(wlc->osh, cfg->psq, TRUE, NULL, 0);
#endif

	/* presence of multiple active bsscfgs changes defkeyvalid, so update on bsscfg up/down */
	if (wlc->pub->up) {
		uint16 defkeyvalid = wlc_key_defkeyflag(wlc);

		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, defkeyvalid, WLC_BAND_ALL);
		WL_WSEC(("wl%d: %s: updating MHF1_DEFKEYVALID to %d\n",
		         wlc->pub->unit, __FUNCTION__, defkeyvalid != 0));
	}

	return callbacks;
}

int
wlc_bsscfg_disable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb_iter scbiter;
	int callbacks = 0;

	ASSERT(bsscfg != NULL);

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_disable %p currently %s\n",
	          wlc->pub->unit, bsscfg, (bsscfg->enable ? "ENABLED" : "DISABLED")));

	if (BSSCFG_AP(bsscfg)) {
		struct scb *scb;
		int assoc_scb_count = 0;

		/* WIN7 cleans up stas in wlc_ap_down */
		if (!WIN7_OS(wlc->pub)) {
			FOREACHSCB(wlc->scbstate, &scbiter, scb) {
				if (SCB_ASSOCIATED(scb) && scb->bsscfg == bsscfg) {
					wlc_scbfree(wlc, scb);
					assoc_scb_count++;
				}
			}

			/* send up a broadcast deauth mac event if there were any
			 * associated STAs
			 */
			if (assoc_scb_count)
				wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &ether_bcast,
					DOT11_RC_DEAUTH_LEAVING, 0);
		}
	}

	callbacks += wlc_bsscfg_down(wlc, bsscfg);
	ASSERT(!bsscfg->up);

#ifdef STA
	if (BSSCFG_STA(bsscfg)) {
		if (bsscfg->associated) {
			if (wlc->pub->up) {
				wlc_disassociate_client(bsscfg, TRUE, NULL, NULL);
			} else {
				wlc_sta_assoc_upd(bsscfg, FALSE);
			}
		}

		/* make sure we don't retry */
		if (bsscfg->assoc->timer != NULL) {
			if (!wl_del_timer(wlc->wl, bsscfg->assoc->timer))
				callbacks ++;
		}
	}
#endif /* STA */

	bsscfg->flags &= ~WLC_BSSCFG_PRESERVE;

	bsscfg->enable = FALSE;

	/* do a full cleanup of scbs if all configs disabled */
	if (wlc_bsscfg_ena_cnt(wlc) == 0)
		wlc_scbclear(wlc, FALSE);

	return callbacks;
}

#ifdef AP
/* Mark all but the primary cfg as disabled */
void
wlc_bsscfg_disablemulti(wlc_info_t *wlc)
{
	int i;
	wlc_bsscfg_t * bsscfg;

	/* iterate along the ssid cfgs */
	for (i = 1; i < WLC_MAXBSSCFG; i++)
		if ((bsscfg = WLC_BSSCFG(wlc, i)))
			wlc_bsscfg_disable(wlc, bsscfg);
}

int
wlc_bsscfg_ap_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_ap_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* Init flags: Beacons/probe resp in HW by default */
	bsscfg->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);

#if defined(MBSS)
	bsscfg->maxassoc = wlc->pub->tunables->maxscb;
	BCMC_FID_INIT(bsscfg);
	bsscfg->prb_ttl_us = WLC_PRB_TTL_us;
#endif

	bsscfg->wpa2_preauth = TRUE;

	bsscfg->_ap = TRUE;

#if defined(BCMAUTH_PSK)
	ASSERT(bsscfg->authenticator == NULL);
	if ((bsscfg->authenticator = wlc_authenticator_attach(wlc, bsscfg)) == NULL) {
		WL_ERROR(("wl%d: wlc_bsscfg_ap_init: wlc_authenticator_attach failed\n",
		          wlc->pub->unit));
		return BCME_ERROR;

	}
#endif	/* BCMAUTH_PSK */

	return BCME_OK;
}

static void
wlc_bsscfg_ap_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_ap_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

#if defined(BCMAUTH_PSK)
	/* free the authenticator */
	if (bsscfg->authenticator) {
		wlc_authenticator_detach(bsscfg->authenticator);
		bsscfg->authenticator = NULL;
	}
#endif /* BCMAUTH_PSK */

	_wlc_bsscfg_deinit(wlc, bsscfg);

	bsscfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
}
#endif /* AP */

#ifdef STA
int
wlc_bsscfg_sta_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_roam_t *roam = bsscfg->roam;
	wlc_assoc_t *as = bsscfg->assoc;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_sta_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* Init flags: Beacons/probe resp in HW by default (IBSS) */
	bsscfg->flags |= (WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);

	bsscfg->_ap = FALSE;

	/* init beacon timeouts */
	roam->bcn_timeout = WLC_BCN_TIMEOUT;

	/* init beacon-lost roaming thresholds */
	roam->uatbtt_tbtt_thresh = WLC_UATBTT_TBTT_THRESH;
	roam->tbtt_thresh = WLC_ROAM_TBTT_THRESH;

	/* roam scan inits */
	roam->scan_block = 0;
	roam->partialscan_period = WLC_ROAM_SCAN_PERIOD;
	roam->fullscan_period = WLC_FULLROAM_PERIOD;
	roam->ap_environment = AP_ENV_DETECT_NOT_USED;
	roam->motion_timeout = ROAM_MOTION_TIMEOUT;

	/* create association timer */
	if (!(as->timer =
	      wl_init_timer(wlc->wl, wlc_assoc_timeout, bsscfg, "assoc"))) {
		WL_ERROR(("wl%d: wl_init_timer for bsscfg %d assoc_timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return BCME_NORESOURCE;
	}

	as->recreate_bi_timeout = WLC_ASSOC_RECREATE_BI_TIMEOUT;
	as->listen = WLC_ADVERTISED_LISTEN;

	/* default AP disassoc/deauth timeout */
	as->verify_timeout = WLC_ASSOC_VERIFY_TIMEOUT;

	/* join preference */
	if ((bsscfg->join_pref = (wlc_join_pref_t *)
	     wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_join_pref_t))) == NULL)
		return BCME_NOMEM;

	/* init join pref */
	bsscfg->join_pref->band = WLC_BAND_AUTO;

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	ASSERT(bsscfg->sup == NULL);
	if ((bsscfg->sup = wlc_sup_attach(wlc, bsscfg)) == NULL) {
		WL_ERROR(("wl%d: wlc_bsscfg_sta_init: wlc_sup_attach failed\n", wlc->pub->unit));
		return BCME_ERROR;

	}
#endif	

	return BCME_OK;
}

static void
wlc_bsscfg_sta_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_sta_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* free the association timer */
	if (bsscfg->assoc->timer != NULL) {
		wl_free_timer(wlc->wl, bsscfg->assoc->timer);
		bsscfg->assoc->timer = NULL;
	}

	if (bsscfg->join_pref != NULL) {
		MFREE(wlc->osh, bsscfg->join_pref, sizeof(wlc_join_pref_t));
		bsscfg->join_pref = NULL;
	}

	_wlc_bsscfg_deinit(wlc, bsscfg);

	wlc_bsscfg_assoc_params_reset(wlc, bsscfg);

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	/* free the supplicant */
	if (bsscfg->sup != NULL) {
		wlc_sup_detach(bsscfg->sup);
		bsscfg->sup = NULL;
	}
#endif 

	bsscfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
}
#endif /* STA */

#ifdef WLBTAMP
int
wlc_bsscfg_bta_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_bta_init: bsscfg %p\n",
	          wlc->pub->unit, bsscfg));

	/* N.B.: QoS settings configured implicitly */
	bsscfg->flags |= WLC_BSSCFG_BTA;

	return wlc_bsscfg_init(wlc, bsscfg);
}
#endif /* WLBTAMP */

#if defined(WLP2P)
static int
wlc_bsscfg_bss_rsinit(wlc_info_t *wlc, wlc_bss_info_t *bi, uint8 rates, uint8 bw, bool allow_mcs)
{
	wlcband_t *band = wlc->band;
	wlc_rateset_t *rs = &bi->rateset;

	wlc_rateset_filter(&band->hw_rateset, rs, FALSE, rates, RATE_MASK_FULL, allow_mcs);
	if (rs->count == 0)
		return BCME_NORESOURCE;
#ifdef WL11N
	wlc_rateset_bw_mcs_filter(&bi->rateset, bw);
#endif
	wlc_rate_lookup_init(wlc, &bi->rateset);

	return BCME_OK;
}

static int
wlc_bsscfg_rateset_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint8 rates, uint8 bw, bool allow_mcs)
{
	int err;

	if ((err = wlc_bsscfg_bss_rsinit(wlc, cfg->target_bss, rates, bw, allow_mcs)) != BCME_OK)
		return err;
	if ((err = wlc_bsscfg_bss_rsinit(wlc, cfg->current_bss, rates, bw, allow_mcs)) != BCME_OK)
		return err;

	return err;
}
#endif 


#ifdef WLBDD
void
wlc_bsscfg_bdd_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	bsscfg->_ap = FALSE;

	/* share the same flag with DPT */
	bsscfg->flags |= WLC_BSSCFG_DPT;

	bsscfg->up = TRUE;
}
#endif /* WLBDD */

#ifdef WLP2P
int
wlc_bsscfg_p2p_init(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ret;

	cfg->flags |= WLC_BSSCFG_P2P;

	if ((ret = wlc_bsscfg_init(wlc, cfg)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: cannot init bsscfg\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	if ((ret = wlc_bsscfg_rateset_init(wlc, cfg, WLC_RATES_OFDM,
	                wlc->band->mimo_cap_40 ? CHSPEC_WLC_BW(wlc->home_chanspec) : 0,
	                BSS_N_ENAB(wlc, cfg))) != BCME_OK) {
		WL_ERROR(("wl%d: %s: failed rateset init\n", wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

	if (!BSS_P2P_DISC_ENAB(wlc, cfg)) {
		if (BSSCFG_STA(cfg) &&
		    (cfg->p2p = wlc_p2p_info_alloc(wlc->p2p, cfg)) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory for p2p info, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			ret = BCME_NOMEM;
			goto exit;
		}

		if ((ret = wlc_p2p_d11cb_alloc(wlc->p2p, cfg)) != BCME_OK) {
			WL_ERROR(("wl%d: %s: failed to alloc d11 shm control block\n",
			          wlc->pub->unit, __FUNCTION__));
			goto exit;
		}
	}
	else {
		bcopy(&cfg->cur_etheraddr, &cfg->BSSID, ETHER_ADDR_LEN);
		cfg->current_bss->chanspec = wlc->home_chanspec;
		cfg->current_bss->infra = 0;
	}

	if ((ret = wlc_p2p_d11ra_alloc(wlc->p2p, &cfg->rcmta_ra_idx)) != BCME_OK) {
		WL_ERROR(("wl%d: %s: failed to alloc rcmta entry\n",
		          wlc->pub->unit, __FUNCTION__));
		goto exit;
	}

exit:
	return ret;
}
#endif /* WLP2P */

int
wlc_bsscfg_get_free_idx(wlc_info_t *wlc)
{
	int idx;

	for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
		if (wlc->bsscfg[idx] == NULL)
			return idx;
	}

	return -1;
}

void
wlc_bsscfg_ID_assign(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	bsscfg->ID = wlc->next_bsscfg_ID;
	wlc->next_bsscfg_ID ++;
}

wlc_bsscfg_t *
wlc_bsscfg_alloc(wlc_info_t *wlc, int idx, uint flags, struct ether_addr *ea, bool ap)
{
	wlc_bsscfg_t *bsscfg;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_alloc: index %d flags 0x%08x ap %d\n",
	               wlc->pub->unit, idx, flags, ap));

	ASSERT((idx > 0 && idx < WLC_MAXBSSCFG));

	if ((bsscfg = wlc_bsscfg_malloc(wlc->osh, wlc->pub->unit)) == NULL) {
		WL_ERROR(("wl%d: wlc_bsscfg_alloc: out of memory for bsscfg, malloc failed\n",
		          wlc->pub->unit));
		return NULL;
	}
	wlc_bsscfg_ID_assign(wlc, bsscfg);

	wlc->bsscfg[idx] = bsscfg;
	bsscfg->_idx = (int8)idx;

	if (_wlc_bsscfg_init(wlc, bsscfg, ea != NULL ? ea : &wlc->pub->cur_etheraddr, flags, ap)) {
		WL_ERROR(("wl%d: wlc_bsscfg_alloc: _wlc_bsscfg_init() failed\n", wlc->pub->unit));
		wlc_bsscfg_free(wlc, bsscfg);
		bsscfg = NULL;
	}

	return bsscfg;
}

void
wlc_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm)
{
	struct scb_iter scbiter;
	struct scb *scb;
	int ii;

	if (wlc->scbstate == NULL)
		return;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->bsscfg != bsscfg)
			continue;
		if (scb->permanent) {
			if (!perm)
				continue;
			scb->permanent = FALSE;
		}
		wlc_scbfree(wlc, scb);
	}

	if (perm) {
		for (ii = 0; ii < MAXBANDS; ii++) {
			if (bsscfg->bcmc_scb[ii]) {
				WL_INFORM(("bcmc_scb: band %d: free internal scb for 0x%p\n",
					ii, bsscfg->bcmc_scb[ii]));
				wlc_internalscb_free(wlc, bsscfg->bcmc_scb[ii]);
				bsscfg->bcmc_scb[ii] = NULL;
			}
		}
	}
}

#if defined(AP) || defined(STA)
static void
_wlc_bsscfg_deinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int ii;
	int index;

	WL_APSTA_UPDN(("wl%d: _wlc_bsscfg_deinit: bsscfg %p\n", wlc->pub->unit, bsscfg));

	/* free all but bcmc scbs */
	wlc_bsscfg_scbclear(wlc, bsscfg, FALSE);

	/* process event queue */
	wlc_eventq_flush(wlc->eventq);

#if defined(AP) || defined(WLBDD)
	wlc_vndr_ie_free(bsscfg);
#endif


	/*
	 * If the index into the wsec_keys table is less than WSEC_MAX_DEFAULT_KEYS,
	 * the keys were allocated statically, and should not be deleted or removed.
	 */
	for (ii = 0; ii < WLC_DEFAULT_KEYS; ii++) {
		if (bsscfg->bss_def_keys[ii] &&
		    (bsscfg->bss_def_keys[ii]->idx >= WSEC_MAX_DEFAULT_KEYS)) {
			index = bsscfg->bss_def_keys[ii]->idx;
			wlc_key_delete(wlc, bsscfg, index);
		}
	}

#ifdef WLP2P
	/* free WiFi P2P info */
	if (P2P_ENAB(wlc->pub)) {
		if (BSS_P2P_ENAB(wlc, bsscfg)) {
			if (!BSS_P2P_DISC_ENAB(wlc, (bsscfg))) {
				if (BSSCFG_STA(bsscfg))
					wlc_p2p_d11cb_free(wlc->p2p, bsscfg);
				if (bsscfg->p2p != NULL) {
					wlc_p2p_info_free(wlc->p2p, bsscfg->p2p);
					bsscfg->p2p = NULL;
				}
			}
			wlc_p2p_d11ra_free(wlc->p2p, bsscfg->rcmta_ra_idx);
			bsscfg->rcmta_ra_idx = RCMTA_SIZE;
		}
	}
	else
#endif /* WLP2P */
	/* free RCMTA keys if keys allocated */
	if (bsscfg->rcmta)
		wlc_rcmta_del_bssid(wlc, bsscfg);
}
#endif /* AP || STA */

void
wlc_bsscfg_free(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int index;

	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_free: bsscfg %p\n", wlc->pub->unit, bsscfg));

	if (bsscfg->_ap) {
#ifdef AP
		wlc_bsscfg_ap_deinit(wlc, bsscfg);
#endif
	}
	else {
#ifdef STA
		wlc_bsscfg_sta_deinit(wlc, bsscfg);
#endif
	}

	if (WIN7_OS(wlc->pub) && bsscfg->bcn) {
		/* free AP beacon info in case wlc_bsscfg_disable() not called */
		/* This memory is typically freed in wlc_bsscfg_down(), which is */
		/* called by wlc_bsscfg_disable() */
		MFREE(wlc->pub->osh, bsscfg->bcn, bsscfg->bcn_len);
		bsscfg->bcn = NULL;
		bsscfg->bcn_len = 0;
	}

	/* free all scbs */
	wlc_bsscfg_scbclear(wlc, bsscfg, TRUE);

#ifdef WMF
	/* Delete WMF instance if it created for this bsscfg */
	if (WMF_ENAB(bsscfg)) {
		wlc_wmf_instance_del(bsscfg);
	}
#endif


#ifdef AP
	/* delete the upper-edge driver interface */
	if (bsscfg != wlc_bsscfg_primary(wlc) && bsscfg->wlcif->wlif != NULL) {
		wlc_if_event(wlc, WLC_E_IF_DEL, bsscfg->wlcif);
		wl_del_if(wlc->wl, bsscfg->wlcif->wlif);
		bsscfg->wlcif->wlif = NULL;
	}
#endif

#ifdef SMF_STATS
	wlc_bsscfg_smfsfree(wlc, bsscfg);
#endif

	/* free the wlc_bsscfg struct if it was an allocated one */
	index = bsscfg->_idx;
	if (bsscfg != wlc_bsscfg_primary(wlc)) {
		wlc_bsscfg_mfree(wlc->osh, bsscfg);
	}
	wlc->bsscfg[index] = NULL;

	/* update txcache since bsscfg going away may change settings */
	if (WLC_TXC_ENAB(wlc))
		wlc_txc_upd(wlc);
}

int
wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_init: bsscfg %p\n", wlc->pub->unit, bsscfg));

#if defined(AP) && defined(STA)
	if (bsscfg->_ap)
		return wlc_bsscfg_ap_init(wlc, bsscfg);
	return wlc_bsscfg_sta_init(wlc, bsscfg);
#elif defined(AP)
	return wlc_bsscfg_ap_init(wlc, bsscfg);
#elif defined(STA)
	return wlc_bsscfg_sta_init(wlc, bsscfg);
#else
	return BCME_OK;
#endif
}

int
wlc_bsscfg_reinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool ap)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_reinit: bsscfg %p ap %d\n", wlc->pub->unit, bsscfg, ap));

	if (bsscfg->_ap == ap)
		return BCME_OK;

#if defined(AP) && defined(STA)
	if (ap) {
		wlc_bsscfg_sta_deinit(wlc, bsscfg);
		return wlc_bsscfg_ap_init(wlc, bsscfg);
	}
	wlc_bsscfg_ap_deinit(wlc, bsscfg);
	return wlc_bsscfg_sta_init(wlc, bsscfg);
#else
	return BCME_OK;
#endif
}

#ifdef WLBTAMP
int
wlc_bsscfg_reinit_ext(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool ap)
{
	WL_APSTA_UPDN(("wl%d: wlc_bsscfg_reinit_ext: bsscfg %p ap %d\n",
	               wlc->pub->unit, bsscfg, ap));

	if (bsscfg->_ap)
		wlc_bsscfg_ap_deinit(wlc, bsscfg);
	else
		wlc_bsscfg_sta_deinit(wlc, bsscfg);
	if (ap)
		return wlc_bsscfg_ap_init(wlc, bsscfg);
	return wlc_bsscfg_sta_init(wlc, bsscfg);
}
#endif /* WLBTAMP */

/* Get a bsscfg pointer, failing if the bsscfg does not alreay exist.
 * Sets the bsscfg pointer in any event.
 * Returns BCME_RANGE if the index is out of range or BCME_NOTFOUND
 * if the wlc->bsscfg[i] pointer is null
 */
wlc_bsscfg_t *
wlc_bsscfg_find(wlc_info_t *wlc, int idx, int *perr)
{
	wlc_bsscfg_t *bsscfg;

	if ((idx < 0) || (idx >= WLC_MAXBSSCFG)) {
		*perr = BCME_RANGE;
		return NULL;
	}

	bsscfg = wlc->bsscfg[idx];
	*perr = bsscfg ? 0 : BCME_NOTFOUND;

	return bsscfg;
}

wlc_bsscfg_t *
wlc_bsscfg_primary(wlc_info_t *wlc)
{
	return wlc->cfg;
}

void
BCMATTACHFN(wlc_bsscfg_primary_cleanup)(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_primary(wlc);

	wlc_bsscfg_scbclear(wlc, bsscfg, TRUE);
}

int
BCMATTACHFN(wlc_bsscfg_primary_init)(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg = wlc_bsscfg_primary(wlc);

	wlc->bsscfg[0] = bsscfg;
	bsscfg->_idx = 0;

	if (_wlc_bsscfg_init(wlc, bsscfg, &wlc->pub->cur_etheraddr, 0, wlc->pub->_ap)) {
		WL_ERROR(("wl%d: wlc_bsscfg_primary_init: _wlc_bsscfg_init() failed\n",
		          wlc->pub->unit));
		wlc_bsscfg_free(wlc, bsscfg);
		return BCME_ERROR;
	}

	return wlc_bsscfg_init(wlc, bsscfg);
}

/*
 * Find a bsscfg from matching cur_etheraddr, BSSID, SSID, or something unique.
 */

/* match wlcif */
wlc_bsscfg_t *
wlc_bsscfg_find_by_wlcif(wlc_info_t *wlc, wlc_if_t *wlcif)
{
	/* wlcif being NULL implies primary interface hence primary bsscfg */
	if (wlcif == NULL)
		return wlc_bsscfg_primary(wlc);

	switch (wlcif->type) {
	case WLC_IFTYPE_BSS:
		return wlcif->u.bsscfg;
#ifdef AP
	case WLC_IFTYPE_WDS:
		return SCB_BSSCFG(wlcif->u.scb);
#endif
	}

	WL_ERROR(("wl%d: Unknown wlcif %p type %d\n", wlc->pub->unit, wlcif, wlcif->type));
	return NULL;
}

/* match cur_etheraddr */
wlc_bsscfg_t * BCMFASTPATH
wlc_bsscfg_find_by_hwaddr(wlc_info_t *wlc, struct ether_addr *hwaddr)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	if (ETHER_ISNULLADDR(hwaddr) || ETHER_ISMULTI(hwaddr))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bcmp(hwaddr->octet, bsscfg->cur_etheraddr.octet, ETHER_ADDR_LEN) == 0)
			return bsscfg;
	}

	return NULL;
}

/* match BSSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_bssid(wlc_info_t *wlc, struct ether_addr *bssid)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bcmp(bssid->octet, bsscfg->BSSID.octet, ETHER_ADDR_LEN) == 0)
			return bsscfg;
	}

	return NULL;
}

/* match target_BSSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_target_bssid(wlc_info_t *wlc, struct ether_addr *bssid)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	if (ETHER_ISNULLADDR(bssid) || ETHER_ISMULTI(bssid))
		return NULL;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (!BSSCFG_STA(bsscfg))
			continue;
		if (bcmp(bssid->octet, bsscfg->target_bss->BSSID.octet, ETHER_ADDR_LEN) == 0)
			return bsscfg;
	}

	return NULL;
}

/* match SSID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_ssid(wlc_info_t *wlc, uint8 *ssid, int ssid_len)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (ssid_len > 0 &&
		    ssid_len == bsscfg->SSID_len && bcmp(ssid, bsscfg->SSID, ssid_len) == 0)
			return bsscfg;
	}

	return NULL;
}

/* match ID */
wlc_bsscfg_t *
wlc_bsscfg_find_by_ID(wlc_info_t *wlc, uint16 id)
{
	int i;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, i, bsscfg) {
		if (bsscfg->ID == id)
			return bsscfg;
	}

	return NULL;
}

static void
wlc_bsscfg_bss_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_bss_info_t * bi = wlc->default_bss;

	bcopy((char*)bi, (char*)bsscfg->target_bss, sizeof(wlc_bss_info_t));
	bcopy((char*)bi, (char*)bsscfg->current_bss, sizeof(wlc_bss_info_t));
}

static int
_wlc_bsscfg_init(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *ea, uint flags, bool ap)
{
	ASSERT(bsscfg != NULL);
	ASSERT(ea != NULL);

	bsscfg->wlc = wlc;

	bsscfg->flags = flags;
	bsscfg->_ap = ap;

	bcopy(ea, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);

#ifdef WLP2P
	/* by default they don't require RCMTA entries */
	bsscfg->rcmta_ra_idx = RCMTA_SIZE;
	bsscfg->rcmta_bssid_idx = RCMTA_SIZE;
#endif

#ifdef WL_BSSCFG_TX_SUPR
	ASSERT(bsscfg->psq != NULL);
	pktq_init(bsscfg->psq, WLC_PREC_COUNT, PKTQ_LEN_DEFAULT);
#endif

	bsscfg->BSS = TRUE;	/* set the mode to INFRA */

	/* initialize security state */
	bsscfg->wsec_index = -1;
	bsscfg->wsec = 0;

	/* Match Wi-Fi default of true for aExcludeUnencrypted,
	 * instead of 802.11 default of false.
	 */
	bsscfg->wsec_restrict = TRUE;

	/* disable 802.1X authentication by default */
	bsscfg->eap_restrict = FALSE;

#ifdef BCMWAPI_WAI
	/* disable WAI authentication by default */
	bsscfg->wai_restrict = FALSE;
#endif /* BCMWAPI_WAI */

	/* disable WPA by default */
	bsscfg->WPA_auth = WPA_AUTH_DISABLED;

	/* Allocate a broadcast SCB for each band */
	if (!(bsscfg->flags & WLC_BSSCFG_NOBCMC)) {
		if (!IS_SINGLEBAND_5G(wlc->deviceid)) {
			if (wlc_bsscfg_bcmcscbinit(wlc, bsscfg, BAND_2G_INDEX))
				return BCME_NOMEM;
		}

		if (NBANDS(wlc) > 1 || IS_SINGLEBAND_5G(wlc->deviceid)) {
			if (wlc_bsscfg_bcmcscbinit(wlc, bsscfg, BAND_5G_INDEX))
				return BCME_NOMEM;
		}
	}

#ifdef SMF_STATS
	if (wlc_bsscfg_smfsinit(wlc, bsscfg))
		return BCME_NOMEM;
#endif

	/* create a new upper-edge driver interface */
	bsscfg->wlcif->type = WLC_IFTYPE_BSS;
	bsscfg->wlcif->flags = 0;
	bsscfg->wlcif->u.bsscfg = bsscfg;

#ifdef AP
	/* create an OS interface */
	if (bsscfg != wlc_bsscfg_primary(wlc) && !BSSCFG_HAS_NOIF(bsscfg)) {
		uint idx = WLC_BSSCFG_IDX(bsscfg);
		bsscfg->wlcif->wlif = wl_add_if(wlc->wl, bsscfg->wlcif, idx, NULL);
		if (bsscfg->wlcif->wlif == NULL) {
			WL_ERROR(("wl%d: _wlc_bsscfg_init: wl_add_if failed for index %d\n",
			          wlc->pub->unit, idx));
			return BCME_ERROR;
		}
		wlc_if_event(wlc, WLC_E_IF_ADD, bsscfg->wlcif);
	}
#endif

	wlc_bsscfg_bss_init(wlc, bsscfg);

	return BCME_OK;
}

static int
wlc_bsscfg_bcmcscbinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint band)
{
	ASSERT(bsscfg != NULL);
	ASSERT(wlc != NULL);

	if (!bsscfg->bcmc_scb[band]) {
		bsscfg->bcmc_scb[band] =
		        wlc_internalscb_alloc(wlc, &ether_bcast, wlc->bandstate[band]);
		WL_INFORM(("wl%d: wlc_bsscfg_bcmcscbinit: band %d: alloc internal scb 0x%p "
		           "for bsscfg 0x%p\n",
		           wlc->pub->unit, band, bsscfg->bcmc_scb[band], bsscfg));
	}
	if (!bsscfg->bcmc_scb[band]) {
		WL_ERROR(("wl%d: wlc_bsscfg_bcmcscbinit: fail to alloc scb for bsscfg 0x%p\n",
		          wlc->pub->unit, bsscfg));
		return BCME_NOMEM;
	}

	/* make this scb point to bsscfg */
	bsscfg->bcmc_scb[band]->bsscfg = bsscfg;
	bsscfg->bcmc_scb[band]->bandunit = band;

	return  0;
}

#ifdef MBSS
/* Write the base MAC/BSSID into shared memory.  For MBSS, the MAC and BSSID
 * are required to be the same.
 */
int
wlc_write_mbss_basemac(wlc_info_t *wlc, const struct ether_addr *addr)
{
	uint16 mac_l;
	uint16 mac_m;
	uint16 mac_h;

	mac_l = addr->octet[0] | (addr->octet[1] << 8);
	mac_m = addr->octet[2] | (addr->octet[3] << 8);
	/* Mask low bits of BSSID base */
	mac_h = addr->octet[4] | ((addr->octet[5] & ~(wlc->mbss_ucidx_mask)) << 8);

	wlc_write_shm(wlc, SHM_MBSS_BSSID0, mac_l);
	wlc_write_shm(wlc, SHM_MBSS_BSSID1, mac_m);
	wlc_write_shm(wlc, SHM_MBSS_BSSID2, mac_h);

	return BCME_OK;
}

/* Generate a MAC address for the MBSS AP BSS config */
static int
wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ii, jj;
	bool collision = TRUE;
	int cfg_idx = WLC_BSSCFG_IDX(cfg);
	struct ether_addr newmac;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	if (ETHER_ISNULLADDR(&wlc->vether_base)) {
		/* initialize virtual MAC base for MBSS
		 * the base should come from an external source,
		 * this initialization is in case one isn't provided
		 */
		bcopy(&wlc->pub->cur_etheraddr, &wlc->vether_base, ETHER_ADDR_LEN);
		/* avoid collision */
		wlc->vether_base.octet[5] += 1;

		/* force locally administered address */
		ETHER_SET_LOCALADDR(&wlc->vether_base);
	}

	bcopy(&wlc->vether_base, &newmac, ETHER_ADDR_LEN);

	/* brute force attempt to make a MAC for this interface,
	 * the user didn't provide one.
	 * outside loop limits the # of times we increment the low byte of
	 * the MAC address we're attempting to create, and the inner loop
	 * checks for collisions with other configs.
	 */
	for (ii = 0; (ii < WLC_MAXBSSCFG) && (collision == TRUE); ii++) {
		collision = FALSE;
		for (jj = 0; jj < WLC_MAXBSSCFG; jj++) {
			/* don't compare with the bss config we're updating */
			if (jj == cfg_idx || (!wlc->bsscfg[jj]))
				continue;
			if (EADDR_TO_UC_IDX(wlc->bsscfg[jj]->cur_etheraddr, wlc->mbss_ucidx_mask) ==
			    EADDR_TO_UC_IDX(newmac, wlc->mbss_ucidx_mask)) {
				collision = TRUE;
				break;
			}
		}
		if (collision == TRUE) /* increment and try again */
			newmac.octet[5] = (newmac.octet[5] & ~(wlc->mbss_ucidx_mask))
			        | (wlc->mbss_ucidx_mask & (newmac.octet[5]+1));
		else
			bcopy(&newmac, &cfg->cur_etheraddr, ETHER_ADDR_LEN);
	}

	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr)) {
		WL_MBSS(("wl%d.%d: wlc_bsscfg_macgen couldn't generate MAC address\n",
		         wlc->pub->unit, cfg_idx));

		return BCME_BADADDR;
	}
	else {
		WL_MBSS(("wl%d.%d: wlc_bsscfg_macgen assigned MAC %s\n",
		         wlc->pub->unit, cfg_idx,
		         bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));
		return BCME_OK;
	}
}
#endif /* MBSS */

#if defined(AP)
uint16
wlc_bsscfg_newaid(wlc_bsscfg_t *cfg)
{
	int pos;

	ASSERT(cfg);

	/* get an unused number from aidmap */
	for (pos = 0; pos < cfg->wlc->pub->tunables->maxscb; pos++) {
		if (isclr(cfg->aidmap, pos)) {
			WL_ASSOC(("wlc_bsscfg_newaid marking bit = %d for "
			          "bsscfg %d AIDMAP\n", pos,
			          WLC_BSSCFG_IDX(cfg)));
			/* mark the position being used */
			setbit(cfg->aidmap, pos);
			break;
		}
	}
	ASSERT(pos < cfg->wlc->pub->tunables->maxscb);

	return ((uint16)AIDMAP2AID(pos));
}
#endif /* AP */

#ifdef STA
/* Set/reset association parameters */
int
wlc_bsscfg_assoc_params_set(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	wl_assoc_params_t *assoc_params, int assoc_params_len)
{
	ASSERT(wlc != NULL);
	ASSERT(bsscfg != NULL);

	if (bsscfg->assoc_params != NULL) {
		MFREE(wlc->osh, bsscfg->assoc_params, bsscfg->assoc_params_len);
		bsscfg->assoc_params = NULL;
		bsscfg->assoc_params_len = 0;
	}
	if (assoc_params == NULL || assoc_params_len == 0)
		return BCME_OK;
	if ((bsscfg->assoc_params = MALLOC(wlc->osh, assoc_params_len)) == NULL) {
		WL_ERROR(("wl%d: wlc_bsscfg_assoc_params_set: out of memory for bsscfg, "
		          "malloced %d bytes\n", wlc->pub->unit, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bcopy(assoc_params, bsscfg->assoc_params, assoc_params_len);
	bsscfg->assoc_params_len = (uint16)assoc_params_len;

	return BCME_OK;
}

void
wlc_bsscfg_assoc_params_reset(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	if (bsscfg != NULL)
		wlc_bsscfg_assoc_params_set(wlc, bsscfg, NULL, 0);
}
#endif /* STA */

void
wlc_bsscfg_SSID_set(wlc_bsscfg_t *bsscfg, uint8 *SSID, int len)
{
	ASSERT(bsscfg != NULL);
	ASSERT(len <= DOT11_MAX_SSID_LEN);

	if ((bsscfg->SSID_len = (uint8)len) > 0) {
		ASSERT(SSID != NULL);
		/* need to use memove here to handle overlapping copy */
		memmove(bsscfg->SSID, SSID, len);

		if (len < DOT11_MAX_SSID_LEN)
			bzero(&bsscfg->SSID[len], DOT11_MAX_SSID_LEN - len);
		return;
	}

	bzero(bsscfg->SSID, DOT11_MAX_SSID_LEN);
}

/*
 * Vendor IE lists
 */

#if defined(AP) || defined(WLBDD)
void
wlc_vndr_ie_free(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc;
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_listel_t *next_list_el;
	int freelen;

	wlc = bsscfg->wlc;
	curr_list_el = bsscfg->vndr_ie_listp;

	while (curr_list_el != NULL) {
		next_list_el = curr_list_el->next_el;

		freelen =
			VNDR_IE_EL_HDR_LEN +
			sizeof(uint32) +
			VNDR_IE_HDR_LEN +
			curr_list_el->vndr_ie_infoel.vndr_ie_data.len;

		MFREE(wlc->osh, curr_list_el, freelen);
		curr_list_el = next_list_el;
	}

	bsscfg->vndr_ie_listp = NULL;
}
#endif 

static int
wlc_vndr_ie_verify_buflen(vndr_ie_buf_t *ie_buf, int len, int *bcn_ielen, int *prbrsp_ielen)
{
	int totie, ieindex;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int ie_len, info_len;
	char *bufaddr;
	uint32 pktflag;
	int bcn_len, prbrsp_len;

	if (len < (int) sizeof(vndr_ie_buf_t)) {
		return BCME_BUFTOOSHORT;
	}

	bcn_len = prbrsp_len = 0;

	bcopy(&ie_buf->iecount, &totie, (int) sizeof(int));

	bufaddr = (char *) &ie_buf->vndr_ie_list;
	len -= (int) sizeof(int);       /* reduce by the size of iecount */

	for (ieindex = 0; ieindex < totie; ieindex++) {
		if (len < (int) sizeof(vndr_ie_info_t)) {
			return BCME_BUFTOOSHORT;
		}

		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy(&ie_info->pktflag, &pktflag, (int) sizeof(uint32));

		vndr_iep = &ie_info->vndr_ie_data;
		ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
		info_len = (int) sizeof(uint32) + ie_len;

		if (pktflag & VNDR_IE_BEACON_FLAG) {
			bcn_len += ie_len;
		}

		if (pktflag & VNDR_IE_PRBRSP_FLAG) {
			prbrsp_len += ie_len;
		}

		/* reduce the bufer length by the size of this vndr_ie_info */
		len -= info_len;

		/* point to the next vndr_ie_info */
		bufaddr += info_len;
	}

	if (len < 0) {
		return BCME_BUFTOOSHORT;
	}

	if (bcn_ielen) {
		*bcn_ielen = bcn_len;
	}

	if (prbrsp_ielen) {
		*prbrsp_ielen = prbrsp_len;
	}

	return 0;
}

int
wlc_vndr_ie_getlen(wlc_bsscfg_t *bsscfg, uint32 pktflag, int *totie)
{
	vndr_ie_listel_t *curr;
	int ie_count = 0;
	int tot_ielen = 0;

	for (curr = bsscfg->vndr_ie_listp; curr != NULL; curr = curr->next_el) {
		if (curr->vndr_ie_infoel.pktflag & pktflag) {
			ie_count++;
			tot_ielen += curr->vndr_ie_infoel.vndr_ie_data.len + VNDR_IE_HDR_LEN;
		}
	}

	if (totie) {
		*totie = ie_count;
	}

	return tot_ielen;
}

uint8 *
wlc_vndr_ie_write_ext(wlc_bsscfg_t *bsscfg, vndr_ie_write_filter_fn_t filter,
	uint type, uint8 *cp, int buflen, uint32 pktflag)
{
	vndr_ie_listel_t *curr;

	for (curr = bsscfg->vndr_ie_listp; curr != NULL; curr = curr->next_el) {
		if (curr->vndr_ie_infoel.pktflag & pktflag) {
			if (curr->vndr_ie_infoel.pktflag & VNDR_IE_CUSTOM_FLAG) {
				cp = wlc_write_info_elt_safe(cp, buflen,
					curr->vndr_ie_infoel.vndr_ie_data.id,
					curr->vndr_ie_infoel.vndr_ie_data.len,
					&curr->vndr_ie_infoel.vndr_ie_data.oui[0]);
			}
			else if (filter == NULL ||
			         (filter)(bsscfg, type, &curr->vndr_ie_infoel.vndr_ie_data)) {
				cp = wlc_write_info_elt_safe(cp, buflen,
					DOT11_MNG_PROPR_ID,
					curr->vndr_ie_infoel.vndr_ie_data.len,
					&curr->vndr_ie_infoel.vndr_ie_data.oui[0]);
			}
		}
	}

	return cp;
}

#ifdef WLP2P
static bool
wlc_p2p_vndr_ie_write_filter(wlc_bsscfg_t *cfg, uint type, vndr_ie_t *ie)
{
	uint8 *parse;
	uint parse_len;

	if (type != FC_PROBE_RESP ||
	    (cfg->flags & WLC_BSSCFG_P2P_IE) ||
	    !bcm_is_p2p_ie((uint8 *)ie, &parse, &parse_len))
		return TRUE;

	return FALSE;
}

uint8 *
wlc_p2p_vndr_ie_write(wlc_bsscfg_t *bsscfg, uint type, uint8 *cp, int buflen, uint32 pktflag)
{
	return wlc_vndr_ie_write_ext(bsscfg, wlc_p2p_vndr_ie_write_filter, type,
	                             cp, buflen, pktflag);
}
#endif /* WLP2P */

/*
 * Create a vendor IE information element object and add to the list.
 * Return value: address of the new object.
 */
vndr_ie_listel_t *
wlc_vndr_ie_add_elem(wlc_bsscfg_t *bsscfg, uint32 pktflag, vndr_ie_t *vndr_iep)
{
	wlc_info_t *wlc;
	vndr_ie_listel_t *new_list_el;
	int info_len, ie_len;
	vndr_ie_listel_t *last;

	wlc = bsscfg->wlc;
	ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
	info_len = (int) (VNDR_IE_INFO_HDR_LEN + ie_len);

	if ((new_list_el = (vndr_ie_listel_t *) MALLOC(wlc->osh, info_len +
		VNDR_IE_EL_HDR_LEN)) == NULL) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add_elem: out of memory\n", wlc->pub->unit));
		return NULL;
	}

	new_list_el->vndr_ie_infoel.pktflag = pktflag;
	bcopy((char *)vndr_iep, (char *)&new_list_el->vndr_ie_infoel.vndr_ie_data, ie_len);

	/* Add to the tail of the list */
	for (last = bsscfg->vndr_ie_listp;
	     last != NULL && last->next_el != NULL;
	     last = last->next_el)
		;
	new_list_el->next_el = NULL;
	if (last != NULL)
		last->next_el = new_list_el;
	else
		bsscfg->vndr_ie_listp = new_list_el;

	return (new_list_el);
}

int
wlc_vndr_ie_add(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len)
{
	wlc_info_t *wlc;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int info_len;
	int totie, ieindex;
	int bcn_ielen, prbrsp_ielen;
	uint32 pktflag, currflag;
	char *bufaddr;

	wlc = bsscfg->wlc;
	pktflag = 0;

	if (wlc_vndr_ie_verify_buflen(ie_buf, len, &bcn_ielen, &prbrsp_ielen) != 0) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add: buffer too small, length = %d\n", wlc->pub->unit,
			len));
		return BCME_BUFTOOSHORT;
	}

	if (bcn_ielen > VNDR_IE_MAX_TOTLEN) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add: bcn IE length %d exceeds max template length"
			" %d\n",
			wlc->pub->unit, bcn_ielen, VNDR_IE_MAX_TOTLEN));
		return BCME_BUFTOOLONG;
	}

	if (prbrsp_ielen > VNDR_IE_MAX_TOTLEN) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add: prb rsp IE length %d exceed max template length"
			" %d\n",
			wlc->pub->unit, prbrsp_ielen, VNDR_IE_MAX_TOTLEN));
		return BCME_BUFTOOLONG;
	}

	bcopy(&ie_buf->iecount, &totie, sizeof(int));
	bufaddr = (char *) &ie_buf->vndr_ie_list;

	for (ieindex = 0; ieindex < totie; ieindex++) {
		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy((char *)&ie_info->pktflag, (char *)&currflag, (int) sizeof(uint32));
		pktflag |= currflag;

		vndr_iep = &ie_info->vndr_ie_data;

		if (!(ie_info->pktflag & VNDR_IE_CUSTOM_FLAG))
			vndr_iep->id = DOT11_MNG_PROPR_ID;

		info_len = (int) (VNDR_IE_INFO_HDR_LEN + VNDR_IE_HDR_LEN + vndr_iep->len);
		if (wlc_vndr_ie_add_elem(bsscfg, currflag, vndr_iep) == NULL) {
			return BCME_NORESOURCE;
		}
		/* point to the next vndr_ie_info */
		bufaddr += info_len;
	}

	return 0;
}

int
wlc_vndr_ie_del(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len)
{
	wlc_info_t *wlc;
	vndr_ie_listel_t *prev_list_el;
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_listel_t *next_list_el;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	int ie_len, info_len;
	int totie, ieindex;
	uint32 pktflag, currflag;
	char *bufaddr;
	bool found;
	int err = 0;

	wlc = bsscfg->wlc;
	pktflag = 0;

	if (wlc_vndr_ie_verify_buflen(ie_buf, len, NULL, NULL) != 0) {
		WL_ERROR(("wl%d: wlc_vndr_ie_add: buffer too small, length = %d\n", wlc->pub->unit,
			len));
		return BCME_BUFTOOSHORT;
	}

	bcopy(&ie_buf->iecount, &totie, sizeof(int));
	bufaddr = (char *) &ie_buf->vndr_ie_list;

	for (ieindex = 0; ieindex < totie; ieindex++) {
		ie_info = (vndr_ie_info_t *) bufaddr;
		bcopy((char *)&ie_info->pktflag, (char *)&currflag, (int) sizeof(uint32));
		pktflag |= currflag;

		vndr_iep = &ie_info->vndr_ie_data;
		if (!(ie_info->pktflag & VNDR_IE_CUSTOM_FLAG))
			vndr_iep->id = DOT11_MNG_PROPR_ID;

		ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
		info_len = (int) sizeof(uint32) + ie_len;

		curr_list_el = bsscfg->vndr_ie_listp;
		prev_list_el = NULL;

		found = FALSE;

		while (curr_list_el != NULL) {
			next_list_el = curr_list_el->next_el;

			if (vndr_iep->len == curr_list_el->vndr_ie_infoel.vndr_ie_data.len) {
				if (!bcmp((char*)&curr_list_el->vndr_ie_infoel, (char*) ie_info,
					info_len)) {
					if (bsscfg->vndr_ie_listp == curr_list_el) {
						bsscfg->vndr_ie_listp = next_list_el;
					} else {
						prev_list_el->next_el = next_list_el;
					}
					MFREE(wlc->osh, curr_list_el, info_len +
					      VNDR_IE_EL_HDR_LEN);
					curr_list_el = NULL;
					found = TRUE;
					break;
				}
			}

			prev_list_el = curr_list_el;
			curr_list_el = next_list_el;
		}

		if (! found) {
			WL_ERROR(("wl%d: wlc_del_ie: IE not in list\n", wlc->pub->unit));
			err = BCME_NOTFOUND;
		}

		/* point to the next vndr_ie_info */
		bufaddr += info_len;
	}

	return err;
}

int
wlc_vndr_ie_get(wlc_bsscfg_t *bsscfg, vndr_ie_buf_t *ie_buf, int len)
{
	wlc_info_t *wlc;
	int copylen;
	int totie;
	vndr_ie_listel_t *curr_list_el;
	vndr_ie_info_t *ie_info;
	vndr_ie_t *vndr_iep;
	char *bufaddr;
	int ie_len, info_len;

	wlc = bsscfg->wlc;
	/* Vendor IE data */
	copylen = wlc_vndr_ie_getlen(bsscfg,
		VNDR_IE_BEACON_FLAG |
		VNDR_IE_PRBRSP_FLAG |
		VNDR_IE_ASSOCRSP_FLAG |
		VNDR_IE_AUTHRSP_FLAG|
		VNDR_IE_PRBREQ_FLAG |
		VNDR_IE_ASSOCREQ_FLAG,
		&totie);

	if (totie != 0) {
		/* iecount */
		copylen += (int) sizeof(int);

		/* pktflag for each vndr_ie_info struct */
		copylen += (int) sizeof(uint32) * totie;
	} else {
		copylen = (int) sizeof(vndr_ie_buf_t);
	}
	if (len < copylen) {
		WL_ERROR(("wl%d: wlc_vndr_ie_get: buf too small (copylen=%d, buflen=%d)\n",
			wlc->pub->unit, copylen, len));
		/* Store the required buffer size value in the buffer provided */
		bcopy((char *) &copylen, (char *)ie_buf, sizeof(int));
		return BCME_BUFTOOSHORT;
	}

	bcopy(&totie, &ie_buf->iecount, sizeof(int));
	bufaddr = (char *) &ie_buf->vndr_ie_list;

	curr_list_el = bsscfg->vndr_ie_listp;

	while (curr_list_el != NULL) {
		ie_info = (vndr_ie_info_t *) bufaddr;
		vndr_iep = &curr_list_el->vndr_ie_infoel.vndr_ie_data;

		ie_len = (int) (vndr_iep->len + VNDR_IE_HDR_LEN);
		info_len = (int) sizeof(uint32) + ie_len;

		bcopy((char*)&curr_list_el->vndr_ie_infoel, (char*) ie_info, info_len);

		curr_list_el = curr_list_el->next_el;

		/* point to the next vndr_ie_info */
		bufaddr += info_len;
	}

	return 0;
}

/*
 * Modify the data in the previously added vendor IE info.
 */
vndr_ie_listel_t *
wlc_vndr_ie_mod_elem(wlc_bsscfg_t *bsscfg, vndr_ie_listel_t *old_listel,
	uint32 pktflag, vndr_ie_t *vndr_iep)
{
	wlc_info_t *wlc;
	vndr_ie_listel_t *curr_list_el, *prev_list_el;

	wlc = bsscfg->wlc;
	curr_list_el = bsscfg->vndr_ie_listp;
	prev_list_el = NULL;

	while (curr_list_el != NULL) {
		/* found list element, update the vendor info */
		if (curr_list_el == old_listel) {
			/* reuse buffer if length of current elem is same as the new */
			if (curr_list_el->vndr_ie_infoel.vndr_ie_data.len == vndr_iep->len) {
				curr_list_el->vndr_ie_infoel.pktflag = pktflag;
				bcopy((char *)vndr_iep,
				      (char *)&curr_list_el->vndr_ie_infoel.vndr_ie_data,
				      (vndr_iep->len + VNDR_IE_HDR_LEN));

				return (curr_list_el);
			} else {
				/* Delete the old one from the list and free it */
				if (bsscfg->vndr_ie_listp == curr_list_el) {
					bsscfg->vndr_ie_listp = curr_list_el->next_el;
				} else {
					prev_list_el->next_el = curr_list_el->next_el;
				}
				MFREE(wlc->osh, curr_list_el,
				      (curr_list_el->vndr_ie_infoel.vndr_ie_data.len +
				       VNDR_IE_HDR_LEN + VNDR_IE_INFO_HDR_LEN +
				       VNDR_IE_EL_HDR_LEN));

				/* Add a new elem to the list */
				return wlc_vndr_ie_add_elem(bsscfg, pktflag, vndr_iep);
			}
		}

		prev_list_el = curr_list_el;
		curr_list_el = curr_list_el->next_el;
	}

	/* Should not come here */
	ASSERT(0);

	return 0;
}

#ifdef SMF_STATS
int
wlc_bsscfg_smfsinit(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	uint8 i;
	wlc_smf_stats_t *smf_stats;

	if (bsscfg->smfs_info)
		return 0;

	bsscfg->smfs_info = MALLOC(wlc->osh, sizeof(wlc_smfs_info_t));
	if (!bsscfg->smfs_info) {
		WL_ERROR(("wl%d: %s out of memory for bsscfg, "
		   "malloced %d bytes\n", wlc->pub->unit, __FUNCTION__,
		   MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bzero(bsscfg->smfs_info, sizeof(wlc_smfs_info_t));

	bsscfg->smfs_info->enable = 1;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		smf_stats = &bsscfg->smfs_info->smf_stats[i];

		smf_stats->smfs_main.type = i;
		smf_stats->smfs_main.version = SMFS_VERSION;

		if ((i == SMFS_TYPE_AUTH) || (i == SMFS_TYPE_ASSOC) ||
			(i == SMFS_TYPE_REASSOC))
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_SC;
		else
			smf_stats->smfs_main.codetype = SMFS_CODETYPE_RC;
	}
	return 0;
}

static int
smfs_elem_free(struct wlc_info *wlc, wlc_smf_stats_t *smf_stats)
{
	wlc_smfs_elem_t *headptr = smf_stats->stats;
	wlc_smfs_elem_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		MFREE(wlc->osh, curptr, sizeof(wlc_smfs_elem_t));
	}
	smf_stats->stats = NULL;
	return 0;
}

static int
wlc_bsscfg_smfsfree(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg)
{
	int i;

	if (!bsscfg->smfs_info)
		return 0;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		wlc_smf_stats_t *smf_stats = &bsscfg->smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);
	}
	MFREE(wlc->osh, bsscfg->smfs_info, sizeof(wlc_smfs_info_t));
	bsscfg->smfs_info = NULL;

	return 0;
}

static int
linear_search_u16(const uint16 array[], uint16 key, int size)
{
	int n;
	for (n = 0; n < size; ++n) {
		if (array[ n ] == key) {
			return n;
		}
	}
	return -1;
}

static wlc_smfs_elem_t *
smfs_elem_create(osl_t *osh, uint16 code)
{
	wlc_smfs_elem_t *elem = NULL;
	elem = MALLOC(osh, sizeof(wlc_smfs_elem_t));

	if (elem) {
		elem->next = NULL;
		elem->smfs_elem.code = code;
		elem->smfs_elem.count = 0;
	}

	return elem;
}

static wlc_smfs_elem_t *
smfs_elem_find(uint16 code, wlc_smfs_elem_t *start)
{
	while (start != NULL) {
		if (code == start->smfs_elem.code)
			break;
		start = start->next;
	}
	return start;
}

/* sort based on code define */
static void
smfs_elem_insert(wlc_smfs_elem_t **rootp, wlc_smfs_elem_t *new)
{
	wlc_smfs_elem_t *curptr;
	wlc_smfs_elem_t *previous;

	curptr = *rootp;
	previous = NULL;

	while (curptr && (curptr->smfs_elem.code < new->smfs_elem.code)) {
		previous = curptr;
		curptr = curptr->next;
	}
	new->next = curptr;

	if (previous == NULL)
		*rootp = new;
	else
		previous->next = new;
}

static bool
smfstats_codetype_included(uint16 code, uint16 codetype)
{
	bool included = FALSE;
	int index = -1;

	if (codetype == SMFS_CODETYPE_SC)
		index = linear_search_u16(smfs_sc_table, code,
		  sizeof(smfs_sc_table)/sizeof(uint16));
	else
		index = linear_search_u16(smfs_rc_table, code,
		  sizeof(smfs_rc_table)/sizeof(uint16));

	if (index != -1)
		included = TRUE;

	return included;
}

static int
smfstats_update(wlc_info_t *wlc, wlc_smf_stats_t *smf_stats, uint16 code)
{
	uint8 codetype = smf_stats->smfs_main.codetype;
	uint32 count_excl = smf_stats->count_excl;
	wlc_smfs_elem_t *elem = smf_stats->stats;
	wlc_smfs_elem_t *new_elem = NULL;
	bool included = smfstats_codetype_included(code, codetype);
	osl_t *osh;

	if (!included && (count_excl > MAX_SCRC_EXCLUDED)) {
		WL_INFORM(("%s: sc/rc  outside the scope, discard\n", __FUNCTION__));
		return 0;
	}

	osh = wlc->osh;
	new_elem = smfs_elem_find(code, elem);

	if (!new_elem) {
		new_elem = smfs_elem_create(osh, code);

		if (!new_elem) {
			WL_ERROR(("wl%d: %s: out of memory for smfs_elem, "
					  "malloced %d bytes\n", wlc->pub->unit, __FUNCTION__,
					  MALLOCED(osh)));
			return BCME_NOMEM;
		}
		else {
			smfs_elem_insert(&smf_stats->stats, new_elem);
			if (!included)
				smf_stats->count_excl++;
			smf_stats->smfs_main.count_total++;
		}
	}
	new_elem->smfs_elem.count++;

	return 0;
}

int
wlc_smfstats_update(struct wlc_info *wlc, wlc_bsscfg_t *cfg, uint8 smfs_type, uint16 code)
{
	wlc_smf_stats_t *smf_stats;
	int err = 0;

	ASSERT(cfg->smfs_info);

	if (!SMFS_ENAB(cfg))
		return err;

	smf_stats = &cfg->smfs_info->smf_stats[smfs_type];

	if (code == SMFS_CODE_MALFORMED) {
		smf_stats->smfs_main.malformed_cnt++;
		return 0;
	}

	if (code == SMFS_CODE_IGNORED) {
		smf_stats->smfs_main.ignored_cnt++;
		return 0;
	}

	err = smfstats_update(wlc, smf_stats, code);

	return err;
}

int
wlc_bsscfg_get_smfs(wlc_bsscfg_t *cfg, int idx, char *buf, int len)
{
	wlc_smf_stats_t *smf_stat;
	wlc_smfs_elem_t *elemt;
	int used_len = 0;
	int err = 0;

	ASSERT((uint)len >= sizeof(wl_smf_stats_t));

	if (idx < 0 || idx > SMFS_TYPE_MAX) {
		err = BCME_RANGE;
		return err;
	}

	smf_stat =  &cfg->smfs_info->smf_stats[idx];
	bcopy(&smf_stat->smfs_main, buf, sizeof(wl_smf_stats_t));

	buf += WL_SMFSTATS_FIXED_LEN;
	used_len += WL_SMFSTATS_FIXED_LEN;

	elemt = smf_stat->stats;

	while (elemt) {
		used_len += sizeof(wl_smfs_elem_t);
		if (used_len > len) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy(&elemt->smfs_elem, buf, sizeof(wl_smfs_elem_t));
		elemt = elemt->next;
		buf += sizeof(wl_smfs_elem_t);
	}
	return err;
}

int
wlc_bsscfg_clear_smfs(struct wlc_info *wlc, wlc_bsscfg_t *cfg)
{
	int i;

	if (!cfg->smfs_info)
		return 0;

	for (i = 0; i < SMFS_TYPE_MAX; i++) {
		wlc_smf_stats_t *smf_stats = &cfg->smfs_info->smf_stats[i];
		smfs_elem_free(wlc, smf_stats);

		smf_stats->smfs_main.length = 0;
		smf_stats->smfs_main.ignored_cnt = 0;
		smf_stats->smfs_main.malformed_cnt = 0;
		smf_stats->smfs_main.count_total = 0;
		smf_stats->count_excl = 0;
	}
	return 0;
}
#endif /* SMF_STATS */

#ifdef WL_BSSCFG_TX_SUPR
void
wlc_bsscfg_tx_stop(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;

	/* Nothing to do */
	if (BSS_TX_SUPR(bsscfg))
		return;

	bsscfg->flags |= WLC_BSSCFG_TX_SUPR;

	/* If there is anything in the data fifo then allow it to drain */
	if (TXPKTPENDTOT(wlc))
		wlc->block_datafifo |= DATA_BLOCK_TX_SUPR;
}

/* Call after the FIFO has drained */
void
wlc_bsscfg_tx_check(wlc_info_t *wlc)
{
	ASSERT(!TXPKTPENDTOT(wlc));

	if (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) {
		int i;
		wlc_bsscfg_t *bsscfg;

		wlc->block_datafifo &= ~DATA_BLOCK_TX_SUPR;

		/* Now complete all the pending transitions */
		FOREACH_BSS(wlc, i, bsscfg) {
			if (bsscfg->tx_start_pending) {
				bsscfg->tx_start_pending = FALSE;
				wlc_bsscfg_tx_start(bsscfg);
			}
		}
	}
}

void
wlc_bsscfg_tx_start(wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = bsscfg->wlc;
	void *pkt;
	int prec;

	/* Nothing to do */
	if (!BSS_TX_SUPR(bsscfg))
		return;

	if (wlc->block_datafifo & DATA_BLOCK_TX_SUPR) {
		/* Finish the transition first to avoid reordering frames */
		if (TXPKTPENDTOT(bsscfg->wlc)) {
			bsscfg->tx_start_pending = TRUE;
			return;
		}
		wlc->block_datafifo &= ~DATA_BLOCK_TX_SUPR;
	}

	bsscfg->flags &= ~WLC_BSSCFG_TX_SUPR;

	/* Dump all the packets from bsscfg->psq to txq but to the front */
	/* This is done to preserve the ordering w/o changing the precedence level
	 * since AMPDU module keeps track of sequence numbers according to their
	 * precedence!
	 */
	while ((pkt = pktq_deq_tail(bsscfg->psq, &prec))) {
		if (!wlc_prec_enq_head(wlc, &wlc->txq, pkt, prec, TRUE)) {
			WL_P2P(("wl%d: wlc_bsscfg_tx_start: txq full, frame discarded\n",
			          wlc->pub->unit));
			PKTFREE(wlc->osh, pkt, TRUE);
			WLCNTINCR(wlc->pub->_cnt->txnobuf);
		}
	}

	if (!pktq_empty(&wlc->txq))
		wlc_send_q(wlc, &wlc->txq);
}

bool
wlc_bsscfg_txq_enq(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu, uint prec)
{
	/* Caller should free the packet if it cannot be accomodated */
	if (!wlc_prec_enq(wlc, bsscfg->psq, sdu, prec)) {
		WL_P2P(("wl%d: %s: txq full, frame discarded\n",
		        wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return TRUE;
	}

	return FALSE;
}
#endif /* WL_BSSCFG_TX_SUPR */
