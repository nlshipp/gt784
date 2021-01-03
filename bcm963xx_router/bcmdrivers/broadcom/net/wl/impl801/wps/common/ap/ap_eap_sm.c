/*
 * WPS AP eap state machine
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: ap_eap_sm.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <stdio.h>
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

#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>

#ifdef WIN32
#include <windows.h>
#endif

#include <tutrace.h>

#include <slist.h>

#include <wps_wl.h>
#include <wps_apapi.h>
#include <ap_eap_sm.h>

#if defined(TARGETOS_nucleus)
uint16 ntohs(uint16 netshort);
#endif /* TARGETOS_nucleus */

static unsigned char* lookupSta(unsigned char *sta_mac, sta_lookup_mode_t mode);
static void cleanUpSta(int reason, int deauthFlag);
static void ap_eap_sm_sendEapol(char *eap_data, unsigned short eap_len);
static void ap_eap_sm_sendFailure(void);
static int ap_eap_sm_req_start(void);
static int ap_eap_sm_process_protocol(char *eap_msg);
static int ap_eap_sm_sta_validate(char *eapol_msg);
static uint32 ap_eap_sm_create_pkt(char * dataBuffer, int dataLen, uint8 eapCode);


static EAP_WPS_AP_STATE s_apEapState;
static EAP_WPS_AP_STATE *apEapState = NULL;
static char zero_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#define WPS_STA_ACTIVE()	memcmp(&apEapState->sta_mac, zero_mac, ETHER_ADDR_LEN)
#define WPS_DATA_OFFSET	(EAPOL_HEADER_LEN + sizeof(WpsEapHdr))

#define WPS_MSGTYPE_OFFSET  9

/*
 * Search for or create a STA.
 * We can only handle only ONE station at a time.
 * If `enter' is not set, do not create it when one is not found.
 */
static unsigned char*
lookupSta(unsigned char *sta_mac, sta_lookup_mode_t mode)
{
	unsigned char *mac = NULL;
	int sta_active = WPS_STA_ACTIVE();

	/* Search if the entry in on the list */
	if (sta_active &&
		(!memcmp(&apEapState->sta_mac, sta_mac, ETHER_ADDR_LEN))) {
		mac = apEapState->sta_mac;
	}

	/* create a new entry if necessary */
	if (!mac && mode == SEARCH_ENTER) {
		if (!sta_active) {
			/* Initialize entry */
			memcpy(&apEapState->sta_mac, sta_mac, ETHER_ADDR_LEN);
			/* Initial STA state: */
			apEapState->state = INIT;
			apEapState->eap_id = 0;
			mac = apEapState->sta_mac;
		}
		else
			TUTRACE((TUTRACE_ERR,  "Sta in use\n"));
	}

	return mac;
}

static void
cleanUpSta(int reason, int deauthFlag)
{
	if (!WPS_STA_ACTIVE()) {
		TUTRACE((TUTRACE_ERR,  "cleanUpSta: Not a good sta\n"));
		return;
	}

	if (deauthFlag)
		wps_deauthenticate(apEapState->bssid, apEapState->sta_mac, reason);

	memset(&apEapState->sta_mac, 0, ETHER_ADDR_LEN);
	apEapState->sent_msg_id = 0;
	apEapState->recv_msg_id = 0;

	TUTRACE((TUTRACE_ERR,  "cleanup STA Finish\n"));

	return;
}


/* Send a EAPOL packet */
static void
ap_eap_sm_sendEapol(char *eap_data, unsigned short eap_len)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) apEapState->msg_to_send;
	int sendLen;

	memcpy(&eapolHdr->eth.ether_dhost, &apEapState->sta_mac, ETHER_ADDR_LEN);
	memcpy(&eapolHdr->eth.ether_shost, &apEapState->bssid, ETHER_ADDR_LEN);
	eapolHdr->eth.ether_type = WpsHtons(ETHER_TYPE_802_1X);
	eapolHdr->version = WPA_EAPOL_VERSION;
	eapolHdr->type = EAP_PACKET;
	eapolHdr->length = WpsHtons(eap_len);

	memcpy(eapolHdr->body, eap_data, eap_len);
	sendLen = EAPOL_HEADER_LEN + eap_len;

	/* increase send_count */
	apEapState->send_count ++;

	if (apEapState->send_data)
		(*apEapState->send_data)(apEapState->msg_to_send, sendLen);

	apEapState->last_sent_len = sendLen;
	return;
}

