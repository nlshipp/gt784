/*
 * EAP WPS state machine specific to STA
 *
 * Can apply both to enrollee sta and wireless registrar.
 * This code is intended to be used on embedded platform on a mono-thread system,
 * using as litle resources as possible.
 * It is also trying to be as little system dependant as possible.
 * For that reason we use statically allocated memory.
 * This code IS NOT RE-ENTRANT.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: sta_eap_sm.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 */

#include <bn.h>
#include <wps_dh.h>
#include <tutrace.h>

#include <slist.h>

#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <reg_prototlv.h>
#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <wps_staeapsm.h>
#include <wps_enrapi.h>
#include <eap_defs.h>
#include <wps_enr.h>
#include <wps_enr_osl.h>

/* useful macros */
#define ARRAYSIZE(a)  (sizeof(a)/sizeof(a[0]))

#include <packed_section_start.h>
typedef BWL_PRE_PACKED_STRUCT struct eap_header {
	uint8 code;
	uint8 id;
	uint16 length;
	uint8 type;
} BWL_POST_PACKED_STRUCT EAP_HDR;

typedef BWL_PRE_PACKED_STRUCT struct eapol_hdr {
	uint8 version;
	uint8 type;
	uint16 len;
} BWL_POST_PACKED_STRUCT EAPOL_HDR;
#include <packed_section_end.h>

typedef struct  {
	char state; /* current state. */
	char sent_msg_id; /* out wps message ID */
	char recv_msg_id; /* in wps message ID */
	unsigned char eap_id; /* identifier of the last request  */
	char peer_mac[6];
	uint8 sta_type; /* enrollee or registrar */
	char pad1;
	char msg_to_send[WPS_EAP_DATA_MAX_LENGTH]; /* message to be sent */
	int msg_len;
	uint32 time_sent; /* Time of the first call to wps_get_msg_to_send  (per msg) */
	int send_count; /* number of times the current message has been sent */
	void *sm_hdle; /* handle to state machine  */
} eap_wps_sta_state;

#define EAP_MSG_OFFSET 4

#define WPS_DATA_OFFSET	(EAP_MSG_OFFSET + sizeof(WpsEapHdr))

static eap_wps_sta_state s_staEapState;
static eap_wps_sta_state *staEapState = NULL;
static int sta_e_mode = EModeClient;

static char *WpsEapEnrIdentity =  "WFA-SimpleConfig-Enrollee-1-0";
static char *WpsEapRegIdentity =  "WFA-SimpleConfig-Registrar-1-0";
static char* WPS_MSG_STRS[] = {
	" NONE",
	"(BEACON)",
	"(PROBE_REQ)",
	"(PROBE_RESP)",
	"(M1)",
	"(M2)",
	"(M2D)",
	"(M3)",
	"(M4)",
	"(M5)",
	"(M6)",
	"(M7)",
	"(M8)",
	"(ACK)",
	"(NACK)",
	"(DONE)",
	" / Identity",
	"(Start)",
	"(FAILURE)"
};

extern int WpsEnrolleeSM_Step(void *handle, uint32 msgLen, uint8 *p_msg);

int wps_eap_eapol_start(char *pkt);
int wps_eap_ap_retrans(char *eapol_msg, int len);
int wps_eap_enr_process_protocol(char *eap_msg, int len);
int wps_eap_identity_resp(char *pkt);

int
wps_eap_sta_init(char *bssid, void *handle, int e_mode)
{
	/*
	 * I like working on pointer better, and that way, we can make sure
	 * we are initialized...
	 */
	staEapState = &s_staEapState;
	if (bssid)
		memcpy(staEapState->peer_mac, bssid, 6);

	/* prepare the EAPOL START */
	wps_eap_eapol_start(staEapState->msg_to_send);
	staEapState->sm_hdle = handle;
	sta_e_mode = e_mode;
	staEapState->recv_msg_id = 0;
	staEapState->sent_msg_id = 0;

	return 0;
}

