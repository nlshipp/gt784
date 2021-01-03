/*
 * Broadcom 802.11abg Networking Device Driver Configuration file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wltunable_rte_4322_bmac_usbap.h,v 1.1 2010/08/05 21:59:00 ywu Exp $
 *
 * wl driver tunables for 4322 rte dev
 */

#define D11CONF		0x10000		/* D11 Core Rev 16 */
#define GCONF		0		/* No G-Phy */
#define ACONF		0		/* No A-Phy */
#define HTCONF		0		/* No HT-Phy */
#define NCONF		0x10		/* N-Phy Rev 4 (use 0x18 for Rev 3 & 4) */
#define LPCONF		0		/* No LP-Phy */
#define SSLPNCONF	0		/* No SSLPN-Phy */
#define LCNCONF		0		/* No LCN-Phy */

#define NTXD		32	/* THIS HAS TO MATCH with HIGH driver tunable */
#define NRXD		64
#define NRXBUFPOST	32
#define WLC_DATAHIWAT	10
#define RXBND		16
#define NRPCTXBUFPOST	32
#define MEM_RESERVED    (6*2048)