static void
ap_eap_sm_sendFailure(void)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *) eapolHdr->body;

	TUTRACE((TUTRACE_ERR,  "EAP: Building EAP-Failure (id=%d)\n", apEapState->eap_id));

	memset(wpsEapHdr, 0, sizeof(WpsEapHdr));
	wpsEapHdr->code = EAP_CODE_FAILURE;
	wpsEapHdr->id = apEapState->eap_id;
	/* Only send code, id and length, otherwise the DTM testing will have trouble */
	wpsEapHdr->length = WpsHtons(4);

	ap_eap_sm_sendEapol((char *)wpsEapHdr, 4);
	apEapState->state = EAP_START_SEND;

	return;
}

static int
ap_eap_sm_req_start(void)
{
	/* reset msg_len */
	apEapState->msg_len = sizeof(apEapState->msg_to_send) - WPS_DATA_OFFSET;
	return wps_processMsg(apEapState->mc_dev, NULL, 0,
		&apEapState->msg_to_send[WPS_DATA_OFFSET],
		(uint32 *)&apEapState->msg_len, TRANSPORT_TYPE_EAP);
}


int
ap_eap_sm_check_timer(uint32 time)
{
	int mode;

	if (!apEapState) {
		return WPS_CONT;
	}

	if (apEapState->send_count != 0) {
		/* update current time */
		apEapState->last_check_time = time;

		/* reset send_count */
		apEapState->send_count = 0;
		return WPS_CONT;
	}

	/* Check every 5 seconds if send count is 0 */
	if ((time - apEapState->last_check_time) < 5)
		return WPS_CONT;

	/* update current time */
	apEapState->last_check_time = time;

	/* Timed out for waiting NACK, send EAP-Fail, and close */
	if (apEapState->sent_msg_id == WPS_ID_MESSAGE_NACK) {
		/* send failure */
		ap_eap_sm_Failure(0);

		TUTRACE((TUTRACE_ERR, "Wait NACK time out!\n"));
		return EAP_TIMEOUT;
	}

	/* Process based on mode */
	mode = wps_get_mode(apEapState->mc_dev);
	if (mode == EModeUnconfAp) {
		if (apEapState->sent_msg_id == WPS_ID_MESSAGE_M1) {
			/* Timed-out, send eap failure */		
			if (apEapState->resent_count >= 2) {
				ap_eap_sm_Failure(0);

				TUTRACE((TUTRACE_ERR, "Resend over 2 times, timed-out\n"));
				return EAP_TIMEOUT;
			}

			/* Resend M1 */
			if (apEapState->send_data)
				(*apEapState->send_data)(apEapState->msg_to_send,
					apEapState->last_sent_len);

			apEapState->resent_count++;

			TUTRACE((TUTRACE_ERR, "Resend previous EAPOL packet %d bytes\n",
				apEapState->last_sent_len));

			/* Continue EModeUnconfAp state machine */
		}
	}
	else if (mode == EModeApProxy) {
		/* Check proxy now */
		if (apEapState->recv_msg_id == WPS_ID_MESSAGE_M1 ||
			(apEapState->sent_msg_id != 0 &&
			apEapState->sent_msg_id <= WPS_ID_MESSAGE_M2D)) {

			ap_eap_sm_Failure(0);

			TUTRACE((TUTRACE_ERR, "EModeApProxy M2/M2D timeout!\n"));
			/* Continue EModeApProxy state machine */
		}
	}
	else if (mode == EModeApProxyRegistrar) {
	}

	return WPS_CONT;
}


