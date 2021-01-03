/*
 * Code that controls the antenna/core/chain
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * 
 * $Id: wlc_stf.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <epivers.h>
#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <bcmwifi.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ap.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#include <wl_export.h>
#include <wlc_assoc.h>
#include <wlc_bmac.h>
#include <wlc_stf.h>

/* this macro define all PHYs REV that can NOT receive STBC with one RX core active */
#define WLC_STF_NO_STBC_RX_1_CORE(wlc) (WLCISNPHY(wlc->band) && \
	NREV_GT(wlc->band->phyrev, 3) && NREV_LE(wlc->band->phyrev, 6))

#define WLC_TEMPSENSE_PERIOD		10	/* 10 second timeout */


#ifdef WL11N
static int wlc_stf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_stf_ss_get(wlc_info_t* wlc, int8 band);
static bool wlc_stf_ss_set(wlc_info_t* wlc, int32 int_val, int8 band);
static bool wlc_stf_ss_auto(wlc_info_t* wlc);
static int  wlc_stf_ss_auto_set(wlc_info_t* wlc, bool enable);
static int8 wlc_stf_mimops_get(wlc_info_t* wlc);
static bool wlc_stf_mimops_set(wlc_info_t* wlc, int32 int_val);
static int8 wlc_stf_stbc_tx_get(wlc_info_t* wlc);
static int8 wlc_stf_stbc_rx_get(wlc_info_t* wlc);
static bool wlc_stf_stbc_tx_set(wlc_info_t* wlc, int32 int_val);
static int wlc_stf_txcore_set(wlc_info_t *wlc, uint8 idx, uint8 val);
static void wlc_stf_stbc_rx_ht_update(wlc_info_t *wlc, int val);
static bool wlc_stf_stbc_rx_set(wlc_info_t* wlc, int32 int_val);
#endif /* WL11N */

static void _wlc_stf_phy_txant_upd(wlc_info_t *wlc);
static uint16 _wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec);

const uint8 txcore_default[6][2] = {
	{1, 0x01},	/* CCK */
	{1, 0x01},	/* OFDM */
	{1, 0x01},	/* For Nsts = 1, enable core 1 */
	{2, 0x03},	/* For Nsts = 2, enable core 1 & 2 */
	{3, 0x07},	/* For Nsts = 3, enable core 1, 2 & 3 */
	{4, 0x0F} 	/* For Nsts = 4, enable all cores */
};

/* iovar table */
enum {
	IOV_STF_SS,		/* MIMO STS coding for single stream mcs or legacy ofdm rates */
	IOV_STF_SS_AUTO,	/* auto selection of channel-based */
				/* MIMO STS coding for single stream mcs */
				/* OR LEGACY ofdm rates */
	IOV_STF_MIMOPS,		/* MIMO power savw mode */
	IOV_STF_STBC_RX,	/* MIMO, STBC RX */
	IOV_STF_STBC_TX,	/* MIMO, STBC TX */
	IOV_STF_TXSTREAMS,	/* MIMO, tx stream */
	IOV_STF_TXCHAIN,	/* MIMO, tx chain */
	IOV_STF_HW_TXCHAIN,	/* MIMO, HW tx chain */
	IOV_STF_RXSTREAMS,	/* MIMO, rx stream */
	IOV_STF_RXCHAIN,	/* MIMO, rx chain */
	IOV_STF_HW_RXCHAIN,	/* MIMO, HW rx chain */
	IOV_STF_TXCORE,		/* MIMO, tx core enable and selected */
	IOV_STF_POLICY,		/* spatial policy to use */
	IOV_STF_TEMPS_DISABLE,
	IOV_STF_LAST
};

static const bcm_iovar_t stf_iovars[] = {
	{"mimo_ps", IOV_STF_MIMOPS,
	(IOVF_OPEN_ALLOW), IOVT_UINT8, 0
	},
	{"mimo_ss_stf", IOV_STF_SS,
	(IOVF_OPEN_ALLOW), IOVT_INT8, 0
	},
	{"stf_ss_auto", IOV_STF_SS_AUTO,
	(0), IOVT_INT8, 0
	},
	{"stbc_rx", IOV_STF_STBC_RX,
	(IOVF_OPEN_ALLOW), IOVT_UINT8, 0
	},
	{"stbc_tx", IOV_STF_STBC_TX,
	(IOVF_OPEN_ALLOW), IOVT_INT8, 0
	},
	{"txstreams", IOV_STF_TXSTREAMS,
	(0), IOVT_UINT8, 0
	},
	{"txchain", IOV_STF_TXCHAIN,
	(0), IOVT_UINT8, 0
	},
	{"hw_txchain", IOV_STF_HW_TXCHAIN,
	(0), IOVT_UINT8, 0
	},
	{"rxstreams", IOV_STF_RXSTREAMS,
	(0), IOVT_UINT8, 0
	},
	{"hw_rxchain", IOV_STF_HW_RXCHAIN,
	(0), IOVT_UINT8, 0
	},
	{"rxchain", IOV_STF_RXCHAIN,
	(0), IOVT_UINT8, 0
	},
	{"txcore", IOV_STF_TXCORE,
	(0), IOVT_BUFFER,  sizeof(uint32)*2
	},
	{"spatial_policy", IOV_STF_POLICY,
	(0), IOVT_INT8, 0
	},
	{"tempsense_disable", IOV_STF_TEMPS_DISABLE,
	(0), IOVT_BOOL, 0
	},
	{NULL, 0, 0, 0, 0}
};

