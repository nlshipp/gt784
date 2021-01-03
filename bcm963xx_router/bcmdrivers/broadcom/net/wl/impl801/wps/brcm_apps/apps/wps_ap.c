/*
 * WPS AP thread (Platform independent portion)
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ap.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <stdio.h>
#include <unistd.h>

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#endif

#include <netinet/in.h>
#include <net/if.h>

#include <bn.h>
#include <wps_dh.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wps_ap.h>
#include <sminfo.h>
#include <wpserror.h>
#include <portability.h>
#include <reg_proto.h>
#include <statemachine.h>

#include <tutrace.h>

#include <slist.h>

#include <ctype.h>
#include <time.h>
#include <wps_wl.h>
#include <wpsapi.h>
#include <wlif_utils.h>
#include <shutils.h>
#include <wps_apapi.h>
#include <ap_eap_sm.h>
#include <ap_upnp_sm.h>
#include <wps.h>
#include <wps_ui.h>
#include <wps_upnp.h>
#include <wps_ie.h>
#include <wps_aplockdown.h>
#include <wps_pb.h>

static void wpsap_bcmwps_add_pktcnt(wpsap_wksp_t *app_wksp);

#ifdef _TUDEBUGTRACE
static void
wpsap_dump_config(devcfg *ap_devcfg, DevInfo *ap_devinfo)
{
	char *Pchar;
	unsigned char *Puchar;

	/* Mode */
	switch (ap_devcfg->e_mode) {
		case EModeUnconfAp:
			Pchar = "Unconfig AP";
			break;
		case EModeClient:
			Pchar = "Client";
			break;
		case EModeRegistrar:
			Pchar = "Registrar";
			break;
		case EModeApProxy:
			Pchar = "Ap Proxy";
			break;
		case EModeApProxyRegistrar:
			Pchar = "Ap Proxy with built-in Registrar";
			break;
		case EModeUnknown:
		default:
			Pchar = "Unknown";
			break;
	}
	TUTRACE((TUTRACE_INFO, "Mode: [%s]\n", Pchar));

	/* Configure Mode */
	TUTRACE((TUTRACE_INFO, "AP %s configured\n",
		(ap_devinfo->scState == 2) ? "already" : "not"));

	/* Registrar enable */
	TUTRACE((TUTRACE_INFO, "%s Built-in Registrar\n",
		(ap_devcfg->mb_regWireless) ? "Enabled" : "Disabled"));

	/* USE UPnP */
	TUTRACE((TUTRACE_INFO, "%s UPnP Module\n",
		(ap_devcfg->mb_useUpnp) ? "Enabled" : "Disabled"));

	/* UUID */
	Puchar = ap_devinfo->uuid;
	TUTRACE((TUTRACE_INFO, "UUID: [0x%02x%02x%02x%02x%02x%02x%02x%02x"
		"%02x%02x%02x%02x%02x%02x%02x%02x]\n",
		Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4],
		Puchar[5], Puchar[6], Puchar[7], Puchar[8], Puchar[9],
		Puchar[10], Puchar[11], Puchar[12], Puchar[13], Puchar[14], Puchar[15]));

	/* Version */
	TUTRACE((TUTRACE_INFO, "WPS Version: [0x%02X]\n",
		ap_devinfo->version));

	/* Device Nake */
	TUTRACE((TUTRACE_INFO, "Device name: [%s]\n",
		ap_devinfo->deviceName));

	/* Device category */
	TUTRACE((TUTRACE_INFO, "Device category: [%d]\n",
		ap_devinfo->primDeviceCategory));

	/* Device sub category */
	TUTRACE((TUTRACE_INFO, "Device sub category: [%d]\n",
		ap_devinfo->primDeviceSubCategory));

	/* Device OUI */
	TUTRACE((TUTRACE_INFO, "Device OUI: [0x%08X]\n",
		ap_devinfo->primDeviceOui));

	/* Mac address */
	Puchar = ap_devinfo->macAddr;
	TUTRACE((TUTRACE_INFO, "Mac address: [%02x:%02x:%02x:%02x:%02x:%02x]\n",
		Puchar[0], Puchar[1], Puchar[2], Puchar[3], Puchar[4], Puchar[5]));

	/* MANUFACTURER */
	TUTRACE((TUTRACE_INFO, "Manufacturer name: [%s]\n",
		ap_devinfo->manufacturer));

	/* MODEL_NAME */
	TUTRACE((TUTRACE_INFO, "Model name: [%s]\n",
		ap_devinfo->modelName));

	/* MODEL_NUMBER */
	TUTRACE((TUTRACE_INFO, "Model number: [%s]\n",
		ap_devinfo->modelNumber));

	/* SERIAL_NUMBER */
	TUTRACE((TUTRACE_INFO, "Serial number: [%s]\n",
		ap_devinfo->serialNumber));

	/* CONFIG_METHODS */
	TUTRACE((TUTRACE_INFO, "Config methods: [0x%04X]\n",
		ap_devinfo->configMethods));

	/* AUTH_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Auth type flags: [0x%04X]\n",
		ap_devinfo->authTypeFlags));

	/* ENCR_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Encrypto type flags: [0x%04X]\n",
		ap_devinfo->encrTypeFlags));

	/* CONN_TYPE_FLAGS */
	TUTRACE((TUTRACE_INFO, "Conn type flags: [%d]\n",
		ap_devinfo->connTypeFlags));

	/* RF Band */
	TUTRACE((TUTRACE_INFO, "RF band: [%d]\n",
		ap_devinfo->rfBand));

	/* OS_VER */
	TUTRACE((TUTRACE_INFO, "OS version: [0x%08X]\n",
		ap_devinfo->osVersion));

	/* FEATURE_ID */
	TUTRACE((TUTRACE_INFO, "Feature ID: [0x%08X]\n",
		ap_devinfo->featureId));

	/* SSID */
	TUTRACE((TUTRACE_INFO, "SSID: [%s]\n", ap_devinfo->ssid));

	/* Auth */
	TUTRACE((TUTRACE_INFO, "Auth: [%d]\n", ap_devinfo->auth));

	/* KEY MGMT */
	TUTRACE((TUTRACE_INFO, "Key management: [%s]\n",
		(ap_devinfo->keyMgmt[0]) ? ap_devinfo->keyMgmt : "Open network"));

	/* USB_KEY */
	TUTRACE((TUTRACE_INFO, "%s USB Key\n",
		(ap_devcfg->mb_useUsbKey) ? "Enabled" : "Disabled"));

	/* Network key */
	TUTRACE((TUTRACE_INFO, "Network Key: [%s]\n", ap_devcfg->m_nwKey));

	/* Network key */
	TUTRACE((TUTRACE_INFO, "Network Key Index: [%x]\n", ap_devcfg->m_wepKeyIdx));

	/* Crypto falgs */
	TUTRACE((TUTRACE_INFO, "Crypto flags: [%s]\n",
		((ap_devinfo->crypto & WPS_ENCRTYPE_TKIP) &&
		(ap_devinfo->crypto & WPS_ENCRTYPE_AES)) ? "TKIP|AES" :
		(ap_devinfo->crypto & WPS_ENCRTYPE_TKIP) ? "TKIP" :
		(ap_devinfo->crypto & WPS_ENCRTYPE_AES) ? "AES" : "None"));
}
#endif /* _TUDEBUGTRACE */

