/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_softap.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement SoftAP side APIs in wpscli_api.h 
 *
 */
// Include wpscli library OSL header file
#include "wpscli_osl.h"

// Include common OSI header file
#include "wpscli_common.h"

// Include WPS core common header files
// This set of wps core common #include header files need be in this order because of their inter-dependency
#include <typedefs.h>
#include <wpstypes.h>
#include <tutrace.h>
#include <wpstlvbase.h>
#include <wps_devinfo.h>
#include <bcmcrypto/bn.h>
#include <bcmcrypto/dh.h>
#include <sminfo.h>
#include <wps_devinfo.h>
#include <reg_prototlv.h>
#include <reg_proto.h>
#include <proto/eapol.h>
#include <wps_wl.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <wps_ap.h>
#include <wps_apapi.h>
#if defined(UNDER_CE)
#include <proto/eap.h>
#else
#include <eap.h>
#endif
#include <ap_eap_sm.h>
#include <wps_enr_osl.h>
#include <ethernet.h>

// Variable length of AP device information in WPS IE fields. Try to shortern 
// string length as device driver needs WPS IE size to be as small as possible
#define AP_DEVINFO_MANUFACTURER		"Broadcom"
#define AP_DEVINFO_MODEL_NAME		"SoftAP"
#define AP_DEVINFO_MODEL_NO			"0"
#define AP_DEVINFO_SERIAL_NO		"0"

// External function declarations
extern BOOL wpscli_set_wl_prov_svc_ie(unsigned char *p_data, int length, unsigned int cmdtype);
extern BOOL wpscli_del_wl_prov_svc_ie(unsigned int cmdtype);
extern BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype);
extern BOOL wpscli_del_wps_ie(unsigned int frametype);
extern void wpscli_update_status(brcm_wpscli_status status, brcm_wpscli_status_cb_data cb_data);
extern void upnp_device_uuid(unsigned char *uuid);

// Local function declarations 
static brcm_wpscli_status wpscli_softap_add_wps_ie(void);
static int wpscli_softap_readConfigure(wpsap_wksp_t *bcmwps, devcfg *ap_devcfg, WpsEnrCred *credential);
static void wpscli_softap_cleanup_wps(void);
static void wpscli_softap_session_end(int opcode);
static brcm_wpscli_status wpscli_convert_nw_settings(const brcm_wpscli_nw_settings *nw_settings, WpsEnrCred *wps_cred);
static brcm_wpscli_status wpscli_softap_sm_init(WpsEnrCred *pNwCred, EMode ap_wps_mode, const uint8 *sta_mac);
static brcm_wpscli_status wpscli_softap_init_wps(brcm_wpscli_wps_mode wps_mode, 
	 										     brcm_wpscli_pwd_type pwd_type,
												 const char *pin_code, 
												 const brcm_wpscli_nw_settings *nw_settings, 
												 uint32 time_out);
static int wpscli_softap_process_eap_msg(char *eapol_msg, uint32 len);
static int wpscli_softap_process_sta_eapol_start_msg(char *sta_buf, uint8 *sta_mac);
static void wpscli_softap_bcmwps_add_pktcnt(wpsap_wksp_t *bcmwps);
static void wps_pb_update_pushtime(char *mac);
static int wps_pb_check_pushtime(unsigned long time);
static void wps_pb_remove_sta_from_mon_checker(const uint8 *mac);

// Local variables
static WPSAPI_T *g_mc = NULL;
static wpsap_wksp_t *g_bcmwps = NULL;
static char gDeviceName[SIZE_32_BYTES+1];
static time_t g_tStartTime;
static BOOL g_bWpsEnabled = FALSE;
static BOOL g_bWpsSessionOpen;
static BOOL g_bAbortMonitor;

// Constant definitions
const char ZERO_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const char *WpsEapEnrIdentity =  "WFA-SimpleConfig-Enrollee-1-0";
const char *WpsEapRegIdentity =  "WFA-SimpleConfig-Registrar-1-0";

enum { WPS_EAP_ID_ENROLLEE = 0,	WPS_EAP_ID_REGISTRAR, WPS_EAP_ID_NONE };

// PBC Monitoring Checker
#define PBC_OVERLAP_CNT			2
#define PBC_MONITOR_TIME		120		/* in seconds */

typedef struct {
	unsigned char	mac[6];
	unsigned int	last_time;
} PBC_STA_INFO;


static PBC_STA_INFO pbc_info[PBC_OVERLAP_CNT];

brcm_wpscli_status wpscli_softap_open()
{
	TUTRACE((TUTRACE_INFO, "wpscli_softap_open: Entered.\n"));

	TUTRACE((TUTRACE_INFO, "wpscli_softap_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
};

brcm_wpscli_status wpscli_softap_advertise_no_8021x()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	unsigned char ie_no8021x[1] = { 0x00 };

	TUTRACE((TUTRACE_INFO, "wpscli_softap_advertise_no_8021x: Entered.\n"));
	
	wpscli_set_wl_prov_svc_ie(ie_no8021x, 1, WPS_IE_TYPE_SET_BEACON_IE);	
	wpscli_set_wl_prov_svc_ie(ie_no8021x, 1, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);	

	TUTRACE((TUTRACE_INFO, "wpscli_softap_advertise_no_8021x: Exiting.\n"));
	return status;
}

brcm_wpscli_status wpscli_softap_cleanup_wl_prov_svc()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wl_prov_svc: Entered.\n"));
	
	wpscli_del_wl_prov_svc_ie(WPS_IE_TYPE_SET_BEACON_IE);	
	wpscli_del_wl_prov_svc_ie(WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);	

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wl_prov_svc: Exiting.\n"));
	return status;
}

// Enable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_enable_wps(const char *deviceName)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_enable_wps: Entered (devName=%s).\n", deviceName));

	strncpy(gDeviceName, deviceName, sizeof(gDeviceName));
	wpscli_softap_advertise_no_8021x();

	// Add and broadcast WPS IE
	status = wpscli_softap_add_wps_ie();
	if(status != WPS_STATUS_SUCCESS) {
		 TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_enable_wps: Failed to set WPS IE.\n"));
	}

	g_bWpsEnabled = TRUE;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_enable_wps: Exiting. status=%d, gDeviceName=%s\n", 
			status, gDeviceName));
	return status;
}

// Disable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_disable_wps()
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: Entered.\n"));

	if(g_bWpsEnabled) {
		wpscli_softap_cleanup_wl_prov_svc();

		// Remove wps IEs
		wpscli_softap_cleanup_wps();
		g_bWpsEnabled = FALSE;
	}
	else {
		TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: WPS was not enabled.\n"));
	}

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

