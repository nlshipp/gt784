/*
 * Wireless network adapter utilities (linux-specific)
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wl_bsd.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/types.h>

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

#include <typedefs.h>
#include <wlioctl.h>
#include <wlutils.h>
#include <shutils.h>


int
wl_ioctl(char *name, int cmd, void *buf, int len)
{
	struct ifreq ifr;
	wl_ioctl_t ioc;
	int ret = 0;
	int s;
	char nv_name[IFNAMSIZ], bwl_name[IFNAMSIZ];
	int unit;

	if (osifname_to_nvifname(name, nv_name, sizeof(nv_name)) != 0)
		return -1;

	/* NVRAM I/F names for wireless devices are always wlX or wlX.Y */
	if (strncmp(nv_name, "wl", 2))
		return -1;

	if (get_ifname_unit(nv_name, &unit, NULL) != 0)
		return -1;
	snprintf(bwl_name, IFNAMSIZ, "bwl%d", unit);

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	/* do it */
	ioc.cmd = cmd;
	ioc.buf = buf;
	ioc.len = len;

	/* initializing the remaining fields */
	ioc.set = FALSE;
	ioc.used = 0;
	ioc.needed = 0;

	/* strncpy(ifr.ifr_name, name, IFNAMSIZ); */
	strncpy(ifr.ifr_name, bwl_name, IFNAMSIZ);
	ifr.ifr_data = (caddr_t) &ioc;
	if ((ret = ioctl(s, SIOCDEVPRIVATE, &ifr)) < 0) {
		/* cprintf("%s: cmd=%d\n", __func__, cmd); */
		if (cmd != WLC_GET_MAGIC)
			perror(ifr.ifr_name);
	}

	/* cleanup */
	close(s);
	/* printf("%s(%d) ret %d\n", __func__, cmd, ret); */
	return ret;
}


int
wl_hwaddr(char *name, unsigned char *hwaddr)
{
	struct ifreq ifr;
	int ret = 0;
	int s;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return errno;
	}

	/* do it */
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	if ((ret = ioctl(s, SIOCGIFHWADDR, &ifr)) == 0)
		memcpy(hwaddr, ifr.ifr_addr.sa_data, ETHER_ADDR_LEN);

	/* cleanup */
	close(s);
	return ret;
}