#ifdef	BCMUPNP
int
wps_setWpsIE(void *bcmdev, uint8 scState, uint8 selRegistrar,
	uint16 devPwdId, uint16 selRegCfgMethods)
{
	WPSAPI_T *g_mc = (WPSAPI_T *)bcmdev;
	wpsap_wksp_t *wps_wksp = (wpsap_wksp_t *)g_mc->bcmwps;
	char *ifname;
	CTlvSsrIE ssrmsg;

	if (!wps_wksp)
		return -1;

	ifname = wps_wksp->ifname;

	TUTRACE((TUTRACE_INFO, "Add IE from %s\n", ifname));

	/* setup default data to ssrmsg */
	ssrmsg.version.m_data = 0x10;
	ssrmsg.scState.m_data = scState;
	ssrmsg.selReg.m_data = selRegistrar;
	ssrmsg.devPwdId.m_data = devPwdId;
	ssrmsg.selRegCfgMethods.m_data = selRegCfgMethods;

	wps_ie_set(ifname, &ssrmsg);

	return 0;
}
#else
int
wps_set_wps_ie(void *bcmdev, unsigned char *p_data, int length, unsigned int cmdtype)
{
	wpsap_wksp_t *wps_wksp = (wpsap_wksp_t *)bcmdev;
	char data8;
	char *ifname;
	WPSAPI_T *gp_mc;
	DevInfo *devinfo;

	if (!wps_wksp)
		return -1;

	ifname = wps_wksp->ifname;

	TUTRACE((TUTRACE_INFO, "Add IE from %s\n", ifname));
	wps_wl_set_wps_ie(ifname, p_data, length, cmdtype, OUITYPE_WPS);

	/* Set wep OUI if wep on */
	gp_mc = (WPSAPI_T *)wps_wksp->mc;
	devinfo = gp_mc->mp_info->mp_deviceInfo;

	if (devinfo->wep) {
		data8 = 0;
		wps_wl_set_wps_ie(ifname, &data8, 1, cmdtype, OUITYPE_PROVISION_STATIC_WEP);
	}
	else
		wps_wl_del_wps_ie(ifname, cmdtype, OUITYPE_PROVISION_STATIC_WEP);

	return 0;
}
#endif	/* BCMUPNP */

static int
wpsap_process_msg(wpsap_wksp_t *wps_wksp, char *buf, uint32 len, uint32 msgtype)
{
	int send_len;
	char *sendBuf;
	uint32 retVal = WPS_CONT;

	switch (msgtype) {
	/* EAP */
	case TRANSPORT_TYPE_EAP:
		retVal = ap_eap_sm_process_sta_msg(buf, len);

		/* update packet count */
		if (retVal == WPS_CONT || retVal == WPS_SEND_MSG_CONT)
			wpsap_bcmwps_add_pktcnt(wps_wksp);

		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT ||
			retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_ADDCLIENT_PINFAIL ||
			retVal == WPS_ERR_CONFIGAP_PINFAIL) {
			send_len = ap_eap_sm_get_msg_to_send(&sendBuf);
			if (send_len >= 0)
				ap_eap_sm_sendMsg(sendBuf, send_len);

			/* WPS_SUCCESS or WPS_MESSAGE_PROCESSING_ERROR case */
			if (retVal == WPS_SEND_MSG_SUCCESS ||
				retVal == WPS_SEND_MSG_ERROR ||
				retVal == WPS_ERR_ADDCLIENT_PINFAIL ||
				retVal == WPS_ERR_CONFIGAP_PINFAIL) {
				ap_eap_sm_Failure(0);
			}

			/* over-write retVal */
			if (retVal == WPS_SEND_MSG_SUCCESS)
				retVal = WPS_SUCCESS;
			else if (retVal == WPS_SEND_MSG_ERROR)
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
			else if (retVal == WPS_ERR_ADDCLIENT_PINFAIL) {
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
				wps_ui_set_env("wps_pinfail", "1");
				wps_set_conf("wps_pinfail", "1");
			}
			else if (retVal == WPS_ERR_CONFIGAP_PINFAIL)
				retVal = WPS_ERR_CONFIGAP_PINFAIL;
			else
				retVal = WPS_CONT;
		}
		/* other error case */
		else if (retVal != WPS_CONT) {
			TUTRACE((TUTRACE_INFO,
				"Prevent EAP-Fail faster than EAP-Done for DTM 1.4 case 913.\n",
				retVal));
			WpsSleepMs(50);

			ap_eap_sm_Failure(0);
		}
		break;

	/* UPNP */
	case TRANSPORT_TYPE_UPNP_DEV:
		retVal = ap_upnp_sm_process_msg(buf, len);

		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT ||
			retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_CONFIGAP_PINFAIL) {
			send_len = ap_upnp_sm_get_msg_to_send(&sendBuf);
			if (send_len >= 0)
				ap_upnp_sm_sendMsg(sendBuf, send_len);

			/* over-write retVal */
			if (retVal == WPS_SEND_MSG_SUCCESS)
				retVal = WPS_SUCCESS;
			else if (retVal == WPS_SEND_MSG_ERROR)
				retVal = WPS_MESSAGE_PROCESSING_ERROR;
			else if (retVal == WPS_ERR_CONFIGAP_PINFAIL)
				retVal = WPS_ERR_CONFIGAP_PINFAIL;
			else
				retVal = WPS_CONT;
		}
		break;

	default:
		break;
	}

	return retVal;
}

#ifdef BCMWPA2
#define WPS_WLAKM_BOTH(akm) ((akm & WPA_AUTH_PSK) && (akm & WPA2_AUTH_PSK))
#define WPS_WLAKM_PSK2(akm) ((akm & WPA2_AUTH_PSK))
#define WPS_WLAKM_PSK(akm) ((akm & WPA_AUTH_PSK))
#else
#define WPS_WLAKM_BOTH(akm) (0)
#define WPS_WLAKM_PSK2(akm) (0)
#define WPS_WLAKM_PSK(akm) ((akm & WPA_AUTH_PSK))
#endif
#define WPS_WLAKM_NONE(akm) (!(WPS_WLAKM_BOTH(akm) | WPS_WLAKM_PSK2(akm) | WPS_WLAKM_PSK(akm)))

