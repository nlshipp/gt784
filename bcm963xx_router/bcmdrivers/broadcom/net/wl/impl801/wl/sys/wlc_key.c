/*
 * Key Management routines for
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
 * $Id: wlc_key.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#ifndef LINUX_CRYPTO
#include <bcmcrypto/rc4.h>
#include <bcmcrypto/tkhash.h>
#else
#define RC4_STATE_NBYTES	256
#endif /* LINUX_CRYPTO */
#include <proto/802.11.h>
#include <proto/ethernet.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_security.h>
#include <wl_export.h>
#ifdef BCMSUP_PSK
#include <wlc_sup.h>
#endif /* BCMSUP_PSK */
#include <wlc_extlog.h>
#ifdef WLP2P
#include <wlc_p2p.h>
#endif

static int wlc_key_allocate(wlc_info_t *wlc, int start, int total, int slots);

static void wlc_key_reset(wlc_info_t *wlc, wsec_key_t *key);
static void wlc_key_hw_reallocate(wlc_info_t *wlc, int hw_index, struct wlc_bsscfg *bsscfg);
static void wlc_key_hw_clear_all(wlc_info_t *wlc);
static void wlc_key_write_addr(wlc_info_t *wlc, int i, const struct ether_addr *ea);

#ifdef BCMWAPI_WPI
static void wlc_wapi_key_iv_init(wlc_info_t *wlc, wsec_key_t *key, wsec_iv_t *initial_iv);
static void _wlc_wapi_key_iv_update(wsec_key_t *key, uchar *buf, bool update);
#endif /* BCMWAPI_WPI */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static const char *wsec_algo_names[] = {
	"NONE",
	"WEP1",
	"TKIP",
	"AES",
	"WEP128",
	"AES_LEGACY",
	"SMS4",
	"NALG"
};
#endif /* BCMDBG || BCMDBG_DUMP */

static wsec_key_t *
wlc_key_allocate_ex(wlc_info_t *wlc, int idx)
{
	wsec_key_t *key;

	ASSERT(wlc->wsec_keys[idx] == NULL);
	if ((key = MALLOC(wlc->osh, sizeof(wsec_key_t))) != NULL) {
		bzero(key, sizeof(wsec_key_t));
		key->idx = (uint8)idx;
#ifdef WLP2P
		key->rcmta = RCMTA_SIZE;
#endif
	}
	wlc->wsec_keys[idx] = key;
	return key;
}

/*
 * Find N free key slot(s) in the wsec_keys array, and allocate N new key entry(s).
 */