static int
ap_eap_sm_process_protocol(char *eap_msg)
{
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;
	char *in_data = (char *)(wpsEapHdr + 1);
	uint32 in_len = ntohs(wpsEapHdr->length) - sizeof(WpsEapHdr);

	/* record the in message id, ingore WPS_Start */
	if (wpsEapHdr->opcode != WPS_Start)
		apEapState->recv_msg_id = in_data[WPS_MSGTYPE_OFFSET];

	/* reset msg_len */
	apEapState->msg_len = sizeof(apEapState->msg_to_send) - WPS_DATA_OFFSET;
	return wps_processMsg(apEapState->mc_dev, in_data, in_len,
		&apEapState->msg_to_send[WPS_DATA_OFFSET], (uint32 *)&apEapState->msg_len,
		TRANSPORT_TYPE_EAP);
}

static int
ap_eap_sm_sta_validate(char *eapol_msg)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) eapol_msg;
	WpsEapHdr * wpsEapHdr = (WpsEapHdr *) eapolHdr->body;
	unsigned char *mac;

	/* Ignore non 802.1x EAP BRCM packets (dont error) */
	if (ntohs(eapolHdr->eth.ether_type) != ETHER_TYPE_802_1X)
		return WPS_ERR_GENERIC;

	/* Ignore EAPOL_LOGOFF */
	if (eapolHdr->type == EAPOL_LOGOFF) {
		return WPS_CONT;
	}

	if ((ntohs(eapolHdr->length) <= EAP_HEADER_LEN) ||
		(eapolHdr->type != EAP_PACKET) ||
		(ntohs(eapolHdr->length) <= EAP_HEADER_LEN) ||
		((wpsEapHdr->type != EAP_EXPANDED) && (wpsEapHdr->type != EAP_IDENTITY))) {
		return WPS_ERR_GENERIC;
	}

	mac = lookupSta(eapolHdr->eth.ether_shost, SEARCH_ONLY);
	if ((wpsEapHdr->type == EAP_IDENTITY) && mac) {
		TUTRACE((TUTRACE_ERR,  "EAP_IDENTITY comes again...\n"));
		return WPS_CONT;
	}

	mac = lookupSta(eapolHdr->eth.ether_shost, SEARCH_ENTER);
	if (!mac) {
		TUTRACE((TUTRACE_ERR,  "no sta...\n"));
		return WPS_ERR_GENERIC;
	}

	return WPS_SUCCESS;
}

static uint32
ap_eap_sm_create_pkt(char * dataBuffer, int dataLen, uint8 eapCode)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eapolHdr->body;

	wpsEapHdr->code = eapCode;
	wpsEapHdr->id = apEapState->eap_id;
	wpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr) + dataLen);
	wpsEapHdr->type = EAP_TYPE_WPS;
	wpsEapHdr->vendorId[0] = WPS_VENDORID1;
	wpsEapHdr->vendorId[1] = WPS_VENDORID2;
	wpsEapHdr->vendorId[2] = WPS_VENDORID3;
	wpsEapHdr->vendorType = WpsHtonl(WPS_VENDORTYPE);

	if (dataBuffer) {
		if (dataBuffer[WPS_MSGTYPE_OFFSET] >= WPS_ID_MESSAGE_M1 &&
			dataBuffer[WPS_MSGTYPE_OFFSET] <= WPS_ID_MESSAGE_M8) {
			wpsEapHdr->opcode = WPS_MSG;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_ACK) {
			wpsEapHdr->opcode = WPS_ACK;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_NACK) {
			wpsEapHdr->opcode = WPS_NACK;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_DONE) {
			wpsEapHdr->opcode = WPS_Done;
		}
		else {
			TUTRACE((TUTRACE_ERR, "Unknown Message Type code %d; "
				"Not sending msg\n", dataBuffer[WPS_MSGTYPE_OFFSET]));
			return TREAP_ERR_SENDRECV;
		}

		/* record the out message id */
		apEapState->sent_msg_id = dataBuffer[WPS_MSGTYPE_OFFSET];

		/* TBD: Flags are always set to zero for now, if message is too big */
		/*
		 * fragmentation must be done here and flags will have some bits set
		 * and message length field is added
		 */
		wpsEapHdr->flags = 0;

		/* in case we passed this buffer to the protocol, do not copy :-) */
		if (dataBuffer != (char *)(wpsEapHdr + 1)) {
			memcpy((char *)wpsEapHdr + sizeof(WpsEapHdr), dataBuffer, dataLen);
		}
	}

	return WPS_SUCCESS;
}

