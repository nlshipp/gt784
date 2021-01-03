/*
 * DPT (Direct Packet Transfer) related header file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_dpt.h,v 1.1 2010/08/05 21:59:02 ywu Exp $
*/

#ifndef _wlc_dpt_h_
#define _wlc_dpt_h_

#define wlc_dpt_attach(a) (dpt_info_t *)0x0dadbeef
#define	wlc_dpt_detach(a) do {} while (0)
#define	wlc_dpt_cap(a) FALSE
#define	wlc_dpt_update_pm_all(a, b) do {} while (0)
#define wlc_dpt_pm_pending(a) FALSE
#define wlc_dpt_process_prbreq(a, b, c, d, e, f) do {} while (0)
#define wlc_dpt_process_prbresp(a, b, c, d, e, f) do {} while (0)
#define wlc_dpt_query(a, b, c) NULL
#define wlc_dpt_used(a, b) do {} while (0)
#define wlc_dpt_rcv_pkt(a, b, c, d) do {} (FALSE)
#define wlc_dpt_set(a, b) do {} while (0)
#define wlc_dpt_cleanup(a) do {} while (0)
#define wlc_dpt_free_scb(a, b) do {} while (0)
#define wlc_dpt_wpa_passhash_done(a, b) do {} while (0)
#define wlc_dpt_port_open(a, b) do {} while (0)
#define wlc_dpt_write_ie(a, b, c) (0)

#endif /* _wlc_dpt_h_ */