#ifdef WL11N
/* handle STS related iovars */
static int
wlc_stf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;

	ret_int_ptr = (int32 *)a;
	if ((err = wlc_iovar_check(wlc->pub, vi, a, alen, IOV_ISSET(actionid))) != 0)
		return err;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	if (plen >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)p + sizeof(int_val)), &int_val2, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
		case IOV_GVAL(IOV_STF_SS):
			*ret_int_ptr = wlc_stf_ss_get(wlc, (int8)int_val);
			break;

		case IOV_SVAL(IOV_STF_SS):
			if (!wlc_stf_ss_set(wlc, int_val, (int8)int_val2)) {
				err = BCME_RANGE;
			}
			break;

		case IOV_GVAL(IOV_STF_SS_AUTO):
			*ret_int_ptr = (int)wlc_stf_ss_auto(wlc);
			break;

		case IOV_SVAL(IOV_STF_SS_AUTO):
			err = wlc_stf_ss_auto_set(wlc, bool_val);
			break;

		case IOV_GVAL(IOV_STF_MIMOPS):
			*ret_int_ptr = wlc_stf_mimops_get(wlc);
			break;

		case IOV_SVAL(IOV_STF_MIMOPS):
			if (!wlc_stf_mimops_set(wlc, int_val)) {
				err = BCME_RANGE;
			}
			break;

		case IOV_GVAL(IOV_STF_STBC_TX):
			*ret_int_ptr = wlc_stf_stbc_tx_get(wlc);
			break;

		case IOV_SVAL(IOV_STF_STBC_TX):
			if (!WLC_STBC_CAP_PHY(wlc)) {
				err = BCME_UNSUPPORTED;
				break;
			}

			if (!wlc_stf_stbc_tx_set(wlc, int_val))
				err = BCME_RANGE;

			break;

		case IOV_GVAL(IOV_STF_STBC_RX):
			*ret_int_ptr = wlc_stf_stbc_rx_get(wlc);
			break;

		case IOV_SVAL(IOV_STF_STBC_RX):
			if (!WLC_STBC_CAP_PHY(wlc)) {
				err = BCME_UNSUPPORTED;
				break;
			}
			if (!wlc_stf_stbc_rx_set(wlc, int_val))
				err = BCME_RANGE;

			break;

		case IOV_GVAL(IOV_STF_TXSTREAMS):
			*ret_int_ptr = wlc->stf->txstreams;
			break;

		case IOV_GVAL(IOV_STF_TXCHAIN):
			*ret_int_ptr = wlc->stf->txchain;
			break;

		case IOV_SVAL(IOV_STF_TXCHAIN):
			if (int_val == -1)
				int_val = wlc->stf->hw_txchain;

			err = wlc_stf_txchain_set(wlc, int_val, FALSE);
			break;

		case IOV_GVAL(IOV_STF_HW_TXCHAIN):
			*ret_int_ptr = wlc->stf->hw_txchain;
			break;

		case IOV_GVAL(IOV_STF_RXSTREAMS):
			*ret_int_ptr = (uint8)WLC_BITSCNT(wlc->stf->rxchain);
			break;

		case IOV_GVAL(IOV_STF_RXCHAIN):
			/* use SW Rx chain state */
			*ret_int_ptr = wlc->stf->rxchain;
			break;

		case IOV_GVAL(IOV_STF_HW_RXCHAIN):
			*ret_int_ptr = wlc->stf->hw_rxchain;
			break;

		case IOV_SVAL(IOV_STF_RXCHAIN):
			/* don't allow setting of rxchain if STA is
			 * associated, due to supported rateset needs
			 * to be updated
			 */
			if (wlc->pub->associated)
				return BCME_ASSOCIATED;

#ifdef RXCHAIN_PWRSAVE
			if (RXCHAIN_PWRSAVE_ENAB(wlc->ap)) {
				if (int_val == wlc->stf->hw_rxchain)
					wlc_reset_rxchain_pwrsave_mode(wlc->ap);
				else
					wlc_disable_rxchain_pwrsave(wlc->ap);
			}
#endif
			err = wlc_stf_rxchain_set(wlc, int_val);

			break;

		case IOV_GVAL(IOV_STF_TXCORE):
		{
			uint32 core[2] = {0, 0};

			core[0] |= wlc->stf->txcore[NSTS4_IDX][1] << 24;
			core[0] |= wlc->stf->txcore[NSTS3_IDX][1] << 16;
			core[0] |= wlc->stf->txcore[NSTS2_IDX][1] << 8;
			core[0] |= wlc->stf->txcore[NSTS1_IDX][1];
			core[1] |= wlc->stf->txcore[OFDM_IDX][1] << 8;
			core[1] |= wlc->stf->txcore[CCK_IDX][1];
			bcopy(core, a, sizeof(uint32)*2);
			break;
		}

		case IOV_SVAL(IOV_STF_TXCORE):
		{
			uint32 core[2];
			uint8 i, Nsts = 0;
			uint8 mcs_mask[4] = {0, 0, 0, 0};
			uint8 ofdm_mask = 0, cck_mask = 0;
			bool write_shmem = FALSE;

			bcopy(p, core, sizeof(uint32)*2);
			if (core[0]) {
				for (i = 0; i < 4; i++) {
					Nsts = ((uint8)core[0] & 0x0f);
					mcs_mask[Nsts-1] = ((uint8)core[0] & 0xf0) >> 4;
					if (Nsts > MAX_STREAMS_SUPPORTED) {
						WL_ERROR(("wl%d: %s: Nsts(%d) out of range\n",
							wlc->pub->unit, __FUNCTION__, Nsts));
						err = BCME_RANGE;
						write_shmem = FALSE;
						break;
					}

					if (Nsts > WLC_BITSCNT(mcs_mask[Nsts-1])) {
						WL_ERROR(("wl%d: %s: Nsts(%d) >"
							" # of core enabled (0x%x)\n",
							wlc->pub->unit, __FUNCTION__,
							Nsts, mcs_mask[Nsts-1]));
						err = BCME_BADARG;
						write_shmem = FALSE;
						break;
					}
					core[0] >>= 8;
					write_shmem = TRUE;
				}
			}
			if (core[1]) {
				cck_mask = core[1] & 0x0f;
				if (WLC_BITSCNT(cck_mask) > wlc->stf->txstreams) {
					WL_ERROR(("wl%d: %s: cck core (0x%x) > HW core (0x%x)\n",
						wlc->pub->unit, __FUNCTION__,
						cck_mask, wlc->stf->hw_txchain));
					err = BCME_BADARG;
					write_shmem = FALSE;
					break;
				}
				ofdm_mask = (core[1] >> 8) & 0x0f;
				if (WLC_BITSCNT(ofdm_mask) > wlc->stf->txstreams) {
					WL_ERROR(("wl%d: %s: ofdm core (0x%x) > HW core (0x%x)\n",
						wlc->pub->unit, __FUNCTION__,
						ofdm_mask, wlc->stf->hw_txchain));
					err = BCME_BADARG;
					write_shmem = FALSE;
					break;
				}
				write_shmem = TRUE;
			}

			if (wlc->pub->up && wlc->pub->associated) {
				/* do not set txcore if the interface is associated. */
				WL_ERROR(("wl%d: can't change txcore when associated\n",
					wlc->pub->unit));
				return BCME_ASSOCIATED;
			}

			if (write_shmem) {
				WL_INFORM(("wl%d: %s: cck_mask 0x%02x ofdm_mask 0x%02x\n",
					wlc->pub->unit, __FUNCTION__, cck_mask, ofdm_mask));
				if (cck_mask)
					wlc_stf_txcore_set(wlc, CCK_IDX, cck_mask);
				if (ofdm_mask)
					wlc_stf_txcore_set(wlc, OFDM_IDX, ofdm_mask);
				for (i = 0; i < 4; i++) {
					if (mcs_mask[i] == 0)
						continue;
					WL_INFORM(("wl%d: %s: mcs_mask[%d] 0x%02x\n",
						wlc->pub->unit, __FUNCTION__, i, mcs_mask[i]));
					wlc_stf_txcore_set(wlc, (i + NSTS1_IDX), mcs_mask[i]);
				}
				wlc_stf_txcore_shmem_write(wlc);
			}
			break;
		}

		case IOV_GVAL(IOV_STF_POLICY):
			*ret_int_ptr = wlc->stf->spatial_policy;
			break;

		case IOV_SVAL(IOV_STF_POLICY):
			if (int_val != MIN_SPATIAL_EXPANSION && int_val != MAX_SPATIAL_EXPANSION) {
				WL_ERROR(("wl%d: %s: spatial policy (%d) out of range\n",
					wlc->pub->unit, __FUNCTION__, int_val));
				return BCME_RANGE;
			}

			if (wlc->pub->up && wlc->pub->associated) {
				/* do not set spatial policy if the interface is associated. */
				WL_ERROR(("wl%d: can't change spatial policy when associated\n",
					wlc->pub->unit));
				return BCME_ASSOCIATED;
			}

			err = wlc_stf_spatial_policy_set(wlc, int_val);
			break;

		case IOV_GVAL(IOV_STF_TEMPS_DISABLE):
			*ret_int_ptr = wlc->stf->tempsense_disable;
			break;

		case IOV_SVAL(IOV_STF_TEMPS_DISABLE):
			wlc->stf->tempsense_disable = (uint8)int_val;
			break;

		default:
			err = BCME_UNSUPPORTED;
	}
	return err;
}