#define WPS_WLENCR_BOTH(wsec) ((wsec & TKIP_ENABLED) && (wsec & AES_ENABLED))
#define WPS_WLENCR_TKIP(wsec) (wsec & TKIP_ENABLED)
#define WPS_WLENCR_AES(wsec) (wsec & AES_ENABLED)


static int
wpsap_generate_ssid(char *ssid)
{
	unsigned short ssid_length;
	unsigned char random_ssid[32] = {0};
	int i;
	char mac[18] = {0};
	char *random_ssid_prx = wps_get_conf("wps_random_ssid_prefix");

	strncpy(mac, wps_safe_get_conf("wl0_hwaddr"), sizeof(mac));
	RAND_bytes((unsigned char *)&ssid_length, sizeof(ssid_length));
	ssid_length = ((((long)ssid_length + 56791)*13579)%23) + 1;

	RAND_bytes((unsigned char *)random_ssid, ssid_length);

	for (i = 0; i < ssid_length; i++) {
		if ((random_ssid[i] < 48) || (random_ssid[i] > 126))
			random_ssid[i] = random_ssid[i]%79 + 48;
		/* Aotomatically advance those chars < > ` and ' */
		if (random_ssid[i] == 0x3C /* < */ || random_ssid[i] == 0x3E /* > */ ||
		    random_ssid[i] == 0x60 /* ` */ || random_ssid[i] == 0x27 /* ' */)
			random_ssid[i] = random_ssid[i] + 1;
	}

	random_ssid[ssid_length++] = tolower(mac[6]);
	random_ssid[ssid_length++] = tolower(mac[7]);
	random_ssid[ssid_length++] = tolower(mac[9]);
	random_ssid[ssid_length++] = tolower(mac[10]);
	random_ssid[ssid_length++] = tolower(mac[12]);
	random_ssid[ssid_length++] = tolower(mac[13]);
	random_ssid[ssid_length++] = tolower(mac[15]);
	random_ssid[ssid_length++] = tolower(mac[16]);

	memset(ssid, 0, SIZE_SSID_LENGTH);
	if (random_ssid_prx)
		snprintf(ssid, SIZE_SSID_LENGTH - 1, "%s", random_ssid_prx);

	strncat(ssid, random_ssid, SIZE_SSID_LENGTH - strlen(ssid) - 1);

	return 0;
}

static int
wpsap_generate_key(char *key)
{
	unsigned short key_length;
	unsigned char random_key[64] = {0};
	int i = 0;

	RAND_bytes((unsigned char *)&key_length, sizeof(key_length));
	key_length = ((((long)key_length + 56791)*13579)%8) + 8;

	while (i < key_length) {
		RAND_bytes(&random_key[i], 1);
		if ((islower(random_key[i]) || isdigit(random_key[i])) &&
			(random_key[i] < 0x7f)) {
			i++;
		}
	}

	memset(key, 0, SIZE_64_BYTES);
	strncpy(key, (char *)random_key, key_length);

	return 0;
}

static int
wpsap_update_custom_cred(char *ssid, char *key, char *akm, char *crypto, int oob_addenr)
{
	/*
	 * Default set to WPA2/AES.
	 */
	int mix_mode = 2;

	if (!strcmp(wps_safe_get_conf("wps_mixedmode"), "1"))
		mix_mode = 1;

	if (!strcmp(akm, "WPA-PSK WPA2-PSK")) {
		if (mix_mode == 1)
			strcpy(akm, "WPA-PSK");
		else
			strcpy(akm, "WPA2-PSK");

		TUTRACE((TUTRACE_INFO, "Update customized Key Mode : %s, ", akm));
	}

	if (!strcmp(crypto, "AES+TKIP")) {
		if (mix_mode == 1)
			strcpy(crypto, "TKIP");
		else
			strcpy(crypto, "AES");

		TUTRACE((TUTRACE_INFO, "Update customized encrypt mode = %s\n", crypto));
	}

	if (oob_addenr) {
		char *p;

		/* get randomssid */
		if ((p = wps_get_conf("wps_randomssid"))) {
			strncpy(ssid, p, MAX_SSID_LEN);
			TUTRACE((TUTRACE_INFO, "Update customized SSID : %s\n", ssid));
		}

		/* get randomkey */
		if ((p = wps_get_conf("wps_randomkey")) && (strlen (p) >= 8)) {
			strncpy(key, p, SIZE_64_BYTES);
			TUTRACE((TUTRACE_INFO, "Update customized Key : %s\n", key));
		}

		/* Modify the crypto, if the station is legacy */
	}

	return 0;
}

