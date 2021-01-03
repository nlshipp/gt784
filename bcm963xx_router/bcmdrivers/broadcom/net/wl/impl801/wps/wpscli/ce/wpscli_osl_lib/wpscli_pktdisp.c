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
#include <Ntddndis.h>
#include <packet32.h>
#include "wpscli_osl_common.h"
#include <tutrace.h>

/* enable structure packing */
#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

#define WPS_ENROLLEE_FAILURE 0

/* position of wps message in eap packet */
#define WPS_MSGTYPE_OFFSET  9

/* Frame Polling Interval */
#define EAPOL_FRAME_POLL_TIMEOUT	7000	/* ms */
#define EAPOL_FRAME_MAX_SIZE	2048	/* bytes */
#define XSUPPD_MAX_PACKET	4096

#define XSUPPD_ETHADDR_LENGTH	6
#define XSUPPD_MAX_FILENAME	256
#define XSUPPD_MAX_NETNAME	64
/* memory manipulation */
#define B1XS_ALLOC(s)	malloc(s)
#define B1XS_FREE(m)	free(m)
#define B1XS_COPY(d,s,l)	memcpy(d,s,l)
#define B1XS_ZERO(m,l)	memset(m,0,l)

/* set/get type field in ether header */
#define ether_get_type(p)	(((p)[0] << 8) + (p)[1])
#define ether_set_type(p, t)	((p)[0] = (uint8)((t) >> 8), (p)[1] = (uint8)(t))

DWORD g_dwFilter = NDIS_PACKET_TYPE_DIRECTED | NDIS_PACKET_TYPE_MULTICAST | NDIS_PACKET_TYPE_ALL_MULTICAST |
			NDIS_PACKET_TYPE_BROADCAST | NDIS_PACKET_TYPE_ALL_LOCAL;

static uint32 Eap_OSInit(uint8 *bssid);
static void Eap_OSDeInit();
static uint32 Eap_ReadData(char * dataBuffer, uint32 * dataLen, struct timeval timeout, bool bRaw);

static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
static const __int64 SECS_TO_100NS = 10000000; /* 10^7 */

extern uint8* getMacAddress();
extern wchar_t* getAdapterName();
extern char* getNetwork();
/* EAPOL Ethernet Type */
#define EAPOL_ETHER_TYPE	0x888e

/* 
   we need to set the ifname before anything else. 
 */
/* ethernet header */
typedef struct
{
	uint8 dest[XSUPPD_ETHADDR_LENGTH];
	uint8 src[XSUPPD_ETHADDR_LENGTH];
	uint8 type[2];
} PACKED ether_header;

typedef struct xsup
{
	char eapol_device[XSUPPD_MAX_FILENAME]; /* Interface name */

	/* wireless device/framer */
	struct
	{
		uint32 timeout;
		void *adapter;
		void *rxbuf;
		void *rxpkt;
		void *rxptr;
		void *txpkt;
	} framer;
	void *device;

	uint8 eapol_bssid[XSUPPD_ETHADDR_LENGTH];  /* MAC address of authenticator */
	char eapol_ssid[XSUPPD_MAX_NETNAME];  /* network name */

	char padding1[2];
	uint8 source_mac[XSUPPD_ETHADDR_LENGTH];
	char padding2[2];
	uint8 dest_mac[XSUPPD_ETHADDR_LENGTH];

	/* incoming packet */
	/*uint8_t in[XSUPPD_MAX_PACKET];*/
	uint8 *in;
	int in_size;

} xsup_t;

xsup_t g_EAPParams;
uint8 *frame_recv_frame(xsup_t *xsup, int *len);
int frame_send_frame(xsup_t *xsup, uint8 *frame, int len);
int frame_init(xsup_t *xsup, wchar_t *device, uint8 *mac, int size, int timeout);
int frame_cleanup(xsup_t *xsup);
int eapol_wireless_init(xsup_t *xsup, wchar_t *device, uint8 *bssid, char *ssid);

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


#ifndef FRAME_DEBUG
#define FRAME_DEBUG 1
#endif

#ifndef FRAME_DUMP
#define FRAME_DUMP	0
#endif

#ifndef PACKET32_MAX_RX_BUFSIZE
#define PACKET32_MAX_RX_BUFSIZE	(64*1024)
#endif

#ifdef UNDER_CE
#ifndef Packet_WORDALIGN
#define Packet_WORDALIGN PACKET_WORDALIGN
#endif
#endif

#if ( _WIN32_WCE >= 101 )

#define atow(strA,strW,lenW) \
MultiByteToWideChar(CP_ACP,0,strA,-1,strW,lenW)

#define wtoa(strW,strA,lenA) \
WideCharToMultiByte(CP_ACP,0,strW,-1,strA,lenA,NULL,NULL)

#else /* _WIN32_WCE >= 101 */

