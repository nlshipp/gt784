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
#include "wpscli_wlan_vista.h"
#include "osi_wl_join.h"

// initialize wlan calls
brcm_wpscli_status wpscli_wlan_open()
{
	TCHAR szAdapterName[MAX_PATH] = { _T('\0') };

	if(wps_GetOSVer() == VER_VISTA)
		return wpscli_wlan_open_vista(getShortAdapterName());

	return WPS_STATUS_SUCCESS;
}

// uninitialize wlan calls
brcm_wpscli_status wpscli_wlan_close()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	if(wps_GetOSVer() == VER_VISTA)
		status = wpscli_wlan_close_vista();

	return status;
}

// make a wlan connection. return immediately. status and result will be returned via wpscli_wlh_register_ntf asynchronously
brcm_wpscli_status wpscli_wlan_connect(const char* ssid, uint8 wsec, const char *bssid,
	int num_channels, uint8 *channels)
{
	if(wps_GetOSVer() == VER_VISTA)
	{
		if (bssid != (const char *) NULL)
		{
			TUTRACE((TUTRACE_INFO, "wpscli_wlan_connect: bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
				(uint8) bssid[0], (uint8) bssid[1], 
				(uint8) bssid[2], (uint8) bssid[3],
				(uint8) bssid[4], (uint8) bssid[5]));

			return wpsosi_join_network_with_bssid(ssid, wsec, (const uint8 *)bssid, num_channels, channels);
		}
		else
			return wpscli_wlan_connect_vista(ssid, wsec);
	}

	return 0;
}

// disconnect wlan connection
brcm_wpscli_status wpscli_wlan_disconnect()
{
	if(wps_GetOSVer() == VER_VISTA)
		return wpscli_wlan_disconnect_vista();

	return 0;
}

// scan APs. return immediately. status and result will be returned via wpscli_wlh_register_ntf asynchronously
brcm_wpscli_status wpscli_wlan_scan(wl_scan_results_t *ap_list, uint32 buf_size)
{
	if(wps_GetOSVer() == VER_VISTA)
		return wpscli_wlan_scan_vista(ap_list, buf_size);

	return 0;
}