static void
wlc_stf_stbc_rx_ht_update(wlc_info_t *wlc, int val)
{
	ASSERT((val == HT_CAP_RX_STBC_NO) || (val == HT_CAP_RX_STBC_ONE_STREAM));

	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
		if ((wlc->stf->rxstreams == 1) && (val != HT_CAP_RX_STBC_NO))
			return;
	}

	wlc->ht_cap.cap &= ~HT_CAP_RX_STBC_MASK;
	wlc->ht_cap.cap |= (val << HT_CAP_RX_STBC_SHIFT);

	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

#if defined(BCMDBG_DUMP)
static void
wlc_dump_stf(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;

	bcm_bprintf(b, "STF dump:\n");

	bcm_bprintf(b, "hw_txchain 0X%x hw_rxchain 0x%x\n",
		wlc->stf->hw_txchain, wlc->stf->hw_rxchain);
	bcm_bprintf(b, "txchain 0x%x txstreams %d \nrxchain 0x%x rxstreams %d\n",
		wlc->stf->txchain, wlc->stf->txstreams, wlc->stf->rxchain, wlc->stf->rxstreams);
	bcm_bprintf(b, "txant %d ant_rx_ovr %d phytxant 0x%x\n",
		wlc->stf->txant, wlc->stf->ant_rx_ovr, wlc->stf->phytxant);
	bcm_bprintf(b, "txcore %d %d %d %d %d %d\n",
		wlc->stf->txcore[NSTS4_IDX][1], wlc->stf->txcore[NSTS3_IDX][1],
		wlc->stf->txcore[NSTS2_IDX][1], wlc->stf->txcore[NSTS1_IDX][1],
		wlc->stf->txcore[OFDM_IDX][1], wlc->stf->txcore[CCK_IDX][1]);
	bcm_bprintf(b, "stf_ss_auto: %d ss_opmode %d ss_algo_channel 0x%x no_cddstbc %d\n",
		wlc->stf->ss_algosel_auto, wlc->stf->ss_opmode, wlc->stf->ss_algo_channel,
		wlc->stf->no_cddstbc);
	bcm_bprintf(b, "\nop_stf_ss: %s", wlc->band->band_stf_stbc_tx == AUTO ? "STBC" :
		(wlc->stf->ss_opmode == PHY_TXC1_MODE_SISO ? "SISO" : "CDD"));

	for (i = 0; i < NBANDS(wlc); i++) {
		bcm_bprintf(b, "\tband%d stf_ss: %s ", i,
			wlc->bandstate[i]->band_stf_stbc_tx == AUTO ? "STBC" :
			(wlc->bandstate[i]->band_stf_ss_mode == PHY_TXC1_MODE_SISO ?
			"SISO" : "CDD"));
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "ipaon %d", wlc->stf->ipaon);

	bcm_bprintf(b, "\n");
}
#endif 

/* every WLC_TEMPSENSE_PERIOD seconds temperature check to decide whether to turn on/off txchain */
void
wlc_stf_tempsense_upd(wlc_info_t *wlc)
{
	wlc_phy_t *pi = wlc->band->pi;
	uint active_chains, txchain;


	if (!WLCISNPHY(wlc->band) || wlc->stf->tempsense_disable)
		return;

	if ((wlc->pub->now - wlc->stf->tempsense_lasttime) < wlc->stf->tempsense_period)
		return;

	wlc->stf->tempsense_lasttime = wlc->pub->now;

	/* Check if the chip is too hot. Disable one Tx chain, if it is */
	/* high 4 bits are for Rx chain, low 4 bits are  for Tx chain */
	active_chains = wlc_phy_stf_chain_active_get(pi);
	txchain = active_chains & 0xf;

	if (wlc->stf->txchain == wlc->stf->hw_txchain) {
		if (txchain && (txchain < wlc->stf->hw_txchain)) {
			/* turn off 1 tx chain */
			wlc_stf_txchain_set(wlc, txchain, TRUE);
		}
	} else if (wlc->stf->txchain < wlc->stf->hw_txchain) {
		if (txchain == wlc->stf->hw_txchain) {
			/* turn back on txchain */
			wlc_stf_txchain_set(wlc, txchain, TRUE);
		}
	}
}

