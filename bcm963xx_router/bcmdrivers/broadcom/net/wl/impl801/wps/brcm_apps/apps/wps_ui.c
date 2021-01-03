/*
 * WPS ui
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_ui.c,v 1.2 2010/12/10 02:20:10 khuang Exp $
 */

#include <stdio.h>
#include <tutrace.h>
#include <time.h>
#include <wps_wl.h>
#include <wps_ui.h>
#include <shutils.h>
#include <wlif_utils.h>
#include <wps_ap.h>
#include <wps_enr.h>
#include <security_ipc.h>
#include <wps.h>
#include <wps_pb.h>
#include <wps_led.h>
#include <wps_upnp.h>
#include <wps_ie.h>

typedef struct {
	int wps_config_command;	/* Config */
	int wps_method;
	int wps_action;
	int wps_pbc_method;
	char wps_ifname[16];
	char wps_sta_pin[64];
	int wps_aplockdown;
	int wps_proc_status;	/* Status report */
	int wps_pinfail;
	char wps_sta_mac[sizeof("00:00:00:00:00:00")];
	char wps_sta_devname[32];
	char wps_enr_wsec[32];
	char wps_enr_ssid[32];
	char wps_enr_bssid[32];
	int wps_enr_scan;
	int wps_enr_auto; /* automatically select network to enroll */
} wps_ui_t;

static wps_hndl_t ui_hndl;
static int pending_flag = 0;
static unsigned long pending_time = 0;
static wps_ui_t s_wps_ui;
static wps_ui_t *wps_ui = &s_wps_ui;

char *
wps_ui_get_env(char *name)
{
	static char buf[32];

	if (!strcmp(name, "wps_config_command")) {
		sprintf(buf, "%d", wps_ui->wps_config_command);
		return buf;
	}

	if (!strcmp(name, "wps_method")) {
		sprintf(buf, "%d", wps_ui->wps_method);
		return buf;
	}

	if (!strcmp(name, "wps_pbc_method")) {
		sprintf(buf, "%d", wps_ui->wps_pbc_method);
		return buf;
	}

	if (!strcmp(name, "wps_sta_pin"))
		return wps_ui->wps_sta_pin;

	if (!strcmp(name, "wps_proc_status")) {
		sprintf(buf, "%d", wps_ui->wps_proc_status);
		return buf;
	}

	if (!strcmp(name, "wps_enr_wsec")) {
		return wps_ui->wps_enr_wsec;
	}

	if (!strcmp(name, "wps_enr_ssid")) {
		return wps_ui->wps_enr_ssid;
	}

	if (!strcmp(name, "wps_enr_bssid")) {
		return wps_ui->wps_enr_bssid;
	}

	if (!strcmp(name, "wps_enr_scan")) {
		sprintf(buf, "%d", wps_ui->wps_enr_scan);
		return buf;
	}

	if (!strcmp(name, "wps_enr_auto")) {
		sprintf(buf, "%d", wps_ui->wps_enr_auto);
		return buf;
	}

	return "";
}

void
wps_ui_set_env(char *name, char *value)
{
	if (!strcmp(name, "wps_ifname"))
		strncpy(wps_ui->wps_ifname, value, sizeof(wps_ui->wps_ifname));

	if (!strcmp(name, "wps_config_command"))
		wps_ui->wps_config_command = atoi(value);

	if (!strcmp(name, "wps_action"))
		wps_ui->wps_action = atoi(value);

	if (!strcmp(name, "wps_aplockdown"))
		wps_ui->wps_aplockdown = atoi(value);

	if (!strcmp(name, "wps_proc_status"))
		wps_ui->wps_proc_status = atoi(value);

	if (!strcmp(name, "wps_pinfail"))
		wps_ui->wps_pinfail = atoi(value);

	if (!strcmp(name, "wps_sta_mac"))
		strncpy(wps_ui->wps_sta_mac, value, sizeof(wps_ui->wps_sta_mac));

	if (!strcmp(name, "wps_sta_devname"))
		strncpy(wps_ui->wps_sta_devname, value, sizeof(wps_ui->wps_sta_devname));

	return;
}

int
wps_ui_is_pending()
{
	return pending_flag;
}