static int
wpsap_readConfigure(wpsap_wksp_t *wps_wksp, devcfg *ap_devcfg, DevInfo *ap_devinfo)
{
	char *dev_key = NULL;
	char *value, *next;
	int auth = 0;
	char ssid[MAX_SSID_LEN + 1] = {0};
	char psk[MAX_USER_KEY_LEN + 1] = {0};
	char akmstr[32];
	char key[8];
	unsigned int akm = 0;
	unsigned int wsec = 0;
	int wep_index = 0;			/* wep key index */
	char *wep_key = NULL;			/* user-supplied wep key */
	char dev_akm[64] = {0};
	char dev_crypto[64] = {0};
	bool oob_addenr = 0;
	char prefix[] = "wlXXXXXXXXXX_";
	char tmp[100];

	/* TBD, is going to use osname only */
	sprintf(prefix, "%s_", wps_wksp->ifname);

	if (wps_wksp->wps_mode == EModeUnknown) {
		TUTRACE((TUTRACE_ERR, "Error getting wps config mode\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	/* raw OOB config state (per-ESS) */
	sprintf(tmp, "ess%d_wps_oob", wps_wksp->ess_id);
	if (!strcmp(wps_safe_get_conf(tmp), "disabled"))
		wps_wksp->config_state = 0x2;
	else
		wps_wksp->config_state = 0x1;

	/* Configure Mode sending to peer */
	ap_devinfo->scState = wps_wksp->config_state;
	if (wps_wksp->wps_mode == EModeApProxyRegistrar)
		ap_devinfo->scState = WPS_SCSTATE_CONFIGURED;

	/* Operating Mode */
	ap_devcfg->e_mode = wps_wksp->wps_mode;
	ap_devinfo->b_ap = true;

	/* Registrar enable (per-ESS) */
	sprintf(tmp, "ess%d_wps_reg", wps_wksp->ess_id);
	if (!strcmp(wps_safe_get_conf(tmp), "enabled"))
		ap_devcfg->mb_regWireless = true;
	else
		ap_devcfg->mb_regWireless = false;

	/* USE UPnP */
	ap_devcfg->mb_useUpnp = true;

	/* UUID */
	memcpy(ap_devinfo->uuid, wps_get_uuid(), SIZE_16_BYTES);

	/* Version */
	ap_devinfo->version = (uint8) 0x10;

	/* Device Name */
	value = wps_get_conf("wps_device_name");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <DEVICE_NAME>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->deviceName, value);

	/* Device category */
	ap_devinfo->primDeviceCategory = 6;

	/* Device OUI */
	ap_devinfo->primDeviceOui = 0x0050F204;

	/* Device sub category */
	ap_devinfo->primDeviceSubCategory = 1;

	/* Mac address */
	memcpy(ap_devinfo->macAddr, wps_wksp->mac_ap, SIZE_6_BYTES);

	/* MANUFACTURER */
	value = wps_get_conf("wps_mfstring");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MANUFACTURER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->manufacturer, value);

	/* MODEL_NAME */
	value = wps_get_conf("wps_modelname");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MODEL_NAME>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->modelName, value);

	/* MODEL_NUMBER */
	value = wps_get_conf("wps_modelnum");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <MODEL_NUMBER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->modelNumber, value);

	/* SERIAL_NUMBER */
	value = wps_get_conf("boardnum");
	if (!value) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure: Error in cfg file, <BOARD_NUMBER>\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}
	strcpy(ap_devinfo->serialNumber, value);

	/* CONFIG_METHODS */
	ap_devinfo->configMethods = 0x88;
	value = wps_get_conf("wps_config_method");
	if (value)
		ap_devinfo->configMethods = (uint16)(strtoul(value, NULL, 16));

	/* AUTH_TYPE_FLAGS */
	ap_devinfo->authTypeFlags = (uint16) 0x27;

	/* ENCR_TYPE_FLAGS */
	ap_devinfo->encrTypeFlags = (uint16) 0xf;

	/* CONN_TYPE_FLAGS */
	ap_devinfo->connTypeFlags = 1;

	/* RF Band */
	/* use ESS's rfBand which used in wps_upnp_create_device_info() instead of wireless */
	if (wps_wksp->pre_nonce) {
		sprintf(tmp, "ess%d_band", wps_wksp->ess_id);
		value = wps_safe_get_conf(tmp);
		ap_devinfo->rfBand = atoi(value);
	} else {
		sprintf(tmp, "%s_band", wps_wksp->ifname);
		value = wps_safe_get_conf(tmp);
		switch (atoi(value)) {
		case WLC_BAND_ALL:
			ap_devinfo->rfBand = WPS_RFBAND_24GHZ | WPS_RFBAND_50GHZ;
			break;
		case WLC_BAND_5G:
			ap_devinfo->rfBand = WPS_RFBAND_50GHZ;
			break;
		case WLC_BAND_2G:
		default:
			ap_devinfo->rfBand = WPS_RFBAND_24GHZ;
			break;
		}
	}

	/* OS_VER */
	ap_devinfo->osVersion = 0x80000000;

	/* FEATURE_ID */
	ap_devinfo->featureId = 0x80000000;

	/* Auth */
	if (!strcmp(strcat_r(prefix, "auth", tmp), "1"))
		auth = 1;

	ap_devinfo->auth = auth;

	/* USB_KEY */
	ap_devcfg->mb_useUsbKey = false;

	/*
	 * Before check oob mode, we have to
	 * get ssid, akm, wep, crypto and mgmt key from config.
	 * because oob mode might change the settings.
	 */
	value = wps_safe_get_conf(strcat_r(prefix, "ssid", tmp));
	strncpy(ssid, value, MAX_SSID_LEN);

	value = wps_safe_get_conf(strcat_r(prefix, "akm", tmp));
	foreach(akmstr, value, next) {
		if (!strcmp(akmstr, "psk"))
			akm |= WPA_AUTH_PSK;
#ifdef BCMWPA2
		if (!strcmp(akmstr, "psk2"))
			akm |= WPA2_AUTH_PSK;
#endif
	}

	value = wps_safe_get_conf(strcat_r(prefix, "wep", tmp));
	wsec = !strcmp(value, "enabled") ? WEP_ENABLED : 0;

	value = wps_safe_get_conf(strcat_r(prefix, "crypto", tmp));
	if (WPS_WLAKM_PSK(akm) || WPS_WLAKM_PSK2(akm)) {
		if (!strcmp(value, "tkip"))
			wsec |= TKIP_ENABLED;
		else if (!strcmp(value, "aes"))
			wsec |= AES_ENABLED;
		else if (!strcmp(value, "tkip+aes"))
			wsec |= TKIP_ENABLED|AES_ENABLED;

		/* Set PSK key */
		value = wps_safe_get_conf(strcat_r(prefix, "wpa_psk", tmp));
		strncpy(psk, value, MAX_USER_KEY_LEN);
		psk[MAX_USER_KEY_LEN] = 0;
	}

	if (wsec & WEP_ENABLED) {
		/* key index */
		value = wps_safe_get_conf(strcat_r(prefix, "key", tmp));
		wep_index = (int)strtoul(value, NULL, 0);

		/* key */
		sprintf(key, "key%s", value);
		wep_key = wps_safe_get_conf(strcat_r(prefix, key, tmp));
	}

	if (wps_wksp->config_state == WPS_SCSTATE_UNCONFIGURED &&
		wps_wksp->wps_mode == EModeApProxyRegistrar) {
		oob_addenr = 1;
	}

	if (oob_addenr) {
		/* Generate random ssid */
		wpsap_generate_ssid(ssid);

		/* Generate random key and key length */
		wpsap_generate_key(psk);

		/* Open */
		auth = 0;

		/* PSK, PSK2 */
#ifdef BCMWPA2
		akm = WPA_AUTH_PSK | WPA2_AUTH_PSK;
		wsec = AES_ENABLED;
#else
		akm = WPA_AUTH_PSK;
		wsec = TKIP_ENABLED;
#endif
	}
	/*
	 * Let the user have a chance to override the credential.
	 */
	if (WPS_WLAKM_BOTH(akm))
		strcpy(dev_akm, "WPA-PSK WPA2-PSK");
	else if (WPS_WLAKM_PSK(akm))
		strcpy(dev_akm, "WPA-PSK");
	else if (WPS_WLAKM_PSK2(akm))
		strcpy(dev_akm, "WPA2-PSK");
	else
		dev_akm[0] = 0;

	/* save default KEY MGMT */
	strcpy(wps_wksp->default_keyMgmt, dev_akm);

	/* Encryption algorithm */
	if (WPS_WLENCR_BOTH(wsec))
		strcpy(dev_crypto, "AES+TKIP");
	else if (WPS_WLENCR_TKIP(wsec))
		strcpy(dev_crypto, "TKIP");
	else if (WPS_WLENCR_AES(wsec))
		strcpy(dev_crypto, "AES");
	else
		dev_crypto[0] = 0;

	/* Do customization, and check credentials again */
	wpsap_update_custom_cred(ssid, psk, dev_akm, dev_crypto, oob_addenr);

	/*
	 * After doing customized credentials modification,
	 * fill ssid, psk, akm and crypto to ap_deviceinfo
	 */
	strcpy(ap_devinfo->ssid, ssid);

	/* parsing return amk and crypto */
	if (strlen(dev_akm)) {
		if (!strcmp(dev_akm, "WPA-PSK WPA2-PSK"))
			akm = WPA_AUTH_PSK | WPA2_AUTH_PSK;
		else if (!strcmp(dev_akm, "WPA-PSK"))
			akm = WPA_AUTH_PSK;
		else if (!strcmp(dev_akm, "WPA2-PSK"))
			akm = WPA2_AUTH_PSK;
	}
	if (strlen(dev_crypto)) {
		if (!strcmp(dev_crypto, "AES+TKIP"))
			wsec = AES_ENABLED | TKIP_ENABLED;
		else if (!strcmp(dev_crypto, "AES"))
			wsec = AES_ENABLED;
		else if (!strcmp(dev_crypto, "TKIP"))
			wsec = TKIP_ENABLED;
	}

	/* KEY MGMT */
	if (auth)
		strcpy(ap_devinfo->keyMgmt, "SHARED");
	else {
		if (WPS_WLAKM_BOTH(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA-PSK WPA2-PSK");
		else if (WPS_WLAKM_PSK(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA-PSK");
		else if (WPS_WLAKM_PSK2(akm))
			strcpy(ap_devinfo->keyMgmt, "WPA2-PSK");
		else
			ap_devinfo->keyMgmt[0] = 0;
	}

	/* WEP index */
	ap_devinfo->wep = (wsec & WEP_ENABLED) ? 1 : 0;

	if (WPS_WLAKM_NONE(akm) && ap_devinfo->wep) {
		/* get wps key index */
		ap_devcfg->m_wepKeyIdx = wep_index;

		/* get wps key content */
		dev_key = wep_key;
	}
	else if (!WPS_WLAKM_NONE(akm))
		dev_key = psk;

	/* Network key */
	memset(ap_devcfg->m_nwKey, 0, SIZE_64_BYTES);
	ap_devcfg->m_nwKeyLen = (dev_key) ? (uint32) strlen(dev_key) : 0;
	strncpy(ap_devcfg->m_nwKey, dev_key, ap_devcfg->m_nwKeyLen);
	ap_devcfg->mb_nwKeySet = true;

	/* fill in prebuild enrollee nonce and private key */
	memset(&ap_devcfg->pre_enrolleeNonce, 0, SIZE_128_BITS);
	if (wps_wksp->pre_nonce) {
		/* copy prebuild enrollee nonce */
		memcpy(ap_devcfg->pre_enrolleeNonce, wps_wksp->pre_nonce, SIZE_128_BITS);
		memcpy(ap_devcfg->pre_privKey, wps_wksp->pre_privkey, SIZE_PUB_KEY);
		TUTRACE((TUTRACE_INFO, "wpsap is triggered by UPnP PMR message\n"));
	}

	ap_devinfo->crypto = 0;
	if (wsec & TKIP_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_TKIP;
	if (wsec & AES_ENABLED)
		ap_devinfo->crypto |= WPS_ENCRTYPE_AES;

	if (ap_devinfo->crypto == 0)
		ap_devinfo->crypto = WPS_ENCRTYPE_TKIP;

#ifdef _TUDEBUGTRACE
	wpsap_dump_config(ap_devcfg, ap_devinfo);
#endif
	return WPS_SUCCESS;
}

static int
wpsap_wksp_init(wpsap_wksp_t *ap_wksp)
{
	time_t now;
	char tmp[100];
	devcfg ap_devcfg = {0};
	DevInfo ap_devinfo = {0};

	char *dev_pwd = NULL;
	uint16 dev_pwd_id = WPS_DEVICEPWDID_DEFAULT;

	/* !!! shold be removed */
	wps_set_ifname(ap_wksp->ifname);

	TUTRACE((TUTRACE_INFO, "wpsap_wksp_init: init %s\n", ap_wksp->ifname));

	/* workspace init time */
	time(&now);
	ap_wksp->start_time = now;
	ap_wksp->touch_time = now;

	/* read device config here and pass to wps_init */
	if (wpsap_readConfigure(ap_wksp, &ap_devcfg, &ap_devinfo) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "wpsap_readConfigure fail...\n"));
		return -1;
	}

	if ((ap_wksp->mc = (void*)wps_init(ap_wksp, &ap_devcfg, &ap_devinfo)) == NULL) {
		TUTRACE((TUTRACE_INFO, "wps_init fail...\n"));
		return -1;
	}

	if (ap_eap_sm_init(ap_wksp->mc, ap_wksp->mac_sta, wps_eapol_parse_msg,
		wps_eapol_send_data) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "ap_eap_sm_init fail...\n"));
		return -1;
	}
	TUTRACE((TUTRACE_INFO, "ap_eap_sm_init successful...\n"));

	if ((ap_wksp->wps_mode == EModeUnconfAp) || (ap_wksp->wps_mode == EModeApProxy)) {
		int upnp_instance;

		sprintf(tmp, "ess%d_upnp_instance", ap_wksp->ess_id);
		upnp_instance = atoi(wps_safe_get_conf(tmp));

		if (ap_upnp_sm_init(ap_wksp->mc, (int)ap_wksp->pre_nonce,
			(void *)wps_upnp_update_init_wlan_event,
			(void *)wps_upnp_update_wlan_event,
			(void *)wps_upnp_send_msg,
			(void *)wps_upnp_parse_msg,
			upnp_instance) != WPS_SUCCESS) {
			TUTRACE((TUTRACE_ERR, "ap_upnp_sm_init fail...\n"));
			return -1;
		}
		TUTRACE((TUTRACE_INFO, "ap_upnp_sm_init lan%s successful...\n", upnp_instance));
	}

	/* Get PIN */
	if (ap_wksp->wps_mode == EModeUnconfAp) {
		dev_pwd = wps_get_conf("wps_device_pin");
	}
	else if (ap_wksp->wps_mode == EModeApProxyRegistrar) {
		dev_pwd = wps_ui_get_env("wps_sta_pin");
		if (dev_pwd) {
			if (strncmp(dev_pwd, "00000000", 8) == 0)
				dev_pwd_id = WPS_DEVICEPWDID_PUSH_BTN;
		}
	}
	else {
		/* Proxy mode has no PIN */
		dev_pwd = NULL;
	}

	/* do registration */
	if (dev_pwd) {
		char c_dev_pwd[16] = {0};
		strncpy(c_dev_pwd, dev_pwd, 8);
		if (WPS_SUCCESS != wps_start_ap_registration(ap_wksp->mc,
			ap_wksp->wps_mode, c_dev_pwd, dev_pwd_id)) {
			TUTRACE((TUTRACE_INFO, "Start ap registrattion fail...\n"));
			return -1;
		}
	}

	TUTRACE((TUTRACE_INFO, "init pin = %s\n", dev_pwd? dev_pwd: "NULL"));
	TUTRACE((TUTRACE_INFO, "wpsap init done\n"));

	return WPS_SUCCESS;
}

