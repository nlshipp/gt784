/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wlan_vista.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Handling Vista specific WLAN functionalities
 *
 */
#ifndef _WIN32_WINNT		  // Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0600	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include "stdafx.h"
#include "wpscli_wlan_api.h"
#include <ks.h>
#include <rpc.h>
#include <windot11.h>
#include <Objbase.h>

#define XML_PROFILE_MAX_LEN		8192

#ifndef WLAN_CONNECTION_EAPOL_PASSTHROUGH_BIT
#define WLAN_CONNECTION_EAPOL_PASSTHROUGH_BIT 0x8
#endif

//
// Delare Nativie Wi-Fi API functions pointers
//
WLANOPENHANDLE				fnWlanOpenHandle = NULL;
WLANCLOSEHANDLE				fnWlanCloseHandle = NULL;
WLANQUERYINTERFACE			fnWlanQueryInterface = NULL;
WLANREGISTERNOTIFICATION	fnWlanRegisterNotification = NULL;
WLANSCAN					fnWlanScan = NULL;
WLANENUMINTERFACE			fnWlanEnumInterfaces = NULL;
WLANGETNETWORKBSSLIST		fnWlanGetNetworkBssList = NULL;
WLANSETPROFILE				fnWlanSetProfile = NULL;
WLANCONNECT					fnWlanConnect = NULL;
WLANDISCONNECT				fnWlanDisconnect = NULL;
WLANIHVCONTROL				fnWlanIhvControl = NULL;
WLANFREEMEMORY				fnWlanFreeMemory = NULL;
WLANREASONCODETOSTRING		fnWlanReasonCodeToString = NULL;

static HANDLE g_hAcmClient = NULL;
static GUID g_guidInterface;
static brcm_wpscli_status scan_list_acm_to_wl(wl_scan_results_t *pWlScanList, uint32 buf_size, PWLAN_BSS_LIST const pBssidList);
static brcm_wpscli_status get_network_bss_list(wl_scan_results_t *ap_list, uint32 buf_size);

typedef struct td_acm_request_ctx {
	HANDLE hScanEvent;
	HANDLE hConnectEvent;
	DWORD ntf_code;
} acm_request_ctx;

acm_request_ctx g_ctxAcmRequest;

void CloseAcmHandle()
{
	if(g_hAcmClient)
	{
		fnWlanCloseHandle(g_hAcmClient, NULL);
		g_hAcmClient = NULL;
	}
}

BOOL GetGuid(LPCWSTR wcsInterfaceGuid)
{
	BOOL bRet = FALSE;
	DWORD dwError = ERROR_SUCCESS;
    PWLAN_INTERFACE_INFO_LIST pIntfList = NULL;
    RPC_WSTR strGuid = NULL;
	DWORD dwServiceVersion;
	
	dwError = fnWlanOpenHandle(2, NULL, &dwServiceVersion, &g_hAcmClient);
	if(dwError == ERROR_SUCCESS && g_hAcmClient)
    {
		UINT i;
		if(fnWlanEnumInterfaces(g_hAcmClient, NULL, &pIntfList) == ERROR_SUCCESS)
		{
			for(i = 0; i < pIntfList->dwNumberOfItems && !bRet; i++)
			{
				WCHAR wszGuid[100];
				StringFromGUID2(&(pIntfList->InterfaceInfo[i].InterfaceGuid), (LPOLESTR)wszGuid, 100);
				if(wcscmp(wszGuid, wcsInterfaceGuid) == 0)
				{
					memcpy(&g_guidInterface, &(pIntfList->InterfaceInfo[i].InterfaceGuid), sizeof(GUID));
					bRet = TRUE;
				}
			}

			if (pIntfList != NULL) 
				fnWlanFreeMemory(pIntfList);
		}
	}

	return bRet;
}

