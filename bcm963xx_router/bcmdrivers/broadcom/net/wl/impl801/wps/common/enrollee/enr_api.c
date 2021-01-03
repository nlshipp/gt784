/*
 * Enrollee API
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_api.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#if defined(WIN32)
#include <stdio.h>
#include <windows.h>
#endif

#if !defined(WIN32)
#include <string.h>
#endif
#include <bn.h>
#include <wps_dh.h>

#include <portability.h>
#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <wpserror.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <tutrace.h>

#include <slist.h>

#include <info.h>
#include <wpsapi.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <wps_sta.h>
#include <wlioctl.h>

#define SSR_WALK_TIME 120

static WPSAPI_T *g_mc = NULL;
static WPSAPI_T *wps_new(void);
static uint32 wps_initReg(IN EMode e_currMode, IN EMode e_targetMode,
	IN char *devPwd, IN char *ssid, IN bool b_useIe /* = false */,
	IN uint16 devPwdId /* = WPS_DEVICEPWDID_DEFAULT */);
static uint32 wps_exit(void);
static void wps_delete(void);
static uint32 wps_stopStack(void);
static uint32 wps_startStack(IN EMode e_mode);
static uint32 wps_switchModeOn(IN EMode e_mode);
static uint32 wps_switchModeOff(IN EMode e_mode);

/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
uint32
wps_enr_config_init(DevInfo *dev_info)
{
	g_mc = wps_new();
	if (g_mc) {
		if ((g_mc->mp_info = devcfg_new())) {
			DevInfo *info =  g_mc->mp_info->mp_deviceInfo;
			memcpy(info, dev_info, sizeof(DevInfo));
			info->scState = WPS_SCSTATE_UNCONFIGURED; /* Unconfigured */
			g_mc->mp_info->mb_regWireless = 0;
		}
		else
			goto error;

		g_mc->mp_regProt = (CRegProtocol *)alloc_init(sizeof(CRegProtocol));
		if (!g_mc->mp_regProt)
			goto error;

		g_mc->mb_initialized = true;
	}
	else {
		TUTRACE((TUTRACE_ERR, "enrApi::Init: MC initialization failed\n"));
		goto error;
	}

	/*
	 * Everything's initialized ok
	 * Transfer control to member variables
	 */
	return WPS_SUCCESS;

error:

	TUTRACE((TUTRACE_ERR, "enrApi::Init failed\n"));
	if (g_mc) {
		wps_cleanup();
	}

	return WPS_ERR_SYSTEM;
}

/*
 * Name        : Init
 * Description : Initialize member variables
 * Arguments   : none
 * Return type : uint32 - result of the initialize operation
 */
uint32
wps_enr_reg_config_init(DevInfo *dev_info, char *nwKey, int nwKeyLen)
{
	g_mc = wps_new();
	if (g_mc) {
		if ((g_mc->mp_info = devcfg_new())) {
			DevInfo *info =  g_mc->mp_info->mp_deviceInfo;
			memcpy(info, dev_info, sizeof(DevInfo));
			info->scState = WPS_SCSTATE_CONFIGURED; /* Configured */
		}
		else
			goto error;

		/* update nwKey */
		if (nwKey) {
			g_mc->mp_info->m_nwKeyLen = nwKeyLen;
			strncpy(g_mc->mp_info->m_nwKey, nwKey, nwKeyLen);
		}
		g_mc->mp_info->mb_nwKeySet = true; /* always true */

		g_mc->mp_info->mb_regWireless = 1;
		g_mc->mp_regProt = (CRegProtocol *)alloc_init(sizeof(CRegProtocol));
		if (!g_mc->mp_regProt)
			goto error;

		g_mc->mb_initialized = true;
	}
	else {
		TUTRACE((TUTRACE_ERR, "enrApi::Init: MC initialization failed\n"));
		goto error;
	}

	/*
	 * Everything's initialized ok
	 * Transfer control to member variables
	 */
	return WPS_SUCCESS;

error:

	TUTRACE((TUTRACE_ERR, "enrApi::Init failed\n"));
	if (g_mc) {
		wps_cleanup();
	}

	return WPS_ERR_SYSTEM;
}

/*
 * Name        : DeInit
 * Description : Deinitialize member variables
 * Arguments   : none
 * Return type : uint32
 */
void
wps_cleanup()
{
	if (g_mc) {
		wps_exit();
		wps_delete();
	}
	TUTRACE((TUTRACE_INFO, "enrApi::MC deinitialized\n"));
}

uint32
wps_start_enrollment(char *pin, unsigned long time)
{
	uint devicepwid = WPS_DEVICEPWDID_DEFAULT;
	char *PBC_PIN = "00000000\0";
	char *dev_pin = pin;
	uint32 ret;

	ret = wps_startStack(EModeClient);

	/* for enrollment again case */
	if (ret == MC_ERR_STACK_ALREADY_STARTED)
		ret = WPS_SUCCESS;

	if (ret != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Start EModeClient failed\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	if (!pin) {
		dev_pin = PBC_PIN;
		devicepwid = WPS_DEVICEPWDID_PUSH_BTN;
	}

	return wps_initReg(EModeClient, EModeRegistrar, dev_pin, NULL, false, devicepwid);
}

uint32
wps_start_registration(char *pin, unsigned long time)
{
	uint devicepwid = WPS_DEVICEPWDID_DEFAULT;
	char *dev_pin = pin;

	if (wps_startStack(EModeRegistrar) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "Start EModeRegistrar failed\n"));
		return WPS_ERR_OUTOFMEMORY;
	}

	return wps_initReg(EModeRegistrar, EModeRegistrar, dev_pin, NULL, false, devicepwid);
}

