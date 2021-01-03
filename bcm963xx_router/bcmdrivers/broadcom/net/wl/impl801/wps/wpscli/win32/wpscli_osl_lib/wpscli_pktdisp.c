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
 * Description: Implement packet dispatcher related OSL APIs  
 *
 */
#include "stdafx.h"
#include <stdlib.h>
#include <Ntddndis.h>
#include <packet32.h>
#include "wpscli_osl_common.h"
#include <tutrace.h>

#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

// The size of the buffer associated with the PACKET structure fore receiving packet
#define PACKET32_MAX_RX_BUFSIZE		(256*1024)  // The greatest MTU

// The size of the kernel-level buffer associated with a capture
#define PACKET32_KERNEL_BUFSIZE		(1024*1024)  

/* memory manipulation */
#define B1XS_ALLOC(s)			malloc(s)
#define B1XS_FREE(m)			free(m)
#define B1XS_COPY(d,s,l)		memcpy(d,s,l)
#define B1XS_ZERO(m,l)			memset(m,0,l)

#define ether_get_type(p)		(((p)[0] << 8) + (p)[1])
#define EAPOL_ETHER_TYPE		0x888e  // EAPOL Ethernet Type
#define XSUPPD_ETHADDR_LENGTH	6

DWORD g_dwFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_ALL_MULTICAST | NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_ALL_LOCAL;

/*
 * The instruction encodings.
 */
/* instruction classes */
#define BPF_CLASS(code) ((code) & 0x07)
#define		BPF_LD		0x00
#define		BPF_LDX		0x01
#define		BPF_ST		0x02
#define		BPF_STX		0x03
#define		BPF_ALU		0x04
#define		BPF_JMP		0x05
#define		BPF_RET		0x06
#define		BPF_MISC	0x07

/* ld/ldx fields */
#define BPF_SIZE(code)	((code) & 0x18)
#define		BPF_W		0x00
#define		BPF_H		0x08
#define		BPF_B		0x10
#define BPF_MODE(code)	((code) & 0xe0)
#define		BPF_IMM 	0x00
#define		BPF_ABS		0x20
#define		BPF_IND		0x40
#define		BPF_MEM		0x60
#define		BPF_LEN		0x80
#define		BPF_MSH		0xa0

/* alu/jmp fields */
#define BPF_OP(code)	((code) & 0xf0)
#define		BPF_ADD		0x00
#define		BPF_SUB		0x10
#define		BPF_MUL		0x20
#define		BPF_DIV		0x30
#define		BPF_OR		0x40
#define		BPF_AND		0x50
#define		BPF_LSH		0x60
#define		BPF_RSH		0x70
#define		BPF_NEG		0x80
#define		BPF_JA		0x00
#define		BPF_JEQ		0x10
#define		BPF_JGT		0x20
#define		BPF_JGE		0x30
#define		BPF_JSET	0x40
#define BPF_SRC(code)	((code) & 0x08)
#define		BPF_K		0x00
#define		BPF_X		0x08

/* ret - BPF_K and BPF_X also apply */
#define BPF_RVAL(code)	((code) & 0x18)
#define		BPF_A		0x10

/* misc */
#define BPF_MISCOP(code) ((code) & 0xf8)
#define		BPF_TAX		0x00
#define		BPF_TXA		0x80

/*
 * Macros for insn array initializers.
 */
#define BPF_STMT(code, k) { (u_short)(code), 0, 0, k }
#define BPF_JUMP(code, k, jt, jf) { (u_short)(code), jt, jf, k }


/* ethernet header */
typedef struct
{
	uint8 dest[6];
	uint8 src[6];
	uint8 type[2];
} PACKED ether_header;

typedef	struct
{
	uint32 timeout;
	void *adapter;
	void *rxbuf;
	void *rxpkt;
	void *rxptr;
	void *txpkt;
	uint8 peer_mac[6];
	uint8 src_mac[6];
} framer_t;

extern TCHAR* getAdapterName();

static framer_t g_framer;
static void frame_cleanup();
static BOOL frame_init(const char *npf_adapter_name);
static brcm_wpscli_status Eap_ReadData(char * dataBuffer, uint32 * dataLen, int read_timeout, bool b_raw);
static BOOL frame_recv_frame(char *data_msg, int *len, int recv_timeout);
static BOOL frame_send_frame(uint8 *frame, int len);