static void
wpsap_wksp_deinit(wpsap_wksp_t *wps_wksp)
{
	ap_eap_sm_deinit();
	ap_upnp_sm_deinit();

	if (wps_wksp->mc) {
		wps_deinit(wps_wksp->mc);
	}

	return;
}

static void
wpsap_bcmwps_add_pktcnt(wpsap_wksp_t *app_wksp)
{
	if (app_wksp)
		app_wksp->pkt_count++;

	return;
}

static int
wpsap_time_check(wpsap_wksp_t *wps_wksp, unsigned long now)
{
	int procstate = 0;

	/* check overall timeout first */
	if ((now < wps_wksp->start_time) ||
		((now - wps_wksp->start_time) > WPS_OVERALL_TIMEOUT)) {
		return WPS_RESULT_PROCESS_TIMEOUT;
	}

	if (now < wps_wksp->touch_time) {
		wps_wksp->touch_time = now;
	}

	/* check eap sm message timeout */
	if (ap_eap_sm_check_timer(now) == EAP_TIMEOUT) {
		return WPS_RESULT_PROCESS_TIMEOUT;
	}

	switch (wps_wksp->wps_mode) {
		case EModeUnconfAp:
			/* check enr state */
			procstate = wps_getenrState(wps_wksp->mc);
			if (procstate == 0)
				break;
			if (procstate != wps_wksp->wps_state) {
				/* state change, update the last time */
				wps_wksp->wps_state = procstate;
				wps_wksp->touch_time = now;
			}
			else {
				if (((now - wps_wksp->touch_time) > WPS_MSG_TIMEOUT) &&
					((procstate != MNONE && procstate != M2D) &&
					(procstate >= M2))) {
					WPSAPI_T *g_mc = wps_wksp->mc;
					EnrSM *e = g_mc->mp_enrSM;

					/* reset the SM */
					enr_sm_restartSM(e);

					wps_wksp->touch_time = now;
				}
			}
			break;
		case EModeApProxyRegistrar:
			/* check reg state */
			procstate = wps_getregState(wps_wksp->mc);
			if (procstate == 0)
				break;

			if (procstate != wps_wksp->wps_state) {
				/* state change, update the last time */
				wps_wksp->wps_state = procstate;
				wps_wksp->touch_time = now;
			}
			else {
				if (((now - wps_wksp->touch_time) > WPS_MSG_TIMEOUT) &&
					((procstate != MNONE && procstate != M2D) &&
					(procstate >= M2))) {
					return WPS_RESULT_PROCESS_TIMEOUT;
				}
			}
			break;
		case EModeApProxy:
		default:
			if (wps_wksp->pkt_count != wps_wksp->pkt_count_prv) {
				/* state change, update the last time */
				wps_wksp->pkt_count_prv = wps_wksp->pkt_count;
				wps_wksp->touch_time = now;
			}
			else {
				if ((now - wps_wksp->touch_time) > WPS_MSG_TIMEOUT) {
					return WPS_RESULT_PROCESS_TIMEOUT;
				}
			}
			break;
	}

	return WPS_CONT;
}

