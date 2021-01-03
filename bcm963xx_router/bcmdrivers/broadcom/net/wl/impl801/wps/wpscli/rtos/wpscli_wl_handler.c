/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl_handler.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: RTOS functions handling device
 *
 */

#include "stdio.h"
#include "stdlib.h"
#include "wpscli_osl.h"
#include "tutrace.h"
#include "wps_enr_osl.h"

void wps_ifidx(unsigned int ifidx);
void wps_restore_ifidx(void);
int wps_wl_ioctl(int cmd, void *buf, int len, bool set);

brcm_wpscli_status wpscli_wlh_open(const char *adapter_name, int is_virutal)
{
	wps_ifidx(dhd_ifname2idx(0, adapter_name));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_close()
{
	wps_restore_ifidx();
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_get_adapter_mac(uint8 *adapter_mac)
{
	char szDebug[256] = { 0 };

	TUTRACE((TUTRACE_INFO, "Entered : wpscli_wlh_get_adapter_mac.\n"));

	if(adapter_mac == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_wlh_get_adapter_mac : Invalid NULL adapter_mac\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if(wps_osl_get_mac(adapter_mac) < 0) {
		printf("ioctl to get hwaddr failed.\n");
		return WPS_STATUS_SYSTEM_ERR;
	}

	sprintf(szDebug, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", adapter_mac[0],adapter_mac[1],
		adapter_mac[2],adapter_mac[3],adapter_mac[4],adapter_mac[5]);

	TUTRACE((TUTRACE_INFO, "Exit : wpscli_wlh_get_adapter_mac. adapter_mac [%s]\n", szDebug));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_ioctl_set(int cmd, const char *data, ulong data_len)
{
	brcm_wpscli_status status;
	int err;

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_set: Entered.\n"));

	err = wps_wl_ioctl(cmd, (void *)data, data_len, TRUE);  // Set
	status = (err==0 ? WPS_STATUS_SUCCESS : WPS_STATUS_IOCTL_SET_FAIL);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_set: Exiting. status=%d\n", status));
	return status;
}

brcm_wpscli_status wpscli_wlh_ioctl_get(int cmd, char *buf, ulong buf_len)
{
	brcm_wpscli_status status;
	int err;
	
	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_get: Entered.\n"));

	err = wps_wl_ioctl((uint)cmd, buf, buf_len, FALSE);  // Get

	status = (err==0 ? WPS_STATUS_SUCCESS : WPS_STATUS_IOCTL_GET_FAIL);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_get: Exiting. status=%d\n", status));
	return status;
}
