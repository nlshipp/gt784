/*
 * WPS AP UPnP state machin
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_upnp_sm.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__) || defined(__ECOS)
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef __ECOS
#include <net/if.h>
#include <sys/param.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#endif /* __linux__ || __ECOS */

#ifdef WIN32
#include <windows.h>
#endif /* WIN32 */

#include <tutrace.h>
#include <wpscommon.h>
#include <wpsheaders.h>

#include <slist.h>

#include <wps_apapi.h>
#include <sminfo.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <ap_upnp_sm.h>
#include <ap_eap_sm.h>

#ifdef __linux__
#define SLEEP(X) sleep(X)
#endif /* __linux__ */

#if defined(WIN32) || defined(WINCE)
#define SLEEP(X) Sleep(X * 1000)
#endif /* WIN32 */

#if defined(__ECOS)
#define SLEEP(X) WpsSleep(X)
#endif 

static void ap_upnp_sm_init_notify_wlan_event(void);
static UPNP_WPS_AP_STATE s_apUpnpState;
static UPNP_WPS_AP_STATE *apUpnpState = NULL;

static char upnpssr_ipaddr[sizeof("255.255.255.255")] = {0};


static void
ap_upnp_sm_init_notify_wlan_event(void)
{
	unsigned char *macaddr = wps_get_mac(apUpnpState->mc_dev);

	if (apUpnpState->init_wlan_event)
		(*apUpnpState->init_wlan_event)(apUpnpState->if_instance, (char *)macaddr, 1);

	return;
}

uint32
ap_upnp_sm_init(void *mc_dev, int pmr_trigger, void * init_wlan_event, void *update_wlan_event,
	void *send_data, void *parse_msg, int instance)
{
	unsigned char *mac;

	/* Save interface name */
	mac = wps_get_mac_income(mc_dev);
	if  (!mac)
		return WPS_ERR_SYSTEM;

	memset(&s_apUpnpState, 0, sizeof(UPNP_WPS_AP_STATE));
	apUpnpState = &s_apUpnpState;

	apUpnpState->if_instance = instance;

	apUpnpState->mc_dev = mc_dev;

	apUpnpState->m_waitForGetDevInfoResp = false;
	apUpnpState->m_waitForPutMessageResp = false;
	if (init_wlan_event)
		apUpnpState->init_wlan_event =
			(void (*)(int, char *, int))init_wlan_event;

	if (update_wlan_event)
		apUpnpState->update_wlan_event =
			(void (*)(int, unsigned char *, char *, int, int))update_wlan_event;

	if (send_data)
		apUpnpState->send_data = (uint32 (*)(int, char *, uint32, int))send_data;

	if (parse_msg)
		apUpnpState->parse_msg = (char* (*)(char *, int, int *, int *, char *))parse_msg;

	/* Notify WLAN Evetn */
	ap_upnp_sm_init_notify_wlan_event();

	return WPS_SUCCESS;
}

uint32
ap_upnp_sm_deinit()
{
	if (!apUpnpState) {
		TUTRACE((TUTRACE_ERR, "Not initialized; Returning\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	return WPS_SUCCESS;
}

uint32
ap_upnp_sm_sendMsg(char *dataBuffer, uint32 dataLen)
{
	unsigned char *macaddr = wps_get_mac(apUpnpState->mc_dev);

	TUTRACE((TUTRACE_INFO, "In %s buffer Length = %d\n", __FUNCTION__, dataLen));

	if ((!dataBuffer) || (! dataLen)) {
		TUTRACE((TUTRACE_ERR, "Invalid Parameters\n"));
		return WPS_ERR_INVALID_PARAMETERS;
	}

	if (apUpnpState->m_waitForGetDevInfoResp) {
		/* Send message back to UPnP thread with GDIR type. */
		if (apUpnpState->send_data)
			(*apUpnpState->send_data)(apUpnpState->if_instance, dataBuffer, dataLen,
				UPNP_WPS_TYPE_GDIR);

		apUpnpState->m_waitForGetDevInfoResp = false;
	}
	else if (apUpnpState->m_waitForPutMessageResp) {
		/* Send message back to UPnP thread. */
		if (apUpnpState->send_data)
			(*apUpnpState->send_data)(apUpnpState->if_instance, dataBuffer, dataLen,
				UPNP_WPS_TYPE_PMR);

		apUpnpState->m_waitForPutMessageResp = false;
	}
	else {
		/* Send message back to UPnP thread. */
		if (apUpnpState->update_wlan_event)
			(*apUpnpState->update_wlan_event)(apUpnpState->if_instance, macaddr,
				dataBuffer, dataLen, 0);
	}

	return WPS_SUCCESS;
}

/* process message from WFADevice */
uint32
ap_upnp_sm_process_msg(char *msg, int msg_len)
{
	char *data = NULL;
	int len, type, ret = WPS_CONT;
	char ssr_client[sizeof("255.255.255.255")] = {0};

	/* Check initialization */
	if (apUpnpState == 0)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (apUpnpState->parse_msg)
		data = (*apUpnpState->parse_msg)(msg, msg_len, &len, &type, ssr_client);
	if (!data) {
		TUTRACE((TUTRACE_ERR,  "Missing data in message\n"));
		return WPS_ERR_GENERIC;
	}

	switch (type) {
		case UPNP_WPS_TYPE_SSR:
			TUTRACE((TUTRACE_INFO, "Set Selected Registrar received; "
				"InvokeCallback\n"));
			break;

		case UPNP_WPS_TYPE_PMR:
			TUTRACE((TUTRACE_INFO, "Wait For Put Message Resp received; "
				"InvokeCallback\n"));
			apUpnpState->m_waitForPutMessageResp = true;
			break;

		case UPNP_WPS_TYPE_GDIR:
			TUTRACE((TUTRACE_INFO, "Wait For Get DevInfo Resp received; "
				"InvokeCallback\n"));

			/* Update WPS state */
			wps_setUpnpDevGetDeviceInfo(apUpnpState->mc_dev, true);

			apUpnpState->m_waitForGetDevInfoResp = true;
			break;

		case UPNP_WPS_TYPE_PWR:
			TUTRACE((TUTRACE_INFO, "Put WLAN Response received; InvokeCallback\n"));
			break;

		default:
			TUTRACE((TUTRACE_INFO, "Type Unknown; continuing..\n"));
			return ret;
	}

	/* call callback */
	if (type == UPNP_WPS_TYPE_SSR) {
		ap_upnp_sm_set_ssr_ipaddr(ssr_client);
		ret = wps_upnpDevSSR(apUpnpState->mc_dev, (void *)data, len, CB_TRUPNP_DEV_SSR);
	}
	else {
		/* reset msg_len */
		apUpnpState->msg_len = sizeof(apUpnpState->msg_to_send);
		ret = wps_processMsg(apUpnpState->mc_dev, (void *)data, len,
			apUpnpState->msg_to_send, (uint32 *)&apUpnpState->msg_len,
			TRANSPORT_TYPE_UPNP_DEV);
	}

	return ret;
}

int
ap_upnp_sm_get_msg_to_send(char **data)
{
	if (!apUpnpState) {
		*data = NULL;
		return -1;
	}

	*data = apUpnpState->msg_to_send;
	return apUpnpState->msg_len;
}

void
ap_upnp_sm_set_ssr_ipaddr(char *ipaddr)
{
	if (ipaddr)
		strncpy(upnpssr_ipaddr, ipaddr, sizeof(upnpssr_ipaddr));
}

char *
ap_upnp_sm_get_ssr_ipaddr()
{
	return upnpssr_ipaddr;
}
