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
 * Description: Implement wl handler related OSL APIs  
 *
 */
#include "stdafx.h"
#include "wpscli_osl_common.h"

extern BOOL wpscli_init_adapter(const char *cszAdapterName, int is_virutal);
extern BOOL LoadWlanApi();
extern void UnloadWlanApi();
extern BOOL QueryOid_Virtual(ULONG oid, PCHAR pResults, PULONG nbytes);
extern BOOL SetOid_Virtual(ulong oid, void* data, ulong nbytes);
extern BOOL QueryOid_Vista(LPCTSTR cszAdapterName, ULONG oid, void* results, ULONG nbytes);
extern BOOL SetOid_Vista(LPCTSTR cszAdapterName, ULONG oid, PUCHAR pInData, DWORD inlen);
extern BOOL QueryOid_Xp(HANDLE irh, ULONG oid, void* results, ULONG nbytes);
extern BOOL SetOid_Xp(HANDLE irh, ULONG oid, void* data, ULONG nbytes);

static BOOL QueryOid(ulong oid, void* results, ulong nbytes);
static BOOL SetOid(ulong oid, void* data, ulong nbytes);

// initialize wlan calls and also register cabllback notfication function
brcm_wpscli_status wpscli_wlh_open(const char *dapter_name, int is_virtual)
{

	is_virtual = TRUE;
	TUTRACE((TUTRACE_INFO, "wpscli_wlh_open: Entered (input is_virtual=%d), always set is_virtual to TRUE\n",
			is_virtual));

	if(!LoadWlanApi())
		return WPS_STATUS_WLAN_INIT_FAIL;

	if(!wpscli_init_adapter(dapter_name, is_virtual))
		return WPS_STATUS_OPEN_ADAPTER_FAIL;


	TUTRACE((TUTRACE_INFO, "wpscli_wlh_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

// uninitialize wlan calls
brcm_wpscli_status wpscli_wlh_close()
{
	closeAdapter(getAdapterHandle());

	UnloadWlanApi();

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_ioctl_set(int cmd, const char *data, ulong data_len)
{
	// On Windows, WL_OID_BASE is required being added to the cmd ioctl definition
	// On Linux, only the cmd ioctl without WL_OID_BASE is needed to pass to ioctl call
	if(SetOid(WL_OID_BASE+cmd, (void *)data, data_len))
		return WPS_STATUS_SUCCESS;

	return WPS_STATUS_IOCTL_SET_FAIL;
}

brcm_wpscli_status wpscli_wlh_ioctl_get(int cmd, char *buf, ulong buf_len)
{
	// On Windows, WL_OID_BASE is required being added to the cmd ioctl definition
	// On Linux, only the cmd ioctl without WL_OID_BASE is needed to pass to ioctl call
	if(QueryOid(WL_OID_BASE+cmd, buf, buf_len))
		return WPS_STATUS_SUCCESS;

	return WPS_STATUS_IOCTL_GET_FAIL;
}

brcm_wpscli_status wpscli_wlh_get_adapter_mac(uint8 *adapter_mac)
{
	if(adapter_mac == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	memcpy(adapter_mac, getMacAddress(), 6);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_get_adapter_mac() got %02x:%02x:%02x:%02x:%02x:%02x\n",
		adapter_mac[0], adapter_mac[1], adapter_mac[2],
		adapter_mac[3], adapter_mac[4], adapter_mac[5]));

	return WPS_STATUS_SUCCESS;
}

BOOL QueryOid(ulong oid, void* results, ulong nbytes)
{
	if(getIsVirtualAdapter())
		return QueryOid_Virtual(oid, results, &nbytes);
	
	if(wps_GetOSVer() == VER_VISTA)
		return QueryOid_Vista(getShortAdapterName(), oid, results, nbytes);
	else
		return QueryOid_Xp(getAdapterHandle(), oid, results, nbytes);
}

BOOL SetOid(ulong oid, void* data, ulong nbytes)
{
	if(getIsVirtualAdapter())
		return SetOid_Virtual(oid, data, nbytes);

	if(wps_GetOSVer() == VER_VISTA)
		return SetOid_Vista(getShortAdapterName(), oid, data, nbytes);
	else
		return SetOid_Xp(getAdapterHandle(), oid, data, nbytes);
}