static int
wps_recv_msg_id_upd(char *eap_msg)
{
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)eap_msg;

	if (wpsEapHdr->code == EAP_CODE_FAILURE) {
		/* Failure */
		staEapState->recv_msg_id = (WPS_ID_MESSAGE_DONE + 3);
	}
	else if (wpsEapHdr->type == EAP_TYPE_IDENTITY) {
		/* Identity */
		staEapState->recv_msg_id = (WPS_ID_MESSAGE_DONE + 1);
	}
	else if (wpsEapHdr->opcode == WPS_Start) {
		/* WPS Start */
		staEapState->recv_msg_id = (WPS_ID_MESSAGE_DONE + 2);
	}
	else {
		staEapState->recv_msg_id = eap_msg[sizeof(WpsEapHdr) + WPS_MSGTYPE_OFFSET];
	}

	return 0;
}

/* sta dependent  */
int
wps_process_ap_msg(char *eapol_msg, int len)
{
	char *eap_msg;
	WpsEapHdr *wpsEapHdr = NULL;
	int status = 0;
	int eap_len = len - EAP_MSG_OFFSET;

	eap_msg = eapol_msg + EAP_MSG_OFFSET;
	wpsEapHdr = (WpsEapHdr *)eap_msg;

	/* check the code  */
	if (wpsEapHdr->code != EAP_CODE_REQUEST) {
		/* oops, probably a failure  */
		return EAP_FAILURE;
	}

	if (!staEapState) {
		return INTERNAL_ERROR;
	}

	/* now check that this is not a re-transmission from ap */
	status = wps_eap_ap_retrans(eapol_msg, len);
	if (status == WPS_SEND_MSG_CONT || status == REG_FAILURE)
		return status;

	/* update receive message id */
	wps_recv_msg_id_upd(eap_msg);

	/*
	 * invalidate the previous message to send.
	 * This allows for checking if we need to re-transmit and if the message
	 * to send is still valid.
	 */
	switch (staEapState->state) {
	case EAPOL_START_SENT :
	{
		if (wpsEapHdr->type != EAP_TYPE_IDENTITY)
			return WPS_CONT;
		return  wps_eap_identity_resp(staEapState->msg_to_send);
	}
	case EAP_IDENTITY_SENT :
	case PROCESSING_PROTOCOL:
		return wps_eap_enr_process_protocol(eap_msg, eap_len);
	case REG_SUCCESSFUL:
		return REG_SUCCESS;
	}

	return WPS_CONT;
}

char
wps_get_ap_msg_type(char *eapol_msg, int len)
{
	char *eap_msg;
	WpsEapHdr * wpsEapHdr = NULL;
	unsigned char *wps_msg;

	eap_msg = eapol_msg + EAP_MSG_OFFSET;
	wpsEapHdr = (WpsEapHdr *)eap_msg;
	wps_msg = (unsigned char *)(wpsEapHdr + 1);

	if (!staEapState) {
		return 0;
	}
	/* check the code  */
	if (wpsEapHdr->code != EAP_CODE_REQUEST) {
		/* oops, probably a failure  */
		if (wpsEapHdr->code == EAP_CODE_FAILURE)
			return (WPS_ID_MESSAGE_DONE + 3);
		else
			return 0;
	}

	/* Identity */
	if (wpsEapHdr->type == EAP_TYPE_IDENTITY) {
		return (WPS_ID_MESSAGE_DONE + 1);
	}

	/* WPS Start */
	if (wpsEapHdr->opcode == WPS_Start) {
		return (WPS_ID_MESSAGE_DONE + 2);
	}

	return wps_msg[WPS_MSGTYPE_OFFSET];
}

/*
 * Check if we already have answered this request and our peer didn't receive it.
 * Also check that that the id matches the request. Sta dependent.
 */
