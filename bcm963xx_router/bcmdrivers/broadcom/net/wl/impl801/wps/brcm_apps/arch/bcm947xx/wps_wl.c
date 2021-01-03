/*
 * Broadcom 802.11 device interface
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_wl.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <typedefs.h>
#include <unistd.h>
#include <string.h>

#include <bcmutils.h>
#include <wlutils.h>
#include <wlioctl.h>

#ifdef __linux__
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/filter.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

#include <wpscommon.h>
#include <wpsheaders.h>
#include <tutrace.h>
#include <wps_wl.h>
#include <shutils.h>
#include <wps.h>

static int
wps_wl_set_key(char *wl_if, unsigned char *sta_mac, uint8 *key, int len, int index, int tx)
{
	wl_wsec_key_t wep;
#ifdef _TUDEBUGTRACE
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
	char ki[] = "index XXXXXXXXXXX";

	memset(&wep, 0, sizeof(wep));
	wep.index = index;
	if (sta_mac)
		memcpy(&wep.ea, sta_mac, ETHER_ADDR_LEN);
	else {
		wep.flags = tx ? WL_PRIMARY_KEY : 0;
		snprintf(ki, sizeof(ki), "index %d", index);
	}

	wep.len = len;
	if (len)
		memcpy(wep.data, key, MIN(len, DOT11_MAX_KEY_SIZE));

	TUTRACE((TUTRACE_INFO, "%s, flags %x, len %d",
		sta_mac ? (char *)ether_etoa(sta_mac, eabuf) : ki,
		wep.flags, wep.len));

	return wl_ioctl(wl_if, WLC_SET_KEY, &wep, sizeof(wep));
}

int
wps_wl_deauthenticate(char *wl_if, unsigned char *sta_mac, int reason)
{
	scb_val_t scb_val;

	/* remove the key if one is installed */
	wps_wl_set_key(wl_if, sta_mac, NULL, 0, 0, 0);

	scb_val.val = (uint32) reason;
	memcpy(&scb_val.ea, sta_mac, ETHER_ADDR_LEN);

	return wl_ioctl(wl_if, WLC_SCB_DEAUTHENTICATE_FOR_REASON,
		&scb_val, sizeof(scb_val));
}

int
wps_wl_del_wps_ie(char *wl_if, unsigned int cmdtype, unsigned char ouitype)
{
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf;
	int iecount, i;

	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;
	wps_oui[3] = ouitype;

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		frametype = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		frametype = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "unknown frame type\n"));
		return -1;
	}

	wl_iovar_get(wl_if, "vndr_ie", getbuf, 2048);
	iebuf = (vndr_ie_buf_t *) getbuf;

	bufaddr = (char*) iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*) bufaddr;
		bcopy(bufaddr, (char*)&pktflag, (int) sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4)) {
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}
		bufaddr = ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len;
	}

	if (!found)
		return -1;

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(iebuf_len);
	if (! ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return -1;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	wl_iovar_set(wl_if, "vndr_ie", ie_setbuf, iebuf_len);

	free(ie_setbuf);

	return 0;
}

/* The p_data and length does not contains the one byte OUI_TPYE */
int
wps_wl_set_wps_ie(char *wl_if, unsigned char *p_data, int length, unsigned int cmdtype,
	unsigned char ouitype)
{
	vndr_ie_setbuf_t *ie_setbuf;
	unsigned int pktflag;
	int buflen, iecount, i;
	int err = 0;

#ifdef _TUDEBUGTRACE
	printf("\niebuf (len=%d): ", length);
	for (i = 0; i < length; i++) {
		printf("%02x ", p_data[i]);
	}
	printf("\n");
#endif

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		pktflag = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		pktflag = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "unknown frame type\n"));
		return -1;
	}

	if (length > VNDR_IE_MAX_LEN) {
		TUTRACE((TUTRACE_ERR, "IE length should be <= %d\n", VNDR_IE_MAX_LEN));
		return -1;
	}

	buflen = sizeof(vndr_ie_setbuf_t) + length;
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);

	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return -1;
	}
	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy(&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* 
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer, +1: one byte OUI_TYPE */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8) length +
		VNDR_IE_MIN_LEN + 1;

	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = ouitype;

	for (i = 0; i < length; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = p_data[i];
	}

	wps_wl_del_wps_ie(wl_if, cmdtype, ouitype);
	err = wl_iovar_set(wl_if, "vndr_ie", ie_setbuf, buflen);
	free(ie_setbuf);

	return (err);
}

/* There is another wps_wet_wsec in wps_linux */
int
wps_wl_open_wps_window(char *ifname)
{
	uint32 wsec;

	wl_ioctl(ifname, WLC_GET_WSEC, &wsec, sizeof(wsec));
	wsec |= SES_OW_ENABLED;
	return (wl_ioctl(ifname, WLC_SET_WSEC, &wsec, sizeof(wsec)));
}

int
wps_wl_close_wps_window(char *ifname)
{
	uint32 wsec;

	wl_ioctl(ifname, WLC_GET_WSEC, &wsec, sizeof(wsec));
	wsec &= ~SES_OW_ENABLED;
	return (wl_ioctl(ifname, WLC_SET_WSEC, &wsec, sizeof(wsec)));
}

int
wps_wl_get_maclist(char *ifname, char *buf, int count)
{
	struct maclist *mac_list;
	int mac_list_size;
	int num;

	mac_list_size = sizeof(mac_list->count) + count * sizeof(struct ether_addr);
	mac_list = malloc(mac_list_size);
	if (mac_list == NULL)
		return -1;

	/* query wl for authenticated sta list */
	strcpy((char*)mac_list, "authe_sta_list");
	if (wl_ioctl(ifname, WLC_GET_VAR, mac_list, mac_list_size)) {
		TUTRACE((TUTRACE_ERR, "GET authe_sta_list error!!!\n"));
		free(mac_list);
		return -1;
	}

	num = mac_list->count;
	memcpy(buf, mac_list->ea, num * sizeof(struct ether_addr));

	free(mac_list);
	return num;
}
