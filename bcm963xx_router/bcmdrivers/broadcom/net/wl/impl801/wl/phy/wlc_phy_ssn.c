/*
 * PHY and RADIO specific portion of Broadcom BCM43XX 802.11abgn
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
 * $Id: wlc_phy_ssn.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
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
#include <bcmsrom_fmt.h>
#include <sbsprom.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>

#include <wlc_phyreg_ssn.h>
#include <wlc_phytbl_ssn.h>
#include <wlc_phy_ssn.h>

/* %%%%%% SSLPNPHY macros/structure */
#define PLL_2063_LOW_END_VCO 	3000
#define PLL_2063_LOW_END_KVCO 	27
#define PLL_2063_HIGH_END_VCO	4200
#define PLL_2063_HIGH_END_KVCO	68
#define PLL_2063_LOOP_BW			300
#define PLL_2063_LOOP_BW_ePA			500
#define PLL_2063_D30				3000
#define PLL_2063_D30_ePA			1500
#define PLL_2063_CAL_REF_TO		8
#define PLL_2063_MHZ				1000000
#define PLL_2063_OPEN_LOOP_DELAY	5

#define SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT + 8)
#define SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(SSLPNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT + 8)
#define SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define wlc_sslpnphy_enable_tx_gain_override(pi) \
	wlc_sslpnphy_set_tx_gain_override(pi, TRUE)
#define wlc_sslpnphy_disable_tx_gain_override(pi) \
	wlc_sslpnphy_set_tx_gain_override(pi, FALSE)

#define wlc_sslpnphy_set_start_tx_pwr_idx(pi, idx) \
	mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlCmd, \
		SSLPNPHY_TxPwrCtrlCmd_pwrIndex_init_MASK, \
		(uint16)(idx) << SSLPNPHY_TxPwrCtrlCmd_pwrIndex_init_SHIFT)

#define wlc_sslpnphy_set_tx_pwr_npt(pi, npt) \
	mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlNnum, \
		SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK, \
		(uint16)(npt) << SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_sslpnphy_get_tx_pwr_ctrl(pi) \
	(read_phy_reg((pi), SSLPNPHY_TxPwrCtrlCmd) & \
			(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK | \
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK))

#define wlc_sslpnphy_get_tx_pwr_npt(pi) \
	((read_phy_reg(pi, SSLPNPHY_TxPwrCtrlNnum) & \
		SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK) >> \
		SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_sslpnphy_get_current_tx_pwr_idx(pi) \
	((read_phy_reg(pi, SSLPNPHY_TxPwrCtrlStatus) & \
		SSLPNPHY_TxPwrCtrlStatus_baseIndex_MASK) >> \
		SSLPNPHY_TxPwrCtrlStatus_baseIndex_SHIFT)

#define wlc_sslpnphy_get_target_tx_pwr(pi) \
	((read_phy_reg(pi, SSLPNPHY_TxPwrCtrlTargetPwr) & \
		SSLPNPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK) >> \
		SSLPNPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_sslpnphy_set_target_tx_pwr(pi, target) \
	mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlTargetPwr, \
		SSLPNPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK, \
		(uint16)(target) << SSLPNPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

/* Turn off all the crs signals to the MAC */
#define wlc_sslpnphy_iqcal_active(pi)	\
	(read_phy_reg((pi), SSLPNPHY_iqloCalCmd) & \
	(SSLPNPHY_iqloCalCmd_iqloCalCmd_MASK | SSLPNPHY_iqloCalCmd_iqloCalDFTCmd_MASK))

#define wlc_sslpnphy_tssi_enabled(pi) \
	(SSLPNPHY_TX_PWR_CTRL_OFF != wlc_sslpnphy_get_tx_pwr_ctrl((pi)))

#define SWCTRL_BT_TX		0x18
#define SWCTRL_OVR_DISABLE	0x40

#define	AFE_CLK_INIT_MODE_TXRX2X	1
#define	AFE_CLK_INIT_MODE_PAPD		0

#define SSLPNPHY_TBL_ID_IQLOCAL			0x00
/* #define SSLPNPHY_TBL_ID_TXPWRCTL 		0x07 move to wlc_phy_int.h */
#define SSLPNPHY_TBL_ID_GAIN_IDX		0x0d
#define SSLPNPHY_TBL_ID_GAINVALTBL_IDX		0x11
#define SSLPNPHY_TBL_ID_GAIN_TBL		0x12
#define SSLPNPHY_TBL_ID_SW_CTRL			0x0f
#define SSLPNPHY_TBL_ID_SPUR			0x14
#define SSLPNPHY_TBL_ID_SAMPLEPLAY		0x15
#define SSLPNPHY_TBL_ID_SAMPLEPLAY1		0x16
#define SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL	0x18

#define SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET 	64
#define SSLPNPHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET		320
#define SSLPNPHY_TX_PWR_CTRL_LO_OFFSET		448
#define SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET		576

#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G	100
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G	70
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G_PAPD	100
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G_PAPD	70

#define SSLPNPHY_TX_PWR_CTRL_START_NPT			1
#define SSLPNPHY_TX_PWR_CTRL_MAX_NPT			7

#define SSLPNPHY_NUM_DIG_FILT_COEFFS 9

#define SSLPNPHY_NOISE_SAMPLES_DEFAULT 5000

#define SSLPNPHY_ACI_DETECT_START      1
#define SSLPNPHY_ACI_DETECT_PROGRESS   2
#define SSLPNPHY_ACI_DETECT_STOP       3

#define SSLPNPHY_ACI_CRSHIFRMLO_TRSH 100
#define SSLPNPHY_ACI_GLITCH_TRSH 2000
#define	SSLPNPHY_ACI_TMOUT 250		/* Time for CRS HI and FRM LO (in micro seconds) */
#define SSLPNPHY_ACI_DETECT_TIMEOUT  2	/* in  seconds */
#define SSLPNPHY_ACI_START_DELAY 0

#define wlc_sslpnphy_tx_gain_override_enabled(pi) \
	(0 != (read_phy_reg((pi), SSLPNPHY_AfeCtrlOvr) & SSLPNPHY_AfeCtrlOvr_dacattctrl_ovr_MASK))

#define wlc_sslpnphy_total_tx_frames(pi) \
	wlapi_bmac_read_shm((pi)->sh->physhim, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txallfrm))

void wlc_sslpnphy_periodic_cal(phy_info_t *pi);

typedef struct {
	uint16 gm_gain;
	uint16 pga_gain;
	uint16 pad_gain;
	uint16 dac_gain;
} sslpnphy_txgains_t;

typedef struct {
	sslpnphy_txgains_t gains;
	bool useindex;
	uint8 index;
} sslpnphy_txcalgains_t;

typedef struct {
	uint8 chan;
	int16 a;
	int16 b;
} sslpnphy_rx_iqcomp_t;

typedef struct {
	uint32 iq_prod;
	uint32 i_pwr;
	uint32 q_pwr;
} sslpnphy_iq_est_t;

typedef struct {
	uint16 ptcentreTs20;
	uint16 ptcentreFactor;
} sslpnphy_sfo_cfg_t;

typedef enum {
	SSLPNPHY_CAL_FULL,
	SSLPNPHY_CAL_RECAL,
	SSLPNPHY_CAL_CURRECAL,
	SSLPNPHY_CAL_DIGCAL,
	SSLPNPHY_CAL_GCTRL
} sslpnphy_cal_mode_t;

typedef enum {
	SSLPNPHY_PAPD_CAL_CW,
	SSLPNPHY_PAPD_CAL_OFDM
} sslpnphy_papd_cal_type_t;

/* SSLPNPHY IQCAL parameters for various Tx gain settings */
/* table format: */
/*	target, gm, pga, pad, ncorr for each of 5 cal types */
typedef uint16 iqcal_gain_params_sslpnphy[9];

static const iqcal_gain_params_sslpnphy tbl_iqcal_gainparams_sslpnphy_2G[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

static const iqcal_gain_params_sslpnphy tbl_iqcal_gainparams_sslpnphy_5G[] = {
	{0x7ef, 7, 0xe, 0xe, 0, 0, 0, 0, 0},
	};

static const iqcal_gain_params_sslpnphy *tbl_iqcal_gainparams_sslpnphy[2] = {
	tbl_iqcal_gainparams_sslpnphy_2G,
	tbl_iqcal_gainparams_sslpnphy_5G
	};

static const uint16 iqcal_gainparams_numgains_sslpnphy[2] = {
	sizeof(tbl_iqcal_gainparams_sslpnphy_2G) / sizeof(*tbl_iqcal_gainparams_sslpnphy_2G),
	sizeof(tbl_iqcal_gainparams_sslpnphy_5G) / sizeof(*tbl_iqcal_gainparams_sslpnphy_5G),
	};

/* LO Comp Gain ladder. Format: {m genv} */
static CONST
uint16 sslpnphy_iqcal_loft_gainladder[]  = {
	((2 << 8) | 0),
	((3 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((16 << 8) | 1),
	((16 << 8) | 2),
	((16 << 8) | 3),
	((16 << 8) | 4),
	((16 << 8) | 5),
	((16 << 8) | 6),
	((16 << 8) | 7),
	((23 << 8) | 7),
	((32 << 8) | 7),
	((45 << 8) | 7),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

/* Image Rejection Gain ladder. Format: {m genv} */
static CONST
uint16 sslpnphy_iqcal_ir_gainladder[] = {
	((1 << 8) | 0),
	((2 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((23 << 8) | 0),
	((32 << 8) | 0),
	((45 << 8) | 0),
	((64 << 8) | 0),
	((64 << 8) | 1),
	((64 << 8) | 2),
	((64 << 8) | 3),
	((64 << 8) | 4),
	((64 << 8) | 5),
	((64 << 8) | 6),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

/* Autogenerated by 2063_chantbl_tcl2c.tcl */
static const
sslpnphy_rx_iqcomp_t sslpnphy_rx_iqcomp_table_rev0[] = {
	{ 1, 0, 0 },
	{ 2, 0, 0 },
	{ 3, 0, 0 },
	{ 4, 0, 0 },
	{ 5, 0, 0 },
	{ 6, 0, 0 },
	{ 7, 0, 0 },
	{ 8, 0, 0 },
	{ 9, 0, 0 },
	{ 10, 0, 0 },
	{ 11, 0, 0 },
	{ 12, 0, 0 },
	{ 13, 0, 0 },
	{ 14, 0, 0 },
	{ 34, 0, 0 },
	{ 38, 0, 0 },
	{ 42, 0, 0 },
	{ 46, 0, 0 },
	{ 36, 0, 0 },
	{ 40, 0, 0 },
	{ 44, 0, 0 },
	{ 48, 0, 0 },
	{ 52, 0, 0 },
	{ 56, 0, 0 },
	{ 60, 0, 0 },
	{ 64, 0, 0 },
	{ 100, 0, 0 },
	{ 104, 0, 0 },
	{ 108, 0, 0 },
	{ 112, 0, 0 },
	{ 116, 0, 0 },
	{ 120, 0, 0 },
	{ 124, 0, 0 },
	{ 128, 0, 0 },
	{ 132, 0, 0 },
	{ 136, 0, 0 },
	{ 140, 0, 0 },
	{ 149, 0, 0 },
	{ 153, 0, 0 },
	{ 157, 0, 0 },
	{ 161, 0, 0 },
	{ 165, 0, 0 },
	{ 184, 0, 0 },
	{ 188, 0, 0 },
	{ 192, 0, 0 },
	{ 196, 0, 0 },
	{ 200, 0, 0 },
	{ 204, 0, 0 },
	{ 208, 0, 0 },
	{ 212, 0, 0 },
	{ 216, 0, 0 },
	};

uint32 sslpnphy_gaincode_table[] = {
	0x100800,
	0x100050,
	0x100150,
	0x100250,
	0x100950,
	0x100255,
	0x100955,
	0x100a55,
	0x110a55,
	0x1009f5,
	0x10095f,
	0x100a5f,
	0x110a5f,
	0x10305,
	0x10405,
	0x10b05,
	0x1305,
	0x35a,
	0xa5a,
	0xb5a,
	0x125a,
	0x135a,
	0x10b5f,
	0x135f,
	0x1135f,
	0x1145f,
	0x1155f,
	0x33af,
	0x132ff,
	0x232ff,
	0x215ff,
	0x216ff,
	0x235ff,
	0x236ff,
	0x255ff,
	0x256ff,
	0x2d5ff
};

uint8 sslpnphy_gain_table[] = {
	-14,
	-11,
	-8,
	-6,
	-2,
	0,
	4,
	6,
	9,
	12,
	16,
	18,
	21,
	25,
	27,
	31,
	34,
	37,
	39,
	43,
	45,
	49,
	52,
	55,
	58,
	60,
	63,
	65,
	68,
	71,
	74,
	77,
	80,
	83,
	86,
	89,
	92
};

int8 sslpnphy_gain_index_offset_for_rssi[] = {
	7,	/* 0 */
	7,	/* 1 */
	7,	/* 2 */
	7,	/* 3 */
	7,	/* 4 */
	7,	/* 5 */
	7,	/* 6 */
	8,	/* 7 */
	7,	/* 8 */
	7,	/* 9 */
	6,	/* 10 */
	7,	/* 11 */
	7,	/* 12 */
	4,	/* 13 */
	4,	/* 14 */
	4,	/* 15 */
	4,	/* 16 */
	4,	/* 17 */
	4,	/* 18 */
	4,	/* 19 */
	4,	/* 20 */
	3,	/* 21 */
	3,	/* 22 */
	3,	/* 23 */
	3,	/* 24 */
	3,	/* 25 */
	3,	/* 26 */
	4,	/* 27 */
	2,	/* 28 */
	2,	/* 29 */
	2,	/* 30 */
	2,	/* 31 */
	2,	/* 32 */
	2,	/* 33 */
	-1,	/* 34 */
	-2,	/* 35 */
	-2,	/* 36 */
	-2	/* 37 */
};

static int8 sslpnphy_gain_index_offset_for_pkt_rssi[] = {
	8,	/* 0 */
	8,	/* 1 */
	8,	/* 2 */
	8,	/* 3 */
	8,	/* 4 */
	8,	/* 5 */
	8,	/* 6 */
	9,	/* 7 */
	10,	/* 8 */
	8,	/* 9 */
	8,	/* 10 */
	7,	/* 11 */
	7,	/* 12 */
	1,	/* 13 */
	2,	/* 14 */
	2,	/* 15 */
	2,	/* 16 */
	2,	/* 17 */
	2,	/* 18 */
	2,	/* 19 */
	2,	/* 20 */
	2,	/* 21 */
	2,	/* 22 */
	2,	/* 23 */
	2,	/* 24 */
	2,	/* 25 */
	2,	/* 26 */
	2,	/* 27 */
	2,	/* 28 */
	2,	/* 29 */
	2,	/* 30 */
	2,	/* 31 */
	1,	/* 32 */
	1,	/* 33 */
	0,	/* 34 */
	0,	/* 35 */
	0,	/* 36 */
	0	/* 37 */
};

extern CONST uint8 spur_tbl_rev0[];
extern CONST uint8 spur_tbl_rev2[];
/* FIX ME */
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev2_shared[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev3[];
extern CONST uint32 dot11sslpnphytbl_info_sz_rev3;
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_40Mhz_sz_rev3;
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_40Mhz_rev3[];
extern uint16 sslpnphy_cx_cck[10][10];
extern uint16 sslpnphy_rev0_cx_cck[10];
extern uint16 sslpnphy_rev0_cx_ofdm[10];
extern uint16 sslpnphy_rev4_phybw40_real_ofdm[5];
extern uint16 sslpnphy_rev2_real_ofdm[5];
extern uint16 sslpnphy_rev4_real_ofdm[5];
extern uint16 sslpnphy_rev4_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev2_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev4_cx_ofdm[10];
extern uint16 sslpnphy_rev4_phybw40_cx_ht[10];
extern uint16 sslpnphy_real_cck[10][5];
extern uint16 sslpnphy_rev2_phybw40_cx_ht[10];
extern uint16 sslpnphy_rev2_cx_ht[10];
extern uint16 sslpnphy_rev4_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_default[10];

/* channel info type for 2063 radio used in sslpnphy */
typedef struct _chan_info_2063_sslpnphy {
	uint   chan;            /* channel number */
	uint   freq;            /* in Mhz */
	uint8 RF_logen_vcobuf_1;
	uint8 RF_logen_mixer_2;
	uint8 RF_logen_buf_2;
	uint8 RF_logen_rccr_1;
	uint8 RF_grx_1st_3;
	uint8 RF_grx_2nd_2;
	uint8 RF_arx_1st_3;
	uint8 RF_arx_2nd_1;
	uint8 RF_arx_2nd_4;
	uint8 RF_arx_2nd_7;
	uint8 RF_arx_ps_6;
	uint8 dummy2;
	uint8 RF_txrf_ctrl_2;
	uint8 RF_txrf_ctrl_5;
	uint8 RF_pa_ctrl_11;
	uint8 RF_arx_mix_4;
	uint8 dummy4;
} chan_info_2063_sslpnphy_t;

#ifdef BAND5G
typedef struct _chan_info_2063_sslpnphy_aband_tweaks {
	uint freq;
	uint8 RF_PA_CTRL_11;
	uint8 RF_TXRF_CTRL_8;
	uint8 RF_TXRF_CTRL_5;
	uint8 RF_TXRF_CTRL_2;
	uint8 RF_TXRF_CTRL_4;
	uint8 RF_TXRF_CTRL_7;
	uint8 RF_TXRF_CTRL_6;
	uint8 RF_PA_CTRL_2;
	uint8 RF_PA_CTRL_5;
	uint8 RF_TXRF_CTRL_15;
	uint8 RF_TXRF_CTRL_14;
} chan_info_2063_sslpnphy_aband_tweaks_t;

static chan_info_2063_sslpnphy_aband_tweaks_t chan_info_2063_sslpnphy_aband_tweaks[] = {
	{4920, 0x70, 0xf5, 0xff, 0xff, 0xb8, 0x79, 0x7f, 0xf, 0x40, 0x80, 0x80},
	{5100, 0x60, 0xf6, 0xff, 0xff, 0xb8, 0x79, 0x77, 0xf, 0x40, 0x80, 0x80},
	{5320, 0x40, 0x68, 0xd6, 0xf8, 0xb8, 0x79, 0x77, 0xf, 0x40, 0x80, 0x80},
	{5640, 0x10, 0x68, 0x86, 0xf8, 0xb8, 0x79, 0x77, 0xf, 0x30, 0x80, 0x80},
	{5650, 0x10, 0x68, 0x86, 0xf8, 0xb8, 0x79, 0x77, 0xf, 0x30, 0x80, 0x80},
	{5825, 0x00, 0x68, 0x36, 0x98, 0xb8, 0x79, 0x77, 0xf, 0x40, 0x80, 0x80}
};

static chan_info_2063_sslpnphy_t chan_info_2063_sslpnphy_aband[] = {
	{  34, 5170, 0x6A, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x08,
	0x0F, 0x08, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0xdd },
	{  38, 5190, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0xdd },
	{  42, 5210, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x07,
	0x0E, 0x07, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  46, 5230, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0D, 0x06,
	0x0D, 0x06, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  36, 5180, 0x69, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0xdd },
	{  40, 5200, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  44, 5220, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  48, 5240, 0x69, 0x04, 0x0C, 0x05, 0x05, 0x55, 0x0D, 0x05,
	0x0D, 0x05, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  52, 5260, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0C, 0x04,
	0x0C, 0x04, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0xdd },
	{  56, 5280, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0B, 0x03,
	0x0B, 0x03, 0x77, 0xdd, 0xAF, 0x70, 0x10, 0xF3, 0xdd },
	{  60, 5300, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0A, 0x02,
	0x0A, 0x02, 0x77, 0xdd, 0xAF, 0x60, 0x10, 0xF3, 0xdd },
	{  64, 5320, 0x67, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x09, 0x01,
	0x09, 0x01, 0x77, 0xdd, 0xAF, 0x60, 0x10, 0xF3, 0xdd },
	{ 100, 5500, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x8F, 0x50, 0x00, 0x03, 0xdd },
	{ 104, 5520, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0xdd },
	{ 108, 5540, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0xdd },
	{ 112, 5560, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0xdd },
	{ 116, 5580, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0xdd },
	{ 120, 5600, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0xdd },
	{ 124, 5620, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0xdd },
	{ 128, 5640, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0xdd },
	{ 132, 5660, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0xdd },
	{ 136, 5680, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0xdd },
	{ 140, 5700, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x5F, 0x40, 0x00, 0x03, 0xdd },
	{ 149, 5745, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x5F, 0x40, 0x00, 0x03, 0xdd },
	{ 153, 5765, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0xdd },
	{ 157, 5785, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0xdd },
	{ 161, 5805, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0xdd },
	{ 165, 5825, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x3F, 0x30, 0x00, 0x03, 0xdd },
	{ 184, 4920, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xdd, 0xFF, 0x70, 0x30, 0xF3, 0xdd },
	{ 188, 4940, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xdd, 0xFF, 0x70, 0x30, 0xF3, 0xdd },
	{ 192, 4960, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xdd, 0xEF, 0x70, 0x30, 0xF3, 0xdd },
	{ 196, 4980, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xdd, 0xEF, 0x70, 0x20, 0xF3, 0xdd },
	{ 200, 5000, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xdd, 0xEF, 0x70, 0x20, 0xF3, 0xdd },
	{ 204, 5020, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0E,
	0x0F, 0x0E, 0x77, 0xdd, 0xEF, 0x70, 0x20, 0xF3, 0xdd },
	{ 208, 5040, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0D,
	0x0F, 0x0D, 0x77, 0xdd, 0xDF, 0x70, 0x20, 0xF3, 0xdd },
	{ 212, 5060, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0C,
	0x0F, 0x0C, 0x77, 0xdd, 0xDF, 0x70, 0x20, 0xF3, 0xdd },
	{ 216, 5080, 0x6B, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0B,
	0x0F, 0x0B, 0x77, 0xdd, 0xDF, 0x70, 0x20, 0xF3, 0xdd }
};
#endif /* BAND5G */


/* Autogenerated by 2063_chantbl_tcl2c.tcl */
static chan_info_2063_sslpnphy_t chan_info_2063_sslpnphy[] = {
	{1, 2412, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x0c },
	{2, 2417, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x0b },
	{3, 2422, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x09 },
	{4, 2427, 0xff, 0x2c, 0x2c, 0x04, 0x0d, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x08 },
	{5, 2432, 0xff, 0x2c, 0x2c, 0x04, 0x0d, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x07 },
	{6, 2437, 0xff, 0x2c, 0x2c, 0x04, 0x0c, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x06 },
	{7, 2442, 0xff, 0x2c, 0x2c, 0x04, 0x0b, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x05 },
	{8, 2447, 0xff, 0x2c, 0x2c, 0x04, 0x0b, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x04 },
	{9, 2452, 0xff, 0x1c, 0x1c, 0x04, 0x0a, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x04 },
	{10, 2457, 0xff, 0x1c, 0x1c, 0x04, 0x09, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x03 },
	{11, 2462, 0xfe, 0x1c, 0x1c, 0x04, 0x08, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x03 },
	{12, 2467, 0xfe, 0x1c, 0x1c, 0x04, 0x07, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x02 },
	{13, 2472, 0xfe, 0x1c, 0x1c, 0x04, 0x06, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x02 },
	{14, 2484, 0xfe, 0x0c, 0x0c, 0x04, 0x02, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x01 }
};


sslpnphy_radio_regs_t WLBANDINITDATA(sslpnphy_radio_regs_2063)[] = {
	{ 0x4000,		0,		0 },
	{ 0x800A,		0x1,		0 },
	{ 0x4010,		0,		0 },
	{ 0x4011,		0,		0 },
	{ 0x4012,		0,		0 },
	{ 0x4013,		0,		0 },
	{ 0x4014,		0,		0 },
	{ 0x4015,		0,		0 },
	{ 0x4016,		0,		0 },
	{ 0x4017,		0,		0 },
	{ 0x4018,		0,		0 },
	{ 0xC01C,		0xe8,		0xd4 },
	{ 0xC01D,		0xa7,		0x53 },
	{ 0xC01F,		0xf0,		0xf },
	{ 0xC021,		0x5e,		0x5e },
	{ 0xC022,		0x7e,		0x7e },
	{ 0xC023,		0xf0,		0xf0 },
	{ 0xC026,		0x2,		0x2 },
	{ 0xC027,		0x7f,		0x7f },
	{ 0xC02A,		0xc,		0xc },
	{ 0x802C,		0x3c,		0x3f },
	{ 0x802D,		0xfc,		0xfe },
	{ 0xC032,		0x8,		0x8 },
	{ 0xC036,		0x60,		0x60 },
	{ 0xC03A,		0x30,		0x30 },
	{ 0xC03D,		0xc,		0xb },
	{ 0xC03E,		0x10,		0xf },
	{ 0xC04C,		0x3d,		0xfd },
	{ 0xC053,		0x2,		0x2 },
	{ 0xC057,		0x56,		0x56 },
	{ 0xC076,		0xf7,		0xf7 },
	{ 0xC0B2,		0xf0,		0xf0 },
	{ 0xC0C4,		0x71,		0x71 },
	{ 0xC0C5,		0x71,		0x71 },
	{ 0x80CF,		0xf0,		0x30 },
	{ 0xC0DF,		0x77,		0x77 },
	{ 0xC0E3,		0x3,		0x3 },
	{ 0xC0E4,		0xf,		0xf },
	{ 0xC0E5,		0xf,		0xf },
	{ 0xC0EC,		0x77,		0x77 },
	{ 0xC0EE,		0x77,		0x77 },
	{ 0xC0F3,		0x4,		0x4 },
	{ 0xC0F7,		0x9,		0x9 },
	{ 0x810B,		0,		0x4 },
	{ 0xC11D,		0x3,		0x3 },
	{ 0xFFFF,		0,		0}
};

static void wlc_radio_2063_init_sslpnphy(phy_info_t *pi);

#define wlc_radio_2063_rc_cal_done(pi) (0 != (read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_6) & 0x02))

/* %%%%%% SSLPNPHY specific function declaration */
static void wlc_sslpnphy_papd_cal_txpwr(phy_info_t *pi,	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl, bool frcTxGnCtrl, uint16 frcTxidx);
static uint wlc_sslpnphy_init_radio_regs(phy_info_t *pi, sslpnphy_radio_regs_t *radioregs,
	uint16 core_offset);

static void wlc_sslpnphy_save_papd_calibration_results(phy_info_t *pi);
static void wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi);
static void wlc_sslpnphy_restore_calibration_results(phy_info_t *pi);
static void wlc_sslpnphy_set_rx_gain_by_distribution(phy_info_t *pi,
	uint16 pga, uint16 biq2, uint16 pole1, uint16 biq1, uint16 tia, uint16 lna2,
	uint16 lna1);
static void wlc_sslpnphy_set_swctrl_override(phy_info_t *pi, uint8 index);
static void wlc_sslpnphy_clear_tx_power_offsets(phy_info_t *pi);
static uint32 wlc_sslpnphy_qdiv_roundup(uint32 divident, uint32 divisor, uint8 precision);
static void wlc_sslpnphy_set_pa_gain(phy_info_t *pi, uint16 gain);
static void wlc_sslpnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx);
static void wlc_sslpnphy_stop_ddfs(phy_info_t *pi);
static void wlc_sslpnphy_set_bbmult(phy_info_t *pi, uint8 m0);
static uint8 wlc_sslpnphy_get_bbmult(phy_info_t *pi);
static void wlc_sslpnphy_set_tx_locc(phy_info_t *pi, uint16 didq);
static void wlc_sslpnphy_get_tx_gain(phy_info_t *pi,  sslpnphy_txgains_t *gains);
static void wlc_sslpnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable);
static void wlc_sslpnphy_toggle_afe_pwdn(phy_info_t *pi);
static void wlc_sslpnphy_rx_gain_override_enable(phy_info_t *pi, bool enable);
static void wlc_sslpnphy_set_tx_gain(phy_info_t *pi,  sslpnphy_txgains_t *target_gains);
static void wlc_sslpnphy_set_rx_gain(phy_info_t *pi, uint32 gain);
static void wlc_sslpnphy_saveIntpapdlut(phy_info_t *pi, int8 Max, int8 Min,
	uint32 *papdIntlut, uint8 *papdIntlutVld);
static void wlc_sslpnphy_GetpapdMaxMinIdxupdt(phy_info_t *pi,
	int8 *maxUpdtIdx, int8 *minUpdtIdx);
static bool wlc_sslpnphy_rx_iq_est(phy_info_t *pi, uint16 num_samps,
	uint8 wait_time, sslpnphy_iq_est_t *iq_est);
static int wlc_sslpnphy_aux_adc_accum(phy_info_t *pi, uint32 numberOfSamples,
	uint32 waitTime, int32 *sum, int32 *prod);
static void wlc_sslpnphy_rx_pu(phy_info_t *pi, bool bEnable);
static bool wlc_sslpnphy_calc_rx_iq_comp(phy_info_t *pi,  uint16 num_samps);
static uint16 wlc_sslpnphy_get_pa_gain(phy_info_t *pi);

static int wlc_sslpnphy_tempsense(phy_info_t *pi);
static void wlc_sslpnphy_afe_clk_init(phy_info_t *pi, uint8 mode);

extern void wlc_sslpnphy_pktengtx(wlc_phy_t *ppi, wl_pkteng_t *pkteng, uint8 rate,
	struct ether_addr *sa, uint32 wait_delay);

static void wlc_sslpnphy_radio_2063_channel_tune(phy_info_t *pi, uint8 channel);
static void wlc_sslpnphy_CmRxAciGainTbl_Tweaks(void *arg);
static void wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi);

static void wlc_sslpnphy_load_tx_gain_table(phy_info_t *pi, const sslpnphy_tx_gain_tbl_entry *g);

static void wlc_sslpnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec);
static void wlc_sslpnphy_agc_temp_init(phy_info_t *pi);
#if defined(BCMDBG) || defined(WLTEST)
static void wlc_sslpnphy_full_cal(phy_info_t *pi);
#endif
static void wlc_sslpnphy_temp_adj(phy_info_t *pi);
static void wlc_sslpnphy_clear_papd_comptable(phy_info_t *pi);
static void wlc_sslpnphy_baseband_init(phy_info_t *pi);
static void wlc_sslpnphy_radio_init(phy_info_t *pi);
static void wlc_sslpnphy_rc_cal(phy_info_t *pi);

#if defined(WLTEST)
static void wlc_sslpnphy_reset_radio_loft(phy_info_t *pi);
#endif

static bool wlc_sslpnphy_fcc_chan_check(phy_info_t *pi, uint channel);
void wlc_phy_chanspec_set_fixup_sslpnphy(phy_info_t *pi, chanspec_t chanspec);
static void wlc_sslpnphy_noise_init(phy_info_t *pi);

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  function implementation   					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

void
wlc_sslpnphy_write_table(phy_info_t *pi, const phytbl_info_t *pti)
{
	wlc_phy_write_table(pi, pti, SSLPNPHY_TableAddress,
	                    SSLPNPHY_TabledataHi, SSLPNPHY_TabledataLo);
}

void
wlc_sslpnphy_read_table(phy_info_t *pi, phytbl_info_t *pti)
{
	wlc_phy_read_table(pi, pti, SSLPNPHY_TableAddress,
	                   SSLPNPHY_TabledataHi, SSLPNPHY_TabledataLo);
}

static void
wlc_sslpnphy_common_read_table(phy_info_t *pi, uint32 tbl_id,
	CONST void *tbl_ptr, uint32 tbl_len, uint32 tbl_width,
	uint32 tbl_offset) {
	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_sslpnphy_read_table(pi, &tab);
}

static void
wlc_sslpnphy_common_write_table(phy_info_t *pi, uint32 tbl_id, CONST void *tbl_ptr,
	uint32 tbl_len, uint32 tbl_width, uint32 tbl_offset) {

	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_sslpnphy_write_table(pi, &tab);
}

static uint
wlc_sslpnphy_init_radio_regs(phy_info_t *pi, sslpnphy_radio_regs_t *radioregs, uint16 core_offset)
{
	uint i = 0;

	do {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			if (radioregs[i].address & 0x8000) {
				write_radio_reg(pi, (radioregs[i].address & 0x3fff) | core_offset,
					(uint16)radioregs[i].init_a);
			}
		} else {
			if (radioregs[i].address & 0x4000) {
				write_radio_reg(pi, (radioregs[i].address & 0x3fff) | core_offset,
					(uint16)radioregs[i].init_g);
			}
		}

		i++;
	} while (radioregs[i].address != 0xffff);
	if ((pi->u.pi_sslpnphy->sslpnphy_fabid == 2) ||
		(pi->u.pi_sslpnphy->sslpnphy_fabid_otp == TSMC_FAB12)) {
		write_radio_reg(pi, RADIO_2063_GRX_SP_6, 0);
		write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x33);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVR_1, 0xc0);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_4, 0x0);
	}
	return i;
}

#if defined(WLTEST)
static void
wlc_sslpnphy_reset_radio_loft(phy_info_t *pi)
{
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, 0x88);
}
#endif

static void
WLBANDINITFN(wlc_sslpnphy_load_tx_gain_table) (phy_info_t *pi,
        const sslpnphy_tx_gain_tbl_entry * gain_table)
{
	uint16 j;
	uint32 val;
	uint16 pa_gain;

	if (CHSPEC_IS5G(pi->radio_chanspec))
		pa_gain = 0x70;
	else
		pa_gain = 0x70;

	/* 5356: External PA support */
	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->boardflags & BFL_HGPA)) {
		pa_gain = 0x20;
	}

	for (j = 0; j < 128; j++) {
		val = ((uint32)pa_gain << 24) |
			(gain_table[j].pad << 16) |
			(gain_table[j].pga << 8) |
			(gain_table[j].gm << 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET + j);

		val = (gain_table[j].dac << 28) |
			(gain_table[j].bb_mult << 20);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + j);
	}
}

static void
wlc_sslpnphy_load_rfpower(phy_info_t *pi)
{
	uint32 val;
	uint8 index;
	for (index = 0; index < 128; index++) {
		val = index * 32 / 10;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + index);
	}
}

/* initialize the static tables defined in auto-generated lpphytbls.c,
 * see lpphyprocs.tcl, proc lpphy_tbl_init
 * After called in the attach stage, all the static phy tables are reclaimed.
 */
static void
BCMATTACHFN(wlc_sslpnphy_store_tbls)(phy_info_t *pi)
{

	bool x17_board_flag = ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);
	bool N90_board_flag = ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID) ? 1 : 0);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		/* Allocate software memory for the 2G tables */
		ph->sslpnphy_gain_idx_2g = (uint32 *)MALLOC(pi->sh->osh, 152 * sizeof(uint32));
		ph->sslpnphy_gain_tbl_2g = (uint16 *)MALLOC(pi->sh->osh, 96 * sizeof(uint16));
		ph->sslpnphy_swctrl_lut_2g = (uint16 *)MALLOC(pi->sh->osh, 64 * sizeof(uint16));

		bcopy(gain_idx_tbl_rev0, ph->sslpnphy_gain_idx_2g,
			gain_idx_tbl_rev0_sz);
		bcopy(gain_tbl_rev0, ph->sslpnphy_gain_tbl_2g,
			gain_tbl_rev0_sz);
		bcopy(sw_ctrl_tbl_rev0, ph->sslpnphy_swctrl_lut_2g,
			sw_ctrl_tbl_rev0_sz);
		if ((pi->sh->boardflags & BFL_EXTLNA)) {
			bcopy(sslpnphy_gain_idx_extlna_cmrxaci_tbl, ph->sslpnphy_gain_idx_2g,
				sslpnphy_gain_idx_extlna_cmrxaci_tbl_sz);
			bcopy(sslpnphy_gain_extlna_cmrxaci_tbl, ph->sslpnphy_gain_tbl_2g,
				sslpnphy_gain_extlna_cmrxaci_tbl_sz);
			if (x17_board_flag || N90_board_flag) {
				bcopy(sslpnphy_gain_idx_extlna_2g_x17, ph->sslpnphy_gain_idx_2g,
					sslpnphy_gain_idx_extlna_2g_x17_sz);
				bcopy(sslpnphy_gain_tbl_extlna_2g_x17, ph->sslpnphy_gain_tbl_2g,
					sslpnphy_gain_tbl_extlna_2g_x17_sz);

			}
			if (x17_board_flag) {
				bcopy(sw_ctrl_tbl_rev0_olympic_x17_2g, ph->sslpnphy_swctrl_lut_2g,
					sw_ctrl_tbl_rev0_olympic_x17_2g_sz);
			}
		}
	}

}

static void
WLBANDINITFN(wlc_sslpnphy_restore_tbls)(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			ph->sslpnphy_gain_idx_2g, 152, 32, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
			ph->sslpnphy_gain_tbl_2g, 96, 16, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
			ph->sslpnphy_swctrl_lut_2g, 64, 16, 0);
	}
#ifdef BAND5G
	else {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			ph->sslpnphy_gain_idx_5g, 152, 32, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
			ph->sslpnphy_gain_tbl_5g, 96, 16, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
			ph->sslpnphy_swctrl_lut_5g, 64, 16, 0);

	}
#endif
}

static void
WLBANDINITFN(wlc_sslpnphy_static_table_download)(phy_info_t *pi)
{
	uint idx;
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev0; idx++) {
			wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev0[idx]);
		}
		/* Restore the tables which were reclaimed */
		wlc_sslpnphy_restore_tbls(pi);
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev2; idx++) {
			if (pi->sh->boardflags & BFL_FEM_BT)
				wlc_sslpnphy_write_table(pi,
					&dot11sslpnphytbl_info_rev2_shared[idx]);
			else
				wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev2[idx]);
		}
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev3; idx++) {
				wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev3[idx]);
		}
	}

	if (CHSPEC_IS2G(pi->radio_chanspec) && CHSPEC_IS40(pi->radio_chanspec)) {
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			for (idx = 0; idx < dot11lpphy_rx_gain_init_tbls_40Mhz_sz_rev3; idx++) {
				wlc_sslpnphy_write_table(pi,
					&dot11lpphy_rx_gain_init_tbls_40Mhz_rev3[idx]);
			}
		} else {
			for (idx = 0; idx < dot11lpphy_rx_gain_init_tbls_40Mhz_sz; idx++) {
				wlc_sslpnphy_write_table(pi,
					&dot11lpphy_rx_gain_init_tbls_40Mhz[idx]);
			}
		}
	}
}

static void
wlc_sslpnphy_clear_papd_comptable(phy_info_t *pi)
{
	uint32 j;
	uint32 temp_offset[128];
	bzero(temp_offset, sizeof(temp_offset));
	for (j = 1; j < 128; j += 2)
		temp_offset[j] = 0x80000;
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
		temp_offset, 128, 32, 0);
}

/* initialize all the tables defined in auto-generated sslpnphytbls.c,
 * see sslpnphyprocs.tcl, proc sslpnphy_tbl_init
 */
static void
WLBANDINITFN(wlc_sslpnphy_tbl_init)(phy_info_t *pi)
{
	uint idx;
	uint16 j;
	uint val;
	uint32 tbl_val[2];
	uint8 phybw40 = IS40MHZ(pi);
	bool x17_board_flag = ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);
	bool N90_board_flag = ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID) ? 1 : 0);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Resetting the txpwrctrl tbl */
	val = 0;
	for (j = 0; j < 703; j++) {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, j);
	}

	wlc_sslpnphy_static_table_download(pi);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		wlc_sslpnphy_load_tx_gain_table(pi, dot11sslpnphy_2GHz_gaintable_rev0);
		if ((pi->sh->boardflags & BFL_EXTLNA) && (SSLPNREV_LT(pi->pubpi.phy_rev, 2))) {
			if (x17_board_flag || N90_board_flag) {
				tbl_val[0] = 0xee;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
					tbl_val, 1, 32, 65);
			}
		}
	}
#ifdef BAND5G
	else {
		wlc_sslpnphy_write_table(pi, &dot11lpphy_rx_gain_init_tbls_A);
		tbl_val[0] = 0x00E38208;
		tbl_val[1] = 0x00E38208;
		wlc_sslpnphy_common_write_table(pi, 12, tbl_val, 2, 32, 0);
		tbl_val[0] = 0xfa;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
			tbl_val, 1, 32, 64);
		if (x17_board_flag) {
			tbl_val[0] = 0xf2;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
				tbl_val, 1, 32, 65);
		}
	}
#endif /* BAND5G */
	if ((SSLPNREV_GE(pi->pubpi.phy_rev, 2)) && (phybw40 == 1)) {
		for (idx = 0; idx < dot11lpphy_rx_gain_init_tbls_40Mhz_sz; idx++) {
			wlc_sslpnphy_write_table(pi, &dot11lpphy_rx_gain_init_tbls_40Mhz[idx]);
		}
	}
	wlc_sslpnphy_load_rfpower(pi);

	/* clear our PAPD Compensation table */
	wlc_sslpnphy_clear_papd_comptable(pi);
}

/* Reclaimable strings used by wlc_phy_txpwr_srom_read_sslpnphy */
#ifdef BAND5G
static const char BCMATTACHDATA(rstr_tri5gl)[] = "tri5gl";
static const char BCMATTACHDATA(rstr_tri5g)[] = "tri5g";
static const char BCMATTACHDATA(rstr_tri5gh)[] = "tri5gh";
static const char BCMATTACHDATA(rstr_bxa5g)[] = "bxa5g";
static const char BCMATTACHDATA(rstr_rxpo5g)[] = "rxpo5g";
static const char BCMATTACHDATA(rstr_rssismf5g)[] = "rssismf5g";
static const char BCMATTACHDATA(rstr_rssismc5g)[] = "rssismc5g";
static const char BCMATTACHDATA(rstr_rssisav5g)[] = "rssisav5g";
static const char BCMATTACHDATA(rstr_pa1maxpwr)[] = "pa1maxpwr";
static const char BCMATTACHDATA(rstr_pa1b_d)[] = "pa1b%d";
static const char BCMATTACHDATA(rstr_pa1lob_d)[] = "pa1lob%d";
static const char BCMATTACHDATA(rstr_pa1hib_d)[] = "pa1hib%d";
static const char BCMATTACHDATA(rstr_ofdmapo)[] = "ofdmapo";
static const char BCMATTACHDATA(rstr_ofdmalpo)[] = "ofdmalpo";
static const char BCMATTACHDATA(rstr_pa1lomaxpwr)[] = "pa1lomaxpwr";
static const char BCMATTACHDATA(rstr_ofdmahpo)[] = "ofdmahpo";
static const char BCMATTACHDATA(rstr_pa1himaxpwr)[] = "pa1himaxpwr";
#endif /* BAND5G */
static const char BCMATTACHDATA(rstr_tri2g)[] = "tri2g";
static const char BCMATTACHDATA(rstr_bxa2g)[] = "bxa2g";
static const char BCMATTACHDATA(rstr_rxpo2g)[] = "rxpo2g";
static const char BCMATTACHDATA(rstr_cckdigfilttype)[] = "cckdigfilttype";
static const char BCMATTACHDATA(rstr_ofdmdigfilttype)[] = "ofdmdigfilttype";
static const char BCMATTACHDATA(rstr_rxpo2gchnflg)[] = "rxpo2gchnflg";
static const char BCMATTACHDATA(rstr_rssismf2g)[] = "rssismf2g";
static const char BCMATTACHDATA(rstr_rssismc2g)[] = "rssismc2g";
static const char BCMATTACHDATA(rstr_rssisav2g)[] = "rssisav2g";
static const char BCMATTACHDATA(rstr_pa0maxpwr)[] = "pa0maxpwr";
static const char BCMATTACHDATA(rstr_pa0b_d)[] = "pa0b%d";
static const char BCMATTACHDATA(rstr_cckpo)[] = "cckpo";
static const char BCMATTACHDATA(rstr_ofdmpo)[] = "ofdmpo";
static const char BCMATTACHDATA(rstr_opo)[] = "opo";

/* Read band specific data from the SROM */
static bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_sslpnphy)(phy_info_t *pi)
{
	char varname[32];
	int8 txpwr = 0;
	int i;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	/* Band specific setup */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		uint32 offset;

		/* TR switch isolation */
		ph->sslpnphy_tr_isolation_low = (uint8)PHY_GETINTVAR(pi, rstr_tri5gl);
		ph->sslpnphy_tr_isolation_mid = (uint8)PHY_GETINTVAR(pi, rstr_tri5g);
		ph->sslpnphy_tr_isolation_hi = (uint8)PHY_GETINTVAR(pi, rstr_tri5gh);

		/* Board switch architecture */
		ph->sslpnphy_bx_arch = (uint8)PHY_GETINTVAR(pi, rstr_bxa5g);

		/* Input power offset */
		ph->sslpnphy_rx_power_offset_5g = (uint8)PHY_GETINTVAR(pi, rstr_rxpo5g);

		/* RSSI */
		ph->sslpnphy_rssi_vf = (uint8)PHY_GETINTVAR(pi, rstr_rssismf5g);
		ph->sslpnphy_rssi_vc = (uint8)PHY_GETINTVAR(pi, rstr_rssismc5g);
		ph->sslpnphy_rssi_gs = (uint8)PHY_GETINTVAR(pi, rstr_rssisav5g);

		/* Max tx power */
		txpwr = (int8)PHY_GETINTVAR(pi, rstr_pa1maxpwr);

		/* PA coeffs */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), rstr_pa1b_d, i);
			pi->txpa_5g_mid[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* Low channels */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), rstr_pa1lob_d, i);
			pi->txpa_5g_low[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* High channels */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), rstr_pa1hib_d, i);
			pi->txpa_5g_hi[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* The *po variables introduce a separate max tx power for reach rate.
		 * Each per-rate txpower is specified as offset from the maxtxpower
		 * from the maxtxpwr in that band (lo,mid,hi).
		 * The offsets in the variables is stored in half dbm units to save
		 * srom space, which need to be doubled to convert to quarter dbm units
		 * before using.
		 * For small 1Kbit sroms of PCI/PCIe cards, the getintav will always return 0;
		 * For bigger sroms or NVRAM or CIS, they are present
		 */

		/* Mid band channels */
		/* Extract 8 OFDM rates for mid channels */
		offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmapo);
		pi->tx_srom_max_5g_mid = txpwr;

		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_mid[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}

		/* Extract 8 OFDM rates for low channels */
		offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmalpo);
		pi->tx_srom_max_5g_low = txpwr =
			(int8)PHY_GETINTVAR(pi, rstr_pa1lomaxpwr);
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_low[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}

		/* Extract 8 OFDM rates for hi channels */
		offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmahpo);
		pi->tx_srom_max_5g_hi = txpwr =
			(int8)PHY_GETINTVAR(pi, rstr_pa1himaxpwr);

		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_hi[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}
	}
#endif /* BAND5G */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		uint16 cckpo = 0;
		uint32 offset_ofdm, offset_mcs;

		/* TR switch isolation */
		ph->sslpnphy_tr_isolation_mid = (uint8)PHY_GETINTVAR(pi, rstr_tri2g);

		/* Board switch architecture */
		ph->sslpnphy_bx_arch = (uint8)PHY_GETINTVAR(pi, rstr_bxa2g);
		pi->phy_aa2g = (uint8)PHY_GETINTVAR(pi, "aa2g");

		/* Input power offset */
		ph->sslpnphy_rx_power_offset = (uint8)PHY_GETINTVAR(pi, rstr_rxpo2g);

		/* Sslpnphy  filter select */
		ph->sslpnphy_cck_filt_sel = (uint8)PHY_GETINTVAR(pi, rstr_cckdigfilttype);
		ph->sslpnphy_ofdm_filt_sel = (uint8)PHY_GETINTVAR(pi, rstr_ofdmdigfilttype);

		/* Channel based selection for rxpo2g */
		ph->sslpnphy_rxpo2gchnflg = (uint16)PHY_GETINTVAR(pi, rstr_rxpo2gchnflg);

		/* force periodic cal */
		ph->sslpnphy_force_percal = (uint8)PHY_GETINTVAR(pi, "forcepercal");

		/* RSSI */
		ph->sslpnphy_rssi_vf = (uint8)PHY_GETINTVAR(pi, rstr_rssismf2g);
		ph->sslpnphy_rssi_vc = (uint8)PHY_GETINTVAR(pi, rstr_rssismc2g);
		ph->sslpnphy_rssi_gs = (uint8)PHY_GETINTVAR(pi, rstr_rssisav2g);

		{
			ph->sslpnphy_rssi_vf_lowtemp = ph->sslpnphy_rssi_vf;
			ph->sslpnphy_rssi_vc_lowtemp = ph->sslpnphy_rssi_vc;
			ph->sslpnphy_rssi_gs_lowtemp = ph->sslpnphy_rssi_gs;

			ph->sslpnphy_rssi_vf_hightemp = ph->sslpnphy_rssi_vf;
			ph->sslpnphy_rssi_vc_hightemp = ph->sslpnphy_rssi_vc;
			ph->sslpnphy_rssi_gs_hightemp = ph->sslpnphy_rssi_gs;
		}

		/* Max tx power */
		txpwr = (int8)PHY_GETINTVAR(pi, rstr_pa0maxpwr);
		pi->tx_srom_max_2g = txpwr;

		/* PA coeffs */
		for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
			snprintf(varname, sizeof(varname), rstr_pa0b_d, i);
			pi->txpa_2g[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		{
			for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
				pi->txpa_2g_low_temp[i] = pi->txpa_2g[i];
				pi->txpa_2g_high_temp[i] = pi->txpa_2g[i];
			}
		}

		cckpo = (uint16)PHY_GETINTVAR(pi, rstr_cckpo);
		if (cckpo) {
			uint max_pwr_chan = txpwr;

			/* Extract offsets for 4 CCK rates. Remember to convert from
			* .5 to .25 dbm units
			*/
			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = max_pwr_chan -
					((cckpo & 0xf) * 2);
				cckpo >>= 4;
			}
		} else {
			/* Populate max power array for CCK rates */
			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr;
			}
		}

		/* Extract offsets for 8 OFDM rates */
		offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdm2gpo");
		if (offset_ofdm == 0)
			offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdmpo");

		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr -
				((offset_ofdm & 0xf) * 2);
			offset_ofdm >>= 4;
		}

		pi->mcs20_po = offset_mcs =
			((uint16)PHY_GETINTVAR(pi, "mcs2gpo1") << 16) |
			(uint16)PHY_GETINTVAR(pi, "mcs2gpo0");

		for (i = TXP_FIRST_SISO_MCS_20; i <= TXP_LAST_SISO_MCS_20;  i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr -
				((offset_mcs & 0xf) * 2);
			offset_mcs >>= 4;
		}

		pi->mcs40_po = ((uint16)PHY_GETINTVAR(pi, "mcs2gpo3") << 16) |
			(uint16)PHY_GETINTVAR(pi, "mcs2gpo2");

		if (pi->mcs40_po == 0) {
			/* 4319 does not use mcs2gpo[2,3], use 20MHz power
			 * offsets
			 */
			pi->mcs40_po = pi->mcs20_po;
		}
	}
	return TRUE;
}

static void
WLBANDINITFN(wlc_sslpnphy_rev0_baseband_init)(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		or_phy_reg(pi, SSLPNPHY_lpphyCtrl, SSLPNPHY_lpphyCtrl_muxGmode_MASK);
		or_phy_reg(pi, SSLPNPHY_crsgainCtrl, SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK);
	} else {
		and_phy_reg(pi, SSLPNPHY_lpphyCtrl, (uint16)~SSLPNPHY_lpphyCtrl_muxGmode_MASK);
		and_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			(uint16)~SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK);
	}

	mod_phy_reg(pi, SSLPNPHY_lpphyCtrl,
		SSLPNPHY_lpphyCtrl_txfiltSelect_MASK,
		1 << SSLPNPHY_lpphyCtrl_txfiltSelect_SHIFT);

	/* Enable DAC/ADC and disable rf overrides */
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
		write_phy_reg(pi, SSLPNPHY_AfeDACCtrl, 0x54);
	else
		write_phy_reg(pi, SSLPNPHY_AfeDACCtrl, 0x50);

	write_phy_reg(pi, SSLPNPHY_AfeCtrl, 0x8800);
	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvr, 0x0000);
	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal, 0x0000);
	write_phy_reg(pi, SSLPNPHY_RFinputOverride, 0x0000);
	write_phy_reg(pi, SSLPNPHY_RFOverride0, 0x0000);
	write_phy_reg(pi, SSLPNPHY_rfoverride2, 0x0000);
	write_phy_reg(pi, SSLPNPHY_rfoverride3, 0x0000);
	write_phy_reg(pi, SSLPNPHY_swctrlOvr, 0x0000);

	mod_phy_reg(pi, SSLPNPHY_RxIqCoeffCtrl,
		SSLPNPHY_RxIqCoeffCtrl_RxIqCrsCoeffOverRide_MASK,
		1 << SSLPNPHY_RxIqCoeffCtrl_RxIqCrsCoeffOverRide_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_RxIqCoeffCtrl,
		SSLPNPHY_RxIqCoeffCtrl_RxIqCrsCoeffOverRide11b_MASK,
		1 << SSLPNPHY_RxIqCoeffCtrl_RxIqCrsCoeffOverRide11b_SHIFT);

	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);

	/* RSSI settings */
	write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl0, 0xA954);

	write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1,
		((uint16)ph->sslpnphy_rssi_vf_lowtemp << 0) | /* selmid_rssi: RSSI Vmid fine */
		((uint16)ph->sslpnphy_rssi_vc_lowtemp << 4) | /* selmid_rssi: RSSI Vmid coarse */
		(0x00 << 8) | /* selmid_rssi: default value from AMS */
		((uint16)ph->sslpnphy_rssi_gs_lowtemp << 10) | /* selav_rssi: RSSI gain select */
		(0x01 << 13)); /* slpinv_rssi */

}

static void
wlc_sslpnphy_bu_tweaks(phy_info_t *pi)
{
	uint8 phybw40;
	int8 aa;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 500);
		return;
	}

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		/* CRS Parameters tuning */
		mod_phy_reg(pi, SSLPNPHY_gaindirectMismatch,
			SSLPNPHY_gaindirectMismatch_medGainGmShftVal_MASK,
			3 << SSLPNPHY_gaindirectMismatch_medGainGmShftVal_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
			SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK |
			SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_MASK,
			30 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT |
			20 << SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_HiGainDB,
			SSLPNPHY_HiGainDB_HiGainDB_MASK |
			SSLPNPHY_HiGainDB_MedHiGainDB_MASK,
			70 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT |
			45 << SSLPNPHY_HiGainDB_MedHiGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
			SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK |
			SSLPNPHY_VeryLowGainDB_NominalPwrDB_MASK,
			6 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT |
			95 << SSLPNPHY_VeryLowGainDB_NominalPwrDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
			SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK |
			SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
			1 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT |
			25 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs2,
			SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_MASK,
			12 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_gainMismatch,
			SSLPNPHY_gainMismatch_GainMismatchHigain_MASK,
			10 << SSLPNPHY_gainMismatch_GainMismatchHigain_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
			SSLPNPHY_PwrThresh1_LargeGainMismatchThresh_MASK,
			9 << SSLPNPHY_PwrThresh1_LargeGainMismatchThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_gainMismatchMedGainEx,
			SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_MASK,
			3 << SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsMiscCtrl2,
			SSLPNPHY_crsMiscCtrl2_eghtSmplFstPwrLogicEn_MASK,
			0 << SSLPNPHY_crsMiscCtrl2_eghtSmplFstPwrLogicEn_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsTimingCtrl,
			SSLPNPHY_crsTimingCtrl_gainThrsh4Timing_MASK |
			SSLPNPHY_crsTimingCtrl_gainThrsh4MF_MASK,
			0 << SSLPNPHY_crsTimingCtrl_gainThrsh4Timing_SHIFT |
			73 << SSLPNPHY_crsTimingCtrl_gainThrsh4MF_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_ofdmSyncThresh1,
			SSLPNPHY_ofdmSyncThresh1_ofdmSyncThresh2_MASK,
			2 << SSLPNPHY_ofdmSyncThresh1_ofdmSyncThresh2_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_SyncPeakCnt,
			SSLPNPHY_SyncPeakCnt_MaxPeakCntM1_MASK,
			7 << SSLPNPHY_SyncPeakCnt_MaxPeakCntM1_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
			SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_MASK,
			3 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
			SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			255 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_MinPwrLevel,
			SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_MASK,
			162 << SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_LowGainDB,
			SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
			29 << SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
			SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_MASK,
			244 << SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
			SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
			10 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsMiscCtrl0,
			SSLPNPHY_crsMiscCtrl0_usePreFiltPwr_MASK,
			0 << SSLPNPHY_crsMiscCtrl0_usePreFiltPwr_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_ofdmPwrThresh1,
			SSLPNPHY_ofdmPwrThresh1_ofdmPwrThresh3_MASK,
			48 << SSLPNPHY_ofdmPwrThresh1_ofdmPwrThresh3_SHIFT);

		write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6033);
		write_phy_reg(pi, SSLPNPHY_ClipThresh, 108);
		write_phy_reg(pi, SSLPNPHY_SgiprgReg, 3);

		if (phybw40 == 1)
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
		else
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				1 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
				SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK,
				0 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT);
			aa = (uint8)getintvar(pi->vars, "aa5g");
		} else {
			aa = (uint8)getintvar(pi->vars, "aa2g");
		}
		if (aa > 1) {

			/* Antenna diveristy related changes */
			mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
				SSLPNPHY_crsgainCtrl_wlpriogainChangeEn_MASK |
				SSLPNPHY_crsgainCtrl_preferredAntEn_MASK,
				0 << SSLPNPHY_crsgainCtrl_wlpriogainChangeEn_SHIFT |
				0 << SSLPNPHY_crsgainCtrl_preferredAntEn_SHIFT);
			write_phy_reg(pi, SSLPNPHY_lnaputable, 0x5555);
			mod_phy_reg(pi, SSLPNPHY_radioCtrl,
				SSLPNPHY_radioCtrl_auxgaintblEn_MASK,
				0 << SSLPNPHY_radioCtrl_auxgaintblEn_SHIFT);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg0, 0x4210);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg1, 0x4210);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg2, 0x0270);
			/* mod_phy_reg(pi, SSLPNPHY_PwrThresh1, */
			/*	SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_MASK, */
			/*	20 << SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_SHIFT); */
			if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
					SSLPNPHY_Rev2_crsgainCtrl_40_wlpriogainChangeEn_MASK |
					SSLPNPHY_Rev2_crsgainCtrl_40_preferredAntEn_MASK,
					0 << SSLPNPHY_Rev2_crsgainCtrl_40_wlpriogainChangeEn_SHIFT |
					0 << SSLPNPHY_Rev2_crsgainCtrl_40_preferredAntEn_SHIFT);
			}
			/* enable Diversity for  Dual Antenna Boards */
			if (aa > 2) {
				if (phybw40)
					mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
					SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_MASK,
					0x01 <<
					SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_SHIFT);
				else
					mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DiversityChkEnable_MASK,
					0x01 << SSLPNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
			}
		}
		if (ph->sslpnphy_OLYMPIC)
			mod_phy_reg(pi, SSLPNPHY_BphyControl3,
				SSLPNPHY_BphyControl3_bphyScale_MASK,
				0x7 << SSLPNPHY_BphyControl3_bphyScale_SHIFT);
		else
			mod_phy_reg(pi, SSLPNPHY_BphyControl3,
				SSLPNPHY_BphyControl3_bphyScale_MASK,
				0xc << SSLPNPHY_BphyControl3_bphyScale_SHIFT);

		/* Change timing to 11.5us */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 40);
		ph->sslpnphy_current_index = 40;
		write_phy_reg(pi, SSLPNPHY_TxMacIfHoldOff, 23);
		write_phy_reg(pi, SSLPNPHY_TxMacDelay, 1002);
		/* Adjust RIFS timings */
		if (phybw40 == 0) {
			write_phy_reg(pi, SSLPNPHY_rifsSttimeout, 0x1214);
			write_phy_reg(pi, SSLPNPHY_readsym2resetCtrl, 0x7800);
		}
	}


	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		/* CRS Parameters tuning */
		mod_phy_reg(pi, SSLPNPHY_gaindirectMismatch,
			SSLPNPHY_gaindirectMismatch_medGainGmShftVal_MASK,
			3 << SSLPNPHY_gaindirectMismatch_medGainGmShftVal_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
			SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK |
			SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_MASK,
			30 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT |
			20 << SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_SHIFT);

		if (phybw40 == 1)
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
		else
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				1 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
			SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK |
			SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
			1 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT |
			25 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_HiGainDB,
			SSLPNPHY_HiGainDB_HiGainDB_MASK |
			SSLPNPHY_HiGainDB_MedHiGainDB_MASK,
			70 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT |
			45 << SSLPNPHY_HiGainDB_MedHiGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
			SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
			9 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_gainMismatch,
			SSLPNPHY_gainMismatch_GainMismatchHigain_MASK,
			10 << SSLPNPHY_gainMismatch_GainMismatchHigain_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsMiscCtrl2,
			SSLPNPHY_crsMiscCtrl2_eghtSmplFstPwrLogicEn_MASK,
			0 << SSLPNPHY_crsMiscCtrl2_eghtSmplFstPwrLogicEn_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
			SSLPNPHY_PwrThresh1_LargeGainMismatchThresh_MASK,
			4 << SSLPNPHY_PwrThresh1_LargeGainMismatchThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
			SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			255 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_MinPwrLevel,
			SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_MASK,
			162 << SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_MinPwrLevel,
			SSLPNPHY_MinPwrLevel_dsssMinPwrLevel_MASK,
			158 << SSLPNPHY_MinPwrLevel_dsssMinPwrLevel_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_phycrsctrl_MASK,
			11 << SSLPNPHY_crsgainCtrl_phycrsctrl_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_SyncPeakCnt,
			SSLPNPHY_SyncPeakCnt_MaxPeakCntM1_MASK,
			7 << SSLPNPHY_SyncPeakCnt_MaxPeakCntM1_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
			SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK |
			SSLPNPHY_VeryLowGainDB_NominalPwrDB_MASK,
			6 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT |
			95 << SSLPNPHY_VeryLowGainDB_NominalPwrDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_LowGainDB,
			SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
			29 << SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
			SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_MASK,
			244 << SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
			SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
			10 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_crsTimingCtrl,
			SSLPNPHY_crsTimingCtrl_gainThrsh4Timing_MASK |
			SSLPNPHY_crsTimingCtrl_gainThrsh4MF_MASK,
			0 << SSLPNPHY_crsTimingCtrl_gainThrsh4Timing_SHIFT |
			73 << SSLPNPHY_crsTimingCtrl_gainThrsh4MF_SHIFT);

		write_phy_reg(pi, SSLPNPHY_SgiprgReg, 3);

		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
			SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
			20 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs2,
			SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_MASK,
			11 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_SHIFT);

		write_phy_reg(pi, SSLPNPHY_ClipThresh, 72);

		mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
			SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_MASK,
			18 << SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_SHIFT);

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3))
			mod_phy_reg(pi, SSLPNPHY_HiGainDB,
				SSLPNPHY_HiGainDB_MedHiGainDB_MASK,
				45 << SSLPNPHY_HiGainDB_MedHiGainDB_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_ofdmSyncThresh1,
			SSLPNPHY_ofdmSyncThresh1_ofdmSyncThresh2_MASK,
			2 << SSLPNPHY_ofdmSyncThresh1_ofdmSyncThresh2_SHIFT);

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			if (phybw40) {
				mod_phy_reg(pi, SSLPNPHY_BphyControl3,
					SSLPNPHY_BphyControl3_bphyScale_MASK,
					0x11 << SSLPNPHY_BphyControl3_bphyScale_SHIFT);
			} else {
				mod_phy_reg(pi, SSLPNPHY_BphyControl3,
					SSLPNPHY_BphyControl3_bphyScale_MASK,
					0x13 << SSLPNPHY_BphyControl3_bphyScale_SHIFT);
			}

			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
				SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK,
				0 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				12 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
				SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
				9 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);

			if (!phybw40) {
				mod_phy_reg(pi, SSLPNPHY_bphyacireg,
					SSLPNPHY_bphyacireg_enBphyAciFilt_MASK,
					1 << SSLPNPHY_bphyacireg_enBphyAciFilt_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_bphyacireg,
					SSLPNPHY_bphyacireg_enDmdBphyAciFilt_MASK,
					1 << SSLPNPHY_bphyacireg_enDmdBphyAciFilt_SHIFT);
			} else {
				mod_phy_reg(pi, SSLPNPHY_bphyacireg,
					SSLPNPHY_bphyacireg_enBphyAciFilt_MASK,
					0 << SSLPNPHY_bphyacireg_enBphyAciFilt_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_bphyacireg,
					SSLPNPHY_bphyacireg_enDmdBphyAciFilt_MASK,
					0 << SSLPNPHY_bphyacireg_enDmdBphyAciFilt_SHIFT);
			}

			mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
				SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
				11 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_MASK,
				4 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_MASK,
				4 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGainCnfrm_MASK,
				2 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGainCnfrm_SHIFT);

		} else {
			mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_MASK,
				3 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_SHIFT);

			if (phybw40)
			{
					mod_phy_reg(pi, SSLPNPHY_BphyControl3,
						SSLPNPHY_BphyControl3_bphyScale_MASK,
						0x8 << SSLPNPHY_BphyControl3_bphyScale_SHIFT);
				} else {
					mod_phy_reg(pi, SSLPNPHY_BphyControl3,
						SSLPNPHY_BphyControl3_bphyScale_MASK,
						0xc << SSLPNPHY_BphyControl3_bphyScale_SHIFT);
				}
			mod_phy_reg(pi, SSLPNPHY_lnsrOfParam1,
				SSLPNPHY_lnsrOfParam1_ofdmSyncConfirmAdjst_MASK,
				5 << SSLPNPHY_lnsrOfParam1_ofdmSyncConfirmAdjst_SHIFT);
		}

		mod_phy_reg(pi, SSLPNPHY_ofdmPwrThresh1,
			SSLPNPHY_ofdmPwrThresh1_ofdmPwrThresh3_MASK,
			48 << SSLPNPHY_ofdmPwrThresh1_ofdmPwrThresh3_SHIFT);

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
				SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		} else {
			mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
				SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		}

		if ((int8)getintvar(pi->vars, "aa2g") > 1) {
			/* Antenna diveristy related changes */
			mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
				SSLPNPHY_crsgainCtrl_wlpriogainChangeEn_MASK,
				0 << SSLPNPHY_crsgainCtrl_wlpriogainChangeEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
				SSLPNPHY_crsgainCtrl_preferredAntEn_MASK,
				0 << SSLPNPHY_crsgainCtrl_preferredAntEn_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
				SSLPNPHY_Rev2_crsgainCtrl_40_wlpriogainChangeEn_MASK,
				0 << SSLPNPHY_Rev2_crsgainCtrl_40_wlpriogainChangeEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
				SSLPNPHY_Rev2_crsgainCtrl_40_preferredAntEn_MASK,
				0 << SSLPNPHY_Rev2_crsgainCtrl_40_preferredAntEn_SHIFT);

			write_phy_reg(pi, SSLPNPHY_lnaputable, 0x5555);
			mod_phy_reg(pi, SSLPNPHY_radioCtrl,
				SSLPNPHY_radioCtrl_auxgaintblEn_MASK,
				0 << SSLPNPHY_radioCtrl_auxgaintblEn_SHIFT);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg0, 0x4210);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg1, 0x4210);
			write_phy_reg(pi, SSLPNPHY_slnanoisetblreg2, 0x0270);

			/* enable Diversity for  Dual Antenna Boards */
			if ((int8)getintvar(pi->vars, "aa2g") > 2) {
				if (phybw40) {
					mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
					SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_MASK,
					0x01 <<
					SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_SHIFT);
				} else {
					mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DiversityChkEnable_MASK,
					0x01 << SSLPNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
				}
			}
		}

		mod_phy_reg(pi, SSLPNPHY_crsMiscCtrl0,
			SSLPNPHY_crsMiscCtrl0_usePreFiltPwr_MASK,
			0 << SSLPNPHY_crsMiscCtrl0_usePreFiltPwr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
			SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_MASK,
			18 << SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_SHIFT);

		write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6366);

		mod_phy_reg(pi, SSLPNPHY_gainMismatchMedGainEx,
			SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_MASK,
			0 << SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_SHIFT);

		if (phybw40 == 1) {
			mod_phy_reg(pi, SSLPNPHY_Rev2_radioCtrl_40mhz,
				SSLPNPHY_Rev2_radioCtrl_40mhz_round_control_40mhz_MASK,
				0 << SSLPNPHY_Rev2_radioCtrl_40mhz_round_control_40mhz_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_gaindirectMismatch_40,
				SSLPNPHY_Rev2_gaindirectMismatch_40_medGainGmShftVal_MASK,
				3 << SSLPNPHY_Rev2_gaindirectMismatch_40_medGainGmShftVal_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_ClipCtrThresh_40,
				SSLPNPHY_Rev2_ClipCtrThresh_40_clipCtrThreshLoGain_MASK,
				36 << SSLPNPHY_Rev2_ClipCtrThresh_40_clipCtrThreshLoGain_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_radioTRCtrl,
				SSLPNPHY_Rev2_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT,
				0  << SSLPNPHY_Rev2_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_radioCtrl_40mhz,
				SSLPNPHY_Rev2_radioCtrl_40mhz_gainReqTrAttOnEnByCrs40_MASK,
				0  << SSLPNPHY_Rev2_radioCtrl_40mhz_gainReqTrAttOnEnByCrs40_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40,
				SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK,
				70  << SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
				SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK,
				9  << SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_gainMismatch_40,
				SSLPNPHY_Rev2_gainMismatch_40_GainMismatchHigain_MASK,
				10  << SSLPNPHY_Rev2_gainMismatch_40_GainMismatchHigain_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsMiscCtrl2_40,
				SSLPNPHY_Rev2_crsMiscCtrl2_40_eghtSmplFstPwrLogicEn_MASK,
				0  << SSLPNPHY_Rev2_crsMiscCtrl2_40_eghtSmplFstPwrLogicEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh1_40,
				SSLPNPHY_Rev2_PwrThresh1_40_LargeGainMismatchThresh_MASK,
				9  << SSLPNPHY_Rev2_PwrThresh1_40_LargeGainMismatchThresh_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				255 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_MinPwrLevel_40,
				SSLPNPHY_Rev2_MinPwrLevel_40_ofdmMinPwrLevel_MASK,
				164 << SSLPNPHY_Rev2_MinPwrLevel_40_ofdmMinPwrLevel_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_MinPwrLevel_40,
				SSLPNPHY_Rev2_MinPwrLevel_40_dsssMinPwrLevel_MASK,
				159 << SSLPNPHY_Rev2_MinPwrLevel_40_dsssMinPwrLevel_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsMiscCtrl0_40,
				SSLPNPHY_Rev2_crsMiscCtrl0_usePreFiltPwr_MASK,
				0 << SSLPNPHY_Rev2_crsMiscCtrl0_usePreFiltPwr_SHIFT);
			write_phy_reg(pi, SSLPNPHY_Rev2_gainBackOffVal_40, 0x6366);
			mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
				SSLPNPHY_Rev2_VeryLowGainDB_40_NominalPwrDB_MASK,
				103  << SSLPNPHY_Rev2_VeryLowGainDB_40_NominalPwrDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_LowGainDB_40,
				SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_MASK,
				29 << SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				11 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				11 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
				11 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_gainMismatchMedGainEx_40,
			SSLPNPHY_Rev2_gainMismatchMedGainEx_40_medHiGainDirectMismatchOFDMDet_MASK,
			3 <<
		      SSLPNPHY_Rev2_gainMismatchMedGainEx_40_medHiGainDirectMismatchOFDMDet_SHIFT);
			/* SGI -56 to -64dBm Hump Fixes */
			write_phy_reg(pi, SSLPNPHY_Rev2_ClipThresh_40, 72);
			mod_phy_reg(pi, SSLPNPHY_Rev2_ClipCtrThresh_40,
				SSLPNPHY_Rev2_ClipCtrThresh_40_ClipCtrThreshHiGain_MASK,
				44 << SSLPNPHY_Rev2_ClipCtrThresh_40_ClipCtrThreshHiGain_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40,
				SSLPNPHY_Rev2_HiGainDB_40_MedHiGainDB_MASK,
				45  << SSLPNPHY_Rev2_HiGainDB_40_MedHiGainDB_SHIFT);

			/* SGI -20 to -32dBm Hump Fixes */
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsTimingCtrl_40,
				SSLPNPHY_Rev2_crsTimingCtrl_40_gainThrsh4MF_MASK,
				73 << SSLPNPHY_Rev2_crsTimingCtrl_40_gainThrsh4MF_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsTimingCtrl_40,
				SSLPNPHY_Rev2_crsTimingCtrl_40_gainThrsh4Timing_MASK,
				0 << SSLPNPHY_Rev2_crsTimingCtrl_40_gainThrsh4Timing_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_crsMiscParams_40,
				SSLPNPHY_Rev2_crsMiscParams_40_incSyncCntVal_MASK,
				0 << SSLPNPHY_Rev2_crsMiscParams_40_incSyncCntVal_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_lpParam2_40,
				SSLPNPHY_Rev2_lpParam2_40_gainSettleDlySmplCnt_MASK,
				60 << SSLPNPHY_Rev2_lpParam2_40_gainSettleDlySmplCnt_SHIFT);

			if ((CHIPID(pi->sh->chip) == BCM4319_CHIP_ID)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams2_20U,
					SSLPNPHY_Rev2_syncParams2_20U_gainThrsh4MF_MASK,
					66 << SSLPNPHY_Rev2_syncParams2_20U_gainThrsh4MF_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams2_20U,
					SSLPNPHY_Rev2_syncParams2_20U_gainThrsh4Timing_MASK,
					0 << SSLPNPHY_Rev2_syncParams2_20U_gainThrsh4Timing_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams1_20U,
					SSLPNPHY_Rev2_syncParams1_20U_incSyncCntVal_MASK,
					0 << SSLPNPHY_Rev2_syncParams1_20U_incSyncCntVal_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams2_20L,
					SSLPNPHY_Rev2_syncParams2_20L_gainThrsh4MF_MASK,
					66 << SSLPNPHY_Rev2_syncParams2_20L_gainThrsh4MF_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams2_20L,
					SSLPNPHY_Rev2_syncParams2_20L_gainThrsh4Timing_MASK,
					0 << SSLPNPHY_Rev2_syncParams2_20L_gainThrsh4Timing_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_syncParams1_20L,
					SSLPNPHY_Rev2_syncParams1_20L_incSyncCntVal_MASK,
					0 << SSLPNPHY_Rev2_syncParams1_20L_incSyncCntVal_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_Rev2_bndWdthClsfy2_40,
				SSLPNPHY_Rev2_bndWdthClsfy2_40_bwClsfyGainThresh_MASK,
				66 <<
				SSLPNPHY_Rev2_bndWdthClsfy2_40_bwClsfyGainThresh_SHIFT);
			}

			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmPwrThresh0_20L,
				SSLPNPHY_Rev2_ofdmPwrThresh0_20L_ofdmPwrThresh0_MASK,
				3 << SSLPNPHY_Rev2_ofdmPwrThresh0_20L_ofdmPwrThresh0_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmPwrThresh1_20L,
				SSLPNPHY_Rev2_ofdmPwrThresh1_20L_ofdmPwrThresh3_MASK,
				48 << SSLPNPHY_Rev2_ofdmPwrThresh1_20L_ofdmPwrThresh3_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmPwrThresh0_20U,
				SSLPNPHY_Rev2_ofdmPwrThresh0_20U_ofdmPwrThresh0_MASK,
				3 << SSLPNPHY_Rev2_ofdmPwrThresh0_20U_ofdmPwrThresh0_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmPwrThresh1_20U,
				SSLPNPHY_Rev2_ofdmPwrThresh1_20U_ofdmPwrThresh3_MASK,
				48 << SSLPNPHY_Rev2_ofdmPwrThresh1_20U_ofdmPwrThresh3_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_radioTRCtrl,
				SSLPNPHY_Rev2_radioTRCtrl_gainrequestTRAttnOnOffset_MASK,
				7 << SSLPNPHY_Rev2_radioTRCtrl_gainrequestTRAttnOnOffset_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_trGainthresh_40,
				SSLPNPHY_Rev2_trGainthresh_40_trGainThresh_MASK,
				20 << SSLPNPHY_Rev2_trGainthresh_40_trGainThresh_SHIFT);

			/* TO REDUCE PER HUMPS @HIGH Rx Powers */
			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmSyncThresh1_40,
				SSLPNPHY_Rev2_ofdmSyncThresh1_40_ofdmSyncThresh2_MASK,
				2 << SSLPNPHY_Rev2_ofdmSyncThresh1_40_ofdmSyncThresh2_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmSyncThresh1_20U,
				SSLPNPHY_Rev2_ofdmSyncThresh1_20U_ofdmSyncThresh2_MASK,
				2 << SSLPNPHY_Rev2_ofdmSyncThresh1_20U_ofdmSyncThresh2_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_ofdmSyncThresh1_20L,
				SSLPNPHY_Rev2_ofdmSyncThresh1_20L_ofdmSyncThresh2_MASK,
				2 << SSLPNPHY_Rev2_ofdmSyncThresh1_20L_ofdmSyncThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_DSSSConfirmCnt_40,
			     SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntHiGainCnfrm_MASK,
			     4 << SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntHiGainCnfrm_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_DSSSConfirmCnt_40,
				SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntLoGain_MASK,
				4 << SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntLoGain_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_DSSSConfirmCnt_40,
			     SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntHiGainCnfrm_MASK,
			     2 << SSLPNPHY_Rev2_DSSSConfirmCnt_40_DSSSConfirmCntHiGainCnfrm_SHIFT);

		}

		/* enable extlna */
		if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
			(pi->sh->boardflags & BFL_EXTLNA)) {

			mod_phy_reg(pi, SSLPNPHY_radioCtrl,
				SSLPNPHY_radioCtrl_extlnaen_MASK,
				1 << SSLPNPHY_radioCtrl_extlnaen_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
				SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
				253 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
				SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK,
				253 << SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				20 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_ClipCtrThresh_40,
				SSLPNPHY_Rev2_ClipCtrThresh_40_clipCtrThreshLoGain_MASK,
				30 << SSLPNPHY_Rev2_ClipCtrThresh_40_clipCtrThreshLoGain_SHIFT);

			write_phy_reg(pi, SSLPNPHY_ClipThresh, 96);
			write_phy_reg(pi, SSLPNPHY_Rev2_ClipThresh_40, 96);

			mod_phy_reg(pi, SSLPNPHY_LowGainDB,
				SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
				26 << SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_LowGainDB_40,
				SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_MASK,
				26 << SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
				SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_MASK,
				24 << SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_SHIFT);

			write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6666);
			write_phy_reg(pi, SSLPNPHY_Rev2_gainBackOffVal_40, 0x6666);
			write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);

			mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
				SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_MASK,
				0 << SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
				SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_MASK,
				3 << SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT);

			{
				phytbl_info_t tab;
				uint32 tableBuffer[2] = {18, -3};
				tab.tbl_ptr = tableBuffer;	/* ptr to buf */
				tab.tbl_len = 2;			/* # values   */
				tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
				tab.tbl_offset = 64;		/* tbl offset */
				tab.tbl_width = 32;			/* 32 bit wide */
				wlc_sslpnphy_write_table(pi, &tab);
			}

			mod_phy_reg(pi, SSLPNPHY_LowGainDB,
				SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
				41 << SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_LowGainDB_40,
				SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_MASK,
				41 << SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
				SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
				12 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
				SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK,
				12  << SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_gainMismatch,
				SSLPNPHY_Rev2_gainMismatch_GainmisMatchPktRx_MASK,
				9 << SSLPNPHY_Rev2_gainMismatch_GainmisMatchPktRx_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_Rev2_crsMiscCtrl0_40,
				SSLPNPHY_Rev2_crsMiscCtrl0_usePreFiltPwr_MASK,
				0 << SSLPNPHY_Rev2_crsMiscCtrl0_usePreFiltPwr_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				12 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
				SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_MASK,
				15 << SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_SHIFT);
		}

		/* Reset radio ctrl and crs gain */
		or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
		write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);

	}

	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->chiprev == 0)) {
		/* tssi does not work on 5356a0; hard code tx power */
		ph->sslpnphy_current_index = 40;
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 40);
	}
}

static void
WLBANDINITFN(wlc_sslpnphy_baseband_init)(phy_info_t *pi)
{
	/* Initialize SSLPNPHY tables */
	wlc_sslpnphy_tbl_init(pi);
	wlc_sslpnphy_rev0_baseband_init(pi);
	wlc_sslpnphy_bu_tweaks(pi);
}

typedef struct {
	uint16 phy_addr;
	uint8 phy_shift;
	uint8 rf_addr;
	uint8 rf_shift;
	uint8 mask;
} sslpnphy_extstxdata_t;

static sslpnphy_extstxdata_t
WLBANDINITDATA(sslpnphy_extstxdata)[] = {
	{SSLPNPHY_extstxctrl0 + 2, 6, 0x3d, 3, 0x1},
	{SSLPNPHY_extstxctrl0 + 1, 12, 0x4c, 1, 0x1},
	{SSLPNPHY_extstxctrl0 + 1, 8, 0x50, 0, 0x7f},
	{SSLPNPHY_extstxctrl0 + 0, 8, 0x44, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 1, 0, 0x4a, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 0, 4, 0x4d, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 1, 4, 0x4e, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 0, 12, 0x4f, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 1, 0, 0x4f, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 3, 0, 0x49, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 3, 0x46, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 15, 0x46, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 4, 0, 0x46, 1, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 8, 0x48, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 11, 0x48, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 3, 4, 0x49, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 2, 15, 0x45, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 13, 0x52, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 6, 0, 0x52, 7, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 3, 0x41, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 5, 6, 0x41, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 5, 10, 0x42, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 4, 15, 0x42, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 0, 0x42, 1, 0x7},
	{SSLPNPHY_extstxctrl0 + 4, 11, 0x43, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 7, 0x43, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 6, 0x45, 1, 0x1},
	{SSLPNPHY_extstxctrl0 + 2, 7, 0x40, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 2, 11, 0x40, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 1, 14, 0x3c, 3, 0x3},
	{SSLPNPHY_extstxctrl0 + 2, 0, 0x3c, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 2, 3, 0x3c, 0, 0x7},
	{SSLPNPHY_extstxctrl0 + 0, 0, 0x52, 0, 0xf},
	};

static void
WLBANDINITFN(wlc_sslpnphy_synch_stx)(phy_info_t *pi)
{
	uint i;

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0xff);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0xff);

	for (i = 0; i < ARRAYSIZE(sslpnphy_extstxdata); i++) {
		mod_phy_reg(pi,
			sslpnphy_extstxdata[i].phy_addr,
			(uint16)sslpnphy_extstxdata[i].mask << sslpnphy_extstxdata[i].phy_shift,
			(uint16)(read_radio_reg(pi, sslpnphy_extstxdata[i].rf_addr) >>
			sslpnphy_extstxdata[i].rf_shift) << sslpnphy_extstxdata[i].phy_shift);
	}

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0);

}

static void
WLBANDINITFN(sslpnphy_run_jtag_rcal)(phy_info_t *pi)
{
	int RCAL_done;
	int RCAL_timeout = 10;

	/* global RCAL override Enable */
	write_radio_reg(pi, RADIO_2063_COMMON_13, 0x10);

	/* put in override and power down RCAL */
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x10);

	/* Run RCAL */
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x14);

	/* Wait for RCAL Valid bit to be set */
	RCAL_done = (read_radio_reg(pi, RADIO_2063_COMMON_17) & 0x20) >> 5;

	while (RCAL_done == 0 && RCAL_timeout > 0) {
		OSL_DELAY(1);
		RCAL_done = (read_radio_reg(pi, RADIO_2063_COMMON_17) & 0x20) >> 5;
		RCAL_timeout--;
	}

	ASSERT(RCAL_done != 0);
	/* RCAL is done, now power down RCAL to save 100uA leakage during IEEE PS
	 * sleep, last RCAL value will remain valid even if RCAL is powered down
	*/
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x10);
}

static void
WLBANDINITFN(wlc_radio_2063_init_sslpnphy)(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	PHY_INFORM(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Load registers from the table */
	wlc_sslpnphy_init_radio_regs(pi, sslpnphy_radio_regs_2063, RADIO_DEFAULT_CORE);

	/* Set some PLL registers overridden by DC/CLB */
	write_radio_reg(pi, RADIO_2063_LOGEN_SP_5, 0x0);

	or_radio_reg(pi, RADIO_2063_COMMON_08, (0x07 << 3));
	write_radio_reg(pi, RADIO_2063_BANDGAP_CTRL_1, 0x56);

	if (ISSSLPNPHY(pi)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);
		} else {
			if (phybw40 == 0) {
				/* Set rx lpf bw to 9MHz */
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);
			} else if (phybw40 == 1) {
				/* Set rx lpf bw to 19MHz for 40Mhz operation */
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 3 << 1, 1 << 1);
				or_radio_reg(pi, RADIO_2063_COMMON_02, (0x1 << 1));
				mod_radio_reg(pi, RADIO_2063_RXBB_SP_4, 7 << 4, 0x30);
				and_radio_reg(pi, RADIO_2063_COMMON_02, (0x0 << 1));
			}
		}
	} else {
		/* Set rx lpf bw to 9MHz */
		mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);
	}

	/*
	* Apply rf reg settings to mitigate 2063 spectrum
	* asymmetry problems, including setting
	* PA and PAD in class A mode
	*/
	write_radio_reg(pi, RADIO_2063_PA_SP_7, 0);
	/* pga/pad */
	write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x20);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x40);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		/*  pa cascode voltage */
		write_radio_reg(pi, RADIO_2063_COMMON_05, 0x82);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x50);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x80);
	}

	/*  PA, PAD class B settings */
	write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x15);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x09);
		write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x21);

		if ((ph->sslpnphy_fabid == 2) ||
			(ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x30);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x40);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x1);
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x00);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x00);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0xF0);
		}
	}
	else if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x09);
		write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x21);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_15, 0xc8);

		if (phybw40 == 1) {
			/*  PA, PAD class B settings */
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0xF0);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0xF0);
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x10);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x1);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x30);
		}
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		/*  PA, PAD class B settings */
		write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x15);

		write_radio_reg(pi, RADIO_2063_COMMON_06, 0x30);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0xff);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x00);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_12,
			read_radio_reg(pi, RADIO_2063_TXRF_SP_12) | 0x0f);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_15, 0xc8);

		/* 5356: External PA support */
		if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
			(pi->sh->boardflags & BFL_HGPA)) {
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x0);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x30);
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0xf0);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x30);
		} else {
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x00);
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x0b);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x2e);
		}
	}
}

static void
WLBANDINITFN(wlc_sslpnphy_radio_init)(phy_info_t *pi)
{
	uint32 macintmask;
	uint8 phybw40;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* Toggle radio reset */
	or_phy_reg(pi, SSLPNPHY_fourwireControl, SSLPNPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);
	and_phy_reg(pi, SSLPNPHY_fourwireControl, ~SSLPNPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);

	/* Initialize 2063 radio */
	wlc_radio_2063_init_sslpnphy(pi);

	/* Synchronize phy overrides for RF registers that are mapped through the CLB */
	wlc_sslpnphy_synch_stx(pi);

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		or_radio_reg(pi, RADIO_2063_COMMON_04, 0x40);
		or_radio_reg(pi, RADIO_2063_TXRF_SP_3, 0x08);
	} else
		and_radio_reg(pi, RADIO_2063_COMMON_04, ~(uint8)0x40);

	/* Shared RX clb signals */
	write_phy_reg(pi, SSLPNPHY_extslnactrl0, 0x5f80);
	write_phy_reg(pi, SSLPNPHY_extslnactrl1, 0x0);

	/* Set Tx Filter Bandwidth */
	if (ph->sslpnphy_OLYMPIC) {
		mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
			SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
			1 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg1,
			SSLPNPHY_lpfbwlutreg1_lpfbwlut5_MASK,
			2 << SSLPNPHY_lpfbwlutreg1_lpfbwlut5_SHIFT);
	}

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if (phybw40) {
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
				SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
				2 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
		} else {
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
				SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
				3 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
		}

		mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg1,
			SSLPNPHY_lpfbwlutreg1_lpfbwlut5_MASK,
			2 << SSLPNPHY_lpfbwlutreg1_lpfbwlut5_SHIFT);
	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		if (phybw40 == 1) {
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
				SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
				3 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg1,
				SSLPNPHY_lpfbwlutreg1_lpfbwlut5_MASK,
				4 << SSLPNPHY_lpfbwlutreg1_lpfbwlut5_SHIFT);
		}
		else {
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
				SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
				0 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg1,
				SSLPNPHY_lpfbwlutreg1_lpfbwlut5_MASK,
				2 << SSLPNPHY_lpfbwlutreg1_lpfbwlut5_SHIFT);
		}
	}

	write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xee);


	/* Run RCal */
	macintmask = wlapi_intrsoff(pi->sh->physhim);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		sslpnphy_run_jtag_rcal(pi);
	} else {
		si_pmu_rcal(pi->sh->sih, pi->sh->osh);
	}
	wlapi_intrsrestore(pi->sh->physhim, macintmask);
}

static void
wlc_sslpnphy_rc_cal(phy_info_t *pi)
{
	uint8 rxbb_sp8, txbb_sp_3;
	uint8 save_pll_jtag_pll_xtal;
	uint16 epa_ovr, epa_ovr_val;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* RF_PLL_jtag_pll_xtal_1_2 */
	save_pll_jtag_pll_xtal = (uint8) read_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x4);

	/* Save old cap value incase RCCal fails */
	rxbb_sp8 = (uint8)read_radio_reg(pi, RADIO_2063_RXBB_SP_8);

	/* Save RF overide values */
	epa_ovr = read_phy_reg(pi, SSLPNPHY_RFOverride0);
	epa_ovr_val = read_phy_reg(pi, SSLPNPHY_RFOverrideVal0);

	/* Switch off ext PA */
	if ((CHSPEC_IS5G(pi->radio_chanspec)) &&
		(pi->sh->boardflags & BFL_HGPA)) {
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK,
			0 << SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_SHIFT);
	}

	/* Clear the RCCal Reg Override */
	write_radio_reg(pi, RADIO_2063_RXBB_SP_8, 0x0);

	/* Power down RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	/* Power up PLL_cal_out_pd_0 (bit 4) */
	and_radio_reg(pi, RADIO_2063_PLL_SP_1, 0xf7);

	/* Power Up RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7c);

	/* setup to run RX RC Cal and setup R1/Q1/P1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_2, 0x15);

	/* set X1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_3, 0x70);

	/* set Trc1 */
	if ((pi->xtalfreq == 38400000) || (pi->xtalfreq == 37400000)) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0xa0);
	} else if (pi->xtalfreq == 30000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x52);   /* only for 30MHz in 4319 */
	} else if (pi->xtalfreq == 25000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x18);   /* 25MHz in 5356 */
	} else if (pi->xtalfreq == 26000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x32);   /* For 26MHz Xtal */
	}

	/* set Trc2 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);

	/* Start rx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for rx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Rx RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_RXBB_SP_8, rxbb_sp8);
	} else
		PHY_INFORM(("wl%d: %s:  Rx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			pi->sh->unit, __FUNCTION__,
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_8),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_7),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_10),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_9),
			read_radio_reg(pi, RADIO_2063_COMMON_11) & 0x1f));

	/* Save old cap value incase RCCal fails */
	txbb_sp_3 = (uint8)read_radio_reg(pi, RADIO_2063_TXBB_SP_3);

	/* Clear the RCCal Reg Override */
	write_radio_reg(pi, RADIO_2063_TXBB_SP_3, 0x0);

	/* Power down RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	/* Power Up RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7c);

	/* setup to run TX RC Cal and setup R1/Q1/P1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_2, 0x55);

	/* set X1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_3, 0x76);

	if (pi->xtalfreq == 26000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x30);
	} else if ((pi->xtalfreq == 38400000) || (pi->xtalfreq == 37400000)) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x96);
	} else if (pi->xtalfreq == 30000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x3d);  /* for 30Mhz 4319 */
	} else if (pi->xtalfreq == 25000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x30);
		/* set Trc2 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);
	} else {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x30);
	}

	/* set Trc2 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);

	/* Start tx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for tx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Tx RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, txbb_sp_3);
	} else
		PHY_INFORM(("wl%d: %s:  Tx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			pi->sh->unit, __FUNCTION__,
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_8),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_7),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_10),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_9),
			read_radio_reg(pi, RADIO_2063_COMMON_12) & 0x1f));

	/* Power down RCCAL after it is done */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, save_pll_jtag_pll_xtal);

	/* Restore back amode tx pu */
	write_phy_reg(pi, SSLPNPHY_RFOverride0, epa_ovr);
	write_phy_reg(pi, SSLPNPHY_RFOverrideVal0, epa_ovr_val);
}

static void
wlc_sslpnphy_toggle_afe_pwdn(phy_info_t *pi)
{
	uint16 save_AfeCtrlOvrVal, save_AfeCtrlOvr;

	save_AfeCtrlOvrVal = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal);
	save_AfeCtrlOvr = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvr);

	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal | 0x1);
	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvr, save_AfeCtrlOvr | 0x1);

	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal & 0xfffe);
	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvr, save_AfeCtrlOvr & 0xfffe);

	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal);
	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvr, save_AfeCtrlOvr);
}

static void
wlc_sslpnphy_set_chanspec_default_tweaks(phy_info_t *pi)
{
	if (!NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				spur_tbl_rev2, 192, 8, 0);
		} else {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				spur_tbl_rev0, 64, 8, 0);
		}

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		si_pmu_chipcontrol(pi->sh->sih, 0, 0xfff,
		                   ((0x20 << 0) | (0x3f << 6)));
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
			si_pmu_regcontrol(pi->sh->sih, 3,
				((1 << 26) | (1 << 21)), ((1 << 26) | (1 << 21)));
			si_pmu_regcontrol(pi->sh->sih, 5, (0x1ff << 9), (0x1ff << 9));
		}
	}

	mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
		SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
		255 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_lnsrOfParam1,
			SSLPNPHY_lnsrOfParam1_ofMaxPThrUpdtThresh_MASK,
			11 << SSLPNPHY_lnsrOfParam1_ofMaxPThrUpdtThresh_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_lnsrOfParam2,
			SSLPNPHY_lnsrOfParam2_oFiltSyncCtrShft_MASK,
			1 << SSLPNPHY_lnsrOfParam2_oFiltSyncCtrShft_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
			SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
			25 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_ofdmSyncThresh0,
			SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_MASK,
			100 << SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_HiGainDB,
			SSLPNPHY_HiGainDB_HiGainDB_MASK,
			70 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
	}

	write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 360);

	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);
	}
}

#ifdef BAND5G
static void wlc_sslpnphy_radio_2063_channel_tweaks_A_band(phy_info_t *pi, uint freq);
#endif

static void wlc_sslpnphy_load_filt_coeff(phy_info_t *pi,
	uint16 reg_address, uint16 *coeff_val, uint count)
{
	uint i;
	for (i = 0; i < count; i++) {
		write_phy_reg(pi, reg_address + i, coeff_val[i]);
	}
}

static void wlc_sslpnphy_ofdm_filt_load(phy_info_t *pi)
{

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
			sslpnphy_cx_ofdm[ph->sslpnphy_ofdm_filt_sel], 10);
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
			sslpnphy_real_ofdm[ph->sslpnphy_ofdm_filt_sel], 5);
	}
}

static void wlc_sslpnphy_cck_filt_load(phy_info_t *pi, uint8 filtsel)
{
	if (SSLPNREV_GT(pi->pubpi.phy_rev, 0)) {
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
			sslpnphy_cx_cck[filtsel], 10);
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0,
				sslpnphy_real_cck[filtsel], 5);
		} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_cck_tap0,
				sslpnphy_real_cck[filtsel], 5);
		}
	}

}

void
wlc_phy_chanspec_set_sslpnphy(phy_info_t *pi, chanspec_t chanspec)
{
	uint16 m_cur_channel = 0;
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	uint32 centreTs20, centreFactor;
	uint16 bw = CHSPEC_BW(chanspec);
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_sslpnphy_percal_flags_off(pi);

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);

	/* Set the phy bandwidth as dictated by the chanspec */
	if (bw != pi->bw) {
		wlapi_bmac_bw_set(pi->sh->physhim, bw);
		/* chk for 5356 */
		/*
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
			wlc_sslpnphy_tx_pwr_ctrl_init(pi);
		*/
	}

	wlc_sslpnphy_set_chanspec_default_tweaks(pi);

	/* Tune radio for the channel */
	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_sslpnphy_radio_2063_channel_tune(pi, channel);

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				/* Gurus FCC changes */
				if ((channel == 1) || (channel == 11)) {
					mod_phy_reg(pi, SSLPNPHY_extstxctrl0,
						0xfff << 4, 0x040 << 4);
					mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xf, 0);
				} else {
					mod_phy_reg(pi, SSLPNPHY_extstxctrl0,
						0xfff << 4, 0x035 << 4);
					mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xf, 0);
				}
			}
		}

		if (channel == 14) {
			mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xfff, 0x15 << 4);
			mod_phy_reg(pi, SSLPNPHY_extstxctrl0, 0xfff << 4, 0x240 << 4);
			mod_phy_reg(pi, SSLPNPHY_extstxctrl4, 0xff << 7, 0x80 << 7);
			mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xf, 0x0);
			mod_phy_reg(pi, SSLPNPHY_lpphyCtrl,
				SSLPNPHY_lpphyCtrl_txfiltSelect_MASK,
				2 << SSLPNPHY_lpphyCtrl_txfiltSelect_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
				SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
				2 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
			wlc_sslpnphy_cck_filt_load(pi, 4);

		} else {
			write_phy_reg(pi, SSLPNPHY_extstxctrl0, ph->sslpnphy_extstxctrl0);
			write_phy_reg(pi, SSLPNPHY_extstxctrl1, ph->sslpnphy_extstxctrl1);
			write_phy_reg(pi, SSLPNPHY_extstxctrl4, ph->sslpnphy_extstxctrl4);
			mod_phy_reg(pi, SSLPNPHY_lpphyCtrl,
				SSLPNPHY_lpphyCtrl_txfiltSelect_MASK,
				1 << SSLPNPHY_lpphyCtrl_txfiltSelect_SHIFT);
			wlc_sslpnphy_cck_filt_load(pi, ph->sslpnphy_cck_filt_sel);
			if (SSLPNREV_LE(pi->pubpi.phy_rev, 1)) {
				if (wlc_sslpnphy_fcc_chan_check(pi, channel)) {
					mod_phy_reg(pi, SSLPNPHY_extstxctrl0,
						0xfff << 4, 0xf00 << 4);
					mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xfff, 0x0c3);
					mod_phy_reg(pi, SSLPNPHY_extstxctrl3, 0xff, 0x90);
				} else {
					write_phy_reg(pi, SSLPNPHY_extstxctrl0,
						ph->sslpnphy_extstxctrl0);
					write_phy_reg(pi, SSLPNPHY_extstxctrl1,
						ph->sslpnphy_extstxctrl1);
					write_phy_reg(pi, SSLPNPHY_extstxctrl3,
						ph->sslpnphy_extstxctrl3);
				}
			}
		}

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (freq <= 5320) /* lower A band channel */
			wlc_sslpnphy_load_tx_gain_table(pi, dot11lpphy_5GHz_gaintable);
		else if (freq <= 5640) /* Mid A band channel */
			wlc_sslpnphy_load_tx_gain_table(pi, dot11lpphy_5GHz_gaintable_MidBand);
		else /* Upper A band channel */
			wlc_sslpnphy_load_tx_gain_table(pi, dot11lpphy_5GHz_gaintable_HighBand);
		wlc_sslpnphy_radio_2063_channel_tweaks_A_band(pi, freq);
	}
#endif /* BAND5G */
		OSL_DELAY(1000);
	}
	/* toggle the afe whenever we move to a new channel */
	wlc_sslpnphy_toggle_afe_pwdn(pi);

	m_cur_channel = channel;

	if (CHSPEC_IS5G(chanspec))
		m_cur_channel |= D11_CURCHANNEL_5G;

	if (CHSPEC_IS40(chanspec)) {
		m_cur_channel |= D11_CURCHANNEL_40;
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			/* tuning for 40MHz FCC band edge */
			if (channel == 3 || channel == 9) {
				mod_phy_reg(pi, SSLPNPHY_extstxctrl0,
					0xfff << 4, (0xe << 12) | (0x0 << 4));
				mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xfff,
					(0x18 << 4) | (0x2 << 0));
			} else {
				mod_phy_reg(pi, SSLPNPHY_extstxctrl0,
					0xfff << 4, (0x0 << 12) | (0x2e << 4));
				mod_phy_reg(pi, SSLPNPHY_extstxctrl1,
					0xfff, (0x0b << 4) | (0x0 << 0));
			}
		}

		if (CHSPEC_SB_UPPER(chanspec)) {
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		} else {
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		}
	}

	centreTs20 = wlc_sslpnphy_qdiv_roundup(freq * 2, 5, 0);
	centreFactor = wlc_sslpnphy_qdiv_roundup(2621440, freq, 0);
	write_phy_reg(pi, SSLPNPHY_ptcentreTs20, (uint16)centreTs20);
	write_phy_reg(pi, SSLPNPHY_ptcentreFactor, (uint16)centreFactor);

	wlapi_bmac_write_shm(pi->sh->physhim, M_CURCHANNEL, m_cur_channel);
	write_phy_channel_reg(pi, channel);

	/* Indicate correct antdiv register offset to ucode */
	if (CHSPEC_IS40(chanspec))
		wlapi_bmac_write_shm(pi->sh->physhim,
			(2 * (sslpnphy_shm_ptr + M_SSLPNPHY_ANTDIV_REG)),
			SSLPNPHY_Rev2_crsgainCtrl_40);
	else
		wlapi_bmac_write_shm(pi->sh->physhim,
			(2 * (sslpnphy_shm_ptr + M_SSLPNPHY_ANTDIV_REG)),
			SSLPNPHY_Rev2_crsgainCtrl);

	/* Trigger calibration */
	pi->phy_forcecal = TRUE;

	ph->sslpnphy_papd_cal_done = 0;

	wlc_phy_chanspec_set_fixup_sslpnphy(pi, chanspec);

	wlc_sslpnphy_tx_pwr_ctrl_init(pi);

	if (!SCAN_RM_IN_PROGRESS(pi)) {
		if (ph->sslpnphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec)) {
			wlc_sslpnphy_periodic_cal(pi);
		} else {
			wlc_sslpnphy_restore_calibration_results(pi);
			ph->sslpnphy_restore_papd_cal_results = 1;
		}
	}
}

void
wlc_phy_chanspec_set_fixup_sslpnphy(phy_info_t *pi, chanspec_t chanspec)
{
	/* Some of CRS/AGC values are dependent on Channel and VT. So initialise to known values */
	wlc_sslpnphy_set_chanspec_tweaks(pi, pi->radio_chanspec);
	/* Common GainTable For Rx/ACI Tweaks Adding Here */
	wlc_sslpnphy_CmRxAciGainTbl_Tweaks(pi);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		phytbl_info_t tab;
		if (CHSPEC_IS40(chanspec)) {
			tab.tbl_ptr = fltr_ctrl_tbl_40Mhz;
			tab.tbl_len = 10;
			tab.tbl_id = 0xb;
			tab.tbl_offset = 0;
			tab.tbl_width = 32;
			wlc_sslpnphy_write_table(pi, &tab);
		}
		mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
			SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
			0xfa << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
	}
}

static void
wlc_sslpnphy_RxNvParam_Adj(phy_info_t *pi)
{
	int8 path_loss = 0, temp1;
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	uint8 phybw40;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		path_loss = (int8)ph->sslpnphy_rx_power_offset;
	} else {
#ifdef BAND5G
		path_loss = (int8)ph->sslpnphy_rx_power_offset_5g;
#endif /* BAND5G */
	}
	temp1 = (int8)(read_phy_reg(pi, SSLPNPHY_InputPowerDB)
		& SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40)
			temp1 = (int8)(read_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40)
				& SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK);
	}
	if ((ph->sslpnphy_rxpo2gchnflg) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		if (ph->sslpnphy_rxpo2gchnflg & (0x1 << (channel - 1)))
			temp1 -= path_loss;
	} else {
		temp1 -= path_loss;
	}
	mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
		SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
		(temp1 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40)
			mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				(temp1 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT));
	}
}


STATIC uint16 ELNA_CCK_ACI_GAINTBL_TWEAKS [][2] = {
	{50,  0x24C},
	{51,  0x233},
	{52,  0x24D},
	{53,  0x24E},
	{54,  0x240},
	{55,  0x24F},
	{56,  0x2C0},
	{57,  0x2D0},
	{58,  0x2D8},
	{59,  0x2AF},
	{60,  0x2B9},
	{61,  0x32F},
	{62,  0x33A},
	{63,  0x33B},
	{64,  0x33C},
	{65,  0x33D},
	{66,  0x33E},
	{67,  0x3BF},
	{68,  0x3BE},
	{69,  0x3C2},
	{70,  0x3C1},
	{71,  0x3C4},
	{72,  0x3B0},
	{73,  0x3B1}
};
STATIC uint16 ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS [][2] = {
	{23,  0x381},
	{24,  0x385},
	{25,  0x3D4},
	{26,  0x3D5},
	{27,  0x3D7},
	{28,  0x3AB},
	{29,  0x3D6},
	{30,  0x3D8},
	{31,  0x38E},
	{32,  0x38C},
	{33,  0x38A}
};
STATIC uint16 ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS [][2] = {
	{61,  0x28F },
	{62,  0x32C },
	{63,  0x33E },
	{64,  0x33F },
	{65,  0x343 },
	{66,  0x345 },
	{67,  0x34A },
	{68,  0x3C9 },
	{69,  0x387 },
	{70,  0x3CB },
	{71,  0x3CE },
	{72,  0x3CF },
	{73,  0x3D0 }
};
uint8 ELNA_CCK_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_CCK_ACI_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS);
static void
wlc_sslpnphy_rx_gain_table_tweaks(phy_info_t *pi, uint8 idx, uint16 ptr[][2], uint8 array_size)
{
	uint8 i = 0, tbl_entry;
	uint16 wlprio_11bit_code;
	uint32 tableBuffer[2];
	for (i = 0; i < array_size; i++) {
		if (ptr[i][0] == idx) {
			wlprio_11bit_code = ptr[i][1];
			tbl_entry = idx << 1;
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, tbl_entry);
			tableBuffer[0] = (tableBuffer[0] & 0x0FFFFFFF)
				| ((wlprio_11bit_code & 0x00F) << 28);
			tableBuffer[1] = (tableBuffer[1] & 0xFFFFFF80)
				| ((wlprio_11bit_code & 0x7F0) >> 4);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, tbl_entry);
		}
	}
}

static void
wlc_sslpnphy_CmRxAciGainTbl_Tweaks(void *arg)
{
	phy_info_t *pi = (phy_info_t *)arg;
	uint32 tblBuffer[2];
	uint32 lnaBuffer[2] = {0, 9};
	uint8 phybw40, i;
	bool extlna;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	extlna = ((pi->sh->boardflags & BFL_EXTLNA_5GHz) &&
		CHSPEC_IS5G(pi->radio_chanspec)) ||
		((pi->sh->boardflags & BFL_EXTLNA) &&
		CHSPEC_IS2G(pi->radio_chanspec));

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
			SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_MASK,
			18 << SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_MinPwrLevel,
			SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_MASK,
			164 << SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_SHIFT);
		write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6333);
		mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
			SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
			8 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);

		if ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17_SSID) ||
			BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID ||
			BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) {
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs2,
				SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_MASK,
				8 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				1 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
			/* mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
				SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK,
				1 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT);
			*/
			mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
				SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
				5 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_gaindirectMismatch,
				SSLPNPHY_gaindirectMismatch_MedHigainDirectMismatch_MASK,
				12 << SSLPNPHY_gaindirectMismatch_MedHigainDirectMismatch_SHIFT);
			write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6363);
		}
	}
#endif /* BAND5G */
	if ((SSLPNREV_IS(pi->pubpi.phy_rev, 1)) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
			SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_MASK,
			4 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
			SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
			11 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_gainMismatchMedGainEx,
			SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_MASK,
			6 << SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_SHIFT);
		write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6666);
		write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 360);
		if ((ph->sslpnphy_fabid == 2) || (ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x33);
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 340);
		}

		if (!(pi->sh->boardflags & BFL_EXTLNA)) {
			if ((ph->sslpnphy_fabid != 2) && (ph->sslpnphy_fabid_otp != TSMC_FAB12)) {
				write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF6);
				mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
					SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
					8 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
			}
			if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID) {
				mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
					SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
					12 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
			}
		} else if (pi->sh->boardflags & BFL_EXTLNA) {
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs2,
				SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_MASK |
				SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtDsss_MASK,
				6 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_SHIFT |
				6 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtDsss_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_LowGainDB,
				SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
				32 << SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				18 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
			write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);

			/* Rx ELNA Boards ACI Improvements W/o uCode Interventions */
			if (BOARDTYPE(pi->sh->boardtype) != BCM94329OLYMPICN18_SSID) {
				write_phy_reg(pi, SSLPNPHY_ClipThresh, 63);
				mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
					SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_MASK,
					2 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_SHIFT);
				write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 20);
				mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
					SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
					12 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
					SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
					4 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);
			}
			if (BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) {
				/* Gain idx tweaking for 2g band of dual band board */
				tblBuffer[0] = 0xc0000001;
				tblBuffer[1] = 0x0000006c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 14);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 88);
				tblBuffer[0] = 0x70000002;
				tblBuffer[1] = 0x0000006a;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 16);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 90);
				tblBuffer[0] = 0x20000002;
				tblBuffer[1] = 0x0000006b;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 18);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 92);
				tblBuffer[0] = 0xc020c287;
				tblBuffer[1] = 0x0000002c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 30);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 104);
				tblBuffer[0] = 0x30410308;
				tblBuffer[1] = 0x0000002b;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 32);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 106);
				mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs2,
					SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_MASK |
					SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtDsss_MASK,
					9 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtOfdm_SHIFT |
					9 << SSLPNPHY_radioTRCtrlCrs2_trTransAddrLmtDsss_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
					SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
					22 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_ofdmSyncThresh0,
					SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_MASK,
					120 << SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
					SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_MASK,
					18 << SSLPNPHY_ClipCtrThresh_ClipCtrThreshHiGain_SHIFT);
				tblBuffer[0] = 0x51e64d96;
				tblBuffer[1] = 0x0000003c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 54);
				tblBuffer[0] = 0x6204ca9e;
				tblBuffer[1] = 0x0000003c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 56);
				for (i = 50; i <= 73; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_CCK_ACI_GAINTBL_TWEAKS,
						ELNA_CCK_ACI_GAINTBL_TWEAKS_sz);
				}
			} else if ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID)) {
				lnaBuffer[0] = 0;
				lnaBuffer[1] = 12;

				for (i = 23; i <= 33; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS,
						ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS_sz);
				}
				for (i = 61; i <= 73; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS,
						ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS_sz);
				}
				mod_phy_reg(pi, SSLPNPHY_HiGainDB,
					SSLPNPHY_HiGainDB_HiGainDB_MASK,
					73 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
				write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 360);
				mod_phy_reg(pi, SSLPNPHY_gainMismatchMedGainEx,
					SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_MASK,
					3 << SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
					SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK,
					0 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
					SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
					0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
					SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_MASK,
					15 << SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_gainMismatch,
					SSLPNPHY_gainMismatch_GainmisMatchPktRx_MASK,
					9 << SSLPNPHY_gainMismatch_GainmisMatchPktRx_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
					SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
					8 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
					SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
					15 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
					SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
					9 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);
				if ((ph->sslpnphy_fabid == 2) ||
					(ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
				tblBuffer[0] = 0x651123C7;
				tblBuffer[1] = 0x00000008;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 30);
				}
			}
		}
	}
	else if ((SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) &&
		(CHSPEC_IS2G(pi->radio_chanspec))) {
		if (!phybw40) {
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 360);
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF0); /* Dflt Value */
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
				SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_MASK,
				0 << SSLPNPHY_radioTRCtrlCrs1_gainReqTrAttOnEnByCrs_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrl,
				SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_MASK,
				0 << SSLPNPHY_radioTRCtrl_gainrequestTRAttnOnEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
				SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
				11 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_DSSSConfirmCnt,
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_MASK |
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_MASK |
				SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGainCnfrm_MASK,
				((4 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGain_SHIFT) |
				(4 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntLoGain_SHIFT) |
				(2 << SSLPNPHY_DSSSConfirmCnt_DSSSConfirmCntHiGainCnfrm_SHIFT)));
			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				20 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
				SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
				9 << SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_lnsrOfParam4,
				SSLPNPHY_lnsrOfParam4_ofMaxPThrUpdtThresh_MASK |
				SSLPNPHY_lnsrOfParam4_oFiltSyncCtrShft_MASK,
				((0 << SSLPNPHY_lnsrOfParam4_ofMaxPThrUpdtThresh_SHIFT) |
				(2 << SSLPNPHY_lnsrOfParam4_oFiltSyncCtrShft_SHIFT)));

			mod_phy_reg(pi, SSLPNPHY_ofdmSyncThresh0,
				SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_MASK,
				120 << SSLPNPHY_ofdmSyncThresh0_ofdmSyncThresh0_SHIFT);
		}
		else if (phybw40) {
		        write_phy_reg(pi, SSLPNPHY_Rev2_nfSubtractVal_40, 320);
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF6);
		}
		if (BOARDTYPE(pi->sh->boardtype) != BCM94319WLUSBN4L_SSID) {
			tblBuffer[0] = 0x0110;
			tblBuffer[1] = 0x0101;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				tblBuffer, 2, 32, 0);

			tblBuffer[0] = 0xb0000000;
			tblBuffer[1] = 0x00000040;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 0);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 74);
			tblBuffer[0] = 0x00000000;
			tblBuffer[1] = 0x00000048;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 2);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 76);
			tblBuffer[0] = 0xb0000000;
			tblBuffer[1] = 0x00000048;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 4);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 78);
			tblBuffer[0] = 0xe0000000;
			tblBuffer[1] = 0x0000004d;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 6);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 80);
			tblBuffer[0] = 0xc0000000;
			tblBuffer[1] = 0x0000004c;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 8);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 82);
			tblBuffer[0] = 0x00000000;
			tblBuffer[1] = 0x00000058;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 10);
			if (phybw40) {
				tblBuffer[0] = 0xb0000000;
				tblBuffer[1] = 0x00000058;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 12);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 86);
			}
			mod_phy_reg(pi, SSLPNPHY_ClipCtrThresh,
				SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_MASK,
				12 << SSLPNPHY_ClipCtrThresh_clipCtrThreshLoGain_SHIFT);
		}
	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		if (extlna) {
			mod_phy_reg(pi, SSLPNPHY_radioCtrl,
				SSLPNPHY_radioCtrl_extlnaen_MASK,
				1 << SSLPNPHY_radioCtrl_extlnaen_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_extlnagainvalue0,
				SSLPNPHY_extlnagainvalue0_extlnagain0_MASK,
				lnaBuffer[0] << SSLPNPHY_extlnagainvalue0_extlnagain0_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_extlnagainvalue0,
				SSLPNPHY_extlnagainvalue0_extlnagain1_MASK,
				lnaBuffer[1] << SSLPNPHY_extlnagainvalue0_extlnagain1_SHIFT);
			wlc_sslpnphy_common_write_table(pi, 17, lnaBuffer, 2, 32, 66);
		}
	}
	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);
}

static void
wlc_sslpnphy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl = (read_phy_reg(pi, SSLPNPHY_AfeDACCtrl) >> SSLPNPHY_AfeDACCtrl_dac_ctrl_SHIFT);
	dac_ctrl = dac_ctrl & 0xc7f;
	dac_ctrl = dac_ctrl | (dac_gain << 7);
	mod_phy_reg(pi, SSLPNPHY_AfeDACCtrl,
		SSLPNPHY_AfeDACCtrl_dac_ctrl_MASK,
		dac_ctrl << SSLPNPHY_AfeDACCtrl_dac_ctrl_SHIFT);
}

static void
wlc_sslpnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_txgainctrl_ovr_MASK,
		bit << SSLPNPHY_rfoverride2_txgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_stxtxgainctrl_ovr_MASK,
		bit << SSLPNPHY_rfoverride2_stxtxgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_dacattctrl_ovr_MASK,
		bit << SSLPNPHY_AfeCtrlOvr_dacattctrl_ovr_SHIFT);
}

static uint16
wlc_sslpnphy_get_pa_gain(phy_info_t *pi)
{
	uint16 pa_gain;

	pa_gain = (read_phy_reg(pi, SSLPNPHY_txgainctrlovrval1) &
		SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK) >>
		SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT;

	return pa_gain;
}

static void
wlc_sslpnphy_set_tx_gain(phy_info_t *pi,  sslpnphy_txgains_t *target_gains)
{
	uint16 pa_gain = wlc_sslpnphy_get_pa_gain(pi);

	mod_phy_reg(pi, SSLPNPHY_txgainctrlovrval0,
		SSLPNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_MASK,
		((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		SSLPNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_txgainctrlovrval1,
		SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_MASK,
		((target_gains->pad_gain) | (pa_gain << 8)) <<
		SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_stxtxgainctrlovrval0,
		SSLPNPHY_stxtxgainctrlovrval0_stxtxgainctrl_ovr_val0_MASK,
		((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		SSLPNPHY_stxtxgainctrlovrval0_stxtxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_stxtxgainctrlovrval1,
		SSLPNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_MASK,
		((target_gains->pad_gain) | (pa_gain << 8)) <<
		SSLPNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT);

	wlc_sslpnphy_set_dac_gain(pi, target_gains->dac_gain);

	/* Enable gain overrides */
	wlc_sslpnphy_enable_tx_gain_override(pi);
}

static void
wlc_sslpnphy_set_bbmult(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16)m0 << 8;
	phytbl_info_t tab;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_sslpnphy_write_table(pi, &tab);
}

static void
wlc_sslpnphy_clear_tx_power_offsets(phy_info_t *pi)
{
	uint32 data_buf[64];
	phytbl_info_t tab;

	/* Clear out buffer */
	bzero(data_buf, sizeof(data_buf));

	/* Preset txPwrCtrltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = data_buf; /* ptr to buf */

	/* Per rate power offset */
	tab.tbl_len = 24; /* # values   */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_sslpnphy_write_table(pi, &tab);

	/* Per index power offset */
	tab.tbl_len = 64; /* # values   */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_sslpnphy_write_table(pi, &tab);
}

typedef enum {
	SSLPNPHY_TSSI_PRE_PA,
	SSLPNPHY_TSSI_POST_PA,
	SSLPNPHY_TSSI_EXT
} sslpnphy_tssi_mode_t;

static void
wlc_sslpnphy_set_tssi_mux(phy_info_t *pi, sslpnphy_tssi_mode_t pos)
{
	if (SSLPNPHY_TSSI_EXT == pos) {
		/* Power down internal TSSI */
		or_phy_reg(pi, SSLPNPHY_extstxctrl1, 0x00 << 12);

		/* Power up External TSSI */
		write_phy_reg(pi, RADIO_2063_EXTTSSI_CTRL_2, 0x21);

		/* Adjust TSSI bias */
		write_radio_reg(pi, RADIO_2063_EXTTSSI_CTRL_1, 0x51);

		/* Set TSSI/RSSI mux */
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
			SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_MASK,
			0x01 << SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
			SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
			0x03 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);
	} else {
		/* Power up internal TSSI */
		or_phy_reg(pi, SSLPNPHY_extstxctrl1, 0x01 << 12);

#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 0 << 2);
			write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x5c);
		} else
#endif
		{
			write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x51);
		}

		/* Set TSSI/RSSI mux */
		if (SSLPNPHY_TSSI_POST_PA == pos) {
			mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
				SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
				0x00 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);
		} else {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, 0x01, 1);
			mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
				SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
				0x04 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);
		}
	}
}

#define BTCX_FLUSH_WAIT_MAX_MS	  500
static bool wlc_sslpnphy_btcx_override_enable(phy_info_t *pi)
{
	bool val = TRUE;
	int delay;

	if ((CHSPEC_IS2G(pi->radio_chanspec)) && (pi->sh->machwcap & MCAP_BTCX))
	{
		/* Ucode better be suspended when we mess with BTCX regs directly */
		ASSERT(0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		/* Enable manual BTCX mode */
		OR_REG(pi->sh->osh, &pi->regs->btcx_ctrl, BTCX_CTRL_EN | BTCX_CTRL_SW);
		/* Set BT priority & antenna to allow A2DP to catchup */
		AND_REG(pi->sh->osh,
			&pi->regs->btcx_trans_ctrl, ~(BTCX_TRANS_TXCONF | BTCX_TRANS_ANTSEL));


		/* Set WLAN priority */
		OR_REG(pi->sh->osh, &pi->regs->btcx_trans_ctrl, BTCX_TRANS_TXCONF);
		/* Wait for BT activity to finish */
		delay = 0;
		while (R_REG(pi->sh->osh, &pi->regs->btcx_stat) & BTCX_STAT_RA) {
			if (delay++ > BTCX_FLUSH_WAIT_MAX_MS) {
				PHY_ERROR(("wl%d: %s: BT still active\n",
					pi->sh->unit, __FUNCTION__));
				val = FALSE;
				break;
			}
			OSL_DELAY(100);
		}

		/* Set WLAN antenna & priority */
		OR_REG(pi->sh->osh,
			&pi->regs->btcx_trans_ctrl, BTCX_TRANS_ANTSEL | BTCX_TRANS_TXCONF);
	}

	return val;
}

void
wlc_sslpnphy_tx_pwr_update_npt(phy_info_t *pi)
{
	uint16 tx_cnt, tx_total, npt;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	tx_total = wlc_sslpnphy_total_tx_frames(pi);
	tx_cnt = tx_total - ph->sslpnphy_tssi_tx_cnt;
	npt = wlc_sslpnphy_get_tx_pwr_npt(pi);

	if ((tx_cnt > (1 << npt)) || (ph->sslpnphy_dummy_tx_done)) {
		/* Reset frame counter */
		ph->sslpnphy_tssi_tx_cnt = tx_total;


		/* Update cached power index & NPT */
		ph->sslpnphy_tssi_idx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
		ph->sslpnphy_tssi_npt = npt;

		PHY_INFORM(("wl%d: %s: Index: %d, NPT: %d, TxCount: %d\n",
			pi->sh->unit, __FUNCTION__, ph->sslpnphy_tssi_idx, npt, tx_cnt));
	}
}

int32
wlc_sslpnphy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;

	a = 32768 + (a1 * tssi);
	b = (512 * b0) + (32 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	return p;
}

static void
wlc_sslpnphy_txpower_reset_npt(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (!ph->sslpnphy_papd_cal_done) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			ph->sslpnphy_tssi_idx = SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G_PAPD;
		else
			ph->sslpnphy_tssi_idx = SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G_PAPD;
	} else {
	if (CHSPEC_IS5G(pi->radio_chanspec))
		ph->sslpnphy_tssi_idx = SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G;
	else
		ph->sslpnphy_tssi_idx = SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G;
	}

	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->boardflags & BFL_HGPA)) {
		ph->sslpnphy_tssi_idx = 74;
	}

	ph->sslpnphy_tssi_npt = SSLPNPHY_TX_PWR_CTRL_START_NPT;
}

uint8 sslpnphy_mcs_to_legacy_map[8] = {0, 2, 3, 4, 5, 6, 7, 7 + 8};

int8
wlc_phy_get_tx_power_offset(wlc_phy_t *ppi, uint8 tbl_offset)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	return pi->tx_power_offset[sslpnphy_mcs_to_legacy_map[tbl_offset] + 4];
}

int8
wlc_phy_get_tx_power_offset_by_mcs(wlc_phy_t *ppi, uint8 mcs_offset)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	return pi->tx_power_offset[mcs_offset + 12];
}

void
wlc_sslpnphy_txpower_recalc_target(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint freq;
	uint32 rate_table[24];
	uint i, idx;
	bool reset_npt = FALSE;
	int8 mac_pwr;
	uint sidx;
	int8 mac_pwr_high_index = 0;
	uint8 pwr_boost = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	pwr_boost = ((ph->sslpnphy_OLYMPIC) ? 0 : 2);

	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));

	/* Adjust rate based power offset */
	for (i = 0, idx = 0, sidx = 0; i < 4; i++) {
		ph->sslpnphy_tx_power_offset[sidx] = rate_table[idx] =
			(uint32)((int32)(-pi->tx_power_offset[i]));
		idx = idx + 1;
		sidx = sidx + 1;
		PHY_TMP((" Rate %d, offset %d\n", i, rate_table[i]));
	}

	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);

	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);

	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);

	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i++]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i]) - pwr_boost);
	ph->sslpnphy_tx_power_offset[sidx++] = rate_table[idx++] =
		(uint32)((int32)(-pi->tx_power_offset[i+8]) - pwr_boost);

	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, rate_table,
		ARRAYSIZE(rate_table), 32, SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3))
		mac_pwr_high_index = -12;

	else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		if (CHSPEC_IS40(pi->radio_chanspec))
			mac_pwr_high_index = 3;
		else
			mac_pwr_high_index = -32;
	}
		/* Preset txPwrCtrltbl */
		tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_ptr = &mac_pwr; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_width = 8;	/* 32 bit wide	*/
		tab.tbl_offset = 128;
		mac_pwr = 0;

		for (idx = 0; idx < 64; idx++) {
			if (idx <= 31) {
				wlc_sslpnphy_write_table(pi,  &tab);
				tab.tbl_offset++;
			mac_pwr++;
			} else {
				tab.tbl_ptr = &mac_pwr_high_index; /* ptr to buf */
				wlc_sslpnphy_write_table(pi,  &tab);
				tab.tbl_offset++;
				mac_pwr_high_index++;
			}
		}

	/* Set new target power */
	if ((wlc_sslpnphy_get_target_tx_pwr(pi) != pi->tx_power_min) ||
		(pi->tx_power_min > ph->sslpnphy_tssi_pwr_limit)) {
		wlc_sslpnphy_set_target_tx_pwr(pi, pi->tx_power_min);
		/* Should reset power index cache */
		reset_npt = TRUE;
	}
}

void
wlc_sslpnphy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode)
{
	uint16 old_mode = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ASSERT(
		(SSLPNPHY_TX_PWR_CTRL_OFF == mode) ||
		(SSLPNPHY_TX_PWR_CTRL_SW == mode) ||
		(SSLPNPHY_TX_PWR_CTRL_HW == mode));

	/* Setting txfront end clock also along with hwpwr control */
	MOD_PHY_REG(pi, SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn,
		(SSLPNPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0);

	/* Feed back RF power level to PAPD block */
	MOD_PHY_REG(pi, SSLPNPHY, papd_control2, papd_analog_gain_ovr,
		(SSLPNPHY_TX_PWR_CTRL_HW == mode) ? 0 : 1);

	if (old_mode != mode) {
		if (SSLPNPHY_TX_PWR_CTRL_HW == old_mode) {
			/* Get latest power estimates before turning off power control */
			wlc_sslpnphy_tx_pwr_update_npt(pi);

			/* Clear out all power offsets */
			wlc_sslpnphy_clear_tx_power_offsets(pi);
		} else if (SSLPNPHY_TX_PWR_CTRL_HW == mode) {
			/* Recalculate target power to restore power offsets */
			wlc_sslpnphy_txpower_recalc_target(pi);

			/* Set starting index & NPT to best known values for that target */
			wlc_sslpnphy_set_start_tx_pwr_idx(pi, ph->sslpnphy_tssi_idx);
			wlc_sslpnphy_set_tx_pwr_npt(pi, ph->sslpnphy_tssi_npt);

			/* Reset frame counter for NPT calculations */
			ph->sslpnphy_tssi_tx_cnt = wlc_sslpnphy_total_tx_frames(pi);

			/* Disable any gain overrides */
			wlc_sslpnphy_disable_tx_gain_override(pi);
			ph->sslpnphy_tx_power_idx_override = -1;
		}

		/* Set requested tx power control mode */
		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlCmd,
			(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			mode);

		PHY_INFORM(("wl%d: %s: %s \n", pi->sh->unit, __FUNCTION__,
			mode ? ((SSLPNPHY_TX_PWR_CTRL_HW == mode) ? "Auto" : "Manual") : "Off"));
	}
}

static void
wlc_sslpnphy_idle_tssi_est(phy_info_t *pi)
{
	uint16 status, tssi_val;
	wl_pkteng_t pkteng;
	struct ether_addr sa;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	sa.octet[0] = 10;
	ph->sslpnphy_tssi_val = 0;

	PHY_TRACE(("Pkteng TX Start Called\n"));
	pkteng.flags = WL_PKTENG_PER_TX_START;
	if (ph->sslpnphy_recal)
		pkteng.delay = 2;              /* Inter packet delay */
	else
		pkteng.delay = 50;              /* Inter packet delay */
	pkteng.nframes = 50;            /* number of frames */
	pkteng.length = 0;              /* packet length */
	pkteng.seqno = FALSE;                   /* enable/disable sequence no. */
	wlc_sslpnphy_pktengtx((wlc_phy_t *)pi, &pkteng, 108, &sa, (1000*10));

	status = read_phy_reg(pi, SSLPNPHY_TxPwrCtrlStatus);
	if (status & SSLPNPHY_TxPwrCtrlStatus_estPwrValid_MASK) {
		tssi_val = (status & SSLPNPHY_TxPwrCtrlStatus_estPwr_MASK) >>
			SSLPNPHY_TxPwrCtrlStatus_estPwr_SHIFT;
		PHY_INFORM(("wl%d: %s: Measured idle TSSI: %d\n",
			pi->sh->unit, __FUNCTION__, (int8)tssi_val - 32));
		ph->sslpnphy_tssi_val = (uint8)tssi_val;
	} else {
		PHY_INFORM(("wl%d: %s: Failed to measure idle TSSI\n",
			pi->sh->unit, __FUNCTION__));
		}

}

void
WLBANDINITFN(wlc_sslpnphy_tx_pwr_ctrl_init)(phy_info_t *pi)
{
	sslpnphy_txgains_t tx_gains;
	sslpnphy_txgains_t ltx_gains;
	uint8 bbmult;
	int32 a1, b0, b1;
	int32 tssi, pwr, prev_pwr = 255;
	uint tssi_ladder_cnt = 0;
	uint freq;
	bool reset_npt = FALSE;
	bool suspend;
	uint32 ind;
	uint16 tssi_val = 0;
	uint8 iqcal_ctrl2;
	uint8 pos;
	uint8 phybw40 = CHSPEC_IS40(pi->radio_chanspec);
	uint16 tempsense;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* Saving tempsense value before starting idle tssi est */
	/* Reset Tempsese to 0 . Idle tssi est expects it to be 0 */
	tempsense = read_phy_reg(pi, SSLPNPHY_TempSenseCorrection);
	write_phy_reg(pi, SSLPNPHY_TempSenseCorrection, 0);
	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	a1 = b0 = b1 = 0;

	if (NORADIO_ENAB(pi->pubpi)) {
		wlc_sslpnphy_set_bbmult(pi, 0x30);
		return;
	}
	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));

	if (!pi->hwpwrctrl_capable) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tx_gains.gm_gain = 4;
			tx_gains.pga_gain = 12;
			tx_gains.pad_gain = 12;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		} else {
			tx_gains.gm_gain = 7;
			tx_gains.pga_gain = 15;
			tx_gains.pad_gain = 14;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		}
		wlc_sslpnphy_set_tx_gain(pi, &tx_gains);
		wlc_sslpnphy_set_bbmult(pi, bbmult);
	} else {
		/* Adjust power LUT's */
		freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
		if (ph->sslpnphy_target_tx_freq != (uint16)freq) {
			switch (wlc_phy_chanspec_freq2bandrange_lpssn(freq)) {
			case WL_CHAN_FREQ_RANGE_2G:
				/* 2.4 GHz */
				b0 = pi->txpa_2g[0];
				b1 = pi->txpa_2g[1];
				a1 = pi->txpa_2g[2];
				write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1,
					((uint16)ph->sslpnphy_rssi_vf_lowtemp << 0) |
					((uint16)ph->sslpnphy_rssi_vc_lowtemp << 4) |
					(0x00 << 8) |
					((uint16)ph->sslpnphy_rssi_gs_lowtemp << 10) |
					(0x01 << 13));
				break;
#ifdef BAND5G
			case WL_CHAN_FREQ_RANGE_5GL:
				/* 5 GHz low */
				b0 = pi->txpa_5g_low[0];
				b1 = pi->txpa_5g_low[1];
				a1 = pi->txpa_5g_low[2];
				write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1,
					((uint16)ph->sslpnphy_rssi_5g_vf << 0) |
					((uint16)ph->sslpnphy_rssi_5g_vc << 4) |
					(0x00 << 8) |
					((uint16)ph->sslpnphy_rssi_5g_gs << 10) |
					(0x01 << 13));
				break;
			case WL_CHAN_FREQ_RANGE_5GM:
				/* 5 GHz middle */
				b0 = pi->txpa_5g_mid[0];
				b1 = pi->txpa_5g_mid[1];
				a1 = pi->txpa_5g_mid[2];
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
			default:
				/* 5 GHz high */
				b0 = pi->txpa_5g_hi[0];
				b1 = pi->txpa_5g_hi[1];
				a1 = pi->txpa_5g_hi[2];
				break;
#endif /* BAND5G */
			}

#ifdef BAND5G
			write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1,
			((uint16)ph->sslpnphy_rssi_5g_vf << 0) |
			((uint16)ph->sslpnphy_rssi_5g_vc << 4) |
			(0x00 << 8) |
			((uint16)ph->sslpnphy_rssi_5g_gs << 10) |
			(0x01 << 13));
#endif /* BAND5G */

			/* Save new target frequency */
			ph->sslpnphy_target_tx_freq = (uint16)freq;
			ph->sslpnphy_last_tx_freq = (uint16)freq;

			/* Should reset power index cache */
			reset_npt = TRUE;
		}

		/* Clear out all power offsets */
		wlc_sslpnphy_clear_tx_power_offsets(pi);

		/* Setup estPwrLuts for measuring idle TSSI */
		for (ind = 0; ind < 64; ind++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
				&ind, 1, 32, ind);
		}

		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlNnum,
			SSLPNPHY_TxPwrCtrlNnum_Ntssi_delay_MASK |
			SSLPNPHY_TxPwrCtrlNnum_Ntssi_intg_log2_MASK |
			SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK,
			((255 << SSLPNPHY_TxPwrCtrlNnum_Ntssi_delay_SHIFT) |
			(5 << SSLPNPHY_TxPwrCtrlNnum_Ntssi_intg_log2_SHIFT) |
			(0 << SSLPNPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)));

		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlIdleTssi,
			SSLPNPHY_TxPwrCtrlIdleTssi_idleTssi0_MASK,
			0x1f << SSLPNPHY_TxPwrCtrlIdleTssi_idleTssi0_SHIFT);


			mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
				SSLPNPHY_auxadcCtrl_rssifiltEn_MASK |
				SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK |
				SSLPNPHY_auxadcCtrl_txpwrctrlEn_MASK,
				((0 << SSLPNPHY_auxadcCtrl_rssifiltEn_SHIFT) |
				(1 << SSLPNPHY_auxadcCtrl_rssiformatConvEn_SHIFT) |
				(1 << SSLPNPHY_auxadcCtrl_txpwrctrlEn_SHIFT)));

			/* Set IQCAL mux to TSSI */
			iqcal_ctrl2 = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
			if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
				(pi->sh->boardflags & BFL_HGPA)) {
				/* 5356: External PA support */
				pos = SSLPNPHY_TSSI_EXT;

				iqcal_ctrl2 &= (uint8)~(0x0f);
				iqcal_ctrl2 |= 0x08;
			} else {
				pos = SSLPNPHY_TSSI_POST_PA;
				iqcal_ctrl2 &= (uint8)~(0x0c);
				iqcal_ctrl2 |= 0x01;
			}
			write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_ctrl2);

			/* Use PA output for TSSI */
			wlc_sslpnphy_set_tssi_mux(pi, pos);

		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlIdleTssi,
			SSLPNPHY_TxPwrCtrlIdleTssi_rawTssiOffsetBinFormat_MASK,
			1 << SSLPNPHY_TxPwrCtrlIdleTssi_rawTssiOffsetBinFormat_SHIFT);

		/* Synch up with tcl */
		mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_crseddisable_MASK,
			1 << SSLPNPHY_crsgainCtrl_crseddisable_SHIFT);

		/* CCK calculation offset */
		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
			SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_MASK,
			0 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_SHIFT);

		/* Set starting index & NPT to 0 for idle TSSI measurements */
		wlc_sslpnphy_set_start_tx_pwr_idx(pi, 0);
		wlc_sslpnphy_set_tx_pwr_npt(pi, 0);

		/* Force manual power control */
		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlCmd,
			(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			SSLPNPHY_TX_PWR_CTRL_SW);

		/* Force WLAN antenna */
		wlc_sslpnphy_btcx_override_enable(pi);
		wlc_sslpnphy_set_tx_gain_override(pi, TRUE);

		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_internalrftxpu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK,
			0 << SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_SHIFT);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
			b0 = pi->txpa_2g[0];
			b1 = pi->txpa_2g[1];
			a1 = pi->txpa_2g[2];
			if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
				/* save */
				wlc_sslpnphy_get_tx_gain(pi, &ltx_gains);

				/* Idle tssi WAR */
				tx_gains.gm_gain = 0;
				tx_gains.pga_gain = 0;
				tx_gains.pad_gain = 0;
				tx_gains.dac_gain = 0;

				wlc_sslpnphy_set_tx_gain(pi, &tx_gains);
			}

			write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1,
				((uint16)ph->sslpnphy_rssi_vf << 0) |
				((uint16)ph->sslpnphy_rssi_vc << 4) |
				(0x00 << 8) |
				((uint16)ph->sslpnphy_rssi_gs << 10) |
				(0x01 << 13));
			wlc_sslpnphy_idle_tssi_est(pi);
	} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
		wlc_sslpnphy_idle_tssi_est(pi);
	}
		tssi_val = ph->sslpnphy_tssi_val;
		tssi_val -= 32;

		/* Write measured idle TSSI value */
		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlIdleTssi,
			SSLPNPHY_TxPwrCtrlIdleTssi_idleTssi0_MASK,
			tssi_val << SSLPNPHY_TxPwrCtrlIdleTssi_idleTssi0_SHIFT);

		/* Sych up with tcl */
		mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_crseddisable_MASK,
			0 << SSLPNPHY_crsgainCtrl_crseddisable_SHIFT);

		/* Clear tx PU override */
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK,
			0 << SSLPNPHY_RFOverride0_internalrftxpu_ovr_SHIFT);

		/* Invalidate target frequency */
		ph->sslpnphy_target_tx_freq = 0;

		/* CCK calculation offset */
		if (ph->sslpnphy_OLYMPIC)
			mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
				SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_MASK,
				6 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_SHIFT);
		else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			if (phybw40)
				mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
					SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_MASK,
					2 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_SHIFT);
			else
				mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
					SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_MASK,
					1 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_SHIFT);
		}
		else
			mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
				SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_MASK,
				3 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_cckPwrOffset_SHIFT);

		/* Restore back Tempsense */
		write_phy_reg(pi, SSLPNPHY_TempSenseCorrection, tempsense);

		/* Convert tssi to power LUT */
		for (tssi = 0; tssi < 64; tssi++) {
			pwr = wlc_sslpnphy_tssi2dbm(tssi, a1, b0, b1);
			if (freq == 2412)
				pwr = pwr - 0;
			/* Limit max power by 3d highest value from tssi2db table */
			if (pwr < prev_pwr) {
				prev_pwr  = pwr;
				if (++tssi_ladder_cnt == 3)
					ph->sslpnphy_tssi_pwr_limit = (uint8)pwr;
			}
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
				&pwr, 1, 32, tssi);
		}

		/* Invalidate power index and NPT if new power targets */
		if (reset_npt || ph->sslpnphy_papd_cal_done)
			wlc_sslpnphy_txpower_reset_npt(pi);
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_set_tx_gain(pi, &ltx_gains);
		}
		/* Enable hardware power control */
		wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
	}
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}


static uint8
wlc_sslpnphy_get_bbmult(phy_info_t *pi)
{
	uint16 m0m1;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_sslpnphy_read_table(pi, &tab);

	return (uint8)((m0m1 & 0xff00) >> 8);
}

static void
wlc_sslpnphy_set_pa_gain(phy_info_t *pi, uint16 gain)
{
	mod_phy_reg(pi, SSLPNPHY_txgainctrlovrval1,
		SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK,
		gain << SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_stxtxgainctrlovrval1,
		SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK,
		gain << SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT);

}

#define SSLPNPHY_IQLOCC_READ(val) ((uint8)(-(int8)(((val) & 0xf0) >> 4) + (int8)((val) & 0x0f)))

void
wlc_sslpnphy_get_radio_loft(phy_info_t *pi,
	uint8 *ei0,
	uint8 *eq0,
	uint8 *fi0,
	uint8 *fq0)
{
	*ei0 = SSLPNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I));
	*eq0 = SSLPNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q));
	*fi0 = SSLPNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I));
	*fq0 = SSLPNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q));
}

static void
wlc_sslpnphy_get_tx_gain(phy_info_t *pi,  sslpnphy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = read_phy_reg(pi, SSLPNPHY_AfeDACCtrl) >>
		SSLPNPHY_AfeDACCtrl_dac_ctrl_SHIFT;
	gains->dac_gain = (dac_gain & 0x380) >> 7;

	{
		uint16 rfgain0, rfgain1;

		rfgain0 = (read_phy_reg(pi, SSLPNPHY_txgainctrlovrval0) &
			SSLPNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_MASK) >>
			SSLPNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_SHIFT;
		rfgain1 = (read_phy_reg(pi, SSLPNPHY_txgainctrlovrval1) &
			SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_MASK) >>
			SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT;

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

void
wlc_sslpnphy_set_tx_iqcc(phy_info_t *pi, uint16 a, uint16 b)
{
	uint16 iqcc[2];
	/* Fill buffer with coeffs */
	iqcc[0] = a;
	iqcc[1] = b;
	/* Update iqloCaltbl */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		iqcc, 2, 16, 80);
}

static void
wlc_sslpnphy_set_tx_locc(phy_info_t *pi, uint16 didq)
{
	phytbl_info_t tab;

	/* Update iqloCaltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;			/* iqloCaltbl	*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_sslpnphy_write_table(pi, &tab);
}

void
wlc_sslpnphy_set_tx_pwr_by_index(phy_info_t *pi, int index)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ASSERT(index <= SSLPNPHY_MAX_TX_POWER_INDEX);

	/* Save forced index */
	ph->sslpnphy_tx_power_idx_override = (int8)index;
	ph->sslpnphy_current_index = (uint8)index;

	/* Preset txPwrCtrltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */

	/* Turn off automatic power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + index; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET + index; /* gainCtrlLuts */
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Apply tx gain */
	{
		sslpnphy_txgains_t gains;

		gains.gm_gain = (uint16)(txgain & 0xff);
		gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
		gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
		gains.dac_gain = (uint16)(bbmultiqcomp >> 28) & 0x07;

		wlc_sslpnphy_set_tx_gain(pi, &gains);
		wlc_sslpnphy_set_pa_gain(pi,  (uint16)(txgain >> 24) & 0x7f);
	}

	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 20) & 0xff);
	wlc_sslpnphy_set_bbmult(pi, bb_mult);

	/* Apply iqcc */
	a = (uint16)((bbmultiqcomp >> 10) & 0x3ff);
	b = (uint16)(bbmultiqcomp & 0x3ff);
	wlc_sslpnphy_set_tx_iqcc(pi, a, b);

	/* Read index based di & dq from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + index; /* loftCoefLuts */
	tab.tbl_ptr = &locoeffs; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Apply locc */
	wlc_sslpnphy_set_tx_locc(pi, (uint16)locoeffs);

	/* Apply PAPD rf power correction */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + index;
	tab.tbl_ptr = &rfpower; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	MOD_PHY_REG(pi, SSLPNPHY, papd_analog_gain_ovr_val, papd_analog_gain_ovr_val, rfpower * 8);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		wlc_sslpnphy_set_pa_gain(pi, 116);
#endif

	/* Enable gain overrides */
	wlc_sslpnphy_enable_tx_gain_override(pi);
}

static void
wlc_sslpnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx)
{
	/* Set TR switch */
	mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
		SSLPNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK |
		SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		(tx ? SSLPNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK : 0) |
		(rx ? SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK : 0));

	/* Enable overrides */
	or_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK);
}

static void
wlc_sslpnphy_set_swctrl_override(phy_info_t *pi, uint8 index)
{
	phytbl_info_t tab;
	uint16 swctrl_val;

	if (index == SWCTRL_OVR_DISABLE)
	{
		write_phy_reg(pi, SSLPNPHY_swctrlOvr, 0);
	} else {
		tab.tbl_id = SSLPNPHY_TBL_ID_SW_CTRL;
		tab.tbl_width = 16;	/* 16 bit wide	*/
		tab.tbl_ptr = &swctrl_val; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_offset = index; /* tbl offset */
		wlc_sslpnphy_read_table(pi, &tab);

		write_phy_reg(pi, SSLPNPHY_swctrlOvr, 0xff);
		mod_phy_reg(pi, SSLPNPHY_swctrlOvr_val,
			SSLPNPHY_swctrlOvr_val_swCtrl_p_ovr_val_MASK,
			(swctrl_val & 0xf) << SSLPNPHY_swctrlOvr_val_swCtrl_p_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_swctrlOvr_val,
			SSLPNPHY_swctrlOvr_val_swCtrl_n_ovr_val_MASK,
			((swctrl_val >> 4) & 0xf) << SSLPNPHY_swctrlOvr_val_swCtrl_n_ovr_val_SHIFT);
	}
}

static void
wlc_sslpnphy_set_rx_gain_by_distribution(phy_info_t *pi,
	uint16 pga,
	uint16 biq2,
	uint16 pole1,
	uint16 biq1,
	uint16 tia,
	uint16 lna2,
	uint16 lna1)
{
	uint16 gain0_15, gain16_19;

	gain16_19 = pga & 0xf;
	gain0_15 = ((biq2 & 0x1) << 15) |
		((pole1 & 0x3) << 13) |
		((biq1 & 0x3) << 11) |
		((tia & 0x7) << 8) |
		((lna2 & 0x3) << 6) |
		((lna2 & 0x3) << 4) |
		((lna1 & 0x3) << 2) |
		((lna1 & 0x3) << 0);

	mod_phy_reg(pi, SSLPNPHY_rxgainctrl0ovrval,
		SSLPNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_MASK,
		gain0_15 << SSLPNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rxlnaandgainctrl1ovrval,
		SSLPNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_MASK,
		gain16_19 << SSLPNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_SHIFT);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
			SSLPNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_MASK,
			lna1 << SSLPNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverrideVal,
			SSLPNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_MASK,
			lna1 << SSLPNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_SHIFT);
	}
}

static void
wlc_sslpnphy_rx_gain_override_enable(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_rxgainctrl_ovr_MASK,
		ebit << SSLPNPHY_rfoverride2_rxgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK,
		ebit << SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_gmode_rx_pu_ovr_MASK,
		ebit << SSLPNPHY_RFOverride0_gmode_rx_pu_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_amode_rx_pu_ovr_MASK,
		ebit << SSLPNPHY_RFOverride0_amode_rx_pu_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_gmode_ext_lna_gain_ovr_MASK,
		ebit << SSLPNPHY_rfoverride2_gmode_ext_lna_gain_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_amode_ext_lna_gain_ovr_MASK,
		ebit << SSLPNPHY_rfoverride2_amode_ext_lna_gain_ovr_SHIFT);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_slna_gain_ctrl_ovr_MASK,
			ebit << SSLPNPHY_rfoverride2_slna_gain_ctrl_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverride,
			SSLPNPHY_RFinputOverride_wlslnagainctrl_ovr_MASK,
			ebit << SSLPNPHY_RFinputOverride_wlslnagainctrl_ovr_SHIFT);
	}
}

static void
wlc_sslpnphy_rx_pu(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {
		and_phy_reg(pi, SSLPNPHY_RFOverride0,
			~(uint16)(SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK));
		and_phy_reg(pi, SSLPNPHY_rfoverride2,
			~(uint16)(SSLPNPHY_rfoverride2_rxgainctrl_ovr_MASK));
		wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);
	} else {
		/* Force on the transmit chain */
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK,
			1 << SSLPNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_rxgainctrl_ovr_MASK,
			1 << SSLPNPHY_rfoverride2_rxgainctrl_ovr_SHIFT);

		wlc_sslpnphy_set_rx_gain_by_distribution(pi, 15, 1, 3, 3, 7, 3, 3);
		wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
	}
}

void
wlc_sslpnphy_tx_pu(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {
		/* Clear overrides */
		and_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
			~(uint16)(SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
			SSLPNPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK));

		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
			SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
			1 << SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);

		and_phy_reg(pi, SSLPNPHY_RFOverride0,
			~(uint16)(SSLPNPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
			SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK |
			SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK |
			SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
			SSLPNPHY_RFOverride0_ant_selp_ovr_MASK));

		and_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			~(uint16)(SSLPNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK |
			SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
			1 << SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);

			/* Set TR switch */
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK |
			SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
			SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK);

		and_phy_reg(pi, SSLPNPHY_rfoverride3,
			~(uint16)(SSLPNPHY_rfoverride3_stxpapu_ovr_MASK |
			SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK |
			SSLPNPHY_rfoverride3_stxpapu2g_ovr_MASK));

		and_phy_reg(pi, SSLPNPHY_rfoverride3_val,
			~(uint16)(SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK |
			SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK |
			SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK));
	} else {
		/* Force on DAC */
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
			SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK,
			1 << SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
			SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
			0 << SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
			SSLPNPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK,
			1 << SSLPNPHY_AfeCtrlOvr_dac_clk_disable_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
			SSLPNPHY_AfeCtrlOvrVal_dac_clk_disable_ovr_val_MASK,
			0 << SSLPNPHY_AfeCtrlOvrVal_dac_clk_disable_ovr_val_SHIFT);

		/* Force on the transmit chain */
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			(uint16)(SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK |
			SSLPNPHY_RFOverride0_gmode_tx_pu_ovr_MASK),
			(uint16)(1 << SSLPNPHY_RFOverride0_internalrftxpu_ovr_SHIFT |
			1 << SSLPNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_SHIFT));
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			(uint16)(SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK |
			SSLPNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK),
			(uint16)(1 << SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_SHIFT |
			1 << SSLPNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_SHIFT));


		/* Force the TR switch to transmit */
		wlc_sslpnphy_set_trsw_override(pi, TRUE, FALSE);

		/* Force antenna  0 */
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
			0 << SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_ant_selp_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_ant_selp_ovr_SHIFT);


		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			/* PAD PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK,
				1 << SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_SHIFT);
			/* PGA PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpapu2g_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpapu2g_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK,
				1 << SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_SHIFT);
			/* PA PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
				1 << SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
		} else {
			/* PAP PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK,
				0 << SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_SHIFT);
			/* PGA PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpapu2g_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpapu2g_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK,
				0 << SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_SHIFT);
			/* PA PU */
			mod_phy_reg(pi, SSLPNPHY_rfoverride3,
				SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
				1 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
				SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
				0 << SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
		}
	}
}

/*
 * Play samples from sample play buffer
 */
static void
wlc_sslpnphy_run_samples(phy_info_t *pi,
                         uint16 num_samps,
                         uint16 num_loops,
                         uint16 wait,
                         bool iqcalmode)
{
	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sampleDepthCount,
		SSLPNPHY_sampleDepthCount_DepthCount_MASK,
		(num_samps - 1) << SSLPNPHY_sampleDepthCount_DepthCount_SHIFT);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
			SSLPNPHY_sslpnCalibClkEnCtrl_papdTxBbmultPapdifClkEn_MASK,
			1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxBbmultPapdifClkEn_SHIFT);
	}

	if (num_loops != 0xffff)
		num_loops--;
	mod_phy_reg(pi, SSLPNPHY_sampleLoopCount,
		SSLPNPHY_sampleLoopCount_LoopCount_MASK,
		num_loops << SSLPNPHY_sampleLoopCount_LoopCount_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sampleInitWaitCount,
		SSLPNPHY_sampleInitWaitCount_InitWaitCount_MASK,
		wait << SSLPNPHY_sampleInitWaitCount_InitWaitCount_SHIFT);

	if (iqcalmode) {
		/* Enable calibration */
		and_phy_reg(pi,
			SSLPNPHY_iqloCalCmdGctl,
			(uint16)~SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
		or_phy_reg(pi, SSLPNPHY_iqloCalCmdGctl, SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
	} else {
		write_phy_reg(pi, SSLPNPHY_sampleCmd, 1);
		wlc_sslpnphy_tx_pu(pi, 1);
	}
}

void
wlc_sslpnphy_deaf_mode(phy_info_t *pi, bool mode)
{

	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_gmode_ext_lna_gain_ovr_MASK,
		(mode) << SSLPNPHY_rfoverride2_gmode_ext_lna_gain_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
		SSLPNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_MASK,
		0 << SSLPNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_SHIFT);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_amode_ext_lna_gain_ovr_MASK,
			(mode) << SSLPNPHY_rfoverride2_amode_ext_lna_gain_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
			SSLPNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_MASK,
			0 << SSLPNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_SHIFT);
	}
#endif

	if (phybw40 == 0) {
		mod_phy_reg((pi), SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK |
			SSLPNPHY_crsgainCtrl_OFDMDetectionEnable_MASK,
			((CHSPEC_IS2G(pi->radio_chanspec)) ? (!mode) : 0) <<
			SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT |
			(!mode) << SSLPNPHY_crsgainCtrl_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_crseddisable_MASK,
			(mode) << SSLPNPHY_crsgainCtrl_crseddisable_SHIFT);
	} else {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_crseddisable_MASK,
			(mode) << SSLPNPHY_Rev2_crsgainCtrl_40_crseddisable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_eddisable20ul,
			SSLPNPHY_Rev2_eddisable20ul_crseddisable_20U_MASK,
			(mode) << SSLPNPHY_Rev2_eddisable20ul_crseddisable_20U_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_eddisable20ul,
			SSLPNPHY_Rev2_eddisable20ul_crseddisable_20L_MASK,
			(mode) << SSLPNPHY_Rev2_eddisable20ul_crseddisable_20L_SHIFT);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (mode && SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);

			} else {
				if (CHSPEC_SB_UPPER(pi->radio_chanspec)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);

				} else {
				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
				0 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
				1 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);

				}
			}
		} else {
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
			0x00 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
			0x00 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		}
		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_crsgainCtrl_40_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20U_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20L_OFDMDetectionEnable_SHIFT);
		}
	}
}

/*
* Given a test tone frequency, continuously play the samples. Ensure that num_periods
* specifies the number of periods of the underlying analog signal over which the
* digital samples are periodic
*/
void
wlc_sslpnphy_start_tx_tone(phy_info_t *pi, int32 f_kHz, uint16 max_val, bool iqcalmode)
{
	uint8 phy_bw;
	uint16 num_samps, t, k;
	uint32 bw;
	fixed theta = 0, rot = 0;
	cint32 tone_samp;
	uint32 *data_buf;
	uint16 i_samp, q_samp;

	/* Save active tone frequency */
	pi->phy_tx_tone_freq = f_kHz;

	/* Turn off all the crs signals to the MAC */
	wlc_sslpnphy_deaf_mode(pi, TRUE);

	phy_bw = 40;

	data_buf = (uint32 *)MALLOC(pi->sh->osh, sizeof(uint32)*1024);
	if (data_buf == NULL) {
		printf("couldnt allocate memory\n");
		return;
	}
	/* allocate buffer */
	if (f_kHz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k;
			num_samps = bw / ABS(f_kHz);
			k++;
		} while ((num_samps * (uint32)(ABS(f_kHz))) !=  bw);
	} else
		num_samps = 2;

	PHY_INFORM(("wl%d: %s: %d kHz, %d samples\n",
		pi->sh->unit, __FUNCTION__,
		f_kHz, num_samps));

	/* set up params to generate tone */
	rot = FIXED((f_kHz * 36)/phy_bw) / 100; /* 2*pi*f/bw/1000  Note: f in KHz */
	theta = 0;			/* start angle 0 */

	/* tone freq = f_c MHz ; phy_bw = phy_bw MHz ; # samples = phy_bw (1us) ; max_val = 151 */
	/* TCL: set tone_buff [mimophy_gen_tone $f_c $phy_bw $phy_bw $max_val] */
	for (t = 0; t < num_samps; t++) {
		/* compute phasor */
		wlc_phy_cordic(theta, &tone_samp);
		/* update rotation angle */
		theta += rot;
		/* produce sample values for play buffer */
		i_samp = (uint16)(FLOAT(tone_samp.i * max_val) & 0x3ff);
		q_samp = (uint16)(FLOAT(tone_samp.q * max_val) & 0x3ff);
		data_buf[t] = (i_samp << 10) | q_samp;
	}

	/* in SSLPNPHY, we need to bring SPB out of standby before using it */
	mod_phy_reg(pi, SSLPNPHY_sslpnCtrl3,
		SSLPNPHY_sslpnCtrl3_sram_stby_MASK,
		0 << SSLPNPHY_sslpnCtrl3_sram_stby_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);

	/* load sample table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		data_buf, num_samps, 32, 0);
	/* run samples */
	wlc_sslpnphy_run_samples(pi, num_samps, 0xffff, 0, iqcalmode);
	MFREE(pi->sh->osh, data_buf, sizeof(uint32)*1024);
}

static bool
wlc_sslpnphy_iqcal_wait(phy_info_t *pi)
{
	uint delay_count = 0;

	while (wlc_sslpnphy_iqcal_active(pi)) {
#if defined(DSLCPE_DELAY)
		OSL_YIELD_EXEC(pi->sh->osh, 100);
#else
		OSL_DELAY(100);
#endif
		delay_count++;

		if (delay_count > (10 * 500)) /* 500 ms */
			break;
	}

	PHY_TMP(("wl%d: %s: %u us\n", pi->sh->unit, __FUNCTION__, delay_count * 100));

	return (0 == wlc_sslpnphy_iqcal_active(pi));
}

void
wlc_sslpnphy_stop_tx_tone(phy_info_t *pi)
{
	int16 playback_status;

	pi->phy_tx_tone_freq = 0;

	/* Stop sample buffer playback */
	playback_status = read_phy_reg(pi, SSLPNPHY_sampleStatus);
	if (playback_status & SSLPNPHY_sampleStatus_NormalPlay_MASK) {
		wlc_sslpnphy_tx_pu(pi, 0);
		mod_phy_reg(pi, SSLPNPHY_sampleCmd,
			SSLPNPHY_sampleCmd_stop_MASK,
			1 << SSLPNPHY_sampleCmd_stop_SHIFT);
	} else if (playback_status & SSLPNPHY_sampleStatus_iqlocalPlay_MASK)
		mod_phy_reg(pi, SSLPNPHY_iqloCalCmdGctl,
			SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK,
			0 << SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
		0 << SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_MASK,
		0 << SSLPNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_SHIFT);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
			SSLPNPHY_sslpnCalibClkEnCtrl_papdTxBbmultPapdifClkEn_MASK,
			0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxBbmultPapdifClkEn_SHIFT);
	}

	/* in SSLPNPHY, we need to bring SPB out of standby before using it */
	mod_phy_reg(pi, SSLPNPHY_sslpnCtrl3,
		SSLPNPHY_sslpnCtrl3_sram_stby_MASK,
		1 << SSLPNPHY_sslpnCtrl3_sram_stby_SHIFT);

	/* Restore all the crs signals to the MAC */
	wlc_sslpnphy_deaf_mode(pi, FALSE);
}

static void
wlc_sslpnphy_clear_trsw_override(phy_info_t *pi)
{
	/* Clear overrides */
	and_phy_reg(pi, SSLPNPHY_RFOverride0,
		(uint16)~(SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK));
}

/*
 * TX IQ/LO Calibration
 *
 * args: target_gains = Tx gains *for* which the cal is done, not necessarily *at* which it is done
 *       If not specified, will use current Tx gains as target gains
 *
 */
static void
wlc_sslpnphy_tx_iqlo_cal(
	phy_info_t *pi,
	sslpnphy_txgains_t *target_gains,
	sslpnphy_cal_mode_t cal_mode,
	bool keep_tone)
{
	/* starting values used in full cal
	 * -- can fill non-zero vals based on lab campaign (e.g., per channel)
	 * -- format: a0,b0,a1,b1,ci0_cq0_ci1_cq1,di0_dq0,di1_dq1,ei0_eq0,ei1_eq1,fi0_fq0,fi1_fq1
	 */
	sslpnphy_txgains_t cal_gains, temp_gains;
	uint16 hash;
	uint8 band_idx;
	int j;
	uint16 ncorr_override[5];
	uint16 syst_coeffs[] =
		{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

	/* cal commands full cal and recal */
	uint16 commands_fullcal[] =  { 0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };
	uint16 commands_recal[] =  { 0x8312, 0x8055, 0x8212 };

	/* calCmdNum register: log2 of settle/measure times for search/gain-ctrl, 4 bits each */
	uint16 command_nums_fullcal[] = { 0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };
	uint16 command_nums_recal[] = {  0x7997, 0x7987, 0x7a97 };
	uint16 *command_nums = command_nums_fullcal;

	uint16 *start_coeffs = NULL, *cal_cmds = NULL, cal_type, diq_start;
	uint16 tx_pwr_ctrl_old, rssi_old;
	uint16 papd_ctrl_old = 0, auxadc_ctrl_old = 0;
	uint16 muxsel_old, pa_ctrl_1_old, extstxctrl1_old;
	uint8 iqcal_old;
	bool tx_gain_override_old;
	sslpnphy_txgains_t old_gains;
	uint8 ei0, eq0, fi0, fq0;
	uint i, n_cal_cmds = 0, n_cal_start = 0;
	uint16 ccktap0 = 0, ccktap1 = 0, ccktap2 = 0, ccktap3 = 0, ccktap4 = 0;
	uint16 epa_ovr, epa_ovr_val;
#ifdef BAND5G
	uint16 sslpnCalibClkEnCtrl_old = 0;
	uint16 Core1TxControl_old = 0;
#endif
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (NORADIO_ENAB(pi->pubpi))
		return;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		/* Saving the default states of realfilter coefficients */
		ccktap0 = read_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0);
		ccktap1 = read_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap1);
		ccktap2 = read_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap2);
		ccktap3 = read_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap3);
		ccktap4 = read_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap4);

		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0, 255);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap1, 0);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap2, 0);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap3, 0);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap4, 0);
	}
	switch (cal_mode) {
		case SSLPNPHY_CAL_FULL:
			start_coeffs = syst_coeffs;
			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			break;

		case SSLPNPHY_CAL_RECAL:
			ASSERT(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid);
			start_coeffs = ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs;
			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
			command_nums = command_nums_recal;
			break;

		case SSLPNPHY_CAL_CURRECAL:
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
				syst_coeffs, 7, 16, 80);
			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
		case SSLPNPHY_CAL_DIGCAL:
			wlc_sslpnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);

			syst_coeffs[7] = ((ei0 & 0xff) << 8) | ((eq0 & 0xff) << 0);
			syst_coeffs[8] = 0;
			syst_coeffs[9] = ((fi0 & 0xff) << 8) | ((fq0 & 0xff) << 0);
			syst_coeffs[10] = 0;

			start_coeffs = syst_coeffs;

			if (SSLPNPHY_CAL_CURRECAL == cal_mode)
				break;

			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			/* Skip analog commands */
			n_cal_start = 2;
			break;

		default:
			ASSERT(FALSE);
	}

	/* Fill in Start Coeffs */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		start_coeffs, 11, 16, 64);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Save RF overide values */
	epa_ovr = read_phy_reg(pi, SSLPNPHY_RFOverride0);
	epa_ovr_val = read_phy_reg(pi, SSLPNPHY_RFOverrideVal0);

	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Save old and apply new tx gains if needed */
	tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_sslpnphy_get_tx_gain(pi, &old_gains);

	if (!target_gains) {
		if (!tx_gain_override_old)
			wlc_sslpnphy_set_tx_pwr_by_index(pi, ph->sslpnphy_tssi_idx);
		wlc_sslpnphy_get_tx_gain(pi, &temp_gains);
		target_gains = &temp_gains;
	}

	hash = (target_gains->gm_gain << 8) |
		(target_gains->pga_gain << 4) |
		(target_gains->pad_gain);


	cal_gains = *target_gains;
	bzero(ncorr_override, sizeof(ncorr_override));
	for (j = 0; j < iqcal_gainparams_numgains_sslpnphy[band_idx]; j++) {
		if (hash == tbl_iqcal_gainparams_sslpnphy[band_idx][j][0]) {
			cal_gains.gm_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][1];
			cal_gains.pga_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][2];
			cal_gains.pad_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][3];
			bcopy(&tbl_iqcal_gainparams_sslpnphy[band_idx][j][3], ncorr_override,
				sizeof(ncorr_override));
			break;
		}
	}

	wlc_sslpnphy_set_tx_gain(pi, &cal_gains);

	PHY_INFORM(("wl%d: %s: target gains: %d %d %d %d, cal_gains: %d %d %d %d\n",
		pi->sh->unit, __FUNCTION__,
		target_gains->gm_gain,
		target_gains->pga_gain,
		target_gains->pad_gain,
		target_gains->dac_gain,
		cal_gains.gm_gain,
		cal_gains.pga_gain,
		cal_gains.pad_gain,
		cal_gains.dac_gain));

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_SHIFT);

	/* Open TR switch */
	wlc_sslpnphy_set_swctrl_override(pi, SWCTRL_BT_TX);

	muxsel_old = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal);
	pa_ctrl_1_old = read_radio_reg(pi, RADIO_2063_PA_CTRL_1);
	extstxctrl1_old = read_phy_reg(pi, SSLPNPHY_extstxctrl1);

	/* Removing all the radio reg intervention in selecting the mux */
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_MASK,
		1 << SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
		SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
		4 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		or_radio_reg(pi, RADIO_2063_PA_SP_1, 0x2);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x10);
		mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 0 << 2);
	}
	else
#endif
		mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 1 << 2);

	or_phy_reg(pi, SSLPNPHY_extstxctrl1, 0x1000);

	/* Adjust ADC common mode */
	rssi_old = read_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1);

	/* crk: sync up with tcl */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, 0x3fff, 0x28af);
	else
#endif
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, 0x3ff, 0xaf);

	/* Set tssi switch to use IQLO */
	iqcal_old = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
	and_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (uint8)~0x0d);

	/* Power on IQLO block */
	and_radio_reg(pi, RADIO_2063_IQCAL_GVAR, (uint8)~0x80);

	/* Turn off PAPD */

	papd_ctrl_old = read_phy_reg(pi, SSLPNPHY_papd_control);
	mod_phy_reg(pi, SSLPNPHY_papd_control,
		SSLPNPHY_papd_control_papdCompEn_MASK,
		0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			sslpnCalibClkEnCtrl_old = read_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
			Core1TxControl_old = read_phy_reg(pi, SSLPNPHY_Core1TxControl);
			mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
				SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK,
				0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
				0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
				SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_MASK,
				0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
				SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_MASK,
				0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txrealfilten_MASK,
				0 << SSLPNPHY_Core1TxControl_txrealfilten_SHIFT);
		}
	}
#endif /* BAND5G */
	auxadc_ctrl_old = read_phy_reg(pi, SSLPNPHY_auxadcCtrl);
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_iqlocalEn_MASK,
		1 << SSLPNPHY_auxadcCtrl_iqlocalEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK,
		0 << SSLPNPHY_auxadcCtrl_rssiformatConvEn_SHIFT);

	/* Load the LO compensation gain table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		sslpnphy_iqcal_loft_gainladder, ARRAYSIZE(sslpnphy_iqcal_loft_gainladder),
		16, 0);
	/* Load the IQ calibration gain table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		sslpnphy_iqcal_ir_gainladder, ARRAYSIZE(sslpnphy_iqcal_ir_gainladder),
		16, 32);
	/* Set Gain Control Parameters */
	/* iqlocal_en<15> / start_index / thresh_d2 / ladder_length_d2 */
	write_phy_reg(pi, SSLPNPHY_iqloCalCmdGctl, 0x0aa9);

	/* Send out calibration tone */
	if (!pi->phy_tx_tone_freq) {
		wlc_sslpnphy_start_tx_tone(pi, 3750, 88, 1);
	}

	mod_phy_reg(pi, SSLPNPHY_rfoverride3,
		SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
		1 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
		SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
		0 << SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
	/* Disable epa during calibrations */
	if ((CHSPEC_IS5G(pi->radio_chanspec)) &&
		(pi->sh->boardflags & BFL_HGPA)) {
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK,
			0  << SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_SHIFT);
	}

	/*
	 * Cal Steps
	 */
	for (i = n_cal_start; i < n_cal_cmds; i++) {
		uint16 zero_diq = 0;
		uint16 best_coeffs[11];
		uint16 command_num;

		cal_type = (cal_cmds[i] & 0x0f00) >> 8;


		/* get & set intervals */
		command_num = command_nums[i];
		if (ncorr_override[cal_type])
			command_num = ncorr_override[cal_type] << 8 | (command_num & 0xff);

		write_phy_reg(pi, SSLPNPHY_iqloCalCmdNnum, command_num);

		PHY_TMP(("wl%d: %s: running cmd: %x, cmd_num: %x\n",
			pi->sh->unit, __FUNCTION__, cal_cmds[i], command_nums[i]));

		/* need to set di/dq to zero if analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);
			/* Set to zero during analog LO cal */
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL, &zero_diq,
				1, 16, 69);
		}

		/* Issue cal command */
		write_phy_reg(pi, SSLPNPHY_iqloCalCmd, cal_cmds[i]);

		/* Wait until cal command finished */
		if (!wlc_sslpnphy_iqcal_wait(pi)) {
			PHY_ERROR(("wl%d: %s: tx iqlo cal failed to complete\n",
				pi->sh->unit, __FUNCTION__));
			/* No point to continue */
			goto cleanup;
		}

		/* Copy best coefficients to start coefficients */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
			best_coeffs, ARRAYSIZE(best_coeffs), 16, 96);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL, best_coeffs,
			ARRAYSIZE(best_coeffs), 16, 64);
		/* restore di/dq in case of analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);
		}
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
			ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs,
			ARRAYSIZE(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs), 16, 96);
	}

	/*
	 * Apply Results
	 */

	/* Save calibration results */
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs,
		ARRAYSIZE(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs), 16, 96);
	ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid = TRUE;

	/* Apply IQ Cal Results */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		&ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0], 4, 16, 80);
	/* Apply Digital LOFT Comp */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		&ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5], 2, 16, 85);
	/* Dump results */
	PHY_INFORM(("wl%d: %s complete, IQ %d %d LO %d %d %d %d %d %d\n",
		pi->sh->unit, __FUNCTION__,
		(int16)ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0],
		(int16)ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[1],
		(int8)((ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5] & 0xff00) >> 8),
		(int8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5] & 0x00ff),
		(int8)((ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] & 0xff00) >> 8),
		(int8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] & 0x00ff),
		(int8)((ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] & 0xff00) >> 8),
		(int8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] & 0x00ff)));

cleanup:
	/* Switch off test tone */
	if (!keep_tone)
		wlc_sslpnphy_stop_tx_tone(pi);

	/* Reset calibration  command register */
	write_phy_reg(pi, SSLPNPHY_iqloCalCmdGctl, 0);
	{
		/* RSSI ADC selection */
		mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
			SSLPNPHY_auxadcCtrl_iqlocalEn_MASK,
			0 << SSLPNPHY_auxadcCtrl_iqlocalEn_SHIFT);

		/* Power off IQLO block */
		or_radio_reg(pi, RADIO_2063_IQCAL_GVAR, 0x80);

		/* Adjust ADC common mode */
		write_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, rssi_old);

		/* Restore tssi switch */
		write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_old);

		/* Restore PAPD */
		write_phy_reg(pi, SSLPNPHY_papd_control, papd_ctrl_old);

		/* Restore epa after cal */
		write_phy_reg(pi, SSLPNPHY_RFOverride0, epa_ovr);
		write_phy_reg(pi, SSLPNPHY_RFOverrideVal0, epa_ovr_val);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			write_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, sslpnCalibClkEnCtrl_old);
			write_phy_reg(pi, SSLPNPHY_Core1TxControl, Core1TxControl_old);
		}
	}
#endif
		/* Restore ADC control */
		write_phy_reg(pi, SSLPNPHY_auxadcCtrl, auxadc_ctrl_old);
	}

	write_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal, muxsel_old);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_1, pa_ctrl_1_old);
	write_phy_reg(pi, SSLPNPHY_extstxctrl1, extstxctrl1_old);

	/* TR switch */
	wlc_sslpnphy_set_swctrl_override(pi, SWCTRL_OVR_DISABLE);

	/* RSSI on/off */
	and_phy_reg(pi, SSLPNPHY_AfeCtrlOvr, (uint16)~SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK);

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_MASK,
		0 << SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_SHIFT);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_sslpnphy_set_tx_gain(pi, &old_gains);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		/* Restoring the origianl values */
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0, ccktap0);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap1, ccktap1);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap2, ccktap2);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap3, ccktap3);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap4, ccktap4);
	}
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_MASK,
		0 << SSLPNPHY_AfeCtrlOvr_rssi_muxsel_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride3,
		SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
		0 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT);
}

void
wlc_sslpnphy_get_tx_iqcc(phy_info_t *pi, uint16 *a, uint16 *b)
{
	uint16 iqcc[2];
	wlc_sslpnphy_common_read_table(pi, 0, iqcc, 2, 16, 80);
	*a = iqcc[0];
	*b = iqcc[1];
}

uint16
wlc_sslpnphy_get_tx_locc(phy_info_t *pi)
{
	uint16 didq;

	/* Update iqloCaltbl */
	wlc_sslpnphy_common_read_table(pi, 0, &didq, 1, 16, 85);
	return didq;
}

/* Run iqlo cal and populate iqlo portion of tx power control table */
static void
wlc_sslpnphy_txpwrtbl_iqlo_cal(phy_info_t *pi)
{
	sslpnphy_txgains_t target_gains;
	uint8 save_bb_mult;
	uint16 a, b, didq, save_pa_gain = 0;
	uint idx;
	uint32 val;
#ifdef BAND5G
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	/* Store state */
	save_bb_mult = wlc_sslpnphy_get_bbmult(pi);

	/* Set up appropriate target gains */
	{
		/* PA gain */
		save_pa_gain = wlc_sslpnphy_get_pa_gain(pi);

		wlc_sslpnphy_set_pa_gain(pi, 0x10);

#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			/* Since we can not switch off pa from phy put gain to 0 */
			wlc_sslpnphy_set_pa_gain(pi, 0x00);
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 200;
			target_gains.pad_gain = 245;
			target_gains.dac_gain = 0;
			if (freq < 5150) {
				target_gains.pga_gain = 80;
				target_gains.pad_gain = 200;
			} else if (freq < 5340)
				target_gains.pga_gain = 100;
			else if (freq < 5600)
				target_gains.pga_gain = 60;
			else if (freq < 5640)
				target_gains.pga_gain = 40;
			else
				target_gains.pga_gain = 60;
		} else
#endif /* BAND5G */
		{
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 60;
			target_gains.pad_gain = 248;
			target_gains.dac_gain = 0;
		}
	}

	/* Run cal */
	wlc_sslpnphy_tx_iqlo_cal(pi, &target_gains, (ph->sslpnphy_recal ?
		SSLPNPHY_CAL_RECAL : SSLPNPHY_CAL_FULL), FALSE);

	{
		uint8 ei0, eq0, fi0, fq0;

		wlc_sslpnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);
		if ((ABS((int8)fi0) == 15) && (ABS((int8)fq0) == 15)) {
			PHY_ERROR(("wl%d: %s: tx iqlo cal failed, retrying...\n",
				pi->sh->unit, __FUNCTION__));

#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			target_gains.gm_gain = 255;
			target_gains.pga_gain = 255;
			target_gains.pad_gain = 0xf0;
			target_gains.dac_gain = 0;
		} else
#endif
		{
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 45;
			target_gains.pad_gain = 186;
			target_gains.dac_gain = 0;
		}
			/* Re-run cal */
			wlc_sslpnphy_tx_iqlo_cal(pi, &target_gains, SSLPNPHY_CAL_FULL, FALSE);
		}
	}

	/* Get calibration results */
	wlc_sslpnphy_get_tx_iqcc(pi, &a, &b);
	didq = wlc_sslpnphy_get_tx_locc(pi);

	/* Populate tx power control table with coeffs */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		val = (val & 0xfff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);

		/* loft */
		val = didq;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + idx);
	}

	/* Restore state */
	wlc_sslpnphy_set_bbmult(pi, save_bb_mult);
	wlc_sslpnphy_set_pa_gain(pi, save_pa_gain);
}

static void
wlc_sslpnphy_set_tx_filter_bw(phy_info_t *pi, uint16 bw)
{
	/* cck/all non-ofdm setting */
	mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
		SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
		bw << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
	/* ofdm setting */
	mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg1,
		SSLPNPHY_lpfbwlutreg1_lpfbwlut5_MASK,
		bw << SSLPNPHY_lpfbwlutreg1_lpfbwlut5_SHIFT);
}

static void
wlc_sslpnphy_set_rx_gain(phy_info_t *pi, uint32 gain)
{
	uint16 trsw, ext_lna, lna1, lna2, gain0_15, gain16_19;

	trsw = (gain & ((uint32)1 << 20)) ? 0 : 1;
	ext_lna = (uint16)(gain >> 21) & 0x01;
	lna1 = (uint16)(gain >> 2) & 0x03;
	lna2 = (uint16)(gain >> 6) & 0x03;
	gain0_15 = (uint16)gain & 0xffff;
	gain16_19 = (uint16)(gain >> 16) & 0x0f;

	mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
		SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		trsw << SSLPNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
		SSLPNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_MASK,
		ext_lna << SSLPNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
		SSLPNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_MASK,
		ext_lna << SSLPNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rxgainctrl0ovrval,
		SSLPNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_MASK,
		gain0_15 << SSLPNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_rxlnaandgainctrl1ovrval,
		SSLPNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_MASK,
		gain16_19 << SSLPNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_SHIFT);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
			SSLPNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_MASK,
			lna1 << SSLPNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverrideVal,
			SSLPNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_MASK,
			lna1 << SSLPNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_SHIFT);
	}
	wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
}

static void
wlc_sslpnphy_papd_cal_setup_cw(
	phy_info_t *pi)
{
	uint32 papd_buf[] = {0x7fc00, 0x5a569, 0x1ff, 0xa5d69, 0x80400, 0xa5e97, 0x201, 0x5a697};

	/* Tune the hardware delay */
	write_phy_reg(pi, SSLPNPHY_papd_spb2papdin_dly, 33);

	/* Set samples/cycle/4 for q delay */
		mod_phy_reg(pi, SSLPNPHY_papd_variable_delay,
			(SSLPNPHY_papd_variable_delay_papd_pre_int_est_dly_MASK	|
			SSLPNPHY_papd_variable_delay_papd_int_est_ovr_or_cw_dly_MASK),
			(((4-1) << SSLPNPHY_papd_variable_delay_papd_pre_int_est_dly_SHIFT) |
			0 << SSLPNPHY_papd_variable_delay_papd_int_est_ovr_or_cw_dly_SHIFT));

	/* Set LUT begin gain, step gain, and size (Reset values, remove if possible) */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_begin, 7000);
	else
#endif
		write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_begin, 6000);
	write_phy_reg(pi, SSLPNPHY_papd_rx_gain_comp_dbm, 100);
	write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_step, 0x444);
	write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_end, 0x3f);

	/* Set papd constants (reset values, remove if possible) */
	write_phy_reg(pi, SSLPNPHY_papd_dbm_offset, 0x681);
	write_phy_reg(pi, SSLPNPHY_papd_track_dbm_adj_mult_factor, 0xcd8);
	write_phy_reg(pi, SSLPNPHY_papd_track_dbm_adj_add_factor_lsb, 0xc15c);
	write_phy_reg(pi, SSLPNPHY_papd_track_dbm_adj_add_factor_msb, 0x1b);

	/* Dc estimation samples */
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_dc_est, 0x49);

	/* Processing parameters */
	write_phy_reg(pi, SSLPNPHY_papd_num_skip_count, 0x27);
	write_phy_reg(pi, SSLPNPHY_papd_num_samples_count, 255);
	write_phy_reg(pi, SSLPNPHY_papd_sync_count, 319);
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_index_num_cnt, 255);
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_corelator_run_cnt, 1);
	write_phy_reg(pi, SSLPNPHY_smoothenLut_max_thr, 0x7ff);
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_sync_clip_threshold, 0);

	/* Overide control Params */
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_loop_gain_offset_ovr_15_0, 0x0000);
	write_phy_reg(pi, SSLPNPHY_papd_ofdm_loop_gain_offset_ovr_18_16, 0x0007);
	write_phy_reg(pi, SSLPNPHY_papd_dcest_i_ovr, 0x0000);
	write_phy_reg(pi, SSLPNPHY_papd_dcest_q_ovr, 0x0000);

	/* PAPD Update */
	write_phy_reg(pi, SSLPNPHY_papd_lut_update_beta, 0x0008);

	/* Spb parameters */
	write_phy_reg(pi, SSLPNPHY_papd_spb_num_vld_symbols_n_dly, 0x60);
	write_phy_reg(pi, SSLPNPHY_sampleDepthCount, 8-1);

	/* Load Spb - Remove it latter when CW waveform gets a fixed place inside SPB. */
	write_phy_reg(pi, SSLPNPHY_papd_spb_rd_address, 0x0000);

	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		&papd_buf, 8, 32, 0);

	/* BBMULT parameters */
	write_phy_reg(pi, SSLPNPHY_papd_bbmult_num_symbols, 1-1);
	write_phy_reg(pi, SSLPNPHY_papd_rx_sm_iqmm_gain_comp, 0x100);
}

static void
wlc_sslpnphy_papd_cal_core(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t calType,
	bool rxGnCtrl,
	uint8 num_symbols4lpgn,
	bool init_papd_lut,
	uint16 papd_bbmult_init,
	uint16 papd_bbmult_step,
	bool papd_lpgn_ovr,
	uint16 LPGN_I,
	uint16 LPGN_Q)
{
	uint32 papdcompdeltatbl_init_val;

	mod_phy_reg(pi, SSLPNPHY_papd_control2,
		SSLPNPHY_papd_control2_papd_loop_gain_cw_ovr_MASK,
		papd_lpgn_ovr << SSLPNPHY_papd_control2_papd_loop_gain_cw_ovr_SHIFT);

	/* Load papd comp delta table */

	papdcompdeltatbl_init_val = 0x80000;

	mod_phy_reg(pi, SSLPNPHY_papd_control,
		SSLPNPHY_papd_control_papdCompEn_MASK,
		0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_papd_control,
		SSLPNPHY_papd_control_papd_use_pd_out4learning_MASK,
		0 << SSLPNPHY_papd_control_papd_use_pd_out4learning_SHIFT);
	/* Reset the PAPD Hw to reset register values */
	/* Check if this is what tcl meant */

	if (calType == SSLPNPHY_PAPD_CAL_CW) {

		/* Overide control Params */
		/* write_phy_reg(pi, SSLPNPHY_papd_control2, 0); */
		write_phy_reg(pi, SSLPNPHY_papd_loop_gain_ovr_cw_i, LPGN_I);
		write_phy_reg(pi, SSLPNPHY_papd_loop_gain_ovr_cw_q, LPGN_Q);

		/* Spb parameters */
		write_phy_reg(pi, SSLPNPHY_papd_track_num_symbols_count, num_symbols4lpgn);
		write_phy_reg(pi, SSLPNPHY_sampleLoopCount, (num_symbols4lpgn+1)*20-1);

		/* BBMULT parameters */
		write_phy_reg(pi, SSLPNPHY_papd_bbmult_init, papd_bbmult_init);
		write_phy_reg(pi, SSLPNPHY_papd_bbmult_step, papd_bbmult_step);
		/* Run PAPD HW Cal */
		write_phy_reg(pi, SSLPNPHY_papd_control, 0xa021);

#ifndef SSLPNPHY_PAPD_OFDM
	}
#else
	} else {

		/* Number of Sync and Training Symbols */
		write_phy_reg(pi, SSLPNPHY_papd_track_num_symbols_count, 255);
		write_phy_reg(pi, SSLPNPHY_papd_sync_symbol_count, 49);

		/* Load Spb */
		write_phy_reg(pi, SSLPNPHY_papd_spb_rd_address, 0x0000);


		for (j = 0; j < 16; j++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
				&sslpnphy_papd_cal_ofdm_tbl[j][0], 64, 32, j * 64);
		}

		for (; j < 25; j++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
				&sslpnphy_papd_cal_ofdm_tbl[j][0], 64, 32, j * 64);
		}

		/* Number of CW samples in spb - 1; Num of OFDM samples per symbol in SPB */

		write_phy_reg(pi, SSLPNPHY_sampleDepthCount, 160-1);

		/* Number of loops - 1 for CW; 2-1 for replay with rotation by -j in OFDM */
		write_phy_reg(pi, SSLPNPHY_sampleLoopCount,  1);

		write_phy_reg(pi, SSLPNPHY_papd_bbmult_init, 20000);
		write_phy_reg(pi, SSLPNPHY_papd_bbmult_step, 22000);
		write_phy_reg(pi, SSLPNPHY_papd_bbmult_ofdm_sync, 8192);

		/* If Cal is done at a gain other than the ref gain
		 * (incremental cal over an earlier cal)
		 * then gain difference needs to be subracted here
		*/
		write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_begin, 6700);

		write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_step, 0x444);
		write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_end,  0x3f);
		write_phy_reg(pi, SSLPNPHY_papd_lut_update_beta, 0x8);

		/* [3:0] vld sym in spb         -1, ofdm; [7:4] spb delay */
		write_phy_reg(pi, SSLPNPHY_papd_spb_num_vld_symbols_n_dly, 0x69);

		if (rxGnCtrl) {
			/* Only run the synchronizer - no tracking */
			write_phy_reg(pi, SSLPNPHY_papd_track_num_symbols_count, 0);
		}

		/* Run PAPD HW Cal */
		write_phy_reg(pi, SSLPNPHY_papd_control, 0xb083);
	}
#endif /* SSLPNPHY_PAPD_OFDM */

	/* Wait for completion, around 1ms */
	SPINWAIT(read_phy_reg(pi, SSLPNPHY_papd_control) & SSLPNPHY_papd_control_papd_cal_run_MASK,
		1 * 1000);

}

static int32 wlc_sslpnphy_vbatsense(phy_info_t *pi);

static uint32
wlc_sslpnphy_papd_rxGnCtrl(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	uint8 CurTxGain)
{
	/* Square of Loop Gain (inv) target for CW (reach as close to tgt, but be more than it) */
	/* dB Loop gain (inv) target for OFDM (reach as close to tgt,but be more than it) */
	int32 rxGnInit = 8;
	uint8  bsStep = 4; /* Binary search initial step size */
	uint8  bsDepth = 5; /* Binary search depth */
	uint8  bsCnt;
	int16  lgI, lgQ;
	int32  cwLpGn2;
	int32  cwLpGn2_min = 8192, cwLpGn2_max = 16384;
	uint8  num_symbols4lpgn;
	int32 volt_start, volt_end;
	uint8 counter = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	for (bsCnt = 0; bsCnt < bsDepth; bsCnt++) {
		if (rxGnInit > 15)
			rxGnInit = 15; /* out-of-range correction */

		wlc_sslpnphy_set_rx_gain_by_distribution(pi, (uint16)rxGnInit, 0, 0, 0, 0, 0, 0);

		num_symbols4lpgn = 90;
		counter = 0;
		do {
			if (counter >= 5)
				break;
			volt_start = wlc_sslpnphy_vbatsense(pi);
			wlc_sslpnphy_papd_cal_core(pi, cal_type,
				TRUE,
				num_symbols4lpgn,
				1,
				1400,
				16640,
				0,
				128,
				0);
			volt_end = wlc_sslpnphy_vbatsense(pi);
			if ((volt_start < ph->sslpnphy_volt_winner) ||
				(volt_end < ph->sslpnphy_volt_winner))
				counter ++;
		} while ((volt_start < ph->sslpnphy_volt_winner) ||
			(volt_end < ph->sslpnphy_volt_winner));

		if (cal_type == SSLPNPHY_PAPD_CAL_CW)
		{
			lgI = ((int16) read_phy_reg(pi, SSLPNPHY_papd_loop_gain_cw_i)) << 6;
			lgI = lgI >> 6;
			lgQ = ((int16) read_phy_reg(pi, SSLPNPHY_papd_loop_gain_cw_q)) << 6;
			lgQ = lgQ >> 6;
			cwLpGn2 = (lgI * lgI) + (lgQ * lgQ);

			if (cwLpGn2 < cwLpGn2_min) {
				rxGnInit = rxGnInit - bsStep;
				if (bsCnt == 4)
					rxGnInit = rxGnInit - 1;
			} else if (cwLpGn2 >= cwLpGn2_max) {
				rxGnInit = rxGnInit + bsStep;
			} else {
				break;
			}
#ifndef SSLPNPHY_PAPD_OFDM
		}
#else
		} else {
			int32 lgLow, lgHigh;
			int32 ofdmLpGnDb, ofdmLpGnDbTgt = 0;

			/* is this correct ? */
			lgLow = (uint32) read_phy_reg(pi, SSLPNPHY_papd_ofdm_loop_gain_offset_15_0);
			if (lgLow < 0)
				lgLow = lgLow + 65536;

			/* SSLPNPHY_papd_ofdm_loop_gain_offset_18_16_
			 * papd_ofdm_loop_gain_offset_18_16_MASK
			 * is a veeery long register mask. substituting its value instead
			 */
			lgHigh = ((int16)
				 read_phy_reg(pi, SSLPNPHY_papd_ofdm_loop_gain_offset_18_16)) &
				 0x7;


			ofdmLpGnDb = lgHigh*65536 + lgLow;
			if (ofdmLpGnDb < ofdmLpGnDbTgt) {
				rxGnInit = rxGnInit - bsStep;
				if (bsCnt == 4)
					rxGnInit = rxGnInit - 1;
			} else {
				rxGnInit = rxGnInit + bsStep;
			}

		}
#endif /* SSLPNPHY_PAPD_OFDM */
		bsStep = bsStep >> 1;
	}
	if (rxGnInit < 0)
		rxGnInit = 0; /* out-of-range correction */

	ph->sslpnphy_papdRxGnIdx = rxGnInit;
	return rxGnInit;
}

static void
wlc_sslpnphy_afe_clk_init(phy_info_t *pi, uint8 mode)
{
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (!NORADIO_ENAB(pi->pubpi)) {
		/* Option 2 : NO IQ SWAP for QT @ ADC INPUT */
		mod_phy_reg(pi, SSLPNPHY_rxfe,
			SSLPNPHY_rxfe_swap_rxfiltout_iq_MASK,
			1 << SSLPNPHY_rxfe_swap_rxfiltout_iq_SHIFT);
	} else {
		/* Option 2 : IQ SWAP @ ADC INPUT */
		mod_phy_reg(pi, SSLPNPHY_rxfe,
			SSLPNPHY_rxfe_swap_rxfiltout_iq_MASK,
			0 << SSLPNPHY_rxfe_swap_rxfiltout_iq_SHIFT);
	}

	if (!mode && (phybw40 == 1)) {
		write_phy_reg(pi, SSLPNPHY_adc_2x, 0);
	} else {
	/* Setting adc in 2x mode */
	write_phy_reg(pi, SSLPNPHY_adc_2x, 0x7);
	}

	/* Selecting pos-edge of dac clock for driving the samples to dac */
	mod_phy_reg(pi, SSLPNPHY_sslpnCtrl4,
		SSLPNPHY_sslpnCtrl4_flip_dacclk_edge_MASK,
		0 << SSLPNPHY_sslpnCtrl4_flip_dacclk_edge_SHIFT);

	/* Selecting neg-edge of adc clock for sampling the samples from adc (in adc-presync),
	 * to meet timing
	*/
	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_sslpnCtrl4,
			SSLPNPHY_sslpnCtrl4_flip_adcclk2x_edge_MASK |
			SSLPNPHY_sslpnCtrl4_flip_adcclk1x_edge_MASK,
			(1 << SSLPNPHY_sslpnCtrl4_flip_adcclk2x_edge_SHIFT)|
			(1 << SSLPNPHY_sslpnCtrl4_flip_adcclk1x_edge_SHIFT));
	} else {
		mod_phy_reg(pi, SSLPNPHY_sslpnCtrl4,
			SSLPNPHY_sslpnCtrl4_flip_adcclk2x_edge_MASK |
			SSLPNPHY_sslpnCtrl4_flip_adcclk1x_edge_MASK,
			(0 << SSLPNPHY_sslpnCtrl4_flip_adcclk2x_edge_SHIFT)|
			(0 << SSLPNPHY_sslpnCtrl4_flip_adcclk1x_edge_SHIFT));
	}

	 /* Selecting pos-edge of 80Mhz phy clock for sampling the samples
	  * from adc (in adc-presync)
	  */
	mod_phy_reg(pi, SSLPNPHY_sslpnCtrl4,
		SSLPNPHY_sslpnCtrl4_flip_adcclk2x_80_edge_MASK |
		SSLPNPHY_sslpnCtrl4_flip_adcclk1x_80_edge_MASK,
		((0 << SSLPNPHY_sslpnCtrl4_flip_adcclk2x_80_edge_SHIFT)|
		(0 << SSLPNPHY_sslpnCtrl4_flip_adcclk1x_80_edge_SHIFT)));

	 /* Selecting pos-edge of aux-adc clock, 80Mhz phy clock for sampling the samples
	  * from aux adc (in auxadc-presync)
	  */
	mod_phy_reg(pi, SSLPNPHY_sslpnCtrl4,
		SSLPNPHY_sslpnCtrl4_flip_auxadcclk_edge_MASK |
		SSLPNPHY_sslpnCtrl4_flip_auxadcclkout_edge_MASK |
		SSLPNPHY_sslpnCtrl4_flip_auxadcclk80_edge_MASK,
		((0 << SSLPNPHY_sslpnCtrl4_flip_auxadcclk_edge_SHIFT) |
		(0 << SSLPNPHY_sslpnCtrl4_flip_auxadcclkout_edge_SHIFT) |
		(0 << SSLPNPHY_sslpnCtrl4_flip_auxadcclk80_edge_SHIFT)));

	 /* Setting the adc-presync mux to select the samples registered with adc-clock */
	mod_phy_reg(pi, SSLPNPHY_sslpnAdcCtrl,
		SSLPNPHY_sslpnAdcCtrl_sslpnAdcCtrlMuxAdc2x_MASK |
		SSLPNPHY_sslpnAdcCtrl_sslpnAdcCtrlMuxAdc1x_MASK,
		((mode << SSLPNPHY_sslpnAdcCtrl_sslpnAdcCtrlMuxAdc2x_SHIFT) |
		(mode << SSLPNPHY_sslpnAdcCtrl_sslpnAdcCtrlMuxAdc1x_SHIFT)));

	 /* Setting the auxadc-presync muxes to select
	  * the samples registered with auxadc-clockout
	  */
	mod_phy_reg(pi, SSLPNPHY_sslpnAuxAdcCtrl,
		SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl0_MASK |
		SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl1_MASK |
		SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl2_MASK,
		((0 << SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl0_SHIFT) |
		(1 << SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl1_SHIFT) |
		(1 << SSLPNPHY_sslpnAuxAdcCtrl_sslpnAuxAdcMuxCtrl2_SHIFT)));

	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
		1 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
		0 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	wlc_sslpnphy_toggle_afe_pwdn(pi);
}

static void
InitIntpapdlut(uint8 Max, uint8 Min, uint8 *papdIntlutVld)
{
	uint16 a;

	for (a = Min; a <= Max; a++) {
		papdIntlutVld[a] = 0;
	}
}

static void
wlc_sslpnphy_saveIntpapdlut(phy_info_t *pi, int8 Max,
	int8 Min, uint32 *papdIntlut, uint8 *papdIntlutVld)
{
	phytbl_info_t tab;
	uint16 a;

	tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_width = 32;     /* 32 bit wide */

	/* Max should be in range of 0 to 127 */
	/* Min should be in range of 0 to 126 */
	/* else no updates are available */

	if ((Min < 64) && (Max >= 0)) {
		Max = Max * 2 + 1;
		Min = Min * 2;

		tab.tbl_ptr = papdIntlut + Min; /* ptr to buf */
		tab.tbl_len = Max - Min + 1;        /* # values   */
		tab.tbl_offset = Min; /* tbl offset */
		wlc_sslpnphy_read_table(pi, &tab);

		for (a = Min; a <= Max; a++) {
			papdIntlutVld[a] = 1;
		}
	}
}

static void
wlc_sslpnphy_GetpapdMaxMinIdxupdt(phy_info_t *pi,
	int8 *maxUpdtIdx,
	int8 *minUpdtIdx)
{
	uint16 papd_lut_index_updt_63_48, papd_lut_index_updt_47_32;
	uint16 papd_lut_index_updt_31_16, papd_lut_index_updt_15_0;
	int8 MaxIdx, MinIdx;
	uint8 MaxIdxUpdated, MinIdxUpdated;
	uint8 i;

	papd_lut_index_updt_63_48 = read_phy_reg(pi, SSLPNPHY_papd_lut_index_updated_63_48);
	papd_lut_index_updt_47_32 = read_phy_reg(pi, SSLPNPHY_papd_lut_index_updated_47_32);
	papd_lut_index_updt_31_16 = read_phy_reg(pi, SSLPNPHY_papd_lut_index_updated_31_16);
	papd_lut_index_updt_15_0  = read_phy_reg(pi, SSLPNPHY_papd_lut_index_updated_15_0);

	MaxIdx = 63;
	MinIdx = 0;
	MinIdxUpdated = 0;
	MaxIdxUpdated = 0;

	for (i = 0; i < 16 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_15_0 & (1 << i)) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 32 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_31_16 & (1 << (i - 16))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 48 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_47_32 & (1 << (i - 32))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 64 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_63_48 & (1 << (i - 48))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}

	/* loop for getting max index updated */
	for (i = 0; i < 16 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_63_48 & (1 << (15 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 32 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_47_32 & (1 << (31 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 48 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_31_16 & (1 << (47 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 64 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_15_0 & (1 << (63 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	*maxUpdtIdx = MaxIdx;
	*minUpdtIdx = MinIdx;
}

static void
genpapdlut(phy_info_t *pi, uint32 *papdIntlut, uint8 *papdIntlutVld)
{
	uint32 papdcompdeltatblval;
	uint8 a, b;
	uint8 present, next;
	uint32 present_comp, next_comp;
	int16 present_comp_I, present_comp_Q;
	int16 next_comp_I, next_comp_Q;
	int16 delta_I, delta_Q;

	papdcompdeltatblval = 128 << 12;
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
		&papdcompdeltatblval, 1, 32, 1);
	for (a = 3; a < 128; a = a + 2) {
		if (papdIntlutVld[a] == 1) {
			papdcompdeltatblval = papdIntlut[a];
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		} else {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a - 2);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		}
	}

	/* Writing Deltas */
	for (b = 0; b <= 124; b = b + 2) {
		present = b + 1;
		next = b + 3;

		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, present);
		present_comp = papdcompdeltatblval;
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, next);
		next_comp = papdcompdeltatblval;

		present_comp_I = (present_comp & 0x00fff000) << 8;
		present_comp_Q = (present_comp & 0x00000fff) << 20;

		present_comp_I = present_comp_I >> 20;
		present_comp_Q = present_comp_Q >> 20;

		next_comp_I = (next_comp & 0x00fff000) << 8;
		next_comp_Q = (next_comp & 0x00000fff) << 20;

		next_comp_I = next_comp_I >> 20;
		next_comp_Q = next_comp_Q >> 20;

		delta_I = next_comp_I - present_comp_I;
		delta_Q = next_comp_Q - present_comp_Q;

		if (delta_I > 2048)
			delta_I = 2048;
		else if (delta_I < -2048)
			delta_I = -2048;

		if (delta_Q > 2048)
			delta_Q = 2048;
		else if (delta_Q < -2048)
			delta_Q = -2048;

		papdcompdeltatblval = ((delta_I << 12) & 0xfff000) | (delta_Q & 0xfff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, b);
	}
}

static void
wlc_sslpnphy_pre_papd_cal_setup(phy_info_t *pi, sslpnphy_txcalgains_t *txgains, bool restore)
{
	uint16  rf_common_02_old, rf_common_07_old;
	uint32 refTxAnGn;
#ifdef BAND5G
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (!restore) {
		ph->sslpnphy_store.bb_mult_old = wlc_sslpnphy_get_bbmult(pi);
		wlc_sslpnphy_tx_pu(pi, TRUE);
		wlc_sslpnphy_rx_pu(pi, TRUE);
		ph->sslpnphy_store.lpfbwlut0 = read_phy_reg(pi, SSLPNPHY_lpfbwlutreg0);
		ph->sslpnphy_store.lpfbwlut1 = read_phy_reg(pi, SSLPNPHY_lpfbwlutreg1);
		ph->sslpnphy_store.rf_txbb_sp_3 = read_radio_reg(pi, RADIO_2063_TXBB_SP_3);
		ph->sslpnphy_store.rf_pa_ctrl_14 = read_radio_reg(pi, RADIO_2063_PA_CTRL_14);

		/* Widen tx filter */
		wlc_sslpnphy_set_tx_filter_bw(pi, 5);
		ph->sslpnphy_store.CurTxGain = 0; /* crk: Need to fill this correctly */

		/* Set tx gain */
		if (txgains) {
			if (txgains->useindex) {
				wlc_sslpnphy_set_tx_pwr_by_index(pi, txgains->index);
				ph->sslpnphy_store.CurTxGain = txgains->index;
			} else {
				wlc_sslpnphy_set_tx_gain(pi, &txgains->gains);
			}
		}

		/* Set TR switch to transmit */
		/* wlc_sslpnphy_set_trsw_override(pi, FALSE, FALSE); */
		/* Set Rx path mux to PAPD and turn on PAPD mixer */
		ph->sslpnphy_store.rxbb_ctrl2_old = read_radio_reg(pi, RADIO_2063_RXBB_CTRL_2);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if ((pi->sh->boardflags & BFL_EXTLNA) ||
				(pi->phy_aa2g >= 2) ||
			    ((BOARDTYPE(pi->sh->boardtype) == BCM94319WLUSBN4L_SSID) && (pi->phy_aa2g == 1)))
			{
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
					(3 << 3), (uint8)(2 << 3));
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
					(1 << 1), (uint8)(1 << 1));
			} else {
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, (3 << 3),
					(uint8)(3 << 3));
			}
		}
#ifdef BAND5G
		else {
			if ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ||
				(freq >= 5500)) {
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
					(3 << 3), (uint8)(2 << 3));
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
					(1 << 1), (uint8)(1 << 1));
			} else {
				mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, (3 << 3),
					(uint8)(3 << 3));
			}
		}
#endif /* BAND5G */

		/* turn on PAPD mixer */
		/* no overide for bit 4 & 5 */
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		rf_common_07_old = read_radio_reg(pi, RADIO_2063_COMMON_07);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x1);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x18);
		ph->sslpnphy_store.pa_sp1_old_5_4 = (read_radio_reg(pi, RADIO_2063_PA_SP_1)) & (3 << 4);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(2 << 4));
		} else {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(1 << 4));
		}

		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		write_radio_reg(pi, RADIO_2063_COMMON_07, rf_common_07_old);
		wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_PAPD);
			ph->sslpnphy_store.Core1TxControl_old = read_phy_reg(pi, SSLPNPHY_Core1TxControl);
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_BphyFrqBndSelect_MASK	|
			SSLPNPHY_Core1TxControl_iqImbCompEnable_MASK	|
			SSLPNPHY_Core1TxControl_loft_comp_en_MASK,
			(1 << SSLPNPHY_Core1TxControl_BphyFrqBndSelect_SHIFT)	|
			(1 << SSLPNPHY_Core1TxControl_iqImbCompEnable_SHIFT)	|
			(1 << SSLPNPHY_Core1TxControl_loft_comp_en_SHIFT));

		/* in SSLPNPHY, we need to bring SPB out of standby before using it */
		ph->sslpnphy_store.sslpnCtrl3_old = read_phy_reg(pi, SSLPNPHY_sslpnCtrl3);
		mod_phy_reg(pi, SSLPNPHY_sslpnCtrl3,
			SSLPNPHY_sslpnCtrl3_sram_stby_MASK,
			0 << SSLPNPHY_sslpnCtrl3_sram_stby_SHIFT);
		ph->sslpnphy_store.SSLPNPHY_sslpnCalibClkEnCtrl_old = read_phy_reg(pi,
			SSLPNPHY_sslpnCalibClkEnCtrl);
		or_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, 0x8f);

		/* Set PAPD reference analog gain */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&refTxAnGn, 1, 32,
			SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + txgains->index);
		refTxAnGn = refTxAnGn * 8;
		write_phy_reg(pi, SSLPNPHY_papd_tx_analog_gain_ref,
			(uint16)refTxAnGn);

		/* Turn off LNA */
		ph->sslpnphy_store.rf_common_03_old = read_radio_reg(pi, RADIO_2063_COMMON_03);
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			or_radio_reg(pi, RADIO_2063_COMMON_03, 0x18);
			or_radio_reg(pi, RADIO_2063_COMMON_02, 0x2);
			ph->sslpnphy_store.rf_grx_sp_1_old = read_radio_reg(pi, RADIO_2063_GRX_SP_1);
			write_radio_reg(pi, RADIO_2063_GRX_SP_1, 0x1e);
			/* sslpnphy_rx_pu sets some bits which needs */
			/* to be override here for papdcal . so dont reset common_03 */
			write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		}
	} else {  /* if (!restore) */
		/* restore saved registers */
		write_radio_reg(pi, RADIO_2063_COMMON_03, ph->sslpnphy_store.rf_common_03_old);
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		or_radio_reg(pi, RADIO_2063_COMMON_03, 0x18);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x2);
		write_radio_reg(pi, RADIO_2063_GRX_SP_1, ph->sslpnphy_store.rf_grx_sp_1_old);
		write_radio_reg(pi, RADIO_2063_COMMON_03, ph->sslpnphy_store.rf_common_03_old);
		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		write_phy_reg(pi, SSLPNPHY_lpfbwlutreg0, ph->sslpnphy_store.lpfbwlut0);
		write_phy_reg(pi, SSLPNPHY_lpfbwlutreg1, ph->sslpnphy_store.lpfbwlut1);
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, ph->sslpnphy_store.rf_txbb_sp_3);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_14, ph->sslpnphy_store.rf_pa_ctrl_14);
		write_phy_reg(pi, SSLPNPHY_Core1TxControl, ph->sslpnphy_store.Core1TxControl_old);
		write_phy_reg(pi, SSLPNPHY_sslpnCtrl3, ph->sslpnphy_store.sslpnCtrl3_old);
		/* restore calib ctrl clk */
		/* switch on PAPD clk */
		write_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		ph->sslpnphy_store.SSLPNPHY_sslpnCalibClkEnCtrl_old | 0x1);
		wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);
		/* TR switch */
		wlc_sslpnphy_clear_trsw_override(pi);
		/* Restore rx path mux and turn off PAPD mixer */
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		rf_common_07_old = read_radio_reg(pi, RADIO_2063_COMMON_07);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x1);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x18);
		mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), ph->sslpnphy_store.pa_sp1_old_5_4);
		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		write_radio_reg(pi, RADIO_2063_COMMON_07, rf_common_07_old);
		write_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, ph->sslpnphy_store.rxbb_ctrl2_old);
		/* Clear rx PU override */
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
			0 << SSLPNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
		wlc_sslpnphy_rx_pu(pi, FALSE);
		wlc_sslpnphy_tx_pu(pi, FALSE);
		/* Clear rx gain override */
		wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);

		/* Clear ADC override */
		mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
			SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
			0 << SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
		/* restore bbmult */
		wlc_sslpnphy_set_bbmult(pi, ph->sslpnphy_store.bb_mult_old);
	}
}

static void
wlc_sslpnphy_papd_cal(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	sslpnphy_txcalgains_t *txgains,
	bool frcRxGnCtrl,
	uint8 num_symbols,
	uint8 papd_lastidx_search_mode)
{
	uint32 rxGnIdx;
	phytbl_info_t tab;
	uint32 tmpVar;
	uint32 refTxAnGn;

	uint8 papd_peak_curr_mode = 1;
	uint8 lpgn_ovr;
	uint8 peak_curr_num_symbols_th;
	uint16 bbmult_init, bbmult_step;
	int8 maxUpdtIdx, minUpdtIdx;
	uint16 LPGN_I, LPGN_Q;
	uint16 tmp;
	uint32 bbmult_init_tmp;
	uint16 rem_symb;
	uint32 *papdIntlut = NULL;
	uint8 *papdIntlutVld = NULL;
	int32 volt_start, volt_end;
	uint8 counter = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ASSERT((cal_type == SSLPNPHY_PAPD_CAL_CW) || (cal_type == SSLPNPHY_PAPD_CAL_OFDM));

	PHY_CAL(("Running papd cal, channel: %d cal type: %d\n",
		CHSPEC_CHANNEL(pi->radio_chanspec),
		cal_type));

	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, FALSE);

	/* Do Rx Gain Control */
	wlc_sslpnphy_papd_cal_setup_cw(pi);
	rxGnIdx = wlc_sslpnphy_papd_rxGnCtrl(pi, cal_type, frcRxGnCtrl,
		ph->sslpnphy_store.CurTxGain);

	/* Set Rx Gain */
	wlc_sslpnphy_set_rx_gain_by_distribution(pi, (uint16)rxGnIdx, 0, 0, 0, 0, 0, 0);

	/* clear our PAPD Compensation table */
	wlc_sslpnphy_clear_papd_comptable(pi);

	/* Do PAPD Operation */
	if (papd_peak_curr_mode == 1) {
		lpgn_ovr = 0;
		peak_curr_num_symbols_th = 70;
		bbmult_init = 1400;
		bbmult_step = 16640;
		counter = 0;
		do {
				if (counter >= 5)
					break;
				volt_start = wlc_sslpnphy_vbatsense(pi);
				wlc_sslpnphy_papd_cal_core(pi, cal_type,
					FALSE,
					peak_curr_num_symbols_th,
					1,
					bbmult_init,
					bbmult_step,
					0,
					128,
					0);

				volt_end = wlc_sslpnphy_vbatsense(pi);
				if ((volt_start < ph->sslpnphy_volt_winner) ||
					(volt_end < ph->sslpnphy_volt_winner)) {
					OSL_DELAY(300);
					counter ++;
				}
			} while ((volt_start < ph->sslpnphy_volt_winner) ||
				(volt_end < ph->sslpnphy_volt_winner));

		if (papd_lastidx_search_mode == 1) {
		} else {
			wlc_sslpnphy_GetpapdMaxMinIdxupdt(pi, &maxUpdtIdx, &minUpdtIdx);

			papdIntlut = (uint32 *)MALLOC(pi->sh->osh, 128 * sizeof(uint32));
			papdIntlutVld = (uint8 *)MALLOC(pi->sh->osh, 128 * sizeof(uint8));
			if (papdIntlut == NULL || papdIntlutVld == NULL) {
				PHY_ERROR(("wl%d: %s: MALLOC failure\n",
				           pi->sh->unit, __FUNCTION__));
				ASSERT(0);
				return;		/* function should not be void */
			}

			InitIntpapdlut(127, 0, papdIntlutVld);
			wlc_sslpnphy_saveIntpapdlut(pi, maxUpdtIdx, minUpdtIdx,
				papdIntlut, papdIntlutVld);

			LPGN_I = read_phy_reg(pi, SSLPNPHY_papd_loop_gain_cw_i);
			LPGN_Q = read_phy_reg(pi, SSLPNPHY_papd_loop_gain_cw_q);

			for (tmp = 0; tmp < peak_curr_num_symbols_th; tmp++) {
				bbmult_init_tmp = (bbmult_init * bbmult_step) >> 14;
				if (bbmult_init_tmp >= 65535) {
					bbmult_init = 65535;
				} else {
					bbmult_init = (uint16) bbmult_init_tmp;
				}
			}
			rem_symb = num_symbols- peak_curr_num_symbols_th;
			while (rem_symb != 0) {
				lpgn_ovr = 1;
				bbmult_init_tmp = (bbmult_init * bbmult_step) >> 14;
				if (bbmult_init_tmp >= 65535) {
					bbmult_init = 65535;
				} else {
					bbmult_init = (uint16) bbmult_init_tmp;
				}

				counter = 0;
				do {
					if (counter >= 5)
						break;
					volt_start = wlc_sslpnphy_vbatsense(pi);
					wlc_sslpnphy_papd_cal_core(pi, cal_type,
						FALSE,
						0,
						1,
						bbmult_init,
						bbmult_step,
						lpgn_ovr,
						LPGN_I,
						LPGN_Q);

					volt_end = wlc_sslpnphy_vbatsense(pi);
					if ((volt_start < ph->sslpnphy_volt_winner) ||
						(volt_end < ph->sslpnphy_volt_winner)) {
						OSL_DELAY(600);
						counter ++;
					}
				} while ((volt_start < ph->sslpnphy_volt_winner) ||
					(volt_end < ph->sslpnphy_volt_winner));

				wlc_sslpnphy_GetpapdMaxMinIdxupdt(pi, &maxUpdtIdx, &minUpdtIdx);
				wlc_sslpnphy_saveIntpapdlut(pi, maxUpdtIdx, minUpdtIdx,
					papdIntlut, papdIntlutVld);
				maxUpdtIdx = 2 * maxUpdtIdx + 1;
				minUpdtIdx = 2 * minUpdtIdx;
				if (maxUpdtIdx > 0) {
				tab.tbl_offset = maxUpdtIdx; /* tbl offset */
				tab.tbl_width = 32;     /* 32 bit wide */
				tab.tbl_len = 1;        /* # values   */
				tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
				tab.tbl_ptr = &refTxAnGn; /* ptr to buf */
				wlc_sslpnphy_read_table(pi, &tab);
				}
				if (maxUpdtIdx == 127)
					break;

				rem_symb = rem_symb - 1;
			}
			genpapdlut(pi, papdIntlut, papdIntlutVld);
			MFREE(pi->sh->osh, papdIntlut, 128 * sizeof(uint32));
			MFREE(pi->sh->osh, papdIntlutVld, 128 * sizeof(uint8));

		}
	} else {
		wlc_sslpnphy_papd_cal_core(pi, cal_type,
			FALSE,
			219,
			1,
			1400,
			16640,
			0,
			128,
			0);

		if (cal_type == SSLPNPHY_PAPD_CAL_CW) {
			tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
			tab.tbl_offset = 125;
			tab.tbl_ptr = &tmpVar; /* ptr to buf */
			wlc_sslpnphy_read_table(pi, &tab);
			tab.tbl_offset = 127;
			wlc_sslpnphy_write_table(pi, &tab);

			tmpVar = 0;
			tab.tbl_offset = 124;
			wlc_sslpnphy_write_table(pi, &tab);
			tab.tbl_offset = 126;
			wlc_sslpnphy_write_table(pi, &tab);


			tab.tbl_offset = 0;
			wlc_sslpnphy_write_table(pi, &tab);
			tab.tbl_offset = 3;
			wlc_sslpnphy_read_table(pi, &tab);
			tab.tbl_offset = 1;
			wlc_sslpnphy_write_table(pi, &tab);
		}

	}

	PHY_CAL(("wl%d: %s: PAPD cal completed\n",
		pi->sh->unit, __FUNCTION__));

	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, TRUE);
}

static void
wlc_sslpnphy_vbatsense_papd_cal(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	sslpnphy_txcalgains_t *txgains)
{

	int32 cnt, volt_high_cnt, volt_mid_cnt, volt_low_cnt;
	int32 volt_avg;
	int32 voltage_samples[50];
	int32 volt_high_thresh, volt_low_thresh;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, FALSE);

	{
		volt_high_cnt = 0;
		volt_low_cnt = 0;
		volt_mid_cnt = 0;

		volt_avg = 0;

		for (cnt = 0; cnt < 32; cnt++) {
			voltage_samples[cnt] = wlc_sslpnphy_vbatsense(pi);
			volt_avg += voltage_samples[cnt];
			OSL_DELAY(120);
			/* assuming a 100us time for executing wlc_sslpnphy_vbatsense */
	}
		volt_avg = volt_avg >> 5;

		volt_high_thresh = 0;
		volt_low_thresh = volt_avg;
		for (cnt = 0; cnt < 32; cnt++) {
			if (voltage_samples[cnt] > volt_high_thresh)
				volt_high_thresh = voltage_samples[cnt];
			if (voltage_samples[cnt] < volt_low_thresh)
				volt_low_thresh = voltage_samples[cnt];
		}
		/* for taking care of vhat dip conditions */

		ph->sslpnphy_volt_low = (uint8)volt_low_thresh;
		ph->sslpnphy_volt_winner = (uint8)(volt_high_thresh - 2);

	}


	ph->sslpnphy_last_cal_voltage = volt_low_thresh;
	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, TRUE);
}

static int8
wlc_sslpnphy_gain_based_psat_detect(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type, bool frcRxGnCtrl,
	sslpnphy_txcalgains_t *txgains,	uint8 cur_pwr)
{
	phytbl_info_t tab;
	uint8 papd_lastidx_search_mode = 0;
	int32 Re_div_Im = 60000;
	int32 lowest_gain_diff_local = 59999;
	int32 thrsh_gain = 67600;
	uint16 thrsh_pd = 180;
	int32 gain, gain_diff, psat_check_gain = 0;
	uint32 temp_offset;
	uint32 temp_read[128];
	uint32 papdcompdeltatblval;
	int32 papdcompRe, psat_check_papdcompRe = 0;
	int32 papdcompIm, psat_check_papdcompIm = 0;
	uint8 max_gain_idx = 97;
	uint8 psat_thrsh_num, psat_detected = 0;
	uint8 papdlut_endidx = 97;
	uint8 cur_index = txgains->index;
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_ptr = temp_read;  /* ptr to buf */
	tab.tbl_width = 32;     /* 32 bit wide */
	tab.tbl_len = 87;        /* # values   */
	tab.tbl_offset = 11;

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		max_gain_idx = 97;
		tab.tbl_len = 47;        /* # values   */
		tab.tbl_offset = 81;
		papdlut_endidx = 127;
		thrsh_gain = 25600;
		thrsh_pd = 350;
	}

	if (CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) {
		max_gain_idx = 65;
		tab.tbl_len = 47;        /* # values   */
		tab.tbl_offset = 81;
		papdlut_endidx = 127;
		thrsh_gain = 36100;
		thrsh_pd = 350;
	}
	wlc_sslpnphy_papd_cal(pi, cal_type, txgains,
		frcRxGnCtrl, 219,
		papd_lastidx_search_mode);
	wlc_sslpnphy_read_table(pi, &tab);
	for (temp_offset = 0; temp_offset < tab.tbl_len; temp_offset += 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = (papdcompRe >> 20);
		papdcompIm = (papdcompIm >> 20);
		gain = papdcompRe * papdcompRe + papdcompIm * papdcompIm;
		if (temp_offset == (tab.tbl_len - 1)) {
			psat_check_gain = gain;
			psat_check_papdcompRe = papdcompRe;
			psat_check_papdcompIm = papdcompIm;
		}
		gain_diff = gain - thrsh_gain;
		if (gain_diff < 0) {
			gain_diff = gain_diff * (-1);
		}
		if ((gain_diff < lowest_gain_diff_local) || (temp_offset == 0)) {
			ph->sslpnphy_max_gain = gain;
			max_gain_idx = tab.tbl_offset + temp_offset;
			lowest_gain_diff_local = gain_diff;
		}
	}


	/* Psat Calculation based on gain threshold */
	if (psat_check_gain >= thrsh_gain) {
		psat_detected = 1;
	}
	if (psat_detected == 0) {
		/* Psat Calculation based on PD threshold */
		if (psat_check_papdcompIm != 0) {
			if (psat_check_papdcompIm < 0)
				psat_check_papdcompIm = psat_check_papdcompIm * -1;
			Re_div_Im = (psat_check_papdcompRe / psat_check_papdcompIm) * 100;
		} else {
			Re_div_Im = 60000;
		}
		if (Re_div_Im < thrsh_pd) {
			psat_detected = 1;
		}
	}
	if ((psat_detected == 0) && (cur_index <= 4)) {
		psat_detected = 1;
	}
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) {
			max_gain_idx = max_gain_idx + 16;
		} else {
			if (freq < 5640)
				max_gain_idx = max_gain_idx + 6;
			else if (freq <= 5825)
				max_gain_idx = max_gain_idx + 10;
		}
	}
	if (psat_detected) {
		ph->sslpnphy_psat_pwr = cur_pwr;
		ph->sslpnphy_psat_indx = cur_index;
		psat_thrsh_num = (papdlut_endidx - max_gain_idx)/ 2;
		if (psat_thrsh_num > 6) {
			ph->sslpnphy_psat_pwr = cur_pwr - 2;
			ph->sslpnphy_psat_indx = cur_index + 8;
		} else if (psat_thrsh_num > 2) {
			ph->sslpnphy_psat_pwr = cur_pwr - 1;
			ph->sslpnphy_psat_indx = cur_index + 4;
		}
	}
	if ((lowest_gain_diff_local < ph->sslpnphy_lowest_gain_diff) || (cur_pwr == 17)) {
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			ph->sslpnphy_final_papd_cal_idx = txgains->index +
				(papdlut_endidx - max_gain_idx)*3/8;
		} else {
			ph->sslpnphy_final_papd_cal_idx = txgains->index +
				(papdlut_endidx - max_gain_idx)/2;
		}
		ph->sslpnphy_lowest_gain_diff = lowest_gain_diff_local;

	}

	return psat_detected;
}


static void
wlc_sslpnphy_min_pd_search(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	sslpnphy_txcalgains_t *txgains)
{
	phytbl_info_t tab;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint8 papd_lastidx_search_mode = 0;
	int32 Re_div_Im = 60000;
	int32 lowest_Re_div_Im_local = 59999;
	int32 temp_offset;
	uint32 temp_read[30];
	uint32 papdcompdeltatblval;
	int32 papdcompRe;
	int32 papdcompIm;
	uint8 MinPdIdx = 127;
	tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_ptr = temp_read;  /* ptr to buf */
	tab.tbl_width = 32;     /* 32 bit wide */
	tab.tbl_len = 27;        /* # values   */
	tab.tbl_offset = 101;
	wlc_sslpnphy_papd_cal(pi, cal_type, txgains,
		frcRxGnCtrl, 219,
		papd_lastidx_search_mode);
	wlc_sslpnphy_read_table(pi, &tab);
	for (temp_offset = 0; temp_offset < 27; temp_offset += 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = papdcompRe >> 20;
		papdcompIm = papdcompIm >> 20;
		if (papdcompIm < 0) {
			Re_div_Im = papdcompRe * 100 / papdcompIm * -1;
			if (Re_div_Im < lowest_Re_div_Im_local) {
				lowest_Re_div_Im_local = Re_div_Im;
				MinPdIdx = tab.tbl_offset + temp_offset;
			}
		}
	}

	if (!ph->sslpnphy_force_1_idxcal) {
	if (lowest_Re_div_Im_local < ph->sslpnphy_lowest_Re_div_Im) {
		ph->sslpnphy_final_papd_cal_idx = txgains->index + (127 - MinPdIdx)/2;
		ph->sslpnphy_lowest_Re_div_Im = lowest_Re_div_Im_local;
	}
	} else {
		ph->sslpnphy_final_papd_cal_idx = (int8)ph->sslpnphy_papd_nxt_cal_idx;
		ph->sslpnphy_lowest_Re_div_Im = lowest_Re_div_Im_local;
}
}

static int8
wlc_sslpnphy_psat_detect(phy_info_t *pi,
	uint8 cur_index,
	uint8 cur_pwr)
{
	int8 pd_ph_cnt = 0;
	int8 psat_detected = 0;
	uint32 temp_read[50];
	int32 temp_offset;
	uint32 papdcompdeltatblval;
	int32 papdcompRe;
	int32 papdcompIm;
	int32 voltage;
	bool gain_psat_det_in_phase_papd = 0;
	int32 thrsh_gain = 40000;
	int32 gain;
	uint thrsh1, thrsh2, pd_thresh = 0;
	int gain_xcd_cnt = 0;
	uint32 num_elements = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	voltage = wlc_sslpnphy_vbatsense(pi);
	ph->sslpnphy_voltage_sensed = voltage;
	if (voltage < 52)
		num_elements = 17;
	else
		num_elements = 39;
	thrsh1 = thrsh2 = 0;
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL, temp_read,
		num_elements, 32, (128 - num_elements));
	if (voltage < 52)
		num_elements = 17;
	else
		num_elements = 39;
	num_elements = num_elements -1;
	for (temp_offset = num_elements; temp_offset >= 0; temp_offset -= 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = papdcompRe >> 20;
		papdcompIm = papdcompIm >> 20;
		if (papdcompIm >= 0) {
			pd_ph_cnt ++;
			psat_detected = 1;
		}
		gain = (papdcompRe * papdcompRe) + (papdcompIm * papdcompIm);
		if (gain > thrsh_gain) {
			gain_xcd_cnt ++;
			psat_detected = 1;
		}

	}
	if (cur_pwr <= 21)
		pd_thresh = 12;
	if (cur_pwr <= 19)
		pd_thresh = 10;
	if (cur_pwr <= 17)
		pd_thresh = 8;
	if ((voltage > 52) && (cur_pwr >= 17) && (psat_detected == 1)) {
		gain_psat_det_in_phase_papd = 1;
		if (cur_pwr == 21) {
			thrsh1 = 8;
			thrsh2 = 12;
		} else {
			thrsh1 = 2;
			thrsh2 =  5;
		}
	}
	if (psat_detected == 1) {

		if (gain_psat_det_in_phase_papd == 0) {
		ph->sslpnphy_psat_pwr = cur_pwr;
		ph->sslpnphy_psat_indx = cur_index;
		if (pd_ph_cnt > 2) {
			ph->sslpnphy_psat_pwr = cur_pwr - 1;
			ph->sslpnphy_psat_indx = cur_index + 4;
		}
		if (pd_ph_cnt > 6) {
			ph->sslpnphy_psat_pwr = cur_pwr - 2;
			ph->sslpnphy_psat_indx = cur_index + 8;
		}
		} else {
			if ((gain_xcd_cnt >  0) || (pd_ph_cnt > (int8)pd_thresh))  {
				ph->sslpnphy_psat_pwr = cur_pwr;
				ph->sslpnphy_psat_indx = cur_index;
			} else {
				psat_detected = 0;
			}
			if ((gain_xcd_cnt >  (int)thrsh1) || (pd_ph_cnt > (int8)(pd_thresh + 2))) {
				ph->sslpnphy_psat_pwr = cur_pwr - 1;
				ph->sslpnphy_psat_indx = cur_index + 4;
			}
			if ((gain_xcd_cnt >  (int)thrsh2) || (pd_ph_cnt > (int8)(pd_thresh + 6))) {
				ph->sslpnphy_psat_pwr = cur_pwr - 2;
				ph->sslpnphy_psat_indx = cur_index + 8;
	}

}
	}
	if ((psat_detected == 0) && (cur_index <= 4)) {
		psat_detected = 1;
		ph->sslpnphy_psat_pwr = cur_pwr;
		ph->sslpnphy_psat_indx = cur_index;
	}
	return (psat_detected);
}

/* Run PAPD cal at power level appropriate for tx gain table */
static void
wlc_sslpnphy_papd_cal_txpwr(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl, bool frcTxGnCtrl, uint16 frcTxidx)
{
	sslpnphy_txcalgains_t txgains;
	bool tx_gain_override_old;
	sslpnphy_txgains_t old_gains;
	uint8 bbmult_old;
	uint16 tx_pwr_ctrl_old;
	uint8 papd_lastidx_search_mode = 0;
	uint8 psat_detected = 0;
	uint8 psat_pwr = 255;
	int32 lowest_Re_div_Im;
	uint8 TxIdx_14;
	uint8 flag = 0;  /* keeps track of upto what dbm can the papd calib be done. */
	uint8 papd_gain_based = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
#ifdef WLPLT
	int32 vbat_delta;
#endif
#ifdef BAND5G
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif


	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) ||
		(BOARDTYPE(pi->sh->boardtype) == BCM94319LCUSBSDN4L_SSID) ||
		(BOARDTYPE(pi->sh->boardtype) == BCM94319LCSDN4L_SSID)) {

		papd_gain_based = 1;
		/* If board has External PA then no need to do papd. */
		if (pi->sh->boardflags & BFL_HGPA)
			return;
	}

#ifdef BAND5G
	papd_gain_based = ((CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0));
#endif

	ph->sslpnphy_lowest_Re_div_Im = 60000;
	ph->sslpnphy_final_papd_cal_idx = 30;
	ph->sslpnphy_psat_indx = 255;
	if (!ph->sslpnphy_force_1_idxcal)
		ph->sslpnphy_psat_pwr = 25;

	ph->sslpnphy_psat_indx = 255;
	ph->sslpnphy_lowest_gain_diff = 60000;

	/* Save current bbMult and txPwrCtrl settings and turn txPwrCtrl off. */
	bbmult_old  = wlc_sslpnphy_get_bbmult(pi);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Save old tx gains if needed */
	tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_sslpnphy_get_tx_gain(pi, &old_gains);

	wlc_sslpnphy_tx_pwr_update_npt(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	txgains.useindex = TRUE;

	if (!ph->sslpnphy_force_1_idxcal)
	{
		if (frcTxGnCtrl)
			txgains.index = (uint8) frcTxidx;

		TxIdx_14 = txgains.index;

#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			if (freq <= 5320)
				TxIdx_14 = 30;
			else
				TxIdx_14 = 45;

			if (BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) {
				TxIdx_14 = txgains.index + 20;
			}
		}
#endif
		if (TxIdx_14 <= 0)
			flag = 13;
		else if (TxIdx_14 <= 28)
			flag = 14 + (int) ((TxIdx_14-1) >> 2);
		else if (TxIdx_14 > 28)
			flag = 21;

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (BOARDTYPE(pi->sh->boardtype) != BCM94319WLUSBN4L_SSID) {
				/* Cal at 17dBm */
				if (flag >= 17) {
					txgains.index = TxIdx_14 - 12;
					if (papd_gain_based) {
					psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
						cal_type, FALSE, &txgains, 17);
					} else {
						wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
						psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 17);
					}

					/* Calib for 18dbm */
					if ((psat_detected == 0) && (flag == 18)) {
						txgains.index = TxIdx_14 - 16;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 18);
						} else {
						wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
						psat_detected = wlc_sslpnphy_psat_detect(pi,
							txgains.index, 18);
						}
					}
					/* Calib for 19dbm */
					if ((psat_detected == 0) && (flag >= 19)) {
						txgains.index = TxIdx_14 - 20;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 19);
						} else {
						wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
						psat_detected = wlc_sslpnphy_psat_detect(pi,
							txgains.index, 19);
						}
						/* Calib for 20dbm */
						if ((psat_detected == 0) && (flag == 20)) {
							txgains.index = TxIdx_14 - 24;
							if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 20);
							} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 20);
							}
						}
						/* Calib for 21dBm */
						if (psat_detected == 0 && flag >= 21) {
							txgains.index = TxIdx_14 - 28;
							if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 21);
							} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 21);
							}
						}
					} else {
						/* Calib for 13dBm */
						if ((psat_detected == 1) && (flag >= 13)) {
							txgains.index = TxIdx_14 + 4;
							if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 13);
							} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 13);
							}
						}
						/* Calib for 14dBm */
						if ((psat_detected == 0) && (flag == 14)) {
							txgains.index = TxIdx_14;
							if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 14);
							} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 14);
							}
						}
						/* Calib for 15dBm */
						if ((psat_detected == 0) && (flag >= 15)) {
							txgains.index = TxIdx_14 - 4;
							if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 15);
							} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 15);
							}
						}
					}
				} else {
					/* Calib for 13dBm */
					if ((flag >= 13) && (psat_detected == 0)) {
						if (TxIdx_14 < 2)
							txgains.index = TxIdx_14 + 1;
						else
							txgains.index = TxIdx_14 + 4;
						ph->sslpnphy_psat_pwr = 13;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 13);
						} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 13);
						}
					}
					/* Calib for 14dBm */
					if ((flag >= 14) && (psat_detected == 0)) {
						txgains.index = TxIdx_14;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 14);
						} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 14);
						}
					}
					/* Calib for 15dBm */
					if ((flag >= 15) && (psat_detected == 0)) {
						txgains.index = TxIdx_14 - 4;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 15);
						} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 15);
						}
					}
					/* Calib for 16dBm */
					if ((flag == 16) && (psat_detected == 0)) {
						txgains.index = TxIdx_14 - 8;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 16);
						} else {
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi,
								txgains.index, 16);
						}
					}
				}
				/* Final PAPD Cal with selected Tx Gain */
				txgains.index =  ph->sslpnphy_final_papd_cal_idx;
			} else {
				txgains.index = TxIdx_14 - 12;
			}
		}
#ifdef BAND5G
		else
		{
			uint32 lastval;
			int32 lreal, limag;
			uint32 mag;
			uint32 final_idx_thresh = 32100;
			uint8 start, stop, mid;

			final_idx_thresh = 42000; /* 1.6 */
			if (freq == 5680)
				final_idx_thresh = 42000;
			if (freq == 5825)
				final_idx_thresh = 42000;

			txgains.useindex = TRUE;
			start = 10;
			stop = 90;
			while (1) {
				mid = (start + stop) / 2;
				txgains.index = mid;
				wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
					frcRxGnCtrl, 219,
					1);

				wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
					&lastval, 1, 32, 127);

				lreal = lastval & 0x00fff000;
				limag = lastval & 0x00000fff;
				lreal = lreal << 8;
				limag = limag << 20;
				lreal = lreal >> 20;
				limag = limag >> 20;

				mag = (lreal * lreal) + (limag * limag);
				if (mag <= final_idx_thresh) {
					stop = mid;
				} else {
					start = mid;
				}
				if ((stop - start) < 3)
					break;
			}
		}
#endif /* BAND5G */

		wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
			frcRxGnCtrl, 219, papd_lastidx_search_mode);
	} else {
		ph->sslpnphy_psat_indx = (uint8)ph->sslpnphy_papd_nxt_cal_idx;
		txgains.index =  (uint8)ph->sslpnphy_papd_nxt_cal_idx;
		psat_pwr = flag;
		wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
	}

	ph->sslpnphy_11n_backoff = 0;
	ph->sslpnphy_lowerofdm = 0;
	ph->sslpnphy_54_48_36_24mbps_backoff = 0;
	ph->sslpnphy_cck = 0;
	/* New backoff scheme */
	psat_pwr = ph->sslpnphy_psat_pwr;
	lowest_Re_div_Im = ph->sslpnphy_lowest_Re_div_Im;

#ifdef WLPLT
	if (ph->sslpnphy_psat_2pt3_detected == 1) {
		ph->sslpnphy_11n_backoff = 17;
		ph->sslpnphy_54_48_36_24mbps_backoff = 26;
		ph->sslpnphy_lowerofdm = 20;
		ph->sslpnphy_cck = 20;
	} else if ((psat_pwr == 21) && (lowest_Re_div_Im < 400)) {
		ph->sslpnphy_11n_backoff = 5;
		ph->sslpnphy_54_48_36_24mbps_backoff = 4;
	} else if ((psat_pwr == 20) && (lowest_Re_div_Im < 400)) {
		ph->sslpnphy_11n_backoff = 7;
		ph->sslpnphy_54_48_36_24mbps_backoff = 6;
		ph->sslpnphy_lowerofdm = 4;
		ph->sslpnphy_cck = 4;
	} else if ((psat_pwr == 20) && (ph->sslpnphy_psat_indx < 5)) {
		ph->sslpnphy_11n_backoff = 5;
	} else if (psat_pwr == 19) {
		if (lowest_Re_div_Im < 450) {
			ph->sslpnphy_11n_backoff = 9;
			ph->sslpnphy_54_48_36_24mbps_backoff = 8;
		} else if (lowest_Re_div_Im > 600) {
			ph->sslpnphy_54_48_36_24mbps_backoff = 2;
			ph->sslpnphy_11n_backoff = 5;
		} else {
			ph->sslpnphy_11n_backoff = 5;
			ph->sslpnphy_54_48_36_24mbps_backoff = 1;
		}
	} else if (psat_pwr == 18) {
		ph->sslpnphy_11n_backoff = 5;
		ph->sslpnphy_54_48_36_24mbps_backoff = 8;
		ph->sslpnphy_lowerofdm = 2;
		ph->sslpnphy_cck = 2;
	} else if (psat_pwr == 17) {
		ph->sslpnphy_11n_backoff = 5;
		ph->sslpnphy_54_48_36_24mbps_backoff = 10;
		ph->sslpnphy_lowerofdm = 2;
		ph->sslpnphy_cck = 4;
	} else if (psat_pwr == 16) {
		ph->sslpnphy_11n_backoff = 7;
			ph->sslpnphy_54_48_36_24mbps_backoff = 10;
			ph->sslpnphy_lowerofdm = 2;
			ph->sslpnphy_cck = 4;
	} else if (psat_pwr == 15) {
		ph->sslpnphy_11n_backoff = 7;
			ph->sslpnphy_54_48_36_24mbps_backoff = 10;
			ph->sslpnphy_lowerofdm = 2;
			ph->sslpnphy_cck = 4;
	} else if ((psat_pwr == 14) || (psat_pwr == 13) || (psat_pwr == 12)) {
			ph->sslpnphy_11n_backoff = 7;
			ph->sslpnphy_54_48_36_24mbps_backoff = 10;
			ph->sslpnphy_lowerofdm = 2;
			ph->sslpnphy_cck = 4;
	} else if (psat_pwr < 12) {
		ph->sslpnphy_11n_backoff = 27;
		ph->sslpnphy_54_48_36_24mbps_backoff = 32;
		ph->sslpnphy_lowerofdm = 26;
		ph->sslpnphy_cck = 26;
	}

	if ((ph->sslpnphy_last_cal_voltage <= 52) && (psat_pwr <= 21)) {
		vbat_delta = (ph->sslpnphy_last_cal_voltage - 48) / 2;
		ph->sslpnphy_54_48_36_24mbps_backoff = MAX(ph->sslpnphy_54_48_36_24mbps_backoff,
			(4 - vbat_delta));
		ph->sslpnphy_11n_backoff = MAX(ph->sslpnphy_11n_backoff, (3 - vbat_delta));
		ph->sslpnphy_lowerofdm = MAX(ph->sslpnphy_lowerofdm, 2);
		ph->sslpnphy_cck = MAX(ph->sslpnphy_cck, 2);
	}
#endif /* WLPLT */

	/* Taking a snap shot for debugging purpose */
	ph->sslpnphy_psat_pwr = psat_pwr;
	ph->sslpnphy_min_phase = lowest_Re_div_Im;
	ph->sslpnphy_final_idx = txgains.index;

	/* Save papd lut and regs */
	wlc_sslpnphy_save_papd_calibration_results(pi);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_sslpnphy_set_tx_gain(pi, &old_gains);
	wlc_sslpnphy_set_bbmult(pi, bbmult_old);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);
	wlc_sslpnphy_tx_pwr_update_npt(pi);
}

/*
* Get Rx IQ Imbalance Estimate from modem
*/
static bool
wlc_sslpnphy_rx_iq_est(phy_info_t *pi,
	uint16 num_samps,
	uint8 wait_time,
	sslpnphy_iq_est_t *iq_est)
{
	int wait_count = 0;
	bool result = TRUE;
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	/* Turn on clk to Rx IQ */
	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_SHIFT);

	/* Force OFDM receiver on */
	mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
		SSLPNPHY_crsgainCtrl_APHYGatingEnable_MASK,
		0 << SSLPNPHY_crsgainCtrl_APHYGatingEnable_SHIFT);

	if (phybw40 == 1) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_APHYGatingEnable_MASK,
			0 << SSLPNPHY_Rev2_crsgainCtrl_40_APHYGatingEnable_SHIFT);
	}

	mod_phy_reg(pi, SSLPNPHY_IQNumSampsAddress,
		SSLPNPHY_IQNumSampsAddress_numSamps_MASK,
		num_samps << SSLPNPHY_IQNumSampsAddress_numSamps_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_IQEnableWaitTimeAddress,
		SSLPNPHY_IQEnableWaitTimeAddress_waittimevalue_MASK,
		(uint16)wait_time << SSLPNPHY_IQEnableWaitTimeAddress_waittimevalue_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_IQEnableWaitTimeAddress,
		SSLPNPHY_IQEnableWaitTimeAddress_iqmode_MASK,
		0 << SSLPNPHY_IQEnableWaitTimeAddress_iqmode_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_IQEnableWaitTimeAddress,
		SSLPNPHY_IQEnableWaitTimeAddress_iqstart_MASK,
		1 << SSLPNPHY_IQEnableWaitTimeAddress_iqstart_SHIFT);

	/* Wait for IQ estimation to complete */
	while (read_phy_reg(pi, SSLPNPHY_IQEnableWaitTimeAddress) &
		SSLPNPHY_IQEnableWaitTimeAddress_iqstart_MASK) {
		/* Check for timeout */
		if (wait_count > (10 * 500)) { /* 500 ms */
			PHY_ERROR(("wl%d: %s: IQ estimation failed to complete\n",
				pi->sh->unit, __FUNCTION__));
			result = FALSE;
			goto cleanup;
		}
#if defined(DSLCPE_DELAY)
		OSL_YIELD_EXEC(pi->sh->osh, 100);
#else
		OSL_DELAY(100);
#endif
		wait_count++;
	}

	/* Save results */
	iq_est->iq_prod = ((uint32)read_phy_reg(pi, SSLPNPHY_IQAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, SSLPNPHY_IQAccLoAddress);
	iq_est->i_pwr = ((uint32)read_phy_reg(pi, SSLPNPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, SSLPNPHY_IQIPWRAccLoAddress);
	iq_est->q_pwr = ((uint32)read_phy_reg(pi, SSLPNPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, SSLPNPHY_IQQPWRAccLoAddress);
	PHY_TMP(("wl%d: %s: IQ estimation completed in %d us,"
		"i_pwr: %d, q_pwr: %d, iq_prod: %d\n",
		pi->sh->unit, __FUNCTION__,
		wait_count * 100, iq_est->i_pwr, iq_est->q_pwr, iq_est->iq_prod));

cleanup:
	mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
		SSLPNPHY_crsgainCtrl_APHYGatingEnable_MASK,
		1 << SSLPNPHY_crsgainCtrl_APHYGatingEnable_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_MASK,
		0 << SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_SHIFT);
	if (phybw40 == 1) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_APHYGatingEnable_MASK,
			1 << SSLPNPHY_Rev2_crsgainCtrl_40_APHYGatingEnable_SHIFT);
	}

	return result;
}

/*
* Compute Rx compensation coeffs
*   -- run IQ est and calculate compensation coefficients
*/
static bool
wlc_sslpnphy_calc_rx_iq_comp(phy_info_t *pi,  uint16 num_samps)
{
#define SSLPNPHY_MIN_RXIQ_PWR 30000000
	bool result;
	uint16 a0_new, b0_new;
	sslpnphy_iq_est_t iq_est;
	int32  a, b, temp;
	int16  iq_nbits, qq_nbits, arsh, brsh;
	int32  iq;
	uint32 ii, qq;
	uint8  band_idx;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	bzero(&iq_est, sizeof(iq_est));

	/* Save original c0 & c1 */
	a0_new = ((read_phy_reg(pi, SSLPNPHY_RxCompcoeffa0) & SSLPNPHY_RxCompcoeffa0_a0_MASK) >>
		SSLPNPHY_RxCompcoeffa0_a0_SHIFT);
	b0_new = ((read_phy_reg(pi, SSLPNPHY_RxCompcoeffb0) & SSLPNPHY_RxCompcoeffb0_b0_MASK) >>
		SSLPNPHY_RxCompcoeffb0_b0_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_rxfe,
		SSLPNPHY_rxfe_bypass_iqcomp_MASK,
		0 << SSLPNPHY_rxfe_bypass_iqcomp_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_RxIqCoeffCtrl,
		SSLPNPHY_RxIqCoeffCtrl_RxIqComp11bEn_MASK,
		1 << SSLPNPHY_RxIqCoeffCtrl_RxIqComp11bEn_SHIFT);

	/* Zero out comp coeffs and do "one-shot" calibration */
	mod_phy_reg(pi, SSLPNPHY_RxCompcoeffa0,
		SSLPNPHY_RxCompcoeffa0_a0_MASK,
		0 << SSLPNPHY_RxCompcoeffa0_a0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RxCompcoeffb0,
		SSLPNPHY_RxCompcoeffb0_b0_MASK,
		0 << SSLPNPHY_RxCompcoeffb0_b0_SHIFT);

	if (!(result = wlc_sslpnphy_rx_iq_est(pi, num_samps, 32, &iq_est)))
		goto cleanup;

	iq = (int32)iq_est.iq_prod;
	ii = iq_est.i_pwr;
	qq = iq_est.q_pwr;

	/* bounds check estimate info */
	if ((ii + qq) > SSLPNPHY_MIN_RXIQ_PWR) {
		PHY_ERROR(("wl%d: %s: RX IQ imbalance estimate power too large \n",
			pi->sh->unit, __FUNCTION__));
		result = FALSE;
		goto cleanup;
	}

	/* Calculate new coeffs */
	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);

	arsh = 10-(30-iq_nbits);
	if (arsh >= 0) {
		a = (-(iq << (30 - iq_nbits)) + (ii >> (1 + arsh)));
		temp = (int32) (ii >>  arsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return FALSE;
		}
	} else {
		a = (-(iq << (30 - iq_nbits)) + (ii << (-1 - arsh)));
		temp = (int32) (ii << -arsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return FALSE;
		}
	}
	a /= temp;

	brsh = qq_nbits-31+20;
	if (brsh >= 0) {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii >>  brsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return FALSE;
		}
	} else {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii << -brsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return FALSE;
		}
	}
	b /= temp;
	b -= a*a;
	b = (int32)wlc_phy_sqrt_int((uint32) b);
	b -= (1 << 10);

	a0_new = (uint16)(a & 0x3ff);
	b0_new = (uint16)(b & 0x3ff);

	/* Save calibration results */
	ph->sslpnphy_cal_results[band_idx].rxiqcal_coeffa0 = a0_new;
	ph->sslpnphy_cal_results[band_idx].rxiqcal_coeffb0 = b0_new;
	ph->sslpnphy_cal_results[band_idx].rxiq_enable = read_phy_reg(pi, SSLPNPHY_RxIqCoeffCtrl);
	ph->sslpnphy_cal_results[band_idx].rxfe = (uint8)read_phy_reg(pi, SSLPNPHY_rxfe);
	ph->sslpnphy_cal_results[band_idx].loopback1 = (uint8)read_radio_reg(pi,
		RADIO_2063_TXRX_LOOPBACK_1);
	ph->sslpnphy_cal_results[band_idx].loopback2 = (uint8)read_radio_reg(pi,
		RADIO_2063_TXRX_LOOPBACK_2);

cleanup:
	/* Apply new coeffs */
	mod_phy_reg(pi, SSLPNPHY_RxCompcoeffa0,
		SSLPNPHY_RxCompcoeffa0_a0_MASK,
		a0_new << SSLPNPHY_RxCompcoeffa0_a0_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RxCompcoeffb0,
		SSLPNPHY_RxCompcoeffb0_b0_MASK,
		b0_new << SSLPNPHY_RxCompcoeffb0_b0_SHIFT);

	return result;
}

static void
wlc_sslpnphy_stop_ddfs(phy_info_t *pi)
{
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_playoutEn_MASK,
		0 << SSLPNPHY_afe_ddfs_playoutEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_lpphyCtrl,
		SSLPNPHY_lpphyCtrl_afe_ddfs_en_MASK,
		0 << SSLPNPHY_lpphyCtrl_afe_ddfs_en_SHIFT);

	/* switch ddfs clock off */
	and_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, 0xffef);
}

static void
wlc_sslpnphy_run_ddfs(phy_info_t *pi, int i_on, int q_on,
	int incr1, int incr2, int scale_index)
{
	wlc_sslpnphy_stop_ddfs(pi);

	mod_phy_reg(pi, SSLPNPHY_afe_ddfs_pointer_init,
		SSLPNPHY_afe_ddfs_pointer_init_lutPointer1Init_MASK,
		0 << SSLPNPHY_afe_ddfs_pointer_init_lutPointer1Init_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs_pointer_init,
		SSLPNPHY_afe_ddfs_pointer_init_lutPointer2Init_MASK,
		0 << SSLPNPHY_afe_ddfs_pointer_init_lutPointer2Init_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_afe_ddfs_incr_init,
		SSLPNPHY_afe_ddfs_incr_init_lutIncr1Init_MASK,
		incr1 << SSLPNPHY_afe_ddfs_incr_init_lutIncr1Init_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs_incr_init,
		SSLPNPHY_afe_ddfs_incr_init_lutIncr2Init_MASK,
		incr2 << SSLPNPHY_afe_ddfs_incr_init_lutIncr2Init_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_chanIEn_MASK,
		i_on << SSLPNPHY_afe_ddfs_chanIEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_chanQEn_MASK,
		q_on << SSLPNPHY_afe_ddfs_chanQEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_scaleIndex_MASK,
		scale_index << SSLPNPHY_afe_ddfs_scaleIndex_SHIFT);

	/* Single tone */
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_twoToneEn_MASK,
		0x0 << SSLPNPHY_afe_ddfs_twoToneEn_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_afe_ddfs,
		SSLPNPHY_afe_ddfs_playoutEn_MASK,
		0x1 << SSLPNPHY_afe_ddfs_playoutEn_SHIFT);

	/* switch ddfs clock on */
	or_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, 0x10);

	mod_phy_reg(pi, SSLPNPHY_lpphyCtrl,
		SSLPNPHY_lpphyCtrl_afe_ddfs_en_MASK,
		1 << SSLPNPHY_lpphyCtrl_afe_ddfs_en_SHIFT);
}

#ifdef BAND5G
/* only disable function exists */
static void
wlc_sslpnphy_disable_pad(phy_info_t *pi)
{
	sslpnphy_txgains_t current_gain;

	wlc_sslpnphy_get_tx_gain(pi, &current_gain);
	current_gain.pad_gain = 0;
	wlc_sslpnphy_set_tx_gain(pi, &current_gain);
}
#endif /* BAND5G */

/*
* RX IQ Calibration
*/
static bool
wlc_sslpnphy_rx_iq_cal(phy_info_t *pi, const sslpnphy_rx_iqcomp_t *iqcomp, int iqcomp_sz,
	bool use_noise, bool tx_switch, bool rx_switch, bool pa, int tx_gain_idx)
{
	sslpnphy_txgains_t old_gains;
	uint16 tx_pwr_ctrl;
	uint8 tx_gain_index_old = 0;
	bool result = FALSE, tx_gain_override_old = FALSE;
	uint8 ddfs_scale;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
#ifdef BAND5G
	uint16 papd_ctrl_old = 0;
	uint16 Core1TxControl_old = 0;
	uint16 sslpnCalibClkEnCtrl_old = 0;
#define	MAX_IQ_PWR_LMT		536870912
#define	RX_PWR_THRSH_MAX	30000000
#define	RX_PWR_THRSH_MIN	4200000
#endif

	if (iqcomp) {
		ASSERT(iqcomp_sz);

		while (iqcomp_sz--) {
			if (iqcomp[iqcomp_sz].chan == CHSPEC_CHANNEL(pi->radio_chanspec)) {
				/* Apply new coeffs */
				mod_phy_reg(pi, SSLPNPHY_RxCompcoeffa0,
					SSLPNPHY_RxCompcoeffa0_a0_MASK,
					(uint16)iqcomp[iqcomp_sz].a <<
						SSLPNPHY_RxCompcoeffa0_a0_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_RxCompcoeffb0,
					SSLPNPHY_RxCompcoeffb0_b0_MASK,
					(uint16)iqcomp[iqcomp_sz].b <<
						SSLPNPHY_RxCompcoeffb0_b0_SHIFT);
				result = TRUE;
				break;
			}
		}
		ASSERT(result);
		goto cal_done;
	}
	/* PA driver override PA Over ride */
	mod_phy_reg(pi, SSLPNPHY_rfoverride3,
		SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK |
		SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
		((1 << SSLPNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT) |
		(1 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT)));
	mod_phy_reg(pi, SSLPNPHY_rfoverride3_val,
		SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK |
		SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
		((0 << SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_SHIFT) |
		(0 << SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT)));

	if (use_noise) {
		tx_switch = TRUE;
		rx_switch = FALSE;
		pa = FALSE;
	}

	/* Set TR switch */
	wlc_sslpnphy_set_trsw_override(pi, tx_switch, rx_switch);

	/* turn on PA */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
			SSLPNPHY_rfoverride2val_slna_pu_ovr_val_MASK,
			0 << SSLPNPHY_rfoverride2val_slna_pu_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_slna_pu_ovr_MASK,
			1 << SSLPNPHY_rfoverride2_slna_pu_ovr_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_rxlnaandgainctrl1ovrval,
			SSLPNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_MASK,
			0x20 << SSLPNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_lna_pu_ovr_MASK,
			1 << SSLPNPHY_rfoverride2_lna_pu_ovr_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_RFinputOverrideVal,
			SSLPNPHY_RFinputOverrideVal_wlslnapu_ovr_val_MASK,
			0 << SSLPNPHY_RFinputOverrideVal_wlslnapu_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverride,
			SSLPNPHY_RFinputOverride_wlslnapu_ovr_MASK,
			1 << SSLPNPHY_RFinputOverride_wlslnapu_ovr_SHIFT);

		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0x8c);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0);
#ifndef BAND5G
	}
#else
	} else {
		mod_phy_reg(pi, SSLPNPHY_RFOverride0,
			SSLPNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
			1 << SSLPNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
			SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK,
			0  << SSLPNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_rfoverride2val,
			SSLPNPHY_rfoverride2val_slna_pu_ovr_val_MASK,
			0  << SSLPNPHY_rfoverride2val_slna_pu_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_slna_pu_ovr_MASK,
			1 << SSLPNPHY_rfoverride2_slna_pu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rxlnaandgainctrl1ovrval,
			SSLPNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_MASK,
			0x04 << SSLPNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_rfoverride2,
			SSLPNPHY_rfoverride2_lna_pu_ovr_MASK,
			1 << SSLPNPHY_rfoverride2_lna_pu_ovr_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverrideVal,
			SSLPNPHY_RFinputOverrideVal_wlslnapu_ovr_val_MASK,
			0 << SSLPNPHY_RFinputOverrideVal_wlslnapu_ovr_val_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_RFinputOverride,
			SSLPNPHY_RFinputOverride_wlslnapu_ovr_MASK,
			1 << SSLPNPHY_RFinputOverride_wlslnapu_ovr_SHIFT);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0x8c);
	}
#endif /* BAND5G */
	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			papd_ctrl_old = read_phy_reg(pi, SSLPNPHY_papd_control);
			mod_phy_reg(pi, SSLPNPHY_papd_control,
				SSLPNPHY_papd_control_papdCompEn_MASK,
				0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
			sslpnCalibClkEnCtrl_old = read_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
			Core1TxControl_old = read_phy_reg(pi, SSLPNPHY_Core1TxControl);
			mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
				SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK |
				SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_MASK |
				SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_MASK,
				((0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT) |
				(0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_SHIFT) |
				(0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_SHIFT)));
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txcomplexfilten_MASK |
				SSLPNPHY_Core1TxControl_txrealfilten_MASK,
				((0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT) |
				(0 << SSLPNPHY_Core1TxControl_txrealfilten_SHIFT)));
		}
	}
#endif /* BAND5G */
	if (use_noise) {
		 wlc_sslpnphy_set_rx_gain(pi, 0x2d5d);
	} else {

		/* crk: papd ? */

		/* Save old tx gain settings */
		tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
		if (tx_gain_override_old) {
			wlc_sslpnphy_get_tx_gain(pi, &old_gains);
			tx_gain_index_old = ph->sslpnphy_current_index;
		}
		/* Apply new tx gain */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_gain_idx);
		wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, 6, 3, 0);
		wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
	}

	/* Force ADC on */
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
		1 << SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
		SSLPNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK,
		0 << SSLPNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_SHIFT);

	/* Force Rx on	 */
	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
		1 << SSLPNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RFOverrideVal0,
		SSLPNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK,
		1 << SSLPNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_SHIFT);

	if (read_radio_reg(pi, RADIO_2063_TXBB_CTRL_1) == 0x10) {
		ddfs_scale = 2;
	} else {
		ddfs_scale = 0;
	}
	/* Run calibration */
	if (use_noise) {
		wlc_sslpnphy_deaf_mode(pi, TRUE);
		result = wlc_sslpnphy_calc_rx_iq_comp(pi, 0xfff0);
		wlc_sslpnphy_deaf_mode(pi, FALSE);
	} else {
		int tx_idx = 80;
		uint8 tia_gain = 8, lna2_gain = 3, vga_gain = 0;

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wlc_sslpnphy_run_ddfs(pi, 1, 1, 5, 5, ddfs_scale);
			if (ph->sslpnphy_recal)
				tia_gain = ph->sslpnphy_last_cal_tia_gain;

			while (tia_gain > 0) {
			wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, tia_gain-1, 3, 0);
				result = wlc_sslpnphy_calc_rx_iq_comp(pi, 0xffff);
				if (result)
					break;
				tia_gain--;
			}
			wlc_sslpnphy_stop_ddfs(pi);
		} else {
#ifdef BAND5G
			int tx_idx_init, tx_idx_low_lmt;
			uint32 pwr;
			sslpnphy_iq_est_t iq_est;

			bzero(&iq_est, sizeof(iq_est));

			wlc_sslpnphy_start_tx_tone(pi, 4000, 100, 1);

			if (pi->sh->boardflags & BFL_HGPA)
				tx_idx_init = 23;
			else
				tx_idx_init = 60;

			tx_idx = tx_idx_init;
			tx_idx_low_lmt = tx_idx_init - 28;
			if (ph->sslpnphy_recal) {
				tx_idx = ph->sslpnphy_last_cal_tx_idx;
				tia_gain = ph->sslpnphy_last_cal_tia_gain;
				lna2_gain = ph->sslpnphy_last_cal_lna2_gain;
				vga_gain = ph->sslpnphy_last_cal_vga_gain;
			}
			while ((tx_idx > tx_idx_low_lmt) && ((tx_idx - 8) > 0)) {
				tx_idx -= 8;
				wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_idx);
				wlc_sslpnphy_disable_pad(pi);

				wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, 7, 3, 0);

				if (!(wlc_sslpnphy_rx_iq_est(pi, 0xffff, 32, &iq_est)))
					break;
				pwr = iq_est.i_pwr + iq_est.q_pwr;

				if (pwr > MAX_IQ_PWR_LMT) {
					tx_idx += 40;
					if (tx_idx > 127) {
						tx_idx = 127;
						wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_idx);
						wlc_sslpnphy_disable_pad(pi);

						break;
					}
				} else if (pwr > RX_PWR_THRSH_MIN) {
					break;
				}
			}
			while (((tia_gain > 0) || (lna2_gain > 1)) && (vga_gain < 10)) {
				if (!vga_gain) {
					if (tia_gain != 0)
						tia_gain--;
					else if (tia_gain == 0)
						lna2_gain--;
				}

				wlc_sslpnphy_set_rx_gain_by_distribution(pi, vga_gain, 0, 0, 0,
					tia_gain, lna2_gain, 0);

				if (!(wlc_sslpnphy_rx_iq_est(pi, 0xffff, 32, &iq_est)))
					break;
				pwr = iq_est.i_pwr + iq_est.q_pwr;

				if (pwr < RX_PWR_THRSH_MIN)
					vga_gain++;
				else if (pwr < RX_PWR_THRSH_MAX)
					break;
			}
			wlc_sslpnphy_calc_rx_iq_comp(pi, 0xffff);
			wlc_sslpnphy_stop_tx_tone(pi);
#endif /* BAND5G */
		}
		ph->sslpnphy_last_cal_tx_idx = (int8) tx_idx;
		ph->sslpnphy_last_cal_tia_gain = tia_gain;
		ph->sslpnphy_last_cal_lna2_gain = lna2_gain;
		ph->sslpnphy_last_cal_vga_gain = vga_gain;
	}

	/* Restore TR switch */
	wlc_sslpnphy_clear_trsw_override(pi);

	/* Restore PA */
	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
		SSLPNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
		((0 << SSLPNPHY_RFOverride0_gmode_tx_pu_ovr_SHIFT) |
		(0 << SSLPNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT)));
	mod_phy_reg(pi, SSLPNPHY_rfoverride3,
		SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK |
		SSLPNPHY_rfoverride3_stxpapu_ovr_MASK,
		((0 << SSLPNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT) |
		(0 << SSLPNPHY_rfoverride3_stxpapu_ovr_SHIFT)));

	/* Restore Tx gain */
	if (!use_noise) {
		if (tx_gain_override_old) {
			wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_gain_index_old);
		} else
			wlc_sslpnphy_disable_tx_gain_override(pi);
	}
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			write_phy_reg(pi, SSLPNPHY_papd_control, papd_ctrl_old);
			write_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, sslpnCalibClkEnCtrl_old);
			write_phy_reg(pi, SSLPNPHY_Core1TxControl, Core1TxControl_old);
		}
	}
#endif
	/* Clear various overrides */
	wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);

	mod_phy_reg(pi, SSLPNPHY_rfoverride2,
		SSLPNPHY_rfoverride2_slna_pu_ovr_MASK |
		SSLPNPHY_rfoverride2_lna_pu_ovr_MASK |
		SSLPNPHY_rfoverride2_ps_ctrl_ovr_MASK,
		((0 << SSLPNPHY_rfoverride2_slna_pu_ovr_SHIFT) |
		(0 << SSLPNPHY_rfoverride2_lna_pu_ovr_SHIFT) |
		(0 << SSLPNPHY_rfoverride2_ps_ctrl_ovr_SHIFT)));

	mod_phy_reg(pi, SSLPNPHY_RFinputOverride,
		SSLPNPHY_RFinputOverride_wlslnapu_ovr_MASK,
		0 << SSLPNPHY_RFinputOverride_wlslnapu_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
		0 << SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
		0 << SSLPNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);

cal_done:
	PHY_INFORM(("wl%d: %s: Rx IQ cal complete, coeffs: A0: %d, B0: %d\n",
		pi->sh->unit, __FUNCTION__,
		(int16)((read_phy_reg(pi, SSLPNPHY_RxCompcoeffa0) & SSLPNPHY_RxCompcoeffa0_a0_MASK)
		>> SSLPNPHY_RxCompcoeffa0_a0_SHIFT),
		(int16)((read_phy_reg(pi, SSLPNPHY_RxCompcoeffb0) & SSLPNPHY_RxCompcoeffb0_b0_MASK)
		>> SSLPNPHY_RxCompcoeffb0_b0_SHIFT)));

	return result;
}

static int
wlc_sslpnphy_wait_phy_reg(phy_info_t *pi, uint16 addr,
    uint32 val, uint32 mask, int shift,
    int timeout_us)
{
	int timer_us, done;

	for (timer_us = 0, done = 0; (timer_us < timeout_us) && (!done);
		timer_us = timer_us + 1) {

	  /* wait for poll interval in units of microseconds */
	  OSL_DELAY(1);

	  /* check if the current field value is same as the required value */
	  if (val == (uint32)(((read_phy_reg(pi, addr)) & mask) >> shift)) {
	    done = 1;
	  }
	}
	return done;
}

static int
wlc_sslpnphy_aux_adc_accum(phy_info_t *pi, uint32 numberOfSamples,
    uint32 waitTime, int32 *sum, int32 *prod)
{
	uint32 save_pwdn_rssi_ovr, term0, term1;
	int done;

	save_pwdn_rssi_ovr = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvr) &
		SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK;


	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
		SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK,
		1 << SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
		SSLPNPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_MASK,
		0 << SSLPNPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_SHIFT);

	/* clear accumulators */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
		1 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
		0 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);


	mod_phy_reg(pi, SSLPNPHY_NumrssiSamples,
		SSLPNPHY_NumrssiSamples_numrssisamples_MASK,
		numberOfSamples << SSLPNPHY_NumrssiSamples_numrssisamples_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_rssiwaittime,
		SSLPNPHY_rssiwaittime_rssiwaittimeValue_MASK,
		100 << SSLPNPHY_rssiwaittime_rssiwaittimeValue_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
		SSLPNPHY_auxadcCtrl_rssiestStart_MASK,
		1 << SSLPNPHY_auxadcCtrl_rssiestStart_SHIFT);

	done = wlc_sslpnphy_wait_phy_reg(pi, SSLPNPHY_auxadcCtrl, 0,
		SSLPNPHY_auxadcCtrl_rssiestStart_MASK,
		SSLPNPHY_auxadcCtrl_rssiestStart_SHIFT,
		1000);

	if (done) {
		term0 = read_phy_reg(pi, SSLPNPHY_rssiaccValResult0);
		term0 = (term0 & SSLPNPHY_rssiaccValResult0_rssiaccResult0_MASK);
		term1 = read_phy_reg(pi, SSLPNPHY_rssiaccValResult1);
		term1 = (term1 & SSLPNPHY_rssiaccValResult1_rssiaccResult1_MASK);
		*sum = (term1 << 16) + term0;
		term0 = read_phy_reg(pi, SSLPNPHY_rssiprodValResult0);
		term0 = (term0 & SSLPNPHY_rssiprodValResult0_rssiProdResult0_MASK);
		term1 = read_phy_reg(pi, SSLPNPHY_rssiprodValResult1);
		term1 = (term1 & SSLPNPHY_rssiprodValResult1_rssiProdResult1_MASK);
		*prod = (term1 << 16) + term0;
	}
	else {
		*sum = 0;
		*prod = 0;
	}

	/* restore result */
	mod_phy_reg(pi, (uint16)SSLPNPHY_AfeCtrlOvr,
		(uint16)SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK,
		(uint16)save_pwdn_rssi_ovr);

	return done;
}

static int32
wlc_sslpnphy_vbatsense(phy_info_t *pi)
{
	uint32 save_rssi_settings, save_rssiformat;
	uint16 sslpnCalibClkEnCtr;
	uint32 savemux;
	int32 sum, prod, x, voltage;
	uint32 save_reg0 = 0, save_reg5 = 0;
	uint16 save_iqcal_ctrl_2;

#define numsamps 40	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 26214	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

#define c1 (int16)((0.0580833 * (1<<19)) + 0.5)	/* polynomial coefficient in q.19 format */
#define qc1 19									/* qformat of c1 */
#define c0 (int16)((3.9591333 * (1<<13)) + 0.5) 	/* polynomial coefficient in q.14 format */
#define qc0 13									/* qformat of c0 */

	if (!(SSLPNREV_IS(pi->pubpi.phy_rev, 3))) {
		save_reg0 = si_pmu_regcontrol(pi->sh->sih, 0, 0, 0);
		save_reg5 = si_pmu_regcontrol(pi->sh->sih, 5, 0, 0);
		si_pmu_regcontrol(pi->sh->sih, 0, 1, 1);
		si_pmu_regcontrol(pi->sh->sih, 5, (1 << 31), (1 << 31));
	}

	sslpnCalibClkEnCtr = read_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
	            SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_MASK,
	            1 << SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_SHIFT);

	save_rssi_settings = read_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1);
	save_rssiformat = read_phy_reg(pi, SSLPNPHY_auxadcCtrl) &
	        SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK;


	/* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl, SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK,
	            1<<SSLPNPHY_auxadcCtrl_rssiformatConvEn_SHIFT);


	/* slpinv_rssi */
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (1<<13), (0<<13));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0xf<<0), (13<<0));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0xf<<4), (8<<4));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0x7<<10), (4<<10));

	/* set powerdetector before PA and rssi mux to tempsense */
	savemux = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal) &
	        SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK;
	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
	            SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
	            4 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);

	/* set iqcal mux to select VBAT */
	save_iqcal_ctrl_2 = read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
	mod_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (0xF<<0), (0x4<<0));

	/* reset auxadc */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
	            SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	            1<<SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
	            SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	            0<<SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum, &prod);

	/* restore rssi settings */
	write_phy_reg(pi, (uint16)SSLPNPHY_AfeRSSICtrl1, (uint16)save_rssi_settings);
	mod_phy_reg(pi, (uint16)SSLPNPHY_auxadcCtrl,
	            (uint16)SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK,
	            (uint16)save_rssiformat);
	mod_phy_reg(pi, (uint16)SSLPNPHY_AfeCtrlOvrVal,
	            (uint16)SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
	            (uint16)savemux);

	write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, save_iqcal_ctrl_2);
	write_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, sslpnCalibClkEnCtr);
	/* sum = sum/numsamps in qsum=0+qone_by_numsamps format
	 * as the accumulated values are always less than 200, 6 bit values, the
	 * sum always fits into 16 bits
	 */
	x = qm_mul321616((int16)sum, one_by_numsamps);

	/* compute voltagte = c1 * sum + co */
	voltage = qm_mul323216(x, c1); /* volatage in q.qone_by_numsamps+qc1-16 format */

	/* bring sum to qc0 format */
	voltage = voltage >> (qone_by_numsamps + qc1 - 16 - qc0);

	/* comute c1*x + c0 */
	voltage = voltage + c0;

	/* bring voltage to q.4 format */
	voltage = voltage >> (qc0 - 4);

	if (!(SSLPNREV_IS(pi->pubpi.phy_rev, 3))) {
		si_pmu_regcontrol(pi->sh->sih, 0, ~0, save_reg0);
		si_pmu_regcontrol(pi->sh->sih, 5, ~0, save_reg5);
	}

	return voltage;

#undef numsamps
#undef one_by_numsamps
#undef qone_by_numsamps
#undef c1
#undef qc1
#undef c0
#undef qc0
}

static int
wlc_sslpnphy_tempsense(phy_info_t *pi)
{
	uint32 save_rssi_settings, save_rssiformat;
	uint16  sslpnCalibClkEnCtr;
	uint32 rcalvalue, savemux;
	int32 sum0, prod0, sum1, prod1, sum;
	int32 temp32 = 0;
	bool suspend;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return 0;

#define m 461.5465

	  /* b = -12.0992 in q11 format */
#define b ((int16)(-12.0992*(1<<11)))
#define qb 11

	  /* thousand_by_m = 1000/m = 1000/461.5465 in q13 format */
#define thousand_by_m ((int16)((1000/m)*(1<<13)))
#define qthousand_by_m 13

#define numsamps 400	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 2621	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

	/* suspend the mac if it is not already suspended */
	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	sslpnCalibClkEnCtr = read_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_txFrontEndCalibClkEn_SHIFT);

	save_rssi_settings = read_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1);
	save_rssiformat = read_phy_reg(pi, SSLPNPHY_auxadcCtrl) &
		SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK;

	/* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
	      SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK,
	      1 << SSLPNPHY_auxadcCtrl_rssiformatConvEn_SHIFT);

	/* slpinv_rssi */
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (1<<13), (0<<13));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0xf<<0), (0<<0));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0xf<<4), (11<<4));
	mod_phy_reg(pi, SSLPNPHY_AfeRSSICtrl1, (0x7<<10), (5<<10));

	/* read the rcal value */
	rcalvalue = read_radio_reg(pi, RADIO_2063_COMMON_13) & 0xf;

	/* set powerdetector before PA and rssi mux to tempsense */
	savemux = read_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal) &
	    SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK;

	mod_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
	      SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
	      5 << SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_SHIFT);

	/* reset auxadc */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
	     SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	     1 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl,
	     SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	     0 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	/* set rcal override */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<7), (1<<7));

	/* power up temp sense */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<6), (0<<6));

	/* set temp sense mux */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<5), (0<<5));

	/* set rcal value */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (0xf<<0), (rcalvalue<<0));

	/* set TPSENSE_swap to 0 */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<4), (0<<4));

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum0, &prod0);

	/* reset auxadc */
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl, SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	      1 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);
	mod_phy_reg(pi, SSLPNPHY_auxadcCtrl, SSLPNPHY_auxadcCtrl_auxadcreset_MASK,
	      0 << SSLPNPHY_auxadcCtrl_auxadcreset_SHIFT);

	/* set TPSENSE swap to 1 */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<4), (1<<4));

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum1, &prod1);

	sum = (sum0 + sum1) >> 1;

	/* restore rssi settings */
	write_phy_reg(pi, (uint16)SSLPNPHY_AfeRSSICtrl1, (uint16)save_rssi_settings);
	mod_phy_reg(pi, (uint16)SSLPNPHY_auxadcCtrl,
	      (uint16)SSLPNPHY_auxadcCtrl_rssiformatConvEn_MASK,
	      (uint16)save_rssiformat);
	mod_phy_reg(pi, (uint16)SSLPNPHY_AfeCtrlOvrVal,
	      (uint16)SSLPNPHY_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
	      (uint16)savemux);

	/* powerdown tempsense */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<6), (1<<6));

	/* sum = sum/numsamps in qsum=0+qone_by_numsamps format
	 *as the accumulated values are always less than 200, 6 bit values, the
	 *sum always fits into 16 bits
	 */

	sum = qm_mul321616((int16)sum, one_by_numsamps);

	/* bring sum into qb format */
	sum = sum >> (qone_by_numsamps-qb);

	/* sum-b in qb format */
	temp32 = sum - b;

	/* calculate (sum-b)*1000/m in qb+qthousand_by_m-15=11+13-16 format */
	temp32 = qm_mul323216(temp32, (int16)thousand_by_m);

	/* bring temp32 into q0 format */
	temp32 = (temp32+(1<<7)) >> 8;

	write_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, sslpnCalibClkEnCtr);

	/* enable the mac if it is suspended by tempsense function */
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	ph->sslpnphy_lastsensed_temperature = (int8)temp32;
	return temp32;
#undef m
#undef b
#undef qb
#undef thousand_by_m
#undef qthousand_by_m
#undef numsamps
#undef one_by_numsamps
#undef qone_by_numsamps
}

static void
wlc_sslpnphy_temp_adj_offset(phy_info_t *pi, int8 temp_adj)
{
	uint32 tableBuffer[2];
	uint8 phybw40;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	/* adjust the reference ofdm gain index table offset */
	mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
		SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_MASK,
		((ph->sslpnphy_ofdmgainidxtableoffset +  temp_adj) <<
		 SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_SHIFT));

	/* adjust the reference dsss gain index table offset */
	mod_phy_reg(pi, SSLPNPHY_gainidxoffset,
		SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_MASK,
		((ph->sslpnphy_dsssgainidxtableoffset +  temp_adj) <<
		 SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT));

	/* adjust the reference gain_val_tbl at index 64 and 65 in gain_val_tbl */
	tableBuffer[0] = ph->sslpnphy_tr_R_gain_val + temp_adj;
	tableBuffer[1] = ph->sslpnphy_tr_T_gain_val + temp_adj;

	wlc_sslpnphy_common_write_table(pi, 17, tableBuffer, 2, 32, 64);

	if (phybw40 == 0) {
		mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
			SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			((ph->sslpnphy_input_pwr_offset_db + temp_adj) <<
			SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
	        mod_phy_reg(pi, SSLPNPHY_LowGainDB,
	                SSLPNPHY_LowGainDB_MedLowGainDB_MASK,
	                ((ph->sslpnphy_Med_Low_Gain_db + temp_adj) <<
	                SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT));
	        mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
	                SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
	                ((ph->sslpnphy_Very_Low_Gain_db + temp_adj) <<
	                SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT));
	} else {
		mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
			SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
			((ph->sslpnphy_input_pwr_offset_db + temp_adj) <<
			SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT));
	        mod_phy_reg(pi, SSLPNPHY_Rev2_LowGainDB_40,
	                SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_MASK,
	                ((ph->sslpnphy_Med_Low_Gain_db + temp_adj) <<
	                SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_SHIFT));
	        mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
	                SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK,
	                ((ph->sslpnphy_Very_Low_Gain_db + temp_adj) <<
	                SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT));
	}
}

static uint8 chan_spec_spur_nokref_38p4Mhz [][3] = {
	{1, 5, 23},
	{2, 5, 7},
	{3, 5, 55},
	{4, 5, 39},
	{9, 3, 18},
	{10, 2, 2},
	{10, 3, 22},
	{11, 2, 50},
	{11, 3, 6},
	{12, 3, 54},
	{13, 3, 38},
	{13, 3, 26}
};
uint8 chan_spec_spur_nokref_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_nokref_38p4Mhz);
static uint8 chan_spec_spur_tdkmdl_38p4Mhz [][3] = {
	{1, 3, 23},
	{2, 3, 7},
	{3, 3, 55},
	{4, 3, 39},
	{13, 3, 26}
};
uint8 chan_spec_spur_tdkmdl_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_tdkmdl_38p4Mhz);
static uint8 chan_spec_spur_26Mhz [][3] = {
	{1, 3, 19},
	{2, 5, 3},
	{3, 4, 51},
	{11, 3, 26},
	{12, 3, 10},
	{13, 3, 58}
};
uint8 chan_spec_spur_26Mhz_sz = ARRAYSIZE(chan_spec_spur_26Mhz);
static uint8 chan_spec_spur_37p4Mhz [][3] = {
	{4, 3, 13},
	{5, 3, 61},
	{6, 3, 45},
	{11, 3, 20},
	{12, 3, 4},
	{13, 3, 52}
};
uint8 chan_spec_spur_37p4Mhz_sz = ARRAYSIZE(chan_spec_spur_37p4Mhz);
static uint8 chan_spec_spur_xtlna38p4Mhz [][3] = {
	{7, 2, 57},
	{7, 2, 58},
	{11, 2, 6},
	{11, 2, 7},
	{13, 2, 25},
	{13, 2, 26},
	{13, 2, 38},
	{13, 2, 39}
};
uint8 chan_spec_spur_xtlna38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna38p4Mhz);
static uint8 chan_spec_spur_xtlna26Mhz [][3] = {
	{1, 2, 19},
	{2, 2, 3},
	{3, 2, 51},
	{11, 2, 26},
	{12, 2, 10},
	{13, 2, 58}
};
uint8 chan_spec_spur_xtlna26Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna26Mhz);
static uint8 chan_spec_spur_xtlna37p4Mhz [][3] = {
	{4, 2, 13},
	{5, 2, 61},
	{6, 2, 45}
};
uint8 chan_spec_spur_xtlna37p4Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna37p4Mhz);
static uint8 chan_spec_spur_rev2_26Mhz [][3] = {
	{1, 3, 147},
	{2, 3, 131},
	{3, 3, 179},
	{3, 3, 115},
	{4, 3, 99},
	{5, 3, 83},
	{5, 3, 38},
	{6, 3, 150},
	{6, 3, 22},
	{7, 3, 134},
	{7, 3, 6},
	{8, 3, 182},
	{8, 3, 118},
	{9, 3, 166},
	{9, 3, 102},
	{9, 3, 58},
	{10, 3, 42},
	{10, 3, 86},
	{11, 3, 154},
	{11, 3, 26},
	{11, 3, 70},
	{12, 3, 138},
	{13, 3, 186}
};
uint8 chan_spec_spur_rev2_26Mhz_sz = ARRAYSIZE(chan_spec_spur_rev2_26Mhz);
static uint8 chan_spec_spur_85degc_38p4Mhz [][3] = {
	{0, 1, 2},
	{0, 1, 22},
	{0, 1, 6},
	{0, 1, 54},
	{0, 1, 26},
	{0, 1, 38}
};
uint8 chan_spec_spur_85degc_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_85degc_38p4Mhz);

static void
wlc_sslpnphy_chanspec_spur_weight(phy_info_t *pi, uint channel, uint8 ptr[][3], uint8 array_size)
{
	uint8 i = 0;
	for (i = 0; i < array_size; i++) {
		if (ptr[i][0] == channel) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				&ptr[i][1], 1, 8, ptr[i][2]);
		}
	}
}

static void
wlc_sslpnphy_temp_adj(phy_info_t *pi)
{
	int32 temperature;
	uint32 tableBuffer[2];
	uint freq;
	int16 thresh1, thresh2, thresh3;
	uint8 i;
	uint16 minsig = 0x01bc;
	uint8 spur_weight;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		thresh1 = -30;
		thresh2 = 15;
		thresh3 = 55;
	} else {
		thresh1 = -23;
		thresh2 = 4;
		thresh3 = 60;
	}
	temperature = ph->sslpnphy_lastsensed_temperature;
	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((temperature - 15) <= thresh1) {
		wlc_sslpnphy_temp_adj_offset(pi, 6);
		ph->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) + 30) * 286) >> 12) - 0;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 26);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 28);
			if (!(pi->sh->boardflags & BFL_EXTLNA)) {
				tableBuffer[0] = 0xd1a4099c;
				tableBuffer[1] = 0xd1a40018;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 52);
				tableBuffer[0] = 0xf1e64d96;
				tableBuffer[1] = 0xf1e60018;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 54);
				if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
					if ((ph->sslpnphy_fabid == 2) ||
						(ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
						tableBuffer[0] = 0x204ca9e;
						tableBuffer[1] = 0x2040019;
						wlc_sslpnphy_common_write_table(pi,
							SSLPNPHY_TBL_ID_GAIN_IDX, tableBuffer, 2, 32, 56);
						tableBuffer[0] = 0xa246cea1;
						tableBuffer[1] = 0xa246001c;
						wlc_sslpnphy_common_write_table(pi,
							SSLPNPHY_TBL_ID_GAIN_IDX, tableBuffer, 2, 32, 132);
					}
				}
				write_phy_reg(pi, SSLPNPHY_gainBackOffVal, 0x6666);
				mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
					SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
					31 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
				write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);
				mod_phy_reg(pi, SSLPNPHY_PwrThresh1,
					SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_MASK,
					15 << SSLPNPHY_PwrThresh1_PktRxSignalDropThresh_SHIFT);

				if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(1 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						8 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(0 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);
				}
			} else {
				if (!SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
					mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
						SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
						30 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(4 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					if ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID)) {
						for (i = 0; i < 64; i++)
							wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
					}
				}
			}
		}
	} else if ((temperature - 15) < thresh2) {
		wlc_sslpnphy_temp_adj_offset(pi, 3);
		ph->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) - 4) * 286) >> 12) - 0;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tableBuffer[0] = ph->sslpnphy_gain_idx_14_lowword;
			tableBuffer[1] = ph->sslpnphy_gain_idx_14_hiword;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 28);
			if (!(pi->sh->boardflags & BFL_EXTLNA)) {
				/* Added To Increase The 1Mbps Sense for Temps @Around */
				/* -15C Temp With CmRxAciGainTbl */
				tableBuffer[0] = ph->sslpnphy_gain_idx_27_lowword;
				tableBuffer[1] = ph->sslpnphy_gain_idx_27_hiword;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 54);
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
					if (freq <= 2427) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							253 <<
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					} else if (freq < 2472) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							0 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					} else {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							254 <<
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					}
					if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID)
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							0 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
						SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
						27 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
				} else if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
					for (i = 63; i <= 73; i++) {
						wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
							tableBuffer, 2, 32, ((i - 37) *2));
						wlc_sslpnphy_common_write_table(pi,
							SSLPNPHY_TBL_ID_GAIN_IDX,
							tableBuffer, 2, 32, (i * 2));
					}
					write_phy_reg(pi, SSLPNPHY_slnanoisetblreg2, 0x03F0);
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(2 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
				}
			} else {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
					mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
						SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
						4 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						5 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(2 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
						SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
						27 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
					if ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID) ||
						(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID)) {
						for (i = 0; i < 64; i++)
							wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
					}
				}
			}
		}
	} else if ((temperature - 15) < thresh3) {
		wlc_sslpnphy_temp_adj_offset(pi, 0);
		ph->sslpnphy_pkteng_rssi_slope = (((temperature - 15) - 25) * 286) >> 12;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tableBuffer[0] = ph->sslpnphy_gain_idx_14_lowword;
			tableBuffer[1] = ph->sslpnphy_gain_idx_14_hiword;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 28);
			if (((temperature) >= 50) && ((temperature) < (thresh3 + 15))) {
				if (!(pi->sh->boardflags & BFL_EXTLNA)) {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						((freq <= 2427) ? 255 : 0 <<
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
				 } else {
					if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						5 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
						SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
						4 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						5 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
					} else {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						3 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						7 << SSLPNPHY_Rev2_PwrThresh0_SlowPwrLoThresh_SHIFT);
					}
				}
			}
		}
	} else {
		wlc_sslpnphy_temp_adj_offset(pi, -3);
		ph->sslpnphy_pkteng_rssi_slope = ((((temperature - 10) - 55) * 286) >> 12) - 2;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (!SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);
			mod_phy_reg(pi, SSLPNPHY_radioTRCtrlCrs1,
				SSLPNPHY_radioTRCtrlCrs1_trGainThresh_MASK,
				23 << SSLPNPHY_radioTRCtrlCrs1_trGainThresh_SHIFT);
			}
			if (!(pi->sh->boardflags & BFL_EXTLNA)) {
				write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);
				tableBuffer[0] = 0xd0008206;
				tableBuffer[1] = 0xd0000009;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 28);
				if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
					tableBuffer[0] = 0x41a4099c;
					tableBuffer[1] = 0x41a4001d;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
						tableBuffer, 2, 32, 52);
					tableBuffer[0] = 0x51e64d96;
					tableBuffer[1] = 0x51e6001d;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
						tableBuffer, 2, 32, 54);
				}
				if (pi->xtalfreq == 38400000) {
					/* Special Tuning As The 38.4Mhz Xtal Boards */
					/* SpurProfile Changes Drastically At Very High */
					/* Temp(Especially @85degC) */
					wlc_sslpnphy_chanspec_spur_weight(pi, 0,
						chan_spec_spur_85degc_38p4Mhz,
						chan_spec_spur_85degc_38p4Mhz_sz);
					if (freq == 2452 || freq == 2462) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
						mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh0,
							SSLPNPHY_dsssPwrThresh0_dsssPwrThresh0_MASK,
							21 << SSLPNPHY_dsssPwrThresh0_dsssPwrThresh0_SHIFT);
						spur_weight = 4;
						wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
							&spur_weight, 1, 8, ((freq == 2452) ? 18 : 50));
						if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID) {
							mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
								SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
								(254 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
							spur_weight = 2;
							wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
								&spur_weight, 1, 8, ((freq == 2452) ? 18 : 50));
						}
					} else if (freq == 2467) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
						if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(254 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
						}
					} else {
					if (freq >= 2472)
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					else
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(254 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					}
				} else if (pi->xtalfreq == 26000000) {
					if (freq <= 2467) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							254 <<
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					} else if ((freq > 2467) && (freq <= 2484)) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							253 <<
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					}
				} else {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
				}
			} else {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
					if (freq <= 2427) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					} else if ((freq > 2427) && (freq <= 2467)) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(254 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					} else if ((freq > 2467) && (freq <= 2484)) {
						mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
							SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
							(253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					}
				} else {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						(0 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
					if (BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN18_SSID)
						write_phy_reg(pi, SSLPNPHY_ClipCtrDefThresh, 12);
				}
			}
		}
	}
	wlc_sslpnphy_RxNvParam_Adj(pi);

	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);

	wlc_sslpnphy_txpower_recalc_target(pi);
}

void
wlc_sslpnphy_periodic_cal_top(phy_info_t *pi)
{

	bool full_cal, suspend;
	int current_temperature;
	uint cal_done = 0;
	int temp_drift, temp_drift1;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (NORADIO_ENAB(pi->pubpi))
		return;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	wlc_sslpnphy_btcx_override_enable(pi);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	full_cal = (ph->sslpnphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec));
	ph->sslpnphy_full_cal_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	ph->sslpnphy_recal = (full_cal ? 0 : 1);
	current_temperature = wlc_sslpnphy_tempsense(pi);
	ph->sslpnphy_cur_idx = (int)((read_phy_reg(pi, 0x4ab) & 0x7f00) >> 8);
	ph->sslpnphy_percal_ctr++;

	if ((ph->sslpnphy_force_1_idxcal == 0) && (ph->sslpnphy_force_percal == 0)) {
		temp_drift = current_temperature - ph->sslpnphy_last_cal_temperature;
		temp_drift1 = current_temperature - ph->sslpnphy_last_full_cal_temperature;

		/* Temperature change of 25 degrees or at an interval of 20 minutes do a full cal */
		if ((temp_drift1 < - 25) || (temp_drift1 > 25) ||
			(ph->sslpnphy_percal_ctr == 100)) {
			PHY_TRACE(("wl%d: %s : periodic cal\n", pi->sh->unit, __FUNCTION__));
			wlc_phy_radio_2063_vco_cal(pi);
			wlc_sslpnphy_periodic_cal(pi);
			wlc_sslpnphy_tx_pwr_ctrl_init(pi);
			wlc_sslpnphy_temp_adj(pi);
			ph->sslpnphy_percal_ctr = 0;
			ph->sslpnphy_last_full_cal_temperature = (int8)current_temperature;
			cal_done = 1;
		}

		if (((temp_drift > 10) || (temp_drift < -10)) && (cal_done == 0)) {
			wlc_sslpnphy_tx_pwr_ctrl_init(pi);
			ph->sslpnphy_force_1_idxcal = 1;
			ph->sslpnphy_papd_nxt_cal_idx = ph->sslpnphy_final_idx -
				((current_temperature - ph->sslpnphy_last_cal_temperature) / 3);
			wlc_sslpnphy_papd_recal(pi);
			wlc_sslpnphy_temp_adj(pi);
			cal_done = 1;
		}
	}

	if (ph->sslpnphy_force_1_idxcal) {
		ph->sslpnphy_last_cal_temperature = (int8)current_temperature;
		ph->sslpnphy_force_1_idxcal = 0;
	}

	ph->sslpnphy_recal = 0;
	wlc_phy_txpower_recalc_target(pi);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

}

void
wlc_sslpnphy_periodic_cal(phy_info_t *pi)
{
	bool suspend;
	uint16 tx_pwr_ctrl;
	const sslpnphy_rx_iqcomp_t *rx_iqcomp;
	int rx_iqcomp_sz;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	ph->sslpnphy_full_cal_channel = CHSPEC_CHANNEL(pi->radio_chanspec);

	if (NORADIO_ENAB(pi->pubpi))
		return;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	wlc_sslpnphy_btcx_override_enable(pi);


	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Tx iqlo calibration */
	wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);

	/* Restore tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);


	/* Rx iq calibration */

	rx_iqcomp = sslpnphy_rx_iqcomp_table_rev0;
	rx_iqcomp_sz = ARRAYSIZE(sslpnphy_rx_iqcomp_table_rev0);

	wlc_sslpnphy_deaf_mode(pi, TRUE);
	wlc_sslpnphy_rx_iq_cal(pi,
		NULL,
		0,
		FALSE, TRUE, FALSE, TRUE, 127);
	wlc_sslpnphy_deaf_mode(pi, FALSE);

	wlc_sslpnphy_papd_recal(pi);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
	return;
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_sslpnphy_full_cal(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_sslpnphy_deaf_mode(pi, TRUE);

	/* Force full calibration run */
	ph->sslpnphy_full_cal_channel = 0;

	/* Run sslpnphy cals */
	wlc_sslpnphy_periodic_cal(pi);

	wlc_sslpnphy_deaf_mode(pi, FALSE);
	return;
}
#endif 

#if defined(WLTEST)
void
wlc_phy_carrier_suppress_sslpnphy(phy_info_t *pi)
{
	if (ISSSLPNPHY(pi)) {
		wlc_sslpnphy_reset_radio_loft(pi);
		if (wlc_phy_tpc_isenabled_sslpnphy(pi))
			wlc_sslpnphy_clear_tx_power_offsets(pi);
		else
			wlc_sslpnphy_set_tx_locc(pi, 0);
	} else
		ASSERT(0);

}

static uint32
wlc_sslpnphy_measure_digital_power(phy_info_t *pi, uint16 nsamples)
{
	sslpnphy_iq_est_t iq_est = {0, 0, 0};

	if (!wlc_sslpnphy_rx_iq_est(pi, nsamples, 32, &iq_est))
	    return 0;

	return (iq_est.i_pwr + iq_est.q_pwr) / nsamples;
}

static uint32
wlc_sslpnphy_get_receive_power(phy_info_t *pi, int32 *gain_index)
{
	uint32 received_power = 0;
	int32 max_index = 0;
	uint32 gain_code = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	max_index = 36;
	if (*gain_index >= 0)
		gain_code = sslpnphy_gaincode_table[*gain_index];

	/* wlc_sslpnphy_set_deaf(pi); */
	if (-1 == *gain_index) {
		*gain_index = 0;
		while ((*gain_index <= (int32)max_index) && (received_power < 700)) {
			wlc_sslpnphy_set_rx_gain(pi, sslpnphy_gaincode_table[*gain_index]);
			received_power =
				wlc_sslpnphy_measure_digital_power(pi, ph->sslpnphy_noise_samples);
			(*gain_index) ++;
		}
		(*gain_index) --;
	}
	else {
		wlc_sslpnphy_set_rx_gain(pi, gain_code);
		received_power = wlc_sslpnphy_measure_digital_power(pi, ph->sslpnphy_noise_samples);
	}

	/* wlc_sslpnphy_clear_deaf(pi); */
	return received_power;
}

int32
wlc_sslpnphy_rx_signal_power(phy_info_t *pi, int32 gain_index)
{
	uint32 gain = 0;
	uint32 nominal_power_db;
	uint32 log_val, gain_mismatch, desired_gain, input_power_offset_db, input_power_db;
	int32 received_power, temperature;
	uint freq;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	received_power = wlc_sslpnphy_get_receive_power(pi, &gain_index);

	gain = sslpnphy_gain_table[gain_index];

	nominal_power_db = read_phy_reg(pi, SSLPNPHY_VeryLowGainDB) >>
	                       SSLPNPHY_VeryLowGainDB_NominalPwrDB_SHIFT;

	{
		uint32 power = (received_power*16);
		uint32 msb1, msb2, val1, val2, diff1, diff2;
		msb1 = find_msbit(power);
		msb2 = msb1 + 1;
		val1 = 1 << msb1;
		val2 = 1 << msb2;
		diff1 = (power - val1);
		diff2 = (val2 - power);
		if (diff1 < diff2)
			log_val = msb1;
		else
			log_val = msb2;
	}

	log_val = log_val * 3;

	gain_mismatch = (nominal_power_db/2) - (log_val);

	desired_gain = gain + gain_mismatch;

	input_power_offset_db = read_phy_reg(pi, SSLPNPHY_InputPowerDB) & 0xFF;

	if (input_power_offset_db > 127)
		input_power_offset_db -= 256;

	input_power_db = input_power_offset_db - desired_gain;

	if (!(SSLPNREV_IS(pi->pubpi.phy_rev, 3))) {
		/* compensation from PHY team */
		/* includes path loss of 2dB from Murata connector to chip input */
		input_power_db = input_power_db + sslpnphy_gain_index_offset_for_rssi[gain_index];
		/* Channel Correction Factor */
		freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
		if ((freq > 2427) && (freq <= 2467))
			input_power_db = input_power_db - 1;

		/* temperature correction */
		temperature = ph->sslpnphy_lastsensed_temperature;
		/* printf(" CW_RSSI Temp %d \n",temperature); */
		if ((temperature - 15) < -30) {
			input_power_db = input_power_db +
				(((temperature - 10 - 25) * 286) >> 12) - 7;
		} else if ((temperature - 15) < 4) {
			input_power_db = input_power_db +
				(((temperature - 10 - 25) * 286) >> 12) - 3;
		} else {
			input_power_db = input_power_db +
				(((temperature - 10 - 25) * 286) >> 12);
		}
	}

	wlc_sslpnphy_rx_gain_override_enable(pi, 0);

	return input_power_db;
}
#endif 

void
wlc_sslpnphy_get_tssi(phy_info_t *pi, int8 *ofdm_pwr, int8 *cck_pwr)
{
	int8 cck_offset;
	uint16 status;

	if (wlc_sslpnphy_tssi_enabled(pi) &&
		((status = (read_phy_reg(pi, SSLPNPHY_TxPwrCtrlStatus))) &
		SSLPNPHY_TxPwrCtrlStatus_estPwrValid_MASK)) {
		*ofdm_pwr = (int8)((status &
			SSLPNPHY_TxPwrCtrlStatus_estPwr_MASK) >>
			SSLPNPHY_TxPwrCtrlStatus_estPwr_SHIFT);
		if (wlc_phy_tpc_isenabled_sslpnphy(pi))
			cck_offset = pi->tx_power_offset[TXP_FIRST_CCK];
		else
			cck_offset = 0;
		*cck_pwr = *ofdm_pwr + cck_offset;
	} else {
		*ofdm_pwr = 0;
		*cck_pwr = 0;
	}
}

static void
wlc_sslpnphy_rx_offset_init(phy_info_t *pi)
{
	int16 temp;
	uint8 phybw40;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (phybw40 == 0) {
		temp = (int16)(read_phy_reg(pi, SSLPNPHY_InputPowerDB)
					& SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK);
	} else {
	        temp = (int16)(read_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40)
	                & SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK);
	}
	if (temp > 127)
		temp -= 256;
	ph->sslpnphy_input_pwr_offset_db = (int8)temp;
}

static void
wlc_sslpnphy_agc_temp_init(phy_info_t *pi)
{
	int16 temp;
	phytbl_info_t tab;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint32 tableBuffer[2];

	/* reference ofdm gain index table offset */
	temp = (int16) read_phy_reg(pi, SSLPNPHY_gainidxoffset);
	ph->sslpnphy_ofdmgainidxtableoffset =
	    (temp & SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_MASK) >>
	    SSLPNPHY_gainidxoffset_ofdmgainidxtableoffset_SHIFT;

	if (ph->sslpnphy_ofdmgainidxtableoffset > 127) ph->sslpnphy_ofdmgainidxtableoffset -= 256;

	/* reference dsss gain index table offset */
	ph->sslpnphy_dsssgainidxtableoffset =
	    (temp & SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_MASK) >>
	    SSLPNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT;

	if (ph->sslpnphy_dsssgainidxtableoffset > 127)
		ph->sslpnphy_dsssgainidxtableoffset -= 256;

	wlc_sslpnphy_common_read_table(pi, 17, tableBuffer, 2, 32, 64);

	/* reference value of gain_val_tbl at index 64 */
	if (tableBuffer[0] > 63) tableBuffer[0] -= 128;
	ph->sslpnphy_tr_R_gain_val = tableBuffer[0];

	/* reference value of gain_val_tbl at index 65 */
	if (tableBuffer[1] > 63) tableBuffer[1] -= 128;
	ph->sslpnphy_tr_T_gain_val = tableBuffer[1];


	if (CHSPEC_IS20(pi->radio_chanspec)) {
		ph->sslpnphy_Med_Low_Gain_db = (read_phy_reg(pi, SSLPNPHY_LowGainDB)
			& SSLPNPHY_LowGainDB_MedLowGainDB_MASK)
			>> SSLPNPHY_LowGainDB_MedLowGainDB_SHIFT;
		ph->sslpnphy_Very_Low_Gain_db = (read_phy_reg(pi, SSLPNPHY_VeryLowGainDB)
			& SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK)
			>> SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT;
	} else {
		ph->sslpnphy_Med_Low_Gain_db = (read_phy_reg(pi, SSLPNPHY_Rev2_LowGainDB_40)
			& SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_MASK)
			>> SSLPNPHY_Rev2_LowGainDB_40_MedLowGainDB_SHIFT;

		ph->sslpnphy_Very_Low_Gain_db = (read_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40)
			& SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK)
			>> SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT;
	}

	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
		tableBuffer, 2, 32, 28);

	ph->sslpnphy_gain_idx_14_lowword = tableBuffer[0];
	ph->sslpnphy_gain_idx_14_hiword = tableBuffer[1];

	/* tr isolation adjustments */
	if (ph->sslpnphy_tr_isolation_mid && (BOARDTYPE(pi->sh->boardtype) != BCM94319WLUSBN4L_SSID)) {
		int8 delta_T_change;

		tab.tbl_ptr = tableBuffer;	/* ptr to buf */
		tableBuffer[0] = ph->sslpnphy_tr_R_gain_val;
		tableBuffer[1] = ph->sslpnphy_tr_isolation_mid;
		if (tableBuffer[1] > 63) {
			if (CHIPID(pi->sh->chip) == BCM5356_CHIP_ID)
				tableBuffer[1] = tableBuffer[1] - 128 + 15;
			else
				tableBuffer[1] -= 128;
		} else {
			if (CHIPID(pi->sh->chip) == BCM5356_CHIP_ID)
				tableBuffer[1] += 15;
		}
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
			tableBuffer, 2, 32, 64);

		delta_T_change = ph->sslpnphy_tr_T_gain_val - tableBuffer[1];

		ph->sslpnphy_Very_Low_Gain_db += delta_T_change;
		ph->sslpnphy_tr_T_gain_val = tableBuffer[1];

		if (CHSPEC_IS40(pi->radio_chanspec)) {
			mod_phy_reg(pi, SSLPNPHY_Rev2_VeryLowGainDB_40,
				SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_MASK,
				(ph->sslpnphy_Very_Low_Gain_db <<
				 SSLPNPHY_Rev2_VeryLowGainDB_40_veryLowGainDB_SHIFT));
		} else {
			mod_phy_reg(pi, SSLPNPHY_VeryLowGainDB,
				SSLPNPHY_VeryLowGainDB_veryLowGainDB_MASK,
				(ph->sslpnphy_Very_Low_Gain_db  <<
				 SSLPNPHY_VeryLowGainDB_veryLowGainDB_SHIFT));
		}

	}

	/* Added To Increase The 1Mbps Sense for Temps @Around */
	/* -15C Temp With CmRxAciGainTbl */
	ph->sslpnphy_gain_idx_27_lowword = 0xf1e64d96;
	ph->sslpnphy_gain_idx_27_hiword  = 0xf1e60018;

	/* Storing Input rx offset */
	wlc_sslpnphy_rx_offset_init(pi);
	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);
}

static void wlc_sslpnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 spur_weight;
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	phytbl_info_t tab;
#ifdef BAND5G
	uint freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif

	/* Below are some of the settings required for reducing
	   the spur levels on the 4329 reference board
	 */

	if (NORADIO_ENAB(pi->pubpi)) {
		return;
	}

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		si_pmu_chipcontrol(pi->sh->sih, 0, 0xfff, ((0x8 << 0) | (0xf << 6)));
	}

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (freq < 5000) {
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 350);
		} else if (freq < 5180) {
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 320);
		} else if (freq <= 5500) {
			mod_phy_reg(pi, SSLPNPHY_HiGainDB,
				SSLPNPHY_HiGainDB_HiGainDB_MASK,
				67 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 320);
		} else if (freq < 5660) {
			mod_phy_reg(pi, SSLPNPHY_HiGainDB,
				SSLPNPHY_HiGainDB_HiGainDB_MASK,
				67 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 300);
		} else if (freq < 5775) {
			mod_phy_reg(pi, SSLPNPHY_HiGainDB,
				SSLPNPHY_HiGainDB_HiGainDB_MASK,
				64 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 240);
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else {
			mod_phy_reg(pi, SSLPNPHY_HiGainDB,
				SSLPNPHY_HiGainDB_HiGainDB_MASK,
				64 << SSLPNPHY_HiGainDB_HiGainDB_SHIFT);
			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 240);
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				250 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		}
	} else
#endif /* BAND5G */
	{
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {

		tab.tbl_ptr = &spur_weight; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_id = SSLPNPHY_TBL_ID_SPUR;
		tab.tbl_width = 8;     /* 16 bit wide */

		if (pi->sh->boardflags & BFL_EXTLNA) {
			uint32 curr_clocks, macintmask;

			write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 400);
			write_phy_reg(pi, SSLPNPHY_Rev2_nfSubtractVal_40, 380);
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				3 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				3 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
				SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
				5 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				8 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				6 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			        SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
			        6 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);

			macintmask = wlapi_intrsoff(pi->sh->physhim);
			curr_clocks = si_pmu_cpu_clock(pi->sh->sih, pi->sh->osh);
			wlapi_intrsrestore(pi->sh->physhim, macintmask);
			if (curr_clocks == 300000000) {
				/* For cpu freq of 300MHz */
				if (channel == 3) {
					spur_weight = 3;
					tab.tbl_offset = 58; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 5) {
					spur_weight = 4;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 6) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else if (channel == 7) {
					spur_weight = 3;
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else if (channel == 8) {
					mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
						SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
						4 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else if (channel == 9) {
					spur_weight = 3;
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else if (channel == 10) {
					spur_weight = 3;
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 11) {
					spur_weight = 3;
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 13) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else if (channel == 14) {
					spur_weight = 4;
					tab.tbl_offset = 179; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						6 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				}
			} else {
				/* For cpu freq of 333MHz and rest of the
				 * frequencies.
				 */
				if (channel <= 8 || channel >= 13) {
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						7 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				} else {
					mod_phy_reg(pi, SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						5 << SSLPNPHY_PwrThresh0_SlowPwrLoThresh_SHIFT);
				}

				if (channel == 1) {
					spur_weight = 3;
					tab.tbl_offset = 143; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 3) {
					spur_weight = 3;
					tab.tbl_offset = 175; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 111; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 4) {
					spur_weight = 3;
					tab.tbl_offset = 95; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 5) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 6) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 7) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 8) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 9) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 47; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 10) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 74; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 31; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 11) {
					spur_weight = 3;
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 143; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 15; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 12) {
					spur_weight = 3;
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 13) {
					spur_weight = 4;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					spur_weight = 3;
					tab.tbl_offset = 175; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 14) {
					spur_weight = 4;
					tab.tbl_offset = 179; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					spur_weight = 3;
					tab.tbl_offset = 190; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				}

			}
		} else {
		/* need to optimize this */
		write_phy_reg(pi, SSLPNPHY_nfSubtractVal, 360);

		if (channel == 1) {
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 2) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

		} else if (channel == 3) {
			spur_weight = 5;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 10; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20U,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				    SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				    252 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				    SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				    SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				    SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);
			}
		} else if (channel == 4) {
			spur_weight = 5;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 5) {
			spur_weight = 5;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 6) {
			spur_weight = 2;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 7) {
			spur_weight = 4;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 3;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 26; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 4;
				tab.tbl_offset = 54; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 5;
				tab.tbl_offset = 74; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20U,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20L,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				    SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				    251 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				    SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				    SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				    SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);
			}
		} else if (channel == 8) {
			spur_weight = 5;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 3;
				tab.tbl_offset = 10; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 3;
				tab.tbl_offset = 106; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);
			}
		} else if (channel == 9) {
			spur_weight = 5;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 121; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20L,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				    SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				    252 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				    SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				    SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				    SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);
			}
		} else if (channel == 10) {
			spur_weight = 5;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 150; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				252 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 11) {
			spur_weight = 2;
			tab.tbl_offset = 143; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				253 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 42; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 2;
				tab.tbl_offset = 38; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 5;
				tab.tbl_offset = 90; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 2;
				tab.tbl_offset = 86; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20U,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20U_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_dsssPwrThresh1_20L,
				    SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_MASK,
				    32 << SSLPNPHY_Rev2_dsssPwrThresh1_20L_dsssPwrThresh2_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				    SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				    252 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_PwrThresh0_40,
				    SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_MASK,
				    11 << SSLPNPHY_Rev2_PwrThresh0_40_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
				    SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20U_SlowPwrLoThresh_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
				    SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_MASK,
				    12 << SSLPNPHY_Rev2_transFreeThresh_20L_SlowPwrLoThresh_SHIFT);
			}
		} else if (channel == 12) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				251 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 13) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				251 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		} else if (channel == 14) {
			spur_weight = 3;
			tab.tbl_offset = 179; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			mod_phy_reg(pi, SSLPNPHY_dsssPwrThresh1,
				SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_MASK,
				32 << SSLPNPHY_dsssPwrThresh1_dsssPwrThresh2_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				251 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		}

		mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
			SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			242 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
			SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
			240 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
		}
		} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4) || SSLPNREV_IS(pi->pubpi.phy_rev, 2)) {
			/* 4319 SSLPNPHY REV > 2 */
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			254 << SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);

			if (CHSPEC_IS40(pi->radio_chanspec)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				    SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				    (1 << SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT));
			}
			if (pi->xtalfreq == 30000000) {
				if (channel == 13) {
					spur_weight = 2;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
						&spur_weight, 1, 8, 153);
					spur_weight = 2;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
						&spur_weight, 1, 8, 154);
				}
			}
		}
	}
}

static uint32
wlc_sslpnphy_set_ant_override(phy_info_t *pi, uint16 ant)
{
	uint16 val, ovr;
	uint32 ret;

	ASSERT(ant < 2);

	/* Save original values */
	val = read_phy_reg(pi, SSLPNPHY_RFOverrideVal0);
	ovr = read_phy_reg(pi, SSLPNPHY_RFOverride0);
	ret = ((uint32)ovr << 16) | val;

	/* Write new values */
	val &= ~SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK;
	val |= (ant << SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);
	ovr |= SSLPNPHY_RFOverride0_ant_selp_ovr_MASK;
	write_phy_reg(pi, SSLPNPHY_RFOverrideVal0, val);
	write_phy_reg(pi,  SSLPNPHY_RFOverride0, ovr);

	return ret;
}

static void
wlc_sslpnphy_restore_ant_override(phy_info_t *pi, uint32 ant_ovr)
{
	uint16 ovr, val;

	ovr = (uint16)(ant_ovr >> 16);
	val = (uint16)(ant_ovr & 0xFFFF);

	mod_phy_reg(pi,
		SSLPNPHY_RFOverrideVal0,
		SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
		val);
	mod_phy_reg(pi,
		SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_ant_selp_ovr_MASK,
		ovr);
}


void
wlc_sslpnphy_pktengtx(wlc_phy_t *ppi, wl_pkteng_t *pkteng, uint8 rate,
	struct ether_addr *sa, uint32 wait_delay)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint8 counter = 0;
	uint16 max_pwr_idx = 0;
	uint16 min_pwr_idx = 127;
	uint16 current_txidx = 0;
	uint32 ant_ovr;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ph->sslpnphy_psat_2pt3_detected = 0;
	wlc_sslpnphy_btcx_override_enable(pi);
	wlc_sslpnphy_deaf_mode(pi, TRUE);

	/* Force default antenna */
	ant_ovr = wlc_sslpnphy_set_ant_override(pi, 0);

	for (counter = 0; counter < pkteng->nframes; counter ++) {
		wlc_phy_do_dummy_tx(pi, TRUE, OFF);
		OSL_DELAY(pkteng->delay);
		current_txidx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
		if (current_txidx > max_pwr_idx)
			max_pwr_idx = current_txidx;
		if (current_txidx < min_pwr_idx)
			min_pwr_idx = current_txidx;
	}
	wlc_sslpnphy_deaf_mode(pi, FALSE);

	/* Restore antenna override */
	wlc_sslpnphy_restore_ant_override(pi, ant_ovr);

	if (pkteng->nframes == 100) {
		/* 10 is the value chosen for a start power of 14dBm */
		if (!((max_pwr_idx == 0) && (min_pwr_idx == 127))) {
			if (((max_pwr_idx - min_pwr_idx) > 10) ||
			(min_pwr_idx == 0)) {
				ph->sslpnphy_psat_2pt3_detected = 1;
				current_txidx =  max_pwr_idx;
			}
		}
	}
	ph->sslpnphy_start_idx = (uint8)current_txidx; 	/* debug information */
}

void
wlc_sslpnphy_papd_recal(phy_info_t *pi)
{
	uint16 tx_pwr_ctrl;
	bool suspend;
	uint16 current_txidx = 0;
	wl_pkteng_t pkteng;
	struct ether_addr sa;
	uint8 phybw40 = CHSPEC_IS40(pi->radio_chanspec);
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	sslpnphy_txcalgains_t txgains;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}

	/* temporary arrays needed in child functions of papd cal */
	ph->sslpnphy_papdIntlut = (uint32 *)MALLOC(pi->sh->osh, 128 * sizeof(uint32));
	ph->sslpnphy_papdIntlutVld = (uint8 *)MALLOC(pi->sh->osh, 128 * sizeof(uint8));

	/* if we dont have enough memory, then exit gracefully */
	if ((ph->sslpnphy_papdIntlut == NULL) || (ph->sslpnphy_papdIntlutVld == NULL)) {
		if (ph->sslpnphy_papdIntlut != NULL) {
			MFREE(pi->sh->osh, ph->sslpnphy_papdIntlut, 128 * sizeof(uint32));
		}
		if (ph->sslpnphy_papdIntlutVld != NULL) {
			MFREE(pi->sh->osh, ph->sslpnphy_papdIntlutVld, 128 * sizeof(uint8));
		}
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	if ((CHIPID(pi->sh->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(pi->sh->chip) == BCM4319_CHIP_ID))
		si_pmu_regcontrol(pi->sh->sih, 2, 0x00000007, 0x0);

	/* Resetting all Olympic related microcode settings */
	wlapi_bmac_write_shm(pi->sh->physhim, M_SSLPN_OLYMPIC, 0);

	if (NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			mod_phy_reg(pi, SSLPNPHY_txfefilterctrl,
				SSLPNPHY_txfefilterctrl_txfefilterconfig_en_MASK,
				0 << SSLPNPHY_txfefilterctrl_txfefilterconfig_en_SHIFT);

			mod_phy_reg(pi, SSLPNPHY_txfefilterconfig,
				(SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_MASK |
				SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_MASK),
				((1 << SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_SHIFT) |
				(1 << SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_SHIFT)));
		}
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
			0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_papd_control,
			SSLPNPHY_papd_control_papdCompEn_MASK,
			0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);

		return;
	}

#ifdef PS4319XTRA
	if (CHIPID(pi->sh->chip) == BCM4319_CHIP_ID)
		 wlapi_bmac_write_shm(pi->sh->physhim, M_PS4319XTRA, 0);
#endif /* PS4319XTRA */

	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) &&
	    (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		write_radio_reg(pi, RADIO_2063_LOGEN_BUF_1, ph->sslpnphy_logen_buf_1);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVR_2, ph->sslpnphy_local_ovr_2);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6, ph->sslpnphy_local_oval_6);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5, ph->sslpnphy_local_oval_5);
		write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1, ph->sslpnphy_logen_mixer_1);
	}

	if ((channel != 14) && (channel != 1) && (channel != 11) && ph->sslpnphy_OLYMPIC) {
		write_phy_reg(pi, SSLPNPHY_extstxctrl0, ph->sslpnphy_extstxctrl0);
		write_phy_reg(pi, SSLPNPHY_extstxctrl1, ph->sslpnphy_extstxctrl1);
	}

	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	/* Restore pwr ctrl */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

	wlc_sslpnphy_clear_tx_power_offsets(pi);
	wlc_sslpnphy_set_target_tx_pwr(pi, 56);
	/* Setting npt to 0 for index settling with 30 frames */
	wlc_sslpnphy_set_tx_pwr_npt(pi, 0);

	if (!ph->sslpnphy_force_1_idxcal) {
		/* Enabling Complex filter before transmitting dummy frames */
		/* Check if this is redundant because ucode already does this */
		mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
			(SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_MASK |
			SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK |
			SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_MASK),
			((1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_SHIFT) |
			(1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT) |
			(1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_SHIFT)));

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
				1  << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_papd_control,
				SSLPNPHY_papd_control_papdCompEn_MASK,
				0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
		}

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				(SSLPNPHY_Core1TxControl_txrealfilten_MASK |
				SSLPNPHY_Core1TxControl_txcomplexfilten_MASK |
				SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_MASK),
				((1 << SSLPNPHY_Core1TxControl_txrealfilten_SHIFT) |
				(1  << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT) |
				(1 << SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_SHIFT)));

			mod_phy_reg(pi, SSLPNPHY_papd_control,
				SSLPNPHY_papd_control_papdCompEn_MASK,
				0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
		}
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			mod_phy_reg(pi, SSLPNPHY_txfefilterctrl,
				SSLPNPHY_txfefilterctrl_txfefilterconfig_en_MASK,
				1 << SSLPNPHY_txfefilterctrl_txfefilterconfig_en_SHIFT);
			mod_phy_reg(pi, SSLPNPHY_txfefilterconfig,
				(SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_MASK |
				SSLPNPHY_txfefilterconfig_ofdm_realfilten_MASK |
				SSLPNPHY_txfefilterconfig_ofdm_papden_MASK),
				((1 << SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_SHIFT) |
				(1 << SSLPNPHY_txfefilterconfig_ofdm_realfilten_SHIFT) |
				(0 << SSLPNPHY_txfefilterconfig_ofdm_papden_SHIFT)));

		}
		/* clear our PAPD Compensation table */
		wlc_sslpnphy_clear_papd_comptable(pi);

		mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
			SSLPNPHY_TxPwrCtrlDeltaPwrLimit_DeltaPwrLimit_MASK,
			3 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_DeltaPwrLimit_SHIFT);

		if (SSLPNREV_LE(pi->pubpi.phy_rev, 1)) {
			write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xee);
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txrealfiltcoefsel_MASK,
				1 << SSLPNPHY_Core1TxControl_txrealfiltcoefsel_SHIFT);
		}

		PHY_TRACE(("Pkteng TX Start Called\n"));

		sa.octet[0] = 10;

		pkteng.flags = WL_PKTENG_PER_TX_START;
		pkteng.delay = 2;		/* Inter packet delay */
		pkteng.nframes = 50;		/* number of frames */
		pkteng.length = 0;		/* packet length */
		pkteng.seqno = FALSE;			/* enable/disable sequence no. */
		wlc_sslpnphy_pktengtx((wlc_phy_t *)pi, &pkteng, 108, &sa, (1000*10));
		ph->sslpnphy_dummy_tx_done = 1;

		/* sending out 100 frames to caluclate min & max index */
		if (ph->sslpnphy_vbat_ripple_check) {
			pkteng.delay = 30;		/* Inter packet delay */
			pkteng.nframes = 100;		/* number of frames */
			wlc_sslpnphy_pktengtx((wlc_phy_t *)pi, &pkteng, 108, &sa, (1000*10));
		}
		current_txidx = ph->sslpnphy_start_idx; 	/* debug information */
	}

	/* disabling complex filter for PAPD calibration */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
			0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
	}

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		(SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_MASK |
		SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK |
		SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_MASK),
		((0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_SHIFT) |
		(0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT) |
		(0 << SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_SHIFT)));

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			(SSLPNPHY_Core1TxControl_txcomplexfilten_MASK |
			SSLPNPHY_Core1TxControl_txrealfilten_MASK |
			SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_MASK),
			((0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT) |
			(0 << SSLPNPHY_Core1TxControl_txrealfilten_SHIFT) |
			(0 << SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_SHIFT)));
	}
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_txfefilterctrl,
			SSLPNPHY_txfefilterctrl_txfefilterconfig_en_MASK,
			0 << SSLPNPHY_txfefilterctrl_txfefilterconfig_en_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_txfefilterconfig,
			(SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_realfilten_MASK),
			((0 << SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_SHIFT) |
			(0 << SSLPNPHY_txfefilterconfig_ofdm_realfilten_SHIFT)));
	}
	wlc_sslpnphy_deaf_mode(pi, TRUE);

	wlc_sslpnphy_btcx_override_enable(pi);

	/* Setting npt to 1 for normal transmission */
	wlc_sslpnphy_set_tx_pwr_npt(pi, 1);

	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	ph->sslpnphy_papd_rxGnCtrl_init = 0;

	txgains.useindex = TRUE;
	txgains.index = (uint8) current_txidx;

	if (!ph->sslpnphy_restore_papd_cal_results) {
		if (!ph->sslpnphy_force_1_idxcal)
			wlc_sslpnphy_vbatsense_papd_cal(pi, SSLPNPHY_PAPD_CAL_CW, &txgains);
		wlc_sslpnphy_papd_cal_txpwr(pi, SSLPNPHY_PAPD_CAL_CW, FALSE, TRUE, current_txidx);
	} else {
		wlc_sslpnphy_restore_papd_calibration_results(pi);
	}

	/* Restore tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

	mod_phy_reg(pi, SSLPNPHY_TxPwrCtrlDeltaPwrLimit,
		SSLPNPHY_TxPwrCtrlDeltaPwrLimit_DeltaPwrLimit_MASK,
		1 << SSLPNPHY_TxPwrCtrlDeltaPwrLimit_DeltaPwrLimit_SHIFT);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
			1 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_papd_control,
			SSLPNPHY_papd_control_papdCompEn_MASK,
			0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
	}

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		(SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_MASK |
		SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK |
		SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_MASK),
		((1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdFiltClkEn_SHIFT) |
		(1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT) |
		(1 << SSLPNPHY_sslpnCalibClkEnCtrl_papdRxClkEn_SHIFT)));

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			(SSLPNPHY_Core1TxControl_txrealfilten_MASK |
			SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_MASK |
			SSLPNPHY_Core1TxControl_txcomplexfilten_MASK),
			((1 << SSLPNPHY_Core1TxControl_txrealfilten_SHIFT) |
			(1 << SSLPNPHY_Core1TxControl_txcomplexfiltb4papd_SHIFT) |
			(1 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT)));

		mod_phy_reg(pi, SSLPNPHY_papd_control,
			SSLPNPHY_papd_control_papdCompEn_MASK,
			1 << SSLPNPHY_papd_control_papdCompEn_SHIFT);

		if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID) {
			write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x078f);
			write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x078f);
		}
	}
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_txfiltctrl,
			SSLPNPHY_txfiltctrl_txcomplexfiltb4papd_MASK,
			1 << SSLPNPHY_txfiltctrl_txcomplexfiltb4papd_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_txfefilterconfig,
			(SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_MASK |
			SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_papden_MASK |
			SSLPNPHY_txfefilterconfig_ht_papden_MASK |
			SSLPNPHY_txfefilterconfig_cck_realfilten_MASK |
			SSLPNPHY_txfefilterconfig_cck_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_realfilten_MASK |
			SSLPNPHY_txfefilterconfig_ht_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ht_realfilten_MASK),
			(((!phybw40) << SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_SHIFT) |
			((!phybw40) << SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_papden_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_papden_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_cck_realfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_cck_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_realfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_realfilten_SHIFT)));

		mod_phy_reg(pi, SSLPNPHY_txfefilterctrl,
			SSLPNPHY_txfefilterctrl_txfefilterconfig_en_MASK,
			1 << SSLPNPHY_txfefilterctrl_txfefilterconfig_en_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_papd_control,
			SSLPNPHY_papd_control_papdCompEn_MASK,
			0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
	}
	ph->sslpnphy_papd_cal_done = 1;

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {

		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_txClipEnable_ofdm_MASK,
			1 << SSLPNPHY_Core1TxControl_txClipEnable_ofdm_SHIFT);

		if (phybw40 == 1) {
			write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x0aff);
			write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x0bff);
			write_phy_reg(pi, SSLPNPHY_txClip16Qam, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClip64Qam, 0x7fff);
		} else { /* No clipping for 20Mhz */
			write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClip16Qam, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClip64Qam, 0x7fff);
		}
	}

	if (CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) {
		mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
			SSLPNPHY_Core1TxControl_txClipEnable_ofdm_MASK,
			1 << SSLPNPHY_Core1TxControl_txClipEnable_ofdm_SHIFT);

		if (phybw40 == 1) {
			write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x0aff);
			write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x0bff);
			write_phy_reg(pi, SSLPNPHY_txClip16Qam, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClip64Qam, 0x7fff);
		} else {
			write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x063f);
			write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x071f);
			write_phy_reg(pi, SSLPNPHY_txClip16Qam, 0x7fff);
			write_phy_reg(pi, SSLPNPHY_txClip64Qam, 0x7fff);
		}

	}

	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->chiprev == 0)) {
		/* tssi does not work on 5356a0; hard code tx power */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 50);
	}

	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) &&
	    (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		write_radio_reg(pi, RADIO_2063_LOGEN_BUF_1, 0x06);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVR_2, 0x0f);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6, 0xff);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5, 0xff);
		write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1, 0x66);
	}
	if (ph->sslpnphy_OLYMPIC) {
		uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
		wlapi_bmac_write_shm(pi->sh->physhim, M_SSLPN_OLYMPIC, 1);
		if (channel != 14) {
			if (!(wlc_sslpnphy_fcc_chan_check(pi, channel))) {
				mod_phy_reg(pi, SSLPNPHY_extstxctrl1, 0xf, 0x0);
				if ((ph->sslpnphy_fabid == 2) ||
					(ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_16_64)), 0x1400);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_16_64)), 0x3300);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_2_4)), 0x1550);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_2_4)), 0x3300);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_CCK)), 0x2800);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_CCK)), 0x30c3);
				} else {
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_16_64)), 0x9210);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_16_64)), 0x3150);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_2_4)), 0xf000);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_2_4)), 0x30c3);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_CCK)), 0x2280);
					wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_CCK)), 0x30c3);
				}

				wlapi_bmac_write_shm(pi->sh->physhim, M_SSLPN_OLYMPIC, 3);
			}
		}

	}

	if (channel == 14) {
		mod_phy_reg(pi, SSLPNPHY_lpfbwlutreg0,
			SSLPNPHY_lpfbwlutreg0_lpfbwlut0_MASK,
			2 << SSLPNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_papd_control,
			SSLPNPHY_papd_control_papdCompEn_MASK,
			0 << SSLPNPHY_papd_control_papdCompEn_SHIFT);
		/* Disable complex filter for 4329 and 4319 */
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2))
			mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
				SSLPNPHY_Core1TxControl_txcomplexfilten_MASK,
				0 << SSLPNPHY_Core1TxControl_txcomplexfilten_SHIFT);
		else
			mod_phy_reg(pi, SSLPNPHY_txfefilterconfig,
				SSLPNPHY_txfefilterconfig_cck_cmpxfilten_MASK,
				0 << SSLPNPHY_txfefilterconfig_cck_cmpxfilten_SHIFT);
	} else
		write_phy_reg(pi, SSLPNPHY_lpfbwlutreg0, ph->sslpnphy_filt_bw);

	wlc_sslpnphy_tempsense(pi);

	ph->sslpnphy_last_cal_temperature = ph->sslpnphy_lastsensed_temperature;
	ph->sslpnphy_last_full_cal_temperature = ph->sslpnphy_lastsensed_temperature;

	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);
	if ((CHIPID(pi->sh->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(pi->sh->chip) == BCM4319_CHIP_ID))
		si_pmu_regcontrol(pi->sh->sih, 2, 0x00000007, 0x00000005);

#ifdef PS4319XTRA
	if (CHIPID(pi->sh->chip) == BCM4319_CHIP_ID)
		wlapi_bmac_write_shm(pi->sh->physhim, M_PS4319XTRA, PS4319XTRA);
#endif /* PS4319XTRA */

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	wlc_sslpnphy_deaf_mode(pi, FALSE);

	MFREE(pi->sh->osh, ph->sslpnphy_papdIntlut, 128 * sizeof(uint32));
	MFREE(pi->sh->osh, ph->sslpnphy_papdIntlutVld, 128 * sizeof(uint8));
}

static uint16
sslpnphy_iqlocc_write(phy_info_t *pi, uint8 data)
{
	int32 data32 = (int8)data;
	int32 rf_data32;
	int32 ip, in;
	ip = 8 + (data32 >> 1);
	in = 8 - ((data32+1) >> 1);
	rf_data32 = (in << 4) | ip;
	return (uint16)(rf_data32);
}

static void wlc_sslpnphy_set_radio_loft(phy_info_t *pi,
	uint8 ei0, uint8 eq0, uint8 fi0, uint8 fq0)
{
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, sslpnphy_iqlocc_write(pi, ei0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, sslpnphy_iqlocc_write(pi, eq0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, sslpnphy_iqlocc_write(pi, fi0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, sslpnphy_iqlocc_write(pi, fq0));
}
static void wlc_sslpnphy_restore_txiqlo_calibration_results(phy_info_t *pi)
{
	uint16 a, b;
	uint16 didq;
	uint32 val;
	uint idx;
	uint8 ei0, eq0, fi0, fq0;
	uint8 band_idx;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	ASSERT(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid);

	a = ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0];
	b = ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[1];
	didq = ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5];

	wlc_sslpnphy_set_tx_iqcc(pi, a, b);
	wlc_sslpnphy_set_tx_locc(pi, didq);

	/* restore iqlo portion of tx power control tables */
	/* remaining element */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		val = (val & 0x0ff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		/* loft */
		val = didq;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + idx);
	}
	/* Do not move the below statements up */
	/* We need atleast 2us delay to read phytable after writing radio registers */
	/* Apply analog LO */
	ei0 = (uint8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] >> 8);
	eq0 = (uint8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7]);
	fi0 = (uint8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] >> 8);
	fq0 = (uint8)(ph->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9]);
	wlc_sslpnphy_set_radio_loft(pi, ei0, eq0, fi0, fq0);
}

static void wlc_sslpnphy_restore_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
	wlc_sslpnphy_restore_txiqlo_calibration_results(pi);

	/* restore rx iq cal results */
	write_phy_reg(pi, SSLPNPHY_RxCompcoeffa0, ph->sslpnphy_cal_results[band_idx].rxiqcal_coeffa0);
	write_phy_reg(pi, SSLPNPHY_RxCompcoeffb0, ph->sslpnphy_cal_results[band_idx].rxiqcal_coeffb0);

	write_phy_reg(pi, SSLPNPHY_RxIqCoeffCtrl,
		ph->sslpnphy_cal_results[band_idx].rxiq_enable);
	write_phy_reg(pi, SSLPNPHY_rxfe, ph->sslpnphy_cal_results[band_idx].rxfe);
	write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1,
		ph->sslpnphy_cal_results[band_idx].loopback1);
	write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2,
		ph->sslpnphy_cal_results[band_idx].loopback2);
}

static void
wlc_sslpnphy_compute_delta(phy_info_t *pi)
{
	uint32 papdcompdeltatblval;
	uint8 b;
	uint8 present, next;
	uint32 present_comp, next_comp;
	int32 present_comp_I, present_comp_Q;
	int32 next_comp_I, next_comp_Q;
	int32 delta_I, delta_Q;

	/* Writing Deltas */
	for (b = 0; b <= 124; b = b + 2) {
		present = b + 1;
		next = b + 3;

		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, present);
		present_comp = papdcompdeltatblval;

		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, next);
		next_comp = papdcompdeltatblval;

		present_comp_I = (present_comp & 0x00fff000) << 8;
		present_comp_Q = (present_comp & 0x00000fff) << 20;

		present_comp_I = present_comp_I >> 20;
		present_comp_Q = present_comp_Q >> 20;

		next_comp_I = (next_comp & 0x00fff000) << 8;
		next_comp_Q = (next_comp & 0x00000fff) << 20;

		next_comp_I = next_comp_I >> 20;
		next_comp_Q = next_comp_Q >> 20;

		delta_I = next_comp_I - present_comp_I;
		delta_Q = next_comp_Q - present_comp_Q;

		if (delta_I > 2048)
			delta_I = 2048;
		else if (delta_I < -2048)
			delta_I = -2048;

		if (delta_Q > 2048)
			delta_Q = 2048;
		else if (delta_Q < -2048)
			delta_Q = -2048;

		papdcompdeltatblval = ((delta_I << 12) & 0xfff000) | (delta_Q & 0xfff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, b);
	}
}

static void wlc_sslpnphy_save_papd_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
	uint8 a, i;
	uint32 papdcompdeltatblval;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	/* Save papd calibration results */
	ph->sslpnphy_cal_results[band_idx].analog_gain_ref = read_phy_reg(pi,
		SSLPNPHY_papd_tx_analog_gain_ref);
	ph->sslpnphy_cal_results[band_idx].lut_begin = read_phy_reg(pi,
		SSLPNPHY_papd_track_pa_lut_begin);
	ph->sslpnphy_cal_results[band_idx].lut_end = read_phy_reg(pi,
		SSLPNPHY_papd_track_pa_lut_end);
	ph->sslpnphy_cal_results[band_idx].lut_step = read_phy_reg(pi,
		SSLPNPHY_papd_track_pa_lut_step);
	ph->sslpnphy_cal_results[band_idx].rxcompdbm = read_phy_reg(pi,
		SSLPNPHY_papd_rx_gain_comp_dbm);
	ph->sslpnphy_cal_results[band_idx].papdctrl = read_phy_reg(pi, SSLPNPHY_papd_control);
	/* Save papdcomp delta table */
	for (a = 1, i = 0; a < 128; a = a + 2, i ++) {
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, a);
		ph->sslpnphy_cal_results[band_idx].papd_compdelta_tbl[i] = papdcompdeltatblval;
	}
	ph->sslpnphy_cal_results[band_idx].papd_table_valid = 1;
}

static void
wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
	uint8 a, i;
	uint32 papdcompdeltatblval;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
	if (ph->sslpnphy_cal_results[band_idx].papd_table_valid) {
		/* Apply PAPD cal results */
		for (a = 1, i = 0; a < 128; a = a + 2, i ++) {
			papdcompdeltatblval = ph->sslpnphy_cal_results
				[band_idx].papd_compdelta_tbl[i];
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		}
		/* Writing the deltas */
		wlc_sslpnphy_compute_delta(pi);
	}
	/* Restore saved papd regs */
	write_phy_reg(pi, SSLPNPHY_papd_tx_analog_gain_ref,
		ph->sslpnphy_cal_results[band_idx].analog_gain_ref);
	write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_begin,
		ph->sslpnphy_cal_results[band_idx].lut_begin);
	write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_end,
		ph->sslpnphy_cal_results[band_idx].lut_end);
	write_phy_reg(pi, SSLPNPHY_papd_track_pa_lut_step,
		ph->sslpnphy_cal_results[band_idx].lut_step);
	write_phy_reg(pi, SSLPNPHY_papd_rx_gain_comp_dbm,
		ph->sslpnphy_cal_results[band_idx].rxcompdbm);
	write_phy_reg(pi, SSLPNPHY_papd_control, ph->sslpnphy_cal_results[band_idx].papdctrl);
}

void wlc_sslpnphy_percal_flags_off(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ph->sslpnphy_recal = 0;
	ph->sslpnphy_force_1_idxcal = 0;
	ph->sslpnphy_percal_ctr = 0;
	ph->sslpnphy_papd_nxt_cal_idx = 0;
	ph->sslpnphy_txidx_drift = 0;
	ph->sslpnphy_cur_idx = 0;
	ph->sslpnphy_restore_papd_cal_results = 0;
	ph->sslpnphy_dummy_tx_done = 0;
}

void
WLBANDINITFN(wlc_phy_init_sslpnphy)(phy_info_t *pi)
{
	uint8 i;
	uint8 phybw40;
	uint8 band_idx;
	phytbl_info_t tab;
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	ph->sslpnphy_OLYMPIC = ((BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90U_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN90M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN18_SSID ||
		((BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) &&
		(CHSPEC_IS5G(pi->radio_chanspec))) ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID ||
		BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);

	wlc_sslpnphy_percal_flags_off(pi);

	if (CHIPID(pi->sh->chip) == BCM4319_CHIP_ID)
		si_pmu_chipcontrol(pi->sh->sih, 2, 0x0003f000, (0xa << 12));

	if (SSLPNREV_LE(pi->pubpi.phy_rev, 1))
		si_pmu_regcontrol(pi->sh->sih, 2, 0x00000007, 0x00000005);

	/* enable extlna */
	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->boardflags & BFL_EXTLNA)) {

		si_pmu_chipcontrol(pi->sh->sih, 2, (1 << 26), (1 << 26));
	}

	/* initializing the adc-presync and auxadc-presync for 2x sampling */
	wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);

	/* Initialize baseband */
	wlc_sslpnphy_baseband_init(pi);

	/* Initialize radio */
	wlc_sslpnphy_radio_init(pi);

	/* Run RC Cal */
	wlc_sslpnphy_rc_cal(pi);

	if (!NORADIO_ENAB(pi->pubpi)) {
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, 0x3f);
	}

	ph->sslpnphy_filt_bw = read_phy_reg(pi, SSLPNPHY_lpfbwlutreg0);
	ph->sslpnphy_extstxctrl0 = read_phy_reg(pi, SSLPNPHY_extstxctrl0);
	ph->sslpnphy_extstxctrl1 = read_phy_reg(pi, SSLPNPHY_extstxctrl1);
	ph->sslpnphy_extstxctrl3 = read_phy_reg(pi, SSLPNPHY_extstxctrl3);
	ph->sslpnphy_extstxctrl4 = read_phy_reg(pi, SSLPNPHY_extstxctrl4);

	wlc_sslpnphy_cck_filt_load(pi, ph->sslpnphy_cck_filt_sel);

	wlc_sslpnphy_cck_filt_load(pi, ph->sslpnphy_cck_filt_sel);
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
			sslpnphy_rev0_cx_cck, 10);
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
			sslpnphy_rev0_cx_ofdm, 10);

	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		if (!NORADIO_ENAB(pi->pubpi)) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev1_cx_ofdm, 10);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
				sslpnphy_rev1_real_ofdm, 5);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ht_tap0,
				sslpnphy_rev1_real_ht, 5);

			/* NOK ref board sdagb  and TDK module Es2.11 requires */
			/*  special tuning for spectral flatness */
			if ((BOARDTYPE(pi->sh->boardtype) == BCM94329AGB_SSID) ||
				((BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) &&
				(CHSPEC_IS2G(pi->radio_chanspec))))
				wlc_sslpnphy_ofdm_filt_load(pi);
			if (BOARDTYPE(pi->sh->boardtype) == BCM94329TDKMDL11_SSID)
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
					sslpnphy_tdk_mdl_real_ofdm, 5);

			if (ph->sslpnphy_OLYMPIC) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_olympic_cx_ofdm, 10);
				if ((BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) ||
					(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17U_SSID) ||
					(BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICX17M_SSID))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev1_cx_ofdm, 10);
			}

			if (channel == 14)
				wlc_sslpnphy_cck_filt_load(pi, 4);
		}
	}

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 2) || SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		if (!NORADIO_ENAB(pi->pubpi)) {
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
				if (phybw40 == 1)
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev4_phybw40_real_ofdm, 5);
				else
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev4_real_ofdm, 5);
			} else {
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev2_real_ofdm, 5);
			}
			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev4_phybw40_cx_ofdm, 10);
				else
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev2_phybw40_cx_ofdm, 10);
			} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev4_cx_ofdm, 10);
			}

			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
						sslpnphy_rev4_phybw40_cx_ht, 10);
				else
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
						sslpnphy_rev2_phybw40_cx_ht, 10);
			} else {
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
					sslpnphy_rev2_cx_ht, 10);
			}
			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0,
					sslpnphy_rev4_phybw40_real_ht, 5);
				else
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0,
					sslpnphy_rev2_phybw40_real_ht, 5);
			}
		} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
				sslpnphy_rev2_default, 10);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev2_default, 10);

		}
	}

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		/* 5356 */
		if (phybw40) {
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0, 179);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap1, 172);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap2, (uint16)(-28));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap3, (uint16)(-92));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap4, 26);
		} else {
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0, 52);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap1, 31);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap2, (uint16)(-9));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap3, (uint16)(-15));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap4, 256);
		}

		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0, 141);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap1, 200);
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap2, (uint16)(-231));
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap3, (uint16)(-156));
		write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap4, 256);

		if (phybw40) {
			write_phy_reg(pi, SSLPNPHY_ofdm_tap0_i, 66);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap0_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap1_i, 91);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap1_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap2_i, (uint16)(-6));
			write_phy_reg(pi, SSLPNPHY_ofdm_tap2_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap3_i, (uint16)(-28));
			write_phy_reg(pi, SSLPNPHY_ofdm_tap3_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap4_i, 6);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap4_q, 0);
		} else {
			write_phy_reg(pi, SSLPNPHY_ofdm_tap0_i, 65);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap0_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap1_i, (uint16)(-20));
			write_phy_reg(pi, SSLPNPHY_ofdm_tap1_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap2_i, (uint16)(-162));
			write_phy_reg(pi, SSLPNPHY_ofdm_tap2_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap3_i, 127);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap3_q, 0);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap4_i, 200);
			write_phy_reg(pi, SSLPNPHY_ofdm_tap4_q, 0);
		}

		write_phy_reg(pi, SSLPNPHY_cck_tap0_i, 91);
		write_phy_reg(pi, SSLPNPHY_cck_tap1_i, 24);
		write_phy_reg(pi, SSLPNPHY_cck_tap2_i, 256);
		write_phy_reg(pi, SSLPNPHY_cck_tap3_i, 24);
		write_phy_reg(pi, SSLPNPHY_cck_tap4_i, 91);

		write_phy_reg(pi, SSLPNPHY_cck_tap0_q, (uint16)(-149));
		write_phy_reg(pi, SSLPNPHY_cck_tap1_q, (uint16)(-45));
		write_phy_reg(pi, SSLPNPHY_cck_tap2_q, 0);
		write_phy_reg(pi, SSLPNPHY_cck_tap3_q, 45);
		write_phy_reg(pi, SSLPNPHY_cck_tap4_q, 149);

		if (phybw40 == 1) {
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0, 179);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap1, 172);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap2, (uint16)(-28));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap3, (uint16)(-92));
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap4, 26);

			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap0_i, 66);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap1_i, 91);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap2_i, (uint16)(-6));
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap3_i, (uint16)(-28));
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap4_i, 6);

			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap0_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap1_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap2_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap3_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap4_q, 0);
			ph->sslpnphy_extstxctrl4 = read_phy_reg(pi, SSLPNPHY_extstxctrl4);
			ph->sslpnphy_extstxctrl0 = read_phy_reg(pi, SSLPNPHY_extstxctrl0);
			ph->sslpnphy_extstxctrl1 = read_phy_reg(pi, SSLPNPHY_extstxctrl1);
		} else {
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0, 256);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap1, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap2, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap3, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap4, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap0_i, 47);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap1_i, 17);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap2_i, (uint16)(-27));
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap3_i, (uint16)(-20));
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap4_i, 256);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap0_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap1_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap2_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap3_q, 0);
			write_phy_reg(pi, SSLPNPHY_Rev2_ht_tap4_q, 0);

		}
		/* 5356 */
	}

	wlc_sslpnphy_agc_temp_init(pi);

	wlc_sslpnphy_tempsense(pi);

	wlc_sslpnphy_temp_adj(pi);

	/* Tune to the current channel */
	wlc_phy_chanspec_set_sslpnphy(pi, pi->radio_chanspec);

	wlc_sslpnphy_tx_pwr_ctrl_init(pi);

	for (i = 0; i < TXP_NUM_RATES; i++)
		ph->sslpnphy_saved_tx_user_target[i] = pi->txpwr_limit[i];

	mod_phy_reg(pi, SSLPNPHY_Core1TxControl,
		SSLPNPHY_Core1TxControl_txClipEnable_ofdm_MASK,
		0 << SSLPNPHY_Core1TxControl_txClipEnable_ofdm_SHIFT);
	write_phy_reg(pi, SSLPNPHY_txClipBpsk, 0x7fff);
	write_phy_reg(pi, SSLPNPHY_txClipQpsk, 0x7fff);

	ph->sslpnphy_noise_samples = SSLPNPHY_NOISE_SAMPLES_DEFAULT;
	wlc_sslpnphy_noise_init(pi);

	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) &&
	    (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		ph->sslpnphy_logen_buf_1 = read_radio_reg(pi, RADIO_2063_LOGEN_BUF_1);
		ph->sslpnphy_local_ovr_2 = read_radio_reg(pi, RADIO_2063_LOCAL_OVR_2);
		ph->sslpnphy_local_oval_6 = read_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6);
		ph->sslpnphy_local_oval_5 = read_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5);
		ph->sslpnphy_logen_mixer_1 = read_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1);
	}

	if ((CHIPID(pi->sh->chip) == BCM5356_CHIP_ID) &&
		(pi->sh->chiprev == 0)) {
		/* tssi does not work on 5356a0; hard code tx power */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 50);

		if (phybw40) {
			tab.tbl_ptr = fltr_ctrl_tbl_40Mhz;
			tab.tbl_len = 10;
			tab.tbl_id = 0xb;
			tab.tbl_offset = 0;
			tab.tbl_width = 32;
			wlc_sslpnphy_write_table(pi, &tab);
		}
	}
}

void
WLBANDINITFN(wlc_phy_cal_init_sslpnphy)(phy_info_t *pi)
{
}

static uint32
wlc_sslpnphy_qdiv_roundup(uint32 dividend, uint32 divisor, uint8 precision)
{
	uint32 quotient, remainder, roundup, rbit;

	ASSERT(divisor);

	quotient = dividend / divisor;
	remainder = dividend % divisor;
	rbit = divisor & 1;
	roundup = (divisor >> 1) + rbit;

	while (precision--) {
		quotient <<= 1;
		if (remainder >= roundup) {
			quotient++;
			remainder = ((remainder - roundup) << 1) + rbit;
		} else {
			remainder <<= 1;
		}
	}

	/* Final rounding */
	if (remainder >= roundup)
		quotient++;

	return quotient;
}

#ifdef BAND5G
static void
wlc_sslpnphy_radio_2063_channel_tweaks_A_band(phy_info_t *pi, uint freq)
{
	uint i;
	const chan_info_2063_sslpnphy_aband_tweaks_t *ci;

	write_radio_reg(pi, RADIO_2063_PA_SP_6, 0x7f);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_17, 0xff);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_13, 0xff);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_5, 0xff);
	wlc_sslpnphy_set_pa_gain(pi, 116);
	if (freq <= 5320)
		write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);

	for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_aband_tweaks); i++) {
		if (freq <= chan_info_2063_sslpnphy_aband_tweaks[i].freq)
			break;
	}
	ci = &chan_info_2063_sslpnphy_aband_tweaks[i];

	write_radio_reg(pi, RADIO_2063_PA_CTRL_11, ci->RF_PA_CTRL_11);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_8, ci->RF_TXRF_CTRL_8);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_5, ci->RF_TXRF_CTRL_5);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_2, ci->RF_TXRF_CTRL_2);

	if ((freq > 5100) && (freq <= 5825)) {
		write_radio_reg(pi, RADIO_2063_PA_CTRL_5, ci->RF_PA_CTRL_5);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_15, ci->RF_TXRF_CTRL_15);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_14, ci->RF_TXRF_CTRL_14);
	} else {
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_4, ci->RF_TXRF_CTRL_4);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_7, ci->RF_TXRF_CTRL_7);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_6, ci->RF_TXRF_CTRL_6);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_2, ci->RF_PA_CTRL_2);
	}
	if ((freq > 5100) && (freq <= 5320)) {
		write_radio_reg(pi, RADIO_2063_PA_CTRL_7, 0x40);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_2, 0x50);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_5, 0x40);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_5, 0xd6);
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_2, 0xf8);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_15, 0xee);
	}
}
#endif /* BAND5G */

static void
wlc_sslpnphy_radio_2063_channel_tune(phy_info_t *pi, uint8 channel)
{
	uint i;
	const chan_info_2063_sslpnphy_t *ci;
	uint8 rfpll_doubler = 1;
	uint16 rf_common15;
	fixed qFxtal, qFref, qFvco, qFcal, qVco, qVal;
	uint8  to, refTo, cp_current, kpd_scale, ioff_scale, offset_current;
	uint32 setCount, div_int, div_frac, iVal, fvco3, fref, fref3, fcal_div;
	uint16 loop_bw = 0;
	uint16 d30 = 0;
	uint16 temp_pll, temp_pll_1;
	uint16  h29, h30;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ci = &chan_info_2063_sslpnphy[0];

	/* lookup radio-chip-specific channel code */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy); i++)
			if (chan_info_2063_sslpnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2063_sslpnphy)) {
			PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				pi->sh->unit, __FUNCTION__, channel));
			return;
		}

		ci = &chan_info_2063_sslpnphy[i];
#ifndef BAND5G
	}
#else
	} else {
		for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_aband); i++)
			if (chan_info_2063_sslpnphy_aband[i].chan == channel)
				break;
		if (i >= ARRAYSIZE(chan_info_2063_sslpnphy_aband)) {
			PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				pi->sh->unit, __FUNCTION__, channel));
			return;
		}
		ci = &chan_info_2063_sslpnphy_aband[i];
	}
#endif
	/* Radio tunables */
	write_radio_reg(pi, RADIO_2063_LOGEN_VCOBUF_1, ci->RF_logen_vcobuf_1);
	write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_2, ci->RF_logen_mixer_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_BUF_2, ci->RF_logen_buf_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_RCCR_1, ci->RF_logen_rccr_1);
	write_radio_reg(pi, RADIO_2063_GRX_1ST_3, ci->RF_grx_1st_3);
	if ((ph->sslpnphy_fabid == 2) || (ph->sslpnphy_fabid_otp == TSMC_FAB12)) {
		if (channel == 4)
			write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0b);
		else if (channel == 5)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0b);
		else if (channel == 6)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0a);
		else if (channel == 7)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x09);
		else if (channel == 8)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x09);
		else if (channel == 9)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x08);
		else if (channel == 10)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x05);
		else if (channel == 11)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x04);
		else if (channel == 12)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x03);
		else if (channel == 13)
			write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x02);
	}
	write_radio_reg(pi, RADIO_2063_GRX_2ND_2, ci->RF_grx_2nd_2);
	write_radio_reg(pi, RADIO_2063_ARX_1ST_3, ci->RF_arx_1st_3);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_1, ci->RF_arx_2nd_1);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_4, ci->RF_arx_2nd_4);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_7, ci->RF_arx_2nd_7);
	write_radio_reg(pi, RADIO_2063_ARX_PS_6, ci->RF_arx_ps_6);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_2, ci->RF_txrf_ctrl_2);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_5, ci->RF_txrf_ctrl_5);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_11, ci->RF_pa_ctrl_11);
	write_radio_reg(pi, RADIO_2063_ARX_MIX_4, ci->RF_arx_mix_4);

	/* Turn on PLL power supplies */
	rf_common15 = read_radio_reg(pi, RADIO_2063_COMMON_15);
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15 | (0x0f << 1));

	/* Calculate various input frequencies */
	fref = rfpll_doubler ? pi->xtalfreq : (pi->xtalfreq << 1);
	if (pi->xtalfreq <= 26000000)
		fcal_div = rfpll_doubler ? 1 : 4;
	else
		fcal_div = 2;
	if (ci->freq > 2484)
		fvco3 = (ci->freq << 1);
	else
		fvco3 = (ci->freq << 2);
	fref3 = 3 * fref;

	/* Convert into Q16 MHz */
	qFxtal = wlc_sslpnphy_qdiv_roundup(pi->xtalfreq, PLL_2063_MHZ, 16);
	qFref =  wlc_sslpnphy_qdiv_roundup(fref, PLL_2063_MHZ, 16);
	qFcal = wlc_sslpnphy_qdiv_roundup(fref, fcal_div * PLL_2063_MHZ, 16);
	qFvco = wlc_sslpnphy_qdiv_roundup(fvco3, 3, 16);

	/* PLL_delayBeforeOpenLoop */
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_3, 0x02);

	/* PLL_enableTimeout */
	to = (uint8)((((fref * PLL_2063_CAL_REF_TO) /
		(PLL_2063_OPEN_LOOP_DELAY * fcal_div * PLL_2063_MHZ)) + 1) >> 1) - 1;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_6, (0x07 << 0), to >> 2);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_7, (0x03 << 5), to << 5);


	/* PLL_cal_ref_timeout */
	refTo = (uint8)((((fref * PLL_2063_CAL_REF_TO) / (fcal_div * (to + 1))) +
		(PLL_2063_MHZ - 1)) / PLL_2063_MHZ) - 1;
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_5, refTo);

	/* PLL_calSetCount */
	setCount = (uint32)FLOAT(
		(fixed)wlc_sslpnphy_qdiv_roundup(qFvco, qFcal * 16, 16) * (refTo + 1) * (to + 1)) - 1;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_7, (0x0f << 0), (uint8)(setCount >> 8));
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_8, (uint8)(setCount & 0xff));

	/* Divider, integer bits */
	div_int = ((fvco3 * (PLL_2063_MHZ >> 4)) / fref3) << 4;

	/* Divider, fractional bits */
	div_frac = ((fvco3 * (PLL_2063_MHZ >> 4)) % fref3) << 4;
	while (div_frac >= fref3) {
		div_int++;
		div_frac -= fref3;
	}
	div_frac = wlc_sslpnphy_qdiv_roundup(div_frac, fref3, 20);

	/* Program PLL */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_1, (0x1f << 0), (uint8)(div_int >> 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x1f << 4), (uint8)(div_int << 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x0f << 0), (uint8)(div_frac >> 16));
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_3, (uint8)(div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_4, (uint8)div_frac & 0xff);

	/* REmoving the hard coded values for PLL registers and make it */
	/* programmable with loop bw and d30 */

	/* PLL_cp_current */
	qVco = ((PLL_2063_HIGH_END_KVCO - PLL_2063_LOW_END_KVCO) *
		(qFvco - FIXED(PLL_2063_LOW_END_VCO)) /
		(PLL_2063_HIGH_END_VCO - PLL_2063_LOW_END_VCO)) +
		FIXED(PLL_2063_LOW_END_KVCO);
	if ((CHSPEC_IS2G(pi->radio_chanspec)) && (pi->sh->boardflags & BFL_HGPA)) {
		loop_bw = PLL_2063_LOOP_BW_ePA;
		d30 = PLL_2063_D30_ePA;
	} else {
		loop_bw = PLL_2063_LOOP_BW;
		d30 = PLL_2063_D30;
	}
	h29 = (uint16) wlc_sslpnphy_qdiv_roundup(loop_bw * 10, 270, 0); /* h29 * 10 */
	h30 = (uint16) wlc_sslpnphy_qdiv_roundup(d30 * 10, 2640, 0); /* h30 * 10 */

	/* PLL_lf_r1 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup((d30 - 680), 490, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_3, 0x1f << 3, temp_pll << 3);

	/* PLL_lf_r2 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup((1660 * h30 - 6800), 4900, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_3, 0x7, (temp_pll >> 2));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_4, 0x3 << 5, (temp_pll & 0x3) << 5);

	/* PLL_lf_r3 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup((1660 * h30 - 6800), 4900, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_4, 0x1f, temp_pll);

	/* PLL_lf_c1 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup(1046500, h30 * h29, 0);
	temp_pll_1 = (uint16) wlc_sslpnphy_qdiv_roundup((temp_pll - 1775), 555, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_1, 0xf << 4, temp_pll_1 << 4);

	/* PLL_lf_c2 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup(61700, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_sslpnphy_qdiv_roundup((temp_pll - 123), 38, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_1, 0xf, temp_pll_1);

	/* PLL_lf_c3 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup(27000, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_sslpnphy_qdiv_roundup((temp_pll - 61), 19, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_2, 0xf << 4, temp_pll_1 << 4);

	/* PLL_lf_c4 */
	temp_pll = (uint16) wlc_sslpnphy_qdiv_roundup(26400, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_sslpnphy_qdiv_roundup((temp_pll - 55), 19, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_2, 0xf, temp_pll_1);


	iVal = ((d30 - 680)  + (490 >> 1))/ 490;
	qVal = wlc_sslpnphy_qdiv_roundup(
		440 * loop_bw * div_int,
		27 * (68 + (iVal * 49)), 16);
	kpd_scale = ((qVal + qVco - 1) / qVco) > 60 ? 1 : 0;
	if (kpd_scale)
		cp_current = ((qVal + qVco) / (qVco << 1)) - 8;
	else
		cp_current = ((qVal + (qVco >> 1)) / qVco) - 8;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_2, 0x3f, cp_current);

	/*  PLL_Kpd_scale2 */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_2, 1 << 6, (kpd_scale << 6));

	/* PLL_offset_current */
	qVal = wlc_sslpnphy_qdiv_roundup(100 * qFref, qFvco, 16) * (cp_current + 8) * (kpd_scale + 1);
	ioff_scale = (qVal > FIXED(150)) ? 1 : 0;
	qVal = (qVal / (6 * (ioff_scale + 1))) - FIXED(2);
	if (qVal < 0)
		offset_current = 0;
	else
		offset_current = FLOAT(qVal + (FIXED(1) >> 1));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 0x1f, offset_current);

	/*  PLL_ioff_scale2 */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 1 << 5, ioff_scale << 5);


	/* PLL_pd_div2_BB */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 1 << 2, rfpll_doubler << 2);

	/* PLL_cal_xt_endiv */
	if (!rfpll_doubler || (pi->xtalfreq > 26000000))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x02);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0xfd);

	/* PLL_cal_xt_sdiv */
	if (!rfpll_doubler && (pi->xtalfreq > 26000000))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x01);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0xfe);

	/* PLL_sel_short */
	if (qFref > FIXED(45))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCO_1, 0x02);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCO_1, 0xfd);

	mod_radio_reg(pi, RADIO_2063_PLL_SP_2, 0x03, 0x03);
	OSL_DELAY(1);
	mod_radio_reg(pi, RADIO_2063_PLL_SP_2, 0x03, 0);

	/* Force VCO cal */
	wlc_phy_radio_2063_vco_cal(pi);

	/* Restore state */
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15);
}

bool
wlc_phy_tpc_isenabled_sslpnphy(phy_info_t *pi)
{
	return (SSLPNPHY_TX_PWR_CTRL_HW == wlc_sslpnphy_get_tx_pwr_ctrl((pi)));
}

/* %%%%%% major flow operations */
void
wlc_phy_txpower_recalc_target_sslpnphy(phy_info_t *pi)
{
	uint16 pwr_ctrl;

	if (!wlc_sslpnphy_tssi_enabled(pi))
		return;

	/* Temporary disable power control to update settings */
	pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	wlc_sslpnphy_txpower_recalc_target(pi);
	/* Restore power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, pwr_ctrl);
}

void
wlc_phy_detach_sslpnphy(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_sslpnphy, sizeof(phy_info_sslpnphy_t));
}

bool
wlc_phy_attach_sslpnphy(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph;

	pi->u.pi_sslpnphy = (phy_info_sslpnphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_sslpnphy_t));
	if (pi->u.pi_sslpnphy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_sslpnphy, sizeof(phy_info_sslpnphy_t));

	ph = pi->u.pi_sslpnphy;
	if ((0 == (pi->sh->boardflags & BFL_NOPA)) && !NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	/* Get xtal frequency from PMU */
	pi->xtalfreq = si_alp_clock(pi->sh->sih);
	ASSERT(0 == (pi->xtalfreq % 1000));

	/* set papd_rxGnCtrl_init to 0 */
	ph->sslpnphy_papd_rxGnCtrl_init = 0;

	PHY_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		pi->sh->unit, __FUNCTION__,
		pi->xtalfreq / 1000000, pi->xtalfreq % 1000000));

	pi->pi_fptr.init = wlc_phy_init_sslpnphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_sslpnphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_sslpnphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_sslpnphy;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = wlc_phy_sslpnphy_long_train;
#endif
	pi->pi_fptr.txiqccget = wlc_sslpnphy_get_tx_iqcc;
	pi->pi_fptr.txiqccset = wlc_sslpnphy_set_tx_iqcc;
	pi->pi_fptr.txloccget = wlc_sslpnphy_get_tx_locc;
	pi->pi_fptr.radioloftget = wlc_sslpnphy_get_radio_loft;
#if defined(WLTEST)
	pi->pi_fptr.carrsuppr = wlc_phy_carrier_suppress_sslpnphy;
	pi->pi_fptr.rxsigpwr = wlc_sslpnphy_rx_signal_power;
#endif
	pi->pi_fptr.detach = wlc_phy_detach_sslpnphy;
	if (!wlc_phy_txpwr_srom_read_sslpnphy(pi))
		return FALSE;

	wlc_sslpnphy_store_tbls(pi);

	return TRUE;
}

/* %%%%%% testing */
#if defined(BCMDBG) || defined(WLTEST)
static uint32 ltrn_list_sslpnphy[] = {
	0x40000, 0x2839e, 0xfdf3b, 0xf370a, 0x1074a, 0x2cfef, 0x27888, 0x0f89f, 0x0882e,
	0x16fa5, 0x18b70, 0xf8f8e, 0xd0fa6, 0xcbf81, 0xf0752, 0x1a76e, 0x283d6, 0x1e826,
	0x15c07, 0x12393, 0x00b44, 0xddf60, 0xc83b2, 0xdb7cf, 0x0a3a0, 0x26787, 0x183e8,
	0xfa09b, 0xf6d07, 0x148bd, 0x30ff9, 0x2f372, 0x19b9a, 0x0d434, 0x0f0a1, 0x07890,
	0xe8840, 0xc882b, 0xca46b, 0xf30ab, 0x21c97, 0x31045, 0x1c817, 0xfc043, 0xe7485,
	0xe346c, 0xe8fdc, 0xf073b, 0xf1b09, 0xe5b62, 0xce3e5, 0xbe017, 0xcbfde, 0xf6f8e,
	0x1ef87, 0x21fdf, 0xfec58, 0xda8a9, 0xda4bd, 0xff0b2, 0x258ae, 0x294b1, 0x050a0,
	0xd5862, 0xc0000, 0xd5b9e, 0x05360, 0x2974f, 0x25b52, 0xff34e, 0xda743, 0xdaf57,
	0xfefa8, 0x21c21, 0x1ec79, 0xf6c72, 0xcbc22, 0xbe3e9, 0xce01b, 0xe589e, 0xf1cf7,
	0xf04c6, 0xe8c24, 0xe3794, 0xe777b, 0xfc3bd, 0x1cbe9, 0x313bb, 0x21f69, 0xf3755,
	0xca795, 0xc8bd5, 0xe8bbf, 0x07b71, 0x0f35f, 0x0d7cc, 0x19866, 0x2f08e, 0x30c07,
	0x14b43, 0xf6ef9, 0xfa365, 0x18019, 0x26479, 0x0a060, 0xdb431, 0xc804e, 0xddca0,
	0x008bc, 0x1206c, 0x15ff9, 0x1ebd9, 0x2802a, 0x1a492, 0xf04ae, 0xcbc7f, 0xd0c5a,
	0xf8c72, 0x18890, 0x16c5b, 0x08fd2, 0x0ff62, 0x27f78, 0x2cc11, 0x104b6, 0xf34f6,
	0xfe0c5, 0x28462

};

int
wlc_phy_sslpnphy_long_train(phy_info_t *pi, int channel)
{
	uint16 num_samps;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);

	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		wlc_sslpnphy_stop_tx_tone(pi);
		wlc_sslpnphy_deaf_mode(pi, FALSE);
		write_phy_reg(pi, SSLPNPHY_sslpnCtrl3, 1);
		mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
			SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
			0 << SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);
		wlapi_enable_mac(pi->sh->physhim);
		return 0;
	}

	wlc_sslpnphy_deaf_mode(pi, TRUE);
	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (wlc_phy_test_init(pi, channel, TRUE)) {
		return 1;
	}

	num_samps = sizeof(ltrn_list_sslpnphy)/sizeof(*ltrn_list_sslpnphy);
	/* load sample table */
	write_phy_reg(pi, SSLPNPHY_sslpnCtrl3, 0);
	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		ltrn_list_sslpnphy, num_samps, 32, 0);

	wlc_sslpnphy_run_samples(pi, num_samps, 0xffff, 0, 0);

	return 0;
}

void
wlc_phy_init_test_sslpnphy(phy_info_t *pi)
{
	/* Force WLAN antenna */
	wlc_sslpnphy_btcx_override_enable(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	mod_phy_reg(pi, SSLPNPHY_papd_control,
		SSLPNPHY_papd_control_forcepapdClkOn_MASK,
		1 << SSLPNPHY_papd_control_forcepapdClkOn_SHIFT);
	/* Recalibrate for this channel */
	wlc_sslpnphy_full_cal(pi);
}

#endif 

static bool wlc_sslpnphy_fcc_chan_check(phy_info_t *pi, uint channel)
{
	if (BOARDTYPE(pi->sh->boardtype) == BCM94329OLYMPICN18_SSID) {
		if ((channel == 1) || (channel == 11))
			return TRUE;
	} else if ((BOARDTYPE(pi->sh->boardtype) == BCM94329AGBF_SSID) ||
		(BOARDTYPE(pi->sh->boardtype) == BCM94329AGB_SSID)) {
		if ((channel == 1) || (channel == 11) || (channel == 13))
		return TRUE;
	}
	return FALSE;
}


typedef enum {
	INIT_FILL_FIFO = 0,
	CHK_LISTEN_GAIN_CHANGE,
	CHANGE_RXPO,
	PERIODIC_CAL
} sslpnphy_noise_measure_t;

STATIC uint8 NOISE_ARRAY [][2] = {
	{1, 62 },
	{2, 65 },
	{3, 67 },
	{4, 68 },
	{5, 69 },
	{6, 70 },
	{7, 70 },
	{8, 71 },
	{9, 72 },
	{10, 72 },
	{11, 72 },
	{12, 73 },
	{13, 73 },
	{14, 74 },
	{15, 74 },
	{16, 74 },
	{17, 74 },
	{18, 75 },
	{19, 75 },
	{20, 75 },
	{21, 75 },
	{22, 75 },
	{23, 76 },
	{24, 76 },
	{25, 76 },
	{26, 76 },
	{27, 76 },
	{28, 77 },
	{29, 77 },
	{30, 77 },
	{31, 77 },
	{32, 77 },
	{33, 77 },
	{34, 77 },
	{35, 77 },
	{36, 78 },
	{37, 78 },
	{38, 78 },
	{39, 78 },
	{40, 78 },
	{41, 78 },
	{42, 78 },
	{43, 78 },
	{44, 78 },
	{45, 79 },
	{46, 79 },
	{47, 79 },
	{48, 79 },
	{49, 79 },
	{50, 79 }
};
uint8 NOISE_ARRAY_sz = ARRAYSIZE(NOISE_ARRAY);
static void
wlc_sslpnphy_noise_init(phy_info_t *pi)
{
	uint8 phybw40 = IS40MHZ(pi);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ph->sslpnphy_NPwr_MinLmt = SSLPNPHY_NPWR_MINLMT;
	ph->sslpnphy_NPwr_MaxLmt = SSLPNPHY_NPWR_MAXLMT_2G;
	ph->sslpnphy_noise_measure_window = SSLPNPHY_NOISE_MEASURE_WINDOW_2G;
	ph->sslpnphy_max_listen_gain_change_lmt = SSLPNPHY_MAX_GAIN_CHANGE_LMT_2G;
	ph->sslpnphy_max_rxpo_change_lmt = SSLPNPHY_MAX_RXPO_CHANGE_LMT_2G;
	if (phybw40 || CHSPEC_IS5G(pi->radio_chanspec)) {
		ph->sslpnphy_NPwr_LGC_MinLmt = SSLPNPHY_NPWR_LGC_MINLMT_40MHZ;
		ph->sslpnphy_NPwr_LGC_MaxLmt = SSLPNPHY_NPWR_LGC_MAXLMT_40MHZ;
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			ph->sslpnphy_NPwr_MaxLmt = SSLPNPHY_NPWR_MAXLMT_5G;
			ph->sslpnphy_noise_measure_window = SSLPNPHY_NOISE_MEASURE_WINDOW_5G;
			ph->sslpnphy_max_listen_gain_change_lmt = SSLPNPHY_MAX_GAIN_CHANGE_LMT_5G;
			ph->sslpnphy_max_rxpo_change_lmt = SSLPNPHY_MAX_RXPO_CHANGE_LMT_5G;
		}
	} else {
		ph->sslpnphy_NPwr_LGC_MinLmt = SSLPNPHY_NPWR_LGC_MINLMT_20MHZ;
		ph->sslpnphy_NPwr_LGC_MaxLmt = SSLPNPHY_NPWR_LGC_MAXLMT_20MHZ;
	}
}

static uint8
wlc_sslpnphy_rx_noise_lut(phy_info_t *pi, uint8 noise_val, uint8 ptr[][2], uint8 array_size)
{
	uint8 i = 1;
	uint8 rxpoWoListenGain = 0;
	for (i = 1; i < array_size; i++) {
		if (ptr[i][0] == noise_val) {
			rxpoWoListenGain = ptr[i][1];
		}
	}
	return rxpoWoListenGain;
}

static
void wlc_sslpnphy_reset_radioctrl_crsgain(phy_info_t *pi)
{
	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);
}

static
void wlc_sslpnphy_noise_fifo_init(phy_info_t *pi)
{
	uint8 i;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ph->sslpnphy_noisepwr_fifo_filled = 0;

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		ph->sslpnphy_noisepwr_fifo_Min[i] = 32767;
		ph->sslpnphy_noisepwr_fifo_Max[i] = 0;
	}
}

static
void wlc_sslpnphy_detection_disable(phy_info_t *pi, bool mode)
{
	uint8 phybw40 = IS40MHZ(pi);

	if (phybw40 == 0) {
		mod_phy_reg((pi), SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK |
			SSLPNPHY_crsgainCtrl_OFDMDetectionEnable_MASK,
			((CHSPEC_IS2G(pi->radio_chanspec)) ? (!mode) : 0) <<
			SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT |
			(!mode) << SSLPNPHY_crsgainCtrl_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
			SSLPNPHY_crsgainCtrl_crseddisable_MASK,
			(mode) << SSLPNPHY_crsgainCtrl_crseddisable_SHIFT);
	} else {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_crseddisable_MASK,
			(mode) << SSLPNPHY_Rev2_crsgainCtrl_40_crseddisable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_eddisable20ul,
			SSLPNPHY_Rev2_eddisable20ul_crseddisable_20U_MASK,
			(mode) << SSLPNPHY_Rev2_eddisable20ul_crseddisable_20U_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_eddisable20ul,
			SSLPNPHY_Rev2_eddisable20ul_crseddisable_20L_MASK,
			(mode) << SSLPNPHY_Rev2_eddisable20ul_crseddisable_20L_SHIFT);

		mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
			SSLPNPHY_Rev2_crsgainCtrl_40_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_crsgainCtrl_40_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20U_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_OFDMDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20L_OFDMDetectionEnable_SHIFT);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
		if (CHSPEC_SB_UPPER(pi->radio_chanspec)) {
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
			0 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		} else {
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
			0 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
			(!mode) << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		}
		} else {
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20U,
			SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_MASK,
			0x00 << SSLPNPHY_Rev2_transFreeThresh_20U_DSSSDetectionEnable_SHIFT);
		mod_phy_reg(pi, SSLPNPHY_Rev2_transFreeThresh_20L,
			SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_MASK,
			0x00 << SSLPNPHY_Rev2_transFreeThresh_20L_DSSSDetectionEnable_SHIFT);
		}
		}
	}
}

static
void wlc_sslpnphy_noise_measure_setup(phy_info_t *pi)
{
	int16 temp;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	if (phybw40 == 0) {
		ph->Listen_GaindB_BfrNoiseCal = (uint8)(read_phy_reg(pi, SSLPNPHY_HiGainDB) &
			SSLPNPHY_HiGainDB_HiGainDB_MASK) >> SSLPNPHY_HiGainDB_HiGainDB_SHIFT;
		ph->NfSubtractVal_BfrNoiseCal = (read_phy_reg(pi, SSLPNPHY_nfSubtractVal) & 0x3ff);
	} else {
		ph->Listen_GaindB_BfrNoiseCal =
			(uint8)((read_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40) &
			SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK) >>
			SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT);
		ph->NfSubtractVal_BfrNoiseCal =
			(read_phy_reg(pi, SSLPNPHY_Rev2_nfSubtractVal_40) & 0x3ff);
	}

	ph->Listen_GaindB_AfrNoiseCal = ph->Listen_GaindB_BfrNoiseCal;

	if (ph->sslpnphy_init_noise_cal_done == 0) {
		wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_NOISE_SAMPLES)), 128 << phybw40);
		if (phybw40 == 0) {
			ph->Listen_GaindB_BASE = (uint8)(read_phy_reg(pi, SSLPNPHY_HiGainDB) &
				SSLPNPHY_HiGainDB_HiGainDB_MASK) >>
				SSLPNPHY_HiGainDB_HiGainDB_SHIFT;

			temp = (int16)(read_phy_reg(pi, SSLPNPHY_InputPowerDB)
				& SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK);

			ph->NfSubtractVal_BASE = (read_phy_reg(pi, SSLPNPHY_nfSubtractVal) &
				0x3ff);
		} else {
			ph->Listen_GaindB_BASE =
				(uint8)((read_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40) &
				SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK) >>
				SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT);

			temp = (int16)(read_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40)
				& SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK);

			ph->NfSubtractVal_BASE =
				(read_phy_reg(pi, SSLPNPHY_Rev2_nfSubtractVal_40) & 0x3ff);
		}
		temp = temp << 8;
		temp = temp >> 8;

		ph->RxpowerOffset_Required_BASE = (int8)temp;

		wlc_sslpnphy_detection_disable(pi, TRUE);
		wlc_sslpnphy_reset_radioctrl_crsgain(pi);

		wlc_sslpnphy_noise_fifo_init(pi);
	}

	ph->rxpo_required_AfrNoiseCal = ph->RxpowerOffset_Required_BASE;
}

static
uint32 wlc_sslpnphy_get_rxiq_accum(phy_info_t *pi)
{
	uint32 IPwr, QPwr, IQ_Avg_Pwr = 0;
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);

	IPwr = ((uint32)read_phy_reg(pi, SSLPNPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, SSLPNPHY_IQIPWRAccLoAddress);
	QPwr = ((uint32)read_phy_reg(pi, SSLPNPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, SSLPNPHY_IQQPWRAccLoAddress);

	IQ_Avg_Pwr = (uint32)wlc_phy_qdiv_roundup((IPwr + QPwr),
		wlapi_bmac_read_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +  M_SSLPNPHY_NOISE_SAMPLES))), 0);

	return IQ_Avg_Pwr;
}

static
uint32 wlc_sslpnphy_abs_time(uint32 end, uint32 start)
{
	uint32 timediff;
	uint32 max32 = (uint32)((int)(0) - (int)(1));

	if (end >= start)
		timediff = end - start;
	else
		timediff = (1 + end) + (max32 - start);

	return timediff;
}

static
void wlc_sslpnphy_noise_measure_time_window(phy_info_t *pi, uint32 window_time, uint32 *minpwr,
	uint32 *maxpwr, bool *measurement_valid)
{
	uint32 start_time;
	uint32 IQ_Avg_Pwr = 0;
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	*measurement_valid = FALSE;

	*minpwr = 32767;
	*maxpwr = 0;

	start_time = R_REG(pi->sh->osh, &pi->regs->tsf_timerlow);

	wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
		M_55f_REG_VAL)), 0);
	OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);

	while (wlc_sslpnphy_abs_time(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow),
		start_time) < window_time) {

		if (R_REG(pi->sh->osh, &pi->regs->maccommand) & MCMD_BG_NOISE) {
			OSL_DELAY(8);
		} else {
			IQ_Avg_Pwr = wlc_sslpnphy_get_rxiq_accum(pi);

			*minpwr = MIN(*minpwr, IQ_Avg_Pwr);
			*maxpwr = MAX(*maxpwr, IQ_Avg_Pwr);

			OSL_DELAY(6);
			wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
				M_55f_REG_VAL)), 0);
			OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
		}
	}

	if ((*minpwr >= ph->sslpnphy_NPwr_MinLmt) && (*minpwr <= ph->sslpnphy_NPwr_MaxLmt))
		*measurement_valid = TRUE;
}

static
uint32 wlc_sslpnphy_noise_fifo_min(phy_info_t *pi)
{
	uint8 i;
	uint32 minpwr = 32767;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		PHY_TRACE(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			ph->sslpnphy_noisepwr_fifo_Min[i],
			ph->sslpnphy_noisepwr_fifo_Max[i]));
		minpwr = MIN(minpwr, ph->sslpnphy_noisepwr_fifo_Min[i]);
	}

	return minpwr;
}

static
void wlc_sslpnphy_noise_fifo_avg(phy_info_t *pi, uint32 *avg_noise)
{
	uint8 i;
	uint8 max_min_Idx_1 = 0, max_min_Idx_2 = 0;
	uint32 Min_Min = 65535, Max_Max = 0, Max_Min_2 = 0,  Max_Min_1 = 0;
	uint32 Sum = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (ph->sslpnphy_init_noise_cal_done) {
		*avg_noise = wlc_sslpnphy_noise_fifo_min(pi);
		return;
	}

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		PHY_TRACE(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			ph->sslpnphy_noisepwr_fifo_Min[i],
			ph->sslpnphy_noisepwr_fifo_Max[i]));

		Min_Min = MIN(Min_Min, ph->sslpnphy_noisepwr_fifo_Min[i]);
		Max_Min_1 = MAX(Max_Min_1, ph->sslpnphy_noisepwr_fifo_Min[i]);
		Max_Max = MAX(Max_Max, ph->sslpnphy_noisepwr_fifo_Max[i]);
	}

	if (Max_Max >= ((Min_Min * 5) >> 1))
		*avg_noise = Min_Min;
	else {
		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if (Max_Min_1 == ph->sslpnphy_noisepwr_fifo_Min[i])
				max_min_Idx_1 = i;
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if (i != max_min_Idx_1)
				Max_Min_2 = MAX(Max_Min_2, ph->sslpnphy_noisepwr_fifo_Min[i]);
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if ((Max_Min_2 == ph->sslpnphy_noisepwr_fifo_Min[i]) &&
				(i != max_min_Idx_1))
				max_min_Idx_2 = i;
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if ((i != max_min_Idx_1) && (i != max_min_Idx_2))
				Sum += ph->sslpnphy_noisepwr_fifo_Min[i];
		}

		/* OutOf Six values of MinFifo,Two big values are eliminated
		and averaged out remaining four values of Min-FIFO
		*/

		*avg_noise = wlc_phy_qdiv_roundup(Sum, 4, 0);

		PHY_TRACE(("Sum = %d: Max_Min_1 = %d: Max_Min_2 = %d"
			" max_min_Idx_1 = %d: max_min_Idx_2 = %d\n", Sum,
			Max_Min_1, Max_Min_2, max_min_Idx_1, max_min_Idx_2));
	}
	PHY_TRACE(("Avg_Min_NoisePwr = %d\n", *avg_noise));
}

static void
wlc_sslpnphy_noise_measure_chg_listen_gain(phy_info_t *pi, int8 change_sign)
{
	uint8 phybw40 = IS40MHZ(pi);

	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	ph->Listen_GaindB_AfrNoiseCal = ph->Listen_GaindB_AfrNoiseCal + (3 * change_sign);

	if (ph->Listen_GaindB_AfrNoiseCal <= (ph->Listen_GaindB_BASE -
		ph->sslpnphy_max_listen_gain_change_lmt))
		ph->Listen_GaindB_AfrNoiseCal = ph->Listen_GaindB_BASE -
			ph->sslpnphy_max_listen_gain_change_lmt;
	else if (ph->Listen_GaindB_AfrNoiseCal >= (ph->Listen_GaindB_BASE +
		ph->sslpnphy_max_listen_gain_change_lmt))
		ph->Listen_GaindB_AfrNoiseCal = ph->Listen_GaindB_BASE +
			ph->sslpnphy_max_listen_gain_change_lmt;

	if (phybw40 == 0)
		mod_phy_reg(pi, SSLPNPHY_HiGainDB,
			SSLPNPHY_HiGainDB_HiGainDB_MASK,
			((ph->Listen_GaindB_AfrNoiseCal) <<
			SSLPNPHY_HiGainDB_HiGainDB_SHIFT));
	else
		mod_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40,
			SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK,
			((ph->Listen_GaindB_AfrNoiseCal) <<
			SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT));

	wlc_sslpnphy_reset_radioctrl_crsgain(pi);
	OSL_DELAY(10);
}

static
void wlc_sslpnphy_noise_measure_change_rxpo(phy_info_t *pi, uint32 avg_noise)
{
	uint8 rxpo_Wo_Listengain;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (!ph->sslpnphy_init_noise_cal_done)
		ph->Listen_RF_Gain = (read_phy_reg(pi, SSLPNPHY_crsGainRespVal) & 0xff);
	else
		ph->Listen_RF_Gain = (wlapi_bmac_read_shm(pi->sh->physhim,
			(2 * (sslpnphy_shm_ptr + M_SSLPNPHY_REG_55f_REG_VAL))) & 0xff);

	if ((ph->Listen_RF_Gain > 45) && (ph->Listen_RF_Gain < 85) &&
		(ABS(ph->Listen_GaindB_AfrNoiseCal - ph->Listen_RF_Gain) < 12)) {

		rxpo_Wo_Listengain = wlc_sslpnphy_rx_noise_lut(pi, (uint8)avg_noise, NOISE_ARRAY,
			NOISE_ARRAY_sz);

		ph->rxpo_required_AfrNoiseCal = (int8)(ph->Listen_RF_Gain - rxpo_Wo_Listengain);

		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			if (phybw40 == 0)
				ph->rxpo_required_AfrNoiseCal = ph->rxpo_required_AfrNoiseCal - 1;
			else
				ph->rxpo_required_AfrNoiseCal = ph->rxpo_required_AfrNoiseCal + 3;
		}

		if (ph->rxpo_required_AfrNoiseCal <= (-1 * ph->sslpnphy_max_rxpo_change_lmt))
			ph->rxpo_required_AfrNoiseCal = -1 * ph->sslpnphy_max_rxpo_change_lmt;
		else if (ph->rxpo_required_AfrNoiseCal >= ph->sslpnphy_max_rxpo_change_lmt)
			ph->rxpo_required_AfrNoiseCal = ph->sslpnphy_max_rxpo_change_lmt;

		if (phybw40 == 0)
			mod_phy_reg(pi, SSLPNPHY_InputPowerDB,
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK,
				(ph->rxpo_required_AfrNoiseCal <<
				SSLPNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
		else
			mod_phy_reg(pi, SSLPNPHY_Rev2_InputPowerDB_40,
				SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK,
				ph->rxpo_required_AfrNoiseCal <<
				SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_SHIFT);
	}
}

static
void  wlc_sslpnphy_noise_measure_computeNf(phy_info_t *pi)
{
	int8 Delta_Listen_GaindB_Change, Delta_NfSubtractVal_Change;
	uint8 phybw40 = IS40MHZ(pi);
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	if (phybw40 == 0)
		ph->Listen_GaindB_AfrNoiseCal = (uint8)(read_phy_reg(pi, SSLPNPHY_HiGainDB)
			& SSLPNPHY_HiGainDB_HiGainDB_MASK) >>
			SSLPNPHY_HiGainDB_HiGainDB_SHIFT;
	else
		ph->Listen_GaindB_AfrNoiseCal = (uint8)((read_phy_reg(pi, SSLPNPHY_Rev2_HiGainDB_40)
			& SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK) >>
			SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT);

	Delta_Listen_GaindB_Change = (int8)(ph->Listen_GaindB_AfrNoiseCal -
		ph->Listen_GaindB_BfrNoiseCal);

	if (CHSPEC_IS2G(pi->radio_chanspec) && ((Delta_Listen_GaindB_Change > 3) ||
		(Delta_Listen_GaindB_Change < -3)))
		Delta_NfSubtractVal_Change = 4 * Delta_Listen_GaindB_Change;
	else
		Delta_NfSubtractVal_Change = 0;

	ph->NfSubtractVal_AfrNoiseCal = (ph->NfSubtractVal_BfrNoiseCal +
		Delta_NfSubtractVal_Change) & 0x3ff;

	if (phybw40 == 0)
		write_phy_reg(pi, SSLPNPHY_nfSubtractVal, ph->NfSubtractVal_AfrNoiseCal);
	else
		write_phy_reg(pi, SSLPNPHY_Rev2_nfSubtractVal_40,
			ph->NfSubtractVal_AfrNoiseCal);
}

void
wlc_sslpnphy_noise_measure(phy_info_t *pi)
{
	uint32 start_time = 0, timeout = 0;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint8 noise_measure_state, i;
	uint32 min_anp, max_anp, avg_noise = 0;
	bool measurement_valid;
	bool measurement_done = FALSE;
	uint32 IQ_Avg_Pwr = 0;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);

	PHY_TRACE(("wl%d: %s: begin\n", pi->sh->unit, __FUNCTION__));

	ph->sslpnphy_last_noise_cal = pi->sh->now;

	mod_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl,
		SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_MASK,
		1 << SSLPNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_SHIFT);


	if (ph->sslpnphy_init_noise_cal_done == 0) {
		noise_measure_state = INIT_FILL_FIFO;
		timeout = SSLPNPHY_INIT_NOISE_CAL_TMOUT;
	} else {
		noise_measure_state = PERIODIC_CAL;
		timeout = 0; /* a very small value for just one iteration */
	}

	wlc_sslpnphy_noise_measure_setup(pi);

	start_time = R_REG(pi->sh->osh, &pi->regs->tsf_timerlow);

	do {
		switch (noise_measure_state) {

		case INIT_FILL_FIFO:
			wlc_sslpnphy_noise_measure_time_window(pi,
				ph->sslpnphy_noise_measure_window, &min_anp, &max_anp,
				&measurement_valid);

			if (measurement_valid) {
				ph->sslpnphy_noisepwr_fifo_Min[ph->sslpnphy_noisepwr_fifo_filled]
					= min_anp;
				ph->sslpnphy_noisepwr_fifo_Max[ph->sslpnphy_noisepwr_fifo_filled]
					= max_anp;

				ph->sslpnphy_noisepwr_fifo_filled++;
			}

			if (ph->sslpnphy_noisepwr_fifo_filled == SSLPNPHY_NOISE_PWR_FIFO_DEPTH) {
				noise_measure_state = CHK_LISTEN_GAIN_CHANGE;
				ph->sslpnphy_noisepwr_fifo_filled = 0;
			}

			break;
		case PERIODIC_CAL:
			if (!(R_REG(pi->sh->osh, &pi->regs->maccommand) & MCMD_BG_NOISE)) {
				IQ_Avg_Pwr = wlc_sslpnphy_get_rxiq_accum(pi);
				if ((IQ_Avg_Pwr >= ph->sslpnphy_NPwr_MinLmt) &&
					(IQ_Avg_Pwr <= ph->sslpnphy_NPwr_MaxLmt)) {
				ph->sslpnphy_noisepwr_fifo_Min[ph->sslpnphy_noisepwr_fifo_filled] =
					IQ_Avg_Pwr;
				ph->sslpnphy_noisepwr_fifo_Max[ph->sslpnphy_noisepwr_fifo_filled] =
					IQ_Avg_Pwr;

				ph->sslpnphy_noisepwr_fifo_filled++;
				if (ph->sslpnphy_noisepwr_fifo_filled ==
					SSLPNPHY_NOISE_PWR_FIFO_DEPTH)
					ph->sslpnphy_noisepwr_fifo_filled = 0;
				}
			}
			break;

		case CHK_LISTEN_GAIN_CHANGE:
			wlc_sslpnphy_noise_fifo_avg(pi, &avg_noise);


			if ((avg_noise < ph->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise >= ph->sslpnphy_NPwr_MinLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, +1);

				wlc_sslpnphy_noise_fifo_init(pi);
				noise_measure_state = INIT_FILL_FIFO;

			} else if ((avg_noise > ph->sslpnphy_NPwr_LGC_MaxLmt) &&
				(avg_noise <= ph->sslpnphy_NPwr_MaxLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, -1);

				wlc_sslpnphy_noise_fifo_init(pi);
				noise_measure_state = INIT_FILL_FIFO;

			} else if ((avg_noise >= ph->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise <= ph->sslpnphy_NPwr_LGC_MaxLmt)) {

				noise_measure_state = CHANGE_RXPO;
			}

			break;

		case CHANGE_RXPO:
			wlc_sslpnphy_noise_measure_change_rxpo(pi, avg_noise);
			measurement_done = TRUE;
			break;

		default:
			break;

		}
	} while ((wlc_sslpnphy_abs_time(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow), start_time) <=
		timeout) && (!measurement_done));

	ph->Listen_RF_Gain = (wlapi_bmac_read_shm(pi->sh->physhim,
		(2 * (sslpnphy_shm_ptr + M_55f_REG_VAL))) & 0xff);

	if (!measurement_done) {

		if (!ph->sslpnphy_init_noise_cal_done && (ph->sslpnphy_noisepwr_fifo_filled == 0) &&
			(noise_measure_state == 0)) {
			PHY_TRACE(("Init Noise Cal Timedout After T %d uS And Noise_Cmd = %d:\n",
				wlc_sslpnphy_abs_time(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow),
				start_time), (R_REG(pi->sh->osh, &pi->regs->maccommand) &
				MCMD_BG_NOISE)));
		} else {

			avg_noise = wlc_sslpnphy_noise_fifo_min(pi);

			if ((avg_noise < ph->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise >= ph->sslpnphy_NPwr_MinLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, +1);

				wlc_sslpnphy_noise_fifo_init(pi);

			} else if ((avg_noise > ph->sslpnphy_NPwr_LGC_MaxLmt) &&
				(avg_noise <= ph->sslpnphy_NPwr_MaxLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, -1);

				wlc_sslpnphy_noise_fifo_init(pi);

			} else if ((avg_noise >= ph->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise <= ph->sslpnphy_NPwr_LGC_MaxLmt)) {

				wlc_sslpnphy_noise_measure_change_rxpo(pi, avg_noise);
			}
		}
	}


	wlc_sslpnphy_noise_measure_computeNf(pi);

	PHY_TRACE(("Phy Bw40:%d Noise Cal Stats After T %d uS:Npercal = %d"
		" FifoFil = %d Npwr = %d Rxpo = %d Gain_Set = %d"
		" Delta_Gain_Change = %d\n", IS40MHZ(pi),
		wlc_sslpnphy_abs_time(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow), start_time),
		ph->sslpnphy_init_noise_cal_done, ph->sslpnphy_noisepwr_fifo_filled, avg_noise,
		ph->rxpo_required_AfrNoiseCal, ph->Listen_GaindB_AfrNoiseCal,
		(int8)(ph->Listen_GaindB_AfrNoiseCal - ph->Listen_GaindB_BfrNoiseCal)));

	PHY_TRACE(("Get_RF_Gain = %d NfVal_Set = %d Base_Gain = %d Base_Rxpo = %d"
		" Base_NfVal = %d Noise_Measure_State = %d\n", ph->Listen_RF_Gain,
		ph->NfSubtractVal_AfrNoiseCal, ph->Listen_GaindB_BASE,
		ph->RxpowerOffset_Required_BASE, ph->NfSubtractVal_BASE, noise_measure_state));

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		PHY_TRACE(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			ph->sslpnphy_noisepwr_fifo_Min[i],
			ph->sslpnphy_noisepwr_fifo_Max[i]));
	}

	if (!ph->sslpnphy_init_noise_cal_done) {
		wlc_sslpnphy_detection_disable(pi, FALSE);

		ph->sslpnphy_init_noise_cal_done = TRUE;
	}

	if ((ph->sslpnphy_init_noise_cal_done == 1) && !ph->sslpnphy_disable_noise_percal) {
		wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_NOISE_SAMPLES)), 80 << phybw40);

		wlapi_bmac_write_shm(pi->sh->physhim, (2 * (sslpnphy_shm_ptr +
			M_55f_REG_VAL)), 0);
		OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
	}
}


void
wlc_sslpnphy_txpwr_target_adj(phy_info_t *pi, uint8 *tx_pwr_target, uint8 rate)
{

	uint8 cur_channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	phy_info_sslpnphy_t *pi_sslpn = pi->u.pi_sslpnphy;

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
#if defined(WLPLT)
		if (cur_channel == 7)	/* addressing corner lot power issues */
			tx_pwr_target[rate] = tx_pwr_target[rate] + 2;
		if (tx_pwr_target[rate] < 40) {
			tx_pwr_target[rate] = tx_pwr_target[rate] - 4;
		} else
#endif /* WLPLT */
		{
			if (rate > 11)
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_11n_backoff;
			else if ((rate >= 8) && (rate <= 11))
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_54_48_36_24mbps_backoff;
			else if (rate <= 3)
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_cck;
			else
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_lowerofdm;
		}
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {

		if (cur_channel == 1) {
			if (rate > 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 68);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 70);
		} else if (cur_channel == 11) {
			if (rate <= 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 70);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 64);
		} else {
			if (rate <= 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 72);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 72);
		}
	}
}

int BCMFASTPATH
wlc_sslpnphy_rssi_compute(phy_info_t *pi, int rssi, d11rxhdr_t *rxh)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint8 gidx = (ltoh16(rxh->PhyRxStatus_2) & 0xFC00) >> 10;

	if (rssi > 127)
		rssi -= 256;

	/* RSSI adjustment */
	rssi = rssi + sslpnphy_gain_index_offset_for_pkt_rssi[gidx];
	if ((rssi > -46) && (gidx > 18))
		rssi = rssi + 7;

	/* temperature compensation */
	rssi = rssi + ph->sslpnphy_pkteng_rssi_slope;

	/* 2dB compensation of path loss for 4329 on Ref Boards */
	rssi = rssi + 2;

	return rssi;
}
#if defined(WLTEST)
void
wlc_sslpnphy_pkteng_stats_get(phy_info_t *pi, wl_pkteng_stats_t *stats)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	int16 rssi_sslpn1, rssi_sslpn2, rssi_sslpn3, rssi_sslpn4;
	int16 snr_a_sslpn1, snr_a_sslpn2, snr_a_sslpn3, snr_a_sslpn4;
	int16 snr_b_sslpn1, snr_b_sslpn2, snr_b_sslpn3, snr_b_sslpn4;
	uint8 gidx1, gidx2, gidx3, gidx4;
	int8 snr1, snr2, snr3, snr4;
	uint freq;

	rssi_sslpn1 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_0);
	rssi_sslpn2 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_1);
	rssi_sslpn3 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_2);
	rssi_sslpn4 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_3);

	gidx1 = (wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_0) & 0xfe00) >> 9;
	gidx2 = (wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_1) & 0xfe00) >> 9;
	gidx3 = (wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_2) & 0xfe00) >> 9;
	gidx4 = (wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_3) & 0xfe00) >> 9;

	rssi_sslpn1 = rssi_sslpn1 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx1];
	if (((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_0)) & 0x0100) &&
		(gidx1 == 12))
		rssi_sslpn1 = rssi_sslpn1 - 4;
	if ((rssi_sslpn1 > -46) && (gidx1 > 18))
		rssi_sslpn1 = rssi_sslpn1 + 6;
	rssi_sslpn2 = rssi_sslpn2 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx2];
	if (((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_1)) & 0x0100) &&
		(gidx2 == 12))
		rssi_sslpn2 = rssi_sslpn2 - 4;
	if ((rssi_sslpn2 > -46) && (gidx2 > 18))
		rssi_sslpn2 = rssi_sslpn2 + 6;
	rssi_sslpn3 = rssi_sslpn3 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx3];
	if (((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_2)) & 0x0100) &&
		(gidx3 == 12))
		rssi_sslpn3 = rssi_sslpn3 - 4;
	if ((rssi_sslpn3 > -46) && (gidx3 > 18))
		rssi_sslpn3 = rssi_sslpn3 + 6;
	rssi_sslpn4 = rssi_sslpn4 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx4];
	if (((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_3)) & 0x0100) &&
		(gidx4 == 12))
		rssi_sslpn4 = rssi_sslpn4 - 4;
	if ((rssi_sslpn4 > -46) && (gidx4 > 18))
		rssi_sslpn4 = rssi_sslpn4 + 6;
	stats->rssi = (rssi_sslpn1 + rssi_sslpn2 + rssi_sslpn3 + rssi_sslpn4) >> 2;

	/* Channel Correction Factor */
	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((freq > 2427) && (freq <= 2467))
		stats->rssi = stats->rssi - 1;
	/* temperature compensation */
	stats->rssi = stats->rssi + ph->sslpnphy_pkteng_rssi_slope;

	/* 2dB compensation of path loss for 4329 on Ref Boards */
	stats->rssi = stats->rssi + 2;
	if (((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_1)) & 0x100) &&
		(stats->rssi > -84) && (stats->rssi < -33))
		stats->rssi = stats->rssi + 3;

	/* SNR */
	snr_a_sslpn1 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_A_0);
	snr_b_sslpn1 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_B_0);
	snr1 = ((snr_a_sslpn1 - snr_b_sslpn1)* 3) >> 5;
	if ((stats->rssi < -92) ||
		(((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_0)) & 0x100)))
		snr1 = stats->rssi + 94;

	if (snr1 > 31)
		snr1 = 31;

	snr_a_sslpn2 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_A_1);
	snr_b_sslpn2 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_B_1);
	snr2 = ((snr_a_sslpn2 - snr_b_sslpn2)* 3) >> 5;
	if ((stats->rssi < -92) ||
		(((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_1)) & 0x100)))
		snr2 = stats->rssi + 94;

	if (snr2 > 31)
		snr2 = 31;

	snr_a_sslpn3 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_A_2);
	snr_b_sslpn3 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_B_2);
	snr3 = ((snr_a_sslpn3 - snr_b_sslpn3)* 3) >> 5;
	if ((stats->rssi < -92) ||
		(((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_2)) & 0x100)))
		snr3 = stats->rssi + 94;

	if (snr3 > 31)
		snr3 = 31;

	snr_a_sslpn4 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_A_3);
	snr_b_sslpn4 = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_SNR_B_3);
	snr4 = ((snr_a_sslpn4 - snr_b_sslpn4)* 3) >> 5;
	if ((stats->rssi < -92) ||
		(((wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPN_RSSI_3)) & 0x100)))
		snr4 = stats->rssi + 94;

	if (snr4 > 31)
		snr4 = 31;

	stats->snr = ((snr1 + snr2 + snr3 + snr4)/4);
}
#endif 

int
wlc_sslpnphy_txpwr_idx_get(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	int32 ret_int;

	if (wlc_phy_tpc_isenabled_sslpnphy(pi)) {
		/* Update current power index */
		wlc_sslpnphy_tx_pwr_update_npt(pi);
		ret_int = ph->sslpnphy_tssi_idx;
	} else
		ret_int = ph->sslpnphy_tx_power_idx_override;

	return ret_int;
}

void
wlc_sslpnphy_iovar_papd_debug(phy_info_t *pi, void *a)
{

	wl_sslpnphy_papd_debug_data_t papd_debug_data;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	papd_debug_data.psat_pwr = ph->sslpnphy_psat_pwr;
	papd_debug_data.psat_indx = ph->sslpnphy_psat_indx;
	papd_debug_data.min_phase = ph->sslpnphy_min_phase;
	papd_debug_data.final_idx = ph->sslpnphy_final_idx;
	papd_debug_data.start_idx = ph->sslpnphy_start_idx;

	bcopy(&papd_debug_data, a, sizeof(wl_sslpnphy_papd_debug_data_t));
}

void
wlc_sslpnphy_iovar_txpwrctrl(phy_info_t *pi, int32 int_val)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	wlc_sslpnphy_set_tx_pwr_ctrl(pi,
		int_val ? SSLPNPHY_TX_PWR_CTRL_HW : SSLPNPHY_TX_PWR_CTRL_SW);
	/* Reset calibration */
	ph->sslpnphy_full_cal_channel = 0;
	pi->phy_forcecal = TRUE;

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}