void wpscli_deinit_wps_engine()
{
	TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: Entered.\n"));

	// De-initialize wps engine
	if(g_mc) {
		wps_deinit(g_mc);
		g_mc = NULL;
	}
	else {
		TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: NULL g_mc.\n"));
	}

	TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: Exiting.\n"));
}

brcm_wpscli_status brcm_wpscli_softap_close_session() 
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_close_session: Entered.\n"));
	
	wpscli_deinit_wps_engine();

	if(g_bWpsEnabled) {
		// Restore WPS IE as it will be modified during wps negotiation
		wpscli_softap_add_wps_ie();
	}

	g_bWpsSessionOpen = FALSE;

	// Send a status update to indicate the end of a WPS handshake */
	wpscli_update_status(WPS_STATUS_PROTOCOL_END_EXCHANGE, NULL);

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_close_session: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status brcm_wpscli_softap_is_wps_window_open()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	if(g_bWpsSessionOpen) {
		if((wpscli_current_time() - g_tStartTime) < g_ctxWpscliOsi.time_out) {
			status = WPS_STATUS_SUCCESS;
		}
		else {
			g_bWpsSessionOpen = FALSE;
			status = WPS_STATUS_PROTOCOL_FAIL_TIMEOUT;
		}
	}
	else {
		status = WPS_STATUS_WINDOW_NOT_OPEN;
	}

	return status;
}

static uint32 wpscli_get_eap_wps_id(char *buf)
{
	uint32 eap_id = WPS_EAP_ID_NONE;
	eapol_header_t *eapol = (eapol_header_t *)buf;
	eap_header_t *eap = (eap_header_t *)eapol->body;

	TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Entered.\n"));

	if (eap->type != EAP_IDENTITY) {
		TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Exiting. Not an EAP Idenity message.\n"));
		return WPS_EAP_ID_NONE;
	}

	if(memcmp((char *)eap->data, REGISTRAR_ID_STRING, strlen(REGISTRAR_ID_STRING)) == 0)
		eap_id = WPS_EAP_ID_REGISTRAR;
	else if(memcmp((char *)eap->data, ENROLLEE_ID_STRING, strlen(ENROLLEE_ID_STRING)) == 0)
		eap_id = WPS_EAP_ID_ENROLLEE;
	else
		eap_id = WPS_EAP_ID_NONE;

	TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Exiting. eap_id=%d\n", eap_id));
	return eap_id;
}

brcm_wpscli_status brcm_wpscli_softap_start_eap_monitoring(const brcm_wpscli_nw_settings *nw_conf, const char* dev_pin)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	char buf[WPS_EAP_DATA_MAX_LENGTH];
	uint32 buf_len = WPS_EAP_DATA_MAX_LENGTH;
	int retVal = 0;
	BOOL bIDRespProcessed = FALSE;
	BOOL bSessionContinue = FALSE;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_eap_monitoring: Entered.\n"));

	if(nw_conf == NULL) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_eap_monitoring: Exiting. Invalid NULL nw_conf parameter passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if(dev_pin == NULL) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_eap_monitoring: Exiting. Invalid NULL dev_pin parameter passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	g_bAbortMonitor = FALSE;

	// Open packet dispatcher
	status = wpscli_pktdisp_open((const uint8 *)ZERO_MAC);
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_eap_monitoring: Exiting. Failed to open packet dispatcher.\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	// Eap monitoring loop
	TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_eap_monitoring: Start polling for EAP packets.\n"));
	while(1)
	{
		eapol_header_t *eapol = NULL;
		eap_header_t *eap;
		uint32 wps_id;

		// Waiting for packet
		buf_len = WPS_EAP_DATA_MAX_LENGTH;
		status = wpscli_pktdisp_wait_for_packet(buf, &buf_len, WPSCLI_MONITORING_POLL_TIMEOUT, TRUE);

		// Exit if there is abort request
		if(g_bAbortMonitor) {
			g_bAbortMonitor = FALSE;
			status = WPS_STATUS_ABORTED;
			break;
		}

		if(status != WPS_STATUS_SUCCESS) 
			continue;  // No eapol packet recieved, continue to wait

		// An eapol packet is received
		eapol = (eapol_header_t *)buf;
		switch(eapol->type)
		{
		case EAPOL_START:  // EAPOL-START packet received
			// Set peer sta mac address
			wpscli_set_peer_addr(eapol->eth.ether_shost);
			memcpy(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6);

			// Stop WPS engine/session if it has been started previously
			if(g_mc)
				wpscli_deinit_wps_engine();
			
			wpscli_softap_init_wps(BRCM_WPS_MODE_STA_ER_CONFIG_NW, BRCM_WPS_PWD_TYPE_PIN, dev_pin, nw_conf, 120);

			bIDRespProcessed = FALSE;
			bSessionContinue = TRUE;  // Start processing only after EAPOL-START is received
			wpscli_softap_process_sta_eapol_start_msg((char*)eapol, eapol->eth.ether_shost);
			break;
		case EAP_PACKET:
			if(!bSessionContinue)
				break;  // Ignore as wps session has not started yet

			eap = (eap_header_t *)eapol->body;
			if(eap->type == EAP_IDENTITY) {
				// STA EAP Identity Response is received. Process differently depending on whether this is first 
				// STA Identity Response or not/re-tran

				// Get EAP Identity ID
				wps_id = wpscli_get_eap_wps_id(buf);
				if(wps_id == WPS_EAP_ID_REGISTRAR)
					bSessionContinue = TRUE;
				else {
					// If not WPS external registrar request, stop wps session and don't process eap packet any more
					bSessionContinue = FALSE;
					continue;
				}

				if(!bIDRespProcessed) {
					char *send_buf;
					int send_buf_len;

					// First Identity Response, process this way

					bIDRespProcessed = TRUE;
					
					// For the first identity response, we need to start sm by calling ap_eap_sm_startWPSReg. Also as the
					// mode is EModeUnconfAP, we need send message here as ap_eap_sm_startWPSReg will not send message
					// from inside of it
					retVal = ap_eap_sm_startWPSReg(eapol->eth.ether_shost, g_ctxWpscliOsi.bssid);

					if (retVal == WPS_CONT || retVal == WPS_SEND_MSG_CONT) {
						wpscli_softap_bcmwps_add_pktcnt(g_bcmwps);
						if (retVal == WPS_SEND_MSG_CONT) {
							send_buf_len = ap_eap_sm_get_msg_to_send(&send_buf);
							if (send_buf_len >= 0)
								ap_eap_sm_sendMsg(send_buf, send_buf_len);
						}
					}
				}
				else {
					// Not first Identity Response, it is re-tran so process the other way
					retVal = wpscli_softap_process_eap_msg((char*)eapol, eapol->length);
				}
			}
			else {
				if(g_mc) 
					retVal = wpscli_softap_process_eap_msg((char*)eapol, eapol->length);
			}

			// check the return code and close this session if needed
			if((retVal == WPS_SUCCESS) || 
			   (retVal == WPS_MESSAGE_PROCESSING_ERROR) || 
			   (retVal == WPS_ERR_ADDCLIENT_PINFAIL) || 
			   (retVal == WPS_ERR_CONFIGAP_PINFAIL) || 
			   (retVal == WPS_ERR_GENERIC)) {
				// Map wps engine error code to wpscli status code
				switch(retVal)
				{
				case WPS_SUCCESS:
					status = WPS_STATUS_SUCCESS;
					break;
				case WPS_MESSAGE_PROCESSING_ERROR:
					status = WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG;
					break;
				case WPS_ERR_ADDCLIENT_PINFAIL:
				case WPS_ERR_CONFIGAP_PINFAIL:
					status = WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN;
					break;
				case WPS_ERR_GENERIC:
				default:
					status = WPS_STATUS_SYSTEM_ERR;
				}
				goto END;  // End current wps session
			}
			else {
				// Continue for other situation
				continue;
			}

			/* break;	unreachable */
		default:
			;  // Ignore any other packet
		}  // Switch
	};  // While(1)