void
wlc_stf_ss_algo_channel_get(wlc_info_t *wlc, uint16 *ss_algo_channel, chanspec_t chanspec)
{
	tx_power_t power;
	uint8 siso_mcs_id, cdd_mcs_id, stbc_mcs_id;

	/* Clear previous settings */
	*ss_algo_channel = 0;

	if (!wlc->pub->up) {
		*ss_algo_channel = (uint16)-1;
		return;
	}

	wlc_phy_txpower_get_current(wlc->band->pi, &power, CHSPEC_CHANNEL(chanspec));

	siso_mcs_id = (CHSPEC_IS40(chanspec)) ?
		WL_TX_POWER_MCS40_SISO_FIRST : WL_TX_POWER_MCS20_SISO_FIRST;
	cdd_mcs_id = (CHSPEC_IS40(chanspec)) ?
		WL_TX_POWER_MCS40_CDD_FIRST : WL_TX_POWER_MCS20_CDD_FIRST;
	stbc_mcs_id = (CHSPEC_IS40(chanspec)) ?
		WL_TX_POWER_MCS40_STBC_FIRST : WL_TX_POWER_MCS20_STBC_FIRST;

	/* criteria to choose stf mode */

	/* the "+3dbm (12 0.25db units)" is to account for the fact that with CDD, tx occurs
	 * on both chains
	 */
	if (power.target[siso_mcs_id] > (power.target[cdd_mcs_id] + 12))
		setbit(ss_algo_channel, PHY_TXC1_MODE_SISO);
	else
		setbit(ss_algo_channel, PHY_TXC1_MODE_CDD);

	/* STBC is ORed into to algo channel as STBC requires per-packet SCB capability check
	 * so cannot be default mode of operation. One of SISO, CDD have to be set
	 */
	if (power.target[siso_mcs_id] <= (power.target[stbc_mcs_id] + 12))
		setbit(ss_algo_channel, PHY_TXC1_MODE_STBC);
}

static bool
wlc_stf_stbc_rx_set(wlc_info_t* wlc, int32 int_val)
{
	if ((int_val != HT_CAP_RX_STBC_NO) && (int_val != HT_CAP_RX_STBC_ONE_STREAM)) {
		return FALSE;
	}

	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
		if ((int_val != HT_CAP_RX_STBC_NO) && (wlc->stf->rxstreams == 1))
			return FALSE;
	}

	wlc_stf_stbc_rx_ht_update(wlc, int_val);
	return TRUE;
}

static int
wlc_stf_ss_get(wlc_info_t* wlc, int8 band)
{
	wlcband_t *wlc_band = NULL;

	if (band == -1)
		wlc_band = wlc->band;
	else if ((band < 0) || (band > (int)NBANDS(wlc)))
		return BCME_RANGE;
	else
		wlc_band = wlc->bandstate[band];

	return (int)(wlc_band->band_stf_ss_mode);
}

static bool
wlc_stf_ss_set(wlc_info_t* wlc, int32 int_val, int8 band)
{
	wlcband_t *wlc_band = NULL;

	if (band == -1)
		wlc_band = wlc->band;
	else if ((band < 0) || (band > (int)NBANDS(wlc)))
		return FALSE;
	else
		wlc_band = wlc->bandstate[band];

	if ((int_val == PHY_TXC1_MODE_CDD) && (wlc->stf->txstreams == 1)) {
		return FALSE;
	}

	if (int_val != PHY_TXC1_MODE_SISO && int_val != PHY_TXC1_MODE_CDD)
		return FALSE;

	wlc_band->band_stf_ss_mode = (int8)int_val;
	wlc_stf_ss_update(wlc, wlc_band);

	return TRUE;
}

static bool
wlc_stf_ss_auto(wlc_info_t* wlc)
{
	return wlc->stf->ss_algosel_auto;
}


static int
wlc_stf_ss_auto_set(wlc_info_t *wlc, bool enable)
{
	if (wlc->stf->ss_algosel_auto == enable)
		return 0;

	if (WLC_STBC_CAP_PHY(wlc) && enable)
		wlc_stf_ss_algo_channel_get(wlc, &wlc->stf->ss_algo_channel, wlc->chanspec);

	wlc->stf->ss_algosel_auto = enable;
	wlc_stf_ss_update(wlc, wlc->band);

	return 0;
}

static int8
wlc_stf_mimops_get(wlc_info_t* wlc)
{
	return (int8)((wlc->ht_cap.cap & HT_CAP_MIMO_PS_MASK) >> HT_CAP_MIMO_PS_SHIFT);
}

static bool
wlc_stf_mimops_set(wlc_info_t* wlc, int32 int_val)
{
	if ((int_val < 0) || (int_val > HT_CAP_MIMO_PS_OFF) || (int_val == 2)) {
		return FALSE;
	}

	wlc_ht_mimops_cap_update(wlc, (uint8)int_val);

	if (wlc->pub->associated)
		wlc_send_action_ht_mimops(wlc, (uint8)int_val);

	return TRUE;
}

static int8
wlc_stf_stbc_tx_get(wlc_info_t* wlc)
{
	return wlc->band->band_stf_stbc_tx;
}

static int8
wlc_stf_stbc_rx_get(wlc_info_t* wlc)
{
	return (wlc->ht_cap.cap & HT_CAP_RX_STBC_MASK) >> HT_CAP_RX_STBC_SHIFT;
}

static bool
wlc_stf_stbc_tx_set(wlc_info_t* wlc, int32 int_val)
{
	if ((int_val != AUTO) && (int_val != OFF) && (int_val != ON)) {
		return FALSE;
	}

	if ((int_val == ON) && (wlc->stf->txstreams == 1))
		return FALSE;

	if ((int_val == OFF) || (wlc->stf->txstreams == 1) || !WLC_STBC_CAP_PHY(wlc))
		wlc->ht_cap.cap &= ~HT_CAP_TX_STBC;
	else
		wlc->ht_cap.cap |= HT_CAP_TX_STBC;

	wlc->bandstate[BAND_2G_INDEX]->band_stf_stbc_tx = (int8)int_val;
	wlc->bandstate[BAND_5G_INDEX]->band_stf_stbc_tx = (int8)int_val;

	return TRUE;
}

static uint8
wlc_stf_spatial_map(wlc_info_t *wlc, uint8 idx)
{
	uint8 ncores = (uint8)WLC_BITSCNT(wlc->stf->txcore[idx][1]);
	uint8 Nsts = wlc->stf->txcore[idx][0];

	if (wlc->stf->txstreams < Nsts)
		return 0;

	ASSERT(ncores <= wlc->stf->txstreams);
	/* ncores can be 0 for non-supported Nsts */
	if (ncores == 0)
		return 0;

	if (Nsts == ncores) return 0;
	else if (Nsts == 1 && ncores == 2) return 1;
	else if (Nsts == 1 && ncores == 3) return 2;
	else if (Nsts == 2 && ncores == 3) return 3;
	else ASSERT(0);
	return 0;
}

