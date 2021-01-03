/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_sta.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement STA side APIs in wpscli_api.h 
 *
 */
// Include wpscli library OSL header file
#include "wpscli_osl.h"

// Include common OSI header file
#include "wpscli_common.h"

#include <wpstypes.h>
#include <typedefs.h>
#include <wlioctl.h>
#include <portability.h>
#include <reg_prototlv.h>
#include <wps_enrapi.h>
#include <eap_defs.h>
#include <wpserror.h>
#include <tutrace.h>
#include <ethernet.h>
#include <proto/eapol.h>
#if defined(UNDER_CE)
#include <proto/eap.h>
#else
#include <eap.h>
#endif

// Variable length of WPS client device information in WPS IE fields. 
#define STA_DEVINFO_DEVICE_NAME			"Broadcom Client"
#define STA_DEVINFO_MANUFACTURER		"Broadcom"
#define STA_DEVINFO_MODEL_NAME			"WPS Wireless Client"
#define STA_DEVINFO_MODEL_NO			"1234"
#define STA_DEVINFO_SERIAL_NO			"5678"

#define WPS_APLIST_BUF_LEN			(127 * 1024)
#define WPS_MAX_AP_SCAN_LIST_LEN	50

// External declaration
extern void wpscli_update_status(brcm_wpscli_status status, brcm_wpscli_status_cb_data cb_data);
extern int wps_osl_init(char *bssid);

static char g_ScanResult[WPS_APLIST_BUF_LEN]; 
static uint32 g_WpsApTotal = 0;
static wps_ap_list_info_t ap_list[WPS_MAX_AP_SCAN_LIST_LEN];

// Internal functions
brcm_wpscli_status wpscli_sta_eap_send_data_down(char *dataBuffer, uint32 dataLen);
static brcm_wpscli_status wpscli_create_wps_ap_list(int *nAP);
static void wpscli_sta_config_init(const uint8 *sta_mac);
static brcm_wpscli_status wpscli_start_enroll_device(const uint8 *bssid, const char *pin);
static brcm_wpscli_status wpscli_get_network_settings(const char *ssid, brcm_wpscli_nw_settings *nw_settings);
static int wpscli_on_evt_eap_pkt_received(const char *data, uint32 data_len, uint8 *last_sent_msg);
static int wpscli_on_evt_no_eap_pkt(uint8 *msg_type);

brcm_wpscli_status wpscli_sta_open()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "Entered : wpscli_sta_open. \n"));

	status = wpscli_wlan_open();

	TUTRACE((TUTRACE_INFO, "Exit : wpscli_sta_open. \n"));
	return status;
}