END:
	brcm_wpscli_softap_close_session();
	wpscli_pktdisp_close();

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_eap_monitoring: Exiting. status=%d\n", status));
	return status;
}

brcm_wpscli_status brcm_wpscli_softap_abort_eap_monitoring()
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_abort_eap_monitoring: Entered.\n"));

	g_bAbortMonitor = TRUE;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_abort_eap_monitoring: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

// SoftAP starts WPS registration negotiation to enroll STA device. 
brcm_wpscli_status brcm_wpscli_softap_start_wps(brcm_wpscli_wps_mode wps_mode, 
												brcm_wpscli_pwd_type pwd_type,
												const char *pin_code, 
												const brcm_wpscli_nw_settings *nw_settings, 
												uint32 time_out,
												uint8 *peer_addr)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	char buf[WPS_EAP_DATA_MAX_LENGTH];
	uint32 buf_len = WPS_EAP_DATA_MAX_LENGTH;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: Entered(ssid=%s).\n", nw_settings->ssid));

	// Reset library context and global veriables
	g_tStartTime = wpscli_current_time();  // Get the start time
	g_bWpsSessionOpen = TRUE;
	g_ctxWpscliOsi.time_out = time_out;  // Fill time out value
	g_bRequestAbort = FALSE;
	memcpy(g_ctxWpscliOsi.sta_mac, ZERO_MAC, 6);
	strncpy(gDeviceName, nw_settings->ssid, sizeof(gDeviceName));
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: set gDeviceName=%s\n", gDeviceName));

	// Open packet dispatcher
	status = wpscli_pktdisp_open((const uint8 *)ZERO_MAC);
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_wps: Exiting. Failed to open packet dispatcher\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}
	
	// Initialize wps session
	if(g_mc)
		wpscli_deinit_wps_engine();

	// Initialize WPS session and advertize WPS modes
	status = wpscli_softap_init_wps(wps_mode, pwd_type, pin_code, nw_settings, time_out);
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_wps: Failed to initialize wps session.\n"));
		goto END;
	}

	//
	// Start registration loop to add enrollee
	//
	status = WPS_STATUS_PROTOCOL_FAIL_TIMEOUT;
	while((wpscli_current_time()-g_tStartTime) < time_out) 
	{

		buf_len = WPS_EAP_DATA_MAX_LENGTH;  // Reset buffer length
		status = wpscli_pktdisp_wait_for_packet(buf, &buf_len, WPSCLI_GENERIC_POLL_TIMEOUT, TRUE);

		// Exit if there is abort request
		if(g_bRequestAbort) {
			g_bRequestAbort = FALSE;
			status = WPS_STATUS_ABORTED;
			break;
		}

		if(status == WPS_STATUS_SUCCESS) 
		{
			// An eapol packet is received
			int retVal;
			eapol_header_t *eapol = (eapol_header_t *)buf;
			eap_header_t *eap;
			static BOOL bIDRespProcessed = FALSE;

			// Process eapol packet
			if(eapol->type == EAPOL_START)
			{
				// EAPOL-START received. Begine a new WPS registration process.

				if(memcmp(g_ctxWpscliOsi.sta_mac, ZERO_MAC, 6) == 0) {
					// STA mac is not set, so this is first eapol-start. Set STA mac
					memcpy(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6);
					wpscli_set_peer_addr(eapol->eth.ether_shost);  // Set peer mac to osl
					
					// Return mac of STA requesting WPS
					if(peer_addr)
						memcpy(peer_addr, eapol->eth.ether_shost, 6);  
				}
				else {
					// STA mac is already set, so this is not first eapol-start meaning that
					// wps session starts already. Verify if it is from the same STA requesting WPS
					if(memcmp(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6) == 0) {
						// From same STA, this is re-send of eapol-start. Re-start WPS engine sm
						wpscli_deinit_wps_engine();  // Stop the current running sm
						
						// Start engine sm again
						status = wpscli_softap_init_wps(wps_mode, pwd_type, pin_code, nw_settings, time_out);
						if(status != WPS_STATUS_SUCCESS) {
							TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_wps: Failed to initialize wps session.\n"));
							break;
						}
					}
					else {
						// EAPOL_START from different STA, ignore it as there is already one in progress
						continue;
					}

				}

				// Will send identity request to the peer station
				wpscli_softap_process_sta_eapol_start_msg((char*)eapol, eapol->eth.ether_shost); 

				retVal = WPS_CONT;
				bIDRespProcessed = FALSE;  
			}
			else {
				// EAP_PACKET received

				// If g_ctxWpscliOsi.sta_mac is not set, that means WPS sm is not started yet.
				// Ignore the EAP packet
				if(memcmp(g_ctxWpscliOsi.sta_mac, ZERO_MAC, 6) == 0) 
					continue;

				// Verify if the EAP packet is from the same STA requesting WPS
				// If it is, process the packet, otherwise ignore it.
				if(memcmp(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6) != 0) 
					continue;

				eap = (eap_header_t *)eapol->body;
				if(eap->type == EAP_IDENTITY) {
					// STA EAP Identity Response is received. Process differently depending on whether this is first 
					// STA Identity Response or not/re-tran
					if(!bIDRespProcessed) {
						// First Identity Response, process this way. Noting when the mode is not EModeUnconfAp, "EAP-START" packet
						// will be sent out from inside ap_eap_sm_startWPSReg. This is different way from that of processing other 
						// EAP packet, where we need to call ap_eap_sm_get_msg_to_send and ap_eap_sm_sendMsg to send out a packet.
						bIDRespProcessed = TRUE;
						retVal = ap_eap_sm_startWPSReg(g_ctxWpscliOsi.sta_mac, g_ctxWpscliOsi.bssid);  // Start WPS registrar
					}
					else {
						// Not first IDResp, it is re-tran and process the other way as registrar is already started
						retVal = wpscli_softap_process_eap_msg((char*)eapol, buf_len);
					}
				}
				else {
					retVal = wpscli_softap_process_eap_msg((char*)eapol, buf_len);
				}
			}

			// check the return code and close this session if needed
			if((retVal == WPS_SUCCESS) || 
			   (retVal == WPS_MESSAGE_PROCESSING_ERROR) || 
			   (retVal == WPS_ERR_ADDCLIENT_PINFAIL) || 
			   (retVal == WPS_ERR_CONFIGAP_PINFAIL) || 
			   (retVal == WPS_ERR_GENERIC)) 
			{
				// Map wps engine error code to wpscli status code
				switch(retVal)
				{
				case WPS_SUCCESS:
					status = WPS_STATUS_SUCCESS;
					
					if(g_ctxWpscliOsi.pwd_type == BRCM_WPS_PWD_TYPE_PBC) {
						// Remove the station from PBC monitor checker
						wps_pb_remove_sta_from_mon_checker(g_ctxWpscliOsi.sta_mac);
					}
					break;
				case WPS_MESSAGE_PROCESSING_ERROR:
					status = WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG;
					break;
				case WPS_ERR_ADDCLIENT_PINFAIL:
				case WPS_ERR_CONFIGAP_PINFAIL:
					status = WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN;
					break;
				case WPS_ERR_GENERIC:
				default:
					status = WPS_STATUS_SYSTEM_ERR;
				}

				wpscli_softap_session_end(retVal);
				break;
			}
		}
	}