void
wps_get_credentials(WpsEnrCred* credential, const char *ssid, int len)
{
	CTlvEsM8Sta *p_tlvEncr = (CTlvEsM8Sta *)(g_mc->mp_enrSM->mp_peerEncrSettings);
	CTlvCredential *p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	if (len > 0 && (list_getCount(p_tlvEncr->credential) > 1)) {
		/* try to find same ssid */
		while (p_tlvCred) {
			if (p_tlvCred->ssid.tlvbase.m_len == len &&
				!strncmp((char*)p_tlvCred->ssid.m_data, ssid, len))
				break;
			/* get next one */
			p_tlvCred = (CTlvCredential *)(list_getNext(p_tlvEncr->credential,
				p_tlvCred));
		}

		/* no anyone match, use first one */
		if (p_tlvCred == NULL) {
			p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
		}
	}

	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = credential->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvCred->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */
	mgmt_data = p_tlvCred->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvCred->WEPKeyIndex.m_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

void
wps_get_reg_M7credentials(WpsEnrCred* credential)
{
	CTlvEsM7Ap *p_tlvEncr = (CTlvEsM7Ap *)(g_mc->mp_regSM->mp_peerEncrSettings);
	CTlvNwKey *nwKey;
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	if (!p_tlvEncr) {
		TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: No peer Encrypt settings\n"));
		return;
	}
	TUTRACE((TUTRACE_INFO, "wps_get_reg_M7credentials: p_tlvEncr=0x%x\n", (int)p_tlvEncr));

	/* Fill in SSID */
	cp_data = (char *)(p_tlvEncr->ssid.m_data);
	data16 = credential->ssidLen = p_tlvEncr->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvEncr->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else  if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */

	mgmt_data = p_tlvEncr->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in WEP index, no matter it's WEP type or not */
	credential->wepIndex = p_tlvEncr->wepIdx.m_data;

	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: p_tlvEncr->nwKey=0x%x\n",
		(int)p_tlvEncr->nwKey));
	/* Fill in PSK */
	nwKey = (CTlvNwKey *)(list_getFirst(p_tlvEncr->nwKey));
	TUTRACE((TUTRACE_INFO, "wps_get_reg_credentials: nwKey=0x%x\n", (int)nwKey));

	data16 = nwKey->tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, nwKey->m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

void
wps_get_reg_M8credentials(WpsEnrCred* credential)
{
	CTlvEsM8Sta *p_tlvEncr = (CTlvEsM8Sta *)
		(g_mc->mp_regSM->m_sm->mps_regData->staEncrSettings);
	CTlvCredential *p_tlvCred = (CTlvCredential *)(list_getFirst(p_tlvEncr->credential));
	char *cp_data;
	uint16 data16;
	uint16 mgmt_data;

	/* Fill in SSID */
	cp_data = (char *)(p_tlvCred->ssid.m_data);
	data16 = credential->ssidLen = p_tlvCred->ssid.tlvbase.m_len;
	strncpy(credential->ssid, cp_data, data16);
	credential->ssid[data16] = '\0';

	/* Fill in keyMgmt */
	mgmt_data = p_tlvCred->authType.m_data;

	if (mgmt_data == WPS_AUTHTYPE_SHARED) {
		strncpy(credential->keyMgmt, "SHARED", 6);
		credential->keyMgmt[6] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPAPSK) {
		strncpy(credential->keyMgmt, "WPA-PSK", 7);
		credential->keyMgmt[7] = '\0';
	}
	else if (mgmt_data == WPS_AUTHTYPE_WPA2PSK) {
		strncpy(credential->keyMgmt, "WPA2-PSK", 8);
		credential->keyMgmt[8] = '\0';
	}
	else if (mgmt_data == (WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK)) {
		strncpy(credential->keyMgmt, "WPA-PSK WPA2-PSK", 16);
		credential->keyMgmt[16] = '\0';
	}
	else {
		strncpy(credential->keyMgmt, "OPEN", 4);
		credential->keyMgmt[4] = '\0';
	}

	/* get the real cypher */

	mgmt_data = p_tlvCred->encrType.m_data;
	TUTRACE((TUTRACE_INFO, "Get Encr type is %d\n", mgmt_data));

	credential->encrType = mgmt_data;

	/* Fill in PSK */
	data16 = p_tlvCred->nwKey.tlvbase.m_len;
	memset(credential->nwKey, 0, SIZE_64_BYTES);
	memcpy(credential->nwKey, p_tlvCred->nwKey.m_data, data16);
	/* we have to set key length here */
	credential->nwKeyLen = data16;
}

static WPSAPI_T *
wps_new(void)
{
	/* ONLY 1 instance allowed */
	if (g_mc) {
		TUTRACE((TUTRACE_ERR, "WPSAPI has already been created !!\n"));
		return NULL;
	}
	g_mc = (WPSAPI_T *)malloc(sizeof(WPSAPI_T));
	if (!g_mc)
		return NULL;

	TUTRACE((TUTRACE_INFO, "WPSAPI constructor\n"));
	g_mc->mb_initialized = false;
	g_mc->mb_stackStarted = false;
	g_mc->mb_requestedPwd = false;

	g_mc->mp_regProt = NULL;
	g_mc->mp_regSM = NULL;
	g_mc->mp_enrSM = NULL;
	g_mc->mp_info = NULL;

	g_mc->mp_regInfoList = NULL;
	g_mc->mp_enrInfoList = NULL;
	g_mc->mp_neighborInfoList = NULL;

	g_mc->mp_tlvEsM7Ap = NULL;
	g_mc->mp_tlvEsM7Enr = NULL;
	g_mc->mp_tlvEsM8Ap = NULL;
	g_mc->mp_tlvEsM8Sta = NULL;

	memset(&g_mc->m_peerMacAddr, 0, sizeof(g_mc->m_peerMacAddr));

	return g_mc;
}

static void
wps_delete(void)
{
	TUTRACE((TUTRACE_INFO, "WPSAPI destructor\n"));

	free(g_mc);
	g_mc = NULL;
}

