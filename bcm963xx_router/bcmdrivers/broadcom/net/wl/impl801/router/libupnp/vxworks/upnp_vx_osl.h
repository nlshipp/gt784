/*
 * Broadcom UPnP module vxWorks OS dependent include file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp_vx_osl.h,v 1.1 2010/08/05 21:58:59 ywu Exp $
 */

#ifndef __LIBUPNP_VX_OSL_H__
#define __LIBUPNP_VX_OSL_H__

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <netinet/in.h>
#include <ioLib.h>
#include <sockLib.h>
#include <taskLib.h>
#include <end.h>
#include <time.h>
#include <shutils.h>

extern int sysClkRateGet(void);

#define upnp_syslog(...) {}
#define upnp_printf printf
#define	upnp_pid() taskIdSelf()
#define	upnp_sleep(n) taskDelay(sysClkRateGet() * n)

int upnp_osl_ifaddr(const char *ifname, struct in_addr *inaddr);
int upnp_osl_netmask(const char *ifname, struct in_addr *inaddr);
int upnp_osl_hwaddr(const char *ifname, char *mac);
int upnp_open_udp_socket(struct in_addr addr, unsigned short port);
int upnp_open_tcp_socket(struct in_addr addr, unsigned short port);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __LIBUPNP_VX_OSL_H__ */