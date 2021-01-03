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

extern BOOL wpscli_init_adapter(const char *cszAdapterName);
BOOL QueryOid(ulong oid, void* results, ulong nbytes);
BOOL SetOid(ulong oid, void* data, ulong nbytes);

BOOL g_IsVirtualAdapter = FALSE;
CRITICAL_SECTION ioctl_mutex;
char g_szAdapterName[100]; 

// initialize wlan calls and also register cabllback notfication function
brcm_wpscli_status wpscli_wlh_open(const char *dapter_name, int is_virutal)
{
	TUTRACE((TUTRACE_INFO, "wpscli_wlh_open: Entered.\n"));

	if(!wpscli_init_adapter(dapter_name, is_virutal))
		return WPS_STATUS_OPEN_ADAPTER_FAIL;

	g_IsVirtualAdapter = is_virutal ? TRUE : FALSE;
	strcpy(g_szAdapterName,  dapter_name);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_open: Exiting.\n"));

	InitializeCriticalSection(&ioctl_mutex);
	return WPS_STATUS_SUCCESS;
}

// uninitialize wlan calls
brcm_wpscli_status wpscli_wlh_close()
{
	closeAdapter(getAdapterHandle());
	DeleteCriticalSection(&ioctl_mutex);
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_open_adapter()
{
	if(!wpscli_init_adapter(g_szAdapterName, g_IsVirtualAdapter))
		return WPS_STATUS_OPEN_ADAPTER_FAIL;
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_close_adapter()
{
	closeAdapter(getAdapterHandle());
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_ioctl_set(int cmd, const char *data, ulong data_len)
{
	// On Windows, WL_OID_BASE is required being added to the cmd ioctl definition
	// On Linux, only the cmd ioctl without WL_OID_BASE is needed to pass to ioctl call
	const char *prefix = "bsscfg:";
	int8* p;
	uint prefixlen;
	uint iolen;
	char* modified_data;
	char *name;
	brcm_wpscli_status status = WPS_STATUS_IOCTL_SET_FAIL;
	int bssidx = 1;
	uint i;
	int paramlen = 0;

	__try
	{
		EnterCriticalSection (&ioctl_mutex);
		wpscli_wlh_close_adapter();

		status = wpscli_wlh_open_adapter();

		if (status == WPS_STATUS_SUCCESS) 
		{
			if (g_IsVirtualAdapter && cmd == WLC_SET_VAR) {
				prefixlen = strlen(prefix);	/* length of bsscfg prefix */
				iolen = prefixlen + sizeof(int) + data_len;
				modified_data = (char *) malloc(iolen);
				// Zero memory
				memset(modified_data, 0, iolen);

				p = (int8*)modified_data;

				/* copy prefix, no null */
				memcpy(p, prefix, prefixlen);
				p += prefixlen;

				/* validate the name value */
				name = (char*)data;
				for (i = 0; i < (uint)data_len && *name != '\0'; i++, name++)
					;

				i++; /* include the null in the string length */

				/* copy iovar name including null */
				memcpy(p, data, i);
				p += i;

				/* bss config index as first param */
				memcpy(p, &bssidx, sizeof(int32));
				p += sizeof(int32);

				/* parameter buffer follows */
				paramlen = data_len-i;
				if (paramlen > 0)
					memcpy(p, data + i, paramlen);

				if(SetOid(WL_OID_BASE+cmd, (void*)modified_data, (ulong)iolen))
					status = WPS_STATUS_SUCCESS;
				free(modified_data);
			} else {
				if(SetOid(WL_OID_BASE+cmd, (void*)data, (ulong)data_len))
					status = WPS_STATUS_SUCCESS;
			}
			wpscli_wlh_close_adapter();
		}
	}
	__finally
	{
		// Release ownership of the critical section
		LeaveCriticalSection (&ioctl_mutex);
	}
	return status;
}

brcm_wpscli_status wpscli_wlh_ioctl_get(int cmd, char *data, ulong data_len)
{
	// On Windows, WL_OID_BASE is required being added to the cmd ioctl definition
	// On Linux, only the cmd ioctl without WL_OID_BASE is needed to pass to ioctl call
	const char *prefix = "bsscfg:";
	int8* p;
	uint prefixlen;
	uint iolen;
	char* modified_data;
	char *iovar;
	brcm_wpscli_status status = WPS_STATUS_IOCTL_SET_FAIL;
	int bssidx = 1;
	uint i;
	int paramlen = 0;
	uint namelen;

	__try
	{
		EnterCriticalSection (&ioctl_mutex);
		wpscli_wlh_close_adapter();

		status = wpscli_wlh_open_adapter();

		if (status == WPS_STATUS_SUCCESS) 
		{
			if (g_IsVirtualAdapter && cmd == WLC_GET_VAR) {
				
				/* validate the name value */
				iovar = (char*)data;
				for (i = 0; i < (uint)data_len && *iovar != '\0'; i++, iovar++)
					;

				i++; /* include the null in the string length */
				prefixlen = strlen(prefix);	/* length of bsscfg prefix */
				namelen = i;	/* length of iovar name + null */
				iolen = prefixlen + namelen + sizeof(int);

				modified_data = (char *) malloc(data_len);
				// Zero memory
				memset(modified_data, 0, data_len);

				p = (int8*)modified_data;

				/* copy prefix, no null */
				memcpy(p, prefix, prefixlen);
				p += prefixlen;

				/* copy iovar name including null */
				memcpy(p, data, i);
				p += i;

				/* bss config index as first param */
				memcpy(p, &bssidx, sizeof(int32));
				p += sizeof(int32);
				if(QueryOid(WL_OID_BASE+cmd, (void*)modified_data, (ulong)data_len))
				{
					memcpy(data, modified_data, data_len);
					status =  WPS_STATUS_SUCCESS;
				}
				free(modified_data);
			} else {
				if(QueryOid(WL_OID_BASE+cmd, data, data_len))
					status =  WPS_STATUS_SUCCESS;
			} 
			wpscli_wlh_close_adapter();
		}
	}
	__finally
	{
		// Release ownership of the critical section
		LeaveCriticalSection (&ioctl_mutex);
	}
	return status;
}

brcm_wpscli_status wpscli_wlh_get_adapter_mac(uint8 *adapter_mac)
{
	if(adapter_mac == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	memcpy(adapter_mac, getMacAddress(), 6);

	return WPS_STATUS_SUCCESS;
}

BOOL QueryOid(ulong oid, void* results, ulong nbytes)
{
	if(ir_queryinformation((HANDLE)(void *)getAdapterHandle(), oid, results, &nbytes) == NO_ERROR )
		return TRUE;

	return FALSE;
}

BOOL SetOid(ulong oid, void* data, ulong nbytes)
{
	if(ir_setinformation((HANDLE)(void *)getAdapterHandle(), oid, data, &nbytes) == NO_ERROR )
		return TRUE;
	return FALSE;
}
