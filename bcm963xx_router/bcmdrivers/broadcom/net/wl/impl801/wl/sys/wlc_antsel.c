/*
 * wlc_antsel.c: Antenna Selection code
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_antsel.c,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */

#include <wlc_cfg.h>

#ifdef WLANTSEL

#include <typedefs.h>
#include <qmath.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wlc_phy_radio.h>
#include <bitfuncs.h>

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wl_dbg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>
#include <wlc_antsel.h>

/* iovar table */
enum {
	IOV_ANTSEL_11N,
	IOV_ANTSEL_11N_OVR,	/* antsel override */
};

static const bcm_iovar_t antsel_iovars[] = {
	{"phy_antsel", IOV_ANTSEL_11N, (0), IOVT_BUFFER, sizeof(wlc_antselcfg_t)},
	{"phy_antsel_override", IOV_ANTSEL_11N_OVR, (IOVF_SET_DOWN), IOVT_INT8, 0},
	{NULL, 0, 0, 0, 0}
};

/* useful macros */
#define WLC_ANTSEL_11N_0(ant)	((((ant) & ANT_SELCFG_MASK) >> 4) & 0xf)
#define WLC_ANTSEL_11N_1(ant)	(((ant) & ANT_SELCFG_MASK) & 0xf)
#define WLC_ANTIDX_11N(ant)	(((WLC_ANTSEL_11N_0(ant)) << 2) + (WLC_ANTSEL_11N_1(ant)))
#define WLC_ANT_ISAUTO_11N(ant)	(((ant) & ANT_SELCFG_AUTO) == ANT_SELCFG_AUTO)
#define WLC_ANTSEL_11N(ant)	((ant) & ANT_SELCFG_MASK)

/* antenna switch */
/* defines for no boardlevel antenna diversity */
#define ANT_SELCFG_DEF_2x2	0x01	/* default antenna configuration */

/* 2x3 antdiv defines and tables for GPIO communication */
#define ANT_SELCFG_NUM_2x3	3
#define ANT_SELCFG_DEF_2x3	0x01	/* default antenna configuration */

/* 2x4 antdiv rev4 defines and tables for GPIO communication */
#define ANT_SELCFG_NUM_2x4	4
#define ANT_SELCFG_DEF_2x4	0x02	/* default antenna configuration */

/* static functions */
static int wlc_antsel_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_antsel_cfgupd(antsel_info_t *asi, wlc_antselcfg_t *antsel);
static uint8 wlc_antsel_id2antcfg(antsel_info_t *asi, uint8 id);
static uint16 wlc_antsel_antcfg2antsel(antsel_info_t *asi, uint8 ant_cfg);
static void wlc_antsel_init_cfg(antsel_info_t *asi, wlc_antselcfg_t *antsel,
	bool auto_sel);
static int wlc_antsel(antsel_info_t *asi, wlc_antselcfg_t *antsel);
static bool wlc_antsel_cfgverify(antsel_info_t *asi, uint8 antcfg);
static int wlc_antsel_cfgcheck(antsel_info_t *asi, wlc_antselcfg_t *antsel,
	uint8 uni, uint8 def);

const uint16 mimo_2x4_div_antselpat_tbl[] = {
	0, 0, 0x9, 0xa, /* ant0: 0 ant1: 2,3 */
	0, 0, 0x5, 0x6, /* ant0: 1 ant1: 2,3 */
	0, 0, 0,   0,   /* n.a.              */
	0, 0, 0,   0    /* n.a.              */
};

const uint8 mimo_2x4_div_antselid_tbl[16] = {
	0, 0, 0, 0, 0, 2, 3, 0,
	0, 0, 1, 0, 0, 0, 0, 0 /* pat to antselid */
};

const uint16 mimo_2x3_div_antselpat_tbl[] = {
	16,  0,  1, 16, /* ant0: 0 ant1: 1,2 */
	16, 16, 16, 16, /* n.a.              */
	16,  2, 16, 16, /* ant0: 2 ant1: 1   */
	16, 16, 16, 16  /* n.a.              */
};

