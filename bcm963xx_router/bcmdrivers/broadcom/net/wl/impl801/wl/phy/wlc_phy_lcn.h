/*
 * LCNPHY module header file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_lcn.h,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */

#ifndef _wlc_phy_lcn_h_
#define _wlc_phy_lcn_h_

#include <typedefs.h>

#define LCNPHY_NOISE_DBG_DATA_LEN 20
#define LCNPHY_NOISE_DBG_HISTORY 0

#define k_noise_metric_history_size 4
#define k_noise_metric_history_mask (k_noise_metric_history_size-1)

typedef struct {
	/* state info */
	bool enable;
	bool per_cal_enable;
	bool adj_en;
	bool tainted;
	uint8 state;
	uint16 start_time;
	uint16 time;
	int8 check_cnt;
	uint8 err_cnt;
	uint8 update_noise_cnt;
	/* phy regs saved */
	int16 high_gain;
	int16 input_pwr_offset;
	uint16 nf_substract_val;
	uint16 init_noise;
#if LCNPHY_NOISE_DBG_HISTORY > 0
	/* dbg */
	int8 dbg_dump_idx;
	int8 dbg_dump_sub_idx;
	uint32 dbg_dump_cmd;
	int8 dbg_idx;
	uint16 dbg_starts;
	uint16 dbg_gain_adj_cnt;
	uint16 dbg_gain_adj_time;
	uint16 dbg_input_pwr_adj_cnt;
	uint16 dbg_input_pwr_adj_time;
	uint16 dbg_cnt;
	uint16 dbg_taint_cnt;
	uint16 dbg_watchdog_cnt;
	uint16 dbg_info[LCNPHY_NOISE_DBG_HISTORY][LCNPHY_NOISE_DBG_DATA_LEN];
#endif
} noise_t;

struct phy_info_lcnphy {
	int		lcnphy_txrf_sp_9_override;
	uint8	lcnphy_full_cal_channel;
	uint8 	lcnphy_cal_counter;
	uint16 	lcnphy_cal_temper;
	bool	lcnphy_recal;

	uint8	lcnphy_rc_cap;
	uint32  lcnphy_mcs20_po;

	uint8	lcnphy_tr_isolation_mid;	/* TR switch isolation for each sub-band */
	uint8	lcnphy_tr_isolation_low;
	uint8	lcnphy_tr_isolation_hi;

	uint8	lcnphy_bx_arch;		/* Board switch architecture */
	uint8	lcnphy_rx_power_offset;	 /* Input power offset */
	uint8	lcnphy_rssi_vf;		/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc;		/* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs;		/* RSSI gain select */
	uint8	lcnphy_tssi_val; /* tssi value */
	uint8	lcnphy_rssi_vf_lowtemp;	/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc_lowtemp; /* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs_lowtemp; /* RSSI gain select */

	uint8	lcnphy_rssi_vf_hightemp;	/* RSSI Vmid fine */
	uint8	lcnphy_rssi_vc_hightemp;	/* RSSI Vmid coarse */
	uint8	lcnphy_rssi_gs_hightemp;	/* RSSI gain select */

	int16   lcnphy_pa0b0;		/* pa0b0 */
	int16   lcnphy_pa0b1;		/* pa0b1 */
	int16   lcnphy_pa0b2;		/* pa0b2 */
	/* next 3 are for tempcompensated tx power control algo of 4313A0 */
	uint16 	lcnphy_rawtempsense;
	uint8   lcnphy_measPower;
	uint8  	lcnphy_tempsense_slope;
	uint8	lcnphy_freqoffset_corr;
	uint8	lcnphy_tempsense_option;
	uint8	lcnphy_tempcorrx;
	bool	lcnphy_iqcal_swp_dis;
	bool	lcnphy_hw_iqcal_en;
	uint    lcnphy_bandedge_corr;
	bool    lcnphy_spurmod;
	uint16	lcnphy_tssi_tx_cnt; /* Tx frames at that level for NPT calculations */
	uint16	lcnphy_tssi_idx;	/* Estimated index for target power */
	uint16	lcnphy_tssi_npt;	/* NPT for TSSI averaging */

	uint16	lcnphy_target_tx_freq;	/* Target freq for which LUT's were calculated */
	int8	lcnphy_tx_power_idx_override; /* Forced tx power index */
	uint16	lcnphy_noise_samples;

	uint32	lcnphy_papdRxGnIdx;
	uint32	lcnphy_papd_rxGnCtrl_init;

	uint32	lcnphy_gain_idx_14_lowword;
	uint32	lcnphy_gain_idx_14_hiword;
	uint32	lcnphy_gain_idx_27_lowword;
	uint32	lcnphy_gain_idx_27_hiword;
	int16	lcnphy_ofdmgainidxtableoffset;  /* reference ofdm gain index table offset */
	int16	lcnphy_dsssgainidxtableoffset;  /* reference dsss gain index table offset */
	uint32	lcnphy_tr_R_gain_val;  /* reference value of gain_val_tbl at index 64 */
	uint32	lcnphy_tr_T_gain_val;	/* reference value of gain_val_tbl at index 65 */
	int8	lcnphy_input_pwr_offset_db;
	uint16	lcnphy_Med_Low_Gain_db;
	uint16	lcnphy_Very_Low_Gain_db;
	int8	lcnphy_lastsensed_temperature;
	int8	lcnphy_pkteng_rssi_slope;
	uint8	lcnphy_saved_tx_user_target[TXP_NUM_RATES];
	uint8	lcnphy_volt_winner;
	uint8	lcnphy_volt_low;
	uint8	lcnphy_54_48_36_24mbps_backoff;
	uint8	lcnphy_11n_backoff;
	uint8	lcnphy_lowerofdm;
	uint8	lcnphy_cck;
	uint8	lcnphy_psat_2pt3_detected;
	int32	lcnphy_lowest_Re_div_Im;
	int8 	lcnphy_final_papd_cal_idx;
	uint16	lcnphy_extstxctrl4;
	uint16	lcnphy_extstxctrl0;
	uint16	lcnphy_extstxctrl1;
	bool    lcnphy_papd_4336_mode;
	int16	lcnphy_cck_dig_filt_type;
	int16	lcnphy_ofdm_dig_filt_type;
	bool	lcnphy_uses_rate_offset_table;
	lcnphy_cal_results_t lcnphy_cal_results;

	/* used for debug */
	uint8 	lcnphy_psat_pwr;
	uint8 	lcnphy_psat_indx;
	int32	lcnphy_min_phase;
	uint8	lcnphy_final_idx;
	uint8	lcnphy_start_idx;
	uint8	lcnphy_current_index;
	uint16	lcnphy_logen_buf_1;
	uint16	lcnphy_local_ovr_2;
	uint16	lcnphy_local_oval_6;
	uint16	lcnphy_local_oval_5;
	uint16	lcnphy_logen_mixer_1;

	uint8	lcnphy_aci_stat;
	uint	lcnphy_aci_start_time;
	int8	lcnphy_tx_power_offset[TXP_NUM_RATES];		/* Offset from base power */

	/* noise cal data */
	noise_t noise;

	bool    ePA;
	uint8   extpagain2g;
	uint8   extpagain5g;
	uint8	txpwrindex_nvram;
	int16	pa_gain_ovr_val_2g;
	int16	pa_gain_ovr_val_5g;
};
#endif /* _wlc_phy_lcn_h_ */
