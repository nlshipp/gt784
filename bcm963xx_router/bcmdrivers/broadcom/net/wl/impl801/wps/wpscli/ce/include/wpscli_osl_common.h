/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl_common.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Common OSL definitions
 *
 */
#ifndef __WPSCLI_OSL_COMMON__
#define __WPSCLI_OSL_COMMON__

#define WPS_EAP_DATA_MAX_LENGTH         2048
#define WPS_EAP_READ_DATA_TIMEOUT       1

typedef struct td_adapter_info {
	HANDLE irh;
	TCHAR adaptername[80];
	TCHAR shortadaptername[80];
	uint8 macaddress[6];
	TCHAR description[256];
	char ssid[64];
	BOOL isvirtual;
} adapter_info;

//extern adapter_info stAdapterInfo;

extern TCHAR* getShortAdapterName();
extern HANDLE getAdapterHandle();
extern uint8* getMacAddress();
extern TCHAR* getAdapterName();
extern TCHAR* getAdapterDescription();
extern BOOL getIsVirtualAdapter();
extern char* getNetwork();
extern void closeAdapter(HANDLE irh);

#endif  // __WPSCLI_OSL_COMMON__