// There maybe case that the client simply just want to know how many wps APs are around
// without conducting WPS negotiation after scanning, so we need provide a separate wps 
// scan API for that
brcm_wpscli_status brcm_wpscli_sta_search_wps_ap(uint32 *wps_ap_total)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "Entered : brcm_wpscli_sta_search_wps_ap\n"));

	if(wps_ap_total == NULL) {
		TUTRACE((TUTRACE_ERR, "Exit : brcm_wpscli_sta_search_wps_ap. Invalid NULL parameter passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	g_WpsApTotal = 0;

	wpscli_update_status(WPS_STATUS_WLAN_SCAN_START, NULL);

	// Scan and get the found ap list in wl_bss_info_t format
	TUTRACE((TUTRACE_ERR, "brcm_wpscli_sta_search_wps_ap : calling wpscli_wlan_scan.\n"));
	status = wpscli_wlan_scan((wl_scan_results_t *)g_ScanResult, WPS_APLIST_BUF_LEN);
	if(status == WPS_STATUS_SUCCESS)
		// Convert and generate wps-capable ap list in wps_ap_list_info_t format
		status = wpscli_create_wps_ap_list((int *)&g_WpsApTotal);

	*wps_ap_total = g_WpsApTotal;

	TUTRACE((TUTRACE_INFO, "Exit : brcm_wpscli_sta_search_wps_ap\n"));
	return status;
}

brcm_wpscli_status brcm_wpscli_sta_get_wps_ap_list(brcm_wpscli_ap_list *wps_ap_list, uint32 buf_size, uint32 *wps_ap_total)
{
	brcm_wpscli_ap_entry *wpscli_ap_entry;
	wps_ap_list_info_t *bi;
	uint32 i;

	TUTRACE((TUTRACE_INFO, "Entered : brcm_wpscli_sta_get_wps_ap_list\n"));

	if(wps_ap_total == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	// NULL list buffer passed in and return total number of WPS APs
	if(wps_ap_list == NULL) {
		*wps_ap_total = g_WpsApTotal;
		return WPS_STATUS_SUCCESS;
	}

	if(g_WpsApTotal == 0) {
		*wps_ap_total = 0;
		return WPS_STATUS_WLAN_NO_WPS_AP_FOUND;
	}

	// Set total number of WPS APs
	*wps_ap_total = g_WpsApTotal;

	// Verify the input buffer size to be big enough
	if(buf_size < (sizeof(wps_ap_list->total_items) + g_WpsApTotal*sizeof(brcm_wpscli_ap_entry)))
		return WPS_STATUS_NOT_ENOUGH_MEMORY;

	// Fill AP information and convert 
	wps_ap_list->total_items = g_WpsApTotal;
	wpscli_ap_entry = wps_ap_list->ap_entries;
	for (i=0; i<g_WpsApTotal; i++)
	{
		// Initialize the buffer to "0"
		memset(wpscli_ap_entry, 0, sizeof(brcm_wpscli_ap_entry));  
		
		// Point to the current entry in the array
		bi = &(ap_list[i]);

		// ssid
		memcpy(wpscli_ap_entry->ssid, bi->ssid, bi->ssidLen);

		// bssid
		memcpy(wpscli_ap_entry->bssid, bi->BSSID, 6);

		// band
		wpscli_ap_entry->band = bi->band;

		// wsec. Tell whether the AP is configured Open or Secure
		wpscli_ap_entry->wsec = bi->wep;;

		// activated
		wpscli_ap_entry->activated = wps_get_select_reg(bi);

		// scstate
		wpscli_ap_entry->scstate = bi->scstate;

		// pwd_type determined by "is_SelectReg_PBC(ap_in->ie_buf, ap_in->ie_buflen)"
		if(is_SelectReg_PBC(bi->ie_buf, bi->ie_buflen))
			wpscli_ap_entry->pwd_type = BRCM_WPS_PWD_TYPE_PBC;
		else
			wpscli_ap_entry->pwd_type = BRCM_WPS_PWD_TYPE_PIN;

		// Point to next entry
		wpscli_ap_entry = (brcm_wpscli_ap_entry *)((uint8 *)wpscli_ap_entry + sizeof(brcm_wpscli_ap_entry));
	}

	TUTRACE((TUTRACE_INFO, "Exit : brcm_wpscli_sta_get_wps_ap_list\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status brcm_wpscli_sta_start_wps(const char *ssid,
											 uint8 wsec, 
											 const uint8 *bssid,
											 int num_channels, uint8 *channels,
											 brcm_wpscli_wps_mode wps_mode,
											 brcm_wpscli_pwd_type pwd_type,
											 const char *pin_code,
											 uint32 time_out,
											 brcm_wpscli_nw_settings *nw_settings)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	char buf[WPS_EAP_DATA_MAX_LENGTH];
	uint32 buf_len = WPS_EAP_DATA_MAX_LENGTH;
	time_t start_time;
	int retVal;
	uint8 rev_msg_type;
	uint8 send_msg_type;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: Entered bssid=%02x:%02x:%02x:%02x:%02x:%02x\n",
		bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]));

	if(ssid == NULL || bssid == NULL || nw_settings == NULL) {
		TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: Exiting.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}
	
	g_bRequestAbort = FALSE;
	g_ctxWpscliOsi.start_time = wpscli_current_time();
	g_ctxWpscliOsi.time_out = time_out;  // in seconds
	strcpy(g_ctxWpscliOsi.ssid, ssid);
	memcpy(g_ctxWpscliOsi.bssid, bssid, 6);

	// Start WPS preliminary association
	status = wpscli_wlan_connect(ssid, wsec, (const char *)bssid,
		num_channels, channels);
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: Exiting. wpscli_wlan_connect failed.\n"));
		return status;
	}

	// Open packet dispatcher
	status = wpscli_pktdisp_open(bssid);  
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: Exiting. wpscli_pktdisp_open failed\n"));
		goto END;
	}

	// Continue to enter the wps protocol phrase
	// Pass in NULL as pin for PBC mode
	status = wpscli_start_enroll_device(bssid, pin_code);  
	if(status != WPS_STATUS_SUCCESS) {
		goto END;
	}

	// Get protocol start time
	start_time = wpscli_current_time();

	// WPS registration loop
	do {
		// Exit if timeout is reached
		if((wpscli_current_time()-start_time) > time_out) {
			status = WPS_STATUS_PROTOCOL_FAIL_TIMEOUT;
			break;
		}

		// Exit if there is request to abort the current process
		if(g_bRequestAbort) {
			g_bRequestAbort = FALSE;
			status = WPS_STATUS_ABORTED;
			break;
		}

		// Wait for a eapol packet
		buf_len = WPS_EAP_DATA_MAX_LENGTH;  // Set buffer length
		
		status = wpscli_pktdisp_wait_for_packet(buf, &buf_len, WPSCLI_GENERIC_POLL_TIMEOUT, FALSE);
		if(status == WPS_STATUS_SUCCESS) {  
			
			// Eap data (without ether header) is received 

			if(buf != NULL) {
				rev_msg_type = wps_get_ap_msg_type(buf, buf_len);  // Get wps message type
				TUTRACE((TUTRACE_INFO, "Received EAP-Request%s\n", wps_get_msg_string((int)rev_msg_type)));

				// Notify received message type
				wpscli_update_status(WPS_STATUS_PROTOCOL_RECV_MSG, &rev_msg_type);

				// Process peer message and if needed send message inside this function
				retVal = wpscli_on_evt_eap_pkt_received(buf, buf_len, &send_msg_type);

				// Map wps engine error code to wpscli status code
				switch(retVal) 
				{
				case WPS_SUCCESS:
					// WPS protocol registration (M1~M8 phrase) is completed successful
					status = WPS_STATUS_PROTOCOL_SUCCESS;		// Protocol registration is successful
					break;
				case WPS_CONT:
				case WPS_SEND_MSG_IDRESP:						// Continue the registration process
					status = WPS_STATUS_PROTOCOL_CONTINUE;
					break;
				case WPS_ERR_ADDCLIENT_PINFAIL:
				case WPS_ERR_CONFIGAP_PINFAIL:					// Protocol registration is failed because PIN is wrong
					status = WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN;
					break;
				case REG_FAILURE:								// Protocol registration is failed because of EAP failure
					status = WPS_STATUS_PROTOCOL_FAIL_EAP;
					break;
				default:										// Protocol registration is failed because of other reasons
					status = WPS_STATUS_SYSTEM_ERR;
				}
			}
			else {
				TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: buf is NULL. System error.\n"));
				status = WPS_STATUS_SYSTEM_ERR;						// Protocol registration is failed because of other reasons
			}
		}
		/* timeout with no data, should we re-transmit ? */
		else if (status == WPS_STATUS_PKTD_NOT_EAPOL_PKT || status == WPS_STATUS_PKTD_NO_PKT) {  
			retVal = wpscli_on_evt_no_eap_pkt(&send_msg_type);

			if(retVal == WPS_STATUS_PKTD_NOT_EAPOL_PKT)
				TUTRACE((TUTRACE_INFO, "Received non-eap packet.\n"));

			switch(retVal)
			{
			case WPS_CONT:
			case WPS_SEND_MSG_CONT:								 // Continue protocol registration
				wpscli_sleep(100);
				status = WPS_STATUS_PROTOCOL_CONTINUE;
				break;
			case REG_FAILURE:									// Protocol registration is failed because of EAP not retrying anymore
				status = WPS_STATUS_PROTOCOL_FAIL_MAX_EAP_RETRY;
				break;
			default:
				status = WPS_STATUS_SYSTEM_ERR;						// Protocol registration is failed because of other reasons
			}
		}

		if(status != WPS_STATUS_PROTOCOL_CONTINUE) {
			// Done with registration
			if(status == WPS_STATUS_PROTOCOL_SUCCESS)
				// WPS protocol registration stage is successful!
				status = wpscli_get_network_settings(ssid, nw_settings);

			break;  // Exit registration loop
		}
	} while(status == WPS_STATUS_PROTOCOL_CONTINUE); 

	wps_cleanup();