static void
wps_ui_start_pending(char *wps_ifname)
{
	unsigned long now;
	CTlvSsrIE ssrmsg;

	(void) time((time_t*)&now);

	/* Check push time only when we use PBC method */
	if (!strcmp(wps_ui_get_env("wps_sta_pin"), "00000000") &&
	    wps_pb_check_pushtime(now) == PBC_OVERLAP_CNT) {
		TUTRACE((TUTRACE_INFO, "%d PBC station found, ignored PBC!\n", PBC_OVERLAP_CNT));
		wps_ui->wps_config_command = WPS_UI_CMD_NONE;
		wps_setProcessStates(WPS_PBCOVERLAP);

		return;
	}

	pending_flag = 1;
	(void) time((time_t*)&pending_time);

	/* Set built-in registrar to be selected registrar */
	wps_ie_default_ssr_info(&ssrmsg);

	wps_ie_set(wps_ifname, &ssrmsg);

	return;
}

void
wps_ui_clear_pending()
{
	pending_flag = 0;
	pending_time = 0;
}

int
wps_ui_pending_expire()
{
	unsigned long now;

	if (pending_flag && pending_time) {
		(void) time((time_t*)&now);
#if defined(AEI_VDSL_CUSTOMER_TELUS) //hk_telus
		if ((now < pending_time) || ((now - pending_time) > 180))
#else
		if ((now < pending_time) || ((now - pending_time) > WPS_MAX_TIMEOUT))
#endif			
			return 1;
	}

	return 0;
}

static void
wps_ui_do_get()
{
	unsigned char wps_uuid[16];
	char buf[256];
	int len = 0;

	len += sprintf(buf + len, "wps_config_command=%d ", wps_ui->wps_config_command);
	len += sprintf(buf + len, "wps_action=%d ", wps_ui->wps_action);
	len += sprintf(buf + len, "wps_ifname=%s ", wps_ui->wps_ifname);
#ifdef BCMWPSAP
	wps_upnp_device_uuid(wps_uuid);
	len += sprintf(buf + len,
		"wps_uuid=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x ",
		wps_uuid[0], wps_uuid[1], wps_uuid[2], wps_uuid[3],
		wps_uuid[4], wps_uuid[5], wps_uuid[6], wps_uuid[7],
		wps_uuid[8], wps_uuid[9], wps_uuid[10], wps_uuid[11],
		wps_uuid[12], wps_uuid[13], wps_uuid[14], wps_uuid[15]);
#endif /* BCMWPSAP */
	len += sprintf(buf + len, "wps_aplockdown=%d ", wps_ui->wps_aplockdown);
	len += sprintf(buf + len, "wps_proc_status=%d ", wps_ui->wps_proc_status);
	len += sprintf(buf + len, "wps_pinfail=%d ", wps_ui->wps_pinfail);
	len += sprintf(buf + len, "wps_sta_mac=%s ", wps_ui->wps_sta_mac);
	len += sprintf(buf + len, "wps_sta_devname=%s ", wps_ui->wps_sta_devname);

	wps_osl_send_uimsg(&ui_hndl, buf, len+1);
	return;
}

static int
wps_ui_parse_env(char *buf, wps_ui_t *ui)
{
	char *argv[32] = {0};
	char *item, *p, *next;
	char *name, *value;
	int i;

	/* Seperate buf into argv[] */
	for (i = 0, item = buf, p = item; item && item[0]; item = next, p = 0, i++) {
		/* Get next token */
		strtok_r(p, " ", &next);
		argv[i] = item;
	}

	for (i = 0; argv[i]; i++) {
		value = argv[i];
		name = strsep(&value, "=");
		if (name) {
			if (!strcmp(name, "wps_config_command"))
				ui->wps_config_command = atoi(value);
			else if (!strcmp(name, "wps_method"))
				ui->wps_method = atoi(value);
			else if (!strcmp(name, "wps_action"))
				ui->wps_action = atoi(value);
			else if (!strcmp(name, "wps_pbc_method"))
				ui->wps_pbc_method = atoi(value);
			else if (!strcmp(name, "wps_ifname"))
				strcpy(ui->wps_ifname, value);
			else if (!strcmp(name, "wps_sta_pin"))
				strcpy(ui->wps_sta_pin, value);
			else if (!strcmp(name, "wps_enr_wsec"))
				strcpy(ui->wps_enr_wsec, value);
			else if (!strcmp(name, "wps_enr_ssid"))
				strcpy(ui->wps_enr_ssid, value);
			else if (!strcmp(name, "wps_enr_bssid"))
				strcpy(ui->wps_enr_bssid, value);
			else if (!strcmp(name, "wps_enr_scan"))
				ui->wps_enr_scan = atoi(value);
			else if (!strcmp(name, "wps_enr_auto"))
				ui->wps_enr_auto = atoi(value);
		}
	}

	return 0;
}

