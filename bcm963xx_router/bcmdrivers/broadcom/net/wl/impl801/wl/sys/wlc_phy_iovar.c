/*
 * PHY and RADIO specific portion of Broadcom BCM43XX 802.11abg
 * PHY iovar processing of Broadcom BCM43XX 802.11abg
 * Networking Device Driver.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_iovar.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

/*
 * This file contains high portion PHY iovar processing and table.
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <qmath.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wlc_phy_radio.h>
#include <bitfuncs.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <proto/802.11.h>
#include <hndpmu.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wl_dbg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wl_export.h>
#include <sbsprom.h>
#include <bcmwifi.h>
#include <wlc_phy_shim.h>

const bcm_iovar_t phy_iovars[] = {
	/* OLD, PHYTYPE specific iovars, to phase out, use unified ones at the end of this array */
#if defined(WLTEST)
#if NCONF
	{"nphy_5g_pwrgain", IOV_NPHY_5G_PWRGAIN,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_txpwrctrl", IOV_NPHY_TXPWRCTRL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_txpwrindex", IOV_NPHY_TXPWRINDEX,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_UINT16, 0
	},
	{"nphy_rssisel", IOV_NPHY_RSSISEL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_scraminit", IOV_NPHY_SCRAMINIT,
	(IOVF_SET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_rfseq", IOV_NPHY_RFSEQ,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_txiqlocal", IOV_NPHY_TXIQLOCAL,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_rxiqcal", IOV_NPHY_RXIQCAL,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_rssical", IOV_NPHY_RSSICAL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_dfs_lp_buffer", IOV_NPHY_DFS_LP_BUFFER,
	0, IOVT_UINT8, 0
	},
	{"nphy_gpiosel", IOV_NPHY_GPIOSEL,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"nphy_tx_tone", IOV_NPHY_TX_TONE,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT32, 0
	},
	{"nphy_test_tssi", IOV_NPHY_TEST_TSSI,
	(IOVF_SET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_test_tssi_offs", IOV_NPHY_TEST_TSSI_OFFS,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_gain_boost", IOV_NPHY_GAIN_BOOST,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_percal", IOV_NPHY_PERICAL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_forcecal", IOV_NPHY_FORCECAL,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_spuravoid", IOV_PHY_SPURAVOID,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_papdcal", IOV_NPHY_PAPDCAL,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_pacaltype", IOV_NPHY_PAPDCALTYPE,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_skippapd", IOV_NPHY_SKIPPAPD,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_pacalindex", IOV_NPHY_PAPDCALINDEX,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"nphy_aci_scan", IOV_NPHY_ACI_SCAN,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_bphy_evm", IOV_NPHY_BPHY_EVM,
	(IOVF_SET_DOWN | IOVF_SET_BAND | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_bphy_rfcs", IOV_NPHY_BPHY_RFCS,
	(IOVF_SET_DOWN | IOVF_SET_BAND | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_enrxcore", IOV_NPHY_ENABLERXCORE,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"nphy_tbldump_minidx", IOV_NPHY_TBLDUMP_MINIDX,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_tbldump_maxidx", IOV_NPHY_TBLDUMP_MAXIDX,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_phyreg_skipdump", IOV_NPHY_PHYREG_SKIPDUMP,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"nphy_phyreg_skipcount", IOV_NPHY_PHYREG_SKIPCNT,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_caltxgain", IOV_NPHY_CALTXGAIN,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_tempthresh", IOV_NPHY_TEMPTHRESH,
	(IOVF_MFG), IOVT_INT16, 0
	},
	{"nphy_tempoffset", IOV_NPHY_TEMPOFFSET,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"nphy_cal_sanity", IOV_NPHY_CAL_SANITY,
	IOVF_SET_UP, IOVT_UINT32, 0
	},
#endif /* NCONF */

#if LCNCONF
	{"lcnphy_ldovolt", IOV_LCNPHY_LDOVOLT,
	(IOVF_SET_UP), IOVT_UINT32, 0
	},
#endif

#if LPCONF
	{"lpphy_tx_tone", IOV_LPPHY_TX_TONE,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_txiqlocal", IOV_LPPHY_TXIQLOCAL,
	(IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_rxiqcal", IOV_LPPHY_RXIQCAL,
	(IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_fullcal", IOV_LPPHY_FULLCAL,
	(IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_pacal", IOV_LPPHY_PAPDCAL,
	(IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_pacaltype", IOV_LPPHY_PAPDCALTYPE,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_txpwrctrl", IOV_LPPHY_TXPWRCTRL,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"lpphy_txpwrindex", IOV_LPPHY_TXPWRINDEX,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"lpphy_crs", IOV_LPPHY_CRS,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_BOOL, 0
	},
	{"lpphy_pa_slow_cal", IOV_LPPHY_PAPD_SLOW_CAL,
	(IOVF_MFG), IOVT_BOOL, 0
	},
	{"lpphy_pa_recal_min_interval", IOV_LPPHY_PAPD_RECAL_MIN_INTERVAL,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_pa_recal_max_interval", IOV_LPPHY_PAPD_RECAL_MAX_INTERVAL,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_pa_recal_gain_delta", IOV_LPPHY_PAPD_RECAL_GAIN_DELTA,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_pa_recal_enable", IOV_LPPHY_PAPD_RECAL_ENABLE,
	(IOVF_MFG), IOVT_BOOL, 0
	},
	{"lpphy_pa_recal_counter", IOV_LPPHY_PAPD_RECAL_COUNTER,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_cck_dig_filt_type", IOV_LPPHY_CCK_DIG_FILT_TYPE,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_ofdm_dig_filt_type", IOV_LPPHY_OFDM_DIG_FILT_TYPE,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"lpphy_txrf_sp_9", IOV_LPPHY_TXRF_SP_9_OVR,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_aci_on_thresh", IOV_LPPHY_ACI_ON_THRESH,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_aci_off_thresh", IOV_LPPHY_ACI_OFF_THRESH,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_aci_on_timeout", IOV_LPPHY_ACI_ON_TIMEOUT,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_aci_off_timeout", IOV_LPPHY_ACI_OFF_TIMEOUT,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_aci_glitch_timeout", IOV_LPPHY_ACI_GLITCH_TIMEOUT,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_aci_chan_scan_cnt", IOV_LPPHY_ACI_CHAN_SCAN_CNT,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_aci_chan_scan_pwr_thresh", IOV_LPPHY_ACI_CHAN_SCAN_PWR_THRESH,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_aci_chan_scan_cnt_thresh", IOV_LPPHY_ACI_CHAN_SCAN_CNT_THRESH,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_aci_chan_scan_timeout", IOV_LPPHY_ACI_CHAN_SCAN_TIMEOUT,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"lpphy_noise_samples", IOV_LPPHY_NOISE_SAMPLES,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"lpphy_papdepstbl", IOV_LPPHY_PAPDEPSTBL,
	(IOVF_GET_UP | IOVF_MFG), IOVT_BUFFER, 0
	},
	{"lpphy_txiqcc", IOV_LPPHY_TXIQCC,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER,  2*sizeof(int32)
	},
	{"lpphy_txlocc", IOV_LPPHY_TXLOCC,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER, 6
	},
	{"lpphy_idle_tssi_update_delta_temp", IOV_LPPHY_IDLE_TSSI_UPDATE_DELTA_TEMP,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"ofdm_analog_filt_bw_override", IOV_LPPHY_OFDM_ANALOG_FILT_BW_OVERRIDE,
	(IOVF_MFG), IOVT_INT16, 0
	},
	{"cck_analog_filt_bw_override", IOV_LPPHY_CCK_ANALOG_FILT_BW_OVERRIDE,
	(IOVF_MFG), IOVT_INT16, 0
	},
	{"ofdm_rccal_override", IOV_LPPHY_OFDM_RCCAL_OVERRIDE,
	(IOVF_MFG), IOVT_INT16, 0
	},
	{"cck_rccal_override", IOV_LPPHY_CCK_RCCAL_OVERRIDE,
	(IOVF_MFG), IOVT_INT16, 0
	},
#endif /* LPCONF */
#if SSLPNCONF
	{"sslpnphy_tx_tone", IOV_SSLPNPHY_TX_TONE,
	IOVF_SET_UP, IOVT_UINT32, 0
	},
	{"sslpnphy_txpwrindex", IOV_SSLPNPHY_TXPWRINDEX,
	IOVF_SET_UP, IOVT_INT8, 0
	},
#endif /* SSLPNCONF */
#endif	

#if NCONF	    /* move some to internal ?? */
#if defined(BCMDBG)
	{"nphy_initgain", IOV_NPHY_INITGAIN,
	IOVF_SET_UP, IOVT_UINT16, 0
	},
	{"nphy_hpv1gain", IOV_NPHY_HPVGA1GAIN,
	IOVF_SET_UP, IOVT_INT8, 0
	},
	{"nphy_tx_temp_tone", IOV_NPHY_TX_TEMP_TONE,
	IOVF_SET_UP, IOVT_UINT32, 0
	},
	{"nphy_cal_reset", IOV_NPHY_CAL_RESET,
	IOVF_SET_UP, IOVT_UINT32, 0
	},
	{"nphy_est_tonepwr", IOV_NPHY_EST_TONEPWR,
	IOVF_GET_UP, IOVT_INT32, 0
	},
	{"nphy_rfseq_txgain", IOV_NPHY_RFSEQ_TXGAIN,
	IOVF_GET_UP, IOVT_INT32, 0
	},
#endif /* BCMDBG */
#endif /* NCONF */

#ifdef LPCONF
	{"lpphy_tempsense", IOV_LPPHY_TEMPSENSE,
	IOVF_GET_UP, IOVT_INT32, 0
	},
	{"lpphy_cal_delta_temp", IOV_LPPHY_CAL_DELTA_TEMP,
	0, IOVT_INT32, 0
	},
	{"lpphy_vbatsense", IOV_LPPHY_VBATSENSE,
	IOVF_GET_UP, IOVT_INT32, 0
	},
	{"lpphy_rx_gain_temp_adj_tempsense", IOV_LPPHY_RX_GAIN_TEMP_ADJ_TEMPSENSE,
	(0), IOVT_INT8, 0
	},
	{"lpphy_rx_gain_temp_adj_thresh", IOV_LPPHY_RX_GAIN_TEMP_ADJ_THRESH,
	(0), IOVT_UINT32, 0
	},
	{"lpphy_rx_gain_temp_adj_metric", IOV_LPPHY_RX_GAIN_TEMP_ADJ_METRIC,
	(0), IOVT_INT16, 0
	},
#endif /* LPCONF */

	/* ==========================================
	 * unified phy iovar, independent of PHYTYPE
	 * ==========================================
	 */
#if defined(AP) && defined(RADAR)
	{"radarargs", IOV_RADAR_ARGS,
	(0), IOVT_BUFFER, sizeof(wl_radar_args_t)
	},
	{"radarargs40", IOV_RADAR_ARGS_40MHZ,
	(0), IOVT_BUFFER, sizeof(wl_radar_args_t)
	},
	{"radarthrs", IOV_RADAR_THRS,
	(IOVF_SET_UP), IOVT_BUFFER, sizeof(wl_radar_thr_t)
	},
#endif /* AP && RADAR */
#if defined(BCMDBG) || defined(WLTEST)
	{"fast_timer", IOV_FAST_TIMER,
	(IOVF_NTRL | IOVF_MFG), IOVT_UINT32, 0
	},
	{"slow_timer", IOV_SLOW_TIMER,
	(IOVF_NTRL | IOVF_MFG), IOVT_UINT32, 0
	},
	{"glacial_timer", IOV_GLACIAL_TIMER,
	(IOVF_NTRL | IOVF_MFG), IOVT_UINT32, 0
	},
#endif /* BCMDBG || WLTEST */

#if defined(WLTEST)
	{"txinstpwr", IOV_TXINSTPWR,
	(IOVF_GET_CLK | IOVF_GET_BAND | IOVF_MFG), IOVT_BUFFER, sizeof(tx_inst_power_t)
	},
#endif

#ifdef SAMPLE_COLLECT
	{"sample_collect", IOV_PHY_SAMPLE_COLLECT,
	(IOVF_GET_DOWN | IOVF_GET_CLK), IOVT_BUFFER, WLC_SAMPLECOLLECT_MAXLEN
	},
	{"sample_data", IOV_PHY_SAMPLE_DATA,
	(IOVF_GET_DOWN | IOVF_GET_CLK), IOVT_BUFFER, WLC_SAMPLECOLLECT_MAXLEN
	},
#endif
#ifdef WLTEST
	{"fem2g", IOV_PHY_FEM2G,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_BUFFER, 0
	},
	{"fem5g", IOV_PHY_FEM5G,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_BUFFER, 0
	},
	{"maxpower", IOV_PHY_MAXP,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_BUFFER, 0
	},
#endif /* WLTEST */
#if defined(WLTEST)
	{"phymsglevel", IOV_PHYHAL_MSG,
	(0), IOVT_UINT32, 0
	},
	{"phy_watchdog", IOV_PHY_WATCHDOG,
	(IOVF_MFG), IOVT_BOOL, 0
	},
	{"phy_fixed_noise", IOV_PHY_FIXED_NOISE,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"phynoise_polling", IOV_PHYNOISE_POLL,
	(IOVF_MFG), IOVT_BOOL, 0
	},
	{"carrier_suppress", IOV_CARRIER_SUPPRESS,
	(IOVF_SET_UP | IOVF_MFG), IOVT_BOOL, 0
	},
	{"phy_cal_disable", IOV_PHY_CAL_DISABLE,
	(IOVF_MFG), IOVT_BOOL, 0
	},
#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	/* nphy_cal_delay would be a better name. To comply w/ impl5, keep this name here */
	{"enable_percal_delay", IOV_ENABLE_PERCAL_DELAY,
	0, IOVT_BOOL, 0
	},
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */	
	{"unmod_rssi", IOV_UNMOD_RSSI,
	(IOVF_MFG), IOVT_INT32, 0
	},
	{"pkteng_stats", IOV_PKTENG_STATS,
	(IOVF_GET_UP | IOVF_MFG), IOVT_BUFFER, sizeof(wl_pkteng_stats_t)
	},
	{"phytable", IOV_PHYTABLE,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER, 4*4
	},
	{"aci_exit_check_period", IOV_ACI_EXIT_CHECK_PERIOD,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{"pavars", IOV_PAVARS,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_BUFFER, WL_PHY_PAVARS_LEN * sizeof(uint16)
	},
	{"povars", IOV_POVARS,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_BUFFER, sizeof(wl_po_t)
	},
	{"phy_forcecal", IOV_PHY_FORCECAL,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_txpwrctrl", IOV_PHY_TXPWRCTRL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_txpwrindex", IOV_PHY_TXPWRINDEX,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER, 0
	},
	{"phy_idletssi", IOV_PHY_IDLETSSI,
	(IOVF_GET_UP | IOVF_MFG), IOVT_UINT32, 0
	},
	{"phy_txiqcc", IOV_PHY_TXIQCC,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER,  2*sizeof(int32)
	},
	{"phy_txlocc", IOV_PHY_TXLOCC,
	(IOVF_GET_UP | IOVF_SET_UP | IOVF_MFG), IOVT_BUFFER, 6
	},
	{"phy_txrx_chain", IOV_PHY_TXRX_CHAIN,
	(0), IOVT_INT8, 0
	},
	{"phy_bphy_evm", IOV_PHY_BPHY_EVM,
	(IOVF_SET_DOWN | IOVF_SET_BAND | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_bphy_rfcs", IOV_PHY_BPHY_RFCS,
	(IOVF_SET_DOWN | IOVF_SET_BAND | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_enrxcore", IOV_PHY_ENABLERXCORE,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_est_tonepwr", IOV_PHY_EST_TONEPWR,
	IOVF_GET_UP, IOVT_INT32, 0
	},
	{"phy_gpiosel", IOV_PHY_GPIOSEL,
	(IOVF_MFG), IOVT_UINT16, 0
	},
	{"phy_5g_pwrgain", IOV_PHY_5G_PWRGAIN,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_rfseq", IOV_PHY_RFSEQ,
	(IOVF_SET_UP | IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_scraminit", IOV_PHY_SCRAMINIT,
	(IOVF_SET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"phy_tempoffset", IOV_PHY_TEMPOFFSET,
	(IOVF_MFG), IOVT_INT8, 0
	},
	{"phy_tempthresh", IOV_PHY_TEMPTHRESH,
	(IOVF_MFG), IOVT_INT16, 0
	},
	{"phy_test_tssi", IOV_PHY_TEST_TSSI,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"phy_test_tssi_offs", IOV_PHY_TEST_TSSI_OFFS,
	(IOVF_SET_UP | IOVF_GET_UP | IOVF_MFG), IOVT_INT8, 0
	},
	{"phy_tx_tone", IOV_PHY_TX_TONE,
	(IOVF_SET_DOWN | IOVF_MFG), IOVT_UINT32, 0
	},
	{"phycal_tempdelta", IOV_PHYCAL_TEMPDELTA,
	(IOVF_MFG), IOVT_UINT8, 0
	},
	{"phy_activecal", IOV_PHY_ACTIVECAL,
	IOVF_GET_UP, IOVT_BOOL, 0
	},
#endif 
#ifdef PHYMON
	{"phycal_state", IOV_PHYCAL_STATE,
	IOVF_GET_UP, IOVT_UINT32, 0,
	},
#endif /* PHYMON */
#if defined(WLTEST) || defined(AP)
	{"phy_percal", IOV_PHY_PERICAL,
	(IOVF_MFG), IOVT_UINT8, 0
	},
#endif 

	{"phy_rxiqest", IOV_PHY_RXIQ_EST,
	IOVF_SET_UP, IOVT_UINT32, IOVT_UINT32
	},
	{"num_stream", IOV_NUM_STREAM,
	(0), IOVT_INT32, 0
	},
	{"band_range", IOV_BAND_RANGE,
	0, IOVT_INT8, 0
	},
	{"min_txpower", IOV_MIN_TXPOWER,
	0, IOVT_UINT32, 0
	},
	{"phy_tempsense", IOV_PHY_TEMPSENSE,
	IOVF_GET_UP, IOVT_INT16, 0
	},

	/* terminating element, only add new before this */
	{NULL, 0, 0, 0, 0 }
};

LOCATOR_EXTERN int
wlc_phy_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	int err = 0;

	/* BMAC_NOTE: low level cleanup */
	if ((err = wlc_iovar_check(wlc->pub, vi, a, alen, IOV_ISSET(actionid))) != 0)
		return err;

	return wlc_bmac_phy_iovar_dispatch(wlc->hw, actionid, vi->type, p, plen, a, alen, vsize);
}

/* PHY is no longer a wlc.c module. It's slave to wlc_bmac.c
 * still use module_register to move iovar processing to HIGH driver to save dongle space 
 */
int
BCMATTACHFN(wlc_phy_iovar_attach)(void *pub)
{
	/* register module */
	if (wlc_module_register((wlc_pub_t*)pub, phy_iovars, "phy", ((wlc_pub_t*)pub)->wlc,
		wlc_phy_doiovar, NULL, NULL)) {
		WL_ERROR(("%s: wlc_phy_iovar_attach failed!\n", __FUNCTION__));
		return -1;
	}
	return 0;
}

void
BCMATTACHFN(wlc_phy_iovar_detach)(void *pub)
{
	wlc_module_unregister(pub, "phy", ((wlc_pub_t*)pub)->wlc);
}
