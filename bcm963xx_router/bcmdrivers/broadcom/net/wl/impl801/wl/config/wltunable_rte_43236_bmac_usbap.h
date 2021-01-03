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
 * $Id: wltunable_rte_43236_bmac_usbap.h,v 1.1 2010/08/05 21:59:00 ywu Exp $
 *
 * wl driver tunables for 43236 rte dev
 */

#define D11CONF		0x1000000	/* D11 Core Rev 24 */
#define GCONF		0		/* No G-Phy */
#define ACONF		0		/* No A-Phy */
#define HTCONF		0		/* No HT-Phy */
#define NCONF		0x100		/* N-Phy Rev 8 */
#define LPCONF		0		/* No LP-Phy */
#define SSLPNCONF	0		/* No SSLPN-Phy */
#define LCNCONF		0		/* No LCN-Phy */

#define CTFPOOLSZ       128	/* max buffers in ctfpool. 384 caused out-of-memory problems */

#define NTXD		128	/* THIS HAS TO MATCH with HIGH driver tunable, AMPDU/rpc_agg */
#define NRXD		32
#define NRXBUFPOST	16
#define WLC_DATAHIWAT	10
#define RXBND		16
#define NRPCTXBUFPOST	128
#define MEM_RESERVED    (6*2048)