static uint32
wps_exit(void)
{
	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "MC_DeInit: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	/* Stop stack anyway to free DH key if existing */
	wps_stopStack();

	if (g_mc->mp_regProt)
		free(g_mc->mp_regProt);

	devcfg_delete(g_mc->mp_info);

	/*
	 * These data objects should have been already deleted,
	 * but double-check to make sure
	 */
	if (g_mc->mp_tlvEsM7Ap)
		reg_msg_m7ap_del(g_mc->mp_tlvEsM7Ap, 0);
	if (g_mc->mp_tlvEsM7Enr)
		reg_msg_m7enr_del(g_mc->mp_tlvEsM7Enr, 0);
	if (g_mc->mp_tlvEsM8Ap)
		reg_msg_m8ap_del(g_mc->mp_tlvEsM8Ap, 0);
	if (g_mc->mp_tlvEsM8Sta)
		reg_msg_m8sta_del(g_mc->mp_tlvEsM8Sta, 0);

	g_mc->mb_initialized = false;

	return WPS_SUCCESS;
}

static uint32
wps_startStack(IN EMode e_mode)
{
	uint32 ret = WPS_SUCCESS;

	if (!g_mc->mb_initialized) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Not initialized\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (g_mc->mb_stackStarted) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Already started\n"));
		return MC_ERR_STACK_ALREADY_STARTED;
	}

	/*
	 * Stack initialized now. Can be started.
	 * Create enrollee and registrar lists
	 */
	TUTRACE((TUTRACE_INFO, "MC_StartStack: 1\n"));
	g_mc->mp_regInfoList = list_create();

	if (!g_mc->mp_regInfoList) {
		TUTRACE((TUTRACE_ERR, "MC_Init: g_mc->mp_regInfoList not created"));
		ret = MC_ERR_STACK_NOT_STARTED;
	}
	g_mc->mp_enrInfoList = list_create();
	if (!g_mc->mp_enrInfoList) {
		TUTRACE((TUTRACE_ERR, "MC_Init: g_mc->mp_enrInfoList not created"));
		ret = MC_ERR_STACK_NOT_STARTED;
	}

	g_mc->mp_neighborInfoList = list_create();
	if (!g_mc->mp_neighborInfoList) {
		TUTRACE((TUTRACE_ERR, "MC_Init: g_mc->mp_neighborInfoList2 not created"));
		ret = MC_ERR_STACK_NOT_STARTED;

		TUTRACE((TUTRACE_INFO, "MC_StartStack: 2\n"));
	}

	if (WPS_SUCCESS != ret) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: could not create lists\n"));
		if (g_mc->mp_regInfoList)
			list_delete(g_mc->mp_regInfoList);
		if (g_mc->mp_enrInfoList)
			list_delete(g_mc->mp_enrInfoList);
		if (g_mc->mp_neighborInfoList)
			list_delete(g_mc->mp_neighborInfoList);
		return ret;
	}

	g_mc->mp_info->e_mode = e_mode;

	/* set b_ap flag */
	if (g_mc->mp_info->mp_deviceInfo->primDeviceCategory == WPS_DEVICE_TYPE_CAT_NW_INFRA &&
	    g_mc->mp_info->mp_deviceInfo->primDeviceSubCategory == WPS_DEVICE_TYPE_SUB_CAT_NW_AP)
		g_mc->mp_info->mp_deviceInfo->b_ap = true;
	else
		g_mc->mp_info->mp_deviceInfo->b_ap = false;

	TUTRACE((TUTRACE_INFO, "MC_StartStack: 3\n"));

	/* Initialize other member variables */
	g_mc->mb_requestedPwd = false;

	/*
	 * Explicitly perform some actions if the stack is configured
	 * in a particular mode
	 */
	e_mode = devcfg_getConfiguredMode(g_mc->mp_info);
	TUTRACE((TUTRACE_INFO, "MC_StartStack: 4\n"));
	if (WPS_SUCCESS != wps_switchModeOn(e_mode)) {
		TUTRACE((TUTRACE_INFO, "MC_StartStack: Could not start trans\n"));
		return MC_ERR_STACK_NOT_STARTED;
	}

	g_mc->mb_stackStarted = true;

	ret = WPS_SUCCESS;

	return ret;
}

static uint32
wps_stopStack(void)
{
	LISTITR m_listItr;
	LPLISTITR listItr;

	if (!g_mc->mb_initialized)
		return WPS_ERR_NOT_INITIALIZED;

	/* TODO: need to purge data from callback threads here as well */

	/*
	 * Explicitly perform some actions if the stack is configured
	 * in a particular mode
	 */
	wps_switchModeOff(devcfg_getConfiguredMode(g_mc->mp_info));

	/* Reset other member variables */

	/* Delete all old data from Registrar and Enrollee Info lists */
	if (g_mc->mp_regInfoList) {
		DevInfo *p_deviceInfo;
		listItr = list_itrFirst(g_mc->mp_regInfoList, &m_listItr);

		while ((p_deviceInfo = (DevInfo *)list_itrGetNext(listItr))) {
			free(p_deviceInfo);
		}
		list_delete(g_mc->mp_regInfoList);
	}
	if (g_mc->mp_enrInfoList) {
		DevInfo *p_deviceInfo;
		listItr = list_itrFirst(g_mc->mp_enrInfoList, &m_listItr);

		while ((p_deviceInfo = (DevInfo *)list_itrGetNext(listItr))) {
			free(p_deviceInfo);
		}
		list_delete(g_mc->mp_enrInfoList);
	}

	if (g_mc->mp_neighborInfoList) {
		NeighborInfo *p_neighborInfo;
		listItr = list_itrFirst(g_mc->mp_neighborInfoList, &m_listItr);

		while ((p_neighborInfo = (NeighborInfo *)list_itrGetNext(listItr))) {
			free(p_neighborInfo);
		}
		list_delete(g_mc->mp_neighborInfoList);
	}

	g_mc->mb_stackStarted = false;

	return WPS_SUCCESS;
}

