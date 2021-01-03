/*
 * WPS eap
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_eap.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <stdio.h>

#include <bn.h>
#include <wps_dh.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <sminfo.h>
#include <reg_proto.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef __ECOS
#include <sys/socket.h>
#endif
#include <net/if.h>

#include <bcmparams.h>
#include <wlutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wps_wl.h>
#include <tutrace.h>

#include <bcmutils.h>
#include <wps_ap.h>
#include <security_ipc.h>
#include <wps_ui.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <wps_pb.h>
#include <wps.h>
#include <wps_eap.h>


/* under m_xxx in mbuf.h */

struct eap_user_list {
	unsigned char identity[32];
	unsigned int identity_len;
};

static struct eap_user_list  eapuserlist[2] = {
	{"WFA-SimpleConfig-Enrollee-1-0", 29},
	{"WFA-SimpleConfig-Registrar-1-0", 30}
};

static wps_hndl_t eap_hndl;


static uint32
wps_eap_get_id(char *buf)
{
	eapol_header_t *eapol = (eapol_header_t *)buf;
	eap_header_t *eap = (eap_header_t *) eapol->body;
	int i;

	if (eap->type != EAP_IDENTITY)
		return WPS_EAP_ID_NONE;

	for (i = 0; i < 2; i++) {
		if (memcmp((char *)eap->data, eapuserlist[i].identity,
			eapuserlist[i].identity_len) == 0) {
			return i;
		}
	}

	return WPS_EAP_ID_NONE;
}

static uint32
wps_eap_parse_prob_reqIE(unsigned char *mac, unsigned char *p_data, uint32 len)
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

	if (prReqIE.pwdId.m_data == WPS_DEVICEPWDID_PUSH_BTN) {
		TUTRACE((TUTRACE_ERR, "\n\nPush Button sta detect\n\n"));
		wps_pb_update_pushtime(mac);
	}

	buffobj_del(bufObj);
	return 0;
}