const uint8 mimo_2x3_div_antselid_tbl[16] = {
	0, 1, 2, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0 /* pat to antselid */
};

uint8
wlc_antsel_antseltype_get(antsel_info_t *asi)
{
	return (asi->antsel_type);
}

antsel_info_t *
BCMNMIATTACHFN(wlc_antsel_attach)(wlc_info_t *wlc, osl_t *osh, wlc_pub_t *pub,
                                  wlc_hw_info_t *wlc_hw)
{
	antsel_info_t *asi;

	if (!(asi = (antsel_info_t *)MALLOC(osh, sizeof(antsel_info_t)))) {
		WL_ERROR(("wl%d: wlc_antsel_attach: out of mem, malloced %d bytes\n",
			pub->unit, MALLOCED(osh)));
		return NULL;
	}

	bzero((char *)asi, sizeof(antsel_info_t));

	asi->wlc = wlc;
	asi->pub = pub;
	asi->antsel_type = ANTSEL_NA;
	asi->antsel_avail = FALSE;
	asi->antsel_antswitch = (uint8)getintvar(asi->pub->vars, "antswitch");

	if ((asi->pub->sromrev >= 4) && (asi->antsel_antswitch != 0)) {
		switch (asi->antsel_antswitch) {
		case ANTSWITCH_TYPE_1:
		case ANTSWITCH_TYPE_2:
		case ANTSWITCH_TYPE_3:
			/* 4321/2 board with 2x3 switch logic */
			asi->antsel_type = ANTSEL_2x3;
			/* Antenna selection availability */
			if (((uint16)getintvar(asi->pub->vars, "aa2g") == 7) ||
			    ((uint16)getintvar(asi->pub->vars, "aa5g") == 7)) {
				asi->antsel_avail = TRUE;
			} else if (((uint16)getintvar(asi->pub->vars, "aa2g") == 3) ||
				((uint16)getintvar(asi->pub->vars, "aa5g") == 3)) {
				asi->antsel_avail = FALSE;
			} else {
				asi->antsel_avail = FALSE;
				WL_ERROR(("wlc_antsel_attach: 2o3 board cfg invalid\n"));
				ASSERT(0);
			}
			break;
		default:
			break;
		}
	} else if ((asi->pub->sromrev == 4) &&
		((uint16)getintvar(asi->pub->vars, "aa2g") == 7) &&
		((uint16)getintvar(asi->pub->vars, "aa5g") == 0)) {
		/* hack to match old 4321CB2 cards with 2of3 antenna switch */
		asi->antsel_type = ANTSEL_2x3;
		asi->antsel_avail = TRUE;
	} else if (asi->pub->boardflags2 & BFL2_2X4_DIV) {
		asi->antsel_type = ANTSEL_2x4;
		asi->antsel_avail = TRUE;
	}

	/* Set the antenna selection type for the low driver */
	wlc_bmac_antsel_type_set(wlc_hw, asi->antsel_type);

	/* Init (auto/manual) antenna selection */
	wlc_antsel_init_cfg(asi, &asi->antcfg_11n, TRUE);
	wlc_antsel_init_cfg(asi, &asi->antcfg_cur, TRUE);

	/* register module */
	if (wlc_module_register(asi->pub, antsel_iovars, "antsel",
		asi, wlc_antsel_doiovar, NULL, NULL)) {
		WL_ERROR(("wl%d: antsel wlc_module_register() failed\n",
			pub->unit));
		goto fail;
	}

	return asi;

fail:
	MFREE(osh, asi, sizeof(antsel_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_antsel_detach)(antsel_info_t *asi)
{
	if (!asi)
		return;

	wlc_module_unregister(asi->pub, "antsel", asi);
	MFREE(asi->pub->osh, asi, sizeof(antsel_info_t));
}

void
wlc_antsel_init(antsel_info_t *asi)
{
	if ((asi->antsel_type == ANTSEL_2x3) ||
	    (asi->antsel_type == ANTSEL_2x4))
		wlc_antsel_cfgupd(asi, &asi->antcfg_11n);
}

/* handle antsel related iovars */
static int
wlc_antsel_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	antsel_info_t *asi = (antsel_info_t *)hdl;
	wlc_pub_t *pub = asi->pub;
	int32 int_val = 0;
	int err = 0;

	if ((err = wlc_iovar_check(pub, vi, a, alen, IOV_ISSET(actionid))) != 0)
		return err;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_SVAL(IOV_ANTSEL_11N):
		/* antenna diversity control */
		err = wlc_antsel(asi, (wlc_antselcfg_t *)a);
		break;

	case IOV_GVAL(IOV_ANTSEL_11N): {
		uint8 i;

		/* report currently used antenna configuration */
		for (i = ANT_SELCFG_TX_UNICAST; i < ANT_SELCFG_MAX; i++) {
			if (WLC_ANT_ISAUTO_11N(asi->antcfg_11n.ant_config[i]))
				asi->antcfg_11n.ant_config[i] = ANT_SELCFG_AUTO |
					asi->antcfg_cur.ant_config[i];
		}

		bcopy((char*)&asi->antcfg_11n, (char*)a, sizeof(wlc_antselcfg_t));

		break;
	}

	case IOV_SVAL(IOV_ANTSEL_11N_OVR):	/* antenna selection override */
		if (int_val == -1) {
			/* no override, use auto selection if available, else use default */
			wlc_antselcfg_t at;
			wlc_antsel_init_cfg(asi, &at, TRUE);
			err = wlc_antsel(asi, &at);
			break;
		} else if (int_val == 0) {
			/* override to use fixed antennas, no auto selection */
			wlc_antselcfg_t at;
			wlc_antsel_init_cfg(asi, &at, FALSE);
			err = wlc_antsel(asi, &at);
		}
		break;

	default:
		err = BCME_UNSUPPORTED;
	}
	return err;
}