/* Called by the app to initiate registration */
static uint32
wps_initReg(IN EMode e_currMode, IN EMode e_targetMode,
	IN char *devPwd, IN char *ssid, IN bool b_useIe /* = false */,
	IN uint16 devPwdId /* = WPS_DEVICEPWDID_DEFAULT */)
{
	if (WPS_SUCCESS == devcfg_setDevPwdId(g_mc->mp_info, devPwdId)) {
		/* Reset local device info in SM */
		if (e_currMode == EModeClient)
			state_machine_update_devinfo(g_mc->mp_enrSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
		else
			state_machine_update_devinfo(g_mc->mp_regSM->m_sm,
				devcfg_getDeviceInfo(g_mc->mp_info));
	}

	/* Pass in a NULL for the encrypted settings TLV */
	if (e_currMode == EModeClient) {
		g_mc->mp_tlvEsM7Enr = NULL;

		/* Call into the SM now */
		if (WPS_SUCCESS != enr_sm_initializeSM(g_mc->mp_enrSM, NULL, /* p_regInfo */
			(void *)g_mc->mp_tlvEsM7Enr, /* M7 Enr encrSettings */
			NULL, /* M7 AP encrSettings */
			devPwd, /* device password */
			strlen(devPwd))	/* dev pwd length */) {
			TUTRACE((TUTRACE_ERR, "Can't init enr_sm\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		/* Start the EAP transport */
		/* PHIL : link eap and wps state machines  */
		wps_eap_sta_init(NULL, g_mc->mp_enrSM, EModeClient);
	}
	else if (e_currMode == EModeRegistrar) {
		/* Check to see if the devPwd from the app is valid */
		if (!devPwd) {
			/* App sent in a NULL, and USB key isn't sent */
			TUTRACE((TUTRACE_ERR, "MC_InitiateRegistration: "
				"No password to use, expecting one\n"));
			return WPS_ERR_SYSTEM;
		}
		else {
			if (strlen(devPwd)) {
				TUTRACE((TUTRACE_ERR, "Using pwd from app\n"));

				/* Pass the device password on to the SM */
				state_machine_set_passwd(g_mc->mp_regSM->m_sm, devPwd,
					(uint32)(strlen(devPwd)));
			}
		}

		/* Fix transportType to EAP */
		g_mc->mp_regSM->m_sm->m_transportType = TRANSPORT_TYPE_EAP;

		/* Start the EAP transport */
		/* PHIL : link eap and wps state machines  */
		wps_eap_sta_init(NULL, g_mc->mp_regSM, EModeRegistrar);
	}

	return WPS_SUCCESS;
}

bool
wps_validateChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);
	accum += 1 * ((PIN / 1) % 10);

	return (0 == (accum % 10));
}

static uint32
wps_createM8AP(WPSAPI_T *g_mc, IN char *cp_ssid)
{
	char *cp_data;
	uint16 data16;
	uint32 nwKeyLen = 0;
	uint8 *p_macAddr;
	CTlvNwKey *p_tlvKey;
	uint8 wep_exist = 0;

	/*
	 * Create the Encrypted Settings TLV for AP config
	 * Also store this info locally so it can be used
	 * when registration completes for reconfiguration.
	 */
	if (g_mc->mp_tlvEsM8Ap) {
		reg_msg_m8ap_del(g_mc->mp_tlvEsM8Ap, 0);
	}
	g_mc->mp_tlvEsM8Ap = reg_msg_m8ap_new();
	if (!g_mc->mp_tlvEsM8Ap)
		return WPS_ERR_SYSTEM;

	/* ssid */
	if (cp_ssid) {
		/* Use this instead of the one from the config file */
		cp_data = cp_ssid;
		data16 = (uint16)strlen(cp_ssid);
	}
	else {
		/* Use the SSID from the config file */
		cp_data = devcfg_getSSID(g_mc->mp_info, &data16);
	}

	tlv_set(&g_mc->mp_tlvEsM8Ap->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: ssid=%s\n", cp_data));

	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: do REAL sec config here...\n"));
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_AUTHTYPE_SHARED;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK)
		data16 = WPS_AUTHTYPE_WPAPSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK2)
		data16 = WPS_AUTHTYPE_WPA2PSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_BOTH)
		data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
	else
		data16 = WPS_AUTHTYPE_OPEN;
	tlv_set(&g_mc->mp_tlvEsM8Ap->authType, WPS_ID_AUTH_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: Auth type = 0x%x...\n", data16));

	/* encrType */
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_ENCRTYPE_WEP;
	else if (data16 == WPS_AUTHTYPE_OPEN) {
		if (devcfg_getWep(g_mc->mp_info))
			data16 = WPS_ENCRTYPE_WEP;
		else
			data16 = WPS_ENCRTYPE_NONE;
	}
	else
		data16 = devcfg_getCrypto(g_mc->mp_info);
	tlv_set(&g_mc->mp_tlvEsM8Ap->encrType, WPS_ID_ENCR_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Ap: encr type = 0x%x...\n", data16));

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKey */
	p_tlvKey = (CTlvNwKey *)tlv_new(WPS_ID_NW_KEY);
	if (!p_tlvKey)
		return WPS_ERR_SYSTEM;

	cp_data = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
	tlv_set(p_tlvKey, WPS_ID_NW_KEY, cp_data, nwKeyLen);
	if (!list_addItem(g_mc->mp_tlvEsM8Ap->nwKey, p_tlvKey)) {
		tlv_delete(p_tlvKey, 0);
		return WPS_ERR_SYSTEM;
	}

	/* macAddr */
	p_macAddr = devcfg_getMacAddr(g_mc->mp_info);
	data16 = SIZE_MAC_ADDR;
	tlv_set(&g_mc->mp_tlvEsM8Ap->macAddr, WPS_ID_MAC_ADDR, p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&g_mc->mp_tlvEsM8Ap->wepIdx, WPS_ID_WEP_TRANSMIT_KEY,
			(void *)((uint32)devcfg_getWepIdx(g_mc->mp_info)), 0);

	return WPS_SUCCESS;
}

static uint32
wps_createM8Sta(WPSAPI_T *g_mc)
{
	char *cp_data;
	uint8 *p_macAddr;
	uint16 data16;
	uint32 nwKeyLen = 0;
	CTlvCredential *p_tlvCred;
	uint8 wep_exist = 0;

	/*
	 * Create the Encrypted Settings TLV for STA config
	 * We will also need to delete this blob eventually, as the
	 * SM will not delete it.
	 */
	if (g_mc->mp_tlvEsM8Sta) {
		reg_msg_m8sta_del(g_mc->mp_tlvEsM8Sta, 0);
	}
	g_mc->mp_tlvEsM8Sta = reg_msg_m8sta_new();
	if (!g_mc->mp_tlvEsM8Sta)
		return WPS_ERR_SYSTEM;

	/* credential */
	p_tlvCred = (CTlvCredential *)malloc(sizeof(CTlvCredential));
	if (!p_tlvCred)
		return WPS_ERR_SYSTEM;

	memset(p_tlvCred, 0, sizeof(CTlvCredential));

	/* Fill in credential items */
	/* nwIndex */
	tlv_set(&p_tlvCred->nwIndex, WPS_ID_NW_INDEX, (void *)1, 0);

	/* ssid */
	cp_data = devcfg_getSSID(g_mc->mp_info, &data16);
	tlv_set(&p_tlvCred->ssid, WPS_ID_SSID, (uint8 *)cp_data, data16);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: ssid=%s\n", cp_data));

	/* security settings */
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: do REAL sec config here...\n"));
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_AUTHTYPE_SHARED;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK)
		data16 = WPS_AUTHTYPE_WPAPSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_PSK2)
		data16 = WPS_AUTHTYPE_WPA2PSK;
	else if (devcfg_getKeyMgmtType(g_mc->mp_info) == WPS_WL_AKM_BOTH)
		data16 = WPS_AUTHTYPE_WPAPSK | WPS_AUTHTYPE_WPA2PSK;
	else
		data16 = WPS_AUTHTYPE_OPEN;
	tlv_set(&p_tlvCred->authType, WPS_ID_AUTH_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: Auth type = 0x%x...\n", data16));

	/* encrType */
	if (devcfg_getAuth(g_mc->mp_info))
		data16 = WPS_ENCRTYPE_WEP;
	else if (data16 == WPS_AUTHTYPE_OPEN) {
		if (devcfg_getWep(g_mc->mp_info))
			data16 = WPS_ENCRTYPE_WEP;
		else
			data16 = WPS_ENCRTYPE_NONE;
	}
	else
		data16 = devcfg_getCrypto(g_mc->mp_info);
	tlv_set(&p_tlvCred->encrType, WPS_ID_ENCR_TYPE, (void *)((uint32)data16), 0);
	TUTRACE((TUTRACE_ERR, "MC_CreateTlvEsM8Sta: encr type = 0x%x...\n", data16));

	if (data16 == WPS_ENCRTYPE_WEP)
		wep_exist = 1;

	/* nwKeyIndex */
	tlv_set(&p_tlvCred->nwKeyIndex, WPS_ID_NW_KEY_INDEX, (void *)1, 0);

	/* nwKey */
	cp_data = devcfg_getNwKey(g_mc->mp_info, &nwKeyLen);
	tlv_set(&p_tlvCred->nwKey, WPS_ID_NW_KEY, cp_data, nwKeyLen);

	/* macAddr */
	p_macAddr = devcfg_getMacAddr(g_mc->mp_info);
	data16 = SIZE_MAC_ADDR;
	tlv_set(&p_tlvCred->macAddr, WPS_ID_MAC_ADDR, (uint8 *)p_macAddr, data16);

	/* WepKeyIdx */
	if (wep_exist)
		tlv_set(&p_tlvCred->WEPKeyIndex, WPS_ID_WEP_TRANSMIT_KEY,
			(void *)((uint32)devcfg_getWepIdx(g_mc->mp_info)), 0);

	if (!list_addItem(g_mc->mp_tlvEsM8Sta->credential, p_tlvCred)) {
		tlv_credentialDelete(p_tlvCred, 0);
		return WPS_ERR_SYSTEM;
	}

	return WPS_SUCCESS;
}

/*
 * Name        : SwitchModeOn
 * Description : Switch to the specified mode of operation.
 *                 Turn off other modes, if necessary.
 * Arguments   : IN EMode e_mode - mode of operation to switch to
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOn(IN EMode e_mode)
{
	uint32 ret = WPS_SUCCESS;
	uint32 dum_lock;

	/* Create DH Key Pair */
	DH *p_dhKeyPair;
	BufferObj *bo_pubKey, *bo_sha256Hash;

	if ((bo_pubKey = buffobj_new()) == NULL)
		return WPS_ERR_SYSTEM;

	if ((bo_sha256Hash = buffobj_new()) == NULL) {
		buffobj_del(bo_pubKey);
		return WPS_ERR_SYSTEM;
	}


	TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 1\n"));
	ret = reg_proto_generate_dhkeypair(&p_dhKeyPair, bo_pubKey);
	if (WPS_SUCCESS != ret) {
		buffobj_del(bo_sha256Hash);
		buffobj_del(bo_pubKey);
		if (p_dhKeyPair)
			DH_free(p_dhKeyPair);

		TUTRACE((TUTRACE_ERR,
			"MC_SwitchModeOn: Could not generate DH Key\n"));
		return ret;
	}
	else {
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: 2\n"));
		/* DH Key generated Now generate the SHA 256 hash */
		reg_proto_generate_sha256hash(bo_pubKey, bo_sha256Hash);

		/* Store this info in g_mc->mp_info */
		WpsLock(devcfg_getLock(g_mc->mp_info));
		devcfg_setDHKeyPair(g_mc->mp_info, p_dhKeyPair);
		if (WPS_SUCCESS != (devcfg_setPubKey(g_mc->mp_info, bo_pubKey))) {
			TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not set pubKey\n"));
			ret = WPS_ERR_SYSTEM;
		}
		if (WPS_SUCCESS != (devcfg_setSHA256Hash(g_mc->mp_info, bo_sha256Hash))) {
			TUTRACE((TUTRACE_ERR, "MC_SwitchModeOn: Could not set sha256Hash\n"));
			ret = WPS_ERR_SYSTEM;
		}
		buffobj_del(bo_sha256Hash);
		buffobj_del(bo_pubKey);

		WpsUnlock(devcfg_getLock(g_mc->mp_info));
		if (WPS_ERR_SYSTEM == ret) {
			return ret;
		}
	}

	switch (e_mode) {
	case EModeClient:
	{
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: EModeClient enter\n"));
		/* Instantiate the Enrollee SM */
		g_mc->mp_enrSM = enr_sm_new(g_mc, g_mc->mp_regProt);

		if (!g_mc->mp_enrSM) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not allocate g_mc->mp_enrSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		state_machine_set_local_devinfo(g_mc->mp_enrSM->m_sm,
			devcfg_getDeviceInfo(g_mc->mp_info),
			&dum_lock, p_dhKeyPair);

		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: Exit\n"));
		break;
	}

	case EModeRegistrar:
	{
		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: EModeRegistrar enter\n"));
		/* Instantiate the Registrar SM */
		g_mc->mp_regSM = reg_sm_new(g_mc, g_mc->mp_regProt);
		if (!g_mc->mp_regSM) {
			TUTRACE((TUTRACE_ERR,
				"MC_SwitchModeOn: Could not allocate g_mc->mp_regSM\n"));
			return WPS_ERR_OUTOFMEMORY;
		}

		state_machine_set_local_devinfo(g_mc->mp_regSM->m_sm,
			devcfg_getDeviceInfo(g_mc->mp_info),
			&dum_lock, p_dhKeyPair);

		/* Create the encrypted settings in M8 */
		if (g_mc->mp_info->mb_nwKeySet) {
			/* Create the encrypted settings TLV for enrollee is AP */
			if (WPS_SUCCESS != wps_createM8AP(g_mc, NULL))
				return WPS_ERR_OUTOFMEMORY;

			/* Create the encrypted settings TLV for enrollee is STA */
			if (WPS_SUCCESS != wps_createM8Sta(g_mc))
				return WPS_ERR_OUTOFMEMORY;
		}

		/* Initialize the Registrar SM */
		if (WPS_SUCCESS != reg_sm_initsm(g_mc->mp_regSM, NULL, true, false))
			return WPS_ERR_OUTOFMEMORY;

		/* Send the encrypted settings value to the SM */
		if (g_mc->mp_info->mb_nwKeySet)
			state_machine_set_encrypt(g_mc->mp_regSM->m_sm,
				(void *)g_mc->mp_tlvEsM8Sta,
				(void *)g_mc->mp_tlvEsM8Ap);
		else
			state_machine_set_encrypt(g_mc->mp_regSM->m_sm,
				NULL, NULL);

		TUTRACE((TUTRACE_INFO, "MC_SwitchModeOn: Exit\n"));
		break;
	}

	default:
		ret = WPS_ERR_SYSTEM;
		break;
	}

	return ret;
}