static int
wlc_stf_txcore_set(wlc_info_t *wlc, uint8 idx, uint8 core_mask)
{
	WL_TRACE(("wl%d: wlc_stf_txcore_set\n", wlc->pub->unit));

	ASSERT(idx < MAX_CORE_IDX);

	WL_INFORM(("wl%d: %s: Nsts %d core_mask %x\n",
		wlc->pub->unit, __FUNCTION__, wlc->stf->txcore[idx][0], core_mask));

	if (WLC_BITSCNT(core_mask) > wlc->stf->txstreams) {
		WL_INFORM(("wl%d: %s: core_mask(0x%x) > #tx stream(%d) supported, disable it\n",
			wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->txstreams));
		core_mask = 0;
	}

	if ((WLC_BITSCNT(core_mask) == wlc->stf->txstreams) &&
	    ((core_mask & ~wlc->stf->txchain) || !(core_mask & wlc->stf->txchain))) {
		WL_INFORM(("wl%d: %s: core_mask(0x%x) mismatch #txchain(0x%x), force to txchain\n",
			wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->txchain));
		core_mask = wlc->stf->txchain;
	}

	wlc->stf->txcore[idx][1] = core_mask;
	if (idx == OFDM_IDX || (idx == CCK_IDX)) {
		/* Needs to update beacon and ucode generated response
		 * frames when 1 stream core map changed
		 */
		wlc->stf->phytxant = core_mask << PHY_TXC_ANT_SHIFT;
		wlc_bmac_txant_set(wlc->hw, wlc->stf->phytxant);
		if (wlc->clk) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_beacon_phytxctl_txant_upd(wlc, wlc->bcn_rspec);
			wlc_enable_mac(wlc);
		}
	}

	WL_INFORM(("wl%d: %s: IDX %d: Nsts %d Core mask 0x%x\n",
		wlc->pub->unit, __FUNCTION__, idx, wlc->stf->txcore[idx][0], core_mask));

	/* invalid tx cache due to core mask change */
	if (WLC_TXC_ENAB(wlc))
		wlc->txcgen++;
	return BCME_OK;
}

int
wlc_stf_spatial_policy_set(wlc_info_t *wlc, int val)
{
	uint8 idx, Nsts;
	uint8 core_mask = 0;

	WL_TRACE(("wl%d: %s: val %x\n", wlc->pub->unit, __FUNCTION__, val));

	wlc->stf->spatial_policy = (int8)val;
	for (idx = 0; idx < MAX_CORE_IDX; idx++) {
		core_mask = (val == MAX_SPATIAL_EXPANSION) ?
			wlc->stf->txchain : txcore_default[idx][1];
		Nsts = wlc->stf->txcore[idx][0];
		/* only initial mcs_txcore to max hw supported */
		if (Nsts > wlc->stf->txstreams) {
			WL_INFORM(("wl%d: %s: Nsts (%d) > # of streams hw supported (%d)\n",
				wlc->pub->unit, __FUNCTION__, Nsts, wlc->stf->txstreams));
			core_mask = 0;
		}

		if (WLC_BITSCNT(core_mask) > wlc->stf->txstreams) {
			WL_INFORM(("wl%d: %s: core_mask (0x%02x) > # of HW core enabled (0x%x)\n",
				wlc->pub->unit, __FUNCTION__, core_mask, wlc->stf->hw_txchain));
			core_mask = 0;
		}

		wlc_stf_txcore_set(wlc, idx, core_mask);
	}
	wlc_stf_txcore_shmem_write(wlc);
	return BCME_OK;
}

int
wlc_stf_txchain_set(wlc_info_t *wlc, int32 int_val, bool force)
{
	uint8 txchain = (uint8)int_val;
	uint8 txstreams;
	uint i;

	if (wlc->stf->txchain == txchain)
		return BCME_OK;

	if ((txchain & ~wlc->stf->hw_txchain) || !(txchain & wlc->stf->hw_txchain))
		return BCME_RANGE;

	/* if nrate override is configured to be non-SISO STF mode, reject reducing txchain to 1 */
	txstreams = (uint8)WLC_BITSCNT(txchain);
	if (txstreams > MAX_STREAMS_SUPPORTED)
		return BCME_RANGE;

	if (txstreams == 1) {
		for (i = 0; i < NBANDS(wlc); i++)
			if ((RSPEC_STF(wlc->bandstate[i]->rspec_override) != PHY_TXC1_MODE_SISO) ||
			    (RSPEC_STF(wlc->bandstate[i]->mrspec_override) != PHY_TXC1_MODE_SISO)) {
				if (!force)
					return BCME_ERROR;

				/* over-write the override rspec */
				if (RSPEC_STF(wlc->bandstate[i]->rspec_override) !=
				    PHY_TXC1_MODE_SISO) {
					wlc->bandstate[i]->rspec_override = 0;
					WL_ERROR(("%s(): temp sense override non-SISO"
						" rspec_override.\n", __FUNCTION__));
				}
				if (RSPEC_STF(wlc->bandstate[i]->mrspec_override) !=
				    PHY_TXC1_MODE_SISO) {
					wlc->bandstate[i]->mrspec_override = 0;
					WL_ERROR(("%s(): temp sense override non-SISO"
						" mrspec_override.\n", __FUNCTION__));
				}
			}
	}

	wlc->stf->txchain = txchain;
	wlc->stf->txstreams = txstreams;
	wlc_stf_stbc_tx_set(wlc, wlc->band->band_stf_stbc_tx);
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_2G_INDEX]);
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_5G_INDEX]);
	wlc->stf->txant = (wlc->stf->txstreams == 1) ? ANT_TX_FORCE_0 : ANT_TX_DEF;
	_wlc_stf_phy_txant_upd(wlc);
	/* we need to take care of wlc_rate_init for every scb here */
	wlc_scb_ratesel_init_all(wlc);

	wlc_phy_stf_chain_set(wlc->band->pi, wlc->stf->txchain, wlc->stf->rxchain);

	for (i = 0; i < MAX_CORE_IDX; i++)
		wlc_stf_txcore_set(wlc, (uint8)i, txcore_default[i][1]);
	wlc_stf_txcore_shmem_write(wlc);
	return BCME_OK;
}