// Notification callback function
VOID WINAPI wlan_notif_cb_proc(__in PWLAN_NOTIFICATION_DATA pNotifData, __in_opt PVOID pContext)
{
    WCHAR strSsid[DOT11_SSID_MAX_LENGTH+1];
	acm_request_ctx *ctxAcmRequest = (acm_request_ctx *)pContext;
	DWORD dwReasonCode;
	WCHAR wszReasonMsg[256] = { L'\0' };

	if (pNotifData == NULL || ctxAcmRequest == NULL)
		return;

	// Set notification code
	ctxAcmRequest->ntf_code = pNotifData->NotificationCode;


	switch(pNotifData->NotificationCode)
    {
	case wlan_notification_acm_scan_complete:
	case wlan_notification_acm_scan_fail:
		if(ctxAcmRequest->hScanEvent)
			SetEvent(ctxAcmRequest->hScanEvent);
		break;
	case wlan_notification_acm_connection_attempt_fail:
		{
		// Connection attemp failure
		DWORD dwError;
		char szReasonMsg[256];
		size_t msg_size;
		PWLAN_CONNECTION_NOTIFICATION_DATA pConnNotifData = (PWLAN_CONNECTION_NOTIFICATION_DATA)(pNotifData->pData);

		dwError = fnWlanReasonCodeToString(pConnNotifData->wlanReasonCode, 256, wszReasonMsg, NULL);
		if(dwError == ERROR_SUCCESS)
		{
			// Convert to multi-byte as TUTRACE is ANSI version only
			wcstombs_s(&msg_size, szReasonMsg, 256, wszReasonMsg, 256);
			TUTRACE((TUTRACE_INFO, 
					"wlan_notif_cb_proc: WlanConnect received wlan_notification_acm_connection_attempt_fail notification. wlanReasonCode=%d %s\n", 
					pConnNotifData->wlanReasonCode, 
					szReasonMsg));
/*
			if(pConnNotifData->wlanReasonCode == WLAN_REASON_CODE_ASSOCIATION_FAILURE)
				// ignore WLAN_REASON_CODE_ASSOCIATION_FAILURE. For some reason, even for a successful connection, 
				// we will receive this event first before receiving wlan_notification_acm_connection_complete when
				// connecting to a Virtual SoftAP. There could be a bug in SoftAP driver.
				;  
			else
				// Notify connection attemp is failed
				if(ctxAcmRequest->hConnectEvent)
					SetEvent(ctxAcmRequest->hConnectEvent);
*/
		}
		break;
		}
	case wlan_notification_acm_connection_complete:
		{
		// Connection attemp failure
		char szReasonMsg[256];
		size_t msg_size;
		PWLAN_CONNECTION_NOTIFICATION_DATA pConnNotifData = (PWLAN_CONNECTION_NOTIFICATION_DATA)(pNotifData->pData);

		fnWlanReasonCodeToString(pConnNotifData->wlanReasonCode, 256, wszReasonMsg, NULL);
		
		wcstombs_s(&msg_size, szReasonMsg, 256, wszReasonMsg, 256);
		TUTRACE((TUTRACE_INFO, 
				"wlan_notif_cb_proc: WlanConnect received wlan_notification_acm_connection_complete notification. wlanReasonCode=%d %s\n", 
				pConnNotifData->wlanReasonCode, 
				szReasonMsg));

		// Connection succeeded
		if(pConnNotifData->wlanReasonCode == 0) 
		{
			if(ctxAcmRequest->hConnectEvent)
				SetEvent(ctxAcmRequest->hConnectEvent);

			TUTRACE((TUTRACE_INFO, "wlan_notif_cb_proc: WlanConnect is completed successfully.\n"));
		}

		break;
		}
	default:
		;
    }
}


BOOL register_notification(WLAN_NOTIFICATION_CALLBACK NotificationCallback)
{
    BOOL bRet = FALSE;
	DWORD dwError = ERROR_SUCCESS;
    DWORD dwPrevNotifType = 0;

	// register for ACM and MSM notifications
	dwError = fnWlanRegisterNotification(g_hAcmClient,
						WLAN_NOTIFICATION_SOURCE_ACM,	// WLAN_NOTIFICATION_SOURCE_ACM | WLAN_NOTIFICATION_SOURCE_MSM,
						FALSE,							// do not ignore duplications
						NotificationCallback,
						&g_ctxAcmRequest,				// no callback context is needed
						NULL,							// reserved
						&dwPrevNotifType);
	
	bRet = (dwError == ERROR_SUCCESS);
	return bRet;
}

BOOL unregister_notification()
{
    BOOL bRet = FALSE;
    DWORD dwError = ERROR_SUCCESS;
    DWORD dwPrevNotifType = 0;

	 // unregister notifications
	dwError = fnWlanRegisterNotification(g_hAcmClient,
                    WLAN_NOTIFICATION_SOURCE_NONE,
                    FALSE,          // do not ignore duplications
                    NULL,           // no callback function is needed
                    NULL,           // no callback context is needed
                    NULL,           // reserved
                    &dwPrevNotifType
					);

	bRet = (dwError == ERROR_SUCCESS);
    return bRet;
}

