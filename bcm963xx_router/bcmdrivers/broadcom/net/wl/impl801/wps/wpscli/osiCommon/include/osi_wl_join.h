/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: osi_wl_join.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 *
 */
#ifndef _OSI_WL_JOIN_H_
#define _OSI_WL_JOIN_H_

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------
 *
 *
 *--------------------------------------------------------------------
*/
int wpsosi_common_apply_sta_security(uint8 wsec);


/*--------------------------------------------------------------------
 *
 *
 *--------------------------------------------------------------------
*/
brcm_wpscli_status wpsosi_join_network(char* ssid, uint8 wsec);


/*--------------------------------------------------------------------
 *
 *
 *--------------------------------------------------------------------
*/
brcm_wpscli_status wpsosi_join_network_with_bssid(const char* ssid, uint8 wsec, const uint8 *bssid,
	int num_channels, uint8 *channels);


#ifdef __cplusplus
} // extern "C" {
#endif

#endif /* _OSI_WL_JOIN_H_ */
