/*
 * Broadcom UPnP module vxWorks OS dependent implementation
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp_vx_osl.c,v 1.1 2010/08/05 21:58:59 ywu Exp $
 */

#include <ctype.h>
#include <endLib.h>
#include <sockLib.h>
#include <ioLib.h>

#include <upnp.h>

extern int end_ioctl(char *name, int request, void *buf);

/* OS related functions needed by upnp.c */
static int
getAssignedAddr(char *ifname, unsigned long *ipaddr, unsigned long *subnet)
{
	char tmpbuf[256];

	if (ifAddrGet(ifname, tmpbuf) == OK) {

		*ipaddr = (unsigned long)inet_addr(tmpbuf);

		if (ifMaskGet(ifname, (int*)subnet) == OK)
			return (OK);
	}

	return (ERROR);
}

int
upnp_osl_ifaddr(const char *ifname, struct in_addr *inaddr)
{
	int status = -1;
	struct in_addr mask = {0};

	memset(inaddr, 0, sizeof(*inaddr));
	if (getAssignedAddr((char*)ifname, &inaddr->s_addr, &mask.s_addr) != ERROR) {
		/*
		 * DHCP client assigns a bogus IP address
		 * 10.0.[0-15].[0-255]/255.0.0.0 during init)
		 */
		if ((inaddr->s_addr & 0xfffff000) == htonl(0xa0000000) &&
			mask.s_addr == htonl(0xff000000)) {
			inaddr->s_addr = 0;
		}
		else
			status = 0;
	}

	return status;
}

int
upnp_osl_netmask(const char *ifname, struct in_addr *inaddr)
{
	int status = -1;
	struct in_addr addr = {0};

	memset(inaddr, 0, sizeof(*inaddr));
	if (getAssignedAddr((char*)ifname, &addr.s_addr, &inaddr->s_addr) != ERROR) {
		/*
		 * DHCP client assigns a bogus IP address
		 * 10.0.[0-15].[0-255]/255.0.0.0 during init)
		 */
		if ((addr.s_addr & 0xfffff000) == htonl(0xa0000000) &&
			inaddr->s_addr == htonl(0xff000000)) {
			inaddr->s_addr = 0;
		}
		else
			status = 0;
	}

	return status;
}

int
upnp_osl_hwaddr(const char *ifname, char *mac)
{
	if (end_ioctl((char*)ifname, EIOCGADDR, (caddr_t)mac) == ERROR) {
		upnp_syslog(LOG_ERR, "%s: end_ioctl(%s) failed\n", __FUNCTION__, ifname);
		return -1;
	}

	return 0;
}

/* Create a udp socket with a specific ip and port */
int
upnp_open_udp_socket(struct in_addr addr, unsigned short port)
{
	int s;
	struct sockaddr_in sin;
	int val = 1;

	if ((s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket failed: %s", strerror(errno));
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char *)&val, sizeof(val)) == -1) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket: setsockopt SO_REUSEPORT failed");
		close(s);
		return -1;
	}

	/* Bind to the server port */
	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr = addr;
	if (bind(s, (struct sockaddr *)&sin, sizeof(struct sockaddr)) == -1) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket: bind failed: %s", strerror(errno));
		close(s);
		return -1;
	}

	return s;
}

int
upnp_open_tcp_socket(struct in_addr addr, unsigned short port)
{
	int s;
	int reuse = 1;
	struct sockaddr_in sin;
	int max_waits = 10;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket failed: %s", strerror(errno));
		return -1;
	}

	if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (char*)&reuse, sizeof(reuse)) < 0) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket: setsockopt SO_REUSEPORT failed");
		close(s);
		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr = addr;
	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0 || listen (s, max_waits) < 0) {
		upnp_syslog(LOG_ERR, "upnp_open_udp_socket: bind failed: %s", strerror(errno));
		close(s);
		return -1;
	}

	return s;
}
