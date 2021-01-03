/*
 * LPPHY module header file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_lp.h,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */

#ifndef _wlc_phy_lp_h_
#define _wlc_phy_lp_h_

#include <typedefs.h>

struct phy_info_lpphy {
	uint8	lpphy_full_cal_channel;
	uint8	lpphy_rc_cap;

	uint8	lpphy_tr_isolation_2g;		/* TR switch isolation for 2G:     trig2  */
	uint8	lpphy_tr_isolation_5g_low;	/* TR switch isolation for 5G low: tri5gl */
	uint8	lpphy_tr_isolation_5g_mid;	/* TR switch isolation for 5G mid: tri5g  */
	uint8	lpphy_tr_isolation_5g_hi;	/* TR switch isolation for 5G hi:  tri5gh */

	uint8	lpphy_bx_arch_2g;		/* Board switch architecture for 2G: bxa2g */
	uint8	lpphy_bx_arch_5g;		/* Board switch architecture for 5G: bxa5g */

	uint8	lpphy_rx_power_offset_2g; /* Input power offset for 2G: rxpo2g */
	uint8	lpphy_rx_power_offset_5g; /* Input power offset for 5G: rxpo5g */

	uint8	lpphy_rssi_vf_2g;		/* RSSI Vmid fine for 2G: rssismf2g */
	uint8	lpphy_rssi_vf_5g;		/* RSSI Vmid fine for 5G: rssismf5g */

	uint8	lpphy_rssi_vc_2g; 		/* RSSI Vmid coarse for 2G: rssismc2g */
	uint8	lpphy_rssi_vc_5g; 		/* RSSI Vmid coarse for 5G: rssismc5g */

	uint8	lpphy_rssi_gs_2g;		/* RSSI gain select for 2G: rssisav2g */
	uint8	lpphy_rssi_gs_5g;		/* RSSI gain select for 5G: rssisav5g */

	uint16	lpphy_tssi_tx_cnt; /* Tx frames at that level for NPT calculations */
	uint16	lpphy_tssi_idx;	/* Estimated index for target power */
	uint16	lpphy_tssi_npt;	/* NPT for TSSI averaging */

	uint16	lpphy_target_tx_freq;	/* Target frequency for which LUT's were calculated */
	int8	lpphy_tx_power_idx_override; /* Forced tx power index */
	bool	lpphy_papd_slow_cal;
	/* hold the temperature of the chip at which last calibration of the phy was done. */
	int16	lpphy_last_cal_temperature;
	/* hold the temperature deviation needed for calibration */
	int8	lpphy_cal_delta_temp;
	bool	lpphy_cck_papd_disabled;	/* whether PAPD for CCK is disabled or not */
	uint32	lpphy_papd_recal_min_interval;
	uint32	lpphy_papd_recal_max_interval;
	uint32	lpphy_papd_recal_gain_delta;
	uint	lpphy_papd_last_cal;			/* time of last papd cal */
	uint16	lpphy_papd_tx_gain_at_last_cal;	/* Tx gain index at time of last papd cal */
	uint8	lpphy_papd_cal_gain_index;	/* Tx gain index used during last papd cal */
	bool	lpphy_papd_recal_enable;
	uint32	lpphy_papd_recal_counter;
	bool	lpphy_force_papd_cal;

	uint16	lpphy_papd_cal_type;

	int16	lpphy_cck_dig_filt_type;
	int16	lpphy_ofdm_dig_filt_type;
	int		lpphy_txrf_sp_9_override;

	int8	lpphy_crs_disable_by_user;
	int8	lpphy_crs_disable_by_system;

	int16	lpphy_last_idle_tssi_temperature;
	int16	lpphy_idle_tssi_update_delta_temp;

	int16	lpphy_ofdmgainidxtableoffset;  /* reference ofdm gain index table offset */
	int16	lpphy_dsssgainidxtableoffset;  /* reference dsss gain index table offset */

	uint16	lpphy_noise_samples;

	lpphy_cal_results_t	lpphy_cal_results;

	lpphy_aci_t	lpphy_aci;

	/* compensate rx gains for temp using tempsense(1) or result of rx iq cal(0) */
	int8 	lpphy_rx_gain_temp_adj_tempsense;
	int8 	lpphy_rx_gain_temp_adj_thresh[3];
	int8 	lpphy_rx_gain_temp_adj_metric;
	int8 	lpphy_rx_gain_temp_adj_tempsense_metric;

	bool	lpphy_use_cck_dig_loft_coeffs;	/* separate cal for cck digital LO coeffs */
	bool	lpphy_use_tx_pwr_ctrl_coeffs;	/* load bbmult, iqlo coeffs from txpwrctrl tbls */
};
#endif /* _wlc_phy_lp_h_ */
