/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wlan_api.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement loading and exposing WLAN APIs dynamically
 *
 */
#include "stdafx.h"
#include "wpscli_wlan_api.h"

#define WLANAPI_DLL_FILENAME TEXT("wlanapi.dll")

HANDLE	g_hWlanLib = NULL;

BOOL LoadWlanApi()
{
	TUTRACE((TUTRACE_INFO, "LoadWlanApi: Entered.\n"));

	g_hWlanLib = LoadLibrary(WLANAPI_DLL_FILENAME);

	if(!g_hWlanLib) {
		TUTRACE((TUTRACE_ERR, "LoadWlanApi: Exiting. Failed to load wlan library.\n"));
		return FALSE;
	}

	fnWlanOpenHandle = (WLANOPENHANDLE)GetProcAddress(g_hWlanLib, "WlanOpenHandle");
	fnWlanCloseHandle = (WLANCLOSEHANDLE)GetProcAddress(g_hWlanLib, "WlanCloseHandle");
	fnWlanEnumInterfaces = (WLANENUMINTERFACE)GetProcAddress(g_hWlanLib, "WlanEnumInterfaces");
	fnWlanQueryInterface = (WLANQUERYINTERFACE)GetProcAddress(g_hWlanLib, "WlanQueryInterface");
	fnWlanRegisterNotification = (WLANREGISTERNOTIFICATION)GetProcAddress(g_hWlanLib, "WlanRegisterNotification");
	fnWlanScan = (WLANSCAN)GetProcAddress(g_hWlanLib, "WlanScan");
	fnWlanGetNetworkBssList = (WLANGETNETWORKBSSLIST)GetProcAddress(g_hWlanLib, "WlanGetNetworkBssList");
	fnWlanConnect = (WLANCONNECT)GetProcAddress(g_hWlanLib, "WlanConnect");
	fnWlanSetProfile = (WLANSETPROFILE)GetProcAddress(g_hWlanLib, "WlanSetProfile");
	fnWlanDisconnect = (WLANDISCONNECT)GetProcAddress(g_hWlanLib, "WlanDisconnect");
	fnWlanIhvControl = (WLANIHVCONTROL)GetProcAddress(g_hWlanLib, "WlanIhvControl");
	fnWlanFreeMemory = (WLANFREEMEMORY)GetProcAddress(g_hWlanLib, "WlanFreeMemory");
	fnWlanReasonCodeToString = (WLANREASONCODETOSTRING)GetProcAddress(g_hWlanLib, "WlanReasonCodeToString");

	if(fnWlanOpenHandle &&
	   fnWlanCloseHandle &&
	   fnWlanEnumInterfaces &&
	   fnWlanQueryInterface &&
	   fnWlanRegisterNotification &&
	   fnWlanScan &&
	   fnWlanGetNetworkBssList &&
	   fnWlanConnect &&
	   fnWlanSetProfile &&
	   fnWlanDisconnect &&
	   fnWlanIhvControl &&
	   fnWlanFreeMemory &&
	   fnWlanReasonCodeToString)
	{
		TUTRACE((TUTRACE_INFO, "LoadWlanApi: Exiting. Succeeded to load wlan library.\n"));
		return TRUE;
	}

	UnloadWlanApi();

	TUTRACE((TUTRACE_ERR, "LoadWlanApi: Exiting. Failed to load wlan library. Not all required functions are supported.\n"));
	return FALSE;
}

void UnloadWlanApi()
{
	TUTRACE((TUTRACE_INFO, "UnloadWlanApi: Entered.\n"));

	FreeLibrary(g_hWlanLib);
	g_hWlanLib = NULL;

	TUTRACE((TUTRACE_INFO, "UnloadWlanApi: Exiting.\n"));
}

char *ValidIfName(const char *ifname)
{
	static char ValidACMName[256];
	unsigned int i;

	strcpy(ValidACMName,ifname);

	if(ValidACMName[0]=='{')
	{
		for(i=0;i<strlen(ValidACMName)-1;i++)
		{
			ValidACMName[i]=ValidACMName[i+1];
		}
		ValidACMName[strlen(ValidACMName)-2]='\0';
	}

	return ValidACMName;
}

// copy SSID to a null-terminated WCHAR string
// count is the number of WCHAR in the buffer.
LPWSTR
SsidToStringW2(LPWSTR buf, ULONG count, PDOT11_SSID pSsid)
{
    ULONG   bytes, i;

    bytes = min( count-1, pSsid->uSSIDLength);
    for( i=0; i<bytes; i++)
        mbtowc( &buf[i], (const char *)&pSsid->ucSSID[i], 1);
    buf[bytes] = '\0';

    return buf;
}

// copy SSID to a null-terminated WCHAR string
// count is the number of WCHAR in the buffer.
void StringToStringW2(LPWSTR buf, const char *pStr)
{
    unsigned int i;

    for( i=0; i<strlen(pStr); i++) mbtowc( &buf[i], (const char *)&(pStr[i]), 1);
    buf[i] = '\0';
}
