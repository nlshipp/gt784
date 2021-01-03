/*
 * WPS IE
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ie.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <bn.h>
#include <wps_dh.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#ifdef __ECOS
#include <sys/socket.h>
#endif
#include <net/if.h>
#include <wps_wl.h>
#include <tutrace.h>
#include <shutils.h>
#include <wps_upnp.h>
#include <wps_ui.h>
#include <wps_aplockdown.h>
#include <wps_ie.h>

/* under m_xxx in mbuf.h */


static int
wps_set_prb_rsp_IE(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	uint32 ret = 0;
	char prefix[] = "wlXXXXXXXXXX_";
	BufferObj *bufObj;
	uint16 data16;
	uint8 *p_data8;
	uint8 data8;
	char *pc_data;
	char tmp[100];
	uint8 respType = WPS_MSGTYPE_AP_WLAN_MGR;
	uint16 primDeviceCategory = 6;
	uint32 primDeviceOui = 0x0050F204;
	uint16 primDeviceSubCategory = 1;
	CTlvPrimDeviceType tlvPrimDeviceType;
	uint8 version = ssrmsg ? ssrmsg->version.m_data : 0x10;
	uint8 selreg = ssrmsg ? ssrmsg->selReg.m_data : 0;

	bufObj = buffobj_new();
	if (bufObj == NULL)
		return 0;

	snprintf(prefix, sizeof(prefix), "%s_", ifname);

	/* Create the IE */
	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (ssrmsg) {
		data8 = ssrmsg->scState.m_data;
	}
	else {
		sprintf(tmp, "ess%d_wps_oob", ess_id);
		pc_data = wps_safe_get_conf(tmp);
		if (!strcmp(pc_data, "disabled") || wps_ui_is_pending())
			data8 = WPS_SCSTATE_CONFIGURED;
		else
			data8 = WPS_SCSTATE_UNCONFIGURED;
	}

	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false */
	if (wps_aplockdown_islocked()) {
		data8 = 0x1;
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &data8, WPS_ID_AP_SETUP_LOCKED_S);
	}

	/* Selected Registrar */
	tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &selreg, WPS_ID_SEL_REGISTRAR_S);

	/* optional if true */
	if (selreg) {
		/* Device Password ID */
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj,
			&ssrmsg->devPwdId.m_data, WPS_ID_DEVICE_PWD_ID_S);
		/* Selected Registrar Config Methods */
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&ssrmsg->selRegCfgMethods.m_data, WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* Response Type */
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &respType, WPS_ID_RESP_TYPE_S);

	/* UUID */
	p_data8 = wps_get_uuid();

	if (WPS_MSGTYPE_REGISTRAR == respType)
		data16 = WPS_ID_UUID_R; /* This case used for Registrars using ad hoc mode */
	else
		data16 = WPS_ID_UUID_E; /* This is the AP case, so use UUID_E. */
	tlv_serialize(data16, bufObj, p_data8, 16); /* change 16 to SIZE_UUID */

	/* Manufacturer */
	pc_data = wps_safe_get_conf("wps_mfstring");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, pc_data, data16);

	/* Model Name */
	pc_data = wps_safe_get_conf("wps_modelname");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, pc_data, data16);

	/* Model Number */
	pc_data = wps_safe_get_conf("wps_modelnum");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, pc_data, data16);

	/* Serial Number */
	pc_data = wps_safe_get_conf("boardnum");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, pc_data, data16);

	/* Primary Device Type */
	/* This is a complex TLV, so will be handled differently */
	tlvPrimDeviceType.categoryId = primDeviceCategory;
	tlvPrimDeviceType.oui = primDeviceOui;
	tlvPrimDeviceType.subCategoryId = primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Name */
	pc_data = wps_safe_get_conf("wps_device_name");
	data16 = strlen(pc_data);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, pc_data, data16);

	/* Config Methods */
	pc_data = wps_get_conf("wps_config_method");
	data16 = pc_data ? strtoul(pc_data, NULL, 16) : 0x88;
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* RF Bands - optional */
	sprintf(tmp, "ess%d_band", ess_id);
	pc_data = wps_safe_get_conf(tmp);
	data8 = atoi(pc_data);
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);


	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ifname, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_WPS);
	if (ret != 0) {
		TUTRACE((TUTRACE_ERR, "set probrsp IE on %s failed, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}
	else {
		TUTRACE((TUTRACE_ERR, "set probrsp IE on %s successful, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);

	if (!strcmp(wps_safe_get_conf(strcat_r(prefix, "wep", tmp)), "enabled")) {
		/* set STATIC WEP key OUI IE to frame */
		data8 = 0;
		ret = wps_wl_set_wps_ie(ifname, &data8, 1,
			WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_PROVISION_STATIC_WEP);
		if (ret != 0)
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s failed\n",
				ifname));
		else
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s success\n",
				ifname));
	}
	else
		ret = wps_wl_del_wps_ie(ifname,
			WPS_IE_TYPE_SET_PROBE_RESPONSE_IE, OUITYPE_PROVISION_STATIC_WEP);

	return ret;
}

static int
wps_set_beacon_IE(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	uint32 ret;
	char prefix[] = "wlXXXXXXXXXX_";
	uint8 data8;
	uint8 *p_data8;
	char *pc_data;
	char tmp[100];
	uint8 version = ssrmsg ? ssrmsg->version.m_data : 0x10;
	uint8 selreg = ssrmsg ? ssrmsg->selReg.m_data : 0;

	BufferObj *bufObj;

	bufObj = buffobj_new();
	if (bufObj == NULL)
		return 0;

	snprintf(prefix, sizeof(prefix), "%s_", ifname);

	/* Create the IE */
	/* Version */
	tlv_serialize(WPS_ID_VERSION, bufObj, &version, WPS_ID_VERSION_S);

	/* Simple Config State */
	if (ssrmsg) {
		data8 = ssrmsg->scState.m_data;
	}
	else {
		sprintf(tmp, "ess%d_wps_oob", ess_id);
		pc_data = wps_safe_get_conf(tmp);
		if (!strcmp(pc_data, "disabled") || wps_ui_is_pending())
			data8 = WPS_SCSTATE_CONFIGURED;
		else
			data8 = WPS_SCSTATE_UNCONFIGURED;
	}

	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	/* AP Setup Locked - optional if false */
	if (wps_aplockdown_islocked()) {
		data8 = 0x1;
		tlv_serialize(WPS_ID_AP_SETUP_LOCKED, bufObj, &data8, WPS_ID_AP_SETUP_LOCKED_S);
	}

	/* Selected Registrar - optional if false */
	if (selreg) {
		tlv_serialize(WPS_ID_SEL_REGISTRAR, bufObj, &ssrmsg->selReg.m_data,
			WPS_ID_SEL_REGISTRAR_S);
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj,
			&ssrmsg->devPwdId.m_data, WPS_ID_DEVICE_PWD_ID_S);
		tlv_serialize(WPS_ID_SEL_REG_CFG_METHODS, bufObj,
			&ssrmsg->selRegCfgMethods.m_data, WPS_ID_SEL_REG_CFG_METHODS_S);
	}

	/* UUID and RF_BAND for dualband */
	sprintf(tmp, "ess%d_band", ess_id);
	pc_data = wps_safe_get_conf(tmp);
	data8 = atoi(pc_data);
	if (data8 == (WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ)) {
		p_data8 = wps_get_uuid();
		tlv_serialize(WPS_ID_UUID_E, bufObj, p_data8, 16);
		tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);
	}


	/* Send a pointer to the serialized data to Transport */
	ret = wps_wl_set_wps_ie(ifname, bufObj->pBase, bufObj->m_dataLength,
		WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_WPS);
	if (ret != 0) {
		TUTRACE((TUTRACE_ERR, "set beacon IE on %s failed, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}
	else {
		TUTRACE((TUTRACE_ERR, "set beacon IE on %s successful, selreg is %s\n",
			ifname, selreg ? "TRUE" : "FALSE"));
	}

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);

	if (!strcmp(wps_safe_get_conf(strcat_r(prefix, "wep", tmp)), "enabled")) {
		/* set STATIC WEP key OUI IE to frame */
		data8 = 0;
		ret |= wps_wl_set_wps_ie(ifname, &data8, 1,
			WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_PROVISION_STATIC_WEP);
		if (ret != 0)
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s failed\n",
				ifname));
		else
			TUTRACE((TUTRACE_ERR, "set STATIC WEP to probrsp IE on %s success\n",
				ifname));
	}
	else
		ret = wps_wl_del_wps_ie(ifname,
			WPS_IE_TYPE_SET_BEACON_IE, OUITYPE_PROVISION_STATIC_WEP);

	return ret;

}