static int
wpsap_close_session(wpsap_wksp_t *wps_wksp, int opcode)
{
	WPSAPI_T *g_mc = wps_wksp->mc;
	int mode = wps_wksp->wps_mode;
	int restart = 0;
	uint16 mgmt_data;
	char tmp[100];

	if (opcode == WPS_SUCCESS) {
		switch (mode) {
		case EModeUnconfAp:
		{
			/*
			 * AP (Enrollee) must have just completed initial setup
			 * p_encrSettings contain CTlvEsM8Ap
			 */
			CTlvEsM8Ap *p_tlvEncr;
			char *cp_data, *nwKey;
			uint16 data16;
			DevInfo *devinfo = g_mc->mp_info->mp_deviceInfo;
			CTlvNwKey *p_tlvKey;
			WpsEnrCred credential;

			memset(&credential, 0, sizeof(credential));

			TUTRACE((TUTRACE_INFO, "Entering EModeUnconfAp\n"));

			p_tlvEncr = (CTlvEsM8Ap *)g_mc->mp_enrSM->mp_peerEncrSettings;
			if (!p_tlvEncr) {
				TUTRACE((TUTRACE_ERR, "Peer Encr Settings not exist\n"));
				break;
			}

			/* Fill in SSID */
			cp_data = (char *)(p_tlvEncr->ssid.m_data);
			data16 = p_tlvEncr->ssid.tlvbase.m_len;
			credential.ssidLen = data16;
			strncpy(devinfo->ssid, cp_data, data16);
			devinfo->ssid[data16] = '\0';
			TUTRACE((TUTRACE_INFO, "SSID = %s\n", devinfo->ssid));

			/* Fill in KeyMgmt from the encrSettings TLV */
			mgmt_data = p_tlvEncr->authType.m_data;
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: "
				"Get Key Mgmt mgmt_data is %d\n", mgmt_data));
			if (mgmt_data == WPS_AUTHTYPE_SHARED) {
				strncpy(devinfo->keyMgmt, "SHARED", 6);
				devinfo->keyMgmt[6] = '\0';
			}
			else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
				strncpy(devinfo->keyMgmt, "WPA-PSK", 7);
				devinfo->keyMgmt[7] = '\0';
			}
			else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
				strncpy(devinfo->keyMgmt, "WPA2-PSK", 8);
				devinfo->keyMgmt[8] = '\0';
			}
			else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK
				| WPS_AUTHTYPE_WPA2PSK)) {
				strncpy(devinfo->keyMgmt, "WPA-PSK WPA2-PSK", 16);
				devinfo->keyMgmt[16] = '\0';
			}
			else {
				strncpy(devinfo->keyMgmt, "OPEN", 4);
				devinfo->keyMgmt[4] = '\0';
			}
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: Get Key Mgmt type is %s\n",
				devinfo->keyMgmt));

			/* get the real cypher */
			mgmt_data = p_tlvEncr->encrType.m_data;
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: Get Encr type is %d\n",
				mgmt_data));
			devinfo->crypto = mgmt_data;
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: Get encr type is %d\n",
				devinfo->crypto));

			/* get wep key index if exist */
			devcfg_setWepIdx(g_mc->mp_info, p_tlvEncr->wepIdx.m_data);
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: wep index = %d\n",
				p_tlvEncr->wepIdx.m_data));

			/* Fill in psk */
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: step 1\n"));

			p_tlvKey = (CTlvNwKey *)(list_getFirst(p_tlvEncr->nwKey));
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: Step 2 tlv key : %p \n",
				p_tlvKey));

			nwKey = (char *)p_tlvKey->m_data;
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: Save PSK info\n"));

			devcfg_setNwKey(g_mc->mp_info, nwKey, p_tlvKey->tlvbase.m_len);
			TUTRACE((TUTRACE_INFO, "MC_InitiateRegistration: calling callback\n"));

			/* constructure to WpsEnrCred format */
			memcpy(credential.nwKey, nwKey, p_tlvKey->tlvbase.m_len);
			credential.nwKeyLen = p_tlvKey->tlvbase.m_len;

			memcpy(credential.ssid, devinfo->ssid, SIZE_SSID_LENGTH);
			memcpy(credential.keyMgmt, devinfo->keyMgmt, SIZE_20_BYTES);
			credential.encrType = devinfo->crypto;
			credential.wepIndex = devcfg_getWepIdx(g_mc->mp_info);

			/* Apply settings to driver */
			restart = wps_set_wsec(wps_wksp->ess_id, wps_wksp->ifname,
				&credential, mode);

			/*
			  * Delete the locally stored encrypted settings for this
			  * registration instance
			  */
			if (g_mc->mp_tlvEsM7Ap) {
				reg_msg_m7ap_del(g_mc->mp_tlvEsM7Ap, 0);
				g_mc->mp_tlvEsM7Ap = NULL;
			}
			/* Delete encrypted settings that were sent from SM */
			reg_msg_m8ap_del(p_tlvEncr, 0);
			TUTRACE((TUTRACE_INFO, "MC_ProcessRegCompleted: \n"));
			break;
		}

		case EModeApProxyRegistrar:
		{
			TlvEsM7Enr *p_tlvEncr;

			if (wps_wksp->config_state == WPS_SCSTATE_UNCONFIGURED) {
				WpsEnrCred credential;
				devcfg *ap_devcfg = g_mc->mp_info;
				DevInfo *devinfo = g_mc->mp_info->mp_deviceInfo;

				memset(&credential, 0, sizeof(credential));

				/* Fill in SSID */
				credential.ssidLen = strlen(devinfo->ssid);
				strncpy(credential.ssid, devinfo->ssid,	credential.ssidLen);
				TUTRACE((TUTRACE_INFO, "ssid = \"%s\", ssid length = %d\n",
					credential.ssid, credential.ssidLen));

				/* Fill in Key */
				credential.nwKeyLen = ap_devcfg->m_nwKeyLen;
				memcpy(credential.nwKey, ap_devcfg->m_nwKey, ap_devcfg->m_nwKeyLen);
				TUTRACE((TUTRACE_INFO, "key = \"%s\", key length = %d\n",
					credential.nwKey, credential.nwKeyLen));

				/* Fill in KeyMgmt */
				memcpy(credential.keyMgmt, wps_wksp->default_keyMgmt,
					SIZE_20_BYTES);

				/*
				 * Set AES+TKIP in OOB mode, otherwise in WPS test plan 4.2.4
				 * the Broadcom legacy is not able to associate in TKIP
				 */
				credential.encrType = devinfo->crypto;
				credential.encrType |= WPS_ENCRTYPE_TKIP;
#ifdef BCMWPA2
				credential.encrType |= WPS_ENCRTYPE_AES;
#endif
				TUTRACE((TUTRACE_INFO, "KeyMgmt = %s, encryptType = %x\n",
					credential.keyMgmt, credential.encrType));

				/* Apply settings to driver */
				restart = wps_set_wsec(wps_wksp->ess_id, wps_wksp->ifname,
					&credential, mode);
			}

			ether_etoa(wps_wksp->mac_sta, tmp);
			wps_ui_set_env("wps_sta_mac", tmp);
			wps_set_conf("wps_sta_mac", tmp);

			p_tlvEncr = (TlvEsM7Enr *)g_mc->mp_regSM->mp_peerEncrSettings;
			/* Delete encrypted settings that were sent from SM */
			if (!p_tlvEncr) {
				TUTRACE((TUTRACE_ERR, "Peer Encr Settings not exist\n"));
				break;
			}
			reg_msg_m7enr_del(p_tlvEncr, 0);

			/*
			 * According to WPS sepc 1.0h Section 10.3 page 79 middle,
			 * Remove enrollee's probe request when Registrar successfully
			 * runs the PBC method
			 */
			if (!strcmp(wps_ui_get_env("wps_sta_pin"), "00000000"))
				wps_pb_clear_sta(wps_wksp->mac_sta);

			break;
		}

		default:
			break;
		}

		wps_ui_set_env("wps_pinfail", "0");
		wps_set_conf("wps_pinfail", "0");

		if (restart)
			wps_wksp->return_code = WPS_RESULT_SUCCESS_RESTART;
		else
			wps_wksp->return_code = WPS_RESULT_SUCCESS;

		wps_setProcessStates(WPS_OK);
	}
	else {
		wps_wksp->return_code = WPS_RESULT_FAILURE;

		if (opcode == WPS_ERR_CONFIGAP_PINFAIL)
			wps_wksp->return_code = WPS_RESULT_CONFIGAP_PINFAIL;

		wps_setProcessStates(WPS_MSG_ERR);
	}

	return 1;
}