int
wps_eap_ap_retrans(char *eapol_msg, int len)
{
	char *eap_msg;
	WpsEapHdr *wpsEapHdr;
	/* normal status : we are waiting from a new message from reg */
	int status = WPS_CONT;
	char wps_msg_type;

	eap_msg = eapol_msg + EAP_MSG_OFFSET;
	wpsEapHdr = (WpsEapHdr *)eap_msg;

	/* get wps messge type id */
	wps_msg_type = wps_get_ap_msg_type(eapol_msg, len);

	/*
	 * Some AP use same identifier for different eap message.
	 * thus, we should check not only identifier, but also message type.
	 * if both identifier and message type is same, we can handle it as
	 * re-sent message from AP.
	 */
	if ((wpsEapHdr->id == staEapState->eap_id) &&
	    (wps_msg_type == staEapState->recv_msg_id)) {
		switch (staEapState->state) {
		case EAPOL_START_SENT :
			/* we haven't received the first valid id yet, process normally */
			return status;
			/* break;	unreachable */
		case EAP_IDENTITY_SENT :
			/*
			 * some AP reused request id in request identity and M1,
			 * don't check it, let up layer to check it.
			 */
			return status;
			/* break;	unreachable */
		case PROCESSING_PROTOCOL:
			/* wps type ? */
			if (wpsEapHdr->type != EAP_TYPE_WPS)
				return REG_FAILURE;

			/*
			 * Reduce sizeof(EAPOL_HDR) + sizeof(WpsEapHdr), because caller
			 * use wps_eap_send_msg() to send ths re-transmission packet
			 */
			staEapState->msg_len -= (sizeof(EAPOL_HDR) + sizeof(WpsEapHdr));
			break;
		}
		/*
		 * valid re-transmission, do not process, just indicate that
		 * the old response is waiting to be sent.
		 */
		status = WPS_SEND_MSG_CONT;
	}
	else if ((wpsEapHdr->id != staEapState->eap_id) &&
		(wps_msg_type == staEapState->recv_msg_id) &&
		(wps_msg_type == (WPS_ID_MESSAGE_DONE + 1))) {
		/*
		 * Some AP change identifier when it retransmit Identity request message.
		 * In that case, we change staEapState->state to EAPOL_START_SENT
		 * from EAP_IDENTITY_SENT because we should re-create Identity response
		 * message with new identifier
		 */
		staEapState->state = EAPOL_START_SENT;
		staEapState->eap_id = wpsEapHdr->id;
	}
	else {
		staEapState->eap_id = wpsEapHdr->id;
	}

	return status;
}

/*
* returns the length of current message to be sent or -1 if none.
* move the counter.
* Independent from enr/reg or ap/sta
*/
int
wps_get_msg_to_send(char **data, uint32 time)
{
	if (!staEapState) {
		*data = NULL;
		return -1;
	}

	if (staEapState->send_count == 0)
		staEapState->time_sent = time;

	/* we should check that counter when checking timers. */
	staEapState->send_count ++;
	if (staEapState->state >= PROCESSING_PROTOCOL)
		*data = &staEapState->msg_to_send[WPS_DATA_OFFSET];
	else
		*data = staEapState->msg_to_send;

	return staEapState->msg_len;
}

int
wps_get_retrans_msg_to_send(char **data, uint32 time, char *mtype)
{
	if (!staEapState) {
		*data = NULL;
		return -1;
	}

	if (staEapState->send_count == 0)
		staEapState->time_sent = time;

	/* we should check that counter when checking timers. */
	staEapState->send_count ++;

	*data = staEapState->msg_to_send;
	*mtype = 0;

	if (staEapState->state >= PROCESSING_PROTOCOL)
		*mtype = staEapState->msg_to_send[WPS_DATA_OFFSET + WPS_MSGTYPE_OFFSET];

	return staEapState->msg_len;
}

char *
wps_get_msg_string(int mtype)
{
	if (mtype < WPS_ID_BEACON || mtype > (ARRAYSIZE(WPS_MSG_STRS)-1))
		return WPS_MSG_STRS[0];
	else
		return WPS_MSG_STRS[mtype];
}

int
wps_eap_check_timer(uint32 time)
{
	if (!staEapState) {
		return WPS_CONT;
	}

	if (5 < (time - staEapState->time_sent)) {
		if (staEapState->send_count >= 3) {
			return EAP_TIMEOUT;
		}

		/* update current time */
		staEapState->time_sent = time;

		/* In M2D time wait */
		if (staEapState->recv_msg_id == WPS_ID_MESSAGE_M2D) {
			staEapState->send_count ++;
			return WPS_CONT;
		}

		/* Do not retransmit after M3 */
		if (staEapState->recv_msg_id > WPS_ID_MESSAGE_M2D &&
		    staEapState->recv_msg_id != (WPS_ID_MESSAGE_DONE + 1) /* Identity */ &&
		    staEapState->recv_msg_id != (WPS_ID_MESSAGE_DONE + 2) /* WPS Start */ &&
		    staEapState->recv_msg_id != (WPS_ID_MESSAGE_DONE + 3) /* Failure */)
			return EAP_TIMEOUT;

		/*
		 * should we check for wrap??
		 * At the worst, it would happen once and re-transmit
		 * will not break anything anyway.
		 * Or will it ??? To be investigated ...
		 */
		return WPS_SEND_MSG_CONT; /* re-transmit  */
	}

	return WPS_CONT;
}

