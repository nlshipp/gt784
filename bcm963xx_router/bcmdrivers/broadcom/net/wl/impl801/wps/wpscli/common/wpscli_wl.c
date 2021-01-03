/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement wl related functionalities to communicated with driver
 *
 */
#include <ctype.h>
#include "wpscli_osl.h"
#include "wlioctl.h"
#include "wps_wl.h"
#include "tutrace.h"
#include "wpscli_common.h"

/* enable structure packing */
#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

#define XSUPPD_MAX_PACKET			4096
#define XSUPPD_ETHADDR_LENGTH		6
#define EAPOL_ETHER_TYPE			0x888e
#define ether_set_type(p, t)		((p)[0] = (uint8)((t) >> 8), (p)[1] = (uint8)(t))

/* ethernet header */
typedef struct
{
	uint8 dest[XSUPPD_ETHADDR_LENGTH];
	uint8 src[XSUPPD_ETHADDR_LENGTH];
	uint8 type[2];
} PACKED ether_header;

typedef struct
{
	/* outgoing packet */
	uint8 out[XSUPPD_MAX_PACKET];
	int out_size;
} sendpacket;

BOOL wpscli_del_wps_ie(unsigned int frametype);

static uint wpscli_iovar_mkbuf(const char *name, const char *data, uint datalen, char *iovar_buf, uint buflen)
{
	uint iovar_len;
	char *p;

	iovar_len = (uint) strlen(name) + 1;

	/* check for overflow */
	if ((iovar_len + datalen) > buflen) 
		return 0;

	/* copy data to the buffer past the end of the iovar name string */
	if (datalen > 0)
		memmove(&iovar_buf[iovar_len], data, datalen);

	/* copy the name to the beginning of the buffer */
	strcpy(iovar_buf, name);

	/* wl command line automatically converts iovar names to lower case for
	 * ease of use
	 */
	p = iovar_buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}

	return (iovar_len + datalen);
}

BOOL wpscli_iovar_get(const char *iovar, void *buf, int buf_len)
{
	BOOL bRet;
	brcm_wpscli_status status;

	memcpy(buf, iovar, strlen(iovar));

	status = wpscli_wlh_ioctl_get(WLC_GET_VAR, buf, buf_len);
	bRet = (status == WPS_STATUS_SUCCESS);

	return bRet;
}

BOOL wpscli_iovar_set(const char *iovar, void *param, uint paramlen)
{
	BOOL bRet = FALSE;
	char smbuf[WLC_IOCTL_SMLEN];
	uint iolen;
	brcm_wpscli_status status;
	
	memset(smbuf, 0, sizeof(smbuf));
	iolen = wpscli_iovar_mkbuf(iovar, param, paramlen, smbuf, sizeof(smbuf));
	if (iolen == 0)
	{
		TUTRACE((TUTRACE_ERR, "wpscli_iovar_set(%s, paramlen=%d): wpscli_iovar_mkbuf() failed\n"));
		return bRet;
	}

	status = wpscli_wlh_ioctl_set(WLC_SET_VAR, smbuf, iolen);
	if(status == WPS_STATUS_SUCCESS)
		bRet = TRUE;

	return bRet;
}