uint32
ap_eap_sm_process_sta_msg(char *msg, int msg_len)
{
	eapol_header_t *eapolHdr = NULL;
	WpsEapHdr * wpsEapHdr = NULL;
	int ret, len;

	/* Check initialization */
	if (apEapState == 0)
		return WPS_MESSAGE_PROCESSING_ERROR;

	if (apEapState->parse_msg)
		eapolHdr = (eapol_header_t *) (*apEapState->parse_msg)(msg, msg_len, &len);

	if (!eapolHdr) {
		TUTRACE((TUTRACE_ERR,  "Missing EAPOL header\n"));
		return WPS_ERR_GENERIC;
	}

	/* validate eapol message */
	if ((ret = ap_eap_sm_sta_validate((char*)eapolHdr)) != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR,  "Vaildation failed...\n"));
		return ret;
	}

	wpsEapHdr = (WpsEapHdr *) eapolHdr->body;
	switch (apEapState->state) {
		case INIT :
			if (wpsEapHdr->type != EAP_TYPE_IDENTITY)
				return WPS_ERR_GENERIC;
			if (wpsEapHdr->id != apEapState->eap_id ||
				wpsEapHdr->code != EAP_CODE_RESPONSE) {
				TUTRACE((TUTRACE_ERR,  "bogus EAP packet id %d code %d, "
					"expected %d\n", wpsEapHdr->id, wpsEapHdr->code,
					apEapState->eap_id));
				cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
				return WPS_ERR_GENERIC;
			}

			TUTRACE((TUTRACE_ERR,  "Response, Identity, eap code=%d, id = %d, "
				"length=%d, type=%d\n", wpsEapHdr->code, wpsEapHdr->id,
				ntohs(wpsEapHdr->length), wpsEapHdr->type));
			/* reset counter  */
			apEapState->eap_id = 1;
			/* store whcih if sta come from */
			memcpy(&apEapState->bssid, &eapolHdr->eth.ether_dhost, ETHER_ADDR_LEN);
			/* Request Start message */
			ret = ap_eap_sm_req_start();
			return ret;

		case EAP_START_SEND:
		case PROCESSING_PROTOCOL:
			if (wpsEapHdr->code != EAP_CODE_RESPONSE)
				return WPS_ERR_GENERIC;

			apEapState->eap_id++;
			ret = ap_eap_sm_process_protocol((char *)wpsEapHdr);
			return ret;

		case REG_SUCCESSFUL:
			return REG_SUCCESSFUL;
	}

	return WPS_CONT;
}

int
ap_eap_sm_startWPSReg(unsigned char *sta_mac, unsigned char *ap_mac)
{
	unsigned char *mac;
	int retVal;

	mac = lookupSta(sta_mac, SEARCH_ENTER);

	if (!mac) {
		TUTRACE((TUTRACE_ERR,  "no sta...\n"));
		return -1;
	}

	TUTRACE((TUTRACE_ERR,  "Build WPS Start!\n"));

	/* reset counter  */
	apEapState->eap_id = 1;
	/* store whcih if sta come from */
	memcpy(&apEapState->bssid, ap_mac, ETHER_ADDR_LEN);

	/* Request Start message */
	retVal = ap_eap_sm_req_start();

	return retVal;
}

uint32
ap_eap_sm_init(void *mc_dev, char *mac_sta, char * (*parse_msg)(char *, int, int *),
	unsigned int (*send_data)(char *, uint32))
{
	TUTRACE((TUTRACE_INFO,  "Initial...\n"));

	memset(&s_apEapState, 0, sizeof(EAP_WPS_AP_STATE));
	apEapState = &s_apEapState;
	apEapState->mc_dev = mc_dev;
	memcpy(apEapState->sta_mac, mac_sta, ETHER_ADDR_LEN);
	if (parse_msg)
		apEapState->parse_msg = parse_msg;
	if (send_data)
		apEapState->send_data = send_data;

	return WPS_SUCCESS;
}

