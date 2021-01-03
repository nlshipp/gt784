/*
 * WPS push button
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_pb.c,v 1.4 2011/02/16 05:57:54 khuang Exp $
 */

#include <stdio.h>
#include <time.h>
#include <tutrace.h>
#include <wps_pb.h>
#include <wpscommon.h>
#include <bcmparams.h>
#include <wps_led.h>
#include <security_ipc.h>
#include <wps_wl.h>
#include <bcmutils.h>
#include <wlif_utils.h>
#include <shutils.h>
#include <wps.h>

static time_t wps_pb_selecting_time = 0;
static PBC_STA_INFO pbc_info[PBC_OVERLAP_CNT];
static int wps_pb_state = WPS_PB_STATE_INIT;
static wps_hndl_t pb_hndl;
static int pb_last_led_id = -1;
static char pb_ifname[IFNAMSIZ] = {0};

/* PBC Overlapped detection */
int
wps_pb_check_pushtime(unsigned long time)
{
	int i;
	int PBC_sta = PBC_OVERLAP_CNT;
	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if ((time < pbc_info[i].last_time) || ((time - pbc_info[i].last_time) > 120)) {
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));
		}

		if (pbc_info[i].last_time == 0)
			PBC_sta--;
	}

	TUTRACE((TUTRACE_INFO, "There are %d sta in PBC mode!\n", PBC_sta));
	TUTRACE((TUTRACE_INFO, "sta1: %02x:%02x:%02x:%02x:%02x:%02x, %d\n",
		pbc_info[0].mac[0], pbc_info[0].mac[1], pbc_info[0].mac[2], pbc_info[0].mac[3],
		pbc_info[0].mac[4], pbc_info[0].mac[5], pbc_info[0].last_time));
	TUTRACE((TUTRACE_INFO, "sta2: %02x:%02x:%02x:%02x:%02x:%02x, %d\n",
		pbc_info[1].mac[1], pbc_info[1].mac[1], pbc_info[1].mac[2], pbc_info[1].mac[3],
		pbc_info[1].mac[4], pbc_info[1].mac[5], pbc_info[1].last_time));
	return PBC_sta;
}

void
wps_pb_update_pushtime(char *mac)
{
	int i;
	unsigned long now;

	(void) time((time_t*)&now);

	wps_pb_check_pushtime(now);

	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (memcmp(mac, pbc_info[i].mac, 6) == 0) {
			pbc_info[i].last_time = now;
			return;
		}
	}

	if (pbc_info[0].last_time <= pbc_info[1].last_time)
		i = 0;
	else
		i = 1;

	memcpy(pbc_info[i].mac, mac, 6);
	pbc_info[i].last_time = now;

	return;
}

void
wps_pb_clear_sta(char *mac)
{
	int i;

	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (memcmp(mac, pbc_info[i].mac, 6) == 0) {
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));
			return;
		}
	}

	return;
}

static int
wps_pb_find_next()
{
	char *value, *next;
	int next_id;
	int max_id = wps_hal_led_wl_max();
	char tmp[100];
	char ifname[IFNAMSIZ];
	char *wlnames;

	int target_id = -1;
	int target_instance = 0;
	int i, imax;

	imax = wps_get_ess_num();

refind:
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_led_id", i);
		value = wps_get_conf(tmp);
		if (value == NULL)
			continue;

		next_id = atoi(value);
		if ((next_id > pb_last_led_id) && (next_id <= max_id)) {
			if ((target_id == -1) || (next_id < target_id)) {
				/* Save the candidate */
				target_id = next_id;
				target_instance = i;
			}
		}
	}

	/* A candidate found ? */
	if (target_id == -1) {
		pb_last_led_id = -1;
		goto refind;
	}

	pb_last_led_id = target_id;

	/* Take the first wl interface */
	sprintf(tmp, "ess%d_wlnames", target_instance);

	wlnames = wps_safe_get_conf(tmp);
	foreach(ifname, wlnames, next) {
		strncpy(pb_ifname, ifname, sizeof(pb_ifname));
		break;
	}

	return target_id;
}

