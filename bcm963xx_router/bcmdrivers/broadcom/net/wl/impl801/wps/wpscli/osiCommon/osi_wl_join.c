/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: osi_wl_join.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * WL common functions -- Implement wl-related methods to communicate with driver,
 *    -- codes are created from linux\wpscli_wlan.c: refer join_network() and
 *       join_network_with_bssid() from linux\wpscli_wlan.c
 *    -- use 'wpsosi_' prefix to indicate the function can be shared across platforms.
 *
 */
#include <ctype.h>
#include "wpscli_osl.h"
#include "bcmutils.h"
#include "wlioctl.h"
#include "wps_wl.h"
#include "tutrace.h"
#include "wpscli_common.h"

/* enable structure packing */
#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

/*--------------------------------------------------------------------
 *
 * apply security for a STA
 *
 *--------------------------------------------------------------------
*/
int wpsosi_common_apply_sta_security(uint8 wsec)
{
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	int ret = 0;

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
	{
		TUTRACE((TUTRACE_INFO,
			"wpsosi_common_apply_sta_security, WLC_SET_INFRA (%d) fail, ret=%d\n",
			infra, ret));
		return ret;
	}

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
	{
		TUTRACE((TUTRACE_INFO,
			"wpsosi_common_apply_sta_security, WLC_SET_AUTH (%d) fail, ret=%d\n",
			auth, ret));
		return ret;
	}

	/* set wsec mode */
	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */

	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
	{
		TUTRACE((TUTRACE_INFO,
			"wpsosi_common_apply_sta_security, WLC_SET_WSEC (%d) fail, ret=%d\n",
			wsec, ret));
		return ret;
	}

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
	{
		TUTRACE((TUTRACE_INFO,
			"wpsosi_common_apply_sta_security, WLC_SET_WPA_AUTH(%d) fail, ret=%d\n",
			wpa_auth, ret));
		return ret;
	}

	return 0;

}

/*--------------------------------------------------------------------
 *
 * Join the network (using ssid)
 *
 *--------------------------------------------------------------------
*/
brcm_wpscli_status wpsosi_join_network(char* ssid, uint8 wsec)
{
	int ret = 0, retry;
	wlc_ssid_t ssid_t;
	char bssid[6];

	TUTRACE((TUTRACE_INFO, "Entered: wpsosi_join_network. ssid=[%s] wsec=%d\n", ssid, wsec));

	if ((ret = wpsosi_common_apply_sta_security(wsec)) != 0)
	{
		TUTRACE((TUTRACE_INFO, "wpsosi__join_network: apply_sta_security failed\n"));
		return -1;
	}

	ssid_t.SSID_len = strlen(ssid);
	strncpy((char *)ssid_t.SSID, ssid, ssid_t.SSID_len);

	/* set ssid */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
		(const char *)&ssid_t, sizeof(wlc_ssid_t))) == 0) {
		/* Poll for the results once a second until we got BSSID */
		for (retry = 0; retry < WLAN_CONNECTION_TIMEOUT; retry++) {

			wpscli_sleep(1000);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, bssid, 6);

			/* break out if the scan result is ready */
			if (ret == 0)
				break;

			if (retry != 0 && retry % 10 == 0) {
				if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
					(const char *)&ssid_t, sizeof(wlc_ssid_t))) < 0) {
					TUTRACE((TUTRACE_INFO,
						"wpsosi_join_network: Exiting. ret=%d\n", ret));
					return ret;
				}
			}
		}
	}

	TUTRACE((TUTRACE_INFO, "wpsosi_join_network: Exiting. ret=%d\n", ret));
	return ret;
}

