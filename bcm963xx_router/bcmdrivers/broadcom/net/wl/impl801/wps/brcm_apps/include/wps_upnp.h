/*
 * WPS upnp
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_upnp.h,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#ifndef __WPS_UPNP_H__
#define __WPS_UPNP_H__

#include <wps.h>

typedef struct upnp_attached_if_list {
	struct upnp_attached_if_list *next;
	int ess_id;
	char ifname[IFNAMSIZ]; /* for libupnp */
	int instance;
	wps_hndl_t upnp_hndl;
	char mac[6];
	char wl_name[IFNAMSIZ];
	char *m1_buf;
	int m1_len;
	unsigned int m1_built_time;
	char *enr_nonce;
	char *private_key;
} upnp_attached_if;

void wps_upnp_init();
void wps_upnp_deinit();
void wps_upnp_clear_ssr();
void wps_upnp_clear_ssr_timer();
int wps_upnp_ssr_expire();
void wps_upnp_device_uuid(unsigned char *uuid);
char *wps_upnp_parse_msg(char *upnpmsg, int upnpmsg_len, int *len, int *type, char *addr);
int wps_upnp_process_msg(char *upnpmsg, int upnpmsg_len, wps_hndl_t *hndl);
int wps_upnp_send_msg(int if_instance, char *buf, int len, int type);

void wps_upnp_update_wlan_event(int if_instance, unsigned char *macaddr,
	char *databuf, int datalen, int init);
void wps_upnp_update_init_wlan_event(int if_instance, char *mac, int init);

#ifdef __CONFIG_LIBUPNP__
int wps_libupnp_ProcessMsg(char *ifname, char *upnpmsg, int upnpmsg_len);
int wps_libupnp_GetOutMsgLen(char *ifname);
char *wps_libupnp_GetOutMsg(char *ifname);
#endif /* __CONFIG_LIBUPNP__ */

#endif	/* __WPS_UPNP_H__ */