static int
wlc_key_allocate(wlc_info_t *wlc, int start, int total, int slots)
{
	int end;
	int i, j;

	ASSERT(total >= 1);
	ASSERT(slots >= 1);
	ASSERT(slots <= total);

	end = start + total - 1;

	/* find one or more consecutive free key slots */
	for (i = start; i <= end; i ++) {
		if (wlc->wsec_keys[i] != NULL)
			continue;
		for (j = 1; j < slots && i + j <= end; j ++) {
			if (wlc->wsec_keys[i + j] != NULL)
				break;
		}
		if (j == slots)
			break;
		i += j;
	}
	if (i > end) {
		WL_WSEC(("wl%d: out of wsec_keys entries\n", wlc->pub->unit));
		return BCME_NORESOURCE;
	}

	for (j = 0; j < slots; j ++) {
		if (wlc_key_allocate_ex(wlc, i + j) == NULL) {
			WL_ERROR(("wl%d: wlc_key_allocate: out of memory, malloced %d bytes\n",
			          wlc->pub->unit, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
	}

	return i;
}

#ifdef BCMWAPI_WPI
static bool
wlc_key_rotation_wapi_update(wlc_info_t *wlc, struct scb *scb, uint32 key_algo, uint32 key_id);
static bool
wlc_key_rotation_wapi_update(wlc_info_t *wlc, struct scb *scb, uint32 key_algo, uint32 key_id)
{
	wsec_key_t *key = scb->key;
	if ((key->algo == key_algo) && (key->algo == CRYPTO_ALGO_SMS4)) {
		if (key->id != key_id) {
			/* save the key until AP starts using the new Key */
			if (scb->prev_key)
				wlc_key_delete(wlc, scb->bsscfg, scb->prev_key->idx);

			scb->prev_key_valid_time = wlc->pub->now + SMS4_OLD_KEY_MAXVALIDTIME;
			scb->prev_key = scb->key;
			scb->key = NULL;
			return TRUE;
		}
	}
	return FALSE;
}
#endif /* BCMWAPI_WPI */

/* wsec_key_t as of now is all flat so a bcopy does it all. */
static void
wlc_key_copy(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wsec_key_t *src, wsec_key_t *dst)
{
	ASSERT(src != NULL && dst != NULL);

	if (src->len > 0) {
		int dst_idx = WSEC_KEY_INDEX(wlc, dst);
		bcopy(src, dst, sizeof(wsec_key_t));
		dst->idx = (uint8)dst_idx;
		wlc_key_update(wlc, dst_idx, cfg);
	}
	/* src->len == 0 is unlikely!
	 * ASSERT(src->len > 0);
	 */
}

/* wsec_key_t as of now is all flat so a bcopy does it all. */
static void
wlc_key_move(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wsec_key_t *src, wsec_key_t *dst)
{
	ASSERT(src != NULL && dst != NULL);

	if (src->len > 0) {
		int dst_idx = WSEC_KEY_INDEX(wlc, dst);
		int src_idx = WSEC_KEY_INDEX(wlc, src);
		bcopy(src, dst, sizeof(wsec_key_t));
		dst->idx = (uint8)dst_idx;
		wlc_key_update(wlc, dst_idx, cfg);
		wlc_key_delete(wlc, cfg, src_idx);
	}
	/* src->len == 0 is unlikely!
	 * ASSERT(src->len > 0);
	 */
}



#define WLC_STA_GROUP_KEYS	2

static int
wlc_get_new_key(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint32 key_flags, struct ether_addr *key_ea)
{
	int wsec_idx;
	int max_keys =
	        cfg->BSS ?
	        WLC_MAX_WSEC_KEYS(wlc) - WSEC_MAX_DEFAULT_KEYS :
	        WSEC_IBSS_MAX_PEERS;
	uint grpkeys = 0;

#ifdef WLP2P
	/* allocate contiguous key slots for non-primary STA */
	grpkeys =
	        (P2P_ENAB(wlc->pub) && cfg != wlc->cfg && BSSCFG_STA(cfg)) ?
	        (!(cfg->flags & WLC_BSSCFG_NOBCMC) ?
	         WLC_STA_GROUP_KEYS :
	         0) :
	        0;
#endif /* WLP2P */

	/* Get new keys with the first key's index at 'wsec_idx'. */
	if ((wsec_idx = wlc_key_allocate(wlc, WSEC_MAX_DEFAULT_KEYS, max_keys, 1 + grpkeys)) < 0)
		return wsec_idx;

#if defined(IBSS_PEER_GROUP_KEY)
	/* [N.B.: If this is the IBSS peer's first key, the code above
	 *        allocates a unicast key to reserve the wsec_keys slots.]
	 */
	if (key_flags & WL_IBSS_PEER_GROUP_KEY)
		bcopy((char*)key_ea, (char*)&wlc->wsec_keys[wsec_idx]->ea, ETHER_ADDR_LEN);
#else
#ifdef WLP2P
	/* init bss_def_keys with pre-allocated group keys for non-primary STA */
	if (P2P_ENAB(wlc->pub) && cfg != wlc->cfg && BSSCFG_STA(cfg)) {
		wsec_key_t *key = wlc->wsec_keys[wsec_idx];
		wsec_key_t *grp;
		uint i;

		/* transfer the existing group key when it was set (i.e. static WEP key):
		 * - copy it to the unicast key slot so that the ucode can use
		 *   TA match for unicast frame decryption
		 * - move it to the new bcmc key slot
		 */
		for (i = 0; i < grpkeys; i ++) {
			wsec_key_t *wep = cfg->bss_def_keys[i];

			grp = wlc->wsec_keys[wsec_idx + 1 + i];

			if (wep != NULL) {
				int wsec_index = cfg->wsec_index;

				wlc_key_copy(wlc, cfg, wep, key);
				wlc_key_move(wlc, cfg, wep, grp);
				cfg->wsec_index = wsec_index;
			}

			cfg->bss_def_keys[i] = grp;
		}
	}
#endif /* WLP2P */
#endif /* defined(IBSS_PEER_GROUP_KEY) */
	return wsec_idx;
}


int
wlc_rcmta_add_bssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#ifdef WLP2P
	if (P2P_ENAB(wlc->pub)) {
	WL_WSEC(("wl%d.%d: %s:\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));

	ASSERT(cfg != wlc->cfg && BSSCFG_STA(cfg) && cfg->BSS);

	if (ETHER_ISNULLADDR(&cfg->BSSID)) {
		WL_ERROR(("wl%d.%d: %s: BSSID is empty\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return BCME_ERROR;
	}

	if (cfg->rcmta_bssid_idx < RCMTA_SIZE) {
		WL_ERROR(("wl%d.%d: %s: RCMTA entry %u has been allocated\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
		          cfg->rcmta_bssid_idx));
		return BCME_ERROR;
	}

	if (wlc_p2p_d11ra_alloc(wlc->p2p, &cfg->rcmta_bssid_idx) != BCME_OK) {
		WL_ERROR(("wl%d.%d: %s: fail to allocate RCMTA entry\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return BCME_ERROR;
	}

	if (wlc->pub->up) {
		uint16 mask = ADDR_BMP_TA|ADDR_BMP_BSSID;
		uint16 val = ADDR_BMP_TA|ADDR_BMP_BSSID;
		wlc_set_rcmta(wlc, cfg->rcmta_bssid_idx, &cfg->BSSID);
		wlc_set_rcmta_type(wlc, cfg->rcmta_bssid_idx, mask, val);
	}
	}
	else
#endif /* WLP2P */
	{
	/* don't expect this flag ever change during the life time of the bsscfg */
	uint grpkeys =
	        (cfg->wsec == 0 || (cfg->flags & WLC_BSSCFG_NOBCMC)) ? 0 : WLC_STA_GROUP_KEYS;
	int wsec_idx = 0;
	wsec_key_t *key;
	wsec_key_t *grpkey;
	uint i;

	WL_WSEC(("wl%d.%d: %s: rcmta %p\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, cfg->rcmta));

	ASSERT(cfg != wlc->cfg && BSSCFG_STA(cfg) && cfg->BSS);
	ASSERT(grpkeys <= ARRAYSIZE(cfg->bss_def_keys));

	if (ETHER_ISNULLADDR(&cfg->BSSID))
		return BCME_ERROR;

	/* if the rcmta entry exists just go update the TA */
	if ((key = cfg->rcmta) != NULL) {
		wsec_idx = WSEC_KEY_INDEX(wlc, key);
		goto hw_upd;
	}

	/* allocate one or more consecutive new key slot(s) */
	if ((wsec_idx = wlc_key_allocate(wlc, WSEC_MAX_DEFAULT_KEYS,
	                WLC_MAX_WSEC_KEYS(wlc) - WSEC_MAX_DEFAULT_KEYS,
	                1 + grpkeys)) < 0)
		return BCME_ERROR;

	cfg->rcmta = key = wlc->wsec_keys[wsec_idx];

	/* don't expect this flag ever change during the life time of the bsscfg */
	if (cfg->flags & WLC_BSSCFG_NOBCMC)
		goto hw_upd;

	/* init cfg->bss_def_keys and transfer the existing group key
	 * when it was set (i.e. static WEP key):
	 * - copy it to the unicast key slot so that the ucode can use
	 *   TA match for unicast frame decryption
	 * - move it to the new bcmc key slot
	 */
	for (i = 0; i < grpkeys; i ++) {
		wsec_key_t *wep = cfg->bss_def_keys[i];
		grpkey = wlc->wsec_keys[wsec_idx + 1 + i];
		grpkey->idx = (uint8)(wsec_idx + 1 + i);
		if (wep != NULL) {
			int wsec_index = cfg->wsec_index;
			wlc_key_copy(wlc, cfg, wep, key);
			wlc_key_move(wlc, cfg, wep, grpkey);
			cfg->wsec_index = wsec_index;
		}
		cfg->bss_def_keys[i] = grpkey;
	}

hw_upd:
	bcopy(&cfg->BSSID, &key->ea, ETHER_ADDR_LEN);

	/* shared by BSSID match and wsec key */
	if (key->len > 0)
		wlc_key_update(wlc, wsec_idx, cfg);
	/* used by BSSID match only */
	else if (wlc->pub->up)
		wlc_key_write_addr(wlc, wsec_idx, &key->ea);

	/* enable BSSID filter */
	wlc_mhf(wlc, MHF4, MHF4_RCMTA_BSSID_EN, MHF4_RCMTA_BSSID_EN, WLC_BAND_ALL);

	}
	return BCME_OK;
}

void
wlc_rcmta_del_bssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#ifdef WLP2P
	if (P2P_ENAB(wlc->pub)) {
	uint i;

	WL_WSEC(("wl%d.%d: %s: rcmta_bssid_idx %u\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
	         cfg->rcmta_bssid_idx));

	ASSERT(cfg != wlc->cfg && BSSCFG_STA(cfg) && cfg->BSS);

	if (cfg->rcmta_bssid_idx >= RCMTA_SIZE) {
		WL_ERROR(("wl%d.%d: %s: no RCMTA entry has been allocate\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}

	if (wlc->pub->up) {
		uint16 mask = ADDR_BMP_TA|ADDR_BMP_BSSID;
		wlc_set_rcmta(wlc, cfg->rcmta_bssid_idx, &ether_null);
		wlc_set_rcmta_type(wlc, cfg->rcmta_bssid_idx, mask, 0);

		wlc_p2p_d11ra_free(wlc->p2p, cfg->rcmta_bssid_idx);
		cfg->rcmta_bssid_idx = RCMTA_SIZE;
	}

	for (i = 0; i < ARRAYSIZE(cfg->bss_def_keys); i ++) {
		wsec_key_t *key = cfg->bss_def_keys[i];

		if (key != NULL) {
			int wsec_idx = WSEC_KEY_INDEX(wlc, key);

			WL_WSEC(("wl%d.%d: %s: deleting group key %d ID %d\n",
			         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			         i, key->id));
			MFREE(wlc->osh, key, sizeof(wsec_key_t));
			wlc->wsec_keys[wsec_idx] = NULL;
			cfg->bss_def_keys[i] = NULL;
		}
	}
	}
	else
#endif /* WLP2P */
	{
	int wsec_idx;
	wsec_key_t *key;
	uint i;
	wlc_bsscfg_t *bc;

	WL_WSEC(("wl%d.%d: %s: rcmta %p\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, cfg->rcmta));

	ASSERT(cfg != wlc->cfg && BSSCFG_STA(cfg) && cfg->BSS);

	if ((key = cfg->rcmta) != NULL) {
		wsec_idx = WSEC_KEY_INDEX(wlc, key);
		/* shared by BSSID match and wsec key */
		if (key->len > 0)
			wlc_key_delete(wlc, cfg, wsec_idx);
		/* used by BSSID match only */
		else {
			MFREE(wlc->osh, key, sizeof(wsec_key_t));
			wlc->wsec_keys[wsec_idx] = NULL;
			cfg->rcmta = NULL;
			if (wlc->pub->up)
				wlc_key_write_addr(wlc, wsec_idx, &ether_null);
		}
	}

	for (i = 0; i < ARRAYSIZE(cfg->bss_def_keys); i ++) {
		if ((key = cfg->bss_def_keys[i]) != NULL) {
			wsec_idx = WSEC_KEY_INDEX(wlc, key);
			/* shared by BSSID match and wsec key */
			if (key->len > 0)
				wlc_key_delete(wlc, cfg, wsec_idx);
			/* used by BSSID match only */
			else {
				MFREE(wlc->osh, key, sizeof(wsec_key_t));
				wlc->wsec_keys[wsec_idx] = NULL;
				cfg->bss_def_keys[i] = NULL;

			}
		}
	}

	/* disable BSSID filter if this is the last association */
	FOREACH_AS_STA(wlc, i, bc) {
		if (bc->BSS && bc != cfg && bc != wlc->cfg)
			return;
	}
	wlc_mhf(wlc, MHF4, MHF4_RCMTA_BSSID_EN, 0, WLC_BAND_ALL);
	}
}

/*
 * Insert a key.  At exit the key can be used as an rx key
 * but not tx.  wlc_set_crypto_index() needs to be called for that.
 */
int
wlc_key_insert(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 key_len,
	uint32 key_id, uint32 key_flags, uint32 key_algo,
	uint8 *key_data, struct ether_addr *key_ea,
	wsec_iv_t *initial_iv, wsec_key_t **pkey_ptr)
{
	struct scb *scb = NULL;
	struct scb *scb_otherband = NULL;
	wsec_key_t *key;
	int wsec_idx = 0;
	bool newscb = FALSE;
	bool new_key = FALSE;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */
	uint32 def_idx = 0;
#ifdef WLP2P
	uint8 rcmta_idx;
#endif

	WL_WSEC(("wl%d.%d: %s: key ID %d len %d addr %s\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, key_id, key_len,
	         bcm_ether_ntoa(key_ea, eabuf)));

	ASSERT(bsscfg != NULL);
	ASSERT(!ETHER_ISMULTI(key_ea));

	/* check length and IV index */
	if (key_len != WEP1_KEY_SIZE &&
	    key_len != WEP128_KEY_SIZE &&
	    key_len != TKIP_KEY_SIZE &&
#ifdef BCMWAPI_WPI
	    key_len != SMS4_KEY_LEN &&
#endif /* BCMWAPI_WPI */
	    key_len != AES_KEY_SIZE) {
		WL_ERROR(("wl%d.%d: %s: unsupported key size %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, key_len));
		return BCME_BADLEN;
	}
	if (key_id >= WSEC_MAX_DEFAULT_KEYS) {
		WL_ERROR(("wl%d.%d: %s: illegal key index %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, key_id));
		return BCME_BADKEYIDX;
	}
#if defined(IBSS_PEER_GROUP_KEY)
	if ((key_flags & WL_IBSS_PEER_GROUP_KEY) && bsscfg->BSS) {
		WL_ERROR(("wl%d: %s: invalid flags %d in infra mode\n",
		          wlc->pub->unit, __FUNCTION__, key_flags));
		return BCME_BADKEYIDX;
	}
#endif /* defined(IBSS_PEER_GROUP_KEY) */

	/* set a default key */
	if (ETHER_ISNULLADDR(key_ea)) {
		/* If this is the primary(first) BSS config, just use the
		 * key-ID; otherwise the default keys must not be the
		 * hardware default keys.
		 */
		if (bsscfg == wlc->cfg) {
			/*
			 * If the bsscfg key pointer has not already been set, set it to point to
			 * the statically allocated memory here.
			 */
			if (bsscfg->bss_def_keys[key_id] == NULL) {
				bsscfg->bss_def_keys[key_id] = wlc->wsec_def_keys[key_id];
			}
			ASSERT(bsscfg->bss_def_keys[key_id]->idx == key_id);
			def_idx = wsec_idx = key_id;
		}
		else {
			if (BSSCFG_STA(bsscfg) &&
			    WLC_STA_GROUP_KEYS == (WSEC_MAX_DEFAULT_KEYS >> 1))
				def_idx = key_id >> 1;
			else
				def_idx = key_id;

			if (bsscfg->bss_def_keys[def_idx] != NULL) {
				wsec_idx = WSEC_KEY_INDEX(wlc, bsscfg->bss_def_keys[def_idx]);
				ASSERT((wsec_idx >= 0) && (wsec_idx < WLC_MAX_WSEC_KEYS(wlc)));
				ASSERT(bsscfg->bss_def_keys[def_idx] == wlc->wsec_keys[wsec_idx]);
			}
			else {
				int max_keys =
				        bsscfg->BSS ?
				        WLC_MAX_WSEC_KEYS(wlc) - WSEC_MAX_DEFAULT_KEYS :
				        WSEC_IBSS_MAX_PEERS;
				/* Get a new key. */
				if ((wsec_idx = wlc_key_allocate(wlc,
				        WSEC_MAX_DEFAULT_KEYS, max_keys, 1)) < 0)
					return wsec_idx;
				bsscfg->bss_def_keys[def_idx] = wlc->wsec_keys[wsec_idx];
			}
		}

		WL_WSEC(("wl%d.%d: %s: group key index %d\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		         def_idx));
	}
	/* set a per station key */
	else {
		if (bsscfg->associated) {
			uint assoc_bandunit = CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec);
			if (!(scb = wlc_scbfindband(wlc, key_ea, assoc_bandunit))) {
				if (!(scb = wlc_scblookupband(wlc, key_ea, assoc_bandunit))) {
					WL_ERROR(("wl%d: out of scbs\n", wlc->pub->unit));
					return BCME_NOTFOUND;
				}
				newscb = TRUE;
			}
		} else {
			scb = wlc_scblookup(wlc, key_ea);
			if (!scb) {
				WL_ERROR(("wl%d: out of scbs\n", wlc->pub->unit));
				return BCME_NOTFOUND;
			}
			newscb = TRUE;
		}

		if (scb->key) {
			/* replace the existing scb key */
			wsec_idx = WSEC_KEY_INDEX(wlc, scb->key);
			ASSERT((wsec_idx >= WSEC_MAX_DEFAULT_KEYS) &&
			       (wsec_idx < WLC_MAX_WSEC_KEYS(wlc)));
			ASSERT(scb->key == wlc->wsec_keys[wsec_idx]);
		}
		else if (NBANDS(wlc) > 1 &&
		         (scb_otherband = wlc_scbfindband(wlc, key_ea, OTHERBANDUNIT(wlc))) &&
		         scb_otherband->key) {
			/* use the same key for scbs with the same MAC in each band */
			wsec_idx = WSEC_KEY_INDEX(wlc, scb_otherband->key);
			ASSERT((wsec_idx >= WSEC_MAX_DEFAULT_KEYS) &&
			       (wsec_idx < WLC_MAX_WSEC_KEYS(wlc)));
			ASSERT(scb_otherband->key == wlc->wsec_keys[wsec_idx]);
			scb->key = wlc->wsec_keys[wsec_idx];
		}
#ifdef WLP2P
		else if (P2P_ENAB(wlc->pub))
			new_key = TRUE;
#endif
		else if (bsscfg->rcmta != NULL) {
			WL_WSEC(("wl%d: transfer RCMTA BSSID key to scb\n",
			         wlc->pub->unit));
			scb->key = bsscfg->rcmta;
			wsec_idx = WSEC_KEY_INDEX(wlc, scb->key);
			ASSERT((wsec_idx >= WSEC_MAX_DEFAULT_KEYS) &&
			       (wsec_idx < WLC_MAX_WSEC_KEYS(wlc)));
		}
		else
			new_key = TRUE;

#ifdef BCMWAPI_WPI
		if (!new_key)
			new_key = wlc_key_rotation_wapi_update(wlc, scb, key_algo, key_id);
#endif /* BCMWAPI_WPI */

		if (new_key) {
			/* Get a new key. */
			wsec_idx = wlc_get_new_key(wlc, bsscfg, key_flags, key_ea);
			if (wsec_idx < 0)
				return wsec_idx;
			scb->key = wlc->wsec_keys[wsec_idx];
#ifdef WLP2P
			/* init RCMTA entry index with bsscfg specified entry first */
			if (P2P_ENAB(wlc->pub)) {
				key = wlc->wsec_keys[wsec_idx];
				if (bsscfg->rcmta_bssid_idx < RCMTA_SIZE)
					key->rcmta = bsscfg->rcmta_bssid_idx;
				else if (key->idx >= WSEC_MAX_DEFAULT_KEYS)
					key->rcmta = key->idx - WSEC_MAX_DEFAULT_KEYS;
			}
#endif
		}

#if defined(IBSS_PEER_GROUP_KEY)
		/* For IBSS peer group key, map wsec_idx and update scb */
		if (key_flags & WL_IBSS_PEER_GROUP_KEY) {
			wsec_idx += WSEC_IBSS_MAX_PEERS;	/* 1st group key slot */
			if ((wlc->wsec_keys[wsec_idx] != NULL) &&
			    (wlc->wsec_keys[wsec_idx]->id != key_id)) {
				wsec_idx += WSEC_IBSS_MAX_PEERS;	/* 2nd group key slot */
				if ((wlc->wsec_keys[wsec_idx] != NULL) &&
				    (wlc->wsec_keys[wsec_idx]->id != key_id)) {
					WL_ERROR(("wl%d: %s: no free slot for IBSS group key\n",
					          wlc->pub->unit,  __FUNCTION__));
					return BCME_NOMEM;
				}
			}

			if (wlc->wsec_keys[wsec_idx] == NULL) {
				if (wlc_key_allocate_ex(wlc, wsec_idx) == NULL) {
					WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
						wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
					return BCME_NOMEM;
				}

				WL_WSEC(("wl%d: %s: setting IBSS group key %d for scb %s\n",
				         wlc->pub->unit, __FUNCTION__,
				         key_id, bcm_ether_ntoa(&scb->ea, eabuf)));
				scb->ibss_grp_keys[key_id] = wlc->wsec_keys[wsec_idx];
			}
		}
#endif /* defined(IBSS_PEER_GROUP_KEY) */
	}

	key = wlc->wsec_keys[wsec_idx];
#ifdef WLP2P
	rcmta_idx = key->rcmta;
#endif

	/* update the key */
	/* The following line can't use WSEC_KEY macro, since the len might be 0. */
	bzero((char*)key, sizeof(wsec_key_t));
	key->idx = (uint8)wsec_idx;
#ifdef WLP2P
	key->rcmta = rcmta_idx;
#endif
	bcopy((char*)key_data, (char*)key->data, key_len);
	key->len = (uint8)key_len;
	key->id = (uint8)key_id;
	bcopy((char*)key_ea, (char*)&key->ea, ETHER_ADDR_LEN);
	key->aes_mode = AES_MODE_NONE;
#if defined(IBSS_PEER_GROUP_KEY)
	if (key_flags & WL_IBSS_PEER_GROUP_KEY)
		key->flags |= WSEC_IBSS_PEER_GROUP_KEY;
#endif /* defined(IBSS_PEER_GROUP_KEY) */

	switch (key_len) {
	case WEP1_KEY_SIZE:
		WL_WSEC(("wl%d.%d: wlc_key_insert: WEP (40-bit)\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		key->algo = CRYPTO_ALGO_WEP1;
		key->algo_hw = WSEC_ALGO_WEP1;
		key->iv_len = DOT11_IV_LEN;
		key->icv_len = DOT11_ICV_LEN;
		break;
	case WEP128_KEY_SIZE:
		WL_WSEC(("wl%d.%d: wlc_key_insert: WEP (128-bit)\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		key->algo = CRYPTO_ALGO_WEP128;
		key->algo_hw = WSEC_ALGO_WEP128;
		key->iv_len = DOT11_IV_LEN;
		key->icv_len = DOT11_ICV_LEN;
		break;
	case TKIP_KEY_SIZE:
#ifdef BCMWAPI_WPI
		/* key size is same for WAPI and TKIP */
		/* WAPI Case Breaks after this if check */
		if (key_algo == CRYPTO_ALGO_SMS4) {
			key->algo = CRYPTO_ALGO_SMS4;
			key->algo_hw = WSEC_ALGO_SMS4;
			key->iv_len = SMS4_WPI_IV_LEN;
			key->icv_len = 0;
			bcopy(key_data, &key->data[0], (SMS4_KEY_LEN + SMS4_WPI_CBC_MAC_LEN));
			break;
		}
#endif /* BCMWAPI_WPI */
		WL_WSEC(("wl%d.%d: wlc_key_insert: TKIP\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		key->algo = CRYPTO_ALGO_TKIP;
		key->algo_hw = WSEC_ALGO_TKIP;
		key->iv_len = DOT11_IV_TKIP_LEN;
		key->icv_len = DOT11_ICV_LEN;
		break;
	case AES_KEY_SIZE:
		switch (key_algo) {
		case CRYPTO_ALGO_AES_OCB_MSDU :
		case CRYPTO_ALGO_AES_OCB_MPDU:
			WL_WSEC(("wl%d.%d: wlc_key_insert: AES\n", wlc->pub->unit,
			         WLC_BSSCFG_IDX(bsscfg)));
			key->algo = (uint8)key_algo;
			key->algo_hw = WSEC_ALGO_AES;
			key->iv_len = DOT11_IV_AES_OCB_LEN;
			key->icv_len = DOT11_ICV_AES_LEN;
			if (key->algo == CRYPTO_ALGO_AES_OCB_MSDU)
				key->aes_mode = AES_MODE_OCB_MSDU;
			else
				key->aes_mode = AES_MODE_OCB_MPDU;
			break;
		case CRYPTO_ALGO_AES_CCM:
		default:
			WL_WSEC(("wl%d.%d: wlc_key_insert: AES\n", wlc->pub->unit,
			         WLC_BSSCFG_IDX(bsscfg)));
			key->algo = CRYPTO_ALGO_AES_CCM;
			key->algo_hw = WSEC_ALGO_AES;
			if (scb) {
				if (scb->flags & SCB_LEGACY_AES)
					key->algo_hw = WSEC_ALGO_AES_LEGACY;
			}
#ifdef STA
			/*
			 * If this STA is talking to a legacy AP, mark the group key
			 * as legacy also.
			 */
			else if (BSSCFG_STA(bsscfg)) {
				struct scb *apscb;

				apscb = wlc_scbfind(wlc, &bsscfg->current_bss->BSSID);
				if (apscb && (apscb->flags & SCB_LEGACY_AES))
					key->algo_hw = WSEC_ALGO_AES_LEGACY;
			}
#endif /* STA */

			WL_WSEC(("wl%d.%d: wlc_key_insert: setting %s key for %s\n",
			    wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			    ((key_flags & WL_IBSS_PEER_GROUP_KEY) || (!scb)) ?
			        "group" : "pairwise",
			    (key->algo_hw == WSEC_ALGO_AES) ? "AES" : "Legacy AES"));

			key->iv_len = DOT11_IV_AES_CCM_LEN;
			key->icv_len = DOT11_ICV_AES_LEN;
			key->aes_mode = AES_MODE_CCM;
			break;
		}
		break;
	}

	/* For the newly created SCB, sync up the wsec flags */
	if (newscb) {
		switch (key->algo) {
		case CRYPTO_ALGO_WEP1:
		case CRYPTO_ALGO_WEP128:
			scb->wsec = WEP_ENABLED;
			break;
		case CRYPTO_ALGO_TKIP:
			scb->wsec = TKIP_ENABLED;
			break;
		case CRYPTO_ALGO_AES_CCM:
			scb->wsec = AES_ENABLED;
			break;
#ifdef BCMWAPI_WPI
		case CRYPTO_ALGO_SMS4:
			scb->wsec = SMS4_ENABLED;
			break;
#endif /* BCMWAPI_WPI */
		}
	}

	/* set new default key */
	if (ETHER_ISNULLADDR(key_ea)) {
		if (key_flags & WL_PRIMARY_KEY) {
			if (WSEC_BSS_DEFAULT_KEY(bsscfg)) {
				WSEC_BSS_DEFAULT_KEY(bsscfg)->flags &= ~WSEC_PRIMARY_KEY;
			}
			bsscfg->wsec_index = def_idx;
			key->flags |= WSEC_PRIMARY_KEY;
		} else if ((uint32)bsscfg->wsec_index == def_idx) {
			/* this key was the primary, but the key insert cleared the flag */
			bsscfg->wsec_index = -1;
		}
	}

	WL_WSEC(("wl%d.%d: wlc_key_insert: key algo %d algo_hw %d flags %d\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
	         key->algo, key->algo_hw, key->flags));

	WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_KEY_INSERTED_ID,
		WL_LOG_LEVEL_ERR, 0, key->algo, NULL);

#ifdef LINUX_CRYPTO
	/* call the tkip module for the key setup */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		wl_tkip_keyset(wlc->wl, key);
	}
#endif /* LINUX_CRYPTO */

	/* check for a provided IV init value */
	wlc_key_iv_init(wlc, key, initial_iv);

#ifndef LINUX_CRYPTO
	/* At this point, all rxivs are initialized with the same value */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		wsec_iv_t txiv;

		/* group keys in WPA-NONE (IBSS only) AES and TKIP use a global TXIV */
		if ((bsscfg->WPA_auth & WPA_AUTH_NONE) && ETHER_ISNULLADDR(&key->ea))
			bcopy(&bsscfg->wpa_none_txiv, &txiv, sizeof(wsec_iv_t));
		else
			bcopy(&key->txiv, &txiv, sizeof(wsec_iv_t));

		/* calculate initial TKIP keyhash phase1 tx and rx key */
		tkhash_phase1(key->tkip_tx.phase1,
			key->data, bsscfg->cur_etheraddr.octet, txiv.hi);

		WL_WSEC(("wl%d.%d: wlc_key_insert: TKIPtx iv32 0x%08x iv16 0x%04x ta %s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), txiv.hi, txiv.lo,
			bcm_ether_ntoa(&bsscfg->cur_etheraddr, eabuf)));
#ifdef BCMDBG
		if (WL_WSEC_DUMP_ON())
			prhex("  wlc_key_insert: TKIPtx hash", (uchar *) key->tkip_tx.phase1,
			      TKHASH_P1_KEY_SIZE);
#endif /* BCMDBG */
		if (!ETHER_ISNULLADDR(&key->ea)) {
			ASSERT(scb->key == key);
			tkhash_phase1(key->tkip_rx.phase1,
				key->data, key->ea.octet, key->tkip_rx_iv32);

			WL_WSEC(("wl%d.%d: wlc_key_insert: TKIPrx-pair iv32 "
			         "0x%08x ividx %d ta %s\n",
			         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			         key->tkip_rx_iv32, key->tkip_rx_ividx,
			         bcm_ether_ntoa(&scb->ea, eabuf)));
#ifdef BCMDBG
			if (WL_WSEC_DUMP_ON()) {
				prhex("  wlc_key_insert: TKIPrx-pair hash",
				      (uchar *) key->tkip_rx.phase1, TKHASH_P1_KEY_SIZE);
				prhex("  wlc_key_insert: TKIPrx-pair data", (uchar *) key->data,
				      TKIP_KEY_SIZE);
			}
#endif /* BCMDBG */
		} else {
			tkhash_phase1(key->tkip_rx.phase1,
				key->data, bsscfg->BSSID.octet, key->tkip_rx_iv32);

			WL_WSEC(("wl%d.%d: wlc_key_insert: TKIPrx-group iv32 "
			         "0x%08x ividx %d ta %s\n",
			         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			         key->tkip_rx_iv32, key->tkip_rx_ividx,
			         bcm_ether_ntoa(&bsscfg->BSSID, eabuf)));

#ifdef BCMDBG
			if (WL_WSEC_DUMP_ON()) {
				prhex("  wlc_key_insert: TKIPrx-group hash",
				      (uchar *) key->tkip_rx.phase1, TKHASH_P1_KEY_SIZE);
				prhex("  wlc_key_insert: TKIPrx-group data", (uchar *) key->data,
				      TKIP_KEY_SIZE);
			}
#endif /* BCMDBG */
		}
	}
#endif /* LINUX_CRYPTO */

	wlc_key_update(wlc, wsec_idx, bsscfg);

#ifdef STA
	if (bsscfg != wlc->cfg && BSSCFG_STA(bsscfg) && bsscfg->BSS &&
	    ETHER_ISNULLADDR(&key->ea)) {
		int idx = -1;
#ifdef WLP2P
		if (P2P_ENAB(wlc->pub)) {
			if ((scb = wlc_scbfindband(wlc, &bsscfg->BSSID,
			                           wlc->band->bandunit)) != NULL &&
			    scb->key != NULL)
				idx = WSEC_KEY_INDEX(wlc, scb->key);
		}
		else
#endif
		if (bsscfg->rcmta != NULL)
			idx = WSEC_KEY_INDEX(wlc, bsscfg->rcmta);
		if (idx != -1)
			wlc_key_update(wlc, idx, bsscfg);
	}

	/*
	 * A default key is used as an indication of the presence of a group
	 * key, which in turn indicates whether the port is considered open or closed.
	 */
	if (BSSCFG_STA(bsscfg) &&
	    ((bsscfg->flags & WLC_BSSCFG_NOBCMC) || ETHER_ISNULLADDR(&key->ea))) {
		bool prev_psallowed;
		bool prev_portopen = bsscfg->wsec_portopen;

		/* Open port.  If PS_ALLOWED is affected, inform the AP of new PS mode. */
		prev_psallowed = PS_ALLOWED(wlc);
		bsscfg->wsec_portopen = TRUE;
		wlc_enable_btc_ps_protection(wlc, bsscfg, TRUE);
		if (!prev_psallowed && PS_ALLOWED(wlc))
			wlc_set_pmstate(wlc, wlc->PMenabled);

		/* Request PMKID cache plumb */
		if (!prev_portopen && bsscfg->WPA_auth == WPA2_AUTH_UNSPECIFIED) {
#ifdef BCMSUP_PSK
			if (bsscfg->sup_enable_wpa)
				wlc_sup_pmkid_cache_req(bsscfg->sup);
			else
#endif /* BCMSUP_PSK */
				wlc_pmkid_event(bsscfg);
		}
	}
#endif /* STA */

	if (pkey_ptr)
		*pkey_ptr = key;

	return 0;
}

void
wlc_key_remove_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int i;

	WL_WSEC(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));

#if !defined(IBSS_PEER_GROUP_KEY)
	for (i = 0; i < WLC_MAX_WSEC_KEYS(wlc); i++)
		wlc_key_delete(wlc, bsscfg, i);
#else

	for (i = 0; i < WLC_MAX_WSEC_KEYS(wlc); i++) {
		wsec_key_t *key;

		if ((key = WSEC_KEY(wlc, i)) != NULL) {
			if (ETHER_ISNULLADDR(&key->ea))
				ASSERT(key->id < WSEC_MAX_DEFAULT_KEYS);
			wlc_key_delete(wlc, bsscfg, i);
		}
	}
#endif /* defined(IBSS_PEER_GROUP_KEY) */
}

void
wlc_key_remove(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wl_wsec_key_t *remove)
{
	struct scb *scb;
	int max_keys = WLC_MAX_WSEC_KEYS(wlc);
	wsec_key_t *key = NULL;
	int i;
	uint bandunit;

	WL_WSEC(("wl%d.%d: %s\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__));

	/* remove a default key */
	if (ETHER_ISNULLADDR(&remove->ea)) {
		if (remove->index < WSEC_MAX_DEFAULT_KEYS) {
			key = bsscfg->bss_def_keys[remove->index];
			if (key) {
				wlc_key_delete(wlc, bsscfg, key->idx);
				bsscfg->bss_def_keys[remove->index] = NULL;
			}
		}
	}
	/* remove all per station keys */
	else if (ETHER_ISBCAST(&remove->ea)) {
		for (i = WSEC_MAX_DEFAULT_KEYS; i < max_keys; i++)
			wlc_key_delete(wlc, bsscfg, i);
	}
	/* remove a per station key */
	else {
		bandunit = wlc->band->bandunit;
		do {
			scb = wlc_scbfindband(wlc, &remove->ea, bandunit);
			if (scb) {
#if defined(IBSS_PEER_GROUP_KEY)
				key = (remove->flags & WL_IBSS_PEER_GROUP_KEY) ?
				          scb->ibss_grp_keys[remove->index] : scb->key;
#else
				key = scb->key;
#endif /* defined(IBSS_PEER_GROUP_KEY) */
				if (key)
					break;
			}

			if (NBANDS(wlc) > 1)
				bandunit = (bandunit == 0) ? 1 : 0;
		} while (bandunit != wlc->band->bandunit);

		if (key)
			wlc_key_delete(wlc, bsscfg, WSEC_KEY_INDEX(wlc, key));
	}
}

void
wlc_key_delete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int index)
{
	struct scb *scb;
	wsec_key_t *key;
	bool delete = FALSE;
	bool active_key;
	uint i;

	ASSERT(index < WSEC_MAX_KEYS);

#if !defined(IBSS_PEER_GROUP_KEY)
	key = WSEC_KEY(wlc, index);
#else
	key = wlc->wsec_keys[index];
#endif /* defined(IBSS_PEER_GROUP_KEY) */

	WL_WSEC(("wl%d.%d: %s index %d key %p\n", wlc->pub->unit,
	         WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, index, key));

	/* check if already deleted */
	if (key == NULL)
		return;

	active_key = TRUE;
	/* if this is a paired key, clean up any scb pointers */
	if (!ETHER_ISNULLADDR(&key->ea)) {
		uint bandunit, band_idx;
		bandunit = wlc->band->bandunit;
		for (band_idx = 0; band_idx < NBANDS(wlc); band_idx++) {
			if ((scb = wlc_scbfindband(wlc, &key->ea, bandunit)) &&
			    (scb->bsscfg == bsscfg) &&
			    ((key == scb->key) ||
			     (key == SCB_PREVOUS_KEY(scb)) ||
#if defined(IBSS_PEER_GROUP_KEY)
			     (key == scb->ibss_grp_keys[key->id]) ||
#endif
			     0)) {
#if defined(BCMDBG) || defined(WLMSG_WSEC)
				char eabuf[ETHER_ADDR_STR_LEN];
#endif
				ASSERT(!bcmp(key->ea.octet, scb->ea.octet, ETHER_ADDR_LEN));
				WL_WSEC(("wl%d.%d: reset key pointer in scb %s\n",
				         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				         bcm_ether_ntoa(&scb->ea, eabuf)));
#if defined(IBSS_PEER_GROUP_KEY)
				if (key->flags & WSEC_IBSS_PEER_GROUP_KEY) {
					ASSERT(key == scb->ibss_grp_keys[key->id]);
					scb->ibss_grp_keys[key->id] = NULL;
				} else
#endif /* defined(IBSS_PEER_GROUP_KEY) */
				{
#ifdef BCMWAPI_WPI
					if (key == SCB_PREVOUS_KEY(scb)) {
						ASSERT(key->algo == CRYPTO_ALGO_SMS4);
						ASSERT(scb->prev_key->algo == CRYPTO_ALGO_SMS4);
						scb->prev_key = NULL;
						scb->prev_key_valid_time = 0;
						active_key = FALSE;
					}
					else
#endif /* BCMWAPI_WPI */
						scb->key = NULL;
				}
				delete = TRUE;
			}
			bandunit = 1 - bandunit;
		}
#ifdef WLP2P
		if (P2P_ENAB(wlc->pub))
			;
		else
#endif
		if (key == bsscfg->rcmta) {
			WL_WSEC(("wl%d.%d: reset rcmta pointer in bsscfg\n", wlc->pub->unit,
			         WLC_BSSCFG_IDX(bsscfg)));
			bsscfg->rcmta = NULL;
			if (!delete) {
				active_key = FALSE;
				delete = TRUE;
			}
		}
	}
	/* bcmc/group key */
	else {
		for (i = 0; i < ARRAYSIZE(bsscfg->bss_def_keys); i ++) {
			if (bsscfg->bss_def_keys[i] == key) {
				/* if this was the primary tx key, update wsec_index */
				if (i == (uint)bsscfg->wsec_index)
					bsscfg->wsec_index = -1;
				WL_WSEC(("wl%d.%d: reset group key pointer %d in bsscfg\n",
				         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), i));
				bsscfg->bss_def_keys[i] = NULL;
				delete = TRUE;
				break;
			}
		}
	}

	/* this key doesn't belong to the bsscfg */
	if (!delete)
		return;

#ifdef LINUX_CRYPTO
	/* call the tkip module key cleanup */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		key->len = 0;
		wl_tkip_keyset(wlc->wl, key);
	}
#endif /* LINUX_CRYPTO */

#ifdef STA
	/* If Pairwise key or Group key is removed sec port is closed */
	if (BSSCFG_STA(bsscfg) && active_key) {
		/* Close port.  If PS_ALLOWED is affected, inform the AP of new PS mode. */
		bool prev_psallowed = PS_ALLOWED(wlc);
		bsscfg->wsec_portopen = FALSE;
		wlc_enable_btc_ps_protection(wlc, bsscfg, FALSE);
		if (prev_psallowed && !PS_ALLOWED(wlc))
			wlc_set_pmstate(wlc, wlc->PMenabled);
	}
#endif /* STA */

	/* reset even if we deallocate to catch use after free */
	wlc_key_reset(wlc, key);

	/* free the memory block for paired keys */
	if (index >= WSEC_MAX_DEFAULT_KEYS) {
		MFREE(wlc->osh, key, sizeof(wsec_key_t));
		wlc->wsec_keys[index] = NULL;
	}

	wlc_key_update(wlc, index, bsscfg);
}

/* reset the given key to initial values */
static void
wlc_key_reset(wlc_info_t *wlc, wsec_key_t *key)
{
	WL_WSEC(("wl%d: %s index %d\n", wlc->pub->unit, __FUNCTION__,
	         WSEC_KEY_INDEX(wlc, key)));

	ASSERT(WSEC_KEY_INDEX(wlc, key) < WSEC_MAX_KEYS);

	key->len = 0;
	key->flags = 0;
	key->algo = CRYPTO_ALGO_OFF;
	key->algo_hw = WSEC_ALGO_OFF;
	bzero(key->ea.octet, ETHER_ADDR_LEN);
}

void
wlc_key_scb_delete(wlc_info_t *wlc, struct scb *scb)
{
	int key_index;
	wsec_key_t *key;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */
#if defined(IBSS_PEER_GROUP_KEY)
	int i;
#endif
	wlc_bsscfg_t *bsscfg;

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

#if defined(IBSS_PEER_GROUP_KEY)
	/* In IBSS mode, delete any per sta group keys */
	for (i = 0; i < WSEC_MAX_DEFAULT_KEYS; i++) {
		if (scb->ibss_grp_keys[i]) {
			WL_WSEC(("wl%d: %s: deleting ibss group key %d for %s\n",
			         wlc->pub->unit, __FUNCTION__, i, bcm_ether_ntoa(&scb->ea, eabuf)));
			wlc_key_delete(wlc, bsscfg, WSEC_KEY_INDEX(wlc, scb->ibss_grp_keys[i]));
			scb->ibss_grp_keys[i] = NULL;
		}
	}
#endif /* defined(IBSS_PEER_GROUP_KEY) */

	if (bsscfg != wlc->cfg && BSSCFG_STA(bsscfg) && bsscfg->BSS) {
		wlc_rcmta_del_bssid(wlc, bsscfg);
#ifdef WLP2P
		if (P2P_ENAB(wlc->pub))
			;
		else
#endif /* WLP2P */
		return;
	}

	key = scb->key;
	if (key) {
		key_index = WSEC_KEY_INDEX(wlc, key);

		WL_WSEC(("wl%d: %s: deleting pairwise key %d for %s\n",
		         wlc->pub->unit, __FUNCTION__,
		         key_index, bcm_ether_ntoa(&scb->ea, eabuf)));

		/* the scb key pointer will be set to NULL by this function. */
		wlc_key_delete(wlc, bsscfg, key_index);

		/* if we are freeing up a hardware key, look for a scb using a softkey
		 * to swap in to the hardware key slot
		 */
		if (key_index < WLC_MAX_WSEC_HW_KEYS(wlc) && !WLC_SW_KEYS(wlc, bsscfg)) {
			wlc_key_hw_reallocate(wlc, key_index, bsscfg);
		}
	}
#ifdef BCMWAPI_WPI
	key = scb->prev_key;
	scb->prev_key = NULL;
	scb->prev_key_valid_time = 0;
	if (key) {
		ASSERT(key->algo == CRYPTO_ALGO_SMS4);
		key_index = WSEC_KEY_INDEX(wlc, key);

		WL_WSEC(("wl%d: %s: deleting prev pairwise key %d for %s\n",
		         wlc->pub->unit, __FUNCTION__,
		         key_index, bcm_ether_ntoa(&scb->ea, eabuf)));
		/* note: the scb key pointer will be set to NULL by this function. */
		wlc_key_delete(wlc, scb->bsscfg, key_index);

		/* if we are freeing up a hardware key, look for a scb using a softkey
		 * to swap in to the hardware key slot
		 */
		if (key_index < WLC_MAX_WSEC_HW_KEYS(wlc) && !WLC_SW_KEYS(wlc, bsscfg)) {
			wlc_key_hw_reallocate(wlc, key_index, SCB_BSSCFG(scb));
		}
	}
#endif /* BCMWAPI_WPI */
}

static void
wlc_key_hw_reallocate(wlc_info_t *wlc, int hw_index, struct wlc_bsscfg *bsscfg)
{
	int i;
	int soft_index;
	uint max_used;
	struct scb *scb = NULL;
	wsec_key_t *key;
	uint bandunit;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */

	WL_WSEC(("wl%d: %s: hw_index = %d\n",
		wlc->pub->unit, __FUNCTION__, hw_index));

	ASSERT(wlc->wsec_keys[hw_index] == NULL);

	/*
	 * Find a candidate paired soft key to move into the hw key slot
	 */
	max_used = 0;
	soft_index = -1;

	for (i = WLC_MAX_WSEC_HW_KEYS(wlc); i < WLC_MAX_WSEC_KEYS(wlc); i++) {
		key = wlc->wsec_keys[i];
		if (key == NULL)
			continue;

		/* do the scb lookup in both bands */
		bandunit = wlc->band->bandunit;

		do {
			/* look up the scb for this key */
			scb = wlc_scbfindband(wlc, &key->ea, bandunit);

			/* if this scb is the most recently used, remember the key index */
			if (scb && (max_used < scb->used)) {
				max_used = scb->used;
				soft_index = i;
			}

			if (NBANDS(wlc) > 1)
				bandunit = (bandunit == 0) ? 1 : 0;
		} while (bandunit != wlc->band->bandunit);
	}

	/* return if no soft keys in use */
	if (soft_index == -1)
		return;

	/*
	 * A taker for the hw slot has been found.
	 */
	key = wlc->wsec_keys[soft_index];

	WL_WSEC(("wl%d: %s: swapping soft key %d to hw key %d for MAC %s\n",
		wlc->pub->unit, __FUNCTION__,
		soft_index, hw_index, bcm_ether_ntoa(&key->ea, eabuf)));

	/* move the soft key to the hw slot */
	wlc->wsec_keys[hw_index] = key;
	wlc->wsec_keys[soft_index] = NULL;
	key->idx = (uint8)hw_index;

	/* now push the new key info to the hw key */
	wlc_key_update(wlc, hw_index, bsscfg);

	/* clear out the soft key */
	wlc_key_update(wlc, soft_index, bsscfg);
}

#if defined(BCMDBG)
/* Set the key flag that provokes a deliberate error. */
int
wlc_key_set_error(wlc_info_t *wlc, int kflag, wl_wsec_key_t *key_param)
{
	wsec_key_t *key = NULL;
	struct scb *scb;

	/* Look up by ether addr if one was furnished; otherwise, if key
	 * index is in range use that.
	 */
	if (!ETHER_ISNULLADDR(&key_param->ea)) {
		if ((scb = wlc_scbfind(wlc, &key_param->ea)) == NULL)
			return BCME_NOTFOUND;
		key = scb->key;

	} else if (key_param->index < (uint32)WLC_MAX_WSEC_KEYS(wlc)) {
		key = wlc->wsec_keys[key_param->index];
	}
	if ((key == NULL) || (key->len == 0))
		return BCME_BADKEYIDX;
	switch (kflag) {
	case WSEC_GEN_MIC_ERROR:
		if (key->algo != CRYPTO_ALGO_TKIP)
			return BCME_BADARG;
		key->flags |= WSEC_TKIP_ERROR;
		break;
	case WSEC_GEN_ICV_ERROR:
		key->flags |= WSEC_ICV_ERROR;
		break;
	case WSEC_GEN_REPLAY:
		switch (key->algo) {
		case CRYPTO_ALGO_TKIP:
		case CRYPTO_ALGO_AES_CCM:
		case CRYPTO_ALGO_AES_OCB_MSDU:
		case CRYPTO_ALGO_AES_OCB_MPDU:
			key->flags |= WSEC_REPLAY_ERROR;
			break;
		default:
			return BCME_RANGE;
			break;
		}
		break;
	default:
		return BCME_BADARG;
		break;
	}
	return 0;
}
#endif /* defined(BCMDBG) */


void
wlc_key_iv_init(wlc_info_t *wlc, wsec_key_t *key, wsec_iv_t *initial_iv)
{
	d11regs_t *regs = wlc->regs;
	uint16 r1, r2;
	int i;
	osl_t *osh;

	WL_TRACE(("wl%d: wlc_key_iv_init\n", wlc->pub->unit));

	if (!wlc->pub->up)
		return;

	ASSERT(key != NULL);

	osh = wlc->osh;

	switch (key->algo) {

	case CRYPTO_ALGO_WEP1:
	case CRYPTO_ALGO_WEP128:
		/* initialize transmit IV to a ~random value to minimize IV re-use
		 * at startup
		 */
		r1 = R_REG(osh, &regs->tsf_random);
		r2 = R_REG(osh, &regs->tsf_random);
		key->txiv.hi = ((r2 & 0xff) << 16) | (r1 & 0xff00) | (r1 & 0x00ff);
		key->txiv.lo = 0;	/* WEP only uses hi */
		/* rx IV is not used for WEP */
		bzero(key->rxiv, sizeof(key->rxiv));
		break;

	case CRYPTO_ALGO_TKIP:
#ifdef LINUX_CRYPTO
		break;
#else
		if (initial_iv)
			key->tkip_rx_iv32 = initial_iv->hi;
		else
			key->tkip_rx_iv32 = 0;
		key->tkip_rx_ividx = 0;
		/* fall thru... */
#endif /* LINUX_CRYPTO */
	case CRYPTO_ALGO_AES_CCM:
		for (i = 0; i < WLC_NUMRXIVS; i ++) {
			if (initial_iv != NULL) {
				key->rxiv[i].hi = initial_iv->hi;
				key->rxiv[i].lo = initial_iv->lo;
			} else {
				key->rxiv[i].hi = 0;
				key->rxiv[i].lo = 0;
			}
		}
		key->txiv.hi = key->txiv.lo = 0;
		break;
#ifdef BCMWAPI_WPI
	case CRYPTO_ALGO_SMS4:
		wlc_wapi_key_iv_init(wlc, key, initial_iv);
		break;
#endif /* BCMWAPI_WPI */

	default:
		WL_ERROR(("wl%d: wlc_key_iv_init: unsupported algorithm %d\n", wlc->pub->unit,
			key->algo));
		break;
	}

}

#ifdef BCMWAPI_WPI
static void
_wlc_wapi_key_iv_update(wsec_key_t *key, uchar *buf, bool update)
{
	ASSERT(key->algo == CRYPTO_ALGO_SMS4);
	/* as per spec increment 8.2.4 of WAPI specification increment counter before sending */
	/* if multilcast packet should increment the iv by 1, IBSS or AP */
	if (update) {
		if (ETHER_ISNULLADDR(&key->ea))
			bcm_inc_bytes((uchar *)key->wapi_txiv.PN, 16, 1);
		else
			bcm_inc_bytes((uchar *)key->wapi_txiv.PN, 16, 2);
	}
	bcopy((char *)&key->wapi_txiv, &buf[0], sizeof(struct wpi_iv));
}

static void
wlc_wapi_key_iv_init(wlc_info_t *wlc, wsec_key_t *key, wsec_iv_t *initial_iv)
{
	int i;

	bzero(&key->wapi_txiv, SMS4_WPI_IV_LEN);
	bzero(&key->wapi_rxiv, SMS4_WPI_IV_LEN);

	/* AP is AE and STA is ASUE */
	for (i = 0; i < SMS4_WPI_PN_LEN;) {
		key->wapi_txiv.PN[i++] = 0x36;
		key->wapi_txiv.PN[i++] = 0x5C;
	}
	for (i = 0; i < SMS4_WPI_PN_LEN;) {
		key->wapi_rxiv.PN[i++] = 0x36;
		key->wapi_rxiv.PN[i++] = 0x5C;
	}
	if (AP_ENAB(wlc->pub) && AP_ACTIVE(wlc)) {
		key->wapi_txiv.PN[0] = 0x37;
	}
	else {
		if (wlc->bsscfg[0]->BSS)
			key->wapi_rxiv.PN[0] = 0x37;
		else {
			int val;
			/* Compare the mac addresses to figure which one is AE which is ASE */
			val = bcm_cmp_bytes((uchar *)&key->ea, (uchar *)&wlc->pub->cur_etheraddr,
				ETHER_ADDR_LEN);
			if (val >= 0)
				key->wapi_rxiv.PN[0] = 0x37;
			else
				key->wapi_txiv.PN[0] = 0x37;
		}
	}
	key->wapi_txiv.key_idx = key->id;
	key->wapi_rxiv.key_idx = key->id;
}
#endif /* BCMWAPI_WPI */

#define WLC_AES_EXTENDED_PACKET	(1 << 5)
#define WLC_AES_OCB_IV_MAX	((1 << 28) - 3)

void
wlc_key_iv_update(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wsec_key_t *key, uchar *buf, bool update)
{
	int index;
	uint32 iv_tmp = 0;
	wsec_iv_t *txiv;

	ASSERT(key != NULL);

#ifdef BCMWAPI_WPI
	if (key->algo == CRYPTO_ALGO_SMS4) {
		_wlc_wapi_key_iv_update(key, buf, update);
		return;
	}
#endif /* BCMWAPI_WPI */

	/* group keys in WPA-NONE (IBSS only, AES and TKIP) use a global TXIV */
	if ((bsscfg->WPA_auth & WPA_AUTH_NONE) &&
	    ETHER_ISNULLADDR(&key->ea))
		txiv = &bsscfg->wpa_none_txiv;
	else
		txiv = &key->txiv;

	WL_WSEC(("wl%d.%d: wlc_key_iv_update start: iv32 0x%08x iv16 0x%04x "
	         "algo %d \n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
	         txiv->hi, txiv->lo, key->algo));

	index = key->id;

	switch (key->algo) {

	case CRYPTO_ALGO_AES_CCM:
		if (update) {
			txiv->lo++;
			if (txiv->lo == 0)
				txiv->hi++;
		}

		/* Add in control flags and properly format */
		buf[0] = txiv->lo & 0xff;
		buf[1] = (txiv->lo >> 8) & 0xff;
		buf[2] = 0;
		buf[3] = (uint8)(((index & 3) << DOT11_KEY_INDEX_SHIFT) | WLC_AES_EXTENDED_PACKET);
		buf[4] = txiv->hi & 0xff;
		buf[5] = (txiv->hi >>  8) & 0xff;
		buf[6] = (txiv->hi >> 16) & 0xff;
		buf[7] = (txiv->hi >> 24) & 0xff;
		break;

	case CRYPTO_ALGO_AES_OCB_MPDU:
	case CRYPTO_ALGO_AES_OCB_MSDU:
		if (update) {
			if (txiv->hi >= (1 << 28) - 3) {
				txiv->hi = 0;
			} else {
				txiv->hi++;
			}
		}

		/* Deconstruct the IV */
		buf[0] = txiv->hi & 0xff;
		buf[1] = (txiv->hi >>  8) & 0xff;
		buf[2] = (txiv->hi >> 16) & 0xff;
		buf[3] = (index & 3) << DOT11_KEY_INDEX_SHIFT;
		buf[3] |= (txiv->hi >> 24) & 0xf;
		break;

	case CRYPTO_ALGO_TKIP:
#ifndef LINUX_CRYPTO
		if (update) {
			txiv->lo++;
			if (txiv->lo == 0) {
				ASSERT(txiv->hi != 0xffffffff);
				txiv->hi++;
				tkhash_phase1(key->tkip_tx.phase1, key->data,
					&(bsscfg->cur_etheraddr.octet[0]), txiv->hi);
			}
		}

		tkhash_phase2(key->tkip_tx.phase2, key->data, key->tkip_tx.phase1, txiv->lo);
		buf[0] = key->tkip_tx.phase2[0];
		buf[1] = key->tkip_tx.phase2[1];
		buf[2] = key->tkip_tx.phase2[2];
		buf[3] = (uchar)((index << DOT11_KEY_INDEX_SHIFT) | DOT11_EXT_IV_FLAG);
		buf[4] = txiv->hi & 0xff;
		buf[5] = (txiv->hi >>  8) & 0xff;
		buf[6] = (txiv->hi >> 16) & 0xff;
		buf[7] = (txiv->hi >> 24) & 0xff;

#ifdef BCMDBG
		if (WL_WSEC_DUMP_ON())
			prhex("  wlc_key_iv_update: updated TKIP iv", buf, DOT11_IV_TKIP_LEN);
#endif /* BCMDBG */
#endif /* LINUX_CRYPTO */
		break;

	case CRYPTO_ALGO_WEP1:
	case CRYPTO_ALGO_WEP128:
		{
		/* Handle WEP */
		uint bad = 1;
		uchar x, y, z, a, b, B;

		if (update) {
			{
				/* skip weak ivs */
				iv_tmp = txiv->hi;
				while (bad) {
					iv_tmp++;

					x = iv_tmp & 0xff;
					y = (iv_tmp >> 8) & 0xff;
					z = (iv_tmp >> 16) & 0xff;

					a = x + y;
					b = (x + y) - z;

					/* Avoid weak IVs used in several published WEP
					 * crackingtools.
					 */
					if (a <= WEP128_KEY_SIZE)
						continue;

					if (a == 1 && b == 2)
						continue;

					if ((y == RC4_STATE_NBYTES - 1) &&
						(x > 2 && x <= WEP128_KEY_SIZE + 2))
						continue;

					if (x == 1 && (y >= 2 && y <= ((WEP128_KEY_SIZE-1)/2 + 1)))
						continue;

					bad = 0;
					B = 1;
					while (B < WEP128_KEY_SIZE) {
						if ((a == B && b == (B + 1) * 2)) {
							bad++;
							break;
						}
						B += 2;
					}
					if (bad) continue;

					B = 3;
					while (B < WEP128_KEY_SIZE/2 + 3) {
						if ((x == B) && (y == (RC4_STATE_NBYTES-1)-x)) {
							bad++;
							break;
						}
						B++;
					}
				}

				txiv->hi = iv_tmp;
			}
		}

		/* break down in 3 bytes */
		buf[0] = txiv->hi & 0xff;
		buf[1] = (txiv->hi >> 8) & 0xff;
		buf[2] = (txiv->hi >> 16) & 0xff;
		buf[3] = index << DOT11_KEY_INDEX_SHIFT;

		}
		break;
	default:
		WL_ERROR(("wl%d.%d: wlc_key_iv_update: unsupported algorithm %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		          key->algo));
		ASSERT(1 == 0);
		break;
	}

	WL_WSEC(("wl%d.%d: wlc_key_iv_update end: iv32 0x%08x iv16 0x%04x\n",
	         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), txiv->hi,
	         txiv->lo));
}

wsec_key_t*
wlc_key_lookup(wlc_info_t *wlc, struct scb *scb, wlc_bsscfg_t *bsscfg,
	uint index, bool ismulti)
{
	wsec_key_t *key = NULL;

	WL_WSEC(("\nwl%d.%d: wlc_key_lookup: key ID %d\n", wlc->pub->unit,
	         WLC_BSSCFG_IDX(bsscfg), index));

	if (!ismulti && WSEC_SCB_KEY_VALID(scb)) {
		key = scb->key;
#ifdef BCMWAPI_WPI
		if (key->algo == CRYPTO_ALGO_SMS4) {
			if (key->id == index) {
				if (scb->prev_key) {
					wlc_key_delete(wlc, scb->bsscfg, scb->prev_key->idx);
					scb->prev_key = NULL;
					scb->prev_key_valid_time = 0;
				}
			} else if (scb->prev_key && (scb->prev_key->id == index)) {
				if (wlc->pub->now > scb->prev_key_valid_time) {
					wlc_key_delete(wlc, scb->bsscfg, scb->prev_key->idx);
					scb->prev_key = NULL;
					scb->prev_key_valid_time = 0;
				}
				key = scb->prev_key;
			} else {
				WL_WSEC(("WAPI: no Valid key for key_id %d\n", index));
				return NULL;
			}
		}
#endif /* BCMWAPI_WPI */
		ASSERT(WSEC_KEY_INDEX(wlc, key) < WLC_MAX_WSEC_KEYS(wlc));
		/* ASSERT(key->index == index); */
		/* indexes should both be zero for per-path key */
		WL_WSEC(("wl%d.%d: wlc_key_lookup: per-path wsec key index %d algo %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), key->idx, key->algo));
	} else {
#if !defined(IBSS_PEER_GROUP_KEY)
		uint def_idx;
#endif /* defined(IBSS_PEER_GROUP_KEY) */

		ASSERT(index < WSEC_MAX_DEFAULT_KEYS);

#if !defined(IBSS_PEER_GROUP_KEY)
		if (bsscfg != wlc->cfg && BSSCFG_STA(bsscfg) &&
		    WLC_STA_GROUP_KEYS == (WSEC_MAX_DEFAULT_KEYS >> 1))
			def_idx = index >> 1;
		else
			def_idx = index;
		key = WLC_BSSCFG_WEPKEY(bsscfg, def_idx);

		if (key) {
			WL_WSEC(("wl%d.%d: wlc_key_lookup: valid group key index %d "
			         "ID %d algo %d\n",
			         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
			         def_idx, key->id, key->algo));
		} else {
			WL_WSEC(("wl%d.%d: wlc_key_lookup: no valid group key index %d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), def_idx));
		}
#else
		if (bsscfg->BSS || (bsscfg->WPA_auth == WPA_AUTH_DISABLED) ||
		    (bsscfg->WPA_auth == WPA_AUTH_NONE))
			key = WLC_BSSCFG_WEPKEY(bsscfg, index);
		else
			key = scb->ibss_grp_keys[index];

		if (key) {
			WL_WSEC(("wl%d.%d: wlc_key_lookup: valid group key at key-ID %d, algo %d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), index, key->algo));
		} else {
			WL_WSEC(("wl%d.%d: wlc_key_lookup: no valid group key at key-ID %d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), index));
		}
#endif /* defined(IBSS_PEER_GROUP_KEY) */
	}

	return key;
}

/* initialize all hardware keys for the current core */
void
wlc_key_hw_init_all(wlc_info_t *wlc)
{
	uint i;
	uint hw_keys;
	wlc_bsscfg_t *bsscfg;
	wsec_key_t *key;

	WL_TRACE(("wl%d: wlc_key_hw_init_all\n", wlc->pub->unit));

	wlc_key_hw_clear_all(wlc);

	/* return Only if all the bss configs are configured
	 * to use sw keys.
	 */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (!WLC_SW_KEYS(wlc, bsscfg))
			break;
	}

	if (i == WLC_MAXBSSCFG)
		return;

	hw_keys = WLC_MAX_WSEC_HW_KEYS(wlc);

	for (i = 0; i < hw_keys; i++) {
		key = WSEC_KEY(wlc, i);

		if (key) {
#ifdef BCMWAPI_WPI
			if (key->algo == CRYPTO_ALGO_SMS4 &&
			    !(wlc->hw_wapi_capable && wlc->hw_wapi_enabled))
			    continue;
#endif /* BCMWAPI_WPI */
			wlc_key_hw_init(wlc, i, wlc->cfg); /* use primary bsscfg for init */
		}
	}
}

/* Clear all hardware key state in the current core.
 * This assumes that the core has been reset so all registers
 * have their reset value.
 */
static void
wlc_key_hw_clear_all(wlc_info_t *wlc)
{
	int i, hw_keys;

	WL_TRACE(("wl%d: wlc_key_hw_clear_all\n", wlc->pub->unit));

	hw_keys = WLC_MAX_WSEC_HW_KEYS(wlc);

	/* zero out the entire key table */
	wlc_set_shm(wlc, wlc->seckeys, 0, hw_keys * D11_MAX_KEY_SIZE);

	/* clear the Rx-TA MAC addresses */
	WL_NONE(("**** KEY SETTING\n"));
	for (i = 0; i < M_SECKINDXALGO_BLK_SZ; i++) {
		wlc_write_shm(wlc, M_SECKINDXALGO_BLK + (i * 2), 0);
	}

	WL_NONE(("**** KEY SETTING RCMTA corerev %d \n", wlc->pub->corerev));
	/* clear the address table */
	for (i = 0; i < (WSEC_MAX_RCMTA_KEYS - WSEC_MAX_DEFAULT_KEYS); i++)
		wlc_set_rcmta(wlc, i, &ether_null);

	/* update the rcmta_size reg in case we are running with a
	 * WSEC_MAX_RCMTA_KEYS value different than the reset value
	 */
	W_REG(wlc->osh, &wlc->regs->rcmta_size, RCMTA_SIZE);

	/* clear the ucode default key flag MHF1_DEFKEYVALID */
	wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, 0, WLC_BAND_ALL);
	WL_WSEC(("wl%d: wlc_key_hw_clear_all: clear MHF1_DEFKEYVALID\n", wlc->pub->unit));
}

/*
 * Return MHF1_DEFKEYVALID flag based on soft keys and/or default WEP keys
 * existances and other factors.
 */
/*
 * ucode flag MHF1_DEFKEYVALID should be SET only when WEP encryption is
 * enabled, only one SSID is in use, there are valid default WEP keys and
 * there are no soft keys. It is cleared in all other cases.
 * The following explains why.
 *
 * This flag is meaningful only to encrypted unicast frames received. When
 * processing a rx'd unicast frame the ucode tries a pairwise key match first
 * based on the frame's TA and then falls back to a default key indicated by
 * the key index field in IV field of the frame if no pairwise key match is
 * found. For multicast/broadcast frames the ucode uses the key index field
 * in the frames' IV anyway to find the key entry.
 *
 * In case of Multi-SSID, the default keys are only valid for one SSID, and
 * the ucode does not distinguish between the SSIDs when checking the flag.
 *
 * In case of software encryption/decryption when soft keys are installed
 * this flag must be cleared so that encrypted unicast frames can be sent up
 * to the driver without any decryption attempt when there is no key installed
 * in the hardware for the TA. It is true for both STA and AP.
 *
 * In case of WPA/TKIP|AES we don't support group-key-only configuration and
 * there must be a pairwise key there on STA and one pairwise key per
 * associated STA on AP therefore this flag should always be cleared on both
 * STA and AP.
 *
 * In case of mixed WPA/TKIP|AES and 802.1x/WEP this flag should be always
 * cleared on AP since there must be a pairwise key for each STA.
 *
 * In case of 802.1x/WEP it should be always cleared on AP. However it must
 * be set on STA since there is no pairwise key on STA.
 *
 * In case of mixed WPA/TKIP|AES and 802.11/WEP this flag should be always
 * cleared on AP since the current transition mode support on AP plumbs
 * pairwise key for WEP STAs too.
 *
 * In case of multiple BSS configurations, default keys are only used by the
 * primary config.
 * In any other case of 802.11/WEP this flag should always be set. It is true
 * for both STA and AP.
 */

uint16
wlc_key_defkeyflag(wlc_info_t *wlc)
{
	bool defkeys = FALSE, swkeys = FALSE;
	wsec_key_t *key;
	int j;

	/* No default keys for AP-STA configurations with both interfaces active */
	if (APSTA_ENAB(wlc->pub)) {
		int idx, up_cnt = 0;
		wlc_bsscfg_t *cfg;

		FOREACH_BSS(wlc, idx, cfg) {
			if (cfg->up)
				up_cnt++;
		}
		if (up_cnt > 1) {
			WL_WSEC(("wl%d: %s: APSTA_ENB and %d bsscfgs up, clearing defkeyvalid\n",
			         wlc->pub->unit, __FUNCTION__, up_cnt));
			return 0;
		}
	}

	/* no default key attempt for WPA */
	/* assuming AES or TKIP is enabled */
	if (wlc->cfg->WPA_auth != WPA_AUTH_DISABLED)
		return 0;

	/* no default key attempt for 802.1x on AP */
	/* assuming WEP is enabled */
	if (AP_ENAB(wlc->pub) && wlc->cfg->eap_restrict)
		return 0;

#ifdef BCMWAPI_WAI
	/* no default key attempt for WAI on AP */
	/* assuming WEP is enabled */
	if (AP_ENAB(wlc->pub) && wlc->cfg->wai_restrict)
		return 0;
#endif

	/* set defkeys to TRUE if there are any default WEP keys */
	/* assuming WEP is enabled */
	for (j = 0; j < WSEC_MAX_DEFAULT_KEYS; j ++) {
		if ((key = WSEC_KEY(wlc, j)) &&
		    (key->algo == CRYPTO_ALGO_WEP1 || key->algo == CRYPTO_ALGO_WEP128)) {
			defkeys = TRUE;
			break;
		}
	}
	/* set swkeys to TRUE if there are any soft keys */
	for (j = WLC_MAX_WSEC_HW_KEYS(wlc); j < WLC_MAX_WSEC_KEYS(wlc); j++) {
		if (WSEC_KEY(wlc, j)) {
			swkeys = TRUE;
			break;
		}
	}

	/* set the flag only when there are default WEP keys and there are no soft keys */
	WL_WSEC(("wl%d: wlc_key_defkeyflag: default WEP keys %s, soft keys %s\n",
		wlc->pub->unit, defkeys ? "Yes" : "No", swkeys ? "Yes" : "No"));
	return (defkeys && !swkeys) ? MHF1_DEFKEYVALID : 0;
}

/* Carry out any hardware actions needed after a wlc->wsec_key has been modified */
void
wlc_key_update(wlc_info_t *wlc, int index, wlc_bsscfg_t *bsscfg)
{
	wsec_key_t *key;

	WL_WSEC(("wl%d: wlc_key_update wsec key index %d\n", wlc->pub->unit, index));

	/* if we are using soft encryption, then the hardware needs no update */
	if (WLC_SW_KEYS(wlc, bsscfg))
		return;

	/* Invalid index number */
	if (index >= WSEC_MAX_KEYS) {
		ASSERT(0);
		return;
	}

	key = WSEC_KEY(wlc, index);

#ifdef BCMWAPI_WPI
	if (key && (key->algo == CRYPTO_ALGO_SMS4) &&
		!(wlc->hw_wapi_capable || wlc->hw_wapi_enabled)) {
		WL_WSEC(("%s: %d: SW WAPI Key, cap(%d), enable(%d) \n", __FUNCTION__, __LINE__,
			wlc->hw_wapi_capable, wlc->hw_wapi_enabled));
		return;
	}
#endif /* BCMWAPI_WPI */

#ifdef LINUX_CRYPTO
	if (key && (key->algo == CRYPTO_ALGO_TKIP))
		return;
#endif

	/* update the MHF1_DEFKEYVALID flag bit */
	if (index >= WLC_MAX_WSEC_HW_KEYS(wlc)) {
		/* Clear the ucode default key valid flag MHF1_DEFKEYVALID if inserting
		 * a per-STA soft key.
		 * Need to check both soft keys and default WEP keys before
		 * making decision to clear or set this flag when deleting a soft key
		 */
		uint16 flag = key != NULL ? 0 : wlc_key_defkeyflag(wlc);
		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, flag, WLC_BAND_ALL);
		WL_WSEC(("wl%d: wlc_key_update: %s soft key %d, %s MHF1_DEFKEYVALID\n",
		         wlc->pub->unit, key != NULL ? "insert" : "delete", index,
		         flag ? "set" : "clear"));
		/* if this is a soft key, then the hardware needs no update */
		return;
	}

	/* update the hardware */
	if (wlc->pub->up)
		wlc_key_hw_init(wlc, index, bsscfg);
}

#ifdef WOWL
/* update the hardware with values to match the corresponding wlc->wsec_key but for
 * Wake-on-Wireless LAN mode only. The constraints are that there are limited hardware slots
 * so pair-wise key needs to be programmed into one of those hardware slots
 * Also, TSC/PN has to be programmed with the ucode as during 'sleep', ucode will manage that
 */
void
wlc_key_hw_wowl_init(wlc_info_t *wlc, wsec_key_t *key, int rcmta_index, wlc_bsscfg_t *bsscfg)
{
	uint j, offset;
	uint16 v, seckeys, tkmickeys;

	WL_TRACE(("wl%d: wlc_key_hw_init_wowl key %d\n", wlc->pub->unit, rcmta_index));
	ASSERT(rcmta_index < WLC_MAX_WSEC_HW_KEYS(wlc));

	ASSERT(wlc->hw->clk);

	ASSERT(WOWL_ACTIVE(wlc->pub));

	/* Clear the MAC address match if this is a paired key.
	 * This protects the ucode or hardware from getting an
	 * address match while we update this key.
	 */
	if (rcmta_index >= WSEC_MAX_DEFAULT_KEYS)
		wlc_key_write_addr(wlc, rcmta_index, &ether_null);

	/* Set the matching SECALGO table entry (algorithm and table index) */
	v = key->algo_hw | (rcmta_index << SKL_INDEX_SHIFT);
	wlc_write_shm(wlc, M_WOWL_SECKINDXALGO_BLK + rcmta_index * 2, v);

	/*
	 * Set the default key (i < WSEC_MAX_DEFAULT_KEYS)
	 *	or set the per sta key (i >= WSEC_MAX_DEFAULT_KEYS).
	 */
	offset = rcmta_index * D11_MAX_KEY_SIZE;

	ASSERT(key);

	seckeys = wlc_read_shm(wlc, M_WOWL_SECRXKEYS_PTR) * 2;
	tkmickeys = wlc_read_shm(wlc, M_WOWL_TKMICKEYS_PTR) * 2;

	/* write the key */
	wlc_copyto_shm(wlc, seckeys + offset, key->data, D11_MAX_KEY_SIZE);

	/* nothing more to do if WEP */
	if (key->algo_hw == WSEC_ALGO_WEP1 ||
	    key->algo_hw == WSEC_ALGO_WEP128)
		return;

	/* write TKIP MIC key */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		struct scb *scb = NULL;

		if (rcmta_index >= WSEC_MAX_DEFAULT_KEYS) {
			/* BSSCFG BCMC key? */
			if (ETHER_ISNULLADDR(&key->ea)) {
				scb = WLC_BCMCSCB_GET(wlc, bsscfg);
				ASSERT(NULL != scb);
			} else
				scb = wlc_scbfind(wlc, &key->ea);

		}

		offset = rcmta_index * 2 * TKIP_MIC_SIZE;

		/* write transmit MIC key */
		wlc_copyto_shm(wlc, tkmickeys + offset,
		               key->data + wlc_wsec_tx_tkmic_offset(wlc->pub, bsscfg, scb),
		               TKIP_MIC_SIZE);

		/* write receive MIC key */
		wlc_copyto_shm(wlc, tkmickeys + offset + TKIP_MIC_SIZE,
		               key->data + wlc_wsec_rx_tkmic_offset(wlc->pub, bsscfg, scb),
		               TKIP_MIC_SIZE);
	}

	/* All keys are programmed in the hardware */
	/* write phase1 key and TSC into TSC/TTAK table */
	offset = (rcmta_index) * (TKHASH_P1_KEY_SIZE + 4);
	if (key->algo == CRYPTO_ALGO_TKIP) {
		/* phase1 key is an array of uint16s so write to shm by
		 * uint16 instead of a buffer copy
		 */
		for (j = 0; j < TKHASH_P1_KEY_SIZE; j += 2) {
			v = key->tkip_rx.phase1[j/2];
			wlc_write_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset + j, v);
		}
		v = key->tkip_rx_iv32 & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		              TKHASH_P1_KEY_SIZE, v);
		v = ((key->tkip_rx_iv32 & 0xffff0000) >> 16) & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		              TKHASH_P1_KEY_SIZE + 2, v);
	} else if (key->algo_hw == WSEC_ALGO_AES) {
		v = key->rxiv[0].hi & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		              TKHASH_P1_KEY_SIZE, v);
		v = ((key->rxiv[0].hi & 0xffff0000) >> 16) & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		              TKHASH_P1_KEY_SIZE + 2, v);
	}

	/* Write per-AC TSC/PN (network order PN0...PN5) */
	offset = (rcmta_index) * (TSCPN_BLK_SIZE);
	for (j = 0; j < AC_COUNT; j++) {
		uint32 hi_iv = 0;

		/* Programmed rxivs are the last seen one not the next expected ones */
		if ((key->rxiv[j].hi == 0) && (key->rxiv[j].lo == 0))
			v = 0;
		else
			v = key->rxiv[j].lo - 1;

		wlc_write_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6), v);

		if ((key->rxiv[j].lo == 0) && (key->rxiv[j].hi > 0))
			hi_iv = key->rxiv[j].hi - 1;
		else
			hi_iv = 0;

		v = hi_iv & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6) + 2, v);
		v = ((hi_iv & 0xffff0000) >> 16) & 0xffff;
		wlc_write_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6) + 4, v);
	}

	/* update the MAC address match */
	if (rcmta_index >= WSEC_MAX_DEFAULT_KEYS)
		wlc_key_write_addr(wlc, rcmta_index, &key->ea);
}

/* Update SW key information (TSC/PN) after coming out of wake mode
 * If broadcast key rotation was enabled then update additional key information like key->data
 * and iv
 */
void
wlc_key_sw_wowl_update(wlc_info_t *wlc, wsec_key_t *key, int rcmta_index, wlc_bsscfg_t *bsscfg,
                       bool keyrot)
{
	uint16 offset;
	uint16 seckeys, tkmickeys;
	uint32 high_iv32 = 0;
	uint8 high_ividx = 0;

	uint8 j;

	offset = (rcmta_index) * (TSCPN_BLK_SIZE);
	for (j = 0; j < AC_COUNT; j++) {
		uint16 v;
		uint16 lo_iv;
		uint32 hi_iv;

		/* ucode's ivs store the last frame's IVs. So driver needs to store the
		 * next expected ones
		 */
		lo_iv = wlc_read_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6));
		v = wlc_read_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6) + 2);
		hi_iv = v;
		v = wlc_read_shm(wlc, M_WOWL_TSCPN_BLK + offset + (j * 6) + 4);
		hi_iv |= v << 16;

		if (lo_iv == 0xffff)
			key->rxiv[j].hi = hi_iv + 1;
		else
			key->rxiv[j].hi = hi_iv;
		key->rxiv[j].lo = lo_iv + 1;

		/* Remember tkip_rx_iv32/tkip_rx_ividx HIGHEST value */
		if (high_iv32 < key->rxiv[j].hi) {
			high_iv32 = key->rxiv[j].hi;
			high_ividx = j;
		}
	}

	/* For TKIP, update the ividx */
	if (key->algo == CRYPTO_ALGO_TKIP) {
		key->tkip_rx_iv32 = high_iv32;
		key->tkip_rx_ividx = high_ividx;
	}

	/* With key rotation, the data also might have changed */
	if (keyrot) {
		WL_WOWL(("wl%d: wlc_key_sw_wowl_update: Key %d may have been rotated\n",
		         wlc->pub->unit, rcmta_index));
		offset = rcmta_index * D11_MAX_KEY_SIZE;
		seckeys = wlc_read_shm(wlc, M_WOWL_SECRXKEYS_PTR) * 2;

		wlc_copyfrom_shm(wlc, seckeys + offset, key->data, D11_MAX_KEY_SIZE);

		/* Read TKIP MIC key */
		if (key->algo == CRYPTO_ALGO_TKIP) {
			offset = rcmta_index * 2 * TKIP_MIC_SIZE;

			tkmickeys = wlc_read_shm(wlc, M_WOWL_TKMICKEYS_PTR) * 2;

			/* read transmit MIC key */
			wlc_copyfrom_shm(wlc, tkmickeys + offset,
			                 key->data +
			                 wlc_wsec_tx_tkmic_offset(wlc->pub, bsscfg, NULL),
			                 TKIP_MIC_SIZE);

			/* read receive MIC key */
			wlc_copyfrom_shm(wlc, tkmickeys + offset + TKIP_MIC_SIZE,
			                 key->data +
			                 wlc_wsec_rx_tkmic_offset(wlc->pub, bsscfg, NULL),
			                 TKIP_MIC_SIZE);

			/* Recompute phase 1 */
			tkhash_phase1(key->tkip_rx.phase1,
			              key->data, bsscfg->BSSID.octet, key->tkip_rx_iv32);

		}
	}
}

/* When coming up from Sleep, after broadcast key rotation, the ucode might have installed
 * a new broadcast key that didn't exist when going down. Create key using information from
 * Shared memory and plumb it back to the driver's software data structures
 */
int
wlc_key_sw_wowl_create(wlc_info_t *wlc, int rcmta_index, wlc_bsscfg_t *bsscfg)
{
	wl_wsec_key_t key;
	uint16 v, offset, seckeys;
	uint algo_hw;
	wsec_iv_t key_iv;
	wsec_iv_t *initial_iv = NULL;

	bzero((char *)&key, sizeof(key));

	/* Read the data block and SECKINDXALGO_BLK */
	v = wlc_read_shm(wlc, M_WOWL_SECKINDXALGO_BLK + rcmta_index * 2);

	key.index = (v & SKL_INDEX_MASK) >> SKL_INDEX_SHIFT;

	/* Get the HW algo and convert to wl algo */
	algo_hw = (v & SKL_ALGO_MASK);

	switch (algo_hw) {
	case WSEC_ALGO_OFF:
	case WSEC_ALGO_WEP128:
	case WSEC_ALGO_WEP1:
	case WSEC_ALGO_AES_LEGACY:
	case WSEC_ALGO_NALG:
		return -1;
	case WSEC_ALGO_TKIP:
		key.algo = CRYPTO_ALGO_TKIP;
		key.len = TKIP_KEY_SIZE;

		/* Get rxiv32 */
		offset = (rcmta_index) * (TKHASH_P1_KEY_SIZE + 4);
		/* Read bottom 16 bits */
		v = wlc_read_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		                 TKHASH_P1_KEY_SIZE);
		key_iv.hi = v;
		/* Read top 16 bits */
		v = wlc_read_shm(wlc, M_WOWL_TKIP_TSC_TTAK + offset +
		                 TKHASH_P1_KEY_SIZE + 2);
		key_iv.hi |= v << 16;
		key_iv.lo = 0;
		initial_iv = &key_iv;

		break;
	case WSEC_ALGO_AES:
		key.algo = CRYPTO_ALGO_AES_CCM;
		key.len = AES_KEY_SIZE;
	};

	offset = rcmta_index * D11_MAX_KEY_SIZE;

	seckeys = wlc_read_shm(wlc, M_WOWL_SECRXKEYS_PTR) * 2;

	wlc_copyfrom_shm(wlc, seckeys + offset, &key.data, D11_MAX_KEY_SIZE);

	return wlc_key_insert(wlc, wlc->cfg, key.len, key.index, key.flags,
	                      key.algo, key.data, &key.ea, initial_iv, NULL);
}

#endif /* WOWL */

/* update the hardware with values to match the corresponding wlc->wsec_key */
void
wlc_key_hw_init(wlc_info_t *wlc, int index, wlc_bsscfg_t *bsscfg)
{
	wsec_key_t *key;
	uint offset;
	uint16 v;
	uint rcmta_index;	/* actual index + WSEC_MAX_DEFAULT_KEYS */
	uint skl_index;
	uint8 algo_hw;
#if defined(IBSS_PEER_GROUP_KEY)
	uint grpkey_idx = 0;
#endif

	ASSERT(index < WLC_MAX_WSEC_HW_KEYS(wlc));

	ASSERT(wlc->clk);

	if (!WLC_SW_KEYS(wlc, bsscfg))
		key = WSEC_KEY(wlc, index);
	else
		key = NULL;

	/* Hardware TKIP decryption is currently not supported for the default keys */
	if ((key == NULL) ||
	    (key->algo == CRYPTO_ALGO_TKIP && ETHER_ISNULLADDR(&key->ea)))
		algo_hw = WSEC_ALGO_OFF;
	else
		algo_hw = key->algo_hw;

#if defined(IBSS_PEER_GROUP_KEY)
	rcmta_index = (bsscfg->BSS ? index : WSEC_IBSS_RCMTA_INDEX(index));
	skl_index = rcmta_index;
#else
#ifdef WLP2P
	if (P2P_ENAB(wlc->pub)) {
		if (index >= WSEC_MAX_DEFAULT_KEYS) {
			rcmta_index = (key != NULL ? key->rcmta : RCMTA_SIZE) +
			        WSEC_MAX_DEFAULT_KEYS;
			skl_index = rcmta_index;
		}
		else {
			rcmta_index = RCMTA_SIZE + WSEC_MAX_DEFAULT_KEYS;
			skl_index = index;
		}
	}
	else
#endif
	{
		rcmta_index = index;
		skl_index = index;
	}
#endif /* defined(IBSS_PEER_GROUP_KEY) */

	WL_WSEC(("wl%d: wlc_key_hw_init key %p idx %d rcmta %d skl %u\n",
	         wlc->pub->unit, key, index,
	         rcmta_index >= WSEC_MAX_DEFAULT_KEYS ? rcmta_index - WSEC_MAX_DEFAULT_KEYS :
	         (uint)-1,
	         skl_index));

	/* Clear the MAC address match if this is a paired key.
	 * This protects the ucode or hardware from getting an
	 * address match while we update this key.
	 */
	if (rcmta_index >= WSEC_MAX_DEFAULT_KEYS &&
	    rcmta_index < RCMTA_SIZE + WSEC_MAX_DEFAULT_KEYS)
		wlc_key_write_addr(wlc, rcmta_index, &ether_null);

	/* Set the matching SECALGO table entry (algorithm and table index) */
#if defined(IBSS_PEER_GROUP_KEY)
	if (!bsscfg->BSS) {
		grpkey_idx = (index - WSEC_MAX_DEFAULT_KEYS) / (WSEC_IBSS_MAX_PEERS);
		ASSERT(grpkey_idx < 3);

		v = wlc_read_shm(wlc, (M_SECKINDXALGO_BLK + (skl_index * 2)));
		v &= ~SKL_IBSS_INDEX_MASK;
		v |= skl_index << SKL_IBSS_INDEX_SHIFT;

		switch (grpkey_idx) {
		case 0: /* unicast index */
			v &= ~SKL_ALGO_MASK;
			v |= algo_hw;
			break;
		case 1: /* 1st group key */
			v &= ~(SKL_IBSS_KEYID1_MASK | SKL_IBSS_KEYALGO_MASK);
			v |= algo_hw << SKL_IBSS_KEYALGO_SHIFT;
			if (key)
				v |= key->id << SKL_IBSS_KEYID1_SHIFT;
			break;
		case 2: /* second group key */
			v &= ~(SKL_IBSS_KEYID2_MASK | SKL_IBSS_KEYALGO_MASK);
			v |= algo_hw << SKL_IBSS_KEYALGO_SHIFT;
			if (key)
				v |= key->id << SKL_IBSS_KEYID2_SHIFT;
			break;
		}
	}
	else
#endif /* defined(IBSS_PEER_GROUP_KEY) */
	{
		v = algo_hw | (index << SKL_INDEX_SHIFT);

		if (key != NULL &&
		    bsscfg != wlc->cfg && BSSCFG_STA(bsscfg) && bsscfg->BSS &&
		    !ETHER_ISNULLADDR(&key->ea)) {
			wsec_key_t *grpkey;
			uint i;

			for (i = 0; i < ARRAYSIZE(bsscfg->bss_def_keys); i ++) {
				if ((grpkey = bsscfg->bss_def_keys[i]) != NULL &&
				    grpkey->len > 0) {
					if (grpkey->algo_hw != WSEC_ALGO_TKIP)
						v |= (grpkey->algo_hw << SKL_GRP_ALGO_SHIFT);
					break;
				}
			}
		}
#ifdef BCMWAPI_WPI
		if (key && (key->algo == CRYPTO_ALGO_SMS4) && key->id)
			v |= (1 << SKL_KEYID_SHIFT);
#endif /* BCMWAPI_WPI */
	}

	if (skl_index < M_SECKINDXALGO_BLK_SZ)
		wlc_write_shm(wlc, M_SECKINDXALGO_BLK + skl_index * 2, v);

	/*
	 * Set the default key (i < WSEC_MAX_DEFAULT_KEYS)
	 *	or set the per sta key (i >= WSEC_MAX_DEFAULT_KEYS).
	 */
	offset = index * D11_MAX_KEY_SIZE;

	if (key) {
		/* write the key */
		WL_WSEC(("wl%d.%d: %s: write wsec key index %d to shm 0x%x\n",
		         wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
		         index, (wlc->seckeys + offset)));
		wlc_copyto_shm(wlc, wlc->seckeys + offset, key->data, D11_MAX_KEY_SIZE);

		/* write TKIP MIC key if supported in hardware */
		if (WSEC_HW_TKMIC_KEY(wlc, key, bsscfg)) {
			struct scb *scb = NULL;

			if (index >= WSEC_MAX_DEFAULT_KEYS) {
				if (!ETHER_ISNULLADDR(&key->ea))
					scb = wlc_scbfind(wlc, &key->ea);
			}

			offset = index * 2 * TKIP_MIC_SIZE;

			/* write transmit MIC key */
			wlc_copyto_shm(wlc, wlc->tkmickeys + offset,
				key->data + wlc_wsec_tx_tkmic_offset(wlc->pub, bsscfg, scb),
				TKIP_MIC_SIZE);

			/* write receive MIC key */
			wlc_copyto_shm(wlc, wlc->tkmickeys + offset + TKIP_MIC_SIZE,
				key->data + wlc_wsec_rx_tkmic_offset(wlc->pub, bsscfg, scb),
				TKIP_MIC_SIZE);
		}
#ifdef BCMWAPI_WPI
		else if (key->algo == CRYPTO_ALGO_SMS4) {
			offset = index * SMS4_WPI_CBC_MAC_LEN;
			WL_WSEC(("Updating the WAPI MIC key at offset 0x%02x, len %d\n",
				(wlc->wapimickeys + offset), SMS4_WPI_CBC_MAC_LEN));
			wlc_copyto_shm(wlc, wlc->wapimickeys + offset,
				key->data + SMS4_KEY_LEN,
				SMS4_WPI_CBC_MAC_LEN);
		}
#endif /* BCMWAPI_WPI */
	} else {
		/* zero out key data if no key */
		wlc_set_shm(wlc, wlc->seckeys + offset, 0, D11_MAX_KEY_SIZE);
	}

	/* handle per station keys */
	if (key &&
	    rcmta_index >= WSEC_MAX_DEFAULT_KEYS &&
	    rcmta_index < RCMTA_SIZE + WSEC_MAX_DEFAULT_KEYS) {
#ifndef LINUX_CRYPTO
		if (key->algo == CRYPTO_ALGO_TKIP) {
			uint j;

			/* write phase1 key and TSC into TSC/TTAK table */
			offset = (rcmta_index - WSEC_MAX_DEFAULT_KEYS) * (TKHASH_P1_KEY_SIZE + 4);
			/* phase1 key is an array of uint16s so write to shm by
			 * uint16 instead of a buffer copy
			 */
			for (j = 0; j < TKHASH_P1_KEY_SIZE; j += 2) {
				v = key->tkip_rx.phase1[j/2];
				wlc_write_shm(wlc, M_TKIP_TSC_TTAK + offset + j, v);
			}
			v = key->tkip_rx_iv32 & 0xffff;
			wlc_write_shm(wlc, M_TKIP_TSC_TTAK + offset +
				TKHASH_P1_KEY_SIZE, v);
			v = ((key->tkip_rx_iv32 & 0xffff0000) >> 16) & 0xffff;
			wlc_write_shm(wlc, M_TKIP_TSC_TTAK + offset +
				TKHASH_P1_KEY_SIZE + 2, v);
		}
#endif /* LINUX_CRYPTO */

		/* update the MAC address match */
		wlc_key_write_addr(wlc, rcmta_index, &key->ea);
	}

	/* update ucode flag MHF1_DEFKEYVALID */
	/*
	 * Need to check both soft keys and default WEP keys before
	 * making decision to either set or clear it when updating a default key
	 */
	if (index < WSEC_MAX_DEFAULT_KEYS) {
		uint16 flag = wlc_key_defkeyflag(wlc);
		wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, flag, WLC_BAND_ALL);
		WL_WSEC(("wl%d: wlc_key_hw_init: %s default key %d, %s MHF1_DEFKEYVALID\n",
		         wlc->pub->unit, key ? "insert" : "delete", index, flag ? "set" : "clear"));
	}
}

/* update the pairwise key address for a hardware key */
static void
wlc_key_write_addr(wlc_info_t *wlc, int i, const struct ether_addr *ea)
{
	WL_TRACE(("wl%d: wlc_key_write_addr key %d\n", wlc->pub->unit, i));

	ASSERT(i >= WSEC_MAX_DEFAULT_KEYS);
	ASSERT(i < RCMTA_SIZE + WSEC_MAX_DEFAULT_KEYS);

	wlc_set_rcmta(wlc, (i - WSEC_MAX_DEFAULT_KEYS), ea);
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
static void
wlc_key_dump_hw_ex(wlc_info_t *wlc, struct bcmstrbuf *b, bool ibss_mode,
	int8 rcmta_index, int8 skl_index, int8 key_index)
{
	uint8 key[D11_MAX_KEY_SIZE];
	uint16 algo_hw = 0;
	int j;
	struct ether_addr ea;
#ifndef LINUX_CRYPTO
	uint16 phase1[TKHASH_P1_KEY_SIZE/2];
	uint8 mic[TKIP_MIC_SIZE * 2];
	uint32 iv32 = 0;
#endif
	int kl = 0;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 wsec = 0;

	/* read RCMTA */
	if (rcmta_index != -1)
		wlc_get_rcmta(wlc, rcmta_index, &ea);

	/* read M_SECKINDXALGO_BLK */
	if (skl_index != -1) {
		wsec = wlc_read_shm(wlc, M_SECKINDXALGO_BLK + skl_index * 2);

		algo_hw = (wsec & SKL_ALGO_MASK) >> SKL_ALGO_SHIFT;
		switch (algo_hw) {
		case WSEC_ALGO_WEP1:
			kl = WEP1_KEY_SIZE;
			break;
		case WSEC_ALGO_WEP128:
			kl = WEP128_KEY_SIZE;
			break;
		case WSEC_ALGO_TKIP:
			kl = D11_MAX_KEY_SIZE;
			break;
		case WSEC_ALGO_AES:
			kl = D11_MAX_KEY_SIZE;
			break;
		case WSEC_ALGO_AES_LEGACY:
			kl = D11_MAX_KEY_SIZE;
			break;
		default:
			kl = 0;
			break;
		}

	}

	/* read key data */
	if (key_index != -1) {
		uint offset = key_index * D11_MAX_KEY_SIZE;
		wlc_copyfrom_shm(wlc, wlc->seckeys + offset, key, D11_MAX_KEY_SIZE);
	}

#ifndef LINUX_CRYPTO
	/* read tkip phase1 and iv32 */
	if (rcmta_index != -1 && algo_hw == WSEC_ALGO_TKIP &&
	    key_index >= WSEC_MAX_DEFAULT_KEYS) {
		uint offset = rcmta_index * (TKHASH_P1_KEY_SIZE + 4);
		for (j = 0; j < TKHASH_P1_KEY_SIZE/2; j ++)
			phase1[j] = wlc_read_shm(wlc, M_TKIP_TSC_TTAK + offset + j * 2);
		iv32 = wlc_read_shm(wlc, M_TKIP_TSC_TTAK + offset + TKHASH_P1_KEY_SIZE);
		iv32 += wlc_read_shm(wlc, M_TKIP_TSC_TTAK + offset + TKHASH_P1_KEY_SIZE + 2) << 16;
	}
	/* read tkip mic keys */
	if (algo_hw == WSEC_ALGO_TKIP) {
		uint offset = key_index * TKIP_MIC_SIZE * 2;
		wlc_copyfrom_shm(wlc, wlc->tkmickeys + offset, mic, TKIP_MIC_SIZE * 2);
	}
#endif /* LINUX_CRYPTO */

	/* format rcmta index and mac address */
	if (rcmta_index != -1)
		bcm_bprintf(b, "RCMTA:%02d %s ", rcmta_index, bcm_ether_ntoa(&ea, eabuf));

	/* format M_SECKINDXALGO_BLK */
	if (skl_index != -1)
		bcm_bprintf(b, "SKL:%02u: 0x%04X ", skl_index, wsec);

	/* format key index */
	if (key_index != -1)
		bcm_bprintf(b, "KEY:%02u ", key_index);

	/* format key type */
	if (skl_index != -1)
		bcm_bprintf(b, "ALGO:%s ", wsec_algo_names[algo_hw]);

#ifndef LINUX_CRYPTO
	/* format tkip phase1 and iv32 */
	if (rcmta_index != -1 && algo_hw == WSEC_ALGO_TKIP &&
	    key_index >= WSEC_MAX_DEFAULT_KEYS) {
		bcm_bprintf(b, "PHASE1:");
		for (j = 0; j < TKHASH_P1_KEY_SIZE/2; j ++)
			bcm_bprintf(b, "%04X", phase1[j]);
		bcm_bprintf(b, " ");
		bcm_bprintf(b, "IV32:%08X ", iv32);
	}
	/* format tkip mic keys */
	if (algo_hw == WSEC_ALGO_TKIP) {
		bcm_bprintf(b, "MIC:");
		for (j = 0; j < TKIP_MIC_SIZE * 2; j ++)
			bcm_bprintf(b, "%02X", mic[j]);
		bcm_bprintf(b, " ");
	}
#endif /* LINUX_CRYPTO */

	if (key_index != -1) {
		if (rcmta_index != -1)
			bcm_bprintf(b, "UCKEY:");
		else
			bcm_bprintf(b, "MCKEY:");
		for (j = 0; j < kl; j ++)
			bcm_bprintf(b, "%02X", key[j]);
		if (j < D11_MAX_KEY_SIZE) {
			bcm_bprintf(b, "[");
			for (; j < D11_MAX_KEY_SIZE; j ++)
				bcm_bprintf(b, "%02X", key[j]);
			bcm_bprintf(b, "]");
		}
	}
	bcm_bprintf(b, "\n");

	if (ibss_mode &&
	    skl_index != -1 &&
	    key_index >= WSEC_MAX_DEFAULT_KEYS && key_index < WSEC_IBSS_MAX_PEERS) {
		uint16 ibss_mc_algo =
		        (wsec & SKL_IBSS_KEYALGO_MASK) >> SKL_IBSS_KEYALGO_SHIFT;

		bcm_bprintf(b, "          ID1:%d ID2:%d MC ALGO:%s\n",
		            (wsec & SKL_IBSS_KEYID1_MASK) >> SKL_IBSS_KEYID1_SHIFT,
		            (wsec & SKL_IBSS_KEYID2_MASK) >> SKL_IBSS_KEYID2_SHIFT,
		            wsec_algo_names[ibss_mc_algo]);
	}
}

int
wlc_key_dump_hw(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	osl_t *osh;
	uint8 key[D11_MAX_KEY_SIZE];
	uint offset;
	bool ibss_mode;
	int8 i, j;
	struct ether_addr ea;
	bool key_dumped = FALSE;
	int8 skl_index;
	int8 key_index;
	uint8 sbmp[CEIL(M_SECKINDXALGO_BLK_SZ, NBBY)];
	uint8 kbmp[CEIL(WSEC_MAX_RCMTA_KEYS, NBBY)];
	uint16 wsec;

	osh = wlc->osh;

	/* check the case where the hardware is not in a up state */
	if (!wlc->pub->up)
		return BCME_NOTUP;

	if (D11REV_LE(wlc->pub->corerev, 4))
		return BCME_ERROR;

	ibss_mode = (0 != (MHF4_IBSS_SEC & wlc_mhf_get(wlc, MHF4, WLC_BAND_AUTO)));

	bcm_bprintf(b, "IBSS MODE: %s\n", ibss_mode?"ON":"OFF");

	bzero(sbmp, sizeof(sbmp));
	bzero(kbmp, sizeof(kbmp));

	/* dump the RCMTA/M_SECKINDXALGO_BLK/M_SECRXKEYS_PTR entries */
	for (i = 0; i < RCMTA_SIZE; i ++) {

		/* read RCMTA */
		wlc_get_rcmta(wlc, i, &ea);
		if (ETHER_ISNULLADDR(&ea))
			continue;

		skl_index = i + WSEC_MAX_DEFAULT_KEYS;

		/* read M_SECKINDXALGO_BLK */
		wsec = wlc_read_shm(wlc, M_SECKINDXALGO_BLK + skl_index * 2);

		if (!ibss_mode)
			key_index = (wsec & SKL_INDEX_MASK) >> SKL_INDEX_SHIFT;
		else
			key_index = (wsec & SKL_IBSS_INDEX_MASK) >> SKL_IBSS_INDEX_SHIFT;

		/* read M_SECRXKEYS_PTR and dump it */
		wlc_key_dump_hw_ex(wlc, b, ibss_mode, i, skl_index, key_index);

		sbmp[skl_index / 8] |= 1 << (skl_index % 8);
		kbmp[key_index / 8] |= 1 << (key_index % 8);

		key_dumped = TRUE;
	}

	/* dump M_SECKINDXALGO/M_SECRXKEYS_PTR entries */
	for (skl_index = 0; skl_index < M_SECKINDXALGO_BLK_SZ; skl_index ++) {

		/* skip M_SECKINDXALGO entries we already handled */
		if (sbmp[skl_index / 8] & (1 << (skl_index % 8)))
			continue;

		/* read M_SECKINDXALGO_BLK */
		wsec = wlc_read_shm(wlc, M_SECKINDXALGO_BLK + skl_index * 2);
		if (wsec == 0)
			continue;

		if (!ibss_mode)
			key_index = (wsec & SKL_INDEX_MASK) >> SKL_INDEX_SHIFT;
		else
			key_index = (wsec & SKL_IBSS_INDEX_MASK) >> SKL_IBSS_INDEX_SHIFT;

		/* read M_SECRXKEYS_PTR and dump it */
		wlc_key_dump_hw_ex(wlc, b, ibss_mode, -1, skl_index, key_index);

		sbmp[skl_index / 8] |= 1 << (skl_index % 8);
		kbmp[key_index / 8] |= 1 << (key_index % 8);

		key_dumped = TRUE;
	}

	/* dump M_SECRXKEYS_PTR entries */
	for (key_index = 0; key_index < WLC_MAX_WSEC_HW_KEYS(wlc); key_index ++) {

		/* skip M_SECRXKEYS_PTR entries we already handled */
		if (kbmp[key_index / 8] & (1 << (key_index % 8)))
			continue;

		/* read key data */
		offset = key_index * D11_MAX_KEY_SIZE;
		wlc_copyfrom_shm(wlc, wlc->seckeys + offset, key, D11_MAX_KEY_SIZE);

		for (j = 0; j < D11_MAX_KEY_SIZE; j++) {
			if (key[j] != 0)
				break;
		}
		if (j == D11_MAX_KEY_SIZE)
			continue;

		/* read M_SECRXKEYS_PTR and dump it */
		wlc_key_dump_hw_ex(wlc, b, ibss_mode, -1, -1, key_index);

		key_dumped = TRUE;
	}

	if (!key_dumped)
		bcm_bprintf(b, "No keys in HW\n");

	return 0;
}

static void
wlc_key_print(wsec_key_t *key, struct bcmstrbuf *b)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	uint j;

	bcm_bprintf(b, "%d %s ID:%d ALGO:%s ",
	            key->idx, bcm_ether_ntoa(&key->ea, eabuf),
	            key->id, bcm_crypto_algo_name(key->algo));
	bcm_bprintf(b, "KEY: ");
	for (j = 0; j < key->len; j ++)
		bcm_bprintf(b, "%02X", key->data[j]);
	bcm_bprintf(b, "\n");
}

int
wlc_key_dump_sw(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	uint i;

	bcm_bprintf(b, "wsec_swkeys %d\n", wlc->wsec_swkeys);
	bcm_bprintf(b, "wsec_keys:\n");
	for (i = 0; i < ARRAYSIZE(wlc->wsec_keys); i ++)
		if (wlc->wsec_keys[i] != NULL)
			wlc_key_print(wlc->wsec_keys[i], b);

	return 0;
}
#endif /* BCMDBG || BCMDBG_DUMP */