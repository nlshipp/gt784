/*
 * WPS monitor
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_monitor.c,v 1.9 2011/02/16 05:57:54 khuang Exp $
 */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include <wpsheaders.h>
#include <wpscommon.h>

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#endif

#include <ap_upnp_sm.h>
#include <tutrace.h>
#include <shutils.h>
#include <wps_ap.h>
#include <wps_enr.h>
#include <wps.h>
#include <wps_ie.h>
#include <wps_ui.h>
#include <wps_upnp.h>
#include <wps_eap.h>
#include <wps_led.h>
#include <wps_pb.h>
#include <wps_wl.h>
#include <wps_aplockdown.h>


static int wps_flags;
static char wps_readbuf[WPS_EAPD_READ_MAX_LEN];
static int wps_buflen;
static unsigned char wps_uuid[16];
static int wps_ess_num = 0;

static wps_app_t s_wps_app = {0};
static wps_app_t *wps_app = &s_wps_app;

static int wps_conf_num;
static char **wps_conf_list;

static wps_hndl_t *wps_hndl_head = NULL;

unsigned char *wps_get_uuid()
{
	return wps_uuid;
}

int wps_get_ess_num()
{
	return wps_ess_num;
}

wps_app_t *
get_wps_app()
{
	return wps_app;
}

/* WPS packet handle functions */
void
wps_hndl_add(wps_hndl_t *hndl)
{
	wps_hndl_t *node = wps_hndl_head;

	/* Check duplicate */
	while (node) {
		if (node == hndl)
			return;

		node = node->next;
	}

	/* Do prepend */
	hndl->next = wps_hndl_head;
	wps_hndl_head = hndl;

	return;
}

void
wps_hndl_del(wps_hndl_t *hndl)
{
	wps_hndl_t *temp, *prev;

	/* Search match */
	prev = NULL;
	temp = wps_hndl_head;
	while (temp) {
		if (temp == hndl) {
			/* Dequeue */
			if (prev == NULL)
				wps_hndl_head = wps_hndl_head->next;
			else
				prev->next = wps_hndl_head->next;

			return;

		}

		prev = temp;
		temp = temp->next;
	}

	return;
}

void
wps_close_session()
{
	TUTRACE((TUTRACE_INFO, "Session closed!!\n"));

	wps_ui_set_env("wps_config_command", "0");
	wps_ui_set_env("wps_action", "0");

	wps_ui_clear_pending();
#ifdef BCMWPSAP
	wps_upnp_clear_ssr();
#endif

	if (wps_app->wksp) {
		(*wps_app->close)(wps_app->wksp);

		wps_app->wksp = 0;
		wps_app->close = 0;
		wps_app->process = 0;
		wps_app->check_timeout = 0;
	}

	wps_ie_set(NULL, NULL);
}

int
wps_deauthenticate(unsigned char *bssid, unsigned char *sta_mac, int reason)
{
	int i, imax;
	char ifname[16];
	char tmp[256], prefix[] = "wlXXXXXXXXXX_";
	char *wlnames;
	char *next = NULL;
	char wl_mac[SIZE_6_BYTES];
	char *wl_hwaddr = NULL;

	/* Check mac */
	imax = wps_get_ess_num();
	for (i = 0; i < imax; i++) {
		sprintf(tmp, "ess%d_wlnames", i);
		wlnames = wps_safe_get_conf(tmp);
		if (strlen(wlnames) == 0)
			continue;

		foreach(ifname, wlnames, next) {
			/* Get configured ethernet MAC address */
			snprintf(prefix, sizeof(prefix), "%s_", ifname);
			wl_hwaddr = wps_get_conf(strcat_r(prefix, "hwaddr", tmp));
			if (wl_hwaddr == NULL)
				continue;

			ether_atoe(wl_hwaddr, wl_mac);
			if (memcmp(wl_mac, bssid, 6) == 0)
				goto found;
			else
				wl_hwaddr = NULL;
		}
	}

found:
	if (wl_hwaddr == NULL)
		return -1;

	TUTRACE((TUTRACE_INFO, "%s\n", ifname));

	/* Issue wl driver deauth */
	return wps_wl_deauthenticate(ifname, sta_mac, reason);
}

static int
wps_process_msg(char *buf, int buflen, wps_hndl_t *hndl)
{
	int ret = WPS_CONT;

	if (!hndl)
		return ret;

	/* check buffer type */
	switch (hndl->type) {
	case WPS_RECEIVE_PKT_UI:
	case WPS_RECEIVE_PKT_PB:
		/* from ui */
		ret = wps_ui_process_msg(buf, buflen);
		break;

	case WPS_RECEIVE_PKT_EAP:
		/* from eapd */
		ret = wps_eap_process_msg(buf, buflen);
		break;

	case WPS_RECEIVE_PKT_UPNP:
		/* from upnp */
#ifdef BCMWPSAP
		ret = wps_upnp_process_msg(buf, buflen, hndl);
#endif /* BCMWPSAP */
		break;

	default:
		break;
	}

	return ret;
}