static void
wpsap_start_WPSReg(wpsap_wksp_t *ap_wksp)
{
	char *send_buf;
	uint32 send_buf_len;
	int ret_val = WPS_CONT;

	ret_val = ap_eap_sm_startWPSReg(ap_wksp->mac_sta, ap_wksp->mac_ap);
	if (ret_val == WPS_CONT || ret_val == WPS_SEND_MSG_CONT) {
		wpsap_bcmwps_add_pktcnt(ap_wksp);
		/* check return code to do more things */
		if (ret_val == WPS_SEND_MSG_CONT) {
			send_buf_len = ap_eap_sm_get_msg_to_send(&send_buf);
			if (send_buf_len >= 0)
				ap_eap_sm_sendMsg(send_buf, send_buf_len);
		}
	}
}

static int
wpsap_init(wpsap_wksp_t *ap_wksp)
{
	char null_mac[6] = {0};

	TUTRACE((TUTRACE_INFO, "*********************************************\n"));
	TUTRACE((TUTRACE_INFO, "Wi-Fi Simple Config Application - Broadcom Corp.\n"));
	TUTRACE((TUTRACE_INFO, "*********************************************\n"));
	TUTRACE((TUTRACE_INFO, "Initializing...\n"));

	if (wpsap_wksp_init(ap_wksp) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "Can't init workspace parameter...\n"));
		return 0;
	}
	TUTRACE((TUTRACE_INFO, "wpsap workspace init completed\n"));

	/*
	 * Sendout 'EAP-Request(Start) message', because we already
	 * receive 'EAP-Response/Identity' packet in monitor.
	 */
	if (memcmp(ap_wksp->mac_sta, null_mac, 6) != 0) {
		wpsap_start_WPSReg(ap_wksp);
	}

	return (int)ap_wksp;
}