int
wlc_stf_rxchain_set(wlc_info_t* wlc, int32 int_val)
{
	uint8 rxchain_cnt;
	uint8 rxchain = (uint8)int_val;
	uint8 mimops_mode;
	uint8 old_rxchain, old_rxchain_cnt;

	if (wlc->stf->rxchain == rxchain)
		return BCME_OK;

	if ((rxchain & ~wlc->stf->hw_rxchain) || !(rxchain & wlc->stf->hw_rxchain))
		return BCME_RANGE;

	rxchain_cnt = (uint8)WLC_BITSCNT(rxchain);

	/* PHY cannot receive STBC with only one rx core active */
	if (WLC_STF_NO_STBC_RX_1_CORE(wlc)) {
		if ((rxchain_cnt == 1) && (wlc_stf_stbc_rx_get(wlc) != HT_CAP_RX_STBC_NO))
			return BCME_RANGE;
	}

	if (APSTA_ENAB(wlc->pub) && (wlc->pub->associated))
		return BCME_ASSOCIATED;

	old_rxchain = wlc->stf->rxchain;
	old_rxchain_cnt = wlc->stf->rxstreams;

	wlc->stf->rxchain = rxchain;
	wlc->stf->rxstreams = rxchain_cnt;

	if (rxchain_cnt != old_rxchain_cnt) {
		mimops_mode = (rxchain_cnt == 1) ? HT_CAP_MIMO_PS_ON : HT_CAP_MIMO_PS_OFF;
		wlc->mimops_PM = mimops_mode;
		if (AP_ENAB(wlc->pub)) {
			wlc_phy_stf_chain_set(wlc->band->pi, wlc->stf->txchain, wlc->stf->rxchain);
			wlc_ht_mimops_cap_update(wlc, mimops_mode);
			if (wlc->pub->associated)
				wlc_mimops_action_ht_send(wlc, mimops_mode);
			return BCME_OK;
		}
		if (wlc->pub->associated) {
			if (mimops_mode == HT_CAP_MIMO_PS_OFF) {
				/* if mimops is off, turn on the Rx chain first */
				wlc_phy_stf_chain_set(wlc->band->pi, wlc->stf->txchain,
					wlc->stf->rxchain);
				wlc_ht_mimops_cap_update(wlc, mimops_mode);
			}
			wlc_mimops_action_ht_send(wlc, mimops_mode);
		}
		else {
			wlc_phy_stf_chain_set(wlc->band->pi, wlc->stf->txchain, wlc->stf->rxchain);
			wlc_ht_mimops_cap_update(wlc, mimops_mode);
		}
	}
	else if (old_rxchain != rxchain)
		wlc_phy_stf_chain_set(wlc->band->pi, wlc->stf->txchain, wlc->stf->rxchain);

	return BCME_OK;
}

/* update wlc->stf->ss_opmode which represents the operational stf_ss mode we're using */
int
wlc_stf_ss_update(wlc_info_t *wlc, wlcband_t *band)
{
	int ret_code = 0;
	uint8 prev_stf_ss;
	uint8 upd_stf_ss;

	prev_stf_ss = wlc->stf->ss_opmode;

	/* NOTE: opmode can only be SISO or CDD as STBC is decided on a per-packet basis */
	if (WLC_STBC_CAP_PHY(wlc) &&
	    wlc->stf->ss_algosel_auto && (wlc->stf->ss_algo_channel != (uint16)-1)) {
		ASSERT(isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_CDD) ||
		       isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_SISO));
		upd_stf_ss = (wlc->stf->no_cddstbc || (wlc->stf->txstreams == 1) ||
			isset(&wlc->stf->ss_algo_channel, PHY_TXC1_MODE_SISO)) ?
			PHY_TXC1_MODE_SISO : PHY_TXC1_MODE_CDD;
	} else {
		if (wlc->band != band)
			return ret_code;
		upd_stf_ss = (wlc->stf->no_cddstbc || (wlc->stf->txstreams == 1)) ?
			PHY_TXC1_MODE_SISO : band->band_stf_ss_mode;
	}
	if (prev_stf_ss != upd_stf_ss) {
		wlc->stf->ss_opmode = upd_stf_ss;
		wlc_bmac_band_stf_ss_set(wlc->hw, upd_stf_ss);
	}

	return ret_code;
}

uint8
wlc_stf_txcore_get(wlc_info_t *wlc, ratespec_t rspec)
{
	uint8 idx;
	/* MCS_TXS returns # of streams based 0 {0..3} */
	uint8 Nss = IS_MCS(rspec) ? (MCS_TXS(rspec & RSPEC_RATE_MASK) + 1) : 1;
	uint8 Nstbc = (Nss == 1 && RSPEC_STF(rspec) == PHY_TXC1_MODE_STBC) ? 1 : 0;

	if (IS_CCK(rspec)) {
		idx = CCK_IDX;
		return wlc->stf->txcore[CCK_IDX][1];
	} else if (IS_OFDM(rspec)) {
		idx = OFDM_IDX;
		return wlc->stf->txcore[OFDM_IDX][1];
	} else {
		idx = Nss + Nstbc + OFDM_IDX;
	}
	WL_NONE(("wl%d: %s: Nss %d  Nstbc %d\n", wlc->pub->unit, __FUNCTION__, Nss, Nstbc));
	ASSERT(idx < MAX_CORE_IDX);
	WL_NONE(("wl%d: %s: wlc->stf->txcore[%d] 0x%02x\n",
		wlc->pub->unit, __FUNCTION__, idx, wlc->stf->txcore[idx][1]));
	return wlc->stf->txcore[idx][1];
}