END:
	wpscli_pktdisp_close();

	// Disconnect at the end of wps negotiation
	wpscli_wlan_disconnect();
	
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_sta_start_wps: Exiting. status=%d\n", status));
	return status;
}

void wpscli_sta_config_init(const uint8 *sta_mac)
{
	DevInfo info;
	char uuid[16] = {0x22,0x21,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f};

	memset((char *)(&info), 0, sizeof(info));
	info.version = 0x10;
	
	memcpy(info.uuid, uuid, 16);

	// Fill station mac address
	memcpy(info.macAddr, sta_mac, 6);  

	strcpy(info.deviceName, STA_DEVINFO_DEVICE_NAME);
	info.primDeviceCategory = 1;
	info.primDeviceOui = 0x0050F204;
	info.primDeviceSubCategory = 1;
	strcpy(info.manufacturer, STA_DEVINFO_MANUFACTURER);
	strcpy(info.modelName, STA_DEVINFO_MODEL_NAME);
	strcpy(info.modelNumber, STA_DEVINFO_MODEL_NO);
	strcpy(info.serialNumber, STA_DEVINFO_SERIAL_NO);
	info.configMethods = 0x008C;
	info.authTypeFlags = 0x003f;
	info.encrTypeFlags = 0x000f;
	info.connTypeFlags = 0x01;
	info.rfBand = 1;
	info.osVersion = 0x80000000;
	info.featureId = 0x80000000;

	wps_enr_config_init(&info);
}

