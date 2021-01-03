/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT
 * Generated on Thu Jul  5 08:24:00 PDT 2007
 *
 * Copyright(c) 2007 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_ssn.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */
/* FILE-CSTYLED */

typedef phytbl_info_t dot11sslpnphytbl_info_t;


extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev0[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev2[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev2_shared[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev3[];
extern CONST uint32 dot11sslpnphytbl_info_sz_rev0;
extern CONST uint32 dot11sslpnphytbl_info_sz_rev2;
extern CONST uint32 dot11sslpnphytbl_info_sz_rev3;
extern CONST uint32 dot11sslpnphytbl_aci_sz;
extern CONST uint32 dot11sslpnphytbl_no_aci_sz;
extern CONST uint32 dot11sslpnphytbl_cmrxaci_sz;
extern CONST uint32 dot11sslpnphytbl_cmrxaci_rev2_sz;
extern CONST uint32 dot11sslpnphytbl_extlna_cmrxaci_sz;
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_40Mhz_sz;
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_40Mhz_sz_rev3;
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_aci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_no_aci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaci_rev2[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_extlna_cmrxaci[];
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_olympic_x7;
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_A;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_rev1_5Ghz;
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaciWOT;
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_40Mhz[];
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_40Mhz_rev3[];


typedef struct {
	uchar gm;
	uchar pga;
	uchar pad;
	uchar dac;
	uchar bb_mult;
} sslpnphy_tx_gain_tbl_entry;

extern CONST sslpnphy_tx_gain_tbl_entry dot11sslpnphy_2GHz_gaintable_rev0[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_MidBand[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_HighBand[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11sslpnphy_noPA_gaintable_rev0[];

extern CONST uint32 sslpnphy_papd_cal_ofdm_tbl[25][64];
extern uint16 sslpnphy_rev2_real_ofdm[5];
extern uint16 sslpnphy_rev4_real_ofdm[5];
extern uint16 sslpnphy_rev2_real_cck[5];
extern uint16 sslpnphy_rev4_real_cck[5];
extern uint16 sslpnphy_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_cx_ht[10];
extern uint16 sslpnphy_rev2_default[10];
extern uint16 sslpnphy_rev0_cx_cck[10];
extern uint16 sslpnphy_rev0_cx_ofdm[10];
extern uint16 sslpnphy_rev4_cx_ofdm[10];
extern uint16 sslpnphy_cx_cck[10][10];
extern uint16 sslpnphy_real_cck[10][5];
extern uint16 sslpnphy_rev4_phybw40_real_ofdm[5];
extern uint16 sslpnphy_rev4_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev2_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev4_phybw40_cx_ht[10];
extern uint16 sslpnphy_rev2_phybw40_cx_ht[10];
extern uint16 sslpnphy_rev4_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_phybw40_real_ht[5];
extern uint16 sslpnphy_cx_ofdm[2][10];
extern uint16 sslpnphy_real_ofdm[2][5];
extern uint16 sslpnphy_rev1_cx_ofdm[10];
extern uint16 sslpnphy_rev1_real_ht[5];
extern uint16 sslpnphy_rev1_real_ofdm[5];
extern uint16 sslpnphy_tdk_mdl_real_ofdm[5];
extern uint16 sslpnphy_olympic_cx_ofdm[10];

extern CONST uint32 sslpnphy_gain_idx_extlna_cmrxaci_tbl[];
extern CONST uint16 sslpnphy_gain_extlna_cmrxaci_tbl[];
extern CONST uint32 sslpnphy_gain_idx_extlna_2g_x17[];
extern CONST uint16 sslpnphy_gain_tbl_extlna_2g_x17[];
extern CONST uint16 sw_ctrl_tbl_rev0_olympic_x17_2g[];
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_A_tbl[];
extern CONST uint16 gain_tbl_rev0[];
extern CONST uint32 gain_idx_tbl_rev0[];
extern CONST uint16 sw_ctrl_tbl_rev0[];
extern CONST uint32 sslpnphy_gain_idx_extlna_5g_x17[];
extern CONST uint16 sslpnphy_gain_tbl_extlna_5g_x17[];
extern CONST uint16 sw_ctrl_tbl_rev0_olympic_x17_5g[];
extern CONST uint16 sw_ctrl_tbl_rev1_5Ghz_tbl[];


extern uint16 sslpnphy_gain_idx_extlna_cmrxaci_tbl_sz;
extern uint16 sslpnphy_gain_extlna_cmrxaci_tbl_sz;
extern uint16 sslpnphy_gain_idx_extlna_2g_x17_sz;
extern uint16 sslpnphy_gain_tbl_extlna_2g_x17_sz;
extern uint16 sw_ctrl_tbl_rev0_olympic_x17_2g_sz;
extern uint16 dot11lpphy_rx_gain_init_tbls_A_tbl_sz;
extern uint16 gain_tbl_rev0_sz;
extern uint16 gain_idx_tbl_rev0_sz;
extern uint16 sw_ctrl_tbl_rev0_sz;
extern uint16 sslpnphy_gain_idx_extlna_5g_x17_sz;
extern uint16 sslpnphy_gain_tbl_extlna_5g_x17_sz;
extern uint16 sw_ctrl_tbl_rev0_olympic_x17_5g_sz;
extern uint16 sw_ctrl_tbl_rev1_5Ghz_tbl_sz;