/*
 * Name        : SwitchModeOff
 * Description : Switch off the specified mode of operation.
 * Arguments   : IN EMode e_mode - mode of operation to switch off
 * Return type : uint32 - result of the operation
 */
static uint32
wps_switchModeOff(IN EMode e_mode)
{

	if (e_mode == EModeClient) {
		if (g_mc->mp_enrSM) {
			/* TODO: Clear request IEs */
			enr_sm_delete(g_mc->mp_enrSM);
		}
	}
	else if (e_mode == EModeRegistrar) {
		if (g_mc->mp_regSM) {
			/* TODO: Clear request IEs */
			reg_sm_delete(g_mc->mp_regSM);
		}
	}

	if (g_mc->mp_info->mp_dhKeyPair) {
		DH_free(g_mc->mp_info->mp_dhKeyPair);
		g_mc->mp_info->mp_dhKeyPair = 0;

		TUTRACE((TUTRACE_ERR, "Free g_mc->mp_info->mp_dhKeyPair\n"));
	}

	/* Transport module stop now happens in ProcessRegCompleted() */

	return WPS_SUCCESS;
}

/*
 * creates and copy a WPS IE in buff argument.
 * Lenght of the IE is store in buff[0].
 * buff should be a 256 bytes buffer.
 */
uint32
wps_build_probe_req_ie(uint8 *buff, int buflen,  uint8 reqType,
	uint16 assocState, uint16 configError, uint16 passwId)
{
	uint32 ret = WPS_SUCCESS;
	BufferObj *bufObj;
	uint16 data16;
	uint8 data8, *p_data8;
	CTlvPrimDeviceType tlvPrimDeviceType;

	if (!buff)
		return WPS_ERR_SYSTEM;

	/*  fill in the WPA-WPS OUI  */
	buff[0] = 4;
	buff[1] = 0x00;
	buff[2] = 0x50;
	buff[3] = 0xf2;
	buff[4] = 0x04;

	/*  use oa BufferObj for the rest */
	bufObj = buffobj_setbuf(buff+5, buflen-5);
	if (!bufObj)
		return WPS_ERR_SYSTEM;

	/* Create the IE */
	/* Version */
	data8 = devcfg_getVersion(g_mc->mp_info);
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &reqType, WPS_ID_REQ_TYPE_S);

	/* Config Methods */
	data16 = devcfg_getConfigMethods(g_mc->mp_info);
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* UUID */
	p_data8 = devcfg_getUUID(g_mc->mp_info);
	if (WPS_MSGTYPE_REGISTRAR == reqType)
		data16 = WPS_ID_UUID_R;
	else
		data16 = WPS_ID_UUID_E;
	tlv_serialize(data16, bufObj, p_data8, 16); /* change 16 to SIZE_UUID */

	/*
	 * Primary Device Type
	 *This is a complex TLV, so will be handled differently
	 */
	tlvPrimDeviceType.categoryId = devcfg_getPrimDeviceCategory(g_mc->mp_info);
	tlvPrimDeviceType.oui = devcfg_getPrimDeviceOui(g_mc->mp_info);
	tlvPrimDeviceType.subCategoryId = devcfg_getPrimDeviceSubCategory(g_mc->mp_info);
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* RF Bands */
	data8 = devcfg_getRFBand(g_mc->mp_info);
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* Association State */
	tlv_serialize(WPS_ID_ASSOC_STATE, bufObj, &assocState, WPS_ID_ASSOC_STATE_S);

	/* Configuration Error */
	tlv_serialize(WPS_ID_CONFIG_ERROR, bufObj, &configError, WPS_ID_CONFIG_ERROR_S);

	/* Device Password ID */
	data16 = passwId;
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16, WPS_ID_DEVICE_PWD_ID_S);

	/* Portable Device - optional */

	/* Vendor Extension - optional */
	/* store length */
	buff[0] += bufObj->m_dataLength;

	buffobj_del(bufObj);

	return ret;
}