static int
wps_pb_retrieve_event(int unit, char *wps_ifname)
{
	int press;
	int event = WPSBTN_EVTI_NULL;
	int imax = wps_get_ess_num();

	/*
	 * According to WPS Usability and Protocol Best Practices spec
	 * section 3.1 WPS Push Button Enrollee Addition sub-section 2.2.
	 * Skip using LAN led to indicate which ESS is going
	 * to run WPS when WPS ESS only have one
	 */
	press = wps_hal_btn_pressed();
#ifndef DSLCPE
	if (imax == 1 &&
	    (press == WPS_LONG_BTNPRESS || press == WPS_SHORT_BTNPRESS)) {
		wps_pb_find_next();
		wps_pb_state = WPS_PB_STATE_CONFIRM;
		strcpy(wps_ifname, pb_ifname);
		return WPSBTN_EVTI_PUSH;
	}
#endif

	switch (press) {
	case WPS_LONG_BTNPRESS:
#ifdef DSLCPE
	case WPS_SHORT_BTNPRESS:
#endif
		TUTRACE((TUTRACE_INFO, "Detected a wps LONG button-push\n"));

		if (pb_last_led_id == -1)
			wps_pb_find_next();

		/* clear LAN all leds first */
		if (wps_pb_state == WPS_PB_STATE_INIT)
			wps_led_wl_select_on();

		wps_led_wl_confirmed(pb_last_led_id);

		wps_pb_state = WPS_PB_STATE_CONFIRM;
		strcpy(wps_ifname, pb_ifname);

		event = WPSBTN_EVTI_PUSH;
		break;
#ifndef DSLCPE
	/* selecting */
	case WPS_SHORT_BTNPRESS:
		TUTRACE((TUTRACE_INFO, "Detected a wps SHORT button-push, "
			"wps_pb_state = %d\n", wps_pb_state));

		if (wps_pb_state == WPS_PB_STATE_INIT ||
			wps_pb_state == WPS_PB_STATE_SELECTING) {

			/* start bssid selecting, find next enabled bssid */
			/*
			 * NOTE: currently only support wireless unit 0
			 */
			wps_pb_find_next();

			/* clear LAN all leds when first time enter */
			if (wps_pb_state == WPS_PB_STATE_INIT)
				wps_led_wl_select_on();

			wps_led_wl_selecting(pb_last_led_id);

			/* get selecting time */
			wps_pb_state = WPS_PB_STATE_SELECTING;
			wps_pb_selecting_time = time(0);
		}
		break;
#endif
	default:
		break;
	}

	return event;
}

void
wps_pb_timeout()
{
	time_t curr_time;

	/* check timeout when in WPS_PB_STATE_SELECTING */
	if (wps_pb_state == WPS_PB_STATE_SELECTING) {
		curr_time = time(0);
		if (curr_time > (wps_pb_selecting_time+WPS_PB_SELECTING_MAX_TIMEOUT)) {
			/* Reset pb state to init because of timed-out */
			wps_pb_state_reset();
		}
	}
}

int
wps_pb_state_reset()
{
	/* reset wps_pb_state to INIT */
	pb_last_led_id = -1;

	wps_pb_state = WPS_PB_STATE_INIT;

	/* Back to NORMAL */
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS) || defined(AEI_VDSL_CUSTOMER_CENTURYLINK) //hk_ctl//hk_telus 
	int val=wps_getProcessStates();
	if(val != 4)
#endif
	wps_hal_led_wl_select_off();

	return 0;
}


wps_hndl_t *
wps_pb_check(char *buf, int *buflen)
{
	char wps_ifname[IFNAMSIZ] = {0};

	/* note: push button currently only support wireless unit 0 */
	/* user can use PBC to tigger wps_enr start */
	if (WPSBTN_EVTI_PUSH == wps_pb_retrieve_event(0, wps_ifname)) {
		int uilen = 0;

		uilen += sprintf(buf + uilen, "SET ");

		TUTRACE((TUTRACE_INFO, "wps monitor: Button pressed!!\n"));

		wps_close_session();

		uilen += sprintf(buf + uilen, "wps_config_command=%d ", WPS_UI_CMD_START);
		uilen += sprintf(buf + uilen, "wps_ifname=%s ", wps_ifname);
		uilen += sprintf(buf + uilen, "wps_pbc_method=%d ", WPS_UI_PBC_HW);

		wps_setProcessStates(WPS_ASSOCIATED);

		/* for wps_enr application, start it right now */
		if (wps_is_wps_enr(wps_ifname)) {
			uilen += sprintf(buf + uilen, "wps_action=%d ", WPS_UI_ACT_ENROLL);
		}
		/* for wps_ap application */
		else {
			uilen += sprintf(buf + uilen, "wps_action=%d ", WPS_UI_ACT_ADDENROLLEE);
		}

		uilen += sprintf(buf + uilen, "wps_sta_pin=00000000 ");

		*buflen = uilen;
		return &pb_hndl;
	}

	return NULL;
}

int
wps_pb_init()
{
	memset(pbc_info, 0, sizeof(pbc_info));

	memset(&pb_hndl, 0, sizeof(pb_hndl));
	pb_hndl.type = WPS_RECEIVE_PKT_PB;
	pb_hndl.handle = -1;

	return (wps_hal_btn_init());
}

int
wps_pb_deinit()
{
	wps_pb_state_reset();

	wps_hal_btn_cleanup();

	return 0;
}