/* boardlevel antenna selection: helper function to validate ant_cfg specification */
static bool
wlc_antsel_cfgverify(antsel_info_t *asi, uint8 antcfg)
{
	uint8 idx = WLC_ANTIDX_11N(antcfg);

	if (asi->antsel_type == ANTSEL_2x3) {
		if ((idx >= ARRAYSIZE(mimo_2x3_div_antselpat_tbl)) ||
		    (mimo_2x3_div_antselpat_tbl[idx] > 15)) {
			return FALSE;
		}

	} else if (asi->antsel_type == ANTSEL_2x4) {
		if ((idx >= ARRAYSIZE(mimo_2x4_div_antselpat_tbl)) ||
		    ((mimo_2x4_div_antselpat_tbl[idx] & 0xf) == 0)) {
			return FALSE;
		}

	} else {
		ASSERT(0);
		return FALSE;
	}

	return TRUE;
}

/* boardlevel antenna selection: validate ant_cfg specification */
static int
wlc_antsel_cfgcheck(antsel_info_t *asi, wlc_antselcfg_t *antsel, uint8 uni_ant, uint8 def_ant)
{
	uint8 def_ant_auto = 0, uni_ant_auto = 0;
	bool uni_auto, def_auto;

	uni_auto = (antsel->ant_config[uni_ant] == (uint8)AUTO);
	def_auto = (antsel->ant_config[def_ant] == (uint8)AUTO);

	/* check if auto selection enable and dependencies */
	if (uni_auto && def_auto) {
		def_ant_auto = ANT_SELCFG_AUTO;
		uni_ant_auto = ANT_SELCFG_AUTO;
	} else if (uni_auto && !def_auto) {
		if (!wlc_antsel_cfgverify(asi, antsel->ant_config[def_ant]))
			return BCME_RANGE;
		uni_ant_auto = ANT_SELCFG_AUTO;
	} else if (!uni_auto && !def_auto) {
		if (!wlc_antsel_cfgverify(asi, antsel->ant_config[uni_ant]))
			return BCME_RANGE;
		if (!wlc_antsel_cfgverify(asi, antsel->ant_config[def_ant]))
			return BCME_RANGE;
	} else
		return BCME_RANGE;

	if (uni_ant_auto)
		antsel->ant_config[uni_ant] = (asi->antcfg_11n.ant_config[uni_ant] &
			ANT_SELCFG_MASK) | uni_ant_auto;

	if (def_ant_auto)
		antsel->ant_config[def_ant] = (asi->antcfg_11n.ant_config[def_ant] &
			ANT_SELCFG_MASK) | def_ant_auto;

	return BCME_OK;
}