// open/init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Entered.\n"));

	if(peer_addr == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Invalid NULL parameter passed in.\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	if(getMacAddress() == NULL){
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Invalid NULL local adapter mac address.\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	// Set source and desitination address
	memset(&g_framer, 0, sizeof(g_framer));
	memcpy(g_framer.peer_mac, peer_addr, 6);
	memcpy(g_framer.src_mac, getMacAddress(), 6);

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: peer_mac=%02x:%02x:%02x:%02x:%02x:%02x, src_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		g_framer.peer_mac[0],g_framer.peer_mac[1],g_framer.peer_mac[2],
		g_framer.peer_mac[3],g_framer.peer_mac[4],g_framer.peer_mac[5],
		g_framer.src_mac[0],g_framer.src_mac[1],g_framer.src_mac[2],
		g_framer.src_mac[3],g_framer.src_mac[4],g_framer.src_mac[5]));

	// Initialize framer (winpcap library)
	if(!frame_init(getAdapterName())) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Failed to initialize framer.\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Exiting. status=WPS_STATUS_SUCCESS, adapterName=%s\n",
		getAdapterName()));
	return WPS_STATUS_SUCCESS;
}

// close/un-init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_close()
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_close: Entered.\n"));

	frame_cleanup();

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_close: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_set_peer_addr(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Entered.\n"));

	if(peer_addr == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_peer_mac: Exiting. NULL peer_addr is passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	memcpy(g_framer.peer_mac, peer_addr, 6);

	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool b_raw)
{
	brcm_wpscli_status status;

//	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Entered.\n"));

	if(buf == NULL || len == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	status = Eap_ReadData(buf, len, timeout, b_raw);
	
//	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Exiting. status=%d\n", status));
	return status;
}

// send a raw packet with ether header
#define ether_set_type(p, t)		((p)[0] = (uint8)((t) >> 8), (p)[1] = (uint8)(t))
brcm_wpscli_status wpscli_pktdisp_send_packet(char *dataBuffer, uint32 dataLen)
{
	brcm_wpscli_status status;

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Entered.\n"));

	if(dataBuffer == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. Invalid NULL dataBuffer passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if(dataLen == 0) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. Invalid 0 buffer size passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

#ifdef _TUDEBUGTRACE
	wpscli_print_buf("WPS rx", dataBuffer, dataLen);
#endif

	status = frame_send_frame(dataBuffer, dataLen)? WPS_STATUS_SUCCESS : WPS_STATUS_PKTD_SEND_PKT_FAIL;

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Exiting. status=%d\n", status));
	return status;
}


BOOL frame_init(const char *npf_adapter_name)
{
	BOOL bRet = FALSE;

	TUTRACE((TUTRACE_ERR, "frame_init: Entered(nfp_adapter_name=%s).\n", npf_adapter_name));

	if ((g_framer.adapter = PacketOpenAdapter((char *)npf_adapter_name)) == NULL) {
		TUTRACE((TUTRACE_ERR, "frame_init: PacketOpenAdapter failed. npf_adapter_name=%s!\n", npf_adapter_name));
		goto INIT_END;
	}

	
	if(PacketSetHwFilter(g_framer.adapter, NDIS_PACKET_TYPE_PROMISCUOUS)==FALSE){
		TUTRACE((TUTRACE_ERR, "frame_init: Failed to set filter to NDIS_PACKET_TYPE_PROMISCUOUS type!\n"));
		goto INIT_END;
	}

	if(PacketSetBuff(g_framer.adapter, PACKET32_KERNEL_BUFSIZE)==FALSE){
		TUTRACE((TUTRACE_ERR, "frame_init: Failed to set buffer!\n"));
		goto INIT_END;
	}

	// Allocate buffer for receving
	if ((g_framer.rxbuf = malloc(PACKET32_MAX_RX_BUFSIZE)) == NULL)
	{
		TUTRACE((TUTRACE_ERR, "frame_init: B1XS_ALLOC (rxbuf) failed!\n"));
		goto INIT_END;
	}
	memset(g_framer.rxbuf, 0, PACKET32_MAX_RX_BUFSIZE);

	// Allocate a winpcap packet structure object
	if ((g_framer.rxpkt = PacketAllocatePacket()) == NULL)
	{
		TUTRACE((TUTRACE_ERR, "frame_init: PacketAllocatePacket (rx) failed!\n"));
		goto INIT_END;
	}
	// pointer to current frame in the rx buffer (could be multiple frames)
	PacketInitPacket((LPPACKET)g_framer.rxpkt, g_framer.rxbuf, PACKET32_MAX_RX_BUFSIZE);
	
	// Allocate a packet for transmitting
	if ((g_framer.txpkt = PacketAllocatePacket()) == NULL)
	{
		TUTRACE((TUTRACE_ERR, "frame_init: PacketAllocatePacket (tx) failed!\n"));
		goto INIT_END;
	}

	TUTRACE((TUTRACE_ERR, "frame_init: Exiting. bRet=TRUE\n"));
	return TRUE;

INIT_END:
	frame_cleanup();

	TUTRACE((TUTRACE_ERR, "frame_init: Exiting. bRet=FALSE\n"));
	return FALSE;
}


brcm_wpscli_status Eap_ReadData(char * dataBuffer, uint32 * dataLen, int read_timeout, bool b_raw)
{
	brcm_wpscli_status status = 0;
	ether_header *frame;
	
	// for some reason, frame_data can be allocated in heap, possibly related to Winpcap
	char frame_data[WPS_EAP_DATA_MAX_LENGTH];  

	if (dataBuffer == NULL && dataLen == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	/* Read a frame from device and process it... */
	if(frame_recv_frame(frame_data, dataLen, read_timeout))	{
		frame = (ether_header *)frame_data;
		if(frame != NULL) {
			if(ether_get_type(frame->type) == EAPOL_ETHER_TYPE) {
				// Get EAP data with ether header trimmed
				if(b_raw) {
					// Get raw data packet without being trimmed
					memcpy(dataBuffer, frame, *dataLen);
				}
				else {
					// Get data with ether header trimmed
					*dataLen -= sizeof(ether_header);  // Get the data size
					memcpy(dataBuffer, frame+1, *dataLen);  
				}
				status = WPS_STATUS_SUCCESS;
			}
			else {
				status = WPS_STATUS_PKTD_NOT_EAPOL_PKT;
			}
		}
	}
	else {
		status = WPS_STATUS_PKTD_NO_PKT;  // Do not receive any type of packet
	}
	
	return status;
}

BOOL frame_recv_frame(char *data_msg, int *len, int recv_timeout)
{
	BOOL bRet = FALSE;
	DWORD dwLastErr = 0;
	struct bpf_hdr *hdr;
	uint8 *data_begin;
	uint32 off;

	// set timeout to recv_timeout seconds
	if(PacketSetReadTimeout(g_framer.adapter, recv_timeout)==FALSE)
		TUTRACE((TUTRACE_INFO, "frame_recv_frame: PacketSetReadTimeout failed. GetLastError()=%d\n", GetLastError()));

	/* Read frames (could be multiple of them) */
	if (g_framer.rxptr == NULL)
	{
		// Always check adapter existence as PacketReceivePacket will crash if NULL adapter is passed in
//		TUTRACE((TUTRACE_INFO, "frame_recv_frame: To call PacketReceivePacket. GetLastError()=%d\n", GetLastError()));
		bRet = PacketReceivePacket((LPADAPTER)g_framer.adapter, (LPPACKET)g_framer.rxpkt, TRUE);
		if(!bRet || ((LPPACKET)g_framer.rxpkt)->ulBytesReceived == 0)
		{
			DWORD dwErr = GetLastError();
//			TUTRACE((TUTRACE_INFO, "frame_recv_frame: PacketReceivePacket times out and no packet is received. GetLastError()=%d\n", dwErr));
			*len = 0;
			goto RXPKT_END;
		}
		g_framer.rxptr = g_framer.rxbuf;
	}

	/* Grab one frame */
	hdr = (struct bpf_hdr *)(g_framer.rxptr);
	data_begin = (uint8*)hdr + hdr->bh_hdrlen;
	*len = hdr->bh_datalen;

	/* update rxptr to point to the next frame */
	off = (char *)g_framer.rxptr - (char *)g_framer.rxbuf;
	off = Packet_WORDALIGN(off + hdr->bh_hdrlen + hdr->bh_datalen);
	if (off < ((LPPACKET)g_framer.rxpkt)->ulBytesReceived)
		g_framer.rxptr = (char *)g_framer.rxbuf + off;
	else
		g_framer.rxptr = NULL;

	if(*len > 0)
		memcpy(data_msg, data_begin, *len);
	else 
		goto RXPKT_END;

	bRet = TRUE;

RXPKT_END:
	return bRet;
}

BOOL frame_send_frame(uint8 *frame, int len)
{
	/* Setup filter */
	struct bpf_insn fltr[] = {
		/* Check Ethernet destination address At Offset 0 */
		/* 0  */BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 0),
		/* 1  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, htonl(*(uint32*)&g_framer.src_mac[0]), 0, 11),
		/* Check Ethernet destination address At Offset 4 */
		/* 2  */BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 4),
		/* 3  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, htons(*(uint16*)&g_framer.src_mac[4]), 0, 9),
		/* Check Ethernet type/length At Offset 12 */
		/* 4  */BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 12),
		/* 5  */BPF_JUMP(BPF_JMP|BPF_JGE|BPF_K, 1536, 5, 0),
		/* Check SNAP header At Offset 14 */
		/* 6  */BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 14),
		/* 7  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0xAAAA, 0, 5),
		/* 8  */BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 16),
		/* 9  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0x03000000, 0, 3),
		/* 10 */BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 20),
		/* 11 */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, EAPOL_ETHER_TYPE, 0, 1),
		/* Decision */
		/* 12 */BPF_STMT(BPF_RET|BPF_K, (uint32)-1),	/* Accept. it is for us */
		/* 13 */BPF_STMT(BPF_RET|BPF_K, 0)		/* Reject. it is not for us */
	};
	struct bpf_program prog = {sizeof(fltr)/sizeof(fltr[0]), fltr};

	TUTRACE((TUTRACE_INFO, "frame_send_frame: Entered. len=%d.\n", len));

	if(!PacketSetHwFilter((LPADAPTER)g_framer.adapter, g_dwFilter))
	{
		TUTRACE((TUTRACE_ERR, "%s : Failure in setting HW filter\n", __FUNCTION__));
		goto TXPKT_END;
	}

	if(!PacketSetBpf((LPADAPTER)g_framer.adapter, &prog)) {
		TUTRACE((TUTRACE_ERR, "%s : Failure in setting BPF\n", __FUNCTION__));
		goto TXPKT_END;
	}

	if(!PacketSetNumWrites((LPADAPTER)g_framer.adapter, 1))
	{
		TUTRACE((TUTRACE_ERR, "%s : PacketSetNumwrites failure\n", __FUNCTION__));
		goto TXPKT_END;
	}

	// Assoicate sending frame with the txpkt
	PacketInitPacket(g_framer.txpkt, (void *)frame, len);

	if(!PacketSendPacket((LPADAPTER)g_framer.adapter, g_framer.txpkt, TRUE))
	{
		TUTRACE((TUTRACE_ERR, "%s : PacketSendPacket failure.\n",__FUNCTION__));
		goto TXPKT_END;
	}

	TUTRACE((TUTRACE_INFO, "frame_send_frame: Exiting. Return TRUE.\n"));
	return TRUE;

TXPKT_END:
	TUTRACE((TUTRACE_ERR, "frame_send_frame: Exiting. Return FALSE.\n"));
	return FALSE;
}

void frame_cleanup()
{
	/* Shutdown PCAP */
	if (g_framer.rxpkt != NULL)
	{
		PacketFreePacket((LPPACKET)g_framer.rxpkt);
		g_framer.rxpkt = NULL;
	}
	if (g_framer.rxbuf != NULL)
	{
		free(g_framer.rxbuf);
		g_framer.rxbuf = NULL;
	}
	if (g_framer.txpkt != NULL)
	{
		PacketFreePacket((LPPACKET)g_framer.txpkt);
		g_framer.txpkt = NULL;
	}
	if (g_framer.adapter != NULL)
	{
		PacketCloseAdapter((LPADAPTER)g_framer.adapter);
		g_framer.adapter = NULL;
	}
}