uint32
wps_build_pbc_proberq(uint8 *buff, int len)
{
	return wps_build_probe_req_ie(buff, len, 0, 0, 0, WPS_DEVICEPWDID_PUSH_BTN);
}

uint32
wps_computeChecksum(IN unsigned long int PIN)
{
	unsigned long int accum = 0;
	int digit;

	PIN *= 10;
	accum += 3 * ((PIN / 10000000) % 10);
	accum += 1 * ((PIN / 1000000) % 10);
	accum += 3 * ((PIN / 100000) % 10);
	accum += 1 * ((PIN / 10000) % 10);
	accum += 3 * ((PIN / 1000) % 10);
	accum += 1 * ((PIN / 100) % 10);
	accum += 3 * ((PIN / 10) % 10);

	digit = (accum % 10);
	return (10 - digit) % 10;
}

uint32
wps_generatePin(char *c_devPwd, int buf_len, IN bool b_display)
{
	BufferObj *bo_devPwd;
	uint8 *devPwd = NULL;
	uint32 val = 0;
	uint32 checksum = 0;
	char local_pwd[20];

	/*
	 * buffer size needs to big enough to hold 8 digits plus the string terminition 
	 * character '\0'
	*/
	if (buf_len < 9)
		return WPS_ERR_BUFFER_TOO_SMALL;

	bo_devPwd = buffobj_new();
	if (!bo_devPwd)
		return WPS_ERR_SYSTEM;

	/* Use the GeneratePSK() method in RegProtocol to help with this */
	buffobj_Reset(bo_devPwd);
	if (WPS_SUCCESS != reg_proto_generate_psk(8, bo_devPwd)) {
		TUTRACE((TUTRACE_ERR,
			"MC_GenerateDevPwd: Could not generate enrDevPwd\n"));
		buffobj_del(bo_devPwd);
		return WPS_ERR_SYSTEM;
	}
	devPwd = bo_devPwd->pBase;
	sprintf(local_pwd, "%08u", *(uint32 *)devPwd);

	/* Compute the checksum */
	local_pwd[7] = '\0';
	val = strtoul(local_pwd, NULL, 10);
	checksum = wps_computeChecksum(val);
	val = val*10 + checksum;
	sprintf(local_pwd, "%d", val);
	local_pwd[8] = '\0';
	TUTRACE((TUTRACE_ERR, "MC::GenerateDevPwd: wps_device_pin = %s\n", local_pwd));
	buffobj_del(bo_devPwd);

	if (b_display)
		/* Display this pwd */
		TUTRACE((TUTRACE_ERR, "DEVICE PIN: %s\n", local_pwd));

	/* Output result */
	strncpy(c_devPwd, local_pwd, 8);
	c_devPwd[8] = '\0';

	return WPS_SUCCESS;
}