static int
wpsap_deinit(wpsap_wksp_t *wps_wksp)
{
	if (wps_wksp)
		wpsap_wksp_deinit(wps_wksp);
	free(wps_wksp);

	return 0;
}

static int
wpsap_process(wpsap_wksp_t *wps_wksp, char *buf, int len, int msgtype)
{
	int retVal = WPS_CONT;
	wps_wksp->return_code = WPS_CONT;

	retVal = wpsap_process_msg(wps_wksp, buf, len, msgtype);

	/* check the return code and close this session if needed */
	if ((retVal == WPS_SUCCESS) ||
		(retVal == WPS_MESSAGE_PROCESSING_ERROR) ||
		(retVal == WPS_ERR_CONFIGAP_PINFAIL &&
		!strcmp(wps_safe_get_conf("wps_aplockdown_cap"), "1"))) {
		/* Close session */
		wpsap_close_session(wps_wksp, retVal);
	}

	return wps_wksp->return_code;
}

static int
wpsap_check_timeout(wpsap_wksp_t *wps_wksp)
{
	unsigned long now = (unsigned long)time(0);

	/*
	 * 1. check timer and do proper action when timeout happened,
	 * like restart wps or something else
	 */
	wps_wksp->return_code = wpsap_time_check(wps_wksp, now);

	if (wps_wksp->return_code == WPS_RESULT_PROCESS_TIMEOUT)
		wps_setProcessStates(WPS_TIMEOUT);

	return wps_wksp->return_code;
}

int
wpsap_open_session(void *wps_app, int mode, unsigned char *mac, unsigned char *mac_sta,
	char *osifname, char *enr_nonce, char *priv_key)
{
	uint32 mode_enum[3] = {EModeUnconfAp, EModeApProxyRegistrar, EModeApProxy};
	char *wps_mode;

	char tmp[100];
	char *wlnames;
	char ifname[IFNAMSIZ];
	char *next = NULL;
	int i, imax;
	wpsap_wksp_t *ap_wksp;

	if (!osifname || !mac) {
		TUTRACE((TUTRACE_ERR, "Invalid parameter!!\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save wireless interface name for UI */
	wps_ui_set_env("wps_ifname", osifname);

	sprintf(tmp, "%s_wps_mode", osifname);
	wps_mode = wps_safe_get_conf(tmp);
	if (strcmp(wps_mode, "enabled") != 0) {
		TUTRACE((TUTRACE_ERR, "Cannot init an STA interface!!\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save maximum instance number, and probe if any wl interface */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);

		foreach(ifname, wlnames, next) {
			if (!strcmp(ifname, osifname)) {
				goto found;
			}
		}
	}

found:
	if (i == imax) {
		TUTRACE((TUTRACE_ERR, "Can't find upnp instance for %s!!\n", osifname));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Check mode */
	switch (mode) {
	case WPSM_UNCONFAP:
		/* If AP is under aplockdown mode, disable all configuring to AP */
		if (wps_aplockdown_islocked()) {
			TUTRACE((TUTRACE_INFO, "AP in lock up state, deny AP configuring\n"));
			return WPS_ERR_OPEN_SESSION;
		}
	case WPSM_BUILTINREG:
	case WPSM_PROXY:
		break;
	default:
		TUTRACE((TUTRACE_ERR, "invalid mode parameter(%d)\n", mode));
		return WPS_ERR_OPEN_SESSION;
	}

	/* clear pending flag */
	wps_ui_clear_pending();
	wps_upnp_clear_ssr_timer();

	/* init workspace */
	if ((ap_wksp = (wpsap_wksp_t *)alloc_init(sizeof(wpsap_wksp_t))) == NULL) {
		TUTRACE((TUTRACE_INFO, "Can not allocate memory for wps structer...\n"));
		return WPS_ERR_OPEN_SESSION;
	}

	/* Save variables from ui env */
	ap_wksp->wps_mode = mode_enum[mode];
	ap_wksp->ess_id = i;
	strcpy(ap_wksp->ifname, osifname);
	memcpy(ap_wksp->mac_ap, mac, SIZE_6_BYTES);
	if (mac_sta)
		memcpy(ap_wksp->mac_sta, mac_sta, SIZE_6_BYTES);
	ap_wksp->pre_nonce = enr_nonce;
	ap_wksp->pre_privkey = priv_key;

	/* update mode */
	TUTRACE((TUTRACE_INFO, "Start %s mode\n",
		(mode == WPSM_UNCONFAP ? "unconfap" :
		mode == WPSM_BUILTINREG ? "builtinreg" :
		mode == WPSM_PROXY ? "proxy" : "Error!! known mode")));

	((wps_app_t *)wps_app)->wksp = wpsap_init(ap_wksp);
	if (((wps_app_t *)wps_app)->wksp == 0) {
		wpsap_deinit(ap_wksp);
		return WPS_ERR_OPEN_SESSION;
	}

	((wps_app_t *)wps_app)->mode = mode;
	((wps_app_t *)wps_app)->close = (int (*)(int))wpsap_deinit;
	((wps_app_t *)wps_app)->process = (int (*)(int, char *, int, int))wpsap_process;
	((wps_app_t *)wps_app)->check_timeout = (int (*)(int))wpsap_check_timeout;

	return WPS_CONT;
}