/*--------------------------------------------------------------------
 *
 * Join the network
 *
 *--------------------------------------------------------------------
*/
brcm_wpscli_status wpsosi_join_network_with_bssid(const char* ssid, uint8 wsec, const uint8 *bssid,
	int num_channels, uint8 *channels)
{
#if !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE)
	return (wpsosi_join_network(ssid, wsec));
#else
	int ret = 0, retry;
	uint8 associated_bssid[6];
	int join_params_size;
	wl_join_params_t *join_params;
	wlc_ssid_t *ssid_t;
	wl_assoc_params_t *params_t;
	int i;
	int sleep = 500;	/* msec */


	TUTRACE((TUTRACE_INFO,
		"wpsosi_join_network_with_bssid: ssid=%s, wsec=%d bssid=%02x:%02x:%02x:%02x:%02x:%02x, %d channels\n",
		ssid, wsec, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5],
		num_channels));

	if ((ret = wpsosi_common_apply_sta_security(wsec)) != 0)
	{
		TUTRACE((TUTRACE_INFO, "wpsosi_join_network_with_bssid: apply_sta_security failed\n"));
		return -1;
	}

	join_params_size = WL_JOIN_PARAMS_FIXED_SIZE + num_channels * sizeof(chanspec_t);
	if ((join_params = (wl_join_params_t *) malloc(join_params_size)) == NULL) {
		TUTRACE((TUTRACE_INFO, "wpsosi_join_network_with_bssid: malloc failed"));
		return -1;
	}
	memset(join_params, 0, join_params_size);
	ssid_t = &join_params->ssid;
	params_t = &join_params->params;

	/* ssid */
	ssid_t->SSID_len = strlen(ssid);
	strncpy((char *)ssid_t->SSID, ssid, ssid_t->SSID_len);

	/* bssid (if any) */
	if (bssid)
		memcpy(&params_t->bssid, bssid, ETHER_ADDR_LEN);
	else
		memcpy(&params_t->bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* channel spec */
	params_t->chanspec_num = num_channels;
	for (i = 0; i < params_t->chanspec_num; i++) {
		uint16 channel, band, bw, ctl_sb;
		chanspec_t chspec;
		channel = channels[i];
		band = (channel <= CH_MAX_2G_CHANNEL) ? WL_CHANSPEC_BAND_2G
			: WL_CHANSPEC_BAND_5G;
		bw = WL_CHANSPEC_BW_20;
		ctl_sb = WL_CHANSPEC_CTL_SB_NONE;
		chspec = (channel | band | bw | ctl_sb);
		params_t->chanspec_list[i] = chspec;
	}

	/* set ssid */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
		(const char *)join_params, join_params_size)) == 0) {

		TUTRACE((TUTRACE_INFO,
			"wpsosi_join_network_with_bssid, after WLC_SET_SSID, poll for the results\n"));

		/* Poll for the results once a second until we got BSSID */
		for (retry = 0; retry < (WLAN_CONNECTION_TIMEOUT * 1000 / sleep); retry++) {

			wpscli_sleep(sleep);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, (char *)associated_bssid, 6);

			/* break out if the scan result is ready */
			if (ret == 0)
			{
				TUTRACE((TUTRACE_INFO,
					"wpsosi_join_network_with_bssid(retry=%d) succeeds, associated_bssid=%02x:%02x:%02x:%02x:%02x:%02x\n", 
					retry,
					associated_bssid[0], associated_bssid[1], associated_bssid[2],
					associated_bssid[3], associated_bssid[4], associated_bssid[5]));
				break;
			}

			TUTRACE((TUTRACE_INFO,
				"wpsosi_join_network_with_bssid,(retry=%d) WLC_GET_BSSID failed, ret=%d\n",
				retry, ret));

			if (retry != 0 && retry % 10 == 0) {
				TUTRACE((TUTRACE_INFO, "wpsosi_join_network_with_bssid: WLC_SET_SSID retry=%d\n", retry / 10));				
				if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
					(const char *)join_params, join_params_size)) < 0) {
					TUTRACE((TUTRACE_INFO, "wpsosi_join_network_with_bssid: WLC_SET_SSID ret=%d\n", ret));
					free(join_params);
					return ret;
				}
			}
		}
	}

	TUTRACE((TUTRACE_INFO, "wpsosi_join_network_with_bssid: exit. ret=%d\n", ret));
	free(join_params);
	return ret;
#endif /* !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE) */
}
