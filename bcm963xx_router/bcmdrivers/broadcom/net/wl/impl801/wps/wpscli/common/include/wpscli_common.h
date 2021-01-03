/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_common.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Common OSI definition
 *
 */
#ifndef  __WPSCLI_SM_H__
#define  __WPSCLI_SM_H__

#include <time.h>
#include "wpstypes.h"
#include <tutrace.h>
#include <typedefs.h>

// Constant definition
#define WPSCLI_GENERIC_POLL_TIMEOUT		1000  	// in milli-seconds
#define WPSCLI_MONITORING_POLL_TIMEOUT	100		// in milli-seconds
#define WPS_DEFAULT_PIN				"00000000"

// define keyMgmt string values inherited from enrollee engine
#define KEY_MGMT_OPEN				"OPEN"
#define KEY_MGMT_SHARED				"SHARED"
#define KEY_MGMT_WPAPSK				"WPA-PSK"
#define KEY_MGMT_WPA2PSK			"WPA2-PSK"
#define KEY_MGMT_WPAPSK_WPA2PSK		"WPA-PSK WPA2-PSK"

typedef struct td_wpscli_osi_context {
	uint8 state;					// Current state
	uint8 sta_mac[6];				// STA's mac address
	uint8 bssid[6];					// AP's bssid
	uint8 sta_type;					// Enrollee or registrar
	char ssid[SIZE_SSID_LENGTH+1];	// ssid string with string terminator
	brcm_wpscli_wps_mode mode;
	brcm_wpscli_role role;			// SoftAP or Station
	brcm_wpscli_pwd_type pwd_type;
	char pin[8];
	long start_time;
	long time_out;				// WPS timout period in seconds
} wpscli_osi_context;

extern brcm_wpscli_request_ctx g_ctxWpscliReq;  // WPS library caller request context
extern brcm_wpscli_status_cb g_cbWpscliStatus;  // WPS caller client's callback function

extern wpscli_osi_context g_ctxWpscliOsi;
extern int g_bRequestAbort;

extern void print_buf(unsigned char *buff, int buflen);

#endif  //  __WPSCLI_SM_H__