static wps_hndl_t *
wps_check_fd(char *buf, int *buflen)
{
	wps_hndl_t *hndl;

	/* Forge ui_hndl for PBC */
	if ((hndl = (wps_hndl_t *)wps_pb_check(buf, buflen)) == NULL)
		hndl = wps_osl_wait_for_all_packets(buf, buflen, wps_hndl_head);

	return hndl;
}

bool
wps_is_wps_enr(char *wps_ifname)
{
	char tmp[100];
	char *wps_mode;

	if (wps_ifname == NULL)
		return false;

	sprintf(tmp, "%s_wps_mode", wps_ifname);
	wps_mode = wps_safe_get_conf(tmp);
	if (!strcmp(wps_mode, "enr_enabled"))
		return true;
	else
		return false;
}

static int
wps_open()
{
	char *value;

	/* clear variables */
	wps_flags = 0;

#ifdef BCMWPSAP
	wps_upnp_device_uuid(wps_uuid);
#endif

	value = wps_get_conf("wps_ess_num");
	if (!value || (wps_ess_num = atoi(value)) == 0) {
		TUTRACE((TUTRACE_INFO, "No ESS found\n"));
		return -1;
	}

	/* Init push button */
	if (wps_pb_init() == -1) {
		TUTRACE((TUTRACE_INFO, "No wireless interfaces found\n"));
		return -1;
	}

	/* Init wps led */
	wps_led_init();

	/* Init ap lock down log function */
	wps_aplockdown_init();

	/* Init packet handler */
	wps_hndl_head = 0;

	wps_eap_init();

	wps_ui_init();


	wps_ie_set(NULL, NULL);

#ifdef BCMWPSAP
	/* init upnp */
	wps_upnp_init();
#endif /* BCMWPSAP */

	return 0;
}

static int
wps_close()
{
	/* clean up app workspace */
	wps_close_session();

	wps_ie_clear();

	/* free allocated AP Lock down buffer */
	wps_aplockdown_cleanup();

	/* disconnect push button */
	wps_pb_deinit();

	/* disconnect led */
	wps_led_deinit();

	/* close bind socket */
	wps_eap_deinit();

	wps_ui_deinit();

#ifdef BCMWPSAP
	/* detach upnp */
	wps_upnp_deinit();
#endif /* BCMWPSAP */


	return 0;
}

#if 0
defined(AEI_VDSL_CUSTOMER_QWEST)  //hk_wps_q2000
unsigned int c_int=0; 
int flag_tmp=0;
//unsigned long now_tmp=0;
#endif
/* Periodically timed-out check */
static void
wps_timeout()
{
#if 0
defined(AEI_VDSL_CUSTOMER_QWEST)  //hk_wps_q2000
	if(flag_tmp==1)
		c_int++;
	if(c_int==1200 && flag_tmp==1&&wps_getProcessStates()==WPS_TIMEOUT) {
		wps_setProcessStates(WPS_INIT);
		c_int = 0;
		flag_tmp==0;
	}
//end
#endif
	if (wps_ui_pending_expire()) {
		/* state maching in progress, check stop or push button here */
		wps_close_session();
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS) || defined(AEI_VDSL_CUSTOMER_CENTURYLINK) //hk_ctl//hk_telus
		wps_setProcessStates(WPS_TIMEOUT);
//HuangKe Add
/*
                if(flag_tmp==0||now_tmp!=wps_get_penging_time()) {
                        wps_setProcessStates(WPS_TIMEOUT);
//                	unsigned long now;
                        (void) time((time_t*)&now_tmp);
                        wps_set_penging_time(now_tmp);
                        flag_tmp = 1;
                }
                else {
                        wps_setProcessStates(WPS_INIT);
                        flag_tmp = 0;
                }
*/
//printf("\n------------------------------- [WPS_TIMEOUT]  ----------------------------------------\n");
//		flag_tmp = 1;
//		c_int = 0;
//End
#else
		wps_setProcessStates(WPS_INIT);
#endif

		/*
		 * set wps LAN all leds to normal and wps_pb_state
		 * when session timeout or user stop
		 */
		wps_pb_state_reset();
	}

#ifdef BCMWPSAP
	if (wps_upnp_ssr_expire()) {
		wps_upnp_clear_ssr();
		wps_ie_set(NULL, NULL);
	}