/* PBC Overlapped detection */
int
wps_eap_process_msg(char *buf, int buflen)
{
	int ret = WPS_CONT;
	int eapid = WPS_EAP_ID_NONE;
	struct ether_header *eth;
	eapol_header_t *eapol;
	eap_header_t *eap;
	char ifname[IFNAMSIZ] = {0};
	int mode = -1;
	wps_app_t *wps_app = get_wps_app();

	/* Retreive wireless ifname of eap packets */
	strncpy(ifname, eap_hndl.ifname, sizeof(ifname));

	/* check is it bcmevent? */
	eth = (struct ether_header *) buf;
	if (ntohs(eth->ether_type) == ETHER_TYPE_BRCM) {
		bcm_event_t *pvt_data = (bcm_event_t *)buf;
		wl_event_msg_t *event = &(pvt_data->event);
		uint8 *addr = (uint8 *)&(event->addr);
		int totlen = ntohl(event->datalen);
		uint8 *ie = (uint8 *)(event + 1) + DOT11_MGMT_HDR_LEN;
		bcm_tlv_t *elt = (bcm_tlv_t *)ie;

		if  (ntohs(pvt_data->bcm_hdr.usr_subtype) == BCMILCP_BCM_SUBTYPE_EVENT) {
			if ((ntohl(pvt_data->event.event_type) == WLC_E_PROBREQ_MSG)) {
				while (totlen >= 2) {
					int eltlen = elt->len;
					/* validate remaining totlen */
					if ((elt->id == 0xdd) && (totlen >= (eltlen + 2))) {
						if ((elt->len >= WPA_IE_OUITYPE_LEN) &&
							!bcmp(elt->data, WPA_OUI"\x04",
							WPA_IE_OUITYPE_LEN)) {
							TUTRACE((TUTRACE_INFO,
								"found WPS  probe request \n"));

							/* TBD, you have addr */
							wps_eap_parse_prob_reqIE(addr,
								&elt->data[4], elt->len - 4);
							return ret;
						}
					}
					elt = (bcm_tlv_t*)((uint8*)elt + (eltlen + 2));
					ie = (uint8 *)elt;
					totlen -= (eltlen + 2);
				}
				return ret;
			}
			else {
				TUTRACE((TUTRACE_ERR, "%s: recved wl event type %d\n",
					pvt_data->event.ifname,
					ntohl(pvt_data->event.event_type)));
				return ret;
			}
		}
		else {
			return ret;
		}
	}

	/* if is not EAP IDENTITY and there is no existing WPS running, exit   */
	eapol = (eapol_header_t *)buf;
	eap = (eap_header_t *)eapol->body;

	if ((eapid = wps_eap_get_id(buf)) == WPS_EAP_ID_NONE && !wps_app->wksp) {
		return ret;
	}

	/* Indicate station associated */
	if (eapid != WPS_EAP_ID_NONE) {
		wps_setProcessStates(WPS_ASSOCIATED);
	}

	if (wps_app->wksp) {
		if (wps_app->mode != WPSM_ENROLL) {
			/*
			  * Ingnore other eap packet from other interfaces,
			  * if the ap session is opened.
			  */
			if (strcmp(ifname, ((wpsap_wksp_t *)wps_app->wksp)->ifname) != 0) {
				TUTRACE((TUTRACE_ERR, "Expect EAP from %s, rather than %s\n",
					((wpsap_wksp_t *)wps_app->wksp)->ifname, ifname));
				return WPS_CONT;
			}

			/* Check eap source mac */
			if (memcmp(((wpsap_wksp_t *)wps_app->wksp)->mac_sta,
				eapol->eth.ether_shost, 6) != 0) {
#ifdef _TUDEBUGTRACE
				char tmp[64];
#endif
				TUTRACE((TUTRACE_ERR, "Expect EAP from %s, rather than %s\n",
					ether_etoa(((wpsap_wksp_t *)(wps_app->wksp))->mac_sta, tmp),
					ether_etoa(eapol->eth.ether_shost, tmp)));
			}
		}
#ifdef BCMWPSAP
		if (eapid == WPS_EAP_ID_REGISTRAR && wps_app->mode == WPSM_PROXY &&
			wps_getenrState(((wpsap_wksp_t *)wps_app->wksp)->mc) == MSTART) {
			/* WAR, special patch for DTM 1.4 test case 934 */
#ifdef _TUDEBUGTRACE
			int enr_status = wps_getenrState(((wpsap_wksp_t *)wps_app->wksp)->mc);
			int reg_status = wps_getregState(((wpsap_wksp_t *)wps_app->wksp)->mc);
			TUTRACE((TUTRACE_ERR, "enr_status = %d, reg_status = %d\n", enr_status,
				reg_status));
#endif
			wps_close_session();
		}
		else
#endif /* BCMWPSAP */
		{
			return (*wps_app->process)(wps_app->wksp, buf, buflen, TRANSPORT_TYPE_EAP);
		}
	}

	/* This packet might be enrollee or registrar, start wps with suit command */
	if (eapid == WPS_EAP_ID_ENROLLEE) {
		if (atoi(wps_ui_get_env("wps_config_command")) == WPS_UI_CMD_START) {
			/* received EAP enrollee packet and UI button was pushed */
			mode = WPSM_BUILTINREG;
		}
		else {
			/* received EAP packet and no button in UI was pushed */
			mode = WPSM_PROXY;
		}

		TUTRACE((TUTRACE_ERR, "eapid = WPS_EAP_ID_ENROLLEE mode = %s\n",
			mode == WPSM_BUILTINREG ?
			"WPSM_BUILTINREG" : "WPSM_PROXY"));
	}
	/*
	 * Only start enrollee when we are not doing built-in reg,
	 * Special patch for DTM 1.4 test case 913
	 */
	else if (eapid == WPS_EAP_ID_REGISTRAR && !wps_ui_is_pending()) {
		/* receive EAP packet and no buttion in UI pushed */
		mode = WPSM_UNCONFAP;

		TUTRACE((TUTRACE_ERR, "eapid = WPS_EAP_ID_REGISTRAR mode = %WPSM_PROXY\n"));
	}
	else {
		return ret;
	}

#ifdef BCMWPSAP
	ret = wpsap_open_session(wps_app, mode, eapol->eth.ether_dhost, eapol->eth.ether_shost,
		ifname, NULL, NULL);
#endif /* BCMWPSAP */
	return ret;
}

/* WPS eap init */
int
wps_eap_get_handle()
{
	return eap_hndl.handle;
}

int
wps_eap_init()
{
	/* Init eap_hndl */
	memset(&eap_hndl, 0, sizeof(eap_hndl));

	eap_hndl.type = WPS_RECEIVE_PKT_EAP;
	eap_hndl.handle = wps_osl_eap_handle_init();
	if (eap_hndl.handle == -1)
		return -1;

	wps_hndl_add(&eap_hndl);
	return 0;
}

void
wps_eap_deinit()
{
	wps_hndl_del(&eap_hndl);
	wps_osl_eap_handle_deinit(eap_hndl.handle);
	return;
}
