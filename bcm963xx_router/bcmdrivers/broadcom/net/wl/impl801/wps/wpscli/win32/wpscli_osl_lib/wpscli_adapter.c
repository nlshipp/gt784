/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_adapter.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement adapter related functionalities
 *
 */
#include "stdafx.h"
#include <ntddndis.h>
#include <epictrl.h>
#include "bn.h"
#include "wpscli_osl_common.h"


extern HANDLE m_hVirtualDev;
extern DWORD Initialize_Virtual();
extern BOOL QueryOid_Virtual(ULONG oid, PCHAR pResults, PULONG nbytes);

static adapter_info stAdapterInfo;

BOOL wpscli_init_adapter_vista(const char *cszAdapterGuid)
{
	BOOL bRet = FALSE;
	HANDLE irh = NULL;
	WINERR err = ERROR_SUCCESS;
	DWORD status= ERROR_SUCCESS;
	int adapter = -1;
	char szGuid[64] = { 0 };

	TUTRACE((TUTRACE_INFO, "wpscli_init_adapter: Entered.\n"));

	if(cszAdapterGuid == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Exiting. Invalid NULL parameter passed in.\n"));
		return FALSE;
	}

	/* initialize irelay and select default adapter */
	irh = INVALID_HANDLE_VALUE;

	if ((err = ir_init(&irh)) == ERROR_SUCCESS) {
		err = ir_bind(irh, cszAdapterGuid);
		if(err == ERROR_SUCCESS) {
			DWORD len, status;
			
			stAdapterInfo.irh = irh;
			
			// Get adapter mac address
			len = 6;
			status = ir_queryinformation(irh, OID_802_3_CURRENT_ADDRESS, (PUCHAR)stAdapterInfo.macaddress, &len);

			// Generate NPF adapter name
			_tcscpy(stAdapterInfo.adaptername, _T("\\Device\\NPF_"));
			_tcscat(stAdapterInfo.adaptername, cszAdapterGuid);

			// Set shortadaptername which is the adapter GUID string
			_tcscpy(stAdapterInfo.shortadaptername, cszAdapterGuid);
			bRet = TRUE;
		}
		else {
			TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Failed to bind adapter. err=%d.\n", err));
		}
	}
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: ir_init is failed.\n"));
		closeAdapter(irh);
	}

	TUTRACE((TUTRACE_INFO, "wpscli_init_adapter: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_init_adapter_virtual(const char *cszAdapterGuid)
{
	BOOL bRet = FALSE;
	DWORD dwStatus;

	dwStatus = Initialize_Virtual();
	if(dwStatus == ERROR_SUCCESS)
	{
		int len, dev_ver;

		// GUID
		_tcscpy(stAdapterInfo.shortadaptername, cszAdapterGuid);

		// NPF adapter name
		_tcscpy(stAdapterInfo.adaptername, _T("\\Device\\NPF_"));
		_tcscat(stAdapterInfo.adaptername, cszAdapterGuid);

		// Query adapter mac address
		len = 6;
		dwStatus = QueryOid_Virtual(OID_802_3_CURRENT_ADDRESS, (PUCHAR)stAdapterInfo.macaddress, &len);

		// Query adapter version
		len = sizeof(dev_ver);
		dwStatus = QueryOid_Virtual(WL_OID_BASE+WLC_GET_VERSION, (PCHAR)&dev_ver, &len);

		bRet = TRUE;

	}
	
	return bRet;
}

BOOL wpscli_init_adapter(const char *cszAdapterName, int is_virutal)
{
	BOOL bRet = FALSE;

	stAdapterInfo.irh = NULL;
	_tcscpy(stAdapterInfo.adaptername, _T(""));
	_tcscpy(stAdapterInfo.description, _T(""));
	_tcscpy(stAdapterInfo.shortadaptername, _T(""));
	_tcscpy(stAdapterInfo.ssid, _T(""));
	memset(stAdapterInfo.macaddress, 0, 6);
	stAdapterInfo.isvirtual = is_virutal;

	if(is_virutal)
		bRet = wpscli_init_adapter_virtual(cszAdapterName);
	else {
		if(wps_GetOSVer() == VER_VISTA)
			bRet = wpscli_init_adapter_vista(cszAdapterName);
	}

	return bRet;
}

void closeAdapter(HANDLE irh)
{
	if(irh != NULL && irh != INVALID_HANDLE_VALUE)
		ir_exit(irh);
}

HANDLE getAdapterHandle()
{
	return stAdapterInfo.irh;
}

uint8* getMacAddress()
{
	return &stAdapterInfo.macaddress[0];
}

TCHAR* getAdapterName()
{
	return stAdapterInfo.adaptername;
}

TCHAR* getShortAdapterName()
{
	return stAdapterInfo.shortadaptername;
}

TCHAR* getAdapterDescription()
{
	return stAdapterInfo.description;
}

char* getNetwork()
{
	return stAdapterInfo.ssid;
}

BOOL getIsVirtualAdapter()
{
	return stAdapterInfo.isvirtual;
}
