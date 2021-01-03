/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl_helper.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: RTOS OSL helper functions
 *
 */

#include <string.h>
#include <stdarg.h>
#include "slist.h"
#include "wpscli_osl.h"
#include "tutrace.h"
#include "proto/eapol.h"

void RAND_generic_init(void);
uint32 WpsHtonl(uint32 intlong);
uint16 WpsHtons(uint16 intshort);
uint32 WpsNtohl(uint8 *a);
uint16 WpsNtohs(uint8 *a);
BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype);

void wpscli_rand_init()
{
	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Entered.\n"));

	RAND_generic_init();

	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Exiting.\n"));
}

uint16 wpscli_htons(uint16 v)
{
	return WpsHtons(v);
}

uint32 wpscli_htonl(uint32 v)
{
	return WpsHtonl(v);
}

uint16 wpscli_ntohs(uint16 v)
{
	return WpsNtohs((uint8 *)&v);
}

/* used by ap_eap_sm.c */
uint16 ntohs(uint16 v)
{
	return WpsNtohs((uint8 *)&v);
}

uint32 wpscli_ntohl(uint32 v)
{
	return WpsNtohl((uint8 *)&v);
}

void WpsDebugOutput(char *fmt, ...)
{
	char strbuf[1024];
	va_list arg;

	va_start(arg, fmt);
	vsnprintf(strbuf, 1024, fmt, arg);
	va_end(arg);

	printf(strbuf);
	printf("\n");
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

/*
 * functions below from wpscli_wpscore_hooks.c
 */
 
//
// WPS AP side common code hookup functions
//
// Take the raw eapol packet containing the dest and source mac
// address already and send it out. 
uint32 wpsap_osl_eapol_send_data(char *dataBuffer, uint32 dataLen)
{
	wpscli_pktdisp_send_packet(dataBuffer, dataLen);

	return 0;
}

// wps hookup function to common code on WPS ap side 
// return 0 - Success, -1 - Failure
int wps_set_wps_ie(void *bcmdev, unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRel;

	bRel = wpscli_set_wps_ie(p_data, length, cmdtype);
	if(bRel)
		return 0;  // Success
	
	return -1;  // Failure
}

char* wpsap_osl_eapol_parse_msg(char *msg, int msg_len, int *len)
{
	/* just return msg and msg_len, the msg is a eapol raw date */
	*len = msg_len;
	return msg;
}

void upnp_device_uuid(unsigned char *uuid)
{
	char uuid_local[16] = {0x22, 0x21, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0xa, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    TUTRACE((TUTRACE_INFO, "upnp_device_uuid: Entered.\n"));

	if(uuid)
		memcpy(uuid, uuid_local, 16);

    TUTRACE((TUTRACE_INFO, "upnp_device_uuid: Exiting.\n"));
	return;
}

void WpsSleepMs(uint32 ms)
{
    wpscli_sleep(ms);
}

int wps_deauthenticate(unsigned char *bssid, unsigned char *sta_mac, int reason)
{
	return 0;
}