/* boardlevel antenna selection: init antenna selection structure */
static void
wlc_antsel_init_cfg(antsel_info_t *asi, wlc_antselcfg_t *antsel, bool auto_sel)
{
	if (asi->antsel_type == ANTSEL_2x3) {
		uint8 antcfg_def = ANT_SELCFG_DEF_2x3 |
			((asi->antsel_avail && auto_sel) ? ANT_SELCFG_AUTO : 0);
		antsel->ant_config[ANT_SELCFG_TX_DEF] = antcfg_def;
		antsel->ant_config[ANT_SELCFG_TX_UNICAST] = antcfg_def;
		antsel->ant_config[ANT_SELCFG_RX_DEF] = antcfg_def;
		antsel->ant_config[ANT_SELCFG_RX_UNICAST] = antcfg_def;
		antsel->num_antcfg = ANT_SELCFG_NUM_2x3;

	} else if (asi->antsel_type == ANTSEL_2x4) {

		antsel->ant_config[ANT_SELCFG_TX_DEF] = ANT_SELCFG_DEF_2x4;
		antsel->ant_config[ANT_SELCFG_TX_UNICAST] = ANT_SELCFG_DEF_2x4;
		antsel->ant_config[ANT_SELCFG_RX_DEF] = ANT_SELCFG_DEF_2x4;
		antsel->ant_config[ANT_SELCFG_RX_UNICAST] = ANT_SELCFG_DEF_2x4;
		antsel->num_antcfg = ANT_SELCFG_NUM_2x4;

	} else {	/* no antenna selection available */

		antsel->ant_config[ANT_SELCFG_TX_DEF] = ANT_SELCFG_DEF_2x2;
		antsel->ant_config[ANT_SELCFG_TX_UNICAST] = ANT_SELCFG_DEF_2x2;
		antsel->ant_config[ANT_SELCFG_RX_DEF] = ANT_SELCFG_DEF_2x2;
		antsel->ant_config[ANT_SELCFG_RX_UNICAST] = ANT_SELCFG_DEF_2x2;
		antsel->num_antcfg = 0;
	}
}

/* boardlevel antenna selection: configure manual/auto antenna selection */
static int
wlc_antsel(antsel_info_t *asi, wlc_antselcfg_t *antsel)
{
	int i;

	if ((asi->antsel_type == ANTSEL_NA) || (asi->antsel_avail == FALSE))
		return BCME_UNSUPPORTED;

	if (wlc_antsel_cfgcheck(asi, antsel, ANT_SELCFG_TX_UNICAST, ANT_SELCFG_TX_DEF) < 0) {
		WL_ERROR(("wl%d:wlc_antsel: Illegal TX antenna configuration 0x%02x\n",
			asi->pub->unit, antsel->ant_config[ANT_SELCFG_TX_DEF]));
		return BCME_RANGE;
	}

	if (wlc_antsel_cfgcheck(asi, antsel, ANT_SELCFG_RX_UNICAST, ANT_SELCFG_RX_DEF) < 0) {
		WL_ERROR(("wl%d:wlc_antsel: Illegal RX antenna configuration 0x%02x\n",
			asi->pub->unit, antsel->ant_config[ANT_SELCFG_RX_DEF]));
		return BCME_RANGE;
	}

	/* check if down when user enables/disables antenna selection algorithms */
	if (asi->pub->up) {
		for (i = ANT_SELCFG_TX_UNICAST; i < ANT_SELCFG_MAX; i++) {
			if (((asi->antcfg_11n.ant_config[i] & ANT_SELCFG_AUTO) == 0) &&
			    ((antsel->ant_config[i] & ANT_SELCFG_AUTO) == ANT_SELCFG_AUTO)) {
				return BCME_NOTDOWN;
			}
			if (((asi->antcfg_11n.ant_config[i] & ANT_SELCFG_AUTO)) &&
			    ((antsel->ant_config[i] & ANT_SELCFG_AUTO) == ANT_SELCFG_AUTO) == 0) {
				return BCME_NOTDOWN;
			}
		}
	}

	for (i = ANT_SELCFG_TX_UNICAST; i < ANT_SELCFG_MAX; i++)
		asi->antcfg_11n.ant_config[i] = antsel->ant_config[i];

	if (asi->pub->up)
		wlc_antsel_cfgupd(asi, &asi->antcfg_11n);

	wlc_scb_ratesel_init_all(asi->wlc);

	return BCME_OK;
}