void
wlc_stf_txcore_shmem_write(wlc_info_t *wlc)
{
	uint16 offset;
	uint16 shmem;
	uint16 base;
	uint8 idx;

	if (!wlc->clk) {
		WL_ERROR(("wl%d: %s: No clock\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (D11REV_LT(wlc->pub->corerev, 26) || !WLCISHTPHY(wlc->band)) {
		WL_INFORM(("wl%d: %s: For now txcore shmem only supported"
			" by HT PHY for corerev >= 26\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	base = wlc->stf->shmem_base;
	for (offset = 0, idx = 0; offset < MAX_COREMASK_BLK; offset++, idx++) {
		shmem = ((wlc_stf_spatial_map(wlc, idx) << SPATIAL_SHIFT) |
			(wlc->stf->txcore[idx][1] & TXCOREMASK));
		WL_NONE(("shmem 0x%04x shmem 0x%04x\n", (base+offset)*2, shmem));
		wlc_write_shm(wlc, (base+offset)*2, shmem);
	}
}
#endif	/* WL11N */

void
wlc_stf_wowl_upd(wlc_info_t *wlc, uint16 base)
{
#ifdef WL11N
	wlc->stf->shmem_base = base;
	wlc_stf_txcore_shmem_write(wlc);
#endif	/* WL11N */
}

int
BCMATTACHFN(wlc_stf_attach)(wlc_info_t* wlc)
{
	uint temp;
#ifdef WL11N
	/* register module */
	if (wlc_module_register(wlc->pub, stf_iovars, "stf", wlc, wlc_stf_doiovar, NULL, NULL)) {
		WL_ERROR(("wl%d: stf wlc_stf_iovar_attach failed\n", wlc->pub->unit));
		return -1;
	}

#if defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "stf", (dump_fn_t)wlc_dump_stf, (void *)wlc);
#endif
	wlc->bandstate[BAND_2G_INDEX]->band_stf_ss_mode = PHY_TXC1_MODE_SISO;
	wlc->bandstate[BAND_5G_INDEX]->band_stf_ss_mode = PHY_TXC1_MODE_CDD;

	if ((WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band)) &&
	    (wlc_phy_txpower_hw_ctrl_get(wlc->band->pi) != PHY_TPC_HW_ON))
		wlc->bandstate[BAND_2G_INDEX]->band_stf_ss_mode = PHY_TXC1_MODE_CDD;
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_2G_INDEX]);
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_5G_INDEX]);

	wlc_stf_stbc_rx_ht_update(wlc, HT_CAP_RX_STBC_NO);
	wlc->bandstate[BAND_2G_INDEX]->band_stf_stbc_tx = OFF;
	wlc->bandstate[BAND_5G_INDEX]->band_stf_stbc_tx = OFF;

	if (WLC_STBC_CAP_PHY(wlc)) {
		wlc->stf->ss_algosel_auto = TRUE;
		wlc->stf->ss_algo_channel = (uint16)-1; /* Init the default value */
#ifdef LATER
		wlc_stf_stbc_rx_ht_update(wlc, HT_CAP_RX_STBC_ONE_STREAM);
		if (wlc->stf->txstreams > 1) {
			wlc->bandstate[BAND_2G_INDEX]->band_stf_stbc_tx = AUTO;
			wlc->bandstate[BAND_5G_INDEX]->band_stf_stbc_tx = AUTO;
			wlc->ht_cap.cap |= HT_CAP_TX_STBC;
		}
#endif
	}

#endif /* WL11N */

	wlc->stf->tempsense_period = WLC_TEMPSENSE_PERIOD;
	temp = getintvar(wlc->pub->vars, "temps_period");
	/* valid range is 1-14. ignore 0 and 0xf to work with old srom/nvram */
	if ((temp != 0) && (temp < 0xf))
		wlc->stf->tempsense_period = temp;
	return 0;
}

void
BCMATTACHFN(wlc_stf_detach)(wlc_info_t* wlc)
{
#ifdef WL11N
	wlc_module_unregister(wlc->pub, "stf", wlc);
#endif /* WL11N */
}

int
wlc_stf_ant_txant_validate(wlc_info_t *wlc, int8 val)
{
	int bcmerror = BCME_OK;

	/* when there is only 1 tx_streams, don't allow to change the txant */
	if (WLCISNPHY(wlc->band) && (wlc->stf->txstreams == 1))
		return ((val == wlc->stf->txant) ? bcmerror : BCME_RANGE);

	switch (val) {
		case -1:
			val = ANT_TX_DEF;
			break;
		case 0:
			val = ANT_TX_FORCE_0;
			break;
		case 1:
			val = ANT_TX_FORCE_1;
			break;
		case 3:
			val = ANT_TX_LAST_RX;
			break;
		default:
			bcmerror = BCME_RANGE;
			break;
	}

	if (bcmerror == BCME_OK)
		wlc->stf->txant = (int8)val;

	return bcmerror;

}

/*
 * Centralized txant update function. call it whenever wlc->stf->txant and/or wlc->stf->txchain
 *  change
 *
 * Antennas are controlled by ucode indirectly, which drives PHY or GPIO to
 *   achieve various tx/rx antenna selection schemes
 *
 * legacy phy, bit 6 and bit 7 means antenna 0 and 1 respectively, bit6+bit7 means auto(last rx)
 * for NREV<3, bit 6 and bit 7 means antenna 0 and 1 respectively, bit6+bit7 means last rx and
 *    do tx-antenna selection for SISO transmissions
 * for NREV=3, bit 6 and bit _8_ means antenna 0 and 1 respectively, bit6+bit7 means last rx and
 *    do tx-antenna selection for SISO transmissions
 * for NREV>=7, bit 6 and bit 7 mean antenna 0 and 1 respectively, nit6+bit7 means both cores active
*/
static void
_wlc_stf_phy_txant_upd(wlc_info_t *wlc)
{
	int8 txant;

	txant = (int8)wlc->stf->txant;
	ASSERT(txant == ANT_TX_FORCE_0 || txant == ANT_TX_FORCE_1 || txant == ANT_TX_LAST_RX);

	if (WLCISHTPHY(wlc->band)) {
		/* phytxant is not use by HT phy, initialised to single stream core mask */
		wlc->stf->phytxant = wlc->stf->txcore[NSTS1_IDX][1] << PHY_TXC_ANT_SHIFT;
	} else if (WLC_PHY_11N_CAP(wlc->band) || WLCISLPPHY(wlc->band)) {
		if (txant == ANT_TX_FORCE_0) {
			wlc->stf->phytxant = PHY_TXC_ANT_0;
		} else if (txant == ANT_TX_FORCE_1) {
			wlc->stf->phytxant = PHY_TXC_ANT_1;

			if (WLCISNPHY(wlc->band) &&
			    NREV_GE(wlc->band->phyrev, 3) && NREV_LT(wlc->band->phyrev, 7)) {
				if (((CHIPID(wlc->pub->sih->chip)) == BCM43236_CHIP_ID) ||
				    ((CHIPID(wlc->pub->sih->chip)) == BCM5357_CHIP_ID)) {
				} else {
					wlc->stf->phytxant = PHY_TXC_ANT_2;
				}
			}
		} else {
			/* For LPPHY: specific antenna must be selected, ucode would set last rx */
			if (WLCISLPPHY(wlc->band) || WLCISSSLPNPHY(wlc->band) ||
			    WLCISLCNPHY(wlc->band))
				wlc->stf->phytxant = PHY_TXC_LPPHY_ANT_LAST;
			else {
				/* keep this assert to catch out of sync wlc->stf->txcore */
				ASSERT(wlc->stf->txchain > 0);
				wlc->stf->phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
			}
		}
	} else {
		if (txant == ANT_TX_FORCE_0)
			wlc->stf->phytxant = PHY_TXC_OLD_ANT_0;
		else if (txant == ANT_TX_FORCE_1)
			wlc->stf->phytxant = PHY_TXC_OLD_ANT_1;
		else
			wlc->stf->phytxant = PHY_TXC_OLD_ANT_LAST;
	}

	WL_INFORM(("wl%d: _wlc_stf_phy_txant_upd: set core mask 0x%04x\n",
		wlc->pub->unit, wlc->stf->phytxant));
	wlc_bmac_txant_set(wlc->hw, wlc->stf->phytxant);
}