/* Get default select registrar information */
int
wps_ie_default_ssr_info(CTlvSsrIE *ssrmsg)
{
	uint16 devpwd_id;
	char *pc_data;
	uint16 data16;

	if (!ssrmsg) {
		TUTRACE((TUTRACE_ERR, "get default ssr buffer NULL!!\n"));
		return -1;
	}

	if (!strcmp(wps_ui_get_env("wps_sta_pin"), "00000000"))
		devpwd_id = WPS_DEVICEPWDID_PUSH_BTN;
	else
		devpwd_id = WPS_DEVICEPWDID_DEFAULT;

	/* setup default data to ssrmsg */
	ssrmsg->version.m_data = 0x10;
	ssrmsg->scState.m_data = WPS_SCSTATE_CONFIGURED;	/* Builtin register */
	ssrmsg->selReg.m_data = 1;
	ssrmsg->devPwdId.m_data = devpwd_id;

	pc_data = wps_get_conf("wps_config_method");
	data16 = pc_data ? strtoul(pc_data, NULL, 16) : 0x88;
	ssrmsg->selRegCfgMethods.m_data = data16;
	return 0;
}

static void
add_ie(int ess_id, char *ifname, CTlvSsrIE *ssrmsg)
{
	/* Apply ie to probe response and beacon */
	wps_set_beacon_IE(ess_id, ifname, ssrmsg);
	wps_set_prb_rsp_IE(ess_id, ifname, ssrmsg);
	wps_wl_open_wps_window(ifname);
}

