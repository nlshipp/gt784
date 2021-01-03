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
 * $Id: wlc_phy_lcn.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
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
#include <wlc_phy_lcn.h>
#include <sbchipc.h>

#include <wlc_phyreg_lcn.h>
#include <wlc_phytbl_lcn.h>
/* contains settings from BCM2064_JTAG.xls */
#define PLL_2064_NDIV		90
#define PLL_2064_LOW_END_VCO 	3000
#define PLL_2064_LOW_END_KVCO 	27
#define PLL_2064_HIGH_END_VCO	4200
#define PLL_2064_HIGH_END_KVCO	68
#define PLL_2064_LOOP_BW_DOUBLER	200
#define PLL_2064_D30_DOUBLER		10500	/* desired value of PLL_lf_r1(ohm) */
#define PLL_2064_LOOP_BW	260
#define PLL_2064_D30		8000	/* desired value of PLL_lf_r1(ohm) */
#define PLL_2064_CAL_REF_TO	8 	/* PLL cal ref timeout */
#define PLL_2064_MHZ		1000000
#define PLL_2064_OPEN_LOOP_DELAY	5

#define TEMPSENSE 			1
#define VBATSENSE           2

#define NOISE_IF_UPD_CHK_INTERVAL	1	/* s */
#define NOISE_IF_UPD_RST_INTERVAL	60	/* s */
#define NOISE_IF_UPD_THRESHOLD_CNT	1
#define NOISE_IF_UPD_TRHRESHOLD	50
#define NOISE_IF_UPD_TIMEOUT		1000	/* uS */
#define NOISE_IF_OFF			0
#define NOISE_IF_CHK			1
#define NOISE_IF_ON			2

#define PAPD_BLANKING_PROFILE 		3
#define PAPD2LUT			0
#define PAPD_CORR_NORM 			0
#define PAPD_BLANKING_THRESHOLD 	0
#define PAPD_STOP_AFTER_LAST_UPDATE	0

#define LCN_TARGET_TSSI  30
#define LCN_TARGET_PWR  60

#define LCN_VBAT_OFFSET_433X 34649679 /* 528.712121 * (2 ^ 16) */
#define LCN_VBAT_SLOPE_433X  8258032 /* 126.007576 * (2 ^ 16) */

#define LCN_VBAT_SCALE_NOM  53	/* 3.3 * (2 ^ 4) */
#define LCN_VBAT_SCALE_DEN  432

#define LCN_TEMPSENSE_OFFSET  80812 /* 78.918 << 10 */
#define LCN_TEMPSENSE_DEN  2647	/* 2.5847 << 10 */

/* %%%%%% LCNPHY macros/structure */

#define LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(LCNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT + 8)
#define LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(LCNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT + 8)
#define LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define wlc_lcnphy_enable_tx_gain_override(pi) \
	wlc_lcnphy_set_tx_gain_override(pi, TRUE)
#define wlc_lcnphy_disable_tx_gain_override(pi) \
	wlc_lcnphy_set_tx_gain_override(pi, FALSE)

/* Turn off all the crs signals to the MAC */
#define wlc_lcnphy_iqcal_active(pi)	\
	(read_phy_reg((pi), LCNPHY_iqloCalCmd) & \
	(LCNPHY_iqloCalCmd_iqloCalCmd_MASK | LCNPHY_iqloCalCmd_iqloCalDFTCmd_MASK))

#define txpwrctrl_off(pi) (0x7 != ((read_phy_reg(pi, LCNPHY_TxPwrCtrlCmd) & 0xE000) >> 13))
#define wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) \
	(pi->temppwrctrl_capable)
#define wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) \
	(pi->hwpwrctrl_capable)


#define SWCTRL_BT_TX		0x18
#define SWCTRL_OVR_DISABLE	0x40

#define	AFE_CLK_INIT_MODE_TXRX2X	1
#define	AFE_CLK_INIT_MODE_PAPD		0

#define LCNPHY_TBL_ID_IQLOCAL			0x00
/* #define LCNPHY_TBL_ID_TXPWRCTL 		0x07 move to wlc_phy_int.h */
#define LCNPHY_TBL_ID_RFSEQ         0x08
#define LCNPHY_TBL_ID_GAIN_IDX		0x0d
#define LCNPHY_TBL_ID_SW_CTRL			0x0f
#define LCNPHY_TBL_ID_GAIN_TBL		0x12
#define LCNPHY_TBL_ID_SPUR			0x14
#define LCNPHY_TBL_ID_SAMPLEPLAY		0x15
#define LCNPHY_TBL_ID_SAMPLEPLAY1		0x16

#define LCNPHY_TX_PWR_CTRL_RATE_OFFSET 	832
#define LCNPHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define LCNPHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define LCNPHY_TX_PWR_CTRL_IQ_OFFSET		320
#define LCNPHY_TX_PWR_CTRL_LO_OFFSET		448
#define LCNPHY_TX_PWR_CTRL_PWR_OFFSET		576

#define LCNPHY_TX_PWR_CTRL_START_INDEX_2G	35
#define LCNPHY_TX_PWR_CTRL_START_INDEX_2G_4313	90

#define LCNPHY_TX_PWR_CTRL_START_NPT		1
#define LCNPHY_TX_PWR_CTRL_MAX_NPT			7

#define LCNPHY_NOISE_SAMPLES_DEFAULT 5000

#define LCNPHY_ACI_DETECT_START      1
#define LCNPHY_ACI_DETECT_PROGRESS   2
#define LCNPHY_ACI_DETECT_STOP       3

#define LCNPHY_ACI_CRSHIFRMLO_TRSH 100
#define LCNPHY_ACI_GLITCH_TRSH 2000
#define	LCNPHY_ACI_TMOUT 250		/* Time for CRS HI and FRM LO (in micro seconds) */
#define LCNPHY_ACI_DETECT_TIMEOUT  2	/* in  seconds */
#define LCNPHY_ACI_START_DELAY 0

#define wlc_lcnphy_tx_gain_override_enabled(pi) \
	(0 != (read_phy_reg((pi), LCNPHY_AfeCtrlOvr) & LCNPHY_AfeCtrlOvr_dacattctrl_ovr_MASK))

#define wlc_lcnphy_total_tx_frames(pi) \
	wlapi_bmac_read_shm((pi)->sh->physhim, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txallfrm))

typedef struct {
	uint16 gm_gain;
	uint16 pga_gain;
	uint16 pad_gain;
	uint16 dac_gain;
} lcnphy_txgains_t;

typedef enum {
	LCNPHY_CAL_FULL,
	LCNPHY_CAL_RECAL,
	LCNPHY_CAL_CURRECAL,
	LCNPHY_CAL_DIGCAL,
	LCNPHY_CAL_GCTRL
} lcnphy_cal_mode_t;

typedef struct {
	lcnphy_txgains_t gains;
	bool useindex;
	uint8 index;
} lcnphy_txcalgains_t;

typedef struct {
	uint8 chan;
	int16 a;
	int16 b;
} lcnphy_rx_iqcomp_t;

typedef struct {
	int16 re;
	int16 im;
} lcnphy_spb_tone_t;

typedef struct {
	uint16 re;
	uint16 im;
} lcnphy_unsign16_struct;

typedef struct {
	uint32 iq_prod;
	uint32 i_pwr;
	uint32 q_pwr;
} lcnphy_iq_est_t;

typedef struct {
	uint16 ptcentreTs20;
	uint16 ptcentreFactor;
} lcnphy_sfo_cfg_t;

typedef enum {
	LCNPHY_PAPD_CAL_CW,
	LCNPHY_PAPD_CAL_OFDM
} lcnphy_papd_cal_type_t;

/* LCNPHY IQCAL parameters for various Tx gain settings */
/* table format: */
/*	target, gm, pga, pad, ncorr for each of 5 cal types */
typedef uint16 iqcal_gain_params_lcnphy[9];

static const iqcal_gain_params_lcnphy tbl_iqcal_gainparams_lcnphy_2G[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

static const iqcal_gain_params_lcnphy *tbl_iqcal_gainparams_lcnphy[1] = {
	tbl_iqcal_gainparams_lcnphy_2G,
	};

static const uint16 iqcal_gainparams_numgains_lcnphy[1] = {
	sizeof(tbl_iqcal_gainparams_lcnphy_2G) / sizeof(*tbl_iqcal_gainparams_lcnphy_2G),
	};

static const lcnphy_sfo_cfg_t lcnphy_sfo_cfg[] = {
	{965, 1087},
	{967, 1085},
	{969, 1082},
	{971, 1080},
	{973, 1078},
	{975, 1076},
	{977, 1073},
	{979, 1071},
	{981, 1069},
	{983, 1067},
	{985, 1065},
	{987, 1063},
	{989, 1060},
	{994, 1055}
};

/* LO Comp Gain ladder. Format: {m genv} */
static const
uint16 lcnphy_iqcal_loft_gainladder[]  = {
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
static const
uint16 lcnphy_iqcal_ir_gainladder[] = {
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

/* reference waveform autogenerated , freq 3.75 MHz, amplitude 88 */
static const
lcnphy_spb_tone_t lcnphy_spb_tone_3750[] = {
	{88, 0	 },
	{73, 49	 },
	{34, 81	 },
	{-17, 86 },
	{-62, 62 },
	{-86, 17 },
	{-81, -34 },
	{-49, -73 },
	{0, -88 },
	{49, -73 },
	{81, -34 },
	{86, 17 },
	{62, 62 },
	{17, 86 },
	{-34, 81 },
	{-73, 49 },
	{-88, 0 },
	{-73, -49 },
	{-34, -81 },
	{17, -86 },
	{62, -62 },
	{86, -17 },
	{81, 34	 },
	{49, 73	 },
	{0, 88 },
	{-49, 73 },
	{-81, 34 },
	{-86, -17 },
	{-62, -62 },
	{-17, -86 },
	{34, -81 },
	{73, -49 },
	};

/* these rf registers need to be restored after iqlo_soft_cal_full */
static const
uint16 iqlo_loopback_rf_regs[20] = {
	RADIO_2064_REG036,
	RADIO_2064_REG11A,
	RADIO_2064_REG03A,
	RADIO_2064_REG025,
	RADIO_2064_REG028,
	RADIO_2064_REG005,
	RADIO_2064_REG112,
	RADIO_2064_REG0FF,
	RADIO_2064_REG11F,
	RADIO_2064_REG00B,
	RADIO_2064_REG113,
	RADIO_2064_REG007,
	RADIO_2064_REG0FC,
	RADIO_2064_REG0FD,
	RADIO_2064_REG012,
	RADIO_2064_REG057,
	RADIO_2064_REG059,
	RADIO_2064_REG05C,
	RADIO_2064_REG078,
	RADIO_2064_REG092,
	};

static const
uint16 tempsense_phy_regs[14] = {
	LCNPHY_auxadcCtrl,
	LCNPHY_TxPwrCtrlCmd,
	LCNPHY_TxPwrCtrlRangeCmd,
	LCNPHY_TxPwrCtrlRfCtrl0,
	LCNPHY_TxPwrCtrlRfCtrl1,
	LCNPHY_TxPwrCtrlIdleTssi,
	LCNPHY_rfoverride4,
	LCNPHY_rfoverride4val,
	LCNPHY_TxPwrCtrlRfCtrlOverride1,
	LCNPHY_TxPwrCtrlRangeCmd,
	LCNPHY_TxPwrCtrlRfCtrlOverride0,
	LCNPHY_TxPwrCtrlNnum,
	LCNPHY_TxPwrCtrlNum_Vbat,
	LCNPHY_TxPwrCtrlNum_temp,
	};

/* these rf registers need to be restored after rxiq_cal */
static const
uint16 rxiq_cal_rf_reg[11] = {
	RADIO_2064_REG098,
	RADIO_2064_REG116,
	RADIO_2064_REG12C,
	RADIO_2064_REG06A,
	RADIO_2064_REG00B,
	RADIO_2064_REG01B,
	RADIO_2064_REG113,
	RADIO_2064_REG01D,
	RADIO_2064_REG114,
	RADIO_2064_REG02E,
	RADIO_2064_REG12A,
	};

/* Autogenerated by 2064_chantbl_tcl2c.tcl */
static const
lcnphy_rx_iqcomp_t lcnphy_rx_iqcomp_table_rev0[] = {
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

/* The 23bit gaincode has been constructed like this - */
/* 22(extlna),21(tr),(20:2)19bit gain code, (1-0) lna1 */
static const uint32 lcnphy_23bitgaincode_table[] = {
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000100,
	0x00000001,
	0x00000101,
	0x00000201,
	0x00000011,
	0x00000111,
	0x00000211,
	0x00000311,
	0x00001311,
	0x00002311,
	0x00000313,
	0x00001313,
	0x00002313,
	0x00003313,
	0x00001333,
	0x00002333,
	0x00003333,
	0x00003433,
	0x00004433,
	0x00014433,
	0x00024433,
	0x00025433,
	0x00035433,
	0x00045433,
	0x00046433,
	0x00146433,
	0x00246433,
	0x00346433,
	0x00446433
};

static const int8 lcnphy_gain_table[] = {
	-16,
	-13,
	10,
	7,
	4,
	0,
	3,
	6,
	9,
	12,
	15,
	18,
	21,
	24,
	27,
	30,
	33,
	36,
	39,
	42,
	45,
	48,
	50,
	53,
	56,
	59,
	62,
	65,
	68,
	71,
	74,
	77,
	80,
	83,
	86,
	89,
	92,
	};

static const int8 lcnphy_gain_index_offset_for_rssi[] = {
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

extern CONST uint8 spur_tbl_rev0[];
extern CONST uint32 dot11lcnphytbl_rx_gain_info_sz_rev1;
extern CONST dot11lcnphytbl_info_t dot11lcnphytbl_rx_gain_info_rev1[];
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_bt_epa;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250;
extern CONST dot11lcnphytbl_info_t dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250_single;
/* channel info type for 2064 radio used in lcnphy */
typedef struct _chan_info_2064_lcnphy {
	uint   chan;            /* channel number */
	uint   freq;            /* in Mhz */
	uint8 logen_buftune;
	uint8 logen_rccr_tx;
	uint8 txrf_mix_tune_ctrl;
	uint8 pa_input_tune_g;
	uint8 logen_rccr_rx;
	uint8 pa_rxrf_lna1_freq_tune;
	uint8 pa_rxrf_lna2_freq_tune;
	uint8 rxrf_rxrf_spare1;
} chan_info_2064_lcnphy_t;

/* Autogenerated by 2064_chantbl_tcl2c.tcl */
static chan_info_2064_lcnphy_t chan_info_2064_lcnphy[] = {
{   1, 2412, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   2, 2417, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   3, 2422, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   4, 2427, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   5, 2432, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   6, 2437, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   7, 2442, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   8, 2447, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{   9, 2452, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{  10, 2457, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{  11, 2462, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{  12, 2467, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{  13, 2472, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
{  14, 2484, 0x0B, 0x0A, 0x00, 0x07, 0x0A, 0x88, 0x88, 0x80 },
};

/* channel info type for 2066 radio used in lcnphy */
typedef struct _chan_info_2066_lcnphy {
	uint   chan;            /* channel number */		
	uint   freq;            /* in Mhz */
	uint8 logen_buftune;
	uint8 logen_rccr_tx;
	uint8 txrf_mix_tune_ctrl;
	uint8 pa_input_tune_g;
	uint8 pa_rxrf_lna1_freq_tune;
	uint8 rxrf_rxrf_spare1;
	uint8 pad;
	uint8 pad1;
} chan_info_2066_lcnphy_t;

/* a band channel info type for 2066 radio used in lcnphy */
typedef struct _chan_info_2066_a_lcnphy {
	uint   chan;            /* channel number */		
	uint   freq;            /* in Mhz */
	uint8 logen_buftune;
	uint8 ctune;
	uint8 logen_rccr;
	uint8 mix_tune_5g;
	uint8 pga_tune_ctrl;
	uint8 pa_input_tune_5g;
	uint8 pa_rxrf_lna1_freq_tune;
	uint8 rxrf_rxrf_spare1;
} chan_info_2066_a_lcnphy_t;

/* Autogenerated by 2066_chantbl_tcl2c.tcl */
static chan_info_2066_lcnphy_t chan_info_2066_lcnphy[] = {
{   1, 2412, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   2, 2417, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   3, 2422, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   4, 2427, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   5, 2432, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   6, 2437, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   7, 2442, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   8, 2447, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{   9, 2452, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  10, 2457, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  11, 2462, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  12, 2467, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  13, 2472, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  14, 2484, 0x01, 0x0A, 0x00, 0x07, 0x8F, 0x01, 0x00, 0x00 },
{  34, 5170, 0x0C, 0xCB, 0x99, 0x0C, 0x0C, 0x0B, 0xCC, 0x0F },
{  38, 5190, 0x0B, 0xBB, 0x99, 0x0C, 0x0B, 0x0B, 0xCC, 0x0F },
{  42, 5210, 0x0B, 0xBA, 0x99, 0x0B, 0x0B, 0x0B, 0xBB, 0x0F },
{  46, 5230, 0x0B, 0xBA, 0x99, 0x0B, 0x0B, 0x0A, 0xBB, 0x0F },
{  36, 5180, 0x0C, 0xCB, 0x99, 0x0C, 0x0B, 0x0B, 0xCC, 0x0F },
{  40, 5200, 0x0B, 0xBB, 0x99, 0x0B, 0x0B, 0x0B, 0xBB, 0x0F },
{  44, 5220, 0x0B, 0xBA, 0x99, 0x0B, 0x0B, 0x0B, 0xBB, 0x0F },
{  48, 5240, 0x0B, 0xBA, 0x99, 0x0B, 0x0B, 0x0A, 0xBB, 0x0F },
{  52, 5260, 0x0A, 0xAA, 0x99, 0x0B, 0x0A, 0x0A, 0xBB, 0x0F },
{  56, 5280, 0x0A, 0xAA, 0x99, 0x0A, 0x0A, 0x0A, 0xAA, 0x0E },
{  60, 5300, 0x0A, 0xA9, 0x99, 0x0A, 0x0A, 0x09, 0xAA, 0x0E },
{  64, 5320, 0x0A, 0xA9, 0x99, 0x0A, 0x09, 0x09, 0xAA, 0x0E },
{ 100, 5500, 0x07, 0x77, 0xAA, 0x07, 0x07, 0x07, 0x77, 0x0B },
{ 104, 5520, 0x07, 0x77, 0xAA, 0x07, 0x07, 0x06, 0x77, 0x0B },
{ 108, 5540, 0x07, 0x76, 0xAA, 0x07, 0x06, 0x06, 0x77, 0x0B },
{ 112, 5560, 0x06, 0x76, 0xAA, 0x06, 0x06, 0x06, 0x66, 0x0A },
{ 116, 5580, 0x06, 0x76, 0xAA, 0x06, 0x06, 0x05, 0x66, 0x0A },
{ 120, 5600, 0x06, 0x76, 0xAA, 0x06, 0x05, 0x05, 0x66, 0x0A },
{ 124, 5620, 0x06, 0x76, 0xAA, 0x05, 0x05, 0x05, 0x66, 0x09 },
{ 128, 5640, 0x05, 0x55, 0xAA, 0x05, 0x05, 0x05, 0x55, 0x09 },
{ 132, 5660, 0x05, 0x55, 0xBB, 0x05, 0x05, 0x04, 0x55, 0x09 },
{ 136, 5680, 0x05, 0x55, 0xBB, 0x05, 0x04, 0x04, 0x55, 0x09 },
{ 140, 5700, 0x05, 0x55, 0xBB, 0x04, 0x04, 0x04, 0x44, 0x08 },
{ 149, 5745, 0x04, 0x44, 0xBB, 0x03, 0x03, 0x03, 0x44, 0x07 },
{ 153, 5765, 0x04, 0x44, 0xBB, 0x03, 0x02, 0x02, 0x33, 0x07 },
{ 157, 5785, 0x04, 0x44, 0xBB, 0x02, 0x02, 0x02, 0x22, 0x06 },
{ 161, 5805, 0x04, 0x44, 0xBB, 0x01, 0x01, 0x01, 0x22, 0x06 },
{ 165, 5825, 0x03, 0x33, 0xBB, 0x01, 0x01, 0x00, 0x11, 0x05 },
{ 184, 4920, 0x0F, 0xFE, 0x88, 0x0F, 0x0F, 0x0F, 0xFF, 0x0F },
{ 188, 4940, 0x0F, 0xFE, 0x88, 0x0F, 0x0F, 0x0F, 0xFF, 0x0F },
{ 192, 4960, 0x0F, 0xFE, 0x88, 0x0F, 0x0F, 0x0E, 0xFF, 0x0F },
{ 196, 4980, 0x0F, 0xFE, 0x88, 0x0F, 0x0E, 0x0E, 0xFF, 0x0F },
{ 200, 5000, 0x0F, 0xFD, 0x88, 0x0E, 0x0E, 0x0E, 0xEE, 0x0F },
{ 204, 5020, 0x0E, 0xED, 0x99, 0x0E, 0x0E, 0x0D, 0xEE, 0x0F },
{ 208, 5040, 0x0E, 0xED, 0x99, 0x0E, 0x0D, 0x0D, 0xEE, 0x0F },
{ 212, 5060, 0x0E, 0xEC, 0x99, 0x0D, 0x0D, 0x0D, 0xDD, 0x0F },
{ 216, 5080, 0x0D, 0xDC, 0x99, 0x0D, 0x0D, 0x0D, 0xDD, 0x0F },
};

/* Init values for 2066 regs (autogenerated by 2066_regs_tcl2c.tcl)
 *   Entries: addr, init value A, init value G, do_init A, do_init G.
 *   Last line (addr FF is dummy as delimiter. This table has dual use
 *   between dumping and initializing.
 */
lcnphy_radio_regs_t lcnphy_radio_regs_2066[] = {
	{ 0x00,             0,             0,   0,   0  },
	{ 0x01,          0x66,          0x66,   0,   0  },
	{ 0x02,          0x20,          0x20,   0,   0  },
	{ 0x03,          0x66,          0x66,   0,   0  },
	{ 0x04,          0xc8,          0xc8,   0,   0  },
	{ 0x05,             0,           0x8,   0,   1  },
	{ 0x06,          0x10,          0x10,   0,   0  },
	{ 0x07,             0,             0,   0,   0  },
	{ 0x08,             0,             0,   0,   0  },
	{ 0x09,             0,             0,   0,   0  },
	{ 0x0A,          0x37,          0x37,   0,   0  },
	{ 0x0B,           0x6,           0x6,   0,   0  },
	{ 0x0C,          0x55,          0x55,   0,   0  },
	{ 0x0D,          0x8b,          0x8b,   0,   0  },
	{ 0x0E,             0,             0,   0,   0  },
	{ 0x0F,           0x5,           0x5,   0,   0  },
	{ 0x10,             0,             0,   0,   0  },
	{ 0x11,           0xe,           0xe,   0,   0  },
	{ 0x12,             0,             0,   0,   0  },
	{ 0x13,           0xb,           0xb,   0,   0  },
	{ 0x14,           0x2,           0x2,   0,   0  },
	{ 0x15,          0x12,          0x12,   0,   0  },
	{ 0x16,          0x12,          0x12,   0,   0  },
	{ 0x17,           0xc,           0xc,   0,   0  },
	{ 0x18,           0xc,           0xc,   0,   0  },
	{ 0x19,           0xc,           0xc,   0,   0  },
	{ 0x1A,           0x8,           0x8,   0,   0  },
	{ 0x1B,           0x2,           0x2,   0,   0  },
	{ 0x1C,             0,             0,   0,   0  },
	{ 0x1D,           0x1,           0x1,   0,   0  },
	{ 0x1E,          0x12,          0x12,   0,   0  },
	{ 0x1F,          0x6e,          0x6e,   0,   0  },
	{ 0x20,           0x2,           0x2,   0,   0  },
	{ 0x21,          0x23,          0x23,   0,   0  },
	{ 0x22,           0x8,           0x8,   0,   0  },
	{ 0x23,             0,             0,   0,   0  },
	{ 0x24,             0,             0,   0,   0  },
	{ 0x25,           0xc,           0xc,   0,   0  },
	{ 0x26,          0x33,          0x33,   0,   0  },
	{ 0x27,          0x55,          0x55,   0,   0  },
	{ 0x28,           0xe,           0xe,   0,   0  },
	{ 0x29,          0x30,          0x30,   0,   0  },
	{ 0x2A,           0xb,           0xb,   0,   0  },
	{ 0x2B,          0x1b,          0x1b,   0,   0  },
	{ 0x2C,           0x3,           0x3,   0,   0  },
	{ 0x2D,          0x1b,          0x1b,   0,   0  },
	{ 0x2E,             0,             0,   0,   0  },
	{ 0x2F,          0x20,          0x20,   0,   0  },
	{ 0x30,           0xa,           0xa,   0,   0  },
	{ 0x31,             0,             0,   0,   0  },
	{ 0x32,          0x62,          0x62,   0,   0  },
	{ 0x33,          0x19,          0x19,   0,   0  },
	{ 0x34,          0x33,          0x33,   0,   0  },
	{ 0x35,          0x77,          0x77,   0,   0  },
	{ 0x36,             0,             0,   0,   0  },
	{ 0x37,          0x70,          0x70,   0,   0  },
	{ 0x38,           0x3,           0x3,   0,   0  },
	{ 0x39,           0xf,           0xf,   0,   0  },
	{ 0x3A,           0x1,           0x1,   0,   0  },
	{ 0x3B,          0xcf,          0xcf,   0,   0  },
	{ 0x3C,          0x1a,          0x1a,   0,   0  },
	{ 0x3D,           0x6,           0x6,   0,   0  },
	{ 0x3E,          0x42,          0x42,   0,   0  },
	{ 0x3F,             0,             0,   0,   0  },
	{ 0x40,          0xfb,          0xfb,   0,   0  },
	{ 0x41,          0x9a,          0x9a,   0,   0  },
	{ 0x42,          0x7a,          0x7a,   0,   0  },
	{ 0x43,          0x29,          0x29,   0,   0  },
	{ 0x44,             0,             0,   0,   0  },
	{ 0x45,           0x8,           0x8,   0,   0  },
	{ 0x46,          0xce,          0xce,   0,   0  },
	{ 0x47,          0x27,          0x27,   0,   0  },
	{ 0x48,          0x62,          0x62,   0,   0  },
	{ 0x49,           0x6,           0x6,   0,   0  },
	{ 0x4A,          0x58,          0x58,   0,   0  },
	{ 0x4B,          0xf7,          0xf7,   0,   0  },
	{ 0x4C,             0,             0,   0,   0  },
	{ 0x4D,          0xb3,          0xb3,   0,   0  },
	{ 0x4E,             0,             0,   0,   0  },
	{ 0x4F,           0x2,           0x2,   0,   0  },
	{ 0x50,             0,             0,   0,   0  },
	{ 0x51,           0x9,           0x9,   0,   0  },
	{ 0x52,           0x5,           0x5,   0,   0  },
	{ 0x53,          0x17,          0x17,   0,   0  },
	{ 0x54,          0x38,          0x38,   0,   0  },
	{ 0x55,             0,             0,   0,   0  },
	{ 0x56,             0,             0,   0,   0  },
	{ 0x57,           0xb,           0xb,   0,   0  },
	{ 0x58,             0,             0,   0,   0  },
	{ 0x59,             0,             0,   0,   0  },
	{ 0x5A,             0,             0,   0,   0  },
	{ 0x5B,             0,             0,   0,   0  },
	{ 0x5C,             0,             0,   0,   0  },
	{ 0x5D,             0,             0,   0,   0  },
	{ 0x5E,          0x88,          0x88,   0,   0  },
	{ 0x5F,          0x6c,          0x6c,   0,   0  },
	{ 0x60,          0x74,          0x74,   0,   0  },
	{ 0x61,          0x74,          0x74,   0,   0  },
	{ 0x62,          0x74,          0x74,   0,   0  },
	{ 0x63,          0x44,          0x44,   0,   0  },
	{ 0x64,          0x77,          0x77,   0,   0  },
	{ 0x65,          0x44,          0x44,   0,   0  },
	{ 0x66,          0x77,          0x77,   0,   0  },
	{ 0x67,          0x55,          0x55,   0,   0  },
	{ 0x68,          0x77,          0x77,   0,   0  },
	{ 0x69,          0x77,          0x77,   0,   0  },
	{ 0x6A,             0,             0,   0,   0  },
	{ 0x6B,          0x7f,          0x7f,   0,   0  },
	{ 0x6C,           0x8,           0x8,   0,   0  },
	{ 0x6D,             0,             0,   0,   0  },
	{ 0x6E,          0x88,          0x88,   0,   0  },
	{ 0x6F,          0x66,          0x66,   0,   0  },
	{ 0x70,          0x66,          0x66,   0,   0  },
	{ 0x71,          0x28,          0x28,   0,   0  },
	{ 0x72,          0x55,          0x55,   0,   0  },
	{ 0x73,             0,             0,   0,   1  },
	{ 0x74,             0,             0,   0,   0  },
	{ 0x75,             0,             0,   0,   0  },
	{ 0x76,             0,             0,   0,   0  },
	{ 0x77,           0x1,           0x1,   0,   0  },
	{ 0x78,          0x96,          0x96,   0,   0  },
	{ 0x79,             0,             0,   0,   0  },
	{ 0x7A,             0,             0,   0,   0  },
	{ 0x7B,             0,             0,   0,   0  },
	{ 0x7C,             0,             0,   0,   0  },
	{ 0x7D,             0,             0,   0,   0  },
	{ 0x7E,             0,             0,   0,   0  },
	{ 0x7F,             0,             0,   0,   0  },
	{ 0x80,             0,             0,   0,   0  },
	{ 0x81,             0,             0,   0,   0  },
	{ 0x82,             0,             0,   0,   0  },
	{ 0x83,          0xf5,          0xf5,   0,   0  },
	{ 0x84,             0,             0,   0,   0  },
	{ 0x85,          0x30,          0x30,   0,   0  },
	{ 0x86,             0,             0,   0,   0  },
	{ 0x87,          0xff,          0xff,   0,   0  },
	{ 0x88,           0x7,           0x7,   0,   0  },
	{ 0x89,          0x77,          0x77,   0,   0  },
	{ 0x8A,          0x77,          0x77,   0,   0  },
	{ 0x8B,          0x77,          0x77,   0,   0  },
	{ 0x8C,          0x77,          0x77,   0,   0  },
	{ 0x8D,           0x8,           0x8,   0,   0  },
	{ 0x8E,           0xa,           0xa,   0,   0  },
	{ 0x8F,           0x8,           0x8,   0,   0  },
	{ 0x90,           0x8,           0x8,   0,   0  },
	{ 0x91,           0x5,           0x5,   0,   0  },
	{ 0x92,          0x1f,          0x1f,   0,   0  },
	{ 0x93,             0,             0,   0,   0  },
	{ 0x94,           0x3,           0x3,   0,   0  },
	{ 0x95,             0,             0,   0,   0  },
	{ 0x96,             0,             0,   0,   0  },
	{ 0x97,          0xaa,          0xaa,   0,   0  },
	{ 0x98,             0,             0,   0,   0  },
	{ 0x99,          0x23,          0x23,   0,   0  },
	{ 0x9A,           0x7,           0x7,   0,   0  },
	{ 0x9B,           0xf,           0xf,   0,   0  },
	{ 0x9C,          0x10,          0x10,   0,   0  },
	{ 0x9D,           0x3,           0x3,   0,   0  },
	{ 0x9E,          0x20,          0x20,   0,   0  },
	{ 0x9F,          0x20,          0x20,   0,   0  },
	{ 0xA0,             0,             0,   0,   0  },
	{ 0xA1,             0,             0,   0,   0  },
	{ 0xA2,             0,             0,   0,   0  },
	{ 0xA3,             0,             0,   0,   0  },
	{ 0xA4,           0x1,           0x1,   0,   0  },
	{ 0xA5,          0x77,          0x77,   0,   0  },
	{ 0xA6,          0x77,          0x77,   0,   0  },
	{ 0xA7,          0x77,          0x77,   0,   0  },
	{ 0xA8,          0x77,          0x77,   0,   0  },
	{ 0xA9,          0x8c,          0x8c,   0,   0  },
	{ 0xAA,          0x88,          0x88,   0,   0  },
	{ 0xAB,          0x78,          0x78,   0,   0  },
	{ 0xAC,          0x57,          0x57,   0,   0  },
	{ 0xAD,          0x88,          0x88,   0,   0  },
	{ 0xAE,             0,             0,   0,   0  },
	{ 0xAF,           0x8,           0x8,   0,   0  },
	{ 0xB0,          0x88,          0x88,   0,   0  },
	{ 0xB1,             0,             0,   0,   0  },
	{ 0xB2,          0x1b,          0x1b,   0,   0  },
	{ 0xB3,           0x3,           0x3,   0,   0  },
	{ 0xB4,          0x24,          0x24,   0,   0  },
	{ 0xB5,           0x3,           0x3,   0,   0  },
	{ 0xB6,          0x1b,          0x1b,   0,   0  },
	{ 0xB7,          0x24,          0x24,   0,   0  },
	{ 0xB8,           0x3,           0x3,   0,   0  },
	{ 0xB9,             0,             0,   0,   0  },
	{ 0xBA,          0xaa,          0xaa,   0,   0  },
	{ 0xBB,             0,             0,   0,   0  },
	{ 0xBC,           0x4,           0x4,   0,   0  },
	{ 0xBD,             0,             0,   0,   0  },
	{ 0xBE,           0x8,           0x8,   0,   0  },
	{ 0xBF,          0x11,          0x11,   0,   0  },
	{ 0xC0,             0,             0,   0,   0  },
	{ 0xC1,             0,             0,   0,   0  },
	{ 0xC2,          0x62,          0x62,   0,   0  },
	{ 0xC3,          0x1e,          0x1e,   0,   0  },
	{ 0xC4,          0x33,          0x33,   0,   0  },
	{ 0xC5,          0x37,          0x37,   0,   0  },
	{ 0xC6,             0,             0,   0,   0  },
	{ 0xC7,          0x70,          0x70,   0,   0  },
	{ 0xC8,          0x1e,          0x1e,   0,   0  },
	{ 0xC9,           0x6,           0x6,   0,   0  },
	{ 0xCA,           0x4,           0x4,   0,   0  },
	{ 0xCB,          0x2f,          0x2f,   0,   0  },
	{ 0xCC,           0xf,           0xf,   0,   0  },
	{ 0xCD,             0,             0,   0,   0  },
	{ 0xCE,          0xff,          0xff,   0,   0  },
	{ 0xCF,           0x8,           0x8,   0,   0  },
	{ 0xD0,          0x3f,          0x3f,   0,   0  },
	{ 0xD1,          0x3f,          0x3f,   0,   0  },
	{ 0xD2,          0x3f,          0x3f,   0,   0  },
	{ 0xD3,             0,             0,   0,   0  },
	{ 0xD4,             0,             0,   0,   0  },
	{ 0xD5,             0,             0,   0,   0  },
	{ 0xD6,          0xcc,          0xcc,   0,   0  },
	{ 0xD7,             0,             0,   0,   0  },
	{ 0xD8,           0x8,           0x8,   0,   0  },
	{ 0xD9,           0x8,           0x8,   0,   0  },
	{ 0xDA,           0x8,           0x8,   0,   0  },
	{ 0xDB,          0x11,          0x11,   0,   0  },
	{ 0xDC,             0,             0,   0,   0  },
	{ 0xDD,          0x87,          0x87,   0,   0  },
	{ 0xDE,          0x88,          0x88,   0,   0  },
	{ 0xDF,           0x8,           0x8,   0,   0  },
	{ 0xE0,           0x8,           0x8,   0,   0  },
	{ 0xE1,           0x8,           0x8,   0,   0  },
	{ 0xE2,             0,             0,   0,   0  },
	{ 0xE3,             0,             0,   0,   0  },
	{ 0xE4,             0,             0,   0,   0  },
	{ 0xE5,          0xf5,          0xf5,   0,   0  },
	{ 0xE6,          0x30,          0x30,   0,   0  },
	{ 0xE7,             0,             0,   0,   0  },
	{ 0xE8,             0,             0,   0,   0  },
	{ 0xE9,          0xff,          0xff,   0,   0  },
	{ 0xEA,             0,             0,   0,   0  },
	{ 0xEB,             0,             0,   0,   0  },
	{ 0xEC,          0x22,          0x22,   0,   0  },
	{ 0xED,             0,             0,   0,   0  },
	{ 0xEE,             0,             0,   0,   0  },
	{ 0xEF,             0,             0,   0,   0  },
	{ 0xF0,           0x3,           0x3,   0,   0  },
	{ 0xF1,           0x1,           0x1,   0,   0  },
	{ 0xF2,             0,             0,   0,   0  },
	{ 0xF3,             0,             0,   0,   0  },
	{ 0xF4,             0,             0,   0,   0  },
	{ 0xF5,             0,             0,   0,   0  },
	{ 0xF6,             0,             0,   0,   0  },
	{ 0xF7,           0x6,           0x6,   0,   0  },
	{ 0xF8,             0,             0,   0,   0  },
	{ 0xF9,             0,             0,   0,   0  },
	{ 0xFA,          0x40,          0x40,   0,   0  },
	{ 0xFB,             0,             0,   0,   0  },
	{ 0xFC,           0x1,           0x1,   0,   0  },
	{ 0xFD,          0x80,          0x80,   0,   0  },
	{ 0xFE,           0x2,           0x2,   0,   0  },
	{ 0xFF,          0x10,          0x10,   0,   0  },
	{ 0x100,           0x2,           0x2,   0,   0  },
	{ 0x101,          0x1e,          0x1e,   0,   0  },
	{ 0x102,          0x1e,          0x1e,   0,   0  },
	{ 0x103,             0,             0,   0,   0  },
	{ 0x104,          0x1f,          0x1f,   0,   0  },
	{ 0x105,             0,           0x8,   0,   1  },
	{ 0x106,          0x2a,          0x2a,   0,   0  },
	{ 0x107,           0xf,           0xf,   0,   0  },
	{ 0x108,             0,             0,   0,   0  },
	{ 0x109,             0,             0,   0,   0  },
	{ 0x10A,             0,             0,   0,   0  },
	{ 0x10B,             0,             0,   0,   0  },
	{ 0x10C,             0,             0,   0,   0  },
	{ 0x10D,             0,             0,   0,   0  },
	{ 0x10E,             0,             0,   0,   0  },
	{ 0x10F,             0,             0,   0,   0  },
	{ 0x110,             0,             0,   0,   0  },
	{ 0x111,             0,             0,   0,   0  },
	{ 0x112,             0,             0,   0,   0  },
	{ 0x113,             0,             0,   0,   0  },
	{ 0x114,             0,             0,   0,   0  },
	{ 0x115,             0,             0,   0,   0  },
	{ 0x116,             0,             0,   0,   0  },
	{ 0x117,             0,             0,   0,   0  },
	{ 0x118,             0,             0,   0,   0  },
	{ 0x119,             0,             0,   0,   0  },
	{ 0x11A,             0,             0,   0,   0  },
	{ 0x11B,             0,             0,   0,   0  },
	{ 0x11C,           0x1,           0x1,   0,   0  },
	{ 0x11D,             0,             0,   0,   0  },
	{ 0x11E,             0,             0,   0,   0  },
	{ 0x11F,             0,             0,   0,   0  },
	{ 0x120,             0,             0,   0,   0  },
	{ 0x121,             0,             0,   0,   0  },
	{ 0x122,          0x80,          0x80,   0,   0  },
	{ 0x123,             0,             0,   0,   0  },
	{ 0x124,          0xf8,          0xf8,   0,   0  },
	{ 0x125,             0,             0,   0,   0  },
	{ 0x126,             0,             0,   0,   0  },
	{ 0x127,             0,             0,   0,   0  },
	{ 0x128,             0,             0,   0,   0  },
	{ 0x129,             0,             0,   0,   0  },
	{ 0x12A,             0,             0,   0,   0  },
	{ 0x12B,             0,             0,   0,   0  },
	{ 0x12C,             0,             0,   0,   0  },
	{ 0x12D,             0,             0,   0,   0  },
	{ 0x12E,             0,             0,   0,   0  },
	{ 0x12F,             0,             0,   0,   0  },
	{ 0x130,             0,             0,   0,   0  },
	{ 0x131,             0,             0,   0,   0  },
	{ 0x132,             0,             0,   0,   0  },
	{ 0x133,             0,             0,   0,   0  },
	{ 0x134,             0,             0,   0,   0  },
	{ 0x135,             0,             0,   0,   0  },
	{ 0x136,             0,             0,   0,   0  },
	{ 0x137,             0,             0,   0,   0  },
	{ 0x138,             0,             0,   0,   0  },
	{ 0x139,             0,             0,   0,   0  },
	{ 0x13A,             0,             0,   0,   0  },
	{ 0x13B,             0,             0,   0,   0  },
	{ 0x13C,             0,             0,   0,   0  },
	{ 0x13D,             0,             0,   0,   0  },
	{ 0x13E,             0,             0,   0,   0  },
	{ 0x13F,             0,             0,   0,   0  },
	{ 0x140,             0,             0,   0,   0  },
	{ 0x141,             0,             0,   0,   0  },
	{ 0x142,             0,             0,   0,   0  },
	{ 0x143,             0,             0,   0,   0  },
	{ 0x144,             0,             0,   0,   0  },
	{ 0x145,             0,             0,   0,   0  },
	{ 0x146,             0,             0,   0,   0  },
	{ 0x147,             0,             0,   0,   0  },
	{ 0x148,             0,             0,   0,   0  },
	{ 0x149,             0,             0,   0,   0  },
	{ 0x14A,             0,             0,   0,   0  },
	{ 0x14B,             0,             0,   0,   0  },
	{ 0x14C,             0,             0,   0,   0  },
	{ 0x14D,             0,             0,   0,   0  },
	{ 0x14E,             0,             0,   0,   0  },
	{ 0x14F,             0,             0,   0,   0  },
	{ 0x150,             0,             0,   0,   0  },
	{ 0x151,             0,             0,   0,   0  },
	{ 0x152,             0,             0,   0,   0  },
	{ 0x153,             0,             0,   0,   0  },
	{ 0x154,             0,             0,   0,   0  },
	{ 0x155,             0,             0,   0,   0  },
	{ 0x156,             0,             0,   0,   0  },
	{ 0x157,             0,             0,   0,   0  },
	{ 0x158,             0,             0,   0,   0  },
	{ 0x159,             0,             0,   0,   0  },
	{ 0x15A,             0,             0,   0,   0  },
	{ 0x15B,             0,             0,   0,   0  },
	{ 0x15C,             0,             0,   0,   0  },
	{ 0x15D,             0,             0,   0,   0  },
	{ 0x15E,             0,             0,   0,   0  },
	{ 0x15F,             0,             0,   0,   0  },
	{ 0x160,             0,             0,   0,   0  },
	{ 0x161,             0,             0,   0,   0  },
	{ 0x162,             0,             0,   0,   0  },
	{ 0x163,             0,             0,   0,   0  },
	{ 0x164,             0,             0,   0,   0  },
	{ 0x165,             0,             0,   0,   0  },
	{ 0xFFFF,             0,             0,   0,   0  },
	};

/* Init values for 2066 regs (autogenerated by 2066_regs_tcl2c.tcl)
 *   Entries: addr, init value A, init value G, do_init A, do_init G.
 *   Last line (addr FF is dummy as delimiter. This table has dual use
 *   between dumping and initializing.
 */
lcnphy_radio_regs_t lcnphy_radio_regs_2064[] = {
	{ 0x00,             0,             0,   0,   0  },
	{ 0x01,          0x64,          0x64,   0,   0  },
	{ 0x02,          0x20,          0x20,   0,   0  },
	{ 0x03,          0x66,          0x66,   0,   0  },
	{ 0x04,          0xf8,          0xf8,   0,   0  },
	{ 0x05,             0,             0,   0,   0  },
	{ 0x06,          0x10,          0x10,   0,   0  },
	{ 0x07,             0,             0,   0,   0  },
	{ 0x08,             0,             0,   0,   0  },
	{ 0x09,             0,             0,   0,   0  },
	{ 0x0A,          0x37,          0x37,   0,   0  },
	{ 0x0B,           0x6,           0x6,   0,   0  },
	{ 0x0C,          0x55,          0x55,   0,   0  },
	{ 0x0D,          0x8b,          0x8b,   0,   0  },
	{ 0x0E,             0,             0,   0,   0  },
	{ 0x0F,           0x5,           0x5,   0,   0  },
	{ 0x10,             0,             0,   0,   0  },
	{ 0x11,           0xe,           0xe,   0,   0  },
	{ 0x12,             0,             0,   0,   0  },
	{ 0x13,           0xb,           0xb,   0,   0  },
	{ 0x14,           0x2,           0x2,   0,   0  },
	{ 0x15,          0x12,          0x12,   0,   0  },
	{ 0x16,          0x12,          0x12,   0,   0  },
	{ 0x17,           0xc,           0xc,   0,   0  },
	{ 0x18,           0xc,           0xc,   0,   0  },
	{ 0x19,           0xc,           0xc,   0,   0  },
	{ 0x1A,           0x8,           0x8,   0,   0  },
	{ 0x1B,           0x2,           0x2,   0,   0  },
	{ 0x1C,             0,             0,   0,   0  },
	{ 0x1D,           0x1,           0x1,   0,   0  },
	{ 0x1E,          0x12,          0x12,   0,   0  },
	{ 0x1F,          0x6e,          0x6e,   0,   0  },
	{ 0x20,           0x2,           0x2,   0,   0  },
	{ 0x21,          0x23,          0x23,   0,   0  },
	{ 0x22,           0x8,           0x8,   0,   0  },
	{ 0x23,             0,             0,   0,   0  },
	{ 0x24,             0,             0,   0,   0  },
	{ 0x25,           0xc,           0xc,   0,   0  },
	{ 0x26,          0x33,          0x33,   0,   0  },
	{ 0x27,          0x55,          0x55,   0,   0  },
	{ 0x28,             0,             0,   0,   0  },
	{ 0x29,          0x30,          0x30,   0,   0  },
	{ 0x2A,           0xb,           0xb,   0,   0  },
	{ 0x2B,          0x1b,          0x1b,   0,   0  },
	{ 0x2C,           0x3,           0x3,   0,   0  },
	{ 0x2D,          0x1b,          0x1b,   0,   0  },
	{ 0x2E,             0,             0,   0,   0  },
	{ 0x2F,          0x20,          0x20,   0,   0  },
	{ 0x30,           0xa,           0xa,   0,   0  },
	{ 0x31,             0,             0,   0,   0  },
	{ 0x32,          0x62,          0x62,   0,   0  },
	{ 0x33,          0x19,          0x19,   0,   0  },
	{ 0x34,          0x33,          0x33,   0,   0  },
	{ 0x35,          0x77,          0x77,   0,   0  },
	{ 0x36,             0,             0,   0,   0  },
	{ 0x37,          0x70,          0x70,   0,   0  },
	{ 0x38,           0x3,           0x3,   0,   0  },
	{ 0x39,           0xf,           0xf,   0,   0  },
	{ 0x3A,           0x6,           0x6,   0,   0  },
	{ 0x3B,          0xcf,          0xcf,   0,   0  },
	{ 0x3C,          0x1a,          0x1a,   0,   0  },
	{ 0x3D,           0x6,           0x6,   0,   0  },
	{ 0x3E,          0x42,          0x42,   0,   0  },
	{ 0x3F,             0,             0,   0,   0  },
	{ 0x40,          0xfb,          0xfb,   0,   0  },
	{ 0x41,          0x9a,          0x9a,   0,   0  },
	{ 0x42,          0x7a,          0x7a,   0,   0  },
	{ 0x43,          0x29,          0x29,   0,   0  },
	{ 0x44,             0,             0,   0,   0  },
	{ 0x45,           0x8,           0x8,   0,   0  },
	{ 0x46,          0xce,          0xce,   0,   0  },
	{ 0x47,          0x27,          0x27,   0,   0  },
	{ 0x48,          0x62,          0x62,   0,   0  },
	{ 0x49,           0x6,           0x6,   0,   0  },
	{ 0x4A,          0x58,          0x58,   0,   0  },
	{ 0x4B,          0xf7,          0xf7,   0,   0  },
	{ 0x4C,             0,             0,   0,   0  },
	{ 0x4D,          0xb3,          0xb3,   0,   0  },
	{ 0x4E,             0,             0,   0,   0  },
	{ 0x4F,           0x2,           0x2,   0,   0  },
	{ 0x50,             0,             0,   0,   0  },
	{ 0x51,           0x9,           0x9,   0,   0  },
	{ 0x52,           0x5,           0x5,   0,   0  },
	{ 0x53,          0x17,          0x17,   0,   0  },
	{ 0x54,          0x38,          0x38,   0,   0  },
	{ 0x55,             0,             0,   0,   0  },
	{ 0x56,             0,             0,   0,   0  },
	{ 0x57,           0xb,           0xb,   0,   0  },
	{ 0x58,             0,             0,   0,   0  },
	{ 0x59,             0,             0,   0,   0  },
	{ 0x5A,             0,             0,   0,   0  },
	{ 0x5B,             0,             0,   0,   0  },
	{ 0x5C,             0,             0,   0,   0  },
	{ 0x5D,             0,             0,   0,   0  },
	{ 0x5E,          0x88,          0x88,   0,   0  },
	{ 0x5F,          0xcc,          0xcc,   0,   0  },
	{ 0x60,          0x74,          0x74,   0,   0  },
	{ 0x61,          0x74,          0x74,   0,   0  },
	{ 0x62,          0x74,          0x74,   0,   0  },
	{ 0x63,          0x44,          0x44,   0,   0  },
	{ 0x64,          0x77,          0x77,   0,   0  },
	{ 0x65,          0x44,          0x44,   0,   0  },
	{ 0x66,          0x77,          0x77,   0,   0  },
	{ 0x67,          0x55,          0x55,   0,   0  },
	{ 0x68,          0x77,          0x77,   0,   0  },
	{ 0x69,          0x77,          0x77,   0,   0  },
	{ 0x6A,             0,             0,   0,   0  },
	{ 0x6B,          0x7f,          0x7f,   0,   0  },
	{ 0x6C,           0x8,           0x8,   0,   0  },
	{ 0x6D,             0,             0,   0,   0  },
	{ 0x6E,          0x88,          0x88,   0,   0  },
	{ 0x6F,          0x66,          0x66,   0,   0  },
	{ 0x70,          0x66,          0x66,   0,   0  },
	{ 0x71,          0x28,          0x28,   0,   0  },
	{ 0x72,          0x55,          0x55,   0,   0  },
	{ 0x73,           0x4,           0x4,   0,   0  },
	{ 0x74,             0,             0,   0,   0  },
	{ 0x75,             0,             0,   0,   0  },
	{ 0x76,             0,             0,   0,   0  },
	{ 0x77,           0x1,           0x1,   0,   0  },
	{ 0x78,          0xd6,          0xd6,   0,   0  },
	{ 0x79,             0,             0,   0,   0  },
	{ 0x7A,             0,             0,   0,   0  },
	{ 0x7B,             0,             0,   0,   0  },
	{ 0x7C,             0,             0,   0,   0  },
	{ 0x7D,             0,             0,   0,   0  },
	{ 0x7E,             0,             0,   0,   0  },
	{ 0x7F,             0,             0,   0,   0  },
	{ 0x80,             0,             0,   0,   0  },
	{ 0x81,             0,             0,   0,   0  },
	{ 0x82,             0,             0,   0,   0  },
	{ 0x83,          0xb4,          0xb4,   0,   0  },
	{ 0x84,           0x1,           0x1,   0,   0  },
	{ 0x85,          0x20,          0x20,   0,   0  },
	{ 0x86,           0x5,           0x5,   0,   0  },
	{ 0x87,          0xff,          0xff,   0,   0  },
	{ 0x88,           0x7,           0x7,   0,   0  },
	{ 0x89,          0x77,          0x77,   0,   0  },
	{ 0x8A,          0x77,          0x77,   0,   0  },
	{ 0x8B,          0x77,          0x77,   0,   0  },
	{ 0x8C,          0x77,          0x77,   0,   0  },
	{ 0x8D,           0x8,           0x8,   0,   0  },
	{ 0x8E,           0xa,           0xa,   0,   0  },
	{ 0x8F,           0x8,           0x8,   0,   0  },
	{ 0x90,          0x18,          0x18,   0,   0  },
	{ 0x91,           0x5,           0x5,   0,   0  },
	{ 0x92,          0x1f,          0x1f,   0,   0  },
	{ 0x93,          0x10,          0x10,   0,   0  },
	{ 0x94,           0x3,           0x3,   0,   0  },
	{ 0x95,             0,             0,   0,   0  },
	{ 0x96,             0,             0,   0,   0  },
	{ 0x97,          0xaa,          0xaa,   0,   0  },
	{ 0x98,             0,             0,   0,   0  },
	{ 0x99,          0x23,          0x23,   0,   0  },
	{ 0x9A,           0x7,           0x7,   0,   0  },
	{ 0x9B,           0xf,           0xf,   0,   0  },
	{ 0x9C,          0x10,          0x10,   0,   0  },
	{ 0x9D,           0x3,           0x3,   0,   0  },
	{ 0x9E,           0x4,           0x4,   0,   0  },
	{ 0x9F,          0x20,          0x20,   0,   0  },
	{ 0xA0,             0,             0,   0,   0  },
	{ 0xA1,             0,             0,   0,   0  },
	{ 0xA2,             0,             0,   0,   0  },
	{ 0xA3,             0,             0,   0,   0  },
	{ 0xA4,           0x1,           0x1,   0,   0  },
	{ 0xA5,          0x77,          0x77,   0,   0  },
	{ 0xA6,          0x77,          0x77,   0,   0  },
	{ 0xA7,          0x77,          0x77,   0,   0  },
	{ 0xA8,          0x77,          0x77,   0,   0  },
	{ 0xA9,          0x8c,          0x8c,   0,   0  },
	{ 0xAA,          0x88,          0x88,   0,   0  },
	{ 0xAB,          0x78,          0x78,   0,   0  },
	{ 0xAC,          0x57,          0x57,   0,   0  },
	{ 0xAD,          0x88,          0x88,   0,   0  },
	{ 0xAE,             0,             0,   0,   0  },
	{ 0xAF,           0x8,           0x8,   0,   0  },
	{ 0xB0,          0x88,          0x88,   0,   0  },
	{ 0xB1,             0,             0,   0,   0  },
	{ 0xB2,          0x1b,          0x1b,   0,   0  },
	{ 0xB3,           0x3,           0x3,   0,   0  },
	{ 0xB4,          0x24,          0x24,   0,   0  },
	{ 0xB5,           0x3,           0x3,   0,   0  },
	{ 0xB6,          0x1b,          0x1b,   0,   0  },
	{ 0xB7,          0x24,          0x24,   0,   0  },
	{ 0xB8,           0x3,           0x3,   0,   0  },
	{ 0xB9,             0,             0,   0,   0  },
	{ 0xBA,          0xaa,          0xaa,   0,   0  },
	{ 0xBB,             0,             0,   0,   0  },
	{ 0xBC,           0x4,           0x4,   0,   0  },
	{ 0xBD,             0,             0,   0,   0  },
	{ 0xBE,           0x8,           0x8,   0,   0  },
	{ 0xBF,          0x11,          0x11,   0,   0  },
	{ 0xC0,             0,             0,   0,   0  },
	{ 0xC1,             0,             0,   0,   0  },
	{ 0xC2,          0x62,          0x62,   0,   0  },
	{ 0xC3,          0x1e,          0x1e,   0,   0  },
	{ 0xC4,          0x33,          0x33,   0,   0  },
	{ 0xC5,          0x37,          0x37,   0,   0  },
	{ 0xC6,             0,             0,   0,   0  },
	{ 0xC7,          0x70,          0x70,   0,   0  },
	{ 0xC8,          0x1e,          0x1e,   0,   0  },
	{ 0xC9,           0x6,           0x6,   0,   0  },
	{ 0xCA,           0x4,           0x4,   0,   0  },
	{ 0xCB,          0x2f,          0x2f,   0,   0  },
	{ 0xCC,           0xf,           0xf,   0,   0  },
	{ 0xCD,             0,             0,   0,   0  },
	{ 0xCE,          0xff,          0xff,   0,   0  },
	{ 0xCF,           0x8,           0x8,   0,   0  },
	{ 0xD0,          0x3f,          0x3f,   0,   0  },
	{ 0xD1,          0x3f,          0x3f,   0,   0  },
	{ 0xD2,          0x3f,          0x3f,   0,   0  },
	{ 0xD3,             0,             0,   0,   0  },
	{ 0xD4,             0,             0,   0,   0  },
	{ 0xD5,             0,             0,   0,   0  },
	{ 0xD6,          0xcc,          0xcc,   0,   0  },
	{ 0xD7,             0,             0,   0,   0  },
	{ 0xD8,           0x8,           0x8,   0,   0  },
	{ 0xD9,           0x8,           0x8,   0,   0  },
	{ 0xDA,           0x8,           0x8,   0,   0  },
	{ 0xDB,          0x11,          0x11,   0,   0  },
	{ 0xDC,             0,             0,   0,   0  },
	{ 0xDD,          0x87,          0x87,   0,   0  },
	{ 0xDE,          0x88,          0x88,   0,   0  },
	{ 0xDF,           0x8,           0x8,   0,   0  },
	{ 0xE0,           0x8,           0x8,   0,   0  },
	{ 0xE1,           0x8,           0x8,   0,   0  },
	{ 0xE2,             0,             0,   0,   0  },
	{ 0xE3,             0,             0,   0,   0  },
	{ 0xE4,             0,             0,   0,   0  },
	{ 0xE5,          0xf5,          0xf5,   0,   0  },
	{ 0xE6,          0x30,          0x30,   0,   0  },
	{ 0xE7,           0x1,           0x1,   0,   0  },
	{ 0xE8,             0,             0,   0,   0  },
	{ 0xE9,          0xff,          0xff,   0,   0  },
	{ 0xEA,             0,             0,   0,   0  },
	{ 0xEB,             0,             0,   0,   0  },
	{ 0xEC,          0x22,          0x22,   0,   0  },
	{ 0xED,             0,             0,   0,   0  },
	{ 0xEE,             0,             0,   0,   0  },
	{ 0xEF,             0,             0,   0,   0  },
	{ 0xF0,           0x3,           0x3,   0,   0  },
	{ 0xF1,           0x1,           0x1,   0,   0  },
	{ 0xF2,             0,             0,   0,   0  },
	{ 0xF3,             0,             0,   0,   0  },
	{ 0xF4,             0,             0,   0,   0  },
	{ 0xF5,             0,             0,   0,   0  },
	{ 0xF6,             0,             0,   0,   0  },
	{ 0xF7,           0x6,           0x6,   0,   0  },
	{ 0xF8,             0,             0,   0,   0  },
	{ 0xF9,             0,             0,   0,   0  },
	{ 0xFA,          0x40,          0x40,   0,   0  },
	{ 0xFB,             0,             0,   0,   0  },
	{ 0xFC,           0x1,           0x1,   0,   0  },
	{ 0xFD,          0x80,          0x80,   0,   0  },
	{ 0xFE,           0x2,           0x2,   0,   0  },
	{ 0xFF,          0x10,          0x10,   0,   0  },
	{ 0x100,           0x2,           0x2,   0,   0  },
	{ 0x101,          0x1e,          0x1e,   0,   0  },
	{ 0x102,          0x1e,          0x1e,   0,   0  },
	{ 0x103,             0,             0,   0,   0  },
	{ 0x104,          0x1f,          0x1f,   0,   0  },
	{ 0x105,             0,           0x8,   0,   1  },
	{ 0x106,          0x2a,          0x2a,   0,   0  },
	{ 0x107,           0xf,           0xf,   0,   0  },
	{ 0x108,             0,             0,   0,   0  },
	{ 0x109,             0,             0,   0,   0  },
	{ 0x10A,             0,             0,   0,   0  },
	{ 0x10B,             0,             0,   0,   0  },
	{ 0x10C,             0,             0,   0,   0  },
	{ 0x10D,             0,             0,   0,   0  },
	{ 0x10E,             0,             0,   0,   0  },
	{ 0x10F,             0,             0,   0,   0  },
	{ 0x110,             0,             0,   0,   0  },
	{ 0x111,             0,             0,   0,   0  },
	{ 0x112,             0,             0,   0,   0  },
	{ 0x113,             0,             0,   0,   0  },
	{ 0x114,             0,             0,   0,   0  },
	{ 0x115,             0,             0,   0,   0  },
	{ 0x116,             0,             0,   0,   0  },
	{ 0x117,             0,             0,   0,   0  },
	{ 0x118,             0,             0,   0,   0  },
	{ 0x119,             0,             0,   0,   0  },
	{ 0x11A,             0,             0,   0,   0  },
	{ 0x11B,             0,             0,   0,   0  },
	{ 0x11C,           0x1,           0x1,   0,   0  },
	{ 0x11D,             0,             0,   0,   0  },
	{ 0x11E,             0,             0,   0,   0  },
	{ 0x11F,             0,             0,   0,   0  },
	{ 0x120,             0,             0,   0,   0  },
	{ 0x121,             0,             0,   0,   0  },
	{ 0x122,          0x80,          0x80,   0,   0  },
	{ 0x123,             0,             0,   0,   0  },
	{ 0x124,          0xf8,          0xf8,   0,   0  },
	{ 0x125,             0,             0,   0,   0  },
	{ 0x126,             0,             0,   0,   0  },
	{ 0x127,             0,             0,   0,   0  },
	{ 0x128,             0,             0,   0,   0  },
	{ 0x129,             0,             0,   0,   0  },
	{ 0x12A,             0,             0,   0,   0  },
	{ 0x12B,             0,             0,   0,   0  },
	{ 0x12C,             0,             0,   0,   0  },
	{ 0x12D,             0,             0,   0,   0  },
	{ 0x12E,             0,             0,   0,   0  },
	{ 0x12F,             0,             0,   0,   0  },
	{ 0x130,             0,             0,   0,   0  },
	{ 0xFFFF,             0,             0,   0,   0  }
};


#define LCNPHY_NUM_DIG_FILT_COEFFS 16
#define LCNPHY_NUM_TX_DIG_FILTERS_CCK 13
/* filter id, followed by coefficients */
uint16 LCNPHY_txdigfiltcoeffs_cck[LCNPHY_NUM_TX_DIG_FILTERS_CCK][LCNPHY_NUM_DIG_FILT_COEFFS+1] = {
	{ 0, 1, 415, 1874, 64, 128, 64, 792, 1656, 64, 128, 64, 778, 1582, 64, 128, 64, },
	{ 1, 1, 402, 1847, 259, 59, 259, 671, 1794, 68, 54, 68, 608, 1863, 93, 167, 93, },
	{ 2, 1, 415, 1874, 64, 128, 64, 792, 1656, 192, 384, 192, 778, 1582, 64, 128, 64, },
	{ 3, 1, 302, 1841, 129, 258, 129, 658, 1720, 205, 410, 205, 754, 1760, 170, 340, 170, },
	{ 20, 1, 360, 1884, 242, 1734, 242, 752, 1720, 205, 1845, 205, 767, 1760, 256, 185, 256, },
	{ 21, 1, 360, 1884, 149, 1874, 149, 752, 1720, 205, 1883, 205, 767, 1760, 256, 273, 256, },
	{ 22, 1, 360, 1884, 98, 1948, 98, 752, 1720, 205, 1924, 205, 767, 1760, 256, 352, 256, },
	{ 23, 1, 350, 1884, 116, 1966, 116, 752, 1720, 205, 2008, 205, 767, 1760, 128, 233, 128, },
	{ 24, 1, 325, 1884, 32, 40, 32, 756, 1720, 256, 471, 256, 766, 1760, 256, 1881, 256, },
	{ 25, 1, 299, 1884, 51, 64, 51, 736, 1720, 256, 471, 256, 765, 1760, 256, 1881, 256, },
	{ 26, 1, 277, 1943, 39, 117, 88, 637, 1838, 64, 192, 144, 614, 1864, 128, 384, 288, },
	{ 27, 1, 245, 1943, 49, 147, 110, 626, 1838, 256, 768, 576, 613, 1864, 128, 384, 288, },
	{ 30, 1, 302, 1841, 61, 122, 61, 658, 1720, 205, 410, 205, 754, 1760, 170, 340, 170, },
	};

#define LCNPHY_NUM_TX_DIG_FILTERS_OFDM 3
uint16 LCNPHY_txdigfiltcoeffs_ofdm[LCNPHY_NUM_TX_DIG_FILTERS_OFDM][LCNPHY_NUM_DIG_FILT_COEFFS+1] = {
	{ 0, 0, 0xa2, 0x0, 0x100, 0x100, 0x0, 0x0, 0x0, 0x100, 0x0, 0x0,
	0x278, 0xfea0, 0x80, 0x100, 0x80, },
	{ 1, 0, 374, 0xFF79, 16, 32, 16, 799, 0xFE74, 50, 32, 50,
	750, 0xFE2B, 212, 0xFFCE, 212, },
	{2, 0, 375, 0xFF16, 37, 76, 37, 799, 0xFE74, 32, 20, 32, 748,
	0xFEF2, 128, 0xFFE2, 128}
	};


#define wlc_lcnphy_set_start_tx_pwr_idx(pi, idx) \
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlCmd, \
		LCNPHY_TxPwrCtrlCmd_pwrIndex_init_MASK, \
		(uint16)(idx*2) << LCNPHY_TxPwrCtrlCmd_pwrIndex_init_SHIFT)

#define wlc_lcnphy_set_tx_pwr_npt(pi, npt) \
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlNnum, \
		LCNPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK, \
		(uint16)(npt) << LCNPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_lcnphy_get_tx_pwr_ctrl(pi) \
	(read_phy_reg((pi), LCNPHY_TxPwrCtrlCmd) & \
			(LCNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
			LCNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK | \
			LCNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK))

#define wlc_lcnphy_get_tx_pwr_npt(pi) \
	((read_phy_reg(pi, LCNPHY_TxPwrCtrlNnum) & \
		LCNPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK) >> \
		LCNPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

/* the bitsize of the register is 9 bits for lcnphy */
#define wlc_lcnphy_get_current_tx_pwr_idx_if_pwrctrl_on(pi) \
	(read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusExt) & 0x1ff)

#define wlc_lcnphy_get_target_tx_pwr(pi) \
	((read_phy_reg(pi, LCNPHY_TxPwrCtrlTargetPwr) & \
		LCNPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK) >> \
		LCNPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_lcnphy_set_target_tx_pwr(pi, target) \
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlTargetPwr, \
		LCNPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK, \
		(uint16)(target) << LCNPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_radio_2064_rc_cal_done(pi) (0 != (read_radio_reg(pi, RADIO_2064_REG10A) & 0x01))
#define wlc_radio_2064_rcal_done(pi) (0 != (read_radio_reg(pi, RADIO_2064_REG05C) & 0x20))
#define tempsense_done(pi) (0x8000 == (read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusTemp) & 0x8000))

#define LCNPHY_IQLOCC_READ(val) ((uint8)(-(int8)(((val) & 0xf0) >> 4) + (int8)((val) & 0x0f)))
#define FIXED_TXPWR 78
#define LCNPHY_TEMPSENSE(val) ((int16)((val > 255)?(val - 512):val))

/* %%%%%% LCNPHY function declaration */
static uint32 wlc_lcnphy_qdiv_roundup(uint32 divident, uint32 divisor, uint8 precision);
static void wlc_lcnphy_set_rx_gain_by_distribution(phy_info_t *pi, uint16 ext_lna,
	uint16 trsw, uint16 biq2, uint16 biq1, uint16 tia, uint16 lna2, uint16 lna1);
static void wlc_lcnphy_clear_tx_power_offsets(phy_info_t *pi);
static void wlc_lcnphy_set_pa_gain(phy_info_t *pi, uint16 gain);
static void wlc_lcnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx);
static void wlc_lcnphy_set_bbmult(phy_info_t *pi, uint8 m0);
static uint8 wlc_lcnphy_get_bbmult(phy_info_t *pi);
static void wlc_lcnphy_get_tx_gain(phy_info_t *pi,  lcnphy_txgains_t *gains);
static void wlc_lcnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable);
static void wlc_lcnphy_toggle_afe_pwdn(phy_info_t *pi);
static void wlc_lcnphy_rx_gain_override_enable(phy_info_t *pi, bool enable);
static void wlc_lcnphy_set_tx_gain(phy_info_t *pi,  lcnphy_txgains_t *target_gains);
static void wlc_lcnphy_GetpapdMaxMinIdxupdt(phy_info_t *pi,
	int8 *maxUpdtIdx, int8 *minUpdtIdx);
static bool wlc_lcnphy_rx_iq_est(phy_info_t *pi, uint16 num_samps,
	uint8 wait_time, lcnphy_iq_est_t *iq_est);
static void wlc_lcnphy_rx_pu(phy_info_t *pi, bool bEnable);
static bool wlc_lcnphy_calc_rx_iq_comp(phy_info_t *pi,  uint16 num_samps);
static uint16 wlc_lcnphy_get_pa_gain(phy_info_t *pi);
static void wlc_lcnphy_afe_clk_init(phy_info_t *pi, uint8 mode);
extern void wlc_lcnphy_tx_pwr_ctrl_init(wlc_phy_t *ppi);
static void wlc_lcnphy_radio_2064_channel_tune(phy_info_t *pi, uint8 channel);
static void wlc_lcnphy_radio_2064_channel_tune_4313(phy_info_t *pi, uint8 channel);

static void wlc_lcnphy_load_tx_gain_table(phy_info_t *pi, const lcnphy_tx_gain_tbl_entry *g);
static void wlc_lcnphy_force_pwr_index(phy_info_t *pi, int index);

/* added for iqlo soft cal */
static void wlc_lcnphy_samp_cap(phy_info_t *pi, int clip_detect_algo, uint16 thresh,
	int16* ptr, int mode);
static int wlc_lcnphy_calc_floor(int16 coeff, int type);
static void wlc_lcnphy_tx_iqlo_loopback(phy_info_t *pi, uint16 *values_to_save);
static void wlc_lcnphy_tx_iqlo_loopback_cleanup(phy_info_t *pi, uint16 *values_to_save);
static void wlc_lcnphy_set_cc(phy_info_t *pi, int cal_type, int16 coeff_x, int16 coeff_y);
static lcnphy_unsign16_struct wlc_lcnphy_get_cc(phy_info_t *pi, int cal_type);
static void wlc_lcnphy_tx_iqlo_soft_cal(phy_info_t *pi, int cal_type,
	int num_levels, int step_size_lg2);
static void wlc_lcnphy_tx_iqlo_soft_cal_full(phy_info_t *pi);

static void wlc_lcnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec);
static void wlc_lcnphy_agc_temp_init(phy_info_t *pi);
static void wlc_lcnphy_temp_adj(phy_info_t *pi);
static void wlc_lcnphy_clear_papd_comptable(phy_info_t *pi);
static void wlc_lcnphy_baseband_init(phy_info_t *pi);
static void wlc_lcnphy_radio_init(phy_info_t *pi);
static void wlc_lcnphy_rc_cal(phy_info_t *pi);
static void wlc_lcnphy_rcal(phy_info_t *pi);
#if defined(WLTEST)
static void wlc_lcnphy_reset_radio_loft(phy_info_t *pi);
static void wlc_lcnphy_set_rx_gain(phy_info_t *pi, uint32 gain);
#endif
static void wlc_lcnphy_txrx_spur_avoidance_mode(phy_info_t *pi, bool enable);
static int wlc_lcnphy_load_tx_iir_filter(phy_info_t *pi, bool is_ofdm, int16 filt_type);
static void wlc_lcnphy_restore_calibration_results(phy_info_t *pi);
static void wlc_lcnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a, uint16 b);
static void wlc_lcnphy_set_genv(phy_info_t *pi, uint16 genv);
static uint16 wlc_lcnphy_get_genv(phy_info_t *pi);

static void wlc_lcnphy_noise_init(phy_info_t *pi);


/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  function implementation   					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

void
wlc_lcnphy_write_table(phy_info_t *pi, const phytbl_info_t *pti)
{
	wlc_phy_write_table(pi, pti, LCNPHY_TableAddress,
		LCNPHY_TabledataHi, LCNPHY_TabledataLo);
}

void
wlc_lcnphy_read_table(phy_info_t *pi, phytbl_info_t *pti)
{
	wlc_phy_read_table(pi, pti, LCNPHY_TableAddress,
	   LCNPHY_TabledataHi, LCNPHY_TabledataLo);
}

static void
wlc_lcnphy_common_read_table(phy_info_t *pi, uint32 tbl_id,
	CONST void *tbl_ptr, uint32 tbl_len, uint32 tbl_width,
	uint32 tbl_offset) {
	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_lcnphy_read_table(pi, &tab);
}

static void
wlc_lcnphy_common_write_table(phy_info_t *pi, uint32 tbl_id, CONST void *tbl_ptr,
	uint32 tbl_len, uint32 tbl_width, uint32 tbl_offset) {

	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_lcnphy_write_table(pi, &tab);
}

static uint32
wlc_lcnphy_qdiv_roundup(uint32 dividend, uint32 divisor, uint8 precision)
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

static int
wlc_lcnphy_calc_floor(int16 coeff_x, int type)
{
	int k;
	k = 0;
	if (type == 0) {
		if (coeff_x < 0) {
			k = (coeff_x - 1)/2;
		} else {
			k = coeff_x/2;
		}
	}
	if (type == 1) {
		if ((coeff_x + 1) < 0)
			k = (coeff_x)/2;
		else
			k = (coeff_x + 1)/ 2;
	}
	return k;
}

int8
wlc_lcnphy_get_current_tx_pwr_idx(phy_info_t *pi)
{
	int8 index;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	/* for txpwrctrl_off and tempsense_based pwrctrl, return current_index */
	if (txpwrctrl_off(pi))
		index = pi_lcn->lcnphy_current_index;
	else if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		index = (int8)(wlc_lcnphy_get_current_tx_pwr_idx_if_pwrctrl_on(pi)/2);
	else
		index = pi_lcn->lcnphy_current_index;
	return index;
}

static uint32
wlc_lcnphy_measure_digital_power(phy_info_t *pi, uint16 nsamples)
{
	lcnphy_iq_est_t iq_est = {0, 0, 0};

	if (!wlc_lcnphy_rx_iq_est(pi, nsamples, 32, &iq_est))
		return 0;
	return (iq_est.i_pwr + iq_est.q_pwr) / nsamples;
}

/* %%%%%% LCNPHY functions */

#ifdef NOT_USED
static int8 wlc_phy_get_tx_power_offset_by_mcs(wlc_phy_t *ppi, uint8 mcs_offset);
static int8 wlc_phy_get_tx_power_offset(wlc_phy_t *ppi, uint8 tbl_offset);

uint8 lcnphy_mcs_to_legacy_map[8] = {0, 2, 3, 4, 5, 6, 7, 7 + 8};

static int8
wlc_phy_get_tx_power_offset(wlc_phy_t *ppi, uint8 tbl_offset)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	return pi->tx_power_offset[lcnphy_mcs_to_legacy_map[tbl_offset] + 4];
}

static int8
wlc_phy_get_tx_power_offset_by_mcs(wlc_phy_t *ppi, uint8 mcs_offset)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	return pi->tx_power_offset[mcs_offset + 12];
}
#endif	/* NOT_USED */

void
wlc_lcnphy_crsuprs(phy_info_t *pi, int channel)
{
	uint16 afectrlovr, afectrlovrval;
	afectrlovr = read_phy_reg(pi, LCNPHY_AfeCtrlOvr);
	afectrlovrval = read_phy_reg(pi, LCNPHY_AfeCtrlOvrVal);
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (channel != 0) {
		MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvr, pwdn_dac_ovr, 1);
		MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvrVal, pwdn_dac_ovr_val, 0);
		MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvr, dac_clk_disable_ovr, 1);
		MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvrVal, dac_clk_disable_ovr_val, 0);
		write_phy_reg(pi, LCNPHY_ClkEnCtrl, 0xffff);
		wlc_lcnphy_tx_pu(pi, 1);
		/* Turn ON bphyframe */
		MOD_PHY_REG(pi, LCNPHY, BphyControl3, bphyFrmStartCntValue, 0);
		/* Turn on Tx Front End clks */
		or_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0x0080);
		/* start the rfcs signal */
		or_phy_reg(pi, LCNPHY_bphyTest, 0x228);
	} else {
		and_phy_reg(pi, LCNPHY_bphyTest, ~(0x228));
		/* disable clk to txFrontEnd */
		and_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xFF7F);
		write_phy_reg(pi, LCNPHY_AfeCtrlOvr, afectrlovr);
		write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, afectrlovrval);
		}
}

static void
wlc_lcnphy_toggle_afe_pwdn(phy_info_t *pi)
{
	uint16 save_AfeCtrlOvrVal, save_AfeCtrlOvr;

	save_AfeCtrlOvrVal = read_phy_reg(pi, LCNPHY_AfeCtrlOvrVal);
	save_AfeCtrlOvr = read_phy_reg(pi, LCNPHY_AfeCtrlOvr);

	write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal | 0x1);
	write_phy_reg(pi, LCNPHY_AfeCtrlOvr, save_AfeCtrlOvr | 0x1);

	write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal & 0xfffe);
	write_phy_reg(pi, LCNPHY_AfeCtrlOvr, save_AfeCtrlOvr & 0xfffe);

	write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, save_AfeCtrlOvrVal);
	write_phy_reg(pi, LCNPHY_AfeCtrlOvr, save_AfeCtrlOvr);
}

static void
wlc_lcnphy_txrx_spur_avoidance_mode(phy_info_t *pi, bool enable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (enable) {
		write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0x7);
		write_phy_reg(pi, LCNPHY_spur_canceller1, ((1 << 13) + 23));
		write_phy_reg(pi, LCNPHY_spur_canceller2, ((1 << 13) + 1989));
		/* do a soft reset */
		write_phy_reg(pi, LCNPHY_resetCtrl, 0x084);
		write_phy_reg(pi, LCNPHY_resetCtrl, 0x080);
		write_phy_reg(pi, LCNPHY_sslpnCtrl0, 0x2222);
		write_phy_reg(pi, LCNPHY_sslpnCtrl0, 0x2220);
	} else {
		write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0x0);
		write_phy_reg(pi, LCNPHY_spur_canceller1, ((0 << 13) + 23));
		write_phy_reg(pi, LCNPHY_spur_canceller2, ((0 << 13) + 1989));
	}
	wlapi_switch_macfreq(pi->sh->physhim, (enable ? WL_SPURAVOID_ON1 : WL_SPURAVOID_OFF));
}

void
wlc_phy_chanspec_set_lcnphy(phy_info_t *pi, chanspec_t chanspec)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	/* uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi); */
	phytbl_info_t tab;
	uint32 min_res_mask;
	uint32 res_mask_5g;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);

	wlc_lcnphy_set_chanspec_tweaks(pi, pi->radio_chanspec);

	if ((CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) &&
		(pi->sh->chippkg == BCM4336_WLBGA_PKG_ID)) {
	    /* Backoff lna2 gain */
		tab.tbl_len = dot11lcn_gain_tbl_4336wlbga_sz;
		tab.tbl_id = LCNPHY_TBL_ID_GAIN_TBL;
		tab.tbl_offset = 0;
		tab.tbl_width = 32;
		tab.tbl_ptr = dot11lcn_gain_tbl_4336wlbga;
		wlc_lcnphy_write_table(pi, &tab);
	}

	/* lcnphy_agc_reset */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);

	/* Tune radio for the channel */
	if (!NORADIO_ENAB(pi->pubpi)) {
		/* mapped to lcnphy_set_rf_pll_2064 proc */
		/* doubler enabled for 4313A0 */
		if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID))
			wlc_lcnphy_radio_2064_channel_tune_4313(pi, channel);
		else
			wlc_lcnphy_radio_2064_channel_tune(pi, channel);
		OSL_DELAY(1000);
	}

	/* toggle the afe whenever we move to a new channel */
	wlc_lcnphy_toggle_afe_pwdn(pi);
	/* mapped to lcnphy_set_sfo_chan_centers proc, comes from a * precalculated table */
	write_phy_reg(pi, LCNPHY_ptcentreTs20, lcnphy_sfo_cfg[channel - 1].ptcentreTs20);
	write_phy_reg(pi, LCNPHY_ptcentreFactor, lcnphy_sfo_cfg[channel - 1].ptcentreFactor);
	/* wlapi_bmac_write_shm(pi->sh->physhim, M_MAXRXFRM_LEN, 5000); */

	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {

		if (CHSPEC_CHANNEL(pi->radio_chanspec) == 14) {
			MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, txfiltSelect, 2);
			wlc_lcnphy_load_tx_iir_filter(pi, FALSE, 3);
		} else {
			MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, txfiltSelect, 1);
			wlc_lcnphy_load_tx_iir_filter(pi, FALSE, 2);
		}
		/* 4313: ofdm filter coeffs */
		wlc_lcnphy_load_tx_iir_filter(pi, TRUE, 0);

		MOD_PHY_REG(pi, LCNPHY, lpfbwlutreg1, lpf_cck_tx_bw, 1);

	} else {

		int16 cck_dig_filt_type;

		MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, txfiltSelect, 2);

		if (CHSPEC_CHANNEL(pi->radio_chanspec) == 14) {
			cck_dig_filt_type = 30;
		} else if (pi_lcn->lcnphy_cck_dig_filt_type != -1) {
			cck_dig_filt_type = pi_lcn->lcnphy_cck_dig_filt_type;
		} else {
			cck_dig_filt_type = 21;
		}

		if (0 != wlc_lcnphy_load_tx_iir_filter(pi, FALSE, cck_dig_filt_type))
			wlc_lcnphy_load_tx_iir_filter(pi, FALSE, 20);

		MOD_PHY_REG(pi, LCNPHY, lpfbwlutreg1, lpf_cck_tx_bw,
			(CHSPEC_CHANNEL(pi->radio_chanspec) == 14) ? 2 : 0);

		/* 4336: ofdm filter coeffs */
		wlc_lcnphy_load_tx_iir_filter(pi, TRUE, 2);
	}


	#if 0
	if (!SCAN_RM_IN_PROGRESS(pi) && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) &&
		SAVE_txpwrctrl && wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
			wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
			wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_TEMPBASED);
	}
	#endif

	/* Band5G support for 4330 */
	if ((CHIPID(pi->sh->chip) == BCM4330_CHIP_ID)) {

		min_res_mask = si_corereg(pi->sh->sih, SI_CC_IDX,
			OFFSETOF(chipcregs_t, min_res_mask), 0, 0);
		res_mask_5g = PMURES_BIT(RES4330_5gRX_PWRSW_PU) |
			PMURES_BIT(RES4330_5gTX_PWRSW_PU) |
			PMURES_BIT(RES4330_5g_LOGEN_PWRSW_PU);

		if (CHSPEC_IS5G(pi->radio_chanspec))
			min_res_mask |= res_mask_5g;
		else
			min_res_mask &= ~res_mask_5g;

		si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, min_res_mask),
			~0, min_res_mask);
	}

	/* 4313B0 is facing stability issues with per-channel cal */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		if ((CHIPID(pi->sh->chip) != BCM4313_CHIP_ID) && !SCAN_RM_IN_PROGRESS(pi)) {
			if (pi_lcn->lcnphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec))
				wlc_lcnphy_calib_modes(pi, PHY_FULLCAL);
			else
				wlc_lcnphy_restore_calibration_results(pi);
		}
	}

	if (!SCAN_RM_IN_PROGRESS(pi)) {
	  wlc_lcnphy_noise_measure_start(pi);
	}
}

static void
wlc_lcnphy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl = (read_phy_reg(pi, LCNPHY_AfeDACCtrl) >> LCNPHY_AfeDACCtrl_dac_ctrl_SHIFT);
	dac_ctrl = dac_ctrl & 0xc7f;
	dac_ctrl = dac_ctrl | (dac_gain << 7);
	MOD_PHY_REG(pi, LCNPHY, AfeDACCtrl, dac_ctrl, dac_ctrl);
}

static void
wlc_lcnphy_set_tx_gain_override(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	mod_phy_reg(pi, LCNPHY_rfoverride2,
		LCNPHY_rfoverride2_txgainctrl_ovr_MASK,
		bit << LCNPHY_rfoverride2_txgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, LCNPHY_rfoverride2,
		LCNPHY_rfoverride2_stxtxgainctrl_ovr_MASK,
		bit << LCNPHY_rfoverride2_stxtxgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		LCNPHY_AfeCtrlOvr_dacattctrl_ovr_MASK,
		bit << LCNPHY_AfeCtrlOvr_dacattctrl_ovr_SHIFT);
}

static uint16
wlc_lcnphy_get_pa_gain(phy_info_t *pi)
{
	uint16 pa_gain;

	pa_gain = (read_phy_reg(pi, LCNPHY_txgainctrlovrval1) &
		LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK) >>
		LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT;

	return pa_gain;
}

static void
wlc_lcnphy_set_tx_gain(phy_info_t *pi,  lcnphy_txgains_t *target_gains)
{
	uint16 pa_gain = wlc_lcnphy_get_pa_gain(pi);

	mod_phy_reg(pi, LCNPHY_txgainctrlovrval0,
		LCNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_MASK,
		((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		LCNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, LCNPHY_txgainctrlovrval1,
		LCNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_MASK,
		((target_gains->pad_gain) | (pa_gain << 8)) <<
		LCNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT);

	mod_phy_reg(pi, LCNPHY_stxtxgainctrlovrval0,
		LCNPHY_stxtxgainctrlovrval0_stxtxgainctrl_ovr_val0_MASK,
		((target_gains->gm_gain) | (target_gains->pga_gain << 8)) <<
		LCNPHY_stxtxgainctrlovrval0_stxtxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, LCNPHY_stxtxgainctrlovrval1,
		LCNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_MASK,
		((target_gains->pad_gain) | (pa_gain << 8)) <<
		LCNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT);

	wlc_lcnphy_set_dac_gain(pi, target_gains->dac_gain);

	/* Enable gain overrides */
	wlc_lcnphy_enable_tx_gain_override(pi);
}

static void
wlc_lcnphy_set_bbmult(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16)m0 << 8;
	phytbl_info_t tab;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_lcnphy_write_table(pi, &tab);
}

static void
wlc_lcnphy_clear_tx_power_offsets(phy_info_t *pi)
{
	uint32 data_buf[64];
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Clear out buffer */
	bzero(data_buf, sizeof(data_buf));

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = data_buf; /* ptr to buf */
	/* Since 4313A0 uses the rate offset table to do tx pwr ctrl for cck, */
	/* we shouldn't be clearing the rate offset table */

	if (!pi_lcn->lcnphy_uses_rate_offset_table) {
		/* Per rate power offset */
		tab.tbl_len = 30; /* # values   */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
		wlc_lcnphy_write_table(pi, &tab);
	}
	/* Per index power offset */
	tab.tbl_len = 64; /* # values   */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_lcnphy_write_table(pi, &tab);
}

typedef enum {
	LCNPHY_TSSI_PRE_PA,
	LCNPHY_TSSI_POST_PA,
	LCNPHY_TSSI_EXT
} lcnphy_tssi_mode_t;

static void
wlc_lcnphy_set_tssi_mux(phy_info_t *pi, lcnphy_tssi_mode_t pos)
{
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverride, 0x1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, tssiRangeOverrideVal, 1);
	/* Set TSSI/RSSI mux */
	if (LCNPHY_TSSI_POST_PA == pos) {
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, tssiSelVal0, 0);
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, tssiSelVal1, 1);
		if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
			mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
		} else {
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0x1);
			mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 0x8);
		}
	} else if (LCNPHY_TSSI_PRE_PA == pos) {
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, tssiSelVal0, 0x1);
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, tssiSelVal1, 0);
		if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
			mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
		} else {
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0);
			mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 0x8);
		}
	} else if (LCNPHY_TSSI_EXT == pos) {
		if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
			write_radio_reg(pi, RADIO_2064_REG07F, 1);
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1f, 3);
		} else {				/* this is for 4330 */
			if (CHSPEC_IS2G(pi->radio_chanspec))
				write_radio_reg(pi, RADIO_2064_REG07D, 3);
			else
				write_radio_reg(pi, RADIO_2064_REG07D, 2);
			mod_radio_reg(pi, RADIO_2064_REG028, 0xf, 0x1);
		}
		mod_radio_reg(pi, RADIO_2064_REG112, 0x80, 0x1 << 7);
		mod_radio_reg(pi, RADIO_2064_REG005, 0x7, 0x2);
	}
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmdNew, txPwrCtrlScheme, 0);
}

static uint16
wlc_lcnphy_rfseq_tbl_adc_pwrup(phy_info_t *pi)
{
	uint16 N1, N2, N3, N4, N5, N6, N;
	N1 = READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_delay);
	N2 = 1 << READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_intg_log2);
	N3 = READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_delay);
	N4 = 1 << READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_intg_log2);
	N5 = READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_delay);
	N6 = 1 << READ_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_intg_log2);
	N = 2 * (N1 + N2 + N3 + N4 + 2 *(N5 + N6)) + 80;
	if (N < 1600)
		N = 1600; /* min 20 us to avoid tx evm degradation */
	return N;
}

static void
wlc_lcnphy_pwrctrl_rssiparams(phy_info_t *pi)
{
	uint16 auxpga_vmid, auxpga_vmid_temp, auxpga_gain_temp;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	auxpga_vmid = (2 << 8) | (pi_lcn->lcnphy_rssi_vc << 4) | pi_lcn->lcnphy_rssi_vf;
	auxpga_vmid_temp = (2 << 8) | (8 << 4) | 4;
	auxpga_gain_temp = 2;

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelVmidOverride, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelGainOverride, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride, 0);

	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl2,
		LCNPHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelVmidVal0_MASK |
		LCNPHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelGainVal0_MASK,
		(auxpga_vmid << LCNPHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelVmidVal0_SHIFT) |
		(pi_lcn->lcnphy_rssi_gs << LCNPHY_TxPwrCtrlRfCtrl2_afeAuxpgaSelGainVal0_SHIFT));

	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl3,
		LCNPHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelVmidVal1_MASK |
		LCNPHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelGainVal1_MASK,
		(auxpga_vmid << LCNPHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelVmidVal1_SHIFT) |
		(pi_lcn->lcnphy_rssi_gs << LCNPHY_TxPwrCtrlRfCtrl3_afeAuxpgaSelGainVal1_SHIFT));

	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl4,
		LCNPHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelVmidVal2_MASK |
		LCNPHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelGainVal2_MASK,
		(auxpga_vmid << LCNPHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelVmidVal2_SHIFT) |
		(pi_lcn->lcnphy_rssi_gs << LCNPHY_TxPwrCtrlRfCtrl4_afeAuxpgaSelGainVal2_SHIFT));

	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl5,
		LCNPHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelVmidVal3_MASK |
		LCNPHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelGainVal3_MASK,
		(auxpga_vmid_temp << LCNPHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelVmidVal3_SHIFT) |
		(auxpga_gain_temp << LCNPHY_TxPwrCtrlRfCtrl5_afeAuxpgaSelGainVal3_SHIFT));

	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl6,
		LCNPHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelVmidVal4_MASK |
		LCNPHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelGainVal4_MASK,
		(auxpga_vmid_temp << LCNPHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelVmidVal4_SHIFT) |
		(auxpga_gain_temp << LCNPHY_TxPwrCtrlRfCtrl6_afeAuxpgaSelGainVal4_SHIFT));
	/* for tempsense */
	mod_radio_reg(pi, RADIO_2064_REG082, (1 << 5), (1 << 5));
}

static void
wlc_lcnphy_tssi_setup(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 ind;
	uint16 rfseq;
	uint32 cck_offset[4] = {22, 22, 22, 22};
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Setup estPwrLuts for measuring idle TSSI */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = &ind; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_offset = 0;
	for (ind = 0; ind < 128; ind++) {
		wlc_lcnphy_write_table(pi,  &tab);
		tab.tbl_offset++;
	}
	tab.tbl_offset = 704;
	for (ind = 0; ind < 128; ind++) {
		wlc_lcnphy_write_table(pi,  &tab);
		tab.tbl_offset++;
	}
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, rssifiltEn, 0);
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, rssiFormatConvEn, 0);
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, txpwrctrlEn, 1);
	if (!pi_lcn->ePA) {
		wlc_lcnphy_set_tssi_mux(pi, LCNPHY_TSSI_POST_PA);
	} else
		wlc_lcnphy_set_tssi_mux(pi, LCNPHY_TSSI_EXT);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_en, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, force_vbatTemp, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, pwrIndex_init, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_delay, 255);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_intg_log2, 5);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Npt_intg_log2, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_delay, 64);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_intg_log2, 4);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_delay, 64);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_intg_log2, 4);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlDeltaPwrLimit, DeltaPwrLimit, 0x1);
	wlc_lcnphy_clear_tx_power_offsets(pi);

	if (CHIPID(pi->sh->chip) != BCM4313_CHIP_ID) {
		if (LCNREV_GE(pi->pubpi.phy_rev, 2)) {
			MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, cckPwrOffset, 7);
		} else {
			MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, cckPwrOffset, 5);
		}
	} else {
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, cckPwrOffset, 0x1e0);
		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;     /* 32 bit wide  */
		tab.tbl_len = 4;        /* # values   */
		tab.tbl_ptr = cck_offset; /* ptr to buf */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
		pi_lcn->lcnphy_uses_rate_offset_table = TRUE;
		wlc_lcnphy_write_table(pi, &tab);
	}

	/*  Set idleTssi to (2^9-1) in OB format = (2^9-1-2^8) = 0xff in 2C format */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlIdleTssi, idleTssi0, 0xff);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlIdleTssi1, idleTssi1, 0xff);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (!pi_lcn->ePA)
			mod_radio_reg(pi, RADIO_2064_REG028, 0xf, 0xe);
		mod_radio_reg(pi, RADIO_2064_REG086, 0x4, 0x4);
	} else {
		/* 4313 doesn't want REG028 to be touched here */
		if ((CHIPID(pi->sh->chip) != BCM4313_CHIP_ID))
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1e, 0xe << 1);
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x1, 1);
		mod_radio_reg(pi, RADIO_2064_REG11A, 0x8, 1 << 3);
	}

	/* disable iqlocal */
	write_radio_reg(pi, RADIO_2064_REG025, 0xc);
	/* sel g tssi */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x1, 1);
	} else {
		if (CHSPEC_IS2G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 1 << 1);
		else
			mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 0 << 1);
	}
/* pa pwrup */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x2, 1 << 1);
	else
		mod_radio_reg(pi, RADIO_2064_REG03A, 0x4, 1 << 2);

	mod_radio_reg(pi, RADIO_2064_REG11A, 0x1, 1 << 0);
	/* amux sel port */
	mod_radio_reg(pi, RADIO_2064_REG005, 0x8, 1 << 3);
	/* no need for tssi setup for 4313A0 */
	if (!wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_MASK |
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_MASK,
			0 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_SHIFT |
			2 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_SHIFT);
	}

	rfseq = wlc_lcnphy_rfseq_tbl_adc_pwrup(pi);
	tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;	/* 12 bit wide	*/
	tab.tbl_ptr = &rfseq;
	tab.tbl_len = 1;
	tab.tbl_offset = 6;
	wlc_lcnphy_write_table(pi,  &tab);
	/* enable Rx Buf */
	MOD_PHY_REG(pi, LCNPHY, rfoverride4, lpf_buf_pwrup_ovr, 1);
	MOD_PHY_REG(pi, LCNPHY, rfoverride4val, lpf_buf_pwrup_ovr_val, 1);
	/* swap iq */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_swap_iq, 1);
	/* increase envelope detector gain */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, paCtrlTssiOverride, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, paCtrlTssiOverrideVal, 0);
	if (LCNREV_GE(pi->pubpi.phy_rev, 2))
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, invertTssiSamples, 1);

	wlc_lcnphy_pwrctrl_rssiparams(pi);
}

static void
wlc_lcnphy_btcx_override_enable(phy_info_t *pi)
{
	if (pi->sh->machwcap & MCAP_BTCX) {
		/* Ucode better be suspended when we mess with BTCX regs directly */
		ASSERT(0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

		/* Enable manual BTCX mode */
		W_REG(pi->sh->osh, &pi->regs->btcx_ctrl, 0x03);
		/* Force WLAN priority */
		W_REG(pi->sh->osh, &pi->regs->btcx_trans_ctrl, 0xff);
	}
}

void
wlc_lcnphy_tx_pwr_update_npt(phy_info_t *pi)
{
	uint16 tx_cnt, tx_total, npt;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;


	tx_total = wlc_lcnphy_total_tx_frames(pi);
	tx_cnt = tx_total - pi_lcn->lcnphy_tssi_tx_cnt;
	npt = wlc_lcnphy_get_tx_pwr_npt(pi);

	if (tx_cnt > (1 << npt)) {
		/* Reset frame counter */
		pi_lcn->lcnphy_tssi_tx_cnt = tx_total;


		/* Update cached power index & NPT */
		pi_lcn->lcnphy_tssi_idx = wlc_lcnphy_get_current_tx_pwr_idx(pi);
		pi_lcn->lcnphy_tssi_npt = npt;

		PHY_INFORM(("wl%d: %s: Index: %d, NPT: %d, TxCount: %d\n",
			pi->sh->unit, __FUNCTION__, pi_lcn->lcnphy_tssi_idx, npt, tx_cnt));
	}
}

int32
wlc_lcnphy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;
	/* On lcnphy, estPwrLuts0/1 table entries are in S6.3 format */
	a = 32768 + (a1 * tssi);
	b = (1024 * b0) + (64 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	return p;
}

static void
wlc_lcnphy_txpower_reset_npt(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return;
	/* For 4313, we start with index 70 for tssi based pwrctrl */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID))
		pi_lcn->lcnphy_tssi_idx = LCNPHY_TX_PWR_CTRL_START_INDEX_2G_4313;
	else
		pi_lcn->lcnphy_tssi_idx = LCNPHY_TX_PWR_CTRL_START_INDEX_2G;
	pi_lcn->lcnphy_tssi_npt = LCNPHY_TX_PWR_CTRL_START_NPT;
}

void
wlc_lcnphy_txpower_recalc_target(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 rate_table[WLC_NUM_RATES_CCK + WLC_NUM_RATES_OFDM + WLC_NUM_RATES_MCS_1_STREAM];
	uint i, j;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return;

	/* Adjust rate based power offset */
	for (i = 0, j = 0; i < ARRAYSIZE(rate_table); i++, j++) {

		if (i == WLC_NUM_RATES_CCK + WLC_NUM_RATES_OFDM)
			j = TXP_FIRST_MCS_20_SISO;

		rate_table[i] = (uint32)((int32)(-pi->tx_power_offset[j]));
		PHY_TMP((" Rate %d, offset %d\n", i, rate_table[i]));
	}

	if (!pi_lcn->lcnphy_uses_rate_offset_table) {
		/* Preset txPwrCtrltbl */
		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_len = ARRAYSIZE(rate_table); /* # values   */
		tab.tbl_ptr = rate_table; /* ptr to buf */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
		wlc_lcnphy_write_table(pi, &tab);
	}
	/* Set new target power */
	if (wlc_lcnphy_get_target_tx_pwr(pi) != pi->tx_power_min) {
		wlc_lcnphy_set_target_tx_pwr(pi, pi->tx_power_min);
		/* Should reset power index cache */
		wlc_lcnphy_txpower_reset_npt(pi);
	}
}

static void
wlc_lcnphy_set_tx_pwr_soft_ctrl(phy_info_t *pi, int8 index)
{
	uint32 cck_offset[4] = {22, 22, 22, 22};
	uint32 ofdm_offset;
	int i;
	uint16 index2;
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		return;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0x1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0x0);
	or_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0x0040);

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;     /* 32 bit wide  */
	tab.tbl_len = 4;        /* # values   */
	tab.tbl_ptr = cck_offset; /* ptr to buf */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;
	pi_lcn->lcnphy_uses_rate_offset_table = TRUE;
	wlc_lcnphy_write_table(pi, &tab);
	ofdm_offset = 0;
	tab.tbl_len = 1;
	tab.tbl_ptr = &ofdm_offset;
	for (i = 836; i < 862; i++) {
		tab.tbl_offset = i;
		wlc_lcnphy_write_table(pi, &tab);
	}

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_en, 0x1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0x1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, use_txPwrCtrlCoefs, 0x1);
	MOD_PHY_REG(pi, LCNPHY, rfoverride2, txgainctrl_ovr, 0);
	MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvr, dacattctrl_ovr, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlBaseIndex, loadBaseIndex, 1);
	index2 = (uint16)(index * 2);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlBaseIndex, uC_baseIndex0, index2);
	MOD_PHY_REG(pi, LCNPHY, papd_control2, papd_analog_gain_ovr, 0);
}

static int8
wlc_lcnphy_tempcompensated_txpwrctrl(phy_info_t *pi)
{
	int8 index, delta_brd, delta_temp, new_index, tempcorrx;
	int16 manp, meas_temp, temp_diff;
	bool neg = 0;
	uint16 temp;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi))
		return pi_lcn->lcnphy_current_index;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	index = FIXED_TXPWR; /* for 4313A0 FEM boards */

	if (NORADIO_ENAB(pi->pubpi))
		return index;

	if (pi_lcn->lcnphy_tempsense_slope == 0) {
			PHY_ERROR(("wl%d: %s: Correct nvram.txt is not taken\n",
				pi->sh->unit, __FUNCTION__));
			return index;
	}
	temp = (uint16)wlc_lcnphy_tempsense(pi, 0);
	meas_temp = LCNPHY_TEMPSENSE(temp);
	/* delta_brd = round((Precorded - Ptarget)*4) */
	/* for country specific regulatory limits */
	/* this check had to be put as on driver init, pi->tx_power_min comes up as 0 */
	if (pi->tx_power_min != 0) {
		delta_brd = (pi_lcn->lcnphy_measPower - pi->tx_power_min);
	} else {
		delta_brd = 0;
	}
	/* delta_temp = round((Trecorded - Tmeas)/Tslope*0.075*4) */
	/* tempsense_slope is multiplied with a scalar of 64 */
	manp = LCNPHY_TEMPSENSE(pi_lcn->lcnphy_rawtempsense);
	temp_diff = manp - meas_temp;
	if (temp_diff < 0) {
		/* delta_temp is negative */
		neg = 1;
		/* taking only the magnitude */
		temp_diff = -temp_diff;
	}
	/* 192 is coming from 0.075 * 4 * 64 * 10 */
	delta_temp = (int8)wlc_lcnphy_qdiv_roundup((uint32)(temp_diff*192),
		(uint32)(pi_lcn->lcnphy_tempsense_slope*10), 0);
	if (neg)
		delta_temp = -delta_temp;
	/* functionality of tempsense_option OTP param has been changed */
	/* value of 3 is for 4313B0 tssi based pwrctrl			*/
	if (pi_lcn->lcnphy_tempsense_option == 3 && LCNREV_IS(pi->pubpi.phy_rev, 0))
		delta_temp = 0;
	if (pi_lcn->lcnphy_tempcorrx > 31)
		tempcorrx = (int8)(pi_lcn->lcnphy_tempcorrx - 64);
	else
		tempcorrx = (int8)pi_lcn->lcnphy_tempcorrx;
	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		tempcorrx = 4; /* temporarily For B0 */
	new_index = index + delta_brd + delta_temp - pi_lcn->lcnphy_bandedge_corr;
	new_index += tempcorrx;
	/* B0: ceiling the tx pwr index to 127 if it is out of bound */
	if (LCNREV_IS(pi->pubpi.phy_rev, 1))
		index = 127;
	if (new_index < 0 || new_index > 126) {
		PHY_ERROR(("wl%d: %s: Tempcompensated tx index out of bound\n",
			pi->sh->unit, __FUNCTION__));
		return index;
		}
	PHY_INFORM(("wl%d: %s Settled to tx pwr index: %d\n",
		pi->sh->unit, __FUNCTION__, new_index));
	return new_index;
}
static uint16
wlc_lcnphy_set_tx_pwr_ctrl_mode(phy_info_t *pi, uint16 mode)
{
	/* the abstraction is for wlc_phy_cmn.c routines, tempsense based and tssi based */
	/* is LCNPHY_TX_PWR_CTRL_HW . internally, we route it through the correct path , */ 
	/* LCNPHY_TX_PWR_CTRL_TEMPBASED = 0xE001 so that TxPwrCtrlCmd gets set correctly */

	uint16 current_mode = mode;
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) &&
			mode == LCNPHY_TX_PWR_CTRL_HW)
				current_mode = LCNPHY_TX_PWR_CTRL_TEMPBASED;
		if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) &&
			mode == LCNPHY_TX_PWR_CTRL_TEMPBASED)
				current_mode = LCNPHY_TX_PWR_CTRL_HW;
	}
	return current_mode;
}
void
wlc_lcnphy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode)
{
	uint16 old_mode = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	int8 index;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ASSERT(
		(LCNPHY_TX_PWR_CTRL_OFF == mode) ||
		(LCNPHY_TX_PWR_CTRL_SW == mode) ||
		(LCNPHY_TX_PWR_CTRL_HW == mode) ||
		(LCNPHY_TX_PWR_CTRL_TEMPBASED == mode));

	mode = wlc_lcnphy_set_tx_pwr_ctrl_mode(pi, mode);
	old_mode = wlc_lcnphy_set_tx_pwr_ctrl_mode(pi, old_mode);

#if defined(WLTEST)
	/* Override power index if NVRAM says so */
	if (pi_lcn->txpwrindex_nvram && (LCNPHY_TX_PWR_CTRL_HW == mode)) {
		wlc_lcnphy_force_pwr_index(pi, pi_lcn->txpwrindex_nvram);
		mode = LCNPHY_TX_PWR_CTRL_OFF;
	}
#endif 

	/* Setting txfront end clock also along with hwpwr control */
	MOD_PHY_REG(pi, LCNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn,
		(LCNPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0);
	/* Feed back RF power level to PAPD block */
	MOD_PHY_REG(pi, LCNPHY, papd_control2, papd_analog_gain_ovr,
		(LCNPHY_TX_PWR_CTRL_HW == mode) ? 0 : 1);
	/* for phy rev2 and above, we need to set this register */
	if (LCNREV_GE(pi->pubpi.phy_rev, 2))
		MOD_PHY_REG(pi, LCNPHY, BBmultCoeffSel,  use_txPwrCtrlCoeffforBBMult,
			(LCNPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0);

	if (old_mode != mode) {
		if (LCNPHY_TX_PWR_CTRL_HW == old_mode) {
			/* Get latest power estimates before turning off power control */
			wlc_lcnphy_tx_pwr_update_npt(pi);
			/* Clear out all power offsets */
			wlc_lcnphy_clear_tx_power_offsets(pi);
		}
		if (LCNPHY_TX_PWR_CTRL_HW == mode) {
			/* Recalculate target power to restore power offsets */
			wlc_lcnphy_txpower_recalc_target(pi);
			/* Set starting index & NPT to best known values for that target */
			wlc_lcnphy_set_start_tx_pwr_idx(pi, pi_lcn->lcnphy_tssi_idx);
			wlc_lcnphy_set_tx_pwr_npt(pi, pi_lcn->lcnphy_tssi_npt);
			mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 0);
			/* Reset frame counter for NPT calculations */
			pi_lcn->lcnphy_tssi_tx_cnt = wlc_lcnphy_total_tx_frames(pi);
			/* Disable any gain overrides */
			wlc_lcnphy_disable_tx_gain_override(pi);
			pi_lcn->lcnphy_tx_power_idx_override = -1;
		}
		else
			wlc_lcnphy_enable_tx_gain_override(pi);

		/* Set requested tx power control mode */
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlCmd,
			(LCNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			LCNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			LCNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			mode);
		if (mode == LCNPHY_TX_PWR_CTRL_TEMPBASED) {
			index = wlc_lcnphy_tempcompensated_txpwrctrl(pi);
			wlc_lcnphy_set_tx_pwr_soft_ctrl(pi, index);
			pi_lcn->lcnphy_current_index = (int8)
				((read_phy_reg(pi, LCNPHY_TxPwrCtrlBaseIndex) & 0xFF)/2);
		}
		PHY_INFORM(("wl%d: %s: %s \n", pi->sh->unit, __FUNCTION__,
			mode ? ((LCNPHY_TX_PWR_CTRL_HW == mode) ? "Auto" : "Manual") : "Off"));
	}
}

static bool
wlc_lcnphy_iqcal_wait(phy_info_t *pi)
{
	uint delay_count = 0;

	while (wlc_lcnphy_iqcal_active(pi)) {
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

	return (0 == wlc_lcnphy_iqcal_active(pi));
}

static void
wlc_lcnphy_tx_iqlo_cal(
	phy_info_t *pi,
	lcnphy_txgains_t *target_gains,
	lcnphy_cal_mode_t cal_mode,
	bool keep_tone)
{
	/* starting values used in full cal
	 * -- can fill non-zero vals based on lab campaign (e.g., per channel)
	 * -- format: a0,b0,a1,b1,ci0_cq0_ci1_cq1,di0_dq0,di1_dq1,ei0_eq0,ei1_eq1,fi0_fq0,fi1_fq1
	 */
	lcnphy_txgains_t cal_gains, temp_gains;
	uint16 hash;
	uint8 band_idx;
	int j;
	uint16 ncorr_override[5];
	uint16 syst_coeffs[] =
		{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

	/* cal commands full cal and recal */
	uint16 commands_fullcal[] =  { 0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };

	/* do the recal with full cal cmds for now, re-visit if time becomes
	 * an issue.
	 */
	/* uint16 commands_recal[] =  { 0x8312, 0x8055, 0x8212 }; */
	uint16 commands_recal[] =  { 0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };

	/* calCmdNum register: log2 of settle/measure times for search/gain-ctrl, 4 bits each */
	uint16 command_nums_fullcal[] = { 0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };

	/* do the recal with full cal cmds for now, re-visit if time becomes
	 * an issue.
	 */
	/* uint16 command_nums_recal[] = {  0x7997, 0x7987, 0x7a97 }; */
	uint16 command_nums_recal[] = { 0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };
	uint16 *command_nums = command_nums_fullcal;

	uint16 *start_coeffs = NULL, *cal_cmds = NULL, cal_type, diq_start;
	uint16 tx_pwr_ctrl_old, save_txpwrctrlrfctrl2;
	uint16 save_sslpnCalibClkEnCtrl, save_sslpnRxFeClkEnCtrl;
	bool tx_gain_override_old;
	lcnphy_txgains_t old_gains;
	uint i, n_cal_cmds = 0, n_cal_start = 0;
	uint16 *values_to_save;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	if (NULL == (values_to_save = MALLOC(pi->sh->osh, sizeof(uint16) * 20))) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	save_sslpnRxFeClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl);
	save_sslpnCalibClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);

	/* turn on clk to iqlo block */
	or_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0x40);
	or_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, 0x3);

	/* Get desired start coeffs and select calibration command sequence */

	switch (cal_mode) {
		case LCNPHY_CAL_FULL:
			start_coeffs = syst_coeffs;
			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			break;

		case LCNPHY_CAL_RECAL:
			ASSERT(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs_valid);

			/* start_coeffs = pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs; */

			/* since re-cal is same as full cal */
			start_coeffs = syst_coeffs;

			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
			command_nums = command_nums_recal;
			break;
		default:
			ASSERT(FALSE);
	}

	/* Fill in Start Coeffs */
	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		start_coeffs, 11, 16, 64);

	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xffff);
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, iqlocalEn, 1);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_lcnphy_get_tx_pwr_ctrl(pi);

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_swap_iq, 1);

	/* Disable tx power control */
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	save_txpwrctrlrfctrl2 = read_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl2);

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl2, afeAuxpgaSelVmidVal0, 0x2a6);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl2, afeAuxpgaSelGainVal0, 2);

	/* setup tx iq loopback path */
	wlc_lcnphy_tx_iqlo_loopback(pi, values_to_save);

	/* Save old and apply new tx gains if needed */
	tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_lcnphy_get_tx_gain(pi, &old_gains);

	if (!target_gains) {
		if (!tx_gain_override_old)
			wlc_lcnphy_set_tx_pwr_by_index(pi, pi_lcn->lcnphy_tssi_idx);
		wlc_lcnphy_get_tx_gain(pi, &temp_gains);
		target_gains = &temp_gains;
	}

	hash = (target_gains->gm_gain << 8) |
		(target_gains->pga_gain << 4) |
		(target_gains->pad_gain);

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	cal_gains = *target_gains;
	bzero(ncorr_override, sizeof(ncorr_override));
	for (j = 0; j < iqcal_gainparams_numgains_lcnphy[band_idx]; j++) {
		if (hash == tbl_iqcal_gainparams_lcnphy[band_idx][j][0]) {
			cal_gains.gm_gain = tbl_iqcal_gainparams_lcnphy[band_idx][j][1];
			cal_gains.pga_gain = tbl_iqcal_gainparams_lcnphy[band_idx][j][2];
			cal_gains.pad_gain = tbl_iqcal_gainparams_lcnphy[band_idx][j][3];
			bcopy(&tbl_iqcal_gainparams_lcnphy[band_idx][j][3], ncorr_override,
				sizeof(ncorr_override));
			break;
		}
	}
	/* apply cal gains */
	wlc_lcnphy_set_tx_gain(pi, &cal_gains);

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


	/* set gain control parameters */
	write_phy_reg(pi, LCNPHY_iqloCalCmdGctl, 0xaa9);
	write_phy_reg(pi, LCNPHY_iqloCalGainThreshD2, 0xc0);

	/* Load the LO compensation gain table */
	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		(CONST void *)lcnphy_iqcal_loft_gainladder, ARRAYSIZE(lcnphy_iqcal_loft_gainladder),
		16, 0);
	/* Load the IQ calibration gain table */
	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		(CONST void *)lcnphy_iqcal_ir_gainladder, ARRAYSIZE(lcnphy_iqcal_ir_gainladder),
		16, 32);

	/* Send out calibration tone */
	if (pi->phy_tx_tone_freq) {
		/* if tone is already played out with iq cal mode zero then
		 * stop the tone and re-play with iq cal mode 1.
		 */
		wlc_lcnphy_stop_tx_tone(pi);
		OSL_DELAY(5);
		wlc_lcnphy_start_tx_tone(pi, 3750, 88, 1);
	} else {
		wlc_lcnphy_start_tx_tone(pi, 3750, 88, 1);
	}

	/* FIX ME: re-enable all the phy clks. */
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xffff);

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

		write_phy_reg(pi, LCNPHY_iqloCalCmdNnum, command_num);

		PHY_TMP(("wl%d: %s: running cmd: %x, cmd_num: %x\n",
			pi->sh->unit, __FUNCTION__, cal_cmds[i], command_nums[i]));

		/* need to set di/dq to zero if analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			/* store given dig LOFT comp vals */
			wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);
			/* Set to zero during analog LO cal */
			wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL, &zero_diq,
				1, 16, 69);
		}

		/* Issue cal command */
		write_phy_reg(pi, LCNPHY_iqloCalCmd, cal_cmds[i]);

		/* Wait until cal command finished */
		if (!wlc_lcnphy_iqcal_wait(pi)) {
			PHY_ERROR(("wl%d: %s: tx iqlo cal failed to complete\n",
				pi->sh->unit, __FUNCTION__));
			/* No point to continue */
			goto cleanup;
		}

		/* Copy best coefficients to start coefficients */
		wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
			best_coeffs, ARRAYSIZE(best_coeffs), 16, 96);
		wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL, best_coeffs,
			ARRAYSIZE(best_coeffs), 16, 64);
		/* restore di/dq in case of analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);
		}
		wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
			pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs,
			ARRAYSIZE(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs), 16, 96);
	}

	/*
	 * Apply Results
	 */

	/* Save calibration results */
	wlc_lcnphy_common_read_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs,
		ARRAYSIZE(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs), 16, 96);
	pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs_valid = TRUE;

	/* Apply IQ Cal Results */
	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		&pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[0], 4, 16, 80);
	/* Apply Digital LOFT Comp */
	wlc_lcnphy_common_write_table(pi, LCNPHY_TBL_ID_IQLOCAL,
		&pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[5], 2, 16, 85);
	/* Dump results */
	PHY_INFORM(("wl%d: %s complete, IQ %d %d LO %d %d %d %d %d %d\n",
		pi->sh->unit, __FUNCTION__,
		(int16)pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[0],
		(int16)pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[1],
		(int8)((pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[5] & 0xff00) >> 8),
		(int8)(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[5] & 0x00ff),
		(int8)((pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[7] & 0xff00) >> 8),
		(int8)(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[7] & 0x00ff),
		(int8)((pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[9] & 0xff00) >> 8),
		(int8)(pi_lcn->lcnphy_cal_results.txiqlocal_bestcoeffs[9] & 0x00ff)));

cleanup:
	wlc_lcnphy_tx_iqlo_loopback_cleanup(pi, values_to_save);
	MFREE(pi->sh->osh, values_to_save, 20 * sizeof(uint16));
	/* Switch off test tone */
	if (!keep_tone)
		wlc_lcnphy_stop_tx_tone(pi);

	/* restore tx power and reenable tx power control */
	write_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrl2, save_txpwrctrlrfctrl2);

	/* Reset calibration  command register */
	write_phy_reg(pi, LCNPHY_iqloCalCmdGctl, 0);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_lcnphy_set_tx_gain(pi, &old_gains);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);

	/* Restoration RxFE clk */
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, save_sslpnCalibClkEnCtrl);
	write_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, save_sslpnRxFeClkEnCtrl);

}

static void
wlc_lcnphy_idle_tssi_est(wlc_phy_t *ppi)
{
	bool suspend, tx_gain_override_old;
	lcnphy_txgains_t old_gains;
	phy_info_t *pi = (phy_info_t *)ppi;
	uint16 idleTssi, idleTssi0_2C, idleTssi0_OB, idleTssi0_regvalue_OB, idleTssi0_regvalue_2C;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	uint16 SAVE_lpfgain = read_radio_reg(pi, RADIO_2064_REG112);
	uint16 SAVE_jtag_bb_afe_switch = read_radio_reg(pi, RADIO_2064_REG007) & 1;
	uint16 SAVE_jtag_auxpga = read_radio_reg(pi, RADIO_2064_REG0FF) & 0x10;
	uint16 SAVE_iqadc_aux_en = read_radio_reg(pi, RADIO_2064_REG11F) & 4;
	idleTssi = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatus);
	suspend = (0 == (R_REG(pi->sh->osh, &((phy_info_t *)pi)->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
	wlc_lcnphy_btcx_override_enable(pi);
	/* Save old tx gains if needed */
	tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
	wlc_lcnphy_get_tx_gain(pi, &old_gains);
	/* set txgain override */
	wlc_lcnphy_enable_tx_gain_override(pi);
	wlc_lcnphy_set_tx_pwr_by_index(pi, 127);
	write_radio_reg(pi, RADIO_2064_REG112, 0x6);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, 1);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, 1 << 4);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 1 << 2);
	wlc_lcnphy_tssi_setup(pi);
	wlc_phy_do_dummy_tx(pi, TRUE, OFF);
	idleTssi = READ_PHY_REG(pi, LCNPHY, TxPwrCtrlStatus, estPwr);
	/* avgTssi value is in 2C (S9.0) format */
	idleTssi0_2C = READ_PHY_REG(pi, LCNPHY, TxPwrCtrlStatusNew4, avgTssi);
	/* Convert idletssi1_2C from 2C to OB format by toggling MSB OB value */
	/* ranges from 0 to (2^9-1) = 511, 2C value ranges from -256 to (2^9-1-2^8) = 255 */
	/* Convert 9-bit idletssi1_2C to 9-bit idletssi1_OB. */
	if (idleTssi0_2C >= 256)
		idleTssi0_OB = idleTssi0_2C - 256;
	else
		idleTssi0_OB = idleTssi0_2C + 256;
	/* Convert 9-bit idletssi1_OB to 7-bit value for comparison with idletssi */
	if (idleTssi != (idleTssi0_OB >> 2))
	/* causing cstyle error */
	PHY_ERROR(("wl%d: %s, ERROR: idleTssi estPwr(OB): "
	           "0x%04x Register avgTssi(OB, 7MSB): 0x%04x\n",
	           pi->sh->unit, __FUNCTION__, idleTssi, idleTssi0_OB >> 2));

	if ((CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) && LCNREV_LT(pi->pubpi.phy_rev, 1)) {
		int diff = (LCN_TARGET_TSSI * 4 - idleTssi0_OB) > 0 ?
			LCN_TARGET_TSSI * 4 - idleTssi0_OB : 0;
		idleTssi0_regvalue_OB = 511 - diff;
	} else
		idleTssi0_regvalue_OB = idleTssi0_OB;

	if (idleTssi0_regvalue_OB >= 256)
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB - 256;
	else
		idleTssi0_regvalue_2C = idleTssi0_regvalue_OB + 256;

	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlIdleTssi, idleTssi0, idleTssi0_regvalue_2C);
	/* Clear tx PU override */
	MOD_PHY_REG(pi, LCNPHY, RFOverride0, internalrftxpu_ovr, 0);
	/* restore txgain override */
	wlc_lcnphy_set_tx_gain_override(pi, tx_gain_override_old);
	wlc_lcnphy_set_tx_gain(pi, &old_gains);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
	/* restore radio registers */
	write_radio_reg(pi, RADIO_2064_REG112, SAVE_lpfgain);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, SAVE_jtag_bb_afe_switch);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, SAVE_jtag_auxpga);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, SAVE_iqadc_aux_en);
	/* making amux_sel_port to be always 1 for 4313 tx pwr ctrl */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID))
		mod_radio_reg(pi, RADIO_2064_REG112, 0x80, 1 << 7);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

int
wlc_lcnphy_idle_tssi_est_iovar(phy_info_t *pi, bool type)
{
	/* type = 0 will just do idle_tssi_est */
	/* type = 1 will do full tx_pwr_ctrl_init */

	if (!type)
		wlc_lcnphy_idle_tssi_est((wlc_phy_t *)pi);
	else
		wlc_lcnphy_tx_pwr_ctrl_init((wlc_phy_t *)pi);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);

	return (read_phy_reg(pi, LCNPHY_TxPwrCtrlIdleTssi));
}

static void
wlc_lcnphy_vbat_temp_sense_setup(phy_info_t *pi, uint8 mode)
{
	bool suspend;
	uint16 save_txpwrCtrlEn;
	uint8 auxpga_vmidcourse, auxpga_vmidfine, auxpga_gain;
	uint16 auxpga_vmid;
	phytbl_info_t tab;
	uint16 val;
	uint8 save_reg007, save_reg0FF, save_reg11F, save_reg005, save_reg025, save_reg112;
	uint16 values_to_save[14];
	int8 index;
	int i;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	OSL_DELAY(999);

	/* save older states of registers */
	/* radio */
	save_reg007 = (uint8)read_radio_reg(pi, RADIO_2064_REG007);
	save_reg0FF = (uint8)read_radio_reg(pi, RADIO_2064_REG0FF);
	save_reg11F = (uint8)read_radio_reg(pi, RADIO_2064_REG11F);
	save_reg005 = (uint8)read_radio_reg(pi, RADIO_2064_REG005);
	save_reg025 = (uint8)read_radio_reg(pi, RADIO_2064_REG025);
	save_reg112 = (uint8)read_radio_reg(pi, RADIO_2064_REG112);
	/* phy */
	for (i = 0; i < 14; i++)
		values_to_save[i] = read_phy_reg(pi, tempsense_phy_regs[i]);
	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	save_txpwrCtrlEn = read_radio_reg(pi, LCNPHY_TxPwrCtrlCmd);
	/* disable tx power control measurement */
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
	index = pi_lcn->lcnphy_current_index;
	wlc_lcnphy_set_tx_pwr_by_index(pi, 127);
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, 0x1);
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, 0x1 << 4);
	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, 0x1 << 2);
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, rssifiltEn, 0);
	MOD_PHY_REG(pi, LCNPHY, auxadcCtrl, rssiFormatConvEn, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, hwtxPwrCtrl_en, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_en, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, force_vbatTemp, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_delay, 255);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Ntssi_intg_log2, 5);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNnum, Npt_intg_log2, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_delay, 64);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_Vbat, Nvbat_intg_log2, 6);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_delay, 64);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlNum_temp, Ntemp_intg_log2, 6);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, amuxSelPortVal0, 2);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, amuxSelPortVal1, 3);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl0, amuxSelPortVal2, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl1, tempsenseSwapVal0, 0);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrl1, tempsenseSwapVal1, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1);
	/* disable iqlo cal */
	write_radio_reg(pi, RADIO_2064_REG025, 0xC);
	/* amux sel port */
	mod_radio_reg(pi, RADIO_2064_REG005, 0x8, 0x1 << 3);
	/* enable Rx buf */
	MOD_PHY_REG(pi, LCNPHY, rfoverride4, lpf_buf_pwrup_ovr, 1);
	MOD_PHY_REG(pi, LCNPHY, rfoverride4val, lpf_buf_pwrup_ovr_val, 1);
	/* swap iq */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlCmd, txPwrCtrl_swap_iq, 1);
	/* initialize the delay value for keeping ADC powered up during Tx */
	val = wlc_lcnphy_rfseq_tbl_adc_pwrup(pi);
	tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
	tab.tbl_width = 16;     /* 12 bit wide  */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &val; /* ptr to buf */
	tab.tbl_offset = 6;
	wlc_lcnphy_write_table(pi, &tab);
	if (mode == TEMPSENSE) {
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride, 1);
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverrideVal, 1);
		auxpga_vmidcourse = 8;
		auxpga_vmidfine = 0x4;
		auxpga_gain = 2;
		mod_radio_reg(pi, RADIO_2064_REG082, 0x20, 1 << 5);
	} else {
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride, 1);
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverrideVal, 3);
		auxpga_vmidcourse = 7;
		auxpga_vmidfine = 0xa;
		auxpga_gain = 2;
	}
	auxpga_vmid = (uint16)((2 << 8) | (auxpga_vmidcourse << 4) | auxpga_vmidfine);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelVmidOverride, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelVmidOverrideVal, auxpga_vmid);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelGainOverride, 1);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride1, afeAuxpgaSelGainOverrideVal, auxpga_gain);
	/* trigger Vbat and Temp sense measurements */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, force_vbatTemp, 1);
	/* to put lpf in Tx */
	write_radio_reg(pi, RADIO_2064_REG112, 0x6);
	/* do dummy tx */
	wlc_phy_do_dummy_tx(pi, TRUE, OFF);
	if (!tempsense_done(pi))
		OSL_DELAY(10);
	/* clean up */
	write_radio_reg(pi, RADIO_2064_REG007, (uint16)save_reg007);
	write_radio_reg(pi, RADIO_2064_REG0FF, (uint16)save_reg0FF);
	write_radio_reg(pi, RADIO_2064_REG11F, (uint16)save_reg11F);
	write_radio_reg(pi, RADIO_2064_REG005, (uint16)save_reg005);
	write_radio_reg(pi, RADIO_2064_REG025, (uint16)save_reg025);
	write_radio_reg(pi, RADIO_2064_REG112, (uint16)save_reg112);
	for (i = 0; i < 14; i++)
		write_phy_reg(pi, tempsense_phy_regs[i], values_to_save[i]);
	wlc_lcnphy_set_tx_pwr_by_index(pi, (int)index);
	/* restore the state of txpwrctrl */
	write_radio_reg(pi, LCNPHY_TxPwrCtrlCmd, save_txpwrCtrlEn);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
	OSL_DELAY(999);
}

void
WLBANDINITFN(wlc_lcnphy_tx_pwr_ctrl_init)(wlc_phy_t *ppi)
{
	lcnphy_txgains_t tx_gains;
	uint8 bbmult;
	phytbl_info_t tab;
	int32 a1, b0, b1;
	int32 tssi, pwr, maxtargetpwr, mintargetpwr;
	bool suspend;
	phy_info_t *pi = (phy_info_t *)ppi;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (NORADIO_ENAB(pi->pubpi)) {
		wlc_lcnphy_set_bbmult(pi, 0x30);
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
		return;
	}

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
		wlc_lcnphy_set_tx_gain(pi, &tx_gains);
		wlc_lcnphy_set_bbmult(pi, bbmult);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	} else {

		wlc_lcnphy_idle_tssi_est(ppi);

		/* Clear out all power offsets */
		wlc_lcnphy_clear_tx_power_offsets(pi);

		/* On lcnphy, estPwrLuts0/1 table entries are in S6.3 format */
		b0 = pi->txpa_2g[0];
		b1 = pi->txpa_2g[1];
		a1 = pi->txpa_2g[2];
		/* 4336 has an non-inverted tssi slope */
		if ((CHIPID(pi->sh->chip) != BCM4313_CHIP_ID)) {
			if (LCNREV_GE(pi->pubpi.phy_rev, 1)) {
				maxtargetpwr = wlc_lcnphy_tssi2dbm(1, a1, b0, b1);
				mintargetpwr = 0;
			} else {
				maxtargetpwr = wlc_lcnphy_tssi2dbm(127, a1, b0, b1);
				mintargetpwr = wlc_lcnphy_tssi2dbm(LCN_TARGET_TSSI, a1, b0, b1);
			}
		} else {
			maxtargetpwr = wlc_lcnphy_tssi2dbm(10, a1, b0, b1);
			mintargetpwr = wlc_lcnphy_tssi2dbm(125, a1, b0, b1);
		}
		/* Convert tssi to power LUT */
		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_ptr = &pwr; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_offset = 0; /* estPwrLuts */
		for (tssi = 0; tssi < 128; tssi++) {
			pwr = wlc_lcnphy_tssi2dbm(tssi, a1, b0, b1);
			if (LCNREV_LT(pi->pubpi.phy_rev, 2))
				pwr = (pwr < mintargetpwr) ? mintargetpwr : pwr;
			wlc_lcnphy_write_table(pi,  &tab);
			tab.tbl_offset++;
		}

		MOD_PHY_REG(pi, LCNPHY, crsgainCtrl, crseddisable, 0);
		write_phy_reg(pi, LCNPHY_TxPwrCtrlDeltaPwrLimit, 10);

		wlc_lcnphy_set_target_tx_pwr(pi, LCN_TARGET_PWR);

#ifdef WLNOKIA_NVMEM
		/* update the cck power detector offset, these are in 1/8 dBs */
		{
			int8 cckoffset, ofdmoffset;
			int16 diff;
			/* update the cck power detector offset */
			wlc_phy_get_pwrdet_offsets(pi, &cckoffset, &ofdmoffset);
			PHY_INFORM(("cckoffset is %d and ofdmoffset is %d\n",
				cckoffset, ofdmoffset));

			diff = cckoffset - ofdmoffset;
			if (diff >= 0)
				diff &= 0x0FF;
			else
				diff &= 0xFF | 0x100;

			/* program the cckpwoffset bits(6-14) in reg 0x4d0 */
			MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRangeCmd, cckPwrOffset, diff);

			/* program the tempsense curr in reg 0x50c */
			if (ofdmoffset >= 0)
				diff = ofdmoffset;
			else
				diff = 0x100 | ofdmoffset;
			MOD_PHY_REG(pi, LCNPHY, TempSenseCorrection, tempsenseCorr, diff);
		}
#endif /* WLNOKIA_NVMEM */

		/* Enable hardware power control */
		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
	}
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

/* Save/Restore digital filter state OFDM filter settings */
static void
wlc_lcnphy_save_restore_dig_filt_state(phy_info_t *pi, bool save, uint16 *filtcoeffs)
{
	int j;

	uint16 addr_ofdm[] = {
		LCNPHY_txfilt20Stg1Shft,
		LCNPHY_txfilt20CoeffStg0A1,
		LCNPHY_txfilt20CoeffStg0A2,
		LCNPHY_txfilt20CoeffStg0B1,
		LCNPHY_txfilt20CoeffStg0B2,
		LCNPHY_txfilt20CoeffStg0B3,
		LCNPHY_txfilt20CoeffStg1A1,
		LCNPHY_txfilt20CoeffStg1A2,
		LCNPHY_txfilt20CoeffStg1B1,
		LCNPHY_txfilt20CoeffStg1B2,
		LCNPHY_txfilt20CoeffStg1B3,
		LCNPHY_txfilt20CoeffStg2A1,
		LCNPHY_txfilt20CoeffStg2A2,
		LCNPHY_txfilt20CoeffStg2B1,
		LCNPHY_txfilt20CoeffStg2B2,
		LCNPHY_txfilt20CoeffStg2B3
		};


	if (save) {
		for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
			filtcoeffs[j] = read_phy_reg(pi, addr_ofdm[j]);
		}
	} else {
		for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
			write_phy_reg(pi, addr_ofdm[j], filtcoeffs[j]);
		}
	}
}

static uint8
wlc_lcnphy_get_bbmult(phy_info_t *pi)
{
	uint16 m0m1;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_lcnphy_read_table(pi, &tab);

	return (uint8)((m0m1 & 0xff00) >> 8);
}

static void
wlc_lcnphy_set_pa_gain(phy_info_t *pi, uint16 gain)
{
	mod_phy_reg(pi, LCNPHY_txgainctrlovrval1,
		LCNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK,
		gain << LCNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT);
	mod_phy_reg(pi, LCNPHY_stxtxgainctrlovrval1,
		LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK,
		gain << LCNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT);
}

void
wlc_lcnphy_get_radio_loft(phy_info_t *pi,
	uint8 *ei0,
	uint8 *eq0,
	uint8 *fi0,
	uint8 *fq0)
{
	*ei0 = LCNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2064_REG089));
	*eq0 = LCNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2064_REG08A));
	*fi0 = LCNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2064_REG08B));
	*fq0 = LCNPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2064_REG08C));
}

static uint16
lcnphy_iqlocc_write(phy_info_t *pi, uint8 data)
{
	int32 data32 = (int8)data;
	int32 rf_data32;
	int32 ip, in;
	ip = 8 + (data32 >> 1);
	in = 8 - ((data32+1) >> 1);
	rf_data32 = (in << 4) | ip;
	return (uint16)(rf_data32);
}

void
wlc_lcnphy_set_radio_loft(phy_info_t *pi,
	uint8 ei0,
	uint8 eq0,
	uint8 fi0,
	uint8 fq0)
{
	write_radio_reg(pi, RADIO_2064_REG089, lcnphy_iqlocc_write(pi, ei0));
	write_radio_reg(pi, RADIO_2064_REG08A, lcnphy_iqlocc_write(pi, eq0));
	write_radio_reg(pi, RADIO_2064_REG08B, lcnphy_iqlocc_write(pi, fi0));
	write_radio_reg(pi, RADIO_2064_REG08C, lcnphy_iqlocc_write(pi, fq0));
}

static void
wlc_lcnphy_get_tx_gain(phy_info_t *pi,  lcnphy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = read_phy_reg(pi, LCNPHY_AfeDACCtrl) >>
		LCNPHY_AfeDACCtrl_dac_ctrl_SHIFT;
	gains->dac_gain = (dac_gain & 0x380) >> 7;

	{
		uint16 rfgain0, rfgain1;

		rfgain0 = (read_phy_reg(pi, LCNPHY_txgainctrlovrval0) &
			LCNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_MASK) >>
			LCNPHY_txgainctrlovrval0_txgainctrl_ovr_val0_SHIFT;
		rfgain1 = (read_phy_reg(pi, LCNPHY_txgainctrlovrval1) &
			LCNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_MASK) >>
			LCNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT;

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

void
wlc_lcnphy_set_tx_iqcc(phy_info_t *pi, uint16 a, uint16 b)
{
	phytbl_info_t tab;
	uint16 iqcc[2];

	/* Fill buffer with coeffs */
	iqcc[0] = a;
	iqcc[1] = b;
	/* Update iqloCaltbl */
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;			/* iqloCaltbl	*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_ptr = iqcc;
	tab.tbl_len = 2;
	tab.tbl_offset = 80;
	wlc_lcnphy_write_table(pi, &tab);
}

void
wlc_lcnphy_set_tx_locc(phy_info_t *pi, uint16 didq)
{
	phytbl_info_t tab;

	/* Update iqloCaltbl */
	tab.tbl_id = LCNPHY_TBL_ID_IQLOCAL;			/* iqloCaltbl	*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_lcnphy_write_table(pi, &tab);
}

void
wlc_lcnphy_set_tx_pwr_by_index(phy_info_t *pi, int index)
{
	/* Turn off automatic power control */
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	/* Force tx power from the index */
	wlc_lcnphy_force_pwr_index(pi, index);
}

static void
wlc_lcnphy_force_pwr_index(phy_info_t *pi, int index)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
	lcnphy_txgains_t gains;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ASSERT(index <= LCNPHY_MAX_TX_POWER_INDEX);

	/* Save forced index */
	pi_lcn->lcnphy_tx_power_idx_override = (int8)index;
	pi_lcn->lcnphy_current_index = (uint8)index;

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + index; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_lcnphy_read_table(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + index; /* gainCtrlLuts */
	tab.tbl_width = 32;
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_lcnphy_read_table(pi,  &tab);
	/* Apply tx gain */
	gains.gm_gain = (uint16)(txgain & 0xff);
	gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
	gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
	gains.dac_gain = (uint16)(bbmultiqcomp >> 28) & 0x07;
	wlc_lcnphy_set_tx_gain(pi, &gains);
	wlc_lcnphy_set_pa_gain(pi,  (uint16)(txgain >> 24) & 0x7f);
	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 20) & 0xff);
	wlc_lcnphy_set_bbmult(pi, bb_mult);
	/* Enable gain overrides */
	wlc_lcnphy_enable_tx_gain_override(pi);
	/* the reading and applying lo, iqcc coefficients is not getting done for 4313A0 */
	/* to be fixed */

	if (!wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
		/* Apply iqcc */
		a = (uint16)((bbmultiqcomp >> 10) & 0x3ff);
		b = (uint16)(bbmultiqcomp & 0x3ff);
		wlc_lcnphy_set_tx_iqcc(pi, a, b);
		/* Read index based di & dq from the table */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_LO_OFFSET + index; /* loftCoefLuts */
		tab.tbl_ptr = &locoeffs; /* ptr to buf */
		wlc_lcnphy_read_table(pi,  &tab);
		/* Apply locc */
		wlc_lcnphy_set_tx_locc(pi, (uint16)locoeffs);
		/* Apply PAPD rf power correction */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_PWR_OFFSET + index;
		tab.tbl_ptr = &rfpower; /* ptr to buf */
		wlc_lcnphy_read_table(pi,  &tab);
		MOD_PHY_REG(pi, LCNPHY, papd_analog_gain_ovr_val,
			papd_analog_gain_ovr_val, rfpower * 8);
	}
}

static void
wlc_lcnphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx)
{
	/* Set TR switch */
	mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
		LCNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK |
		LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		(tx ? LCNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK : 0) |
		(rx ? LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK : 0));

	/* Enable overrides */
	or_phy_reg(pi, LCNPHY_RFOverride0,
		LCNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		LCNPHY_RFOverride0_trsw_rx_pu_ovr_MASK);
}

static void
wlc_lcnphy_clear_papd_comptable(phy_info_t *pi)
{
	uint32 j;
	phytbl_info_t tab;
	uint32 temp_offset[128];
	tab.tbl_ptr = temp_offset; /* ptr to buf */
	tab.tbl_len = 128;        /* # values   */
	tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;         /* papdcompdeltatbl */
	tab.tbl_width = 32;     /* 32 bit wide */
	tab.tbl_offset = 0; /* tbl offset */

	bzero(temp_offset, sizeof(temp_offset));
	for (j = 1; j < 128; j += 2)
		temp_offset[j] = 0x80000;

	wlc_lcnphy_write_table(pi, &tab);
	return;
}
static void
wlc_lcnphy_set_rx_gain_by_distribution(phy_info_t *pi,
	uint16 trsw,
	uint16 ext_lna,
	uint16 biq2,
	uint16 biq1,
	uint16 tia,
	uint16 lna2,
	uint16 lna1)
{
	uint16 gain0_15, gain16_19;

	gain16_19 = biq2 & 0xf;
	gain0_15 = ((biq1 & 0xf) << 12) |
		((tia & 0xf) << 8) |
		((lna2 & 0x3) << 6) |
		((lna2 & 0x3) << 4) |
		((lna1 & 0x3) << 2) |
		((lna1 & 0x3) << 0);

	mod_phy_reg(pi, LCNPHY_rxgainctrl0ovrval,
		LCNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_MASK,
		gain0_15 << LCNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, LCNPHY_rxlnaandgainctrl1ovrval,
		LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_MASK,
		gain16_19 << LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_SHIFT);
	mod_phy_reg(pi, LCNPHY_rfoverride2val,
		LCNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_MASK,
		lna1 << LCNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_SHIFT);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_MASK,
			ext_lna << LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_MASK,
			ext_lna << LCNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_SHIFT);
	}
	else {
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_slna_mode_ovr_val_MASK,
			0 << LCNPHY_rfoverride2val_slna_mode_ovr_val_SHIFT);

		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_slna_byp_ovr_val_MASK,
			0 << LCNPHY_rfoverride2val_slna_byp_ovr_val_SHIFT);

		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_ext_lna_gain_ovr_val_MASK,
			ext_lna << LCNPHY_rfoverride2val_ext_lna_gain_ovr_val_SHIFT);
	}

	mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
		LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		(!trsw) << LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_SHIFT);

}

static void
wlc_lcnphy_rx_gain_override_enable(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;

	mod_phy_reg(pi, LCNPHY_rfoverride2,
		LCNPHY_rfoverride2_rxgainctrl_ovr_MASK,
		ebit << LCNPHY_rfoverride2_rxgainctrl_ovr_SHIFT);

	mod_phy_reg(pi, LCNPHY_RFOverride0,
		LCNPHY_RFOverride0_trsw_rx_pu_ovr_MASK,
		ebit << LCNPHY_RFOverride0_trsw_rx_pu_ovr_SHIFT);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_gmode_rx_pu_ovr_MASK,
			ebit << LCNPHY_RFOverride0_gmode_rx_pu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_amode_rx_pu_ovr_MASK,
			ebit << LCNPHY_RFOverride0_amode_rx_pu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_gmode_ext_lna_gain_ovr_MASK,
			ebit << LCNPHY_rfoverride2_gmode_ext_lna_gain_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_amode_ext_lna_gain_ovr_MASK,
			ebit << LCNPHY_rfoverride2_amode_ext_lna_gain_ovr_SHIFT);
	}
	else {
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_slna_byp_ovr_MASK,
			ebit << LCNPHY_rfoverride2_slna_byp_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_slna_mode_ovr_MASK,
			ebit << LCNPHY_rfoverride2_slna_mode_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_ext_lna_gain_ovr_MASK,
			ebit << LCNPHY_rfoverride2_ext_lna_gain_ovr_SHIFT);
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_slna_gain_ctrl_ovr_MASK,
			ebit << LCNPHY_rfoverride2_slna_gain_ctrl_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFinputOverride,
			LCNPHY_RFinputOverride_wlslnagainctrl_ovr_MASK,
			ebit << LCNPHY_RFinputOverride_wlslnagainctrl_ovr_SHIFT);
	}
}

static void
wlc_lcnphy_rx_pu(phy_info_t *pi, bool bEnable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (!bEnable) {
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
			1 << LCNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK,
			0 << LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_SHIFT);

		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_rxgainctrl_ovr_MASK,
			1 << LCNPHY_rfoverride2_rxgainctrl_ovr_SHIFT);

		write_phy_reg(pi, LCNPHY_rxgainctrl0ovrval, 0xffff);

		mod_phy_reg(pi, LCNPHY_rxlnaandgainctrl1ovrval,
			LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_MASK,
			0xf << LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_SHIFT);
	} else {
		/* Force on the receive chain */
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
			1 << LCNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK,
			1 << LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_SHIFT);

		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_rxgainctrl_ovr_MASK,
			1 << LCNPHY_rfoverride2_rxgainctrl_ovr_SHIFT);

		wlc_lcnphy_set_rx_gain_by_distribution(pi, 0, 0, 4, 6, 4, 3, 3);
		wlc_lcnphy_rx_gain_override_enable(pi, TRUE);
	}
}

void
wlc_lcnphy_tx_pu(phy_info_t *pi, bool bEnable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (!bEnable) {
		/* Clear overrides */
		and_phy_reg(pi, LCNPHY_AfeCtrlOvr,
			~(uint16)(LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
			LCNPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK));

		mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
			LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
			1 << LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);

		and_phy_reg(pi, LCNPHY_RFOverride0,
			~(uint16)(LCNPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
			LCNPHY_RFOverride0_amode_tx_pu_ovr_MASK |
			LCNPHY_RFOverride0_internalrftxpu_ovr_MASK |
			LCNPHY_RFOverride0_trsw_rx_pu_ovr_MASK |
			LCNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
			LCNPHY_RFOverride0_ant_selp_ovr_MASK));

		and_phy_reg(pi, LCNPHY_RFOverrideVal0,
			~(uint16)(LCNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK |
			LCNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK |
			LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
			1 << LCNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);

			/* Set TR switch */
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK |
			LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
			LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK);

		and_phy_reg(pi, LCNPHY_rfoverride3,
			~(uint16)(LCNPHY_rfoverride3_stxpapu_ovr_MASK |
			LCNPHY_rfoverride3_stxpadpu2g_ovr_MASK |
			LCNPHY_rfoverride3_stxpapu2g_ovr_MASK));

		and_phy_reg(pi, LCNPHY_rfoverride3_val,
			~(uint16)(LCNPHY_rfoverride3_val_stxpapu_ovr_val_MASK |
			LCNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK |
			LCNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK));
	} else {
		/* Force on DAC */
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
			LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK,
			1 << LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
			LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
			0 << LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);

		mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
			LCNPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK,
			1 << LCNPHY_AfeCtrlOvr_dac_clk_disable_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
			LCNPHY_AfeCtrlOvrVal_dac_clk_disable_ovr_val_MASK,
			0 << LCNPHY_AfeCtrlOvrVal_dac_clk_disable_ovr_val_SHIFT);

		/* Force on the transmit chain */
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_internalrftxpu_ovr_MASK,
			1 << LCNPHY_RFOverride0_internalrftxpu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK,
			1 << LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_SHIFT);

		/* Force the TR switch to transmit */
		wlc_lcnphy_set_trsw_override(pi, TRUE, FALSE);

		/* Force antenna  0 */
		mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
			LCNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
			0 << LCNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFOverride0,
			LCNPHY_RFOverride0_ant_selp_ovr_MASK,
			1 << LCNPHY_RFOverride0_ant_selp_ovr_SHIFT);


		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			/* Force on the Gband-ePA */
			mod_phy_reg(pi, LCNPHY_RFOverride0,
				LCNPHY_RFOverride0_gmode_tx_pu_ovr_MASK,
				1 << LCNPHY_RFOverride0_gmode_tx_pu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
				LCNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK,
				1 << LCNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_SHIFT);
			/* force off the Aband-ePA */
			mod_phy_reg(pi, LCNPHY_RFOverride0,
				LCNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
				1 << LCNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
				LCNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK,
				0 << LCNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_SHIFT);
			/* PAD PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpadpu2g_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK,
				1 << LCNPHY_rfoverride3_val_stxpadpu2g_ovr_val_SHIFT);
			/* PGA PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpapu2g_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpapu2g_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK,
				1 << LCNPHY_rfoverride3_val_stxpapu2g_ovr_val_SHIFT);
			/* PA PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpapu_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpapu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
				1 << LCNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
		} else {
			/* Force off the Gband-ePA */
			mod_phy_reg(pi, LCNPHY_RFOverride0,
				LCNPHY_RFOverride0_gmode_tx_pu_ovr_MASK,
				1 << LCNPHY_RFOverride0_gmode_tx_pu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
				LCNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK,
				0 << LCNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_SHIFT);
			/* force off the Aband-ePA */
			mod_phy_reg(pi, LCNPHY_RFOverride0,
				LCNPHY_RFOverride0_amode_tx_pu_ovr_MASK,
				1 << LCNPHY_RFOverride0_amode_tx_pu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
				LCNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_MASK,
				1 << LCNPHY_RFOverrideVal0_amode_tx_pu_ovr_val_SHIFT);
			/* PAP PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpadpu2g_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpadpu2g_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK,
				0 << LCNPHY_rfoverride3_val_stxpadpu2g_ovr_val_SHIFT);
			/* PGA PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpapu2g_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpapu2g_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK,
				0 << LCNPHY_rfoverride3_val_stxpapu2g_ovr_val_SHIFT);
			/* PA PU */
			mod_phy_reg(pi, LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_stxpapu_ovr_MASK,
				1 << LCNPHY_rfoverride3_stxpapu_ovr_SHIFT);
			mod_phy_reg(pi, LCNPHY_rfoverride3_val,
				LCNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
				0 << LCNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
		}
	}
}

/*
 * Play samples from sample play buffer
 */
static void
wlc_lcnphy_run_samples(phy_info_t *pi,
	uint16 num_samps,
	uint16 num_loops,
	uint16 wait,
	bool iqcalmode)
{
	/* enable clk to txFrontEnd */
	or_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0x8080);

	mod_phy_reg(pi, LCNPHY_sampleDepthCount,
		LCNPHY_sampleDepthCount_DepthCount_MASK,
		(num_samps - 1) << LCNPHY_sampleDepthCount_DepthCount_SHIFT);
	if (num_loops != 0xffff)
		num_loops--;
	mod_phy_reg(pi, LCNPHY_sampleLoopCount,
		LCNPHY_sampleLoopCount_LoopCount_MASK,
		num_loops << LCNPHY_sampleLoopCount_LoopCount_SHIFT);

	mod_phy_reg(pi, LCNPHY_sampleInitWaitCount,
		LCNPHY_sampleInitWaitCount_InitWaitCount_MASK,
		wait << LCNPHY_sampleInitWaitCount_InitWaitCount_SHIFT);

	if (iqcalmode) {
		/* Enable calibration */
		and_phy_reg(pi,
			LCNPHY_iqloCalCmdGctl,
			(uint16)~LCNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
		or_phy_reg(pi, LCNPHY_iqloCalCmdGctl, LCNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
	} else {
		write_phy_reg(pi, LCNPHY_sampleCmd, 1);
		wlc_lcnphy_tx_pu(pi, 1);
	}
	/* enable filter override */
	or_radio_reg(pi, RADIO_2064_REG112, 0x6);
}

void
wlc_lcnphy_deaf_mode(phy_info_t *pi, bool mode)
{

	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	if (LCNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_gmode_ext_lna_gain_ovr_MASK,
			(mode) << LCNPHY_rfoverride2_gmode_ext_lna_gain_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_MASK,
			0 << LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_SHIFT);
	}
	else
	{
		mod_phy_reg(pi, LCNPHY_rfoverride2,
			LCNPHY_rfoverride2_ext_lna_gain_ovr_MASK,
			(mode) << LCNPHY_rfoverride2_ext_lna_gain_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_ext_lna_gain_ovr_val_MASK,
			0 << LCNPHY_rfoverride2val_ext_lna_gain_ovr_val_SHIFT);
	}

	if (phybw40 == 0) {
		mod_phy_reg((pi), LCNPHY_crsgainCtrl,
			LCNPHY_crsgainCtrl_DSSSDetectionEnable_MASK |
			LCNPHY_crsgainCtrl_OFDMDetectionEnable_MASK,
			((CHSPEC_IS2G(pi->radio_chanspec)) ? (!mode) : 0) <<
			LCNPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT |
			(!mode) << LCNPHY_crsgainCtrl_OFDMDetectionEnable_SHIFT);
		mod_phy_reg(pi, LCNPHY_crsgainCtrl,
			LCNPHY_crsgainCtrl_crseddisable_MASK,
			(mode) << LCNPHY_crsgainCtrl_crseddisable_SHIFT);
	}
}

/*
* Given a test tone frequency, continuously play the samples. Ensure that num_periods
* specifies the number of periods of the underlying analog signal over which the
* digital samples are periodic
*/
/* equivalent to lcnphy_play_tone */
void
wlc_lcnphy_start_tx_tone(phy_info_t *pi, int32 f_kHz, uint16 max_val, bool iqcalmode)
{
	uint8 phy_bw;
	uint16 num_samps, t, k;
	uint32 bw;
	fixed theta = 0, rot = 0;
	cint32 tone_samp;
	uint32 data_buf[64];
	uint16 i_samp, q_samp;
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Save active tone frequency */
	pi->phy_tx_tone_freq = f_kHz;

	/* Turn off all the crs signals to the MAC */
	wlc_lcnphy_deaf_mode(pi, TRUE);

	phy_bw = 40;
	if (pi_lcn->lcnphy_spurmod) {
			write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0x2);
			write_phy_reg(pi, LCNPHY_spur_canceller1, 0x0);
			write_phy_reg(pi, LCNPHY_spur_canceller2, 0x0);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, FALSE);
	}
	/* allocate buffer */
	if (f_kHz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k;
			num_samps = bw / ABS(f_kHz);
			ASSERT(num_samps <= ARRAYSIZE(data_buf));
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
	/* in LCNPHY, we need to bring SPB out of standby before using it */
	mod_phy_reg(pi, LCNPHY_sslpnCtrl3,
		LCNPHY_sslpnCtrl3_sram_stby_MASK,
		0 << LCNPHY_sslpnCtrl3_sram_stby_SHIFT);

	mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
		1 << LCNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);
	/* lcnphy_load_sample_table */
	tab.tbl_ptr = data_buf;
	tab.tbl_len = num_samps;
	tab.tbl_id = LCNPHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	wlc_lcnphy_write_table(pi, &tab);
	/* play samples from the sample play buffer */
	wlc_lcnphy_run_samples(pi, num_samps, 0xffff, 0, iqcalmode);
}

void
wlc_lcnphy_stop_tx_tone(phy_info_t *pi)
{
	int16 playback_status;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi->phy_tx_tone_freq = 0;
	if (pi_lcn->lcnphy_spurmod) {
			write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0x7);
			write_phy_reg(pi, LCNPHY_spur_canceller1, 0x2017);
			write_phy_reg(pi, LCNPHY_spur_canceller2, 0x27c5);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, TRUE);
	}
	/* Stop sample buffer playback */
	playback_status = read_phy_reg(pi, LCNPHY_sampleStatus);
	if (playback_status & LCNPHY_sampleStatus_NormalPlay_MASK) {
		wlc_lcnphy_tx_pu(pi, 0);
		mod_phy_reg(pi, LCNPHY_sampleCmd,
			LCNPHY_sampleCmd_stop_MASK,
			1 << LCNPHY_sampleCmd_stop_SHIFT);
	} else if (playback_status & LCNPHY_sampleStatus_iqlocalPlay_MASK)
		mod_phy_reg(pi, LCNPHY_iqloCalCmdGctl,
			LCNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK,
			0 << LCNPHY_iqloCalCmdGctl_iqlo_cal_en_SHIFT);

	/* put back SPB into standby */
	mod_phy_reg(pi, LCNPHY_sslpnCtrl3,
		LCNPHY_sslpnCtrl3_sram_stby_MASK,
		1 << LCNPHY_sslpnCtrl3_sram_stby_SHIFT);
	/* disable clokc to spb */
	mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_MASK,
		0 << LCNPHY_sslpnCalibClkEnCtrl_samplePlayClkEn_SHIFT);
	/* disable clock to txFrontEnd */
	mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_MASK,
		0 << LCNPHY_sslpnCalibClkEnCtrl_forceaphytxFeclkOn_SHIFT);
	/* disable filter override */
	and_radio_reg(pi, RADIO_2064_REG112, 0xFFF9);
	/* Restore all the crs signals to the MAC */
	wlc_lcnphy_deaf_mode(pi, FALSE);
}

void
wlc_lcnphy_set_tx_tone_and_gain_idx(phy_info_t *pi)
{
	int8 curr_pwr_idx_val;

	if (LCNPHY_TX_PWR_CTRL_OFF != wlc_lcnphy_get_tx_pwr_ctrl(pi)) {
		curr_pwr_idx_val = wlc_lcnphy_get_current_tx_pwr_idx(pi);
		wlc_lcnphy_set_tx_pwr_by_index(pi, (int)curr_pwr_idx_val);
	} else {
		/* if tx power ctrl was disabled, then gain index already set */
	}

	wlc_lcnphy_start_tx_tone(pi, pi->phy_tx_tone_freq, 120, 1); /* play tone */
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xffff);
}

static void
wlc_lcnphy_clear_trsw_override(phy_info_t *pi)
{
	/* Clear overrides */
	and_phy_reg(pi, LCNPHY_RFOverride0,
		(uint16)~(LCNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		LCNPHY_RFOverride0_trsw_rx_pu_ovr_MASK));
}

void
wlc_lcnphy_get_tx_iqcc(phy_info_t *pi, uint16 *a, uint16 *b)
{
	uint16 iqcc[2];
	phytbl_info_t tab;

	tab.tbl_ptr = iqcc; /* ptr to buf */
	tab.tbl_len = 2;        /* # values   */
	tab.tbl_id = 0;         /* iqloCaltbl      */
	tab.tbl_offset = 80; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_lcnphy_read_table(pi, &tab);

	*a = iqcc[0];
	*b = iqcc[1];
}

uint16
wlc_lcnphy_get_tx_locc(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint16 didq;

	/* Update iqloCaltbl */
	tab.tbl_id = 0;			/* iqloCaltbl		*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_lcnphy_read_table(pi, &tab);

	return didq;
}
/* Run iqlo cal and populate iqlo portion of tx power control table */
static void
wlc_lcnphy_txpwrtbl_iqlo_cal(phy_info_t *pi)
{

	lcnphy_txgains_t old_gains;
	uint8 save_bb_mult;
	uint16 a, b, didq, save_pa_gain = 0;
	uint idx, SAVE_txpwrindex = 0xFF;
	uint32 val;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phytbl_info_t tab;
	uint8 ei0, eq0, fi0, fq0;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	wlc_lcnphy_get_tx_gain(pi, &old_gains);
	save_pa_gain = wlc_lcnphy_get_pa_gain(pi);
	/* Store state */
	save_bb_mult = wlc_lcnphy_get_bbmult(pi);

	if (SAVE_txpwrctrl == LCNPHY_TX_PWR_CTRL_OFF)
		SAVE_txpwrindex = wlc_lcnphy_get_current_tx_pwr_idx(pi);

	if (((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && LCNREV_IS(pi->pubpi.phy_rev, 1)) ||
		pi_lcn->lcnphy_hw_iqcal_en ||
		LCNREV_GE(pi->pubpi.phy_rev, 2)) {

		wlc_lcnphy_set_tx_pwr_by_index(pi, 30);

		wlc_lcnphy_tx_iqlo_cal(pi, NULL, (pi_lcn->lcnphy_recal ?
		LCNPHY_CAL_RECAL : LCNPHY_CAL_FULL), FALSE);
	} else {
		wlc_lcnphy_set_tx_pwr_by_index(pi, 12);
		/* only 4313a0 or lcn phy rev 0 requires sw tx iqlo cal */
		wlc_lcnphy_tx_iqlo_soft_cal_full(pi);
	}

	wlc_lcnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);
	if ((ABS((int8)fi0) == 15) && (ABS((int8)fq0) == 15)) {
		PHY_ERROR(("wl%d: %s: tx iqlo cal failed, retrying...\n",
			pi->sh->unit, __FUNCTION__));

		/* Re-run cal */
		if (((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) &&
			LCNREV_IS(pi->pubpi.phy_rev, 1)) || pi_lcn->lcnphy_hw_iqcal_en ||
			LCNREV_GE(pi->pubpi.phy_rev, 2)) {
			/* 4313P: to be relooked after merge */
			wlc_lcnphy_set_tx_pwr_by_index(pi, 46);
			wlc_lcnphy_tx_iqlo_cal(pi, NULL, LCNPHY_CAL_FULL, FALSE);
		} else {
			/* only 4313a0 or lcn phy rev 0 requires sw tx iqlo cal */
			wlc_lcnphy_set_tx_pwr_by_index(pi, 28);
			wlc_lcnphy_tx_iqlo_soft_cal_full(pi);
		}

	}

	/* Get calibration results */
	wlc_lcnphy_get_tx_iqcc(pi, &a, &b);
	PHY_INFORM(("TXIQCal: %d %d\n", a, b));

	didq = wlc_lcnphy_get_tx_locc(pi);

	/* Populate tx power control table with coeffs */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = &val; /* ptr to buf */

	/* Per rate power offset */
	tab.tbl_len = 1; /* # values   */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_RATE_OFFSET;

	for (idx = 0; idx < 128; idx++) {
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + idx;
		/* iq */
		wlc_lcnphy_read_table(pi, &tab);
		val = (val & 0xfff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_lcnphy_write_table(pi, &tab);

		/* loft */
		val = didq;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_LO_OFFSET + idx;
		wlc_lcnphy_write_table(pi, &tab);
	}

	/* Save Cal Results */
	pi_lcn->lcnphy_cal_results.txiqlocal_a = a;
	pi_lcn->lcnphy_cal_results.txiqlocal_b = b;
	pi_lcn->lcnphy_cal_results.txiqlocal_didq = didq;
	pi_lcn->lcnphy_cal_results.txiqlocal_ei0 = ei0;
	pi_lcn->lcnphy_cal_results.txiqlocal_eq0 = eq0;
	pi_lcn->lcnphy_cal_results.txiqlocal_fi0 = fi0;
	pi_lcn->lcnphy_cal_results.txiqlocal_fq0 = fq0;

	/* Restore state */
	wlc_lcnphy_set_bbmult(pi, save_bb_mult);
	wlc_lcnphy_set_pa_gain(pi, save_pa_gain);
	wlc_lcnphy_set_tx_gain(pi, &old_gains);

	if (SAVE_txpwrctrl != LCNPHY_TX_PWR_CTRL_OFF)
		wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);
	else
		wlc_lcnphy_set_tx_pwr_by_index(pi, SAVE_txpwrindex);
}

static void
wlc_lcnphy_papd_cal_setup_cw(
	phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* Tune the hardware delay */
	write_phy_reg(pi, LCNPHY_papd_spb2papdin_dly, 35);
	/* Set samples/cycle/4 for q delay */
	write_phy_reg(pi, LCNPHY_papd_variable_delay, 3);
	/* Set LUT begin gain, step gain, and size (Reset values, remove if possible) */
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_begin, 6000);
	/* peak_curr_mode dependant */
	write_phy_reg(pi, LCNPHY_papd_rx_gain_comp_dbm, 0);
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_step, 0x444);
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_end, 0x3f);
	/* set papd constants */
	write_phy_reg(pi, LCNPHY_papd_dbm_offset, 0x681);
	/* Dc estimation samples */
	write_phy_reg(pi, LCNPHY_papd_ofdm_dc_est, 0x49);
	/* lcnphy - newly added registers */
	write_phy_reg(pi, LCNPHY_papd_cw_corr_norm, PAPD_CORR_NORM);
	write_phy_reg(pi, LCNPHY_papd_blanking_control,
		(PAPD_STOP_AFTER_LAST_UPDATE << 12 | PAPD_BLANKING_PROFILE << 9 |
		PAPD_BLANKING_THRESHOLD));
	write_phy_reg(pi, LCNPHY_papd_num_skip_count, 0x27);
	write_phy_reg(pi, LCNPHY_papd_num_samples_count, 255);
	write_phy_reg(pi, LCNPHY_papd_sync_count, 319);

	/* Adjust the number of samples to be processed depending on the corr_norm */
	write_phy_reg(pi, LCNPHY_papd_num_samples_count, (((255+1)/(1<<PAPD_CORR_NORM))-1));
	write_phy_reg(pi, LCNPHY_papd_sync_count, (((319+1)/(1<<PAPD_CORR_NORM))-1));

	write_phy_reg(pi, LCNPHY_papd_switch_lut, PAPD2LUT);
	write_phy_reg(pi, LCNPHY_papd_pa_off_control_1, 0);
	write_phy_reg(pi, LCNPHY_papd_only_loop_gain, 0);
	write_phy_reg(pi, LCNPHY_papd_pa_off_control_2, 0);

	write_phy_reg(pi, LCNPHY_smoothenLut_max_thr, 0x7ff);
	write_phy_reg(pi, LCNPHY_papd_dcest_i_ovr, 0x0000);
	write_phy_reg(pi, LCNPHY_papd_dcest_q_ovr, 0x0000);
}

static void
wlc_lcnphy_papd_cal_core(
	phy_info_t *pi,
	lcnphy_papd_cal_type_t calType,
	bool rxGnCtrl,
	bool txGnCtrl,
	bool samplecapture,
	bool papd_dbg_mode,
	uint16 num_symbols,
	bool init_papd_lut,
	uint16 papd_bbmult_init,
	uint16 papd_bbmult_step,
	bool papd_lpgn_ovr,
	uint16 LPGN_I,
	uint16 LPGN_Q)
{
	phytbl_info_t tab;
	uint32 papdcompdeltatbl_init_val;
	uint32 j;
	uint32 temp_buffer[128];
	bool pdBypass, gain0mode;
	uint32 papd_buf[] = {0x7fc00, 0x5a569, 0x1ff, 0xa5d69, 0x80400, 0xa5e97, 0x201, 0x5a697};

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pdBypass = 0; /* PD output(0) or input(1)based training */
	gain0mode = 1; /* 0 for using both I and Q , 1 use only I */

	/* Reset PAPD Hw to reset register values */
	or_phy_reg(pi, LCNPHY_papd_control2, 0x1);
	and_phy_reg(pi, LCNPHY_papd_control2, ~0x1);
	MOD_PHY_REG(pi, LCNPHY, papd_control2, papd_loop_gain_cw_ovr, papd_lpgn_ovr);

	/* set PAPD registers to configure the PAPD calibration */
	if (init_papd_lut != 0) {
		/* Load papd comp delta table */
		papdcompdeltatbl_init_val = 0x80000;
		tab.tbl_ptr = temp_buffer; /* ptr to buf */
		tab.tbl_len = 64;        /* # values   */
		tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;         /* papdcompdeltatbl */
		tab.tbl_width = 32;     /* 32 bit wide */
		if (PAPD2LUT == 1)
			tab.tbl_offset = 64; /* tbl offset */
		else
			tab.tbl_offset = 0; /* tbl offset */
		bzero(temp_buffer, sizeof(temp_buffer));
		for (j = tab.tbl_offset; j < tab.tbl_offset + 64; j += 1) {
			temp_buffer[j] = 0x80000;
			wlc_lcnphy_write_table(pi, &tab);
		}
	} else {
		/* load papd lut */
		if (PAPD2LUT == 0)
			tab.tbl_offset = 0;
		else
			tab.tbl_offset = 64;
		tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
		tab.tbl_len = 64;        /* # values   */
		tab.tbl_width = 32;     /* 32 bit wide */
		tab.tbl_ptr = temp_buffer; /* ptr to buf */
	}

	/* set PAPD registers to configure PAPD calibration */
	wlc_lcnphy_papd_cal_setup_cw(pi);

	/* num_symbols is computed based on corr_norm */
	num_symbols = num_symbols * PAPD_BLANKING_PROFILE + 1;

	/* override control params */
	write_phy_reg(pi, LCNPHY_papd_loop_gain_ovr_cw_i, LPGN_I);
	write_phy_reg(pi, LCNPHY_papd_loop_gain_ovr_cw_q, LPGN_Q);

	/* papd update */
	write_phy_reg(pi, LCNPHY_papd_track_num_symbols_count, num_symbols);

	/* spb parameters */
	write_phy_reg(pi, LCNPHY_papd_spb_num_vld_symbols_n_dly, 0x60);
	write_phy_reg(pi, LCNPHY_sampleDepthCount, 7);
	write_phy_reg(pi, LCNPHY_sampleLoopCount, (num_symbols+1)*20-1);
	write_phy_reg(pi, LCNPHY_papd_spb_rd_address, 0);

	/* load the spb */
	tab.tbl_len = 8;
	tab.tbl_id = LCNPHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	tab.tbl_ptr = &papd_buf;
	wlc_lcnphy_write_table(pi, &tab);

	/* BBMULT parameters */
	write_phy_reg(pi, LCNPHY_papd_bbmult_init, papd_bbmult_init);
	write_phy_reg(pi, LCNPHY_papd_bbmult_step, papd_bbmult_step);
	write_phy_reg(pi, LCNPHY_papd_bbmult_num_symbols, 1-1);

	/* Run PAPD HW Cal */
	if (pdBypass != 0) {
		if (gain0mode != 1) {
			write_phy_reg(pi, LCNPHY_papd_rx_sm_iqmm_gain_comp, 0x100);
			write_phy_reg(pi, LCNPHY_papd_control, 0x8021);
		} else {
			write_phy_reg(pi, LCNPHY_papd_rx_sm_iqmm_gain_comp, 0x00);
			write_phy_reg(pi, LCNPHY_papd_control, 0x8821);
		}
	} else {
		if (gain0mode != 1) {
			write_phy_reg(pi, LCNPHY_papd_rx_sm_iqmm_gain_comp, 0x100);
			write_phy_reg(pi, LCNPHY_papd_control, 0xb0a1);
		} else {
			write_phy_reg(pi, LCNPHY_papd_rx_sm_iqmm_gain_comp, 0x00);
			write_phy_reg(pi, LCNPHY_papd_control, 0xb8a1);
		}
	}
	/* Wait for completion, around 1s */
	SPINWAIT(read_phy_reg(pi, LCNPHY_papd_control) & LCNPHY_papd_control_papd_cal_run_MASK,
		1 * 1000 * 1000);
}

int16
wlc_lcnphy_tempsense_new(phy_info_t *pi, bool mode)
{
	uint16 tempsenseval1, tempsenseval2;
	int16 avg = 0;
	bool suspend = 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return -1;

	/* mode 1 will set-up and do the measurement */
	if (mode == 1) {
		suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	}
	tempsenseval1 = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusTemp) & 0x1FF;
	tempsenseval2 = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusTemp1) & 0x1FF;

	if (tempsenseval1 > 255)
		avg = (int16)(tempsenseval1 - 512);
	else
		avg = (int16)tempsenseval1;

	if (tempsenseval2 > 255)
		avg += (int16)(tempsenseval2 - 512);
	else
		avg += (int16)tempsenseval2;

	avg /= 2;

	if (mode == 1) {
		/* reset CCA */
		MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 1);
		OSL_DELAY(100);
		MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 0);
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return avg;
}

uint16
wlc_lcnphy_tempsense(phy_info_t *pi, bool mode)
{
	uint16 tempsenseval1, tempsenseval2;
	int32 avg = 0;
	bool suspend = 0;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return -1;

	/* mode 1 will set-up and do the measurement */
	if (mode == 1) {
		suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, TEMPSENSE);
	}
	tempsenseval1 = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusTemp) & 0x1FF;
	tempsenseval2 = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusTemp1) & 0x1FF;

	if (tempsenseval1 > 255)
		avg = (int)(tempsenseval1 - 512);
	else
		avg = (int)tempsenseval1;

	if (pi_lcn->lcnphy_tempsense_option == 1 ||
		(pi->hwpwrctrl_capable && CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		if (tempsenseval2 > 255)
			avg = (int)(avg - tempsenseval2 + 512);
		else
			avg = (int)(avg - tempsenseval2);
	} else {
		if (tempsenseval2 > 255)
			avg = (int)(avg + tempsenseval2 - 512);
		else
			avg = (int)(avg + tempsenseval2);
		avg = avg/2;
	}
	if (avg < 0)
		avg = avg + 512;

	if (pi_lcn->lcnphy_tempsense_option == 2)
		avg = tempsenseval1;

	if (mode)
		wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_txpwrctrl);

	if (mode == 1) {
		/* reset CCA */
		MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 1);
		OSL_DELAY(100);
		MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 0);
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return (uint16)avg;
}

int8
wlc_lcnphy_tempsense_degree(phy_info_t *pi, bool mode)
{
	int32 degree = wlc_lcnphy_tempsense_new(pi, mode);
	degree = ((degree << 10) + LCN_TEMPSENSE_OFFSET + (LCN_TEMPSENSE_DEN >> 1))
			/ LCN_TEMPSENSE_DEN;
	return (int8)degree;
}

/* return value is in volt Q4.4 */
int8
wlc_lcnphy_vbatsense(phy_info_t *pi, bool mode)
{
	uint16 vbatsenseval;
	int32 avg = 0;
	bool suspend = 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return -1;
	/* mode 1 will set-up and do the measurement */
	if (mode == 1 && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_lcnphy_vbat_temp_sense_setup(pi, VBATSENSE);
	}

	if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID)
		write_radio_reg(pi, RADIO_2064_REG07C, 1);

	vbatsenseval = read_phy_reg(pi, LCNPHY_TxPwrCtrlStatusVbat) & 0x1FF;

	if (vbatsenseval > 255)
		avg =  (int32)(vbatsenseval - 512);
	else
		avg = (int32)vbatsenseval;

	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)
		avg = (avg * LCN_VBAT_SCALE_NOM + (LCN_VBAT_SCALE_DEN >> 1))
			/ LCN_VBAT_SCALE_DEN;
	else
		avg = ((avg << 16) + LCN_VBAT_OFFSET_433X + (LCN_VBAT_SLOPE_433X >> 5))
			/ (LCN_VBAT_SLOPE_433X >> 4);

	if (mode == 1 && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
	return (int8)avg;
}

static uint32
wlc_lcnphy_papd_rxGnCtrl(
	phy_info_t *pi,
	lcnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	uint8 CurTxGain)
{
	/* Square of Loop Gain (inv) target for CW (reach as close to tgt, but be more than it) */
	/* dB Loop gain (inv) target for OFDM (reach as close to tgt,but be more than it) */
	int32 rxGnInit = 5;
	uint8  bsStep = 3; /* Binary search initial step size */
	uint8  bsDepth = 4; /* Binary search depth */
	int32  cwLpGn2_min = 8192, cwLpGn2_max = 16384;
	uint8  bsCnt;
	int16  lgI, lgQ;
	int32  cwLpGn2;
	uint8  num_symbols4lpgn;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* frcRxGnCtrl conditional missing */
	for (bsCnt = 0; bsCnt < bsDepth; bsCnt++) {
		/* Running PAPD for Tx gain index:CurTxGain */
		/* Rx gain index : tia gain : rxGnInit */
		PHY_PAPD(("Running PAPD for Tx Gain Idx : %d ,Rx Gain Index %d\n",
			CurTxGain, rxGnInit));
		if (rxGnInit > 15)
			rxGnInit = 15; /* out-of-range correction */
		wlc_lcnphy_set_rx_gain_by_distribution(pi, 1, 0, 0, 0, (uint16)rxGnInit, 0, 0);

		num_symbols4lpgn = 219;
		wlc_lcnphy_papd_cal_core(pi, 0,
		                         TRUE,
		                         0,
		                         0,
		                         0,
		                         num_symbols4lpgn,
		                         1,
		                         1400,
		                         16640,
		                         0,
		                         128,
		                         0);
		if (cal_type == LCNPHY_PAPD_CAL_CW) {
			lgI = ((int16) read_phy_reg(pi, LCNPHY_papd_loop_gain_cw_i)) << 6;
			lgI = lgI >> 6;
			lgQ = ((int16) read_phy_reg(pi, LCNPHY_papd_loop_gain_cw_q)) << 6;
			lgQ = lgQ >> 6;

			cwLpGn2 = (lgI * lgI) + (lgQ * lgQ);

			PHY_PAPD(("LCNPHY_papd_loop_gain_cw_i %x LCNPHY_papd_loop_gain_cw_q %x\n",
				read_phy_reg(pi, LCNPHY_papd_loop_gain_cw_i),
				read_phy_reg(pi, LCNPHY_papd_loop_gain_cw_q)));
			PHY_PAPD(("loopgain %d lgI %d lgQ %d\n", cwLpGn2, lgI, lgQ));

			if (cwLpGn2 < cwLpGn2_min) {
				rxGnInit = rxGnInit - bsStep;
			} else if (cwLpGn2 >= cwLpGn2_max) {
				rxGnInit = rxGnInit + bsStep;
			} else {
				break;
			}
		}
		bsStep = bsStep - 1;
	}
	if (rxGnInit > 9)
		rxGnInit = 9;
	if (rxGnInit < 0)
		rxGnInit = 0; /* out-of-range correction */

	pi_lcn->lcnphy_papdRxGnIdx = rxGnInit;
	PHY_PAPD(("wl%d: %s Settled to rxGnInit: %d\n",
		pi->sh->unit, __FUNCTION__, rxGnInit));
	return rxGnInit;
}

static void
wlc_lcnphy_afe_clk_init(phy_info_t *pi, uint8 mode)
{
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);
	/* IQ Swap at adc */
	MOD_PHY_REG(pi, LCNPHY, rxfe, swap_rxfiltout_iq, 1);
	/* Setting adc in 2x mode */
	if (((mode == AFE_CLK_INIT_MODE_PAPD) && (phybw40 == 0)) ||
		(mode == AFE_CLK_INIT_MODE_TXRX2X))
		write_phy_reg(pi, LCNPHY_adc_2x, 0x7);

	wlc_lcnphy_toggle_afe_pwdn(pi);
}

static void
wlc_lcnphy_GetpapdMaxMinIdxupdt(phy_info_t *pi,
	int8 *maxUpdtIdx,
	int8 *minUpdtIdx)
{
	uint16 papd_lut_index_updt_63_48, papd_lut_index_updt_47_32;
	uint16 papd_lut_index_updt_31_16, papd_lut_index_updt_15_0;
	int8 MaxIdx, MinIdx;
	uint8 MaxIdxUpdated, MinIdxUpdated;
	uint8 i;

	papd_lut_index_updt_63_48 = read_phy_reg(pi, LCNPHY_papd_lut_index_updated_63_48);
	papd_lut_index_updt_47_32 = read_phy_reg(pi, LCNPHY_papd_lut_index_updated_47_32);
	papd_lut_index_updt_31_16 = read_phy_reg(pi, LCNPHY_papd_lut_index_updated_31_16);
	papd_lut_index_updt_15_0  = read_phy_reg(pi, LCNPHY_papd_lut_index_updated_15_0);

	PHY_PAPD(("63_48  47_32  31_16  15_0\n"));
	PHY_PAPD((" %4x   %4x   %4x   %4x\n",
		papd_lut_index_updt_63_48, papd_lut_index_updt_47_32,
		papd_lut_index_updt_31_16, papd_lut_index_updt_15_0));

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
wlc_lcnphy_save_papd_calibration_results(phy_info_t *pi)
{
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Save epsilon table */
	tab.tbl_len = PHY_PAPD_EPS_TBL_SIZE_LCNPHY;
	tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	tab.tbl_ptr = pi_lcn->lcnphy_cal_results.papd_eps_tbl;
	wlc_lcnphy_read_table(pi, &tab);

	pi_lcn->lcnphy_cal_results.analog_gain_ref =
		read_phy_reg(pi, LCNPHY_papd_tx_analog_gain_ref);
	pi_lcn->lcnphy_cal_results.lut_begin =
		read_phy_reg(pi, LCNPHY_papd_track_pa_lut_begin);
	pi_lcn->lcnphy_cal_results.lut_step  =
		read_phy_reg(pi, LCNPHY_papd_track_pa_lut_step);
	pi_lcn->lcnphy_cal_results.lut_end   =
		read_phy_reg(pi, LCNPHY_papd_track_pa_lut_end);
	pi_lcn->lcnphy_cal_results.rxcompdbm =
		read_phy_reg(pi, LCNPHY_papd_rx_gain_comp_dbm);
	pi_lcn->lcnphy_cal_results.papdctrl  =
		read_phy_reg(pi, LCNPHY_papd_control);
	pi_lcn->lcnphy_cal_results.sslpnCalibClkEnCtrl =
		read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);

}

static void
wlc_lcnphy_restore_papd_calibration_results(phy_info_t *pi)
{
	phytbl_info_t tab;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* write eps table */
	tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_width = 32;
	tab.tbl_ptr = pi_lcn->lcnphy_cal_results.papd_eps_tbl;
	tab.tbl_len = PHY_PAPD_EPS_TBL_SIZE_LCNPHY;
	tab.tbl_offset = 0;
	wlc_lcnphy_write_table(pi, &tab);

	write_phy_reg(pi, LCNPHY_papd_tx_analog_gain_ref,
		pi_lcn->lcnphy_cal_results.analog_gain_ref);
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_begin,
		pi_lcn->lcnphy_cal_results.lut_begin);
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_step,
		pi_lcn->lcnphy_cal_results.lut_step);
	write_phy_reg(pi, LCNPHY_papd_track_pa_lut_end,
		pi_lcn->lcnphy_cal_results.lut_end);
	write_phy_reg(pi, LCNPHY_papd_rx_gain_comp_dbm,
		pi_lcn->lcnphy_cal_results.rxcompdbm);
	write_phy_reg(pi, LCNPHY_papd_control,
		pi_lcn->lcnphy_cal_results.papdctrl);
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		pi_lcn->lcnphy_cal_results.sslpnCalibClkEnCtrl);
}


static void
wlc_lcnphy_papd_cal(
	phy_info_t *pi,
	lcnphy_papd_cal_type_t cal_type,
	lcnphy_txcalgains_t *txgains,
	bool frcRxGnCtrl,
	bool txGnCtrl,
	bool samplecapture,
	bool papd_dbg_mode,
	uint8 num_symbols,
	uint8 init_papd_lut)
{
	uint8 bb_mult_old;
	uint16 AphyControl_old, lcnCtrl3_old;
	uint16 Core1TxControl_old;
	uint16 save_amuxSelPortOverride, save_amuxSelPortOverrideVal;
	uint32 rxGnIdx;
	phytbl_info_t tab;
	uint16 lcnCalibClkEnCtrl_old;
	uint32 tmpVar;
	uint32 refTxAnGn;
	uint16 lpfbwlut0, lpfbwlut1;
	uint8 CurTxGain;
	lcnphy_txgains_t old_gains;
	uint16 lpf_ofdm_tx_bw;
	uint8 papd_peak_curr_mode = 0;
	int8 maxUpdtIdx, minUpdtIdx, j;

	bool suspend;
	uint8 save_REG116;
	uint8 save_REG12D;
	uint8 save_REG12C;
	uint8 save_REG06A;
	uint8 save_REG098;
	uint8 save_REG097;
	uint8 save_REG12F;
	uint8 save_REG00B;
	uint8 save_REG113;
	uint8 save_REG01D;
	uint8 save_REG114;
	uint8 save_REG02E;
	uint8 save_REG12A;
	uint8 save_REG009;
	uint8 save_REG11F;
	uint8 save_REG007;
	uint8 save_REG0FF;
	uint8 save_REG005;

	uint16 save_filtcoeffs[LCNPHY_NUM_DIG_FILT_COEFFS+1];

	bool papd_lastidx_search_mode;
	uint32 min_papd_lut_val, max_papd_lut_val;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	papd_lastidx_search_mode = 1;
	suspend = 0;
	maxUpdtIdx = 1;
	minUpdtIdx = 1;
	ASSERT((cal_type == LCNPHY_PAPD_CAL_CW));

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	PHY_PAPD(("Running papd cal, channel: %d cal type: %d\n",
		CHSPEC_CHANNEL(pi->radio_chanspec),
		cal_type));

	bb_mult_old = wlc_lcnphy_get_bbmult(pi); /* PAPD cal can modify this value */

	/* AMUX SEL logic */
	save_amuxSelPortOverride =
		READ_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverride);
	save_amuxSelPortOverrideVal =
		READ_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0, amuxSelPortOverrideVal);

	if (pi_lcn->lcnphy_papd_4336_mode) {
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_MASK,
			1 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_SHIFT);
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_MASK,
			2 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_SHIFT);
	}
	else {
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_MASK,
			1 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_SHIFT);
		mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
			LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_MASK,
			0 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_SHIFT);
	}


	/* Disable filters, turn off PAPD */
	mod_phy_reg(pi, LCNPHY_papd_control,
		LCNPHY_papd_control_papdCompEn_MASK,
		0 << LCNPHY_papd_control_papdCompEn_SHIFT);

	mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK,
		0 << LCNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_cck_papden_MASK,
		0 << LCNPHY_txfefilterconfig_cck_papden_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_ofdm_papden_MASK,
		0 << LCNPHY_txfefilterconfig_ofdm_papden_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_ht_papden_MASK,
		0 << LCNPHY_txfefilterconfig_ht_papden_SHIFT);

	/* Save Digital Filter and set to OFDM Coeffs */
	wlc_lcnphy_save_restore_dig_filt_state(pi, TRUE, save_filtcoeffs);
	wlc_lcnphy_load_tx_iir_filter(pi, TRUE, 0);


	if (!txGnCtrl) {
		/* Disable CRS/Rx, MAC/Tx and BT */
		wlc_lcnphy_deaf_mode(pi, TRUE);
		/* suspend the mac if it is not already suspended */
		suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend) {
			/* Set non-zero duration for CTS-to-self */
			wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		}
	}

	/* Set Rx gain override */
	wlc_lcnphy_rx_gain_override_enable(pi, TRUE);

	wlc_lcnphy_tx_pu(pi, TRUE);
	wlc_lcnphy_rx_pu(pi, TRUE);

	/* Save lcnphy filt bw */
	lpfbwlut0 = read_phy_reg(pi, LCNPHY_lpfbwlutreg0);
	lpfbwlut1 = read_phy_reg(pi, LCNPHY_lpfbwlutreg1);

	/* Widen tx filter */
	/* set filter bandwidth in lpphy_rev0_rf_init, so it's common b/n cal and packets tx */
	/* tones use cck setting, we want to cal with ofdm filter setting */
	lpf_ofdm_tx_bw = READ_PHY_REG(pi, LCNPHY, lpfbwlutreg1, lpf_ofdm_tx_bw);
	mod_phy_reg(pi, LCNPHY_lpfbwlutreg1,
		LCNPHY_lpfbwlutreg1_lpf_cck_tx_bw_MASK,
		lpf_ofdm_tx_bw << LCNPHY_lpfbwlutreg1_lpf_cck_tx_bw_SHIFT);

	CurTxGain = pi_lcn->lcnphy_current_index;
	wlc_lcnphy_get_tx_gain(pi, &old_gains);

	/* Set tx gain */
	if (txgains) {
		if (txgains->useindex) {
			wlc_lcnphy_set_tx_pwr_by_index(pi, txgains->index);
			CurTxGain = txgains->index;
			PHY_PAPD(("txgainIndex = %d\n", CurTxGain));
			PHY_PAPD(("papd_analog_gain_ovr_val = %d\n",
				READ_PHY_REG(pi, LCNPHY, papd_analog_gain_ovr_val,
				papd_analog_gain_ovr_val)));
		} else {
			wlc_lcnphy_set_tx_gain(pi, &txgains->gains);
		}
	}

	AphyControl_old = read_phy_reg(pi, LCNPHY_AphyControlAddr);
	Core1TxControl_old = read_phy_reg(pi, LCNPHY_Core1TxControl);
	lcnCtrl3_old = read_phy_reg(pi, LCNPHY_sslpnCtrl3);
	lcnCalibClkEnCtrl_old = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);

	wlc_lcnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_PAPD);

	/* loft comp , iqmm comp enable */
	or_phy_reg(pi, LCNPHY_Core1TxControl, 0x0015);

	/* we need to bring SPB out of standby before using it */
	MOD_PHY_REG(pi, LCNPHY, sslpnCtrl3, sram_stby, 0);

	/* enable clk to SPB, PAPD blks and cal */
	or_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0x8f);

	/* Set PAPD reference analog gain */
	tab.tbl_ptr = &refTxAnGn; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_PWR_OFFSET + txgains->index; /* tbl offset */
	tab.tbl_width = 32;     /* 32 bit wide */
	wlc_lcnphy_read_table(pi, &tab);
	/* format change from x.4 to x.7 */
	refTxAnGn = refTxAnGn * 8;
	write_phy_reg(pi, LCNPHY_papd_tx_analog_gain_ref, (uint16)refTxAnGn);
	PHY_PAPD(("refTxAnGn = %d\n", refTxAnGn));

	/* Force ADC on */
	mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
		1 << LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
		LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK,
		0 << LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_SHIFT);

	/* Switch on the PA */
	mod_phy_reg(pi, LCNPHY_rfoverride4,
		LCNPHY_rfoverride4_papu_ovr_MASK,
		1 << LCNPHY_rfoverride4_papu_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_rfoverride4val,
		LCNPHY_rfoverride4val_papu_ovr_val_MASK,
		1 << LCNPHY_rfoverride4val_papu_ovr_val_SHIFT);

	/* Save the contents of RF Registers */
	save_REG116  = (uint8)read_radio_reg(pi, RADIO_2064_REG116);
	save_REG12D  = (uint8)read_radio_reg(pi, RADIO_2064_REG12D);
	save_REG12C  = (uint8)read_radio_reg(pi, RADIO_2064_REG12C);
	save_REG06A  = (uint8)read_radio_reg(pi, RADIO_2064_REG06A);
	save_REG098  = (uint8)read_radio_reg(pi, RADIO_2064_REG098);
	save_REG097  = (uint8)read_radio_reg(pi, RADIO_2064_REG097);
	save_REG12F  = (uint8)read_radio_reg(pi, RADIO_2064_REG12F);
	save_REG00B  = (uint8)read_radio_reg(pi, RADIO_2064_REG00B);
	save_REG113  = (uint8)read_radio_reg(pi, RADIO_2064_REG113);
	save_REG01D  = (uint8)read_radio_reg(pi, RADIO_2064_REG01D);
	save_REG114  = (uint8)read_radio_reg(pi, RADIO_2064_REG114);
	save_REG02E  = (uint8)read_radio_reg(pi, RADIO_2064_REG02E);
	save_REG12A  = (uint8)read_radio_reg(pi, RADIO_2064_REG12A);
	save_REG009  = (uint8)read_radio_reg(pi, RADIO_2064_REG009);
	save_REG11F  = (uint8)read_radio_reg(pi, RADIO_2064_REG11F);
	save_REG007  = (uint8)read_radio_reg(pi, RADIO_2064_REG007);
	save_REG0FF  = (uint8)read_radio_reg(pi, RADIO_2064_REG0FF);
	save_REG005  = (uint8)read_radio_reg(pi, RADIO_2064_REG005);

	/* RF overrides */
	write_radio_reg(pi, RADIO_2064_REG116, 0x7); /* rxrf , lna1 , lna2 pwrup */
	write_radio_reg(pi, RADIO_2064_REG12D, 0x1); /* tr sw rx pwrup8 */
	write_radio_reg(pi, RADIO_2064_REG12C, 0x7); /* rx rf pwrup1 ,rf pwrup2,rf pwrup7 */

	/* TIA pwrup,G band mixer up,G band Rx global pwrup,  G band TR switch in Rx mode */
	if (pi_lcn->lcnphy_papd_4336_mode)
		write_radio_reg(pi, RADIO_2064_REG06A, 0xc2);
	else
		write_radio_reg(pi, RADIO_2064_REG06A, 0xc3);

	/* set up loopback */
	write_radio_reg(pi, RADIO_2064_REG098, 0xc);  /* PAPD cal path pwrup     */

	if (pi_lcn->lcnphy_papd_4336_mode)
		write_radio_reg(pi, RADIO_2064_REG097, 0xf0); /* atten for papd cal path */
	else
		write_radio_reg(pi, RADIO_2064_REG097, 0xfa); /* atten for papd cal path */

	write_radio_reg(pi, RADIO_2064_REG12F, 0x3);  /* override for papd cal path pwrup */
	write_radio_reg(pi, RADIO_2064_REG00B, 0x7);  /* pu for rx buffer,bw set 25Mhz */
	write_radio_reg(pi, RADIO_2064_REG113, 0x10); /* override for pu rx buffer */
	write_radio_reg(pi, RADIO_2064_REG01D, 0x1);  /* Pu for Tx buffer */
	write_radio_reg(pi, RADIO_2064_REG114, 0x1);  /* override for pu Tx buffer */
	write_radio_reg(pi, RADIO_2064_REG02E, 0x10); /* logen Rx path enble */
	write_radio_reg(pi, RADIO_2064_REG12A, 0x8);  /* override for logen Rx path enable */
	write_radio_reg(pi, RADIO_2064_REG009, 0x2);  /* enable papd loopback path */

	mod_radio_reg(pi, RADIO_2064_REG11F, 0x4, (1<<2));
	mod_radio_reg(pi, RADIO_2064_REG0FF, 0x10, (0<<4));
	mod_radio_reg(pi, RADIO_2064_REG007, 0x1, (0<<0));
	mod_radio_reg(pi, RADIO_2064_REG005, 0x8, (0<<3));

	if (LCNREV_LT(pi->pubpi.phy_rev, 2))
		write_phy_reg(pi, LCNPHY_AphyControlAddr, 0x0001);
	else
		mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_forceTxfiltClkOn_MASK,
		1 << LCNPHY_sslpnCalibClkEnCtrl_forceTxfiltClkOn_SHIFT);

	/* Do Rx Gain Control */
	rxGnIdx = wlc_lcnphy_papd_rxGnCtrl(pi, cal_type, frcRxGnCtrl, CurTxGain);

	/* Set Rx Gain */
	wlc_lcnphy_set_rx_gain_by_distribution(pi, 1, 0, 0, 0, (uint16)rxGnIdx, 0, 0);

	/* clear our PAPD Compensation table */
	 wlc_lcnphy_clear_papd_comptable(pi);

	/* Do PAPD Operation - All symbols in one go */
	wlc_lcnphy_papd_cal_core(pi, cal_type,
		FALSE,
		txGnCtrl,
		samplecapture,
		papd_dbg_mode,
		num_symbols,
		init_papd_lut,
		1400,
		16640,
		0,
		128,
		0);

	wlc_lcnphy_GetpapdMaxMinIdxupdt(pi, &maxUpdtIdx, &minUpdtIdx);

	PHY_PAPD(("wl%d: %s max: %d, min: %d\n",
		pi->sh->unit, __FUNCTION__, maxUpdtIdx, minUpdtIdx));

	if ((minUpdtIdx >= 0) && (minUpdtIdx < PHY_PAPD_EPS_TBL_SIZE_LCNPHY) &&
		(maxUpdtIdx >= 0) && (maxUpdtIdx < PHY_PAPD_EPS_TBL_SIZE_LCNPHY) &&
		(minUpdtIdx <= maxUpdtIdx)) {

		if (cal_type == LCNPHY_PAPD_CAL_CW) {
			tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
			tab.tbl_offset = minUpdtIdx;
			tab.tbl_ptr = &tmpVar; /* ptr to buf */
			wlc_lcnphy_read_table(pi, &tab);
			min_papd_lut_val = tmpVar;
			tab.tbl_offset = maxUpdtIdx;
			wlc_lcnphy_read_table(pi, &tab);
			max_papd_lut_val = tmpVar;
			for (j = 0; j < minUpdtIdx; j++) {
				tmpVar = min_papd_lut_val;
				tab.tbl_offset = j;
				tab.tbl_ptr = &tmpVar;
				wlc_lcnphy_write_table(pi, &tab);
			}
		}

		for (j = 0; j < 64; j++) {
			tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
			tab.tbl_offset = j;
			tab.tbl_ptr = &tmpVar;
			wlc_lcnphy_read_table(pi, &tab);
		}

		PHY_PAPD(("wl%d: %s: PAPD cal completed\n", pi->sh->unit, __FUNCTION__));
		if (papd_peak_curr_mode == 0) {
			tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
			tab.tbl_offset = 62;
			tab.tbl_ptr = &tmpVar;
			wlc_lcnphy_read_table(pi, &tab);
			tab.tbl_offset = 63;
			wlc_lcnphy_write_table(pi, &tab);
			tab.tbl_offset = 1;
			wlc_lcnphy_read_table(pi, &tab);
			tab.tbl_offset = 0;
			wlc_lcnphy_write_table(pi, &tab);
		}

		wlc_lcnphy_save_papd_calibration_results(pi);
	}
	else
		PHY_PAPD(("Error in PAPD Cal. Exiting... \n"));


	/* restore saved registers */
	write_phy_reg(pi, LCNPHY_lpfbwlutreg0, lpfbwlut0);
	write_phy_reg(pi, LCNPHY_lpfbwlutreg1, lpfbwlut1);

	write_phy_reg(pi, LCNPHY_AphyControlAddr, AphyControl_old);
	write_phy_reg(pi, LCNPHY_Core1TxControl, Core1TxControl_old);
	write_phy_reg(pi, LCNPHY_sslpnCtrl3, lcnCtrl3_old);

	/* restore calib ctrl clk */
	/* switch on PAPD clk */
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, lcnCalibClkEnCtrl_old | 0x1);

	wlc_lcnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);

	/* Restore AMUX sel */
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0,
		amuxSelPortOverride, save_amuxSelPortOverride);
	MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlRfCtrlOverride0,
		amuxSelPortOverrideVal, save_amuxSelPortOverrideVal);

	/* TR switch */
	wlc_lcnphy_clear_trsw_override(pi);

	/* Restore rx path mux and turn off PAPD mixer, bias filter settings */
	write_radio_reg(pi, RADIO_2064_REG116, save_REG116);
	write_radio_reg(pi, RADIO_2064_REG12D, save_REG12D);
	write_radio_reg(pi, RADIO_2064_REG12C, save_REG12C);
	write_radio_reg(pi, RADIO_2064_REG06A, save_REG06A);
	write_radio_reg(pi, RADIO_2064_REG098, save_REG098);
	write_radio_reg(pi, RADIO_2064_REG097, save_REG097);
	write_radio_reg(pi, RADIO_2064_REG12F, save_REG12F);
	write_radio_reg(pi, RADIO_2064_REG00B, save_REG00B);
	write_radio_reg(pi, RADIO_2064_REG113, save_REG113);
	write_radio_reg(pi, RADIO_2064_REG01D, save_REG01D);
	write_radio_reg(pi, RADIO_2064_REG114, save_REG114);
	write_radio_reg(pi, RADIO_2064_REG02E, save_REG02E);
	write_radio_reg(pi, RADIO_2064_REG12A, save_REG12A);
	write_radio_reg(pi, RADIO_2064_REG009, save_REG009);
	write_radio_reg(pi, RADIO_2064_REG11F, save_REG11F);
	write_radio_reg(pi, RADIO_2064_REG007, save_REG007);
	write_radio_reg(pi, RADIO_2064_REG0FF, save_REG0FF);
	write_radio_reg(pi, RADIO_2064_REG005, save_REG005);

	/* Clear rx PU override */
	mod_phy_reg(pi, LCNPHY_RFOverride0,
		LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
		0 << LCNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);

	wlc_lcnphy_tx_pu(pi, FALSE);

	/* Clear rx gain override */
	wlc_lcnphy_rx_gain_override_enable(pi, FALSE);

	/* Clear ADC override */
	MOD_PHY_REG(pi, LCNPHY, AfeCtrlOvr, pwdn_adc_ovr, 0);

	/* Restore CRS */
	wlc_lcnphy_deaf_mode(pi, FALSE);

	/* Restore Digital Filter */
	wlc_lcnphy_save_restore_dig_filt_state(pi, FALSE, save_filtcoeffs);

	/* Enable PAPD */
	mod_phy_reg(pi, LCNPHY_papd_control,
		LCNPHY_papd_control_papdCompEn_MASK,
		1 << LCNPHY_papd_control_papdCompEn_SHIFT);

	mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
		LCNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_MASK,
		1 << LCNPHY_sslpnCalibClkEnCtrl_papdTxClkEn_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_cck_papden_MASK,
		1 << LCNPHY_txfefilterconfig_cck_papden_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_ofdm_papden_MASK,
		1 << LCNPHY_txfefilterconfig_ofdm_papden_SHIFT);

	mod_phy_reg(pi, LCNPHY_txfefilterconfig,
		LCNPHY_txfefilterconfig_ht_papden_MASK,
		1 << LCNPHY_txfefilterconfig_ht_papden_SHIFT);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	/* restore bbmult */
	wlc_lcnphy_set_bbmult(pi, (uint8)bb_mult_old);
}

/*
* Get Rx IQ Imbalance Estimate from modem
*/
static bool
wlc_lcnphy_rx_iq_est(phy_info_t *pi,
	uint16 num_samps,
	uint8 wait_time,
	lcnphy_iq_est_t *iq_est)
{
	int wait_count = 0;
	bool result = TRUE;
	uint8 phybw40;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	/* Turn on clk to Rx IQ */
	MOD_PHY_REG(pi, LCNPHY, sslpnCalibClkEnCtrl, iqEstClkEn, 1);
	/* Force OFDM receiver on */
	MOD_PHY_REG(pi, LCNPHY, crsgainCtrl, APHYGatingEnable, 0);
	MOD_PHY_REG(pi, LCNPHY, IQNumSampsAddress, numSamps, num_samps);
	MOD_PHY_REG(pi, LCNPHY, IQEnableWaitTimeAddress, waittimevalue,
		(uint16)wait_time);
	MOD_PHY_REG(pi, LCNPHY, IQEnableWaitTimeAddress, iqmode, 0);
	MOD_PHY_REG(pi, LCNPHY, IQEnableWaitTimeAddress, iqstart, 1);

	/* Wait for IQ estimation to complete */
	while (read_phy_reg(pi, LCNPHY_IQEnableWaitTimeAddress) &
		LCNPHY_IQEnableWaitTimeAddress_iqstart_MASK) {
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
	iq_est->iq_prod = ((uint32)read_phy_reg(pi, LCNPHY_IQAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, LCNPHY_IQAccLoAddress);
	iq_est->i_pwr = ((uint32)read_phy_reg(pi, LCNPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, LCNPHY_IQIPWRAccLoAddress);
	iq_est->q_pwr = ((uint32)read_phy_reg(pi, LCNPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)read_phy_reg(pi, LCNPHY_IQQPWRAccLoAddress);
	PHY_TMP(("wl%d: %s: IQ estimation completed in %d us,"
		"i_pwr: %d, q_pwr: %d, iq_prod: %d\n",
		pi->sh->unit, __FUNCTION__,
		wait_count * 100, iq_est->i_pwr, iq_est->q_pwr, iq_est->iq_prod));

cleanup:
	MOD_PHY_REG(pi, LCNPHY, crsgainCtrl, APHYGatingEnable, 1);
	MOD_PHY_REG(pi, LCNPHY, sslpnCalibClkEnCtrl, iqEstClkEn, 0);
	return result;
}

/*
* Compute Rx compensation coeffs
*   -- run IQ est and calculate compensation coefficients
*/
static bool
wlc_lcnphy_calc_rx_iq_comp(phy_info_t *pi,  uint16 num_samps)
{
#define LCNPHY_MIN_RXIQ_PWR 2
	bool result;
	uint16 a0_new, b0_new;
	lcnphy_iq_est_t iq_est = {0, 0, 0};
	int32  a, b, temp;
	int16  iq_nbits, qq_nbits, arsh, brsh;
	int32  iq;
	uint32 ii, qq;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Save original c0 & c1 */
	a0_new = ((read_phy_reg(pi, LCNPHY_RxCompcoeffa0) & LCNPHY_RxCompcoeffa0_a0_MASK) >>
		LCNPHY_RxCompcoeffa0_a0_SHIFT);
	b0_new = ((read_phy_reg(pi, LCNPHY_RxCompcoeffb0) & LCNPHY_RxCompcoeffb0_b0_MASK) >>
		LCNPHY_RxCompcoeffb0_b0_SHIFT);
	MOD_PHY_REG(pi, LCNPHY, rxfe, bypass_iqcomp, 0);
	MOD_PHY_REG(pi, LCNPHY, RxIqCoeffCtrl, RxIqComp11bEn, 1);
	/* Zero out comp coeffs and do "one-shot" calibration */
	wlc_lcnphy_set_rx_iq_comp(pi, 0, 0);

	if (!(result = wlc_lcnphy_rx_iq_est(pi, num_samps, 32, &iq_est)))
		goto cleanup;

	iq = (int32)iq_est.iq_prod;
	ii = iq_est.i_pwr;
	qq = iq_est.q_pwr;

	/* bounds check estimate info */
	if ((ii + qq) < LCNPHY_MIN_RXIQ_PWR) {
		PHY_ERROR(("wl%d: %s: RX IQ imbalance estimate power too small\n",
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
cleanup:
	/* Apply new coeffs */
	wlc_lcnphy_set_rx_iq_comp(pi, a0_new, b0_new);
	/* enabling the hardware override to choose only a0, b0 coeff */
	MOD_PHY_REG(pi, LCNPHY, RxIqCoeffCtrl, RxIqCrsCoeffOverRide, 1);
	MOD_PHY_REG(pi, LCNPHY, RxIqCoeffCtrl,
		RxIqCrsCoeffOverRide11b, 1);

	pi_lcn->lcnphy_cal_results.rxiqcal_coeff_a0 = a0_new;
	pi_lcn->lcnphy_cal_results.rxiqcal_coeff_b0 = b0_new;

	return result;
}

/*
* RX IQ Calibration
*/
static bool
wlc_lcnphy_rx_iq_cal(phy_info_t *pi, const lcnphy_rx_iqcomp_t *iqcomp, int iqcomp_sz,
	bool tx_switch, bool rx_switch, int module, int tx_gain_idx)
{
	lcnphy_txgains_t old_gains;
	uint16 tx_pwr_ctrl;
	uint8 tx_gain_index_old = 0;
	bool result = FALSE, tx_gain_override_old = FALSE;
	uint16 i, Core1TxControl_old, RFOverride0_old,
	RFOverrideVal0_old, rfoverride2_old, rfoverride2val_old,
	rfoverride3_old, rfoverride3val_old, rfoverride4_old,
	rfoverride4val_old, afectrlovr_old, afectrlovrval_old;
	int tia_gain;
	uint32 received_power, rx_pwr_threshold;
	uint16 old_sslpnCalibClkEnCtrl, old_sslpnRxFeClkEnCtrl;
	uint16 values_to_save[11];
	int16 *ptr;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NULL == (ptr = MALLOC(pi->sh->osh, sizeof(int16) * 131))) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	if (module == 2) {
		ASSERT(iqcomp_sz);

		while (iqcomp_sz--) {
			if (iqcomp[iqcomp_sz].chan == CHSPEC_CHANNEL(pi->radio_chanspec)) {
				/* Apply new coeffs */
				wlc_lcnphy_set_rx_iq_comp(pi, (uint16)iqcomp[iqcomp_sz].a,
					(uint16)iqcomp[iqcomp_sz].b);
				result = TRUE;
				break;
			}
		}
		ASSERT(result);
		goto cal_done;
	}
	/* module : 1 = loopback */
	if (module == 1) {
		/* turn off tx power control */
		tx_pwr_ctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
		/* turn off papd */
		/* save rf register states */
		for (i = 0; i < 11; i++) {
			values_to_save[i] = read_radio_reg(pi, rxiq_cal_rf_reg[i]);
		}
		Core1TxControl_old = read_phy_reg(pi, LCNPHY_Core1TxControl);
		/* loft comp , iqmm comp enable */
		or_phy_reg(pi, LCNPHY_Core1TxControl, 0x0015);
		/* store old values to be restored */
		RFOverride0_old = read_phy_reg(pi, LCNPHY_RFOverride0);
		RFOverrideVal0_old = read_phy_reg(pi, LCNPHY_RFOverrideVal0);
		rfoverride2_old = read_phy_reg(pi, LCNPHY_rfoverride2);
		rfoverride2val_old = read_phy_reg(pi, LCNPHY_rfoverride2val);
		rfoverride3_old = read_phy_reg(pi, LCNPHY_rfoverride3);
		rfoverride3val_old = read_phy_reg(pi, LCNPHY_rfoverride3_val);
		rfoverride4_old = read_phy_reg(pi, LCNPHY_rfoverride4);
		rfoverride4val_old = read_phy_reg(pi, LCNPHY_rfoverride4val);
		afectrlovr_old = read_phy_reg(pi, LCNPHY_AfeCtrlOvr);
		afectrlovrval_old = read_phy_reg(pi, LCNPHY_AfeCtrlOvrVal);
		old_sslpnCalibClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);
		old_sslpnRxFeClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl);
		/* Save old tx gain settings */
		tx_gain_override_old = wlc_lcnphy_tx_gain_override_enabled(pi);
		if (tx_gain_override_old) {
			wlc_lcnphy_get_tx_gain(pi, &old_gains);
			tx_gain_index_old = pi_lcn->lcnphy_current_index;
		}
		/* Apply new tx gain */
		wlc_lcnphy_set_tx_pwr_by_index(pi, tx_gain_idx);
		/* PA override */
		mod_phy_reg(pi, LCNPHY_rfoverride3,
		            LCNPHY_rfoverride3_stxpapu_ovr_MASK,
		            1 << LCNPHY_rfoverride3_stxpapu_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride3_val,
		            LCNPHY_rfoverride3_val_stxpapu_ovr_val_MASK,
		            0 << LCNPHY_rfoverride3_val_stxpapu_ovr_val_SHIFT);
		/* Force DAC on */
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		            LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK,
		            1 << LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
		            LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
		            0 << LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);
		/* radio regs */
		write_radio_reg(pi, RADIO_2064_REG116, 0x06); /* rxrf, lna2 pwrup */
		write_radio_reg(pi, RADIO_2064_REG12C, 0x07); /* rxrf pwrup1, rf pwrup2, rfpwrup7 */
		write_radio_reg(pi, RADIO_2064_REG06A, 0xd3);
		write_radio_reg(pi, RADIO_2064_REG098, 0x03);
		write_radio_reg(pi, RADIO_2064_REG00B, 0x7);
		mod_radio_reg(pi, RADIO_2064_REG113, 1 << 4, 1 << 4);
		write_radio_reg(pi, RADIO_2064_REG01D, 0x01);
		write_radio_reg(pi, RADIO_2064_REG114, 0x01);
		write_radio_reg(pi, RADIO_2064_REG02E, 0x10);
		write_radio_reg(pi, RADIO_2064_REG12A, 0x08);
		/* Filter programming as per the Rx IQCAL Mode A (LPF in Rx mode) */
		mod_phy_reg(pi, LCNPHY_rfoverride4,
		            LCNPHY_rfoverride4_lpf_sel_tx_rx_ovr_MASK,
		            1 << LCNPHY_rfoverride4_lpf_sel_tx_rx_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4val,
		            LCNPHY_rfoverride4val_lpf_sel_tx_rx_ovr_val_MASK,
		            0 << LCNPHY_rfoverride4val_lpf_sel_tx_rx_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4,
		            LCNPHY_rfoverride4_lpf_pwrup_ovr_MASK,
		            1 << LCNPHY_rfoverride4_lpf_pwrup_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4val,
		            LCNPHY_rfoverride4val_lpf_pwrup_override_val_MASK,
		            1 << LCNPHY_rfoverride4val_lpf_pwrup_override_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4,
		            LCNPHY_rfoverride4_lpf_buf_pwrup_ovr_MASK,
		            1 << LCNPHY_rfoverride4_lpf_buf_pwrup_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4val,
		            LCNPHY_rfoverride4val_lpf_buf_pwrup_ovr_val_MASK,
		            1 << LCNPHY_rfoverride4val_lpf_buf_pwrup_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4,
		            LCNPHY_rfoverride4_lpf_tx_buf_pwrup_ovr_MASK,
		            1 << LCNPHY_rfoverride4_lpf_tx_buf_pwrup_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4val,
		            LCNPHY_rfoverride4val_lpf_tx_buf_pwrup_ovr_val_MASK,
		            1 << LCNPHY_rfoverride4val_lpf_tx_buf_pwrup_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4,
		            LCNPHY_rfoverride4_lpf_byp_dc_ovr_MASK,
		            1 << LCNPHY_rfoverride4_lpf_byp_dc_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_rfoverride4val,
		            LCNPHY_rfoverride4val_lpf_byp_dc_ovr_val_MASK,
		            0 << LCNPHY_rfoverride4val_lpf_byp_dc_ovr_val_SHIFT);
		/* Force ADC on */
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		            LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
		            1 << LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
		mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
		            LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK,
		            0 << LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_SHIFT);
		/* Run calibration */
		wlc_lcnphy_start_tx_tone(pi, 2000, 120, 0);
		write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xffff);
		or_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, 0x3);
		wlc_lcnphy_set_trsw_override(pi, tx_switch, rx_switch);
		wlc_lcnphy_rx_gain_override_enable(pi, TRUE);
		/* adjust rx power */
		tia_gain = 8;
		rx_pwr_threshold = 950;
		while (tia_gain > 0) {
			tia_gain -= 1;
			wlc_lcnphy_set_rx_gain_by_distribution(pi,
				0, 0, 2, 2, (uint16)tia_gain, 1, 0);
			OSL_DELAY(500);
			/* this part of the code has been kept to facilitate debug */
			/* wlc_lcnphy_samp_cap(pi, 0, 0, &ptr[0], 2); */
			/* for (m = 0; m < 128; m++) */
			/* 	printf("%d\n", ptr[m]); */
			received_power = wlc_lcnphy_measure_digital_power(pi, 2000);
			if (received_power < rx_pwr_threshold)
				break;
		}
		PHY_INFORM(("wl%d: %s Setting TIA gain to %d",
		            pi->sh->unit, __FUNCTION__, tia_gain));
		result = wlc_lcnphy_calc_rx_iq_comp(pi, 0xffff);
		/* clean up */
		wlc_lcnphy_stop_tx_tone(pi);
		/* restore papd state */
		/* restore Core1TxControl */
		write_phy_reg(pi, LCNPHY_Core1TxControl, Core1TxControl_old);
		/* save the older override settings */
		write_phy_reg(pi, LCNPHY_RFOverride0, RFOverrideVal0_old);
		write_phy_reg(pi, LCNPHY_RFOverrideVal0, RFOverrideVal0_old);
		write_phy_reg(pi, LCNPHY_rfoverride2, rfoverride2_old);
		write_phy_reg(pi, LCNPHY_rfoverride2val, rfoverride2val_old);
		write_phy_reg(pi, LCNPHY_rfoverride3, rfoverride3_old);
		write_phy_reg(pi, LCNPHY_rfoverride3_val, rfoverride3val_old);
		write_phy_reg(pi, LCNPHY_rfoverride4, rfoverride4_old);
		write_phy_reg(pi, LCNPHY_rfoverride4val, rfoverride4val_old);
		write_phy_reg(pi, LCNPHY_AfeCtrlOvr, afectrlovr_old);
		write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, afectrlovrval_old);
		write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, old_sslpnCalibClkEnCtrl);
		write_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, old_sslpnRxFeClkEnCtrl);
		/* Restore TR switch */
		wlc_lcnphy_clear_trsw_override(pi);
		/* temporary fix as RFOverrideVal0 entering into rx_iq_cal */
		/* is seen to have ant_selp_ovr_val turned on */
		mod_phy_reg(pi, LCNPHY_RFOverride0,
		            LCNPHY_RFOverride0_ant_selp_ovr_MASK,
		            0 << LCNPHY_RFOverride0_ant_selp_ovr_SHIFT);
		/* Restore RF Registers */
		for (i = 0; i < 11; i++) {
			write_radio_reg(pi, rxiq_cal_rf_reg[i], values_to_save[i]);
		}
		/* Restore Tx gain */
		if (tx_gain_override_old) {
			wlc_lcnphy_set_tx_pwr_by_index(pi, tx_gain_index_old);
		} else
			wlc_lcnphy_disable_tx_gain_override(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

		/* Clear various overrides */
		wlc_lcnphy_rx_gain_override_enable(pi, FALSE);
	}

cal_done:
	MFREE(pi->sh->osh, ptr, 131 * sizeof(int16));
	PHY_INFORM(("wl%d: %s: Rx IQ cal complete, coeffs: A0: %d, B0: %d\n",
		pi->sh->unit, __FUNCTION__,
		(int16)((read_phy_reg(pi, LCNPHY_RxCompcoeffa0) & LCNPHY_RxCompcoeffa0_a0_MASK)
		>> LCNPHY_RxCompcoeffa0_a0_SHIFT),
		(int16)((read_phy_reg(pi, LCNPHY_RxCompcoeffb0) & LCNPHY_RxCompcoeffb0_b0_MASK)
		>> LCNPHY_RxCompcoeffb0_b0_SHIFT)));
	return result;
}

static void
wlc_lcnphy_temp_adj(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return;
}
static void
wlc_lcnphy_glacial_timer_based_cal(phy_info_t* pi)
{
	bool suspend;
	int8 index;
	uint16 SAVE_pwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}
	wlc_lcnphy_deaf_mode(pi, TRUE);
	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	index = pi_lcn->lcnphy_current_index;
	/* do a full h/w iqlo cal during periodic cal */ 
	wlc_lcnphy_txpwrtbl_iqlo_cal(pi);
	wlc_2064_vco_cal(pi);
	/* cleanup */
	wlc_lcnphy_set_tx_pwr_by_index(pi, index);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_pwrctrl);
	wlc_lcnphy_deaf_mode(pi, FALSE);
	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

}
/* periodic cal does tx iqlo cal, rx iq cal, (tempcompensated txpwrctrl for P200 4313A0 board) */
static void
wlc_lcnphy_periodic_cal(phy_info_t *pi)
{
	bool suspend, full_cal;
	const lcnphy_rx_iqcomp_t *rx_iqcomp;
	int rx_iqcomp_sz;
	lcnphy_txcalgains_t txgains;
	uint16 SAVE_pwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	int8 index;
	phytbl_info_t tab;
	int32 a1, b0, b1;
	int32 tssi, pwr, maxtargetpwr, mintargetpwr;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	pi->phy_lastcal = pi->sh->now;
	pi->phy_forcecal = FALSE;
	full_cal = (pi_lcn->lcnphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec));
	pi_lcn->lcnphy_full_cal_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	index = pi_lcn->lcnphy_current_index;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}
	wlc_lcnphy_deaf_mode(pi, TRUE);

	wlc_lcnphy_btcx_override_enable(pi);

	wlc_lcnphy_txpwrtbl_iqlo_cal(pi);

	rx_iqcomp = lcnphy_rx_iqcomp_table_rev0;
	rx_iqcomp_sz = ARRAYSIZE(lcnphy_rx_iqcomp_table_rev0);
	/* For 4313B0, Tx index at which rxiqcal is done, has to be 40 */
	/* to get good SNR in the loopback path */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && LCNREV_IS(pi->pubpi.phy_rev, 1))
		wlc_lcnphy_rx_iq_cal(pi, NULL, 0, TRUE, FALSE, 1, 40);
	else
		wlc_lcnphy_rx_iq_cal(pi, NULL, 0, TRUE, FALSE, 1, 127);
	/* PAPD cal */
	if (pi_lcn->lcnphy_papd_4336_mode && !pi_lcn->ePA) {
		index = pi_lcn->lcnphy_current_index;
		if ((CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) &&
			(pi->sh->chippkg == BCM4336_WLBGA_PKG_ID))
			txgains.index = 40;
		else if (CHIPID(pi->sh->chip) == BCM4330_CHIP_ID) {
			if (pi->sh->chiprev == 0)
				txgains.index = 60;
			else
				txgains.index = 40;
		}
		else
			txgains.index = 14;
		txgains.useindex = 1;
		wlc_lcnphy_papd_cal(pi, LCNPHY_PAPD_CAL_CW, &txgains, 0, 0, 0, 0, 219, 1);
		wlc_lcnphy_set_tx_pwr_by_index(pi, index);
	}

	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi)) {
		/* Idle TSSI Estimate */
		wlc_lcnphy_idle_tssi_est((wlc_phy_t *) pi);

		/* On lcnphy, estPwrLuts0/1 table entries are in S6.3 format */
		b0 = pi->txpa_2g[0];
		b1 = pi->txpa_2g[1];
		a1 = pi->txpa_2g[2];
		if ((CHIPID(pi->sh->chip) != BCM4313_CHIP_ID)) {
			if (LCNREV_GE(pi->pubpi.phy_rev, 1)) {
				maxtargetpwr = wlc_lcnphy_tssi2dbm(1, a1, b0, b1);
				mintargetpwr = 0;
			} else {
				maxtargetpwr = wlc_lcnphy_tssi2dbm(127, a1, b0, b1);
				mintargetpwr = wlc_lcnphy_tssi2dbm(LCN_TARGET_TSSI, a1, b0, b1);
			}
		} else {
			maxtargetpwr = wlc_lcnphy_tssi2dbm(10, a1, b0, b1);
			mintargetpwr = wlc_lcnphy_tssi2dbm(125, a1, b0, b1);
		}

		/* Convert tssi to power LUT */
		tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_ptr = &pwr; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_offset = 0; /* estPwrLuts */
		for (tssi = 0; tssi < 128; tssi++) {
			pwr = wlc_lcnphy_tssi2dbm(tssi, a1, b0, b1);
			if (LCNREV_LT(pi->pubpi.phy_rev, 2))
				pwr = (pwr < mintargetpwr) ? mintargetpwr : pwr;
			wlc_lcnphy_write_table(pi,  &tab);
			tab.tbl_offset++;
		}
	}

	wlc_lcnphy_set_tx_pwr_by_index(pi, index);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, SAVE_pwrctrl);
	wlc_lcnphy_deaf_mode(pi, FALSE);

	if (pi_lcn->noise.per_cal_enable) {
	  /* Trigger noise cal */
	  wlc_lcnphy_noise_measure_start(pi);
	}

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

}

void
wlc_lcnphy_calib_modes(phy_info_t *pi, uint mode)
{
	uint16 temp_new;
	int temp1, temp2, temp_diff;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	switch (mode) {
	case PHY_PERICAL_CHAN:
		/* right now, no channel based calibration */
		break;
	case PHY_FULLCAL:
		wlc_lcnphy_periodic_cal(pi);
		break;
	case PHY_PERICAL_PHYINIT:
		/* 4313 does a init time full cal */
		if (CHIPID(pi->sh->chip == BCM4313_CHIP_ID))
			wlc_lcnphy_periodic_cal(pi);
		break;
	case PHY_PERICAL_WATCHDOG:
		if (CHIPID(pi->sh->chip == BCM4313_CHIP_ID) &&
			wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
			temp_new = wlc_lcnphy_tempsense(pi, 0);
			temp1 = LCNPHY_TEMPSENSE(temp_new);
			temp2 = LCNPHY_TEMPSENSE(pi_lcn->lcnphy_cal_temper);
			temp_diff = temp1 - temp2;
			if ((pi_lcn->lcnphy_cal_counter > 90) ||
				(temp_diff > 60) || (temp_diff < -60)) {
				wlc_lcnphy_glacial_timer_based_cal(pi);
				pi_lcn->lcnphy_cal_temper = temp_new;
				pi_lcn->lcnphy_cal_counter = 0;
			} else
				pi_lcn->lcnphy_cal_counter++;
		} else if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID ||
			CHIPID(pi->sh->chip) == BCM4330_CHIP_ID)
			wlc_lcnphy_periodic_cal(pi);
		else
			PHY_INFORM(("No periodic cal for tssi based tx pwr ctrl supported chip"));
		break;
	case LCNPHY_PERICAL_TEMPBASED_TXPWRCTRL:
		if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
			wlc_lcnphy_tx_power_adjustment((wlc_phy_t *)pi);
		break;
	default:
		ASSERT(0);
		break;
	}
}
void
wlc_lcnphy_get_tssi(phy_info_t *pi, int8 *ofdm_pwr, int8 *cck_pwr)
{
	int8 cck_offset;
	uint16 status;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi) &&
		((status = (read_phy_reg(pi, LCNPHY_TxPwrCtrlStatus))) &
		LCNPHY_TxPwrCtrlStatus_estPwrValid_MASK)) {
		*ofdm_pwr = (int8)(READ_PHY_REG(pi, LCNPHY, TxPwrCtrlStatus, estPwr) >> 1);

		if (wlc_phy_tpc_isenabled_lcnphy(pi))
			cck_offset = pi->tx_power_offset[TXP_FIRST_CCK];
		else
			cck_offset = 0;
		/* change to 6.2 */
		*cck_pwr = *ofdm_pwr + cck_offset;
	} else {
		*cck_pwr = 0;
		*ofdm_pwr = 0;
	}
}

void
WLBANDINITFN(wlc_phy_cal_init_lcnphy)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	return;

}

static void
wlc_lcnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec);
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return;
	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
		if (channel == 14) {
			MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, txfiltSelect, 2);
		} else {
			MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, txfiltSelect, 1);
		}
		pi_lcn->lcnphy_bandedge_corr = 2;
		if (channel == 1)
			pi_lcn->lcnphy_bandedge_corr = 4;
		/* spur/non-spur channel */
		if (channel == 1 || channel == 2 || channel == 3 ||
		    channel == 4 || channel == 9 ||
		    channel == 10 || channel == 11 || channel == 12) {
			si_pmu_pllcontrol(pi->sh->sih, 0x2, 0xffffffff, 0x03000c04);
			si_pmu_pllcontrol(pi->sh->sih, 0x3, 0xffffff, 0x0);
			si_pmu_pllcontrol(pi->sh->sih, 0x4, 0xffffffff, 0x200005c0);
			/* do a pll update */
			si_pmu_pllupd(pi->sh->sih);
			write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, FALSE);
			pi_lcn->lcnphy_spurmod = 0;
			MOD_PHY_REG(pi, LCNPHY, LowGainDB, MedLowGainDB, 0x1b);
			write_phy_reg(pi, LCNPHY_VeryLowGainDB, 0x5907);
		} else {
			si_pmu_pllcontrol(pi->sh->sih, 0x2, 0xffffffff, 0x03140c04);
			si_pmu_pllcontrol(pi->sh->sih, 0x3, 0xffffff, 0x333333);
			si_pmu_pllcontrol(pi->sh->sih, 0x4, 0xffffffff, 0x202c2820);
			/* do a pll update */
			si_pmu_pllupd(pi->sh->sih);
			write_phy_reg(pi, LCNPHY_lcnphy_clk_muxsel1, 0);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, TRUE);
			/* will become 1 when the functionality has to be brought in */
			pi_lcn->lcnphy_spurmod = 0;
			MOD_PHY_REG(pi, LCNPHY, LowGainDB, MedLowGainDB, 0x1f);
			write_phy_reg(pi, LCNPHY_VeryLowGainDB, 0x590a);
		}

	} else if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) {
		int gain_adj = 0;
		bool spur_enable = (channel == 13 || channel == 14) &&
			(pi->phy_spuravoid != SPURAVOID_DISABLE);

		MOD_PHY_REG(pi, LCNPHY, InputPowerDB, inputpwroffsetdb, -5);

		MOD_PHY_REG(pi, LCNPHY, MinPwrLevel, ofdmMinPwrLevel, -92);

		MOD_PHY_REG(pi, LCNPHY, MinPwrLevel, dsssMinPwrLevel, -97);

		if (spur_enable)
			gain_adj = 3;

		MOD_PHY_REG(pi, LCNPHY, HiGainDB, MedHiGainDB, 42);
		MOD_PHY_REG(pi, LCNPHY, LowGainDB, MedLowGainDB, 27 + gain_adj);
		MOD_PHY_REG(pi, LCNPHY, VeryLowGainDB, veryLowGainDB, 9);
	    MOD_PHY_REG(pi, LCNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 10);
	    MOD_PHY_REG(pi, LCNPHY, InputPowerDB, transientfreeThresh, 1);

		if (spur_enable) {
			si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 1);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, TRUE);
			pi_lcn->lcnphy_spurmod = 1;
		} else {
			si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 0);
			wlc_lcnphy_txrx_spur_avoidance_mode(pi, FALSE);
			pi_lcn->lcnphy_spurmod = 0;
		}
	}
	/* lcnphy_agc_reset */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
}

void
wlc_lcnphy_tx_power_adjustment(wlc_phy_t *ppi)
{
	int8 index;
	uint16 index2;
	phy_info_t *pi = (phy_info_t*)ppi;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint16 SAVE_txpwrctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi) && SAVE_txpwrctrl) {
		index = wlc_lcnphy_tempcompensated_txpwrctrl(pi);
		index2 = (uint16)(index * 2);
		MOD_PHY_REG(pi, LCNPHY, TxPwrCtrlBaseIndex, uC_baseIndex0, index2);
		pi_lcn->lcnphy_current_index = (int8)
			((read_phy_reg(pi, LCNPHY_TxPwrCtrlBaseIndex) & 0xFF)/2);
	}
}

static void
wlc_lcnphy_restore_txiqlo_calibration_results(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint16 didq;
	uint32 val;
	uint idx;
	uint8 ei0, eq0, fi0, fq0;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	a = pi_lcn->lcnphy_cal_results.txiqlocal_a;
	b = pi_lcn->lcnphy_cal_results.txiqlocal_b;
	didq = pi_lcn->lcnphy_cal_results.txiqlocal_didq;

	wlc_lcnphy_set_tx_iqcc(pi, a, b);
	wlc_lcnphy_set_tx_locc(pi, didq);

	/* restore iqlo portion of tx power control tables */
	/* remaining element */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &val; /* ptr to buf */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + idx;
		wlc_lcnphy_read_table(pi,  &tab);
		val = (val & 0x0ff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_lcnphy_write_table(pi,  &tab);
		/* loft */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_LO_OFFSET + idx;
		val = didq;
		wlc_lcnphy_write_table(pi,  &tab);
	}

	/* Do not move the below statements up */
	/* We need at least 2us delay to read phytable after writing radio registers */
	/* Apply analog LO */
	ei0 = (uint8)(pi_lcn->lcnphy_cal_results.txiqlocal_ei0);
	eq0 = (uint8)(pi_lcn->lcnphy_cal_results.txiqlocal_eq0);
	fi0 = (uint8)(pi_lcn->lcnphy_cal_results.txiqlocal_fi0);
	fq0 = (uint8)(pi_lcn->lcnphy_cal_results.txiqlocal_fq0);
	wlc_lcnphy_set_radio_loft(pi, ei0, eq0, fi0, fq0);
}

static void
wlc_lcnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a, uint16 b)
{
	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffa0, a0, a);
	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffb0, b0, b);

	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffa1, a1, a);
	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffb1, b1, b);

	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffa2, a2, a);
	MOD_PHY_REG(pi, LCNPHY, RxCompcoeffb2, b2, b);
}

static void
wlc_lcnphy_restore_calibration_results(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* restore tx iq cal results */
	wlc_lcnphy_restore_txiqlo_calibration_results(pi);

	/* restore PAPD cal results */
	if (pi_lcn->lcnphy_papd_4336_mode) {
		wlc_lcnphy_restore_papd_calibration_results(pi);
	}

	/* restore rx iq cal results */
	wlc_lcnphy_set_rx_iq_comp(pi, pi_lcn->lcnphy_cal_results.rxiqcal_coeff_a0,
		pi_lcn->lcnphy_cal_results.rxiqcal_coeff_b0);
}

void
WLBANDINITFN(wlc_phy_init_lcnphy)(phy_info_t *pi)
{
	uint8 phybw40;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	PHY_TRACE(("%s:\n", __FUNCTION__));

	if (CHIPID(pi->sh->chip) != BCM4313_CHIP_ID)
		pi_lcn->lcnphy_papd_4336_mode = TRUE;
	else
		pi_lcn->lcnphy_papd_4336_mode = FALSE;

	if ((pi->sh->boardflags & BFL_FEM) && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID))
		pi_lcn->ePA = 1;
	if (pi_lcn->lcnphy_papd_4336_mode) {
		pi_lcn->ePA = 0;
		if ((CHSPEC_IS2G(pi->radio_chanspec) &&
			pi_lcn->extpagain2g == 2) ||
			(CHSPEC_IS5G(pi->radio_chanspec) &&
			pi_lcn->extpagain5g == 2))
			pi_lcn->ePA = 1;
	}

	pi_lcn->lcnphy_cal_counter = 0;
	pi_lcn->lcnphy_cal_temper = pi_lcn->lcnphy_rawtempsense;

	/* reset the radio */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
	and_phy_reg(pi, LCNPHY_resetCtrl, 0x7f);

	/* initializing the adc-presync and auxadc-presync for 2x sampling */
	wlc_lcnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);

	/* txrealframe delay increased to ~2us */
	write_phy_reg(pi, LCNPHY_TxRealFrameDelay, 160);

	/* phy crs delay adj for spikes */
	write_phy_reg(pi, LCNPHY_ccktx_phycrsDelayValue, 25);

	/* Initialize baseband : bu_tweaks is a placeholder */
	wlc_lcnphy_baseband_init(pi);

	/* Initialize radio */
	wlc_lcnphy_radio_init(pi);

	/* txpwrctrl is not up in 4313A0 */
	if (CHSPEC_IS2G(pi->radio_chanspec))
		wlc_lcnphy_tx_pwr_ctrl_init((wlc_phy_t *) pi);

	/* Tune to the current channel */
	/* mapped to lcnphy_set_chan_raw minus the agc_temp_init, txpwrctrl init */
	wlc_phy_chanspec_set((wlc_phy_t*)pi, pi->radio_chanspec);

	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		/* set ldo voltage to 1.2 V */
		si_pmu_regcontrol(pi->sh->sih, 0, 0xf, 0x9);
		/* reduce crystal's drive strength */
		si_pmu_chipcontrol(pi->sh->sih, 0, 0xffffffff, 0x03CDDDDD);
	}

	/* here we are forcing the index to 78 */
	if (pi_lcn->ePA && wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		wlc_lcnphy_set_tx_pwr_by_index(pi, FIXED_TXPWR);

	/* save default params for AGC temp adjustments */
	wlc_lcnphy_agc_temp_init(pi);

	wlc_lcnphy_temp_adj(pi);

	MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 1);
	OSL_DELAY(100);
	MOD_PHY_REG(pi, LCNPHY, lpphyCtrl, resetCCA, 0);
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
	pi_lcn->lcnphy_noise_samples = LCNPHY_NOISE_SAMPLES_DEFAULT;
	wlc_lcnphy_calib_modes(pi, PHY_PERICAL_PHYINIT);
}

static void
wlc_lcnphy_tx_iqlo_loopback(phy_info_t *pi, uint16 *values_to_save)
{
	uint16 vmid;
	int i;
	for (i = 0; i < 20; i++) {
		values_to_save[i] = read_radio_reg(pi, iqlo_loopback_rf_regs[i]);
	}
	/* force tx on, rx off , force ADC on */
	mod_phy_reg(pi, LCNPHY_RFOverride0,
		LCNPHY_RFOverride0_internalrftxpu_ovr_MASK,
		1 << LCNPHY_RFOverride0_internalrftxpu_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
		LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK,
		1 << LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_SHIFT);

	mod_phy_reg(pi, LCNPHY_RFOverride0,
		LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK,
		1 << LCNPHY_RFOverride0_internalrfrxpu_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
		LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK,
		0 << LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_SHIFT);

	mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK,
		1 << LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
		LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK,
		0 << LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_SHIFT);

	mod_phy_reg(pi, LCNPHY_AfeCtrlOvr,
		LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK,
		1 << LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_SHIFT);
	mod_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
		LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK,
		0 << LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_SHIFT);

	/* PA override */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		and_radio_reg(pi, RADIO_2064_REG03A, 0xFD);
	else
	and_radio_reg(pi, RADIO_2064_REG03A, 0xF9);
	or_radio_reg(pi, RADIO_2064_REG11A, 0x1);
	/* envelope detector */
	or_radio_reg(pi, RADIO_2064_REG036, 0x01);
	or_radio_reg(pi, RADIO_2064_REG11A, 0x18);
	OSL_DELAY(20);
	/* turn on TSSI, pick band */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 1, 0);
		else
			or_radio_reg(pi, RADIO_2064_REG03A, 1);
	} else {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG03A, 3, 1);
		else
			or_radio_reg(pi, RADIO_2064_REG03A, 0x3);
	}

	OSL_DELAY(20);

	/* iqcal block */
	write_radio_reg(pi, RADIO_2064_REG025, 0xF);
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG028, 0xF, 0x4);
		else
			mod_radio_reg(pi, RADIO_2064_REG028, 0xF, 0x6);
	} else {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1e, 0x4 << 1);
		else
			mod_radio_reg(pi, RADIO_2064_REG028, 0x1e, 0x6 << 1);
	}

	OSL_DELAY(20);
	/* set testmux to iqcal */
	write_radio_reg(pi, RADIO_2064_REG005, 0x8);
	or_radio_reg(pi, RADIO_2064_REG112, 0x80);
	OSL_DELAY(20);
	/* enable aux adc */
	or_radio_reg(pi, RADIO_2064_REG0FF, 0x10);
	or_radio_reg(pi, RADIO_2064_REG11F, 0x44);
	OSL_DELAY(20);
	/* enable 'Rx Buff' */
	or_radio_reg(pi, RADIO_2064_REG00B, 0x7);
	or_radio_reg(pi, RADIO_2064_REG113, 0x10);
	OSL_DELAY(20);
	/* BB */
	write_radio_reg(pi, RADIO_2064_REG007, 0x1);
	OSL_DELAY(20);
	/* ADC Vmid , range etc. */
	vmid = 0x2A6;
	mod_radio_reg(pi, RADIO_2064_REG0FC, 0x3 << 0, (vmid >> 8) & 0x3);
	write_radio_reg(pi, RADIO_2064_REG0FD, (vmid & 0xff));
	or_radio_reg(pi, RADIO_2064_REG11F, 0x44);
	OSL_DELAY(20);
	/* power up aux pga */
	or_radio_reg(pi, RADIO_2064_REG0FF, 0x10);
	OSL_DELAY(20);
	write_radio_reg(pi, RADIO_2064_REG012, 0x02);
	or_radio_reg(pi, RADIO_2064_REG112, 0x06);
	write_radio_reg(pi, RADIO_2064_REG036, 0x11);
	write_radio_reg(pi, RADIO_2064_REG059, 0xcc);
	write_radio_reg(pi, RADIO_2064_REG05C, 0x2e);
	write_radio_reg(pi, RADIO_2064_REG078, 0xd7);
	write_radio_reg(pi, RADIO_2064_REG092, 0x15);
}

static void
wlc_lcnphy_samp_cap(phy_info_t *pi, int clip_detect_algo, uint16 thresh, int16* ptr, int mode)
{
	uint32 curval1, curval2, stpptr, curptr, strptr, val;
	uint16 sslpnCalibClkEnCtrl, timer;
	uint16 old_sslpnCalibClkEnCtrl;
	int16 imag, real;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	timer = 0;
	old_sslpnCalibClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);
	/* ********************samp capture ******************** */
	/* switch dot11mac_clk to macphy clock (80 MHz) */
	curval1 = R_REG(pi->sh->osh, &pi->regs->psm_corectlsts);
	ptr[130] = 0;
	W_REG(pi->sh->osh, &pi->regs->psm_corectlsts, ((1 << 6) | curval1));
	/* using the last 512 locations of the Tx fifo for sample capture */
	W_REG(pi->sh->osh, &pi->regs->smpl_clct_strptr, 0x7E00);
	W_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr, 0x8000);
	OSL_DELAY(20);
	curval2 = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, curval2 | 0x30);
	/* configure phy to do a timer based capture of mode 2(adc o/p) */
	write_phy_reg(pi, LCNPHY_triggerConfiguration, 0x0);
	write_phy_reg(pi, LCNPHY_sslpnphy_gpio_ctrl, 0x5);
	/* if dac has to be captured, the module sel will be 19 */
	write_phy_reg(pi, LCNPHY_sslpnphy_gpio_sel, (uint16)(mode | mode << 6));
	write_phy_reg(pi, LCNPHY_gpio_data_ctrl, 3);
	write_phy_reg(pi, LCNPHY_sslpnphy_gpio_out_en_34_32, 0x3);
	write_phy_reg(pi, LCNPHY_dbg_samp_coll_start_mac_xfer_trig_timer_15_0, 0x0);
	write_phy_reg(pi, LCNPHY_dbg_samp_coll_start_mac_xfer_trig_timer_31_16, 0x0);
	write_phy_reg(pi, LCNPHY_dbg_samp_coll_end_mac_xfer_trig_timer_15_0, 0x0fff);
	write_phy_reg(pi, LCNPHY_dbg_samp_coll_end_mac_xfer_trig_timer_31_16, 0x0000);
	/* programming dbg_samp_coll_ctrl_word */
	write_phy_reg(pi, LCNPHY_dbg_samp_coll_ctrl, 0x4501);
	/* enabling clocks to sampleplay buffer and debug blocks */
	sslpnCalibClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, (uint32)(sslpnCalibClkEnCtrl | 0x2008));
	stpptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr);
	curptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
	do {
		OSL_DELAY(10);
		curptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
		timer++;
	} while ((curptr != stpptr) && (timer < 500));
	/* PHY_CTL to 2 */
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, 0x2);
	strptr = 0x7E00;
	W_REG(pi->sh->osh, &pi->regs->tplatewrptr, strptr);
	while (strptr < 0x8000) {
		val = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
		imag = ((val >> 16) & 0x3ff);
		real = ((val) & 0x3ff);
		if (imag > 511) {
			imag -= 1024;
		}
		if (real > 511) {
			real -= 1024;
		}
		if (pi_lcn->lcnphy_iqcal_swp_dis)
			ptr[(strptr-0x7E00)/4] = real;
		else
			ptr[(strptr-0x7E00)/4] = imag;
		if (clip_detect_algo) {
			if (imag > thresh || imag < -thresh) {
				strptr = 0x8000;
				ptr[130] = 1;
			}
		}
		strptr += 4;
	}
	/* ********************samp capture ends**************** */
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, old_sslpnCalibClkEnCtrl);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, curval2);
	W_REG(pi->sh->osh, &pi->regs->psm_corectlsts, curval1);
}

/* run soft(samp capture based) tx iqlo cal for all sets of tx iq lo coeffs */
static void
wlc_lcnphy_tx_iqlo_soft_cal_full(phy_info_t *pi)
{
	lcnphy_unsign16_struct iqcc0, locc2, locc3, locc4;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	wlc_lcnphy_set_cc(pi, 0, 0, 0);
	wlc_lcnphy_set_cc(pi, 2, 0, 0);
	wlc_lcnphy_set_cc(pi, 3, 0, 0);
	wlc_lcnphy_set_cc(pi, 4, 0, 0);

	wlc_lcnphy_set_genv(pi, 7);
	/* calls for the iqlo calibration */
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 4, 0, 0);
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 3, 0, 0);
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 2, 3, 2);
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 0, 5, 8);
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 2, 2, 1);
	wlc_lcnphy_tx_iqlo_soft_cal(pi, 0, 4, 3);
	/* get the coeffs */
	iqcc0 = wlc_lcnphy_get_cc(pi, 0);
	locc2 = wlc_lcnphy_get_cc(pi, 2);
	locc3 = wlc_lcnphy_get_cc(pi, 3);
	locc4 = wlc_lcnphy_get_cc(pi, 4);
	PHY_INFORM(("wl%d: %s: Tx IQLO cal complete, coeffs: lcnphy_tx_iqcc: %d, %d\n",
		pi->sh->unit, __FUNCTION__, iqcc0.re, iqcc0.im));
	PHY_INFORM(("wl%d: %s coeffs: lcnphy_tx_locc: %d, %d, %d, %d, %d, %d\n",
		pi->sh->unit, __FUNCTION__, locc2.re, locc2.im,
		locc3.re, locc3.im, locc4.re, locc4.im));
}

static void
wlc_lcnphy_set_cc(phy_info_t *pi, int cal_type, int16 coeff_x, int16 coeff_y)
{
	uint16 di0dq0;
	uint16 x, y, data_rf;
	int k;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	switch (cal_type) {
		case 0:
			wlc_lcnphy_set_tx_iqcc(pi, coeff_x, coeff_y);
			break;
		case 2:
			di0dq0 = (coeff_x & 0xff) << 8 | (coeff_y & 0xff);
			wlc_lcnphy_set_tx_locc(pi, di0dq0);
			break;
		case 3:
			k = wlc_lcnphy_calc_floor(coeff_x, 0);
			y = 8 + k;
			k = wlc_lcnphy_calc_floor(coeff_x, 1);
			x = 8 - k;
			data_rf = (x * 16 + y);
			write_radio_reg(pi, RADIO_2064_REG089, data_rf);
			k = wlc_lcnphy_calc_floor(coeff_y, 0);
			y = 8 + k;
			k = wlc_lcnphy_calc_floor(coeff_y, 1);
			x = 8 - k;
			data_rf = (x * 16 + y);
			write_radio_reg(pi, RADIO_2064_REG08A, data_rf);
			break;
		case 4:
			k = wlc_lcnphy_calc_floor(coeff_x, 0);
			y = 8 + k;
			k = wlc_lcnphy_calc_floor(coeff_x, 1);
			x = 8 - k;
			data_rf = (x * 16 + y);
			write_radio_reg(pi, RADIO_2064_REG08B, data_rf);
			k = wlc_lcnphy_calc_floor(coeff_y, 0);
			y = 8 + k;
			k = wlc_lcnphy_calc_floor(coeff_y, 1);
			x = 8 - k;
			data_rf = (x * 16 + y);
			write_radio_reg(pi, RADIO_2064_REG08C, data_rf);
			break;
	}
}

static lcnphy_unsign16_struct
wlc_lcnphy_get_cc(phy_info_t *pi, int cal_type)
{
	uint16 a, b, didq;
	uint8 di0, dq0, ei, eq, fi, fq;
	lcnphy_unsign16_struct cc;
	cc.re = 0;
	cc.im = 0;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	switch (cal_type) {
		case 0:
			wlc_lcnphy_get_tx_iqcc(pi, &a, &b);
			cc.re = a;
			cc.im = b;
			break;
		case 2:
			didq = wlc_lcnphy_get_tx_locc(pi);
			di0 = (((didq & 0xff00) << 16) >> 24);
			dq0 = (((didq & 0x00ff) << 24) >> 24);
			cc.re = (uint16)di0;
			cc.im = (uint16)dq0;
			break;
		case 3:
			wlc_lcnphy_get_radio_loft(pi, &ei, &eq, &fi, &fq);
			cc.re = (uint16)ei;
			cc.im = (uint16)eq;
			break;
		case 4:
			wlc_lcnphy_get_radio_loft(pi, &ei, &eq, &fi, &fq);
			cc.re = (uint16)fi;
			cc.im = (uint16)fq;
			break;
	}
	return cc;
}
/* envelop detector gain */
static uint16
wlc_lcnphy_get_genv(phy_info_t *pi)
{
	return (read_radio_reg(pi, RADIO_2064_REG026) & 0x70) >> 4;
}

static void
wlc_lcnphy_set_genv(phy_info_t *pi, uint16 genv)
{
	write_radio_reg(pi, RADIO_2064_REG026, (genv & 0x7) | ((genv & 0x7) << 4));
}

/* minimal code to set the tx gain index */
/* sets bbmult, dac, rf gain, but not pa gain, iq coeffs, etc. */
/* an utility function only called by iqlo_soft_cal */
static void
wlc_lcnphy_set_tx_idx_raw(phy_info_t *pi, uint16 index)
{
	phytbl_info_t tab;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain;
	lcnphy_txgains_t gains;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	ASSERT(index <= LCNPHY_MAX_TX_POWER_INDEX);
	/* Save forced index */
	pi_lcn->lcnphy_tx_power_idx_override = (int8)index;
	pi_lcn->lcnphy_current_index = (uint8)index;

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */

	/* Turn off automatic power control */
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + index; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_lcnphy_read_table(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + index; /* gainCtrlLuts */
	tab.tbl_width = 32;
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_lcnphy_read_table(pi,  &tab);
	/* Apply tx gain */
	gains.gm_gain = (uint16)(txgain & 0xff);
	gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
	gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
	gains.dac_gain = (uint16)(bbmultiqcomp >> 28) & 0x07;
	wlc_lcnphy_set_tx_gain(pi, &gains);
	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 20) & 0xff);
	wlc_lcnphy_set_bbmult(pi, bb_mult);
	/* Enable gain overrides */
	wlc_lcnphy_enable_tx_gain_override(pi);
}

/* sample capture based tx iqlo cal */
/* cal type : 0 - IQ, 2 -dLO, 3 -eLO, 4 -fLO */
static void
wlc_lcnphy_tx_iqlo_soft_cal(phy_info_t *pi, int cal_type, int num_levels, int step_size_lg2)
{
	const lcnphy_spb_tone_t *tone_out;
	lcnphy_spb_tone_t spb_samp;
	lcnphy_unsign16_struct best;
	int tone_out_sz, genv, k, l, j, spb_index;
	uint16 grid_size, level, thresh, start_tx_idx;
	int16 coeff_max, coeff_x, coeff_y, best_x1, best_y1, best_x, best_y, tx_idx;
	int16 *ptr, samp;
	int32 ripple_re, ripple_im;
	uint32 ripple_abs, ripple_min;
	bool clipped, first_point, prev_clipped, first_pass;
	uint16 save_sslpnCalibClkEnCtrl, save_sslpnRxFeClkEnCtrl;
	uint16 save_rfoverride4, save_txpwrctrlrfctrloverride0, save_txpwrctrlrfctrloverride1;
	uint16 save_reg026;
	uint16 *values_to_save;
	ripple_min = 0;
	coeff_max = best_x1 = best_y1 = level = 0;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NULL == (ptr = MALLOC(pi->sh->osh, sizeof(int16) * 131))) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}
	/* initial setup for tx iqlo cal */
	if (NULL == (values_to_save = MALLOC(pi->sh->osh, sizeof(uint16) * 20))) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}
	start_tx_idx = tx_idx = wlc_lcnphy_get_current_tx_pwr_idx(pi);
	save_sslpnCalibClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl);
	save_sslpnRxFeClkEnCtrl = read_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl);
	save_reg026 = read_radio_reg(pi, RADIO_2064_REG026);
	write_phy_reg(pi, LCNPHY_iqloCalGainThreshD2, 0xC0);
	/* start tx tone, load a precalculated tone of same freq into tone_out */
	wlc_lcnphy_start_tx_tone(pi, 3750, 88, 0);
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, 0xffff);
	or_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, 0x3);
	/* save regs */
	wlc_lcnphy_tx_iqlo_loopback(pi, values_to_save);
	OSL_DELAY(500);
	save_rfoverride4 = read_phy_reg(pi, LCNPHY_rfoverride4);
	save_txpwrctrlrfctrloverride0 = read_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0);
	save_txpwrctrlrfctrloverride1 = read_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1);
	or_phy_reg(pi, LCNPHY_rfoverride4, 0x1 << LCNPHY_rfoverride4_lpf_buf_pwrup_ovr_SHIFT);
	or_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
		0x1 << LCNPHY_TxPwrCtrlRfCtrlOverride0_paCtrlTssiOverride_SHIFT);
	or_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
		0x1 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverride_SHIFT);
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0,
		LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_MASK,
		0x2 << LCNPHY_TxPwrCtrlRfCtrlOverride0_amuxSelPortOverrideVal_SHIFT);
	or_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1,
		1 << LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelVmidOverride_SHIFT);
	or_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1,
		1 << LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelGainOverride_SHIFT);
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1,
		LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelVmidOverrideVal_MASK,
		0x2A6 << LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelVmidOverrideVal_SHIFT);
	mod_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1,
		LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelGainOverrideVal_MASK,
		0x2 << LCNPHY_TxPwrCtrlRfCtrlOverride1_afeAuxpgaSelGainOverrideVal_SHIFT);


	tone_out = &lcnphy_spb_tone_3750[0];
	tone_out_sz = 32;
	/* Grid search */
	if (num_levels == 0) {
		if (cal_type != 0) {
			num_levels = 4;
		} else {
			num_levels = 9;
		}
	}
	if (step_size_lg2 == 0) {
		if (cal_type != 0) {
			step_size_lg2 = 3;
		} else {
			step_size_lg2 = 8;
		}
	}
	/* Grid search inputs */
	grid_size = (1 << step_size_lg2);
	best = wlc_lcnphy_get_cc(pi, cal_type);
	best_x = (int16)best.re;
	best_y = (int16)best.im;
	if (cal_type == 2) {
		if (best.re > 127)
			best_x = best.re - 256;
		if (best.im > 127)
			best_y = best.im - 256;
	}
	wlc_lcnphy_set_cc(pi, cal_type, best_x, best_y);
	OSL_DELAY(20);

	genv = wlc_lcnphy_get_genv(pi);

	for (level = 0; grid_size != 0 && level < num_levels; level++) {
		first_point = 1;
		clipped = 0;
		switch (cal_type) {
			case 0:
				coeff_max = 511;
				break;
			case 2:
				coeff_max = 127;
				break;
			case 3:
				coeff_max = 15;
				break;
			case 4:
				coeff_max = 15;
				break;
		}
		/* gain control */
		thresh = read_phy_reg(pi, LCNPHY_iqloCalGainThreshD2);
		thresh = 2*thresh;
		prev_clipped = 0;

		first_pass = 1;
		while (1) {
			wlc_lcnphy_set_genv(pi, (uint16)genv);
			OSL_DELAY(50);
			wlc_lcnphy_set_tx_idx_raw(pi, (uint16)tx_idx);
			clipped = 0;
			ptr[130] = 0;
			wlc_lcnphy_samp_cap(pi, 1, thresh, &ptr[0], 2);
			if (ptr[130] == 1)
				clipped = 1;
			if (clipped) {
				if (tx_idx < start_tx_idx || genv <= 0)
					tx_idx += 12;
				else
					genv -= 1;
			}
			if ((clipped != prev_clipped) && (!first_pass))
				break;
			if (!clipped) {
				if (tx_idx > start_tx_idx || genv >= 7)
					tx_idx -= 12;
				else
					genv += 1;
			}

			if ((genv <= 0 && tx_idx >= 127) || (genv >= 7 && tx_idx <= 0))
				break;
			prev_clipped = clipped;
			first_pass = 0;
			/* limiting genv to be within 0 to 7 */
			if (genv < 0)
				genv = 0;
			else if (genv > 7)
				genv = 7;

			if (tx_idx < 0)
				tx_idx = 0;
			else if (tx_idx > 127)
			tx_idx = 127;
		}
		/* limiting genv to be within 0 to 7 */
		if (genv < 0)
			genv = 0;
		else if (genv > 7)
			genv = 7;

		if (tx_idx < 0)
			tx_idx = 0;
		else if (tx_idx > 127)
			tx_idx = 127;
		/* printf("Best.re , grid size % d %d %d",best.re, best.im,grid_size); */
		for (k = -grid_size; k <= grid_size; k += grid_size) {
			for (l = -grid_size; l <= grid_size; l += grid_size) {
				coeff_x = best_x + k;
				coeff_y = best_y + l;
				/* limiting the coeffs */
				if (coeff_x < -coeff_max)
					coeff_x = -coeff_max;
				else if (coeff_x > coeff_max)
					coeff_x = coeff_max;
				if (coeff_y < -coeff_max)
					coeff_y = -coeff_max;
				else if (coeff_y > coeff_max)
					coeff_y = coeff_max;
				wlc_lcnphy_set_cc(pi, cal_type, coeff_x, coeff_y);
				OSL_DELAY(20);
				wlc_lcnphy_samp_cap(pi, 0, 0, ptr, 2);
				/* run correlator */
				ripple_re = 0;
				ripple_im = 0;
				for (j = 0; j < 128; j++) {
					if (cal_type != 0) {
						spb_index = j % tone_out_sz;
					} else {
						spb_index = (2*j) % tone_out_sz;
					}
					spb_samp.re = tone_out[spb_index].re;
					spb_samp.im = tone_out[spb_index].im;
					samp = ptr[j];
					ripple_re = ripple_re + samp*spb_samp.re;
					ripple_im = ripple_im + samp*spb_samp.im;
				}
				/* this is done to make sure squares of ripple_re and ripple_im */
				/* don't cross the limits of uint32 */
				ripple_re = ripple_re>>10;
				ripple_im = ripple_im>>10;
				ripple_abs = ((ripple_re*ripple_re)+(ripple_im*ripple_im));
				/* ripple_abs = (uint32)wlc_phy_sqrt_int((uint32) ripple_abs); */
				/* keep track of minimum ripple bin, best coeffs */
				if (first_point || ripple_abs < ripple_min) {
					ripple_min = ripple_abs;
					best_x1 = coeff_x;
					best_y1 = coeff_y;
				}
				first_point = 0;
			}
		}
		first_point = 1;
		best_x = best_x1;
		best_y = best_y1;
		grid_size = grid_size >> 1;
		wlc_lcnphy_set_cc(pi, cal_type, best_x, best_y);
		OSL_DELAY(20);
	}
	goto cleanup;
cleanup:
	wlc_lcnphy_tx_iqlo_loopback_cleanup(pi, values_to_save);
	wlc_lcnphy_stop_tx_tone(pi);
	write_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl, save_sslpnCalibClkEnCtrl);
	write_phy_reg(pi, LCNPHY_sslpnRxFeClkEnCtrl, save_sslpnRxFeClkEnCtrl);
	write_phy_reg(pi, LCNPHY_rfoverride4, save_rfoverride4);
	write_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride0, save_txpwrctrlrfctrloverride0);
	write_phy_reg(pi, LCNPHY_TxPwrCtrlRfCtrlOverride1, save_txpwrctrlrfctrloverride1);
	write_radio_reg(pi, RADIO_2064_REG026, save_reg026);
	/* Free allocated buffer */
	MFREE(pi->sh->osh, values_to_save, 20 * sizeof(uint16));
	MFREE(pi->sh->osh, ptr, 131 * sizeof(int16));
	wlc_lcnphy_set_tx_idx_raw(pi, start_tx_idx);
}

static void
wlc_lcnphy_tx_iqlo_loopback_cleanup(phy_info_t *pi, uint16 *values_to_save)
{
	int i;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* tx, rx PU */
	and_phy_reg(pi, LCNPHY_RFOverride0, 0x0 >> 11);
	/* ADC , DAC PU */
	and_phy_reg(pi, LCNPHY_AfeCtrlOvr, 0xC);
	/* restore the state of radio regs */
	for (i = 0; i < 20; i++) {
		write_radio_reg(pi, iqlo_loopback_rf_regs[i], values_to_save[i]);
	}
}

static void
WLBANDINITFN(wlc_lcnphy_load_tx_gain_table) (phy_info_t *pi,
	const lcnphy_tx_gain_tbl_entry * gain_table)
{
	uint32 j;
	phytbl_info_t tab;
	uint32 val;
	uint16 pa_gain;
	uint16 gm_gain;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	if (!pi_lcn->ePA) {
		pa_gain = (CHSPEC_IS5G(pi->radio_chanspec)) ? 0x70 : 0x70;

		if (CHIPID(pi->sh->chip) == BCM4330_CHIP_ID)
			pa_gain = 0x50;
	} else
		pa_gain = 0x10;

	if (CHSPEC_IS2G(pi->radio_chanspec) && (pi_lcn->pa_gain_ovr_val_2g != -1))
		pa_gain = pi_lcn->pa_gain_ovr_val_2g;
	else if (CHSPEC_IS5G(pi->radio_chanspec) && (pi_lcn->pa_gain_ovr_val_5g != -1))
		pa_gain = pi_lcn->pa_gain_ovr_val_5g;

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;     /* 32 bit wide  */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &val; /* ptr to buf */

	for (j = 0; j < 128; j++) {
		if (pi_lcn->lcnphy_papd_4336_mode && !pi_lcn->ePA &&
			CHSPEC_IS2G(pi->radio_chanspec))
			gm_gain = 15;
		else
			gm_gain = gain_table[j].gm;
		val = (((uint32)pa_gain << 24) |
			(gain_table[j].pad << 16) |
			(gain_table[j].pga << 8) | gm_gain);

		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + j;
		wlc_lcnphy_write_table(pi, &tab);

		val = (gain_table[j].dac << 28) |
			(gain_table[j].bb_mult << 20);
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + j;
		wlc_lcnphy_write_table(pi, &tab);
	}
}

static void
WLBANDINITFN(wlc_lcnphy_load_rfpower)(phy_info_t *pi)
{
	phytbl_info_t tab;
	uint32 val, bbmult, rfgain;
	uint8 index;
	uint8 scale_factor = 1;
	int16 temp, temp1, temp2, qQ, qQ1, qQ2, shift;

	tab.tbl_id = LCNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;     /* 32 bit wide  */
	tab.tbl_len = 1;        /* # values   */

	for (index = 0; index < 128; index++) {
		tab.tbl_ptr = &bbmult; /* ptr to buf */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_IQ_OFFSET + index;
		wlc_lcnphy_read_table(pi, &tab);
		bbmult = bbmult >> 20;

		tab.tbl_ptr = &rfgain; /* ptr to buf */
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_GAIN_OFFSET + index;
		wlc_lcnphy_read_table(pi, &tab);

		qm_log10((int32)(bbmult), 0, &temp1, &qQ1);
		qm_log10((int32)(1<<6), 0, &temp2, &qQ2);

		if (qQ1 < qQ2) {
			temp2 = qm_shr16(temp2, qQ2-qQ1);
			qQ = qQ1;
		}
		else {
			temp1 = qm_shr16(temp1, qQ1-qQ2);
			qQ = qQ2;
		}
		temp = qm_sub16(temp1, temp2);

		if (qQ >= 4)
			shift = qQ-4;
		else
			shift = 4-qQ;

		val = (((index << shift) + (5*temp) +
			(1<<(scale_factor+shift-3)))>>(scale_factor+shift-2));
		PHY_INFORM(("idx = %d, bb: %d, tmp = %d, qQ = %d, sh = %d, val = %d, rfgain = %x\n",
			index, bbmult, temp, qQ, shift, val, rfgain));

		tab.tbl_ptr = &val;
		tab.tbl_offset = LCNPHY_TX_PWR_CTRL_PWR_OFFSET + index;
		wlc_lcnphy_write_table(pi, &tab);
	}
}


/* initialize all the tables defined in auto-generated lcnphytbls.c,
 * see lcnphyprocs.tcl, proc lcnphy_tbl_init
 */
static void
WLBANDINITFN(wlc_lcnphy_tbl_init)(phy_info_t *pi)
{
	uint idx;
	uint8 phybw40;
	phytbl_info_t tab;
	uint16 val;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	for (idx = 0; idx < dot11lcnphytbl_info_sz_rev0; idx++) {
		wlc_lcnphy_write_table(pi, &dot11lcnphytbl_info_rev0[idx]);
	}
	/* delaying the switching on of ePA after Tx start which */
	/* is done by switch control for BT boards */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && (pi->sh->boardflags & BFL_FEM_BT)) {
		tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
		tab.tbl_width = 16;	/* 12 bit wide	*/
		tab.tbl_ptr = &val;
		tab.tbl_len = 1;
		val = 200;
		tab.tbl_offset = 4;
		wlc_lcnphy_write_table(pi,  &tab);
	}

	/* time domain spikes at beginning of packets related to   */
	/* shared lpf switching between tx and rx observed in 4336 */
	/* need to be relooked at for 4313, until then making it 4336 specific */ 

	if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) {
		/* RF seq */
		tab.tbl_id = LCNPHY_TBL_ID_RFSEQ;
		tab.tbl_width = 16;	/* 12 bit wide	*/
		tab.tbl_ptr = &val;
		tab.tbl_len = 1;

		val = 114;
		tab.tbl_offset = 0;
		wlc_lcnphy_write_table(pi,  &tab);

		val = 130;
		tab.tbl_offset = 1;
		wlc_lcnphy_write_table(pi,  &tab);

		val = 6;
		tab.tbl_offset = 8;
		wlc_lcnphy_write_table(pi,  &tab);
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		/* ePA board needs to use extPA gain table */
		if (pi_lcn->ePA)
			wlc_lcnphy_load_tx_gain_table(pi, dot11lcnphy_2GHz_extPA_gaintable_rev0);
		else
			wlc_lcnphy_load_tx_gain_table(pi, dot11lcnphy_2GHz_gaintable_rev0);
	}

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			for (idx = 0; idx < dot11lcnphytbl_rx_gain_info_2G_rev2_sz; idx++)
				if (pi->sh->boardflags & BFL_EXTLNA)
					wlc_lcnphy_write_table(pi,
						&dot11lcnphytbl_rx_gain_info_extlna_2G_rev2[idx]);
				else
					wlc_lcnphy_write_table(pi,
						&dot11lcnphytbl_rx_gain_info_2G_rev2[idx]);
		} else {
			for (idx = 0; idx < dot11lcnphytbl_rx_gain_info_5G_rev2_sz; idx++)
				if (pi->sh->boardflags & BFL_EXTLNA_5GHz)
					wlc_lcnphy_write_table(pi,
						&dot11lcnphytbl_rx_gain_info_extlna_5G_rev2[idx]);
				else
					wlc_lcnphy_write_table(pi,
						&dot11lcnphytbl_rx_gain_info_5G_rev2[idx]);
		}
	}

	/* switch table is different for 4313 */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		if ((pi->sh->boardflags & BFL_FEM) && !(pi->sh->boardflags & BFL_FEM_BT))
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4313_epa);
		else if (pi->sh->boardflags & BFL_FEM_BT) {
			if (pi->sh->boardrev < 0x1250)
				wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4313_bt_epa);
			else if (pi->aa2g > 2)
				wlc_lcnphy_write_table(pi,
					&dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250);
			else
				wlc_lcnphy_write_table(pi,
					&dot11lcn_sw_ctrl_tbl_info_4313_bt_epa_p250_single);
			} else
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4313);
	}
	else if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) {
		if (BOARDTYPE(pi->sh->boardtype) == BCM94336SDGP_SSID)  /* HP Board */
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4336sd_fcbga);
	}
	else if (CHIPID(pi->sh->chip) == BCM4330_CHIP_ID) {
		MOD_PHY_REG(pi, LCNPHY, swctrlconfig, swctrlmask, 0xff);
		if (BOARDTYPE(pi->sh->boardtype) == BCM94330SD_FCBGABU_SSID)
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4330sd_fcbga);
		else if (BOARDTYPE(pi->sh->boardtype) == BCM94330OLYMPICAMG_SSID)
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4330OlympicAMG);
#ifdef BCM94330OLYMPICAMGEPA_SSID
		else if (BOARDTYPE(pi->sh->boardtype) == BCM94330OLYMPICAMGEPA_SSID)
			wlc_lcnphy_write_table(pi, &dot11lcn_sw_ctrl_tbl_info_4330OlympicAMGepa);
#endif
	}

	/* Using A0's Rx gain table for B0 */
	#if 0
	/* increasing TIA gain by one notch for B0 */
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		for (idx = 0; idx < dot11lcnphytbl_rx_gain_info_sz_rev1; idx++) {
		}
	}
	#endif
	wlc_lcnphy_load_rfpower(pi);

	/* clear our PAPD Compensation table */
	wlc_lcnphy_clear_papd_comptable(pi);
}

/* mapped to lcnphy_rev0_reg_init */
static void
WLBANDINITFN(wlc_lcnphy_rev0_baseband_init)(phy_info_t *pi)
{
	uint16 afectrl1;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* disable internal VCO LDO PU override */
	write_radio_reg(pi, RADIO_2064_REG11C, 0x0);
	/* Enable DAC/ADC and disable rf overrides */
	write_phy_reg(pi, LCNPHY_AfeCtrlOvr, 0x0);
	write_phy_reg(pi, LCNPHY_AfeCtrlOvrVal, 0x0);
	write_phy_reg(pi,  LCNPHY_RFOverride0, 0x0);
	write_phy_reg(pi, LCNPHY_RFinputOverrideVal, 0x0);
	write_phy_reg(pi, LCNPHY_rfoverride3, 0x0);
	write_phy_reg(pi, LCNPHY_rfoverride2, 0x0);
	write_phy_reg(pi, LCNPHY_rfoverride4, 0x0);
	write_phy_reg(pi, LCNPHY_rfoverride2, 0x0);
	write_phy_reg(pi, LCNPHY_swctrlOvr, 0);
	/* enabling nominal state timeout */
	or_phy_reg(pi, LCNPHY_nomStTimeOutParams, 0x03);
	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
	/* tx gain init for 4313A0 boards without the ext PA */
	if (!(pi->sh->boardflags & BFL_FEM) && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID))
		wlc_lcnphy_set_tx_pwr_by_index(pi, 52);
	/* initialise rssi gains and vmids */
	/* rssismf2g is 3:0, rssismc2g is 7:4 and rssisav2g is 12:10 of 0x43e, * AfeRSSICtrl1 */
	/* not enabled in tcl */
	if (0) {
		afectrl1 = 0;
		afectrl1 = (uint16)((pi_lcn->lcnphy_rssi_vf) |
		                    (pi_lcn->lcnphy_rssi_vc << 4) | (pi_lcn->lcnphy_rssi_gs << 10));
		write_phy_reg(pi, LCNPHY_AfeRSSICtrl1, afectrl1);
	}
	/* bphy scale fine tuning to make cck dac ac pwr same as ofdm dac */
	mod_phy_reg(pi, LCNPHY_BphyControl3,
	            LCNPHY_BphyControl3_bphyScale_MASK,
	0xC << LCNPHY_BphyControl3_bphyScale_SHIFT);
	if ((pi->sh->boardflags & BFL_FEM) && (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		mod_phy_reg(pi, LCNPHY_BphyControl3,
		            LCNPHY_BphyControl3_bphyScale_MASK,
		            0xA << LCNPHY_BphyControl3_bphyScale_SHIFT);
		/* load iir filter type 1 for cck */
		/* middle stage biquad shift for cck */
		write_phy_reg(pi, LCNPHY_ccktxfilt20Stg1Shft, 0x1);
	}
	/* bphy filter selection , for channel 14 it is 2 */
	mod_phy_reg(pi, LCNPHY_lpphyCtrl,
	            LCNPHY_lpphyCtrl_txfiltSelect_MASK,
	            1 << LCNPHY_lpphyCtrl_txfiltSelect_SHIFT);
	mod_phy_reg(pi, LCNPHY_TxMacIfHoldOff,
	            LCNPHY_TxMacIfHoldOff_holdoffval_MASK,
	            0x17 <<	LCNPHY_TxMacIfHoldOff_holdoffval_SHIFT);
	mod_phy_reg(pi, LCNPHY_TxMacDelay,
	            LCNPHY_TxMacDelay_macdelay_MASK,
	            0x3EA << LCNPHY_TxMacDelay_macdelay_SHIFT);
	/* adc swap and crs stuck state is moved to bu_tweaks */
}

/* mapped to lcnphy_rev2_reg_init */
static void
WLBANDINITFN(wlc_lcnphy_rev2_baseband_init)(phy_info_t *pi)
{
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		mod_phy_reg(pi, LCNPHY_MinPwrLevel,
		LCNPHY_MinPwrLevel_ofdmMinPwrLevel_MASK,
		80 << LCNPHY_MinPwrLevel_ofdmMinPwrLevel_SHIFT);

		mod_phy_reg(pi, LCNPHY_MinPwrLevel,
		LCNPHY_MinPwrLevel_dsssMinPwrLevel_MASK,
		80 << LCNPHY_MinPwrLevel_dsssMinPwrLevel_SHIFT);
	}
}

static void
wlc_lcnphy_agc_temp_init(phy_info_t *pi)
{
	int16 temp;
	phytbl_info_t tab;
	uint32 tableBuffer[2];
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return;
	/* reference ofdm gain index table offset */
	temp = (int16) read_phy_reg(pi, LCNPHY_gainidxoffset);
	pi_lcn->lcnphy_ofdmgainidxtableoffset =
		(temp & LCNPHY_gainidxoffset_ofdmgainidxtableoffset_MASK) >>
		LCNPHY_gainidxoffset_ofdmgainidxtableoffset_SHIFT;

	if (pi_lcn->lcnphy_ofdmgainidxtableoffset > 127)
		pi_lcn->lcnphy_ofdmgainidxtableoffset -= 256;

	/* reference dsss gain index table offset */
	pi_lcn->lcnphy_dsssgainidxtableoffset =
		(temp & LCNPHY_gainidxoffset_dsssgainidxtableoffset_MASK) >>
		LCNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT;

	if (pi_lcn->lcnphy_dsssgainidxtableoffset > 127)
		pi_lcn->lcnphy_dsssgainidxtableoffset -= 256;

	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = 59;		/* tbl offset */
	tab.tbl_width = 32;			/* 32 bit wide */
	wlc_lcnphy_read_table(pi, &tab);

	/* reference value of gain_val_tbl at index 59 */
	if (tableBuffer[0] > 63)
		tableBuffer[0] -= 128;
	pi_lcn->lcnphy_tr_R_gain_val = tableBuffer[0];

	/* reference value of gain_val_tbl at index 60 */
	if (tableBuffer[1] > 63)
		tableBuffer[1] -= 128;
	pi_lcn->lcnphy_tr_T_gain_val = tableBuffer[1];

	temp = (int16)(read_phy_reg(pi, LCNPHY_InputPowerDB)
			& LCNPHY_InputPowerDB_inputpwroffsetdb_MASK);
	if (temp > 127)
		temp -= 256;
	pi_lcn->lcnphy_input_pwr_offset_db = (int8)temp;

	pi_lcn->lcnphy_Med_Low_Gain_db = (read_phy_reg(pi, LCNPHY_LowGainDB)
					& LCNPHY_LowGainDB_MedLowGainDB_MASK)
					>> LCNPHY_LowGainDB_MedLowGainDB_SHIFT;
	pi_lcn->lcnphy_Very_Low_Gain_db = (read_phy_reg(pi, LCNPHY_VeryLowGainDB)
					& LCNPHY_VeryLowGainDB_veryLowGainDB_MASK)
					>> LCNPHY_VeryLowGainDB_veryLowGainDB_SHIFT;

	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = LCNPHY_TBL_ID_GAIN_IDX;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = 28;		/* tbl offset */
	tab.tbl_width = 32;			/* 32 bit wide */
	wlc_lcnphy_read_table(pi, &tab);

	pi_lcn->lcnphy_gain_idx_14_lowword = tableBuffer[0];
	pi_lcn->lcnphy_gain_idx_14_hiword = tableBuffer[1];
	/* Added To Increase The 1Mbps Sense for Temps @Around */
	/* -15C Temp With CmRxAciGainTbl */
}

static void
WLBANDINITFN(wlc_lcnphy_bu_tweaks)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return;
	/* dmdStExitEn */
	or_phy_reg(pi, LCNPHY_pktfsmctrl, 0x1);
	/* DSSS threshold tightening to avoid cross detections */
	MOD_PHY_REG(pi, LCNPHY, DSSSConfirmCnt, DSSSConfirmCntHiGain, 0x3);
	MOD_PHY_REG(pi, LCNPHY, SyncPeakCnt, MaxPeakCntM1, 0x3);
	write_phy_reg(pi, LCNPHY_dsssPwrThresh0, 0x1e10);
	write_phy_reg(pi, LCNPHY_dsssPwrThresh1, 0x0640);

	/* cck gain table tweak to shift the gain table to give uptil */
	/* 98 dBm rx fe power */
	mod_phy_reg(pi, LCNPHY_gainidxoffset,
		LCNPHY_gainidxoffset_dsssgainidxtableoffset_MASK,
		-9 << LCNPHY_gainidxoffset_dsssgainidxtableoffset_SHIFT);
	/* lcnphy_agc_reset */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
		MOD_PHY_REG(pi, LCNPHY, InputPowerDB, inputpwroffsetdb, 0xFD);
		MOD_PHY_REG(pi, LCNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 16);
		if (!(pi->sh->boardrev < 0x1204))
			mod_radio_reg(pi, RADIO_2064_REG09B, 0xF0, 0xF0);
		/* fixes for diversity performance enhancement */
		write_phy_reg(pi, LCNPHY_diversityReg, 0x0902);
		MOD_PHY_REG(pi, LCNPHY, PwrThresh1, LargeGainMismatchThresh, 0x9);
		MOD_PHY_REG(pi, LCNPHY, PwrThresh1, LoPwrMismatchThresh, 0xe);
		if (LCNREV_IS(pi->pubpi.phy_rev, 1)) {
			MOD_PHY_REG(pi, LCNPHY, HiGainDB, HiGainDB, 0x46);
			MOD_PHY_REG(pi, LCNPHY, ofdmPwrThresh0, ofdmPwrThresh0, 1);
			MOD_PHY_REG(pi, LCNPHY, InputPowerDB, inputpwroffsetdb, 0xFF);
			MOD_PHY_REG(pi, LCNPHY, SgiprgReg, sgiPrg, 2);
			MOD_PHY_REG(pi, LCNPHY, RFOverrideVal0, ant_selp_ovr_val, 1);
			/* fix for ADC clip issue */
			mod_radio_reg(pi, RADIO_2064_REG0F7, 0x4, 0x4);
			mod_radio_reg(pi, RADIO_2064_REG0F1, 0x3, 0);
			mod_radio_reg(pi, RADIO_2064_REG0F2, 0xF8, 0x90);
			mod_radio_reg(pi, RADIO_2064_REG0F3, 0x3, 0x2);
			mod_radio_reg(pi, RADIO_2064_REG0F3, 0xf0, 0xa0);
			/* enabling override for o_wrf_jtag_afe_tadj_iqadc_ib20u_1 */
			mod_radio_reg(pi, RADIO_2064_REG11F, 0x2, 0x2);
			/* to be revisited, mac offsets of txpwrctrl table */
			/* are getting written with junk values */
			wlc_lcnphy_clear_tx_power_offsets(pi);
			/* Radio register tweaks for reducing Phase Noise */
			write_radio_reg(pi, RADIO_2064_REG09C, 0x20);
		}
	}
#ifdef BCM94330OLYMPICAMGEPA_SSID
	else if (BOARDTYPE(pi->sh->boardtype) == BCM94330OLYMPICAMGEPA_SSID) {
		MOD_PHY_REG(pi, LCNPHY, HiGainDB, HiGainDB, 76);
		mod_phy_reg(pi, LCNPHY_InputPowerDB,
			LCNPHY_InputPowerDB_inputpwroffsetdb_MASK,
			(0 << LCNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
		write_phy_reg(pi, LCNPHY_nfSubtractVal, 392);
		MOD_PHY_REG(pi, LCNPHY, MinPwrLevel, ofdmMinPwrLevel, -95);
		MOD_PHY_REG(pi, LCNPHY, MinPwrLevel, dsssMinPwrLevel, -100);
		MOD_PHY_REG(pi, LCNPHY, VeryLowGainDB, veryLowGainDB, 22); /* 425:0:6 */
		MOD_PHY_REG(pi, LCNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, 0); /* 50e:0:0 */
		MOD_PHY_REG(pi, LCNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 1); /* 50a:0:0 */
		MOD_PHY_REG(pi, LCNPHY,
			radioTRCtrl, gainrequestTRAttnOnOffset, 5); /* 50a:<7:1>:8 */
		MOD_PHY_REG(pi, LCNPHY, LowGainDB, MedLowGainDB, 40); /* 424:8:27 */
		MOD_PHY_REG(pi, LCNPHY, HiGainDB, MedHiGainDB, 58); /* 423:8:42 */
		MOD_PHY_REG(pi, LCNPHY, veryLowGainEx, veryLowGain_2DB, 40); /* 500:0:-3 */
		MOD_PHY_REG(pi, LCNPHY, veryLowGainEx, veryLowGain_3DB, 40); /* 500:8:-3 */
		MOD_PHY_REG(pi, LCNPHY,
			gaindirectMismatch, MedHigainDirectMismatch, 15); /* 427:<3,0>:12 */
		MOD_PHY_REG(pi, LCNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 10); /* 420:<7:0>:18 */
		MOD_PHY_REG(pi, LCNPHY,
			ClipCtrThresh, clipCtrThreshLoGain, 10); /* 420:<15:8>:15 */
		MOD_PHY_REG(pi, LCNPHY, ClipCtrDefThresh, clipCtrThresh, 20); /* 542:<7:0>:20 */
		MOD_PHY_REG(pi, LCNPHY,
			gaindirectMismatch, GainmisMatchMedGain, 0); /* 427:<12:8>:0 */
		MOD_PHY_REG(pi, LCNPHY, gainBackOffVal, genGainBkOff, 3); /* 54b:<3:0>:0 */
		MOD_PHY_REG(pi, LCNPHY,
			gaindirectMismatch, medGainGmShftVal, 2); /* 427:<15:13>:0 */
		MOD_PHY_REG(pi, LCNPHY, gainMismatch, GainmisMatchPktRx, 9); /* 426:<15:12>:6 */
		MOD_PHY_REG(pi, LCNPHY, ofdmPwrThresh1, ofdmPwrThresh2, 16); /* 411:<15:8>:6 */

	}
#endif /* BCM94330OLYMPICAMGEPA_SSID */
}

static void
WLBANDINITFN(wlc_lcnphy_baseband_init)(phy_info_t *pi)
{
	PHY_TRACE(("%s:***CHECK***\n", __FUNCTION__));
	/* Initialize LCNPHY tables */
	wlc_lcnphy_tbl_init(pi);
	wlc_lcnphy_rev0_baseband_init(pi);
	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		wlc_lcnphy_rev2_baseband_init(pi);
	wlc_lcnphy_bu_tweaks(pi);
	wlc_lcnphy_noise_init(pi);
}

/* mapped onto lcnphy_rev0_rf_init proc */
static void
WLBANDINITFN(wlc_radio_2064_init)(phy_info_t *pi)
{
	uint32 i;
	uint16 lpfbwlut;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	lcnphy_radio_regs_t *lcnphyregs = NULL;

	PHY_INFORM(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* init radio regs */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		lcnphyregs = lcnphy_radio_regs_2066;
	else
		lcnphyregs = lcnphy_radio_regs_2064;


	for (i = 0; lcnphyregs[i].address != 0xffff; i++)
		if (CHSPEC_IS5G(pi->radio_chanspec) &&
			lcnphyregs[i].do_init_a)
			write_radio_reg(pi,
				((lcnphyregs[i].address & 0x3fff) |
				RADIO_DEFAULT_CORE),
				(uint16)lcnphyregs[i].init_a);
		else if (lcnphyregs[i].do_init_g)
			write_radio_reg(pi,
				((lcnphyregs[i].address & 0x3fff) |
				RADIO_DEFAULT_CORE),
				(uint16)lcnphyregs[i].init_g);

	/* pa class setting */
	if (pi_lcn->lcnphy_papd_4336_mode)
		write_radio_reg(pi, RADIO_2064_REG032, 0x65);
	else
		write_radio_reg(pi, RADIO_2064_REG032, 0x62);
	write_radio_reg(pi, RADIO_2064_REG033, 0x19);

	/* TX Tuning */
	write_radio_reg(pi, RADIO_2064_REG090, 0x10);

	/* set LPF gain to 0 dB let ucode enable/disable override register based on tx/rx */
	write_radio_reg(pi, RADIO_2064_REG010, 0x00);

	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		/* adjust lna1,lna2 current for B0 */
		write_radio_reg(pi, RADIO_2064_REG060, 0x7f);
		write_radio_reg(pi, RADIO_2064_REG061, 0x72);
		write_radio_reg(pi, RADIO_2064_REG062, 0x7f);
	}
	if (CHIPID(pi->sh->chip) == BCM4336_CHIP_ID)
		write_radio_reg(pi, RADIO_2064_REG09B, 0x7);

	write_radio_reg(pi, RADIO_2064_REG01D, 0x02);
	write_radio_reg(pi, RADIO_2064_REG01E, 0x06);
	/* lpfbwlutregx = x where x = 0:4 */
	lpfbwlut = (0 << LCNPHY_lpfbwlutreg0_lpfbwlut0_SHIFT) |
		(1 << LCNPHY_lpfbwlutreg0_lpfbwlut1_SHIFT) |
		(2 << LCNPHY_lpfbwlutreg0_lpfbwlut2_SHIFT) |
		(3 << LCNPHY_lpfbwlutreg0_lpfbwlut3_SHIFT) |
		(4 << LCNPHY_lpfbwlutreg0_lpfbwlut4_SHIFT);

	write_phy_reg(pi, LCNPHY_lpfbwlutreg0, lpfbwlut);

	/* setting lpf_ofdm_tx_bw, lpf_cck_tx_bw and lpf_rx_bw */
	mod_phy_reg(pi, LCNPHY_lpfbwlutreg1,
		LCNPHY_lpfbwlutreg1_lpf_ofdm_tx_bw_MASK,
		2 << LCNPHY_lpfbwlutreg1_lpf_ofdm_tx_bw_SHIFT);

	if (LCNREV_IS(pi->pubpi.phy_rev, 2))
		MOD_PHY_REG(pi, LCNPHY, lpfofdmhtlutreg, lpf_ofdm_tx_ht_bw, 2);
	/* code to set lpf_cck_tx_bw moved to wlc_phy_chanspec_set_lcnphy() */

	/* rx setting */
	mod_phy_reg(pi, LCNPHY_lpfbwlutreg1,
		LCNPHY_lpfbwlutreg1_lpf_rx_bw_MASK,
		0 << LCNPHY_lpfbwlutreg1_lpf_rx_bw_SHIFT);

	mod_phy_reg(pi, LCNPHY_ccktx_phycrsDelayValue,
		LCNPHY_ccktx_phycrsDelayValue_ccktx_phycrsDelayValue_MASK,
		25 << LCNPHY_ccktx_phycrsDelayValue_ccktx_phycrsDelayValue_SHIFT);

	if (pi->btclock_tune)
		mod_radio_reg(pi, RADIO_2064_REG077, 0x0F, 0x0F);

	/* setting tx lo coefficients to 0 */
	wlc_lcnphy_set_tx_locc(pi, 0);

	/* run rcal */
	wlc_lcnphy_rcal(pi);

	/* run rc_cal */
	wlc_lcnphy_rc_cal(pi);
}

static void
WLBANDINITFN(wlc_lcnphy_radio_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (NORADIO_ENAB(pi->pubpi))
		return;
	/* Initialize 2064 radio */
	wlc_radio_2064_init(pi);
}

static void
wlc_lcnphy_rcal(phy_info_t *pi)
{
	uint8 rcal_value;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;
	/* power down RCAL */
	and_radio_reg(pi, RADIO_2064_REG05B, 0xfD);
	/* enable pwrsw */
	or_radio_reg(pi, RADIO_2064_REG004, 0x40);
	or_radio_reg(pi, RADIO_2064_REG120, 0x10);
	/* enable bandgap */
	or_radio_reg(pi, RADIO_2064_REG078, 0x80);
	or_radio_reg(pi, RADIO_2064_REG129, 0x02);
	/* enable RCAL clock */
	or_radio_reg(pi, RADIO_2064_REG057, 0x01);
	/* power up RCAL */
	or_radio_reg(pi, RADIO_2064_REG05B, 0x02);
	OSL_DELAY(5000);
	SPINWAIT(!wlc_radio_2064_rcal_done(pi), 10 * 1000 * 1000);
	/* wait for RCAL valid bit to be set */
	if (wlc_radio_2064_rcal_done(pi)) {
		rcal_value = (uint8) read_radio_reg(pi, RADIO_2064_REG05C);
		rcal_value = rcal_value & 0x1f;
		PHY_INFORM(("wl%d: %s:  Rx RCAL completed, code: %x\n",
		pi->sh->unit, __FUNCTION__, rcal_value));
	} else {
		PHY_ERROR(("wl%d: %s: RCAL failed\n", pi->sh->unit, __FUNCTION__));
	}
	/* power down RCAL */
	and_radio_reg(pi, RADIO_2064_REG05B, 0xfD);
	/* disable RCAL clock */
	and_radio_reg(pi, RADIO_2064_REG057, 0xFE);
}

static void
wlc_lcnphy_rc_cal(phy_info_t *pi)
{
	/* Bringing in code from lcnphyprocs.tcl */
	uint8 old_big_cap, old_small_cap;
	uint8 save_pll_jtag_pll_xtal, save_bb_rccal_hpc;
	uint8 dflt_rc_cal_val, hpc_code, hpc_offset, b_cap_rc_code_raw, s_cap_rc_code_raw;
	uint16 flt_val, trc;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	hpc_code = 0xb;
	b_cap_rc_code_raw = 0xb;
	s_cap_rc_code_raw = 0x9;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) {
		dflt_rc_cal_val = 7;
		if (LCNREV_IS(pi->pubpi.phy_rev, 1))
			dflt_rc_cal_val = 11;
		flt_val = (dflt_rc_cal_val << 10) | (dflt_rc_cal_val << 5) | (dflt_rc_cal_val);
		write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_1, flt_val);
		write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_2, flt_val);
		write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_3, flt_val);
		write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_4, flt_val);
		write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_5, (flt_val & 0x1FF));
		and_radio_reg(pi, RADIO_2064_REG057, 0xFD);
		return;
	}
	/* 4330 */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (pi->xtalfreq / 100000 == 374) {
			trc = 0x169;
			hpc_offset = 0;
		} else {
			PHY_INFORM(("wl%d: %s:  RCCAL not run, xtal "
				"%d not known\n",
				pi->sh->unit, __FUNCTION__, pi->xtalfreq));
			return;
		}
	} else {
		/* 4336 WLBGA */
		if (pi->sh->chippkg == BCM4336_WLBGA_PKG_ID) {
			if (pi->xtalfreq / 1000000 == 26) {
				trc = 0xFA;
				hpc_offset = 0;
			} else {
				PHY_INFORM(("wl%d: %s:  RCCAL not run, xtal "
					"%d not known\n",
					pi->sh->unit, __FUNCTION__, pi->xtalfreq));
				return;
			}
		} else {
			trc = pi->xtalfreq / 1000000 * 264 / 26;
			hpc_offset = 2;
		}
	}

	/* Save old value of pll_xtal_1 */
	save_pll_jtag_pll_xtal = (uint8) read_radio_reg(pi, RADIO_2064_REG057);
	/* Save old HPC value incase RCCal fails */
	save_bb_rccal_hpc = (uint8) read_radio_reg(pi, RADIO_2064_REG017);
	/* Save old big cap value incase RCCal fails */
	old_big_cap = (uint8)read_radio_reg(pi, RADIO_2064_REG018);

	/* Power down RC CAL */
	and_radio_reg(pi, RADIO_2064_REG105, (uint8)~(0x04));

	/*	enable Pwrsw	*/
	or_radio_reg(pi, RADIO_2064_REG004, 0x40);
	or_radio_reg(pi, RADIO_2064_REG120, 0x10);
	/* Enable RCCal Clock */
	or_radio_reg(pi, RADIO_2064_REG057, 0x02);
	/* Power up RC Cal */
	or_radio_reg(pi, RADIO_2064_REG105, 0x04);
	/* setup to run Rx RC Cal and setup R1/Q1/P1 */
	write_radio_reg(pi, RADIO_2064_REG106, 0x2A);
	/* set X1<7:0> */
	write_radio_reg(pi, RADIO_2064_REG107, 0x6E);
	/* set Trc <7:0> */
	write_radio_reg(pi, RADIO_2064_REG108, trc & 0xFF);
	/* set Trc <12:8> */
	write_radio_reg(pi, RADIO_2064_REG109, (trc >> 8) & 0x1F);
	/* RCCAL on big cap first */
	and_radio_reg(pi, RADIO_2064_REG105, (uint8)~(0x02));
	/* Start RCCAL */
	or_radio_reg(pi, RADIO_2064_REG106, 0x01);
	/* check to see if RC CAL is done */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2064_rc_cal_done(pi), 10 * 1000 * 1000);
	if (!wlc_radio_2064_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Big Cap RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		or_radio_reg(pi, RADIO_2064_REG018, old_big_cap);
	} else {
		/* RCCAL successful */
		PHY_INFORM(("wl%d: %s:  Rx RC Cal completed for "
		            "cap: N0: %x%x, N1: %x%x, code: %x\n",
		            pi->sh->unit, __FUNCTION__,
		            read_radio_reg(pi, RADIO_2064_REG10C),
		            read_radio_reg(pi, RADIO_2064_REG10B),
		            read_radio_reg(pi, RADIO_2064_REG10E),
		            read_radio_reg(pi, RADIO_2064_REG10D),
		            read_radio_reg(pi, RADIO_2064_REG10F) & 0x1f));
		b_cap_rc_code_raw = read_radio_reg(pi, RADIO_2064_REG10F) & 0x1f;
		if (b_cap_rc_code_raw < 0x1E)
			hpc_code = b_cap_rc_code_raw + hpc_offset;
	}
	/* RCCAL on small cap */
	/* Save old small cap value incase RCCal fails */
	old_small_cap = (uint8)read_radio_reg(pi, RADIO_2064_REG019);
	/* Stop RCCAL */
	and_radio_reg(pi, RADIO_2064_REG106, (uint8)~(0x01));
	/* Power down RC CAL */
	and_radio_reg(pi, RADIO_2064_REG105, (uint8)~(0x04));
	/* Power up RC Cal */
	or_radio_reg(pi, RADIO_2064_REG105, 0x04);
	/* RCCAL on small Cap */
	or_radio_reg(pi, RADIO_2064_REG105, 0x02);
	/* Start RCCAL */
	or_radio_reg(pi, RADIO_2064_REG106, 0x01);
		/* check to see if RC CAL is done */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2064_rc_cal_done(pi), 10 * 1000 * 1000);
	if (!wlc_radio_2064_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Small Cap RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2064_REG019, old_small_cap);
	} else {
		/* RCCAL successful */
		PHY_INFORM(("wl%d: %s:  Rx RC Cal completed for cap: "
		            "N0: %x%x, N1: %x%x, code: %x\n",
		            pi->sh->unit, __FUNCTION__,
		            read_radio_reg(pi, RADIO_2064_REG10C),
		            read_radio_reg(pi, RADIO_2064_REG10B),
		            read_radio_reg(pi, RADIO_2064_REG10E),
		            read_radio_reg(pi, RADIO_2064_REG10D),
		            read_radio_reg(pi, RADIO_2064_REG110) & 0x1f));
		s_cap_rc_code_raw = read_radio_reg(pi, RADIO_2064_REG110) & 0x1f;
	}

	/* Stop RC Cal */
	and_radio_reg(pi, RADIO_2064_REG106, (uint8)~(0x01));
	/* Power down RC CAL */
	and_radio_reg(pi, RADIO_2064_REG105, (uint8)~(0x04));
	/* Restore old value of pll_xtal_1 */
	write_radio_reg(pi, RADIO_2064_REG057, save_pll_jtag_pll_xtal);

	flt_val =
		(s_cap_rc_code_raw << 10) | (s_cap_rc_code_raw << 5) | (s_cap_rc_code_raw);
	write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_1, flt_val);
	write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_2, flt_val);

	flt_val =
		(b_cap_rc_code_raw << 10) | (b_cap_rc_code_raw << 5) | (s_cap_rc_code_raw);
	write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_3, flt_val);

	flt_val =
		(b_cap_rc_code_raw << 10) | (b_cap_rc_code_raw << 5) | (b_cap_rc_code_raw);
	write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_4, flt_val);

	flt_val = (hpc_code << 5) | (hpc_code);
	write_phy_reg(pi, LCNPHY_lpf_rccal_tbl_5, (flt_val & 0x1FF));

	/* narrow 9 MHz tx filter to improve CCK SM */
	if (0 && pi_lcn->lcnphy_papd_4336_mode && LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		MOD_PHY_REG(pi, LCNPHY, lpf_rccal_tbl_1,  small_tx_9, 0x1F);
		MOD_PHY_REG(pi, LCNPHY, lpf_rccal_tbl_3,  big_tx_9, 0x1F);
	}
}

/* Read band specific data from the SROM */
static bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_lcnphy)(phy_info_t *pi)
{
	int8 txpwr = 0;
	int i;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	/* Band specific setup */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		uint16 cckpo = 0;
		uint32 offset_ofdm, offset_mcs;

		/* TR switch isolation */
		pi_lcn->lcnphy_tr_isolation_mid = (uint8)PHY_GETINTVAR(pi, "triso2g");

		/* Input power offset */
		pi_lcn->lcnphy_rx_power_offset = (uint8)PHY_GETINTVAR(pi, "rxpo2g");
		/* pa0b0 */
		pi->txpa_2g[0] = (int16)PHY_GETINTVAR(pi, "pa0b0");
		pi->txpa_2g[1] = (int16)PHY_GETINTVAR(pi, "pa0b1");
		pi->txpa_2g[2] = (int16)PHY_GETINTVAR(pi, "pa0b2");

		/* RSSI */
		pi_lcn->lcnphy_rssi_vf = (uint8)PHY_GETINTVAR(pi, "rssismf2g");
		pi_lcn->lcnphy_rssi_vc = (uint8)PHY_GETINTVAR(pi, "rssismc2g");
		pi_lcn->lcnphy_rssi_gs = (uint8)PHY_GETINTVAR(pi, "rssisav2g");

		{
			pi_lcn->lcnphy_rssi_vf_lowtemp = pi_lcn->lcnphy_rssi_vf;
			pi_lcn->lcnphy_rssi_vc_lowtemp = pi_lcn->lcnphy_rssi_vc;
			pi_lcn->lcnphy_rssi_gs_lowtemp = pi_lcn->lcnphy_rssi_gs;

			pi_lcn->lcnphy_rssi_vf_hightemp = pi_lcn->lcnphy_rssi_vf;
			pi_lcn->lcnphy_rssi_vc_hightemp = pi_lcn->lcnphy_rssi_vc;
			pi_lcn->lcnphy_rssi_gs_hightemp = pi_lcn->lcnphy_rssi_gs;
		}

		/* Max tx power */
		txpwr = (int8)PHY_GETINTVAR(pi, "maxp2ga0");
		pi->tx_srom_max_2g = txpwr;

		for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
				pi->txpa_2g_low_temp[i] = pi->txpa_2g[i];
				pi->txpa_2g_high_temp[i] = pi->txpa_2g[i];
		}

		cckpo = (uint16)PHY_GETINTVAR(pi, "cck2gpo");
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

			/* Extract offsets for 8 OFDM rates */
			offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdm2gpo");
			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
				pi->tx_srom_max_rate_2g[i] = max_pwr_chan -
					((offset_ofdm & 0xf) * 2);
				offset_ofdm >>= 4;
			}
		} else {
			uint8 opo = 0;

			opo = (uint8)PHY_GETINTVAR(pi, "opo");

			/* Populate max power array for CCK rates */
			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr;
			}

			/* Extract offsets for 8 OFDM rates */
			offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdm2gpo");

			/* Populate max power array for OFDM rates */
			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM;  i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr -
					((offset_ofdm & 0xf) * 2);
				offset_ofdm >>= 4;
			}
			offset_mcs =
				((uint16)PHY_GETINTVAR(pi, "mcs2gpo1") << 16) |
				(uint16)PHY_GETINTVAR(pi, "mcs2gpo0");
			pi_lcn->lcnphy_mcs20_po = offset_mcs;
			for (i = TXP_FIRST_SISO_MCS_20; i <= TXP_LAST_SISO_MCS_20;  i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr -
					((offset_mcs & 0xf) * 2);
				offset_mcs >>= 4;
			}
		}
		/* for tempcompensated tx power control */
		pi_lcn->lcnphy_rawtempsense = (uint16)PHY_GETINTVAR(pi, "rawtempsense");
		pi_lcn->lcnphy_measPower = (uint8)PHY_GETINTVAR(pi, "measpower");
		pi_lcn->lcnphy_tempsense_slope = (uint8)PHY_GETINTVAR(pi, "tempsense_slope");
		pi_lcn->lcnphy_hw_iqcal_en = (bool)PHY_GETINTVAR(pi, "hw_iqcal_en");
		pi_lcn->lcnphy_iqcal_swp_dis = (bool)PHY_GETINTVAR(pi, "iqcal_swp_dis");
		pi_lcn->lcnphy_tempcorrx = (uint8)PHY_GETINTVAR(pi, "tempcorrx");
		pi_lcn->lcnphy_tempsense_option = (uint8)PHY_GETINTVAR(pi, "tempsense_option");
		pi_lcn->lcnphy_freqoffset_corr = (uint8)PHY_GETINTVAR(pi, "freqoffset_corr");
		pi->aa2g = (uint8) PHY_GETINTVAR(pi, "aa2g");
		pi_lcn->extpagain2g = (uint8)PHY_GETINTVAR(pi, "extpagain2g");
		pi_lcn->extpagain5g = (uint8)PHY_GETINTVAR(pi, "extpagain5g");
#if defined(WLTEST)
		pi_lcn->txpwrindex_nvram = (uint8)PHY_GETINTVAR(pi, "txpwrindex");
#endif 
		if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)) {
			if (pi->aa2g > 1)
				wlc_phy_ant_rxdiv_set((wlc_phy_t *)pi, pi->aa2g);
		}
	}
	pi_lcn->lcnphy_cck_dig_filt_type = -1;
	if (PHY_GETVAR(pi, "cckdigfilttype")) {
		int16 temp;
		temp = (int16) PHY_GETINTVAR(pi, "cckdigfilttype");
		if (temp >= 0) {
			pi_lcn->lcnphy_cck_dig_filt_type  = temp;
		}
	}

	/* pa gain override */
	if (PHY_GETVAR(pi, "pagc2g"))
		pi_lcn->pa_gain_ovr_val_2g = (int16)PHY_GETINTVAR(pi, "pagc2g");
	 else
		pi_lcn->pa_gain_ovr_val_2g = -1;
	if (PHY_GETVAR(pi, "pagc5g"))
		pi_lcn->pa_gain_ovr_val_5g = (int16)PHY_GETINTVAR(pi, "pagc5g");
	 else
		pi_lcn->pa_gain_ovr_val_5g = -1;

	return TRUE;
}

void
wlc_2064_vco_cal(phy_info_t *pi)
{
	uint8 calnrst;

	mod_radio_reg(pi, RADIO_2064_REG057, 1 << 3, 1 << 3);
	calnrst = (uint8)read_radio_reg(pi, RADIO_2064_REG056) & 0xf8;
	write_radio_reg(pi, RADIO_2064_REG056, calnrst);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2064_REG056, calnrst | 0x03);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2064_REG056, calnrst | 0x07);
#if defined(DSLCPE_DELAY)
	OSL_YIELD_EXEC(pi->sh->osh, 300);
#else
	OSL_DELAY(300);
#endif
	mod_radio_reg(pi, RADIO_2064_REG057, 1 << 3, 0);
}

/* 4313A0 now has doubler option enabled, have recoded the RF registers's equations          */
/* Plan is to unify the 4336 channel tune function with 4313's once the new code is stable */
static void
wlc_lcnphy_radio_2064_channel_tune_4313(phy_info_t *pi, uint8 channel)
{
	uint i;
	const chan_info_2064_lcnphy_t *ci;
	uint8 rfpll_doubler = 0;
	uint8 pll_pwrup, pll_pwrup_ovr;
	fixed qFxtal, qFref, qFvco, qFcal;
	uint8  d15, d16, f16, e44, e45;
	uint32 div_int, div_frac, fvco3, fpfd, fref3, fcal_div;
	uint16 loop_bw, d30, setCount;
	if (NORADIO_ENAB(pi->pubpi))
		return;
	ci = &chan_info_2064_lcnphy[0];
	rfpll_doubler = 1;
	/* doubler enable */
	mod_radio_reg(pi, RADIO_2064_REG09D, 0x4, 0x1 << 2);
	/* reduce crystal buffer strength */
	write_radio_reg(pi, RADIO_2064_REG09E, 0xf);
	if (!rfpll_doubler) {
		loop_bw = PLL_2064_LOOP_BW;
		d30 = PLL_2064_D30;
	} else {
		loop_bw = PLL_2064_LOOP_BW_DOUBLER;
		d30 = PLL_2064_D30_DOUBLER;
	}
	/* lookup radio-chip-specific channel code */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2064_lcnphy); i++)
			if (chan_info_2064_lcnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2064_lcnphy)) {
			PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				pi->sh->unit, __FUNCTION__, channel));
			return;
		}

		ci = &chan_info_2064_lcnphy[i];
	}
	/* Radio tunables */
	/* <4:3> bits of REG02A are for logen_buftune */
	write_radio_reg(pi, RADIO_2064_REG02A, ci->logen_buftune);
	/* <1:0> bits of REG030 are for logen_rccr_tx */
	mod_radio_reg(pi, RADIO_2064_REG030, 0x3, ci->logen_rccr_tx);
	/* <1:0> bits of REG091 are for txrf_mix_tune_ctrl */
	mod_radio_reg(pi, RADIO_2064_REG091, 0x3, ci->txrf_mix_tune_ctrl);
	/* <3:0> bits of REG038 are for pa_INPUT_TUNE_G */
	mod_radio_reg(pi, RADIO_2064_REG038, 0xf, ci->pa_input_tune_g);
	/* <3:2> bits of REG030 are for logen_rccr_rx */
	mod_radio_reg(pi, RADIO_2064_REG030, 0x3 << 2, (ci->logen_rccr_rx) << 2);
	/* <3:0> bits of REG05E are for rxrf_lna1_freq_tune */
	mod_radio_reg(pi, RADIO_2064_REG05E, 0xf, ci->pa_rxrf_lna1_freq_tune);
	/* <7:4> bits of REG05E are for rxrf_lna2_freq_tune */
	mod_radio_reg(pi, RADIO_2064_REG05E, (0xf)<<4, (ci->pa_rxrf_lna2_freq_tune)<<4);
	/* <7:0> bits of REG06C are for rxrf_rxrf_spare1:trsw tune */
	write_radio_reg(pi, RADIO_2064_REG06C, ci->rxrf_rxrf_spare1);
	/* Turn on PLL power supplies */
	/* to be used later to restore states */
	pll_pwrup = (uint8)read_radio_reg(pi, RADIO_2064_REG044);
	pll_pwrup_ovr = (uint8)read_radio_reg(pi, RADIO_2064_REG12B);
	/* <2:0> bits of REG044 are pll monitor,synth and vco pwrup */
	or_radio_reg(pi, RADIO_2064_REG044, 0x07);
	/* <3:1> bits of REG12B are for the overrides of these pwrups */
	or_radio_reg(pi, RADIO_2064_REG12B, (0x07)<<1);
	e44 = 0;
	e45 = 0;
	/* Calculate various input frequencies */
	fpfd = rfpll_doubler ? (pi->xtalfreq << 1) : (pi->xtalfreq);
	if (pi->xtalfreq > 26000000)
		e44 = 1;
	if (pi->xtalfreq > 52000000)
		e45 = 1;
	if (e44 == 0)
		fcal_div = 1;
	else if (e45 == 0)
		fcal_div = 2;
	else
		fcal_div = 4;
	fvco3 = (ci->freq * 3); /* VCO frequency is 1.5 times the LO freq */
	fref3 = 2 * fpfd;
	/* Convert into Q16 MHz */
	qFxtal = wlc_lcnphy_qdiv_roundup(pi->xtalfreq, PLL_2064_MHZ, 16);
	qFref =  wlc_lcnphy_qdiv_roundup(fpfd, PLL_2064_MHZ, 16);
	qFcal = pi->xtalfreq * fcal_div/PLL_2064_MHZ;
	qFvco = wlc_lcnphy_qdiv_roundup(fvco3, 2, 16);

	/* PLL_delayBeforeOpenLoop */
	write_radio_reg(pi, RADIO_2064_REG04F, 0x02);

	/* PLL_enableTimeout */
	d15 = (pi->xtalfreq * fcal_div* 4/5)/PLL_2064_MHZ - 1;
	write_radio_reg(pi, RADIO_2064_REG052, (0x07 & (d15 >> 2)));
	write_radio_reg(pi, RADIO_2064_REG053, (d15 & 0x3) << 5);
	/* PLL_cal_ref_timeout */
	d16 = (qFcal * 8 /(d15 + 1)) - 1;
	write_radio_reg(pi, RADIO_2064_REG051, d16);

	/* PLL_calSetCount */
	f16 = ((d16 + 1)* (d15 + 1))/qFcal;
	setCount = f16 * 3 * (ci->freq)/32 - 1;
	mod_radio_reg(pi, RADIO_2064_REG053, (0x0f << 0), (uint8)(setCount >> 8));
	/* 4th bit is on if using VCO cal */
	or_radio_reg(pi, RADIO_2064_REG053, 0x10);
	write_radio_reg(pi, RADIO_2064_REG054, (uint8)(setCount & 0xff));
	/* Divider, integer bits */
	div_int = ((fvco3 * (PLL_2064_MHZ >> 4)) / fref3) << 4;

	/* Divider, fractional bits */
	div_frac = ((fvco3 * (PLL_2064_MHZ >> 4)) % fref3) << 4;
	while (div_frac >= fref3) {
		div_int++;
		div_frac -= fref3;
	}
	div_frac = wlc_lcnphy_qdiv_roundup(div_frac, fref3, 20);
	/* Program PLL */
	mod_radio_reg(pi, RADIO_2064_REG045, (0x1f << 0), (uint8)(div_int >> 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x1f << 4), (uint8)(div_int << 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x0f << 0), (uint8)(div_frac >> 16));
	write_radio_reg(pi, RADIO_2064_REG047, (uint8)(div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2064_REG048, (uint8)div_frac & 0xff);

	/* Fixed RC values */
	/* REG040 <7:4> is PLL_lf_c1, <3:0> is PLL_lf_c2 */
	write_radio_reg(pi, RADIO_2064_REG040, 0xfb);
	/* REG041 <7:4> is PLL_lf_c3, <3:0> is PLL_lf_c4 */
	write_radio_reg(pi, RADIO_2064_REG041, 0x9A);
	write_radio_reg(pi, RADIO_2064_REG042, 0xA3);
	write_radio_reg(pi, RADIO_2064_REG043, 0x0C);
	/* PLL_cp_current */
	{
		uint8 h29, h23, c28, d29, h28_ten, e30, h30_ten, cp_current;
		uint16 c29, c38, c30, g30, d28;
		c29 = loop_bw;
		d29 = 200; /* desired loop bw value */
		c38 = 1250;
		h29 = d29/c29; /* change this logic if loop bw is > 200 */
		h23 = 1; /* Ndiv ratio */
		c28 = 30; /* Kvco MHz/V */
		d28 = (((PLL_2064_HIGH_END_KVCO - PLL_2064_LOW_END_KVCO)*
			(fvco3/2 - PLL_2064_LOW_END_VCO))/
			(PLL_2064_HIGH_END_VCO - PLL_2064_LOW_END_VCO))
			+ PLL_2064_LOW_END_KVCO;
		h28_ten = (d28*10)/c28;
		c30 = 2640;
		e30 = (d30 - 680)/490;
		g30 = 680 + (e30 * 490);
		h30_ten = (g30*10)/c30;
		cp_current = ((c38*h29*h23*100)/h28_ten)/h30_ten;
		mod_radio_reg(pi, RADIO_2064_REG03C, 0x3f, cp_current);
	}
	if (channel >= 1 && channel <= 5)
		write_radio_reg(pi, RADIO_2064_REG03C, 0x8);
	else
		write_radio_reg(pi, RADIO_2064_REG03C, 0x7);

	/* Radio reg values set for reducing phase noise */
	if (channel < 4)
		write_radio_reg(pi, RADIO_2064_REG03D, 0xa);
	else if (channel < 8)
		write_radio_reg(pi, RADIO_2064_REG03D, 0x9);
	else
		write_radio_reg(pi, RADIO_2064_REG03D, 0x8);

	/* o_wrf_jtag_pll_cp_reset and o_wrf_jtag_pll_vco_reset (4:3) */
	mod_radio_reg(pi, RADIO_2064_REG044, 0x0c, 0x0c);
	OSL_DELAY(1);
	/* Force VCO cal */
	wlc_2064_vco_cal(pi);
	/* Restore state */
	write_radio_reg(pi, RADIO_2064_REG044, pll_pwrup);
	write_radio_reg(pi, RADIO_2064_REG12B, pll_pwrup_ovr);
	if (LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		write_radio_reg(pi, RADIO_2064_REG038, 0);
		write_radio_reg(pi, RADIO_2064_REG091, 7);
	}
}

static void
wlc_lcnphy_radio_2064_channel_tune(phy_info_t *pi, uint8 channel)
{
	uint i;
	void *chi;
	uint8 rfpll_doubler = 0;
	uint8 pll_pwrup, pll_pwrup_ovr;
	fixed qFxtal, qFref, qFvco, qFcal, qVco, qVal;
	uint8  to, refTo, cp_current, kpd_scale, ioff_scale, offset_current, e44, e45;
	uint32 setCount, div_int, div_frac, iVal, fvco3, fref, fref3, fcal_div;
	uint8 reg_h5e_val = 0;
	uint8 reg_h5e_val_tbl[15] = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0x99, 0x88, 0x77,
		0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* lookup radio-chip-specific channel code */
	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2066_lcnphy); i++)
			if (chan_info_2066_lcnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2066_lcnphy)) {
			PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				pi->sh->unit, __FUNCTION__, channel));
			return;
		}
		chi = &chan_info_2066_lcnphy[i];
	} else {
		for (i = 0; i < ARRAYSIZE(chan_info_2064_lcnphy); i++)
			if (chan_info_2064_lcnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2064_lcnphy)) {
			PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				pi->sh->unit, __FUNCTION__, channel));
			return;
		}
		chi = &chan_info_2064_lcnphy[i];
	}

	if (LCNREV_IS(pi->pubpi.phy_rev, 2)) {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			/* 5G Radio tunables */
			chan_info_2066_a_lcnphy_t *ci = (chan_info_2066_a_lcnphy_t *)chi;
			/* <0:3> bits of REG0AF are for logen_buftune */
			mod_radio_reg(pi, RADIO_2064_REG0AF, 0xf, ci->logen_buftune);
			/* REG0B0 are for ctune */
			write_radio_reg(pi, RADIO_2064_REG0B0, ci->ctune);
			/* REG0BA are for rccr */
			write_radio_reg(pi, RADIO_2064_REG0BA, ci->logen_rccr);
			/* <3:0> bits of REG0BE are for mix_tune_5g */
			mod_radio_reg(pi, RADIO_2064_REG0BE, 0xf, ci->mix_tune_5g);
			/* <3:0> bits of REG0CF are for pga_INPUT_TUNE_G */
			mod_radio_reg(pi, RADIO_2064_REG0CF, 0xf, ci->pga_tune_ctrl);
			/* <3:0> bits of REG0C9 are for pa_INPUT_TUNE_G */
			mod_radio_reg(pi, RADIO_2064_REG0C9, 0xf, ci->pa_input_tune_5g);
			/* <3:0> bits of REG05E are for rxrf_lna1_freq_tune */
			write_radio_reg(pi, RADIO_2064_REG0AD, ci->pa_rxrf_lna1_freq_tune);
			/* <7:0> bits of REG06C are for rxrf_rxrf_spare1:trsw tune */
			mod_radio_reg(pi, RADIO_2064_REG0D4, 0xff, ci->rxrf_rxrf_spare1);
			write_radio_reg(pi, RADIO_2064_REG0D3, 1);
		} else {
			/* Radio tunables */
			chan_info_2066_lcnphy_t *ci = (chan_info_2066_lcnphy_t *)chi;
			/* <4:3> bits of REG02A are for logen_buftune */
			mod_radio_reg(pi, RADIO_2064_REG02A, 0x18, (ci->logen_buftune) << 3);
			/* <1:0> bits of REG030 are for logen_rccr_tx */
			write_radio_reg(pi, RADIO_2064_REG030, ci->logen_rccr_tx);
			/* <1:0> bits of REG091 are for txrf_mix_tune_ctrl */
			mod_radio_reg(pi, RADIO_2064_REG091, 0x3, ci->txrf_mix_tune_ctrl);
			/* <3:0> bits of REG038 are for pa_INPUT_TUNE_G */
			mod_radio_reg(pi, RADIO_2064_REG038, 0xf, ci->pa_input_tune_g);
			/* <3:0> bits of REG05E are for rxrf_lna1_freq_tune */
			write_radio_reg(pi, RADIO_2064_REG05E, ci->pa_rxrf_lna1_freq_tune);
			/* <7:0> bits of REG06C are for rxrf_rxrf_spare1:trsw tune */
			write_radio_reg(pi, RADIO_2064_REG06C, ci->rxrf_rxrf_spare1);
		}
		if (pi->sh->chiprev == 0) {
			/* mixer and PAD tuning, to match TCL */
			write_radio_reg(pi, RADIO_2064_REG038, 0);
			write_radio_reg(pi, RADIO_2064_REG091, 3);
		}

	} else {
		chan_info_2064_lcnphy_t *ci = (chan_info_2064_lcnphy_t *)chi;
		/* Radio tunables */
		/* <4:3> bits of REG02A are for logen_buftune */
		write_radio_reg(pi, RADIO_2064_REG02A, ci->logen_buftune);
		/* <1:0> bits of REG030 are for logen_rccr_tx */
		mod_radio_reg(pi, RADIO_2064_REG030, 0x3, ci->logen_rccr_tx);
		/* <1:0> bits of REG091 are for txrf_mix_tune_ctrl */
		mod_radio_reg(pi, RADIO_2064_REG091, 0x3, ci->txrf_mix_tune_ctrl);
		/* <3:0> bits of REG038 are for pa_INPUT_TUNE_G */
		mod_radio_reg(pi, RADIO_2064_REG038, 0xf, ci->pa_input_tune_g);

		/* Tx tuning - Lookup table does not have correct values for these */
		write_radio_reg(pi, RADIO_2064_REG091, 0x00); /* Mixer Tuning */
		write_radio_reg(pi, RADIO_2064_REG038, 0x07); /* PAD tuning */

		/* <3:2> bits of REG030 are for logen_rccr_rx */
		mod_radio_reg(pi, RADIO_2064_REG030, 0x3 << 2, (ci->logen_rccr_rx) << 2);
		/* <3:0> bits of REG05E are for rxrf_lna1_freq_tune */
		mod_radio_reg(pi, RADIO_2064_REG05E, 0xf, ci->pa_rxrf_lna1_freq_tune);
		/* <7:4> bits of REG05E are for rxrf_lna2_freq_tune */
		mod_radio_reg(pi, RADIO_2064_REG05E, (0xf)<<4, (ci->pa_rxrf_lna2_freq_tune)<<4);
		/* <7:0> bits of REG06C are for rxrf_rxrf_spare1:trsw tune */
		write_radio_reg(pi, RADIO_2064_REG06C, ci->rxrf_rxrf_spare1);


		/* 4336 wlbga, esp A0Vx tx tuning */
		if ((CHIPID(pi->sh->chip) == BCM4336_CHIP_ID) &&
			(pi->sh->chippkg == BCM4336_WLBGA_PKG_ID)) {
			write_radio_reg(pi, RADIO_2064_REG038, 0);
			write_radio_reg(pi, RADIO_2064_REG091, 3);

			reg_h5e_val = reg_h5e_val_tbl[channel-1];
			write_radio_reg(pi, RADIO_2064_REG05E, reg_h5e_val);
		}
		/* internal trsw programming for 4336A0Vx */
		write_radio_reg(pi, RADIO_2064_REG07E, 1);
	}

	/* <2:0> bits of REG02A are for o_wrf_jtag_logen_idac_gm */
	mod_radio_reg(pi, RADIO_2064_REG02A, 0x7, 7);
	/* logen tuning */
	write_radio_reg(pi, RADIO_2064_REG02C, 0);

	/* increase current to ADC to prevent problem with clipping */
	/* <3:2> bits of REG0F3 are for o_wrf_jtag_afe_iqadc_f1_ctl */
	/* new ADC clipping WAR */
	if (1) {
		mod_radio_reg(pi, RADIO_2064_REG11F, 0x2, 1 << 1);
		mod_radio_reg(pi, RADIO_2064_REG0F7, 0x8, 1 << 3);
		mod_radio_reg(pi, RADIO_2064_REG0F1, 0x3, 0);
		mod_radio_reg(pi, RADIO_2064_REG0F2, 0xF8, (2 << 3) | (2 << 6));
		mod_radio_reg(pi, RADIO_2064_REG0F3, 0xFF, (2 << 6) | (2 << 4) | 2);
	} else
		mod_radio_reg(pi, RADIO_2064_REG0F3, 0x3 << 2, 1 << 2);

	/* Turn on PLL power supplies */
	/* to be used later to restore states */
	pll_pwrup = (uint8)read_radio_reg(pi, RADIO_2064_REG044);
	pll_pwrup_ovr = (uint8)read_radio_reg(pi, RADIO_2064_REG12B);
	/* <2:0> bits of REG044 are pll monitor,synth and vco pwrup */
	or_radio_reg(pi, RADIO_2064_REG044, 0x07);
	/* <3:1> bits of REG12B are for the overrides of these pwrups */
	or_radio_reg(pi, RADIO_2064_REG12B, (0x07)<<1);
	e44 = 0;
	e45 = 0;
	/* Calculate various input frequencies */
	fref = rfpll_doubler ? (pi->xtalfreq << 1) : (pi->xtalfreq);
	if (pi->xtalfreq > 26000000)
		e44 = 1;
	if (pi->xtalfreq > 52000000)
		e45 = 1;
	if (e44 == 0)
		fcal_div = 1;
	else if (e45 == 0)
		fcal_div = 2;
	else
		fcal_div = 4;
	fvco3 = ((chan_info_2064_lcnphy_t *)chi)->freq * 3;

	fref3 = 2 * fref;

	/* Convert into Q16 MHz */
	qFxtal = wlc_lcnphy_qdiv_roundup(pi->xtalfreq, PLL_2064_MHZ, 16);
	qFref =  wlc_lcnphy_qdiv_roundup(fref, PLL_2064_MHZ, 16);
	qFcal = wlc_lcnphy_qdiv_roundup(fref, fcal_div * PLL_2064_MHZ, 16);
	qFvco = wlc_lcnphy_qdiv_roundup(fvco3, 2, 16);

	/* PLL_delayBeforeOpenLoop */
	write_radio_reg(pi, RADIO_2064_REG04F, 0x02);

	/* PLL_enableTimeout */
	to = (uint8)((((fref * PLL_2064_CAL_REF_TO) /
		(PLL_2064_OPEN_LOOP_DELAY * fcal_div * PLL_2064_MHZ)) + 1) >> 1) - 1;
	mod_radio_reg(pi, RADIO_2064_REG052, (0x07 << 0), to >> 2);
	mod_radio_reg(pi, RADIO_2064_REG053, (0x03 << 5), to << 5);


	/* PLL_cal_ref_timeout */
	refTo = (uint8)((((fref * PLL_2064_CAL_REF_TO) / (fcal_div * (to + 1))) +
		(PLL_2064_MHZ - 1)) / PLL_2064_MHZ) - 1;
	write_radio_reg(pi, RADIO_2064_REG051, refTo);

	/* PLL_calSetCount */
	setCount = (uint32)FLOAT(
		(fixed)wlc_lcnphy_qdiv_roundup(qFvco, qFcal * 16, 16) * (refTo + 1) * (to + 1)) - 1;
	mod_radio_reg(pi, RADIO_2064_REG053, (0x0f << 0), (uint8)(setCount >> 8));
	write_radio_reg(pi, RADIO_2064_REG054, (uint8)(setCount & 0xff));
	/* Divider, integer bits */
	div_int = ((fvco3 * (PLL_2064_MHZ >> 4)) / fref3) << 4;

	/* Divider, fractional bits */
	div_frac = ((fvco3 * (PLL_2064_MHZ >> 4)) % fref3) << 4;
	while (div_frac >= fref3) {
		div_int++;
		div_frac -= fref3;
	}
	div_frac = wlc_lcnphy_qdiv_roundup(div_frac, fref3, 20);
	/* Program PLL */
	mod_radio_reg(pi, RADIO_2064_REG045, (0x1f << 0), (uint8)(div_int >> 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x1f << 4), (uint8)(div_int << 4));
	mod_radio_reg(pi, RADIO_2064_REG046, (0x0f << 0), (uint8)(div_frac >> 16));
	write_radio_reg(pi, RADIO_2064_REG047, (uint8)(div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2064_REG048, (uint8)div_frac & 0xff);

	/* Fixed RC values */
	/* REG040 <7:4> is PLL_lf_c1, <3:0> is PLL_lf_c2 */
	write_radio_reg(pi, RADIO_2064_REG040, 0xfb);
	/* REG041 <7:4> is PLL_lf_c3, <3:0> is PLL_lf_c4 */
	write_radio_reg(pi, RADIO_2064_REG041, 0x9A);
	/* REG042 <7:3> is PLL_lf_r1 , <2:0> is PLL_lf_r2 */
	write_radio_reg(pi, RADIO_2064_REG042, 0x7A);
	/* REG043 <6:5> is PLL_lf_r2, <4:0> is PLL_lf_r3 */
	write_radio_reg(pi, RADIO_2064_REG043, 0x29);

	/* PLL_cp_current */
	qVco = ((PLL_2064_HIGH_END_KVCO - PLL_2064_LOW_END_KVCO) *
		(qFvco - FIXED(PLL_2064_LOW_END_VCO)) /
		(PLL_2064_HIGH_END_VCO - PLL_2064_LOW_END_VCO)) +
		FIXED(PLL_2064_LOW_END_KVCO);
	iVal = ((PLL_2064_D30 - 680)  + (490 >> 1))/ 490;
	qVal = wlc_lcnphy_qdiv_roundup(
		880 * PLL_2064_LOOP_BW * div_int,
		27 * (68 + (iVal * 49)), 16);

	cp_current = (qVal + (qVco >> 1))/ qVco;

	kpd_scale = cp_current > 60 ? 1 : 0;
	if (kpd_scale)
		cp_current = (cp_current >> 1) - 4;
	else
		cp_current = cp_current - 4;

	mod_radio_reg(pi, RADIO_2064_REG03C, 0x3f, cp_current);
	/*  PLL_Kpd_scale2 */
	mod_radio_reg(pi, RADIO_2064_REG03C, 1 << 6, (kpd_scale << 6));

	/*  PLL_offset_current */
	qVal = wlc_lcnphy_qdiv_roundup(100 * qFref, qFvco, 16) * (cp_current + 4) * (kpd_scale + 1);
	ioff_scale = (qVal > FIXED(150)) ? 1 : 0;
	qVal = (qVal / (3 * (ioff_scale + 1))) - FIXED(2);
	if (qVal < 0)
		offset_current = 0;
	else
		offset_current = FLOAT(qVal + (FIXED(1) >> 1));

	mod_radio_reg(pi, RADIO_2064_REG03D, 0x1f, offset_current);
	write_radio_reg(pi, RADIO_2064_REG03D, 0x6);

	/*  PLL_ioff_scale2 */
	mod_radio_reg(pi, RADIO_2064_REG03D, 1 << 5, ioff_scale << 5);

	/* PLL_cal_xt_endiv */
	if (pi->xtalfreq > 26000000)
		mod_radio_reg(pi, RADIO_2064_REG057, 1 << 5, 1 << 5);
	else
		mod_radio_reg(pi, RADIO_2064_REG057, 1 << 5, 0 << 5);


	/* PLL_sel_short */
	if (qFref > FIXED(45))
		or_radio_reg(pi, RADIO_2064_REG04A, 0x02);
	else
		and_radio_reg(pi, RADIO_2064_REG04A, 0xfd);

	/* o_wrf_jtag_pll_cp_reset and o_wrf_jtag_pll_vco_reset (4:3) */
	mod_radio_reg(pi, RADIO_2064_REG044, 0x0c, 0x0c);
	OSL_DELAY(1);

	/* Force VCO cal */
	wlc_2064_vco_cal(pi);

	/* Restore state */
	write_radio_reg(pi, RADIO_2064_REG044, pll_pwrup);
	write_radio_reg(pi, RADIO_2064_REG12B, pll_pwrup_ovr);
}

bool
wlc_phy_tpc_isenabled_lcnphy(phy_info_t *pi)
{
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi))
		return 0;
	else
		return (LCNPHY_TX_PWR_CTRL_HW == wlc_lcnphy_get_tx_pwr_ctrl((pi)));
}

bool
wlc_phy_tpc_iovar_isenabled_lcnphy(phy_info_t *pi)
{
	if (CHIPID(pi->sh->chip) != BCM4313_CHIP_ID)
		return wlc_phy_tpc_isenabled_lcnphy(pi);
	else
		return 	((read_phy_reg((pi), LCNPHY_TxPwrCtrlCmd) &
			(LCNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			LCNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			LCNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK))
			== LCNPHY_TX_PWR_CTRL_HW);
}

void
wlc_lcnphy_iovar_txpwrctrl(phy_info_t *pi, int32 int_val, int32 *ret_int_ptr)
{
	uint16 pwrctrl;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)
		pwrctrl = int_val ?
			LCNPHY_TX_PWR_CTRL_TEMPBASED : LCNPHY_TX_PWR_CTRL_OFF;
	else
		pwrctrl = int_val ?
			LCNPHY_TX_PWR_CTRL_HW : LCNPHY_TX_PWR_CTRL_OFF;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);
	if (!int_val)
		wlc_lcnphy_set_tx_pwr_by_index(pi, LCNPHY_MAX_TX_POWER_INDEX);

	wlc_lcnphy_set_tx_pwr_ctrl(pi, pwrctrl);

	pi_lcn->lcnphy_full_cal_channel = 0;
	pi->phy_forcecal = TRUE;
	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}

/* %%%%%% major flow operations */
void
wlc_phy_txpower_recalc_target_lcnphy(phy_info_t *pi)
{
	uint16 pwr_ctrl;
	if (wlc_lcnphy_tempsense_based_pwr_ctrl_enabled(pi)) {
		wlc_lcnphy_calib_modes(pi, LCNPHY_PERICAL_TEMPBASED_TXPWRCTRL);
	} else if (wlc_lcnphy_tssi_based_pwr_ctrl_enabled(pi)) {
		/* Temporary disable power control to update settings */
		pwr_ctrl = wlc_lcnphy_get_tx_pwr_ctrl(pi);
		wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
		wlc_lcnphy_txpower_recalc_target(pi);
		/* Restore power control */
		wlc_lcnphy_set_tx_pwr_ctrl(pi, pwr_ctrl);
	} else
		return;
}
void
wlc_phy_detach_lcnphy(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_lcnphy, sizeof(phy_info_lcnphy_t));
}

bool
wlc_phy_attach_lcnphy(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn;

	pi->u.pi_lcnphy = (phy_info_lcnphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_lcnphy_t));
	if (pi->u.pi_lcnphy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_lcnphy, sizeof(phy_info_lcnphy_t));

	pi_lcn = pi->u.pi_lcnphy;

	if ((0 == (pi->sh->boardflags & BFL_NOPA)) && !NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	/* Get xtal frequency from PMU */
	pi->xtalfreq = si_alp_clock(pi->sh->sih);
	ASSERT(0 == (pi->xtalfreq % 1000));

	/* set papd_rxGnCtrl_init to 0 */
	pi_lcn->lcnphy_papd_rxGnCtrl_init = 0;

	PHY_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		pi->sh->unit, __FUNCTION__,
		pi->xtalfreq / 1000000, pi->xtalfreq % 1000000));

	pi->pi_fptr.init = wlc_phy_init_lcnphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_lcnphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_lcnphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_lcnphy;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = wlc_phy_lcnphy_long_train;
#endif
	pi->pi_fptr.txiqccget = wlc_lcnphy_get_tx_iqcc;
	pi->pi_fptr.txiqccset = wlc_lcnphy_set_tx_iqcc;
	pi->pi_fptr.txloccget = wlc_lcnphy_get_tx_locc;
	pi->pi_fptr.radioloftget = wlc_lcnphy_get_radio_loft;
#if defined(WLTEST)
	pi->pi_fptr.carrsuppr = wlc_phy_carrier_suppress_lcnphy;
	pi->pi_fptr.rxsigpwr = wlc_lcnphy_rx_signal_power;
#endif
	pi->pi_fptr.detach = wlc_phy_detach_lcnphy;

	if (!wlc_phy_txpwr_srom_read_lcnphy(pi))
		return FALSE;
	/* Tx Pwr Ctrl */
	if ((pi->sh->boardflags & BFL_FEM) &&
		(CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) && LCNREV_IS(pi->pubpi.phy_rev, 1)) {
		if (pi_lcn->lcnphy_tempsense_option == 3) {
			pi->hwpwrctrl = TRUE;
			pi->hwpwrctrl_capable = TRUE;
			pi->temppwrctrl_capable = FALSE;
			PHY_INFORM(("TSSI based power control is enabled\n"));
		} else {
			pi->hwpwrctrl = FALSE;
			pi->hwpwrctrl_capable = FALSE;
			pi->temppwrctrl_capable = TRUE;
			PHY_INFORM(("Tempsense based power control is enabled\n"));
		}
	}

	return TRUE;
}

/* %%%%%% testing */
#if defined(BCMDBG) || defined(WLTEST)
int
wlc_phy_lcnphy_long_train(phy_info_t *pi, int channel)
{
	uint16 num_samps;
	phytbl_info_t tab;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);

	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		wlc_lcnphy_stop_tx_tone(pi);
		wlc_lcnphy_deaf_mode(pi, FALSE);
		return 0;
	}

	if (wlc_phy_test_init(pi, channel, TRUE)) {
		return 1;
	}

	wlc_lcnphy_deaf_mode(pi, TRUE);

	num_samps = sizeof(ltrn_list)/sizeof(*ltrn_list);
	/* load sample table */
	tab.tbl_ptr = ltrn_list;
	tab.tbl_len = num_samps;
	tab.tbl_id = LCNPHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 16;
	wlc_lcnphy_write_table(pi, &tab);

	wlc_lcnphy_run_samples(pi, num_samps, 0xffff, 0, 0);

	return 0;
}

void
wlc_phy_init_test_lcnphy(phy_info_t *pi)
{
	/* Force WLAN antenna */
	wlc_lcnphy_btcx_override_enable(pi);
	/* Disable tx power control */
	wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_OFF);
	/* Recalibrate for this channel */
	wlc_lcnphy_calib_modes(pi, PHY_FULLCAL);
}
#endif 

#if defined(WLTEST)
void
wlc_phy_carrier_suppress_lcnphy(phy_info_t *pi)
{
	if (ISLCNPHY(pi)) {
		wlc_lcnphy_reset_radio_loft(pi);
		if (wlc_phy_tpc_isenabled_lcnphy(pi))
			wlc_lcnphy_clear_tx_power_offsets(pi);
		else
			wlc_lcnphy_set_tx_locc(pi, 0);
	} else
		ASSERT(0);

}
static void
wlc_lcnphy_reset_radio_loft(phy_info_t *pi)
{
	write_radio_reg(pi, RADIO_2064_REG089, 0x88);
	write_radio_reg(pi, RADIO_2064_REG08A, 0x88);
	write_radio_reg(pi, RADIO_2064_REG08B, 0x88);
	write_radio_reg(pi, RADIO_2064_REG08C, 0x88);
}

#endif 

static void
wlc_lcnphy_set_rx_gain(phy_info_t *pi, uint32 gain)
{
	uint16 trsw, ext_lna, lna1, lna2, tia, biq0, biq1, gain0_15, gain16_19;

	trsw = (gain & ((uint32)1 << 28)) ? 0 : 1;
	ext_lna = (uint16)(gain >> 29) & 0x01;
	lna1 = (uint16)(gain >> 0) & 0x0f;
	lna2 = (uint16)(gain >> 4) & 0x0f;
	tia  = (uint16)(gain >> 8) & 0xf;
	biq0 = (uint16)(gain >> 12) & 0xf;
	biq1 = (uint16)(gain >> 16) & 0xf;
	/* 20 bit gaincontrol is constructed -  gain16_19 is biq1 index; */
	/* gain0_15 is constructed with biq0 in 15:12 bits; 11:8 has tia */
	/* lna2 is repeated at 7:6 and 5:4 , lna1 is repeated at 3:2 and 1:0 */
	gain0_15 = (uint16) ((lna1 & 0x3) | ((lna1 & 0x3) << 2) |
	((lna2 & 0x3) << 4) | ((lna2 & 0x3) << 6) | ((tia & 0xf) << 8) |
	((biq0 & 0xf) << 12));
	gain16_19 = biq1;
	/* override the values of afe components with calculated indices; */
	mod_phy_reg(pi, LCNPHY_RFOverrideVal0,
		LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		trsw << LCNPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_SHIFT);
	mod_phy_reg(pi, LCNPHY_rfoverride2val,
		LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_MASK,
		ext_lna << LCNPHY_rfoverride2val_gmode_ext_lna_gain_ovr_val_SHIFT);
	mod_phy_reg(pi, LCNPHY_rfoverride2val,
		LCNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_MASK,
		ext_lna << LCNPHY_rfoverride2val_amode_ext_lna_gain_ovr_val_SHIFT);
	mod_phy_reg(pi, LCNPHY_rxgainctrl0ovrval,
		LCNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_MASK,
		gain0_15 << LCNPHY_rxgainctrl0ovrval_rxgainctrl_ovr_val0_SHIFT);
	mod_phy_reg(pi, LCNPHY_rxlnaandgainctrl1ovrval,
		LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_MASK,
		gain16_19 << LCNPHY_rxlnaandgainctrl1ovrval_rxgainctrl_ovr_val1_SHIFT);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_phy_reg(pi, LCNPHY_rfoverride2val,
			LCNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_MASK,
			lna1 << LCNPHY_rfoverride2val_slna_gain_ctrl_ovr_val_SHIFT);
		mod_phy_reg(pi, LCNPHY_RFinputOverrideVal,
			LCNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_MASK,
			lna1 << LCNPHY_RFinputOverrideVal_wlslnagainctrl_ovr_val_SHIFT);
	}
	wlc_lcnphy_rx_gain_override_enable(pi, TRUE);
}

static uint32
wlc_lcnphy_get_receive_power(phy_info_t *pi, int32 *gain_index)
{
	uint32 received_power = 0;
	int32 max_index = 0;
	uint32 gain_code = 0;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	max_index = 36;
	if (*gain_index >= 0)
		gain_code = lcnphy_23bitgaincode_table[*gain_index];

	/* wlc_lcnphy_deaf_mode(pi, TRUE); */
	if (-1 == *gain_index) {
		*gain_index = 0;
		while ((*gain_index <= (int32)max_index) && (received_power < 700)) {
			wlc_lcnphy_set_rx_gain(pi, lcnphy_23bitgaincode_table[*gain_index]);
			received_power =
				wlc_lcnphy_measure_digital_power(pi, pi_lcn->lcnphy_noise_samples);
			(*gain_index) ++;
		}
		(*gain_index) --;
	}
	else {
		wlc_lcnphy_set_rx_gain(pi, gain_code);
		received_power = wlc_lcnphy_measure_digital_power(pi, pi_lcn->lcnphy_noise_samples);
	}

	/* wlc_lcnphy_deaf_mode(pi, FALSE); */
	return received_power;
}

int32
wlc_lcnphy_rx_signal_power(phy_info_t *pi, int32 gain_index)
{
	int32 gain = 0;
	int32 nominal_power_db;
	int32 log_val, gain_mismatch, desired_gain, input_power_offset_db, input_power_db;
	int32 received_power, temperature;
	uint freq;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	received_power = wlc_lcnphy_get_receive_power(pi, &gain_index);

	gain = lcnphy_gain_table[gain_index];

	nominal_power_db = read_phy_reg(pi, LCNPHY_VeryLowGainDB) >>
						   LCNPHY_VeryLowGainDB_NominalPwrDB_SHIFT;

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

	input_power_offset_db = read_phy_reg(pi, LCNPHY_InputPowerDB) & 0xFF;

	if (input_power_offset_db > 127)
		input_power_offset_db -= 256;

	input_power_db = input_power_offset_db - desired_gain;

	/* compensation from PHY team */
	/* includes path loss of 2dB from Murata connector to chip input */
	input_power_db = input_power_db + lcnphy_gain_index_offset_for_rssi[gain_index];
	/* Channel Correction Factor */
	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((freq > 2427) && (freq <= 2467))
		input_power_db = input_power_db - 1;

	/* temperature correction */
	temperature = pi_lcn->lcnphy_lastsensed_temperature;
	/* printf(" CW_RSSI Temp %d \n",temperature); */
	if ((temperature - 15) < -30) {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12) - 7;
	} else if ((temperature - 15) < 4) {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12) - 3;
	} else {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12);
	}

	wlc_lcnphy_rx_gain_override_enable(pi, 0);

	return input_power_db;
}


/* set tx digital filter coefficients */
static int
wlc_lcnphy_load_tx_iir_filter(phy_info_t *pi, bool is_ofdm, int16 filt_type)
{
	int16 filt_index = -1;
	int j;

	uint16 addr[] = {
		LCNPHY_ccktxfilt20Stg1Shft,
		LCNPHY_ccktxfilt20CoeffStg0A1,
		LCNPHY_ccktxfilt20CoeffStg0A2,
		LCNPHY_ccktxfilt20CoeffStg0B1,
		LCNPHY_ccktxfilt20CoeffStg0B2,
		LCNPHY_ccktxfilt20CoeffStg0B3,
		LCNPHY_ccktxfilt20CoeffStg1A1,
		LCNPHY_ccktxfilt20CoeffStg1A2,
		LCNPHY_ccktxfilt20CoeffStg1B1,
		LCNPHY_ccktxfilt20CoeffStg1B2,
		LCNPHY_ccktxfilt20CoeffStg1B3,
		LCNPHY_ccktxfilt20CoeffStg2A1,
		LCNPHY_ccktxfilt20CoeffStg2A2,
		LCNPHY_ccktxfilt20CoeffStg2B1,
		LCNPHY_ccktxfilt20CoeffStg2B2,
		LCNPHY_ccktxfilt20CoeffStg2B3
		};

	uint16 addr_ofdm[] = {
		LCNPHY_txfilt20Stg1Shft,
		LCNPHY_txfilt20CoeffStg0A1,
		LCNPHY_txfilt20CoeffStg0A2,
		LCNPHY_txfilt20CoeffStg0B1,
		LCNPHY_txfilt20CoeffStg0B2,
		LCNPHY_txfilt20CoeffStg0B3,
		LCNPHY_txfilt20CoeffStg1A1,
		LCNPHY_txfilt20CoeffStg1A2,
		LCNPHY_txfilt20CoeffStg1B1,
		LCNPHY_txfilt20CoeffStg1B2,
		LCNPHY_txfilt20CoeffStg1B3,
		LCNPHY_txfilt20CoeffStg2A1,
		LCNPHY_txfilt20CoeffStg2A2,
		LCNPHY_txfilt20CoeffStg2B1,
		LCNPHY_txfilt20CoeffStg2B2,
		LCNPHY_txfilt20CoeffStg2B3
		};

	if (!is_ofdm) {
		for (j = 0; j < LCNPHY_NUM_TX_DIG_FILTERS_CCK; j++) {
			if (filt_type == LCNPHY_txdigfiltcoeffs_cck[j][0]) {
				filt_index = (int16)j;
				break;
			}
		}

		if (filt_index == -1) {
			ASSERT(FALSE);
		} else {
			for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
				write_phy_reg(pi, addr[j],
					LCNPHY_txdigfiltcoeffs_cck[filt_index][j+1]);
			}
		}
	}
	else {
		for (j = 0; j < LCNPHY_NUM_TX_DIG_FILTERS_OFDM; j++) {
			if (filt_type == LCNPHY_txdigfiltcoeffs_ofdm[j][0]) {
				filt_index = (int16)j;
				break;
			}
		}

		if (filt_index == -1) {
			ASSERT(FALSE);
		} else {
			for (j = 0; j < LCNPHY_NUM_DIG_FILT_COEFFS; j++) {
				write_phy_reg(pi, addr_ofdm[j],
					LCNPHY_txdigfiltcoeffs_ofdm[filt_index][j+1]);
			}
		}
	}

	return (filt_index != -1) ? 0 : -1;
}


/* ********************** NOISE CAL ********************************* */

#define k_noise_cal_gain_adj_threshold 3
#define k_noise_cal_max_gain_delta 3
#define k_noise_cal_ref_level 5000
#define k_noise_cal_bias_p4_high_level 4000
#define k_noise_cal_bias_m2_low_level 4000
#define k_noise_cal_bias_m1_low_level 3000
#define k_noise_cal_max_input_pwr 8
#define k_noise_cal_min_input_pwr -2
#define k_noise_cal_timeout 5000
#define k_noise_cal_min_check_cnt 4

#if LCNPHY_NOISE_DBG_HISTORY > 0

static void
wlc_lcnphy_noise_reset_log(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi_lcn->noise.dbg_idx = 0;
	pi_lcn->noise.dbg_dump_idx = 0;
	pi_lcn->noise.dbg_dump_sub_idx = 0;
	bzero((void*)pi_lcn->noise.dbg_info, sizeof(pi_lcn->noise.dbg_info));
	pi_lcn->noise.dbg_dump_cmd = 0;
}

static void
wlc_lcnphy_noise_advance_log(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	int8 idx = pi_lcn->noise.dbg_idx;

	if (pi_lcn->noise.dbg_dump_cmd)
		return;

	pi_lcn->noise.dbg_cnt++;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-20] = (uint16)pi_lcn->noise.dbg_cnt;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-19] =
		(uint16)pi_lcn->noise.dbg_starts;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-18] = (uint16)pi_lcn->noise.time;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-17] =
		(uint16)CHSPEC_CHANNEL(pi->radio_chanspec);
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-16] = (uint16)pi_lcn->noise.state;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-3] = pi_lcn->noise.dbg_watchdog_cnt;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-2] = pi_lcn->noise.dbg_taint_cnt;

	idx = (idx + 1) % LCNPHY_NOISE_DBG_HISTORY;
	bzero((void*)&pi_lcn->noise.dbg_info[idx],
		(sizeof(pi_lcn->noise.dbg_info)/LCNPHY_NOISE_DBG_HISTORY));
	pi_lcn->noise.dbg_idx = idx;
}

static void
wlc_lcnphy_noise_log_update(phy_info_t *pi, uint16 noise_cnt, uint16 noise, int16 gain)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	int8 idx = pi_lcn->noise.dbg_idx;

	if (pi_lcn->noise.dbg_dump_cmd)
		return;

	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-4] =
		pi_lcn->noise.init_noise; /* log initial value */
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-15] = (uint16)noise_cnt;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-14] = (uint16)noise;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-13]  = (uint16)gain;
}

static void
wlc_lcnphy_noise_log_adj(phy_info_t *pi, int16 adj, int16 gain,
	int16 po, bool gain_change, bool po_change)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	int8 idx = pi_lcn->noise.dbg_idx;
	uint16 timestamp;

	timestamp = (uint16)(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow) >> 10) & 0xffff;
	timestamp = (timestamp - pi_lcn->noise.start_time);

	if (pi_lcn->noise.dbg_dump_cmd)
		return;

	if (gain_change) {
		pi_lcn->noise.dbg_gain_adj_cnt++;
		pi_lcn->noise.dbg_gain_adj_time = timestamp;
	}
	if (po_change) {
		pi_lcn->noise.dbg_input_pwr_adj_cnt++;
		pi_lcn->noise.dbg_input_pwr_adj_time = timestamp;
	}
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-12] = (uint16)adj;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-11] = (uint16)k_noise_cal_ref_level;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-10] = (uint16)gain;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-9] = (uint16)po;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-8] =
		(uint16)pi_lcn->noise.dbg_gain_adj_cnt;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-7] =
		(uint16)pi_lcn->noise.dbg_gain_adj_time;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-6] =
		(uint16)pi_lcn->noise.dbg_input_pwr_adj_cnt;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-5] =
		(uint16)pi_lcn->noise.dbg_input_pwr_adj_time;
	pi_lcn->noise.dbg_info[idx][LCNPHY_NOISE_DBG_DATA_LEN-1] = pi_lcn->noise.err_cnt;
}

static void
wlc_lcnphy_noise_dump_log(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	int8 idx = (pi_lcn->noise.dbg_idx + 1) % LCNPHY_NOISE_DBG_HISTORY;
	char c = '>';
	while (idx != pi_lcn->noise.dbg_idx) {
	printf("Noise: %c %3d %3d %5d %5d %1d %5d %5d %3d %2d %4d"
		"%3d %2d %3d %5d %3d %5d %4d %3d %3d %3d\n",
		c,
		pi_lcn->noise.dbg_info[idx][0], /* dbg cnt */
		pi_lcn->noise.dbg_info[idx][1], /* starts */
		pi_lcn->noise.dbg_info[idx][2], /* time */
		pi_lcn->noise.dbg_info[idx][3], /* channel */
		pi_lcn->noise.dbg_info[idx][4], /* state */
		pi_lcn->noise.dbg_info[idx][5], /* noise cnt */
		pi_lcn->noise.dbg_info[idx][6], /* noise metric */
		(int16)pi_lcn->noise.dbg_info[idx][7], /* noise gain */
		(int16)pi_lcn->noise.dbg_info[idx][8], /* adj */
		(int16)pi_lcn->noise.dbg_info[idx][9], /* ref_level */
		(int16)pi_lcn->noise.dbg_info[idx][10], /* adj gain */
		(int16)pi_lcn->noise.dbg_info[idx][11], /* adj po */
		(int16)pi_lcn->noise.dbg_info[idx][12], /* adj gain cnt */
		(int16)pi_lcn->noise.dbg_info[idx][13], /* adj gain time */
		(int16)pi_lcn->noise.dbg_info[idx][14], /* adj po cnt */
		(int16)pi_lcn->noise.dbg_info[idx][15], /* adj po time */
		(int16)pi_lcn->noise.dbg_info[idx][16], /* initial noise measurement */
		(int16)pi_lcn->noise.dbg_info[idx][17], /* watchdog cnt */
		(int16)pi_lcn->noise.dbg_info[idx][18], /* taint cnt */
		(int16)pi_lcn->noise.dbg_info[idx][19]); /* err cnt */
	c = ' ';
	idx = (idx + 1) % LCNPHY_NOISE_DBG_HISTORY;
	}
}

static bool
wlc_lcnphy_noise_log_dump_active(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	return pi_lcn->noise.dbg_dump_cmd;
}

static uint32
wlc_lcnphy_noise_log_data(phy_info_t *pi)
{
	uint32 datum;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	int8 idx, sub_idx;

	if (!pi_lcn->noise.dbg_dump_cmd)
		return 0xdeadbeef;

	idx = pi_lcn->noise.dbg_dump_idx;
	sub_idx = pi_lcn->noise.dbg_dump_sub_idx;
	datum  = (uint32)pi_lcn->noise.dbg_info[idx][sub_idx];
	datum |= (uint32)(sub_idx << 16);
	datum |= (uint32)(idx << 24);
	sub_idx++;
	if (sub_idx > (LCNPHY_NOISE_DBG_DATA_LEN-1))
	{
		sub_idx = 0;
		idx--;
		if (idx < 0)
			idx += LCNPHY_NOISE_DBG_HISTORY;
	}
	pi_lcn->noise.dbg_dump_idx = idx;
	pi_lcn->noise.dbg_dump_sub_idx = sub_idx;

	return datum;
}

static bool
wlc_lcnphy_noise_log_ioctl(phy_info_t *pi, uint32 flag)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	bool retval = FALSE;

	if (flag & 1)
	{
		pi_lcn->noise.dbg_dump_cmd = 1;
		pi_lcn->noise.dbg_dump_idx = pi_lcn->noise.dbg_idx - 1;
		if (pi_lcn->noise.dbg_dump_idx < 0)
		pi_lcn->noise.dbg_dump_idx += LCNPHY_NOISE_DBG_HISTORY;
		pi_lcn->noise.dbg_dump_sub_idx = 0;
		retval = TRUE;
	}
	else if (flag & 2)
	{
		wlc_lcnphy_noise_dump_log(pi);
		retval = TRUE;
	}
	else if (flag & 4)
	{
		pi_lcn->noise.dbg_dump_cmd = 0;
		retval = TRUE;
	}
	return retval;
}

static void
wlc_lcnphy_noise_log_start(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi_lcn->noise.dbg_starts++;
	pi_lcn->noise.dbg_gain_adj_cnt = 0;
	pi_lcn->noise.dbg_gain_adj_time = 0;
	pi_lcn->noise.dbg_input_pwr_adj_cnt = 0;
	pi_lcn->noise.dbg_input_pwr_adj_time = 0;
	pi_lcn->noise.dbg_watchdog_cnt = 0;
	pi_lcn->noise.dbg_taint_cnt = 0;
}

static void
wlc_lcnphy_noise_log_tainted(phy_info_t *pi, bool tainted)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi_lcn->noise.dbg_watchdog_cnt++;
	if (tainted)
		pi_lcn->noise.dbg_taint_cnt++;
}

#else

#define wlc_lcnphy_noise_reset_log(a)
#define wlc_lcnphy_noise_log_update(a, b, c, d)
#define wlc_lcnphy_noise_log_adj(a, b, c, d, e, f)
#define wlc_lcnphy_noise_dump_log(a)
#define wlc_lcnphy_noise_log_dump_active(a) FALSE
#define wlc_lcnphy_noise_log_data(a) 0
#define wlc_lcnphy_noise_log_ioctl(a, b) ((b))
#define wlc_lcnphy_noise_log_start(a)
#define wlc_lcnphy_noise_log_tainted(a, b)
#define wlc_lcnphy_noise_advance_log(a)

#endif /* LCNPHY_NOISE_DBG_HISTORY > 0 */

#define k_noise_active_flag  (1<<12)
#define k_noise_init_flag    (1<<13)
#define k_noise_sync_active_mask    (0xf << 12)
#define k_noise_sync_idle_mask      (1 << 12)
#define k_noise_end_state 0
#define k_noise_adj_state 1
#define k_noise_check_state 2
#define k_noise_n_samps (100)
#define k_noise_min_noise_cnt 1
#define k_noise_sync_us_per_tick 20
/* k_noise_sync_stop_timeout: in k_noise_sync_us_per_tick units */
#define k_noise_sync_stop_timeout 25


#define k_noise_adj_tbl_n 6
const uint16 k_noise_adj_tbl[k_noise_adj_tbl_n] = {
	128,
	181,
	228,
	322,
	406,
	512,
};

static uint16
wlc_lcnphy_noise_get_reg(phy_info_t *pi, uint16 reg)
{
	uint16 lcnphy_shm_ptr;

	lcnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	return wlapi_bmac_read_shm(pi->sh->physhim,
		2*(lcnphy_shm_ptr + reg));
}

static void
wlc_lcnphy_noise_set_reg(phy_info_t *pi, uint16 reg, uint16 val)
{
	uint16 lcnphy_shm_ptr;

	lcnphy_shm_ptr = wlapi_bmac_read_shm(pi->sh->physhim, M_SSLPNPHYREGS_PTR);
	wlapi_bmac_write_shm(pi->sh->physhim,
		2*(lcnphy_shm_ptr + reg),
		val);

}


static bool
wlc_lcnphy_noise_sync_ucode(phy_info_t *pi, int timeout, int* p_timeout)
{
	bool ucode_sync;
	uint16 status, ucode_status, mask;

	ucode_sync = FALSE;
	status = wlc_lcnphy_noise_get_reg(pi, M_NOISE_CAL_CMD);
	if (status & k_noise_active_flag)
		mask = k_noise_sync_active_mask;
	else
		mask = k_noise_sync_idle_mask;

		/* Sync w/ ucode */
	do {
		ucode_status = wlc_lcnphy_noise_get_reg(pi, M_NOISE_CAL_RSP);
		if (((status ^ ucode_status) & mask) == 0)
			{
			ucode_sync = TRUE;
			break;
		}
		if (timeout-- > 0)
			OSL_DELAY(k_noise_sync_us_per_tick);
	} while (timeout > 0);

	if (p_timeout)
		*p_timeout = timeout;

	return ucode_sync;
}

static bool
wlc_lcnphy_noise_ucode_is_active(phy_info_t *pi)
{
	if (wlc_lcnphy_noise_get_reg(pi, M_NOISE_CAL_RSP) & k_noise_active_flag)
		return TRUE;
	else
		return FALSE;
}

static void
wlc_lcnphy_noise_ucode_ctrl(phy_info_t *pi, bool enable, bool init)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint16 ctrl;

	ctrl = wlc_lcnphy_noise_get_reg(pi, M_NOISE_CAL_CMD);

	if (enable && pi_lcn->noise.enable)
	{
		/* Turn on clk to Rx IQ */
		mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
			LCNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_MASK,
			1 << LCNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_SHIFT);
		mod_phy_reg(pi, LCNPHY_crsgainCtrl,
			LCNPHY_crsgainCtrl_APHYGatingEnable_MASK,
			0 << LCNPHY_crsgainCtrl_APHYGatingEnable_SHIFT);
		/* Number of samples */
		mod_phy_reg(pi, LCNPHY_IQNumSampsAddress,
			LCNPHY_IQNumSampsAddress_numSamps_MASK,
			k_noise_n_samps << LCNPHY_IQNumSampsAddress_numSamps_SHIFT);

		/* Signal ucode to start */
		ctrl = (ctrl | k_noise_active_flag);
		/* If init is not set the ucode will re-start w/o resetting internal data */
		if (init)
		{
			pi_lcn->noise.update_noise_cnt = 0;
			ctrl = (ctrl + k_noise_init_flag) & 0xffff;
		}
	}
	else
	{
		/* Turn off clk to Rx IQ */
		mod_phy_reg(pi, LCNPHY_sslpnCalibClkEnCtrl,
			LCNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_MASK,
			0 << LCNPHY_sslpnCalibClkEnCtrl_iqEstClkEn_SHIFT);
		mod_phy_reg(pi, LCNPHY_crsgainCtrl,
			LCNPHY_crsgainCtrl_APHYGatingEnable_MASK,
			1 << LCNPHY_crsgainCtrl_APHYGatingEnable_SHIFT);
		/* Signal ucode to stop */
		ctrl = (ctrl & ~k_noise_active_flag);
	}

	wlc_lcnphy_noise_set_reg(pi, M_NOISE_CAL_CMD, ctrl);
}


static void
wlc_lcnphy_noise_save_phy_regs(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi_lcn->noise.high_gain =
	((read_phy_reg(pi, LCNPHY_HiGainDB) &
		LCNPHY_HiGainDB_HiGainDB_MASK) >> LCNPHY_HiGainDB_HiGainDB_SHIFT);
	pi_lcn->noise.input_pwr_offset =
		(read_phy_reg(pi, LCNPHY_InputPowerDB)
		& LCNPHY_InputPowerDB_inputpwroffsetdb_MASK);
	if (pi_lcn->noise.input_pwr_offset > 127)
		pi_lcn->noise.input_pwr_offset -= 256;
	pi_lcn->noise.nf_substract_val =
		(read_phy_reg(pi, LCNPHY_nfSubtractVal) & 0x3ff);

}

static void
wlc_lcnphy_noise_restore_phy_regs(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	mod_phy_reg(pi, LCNPHY_InputPowerDB,
		LCNPHY_InputPowerDB_inputpwroffsetdb_MASK,
		(pi_lcn->noise.input_pwr_offset <<
		LCNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
	mod_phy_reg(pi, LCNPHY_HiGainDB,
		LCNPHY_HiGainDB_HiGainDB_MASK,
		((pi_lcn->noise.high_gain) <<
		LCNPHY_HiGainDB_HiGainDB_SHIFT));
	write_phy_reg(pi, LCNPHY_nfSubtractVal, pi_lcn->noise.nf_substract_val);

	/* Reset radio ctrl and crs gain */
	or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
	write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
	OSL_DELAY(20); /* To give AGC/radio ctrl chance to initialize */
}

static void
wlc_lcnphy_noise_init(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

#ifdef BCM94330OLYMPICAMGEPA_SSID
	if (BOARDTYPE(pi->sh->boardtype) == BCM94330OLYMPICAMGEPA_SSID) {
		pi_lcn->noise.enable = TRUE;
		pi_lcn->noise.per_cal_enable = TRUE;
	} else
#endif
	{
		pi_lcn->noise.enable = FALSE;
		pi_lcn->noise.per_cal_enable = FALSE;
	}
	pi_lcn->noise.adj_en = TRUE;

	wlc_lcnphy_noise_save_phy_regs(pi);

	wlc_lcnphy_noise_reset_log(pi);

}


static bool
wlc_lcnphy_noise_adj(phy_info_t *pi, uint32 metric, int16 gain)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint32 t_h, t_l, t, noise;
	int16 high_gain, high_gain_old, i, input_pwr_offset, input_pwr_offset_old;
	int16 high_gain_adjustment;
	int8 adj;
	bool done = TRUE;
	bool adj_en = pi_lcn->noise.adj_en;

	high_gain = ((read_phy_reg(pi, LCNPHY_HiGainDB) &
		LCNPHY_HiGainDB_HiGainDB_MASK) >> LCNPHY_HiGainDB_HiGainDB_SHIFT);
	high_gain_old = high_gain;
	input_pwr_offset = ((read_phy_reg(pi, LCNPHY_InputPowerDB) &
		LCNPHY_InputPowerDB_inputpwroffsetdb_MASK) >>
		LCNPHY_InputPowerDB_inputpwroffsetdb_SHIFT);
	if (input_pwr_offset > 127)
		input_pwr_offset -= 256;
	input_pwr_offset_old = input_pwr_offset;

	high_gain_adjustment = (high_gain - pi_lcn->noise.high_gain);

	if ((high_gain_adjustment > 0) && (metric < (k_noise_cal_ref_level>>2))) {
		/* This means the gain was increased by 3dB and noise pwr is more 
		than 6 dB below ref.
		Something is wrong so don't adjust anything and try again!
		*/
		adj_en = FALSE;
		done = FALSE;
		pi_lcn->noise.err_cnt += (uint8)(1<<8);
	}

	if ((pi_lcn->noise.update_noise_cnt > 0) &&
		((metric > (uint32)pi_lcn->noise.init_noise) ||
		(metric < (uint32)(pi_lcn->noise.init_noise >> 1)))) {
		/* Metric should only increase or decrase but not by much.
		Something is wrong so don't adjust anything and try again!
		*/
		adj_en = FALSE;
		done = FALSE;
		pi_lcn->noise.err_cnt += 1;
	}

	/* Calculate adjustment = 10*log10(metric/ref_level) */
	adj = 0;
	noise = metric;
	t_h = ((uint32)k_noise_cal_ref_level << 1);
	t_l = ((uint32)k_noise_cal_ref_level >> 1);
	while ((noise > t_h) || (noise < t_l)) {
		if (noise > t_h) {
			noise >>= 1;
			adj += 3;
		}
		if (noise < t_l) {
			noise <<= 1;
			adj -= 3;
		}
	}
	i = 0;
	noise = noise << 8;
	do {
		t = ((uint32)k_noise_cal_ref_level * k_noise_adj_tbl[i] + 128);
		if (t > noise)
			break;
		i++;
	} while (i < k_noise_adj_tbl_n);
	adj += (i - (k_noise_adj_tbl_n/2));

	/* Adjust high gain */
	if (adj <= -(k_noise_cal_gain_adj_threshold))
		high_gain += 3;

	if (high_gain > (pi_lcn->noise.high_gain + k_noise_cal_max_gain_delta))
		high_gain = (pi_lcn->noise.high_gain + k_noise_cal_max_gain_delta);

	if (adj_en && (high_gain != high_gain_old)) {
		mod_phy_reg(pi, LCNPHY_HiGainDB,
			LCNPHY_HiGainDB_HiGainDB_MASK,
			(high_gain << LCNPHY_HiGainDB_HiGainDB_SHIFT));

		/* Reset radio ctrl and crs gain */
		or_phy_reg(pi, LCNPHY_resetCtrl, 0x44);
		write_phy_reg(pi, LCNPHY_resetCtrl, 0x80);
		OSL_DELAY(20); /* To give AGC/radio ctrl chance to initialize */

		done = FALSE;
	}

	/* Adjust nf substract val if needed */
	write_phy_reg(pi, LCNPHY_nfSubtractVal,
		(pi_lcn->noise.nf_substract_val + 4*high_gain_adjustment));

	/* Adjust input pwr offset if needed */
	if (adj_en) {
		/* Account for change in high_gain */
		input_pwr_offset = -(adj) - (high_gain - high_gain_old);

		if (done) {
			if (high_gain_adjustment > 0) {
				/* At high temp thermal noise increases and gain decreases
				so change in noise variance may not account for all
				gain change. To deal w/ this bias estimate
				if gain has increased but noise pwr is still low.
				*/
				if (metric < k_noise_cal_bias_p4_high_level)
					input_pwr_offset += 4;
				else
					input_pwr_offset += 2;
			} else {
				/* At low temps the estimator seems to be biased high which
				results in too big an adjustment
				*/
				if (metric > k_noise_cal_bias_m2_low_level)
					input_pwr_offset -= 2;
				else if (metric > k_noise_cal_bias_m1_low_level)
					input_pwr_offset -= 1;
			}
	}
	/* Limit input pwr offset */
	if (input_pwr_offset > k_noise_cal_max_input_pwr)
	input_pwr_offset = k_noise_cal_max_input_pwr;
	else if (input_pwr_offset < k_noise_cal_min_input_pwr)
	input_pwr_offset = k_noise_cal_min_input_pwr;

	mod_phy_reg(pi, LCNPHY_InputPowerDB,
		LCNPHY_InputPowerDB_inputpwroffsetdb_MASK,
		(input_pwr_offset << LCNPHY_InputPowerDB_inputpwroffsetdb_SHIFT));
	}

	wlc_lcnphy_noise_log_adj(pi,
		adj,
		high_gain,
		input_pwr_offset,
		!done,
		((input_pwr_offset != input_pwr_offset_old) ? TRUE : FALSE));

	return done;
}


static bool
wlc_lcnphy_noise_update(phy_info_t *pi, uint16* p_noise, int16* p_gain)
{
	uint16 noise_cnt, noise;
	int16 gain;
	bool res = FALSE;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (!wlc_lcnphy_noise_sync_ucode(pi, 0, NULL))
	  return FALSE;

	noise_cnt = wlc_lcnphy_noise_get_reg(pi, M_SSLPNPHY_NOISE_SAMPLES);
	noise = wlc_lcnphy_noise_get_reg(pi, M_NOISE_CAL_METRIC);
	gain = wlc_lcnphy_noise_get_reg(pi, M_55f_REG_VAL);
	if (gain > 127)
	  gain -= 256;

	if (noise_cnt > 0)
	{
		res = TRUE;
		*p_noise = noise;
		*p_gain = gain;

		if (pi_lcn->noise.update_noise_cnt == 0)
			pi_lcn->noise.init_noise = noise;
		pi_lcn->noise.update_noise_cnt++;

		wlc_lcnphy_noise_log_update(pi, noise_cnt, noise, gain);
	}

	return res;
}

static void
wlc_lcnphy_noise_reset(phy_info_t *pi, bool restore_regs)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	pi_lcn->noise.state = k_noise_end_state;
	wlc_lcnphy_noise_ucode_ctrl(pi, FALSE, FALSE);
	if (restore_regs)
	  wlc_lcnphy_noise_restore_phy_regs(pi);
}

void
wlc_lcnphy_noise_measure_start(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (pi_lcn->noise.state != k_noise_end_state)
	  return;

	pi_lcn->noise.state = k_noise_adj_state;
	pi_lcn->noise.time = 0;
	pi_lcn->noise.err_cnt = 0;
	pi_lcn->noise.update_noise_cnt = 0;
	pi_lcn->noise.init_noise = 0;
	pi_lcn->noise.tainted = FALSE;
	if (pi_lcn->noise.adj_en)
		wlc_lcnphy_noise_restore_phy_regs(pi);
	wlc_lcnphy_noise_ucode_ctrl(pi, TRUE, TRUE);
	pi_lcn->noise.start_time =
		(uint16)((R_REG(pi->sh->osh, &pi->regs->tsf_timerlow) >> 10) & 0xffff);

	wlc_lcnphy_noise_log_start(pi);
}

void
wlc_lcnphy_noise_measure_stop(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (pi_lcn->noise.state == k_noise_end_state)
		return;

	wlc_lcnphy_noise_ucode_ctrl(pi, FALSE, FALSE);
	/* This is called from begining of watchdog. If ucode cant
	 * be stopped before exiting this function the noise measurement
	 * may be invalid b/c of a cal....
	 */
	if (wlc_lcnphy_noise_sync_ucode(pi, k_noise_sync_stop_timeout, NULL))
		pi_lcn->noise.tainted = FALSE;
	else
		pi_lcn->noise.tainted = TRUE;

	wlc_lcnphy_noise_log_tainted(pi, pi_lcn->noise.tainted);
}

void
wlc_lcnphy_noise_measure(phy_info_t *pi)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	uint16 noise, timestamp;
	int16 gain;

	/* This proc is called from noise measure callback or from watchdog timer */

	if (!(pi_lcn->noise.enable &&
		/* Only run if not complete */
		(pi_lcn->noise.state != k_noise_end_state) &&
		/* No point in running this if ucode is not active */
		(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC)))
		return;

	/* Check for timeout */
	timestamp = (uint16)(R_REG(pi->sh->osh, &pi->regs->tsf_timerlow) >> 10) & 0xffff;
	pi_lcn->noise.time = (timestamp - pi_lcn->noise.start_time);
	if (pi_lcn->noise.time > k_noise_cal_timeout) {
		wlc_lcnphy_noise_reset(pi, TRUE);
		return;
	}

	if (pi_lcn->noise.tainted) {
		/* Restart b/c couldn't stop noise cal before watchdog started */
		pi_lcn->noise.tainted = FALSE;
		pi_lcn->noise.state = k_noise_adj_state;
		pi_lcn->noise.check_cnt = 0;
		wlc_lcnphy_noise_ucode_ctrl(pi, TRUE, TRUE);
		return;
	} else if (!wlc_lcnphy_noise_ucode_is_active(pi)) {
		/* Noise cal stopped before watchdog started -- continue */
		wlc_lcnphy_noise_ucode_ctrl(pi, TRUE, FALSE);
		return;
	}

	/* State machine */
	switch (pi_lcn->noise.state)
	{
		case k_noise_adj_state:
		if (wlc_lcnphy_noise_update(pi, &noise, &gain))
		{
			if (wlc_lcnphy_noise_adj(pi, (uint32)noise, gain))
			{
				pi_lcn->noise.check_cnt = k_noise_cal_min_check_cnt;
				pi_lcn->noise.state = k_noise_check_state;
			}
			else
			{
				wlc_lcnphy_noise_ucode_ctrl(pi, TRUE, TRUE);
			}
		}
		break;
		case k_noise_check_state:
		if (wlc_lcnphy_noise_update(pi, &noise, &gain))
		{
			if (!wlc_lcnphy_noise_adj(pi, (uint32)noise, gain))
			{
				wlc_lcnphy_noise_ucode_ctrl(pi, TRUE, TRUE);
				pi_lcn->noise.state = k_noise_adj_state;
			}
			else if (--(pi_lcn->noise.check_cnt) <= 0)
			{
				wlc_lcnphy_noise_reset(pi, FALSE); /* Done! */
			}
		}
		default:
		break;
	}

	wlc_lcnphy_noise_advance_log(pi);
}


void
wlc_lcnphy_noise_measure_disable(phy_info_t *pi, uint32 flag, uint32* p_flag)
{
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

	if (p_flag) {
		if (wlc_lcnphy_noise_log_dump_active(pi))
			*p_flag = wlc_lcnphy_noise_log_data(pi);
		else
			*p_flag = ((uint32)pi_lcn->noise.enable) |
			(((uint32)pi_lcn->noise.per_cal_enable)<<1) |
			(((uint32)pi_lcn->noise.adj_en)<<2);
		return;
	}

	if (!wlc_lcnphy_noise_log_ioctl(pi, (flag >> 3))) {
		pi_lcn->noise.enable = ((flag & 1) ? TRUE : FALSE);
		pi_lcn->noise.per_cal_enable = ((flag & 2) ? TRUE : FALSE);
		pi_lcn->noise.adj_en = ((flag & 4) ? TRUE : FALSE);
		if (!pi_lcn->noise.enable) {
			wlc_lcnphy_noise_reset(pi, TRUE);
		}
	}
}
