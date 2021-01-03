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
#include <epictrl.h>
#include "bn.h"
#include "wpscli_osl_common.h"

#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))
#define IFNAMSIZ        100
 #define atow(strA,strW,lenW) \
MultiByteToWideChar(CP_ACP,0,strA,-1,strW,lenW)

static adapter_info stAdapterInfo;

BOOL wpscli_init_adapter(const char *cszAdapterName, int is_virutal)
{
	BOOL bRet = FALSE;
	HANDLE irh = INVALID_HANDLE_VALUE;
	int i;
	PADAPTER pdev = NULL;
	DWORD status;
	DWORD ndevs;
	ADAPTER devlist[10];
	TCHAR    if_name[IFNAMSIZ];

	TUTRACE((TUTRACE_INFO, "wpscli_init_adapter: Entered.\n"));
	
	// Why need to call this???
	CeSetThreadPriority(GetCurrentThread(), 0xfe);

	if ((status = ir_init(&irh)) != ERROR_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Exiting. ir_init is failed\n"));
		return FALSE;
	}

	if ((status = ir_adapter_list(irh, &devlist[0], &ndevs)) != ERROR_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Failed to get adapter list. ir_adapter_list returns %d\n", status));
		goto DONE;
	}

	// Get adapter
	ndevs = ARRAYSIZE(devlist);
	for (i = 0; i < ndevs && !bRet; i++) {
		pdev = &devlist[i];
		if (pdev->type == IR_WIRELESS) {
			// Wireless adapter is found
			if(cszAdapterName == NULL) {
				// If no adapter name is passed in, we just pick the first IR_WIRELESS type of adapter
				bRet = TRUE;
			}
			else {
				// Adapter GUID is passed in, get the adapter based on GUID
				if(strcmp(pdev->adaptername, cszAdapterName) == 0)
				{
					bRet = TRUE;
				}
				else
				{
					atow(cszAdapterName, if_name, IFNAMSIZ-1);
					if (_tcscmp(if_name, pdev->name) == 0) {
						bRet = TRUE;
					}
				}
			}
		}
	}

	// Bind the selected adapter and fill stAdapterInfo with adapter information
	if(bRet && pdev) {
		if ((status = ir_bind(irh, pdev->name)) == ERROR_SUCCESS) {
			// Succeeded to initialize adapter and copy over adapter information
			stAdapterInfo.irh = irh;
			memcpy(stAdapterInfo.macaddress, pdev->macaddr, 6);
			_tcscpy(stAdapterInfo.adaptername, pdev->name);  // No need to attach "\\Device\\NPF_" prefix for WinCE
			_tcscpy(stAdapterInfo.shortadaptername, pdev->name);
			bRet = TRUE;
		}
		else {
			TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Failed to bind adapter. ir_bind returns %d\n", status));
		}

	}
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: No matched adapter is found.\n"));
	}

DONE:
	if(!bRet)
	{
		if (irh != INVALID_HANDLE_VALUE) {
			ir_unbind(irh);
			ir_exit(irh);
		}
	}

	TUTRACE((TUTRACE_ERR, "wpscli_init_adapter: Exiting. bRet=%d\n", bRet));
	return bRet;
}

void closeAdapter(HANDLE irh)
{
	if(irh != NULL && irh != INVALID_HANDLE_VALUE) 
	{
		ir_unbind(irh);
		ir_exit(irh);
        irh = INVALID_HANDLE_VALUE;
		stAdapterInfo.irh = INVALID_HANDLE_VALUE;
	}
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