END:
	brcm_wpscli_softap_close_session();
	wpscli_pktdisp_close();

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: Exiting. status=%d\n", status));
	return status;
}

static brcm_wpscli_status wpscli_softap_init_wps(brcm_wpscli_wps_mode wps_mode, 
											     brcm_wpscli_pwd_type pwd_type,
												 const char *pin_code, 
												 const brcm_wpscli_nw_settings *nw_settings, 
												 uint32 time_out)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	WpsEnrCred credAP;
	uint32 uRet = WPS_SUCCESS;
	EMode e_mode;
	unsigned long now;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_init_wps: Entered. wps_mode=%d\n", wps_mode));

	//
	// Validate input parameters
	//
	if(nw_settings == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. network setting is NULL.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	// PBC session overlap check
	now = wpscli_current_time();
	if(pwd_type == BRCM_WPS_PWD_TYPE_PBC) {
		if(wps_pb_check_pushtime(now) > 1) {
			// Session overlap detected. Don't start registrar at all
			TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. STA PBC session overlap detected.\n"));
			return WPS_STATUS_PROTOCOL_FAIL_OVERLAP;
		}
	}

	// Convert wps mode from wpscli format to wps engine format
	if(wps_mode == BRCM_WPS_MODE_STA_ER_JOIN_NW || wps_mode == BRCM_WPS_MODE_STA_ER_CONFIG_NW) {
		// EModeUnconfAp really means AP is running as Enrollee. EModeUnconfAp does NOT necessarily 
		// mean AP is in "Unconfigured" state
		e_mode = EModeUnconfAp;  
	} 
	else if(wps_mode == BRCM_WPS_MODE_STA_ENR_JOIN_NW) {
		e_mode = EModeApProxyRegistrar;
	}
	else {
		// No support to other modes, e.g. AP Proxy mode
		e_mode = EModeUnknown;
	}

	// Convert network setting format to be used by wps engine
	memset(&credAP, 0, sizeof(credAP));
	if((status = wpscli_convert_nw_settings(nw_settings, &credAP)) != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. network setting has wrong format.\n"));
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	// Initialize WPS on SoftAP side and will prepare the data for wps_start_ap_registration 
	status = wpscli_softap_sm_init(&credAP, e_mode, (const uint8 *)ZERO_MAC);
	if(status != WPS_STATUS_SUCCESS || g_mc == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. wps_softap_init failed.\n"));
		return WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	// Remove beacon IE set previously as wps_start_ap_registration will reset beacon IE 
	// as well as probe response IE internally as well
	wpscli_softap_cleanup_wps();

	// Start registration and set WPS Probe Resp IE in wps_start_ap_registration
	// Also via wps_start_ap_registration/wps_initReg, the "g_mc" used in many 
	// places of AP code will be filled with data too 
	if(pwd_type == BRCM_WPS_PWD_TYPE_PBC)
		uRet = wps_start_ap_registration(g_mc, e_mode, WPS_DEFAULT_PIN, WPS_DEVICEPWDID_PUSH_BTN);
	else
		uRet = wps_start_ap_registration(g_mc, e_mode, (char *)pin_code, WPS_DEVICEPWDID_DEFAULT);

	if(uRet != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: wps_start_ap_registration failed. Return error %d.\n", uRet));
		return WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_init_wps: Exiting. Return status %d.\n", status));
	return status;
}

brcm_wpscli_status brcm_wpscli_get_overlap_mac(unsigned char *mac)
{
	int i;
	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (pbc_info[i].last_time != 0) {
			TUTRACE((TUTRACE_INFO, "PBC overlap with %02x:%02x:%02x:%02x:%02x:%02x\n",
				pbc_info[i].mac[0], pbc_info[i].mac[1],
				pbc_info[i].mac[2], pbc_info[i].mac[3],
				pbc_info[i].mac[4], pbc_info[i].mac[5]));
			memcpy(mac, pbc_info[i].mac, sizeof(pbc_info[i].mac));
			return WPS_STATUS_SUCCESS;
		}
	}
	return WPS_STATUS_SYSTEM_ERR;
}

static brcm_wpscli_status wpscli_convert_nw_settings(const brcm_wpscli_nw_settings *nw_settings, WpsEnrCred *wps_cred)
{
	if(wps_cred == NULL || nw_settings == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	// ssid
	wps_cred->ssidLen = strlen(nw_settings->ssid);
	memcpy(wps_cred->ssid, nw_settings->ssid, wps_cred->ssidLen);

	
	// network passphrase/key
	wps_cred->nwKeyLen = strlen(nw_settings->nwKey);
	memcpy(wps_cred->nwKey, nw_settings->nwKey, wps_cred->nwKeyLen);

	
	// key management
	switch(nw_settings->authType)
	{
	case BRCM_WPS_AUTHTYPE_OPEN:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_OPEN);
		break;
	case BRCM_WPS_AUTHTYPE_SHARED:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_SHARED);
		break;
	case BRCM_WPS_AUTHTYPE_WPAPSK:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_WPAPSK);
		break;
	case BRCM_WPS_AUTHTYPE_WPA2PSK:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_WPA2PSK);
		break;
	default:
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	// Wep key index
	wps_cred->wepIndex = nw_settings->wepIndex;

	// Convert encryption method in wps engine format
	switch(nw_settings->encrType)
	{
	case BRCM_WPS_ENCRTYPE_NONE:
		wps_cred->encrType = WPS_ENCRTYPE_NONE;
		break;
	case BRCM_WPS_ENCRTYPE_WEP:
		wps_cred->encrType = WPS_ENCRTYPE_WEP;
		break;
	case BRCM_WPS_ENCRTYPE_TKIP:
		wps_cred->encrType = WPS_ENCRTYPE_TKIP;
		break;
	case BRCM_WPS_ENCRTYPE_AES:
		wps_cred->encrType = WPS_ENCRTYPE_AES;
		break;
	default:
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	return WPS_STATUS_SUCCESS;
}

static void wpscli_softap_bcmwps_add_pktcnt(wpsap_wksp_t *bcmwps)
{
	if (bcmwps)
		bcmwps->pkt_count++;
}

static void wpscli_softap_session_end(int opcode)
{
	if (opcode == WPS_STATUS_SUCCESS) {
		TlvEsM7Enr *p_tlvEncr;

		// Delete encrypted settings that were sent from SM
		p_tlvEncr = (TlvEsM7Enr *)g_mc->mp_regSM->mp_peerEncrSettings;
		if (!p_tlvEncr) {
			TUTRACE((TUTRACE_ERR, "Peer Encr Settings not exist\n"));
			return;
		}
		reg_msg_m7enr_del(p_tlvEncr, 0);

		wps_setProcessStates(WPS_OK);
	}
	else {
		wps_setProcessStates(WPS_MSG_ERR);
	}
}

// Take STA EAPOL-Start packet and send out EAP-Request/Identity packet
static int wpscli_softap_process_sta_eapol_start_msg(char *sta_buf, uint8 *sta_mac)
{
	int retVal;
	int size;
	uint32 data_len;
	char tx_buf[(sizeof(eapol_header_t) - 1) + EAP_HEADER_LEN + 1];

	//
	// Create AP EAP Request/Identity packet
	//
	eapol_header_t *eapol = (eapol_header_t *)tx_buf;
	eap_header_t *eap = (eap_header_t *)eapol->body;
	eapol_header_t *sta_eapol = (eapol_header_t *)sta_buf;

	// Send a status update to indicate the start of a WPS handshake */
	wpscli_update_status(WPS_STATUS_PROTOCOL_START_EXCHANGE, NULL);

	// Fill eapol header
	memcpy(&eapol->eth.ether_dhost, sta_mac, ETHER_ADDR_LEN);
	memcpy(&eapol->eth.ether_shost, g_ctxWpscliOsi.bssid, ETHER_ADDR_LEN);

	eapol->eth.ether_type = wpscli_htons(ETHER_TYPE_802_1X);
	eapol->version = sta_eapol->version;
	eapol->type = EAP_PACKET;
	size = EAP_HEADER_LEN + 1;  // No eap data but eap type is needed
	eapol->length = wpscli_htons(size);  

	// Fill eap header
	eap->code = EAP_REQUEST;  // 1-EAP Request  2-EAP Response
	eap->id = 0;  // Start from 0
	eap->length = eapol->length;
	eap->type = EAP_IDENTITY;  // Identity

	// Set eapol length
	data_len = (sizeof(*eapol) - 1) + size;

	retVal = wpsap_osl_eapol_send_data((char *)eapol, data_len);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_sta_eapol_start_msg: Exiting. retVal=%d\n", retVal));
	return retVal;
}

static int wpscli_softap_process_eap_msg(char *eapol_msg, uint32 len)
{
	uint32 retVal;
	int send_len;
	char *sendBuf;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: Entered.\n"));

	retVal = ap_eap_sm_process_sta_msg(eapol_msg, len);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: ap_eap_sm_process_sta_msg returned %d.\n", retVal));

	/* update packet count */
	if (retVal == WPS_CONT || retVal == WPS_SEND_MSG_CONT)
		wpscli_softap_bcmwps_add_pktcnt(g_bcmwps);

	/* check return code to do more things */
	if (retVal == WPS_SEND_MSG_CONT ||
		retVal == WPS_SEND_MSG_SUCCESS ||
		retVal == WPS_SEND_MSG_ERROR ||
		retVal == WPS_ERR_ADDCLIENT_PINFAIL ||
		retVal == WPS_ERR_CONFIGAP_PINFAIL) {
		send_len = ap_eap_sm_get_msg_to_send(&sendBuf);
		if (send_len >= 0)
			ap_eap_sm_sendMsg(sendBuf, send_len);

		/* WPS_SUCCESS or WPS_MESSAGE_PROCESSING_ERROR case */
		if (retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_ADDCLIENT_PINFAIL ||
			retVal == WPS_ERR_CONFIGAP_PINFAIL) {
			ap_eap_sm_Failure(0);
		}

		/* over-write retVal */
		switch(retVal)
		{
		case WPS_SEND_MSG_SUCCESS:
			retVal = WPS_SUCCESS;
			break;
		case WPS_SEND_MSG_ERROR:
			retVal = WPS_MESSAGE_PROCESSING_ERROR;
			break;
		case WPS_ERR_CONFIGAP_PINFAIL:
		case WPS_ERR_ADDCLIENT_PINFAIL:
			break;  // No need to overwrite
		default:
			retVal = WPS_CONT;  // For all other cases, continue wps negotiation
		}
	}
	/* other error case */
	else if (retVal != WPS_CONT) {
		ap_eap_sm_Failure(0);
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: Exiting.\n"));
	return retVal;
}

void wpscli_softap_cleanup_wps(void)
{
	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wps: Entered.\n"));

	wpscli_del_wps_ie(WPS_IE_TYPE_SET_BEACON_IE);
	wpscli_del_wps_ie(WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wps: Exiting.\n"));
}

static int encode_beacon_wps_ie(BufferObj *bufObj)
{
	uint8 data8;

	// Version
	data8 = 0x10;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	// Simple Config State
	data8 = WPS_SCSTATE_CONFIGURED;  // Assume the SoftAP configured already
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);
	
	return bufObj->m_dataLength;
}

int wpscli_softap_encode_beacon_wps_ie(char *buf, int maxLength)
{
	BufferObj *bufObj = NULL;
	int length;

	bufObj = buffobj_new();
	length = encode_beacon_wps_ie(bufObj);
	if (length && length <= maxLength)
		memcpy(buf, bufObj->pBase, length);
	buffobj_del(bufObj);
	return length;
}

static int encode_probe_request_wps_ie(BufferObj *bufObj, uint16 cfgMeths,
	uint16 primDevCategory, uint16 primDevSubCategory, uint16 passwId,
	char *deviceName, uint16 reqCategoryId, uint16 reqSubCategoryId)
{
	uint8 data8;
	uint16 data16;
	DevInfo ap_devinfo;
	CTlvPrimDeviceType tlvPrimDeviceType;
	CTlvReqDeviceType tlvReqDeviceType;

	/* Version */
	data8 = 0x10;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Request Type */
	data8 = 0;
	tlv_serialize(WPS_ID_REQ_TYPE, bufObj, &data8, WPS_ID_REQ_TYPE_S);

	/* Config Methods */
	data16 = cfgMeths;
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	/* UUID */
	upnp_device_uuid(ap_devinfo.uuid);
	tlv_serialize(WPS_ID_UUID_E, bufObj, ap_devinfo.uuid, 16);

	// Primary Device Type. This is a complex TLV, so will be handled differently
	tlvPrimDeviceType.categoryId = primDevCategory;  // Device category
	tlvPrimDeviceType.oui = 0x0050F204;  // Device OUI
	tlvPrimDeviceType.subCategoryId = primDevSubCategory;  // Device sub category
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* RF Bands */
	data8 = 0;
	tlv_serialize(WPS_ID_RF_BAND, bufObj, &data8, WPS_ID_RF_BAND_S);

	/* Association State */
	data16 = 0;
	tlv_serialize(WPS_ID_ASSOC_STATE, bufObj, &data16, WPS_ID_ASSOC_STATE_S);

	/* Configuration Error */
	data16 = 0;
	tlv_serialize(WPS_ID_CONFIG_ERROR, bufObj, &data16, WPS_ID_CONFIG_ERROR_S);

	/* Device Password ID */
	data16 = passwId;
	tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16, WPS_ID_DEVICE_PWD_ID_S);

	/* Device Name */
	data16= strlen(deviceName);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, deviceName, (uint16)data16);

	// Requested Device Type. This is a complex TLV, so will be handled differently
	if (reqCategoryId) {
		tlvReqDeviceType.categoryId = reqCategoryId;
		tlvReqDeviceType.oui = 0x0050F204;  // Device OUI
		tlvReqDeviceType.subCategoryId = reqSubCategoryId;
		tlv_reqDeviceTypeWrite(&tlvReqDeviceType, bufObj);
	}

	return bufObj->m_dataLength;
}