uint16
wlc_antsel_buildtxh(antsel_info_t *asi, uint8 antcfg, uint8 fbantcfg)
{
	return ((wlc_antsel_antcfg2antsel(asi, antcfg)) |
		(wlc_antsel_antcfg2antsel(asi, fbantcfg) << 8));
}

void
wlc_antsel_ratesel(antsel_info_t *asi, uint8 *active_antcfg_num, uint8 *antselid_init)
{
	uint8 ant;

	ant = asi->antcfg_11n.ant_config[ANT_SELCFG_TX_UNICAST];

	if ((ant & ANT_SELCFG_AUTO) == ANT_SELCFG_AUTO) {
		*active_antcfg_num = asi->antcfg_11n.num_antcfg;
		*antselid_init = 0;
	} else {
		*active_antcfg_num = 0;
		*antselid_init = 0;
	}
}

void
wlc_antsel_set_unicast(antsel_info_t *asi, uint8 antcfg)
{
	asi->antcfg_cur.ant_config[ANT_SELCFG_TX_UNICAST] = antcfg;
}

void
wlc_antsel_antcfg_get(antsel_info_t *asi, bool usedef, bool sel, uint8 antselid,
	uint8 fbantselid, uint8 *antcfg, uint8 *fbantcfg)
{
	uint8 ant;

	/* if use default, assign it and return */
	if (usedef) {
		*antcfg = asi->antcfg_11n.ant_config[ANT_SELCFG_TX_DEF];
		*fbantcfg = *antcfg;
		return;
	}

	if (!sel) {
		*antcfg = asi->antcfg_11n.ant_config[ANT_SELCFG_TX_UNICAST];
		*fbantcfg = *antcfg;

	} else {
		ant = asi->antcfg_11n.ant_config[ANT_SELCFG_TX_UNICAST];
		if ((ant & ANT_SELCFG_AUTO) == ANT_SELCFG_AUTO) {
			*antcfg = wlc_antsel_id2antcfg(asi, antselid);
			*fbantcfg = wlc_antsel_id2antcfg(asi, fbantselid);
		} else {
			*antcfg = asi->antcfg_11n.ant_config[ANT_SELCFG_TX_UNICAST];
			*fbantcfg = *antcfg;
		}
	}
	return;
}

/* boardlevel antenna selection: update "default" (multi-/broadcast) antenna cfg */
void
wlc_antsel_upd_dflt(antsel_info_t *asi, uint8 antselid)
{
	wlc_antselcfg_t antsel;
	uint8 i;

	ASSERT(asi->antsel_type != ANTSEL_NA);

	/* check if default selection is enabled. if yes, update default */
	for (i = ANT_SELCFG_RX_UNICAST; i < ANT_SELCFG_MAX; i++) {
		if (WLC_ANT_ISAUTO_11N(asi->antcfg_11n.ant_config[i]))
			antsel.ant_config[i] = wlc_antsel_id2antcfg(asi, antselid);
		else
			antsel.ant_config[i] = asi->antcfg_11n.ant_config[i];
	}
	wlc_antsel_cfgupd(asi, &antsel);
}

