/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT
 * Generated on Tue May 19 19:40:34 PDT 2009
 *
 * Copyright(c) 2007 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_lcn.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */
/* FILE-CSTYLED */

typedef phytbl_info_t dot11lcnphytbl_info_t;


extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_rev0[];
extern CONST uint32 dot11lcnphytbl_rx_gain_info_sz_rev0;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_epa;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_epa_combo;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4336sd_fcbga;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4330sd_fcbga;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4330OlympicAMG;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4330OlympicAMGepa;
extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_info_rev0[];
extern CONST uint32 dot11lcnphytbl_info_sz_rev0;

extern CONST uint32 dot11lcn_gain_tbl_4336wlbga[];
extern CONST uint32 dot11lcn_gain_tbl_4336wlbga_sz;

extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_2G_rev2[];
extern CONST uint32 dot11lcnphytbl_rx_gain_info_2G_rev2_sz;

extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_5G_rev2[];
extern CONST uint32 dot11lcnphytbl_rx_gain_info_5G_rev2_sz;

extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_extlna_2G_rev2[];

extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_extlna_5G_rev2[];

typedef struct {
	uchar gm;
	uchar pga;
	uchar pad;
	uchar dac;
	uchar bb_mult;
} lcnphy_tx_gain_tbl_entry;

extern CONST lcnphy_tx_gain_tbl_entry dot11lcnphy_2GHz_gaintable_rev0[];
extern CONST lcnphy_tx_gain_tbl_entry dot11lcnphy_2GHz_extPA_gaintable_rev0[];

extern CONST lcnphy_tx_gain_tbl_entry dot11lcnphy_5GHz_gaintable_rev0[];