int wpscli_softap_encode_probe_request_wps_ie(char *buf, int maxLength,
	uint16 cfgMeths, uint16 primDevCategory, uint16 primDevSubCategory,
	uint16 passwId, char *deviceName, uint16 reqCategoryId, uint16 reqSubCategoryId)
{
	BufferObj *bufObj = NULL;
	int length;

	bufObj = buffobj_new();
	length = encode_probe_request_wps_ie(bufObj, cfgMeths, primDevCategory,
		primDevSubCategory, passwId, deviceName, reqCategoryId, reqSubCategoryId);
	if (length && length <= maxLength)
		memcpy(buf, bufObj->pBase, length);
	buffobj_del(bufObj);
	return length;
}


static int encode_probe_response_wps_ie(BufferObj *bufObj,
	uint16 primDevCategory, uint16 primDevSubCategory, uint16 passwdId,
	char *deviceName)
{
	uint8 data8;
	uint16 data16, data16_s;
	DevInfo ap_devinfo;
	CTlvPrimDeviceType tlvPrimDeviceType;

	// Version
	data8 = 0x10;
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	// Simple Config State
	data8 = WPS_SCSTATE_CONFIGURED;  // Assume the SoftAP configured already
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	// Response Type
	data8 = WPS_MSGTYPE_AP_WLAN_MGR;
	tlv_serialize(WPS_ID_RESP_TYPE, bufObj, &data8, WPS_ID_RESP_TYPE_S);

	// Unique UUID of AP
	upnp_device_uuid(ap_devinfo.uuid);
	tlv_serialize(WPS_ID_UUID_E, bufObj, ap_devinfo.uuid, 16);

	// Manufacturer
	strcpy(ap_devinfo.manufacturer, AP_DEVINFO_MANUFACTURER);
	data16_s = strlen(ap_devinfo.manufacturer);
	tlv_serialize(WPS_ID_MANUFACTURER, bufObj, ap_devinfo.manufacturer, (uint16)data16_s);

	// Model Name
	strcpy(ap_devinfo.modelName, AP_DEVINFO_MODEL_NAME);
	data16_s = strlen(ap_devinfo.modelName);
	tlv_serialize(WPS_ID_MODEL_NAME, bufObj, ap_devinfo.modelName, (uint16)data16_s);

	// Model Number
	strcpy(ap_devinfo.modelNumber, AP_DEVINFO_MODEL_NO);
	data16_s = strlen(ap_devinfo.modelNumber);
	tlv_serialize(WPS_ID_MODEL_NUMBER, bufObj, ap_devinfo.modelNumber, (uint16)data16_s);

	// Searial Number
	strcpy(ap_devinfo.serialNumber, AP_DEVINFO_SERIAL_NO);
	data16_s = strlen(ap_devinfo.serialNumber);
	tlv_serialize(WPS_ID_SERIAL_NUM, bufObj, ap_devinfo.serialNumber, (uint16)data16_s);

	// Primary Device Type. This is a complex TLV, so will be handled differently
	tlvPrimDeviceType.categoryId = primDevCategory;  // Device category
	tlvPrimDeviceType.oui = 0x0050F204;  // Device OUI
	tlvPrimDeviceType.subCategoryId = primDevSubCategory;  // Device sub category
	tlv_primDeviceTypeWrite(&tlvPrimDeviceType, bufObj);

	/* Device Password ID */
	if (passwdId != 0) {
		data16 = passwdId;
		tlv_serialize(WPS_ID_DEVICE_PWD_ID, bufObj, &data16,
			WPS_ID_DEVICE_PWD_ID_S);
	}

	// Device Name
	strcpy(ap_devinfo.deviceName, deviceName);
	data16_s = strlen(ap_devinfo.deviceName);
	tlv_serialize(WPS_ID_DEVICE_NAME, bufObj, ap_devinfo.deviceName, (uint16)data16_s);

	// Config Methods
	data16 = (uint16)0x84;
	tlv_serialize(WPS_ID_CONFIG_METHODS, bufObj, &data16, WPS_ID_CONFIG_METHODS_S);

	return bufObj->m_dataLength;
}