BOOL wpscli_del_wl_prov_svc_ie(unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf;
	int iecount, i;

	char setbuf[256] = {0};
	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0x05};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;

	TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Entered.\n"));

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		frametype = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		frametype = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_del_wl_prov_svc_ie: unknown frame type\n"));
		return FALSE;
	}

	if(!wpscli_iovar_get("vndr_ie", getbuf, 2048)) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. Failed to get vndr_ie\n"));
		return FALSE;
	}

	iebuf = (vndr_ie_buf_t *)getbuf;

	bufaddr = (char*) iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*)bufaddr;
		memmove((char*)&pktflag, (char*)&ieinfo->pktflag, (int)sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4))
			{
				found = 1;
				bufaddr = (char*)&ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}

		/* ieinfo->vndr_ie_data.len represents the together size (number of bytes) of OUI + IE data */ 
		bufaddr = (char *)ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len;
	}

	if (!found) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. No Wireless Provisioning Service IE found.\n"));
		return FALSE;
	}

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *)setbuf;

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, iebuf_len);

	TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_set_wl_prov_svc_ie(unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	unsigned int pktflag;
	int buflen, iecount, i;
	char ie_buf[256] = { 0 };
	vndr_ie_setbuf_t *ie_setbuf = (vndr_ie_setbuf_t *)ie_buf;

	TUTRACE((TUTRACE_INFO, "wpscli_set_wl_prov_svc_ie: Entered.\n"));

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		pktflag = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		pktflag = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wl_prov_svc_ie: unknown frame type\n"));
		return FALSE;
	}

	/* Delete wireless provisioning service IE first if it is already existed */
	wpscli_del_wl_prov_svc_ie(cmdtype);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* 
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8)length + VNDR_IE_MIN_LEN + 1;

	/* Wireless Provisioning Service vendor IE with OUI 00 50 F2 05 */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = 0x05;

	for (i = 0; i < length; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = p_data[i];
	}

	buflen = ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len -
		VNDR_IE_MIN_LEN + sizeof(vndr_ie_setbuf_t) - 1;

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, buflen);
	if(!bRet)
		TUTRACE((TUTRACE_ERR, "wpscli_set_wl_prov_svc_ie: wpscli_iovar_set of vndir_ie failed; buflen=%d\n", buflen));
	
	TUTRACE((TUTRACE_INFO, "wpscli_set_wl_prov_svc_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

/* print bytes formatted as hex to a log file */
void wpscli_log_hexdata(char *heading, unsigned char *data, int dataLen)
{
	int	i;
	char dispBuf[4096];		/* debug-output display buffer */
	int dispBufSizeLeft;
	char *dispBufNewLine = "\n                              ";

	if (strlen(heading) >= sizeof(dispBuf))
		return;

	sprintf(dispBuf, "%s", heading);
	dispBufSizeLeft = sizeof(dispBuf) - strlen(dispBuf) - 1;
	for (i = 0; i < dataLen; i++) {
		/* show 20-byte in one row */
		if ((i % 20 == 0) && (dispBufSizeLeft > (int)strlen(dispBufNewLine)))
		{
			strcat(dispBuf, dispBufNewLine);
			dispBufSizeLeft -= strlen(dispBufNewLine);
		}

		/* make sure buffer is large enough, if not, abort it */
		if (dispBufSizeLeft < 3)
			break;
		sprintf(&dispBuf[strlen(dispBuf)], "%02x ", data[i]);
		dispBufSizeLeft -= 3;
	}
	TUTRACE((TUTRACE_INFO,"%s\n", dispBuf));
}


BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	unsigned int pktflag;
	int buflen, iecount, i;
	vndr_ie_setbuf_t *ie_setbuf = NULL;

	TUTRACE((TUTRACE_INFO, "wpscli_set_wps_ie: Entered.\n"));

#ifdef _TUDEBUGTRACE
	TUTRACE((TUTRACE_INFO,"\nwpscli_set_wps_ie: iebuf (len=%d):\n", length));
	wpscli_log_hexdata("wpscli_set_wps_ie:", p_data, length);
#endif

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		pktflag = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		pktflag = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: unknown frame type\n"));
		return FALSE;
	}

	if (length > VNDR_IE_MAX_LEN) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: IE length should be <= %d\n", VNDR_IE_MAX_LEN));
		return FALSE;
	}

	buflen = sizeof(vndr_ie_setbuf_t) + length + 1;
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);

	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: memory alloc failure\n"));
		return FALSE;
	}
	
	// Zero memory
	memset(ie_setbuf, 0, buflen);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* 
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8)length + VNDR_IE_MIN_LEN + 1;

	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = 0x04;

	for (i = 0; i < length; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = p_data[i];
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy (ie_setbuf->cmd, "add");

	buflen = ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len -
		VNDR_IE_MIN_LEN + sizeof(vndr_ie_setbuf_t) - 1;
 	
	/* Always try to delete existing WPS IE first */
	wpscli_del_wps_ie(cmdtype);

	/* Set wps IE */
	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, buflen);
	if(!bRet)
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: wpscli_iovar_set of vndir_ie failed(buflen=%d).\n", buflen));
	
	free(ie_setbuf);

	TUTRACE((TUTRACE_INFO, "wpscli_set_wps_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_del_wps_ie(unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf;
	int iecount, i;

	char getbuf[2048] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0x04};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;

	TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Entered.\n"));

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		frametype = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		frametype = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_del_wps_ie: unknown frame type\n"));
		return FALSE;
	}

	if(!wpscli_iovar_get("vndr_ie", getbuf, 2048)) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. Failed to get vndr_ie\n"));
		return FALSE;
	}

	iebuf = (vndr_ie_buf_t *)getbuf;

	bufaddr = (char*) iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*) bufaddr;
		memmove((char*)&pktflag, (char*)&ieinfo->pktflag, (int) sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4))
			{
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}

		/* ieinfo->vndr_ie_data.len represents the together size (number of bytes) of OUI + IE data */ 
		bufaddr = (char *)ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len;
	}

	if (!found) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. No WPS IE found.\n"));
		return FALSE;
	}

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(iebuf_len);
	if (! ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "wpscli_del_wps_ie: Exiting. memory alloc failure\n"));
		return FALSE;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, iebuf_len);

	free(ie_setbuf);

	TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_set_beacon_IE()
{
	BOOL ret;
	uint8 data8;

	BufferObj *bufObj = buffobj_new();

	// 
	// Create the Beacon IE
	//
	/* Version */
	data8 = 0x10;  // version 1.0
	tlv_serialize(WPS_ID_VERSION, bufObj, &data8, WPS_ID_VERSION_S);

	/* Simple Config State */
	data8 = WPS_SCSTATE_CONFIGURED;  // Assume the SoftAP configured already
	tlv_serialize(WPS_ID_SC_STATE, bufObj, &data8, WPS_ID_SC_STATE_S);

	// Set WPS beacon IE to driver
	ret = wps_set_wps_ie(NULL, bufObj->pBase, bufObj->m_dataLength, WPS_IE_TYPE_SET_BEACON_IE);

	/* bufObj will be destroyed when we exit this function */
	buffobj_del(bufObj);
	return ret;
}
