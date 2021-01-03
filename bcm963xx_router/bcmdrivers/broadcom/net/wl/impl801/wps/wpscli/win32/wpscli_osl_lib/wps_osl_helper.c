/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_osl_helper.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Generic OSL functions
 *
 */
#include "stdafx.h"
#include <winsock2.h>

DWORD wps_g_osver;  // contain OS version information

extern void RAND_windows_init();

DWORD wps_GetOSVer()
{
    OSVERSIONINFO osvi;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));

	osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
    if(GetVersionEx(&osvi)) 
	{
		if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) 
		{
			wps_g_osver = VER_WIN9X;
		}
		else 
		{
			if (osvi.dwMajorVersion == 6) {
				wps_g_osver = VER_VISTA;
			} else if (osvi.dwMajorVersion == 5) {
				wps_g_osver = VER_WIN2K;
			} else {
				wps_g_osver = VER_WINNT;
			}
		}
    }
	else
	{
		printf("GetVersionEx() failed with error %u.\n", GetLastError());
	}

	return wps_g_osver;
}

void wpscli_rand_init()
{
	RAND_windows_init();
}

// Host to network order for short integer
uint16 wpscli_htons(uint16 v)
{
	return htons(v);
}

// Host to network order for long integer
uint32 wpscli_htonl(uint32 v)
{
	return htonl(v);
}

// Network to host order for short integer
uint16 wpscli_ntohs(uint16 v)
{
	return ntohs(v);
}

// Network to host order for long integer
uint32 wpscli_ntohl(uint32 v)
{
	return ntohl(v);
}

#ifdef _TUDEBUGTRACE
void wpscli_print_buf(char *text, unsigned char *buff, int buflen)
{
	int i;
	printf("\n%s : %d\n", text, buflen);
	for (i = 0; i < buflen; i++) {
		printf("%02X ", buff[i]);
		if (!((i+1)%16))
			printf("\n");
	}
	printf("\n");
}
#endif