int wpscli_softap_encode_probe_response_wps_ie(char *buf, int maxLength,
	uint16 primDevCategory, uint16 primDevSubCategory, uint16 passwdId,
	char *deviceName)
{
	BufferObj *bufObj = NULL;
	int length;

	bufObj = buffobj_new();
	length = encode_probe_response_wps_ie(bufObj, primDevCategory,
		primDevSubCategory, passwdId, deviceName);
	if (length && length <= maxLength)
		memcpy(buf, bufObj->pBase, length);
	buffobj_del(bufObj);
	return length;
}

static brcm_wpscli_status wpscli_softap_add_wps_ie(void)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	BOOL bRel;
	BufferObj *bufObj = NULL;
	BufferObj *bufObj1 = NULL;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Entered(gDeviceName=%s).\n", gDeviceName));

	// 
	// Set beacon WPS IE 
	//
	bufObj = buffobj_new();
	encode_beacon_wps_ie(bufObj);
	bRel = wpscli_set_wps_ie(bufObj->pBase, bufObj->m_dataLength, WPS_IE_TYPE_SET_BEACON_IE);
	buffobj_del(bufObj);  // destroy buffer object

	if(!bRel) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_add_wps_ie: Failed to add WPS IE to beacon.\n"));
		return WPS_STATUS_SET_WPS_IE_FAIL;
	}

	//
	// Set probe response WPS IE
	//
	bufObj1 = buffobj_new();
	encode_probe_response_wps_ie(bufObj1, 6, 1, 0, gDeviceName);
	
	// Set WPS probe response IE in driver
	bRel = wpscli_set_wps_ie(bufObj1->pBase, bufObj1->m_dataLength, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);
	
	buffobj_del(bufObj1);  // destroy buffer object

	if(!bRel) {
		TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Failed to add WPS IE to probe response.\n"));
		return WPS_STATUS_SET_WPS_IE_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Exiting.\n"));
	return status;
}