brcm_wpscli_status wpscli_start_enroll_device(const uint8 *bssid, const char *pin)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	uint32 len;
	char *sendBuf;
	time_t now = wpscli_current_time();

	TUTRACE((TUTRACE_INFO, "wpscli_start_enroll_device: Entered.\n"));

	// Call hooks of wps engine
	wps_osl_init(NULL);

	wpscli_sta_config_init(g_ctxWpscliOsi.sta_mac);

	// WPS initialization
	wps_start_enrollment((char *)pin, wpscli_current_time());

	/*
	 * start the process by sending the eapol start. Created from the
	 * Enrollee SM Initialize.
	 */
    TUTRACE((TUTRACE_INFO, "Sending EAPOL Start\n"));
	len = wps_get_msg_to_send(&sendBuf, (uint32)now);
	if (sendBuf) {
		// Send down wps message (eapol packet) via packet dispatcher
		status = wpscli_sta_eap_send_data_down(sendBuf, len);
	}
	else {
		TUTRACE((TUTRACE_INFO, "system not initialized\n"));
		/* this means the system is not initialized properly */
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_start_enroll_device: Exiting.\n"));
	return status;
}

brcm_wpscli_status wpscli_convert_ap_list_format(uint32 *nAP)
{
    wl_scan_results_t *list = (wl_scan_results_t*)g_ScanResult;
    wl_bss_info_t *bi;
    uint i, wps_ap_count = 0;
    wl_bss_info_107_t *old_bi_107;

	// Convert APs in the list from wl_bss_info_t format to wps_ap_list_info_t format
	// We need wps_ap_list_info_t format in order to call wps engine/common code to filter out
	// wps APs

    memset(ap_list, 0, sizeof(ap_list));

#ifdef  LEGACY2_WL_BSS_INFO_VERSION
    if (list->version != WL_BSS_INFO_VERSION && list->version != LEGACY_WL_BSS_INFO_VERSION && list->version != LEGACY2_WL_BSS_INFO_VERSION) 
#else
    if (list->version != WL_BSS_INFO_VERSION && list->version != LEGACY_WL_BSS_INFO_VERSION) 
#endif
	{
        fprintf(stderr, 
				"Sorry, your driver has bss_info_version %d but this program supports only version %d.\n",
				list->version, 
				WL_BSS_INFO_VERSION);
        return WPS_STATUS_NOT_ENOUGH_MEMORY;
    }

	TUTRACE((TUTRACE_INFO, "wpscli_convert_ap_list_format: convert %d bssinfo\n", list->count));

    bi = list->bss_info;
    for (i = 0; i < list->count; i++) {
		TUTRACE((TUTRACE_INFO, "wpscli_convert_ap_list_format: convert bssinfo entry %d... \n", i));

        // Convert version LEGACY_WL_BSS_INFO_VERSION(107) to WL_BSS_INFO_VERSION(109)
		// NO need to convert version LEGACY2_WL_BSS_INFO_VERSION(108) to WL_BSS_INFO_VERSION(109) as 109 is compatible to 108
        if (bi->version == LEGACY_WL_BSS_INFO_VERSION) {
			TUTRACE((TUTRACE_INFO, "wpscli_convert_ap_list_format:(entry %d), version=LEGACY_WL_BSS_INFO_VERSION\n", i));

            old_bi_107 = (wl_bss_info_107_t *)bi;
            bi->chanspec = CH20MHZ_CHSPEC(old_bi_107->channel);
            bi->ie_length = old_bi_107->ie_length;
            bi->ie_offset = sizeof(wl_bss_info_107_t);
        }    
		TUTRACE((TUTRACE_INFO, "wpscli_convert_ap_list_format:(entry %d), ie_length=%d\n", i, bi->ie_length));

        if (bi->ie_length) {
			if(wps_ap_count < WPS_MAX_AP_SCAN_LIST_LEN) {
				ap_list[wps_ap_count].used = TRUE;
				memcpy(ap_list[wps_ap_count].BSSID, bi->BSSID.octet, 6);
				strncpy((char *)ap_list[wps_ap_count].ssid,(char *)bi->SSID,bi->SSID_len);
				ap_list[wps_ap_count].ssid[bi->SSID_len] = '\0';
				ap_list[wps_ap_count].ssidLen= bi->SSID_len;
				ap_list[wps_ap_count].ie_buf = (uint8 *)(((uint8 *)bi) + bi->ie_offset);
				ap_list[wps_ap_count].ie_buflen = bi->ie_length;
				
				// "channel" and "band" are determined this way on Vista. We should really revisit back to if how other OS
				// determin the information (for example, XP is using a different way). This part could be in OS layer
				ap_list[wps_ap_count].channel = bi->ctl_ch;
				ap_list[wps_ap_count].band = (bi->ctl_ch <= CH_MAX_2G_CHANNEL) ? WL_CHANSPEC_BAND_2G : WL_CHANSPEC_BAND_5G;  // Use channel number to decide band

				ap_list[wps_ap_count].wep = bi->capability & DOT11_CAP_PRIVACY;
				wps_ap_count++;
			}
		}
		bi = (wl_bss_info_t*)((int8*)bi + bi->length);
    }
	
	*nAP = wps_ap_count;
	
	TUTRACE((TUTRACE_INFO, "Exit wpscli_convert_ap_list_format:wps_ap_count=%d\n", wps_ap_count));

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_create_wps_ap_list(int *nAP)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	wps_ap_list_info_t *wpsaplist = ap_list;  // Point to the same AP list memory allocatoin

	if(nAP == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	TUTRACE((TUTRACE_INFO, "Enter wpscli_create_wps_ap_list\n"));

	// Get all scanned APs first
	status = wpscli_convert_ap_list_format((uint32 *)nAP);
	if(status == WPS_STATUS_SUCCESS)
	{
		if(*nAP > 0) {
			TUTRACE((TUTRACE_INFO, "wpscli_create_wps_ap_list: wpscli_convert_ap_list_format returns %d AP\n", *nAP));

			// After this call, the first wps_ap_total elements in the ap_list are wps ones
			*nAP = wps_get_aplist(wpsaplist, wpsaplist);  
			TUTRACE((TUTRACE_INFO, "wpscli_create_wps_ap_list: wps_get_aplist returns %d AP\n", *nAP));

			status = *nAP > 0? WPS_STATUS_SUCCESS : WPS_STATUS_WLAN_NO_WPS_AP_FOUND;
		}
		else {
			status = WPS_STATUS_WLAN_NO_ANY_AP_FOUND;
		}
	}

	return status;
}

int wpscli_on_evt_eap_pkt_received(const char *data, uint32 data_len, uint8 *last_sent_msg)
{
	uint32 retVal = WPS_CONT;
	char *sendBuf;
	int len;
	unsigned long now;
	int last_recv_msg;

	TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: Entered. data_len=%d.\n", data_len));

	if(data == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: Exiting. data passed in is NULL.\n"));
		return WPS_CONT;
	}

	/* process ap message */

	// Expect to return a PIN error
	retVal = wps_process_ap_msg((char *)data, data_len);

	TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: wps_process_ap_msg returns. data_len=%d retVal=%d.\n", data_len, retVal));

	now = wpscli_current_time();

	/* check return code to do more things */

	if (retVal == WPS_SEND_MSG_CONT ||
		retVal == WPS_SEND_MSG_SUCCESS ||
		retVal == WPS_SEND_MSG_ERROR) {
		len = wps_get_msg_to_send(&sendBuf, now);
		TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: wps_get_msg_to_send returns data length %d.\n", len));

		if (sendBuf) {
			// wps_eap_send_msg will create the packet and send it out via hook function
			wps_eap_send_msg(sendBuf, len);

			// Send back sending message type
			*last_sent_msg = wps_get_sent_msg_id();
			TUTRACE((TUTRACE_INFO, "Waiting -> Sent packet %s\n", wps_get_msg_string(*last_sent_msg)));

		}

		/* over-write retVal */
		if (retVal == WPS_SEND_MSG_SUCCESS)
			// The return code "retVal" from wps common code is written poorly however WPS_SEND_MSG_SUCCESS here means that 
			// the whole WPS is finished and successful. We should revisit the WPS common code when there is chance
			// to check why it returns WPS_SEND_MSG_SUCCESS, instead of WPS_SUCCESS when the whole WPS process is completed successful
			retVal = WPS_SUCCESS;  
		else if (retVal == WPS_SEND_MSG_ERROR)
			retVal = REG_FAILURE;
		else
			retVal = WPS_CONT;
	}
	else if (retVal == EAP_FAILURE) {
		TUTRACE((TUTRACE_INFO, "Received an eap failure from registrar\n"));

		/* we received an eap failure from registrar */
		/*
		 * check if this is coming AFTER the protocol passed the M2
		 * mark or is the end of the discovery after M2D.
		 */
		last_recv_msg = wps_get_recv_msg_id();
		TUTRACE((TUTRACE_INFO, "Received eap failure, last recv msg EAP-Request%s\n", wps_get_msg_string(last_recv_msg)));
		if (last_recv_msg > WPS_ID_MESSAGE_M2D) {
			return REG_FAILURE;
        }
		else
			return WPS_CONT;
	}
	/* special case, without doing wps_eap_create_pkt */
	else if (retVal == WPS_SEND_MSG_IDRESP) {
		// Sending EAP-Response/Identity
		len = wps_get_msg_to_send(&sendBuf, now);
		if (sendBuf) {
			TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: Sending EAP-Response/Identity. sendBuf=%d\n", sendBuf));

			TUTRACE((TUTRACE_INFO, "Waiting -> Sending IDENTITY Resp packet\n"));
			wpscli_sta_eap_send_data_down(sendBuf, len);

			// Sending back IDENTITY repsonse packet
			*last_sent_msg = BRCM_WPS_MSGTYPE_IDENTITY;  // Identity
		}
	}
	else if(retVal==WPS_ERR_ADDCLIENT_PINFAIL || retVal==WPS_ERR_CONFIGAP_PINFAIL)
	{
		TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: Exiting. Wrong PIN. retVal=%d.\n", retVal));
		return retVal;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_on_evt_eap_pkt_received: Exiting. retVal=%d.\n", retVal));
	return retVal;
}

int wpscli_on_evt_no_eap_pkt(uint8 *msg_type)
{
	uint32 retVal;
	uint32 len;
	char *sendBuf;
	unsigned long now;
	int state;

	TUTRACE((TUTRACE_INFO, "Timeout while waiting for a msg from AP\n"));

	/* check eap receive timer. It might be time to re-transmit */

	/*
	 * Do we need this API ? We could just count how many
	 * times we re-transmit right here.
	 */
	now = wpscli_current_time();
	retVal = wps_eap_check_timer(now);
	if (retVal == WPS_SEND_MSG_CONT) {
        TUTRACE((TUTRACE_INFO, "Trying sending msg again\n"));
		
		len = wps_get_retrans_msg_to_send(&sendBuf, now, (char *)msg_type);
		if (sendBuf) {
			TUTRACE((TUTRACE_INFO, "Trying sending msg again\n"));

			// wps_get_retrans_msg_to_send will return a message type only if "eap state" is beyond PROCESSING_PROTOCOL
			// For other cases, we cover this based on "eap state"
			state = wps_get_eap_state();
			if (state == EAPOL_START_SENT)
			{
				TUTRACE((TUTRACE_INFO, "Re-Send EAPOL-Start\n"));
				*msg_type = BRCM_WPS_MSGTYPE_EAPOL_START; 
			}
			else if (state == EAP_IDENTITY_SENT)
			{
				TUTRACE((TUTRACE_INFO, "Re-Send EAP-Response / Identity\n"));
				*msg_type = BRCM_WPS_MSGTYPE_IDENTITY;
			}
			else
			{
				// Other EAP messages 
				TUTRACE((TUTRACE_INFO, "Re-Send EAP-Response%s\n", wps_get_msg_string((int)(*msg_type))));
			}

			wpscli_sta_eap_send_data_down(sendBuf, len);
			TUTRACE((TUTRACE_INFO, "EAP-Response sent out\n"));
		}
	}
	/* re-transmission count exceeded as wps_eap_check_timer also indicates a timeout, give up */
	else if (retVal == EAP_TIMEOUT) {
		retVal = REG_FAILURE;

/*
		// EAP timeout, we will decide to continue or simply fail the process

		TUTRACE((TUTRACE_INFO, "Re-transmission count exceeded, wait again\n"));

		last_recv_msg = wps_get_recv_msg_id();

		if (last_recv_msg == WPS_ID_MESSAGE_M2D) {
			TUTRACE((TUTRACE_INFO, "M2D Wait timeout, again.\n"));
			return WPS_CONT;
		}
		else if (last_recv_msg > WPS_ID_MESSAGE_M2D) {
			last_sent_msg = wps_get_sent_msg_id();
			TUTRACE((TUTRACE_INFO, "Timeout, give up. Last recv/sent msg "
				"[EAP-Response%s/EAP-Request%s]\n",
				wps_get_msg_string(last_recv_msg),
				wps_get_msg_string(last_sent_msg)));
			return REG_FAILURE;
		}
		else {
			TUTRACE((TUTRACE_INFO, "Re-transmission count exceeded, again\n"));
			return REG_FAILURE;
		}
*/
	}
    else if(retVal == WPS_ERR_ADAPTER_NONEXISTED)
        return retVal;  // This is probably due to adapter being removed during wps negotiation

	return retVal;
}

brcm_wpscli_status wpscli_get_network_settings(const char *ssid, brcm_wpscli_nw_settings *nw_settings)
{
	WpsEnrCred cred;

	if(ssid == NULL || nw_settings == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	// Get network credentials from wps engine
	wps_get_credentials(&cred, ssid, strlen(ssid));
	
	// Convert and output network settings

	memset(nw_settings, 0, sizeof(brcm_wpscli_nw_settings));
	
	// ssid
	memcpy(nw_settings->ssid, cred.ssid, cred.ssidLen);
	
	// Network passphrase
	memcpy(nw_settings->nwKey, cred.nwKey, cred.nwKeyLen);

	// Authentication method
	if(strcmp(cred.keyMgmt, KEY_MGMT_OPEN) == 0)
		nw_settings->authType = BRCM_WPS_AUTHTYPE_OPEN;
	else if(strcmp(cred.keyMgmt, KEY_MGMT_SHARED) == 0)
		nw_settings->authType = BRCM_WPS_AUTHTYPE_SHARED;
	else if(strcmp(cred.keyMgmt, KEY_MGMT_WPAPSK) == 0 || strcmp(cred.keyMgmt, KEY_MGMT_WPAPSK_WPA2PSK) == 0)
		nw_settings->authType = BRCM_WPS_AUTHTYPE_WPAPSK;
	else if(strcmp(cred.keyMgmt, KEY_MGMT_WPA2PSK) == 0)
		nw_settings->authType = BRCM_WPS_AUTHTYPE_WPA2PSK;
	else
		return WPS_STATUS_PROTOCOL_FAIL_UNEXPECTED_NW_CRED;

	// Wep key index
	nw_settings->wepIndex = cred.wepIndex;

	// Encryption method
	if(cred.encrType & WPS_ENCRTYPE_NONE)
		nw_settings->encrType = BRCM_WPS_ENCRTYPE_NONE;
	else if(cred.encrType & WPS_ENCRTYPE_WEP)
		nw_settings->encrType = BRCM_WPS_ENCRTYPE_WEP;
	else if(cred.encrType & WPS_ENCRTYPE_TKIP)
		nw_settings->encrType = BRCM_WPS_ENCRTYPE_TKIP;
	else if(cred.encrType & WPS_ENCRTYPE_AES)
		nw_settings->encrType = BRCM_WPS_ENCRTYPE_AES;
	else
		return WPS_STATUS_PROTOCOL_FAIL_UNEXPECTED_NW_CRED;

	return WPS_STATUS_SUCCESS;
}
// Should be used on STA side only. It will take an EAP data frame and attach
// ether header before passing it down.
brcm_wpscli_status wpscli_sta_eap_send_data_down(char *dataBuffer, uint32 dataLen)
{
	brcm_wpscli_status status;
	char local_buf[WPS_EAP_DATA_MAX_LENGTH] = { 0 };
	eapol_header_t *eapol = (eapol_header_t *)local_buf;

    TUTRACE((TUTRACE_INFO, "wpscli_sta_eap_send_data_down: Entered. dataLen=%d\n", dataLen));

    if ((!dataBuffer) || (!dataLen)) {
	    TUTRACE((TUTRACE_ERR, "wpscli_sta_eap_send_data_down: Exiting. Invalid NULL parameters passed in \n"));
	    return WPS_STATUS_INVALID_NULL_PARAM;
    }

	memcpy(eapol->eth.ether_shost, g_ctxWpscliOsi.sta_mac, ETHER_ADDR_LEN);
	memcpy(eapol->eth.ether_dhost, g_ctxWpscliOsi.bssid, ETHER_ADDR_LEN);
	eapol->eth.ether_type = wpscli_htons(ETHER_TYPE_802_1X);

	// Copy over the EAP data
	memcpy(&(eapol->version), dataBuffer, dataLen);
	dataLen += sizeof(eapol->eth);

	// wpscli_pktdisp_send_packet expects eapol packets with ether header
	status = wpscli_pktdisp_send_packet((char *)eapol, dataLen);

    TUTRACE((TUTRACE_ERR, "wpscli_sta_eap_send_data_down: Exiting. status=%d\n", status));
	return status;
}