static void
del_ie(char *ifname)
{
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
		OUITYPE_WPS);
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
		OUITYPE_WPS);
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
		OUITYPE_PROVISION_STATIC_WEP);
	wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
		OUITYPE_PROVISION_STATIC_WEP);
	wps_wl_close_wps_window(ifname);
}

void
wps_ie_set(char *wps_ifname, CTlvSsrIE *ssrmsg)
{
	char ifname[IFNAMSIZ];
	char *next = NULL;

	int i, imax;
	int matched = -1;
	int matched_band = -1;
	char tmp[100];
	char *wlnames, *wps_mode;
	int band_flag;
	int band;

	/* Search matched ess with the wps_ifname */
	imax = wps_get_ess_num();
	if (wps_ifname) {
		for (i = 0; i < imax; i++) {
			sprintf(tmp, "ess%d_wlnames", i);
			wlnames = wps_safe_get_conf(tmp);
			if (strlen(wlnames) == 0)
				continue;

			foreach(ifname, wlnames, next) {
				if (strcmp(ifname, wps_ifname) == 0) {
					sprintf(tmp, "%s_band", ifname);
					matched_band = atoi(wps_safe_get_conf(tmp));
					matched = i;
					goto found;
				}
			}
		}
	}

found:
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);
		if (strlen(wlnames) == 0)
			continue;

		band_flag = 0;
		foreach(ifname, wlnames, next) {
			sprintf(tmp, "%s_wps_mode", ifname);
			wps_mode = wps_safe_get_conf(tmp);
			if (!strcmp(wps_mode, "enr_enabled"))
				continue; /* Only for AP mode */

			sprintf(tmp, "%s_band", ifname);
			band = atoi(wps_safe_get_conf(tmp));
			/*
			  * 1. If wps_ifname is null, set to all the wl interfaces.
			  * 2. Set ie to the exact matched wl interface if wps_ifname is given.
			  * 3. For each band, at most one wl interface is able to set the ssrmsg.
			  */
			if (wps_ifname == NULL || strcmp(wps_ifname, ifname) == 0 ||
			    ((i == matched) && (band != matched_band) && !(band_flag & band))) {
				/* Set ssrmsg to expected wl interface */
				add_ie(i, ifname, ssrmsg);
				band_flag |= band;
			}
			else {
				del_ie(ifname);
			}
		}
	}

	return;
}


void
wps_ie_clear()
{
	char ifname[IFNAMSIZ];
	char *next = NULL;

	int i, imax;
	char tmp[100];
	char *wlnames, *wps_mode;

	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);
		if (strlen(wlnames) == 0)
			continue;

		foreach(ifname, wlnames, next) {
			sprintf(tmp, "%s_wps_mode", ifname);
			wps_mode = wps_safe_get_conf(tmp);
			if (!strcmp(wps_mode, "enr_enabled"))
				continue; /* Only for AP mode */

			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
				OUITYPE_WPS);
			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
				OUITYPE_WPS);
			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_BEACON_IE,
				OUITYPE_PROVISION_STATIC_WEP);
			wps_wl_del_wps_ie(ifname, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
				OUITYPE_PROVISION_STATIC_WEP);
			wps_wl_close_wps_window(ifname);
		}
	}

	return;
}