static brcm_wpscli_status wpscli_softap_sm_init(WpsEnrCred *pNwCred, 
											 EMode ap_wps_mode, 
											 const uint8 *sta_mac)
{
	wpsap_wksp_t *bcmwps = NULL;
	devcfg *ap_devcfg = NULL;
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "wps_softap_init: Entered.\n"));

	if(pNwCred == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Invalid NULL pNwCred parameter passed\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if(sta_mac == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Invalid NULL sta_mac parameter passed\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: init sta_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]));

	}

	// Initialize packet send/recv handlers which becomes to OS dependent osl.
	wps_osl_init(NULL);

	//
	// Fill in bcmwps information
	//
	bcmwps = (wpsap_wksp_t *)malloc(sizeof(wpsap_wksp_t));
	if (!bcmwps) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Can not allocate memory for wps structer...\n"));
		return WPS_STATUS_SYSTEM_ERR;
	}
	memset(bcmwps, 0, sizeof(wpsap_wksp_t));

	// Set SoftAP mac address
	if(g_ctxWpscliOsi.bssid)
		memcpy(bcmwps->mac_ap, g_ctxWpscliOsi.bssid, 6);  // Fill SoftAP mac address

	// Set STA mac address? Not existed yet. 
	memcpy(bcmwps->mac_sta, sta_mac, 6);  // Fill station mac address

	// Set WPS mode
	bcmwps->wps_mode = ap_wps_mode;

	// read device config here and pass to wps_init
	ap_devcfg = devcfg_new();
	if (wpscli_softap_readConfigure(bcmwps, ap_devcfg, pNwCred) != WPS_SUCCESS) {
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_init: wpsap_readConfigure fail...\n"));
		goto INIT_END;
	}

	ap_devcfg->e_mode = ap_wps_mode;  // Set AP wps mode
	if ((bcmwps->mc = (void*)wps_init(bcmwps, ap_devcfg, ap_devcfg->mp_deviceInfo)) == NULL) {
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_init: wps_init fail...\n"));
		goto INIT_END;
	}
	g_mc = bcmwps->mc;

	// ap_eap_sm_init should be called after devcfg_new and wps_init to get all "bcmwps"
	// struct members initialized properly
	if(ap_eap_sm_init(bcmwps->mc, (char *)bcmwps->mac_sta, wpsap_osl_eapol_parse_msg, wpsap_osl_eapol_send_data) != WPS_SUCCESS) {
		wpscli_deinit_wps_engine();  // De-initialize the wps engine

		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Exiting. ap_eap_sm_init fail...\n"));
		goto INIT_END;
	}

	g_bcmwps = bcmwps;

	TUTRACE((TUTRACE_INFO, "wps_softap_init: ap_eap_sm_init successful...\n"));

INIT_END:
	if(ap_devcfg) 
		devcfg_delete(ap_devcfg);

	TUTRACE((TUTRACE_INFO, "wps_softap_init: Exiting.\n"));
	return status;
}