bool
wps_isSRPBC(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);
	tlv_set(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, (void *)0, 0);

	if (!tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0) &&
	    probrspIE.selRegistrar.m_data == TRUE)
		tlv_find_dserialize(&probrspIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.pwdId.m_data == 0x04)
		return TRUE;
	else
		return FALSE;
}

/* 
 * Return the state of WPS IE attribute "Select Registrar" indicating if the user has
 * recently activated a registrar to add an Enrollee.
*/
bool
wps_isSELR(IN uint8 *p_data, IN uint32 len)
{
	WpsBeaconIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_set(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, (void *)0, 0);

	tlv_find_dserialize(&probrspIE.selRegistrar, WPS_ID_SEL_REGISTRAR, bufObj, 0, 0);

	buffobj_del(bufObj);

	if (probrspIE.selRegistrar.m_data == TRUE)
		return TRUE;
	else
		return FALSE;
}

bool
wps_isWPSS(IN uint8 *p_data, IN uint32 len)
{
	WpsProbrspIE probrspIE;
	/* De-serialize this IE to get to the data */
	BufferObj *bufObj = buffobj_new();

	if (!bufObj)
		return FALSE;

	buffobj_dserial(bufObj, p_data, len);

	tlv_dserialize(&probrspIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	tlv_dserialize(&probrspIE.scState, WPS_ID_SC_STATE, bufObj, 0, 0);

	buffobj_del(bufObj);
	if (probrspIE.scState.m_data == WPS_SCSTATE_CONFIGURED)
		return TRUE; /* configured */
	else
		return FALSE; /* unconfigured */
}

bool is_wps_ies(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie)
		return TRUE;
	else
		return FALSE;
}