static int
wps_ui_do_set(char *buf)
{
	int ret = WPS_CONT;
	char *macaddr;
	wps_ui_t a_ui;
	wps_ui_t *ui = &a_ui;
	wps_app_t *wps_app = get_wps_app();

	/* Process set command */
	memset(ui, 0, sizeof(wps_ui_t));
	if (wps_ui_parse_env(buf, ui) != 0)
		return WPS_CONT;

	/* user click STOPWPS button or pending expire */
	if (ui->wps_config_command == WPS_UI_CMD_STOP) {

		/* state maching in progress, check stop or push button here */
		wps_close_session();
		wps_setProcessStates(WPS_INIT);

		/*
		  * set wps LAN all leds to normal and wps_pb_state
		  * when session timeout or user stop
		  */
		wps_pb_state_reset();
	}
	else {
		if ((!wps_app->wksp || (wps_app->mode == WPSM_PROXY)) && !pending_flag) {

			/* Fine to accept set command */
			memcpy(wps_ui, ui, sizeof(*ui));

			/* some button in UI is pushed */
			if (wps_ui->wps_config_command == WPS_UI_CMD_START) {
				wps_setProcessStates(WPS_INIT);

				/* if proxy in process, stop it */
				if (wps_app->wksp) {
					wps_close_session();
					wps_ui->wps_config_command = WPS_UI_CMD_START;
				}

				if (wps_is_wps_enr(wps_ui->wps_ifname)) {
					/* set what interface you want */
#ifdef BCMWPSAPSTA
					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);
					ret = wpsenr_open_session(wps_app, wps_ui->wps_ifname);
					if (ret != WPS_CONT) {
						/* Normally it cause by associate time out */
						wps_setProcessStates(WPS_TIMEOUT);
					}
#endif /* BCMWPSAPSTA */

				}
				/* for wps_ap application */
				else {
					/* set the process status */
					wps_setProcessStates(WPS_ASSOCIATED);

					if (wps_ui->wps_action == WPS_UI_ACT_CONFIGAP) {
						/* TBD - Initial all relative nvram setting */
						char tmptr[] = "wlXXXXXXXXXX_hwaddr";
						sprintf(tmptr, "%s_hwaddr", wps_ui->wps_ifname);
						macaddr = wps_get_conf(tmptr);
						if (macaddr) {
							char ifmac[SIZE_6_BYTES];
							ether_atoe(macaddr, ifmac);
#ifdef BCMWPSAP
							ret = wpsap_open_session(wps_app,
								WPSM_UNCONFAP, ifmac, NULL,
								wps_ui->wps_ifname, NULL, NULL);
#endif /* BCMWPSAP */
						}
						else {
							TUTRACE((TUTRACE_INFO, "can not find mac "
								"on %s\n", wps_ui->wps_ifname));
						}
					}
					else if (wps_ui->wps_action == WPS_UI_ACT_ADDENROLLEE) {
						/* Do nothing, wait for client connect */
						wps_ui_start_pending(wps_ui->wps_ifname);
					}
				}
			}
		}
	}

	return ret;
}

int
wps_ui_process_msg(char *buf, int buflen)
{
	if (strncmp(buf, "GET", 3) == 0) {
		wps_ui_do_get();
	}

	if (strncmp(buf, "SET ", 4) == 0) {
		return wps_ui_do_set(buf+4);
	}

	return WPS_CONT;
}

void
wps_setStaDevName(unsigned char *str)
{
	if (str) {
		wps_ui_set_env("wps_sta_devname", str);
		wps_set_conf("wps_sta_devname", str);
	}

	return;
}

int
wps_ui_init()
{
	/* Init eap_hndl */
	memset(&ui_hndl, 0, sizeof(ui_hndl));

	ui_hndl.type = WPS_RECEIVE_PKT_UI;
	ui_hndl.handle = wps_osl_ui_handle_init();
	if (ui_hndl.handle == -1)
		return -1;

	wps_hndl_add(&ui_hndl);
	return 0;
}

void
wps_ui_deinit()
{
	wps_hndl_del(&ui_hndl);
	wps_osl_ui_handle_deinit(ui_hndl.handle);
	return;
}