static int wpscli_softap_readConfigure(wpsap_wksp_t *bcmwps, 
									   devcfg *ap_devcfg, 
									   WpsEnrCred *credential)
{
	int wps_mode = bcmwps->wps_mode;
	unsigned char *wl_mac = bcmwps->mac_ap;
	DevInfo *ap_devinfo = ap_devcfg->mp_deviceInfo;

		TUTRACE((TUTRACE_ERR, "Enter wpscli_softap_readConfigure\n"));

	if (!wl_mac) {
		TUTRACE((TUTRACE_ERR, "Error getting mac\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	if (wps_mode == EModeUnknown) {
		TUTRACE((TUTRACE_ERR, "Error getting wps config mode\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	//
	// Fill ap_devcfg object
	//
	ap_devcfg->e_mode = EModeApProxyRegistrar;
			
	/* Registrar enable */
	ap_devcfg->mb_regWireless = true;

	/* USE UPnP */
	ap_devcfg->mb_useUpnp = false;

	/* USB_KEY */
	ap_devcfg->mb_useUsbKey = false;
	
	/* nwKey */
	ap_devcfg->m_nwKeyLen = credential->nwKeyLen;
	
	/* Network Key */
	strncpy(ap_devcfg->m_nwKey, credential->nwKey, credential->nwKeyLen);
	
	/* Network key is set or not */
	ap_devcfg->mb_nwKeySet = true;

	//
	// Fill ap_devinfo object
	//
	/* Configure Mode */
	ap_devinfo->scState = WPS_SCSTATE_CONFIGURED;

	ap_devinfo->b_ap = true;

	/* UUID */
	upnp_device_uuid(ap_devinfo->uuid);

	/* Version */
	ap_devinfo->version = (uint8)0x10;

	/* Device Name */
	strcpy(ap_devinfo->deviceName, gDeviceName);

	/* Device category */
	ap_devinfo->primDeviceCategory = 6;

	/* Device OUI */
	ap_devinfo->primDeviceOui = 0x0050F204;

	/* Device sub category */
	ap_devinfo->primDeviceSubCategory = 1;

	/* Mac address */
	memcpy(ap_devinfo->macAddr, g_ctxWpscliOsi.bssid, SIZE_6_BYTES);

	/* MANUFACTURER */
	strcpy(ap_devinfo->manufacturer, AP_DEVINFO_MANUFACTURER);

	/* MODEL_NAME */
	strcpy(ap_devinfo->modelName, AP_DEVINFO_MODEL_NAME);

	/* MODEL_NUMBER */
	strcpy(ap_devinfo->modelNumber, AP_DEVINFO_MODEL_NO);

	/* SERIAL_NUMBER */
	strcpy(ap_devinfo->serialNumber, AP_DEVINFO_SERIAL_NO);

	/* CONFIG_METHODS */
	ap_devinfo->configMethods = (uint16)0x84;

	/* AUTH_TYPE_FLAGS */
	ap_devinfo->authTypeFlags = (uint16)0x27;

	/* ENCR_TYPE_FLAGS */
	ap_devinfo->encrTypeFlags = (uint16)0xf;

	/* CONN_TYPE_FLAGS */
	ap_devinfo->connTypeFlags = 1;

	/* RF Band */
	ap_devinfo->rfBand = 1;

	/* OS_VER */
	ap_devinfo->osVersion = 0x80000000;

	/* FEATURE_ID */
	 ap_devinfo->featureId = 0x80000000;

	/* Auth */
	ap_devinfo->auth = 0;

	/* SSID */
	memcpy(ap_devinfo->ssid, credential->ssid, SIZE_SSID_LENGTH);

	/* keyMgmt */
	memcpy(ap_devinfo->keyMgmt, credential->keyMgmt, SIZE_20_BYTES);

	/* crypto */
	ap_devinfo->crypto = credential->encrType;
	
	/* wep */
	if(credential->encrType & WPS_ENCRTYPE_WEP)
		ap_devinfo->wep = 1;
	else
		ap_devinfo->wep = 0;

	return WPS_SUCCESS;
}

// Update pbc monitor checker and return current number of requesting STAs within the 
// 2-minute monitoring windows time
static int wps_pb_check_pushtime(unsigned long time)
{
	int i;
	int PBC_sta = PBC_OVERLAP_CNT;

	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if ((time < pbc_info[i].last_time) || ((time - pbc_info[i].last_time) > PBC_MONITOR_TIME)) {
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));
		}

		if (pbc_info[i].last_time == 0)
			PBC_sta--;
	}

	return PBC_sta;
}

static void wps_pb_update_pushtime(char *mac)
{
	int i;
	unsigned long now;

	if(mac == NULL)
		return;

	now = wpscli_current_time();

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
}


static void wps_pb_remove_sta_from_mon_checker(const uint8 *mac)
{
	int i;

	// Remove the STA mac from monitor checker. We should remove the sta from PBC Monitor Checker
	// after the WPS session is completed successfully
	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (memcmp(mac, pbc_info[i].mac, 6) == 0) {
			// Remove the sta from monitor checker by zeroing out the element buffer
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));  
		}
	}
}

static int wps_eap_parse_prob_reqIE(unsigned char *mac, unsigned char *p_data, uint32 len)
{
	int ret_val = 0;
	WpsProbreqIE prReqIE;
	BufferObj *bufObj = buffobj_new();

	/* De-serialize this IE to get to the data */
	buffobj_dserial(bufObj, p_data, len);

	ret_val += tlv_dserialize(&prReqIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	ret_val += tlv_dserialize(&prReqIE.reqType, WPS_ID_REQ_TYPE, bufObj, 0, 0);
	ret_val +=  tlv_dserialize(&prReqIE.confMethods, WPS_ID_CONFIG_METHODS, bufObj, 0, 0);
	if (WPS_ID_UUID_E == buffobj_NextType(bufObj)) {
		ret_val += tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_E, bufObj, SIZE_16_BYTES, 0);
	}
	else if (WPS_ID_UUID_R == buffobj_NextType(bufObj)) {
		ret_val +=  tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_R, bufObj, SIZE_16_BYTES, 0);
	}

	/* Primary Device Type is a complex TLV - handle differently */
	ret_val += tlv_primDeviceTypeParse(&prReqIE.primDevType, bufObj);

	/* if any error, bail  */
	if (ret_val < 0) {
		buffobj_del(bufObj);
		return -1;
	}

	tlv_dserialize(&prReqIE.rfBand, WPS_ID_RF_BAND, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.assocState, WPS_ID_ASSOC_STATE, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.confErr, WPS_ID_CONFIG_ERROR, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	// STA requesting PBC is detected. Update the pb_info monitor checker
	if (prReqIE.pwdId.m_data == WPS_DEVICEPWDID_PUSH_BTN) {
		TUTRACE((TUTRACE_INFO, "\n\nPush Button sta detect\n\n"));
		wps_pb_update_pushtime((char *)mac);
	}

	buffobj_del(bufObj);
	return 0;
}

// STA probe request is received
brcm_wpscli_status brcm_wpscli_softap_on_sta_probreq_wpsie(
	const unsigned char *mac, const uint8 *databuf, uint32 datalen)
{
	if (mac == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	if(databuf == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

		// Process WPS IE and update PBC checker from inside wps_eap_parse_prob_reqIE
	if (wps_eap_parse_prob_reqIE((unsigned char *)mac,
		(unsigned char *)databuf, datalen))
		return WPS_STATUS_SYSTEM_ERR;
		
	return WPS_STATUS_SUCCESS;
}