/*
MultiByteToWideChar() and WideCharToMultiByte() not supported on Windows CE 1.0
*/
int atow(char *strA, wchar_t *strW, int lenW);
int wtoa(wchar_t *strW, char *strA, int lenA);

#endif /* _WIN32_WCE >= 101 */

#if defined(DEBUG) || defined(_DEBUG)
#define DBGPRINT(x)	printf x
#else
#define DBGPRINT(x)
#endif

static int reload_driver();

void printFramer()
{
	DBGPRINT(("eapol_device = %s\n", g_EAPParams.eapol_device));
	DBGPRINT(("eapol_bssid = %x:%x:%x:%x:%x:%x\n",
	                              g_EAPParams.eapol_bssid[0],
		                          g_EAPParams.eapol_bssid[1],
		                          g_EAPParams.eapol_bssid[2],
		                          g_EAPParams.eapol_bssid[3],
		                          g_EAPParams.eapol_bssid[4],
		                          g_EAPParams.eapol_bssid[5]));
	DBGPRINT(("eapol_ssid = %s\n", g_EAPParams.eapol_ssid));
	DBGPRINT(("source_mac = %x:%x:%x:%x:%x:%x\n",
	                              g_EAPParams.source_mac[0],
		                          g_EAPParams.source_mac[1],
		                          g_EAPParams.source_mac[2],
		                          g_EAPParams.source_mac[3],
		                          g_EAPParams.source_mac[4],
		                          g_EAPParams.source_mac[5]));
	DBGPRINT(("dest_mac = %x:%x:%x:%x:%x:%x\n",
	                              g_EAPParams.dest_mac[0],
		                          g_EAPParams.dest_mac[1],
		                          g_EAPParams.dest_mac[2],
		                          g_EAPParams.dest_mac[3],
		                          g_EAPParams.dest_mac[4],
		                          g_EAPParams.dest_mac[5]));
}

uint32 Eap_OSInit(uint8 *bssid)
{
	uint32 status = WPS_STATUS_PKTD_INIT_FAIL;

	if (g_EAPParams.framer.adapter != NULL)
	{
		DBGPRINT(("Eap_OSInit :: WARNING : Already initialized. Use Eap_OSDeInit before re-initilizating\n"));
		return WPS_STATUS_SUCCESS;
	}

	if (!reload_driver())
	{
		DBGPRINT(("Eap_OSInit :: Failed to reload driver\n"));
		return status;
	}

	memset(&g_EAPParams, 0, sizeof(g_EAPParams));

	status = frame_init(&g_EAPParams, getAdapterName(), getMacAddress(), EAPOL_FRAME_MAX_SIZE, 
		EAPOL_FRAME_POLL_TIMEOUT);
	if (status != WPS_STATUS_SUCCESS)
	{
		DBGPRINT(("eapol_init: Couldn't initalize frame device!\n"));
		goto done;
	}

	/* Init wireless calls */
	if (eapol_wireless_init(&g_EAPParams, getAdapterName(), bssid, getNetwork()) != WPS_STATUS_SUCCESS)
	{
		DBGPRINT(( "eapol_init: Couldn't initalize wireless extensions!\n"));
		goto done;
	}

	printFramer();
	status = WPS_STATUS_SUCCESS;
done:
	return status;
}

void Eap_OSDeInit()
{
	frame_cleanup(&g_EAPParams);
	memset(&g_EAPParams, 0, sizeof(g_EAPParams));
}

uint32 Eap_ReadData(char * dataBuffer, uint32 * dataLen, struct timeval timeout, bool b_raw)
{
	ether_header *frame;
	if (dataBuffer && (! dataLen))
	{
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	/* Read a frame from device and process it... */
	frame  = (ether_header *)frame_recv_frame(&g_EAPParams, dataLen);
	
	if (frame != NULL && ether_get_type(frame->type) == EAPOL_ETHER_TYPE)
	{
		memset(dataBuffer, 0, *dataLen);

		if(b_raw) {
			// Get raw data packet without being trimmed
			memcpy(dataBuffer, frame, *dataLen);
		} else {
			*dataLen -= sizeof(ether_header);
			memcpy(dataBuffer,frame+1, *dataLen);
		}
#ifdef _TUDEBUGTRACE
//	wpscli_print_buf("WPS rx", dataBuffer, *dataLen);
#endif
		return WPS_STATUS_SUCCESS;
	}
	else
	{
		if (frame == NULL)
		{
			DBGPRINT(("No packet received.\n" ));
			return WPS_STATUS_PKTD_NO_PKT;
		}
		else
		{
			DBGPRINT(("Received failure :incorrect frame type %x\n",ether_get_type(frame->type)));
			return WPS_STATUS_PKTD_NOT_EAPOL_PKT;
		}
	}
}

unsigned long get_current_time()
{
	SYSTEMTIME systemTime;
	FILETIME fileTime;
	__int64 UnixTime;

	GetSystemTime( &systemTime );
	SystemTimeToFileTime( &systemTime, &fileTime );


	/* get the full win32 value, in 100ns */
	UnixTime = ((__int64)fileTime.dwHighDateTime << 32) + 
		fileTime.dwLowDateTime;


	/* convert to the Unix epoch */
	UnixTime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);

	UnixTime /= SECS_TO_100NS; /* now convert to seconds */

	return (long)(UnixTime);
}

