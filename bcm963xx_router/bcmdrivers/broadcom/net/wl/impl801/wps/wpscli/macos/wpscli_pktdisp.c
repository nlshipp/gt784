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
 * Description: Implement Linux packet dispatcher
 *
 */
#include <wpscli_osl.h>
#include <stdarg.h>
#include <tutrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/bpf.h>
#include <fcntl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <proto/eapol.h>
#include <proto/ethernet.h>

#define ETH_8021X_PROT	0x888e

extern char* ether_ntoa(const struct ether_addr *addr);
extern char *wpscli_get_interface_name();

#ifdef _TUDEBUGTRACE
extern void wpscli_print_buf(char *text, unsigned char *buff, int buflen);
#endif

static char if_name[20] = {0};
static int eap_fd = -1; /* descriptor to raw socket  */
static uint bpf_buf_len = 0;

//static int ifindex = -1; /* interface index */
static uint8 peer_mac[6] = { 0 };

// 
// definitions related to packet dispatcher
//
// open/init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr)
{
	int fd;
	struct ifreq ifr;
	//	struct sockaddr_ll ll;
	int err;
	int n = 0;
	char device[sizeof "/dev/bpf0000000000"];

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Entered.\n"));

	// Get interface name
	strcpy(if_name, wpscli_get_interface_name());

	if(peer_addr == NULL) {
		printf("Peer MAC address is not specified.\n");
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if (!if_name[0]) {
		printf("Wireless Interface not specified.\n");
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open. if_name=%s peer_addr=[%02X:%02X:%02X:%02X:%02X:%02X]\n", 
		if_name, peer_addr[0], peer_addr[1], peer_addr[2], peer_addr[3], peer_addr[4], peer_addr[5]));

	/*
	 * Go through all the minors and find one that isn't in use.
	 */
	do {
		(void)snprintf(device, sizeof(device), "/dev/bpf%d", n++);
		fd = open(device, O_RDWR);
	} while (fd < 0 && errno == EBUSY);

	{
		 int buf_len = 1;
		 struct bpf_program {
			 int bf_len;
			 struct bpf_insn *bf_insns;
		 } bpfp;

		 struct bpf_insn insns[] = {
			 BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
			 BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, htons(ETH_8021X_PROT), 0, 1),
			 BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
			 BPF_STMT(BPF_RET+BPF_K, 0),
		 };

		 // activate immediate mode (therefore, buf_len is initially set to "1")
		 if( ioctl( fd, BIOCIMMEDIATE, &buf_len ) == -1 )
			 return( -1 );
		 
		 // request buffer length
		 if( ioctl( fd, BIOCGBLEN, &buf_len ) == -1 )
			 return( -1 );

		 printf("buflen %d\n", buf_len);

		 //		 osl_hdl->bpf_buf_len = buf_len;
		 bpf_buf_len = buf_len;
		 bpfp.bf_len = 4;
		 bpfp.bf_insns = insns;

		 // request buffer length
		 if( ioctl( fd, BIOCSETF, &bpfp ) == -1 )
			 return WPS_STATUS_PKTD_INIT_FAIL;
	}

	eap_fd = fd;

	// Set peer mac address
	if(peer_addr)
		memcpy(peer_mac, peer_addr, 6);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	err = ioctl(eap_fd, BIOCSETIF, &ifr);
	if (err < 0) {
		close(eap_fd);
		eap_fd = -1;
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Get interface index failed\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

// close/un-init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_close()
{
	// Close the socket
	if (eap_fd != -1)
		close(eap_fd);
        return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_set_peer_addr(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Entered.\n"));

	if(peer_addr == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_peer_mac: Exiting. NULL peer_addr is passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	memcpy(peer_mac, peer_addr, 6);

	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}
typedef struct ether_header ethernet_frame;

// waiting for eap data
brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool b_raw)
{
	int recvBytes = 0;
	fd_set fdvar;
	struct timeval tv_timeout;
	int buf_len = bpf_buf_len;
	struct bpf_hdr* bpf_buf = NULL;
	struct bpf_hdr* bpf_packet;
	ethernet_frame* frame;

//	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Entered.\n"));

	bpf_buf = (struct bpf_hdr*)malloc(bpf_buf_len);

	if (buf == NULL || len == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_INVALID_NULL_PARAM\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	tv_timeout.tv_sec = timeout/1000;  // Convert milli-second to second 
	tv_timeout.tv_usec = 0;

	FD_ZERO(&fdvar);
	FD_SET(eap_fd, &fdvar);
	if (select(eap_fd + 1, &fdvar, NULL, NULL, &tv_timeout) < 0) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. l2 select recv failed\n"));
		return WPS_STATUS_PKTD_SYSTEM_FAIL;
	}

	if (FD_ISSET(eap_fd, &fdvar)) {
		memset( bpf_buf, 0, buf_len );

		if( ( recvBytes = read( eap_fd, bpf_buf, buf_len ) ) > 0 ) {
			// read all packets that are included in bpf_buf. BPF_WORDALIGN is used
			// to proceed to the next BPF packet that is available in the buffer.

			bpf_packet = (struct bpf_hdr*)(bpf_buf);
			// do something with the Ethernet frame
			// [...]
			frame = (ethernet_frame*)((char*) bpf_packet + bpf_packet->bh_hdrlen);
				
			/* process rx frames for Wifi Action Frames */
			*len = bpf_packet->bh_caplen;
			memcpy(buf, frame, bpf_packet->bh_caplen);
			free(bpf_buf);
			return WPS_STATUS_SUCCESS;
		} else if (recvBytes == -1) {
			printf("UDP recv failed; recvBytes = %d\n", recvBytes);
			TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_PKTD_NO_PKT\n"));
			return WPS_STATUS_PKTD_NO_PKT;
		}

#ifdef _TUDEBUGTRACE
		TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: EAPOL packet received. *len=%d\n", *len));
		wpscli_print_buf("WPS rx", (unsigned char *)buf, *len);
#endif

//	TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_PKTD_NO_PKT. FD_ISSET failed.\n"));
	}
	return WPS_STATUS_PKTD_NO_PKT;
}

// send a packet
brcm_wpscli_status wpscli_pktdisp_send_packet(char *dataBuffer, uint32 dataLen)
{
	int sentBytes = 0;

	TUTRACE((TUTRACE_INFO, "Entered: wpscli_pktdisp_send_packet. dataLen=%d peer_addr=[%02X:%02X:%02X:%02X:%02X:%02X]\n", 
		dataLen, peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]));

#ifdef _TUDEBUGTRACE
		wpscli_print_buf("WPS tx", (unsigned char *)dataBuffer, dataLen);
#endif

	if ((!dataBuffer) || (!dataLen)) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. Invalid Parameters\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	sentBytes = write(eap_fd, dataBuffer, dataLen);

	if (sentBytes != (int32) dataLen) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. L2 send failed; sentBytes = %d\n", sentBytes));
		return WPS_STATUS_PKTD_SEND_PKT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Exiting. Succeeded.\n"));
	return WPS_STATUS_SUCCESS;
}
