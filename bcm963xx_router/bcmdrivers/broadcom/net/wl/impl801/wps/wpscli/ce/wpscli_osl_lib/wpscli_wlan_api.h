/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wlan_api.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Define WLAN APIs for dynamic loading to make suer WPS library can be loaded on XP 
 * without WLAN Hotfix installed
 *
 */
#ifndef __WPSCLI_WLAN_API_H__
#define __WPSCLI_WLAN_API_H__

/* The following has been defined in 'Microsoft SDKs\windows\v6.0a\include\sdkddkver.h */
/* also, NTDDI_VISTA is not defined in v6.0a */
#ifndef NTDDI_VERSION
#define NTDDI_VERSION	NTDDI_VISTA
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <wlanapi.h>


typedef DWORD (WINAPI *WLANOPENHANDLE)(
    __in DWORD dwClientVersion,
    __reserved PVOID pReserved,
    __out PDWORD pdwNegotiatedVersion,
    __out PHANDLE phClientHandle
);

typedef DWORD (WINAPI *WLANCLOSEHANDLE)(
    __in HANDLE hClientHandle,
    __reserved PVOID pReserved
);

typedef DWORD (WINAPI *WLANQUERYINTERFACE)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid, 
    __in WLAN_INTF_OPCODE OpCode,
    __reserved PVOID pReserved,
    __out PDWORD pdwDataSize,
    __deref_out_bcount(*pdwDataSize) PVOID *ppData,
    __out_opt PWLAN_OPCODE_VALUE_TYPE pWlanOpcodeValueType
);

typedef DWORD (WINAPI *WLANREGISTERNOTIFICATION)(
    __in HANDLE hClientHandle,
    __in DWORD dwNotifSource,
    __in BOOL bIgnoreDuplicate,
    __in_opt WLAN_NOTIFICATION_CALLBACK funcCallback,
    __in_opt PVOID pCallbackContext,
    __reserved PVOID pReserved,
    __out_opt PDWORD pdwPrevNotifSource
);

typedef DWORD (WINAPI *WLANSCAN)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid, 
    __in_opt CONST PDOT11_SSID pDot11Ssid,
    __in_opt CONST PWLAN_RAW_DATA pIeData,
    __reserved PVOID pReserved
);

typedef DWORD (WINAPI *WLANENUMINTERFACE)(
    __in HANDLE hClientHandle,
    __reserved PVOID pReserved,
    __deref_out PWLAN_INTERFACE_INFO_LIST *ppInterfaceList
);

typedef DWORD (WINAPI *WLANGETNETWORKBSSLIST)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid, 
    __in_opt CONST PDOT11_SSID pDot11Ssid,
    __in DOT11_BSS_TYPE dot11BssType,
    __in BOOL bSecurityEnabled,
    __reserved PVOID pReserved,
    __deref_out PWLAN_BSS_LIST *ppWlanBssList
);

typedef DWORD (WINAPI *WLANCONNECT)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid, 
    __in CONST PWLAN_CONNECTION_PARAMETERS pConnectionParameters,
    __reserved PVOID pReserved
);

typedef DWORD (WINAPI *WLANDISCONNECT)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid, 
    __reserved PVOID pReserved
);

typedef DWORD (WINAPI *WLANSETPROFILE)(
    __in HANDLE hClientHandle,
    __in CONST GUID *pInterfaceGuid,
    __in DWORD dwFlags,
    __in LPCWSTR strProfileXml,
    __in_opt LPCWSTR strAllUserProfileSecurity,
    __in BOOL bOverwrite,
    __reserved PVOID pReserved,
    __out DWORD *pdwReasonCode
);

typedef DWORD (WINAPI *WLANIHVCONTROL)(
	__in         HANDLE hClientHandle,
	__in         const GUID* pInterfaceGuid,
	__in         WLAN_IHV_CONTROL_TYPE Type,
	__in         DWORD dwInBufferSize,
	__in         PVOID pInBuffer,
	__in         DWORD dwOutBufferSize,
	__inout_opt  PVOID pOutBuffer,
	__out        PDWORD pdwBytesReturned
);

typedef DWORD (WINAPI *WLANREASONCODETOSTRING)(
  __in        DWORD dwReasonCode,
  __in        DWORD dwBufferSize,
  __in        PWCHAR pStringBuffer,
  __reserved  PVOID pReserved
);

typedef VOID (WINAPI *WLANFREEMEMORY)(
    __in PVOID pMemory
);

extern WLANOPENHANDLE			fnWlanOpenHandle;
extern WLANCLOSEHANDLE			fnWlanCloseHandle;
extern WLANQUERYINTERFACE		fnWlanQueryInterface;
extern WLANREGISTERNOTIFICATION	fnWlanRegisterNotification;
extern WLANSCAN					fnWlanScan;
extern WLANENUMINTERFACE		fnWlanEnumInterfaces;
extern WLANGETNETWORKBSSLIST	fnWlanGetNetworkBssList;
extern WLANCONNECT				fnWlanConnect;
extern WLANDISCONNECT			fnWlanDisconnect;
extern WLANIHVCONTROL			fnWlanIhvControl;
extern WLANFREEMEMORY			fnWlanFreeMemory;
extern WLANSETPROFILE			fnWlanSetProfile;
extern WLANREASONCODETOSTRING	fnWlanReasonCodeToString;

BOOL LoadWlanApi();
void UnloadWlanApi();
char *ValidIfName(const char *ifname);
LPWSTR SsidToStringW2(LPWSTR buf, ULONG count, PDOT11_SSID pSsid);
void StringToStringW2(LPWSTR buf, const char *pStr);
int freq2channel(DWORD freq);

#ifdef __cplusplus 
}  // extern "C"
#endif

#endif  // #define __WPSCLI_WLAN_API_H__