/*
* frame_recv_frame(int msec) -- returns a pointer to a frame if there is one.
*  NULL if there isn't.  msec is how long to sleep before returning.
*/
uint8 *frame_recv_frame(xsup_t *xsup, int *len)
{
	struct bpf_hdr *hdr;
	uint32 off;

	/* Read frames (could be multiple of them) */
	if (xsup->framer.rxptr == NULL)
	{
		PacketReceivePacket((LPADAPTER)xsup->framer.adapter, (LPPACKET)xsup->framer.rxpkt, TRUE);
		if (xsup->framer.rxpkt==NULL || ((LPPACKET)xsup->framer.rxpkt)->ulBytesReceived == 0)
		{
			DBGPRINT(("Timeout....\n"));

			*len = 0;
			return NULL;
		}
		xsup->framer.rxptr = xsup->framer.rxbuf;
	}
	
	/* Grab one frame */
	hdr = (struct bpf_hdr *)xsup->framer.rxptr;
	xsup->in = (uint8*)hdr + hdr->bh_hdrlen;
	*len = xsup->in_size = hdr->bh_datalen;
	
	/* update rxptr to point to the next frame */
	off = (char *)xsup->framer.rxptr - (char *)xsup->framer.rxbuf;
	off = Packet_WORDALIGN(off + hdr->bh_hdrlen + hdr->bh_datalen);
	if (off < ((LPPACKET)xsup->framer.rxpkt)->ulBytesReceived)
		xsup->framer.rxptr = (char *)xsup->framer.rxbuf + off;
	else
		xsup->framer.rxptr = NULL;

	return xsup->in;
}

/* Send a frame. */
int frame_send_frame(xsup_t *xsup, uint8 *frame, int len)
{
	int status = WPS_STATUS_SUCCESS;

	PacketInitPacket((LPPACKET)xsup->framer.txpkt, (void *)frame, len);
	if(!PacketSendPacket((LPADAPTER)xsup->framer.adapter, (LPPACKET)xsup->framer.txpkt, TRUE)) {
		DBGPRINT(("%s : PacketSendPacket failure.\n",__FUNCTION__));
		status = WPS_STATUS_PKTD_SEND_PKT_FAIL;
	}
	return status; 
}