/* boardlevel antenna selection: convert mimo_antsel (ucode interface) to id */
uint8
wlc_antsel_antsel2id(antsel_info_t *asi, uint16 antsel)
{
	uint8 antselid = 0;

	if (asi->antsel_type == ANTSEL_2x4) {
		/* 2x4 antenna diversity board, 4 cfgs: 0-2 0-3 1-2 1-3 */
		antselid = mimo_2x4_div_antselid_tbl[(antsel & 0xf)];
		return antselid;

	} else if (asi->antsel_type == ANTSEL_2x3) {
		/* 2x3 antenna selection, 3 cfgs: 0-1 0-2 2-1 */
		antselid = mimo_2x3_div_antselid_tbl[(antsel & 0xf)];
		return antselid;
	}

	return antselid;
}

/* boardlevel antenna selection: convert id to ant_cfg */
static uint8
wlc_antsel_id2antcfg(antsel_info_t *asi, uint8 id)
{
	uint8 antcfg = ANT_SELCFG_DEF_2x2;

	if (asi->antsel_type == ANTSEL_2x4) {
		/* 2x4 antenna diversity board, 4 cfgs: 0-2 0-3 1-2 1-3 */
		antcfg = (((id & 0x2) << 3) | ((id & 0x1) + 2));
		return antcfg;

	} else if (asi->antsel_type == ANTSEL_2x3) {
		/* 2x3 antenna selection, 3 cfgs: 0-1 0-2 2-1 */
		antcfg = (((id & 0x02) << 4) | ((id & 0x1) + 1));
		return antcfg;
	}

	return antcfg;
}

/* boardlevel antenna selection: convert ant_cfg to mimo_antsel (ucode interface) */
static uint16
wlc_antsel_antcfg2antsel(antsel_info_t *asi, uint8 ant_cfg)
{
	uint8 idx = WLC_ANTIDX_11N(WLC_ANTSEL_11N(ant_cfg));
	uint16 mimo_antsel = 0;

	if (asi->antsel_type == ANTSEL_2x4) {
		/* 2x4 antenna diversity board, 4 cfgs: 0-2 0-3 1-2 1-3 */
		mimo_antsel = (mimo_2x4_div_antselpat_tbl[idx] & 0xf);
		return mimo_antsel;

	} else if (asi->antsel_type == ANTSEL_2x3) {
		/* 2x3 antenna selection, 3 cfgs: 0-1 0-2 2-1 */
		mimo_antsel = (mimo_2x3_div_antselpat_tbl[idx] & 0xf);
		return mimo_antsel;
	}

	return mimo_antsel;
}

/* boardlevel antenna selection: ucode interface control */
static int
wlc_antsel_cfgupd(antsel_info_t *asi, wlc_antselcfg_t *antsel)
{
	wlc_info_t *wlc = asi->wlc;
	uint8 ant_cfg;
	uint16 mimo_antsel;

	ASSERT(asi->antsel_type != ANTSEL_NA);

	/* 1) Update TX antconfig for all frames that are not unicast data
	 *    (aka default TX)
	 */
	ant_cfg = antsel->ant_config[ANT_SELCFG_TX_DEF];
	mimo_antsel = wlc_antsel_antcfg2antsel(asi, ant_cfg);
	wlc_write_shm(wlc, M_MIMO_ANTSEL_TXDFLT, mimo_antsel);
	/* Update driver stats for currently selected default tx/rx antenna config */
	asi->antcfg_cur.ant_config[ANT_SELCFG_TX_DEF] = ant_cfg;

	/* 2) Update RX antconfig for all frames that are not unicast data
	 *    (aka default RX)
	 */
	ant_cfg = antsel->ant_config[ANT_SELCFG_RX_DEF];
	mimo_antsel = wlc_antsel_antcfg2antsel(asi, ant_cfg);
	wlc_write_shm(wlc, M_MIMO_ANTSEL_RXDFLT, mimo_antsel);
	/* Update driver stats for currently selected default tx/rx antenna config */
	asi->antcfg_cur.ant_config[ANT_SELCFG_RX_DEF] = ant_cfg;

	return 0;
}

#endif /* WLANTSEL */