uint32
ap_eap_sm_deinit()
{
	if (!apEapState) {
		TUTRACE((TUTRACE_ERR, "Not initialized; Returning\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (WPS_STA_ACTIVE()) {
		cleanUpSta(DOT11_RC_8021X_AUTH_FAIL, 1);
	}

	return WPS_SUCCESS;
}

unsigned char *
ap_eap_sm_getMac(int mode)
{
	if ((mode != EModeUnconfAp) && WPS_STA_ACTIVE())
		return (apEapState->sta_mac);
	else
		return (wps_get_mac_income(apEapState->mc_dev));

	/* return NULL;	unreachable */
}

uint32
ap_eap_sm_sendMsg(char * dataBuffer, uint32 dataLen)
{
	eapol_header_t *eapolHdr = (eapol_header_t *) apEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *) eapolHdr->body;
	uint32 retVal;

	TUTRACE((TUTRACE_INFO, "In ap_eap_sm_sendMsg buffer Length = %d\n",
		dataLen));

	retVal = ap_eap_sm_create_pkt(dataBuffer, dataLen, EAP_CODE_REQUEST);
	if (retVal == WPS_SUCCESS) {
		ap_eap_sm_sendEapol((char *)wpsEapHdr, WpsHtons(wpsEapHdr->length));
		apEapState->state = PROCESSING_PROTOCOL;
	}
	else {
		TUTRACE((TUTRACE_ERR,  "Send EAP FAILURE to station!\n"));
		ap_eap_sm_sendFailure();
		cleanUpSta(0, 0);
		apEapState->state = EAP_FAILURED;
		retVal = TREAP_ERR_SENDRECV;
	}

	return retVal;
}

uint32
ap_eap_sm_sendWPSStart()
{
	WpsEapHdr wpsEapHdr;

	/* check sta status */
	if (!WPS_STA_ACTIVE()) {
		TUTRACE((TUTRACE_ERR,  "sta not in use!\n"));
		return WPS_ERROR_MSG_TIMEOUT;
	}

	wpsEapHdr.code = EAP_CODE_REQUEST;
	wpsEapHdr.id = apEapState->eap_id;
	wpsEapHdr.length = WpsHtons(sizeof(WpsEapHdr));
	wpsEapHdr.type = EAP_TYPE_WPS;
	wpsEapHdr.vendorId[0] = WPS_VENDORID1;
	wpsEapHdr.vendorId[1] = WPS_VENDORID2;
	wpsEapHdr.vendorId[2] = WPS_VENDORID3;
	wpsEapHdr.vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr.opcode = WPS_Start;
	wpsEapHdr.flags = 0;

	ap_eap_sm_sendEapol((char *)&wpsEapHdr, sizeof(WpsEapHdr));
	apEapState->state = EAP_START_SEND;

	return WPS_SUCCESS;
}

int
ap_eap_sm_get_msg_to_send(char **data)
{
	if (!apEapState) {
		*data = NULL;
		return -1;
	}

	*data = &apEapState->msg_to_send[WPS_DATA_OFFSET];
	return apEapState->msg_len;
}

void
ap_eap_sm_Failure(int deauthFlag)
{
	apEapState->state = EAP_FAILURED;
	ap_eap_sm_sendFailure();

	/* Sleep a short time to avoid de-auth send before EAP-Failure */
	if (deauthFlag)
		WpsSleepMs(10);

	cleanUpSta(0, deauthFlag);
	return;
}

int
ap_eap_sm_get_sent_msg_id()
{
	if (!apEapState)
		return -1;

	return ((int)apEapState->sent_msg_id);
}

int
ap_eap_sm_get_recv_msg_id()
{
	if (!apEapState)
		return -1;

	return ((int)apEapState->recv_msg_id);
}

int
ap_eap_sm_get_eap_state()
{
	return ((int)apEapState->state);
}