/*
* this is enrollee/registrar dependent.
* Could be made independent by using function pointer for "Step" instead
* of the state machine.
*/
int
wps_eap_enr_process_protocol(char *eap_msg, int len)
{
	/* ideally, it would be nice to pass the buffer to output to the process function ... */
	/* WpsEapHdr * wpsEapHdr_out = (WpsEapHdr *)staEapState->msg_to_send; */
	WpsEapHdr * wpsEapHdr_in = (WpsEapHdr *)eap_msg;
	int retVal;
	/* point after the eap header */
	unsigned char *msg_in = (unsigned char *)(wpsEapHdr_in + 1);

	/* change to PROCESSING_PROTOCOL state */
	staEapState->state = PROCESSING_PROTOCOL;

	/* reset msg_len */
	staEapState->msg_len = WPS_EAP_DATA_MAX_LENGTH - WPS_DATA_OFFSET;
	if (len >= 14) {
		if (sta_e_mode == EModeRegistrar)
			retVal = reg_sm_step(staEapState->sm_hdle, len - sizeof(WpsEapHdr),
				msg_in, (uint8*)&staEapState->msg_to_send[WPS_DATA_OFFSET],
				(uint32*)&staEapState->msg_len);
		else
			retVal = enr_sm_step(staEapState->sm_hdle, len - sizeof(WpsEapHdr),
				msg_in, (uint8*)&staEapState->msg_to_send[WPS_DATA_OFFSET],
				(uint32*)&staEapState->msg_len);

		return retVal;
	}

	return WPS_CONT;
}

/* Construct an EAPOL Start. Sta dependent */
int
wps_eap_eapol_start(char *pkt)
{
	EAPOL_HDR *eapol = (EAPOL_HDR *)pkt;
	eapol->version = 1;
	eapol->type = 1;
	eapol->len = 0;
	staEapState->state = EAPOL_START_SENT;
	staEapState->send_count = 0;
	staEapState->msg_len = sizeof(EAPOL_HDR);

	return WPS_SEND_MSG_CONT;
}

/* EAP Response Identity. Sta dependent  */
int
wps_eap_identity_resp(char *pkt)
{
	int size;
	EAPOL_HDR *eapol;
	EAP_HDR *eapHdr;
	char *identity = WpsEapEnrIdentity;

	if (sta_e_mode == EModeRegistrar)
		identity = WpsEapRegIdentity;

	size = strlen(identity) + sizeof(EAP_HDR);
	eapol = (EAPOL_HDR *)pkt;
	eapHdr = (EAP_HDR *)(eapol +1);
	eapol->version = 1;
	eapol->type = 0;
	eapol->len = WpsHtons(size);
	eapHdr->code = EAP_CODE_RESPONSE;
	eapHdr->id = staEapState->eap_id;
	eapHdr->length = eapol->len;
	eapHdr->type = EAP_TYPE_IDENTITY;
	memcpy((char *)eapHdr + 5, identity, strlen(identity));
	staEapState->send_count = 0;
	staEapState->msg_len = sizeof(EAPOL_HDR) + size;
	staEapState->state = EAP_IDENTITY_SENT;

	return WPS_SEND_MSG_IDRESP;
}

/*
 * called after enrollee processing
 * The message will then be available using
 * get_message_from_enrollee().
 * Independent (must be called from sta/ap dependent code).
 */
