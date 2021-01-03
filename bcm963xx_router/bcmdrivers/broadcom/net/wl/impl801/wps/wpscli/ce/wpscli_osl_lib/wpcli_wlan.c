/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpcli_wlan.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement the general WLAN related functionalities
 *
 */
#include "stdafx.h"
#include "wpscli_osl_common.h"

/* initialize wlan calls and also register cabllback notification function */
extern TCHAR* getShortAdapterName();

brcm_wpscli_status wpscli_wlan_open()
{
	TCHAR szAdapterName[MAX_PATH] = { _T('\0') };

	return WPS_STATUS_SUCCESS;
}

/* uninitialize wlan calls */
brcm_wpscli_status wpscli_wlan_close()
{
	return WPS_STATUS_SUCCESS;
}

/*
 * make a wlan connection. return immediately. status and result will be returned
 *	via wpscli_wlh_register_ntf asynchronously
 */
brcm_wpscli_status wpscli_wlan_connect(const char* ssid, uint8 wsec, const char *bssid,
	int num_channels, uint8 *channels)
{
	brcm_wpscli_status status;
	wlc_ssid_t ssid_t;
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char associated_bssid[6];
	int retry = 0;
	wl_join_params_t join_params;

	/* Is it OK as wpscli_wlh_ioctl_set will add WL_OID_BASE??? */

	/* set infrastructure mode */
	if ((status = wpscli_wlh_ioctl_set(WLC_SET_INFRA, &infra, sizeof(int)))
		!= WPS_STATUS_SUCCESS)
		return status;

	/* set authentication mode */
	if ((status = wpscli_wlh_ioctl_set(WLC_SET_AUTH, &auth, sizeof(int)))
		!= WPS_STATUS_SUCCESS)
		return status;

	/* set wsec mode. If wep bit is on, pick any WPA encryption type to allow association. */
	if (wsec)
	    wsec = 2; /* TKIP */

	if ((status = wpscli_wlh_ioctl_set(WLC_SET_WSEC, &wsec, sizeof(int)))
		!= WPS_STATUS_SUCCESS)
		return status;

	/* set WPA_auth mode */
	if ((status = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH, &wpa_auth, sizeof(wpa_auth)))
		!= WPS_STATUS_SUCCESS)
		return status;

	/* set ssid */
	ssid_t.SSID_len = strlen(ssid);
	strncpy((char *)ssid_t.SSID, ssid, ssid_t.SSID_len);

	if ((status = wpscli_wlh_ioctl_set(WLC_SET_SSID, &ssid_t, sizeof(wlc_ssid_t)))
		!= WPS_STATUS_SUCCESS)
		return status;

	/* How to connect and poll the association status??? */

	/* Poll for the results once a second until we got BSSID */
	for (retry = 0; retry < WLAN_CONNECTION_TIMEOUT; retry++) {

		wpscli_sleep(1000);

		/* Query the association status by retrieving the bssid value */
		status = wpscli_wlh_ioctl_get(WLC_GET_BSSID, associated_bssid, 6);

		/* break out if the scan result is ready */
		if (status == WPS_STATUS_SUCCESS)
			break;
/*
		if (retry != 0 && retry % 10 == 0) {
			if ((status = wpscli_wlh_ioctl_set(WLC_SET_SSID, &join_params,
				sizeof(wl_join_params_t))) != WPS_STATUS_SUCCESS) {
				TUTRACE((TUTRACE_INFO, "join_network_with_bssid: Exiting. ret=%d\n",
					ret));
				return ret;
			}
		}
*/
	}

	return status;
}

/* disconnect wlan connection */
brcm_wpscli_status wpscli_wlan_disconnect()
{
	wpscli_wlh_ioctl_set(WLC_DISASSOC, NULL, 0);

	return 0;
}

/*
 * Scan APs. return immediately. status and result will be returned via wpscli_wlh_register_ntf
 * asynchronously
 */
brcm_wpscli_status wpscli_wlan_scan(wl_scan_results_t *ap_list, uint32 buf_size)
{
	brcm_wpscli_status status;
	wl_scan_params_t* params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);

	TUTRACE((TUTRACE_ERR, "wpscli_wlan_scan: Entered.\n"));

	if (ap_list == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_wlan_scan: Exiting. Invalied NULL ap_list"
			" parameter is passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if (buf_size == 0) {
		TUTRACE((TUTRACE_ERR, "wpscli_wlan_scan: Exiting. buf_size is 0.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	params = (wl_scan_params_t*)malloc(params_size);
	if (params == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_wlan_scan: Exiting. Failed to allocate memory.\n"));
		return WPS_STATUS_SYSTEM_ERR;
	}

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = -1;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	/* Make sure to pass in "WL_OID_BASE+WLC_SCAN" or "WLC_SCAN" */
	status = wpscli_wlh_ioctl_set(WLC_SCAN, params, params_size);
	if (status == WPS_STATUS_SUCCESS) {
		/* Wait for 3000 ms for scanning to finish */
		wpscli_sleep(3000);

		/* Retrieve the scan result */
		ap_list->buflen = buf_size;
		status = wpscli_wlh_ioctl_get(WLC_SCAN_RESULTS, ap_list, buf_size);
	}

	TUTRACE((TUTRACE_INFO, "wpscli_wlan_scan: Exiting. status=%d.\n", status));
	return status;
}