#endif /* BCMWPSAP */

	/* do led status update */
	wps_pb_timeout();

	wps_led_update();

	/* do ap lock down ageout check */
	wps_aplockdown_check();

}

/* WPS message dispatch */
static int
wps_dispatch()
{
	int ret;
	wps_hndl_t *hndl;

	wps_buflen = sizeof(wps_readbuf);

	memset(&wps_readbuf, 0, sizeof(wps_readbuf));

	/* 2. check packet from Eapd, UPnP and Wpsap, and do proper action */
	hndl = wps_check_fd(wps_readbuf, &wps_buflen);

	/* 3. process received buffer */
	ret = wps_process_msg(wps_readbuf, wps_buflen, hndl);

	/* 4. if app exist, check app timeout */
	if (wps_app->wksp && ret == WPS_CONT)
		ret = (*wps_app->check_timeout)(wps_app->wksp);

	/* 5. check wps application process result */
	if (wps_app->wksp) {
		if (ret != WPS_CONT) {
			TUTRACE((TUTRACE_INFO, "ret value = %d\n", ret));

			/* wps operation completed. close session */
			wps_close_session();

			/* wpsap finished, set LAN leds to NORMAL */
			wps_pb_state_reset();

			if (wps_app->mode == WPSM_ENROLL) {
				if (ret == WPS_RESULT_SUCCESS_RESTART) {
					/* get configuration successful */
					wps_flags = WPSM_WKSP_FLAG_SUCCESS_RESTART;
					return 0;
				}
			}
			else {
				if ((wps_app->mode == WPSM_UNCONFAP) &&
					((ret == WPS_RESULT_SUCCESS_RESTART) ||
					(ret == WPS_RESULT_SUCCESS))) {
					if (ret == WPS_RESULT_SUCCESS_RESTART) {
						/* get configuration successful */
						wps_flags = WPSM_WKSP_FLAG_SUCCESS_RESTART;
						return 0;
					}
				}
				else if ((wps_app->mode == WPSM_BUILTINREG) &&
					(ret == WPS_RESULT_SUCCESS_RESTART)) {
					/*
					 * wps_flags must have WPSM_WKSP_FLAG_SET_RESTART flag to
					 * indicate the start_wps() skip to reset wps_proc_status.
					 * In the case of whent Add Enrolle in Unconfiged State.
					 */
					wps_flags = WPSM_WKSP_FLAG_SUCCESS_RESTART;
					return 0;
				}
				else {
					if (ret == WPS_RESULT_CONFIGAP_PINFAIL) {
						wps_aplockdown_add();
					}
				}
			}
		}
	}
	else if (ret == WPS_ERR_OPEN_SESSION) {
		TUTRACE((TUTRACE_ERR, "Session open failed\n"));

		/* wps operation completed. close session */
		wps_close_session();

		/* wpsap finished, set LAN leds to NORMAL */
		wps_pb_state_reset();
	}

	return 0;
}

char *
wps_get_conf(char *name)
{
	int i;
	char *p, **conf, *val = NULL;

	conf = wps_conf_list;

	for (i = 0; i < wps_conf_num; i++, conf++) {
		p = strstr(*conf, "=");
		if (p) {
			if (!strncmp(*conf, name, strlen(name))) {
				val = (p+1);
				break;
			}
		}
	}
	return val;
}

char *
wps_safe_get_conf(char *name)
{
	char *value = wps_get_conf(name);

	if (value == 0)
		value = "";

	return value;
}

int wps_set_conf(char *name, char *value)
{
	return wps_osl_set_conf(name, value);
}

/* WPS mainloop */
int
wps_mainloop(int num, char **list)
{
	wps_conf_list = list;
	wps_conf_num = num;

	/* init */
	if (wps_open() < 0) {
		TUTRACE((TUTRACE_ERR, "WPSM work space initial failed.\n"));
		return 0;
	}

	/* main loop to dispatch message */
	while (1) {
		/* do packets dispatch */
		wps_dispatch();

		/* check flag for shutdown */
		if (wps_flags & WPSM_WKSP_FLAG_SHUTDOWN) {
			wps_close();
			break;
		}

		/* check timeout */
		wps_timeout();
	}

	return wps_flags;
}

/* termination handler, call by osl main function */
void
wps_stophandler(int sig)
{
	wps_flags |= WPSM_WKSP_FLAG_SHUTDOWN;
	return;
}

/* restart handler, call by osl main function */
void
wps_restarthandler(int sig)
{
	wps_flags |= WPSM_WKSP_FLAG_SET_RESTART | WPSM_WKSP_FLAG_SHUTDOWN;
	return;
}