int frame_init(xsup_t *xsup, wchar_t *device, uint8 *mac, int size, int timeout)
{	
	if (device == NULL)
	{
		DBGPRINT(("frame_init: no device specified!\n"));
		goto exit0;
	}

	/* Stash a copy of the device name and address. */
	wtoa(device,xsup->eapol_device,XSUPPD_MAX_FILENAME);
	memcpy(xsup->source_mac, mac, XSUPPD_ETHADDR_LENGTH);

	if ((xsup->framer.adapter = PacketOpenAdapter((LPTSTR)device)) == NULL)
	{
		DBGPRINT(("frame_init: PacketOpenAdapter failed!\n"));
		goto exit0;
	}

	/* Allocate buffer/packet for receving */
	if ((xsup->framer.rxbuf = B1XS_ALLOC(PACKET32_MAX_RX_BUFSIZE)) == NULL)
	{
		DBGPRINT(("frame_init: B1XS_ALLOC (rxbuf) failed!\n"));
		goto exit1;
	}
	memset(xsup->framer.rxbuf, 0, PACKET32_MAX_RX_BUFSIZE);
	if ((xsup->framer.rxpkt = PacketAllocatePacket()) == NULL)
	{
		DBGPRINT(("frame_init: PacketAllocatePacket (rx) failed!\n"));
		goto exit2;
	}
	PacketInitPacket((LPPACKET)xsup->framer.rxpkt, xsup->framer.rxbuf, PACKET32_MAX_RX_BUFSIZE);
	/* pointer to current frame in the rx buffer (could be multiple frames) */
	xsup->framer.rxptr = NULL;
	/* Allocate a packet for transmitting */
	if ((xsup->framer.txpkt = PacketAllocatePacket()) == NULL)
	{
		DBGPRINT(("frame_init: PacketAllocatePacket (tx) failed!\n"));
		goto exit3;
	}

	/* Setup filter */
	{
		struct bpf_insn fltr[] = {
			/* Check Ethernet destination address At Offset 0 */
			/* 0  */BPF_STMT(BPF_LD|BPF_W|BPF_ABS, 0),
			/* 1  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, htonl(*(uint32*)&xsup->source_mac[0]), 0, 11),
			/* Check Ethernet destination address At Offset 4 */
			/* 2  */BPF_STMT(BPF_LD|BPF_H|BPF_ABS, 4),
			/* 3  */BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, htons(*(uint16*)&xsup->source_mac[4]), 0, 9),
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
		if(!PacketSetHwFilter((LPADAPTER)xsup->framer.adapter, g_dwFilter)) {
			DBGPRINT(("%s : Failure in setting HW filter to promiscuous\n", __FUNCTION__));
		}
		if(!PacketSetBpf((LPADAPTER)xsup->framer.adapter, &prog)) {
			DBGPRINT(("%s : Failure in setting BPF\n", __FUNCTION__));
		}

		if(!PacketSetNumWrites((LPADAPTER)xsup->framer.adapter, 1)) {
			DBGPRINT(("%s : PacketSetNumwrites failure\n", __FUNCTION__));
		}

		/* Set adapter to normal mode */
		//PacketSetHwFilter((LPADAPTER)xsup->framer.adapter, NDIS_PACKET_TYPE_ALL_LOCAL);
		/* Set read timeout */
		if(!PacketSetReadTimeout((LPADAPTER)xsup->framer.adapter, EAPOL_FRAME_POLL_TIMEOUT)) {
			DBGPRINT(("%s : Failure in setting read time out\n", __FUNCTION__));
		}
		/* Set capture buffer size */
		if(!PacketSetBuff((LPADAPTER)xsup->framer.adapter, PACKET32_MAX_RX_BUFSIZE)) {
			DBGPRINT(("%s : Failure in setting buff\n", __FUNCTION__));
		}
	}

	return WPS_STATUS_SUCCESS;

exit3:
	PacketFreePacket((LPPACKET)xsup->framer.rxpkt);
exit2:
	B1XS_FREE(xsup->framer.rxbuf);
exit1:
	PacketCloseAdapter((LPADAPTER)xsup->framer.adapter);
exit0:
	return WPS_STATUS_PKTD_INIT_FAIL;
}

int frame_cleanup(xsup_t *xsup)
{
	xsup->eapol_device[0] = 0;

	/* Shutdown PCAP */
	if (xsup->framer.rxpkt != NULL)
	{
		PacketFreePacket((LPPACKET)xsup->framer.rxpkt);
		xsup->framer.rxpkt = NULL;
	}
	if (xsup->framer.rxbuf != NULL)
	{
		B1XS_FREE(xsup->framer.rxbuf);
		xsup->framer.rxbuf = NULL;
	}
	if (xsup->framer.txpkt != NULL)
	{
		PacketFreePacket((LPPACKET)xsup->framer.txpkt);
		xsup->framer.txpkt = NULL;
	}
	if (xsup->framer.adapter != NULL)
	{
		PacketCloseAdapter((LPADAPTER)xsup->framer.adapter);
		xsup->framer.adapter = NULL;
	}
	return WPS_STATUS_SUCCESS;
}

int eapol_wireless_init(xsup_t *xsup, wchar_t *device, uint8 *bssid, char *ssid)
{	
	strcpy(xsup->eapol_ssid, ssid);
	memcpy(&xsup->eapol_bssid, bssid, XSUPPD_ETHADDR_LENGTH);
	memcpy(xsup->dest_mac, xsup->eapol_bssid, XSUPPD_ETHADDR_LENGTH);
	return WPS_STATUS_SUCCESS;
}

int ConfigureWindowsZeroConfig(int bEnable)
{
	return PacketSetWirelessZeroConfig(bEnable) == TRUE ? 1 : 0;
}

int reload_driver()
{
	int ret = 0;
	PacketUnloadDriver();
	if (PacketLoadDriver()) {
		ret = 1;
	}
	return ret;
}

// open/init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Entered.\n"));
	return Eap_OSInit((uint8 *)peer_addr);
}

// close/un-init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_close()
{
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_close: Entered.\n"));

	Eap_OSDeInit();

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

	memcpy(&g_EAPParams.dest_mac[0], peer_addr, 6);

	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool bRaw)
{
	brcm_wpscli_status status;
    struct timeval time;
    
    time.tv_sec = timeout;
    time.tv_usec = 0;

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Entered.\n"));

	if(buf == NULL || len == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	status = Eap_ReadData(buf, len, time, bRaw);
	
	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Exiting. status=%d\n", status));
	return status;
}

// send a packet
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
//	wpscli_print_buf("WPS tx", dataBuffer, dataLen);
#endif

	status = frame_send_frame(&g_EAPParams, dataBuffer, dataLen);

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Exiting. status=%d\n", status));
	return status;
}