bool is_SelectReg_PBC(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;

	if (wpaie) {
		/* parse_len only contain WPS total attributes length */
		if (wpaie[1] < parse_len)
			parse_len = wpaie[1] - 4;
		else
			parse_len -= 6; /* abnormal IE */

		return wps_isSRPBC(&wpaie[6], parse_len);
	}

	return FALSE;
}

bool is_ConfiguredState(uint8* cp, uint len)
{
	uint8 *parse = cp;
	uint parse_len = len;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;
	if (wpaie) {
		/* parse_len only contain WPS total attributes length */
		if (wpaie[1] < parse_len)
			parse_len = wpaie[1] - 4;
		else
			parse_len -= 6; /* abnormal IE */

		return wps_isWPSS(&wpaie[6], parse_len);
	}

	return FALSE;
}

/* Is this body of this tlvs entry a WPA entry? If */
/* not update the tlvs buffer pointer/length */
bool wl_is_wps_ie(uint8 **wpaie, uint8 **tlvs, uint *tlvs_len)
{
	uint8 *ie = *wpaie;

	/* If the contents match the WPA_OUI and type=1 */
	if ((ie[1] >= 6) && !memcmp(&ie[2], WPA_OUI "\x04", 4)) {
		return TRUE;
	}

	/* point to the next ie */
	ie += ie[1] + 2;
	/* calculate the length of the rest of the buffer */
	*tlvs_len -= (int)(ie - *tlvs);
	/* update the pointer to the start of the buffer */
	*tlvs = ie;

	return FALSE;
}

/*
 * Traverse a string of 1-byte tag/1-byte length/variable-length value
 * triples, returning a pointer to the substring whose first element
 * matches tag
 */
uint8 *
wps_parse_tlvs(uint8 *tlv_buf, int buflen, uint key)
{
	uint8 *cp;
	int totlen;

	cp = tlv_buf;
	totlen = buflen;

	/* find tagged parameter */
	while (totlen >= 2) {
		uint tag;
		int len;

		tag = *cp;
		len = *(cp +1);

		/* validate remaining totlen */
		if ((tag == key) && (totlen >= (len + 2)))
			return (cp);

		cp += (len + 2);
		totlen -= (len + 2);
	}

	return NULL;
}

/*
 * filter wps enabled AP list. list_in and list_out can be the same.
 * return the number of WPS APs.
 */
int
wps_get_aplist(wps_ap_list_info_t *list_in, wps_ap_list_info_t *list_out)
{
	wps_ap_list_info_t *ap_in = &list_in[0];
	wps_ap_list_info_t *ap_out = &list_out[0];
	int i = 0, wps_apcount = 0;

	while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
		if (true == is_wps_ies(ap_in->ie_buf, ap_in->ie_buflen)) {
			if (true == is_ConfiguredState(ap_in->ie_buf, ap_in->ie_buflen))
				ap_in->scstate = WPS_SCSTATE_CONFIGURED;
			memcpy(ap_out, ap_in, sizeof(wps_ap_list_info_t));
			wps_apcount++;
			ap_out = &list_out[wps_apcount];
		}
		i++;
		ap_in = &list_in[i];
	}
	/* in case of on-the-spot filtering, make sure we stop the list  */
	if (wps_apcount < WPS_MAX_AP_SCAN_LIST_LEN)
		ap_out->used = 0;

	return wps_apcount;
}

int
wps_get_pbc_ap(wps_ap_list_info_t *list_in, char *bssid, char *ssid, uint8 * wep,
               unsigned long time, char start)
{
		wps_ap_list_info_t *ap_in = &list_in[0];
		int pbc_found = 0;
		int i = 0;
		int band_2G = 0, band_5G = 0;
		static unsigned long start_time;

		if (start)
			start_time = time;
		if (time  > start_time + PBC_WALK_TIME)
			return PBC_TIMEOUT;
		while (ap_in->used == TRUE && i < WPS_MAX_AP_SCAN_LIST_LEN) {
				if (true == is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)) {
				if (ap_in->band == WL_CHANSPEC_BAND_5G)
					band_5G++;
				else if (ap_in->band == WL_CHANSPEC_BAND_2G)
					band_2G++;
				TUTRACE((TUTRACE_INFO, "2.4G = %d, 5G = %d, band = 0x%x\n",
					band_2G, band_5G, ap_in->band));
				if (pbc_found) {
					if ((band_5G > 1) || (band_2G > 1))
						return PBC_OVERLAP;
				}
				pbc_found++;
				memcpy(bssid, ap_in->BSSID, 6);
				strcpy(ssid, (char *)ap_in->ssid);
				*wep = ap_in->wep;
			}
			i++;
			ap_in = &list_in[i];
		}
		if (!pbc_found)
			return PBC_NOT_FOUND;
		return PBC_FOUND_OK;
}

/*
 * Return whether a registrar is activated or not and the information is carried in WPS IE
 */
bool
wps_get_select_reg(const wps_ap_list_info_t *ap_in)
{
	uint8 *parse = ap_in->ie_buf;
	uint parse_len = ap_in->ie_buflen;
	uint8 *wpaie;

	while ((wpaie = wps_parse_tlvs(parse, parse_len, DOT11_MNG_WPA_ID)))
		if (wl_is_wps_ie(&wpaie, &parse, &parse_len))
			break;

	if (wpaie) {
		/* parse_len only contain WPS total attributes length */
		if (wpaie[1] < parse_len)
			parse_len = wpaie[1] - 4;
		else
			parse_len -= 6; /* abnormal IE */

		return wps_isSELR(&wpaie[6], parse_len);
	}

	return FALSE;
}

uint32
wps_update_RFBand(uint8 rfBand)
{
	return devcfg_setRFBand(g_mc->mp_info, rfBand);
}