brcm_wpscli_status wpscli_wlan_open_vista(const char *interface_guid)
{
	WCHAR wcsInterfaceGuid[MAX_PATH] = { L'\0' };

	StringToStringW2(wcsInterfaceGuid, interface_guid);

	if(!GetGuid(wcsInterfaceGuid))
		return WPS_STATUS_WLAN_INIT_FAIL;

	g_ctxAcmRequest.hScanEvent = CreateEvent(NULL, TRUE, FALSE, "WPSCLI_WLAN_SCAN");
	g_ctxAcmRequest.hConnectEvent = CreateEvent(NULL, TRUE, FALSE, "WPSCLI_WLAN_CONNECT");

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlan_close_vista()
{
	if(g_ctxAcmRequest.hScanEvent) {
		CloseHandle(g_ctxAcmRequest.hScanEvent);
		g_ctxAcmRequest.hScanEvent = NULL;
	}

	if(g_ctxAcmRequest.hConnectEvent) {
		CloseHandle(g_ctxAcmRequest.hConnectEvent);
		g_ctxAcmRequest.hConnectEvent = NULL;
	}

	CloseAcmHandle();

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlan_scan_vista(wl_scan_results_t *ap_list, uint32 buf_size)
{
    brcm_wpscli_status status = WPS_STATUS_SYSTEM_ERR;
	DWORD dwError;
	DWORD dwWaitResult;

    if(!g_hAcmClient)
		return WPS_STATUS_SYSTEM_ERR;

	register_notification(wlan_notif_cb_proc);

	dwError = fnWlanScan(g_hAcmClient, &g_guidInterface, NULL, NULL, NULL);
	if(dwError == ERROR_SUCCESS) {
		if(g_ctxAcmRequest.hScanEvent) {
			dwWaitResult = WaitForSingleObject(g_ctxAcmRequest.hScanEvent, WLAN_SCAN_TIMEOUT*1000);
			if(dwWaitResult == WAIT_OBJECT_0) {
				if(g_ctxAcmRequest.ntf_code == wlan_notification_acm_scan_complete) 
					status = get_network_bss_list(ap_list, buf_size);  // Scan successful, retrieve the found ap list
				else if(g_ctxAcmRequest.ntf_code == wlan_notification_acm_scan_fail)
					status = WPS_STATUS_WLAN_NO_ANY_AP_FOUND;
				else
					status = WPS_STATUS_SYSTEM_ERR;
			}
		}
	}

	unregister_notification();

	return status;
}

brcm_wpscli_status get_network_bss_list(wl_scan_results_t *ap_list, uint32 buf_size)
{
    brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	DWORD dwError;
    DOT11_SSID dot11Ssid = {0};
    PDOT11_SSID pDot11Ssid = NULL;
    DOT11_BSS_TYPE dot11BssType = dot11_BSS_type_infrastructure;  // Only care about infrastructure network
    BOOL bSecurityEnabled = TRUE;
    PWLAN_BSS_LIST pWlanBssList = NULL;

	// call GetNetworkBssList to get number of APs
	if ((dwError = fnWlanGetNetworkBssList(
						g_hAcmClient,
						&g_guidInterface,
						pDot11Ssid,
						dot11BssType,
						bSecurityEnabled,
						NULL,
						&pWlanBssList
						)) == ERROR_SUCCESS)
	{
		if(ap_list != NULL)
		{
			// If buffer passed in is not NULL, convert and copy over AP list. Otherwise, return number of 
			// APs only
			status = scan_list_acm_to_wl(ap_list, buf_size, pWlanBssList);
		}

		fnWlanFreeMemory(pWlanBssList);
	}
	else
	{
		status = WPS_STATUS_SYSTEM_ERR;
	}

	return status;
}

BOOL set_wlan_profile(LPCWSTR wszProfileXml)
{
	BOOL bRet = FALSE;
	DWORD dwNegotiatedVersion, dwReasonCode;
	
	// Create the WPS pre-association xml profile
	if(fnWlanSetProfile(g_hAcmClient, &g_guidInterface, 0, wszProfileXml, NULL, TRUE, NULL, &dwReasonCode) == ERROR_SUCCESS)
		bRet = TRUE;

	return bRet;
}

// Create xml profile and to wps pre-connection
brcm_wpscli_status wpscli_wlan_connect_vista(const char *ssid, uint wsec)
{
    brcm_wpscli_status status = WPS_STATUS_SYSTEM_ERR;
	DWORD dwError;
	DWORD dwWaitResult;
	DOT11_SSID dot11Ssid;
	WLAN_CONNECTION_PARAMETERS wlanConnPara;
	WCHAR wszSsid[MAX_PATH] = { L'\0' };
	WCHAR wszProfileXml[XML_PROFILE_MAX_LEN] = { L'\0' }; 
	int start_time;
	PDWORD pDwTmp;
	PWLAN_INTERFACE_STATE pIntrefaceState;
	
	// Create the WPS pre-association xml profile
	LPWSTR strProfilePrototype_part1 = L"<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\"><name>Linksys_golan</name><SSIDConfig><SSID><name>"; /*/<hex>4861696D5345534E6574</hex> */
	LPWSTR strProfilePrototype_part2 = L"</name></SSID><nonBroadcast>true</nonBroadcast></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>WEP</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>networkKey</keyType><protected>false</protected><keyMaterial>melco</keyMaterial></sharedKey></security></MSM></WLANProfile>";

	LPWSTR strProfile_fake = L"<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\"><name>Linksys_golan</name><SSIDConfig><SSID><name>Le chien mange</name></SSID><nonBroadcast>true</nonBroadcast></SSIDConfig><connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security><authEncryption><authentication>open</authentication><encryption>WEP</encryption><useOneX>false</useOneX></authEncryption><sharedKey><keyType>networkKey</keyType><protected>false</protected><keyMaterial>12345</keyMaterial></sharedKey></security></MSM></WLANProfile>";

	// Failure is not very possible if the buffer size is big enough unless there is unexpected system error
	if(MultiByteToWideChar(CP_ACP, MB_COMPOSITE, ssid, -1, wszSsid, MAX_PATH) == 0)
		return WPS_STATUS_SYSTEM_ERR;  

	if(wsec & DOT11_CAP_PRIVACY)
	{
		// Bss is in secure mode

		// Create secure xml profile for connection
		wcscpy(wszProfileXml, strProfilePrototype_part1);
		wcscat(wszProfileXml, wszSsid);
		wcscat(wszProfileXml, strProfilePrototype_part2);

		// Set wlan connection parameters
		wlanConnPara.dot11BssType = dot11_BSS_type_infrastructure;
		wlanConnPara.dwFlags = WLAN_CONNECTION_HIDDEN_NETWORK|WLAN_CONNECTION_EAPOL_PASSTHROUGH_BIT;
		wlanConnPara.pDot11Ssid = NULL;
		wlanConnPara.strProfile = wszProfileXml;
		wlanConnPara.wlanConnectionMode = wlan_connection_mode_temporary_profile;
		wlanConnPara.pDesiredBssidList = NULL;
	}
	else
	{
		// Bss is in unsecure/open mode

		// Fill DOT11_SSID structure
		dot11Ssid.uSSIDLength=strlen(ssid);
		strcpy(dot11Ssid.ucSSID, ssid);

		// Set wlan connection parameters
		wlanConnPara.wlanConnectionMode = wlan_connection_mode_discovery_unsecure;
		wlanConnPara.dwFlags = NULL;
		wlanConnPara.pDot11Ssid = &dot11Ssid;
		wlanConnPara.strProfile = NULL;
		wlanConnPara.dot11BssType = dot11_BSS_type_infrastructure;
		wlanConnPara.pDesiredBssidList = NULL;
	}

	// Register to receive ACM notification
	register_notification(wlan_notif_cb_proc);

	// Make connection
	dwError = fnWlanConnect(g_hAcmClient, &g_guidInterface, &wlanConnPara, NULL);
	if(dwError == ERROR_SUCCESS) {
		if(g_ctxAcmRequest.hConnectEvent) {
			dwWaitResult = WaitForSingleObject(g_ctxAcmRequest.hConnectEvent, WLAN_CONNECTION_TIMEOUT*1000);
			if(dwWaitResult == WAIT_OBJECT_0) {
				if(g_ctxAcmRequest.ntf_code == wlan_notification_acm_connection_complete)
					status = WPS_STATUS_SUCCESS;
				else if(g_ctxAcmRequest.ntf_code == wlan_notification_acm_connection_attempt_fail)
					status = WPS_STATUS_WLAN_CONNECTION_ATTEMPT_FAIL;
				else
					status = WPS_STATUS_SYSTEM_ERR;
			}
		}
	}
	else
	{
		status = WPS_STATUS_SYSTEM_ERR;
	}


	// Unregister ACM notification
	unregister_notification();

	return status;
}

brcm_wpscli_status wpscli_wlan_disconnect_vista()
{
	BOOL bRet = FALSE;
	DWORD dwError;

	dwError = fnWlanDisconnect(g_hAcmClient, &g_guidInterface, NULL);  // Disconnect
	bRet = (dwError == ERROR_SUCCESS);

	return WPS_STATUS_SUCCESS;
}

static brcm_wpscli_status scan_list_acm_to_wl(wl_scan_results_t *pWlScanList, uint32 buf_size, PWLAN_BSS_LIST const pBssidList)
{
	ULONG i, wl_list_length, ielength;
	wl_bss_info_t *pWlBssInfo;
    PBYTE pIe = NULL;

	if(pBssidList == NULL || pWlScanList == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	/* calculate how much memory we need to allocate for the IEs in the WL equivalent structure */
	ielength = 0;
	for (i=0; i < pBssidList->dwNumberOfItems; i++)
	{
		/* DONT strip off the NDIS fixed IEs */
		ielength += pBssidList->wlanBssEntries[i].ulIeSize;  // - sizeof(NDIS_802_11_FIXED_IEs);
	}

	wl_list_length = WL_SCAN_RESULTS_FIXED_SIZE + (sizeof(wl_bss_info_t)*pBssidList->dwNumberOfItems) + ielength;
	pWlScanList->buflen = wl_list_length;
	pWlScanList->version = WL_BSS_INFO_VERSION;
	pWlScanList->count = pBssidList->dwNumberOfItems;

	/* begin copying the bssinfo elements */
	pWlBssInfo = (wl_bss_info_t *)&pWlScanList->bss_info[0];
	for (i=0; i < pBssidList->dwNumberOfItems; i++)
	{
		ielength = /*pBssidList->wlanBssEntries[i].ulIeSize > sizeof(NDIS_802_11_FIXED_IEs) ?*/
			pBssidList->wlanBssEntries[i].ulIeSize/* - sizeof(NDIS_802_11_FIXED_IEs) : 0*/;

		memset(pWlBssInfo, 0, sizeof(wl_bss_info_t) + ielength);

		pWlBssInfo->version = WL_BSS_INFO_VERSION;
		pWlBssInfo->length = sizeof(wl_bss_info_t) + ielength;
		memcpy(&pWlBssInfo->BSSID, pBssidList->wlanBssEntries[i].dot11Bssid, sizeof(pBssidList->wlanBssEntries[i].dot11Bssid));
		pWlBssInfo->beacon_period = (uint16)pBssidList->wlanBssEntries[i].usBeaconPeriod;
		if (pBssidList->wlanBssEntries[i].usCapabilityInformation & DOT11_CAPABILITY_INFO_PRIVACY)
			pWlBssInfo->capability |= DOT11_CAP_PRIVACY; 
		switch (pBssidList->wlanBssEntries[i].dot11BssType)
		{
		case dot11_BSS_type_independent:
			pWlBssInfo->capability |= DOT11_CAP_IBSS;
			break;
		case dot11_BSS_type_infrastructure:
			pWlBssInfo->capability |= DOT11_CAP_ESS;
			break;
		}
		pWlBssInfo->SSID_len = (uint8)pBssidList->wlanBssEntries[i].dot11Ssid.uSSIDLength;
		memcpy(pWlBssInfo->SSID, pBssidList->wlanBssEntries[i].dot11Ssid.ucSSID, pWlBssInfo->SSID_len);
		
		pWlBssInfo->rateset.count = 0;
		
		// Set channel number
		pWlBssInfo->ctl_ch = freq2channel(pBssidList->wlanBssEntries[i].ulChCenterFrequency/1000);

		//pWlBssInfo->atim_window = (uint16)pBssid->Configuration.ATIMWindow;

		pWlBssInfo->dtim_period = 0;
		pWlBssInfo->RSSI = (int16)pBssidList->wlanBssEntries[i].lRssi;
		pWlBssInfo->phy_noise = 0;

		/* copy only the variable length IEs */
		pWlBssInfo->ie_length = ielength;
		pWlBssInfo->ie_offset = sizeof(wl_bss_info_t); //(uint8) (OFFSETOF(wl_bss_info_t, ie_length) + 1);
		pIe = (PBYTE)(&pBssidList->wlanBssEntries[i]) + pBssidList->wlanBssEntries[i].ulIeOffset;
		if (ielength > 0)
			memcpy(((uint8 *)(((uint8 *)pWlBssInfo)+pWlBssInfo->ie_offset)), ((uint8 *)pIe), ielength);

		/* next record */
		pWlBssInfo = (wl_bss_info_t *)(((uint8 *)pWlBssInfo) + pWlBssInfo->length);
	}

	return WPS_STATUS_SUCCESS;
}

int freq2channel(DWORD freq)
{
	int ch;

	if (freq <= 2484) {
		/* 11b */
		if (freq == 2484)
			ch = 14;
		else
			ch = (freq - 2407) / 5;
	} else {
		/* 11a */
		ch = (freq - 5000) / 5;
		if (ch < 0)
			ch += 256;
	}

	return ch;
}