int
wps_eap_create_pkt(char * dataBuffer, int dataLen, uint8 eapCode)
{
	int ret = WPS_SUCCESS;
	EAPOL_HDR *eapol = (EAPOL_HDR *)staEapState->msg_to_send;
	WpsEapHdr * wpsEapHdr = NULL;

	eapol->version = 1;
	eapol->type = 0;
	eapol->len = WpsHtons(dataLen + sizeof(WpsEapHdr));
	wpsEapHdr = (WpsEapHdr *)(eapol +1);
	wpsEapHdr->code = eapCode;
	wpsEapHdr->type = EAP_TYPE_WPS;
	wpsEapHdr->id = staEapState->eap_id;
	wpsEapHdr->vendorId[0] = WPS_VENDORID1;
	wpsEapHdr->vendorId[1] = WPS_VENDORID2;
	wpsEapHdr->vendorId[2] = WPS_VENDORID3;
	wpsEapHdr->vendorType = WpsHtonl(WPS_VENDORTYPE);
	wpsEapHdr->length = eapol->len;

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
			staEapState->state = REG_FAILED;
		}
		else if (dataBuffer[WPS_MSGTYPE_OFFSET] == WPS_ID_MESSAGE_DONE) {
			wpsEapHdr->opcode = WPS_Done;
			staEapState->state = REG_SUCCESSFUL;
		}
		else {
			TUTRACE((TUTRACE_ERR, "Unknown Message Type code %d; "
				"Not a valid msg\n", dataBuffer[WPS_MSGTYPE_OFFSET]));
			return TREAP_ERR_SENDRECV;
		}

		/* record the out message id */
		staEapState->sent_msg_id = dataBuffer[WPS_MSGTYPE_OFFSET];

		/*
		 * in case we passed this buffer to the protocol,
		 * do not copy :-)
		 */
		if (dataBuffer != (char *)(wpsEapHdr + 1)) {
			memcpy((char *)wpsEapHdr + sizeof(WpsEapHdr), dataBuffer, dataLen);
		}
		staEapState->msg_len = dataLen + sizeof(EAPOL_HDR) + sizeof(WpsEapHdr);

		/* mark as not been retreived yet */
		staEapState->send_count = 0;
	}
	else {
		/* don't know if this makes sens ... */
		ret = WPS_CONT;
	}

	return ret;
}

/* Called by Transport, normally inplemented in InbEap.c. sta dependent.  */
uint32
wps_eap_send_msg(char * dataBuffer, uint32 dataLen)
{

	uint32 retVal;

	TUTRACE((TUTRACE_INFO, "wps_eap_send::WriteData buffer Length = %d\n",
		dataLen));

	retVal = wps_eap_create_pkt(dataBuffer, dataLen, EAP_CODE_RESPONSE);
	if (retVal == WPS_SUCCESS) {
		send_eapol_packet(staEapState->msg_to_send, staEapState->msg_len);
	}

	return WPS_SUCCESS;
}

void
wps_eap_failure()
{
	EAPOL_HDR *eapol = (EAPOL_HDR *)staEapState->msg_to_send;
	WpsEapHdr *wpsEapHdr = (WpsEapHdr *)(eapol +1);

	eapol->version = 1;
	eapol->type = 0;
	eapol->len = WpsHtons(sizeof(WpsEapHdr));
	memset(wpsEapHdr, 0, sizeof(WpsEapHdr));
	wpsEapHdr->code = EAP_CODE_FAILURE;
	wpsEapHdr->id = staEapState->eap_id;
	wpsEapHdr->length = WpsHtons(sizeof(WpsEapHdr));

	staEapState->msg_len = sizeof(EAPOL_HDR) + sizeof(WpsEapHdr);

	send_eapol_packet(staEapState->msg_to_send, staEapState->msg_len);

	staEapState->state = EAP_FAILURE;

	return;
}

int
wps_get_sent_msg_id()
{
	return ((int)staEapState->sent_msg_id);
}

int
wps_get_recv_msg_id()
{
	/*
	 * Because we don't want to modify the caller source code,
	 * return 0 (same as before) in case of Identity/WPS Start/Failure. 
	 */
	if (staEapState->recv_msg_id == (WPS_ID_MESSAGE_DONE + 1) /* Identity */ ||
	    staEapState->recv_msg_id == (WPS_ID_MESSAGE_DONE + 2) /* WPS Start */ ||
	    staEapState->recv_msg_id == (WPS_ID_MESSAGE_DONE + 3) /* Failure */)
	    return 0;

	return ((int)staEapState->recv_msg_id);
}

int
wps_get_eap_state()
{
	return ((int)staEapState->state);
}