void
wlc_stf_phy_txant_upd(wlc_info_t *wlc)
{
	_wlc_stf_phy_txant_upd(wlc);
}

void
wlc_stf_phy_chain_calc(wlc_info_t *wlc)
{
	/* get available rx/tx chains */
	wlc->stf->hw_txchain = (uint8)getintvar(wlc->pub->vars, "txchain");
	wlc->stf->hw_rxchain = (uint8)getintvar(wlc->pub->vars, "rxchain");

	if (wlc->pub->sih->chip == BCM43221_CHIP_ID && wlc->stf->hw_txchain == 3) {
		WL_ERROR(("wl%d: %s: wrong txchain setting %x for 43221. Correct it to 1\n",
			wlc->pub->unit, __FUNCTION__, wlc->stf->hw_txchain));
		wlc->stf->hw_txchain = 1;
	}

	/* these parameter are intended to be used for all PHY types */
	if (wlc->stf->hw_txchain == 0 || wlc->stf->hw_txchain == 0xf) {
		if (WLCISHTPHY(wlc->band)) {
			wlc->stf->hw_txchain = TXCHAIN_DEF_HTPHY;
		} else if (WLCISNPHY(wlc->band)) {
			wlc->stf->hw_txchain = TXCHAIN_DEF_NPHY;
		} else {
			wlc->stf->hw_txchain = TXCHAIN_DEF;
		}
	}

	wlc->stf->txchain = wlc->stf->hw_txchain;
	wlc->stf->txstreams = (uint8)WLC_BITSCNT(wlc->stf->hw_txchain);

	if (wlc->stf->hw_rxchain == 0 || wlc->stf->hw_rxchain == 0xf) {
		if (WLCISHTPHY(wlc->band)) {
			wlc->stf->hw_rxchain = RXCHAIN_DEF_HTPHY;
		} else if (WLCISNPHY(wlc->band)) {
			wlc->stf->hw_rxchain = RXCHAIN_DEF_NPHY;
		} else {
			wlc->stf->hw_rxchain = RXCHAIN_DEF;
		}
	}

	wlc->stf->rxchain = wlc->stf->hw_rxchain;
	wlc->stf->rxstreams = (uint8)WLC_BITSCNT(wlc->stf->hw_rxchain);

#ifdef WL11N
	/* initialize the txcore table */
	bcopy(txcore_default, wlc->stf->txcore, sizeof(wlc->stf->txcore));
	/* default spatial_policy */
	wlc_stf_spatial_policy_set(wlc, MAX_SPATIAL_EXPANSION);
#endif /* WL11N */
}

static uint16
_wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 phytxant = wlc->stf->phytxant;

#ifdef WL11N
	if (WLCISHTPHY(wlc->band)) {
		phytxant = wlc_stf_txcore_get(wlc, rspec) << PHY_TXC_ANT_SHIFT;
	} else
#endif /* WL11N */
	{
		if (RSPEC_STF(rspec) != PHY_TXC1_MODE_SISO) {
			ASSERT(wlc->stf->txstreams > 1);
			phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
		} else if (wlc->stf->txant == ANT_TX_DEF)
			phytxant = wlc->stf->txchain << PHY_TXC_ANT_SHIFT;
		phytxant &= PHY_TXC_ANT_MASK;
	}
	return phytxant;
}

uint16
wlc_stf_phytxchain_sel(wlc_info_t *wlc, ratespec_t rspec)
{
	return _wlc_stf_phytxchain_sel(wlc, rspec);
}

uint16
wlc_stf_d11hdrs_phyctl_txant(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 phytxant = wlc->stf->phytxant;
	uint16 mask = PHY_TXC_ANT_MASK;

	/* for non-siso rates or default setting, use the available chains */
	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band)) {
		ASSERT(wlc->stf->txchain != 0);
		phytxant = _wlc_stf_phytxchain_sel(wlc, rspec);
		mask = PHY_TXC_HTANT_MASK;
	}
	phytxant |= phytxant & mask;
	return phytxant;
}

uint16
wlc_stf_spatial_expansion_get(wlc_info_t *wlc, ratespec_t rspec)
{
	uint16 spatial_map = 0;
#ifdef WL11N
	uint8 Nsts;
	uint8 idx = 0;
	/* MCS_TXS returns # of streams based 0 {0..3} */
	uint8 Nss = IS_MCS(rspec) ? (MCS_TXS(rspec & RSPEC_RATE_MASK) + 1) : 1;
	uint8 Nstbc = (Nss == 1 && RSPEC_STF(rspec) == PHY_TXC1_MODE_STBC) ? 1 : 0;

	if (!WLCISHTPHY(wlc->band))
		ASSERT(0);

	WL_NONE(("wlc_stf_spatial_expansion_get: Nss %d  Nstbc %d\n", Nss, Nstbc));
	Nsts = Nss + Nstbc;
	ASSERT(Nsts <= wlc->stf->txstreams);
	if (IS_CCK(rspec))
		idx = CCK_IDX;
	else if (IS_OFDM(rspec))
		idx = OFDM_IDX;
	else
		idx = Nsts + OFDM_IDX;
	ASSERT(idx <= MAX_CORE_IDX);
	wlc_stf_spatial_map(wlc, idx);
#endif /* WL11N */
	return spatial_map;
}
