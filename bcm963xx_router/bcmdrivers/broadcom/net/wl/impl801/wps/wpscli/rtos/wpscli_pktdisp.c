/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_pktdisp.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: RTOS packet dispatcher
 *
 */

#include "wpscli_osl.h"
#include "tutrace.h"
#include "wpserror.h"
#include "wps_enr_osl.h"

#ifdef _TUDEBUGTRACE
extern void wpscli_print_buf(char *text, unsigned char *buff, int buflen);
#endif

void WpsSetBssid(uint8 *bssid);
uint32 wait_for_packet(char* buf, uint32* len, uint32 timeout, bool raw);

/* open/init packet dispatcher */
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Entered.\n"));

	if(peer_addr == NULL) {
		printf("Peer MAC address is not specified.\n");
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open. peer_addr=[%02X:%02X:%02X:%02X:%02X:%02X]\n", 
		peer_addr[0], peer_addr[1], peer_addr[2], peer_addr[3], peer_addr[4], peer_addr[5]));

	wps_osl_init((char *)peer_addr);

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

/* close/un-init packet dispatcher */
brcm_wpscli_status wpscli_pktdisp_close()
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_close: Entered.\n"));	
	wps_osl_deinit();
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_close: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_set_peer_addr(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Entered.\n"));
	WpsSetBssid((uint8 *)peer_addr);
	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

/* waiting for eap data */
brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool b_raw)
{
/*	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Entered.\n")); */
	if (wait_for_packet(buf, len, timeout / 1000, b_raw) == WPS_SUCCESS)
	{
#ifdef _TUDEBUGTRACE
		wpscli_print_buf("WPS rx", (unsigned char *)buf, *len);
#endif
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_SUCCESS\n"));
		return WPS_STATUS_SUCCESS;
	}

/*	TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_PKTD_NO_PKT. FD_ISSET failed.\n")); */
	return WPS_STATUS_PKTD_NO_PKT;
}

/* send a packet */
brcm_wpscli_status wpscli_pktdisp_send_packet(char *dataBuffer, uint32 dataLen)
{
	TUTRACE((TUTRACE_INFO, "Entered: wpscli_pktdisp_send_packet. dataLen=%d\n", dataLen));

#ifdef _TUDEBUGTRACE
	wpscli_print_buf("WPS tx", (unsigned char *)dataBuffer, dataLen);
#endif

	if ((!dataBuffer) || (!dataLen)) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. Invalid Parameters\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	send_packet(dataBuffer, dataLen);
	
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Exiting. Succeeded.\n"));
	return WPS_STATUS_SUCCESS;
}
