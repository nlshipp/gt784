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
 * $Id: wlc_phy_ht.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
 *
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
#include <sbchipc.h>
#include <hndpmu.h>
#include <bcmsrom_fmt.h>
#include <sbsprom.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>

#include <wlc_phyreg_ht.h>
#include <wlc_phytbl_ht.h>


typedef struct _htphy_dac_adc_decouple_war {
	bool   is_on;
	uint16 PapdCalShifts[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 PapdCalCorrelate;
	uint16 PapdEpsilonUpdateIterations;
	uint16 PapdCalSettle;
} htphy_dac_adc_decouple_war_t;

typedef struct _htphy_rxcal_radioregs {
	bool   is_orig;
	uint16 RF_TX_txrxcouple_2g_pwrup[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_2g_atten[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_5g_pwrup[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_5g_atten[PHY_CORE_MAX];
	uint16 RF_rxbb_bias_master[PHY_CORE_MAX];
	uint16 RF_afe_vcm_cal_master[PHY_CORE_MAX];
	uint16 RF_afe_set_vcm_i[PHY_CORE_MAX];
	uint16 RF_afe_set_vcm_q[PHY_CORE_MAX];
} htphy_rxcal_radioregs_t;

typedef struct _htphy_rxcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 bbmult[PHY_CORE_MAX];
	uint16 rfseq_txgain[PHY_CORE_MAX];
	uint16 Afectrl[PHY_CORE_MAX];
	uint16 AfectrlOverride[PHY_CORE_MAX];
	uint16 RfseqCoreActv2059;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlCorePU[PHY_CORE_MAX];
	uint16 RfctrlOverride[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfPU[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfPU[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
} htphy_rxcal_phyregs_t;

typedef struct _htphy_txcal_radioregs {
	bool   is_orig;
	uint16 RF_TX_tx_ssi_master[PHY_CORE_MAX];
	uint16 RF_TX_tx_ssi_mux[PHY_CORE_MAX];
	uint16 RF_TX_tssia[PHY_CORE_MAX];
	uint16 RF_TX_tssig[PHY_CORE_MAX];
	uint16 RF_TX_iqcal_vcm_hg[PHY_CORE_MAX];
	uint16 RF_TX_iqcal_idac[PHY_CORE_MAX];
	uint16 RF_TX_tssi_misc1[PHY_CORE_MAX];
	uint16 RF_TX_tssi_vcm[PHY_CORE_MAX];
} htphy_txcal_radioregs_t;

typedef struct _htphy_txcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 Afectrl[PHY_CORE_MAX];
	uint16 AfectrlOverride[PHY_CORE_MAX];
	uint16 Afectrl_AuxADCmode[PHY_CORE_MAX];
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlPU[PHY_CORE_MAX];
	uint16 RfctrlOverride[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
} htphy_txcal_phyregs_t;

struct phy_info_htphy {
	uint16 classifier_state;
	uint16 clip_state[PHY_CORE_MAX];
	uint16 deaf_count;
	uint16 saved_bbconf;
	uint16 rfctrlIntc_save[PHY_CORE_MAX];
	uint16 bb_mult_save[PHY_CORE_MAX];
	uint8  bb_mult_save_valid;
	uint8  txpwrindex_hw_save[PHY_CORE_MAX]; /* txpwr start index for hwpwrctrl */
	int8   idle_tssi[PHY_CORE_MAX];
	int16  pwrdet_a1[PHY_CORE_MAX][4]; /* 2G, 5G_low, 5G_mid, 5G_high */
	int16  pwrdet_b0[PHY_CORE_MAX][4]; /* 2G, 5G_low, 5G_mid, 5G_high */
	int16  pwrdet_b1[PHY_CORE_MAX][4]; /* 2G, 5G_low, 5G_mid, 5G_high */
	int8   max_pwr[PHY_CORE_MAX][4]; /* 2G, 5G_low, 5G_mid, 5G_high */

	uint32 pstart; /* sample collect fifo begins */
	uint32 pstop;  /* sample collect fifo ends */
	uint32 pfirst; /* sample collect trigger begins */
	uint32 plast;  /* sample collect trigger ends */

	bool   ht_rxldpc_override;	/* LDPC override for RX, both band */

	htphy_dac_adc_decouple_war_t ht_dac_adc_decouple_war_info;
	htphy_txcal_radioregs_t htphy_txcal_radioregs_orig;
	htphy_txcal_phyregs_t ht_txcal_phyregs_orig;
	htphy_rxcal_radioregs_t ht_rxcal_radioregs_orig;
	htphy_rxcal_phyregs_t  ht_rxcal_phyregs_orig;
};


/* %%%%%% shorthand macros */

#define MOD_PHYREG(pi, reg, field, value) \
	mod_phy_reg(pi, reg, \
	reg##_##field##_MASK, (value) << reg##_##field##_##SHIFT);

#define MOD_PHYREGC(pi, reg, core, field, value) \
	mod_phy_reg(pi, (core == 0) ? reg##_CR0 : ((core == 1) ? reg##_CR1 : reg##_CR2), \
	reg##_##field##_MASK, (value) << reg##_##field##_##SHIFT);

#define READ_PHYREG(pi, reg, field) \
	((read_phy_reg(pi, reg) \
	& reg##_##field##_##MASK) >> reg##_##field##_##SHIFT)

#define READ_PHYREGC(pi, reg, core, field)		  \
	((read_phy_reg(pi, (core == 0) ? reg##_CR0 : ((core == 1) ? reg##_CR1 : reg##_CR2)) \
	& reg##_##field##_##MASK) >> reg##_##field##_##SHIFT)


/* spectrum shaping filter (cck) */
#define TXFILT_SHAPING_CCK      0

#define HTPHY_NUM_DIG_FILT_COEFFS 15
uint16 HTPHY_txdigi_filtcoeffs[][HTPHY_NUM_DIG_FILT_COEFFS] = {
	{-360, 164, -376, 164, -1533, 576, 308, -314, 308,
	121, -73, 121, 91, 124, 91},   /* CCK ePA/iPA */
};
/* %%%%%% channel/radio */

#define TUNING_REG(reg, core)  (((core == 0) ? reg##0 : ((core == 1) ? reg##1 : reg##2)))

/* channel info structure for htphy */
typedef struct _chan_info_htphy_radio2059 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */
	uint8  RF_vcocal_countval0;
	uint8  RF_vcocal_countval1;
	uint8  RF_rfpll_refmaster_sparextalsize;
	uint8  RF_rfpll_loopfilter_r1;
	uint8  RF_rfpll_loopfilter_c2;
	uint8  RF_rfpll_loopfilter_c1;
	uint8  RF_cp_kpd_idac;
	uint8  RF_rfpll_mmd0;
	uint8  RF_rfpll_mmd1;
	uint8  RF_vcobuf_tune;
	uint8  RF_logen_mx2g_tune;
	uint8  RF_logen_mx5g_tune;
	uint8  RF_logen_indbuf2g_tune;
	uint8  RF_logen_indbuf5g_tune_core0;
	uint8  RF_txmix2g_tune_boost_pu_core0;
	uint8  RF_pad2g_tune_pus_core0;
	uint8  RF_pga_boost_tune_core0;
	uint8  RF_txmix5g_boost_tune_core0;
	uint8  RF_pad5g_tune_misc_pus_core0;
	uint8  RF_lna2g_tune_core0;
	uint8  RF_lna5g_tune_core0;
	uint8  RF_logen_indbuf5g_tune_core1;
	uint8  RF_txmix2g_tune_boost_pu_core1;
	uint8  RF_pad2g_tune_pus_core1;
	uint8  RF_pga_boost_tune_core1;
	uint8  RF_txmix5g_boost_tune_core1;
	uint8  RF_pad5g_tune_misc_pus_core1;
	uint8  RF_lna2g_tune_core1;
	uint8  RF_lna5g_tune_core1;
	uint8  RF_logen_indbuf5g_tune_core2;
	uint8  RF_txmix2g_tune_boost_pu_core2;
	uint8  RF_pad2g_tune_pus_core2;
	uint8  RF_pga_boost_tune_core2;
	uint8  RF_txmix5g_boost_tune_core2;
	uint8  RF_pad5g_tune_misc_pus_core2;
	uint8  RF_lna2g_tune_core2;
	uint8  RF_lna5g_tune_core2;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_htphy_radio2059_t;

typedef struct htphy_sfo_cfg {
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} htphy_sfo_cfg_t;

/* channel info table for htphyrev0 (autogenerated by gen_tune_2059.tcl) */
static chan_info_htphy_radio2059_t chan_info_htphyrev0_2059v0[] = {
	{
	184,   4920,  0x68,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xec,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0xd3,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07b4,  0x07b0,  0x07ac,  0x0214,  0x0215,  0x0216
	},
	{
	186,   4930,  0x6b,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xed,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0xd3,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07b8,  0x07b4,  0x07b0,  0x0213,  0x0214,  0x0215
	},
	{
	188,   4940,  0x6e,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xee,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0xd3,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07bc,  0x07b8,  0x07b4,  0x0212,  0x0213,  0x0214
	},
	{
	190,   4950,  0x72,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xef,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07c0,  0x07bc,  0x07b8,  0x0211,  0x0212,  0x0213
	},
	{
	192,   4960,  0x75,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf0,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07c4,  0x07c0,  0x07bc,  0x020f,  0x0211,  0x0212
	},
	{
	194,   4970,  0x78,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf1,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07c8,  0x07c4,  0x07c0,  0x020e,  0x020f,  0x0211
	},
	{
	196,   4980,  0x7c,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf2,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07cc,  0x07c8,  0x07c4,  0x020d,  0x020e,  0x020f
	},
	{
	198,   4990,  0x7f,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf3,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xd3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xd3,  0x00,  0xff,  0x07d0,  0x07cc,  0x07c8,  0x020c,  0x020d,  0x020e
	},
	{
	200,   5000,  0x82,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf4,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07d4,  0x07d0,  0x07cc,  0x020b,  0x020c,  0x020d
	},
	{
	202,   5010,  0x86,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf5,  0x01,  0x0f,
	0x00,  0x0f,  0x00,  0x0f,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0f,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0f,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07d8,  0x07d4,  0x07d0,  0x020a,  0x020b,  0x020c
	},
	{
	204,   5020,  0x89,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf6,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07dc,  0x07d8,  0x07d4,  0x0209,  0x020a,  0x020b
	},
	{
	206,   5030,  0x8c,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf7,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07e0,  0x07dc,  0x07d8,  0x0208,  0x0209,  0x020a
	},
	{
	208,   5040,  0x90,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf8,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07e4,  0x07e0,  0x07dc,  0x0207,  0x0208,  0x0209
	},
	{
	210,   5050,  0x93,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xf9,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07e8,  0x07e4,  0x07e0,  0x0206,  0x0207,  0x0208
	},
	{
	212,   5060,  0x96,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xfa,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07ec,  0x07e8,  0x07e4,  0x0205,  0x0206,  0x0207
	},
	{
	214,   5070,  0x9a,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xfb,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07f0,  0x07ec,  0x07e8,  0x0204,  0x0205,  0x0206
	},
	{
	216,   5080,  0x9d,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xfc,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07f4,  0x07f0,  0x07ec,  0x0203,  0x0204,  0x0205
	},
	{
	218,   5090,  0xa0,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xfd,  0x01,  0x0e,
	0x00,  0x0e,  0x00,  0x0e,  0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,
	0x00,  0x00,  0x0f,  0x0f,  0xb3,  0x00,  0xff,  0x0e,  0x00,  0x00,  0x0f,  0x0f,
	0xb3,  0x00,  0xff,  0x07f8,  0x07f4,  0x07f0,  0x0202,  0x0203,  0x0204
	},
	{
	220,   5100,  0xa4,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xfe,  0x01,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x07fc,  0x07f8,  0x07f4,  0x0201,  0x0202,  0x0203
	},
	{
	222,   5110,  0xa7,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0xff,  0x01,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0800,  0x07fc,  0x07f8,  0x0200,  0x0201,  0x0202
	},
	{
	224,   5120,  0xaa,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x00,  0x02,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0804,  0x0800,  0x07fc,  0x01ff,  0x0200,  0x0201
	},
	{
	226,   5130,  0xae,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x01,  0x02,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0808,  0x0804,  0x0800,  0x01fe,  0x01ff,  0x0200
	},
	{
	228,   5140,  0xb1,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x02,  0x02,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x080c,  0x0808,  0x0804,  0x01fd,  0x01fe,  0x01ff
	},
	{
	32,   5160,  0xb8,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x04,  0x02,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0814,  0x0810,  0x080c,  0x01fb,  0x01fc,  0x01fd
	},
	{
	34,   5170,  0xbb,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x05,  0x02,  0x0d,
	0x00,  0x0d,  0x00,  0x0d,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0d,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0818,  0x0814,  0x0810,  0x01fa,  0x01fb,  0x01fc
	},
	{
	36,   5180,  0xbe,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x06,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x081c,  0x0818,  0x0814,  0x01f9,  0x01fa,  0x01fb
	},
	{
	38,   5190,  0xc2,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x07,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0xa3,  0x00,  0xfc,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0xa3,  0x00,  0xfc,  0x0820,  0x081c,  0x0818,  0x01f8,  0x01f9,  0x01fa
	},
	{
	40,   5200,  0xc5,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x08,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0824,  0x0820,  0x081c,  0x01f7,  0x01f8,  0x01f9
	},
	{
	42,   5210,  0xc8,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x09,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0828,  0x0824,  0x0820,  0x01f6,  0x01f7,  0x01f8
	},
	{
	44,   5220,  0xcc,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0a,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x082c,  0x0828,  0x0824,  0x01f5,  0x01f6,  0x01f7
	},
	{
	46,   5230,  0xcf,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0b,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0830,  0x082c,  0x0828,  0x01f4,  0x01f5,  0x01f6
	},
	{
	48,   5240,  0xd2,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0c,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0834,  0x0830,  0x082c,  0x01f3,  0x01f4,  0x01f5
	},
	{
	50,   5250,  0xd6,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0d,  0x02,  0x0c,
	0x00,  0x0c,  0x00,  0x0c,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0c,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0838,  0x0834,  0x0830,  0x01f2,  0x01f3,  0x01f4
	},
	{
	52,   5260,  0xd9,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0e,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x083c,  0x0838,  0x0834,  0x01f1,  0x01f2,  0x01f3
	},
	{
	54,   5270,  0xdc,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x0f,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0840,  0x083c,  0x0838,  0x01f0,  0x01f1,  0x01f2
	},
	{
	56,   5280,  0xe0,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x10,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0844,  0x0840,  0x083c,  0x01f0,  0x01f0,  0x01f1
	},
	{
	58,   5290,  0xe3,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x11,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,
	0x00,  0x00,  0x0f,  0x0f,  0x93,  0x00,  0xf8,  0x0b,  0x00,  0x00,  0x0f,  0x0f,
	0x93,  0x00,  0xf8,  0x0848,  0x0844,  0x0840,  0x01ef,  0x01f0,  0x01f0
	},
	{
	60,   5300,  0xe6,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x12,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x084c,  0x0848,  0x0844,  0x01ee,  0x01ef,  0x01f0
	},
	{
	62,   5310,  0xea,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x13,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0850,  0x084c,  0x0848,  0x01ed,  0x01ee,  0x01ef
	},
	{
	64,   5320,  0xed,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x14,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0854,  0x0850,  0x084c,  0x01ec,  0x01ed,  0x01ee
	},
	{
	66,   5330,  0xf0,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x15,  0x02,  0x0b,
	0x00,  0x0b,  0x00,  0x0b,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0b,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0858,  0x0854,  0x0850,  0x01eb,  0x01ec,  0x01ed
	},
	{
	68,   5340,  0xf4,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x16,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x085c,  0x0858,  0x0854,  0x01ea,  0x01eb,  0x01ec
	},
	{
	70,   5350,  0xf7,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x17,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0860,  0x085c,  0x0858,  0x01e9,  0x01ea,  0x01eb
	},
	{
	72,   5360,  0xfa,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x18,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0864,  0x0860,  0x085c,  0x01e8,  0x01e9,  0x01ea
	},
	{
	74,   5370,  0xfe,  0x16,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x19,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0868,  0x0864,  0x0860,  0x01e7,  0x01e8,  0x01e9
	},
	{
	76,   5380,  0x01,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1a,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x086c,  0x0868,  0x0864,  0x01e6,  0x01e7,  0x01e8
	},
	{
	78,   5390,  0x04,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1b,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,
	0x00,  0x00,  0x0f,  0x0c,  0x83,  0x00,  0xf5,  0x0a,  0x00,  0x00,  0x0f,  0x0c,
	0x83,  0x00,  0xf5,  0x0870,  0x086c,  0x0868,  0x01e5,  0x01e6,  0x01e7
	},
	{
	80,   5400,  0x08,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1c,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x0a,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x0a,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0874,  0x0870,  0x086c,  0x01e5,  0x01e5,  0x01e6
	},
	{
	82,   5410,  0x0b,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1d,  0x02,  0x0a,
	0x00,  0x0a,  0x00,  0x0a,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x0a,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x0a,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0878,  0x0874,  0x0870,  0x01e4,  0x01e5,  0x01e5
	},
	{
	84,   5420,  0x0e,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1e,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x087c,  0x0878,  0x0874,  0x01e3,  0x01e4,  0x01e5
	},
	{
	86,   5430,  0x12,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x1f,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0880,  0x087c,  0x0878,  0x01e2,  0x01e3,  0x01e4
	},
	{
	88,   5440,  0x15,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x20,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0884,  0x0880,  0x087c,  0x01e1,  0x01e2,  0x01e3
	},
	{
	90,   5450,  0x18,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x21,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0888,  0x0884,  0x0880,  0x01e0,  0x01e1,  0x01e2
	},
	{
	92,   5460,  0x1c,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x22,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x088c,  0x0888,  0x0884,  0x01df,  0x01e0,  0x01e1
	},
	{
	94,   5470,  0x1f,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x23,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0890,  0x088c,  0x0888,  0x01de,  0x01df,  0x01e0
	},
	{
	96,   5480,  0x22,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x24,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0894,  0x0890,  0x088c,  0x01dd,  0x01de,  0x01df
	},
	{
	98,   5490,  0x26,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x25,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,
	0x00,  0x00,  0x0d,  0x09,  0x53,  0x00,  0xb1,  0x09,  0x00,  0x00,  0x0d,  0x09,
	0x53,  0x00,  0xb1,  0x0898,  0x0894,  0x0890,  0x01dd,  0x01dd,  0x01de
	},
	{
	100,   5500,  0x29,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x26,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x09,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x09,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x089c,  0x0898,  0x0894,  0x01dc,  0x01dd,  0x01dd
	},
	{
	102,   5510,  0x2c,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x27,  0x02,  0x09,
	0x00,  0x09,  0x00,  0x09,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x09,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x09,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08a0,  0x089c,  0x0898,  0x01db,  0x01dc,  0x01dd
	},
	{
	104,   5520,  0x30,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x28,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08a4,  0x08a0,  0x089c,  0x01da,  0x01db,  0x01dc
	},
	{
	106,   5530,  0x33,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x29,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08a8,  0x08a4,  0x08a0,  0x01d9,  0x01da,  0x01db
	},
	{
	108,   5540,  0x36,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2a,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08ac,  0x08a8,  0x08a4,  0x01d8,  0x01d9,  0x01da
	},
	{
	110,   5550,  0x3a,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2b,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08b0,  0x08ac,  0x08a8,  0x01d7,  0x01d8,  0x01d9
	},
	{
	112,   5560,  0x3d,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2c,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08b4,  0x08b0,  0x08ac,  0x01d7,  0x01d7,  0x01d8
	},
	{
	114,   5570,  0x40,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2d,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08b8,  0x08b4,  0x08b0,  0x01d6,  0x01d7,  0x01d7
	},
	{
	116,   5580,  0x44,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2e,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08bc,  0x08b8,  0x08b4,  0x01d5,  0x01d6,  0x01d7
	},
	{
	118,   5590,  0x47,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x2f,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,
	0x00,  0x00,  0x0a,  0x06,  0x43,  0x00,  0x80,  0x08,  0x00,  0x00,  0x0a,  0x06,
	0x43,  0x00,  0x80,  0x08c0,  0x08bc,  0x08b8,  0x01d4,  0x01d5,  0x01d6
	},
	{
	120,   5600,  0x4a,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x30,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x08,
	0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x08,  0x00,  0x00,  0x09,  0x04,
	0x23,  0x00,  0x60,  0x08c4,  0x08c0,  0x08bc,  0x01d3,  0x01d4,  0x01d5
	},
	{
	122,   5610,  0x4e,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x31,  0x02,  0x08,
	0x00,  0x08,  0x00,  0x08,  0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x08,
	0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x08,  0x00,  0x00,  0x09,  0x04,
	0x23,  0x00,  0x60,  0x08c8,  0x08c4,  0x08c0,  0x01d2,  0x01d3,  0x01d4
	},
	{
	124,   5620,  0x51,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x32,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x04,
	0x23,  0x00,  0x60,  0x08cc,  0x08c8,  0x08c4,  0x01d2,  0x01d2,  0x01d3
	},
	{
	126,   5630,  0x54,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x33,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x04,
	0x23,  0x00,  0x60,  0x08d0,  0x08cc,  0x08c8,  0x01d1,  0x01d2,  0x01d2
	},
	{
	128,   5640,  0x58,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x34,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x04,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x04,
	0x23,  0x00,  0x60,  0x08d4,  0x08d0,  0x08cc,  0x01d0,  0x01d1,  0x01d2
	},
	{
	130,   5650,  0x5b,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x35,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x03,
	0x23,  0x00,  0x60,  0x08d8,  0x08d4,  0x08d0,  0x01cf,  0x01d0,  0x01d1
	},
	{
	132,   5660,  0x5e,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x36,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x03,
	0x23,  0x00,  0x60,  0x08dc,  0x08d8,  0x08d4,  0x01ce,  0x01cf,  0x01d0
	},
	{
	134,   5670,  0x62,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x37,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x03,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x03,
	0x23,  0x00,  0x60,  0x08e0,  0x08dc,  0x08d8,  0x01ce,  0x01ce,  0x01cf
	},
	{
	136,   5680,  0x65,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x38,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x02,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x02,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x02,
	0x23,  0x00,  0x60,  0x08e4,  0x08e0,  0x08dc,  0x01cd,  0x01ce,  0x01ce
	},
	{
	138,   5690,  0x68,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x39,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x09,  0x02,  0x23,  0x00,  0x60,  0x07,
	0x00,  0x00,  0x09,  0x02,  0x23,  0x00,  0x60,  0x07,  0x00,  0x00,  0x09,  0x02,
	0x23,  0x00,  0x60,  0x08e8,  0x08e4,  0x08e0,  0x01cc,  0x01cd,  0x01ce
	},
	{
	140,   5700,  0x6c,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3a,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08ec,  0x08e8,  0x08e4,  0x01cb,  0x01cc,  0x01cd
	},
	{
	142,   5710,  0x6f,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3b,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08f0,  0x08ec,  0x08e8,  0x01ca,  0x01cb,  0x01cc
	},
	{
	144,   5720,  0x72,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3c,  0x02,  0x07,
	0x00,  0x07,  0x00,  0x07,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x07,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08f4,  0x08f0,  0x08ec,  0x01c9,  0x01ca,  0x01cb
	},
	{
	145,   5725,  0x74,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x79,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08f6,  0x08f2,  0x08ee,  0x01c9,  0x01ca,  0x01cb
	},
	{
	146,   5730,  0x76,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3d,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08f8,  0x08f4,  0x08f0,  0x01c9,  0x01c9,  0x01ca
	},
	{
	147,   5735,  0x77,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x7b,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08fa,  0x08f6,  0x08f2,  0x01c8,  0x01c9,  0x01ca
	},
	{
	148,   5740,  0x79,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3e,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08fc,  0x08f8,  0x08f4,  0x01c8,  0x01c9,  0x01c9
	},
	{
	149,   5745,  0x7b,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x7d,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x30,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x30,  0x08fe,  0x08fa,  0x08f6,  0x01c8,  0x01c8,  0x01c9
	},
	{
	150,   5750,  0x7c,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x3f,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0900,  0x08fc,  0x08f8,  0x01c7,  0x01c8,  0x01c9
	},
	{
	151,   5755,  0x7e,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x7f,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0902,  0x08fe,  0x08fa,  0x01c7,  0x01c8,  0x01c8
	},
	{
	152,   5760,  0x80,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x40,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0904,  0x0900,  0x08fc,  0x01c6,  0x01c7,  0x01c8
	},
	{
	153,   5765,  0x81,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x81,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0906,  0x0902,  0x08fe,  0x01c6,  0x01c7,  0x01c8
	},
	{
	154,   5770,  0x83,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x41,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0908,  0x0904,  0x0900,  0x01c6,  0x01c6,  0x01c7
	},
	{
	155,   5775,  0x85,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x83,  0x04,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x090a,  0x0906,  0x0902,  0x01c5,  0x01c6,  0x01c7
	},
	{
	156,   5780,  0x86,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x42,  0x02,  0x06,
	0x00,  0x06,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x06,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x090c,  0x0908,  0x0904,  0x01c5,  0x01c6,  0x01c6
	},
	{
	157,   5785,  0x88,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x85,  0x04,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x090e,  0x090a,  0x0906,  0x01c4,  0x01c5,  0x01c6
	},
	{
	158,   5790,  0x8a,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x43,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0910,  0x090c,  0x0908,  0x01c4,  0x01c5,  0x01c6
	},
	{
	159,   5795,  0x8b,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x87,  0x04,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x08,  0x02,  0x13,  0x00,  0x00,  0x05,  0x00,  0x00,  0x08,  0x02,
	0x13,  0x00,  0x00,  0x0912,  0x090e,  0x090a,  0x01c4,  0x01c4,  0x01c5
	},
	{
	160,   5800,  0x8d,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x44,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x08,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x08,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x08,  0x01,
	0x03,  0x00,  0x00,  0x0914,  0x0910,  0x090c,  0x01c3,  0x01c4,  0x01c5
	},
	{
	161,   5805,  0x8f,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x89,  0x04,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0916,  0x0912,  0x090e,  0x01c3,  0x01c4,  0x01c4
	},
	{
	162,   5810,  0x90,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x45,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0918,  0x0914,  0x0910,  0x01c2,  0x01c3,  0x01c4
	},
	{
	163,   5815,  0x92,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x8b,  0x04,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x091a,  0x0916,  0x0912,  0x01c2,  0x01c3,  0x01c4
	},
	{
	164,   5820,  0x94,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x46,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x091c,  0x0918,  0x0914,  0x01c2,  0x01c2,  0x01c3
	},
	{
	165,   5825,  0x95,  0x17,  0x20,  0x14,  0x08,  0x08,  0x30,  0x8d,  0x04,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x091e,  0x091a,  0x0916,  0x01c1,  0x01c2,  0x01c3
	},
	{
	166,   5830,  0x97,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x47,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0920,  0x091c,  0x0918,  0x01c1,  0x01c2,  0x01c2
	},
	{
	168,   5840,  0x9a,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x48,  0x02,  0x05,
	0x00,  0x05,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x05,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0924,  0x0920,  0x091c,  0x01c0,  0x01c1,  0x01c2
	},
	{
	170,   5850,  0x9e,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x49,  0x02,  0x04,
	0x00,  0x04,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0928,  0x0924,  0x0920,  0x01bf,  0x01c0,  0x01c1
	},
	{
	172,   5860,  0xa1,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x4a,  0x02,  0x04,
	0x00,  0x04,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x092c,  0x0928,  0x0924,  0x01bf,  0x01bf,  0x01c0
	},
	{
	174,   5870,  0xa4,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x4b,  0x02,  0x04,
	0x00,  0x04,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x04,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0930,  0x092c,  0x0928,  0x01be,  0x01bf,  0x01bf
	},
	{
	176,   5880,  0xa8,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x4c,  0x02,  0x03,
	0x00,  0x03,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0934,  0x0930,  0x092c,  0x01bd,  0x01be,  0x01bf
	},
	{
	178,   5890,  0xab,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x4d,  0x02,  0x03,
	0x00,  0x03,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x0938,  0x0934,  0x0930,  0x01bc,  0x01bd,  0x01be
	},
	{
	180,   5900,  0xae,  0x17,  0x10,  0x0c,  0x0c,  0x0c,  0x30,  0x4e,  0x02,  0x03,
	0x00,  0x03,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,
	0x00,  0x00,  0x06,  0x01,  0x03,  0x00,  0x00,  0x03,  0x00,  0x00,  0x06,  0x01,
	0x03,  0x00,  0x00,  0x093c,  0x0938,  0x0934,  0x01bc,  0x01bc,  0x01bd
	},
	{
	1,   2412,  0x48,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x6c,  0x09,  0x0f,
	0x0a,  0x00,  0x0a,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03c9,  0x03c5,  0x03c1,  0x043a,  0x043f,  0x0443
	},
	{
	2,   2417,  0x4b,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x71,  0x09,  0x0f,
	0x0a,  0x00,  0x0a,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03cb,  0x03c7,  0x03c3,  0x0438,  0x043d,  0x0441
	},
	{
	3,   2422,  0x4e,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x76,  0x09,  0x0f,
	0x09,  0x00,  0x09,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03cd,  0x03c9,  0x03c5,  0x0436,  0x043a,  0x043f
	},
	{
	4,   2427,  0x52,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x7b,  0x09,  0x0f,
	0x09,  0x00,  0x09,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03cf,  0x03cb,  0x03c7,  0x0434,  0x0438,  0x043d
	},
	{
	5,   2432,  0x55,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x80,  0x09,  0x0f,
	0x08,  0x00,  0x08,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03d1,  0x03cd,  0x03c9,  0x0431,  0x0436,  0x043a
	},
	{
	6,   2437,  0x58,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x85,  0x09,  0x0f,
	0x08,  0x00,  0x08,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03d3,  0x03cf,  0x03cb,  0x042f,  0x0434,  0x0438
	},
	{
	7,   2442,  0x5c,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x8a,  0x09,  0x0f,
	0x07,  0x00,  0x07,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03d5,  0x03d1,  0x03cd,  0x042d,  0x0431,  0x0436
	},
	{
	8,   2447,  0x5f,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x8f,  0x09,  0x0f,
	0x07,  0x00,  0x07,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03d7,  0x03d3,  0x03cf,  0x042b,  0x042f,  0x0434
	},
	{
	9,   2452,  0x62,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x94,  0x09,  0x0f,
	0x07,  0x00,  0x07,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03d9,  0x03d5,  0x03d1,  0x0429,  0x042d,  0x0431
	},
	{
	10,   2457,  0x66,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x99,  0x09,  0x0f,
	0x06,  0x00,  0x06,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03db,  0x03d7,  0x03d3,  0x0427,  0x042b,  0x042f
	},
	{
	11,   2462,  0x69,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0x9e,  0x09,  0x0f,
	0x06,  0x00,  0x06,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03dd,  0x03d9,  0x03d5,  0x0424,  0x0429,  0x042d
	},
	{
	12,   2467,  0x6c,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0xa3,  0x09,  0x0f,
	0x05,  0x00,  0x05,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03df,  0x03db,  0x03d7,  0x0422,  0x0427,  0x042b
	},
	{
	13,   2472,  0x70,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0xa8,  0x09,  0x0f,
	0x05,  0x00,  0x05,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xf0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xf0,  0x00,  0x03e1,  0x03dd,  0x03d9,  0x0420,  0x0424,  0x0429
	},
	{
	14,   2484,  0x78,  0x16,  0x30,  0x1b,  0x0a,  0x0a,  0x30,  0xb4,  0x09,  0x0f,
	0x04,  0x00,  0x04,  0x00,  0x61,  0x03,  0x00,  0x00,  0x00,  0xe0,  0x00,  0x00,
	0x61,  0x03,  0x00,  0x00,  0x00,  0xe0,  0x00,  0x00,  0x61,  0x03,  0x00,  0x00,
	0x00,  0xe0,  0x00,  0x03e6,  0x03e2,  0x03de,  0x041b,  0x041f,  0x0424
	},
};


/* %%%%%% radio reg */

/* 2059 rev0 register initialization tables */
radio_20xx_regs_t regs_2059_rev0[] = {
	{ 0x0000 | JTAG_2059_ALL,           0,  0 },
	{ 0x0001 | JTAG_2059_ALL,        0x59,  0 },
	{ 0x0002 | JTAG_2059_ALL,        0x20,  0 },
	{ 0x0003 | JTAG_2059_CR0,        0x1f,  0 },
	{ 0x0004 | JTAG_2059_CR2,         0x4,  0 },
	{ 0x0005 | JTAG_2059_CR2,         0x2,  0 },
	{ 0x0006 | JTAG_2059_CR2,         0x1,  0 },
	{ 0x0007 | JTAG_2059_CR2,         0x1,  0 },
	{ 0x0008 | JTAG_2059_CR0,         0x1,  0 },
	{ 0x0009 | JTAG_2059_CR0,        0x69,  0 },
	{ 0x000A | JTAG_2059_CR0,        0x66,  0 },
	{ 0x000B | JTAG_2059_CR0,         0x6,  0 },
	{ 0x000C | JTAG_2059_CR0,           0,  0 },
	{ 0x000D | JTAG_2059_CR0,         0x3,  0 },
	{ 0x000E | JTAG_2059_CR0,        0x20,  0 },
	{ 0x000F | JTAG_2059_CR0,        0x20,  0 },
	{ 0x0010 | JTAG_2059_CR0,           0,  0 },
	{ 0x0011 | JTAG_2059_CR0,        0x7c,  0 },
	{ 0x0012 | JTAG_2059_CR0,        0x42,  0 },
	{ 0x0013 | JTAG_2059_CR0,        0xbd,  0 },
	{ 0x0014 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0015 | JTAG_2059_CR0,        0x87,  0 },
	{ 0x0016 | JTAG_2059_CR0,         0x8,  0 },
	{ 0x0017 | JTAG_2059_CR0,        0x17,  0 },
	{ 0x0018 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0019 | JTAG_2059_CR0,           0,  0 },
	{ 0x001A | JTAG_2059_CR0,         0x2,  0 },
	{ 0x001B | JTAG_2059_CR0,        0x13,  0 },
	{ 0x001C | JTAG_2059_CR0,        0x3e,  0 },
	{ 0x001D | JTAG_2059_CR0,        0x3e,  0 },
	{ 0x001E | JTAG_2059_CR0,        0x96,  0 },
	{ 0x001F | JTAG_2059_CR0,         0x4,  0 },
	{ 0x0020 | JTAG_2059_CR0,           0,  0 },
	{ 0x0021 | JTAG_2059_CR0,           0,  0 },
	{ 0x0022 | JTAG_2059_CR0,        0x17,  0 },
	{ 0x0023 | JTAG_2059_CR0,         0x6,  0 },
	{ 0x0024 | JTAG_2059_CR0,         0x1,  0 },
	{ 0x0025 | JTAG_2059_CR0,         0x6,  0 },
	{ 0x0026 | JTAG_2059_CR0,         0x4,  0 },
	{ 0x0027 | JTAG_2059_CR0,         0xd,  0 },
	{ 0x0028 | JTAG_2059_CR0,         0xd,  0 },
	{ 0x0029 | JTAG_2059_CR0,        0x30,  0 },
	{ 0x002A | JTAG_2059_CR0,        0x32,  0 },
	{ 0x002B | JTAG_2059_CR0,         0x8,  0 },
	{ 0x002C | JTAG_2059_CR0,        0x1c,  0 },
	{ 0x002D | JTAG_2059_CR0,         0x2,  0 },
	{ 0x002E | JTAG_2059_CR0,         0x4,  0 },
	{ 0x002F | JTAG_2059_CR0,        0x7f,  0 },
	{ 0x0030 | JTAG_2059_CR0,        0x27,  0 },
	{ 0x0031 | JTAG_2059_CR0,           0,  0 },
	{ 0x0032 | JTAG_2059_CR0,           0,  0 },
	{ 0x0033 | JTAG_2059_CR0,           0,  0 },
	{ 0x0034 | JTAG_2059_CR0,           0,  0 },
	{ 0x0035 | JTAG_2059_CR0,        0x20,  0 },
	{ 0x0036 | JTAG_2059_CR0,        0x18,  0 },
	{ 0x0037 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0038 | JTAG_2059_ALL,         0x8,  0 },
	{ 0x0039 | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003A | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003B | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003C | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003D | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003E | JTAG_2059_ALL,         0x8,  0 },
	{ 0x003F | JTAG_2059_ALL,         0x8,  0 },
	{ 0x0040 | JTAG_2059_CR0,        0x16,  0 },
	{ 0x0041 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0042 | JTAG_2059_CR0,        0x19,  0 },
	{ 0x0043 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0044 | JTAG_2059_CR0,         0x6,  0 },
	{ 0x0045 | JTAG_2059_CR0,         0x3,  0 },
	{ 0x0046 | JTAG_2059_CR0,         0x1,  0 },
	{ 0x0047 | JTAG_2059_CR0,         0x7,  0 },
	{ 0x0048 | JTAG_2059_ALL,         0x8,  0 },
	{ 0x0049 | JTAG_2059_ALL,         0x5,  0 },
	{ 0x004A | JTAG_2059_ALL,         0x8,  0 },
	{ 0x004B | JTAG_2059_ALL,         0x8,  0 },
	{ 0x004C | JTAG_2059_ALL,         0x8,  0 },
	{ 0x004D | JTAG_2059_CR0,           0,  0 },
	{ 0x004E | JTAG_2059_CR0,         0x4,  0 },
	{ 0x004F | JTAG_2059_ALL,         0xc,  0 },
	{ 0x0050 | JTAG_2059_ALL,           0,  0 },
	{ 0x0051 | JTAG_2059_ALL,        0x70,  1 },
	{ 0x0052 | JTAG_2059_ALL,         0x7,  0 },
	{ 0x0053 | JTAG_2059_ALL,           0,  0 },
	{ 0x0054 | JTAG_2059_ALL,           0,  0 },
	{ 0x0055 | JTAG_2059_ALL,        0x88,  0 },
	{ 0x0056 | JTAG_2059_ALL,           0,  0 },
	{ 0x0057 | JTAG_2059_ALL,        0x1f,  0 },
	{ 0x0058 | JTAG_2059_ALL,        0x20,  0 },
	{ 0x0059 | JTAG_2059_ALL,         0x1,  0 },
	{ 0x005A | JTAG_2059_ALL,         0x3,  1 },
	{ 0x005B | JTAG_2059_ALL,        0x70,  0 },
	{ 0x005C | JTAG_2059_ALL,           0,  0 },
	{ 0x005D | JTAG_2059_ALL,           0,  0 },
	{ 0x005E | JTAG_2059_ALL,        0x33,  0 },
	{ 0x005F | JTAG_2059_ALL,         0xf,  0 },
	{ 0x0060 | JTAG_2059_ALL,         0xf,  0 },
	{ 0x0061 | JTAG_2059_ALL,           0,  0 },
	{ 0x0062 | JTAG_2059_ALL,        0x11,  0 },
	{ 0x0063 | JTAG_2059_ALL,           0,  0 },
	{ 0x0064 | JTAG_2059_ALL,        0x7e,  0 },
	{ 0x0065 | JTAG_2059_ALL,        0x3f,  0 },
	{ 0x0066 | JTAG_2059_ALL,        0x7f,  0 },
	{ 0x0067 | JTAG_2059_ALL,        0x78,  0 },
	{ 0x0068 | JTAG_2059_ALL,        0x58,  0 },
	{ 0x0069 | JTAG_2059_ALL,        0x88,  0 },
	{ 0x006A | JTAG_2059_ALL,         0x8,  0 },
	{ 0x006B | JTAG_2059_ALL,         0xf,  0 },
	{ 0x006C | JTAG_2059_ALL,        0xbc,  0 },
	{ 0x006D | JTAG_2059_ALL,         0x8,  0 },
	{ 0x006E | JTAG_2059_ALL,        0x60,  0 },
	{ 0x006F | JTAG_2059_ALL,        0x13,  0 },
	{ 0x0070 | JTAG_2059_ALL,        0x70,  0 },
	{ 0x0071 | JTAG_2059_ALL,           0,  0 },
	{ 0x0072 | JTAG_2059_ALL,           0,  0 },
	{ 0x0073 | JTAG_2059_ALL,           0,  0 },
	{ 0x0074 | JTAG_2059_ALL,        0x33,  0 },
	{ 0x0075 | JTAG_2059_ALL,        0x13,  0 },
	{ 0x0076 | JTAG_2059_ALL,         0xf,  0 },
	{ 0x0077 | JTAG_2059_ALL,        0x11,  0 },
	{ 0x0078 | JTAG_2059_ALL,        0x3c,  0 },
	{ 0x0079 | JTAG_2059_ALL,         0x1,  1 },
	{ 0x007A | JTAG_2059_ALL,         0xa,  0 },
	{ 0x007B | JTAG_2059_ALL,        0x9d,  0 },
	{ 0x007C | JTAG_2059_ALL,         0xa,  0 },
	{ 0x007D | JTAG_2059_ALL,           0,  0 },
	{ 0x007E | JTAG_2059_ALL,        0x40,  0 },
	{ 0x007F | JTAG_2059_ALL,        0x40,  0 },
	{ 0x0080 | JTAG_2059_ALL,        0x88,  0 },
	{ 0x0081 | JTAG_2059_ALL,        0x10,  0 },
	{ 0x0082 | JTAG_2059_ALL,        0xf0,  0 },
	{ 0x0083 | JTAG_2059_ALL,        0x10,  0 },
	{ 0x0084 | JTAG_2059_ALL,        0xf0,  0 },
	{ 0x0085 | JTAG_2059_ALL,           0,  0 },
	{ 0x0086 | JTAG_2059_ALL,           0,  0 },
	{ 0x0087 | JTAG_2059_ALL,        0x10,  0 },
	{ 0x0088 | JTAG_2059_ALL,        0x55,  0 },
	{ 0x0089 | JTAG_2059_ALL,        0x3f,  0 },
	{ 0x008A | JTAG_2059_ALL,        0x36,  0 },
	{ 0x008B | JTAG_2059_ALL,           0,  0 },
	{ 0x008C | JTAG_2059_ALL,           0,  0 },
	{ 0x008D | JTAG_2059_ALL,           0,  0 },
	{ 0x008E | JTAG_2059_ALL,        0x87,  0 },
	{ 0x008F | JTAG_2059_ALL,        0x11,  0 },
	{ 0x0090 | JTAG_2059_ALL,           0,  0 },
	{ 0x0091 | JTAG_2059_ALL,        0x33,  0 },
	{ 0x0092 | JTAG_2059_ALL,        0x88,  0 },
	{ 0x0093 | JTAG_2059_ALL,           0,  0 },
	{ 0x0094 | JTAG_2059_ALL,        0x87,  0 },
	{ 0x0095 | JTAG_2059_ALL,        0x11,  0 },
	{ 0x0096 | JTAG_2059_ALL,           0,  0 },
	{ 0x0097 | JTAG_2059_ALL,        0x33,  0 },
	{ 0x0098 | JTAG_2059_ALL,        0x88,  0 },
	{ 0x0099 | JTAG_2059_ALL,        0x21,  0 },
	{ 0x009A | JTAG_2059_ALL,        0x3f,  0 },
	{ 0x009B | JTAG_2059_ALL,        0x44,  0 },
	{ 0x009C | JTAG_2059_ALL,        0x8c,  0 },
	{ 0x009D | JTAG_2059_ALL,        0x6c,  0 },
	{ 0x009E | JTAG_2059_ALL,        0x22,  0 },
	{ 0x009F | JTAG_2059_ALL,        0xbe,  0 },
	{ 0x00A0 | JTAG_2059_ALL,        0x55,  0 },
	{ 0x00A1 | JTAG_2059_ALL,         0xc,  0 },
	{ 0x00A2 | JTAG_2059_ALL,        0xaa,  0 },
	{ 0x00A3 | JTAG_2059_ALL,         0x2,  0 },
	{ 0x00A4 | JTAG_2059_ALL,           0,  0 },
	{ 0x00A5 | JTAG_2059_ALL,        0x10,  0 },
	{ 0x00A6 | JTAG_2059_ALL,         0x1,  0 },
	{ 0x00A7 | JTAG_2059_ALL,           0,  0 },
	{ 0x00A8 | JTAG_2059_ALL,           0,  0 },
	{ 0x00A9 | JTAG_2059_ALL,        0x80,  0 },
	{ 0x00AA | JTAG_2059_ALL,        0x60,  0 },
	{ 0x00AB | JTAG_2059_ALL,        0x44,  0 },
	{ 0x00AC | JTAG_2059_ALL,        0x55,  0 },
	{ 0x00AD | JTAG_2059_ALL,         0x1,  0 },
	{ 0x00AE | JTAG_2059_ALL,        0x55,  0 },
	{ 0x00AF | JTAG_2059_ALL,         0x1,  0 },
	{ 0x00B0 | JTAG_2059_ALL,         0x5,  0 },
	{ 0x00B1 | JTAG_2059_ALL,        0x55,  0 },
	{ 0x00B2 | JTAG_2059_ALL,        0x55,  0 },
	{ 0x00B3 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B4 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B5 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B6 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B7 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B8 | JTAG_2059_ALL,           0,  0 },
	{ 0x00B9 | JTAG_2059_ALL,           0,  0 },
	{ 0x00BA | JTAG_2059_ALL,           0,  0 },
	{ 0x00BB | JTAG_2059_ALL,           0,  0 },
	{ 0x00BC | JTAG_2059_ALL,           0,  0 },
	{ 0x00BD | JTAG_2059_ALL,           0,  0 },
	{ 0x00BE | JTAG_2059_ALL,           0,  0 },
	{ 0x00BF | JTAG_2059_CR2,           0,  0 },
	{ 0x00C0 | JTAG_2059_CR0,        0x5e,  0 },
	{ 0x00C1 | JTAG_2059_ALL,         0xc,  0 },
	{ 0x00C2 | JTAG_2059_ALL,         0xc,  0 },
	{ 0x00C3 | JTAG_2059_ALL,         0xc,  0 },
	{ 0x00C4 | JTAG_2059_ALL,           0,  0 },
	{ 0x00C5 | JTAG_2059_ALL,        0x2b,  0 },
	{ 0x013C | JTAG_2059_CR0,        0x15,  0 },
	{ 0x013D | JTAG_2059_CR0,         0xf,  0 },
	{ 0x013E | JTAG_2059_CR0,           0,  0 },
	{ 0x013F | JTAG_2059_CR0,           0,  0 },
	{ 0x0140 | JTAG_2059_CR0,           0,  0 },
	{ 0x0141 | JTAG_2059_CR0,           0,  0 },
	{ 0x0142 | JTAG_2059_CR0,           0,  0 },
	{ 0x0143 | JTAG_2059_CR0,           0,  0 },
	{ 0x0144 | JTAG_2059_CR0,           0,  0 },
	{ 0x0145 | JTAG_2059_CR2,           0,  0 },
	{ 0x0146 | JTAG_2059_ALL,           0,  0 },
	{ 0x0147 | JTAG_2059_ALL,           0,  0 },
	{ 0x0148 | JTAG_2059_ALL,           0,  0 },
	{ 0x0149 | JTAG_2059_ALL,           0,  0 },
	{ 0x014A | JTAG_2059_ALL,           0,  0 },
	{ 0x014B | JTAG_2059_ALL,           0,  0 },
	{ 0x014C | JTAG_2059_CR0,           0,  0 },
	{ 0x014D | JTAG_2059_CR0,           0,  0 },
	{ 0x014E | JTAG_2059_CR0,           0,  0 },
	{ 0x014F | JTAG_2059_ALL,           0,  0 },
	{ 0x0150 | JTAG_2059_ALL,           0,  0 },
	{ 0x0151 | JTAG_2059_ALL,        0x77,  0 },
	{ 0x0152 | JTAG_2059_ALL,        0x77,  0 },
	{ 0x0153 | JTAG_2059_ALL,        0x77,  0 },
	{ 0x0154 | JTAG_2059_ALL,        0x77,  0 },
	{ 0x0155 | JTAG_2059_ALL,           0,  0 },
	{ 0x0156 | JTAG_2059_ALL,         0x3,  0 },
	{ 0x0157 | JTAG_2059_ALL,        0x37,  0 },
	{ 0x0158 | JTAG_2059_ALL,         0x3,  0 },
	{ 0x0159 | JTAG_2059_ALL,           0,  0 },
	{ 0x015A | JTAG_2059_ALL,        0x21,  0 },
	{ 0x015B | JTAG_2059_ALL,        0x21,  0 },
	{ 0x015C | JTAG_2059_ALL,           0,  0 },
	{ 0x015D | JTAG_2059_ALL,        0xaa,  0 },
	{ 0x015E | JTAG_2059_ALL,           0,  0 },
	{ 0x015F | JTAG_2059_ALL,        0xaa,  0 },
	{ 0x0160 | JTAG_2059_ALL,           0,  0 },
	{ 0x0172 | JTAG_2059_ALL,         0x2,  0 },
	{ 0x0173 | JTAG_2059_ALL,         0xf,  0 },
	{ 0x0174 | JTAG_2059_ALL,         0xf,  0 },
	{ 0x0175 | JTAG_2059_ALL,           0,  0 },
	{ 0x0176 | JTAG_2059_ALL,           0,  0 },
	{ 0x0177 | JTAG_2059_ALL,           0,  0 },
	{ 0x017E | JTAG_2059_ALL,        0x84,  0 },
	{ 0x017F | JTAG_2059_CR0,        0x60,  0 },
	{ 0x0180 | JTAG_2059_ALL,        0x47,  0 },
	{ 0x0182 | JTAG_2059_ALL,           0,  0 },
	{ 0x0183 | JTAG_2059_ALL,           0,  0 },
	{ 0x0184 | JTAG_2059_ALL,           0,  0 },
	{ 0x0185 | JTAG_2059_ALL,           0,  0 },
	{ 0x0186 | JTAG_2059_ALL,           0,  0 },
	{ 0x0187 | JTAG_2059_ALL,           0,  0 },
	{ 0x0188 | JTAG_2059_ALL,         0x5,  1 },
	{ 0x0189 | JTAG_2059_ALL,           0,  0 },
	{ 0x018A | JTAG_2059_ALL,           0,  0 },
	{ 0x018B | JTAG_2059_ALL,           0,  0 },
	{ 0x018C | JTAG_2059_ALL,           0,  0 },
	{ 0x018D | JTAG_2059_ALL,           0,  0 },
	{ 0x018E | JTAG_2059_ALL,           0,  0 },
	{ 0x018F | JTAG_2059_ALL,           0,  0 },
	{ 0x019A | JTAG_2059_ALL,           0,  0 },
	{ 0x019B | JTAG_2059_CR2,           0,  0 },
	{ 0x019C | JTAG_2059_ALL,           0,  0 },
	{ 0x019D | JTAG_2059_ALL,         0x2,  0 },
	{ 0x019E | JTAG_2059_ALL,         0x1,  0 },
	{ 0x019F | JTAG_2059_ALL,           0,  0 },
	{ 0x01A0 | JTAG_2059_ALL,         0x1,  0 },
	{ 0xFFFF,                           0,  0 },
};

/* %%%%%% tx gain table */

static uint32 tpc_txgain_htphy_rev0_2g[] = {
	0x10f90040, 0x10e10040, 0x10e1003c, 0x10c9003d,
	0x10b9003c, 0x10a9003d, 0x10a1003c, 0x1099003b,
	0x1091003b, 0x1089003a, 0x1081003a, 0x10790039,
	0x10710039, 0x1069003a, 0x1061003b, 0x1059003d,
	0x1051003f, 0x10490042, 0x1049003e, 0x1049003b,
	0x1041003e, 0x1041003b, 0x1039003e, 0x1039003b,
	0x10390038, 0x10390035, 0x1031003a, 0x10310036,
	0x10310033, 0x1029003a, 0x10290037, 0x10290034,
	0x10290031, 0x10210039, 0x10210036, 0x10210033,
	0x10210030, 0x1019003c, 0x10190039, 0x10190036,
	0x10190033, 0x10190030, 0x1019002d, 0x1019002b,
	0x10190028, 0x1011003a, 0x10110036, 0x10110033,
	0x10110030, 0x1011002e, 0x1011002b, 0x10110029,
	0x10110027, 0x10110024, 0x10110022, 0x10110020,
	0x1011001f, 0x1011001d, 0x1009003a, 0x10090037,
	0x10090034, 0x10090031, 0x1009002e, 0x1009002c,
	0x10090029, 0x10090027, 0x10090025, 0x10090023,
	0x10090021, 0x1009001f, 0x1009001d, 0x1009001b,
	0x1009001a, 0x10090018, 0x10090017, 0x10090016,
	0x10090015, 0x10090013, 0x10090012, 0x10090011,
	0x10090010, 0x1009000f, 0x1009000f, 0x1009000e,
	0x1009000d, 0x1009000c, 0x1009000c, 0x1009000b,
	0x1009000a, 0x1009000a, 0x10090009, 0x10090009,
	0x10090008, 0x10090008, 0x10090007, 0x10090007,
	0x10090007, 0x10090006, 0x10090006, 0x10090005,
	0x10090005, 0x10090005, 0x10090005, 0x10090004,
	0x10090004, 0x10090004, 0x10090004, 0x10090003,
	0x10090003, 0x10090003, 0x10090003, 0x10090003,
	0x10090003, 0x10090002, 0x10090002, 0x10090002,
	0x10090002, 0x10090002, 0x10090002, 0x10090002,
	0x10090002, 0x10090002, 0x10090001, 0x10090001,
	0x10090001, 0x10090001, 0x10090001, 0x10090001,
};

static uint32 tpc_txgain_htphy_rev0_5g[] = {
	0x2f590048, 0x2f590044, 0x2f590040, 0x2f59003d,
	0x2f590039, 0x2e590044, 0x2e590040, 0x2e59003d,
	0x2e590039, 0x2d590042, 0x2d59003e, 0x2d59003b,
	0x2c590046, 0x2c590042, 0x2c59003f, 0x2c59003b,
	0x2b590040, 0x2b59003d, 0x2b590039, 0x2a590044,
	0x2a590040, 0x2a59003d, 0x2a590039, 0x29590043,
	0x2959003f, 0x2959003c, 0x28590045, 0x28590041,
	0x2859003d, 0x2859003a, 0x27590045, 0x27590041,
	0x2759003d, 0x2759003a, 0x26590044, 0x26590040,
	0x2659003d, 0x26590039, 0x25590043, 0x2559003f,
	0x2559003c, 0x24590046, 0x24590042, 0x2459003f,
	0x2459003b, 0x23590045, 0x23590041, 0x2359003d,
	0x2359003a, 0x22590042, 0x2259003f, 0x2259003b,
	0x21590047, 0x21590043, 0x2159003f, 0x2159003c,
	0x20590045, 0x20590041, 0x2059003d, 0x2059003a,
	0x20590037, 0x20590034, 0x20590031, 0x2059002e,
	0x2059002b, 0x20590029, 0x20590027, 0x20590024,
	0x20590022, 0x20590021, 0x2059001f, 0x2059001d,
	0x2059001b, 0x2059001a, 0x20590018, 0x20590017,
	0x20590016, 0x20590015, 0x20590013, 0x20590012,
	0x20590011, 0x20590010, 0x2059000f, 0x2059000f,
	0x2059000e, 0x2059000d, 0x2059000c, 0x2059000c,
	0x2059000b, 0x2059000a, 0x2059000a, 0x20590009,
	0x20590009, 0x20590008, 0x20590008, 0x20590007,
	0x20590007, 0x20590006, 0x20590006, 0x20590006,
	0x20590005, 0x20590005, 0x20590005, 0x20590005,
	0x20590004, 0x20590004, 0x20590004, 0x20590004,
	0x20590003, 0x20590003, 0x20590003, 0x20590003,
	0x20590003, 0x20590003, 0x20590002, 0x20590002,
	0x20590002, 0x20590002, 0x20590002, 0x20590002,
	0x20590002, 0x20590002, 0x20590002, 0x20590001,
	0x20590001, 0x20590001, 0x20590001, 0x20590001,
};

typedef struct {
	uint8 percent;
	uint8 g_env;
} htphy_txiqcal_ladder_t;

typedef struct {
	uint8 nwords;
	uint8 offs;
	uint8 boffs;
} htphy_coeff_access_t;


/* Get Rx power on core 0 */
#define HTPHY_RXPWR_ANT0(rxs)	((((rxs)->PhyRxStatus_2) & PRXS2_HTPHY_RXPWR_ANT0) >> 8)
/* Get Rx power on core 1 */
#define HTPHY_RXPWR_ANT1(rxs)	(((rxs)->PhyRxStatus_3) & PRXS3_HTPHY_RXPWR_ANT1)
/* Get Rx power on core 2 */
#define HTPHY_RXPWR_ANT2(rxs)	((((rxs)->PhyRxStatus_3) & PRXS3_HTPHY_RXPWR_ANT2) >> 8)


/* defs for iqlo cal */
enum {  /* mode selection for reading/writing tx iqlo cal coefficients */
	TB_START_COEFFS_AB, TB_START_COEFFS_D, TB_START_COEFFS_E, TB_START_COEFFS_F,
	TB_BEST_COEFFS_AB,  TB_BEST_COEFFS_D,  TB_BEST_COEFFS_E,  TB_BEST_COEFFS_F,
	TB_OFDM_COEFFS_AB,  TB_OFDM_COEFFS_D,  TB_BPHY_COEFFS_AB,  TB_BPHY_COEFFS_D,
	PI_INTER_COEFFS_AB, PI_INTER_COEFFS_D, PI_INTER_COEFFS_E, PI_INTER_COEFFS_F,
	PI_FINAL_COEFFS_AB, PI_FINAL_COEFFS_D, PI_FINAL_COEFFS_E, PI_FINAL_COEFFS_F
};

#define CAL_TYPE_IQ                 0
#define CAL_TYPE_LOFT_DIG           2
#define CAL_TYPE_LOFT_ANA_FINE      3
#define CAL_TYPE_LOFT_ANA_COARSE    4

#define CAL_COEFF_READ    0
#define CAL_COEFF_WRITE   1
#define MPHASE_TXCAL_CMDS_PER_PHASE  2 /* number of tx iqlo cal commands per phase in mphase cal */

#define IQTBL_CACHE_COOKIE_OFFSET	95
#define TXCAL_CACHE_VALID		0xACDC

/* %%%%%% function declaration */
static void wlc_phy_get_txgain_settings_by_index(phy_info_t *pi, txgain_setting_t *txgain_settings,
	int8 txpwrindex);
static void wlc_phy_write_txmacreg_htphy(wlc_phy_t *ppi, uint16 holdoff, uint16 delay);
static void wlc_phy_txpwrctrl_config_htphy(phy_info_t *pi);
static void wlc_phy_resetcca_htphy(phy_info_t *pi);
static void wlc_phy_workarounds_htphy(phy_info_t *pi);
static void wlc_phy_subband_cust_htphy(phy_info_t *pi);

static void wlc_phy_radio2059_init(phy_info_t *pi);
static bool wlc_phy_chan2freq_htphy(phy_info_t *pi, uint channel, int *f,
                                    chan_info_htphy_radio2059_t **t);
static void wlc_phy_chanspec_setup_htphy(phy_info_t *pi, chanspec_t chans,
                                         const htphy_sfo_cfg_t *c);

static void wlc_phy_txlpfbw_htphy(phy_info_t *pi);
static void wlc_phy_spurwar_htphy(phy_info_t *pi);

static int  wlc_phy_cal_txiqlo_htphy(phy_info_t *pi, uint8 searchmode, uint8 mphase);
static void wlc_phy_cal_txiqlo_coeffs_htphy(phy_info_t *pi,
	uint8 rd_wr, uint16 *coeffs, uint8 select, uint8 core);
static void wlc_phy_precal_txgain_htphy(phy_info_t *pi, txgain_setting_t *target_gains);
static void wlc_phy_txcal_txgain_setup_htphy(phy_info_t *pi, txgain_setting_t *txcal_txgain,
	txgain_setting_t *orig_txgain);
static void wlc_phy_txcal_txgain_cleanup_htphy(phy_info_t *pi, txgain_setting_t *orig_txgain);
static void wlc_phy_txcal_radio_setup_htphy(phy_info_t *pi);
static void wlc_phy_txcal_radio_cleanup_htphy(phy_info_t *pi);
static void wlc_phy_txcal_phy_setup_htphy(phy_info_t *pi);
static void wlc_phy_txcal_phy_cleanup_htphy(phy_info_t *pi);


static void wlc_phy_cal_txiqlo_update_ladder_htphy(phy_info_t *pi, uint16 bbmult);

static void wlc_phy_extpa_set_tx_digi_filts_htphy(phy_info_t *pi);

static void wlc_phy_clip_det_htphy(phy_info_t *pi, uint8 write, uint16 *vals);

static bool wlc_phy_txpwr_srom_read_htphy(phy_info_t *pi);

static void wlc_phy_txpwr_fixpower_htphy(phy_info_t *pi);
static void wlc_phy_tssi_radio_setup_htphy(phy_info_t *pi, uint8 core_mask);
static void wlc_phy_txpwrctrl_idle_tssi_meas_htphy(phy_info_t *pi);
static void wlc_phy_txpwrctrl_pwr_setup_htphy(phy_info_t *pi);

static void wlc_phy_get_txgain_settings_by_index(phy_info_t *pi, txgain_setting_t *txgain_settings,
                                                 int8 txpwrindex);
static bool wlc_phy_txpwrctrl_ison_htphy(phy_info_t *pi);
static void wlc_phy_txpwrctrl_set_idle_tssi_htphy(phy_info_t *pi, int8 idle_tssi, uint8 core);
static void wlc_phy_txpwrctrl_set_target_htphy(phy_info_t *pi, uint8 pwr_qtrdbm, uint8 core);
static uint8 wlc_phy_txpwrctrl_get_cur_index_htphy(phy_info_t *pi, uint8 core);
static void wlc_phy_txpwrctrl_set_cur_index_htphy(phy_info_t *pi, uint8 idx, uint8 core);

static uint16 wlc_phy_gen_load_samples_htphy(phy_info_t *pi, uint32 f_kHz, uint16 max_val,
                                             uint8 dac_test_mode);
static void wlc_phy_loadsampletable_htphy(phy_info_t *pi, cint32 *tone_buf, uint16 num_samps);
static void wlc_phy_init_lpf_sampleplay(phy_info_t *pi);
static void wlc_phy_runsamples_htphy(phy_info_t *pi, uint16 n, uint16 lps, uint16 wait, uint8 iq,
                                     uint8 dac_test_mode, bool modify_bbmult);

static void wlc_phy_rx_iq_comp_htphy(phy_info_t *pi,
	uint8 write, phy_iq_comp_t *pcomp, uint8 rx_core);
static void wlc_phy_cal_dac_adc_decouple_war_htphy(phy_info_t *pi, bool do_not_undo);
static void wlc_phy_rx_iq_est_loopback_htphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
                                             uint8 wait_time, uint8 wait_for_crs);
static int  wlc_phy_calc_ab_from_iq_htphy(phy_iq_est_t *est, phy_iq_comp_t *comp);
static void wlc_phy_calc_rx_iq_comp_htphy(phy_info_t *pi, uint16 num_samps, uint8 core_mask);
static void wlc_phy_rxcal_radio_config_htphy(phy_info_t *pi, bool setup_not_cleanup, uint8 rx_core);
static void wlc_phy_rxcal_phy_config_htphy(phy_info_t *pi, bool setup_not_cleanup, uint8 rx_core);
static void wlc_phy_rxcal_gainctrl_htphy(phy_info_t *pi, uint8 rx_core);
static int  wlc_phy_cal_rxiq_htphy(phy_info_t *pi, bool debug);


/* %%%%%% function implementation */

/* Start phy init code from d11procs.tcl */
/* Initialize the bphy in an htphy */
static void
WLBANDINITFN(wlc_phy_bphy_init_htphy)(phy_info_t *pi)
{
	uint16	addr, val;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(ISHTPHY(pi));

	/* RSSI LUT is now a memory and must therefore be initialized */
	val = 0x1e1f;
	for (addr = (HTPHY_TO_BPHY_OFF + BPHY_RSSI_LUT);
	     addr <= (HTPHY_TO_BPHY_OFF + BPHY_RSSI_LUT_END); addr++) {
		write_phy_reg(pi, addr, val);
		if (addr == (HTPHY_TO_BPHY_OFF + 0x97))
			val = 0x3e3f;
		else
			val -= 0x0202;
	}

	if (ISSIM_ENAB(pi->sh->sih)) {
		/* only for use on QT */

		/* CRS thresholds */
		write_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_PHYCRSTH, 0x3206);

		/* RSSI thresholds */
		write_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_RSSI_TRESH, 0x281e);

		/* LNA gain range */
		or_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_LNA_GAIN_RANGE, 0x1a);
	} else	{
		/* kimmer - add change from 0x667 to x668 very slight improvement */
		write_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_STEP, 0x668);
	}
}

void
wlc_phy_scanroam_cache_cal_htphy(phy_info_t *pi, bool set)
{
	uint16 tbl_cookie;
	uint16 ab_int[2];
	uint8 core;

	PHY_TRACE(("wl%d: %s: in scan/roam set %d\n", pi->sh->unit, __FUNCTION__, set));
	wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
		IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);

	if (set) {
		if (tbl_cookie == TXCAL_CACHE_VALID) {
			PHY_CAL(("wl%d: %s: save the txcal for scan/roam\n",
				pi->sh->unit, __FUNCTION__));
			/* save the txcal to cache */
			FOREACH_CORE(pi, core) {
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					ab_int, TB_OFDM_COEFFS_AB, core);
				pi->txcal_cache[core].txa = ab_int[0];
				pi->txcal_cache[core].txb = ab_int[1];
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					&pi->txcal_cache[core].txd, TB_OFDM_COEFFS_D, core);
				pi->txcal_cache[core].txei = (uint8)read_radio_reg(pi,
					RADIO_2059_TX_LOFT_FINE_I(core));
				pi->txcal_cache[core].txeq = (uint8)read_radio_reg(pi,
					RADIO_2059_TX_LOFT_FINE_Q(core));
				pi->txcal_cache[core].txfi = (uint8)read_radio_reg(pi,
					RADIO_2059_TX_LOFT_COARSE_I(core));
				pi->txcal_cache[core].txfq = (uint8)read_radio_reg(pi,
					RADIO_2059_TX_LOFT_COARSE_Q(core));
				pi->txcal_cache[core].rxa = read_phy_reg(pi, HTPHY_RxIQCompA(core));
				pi->txcal_cache[core].rxb = read_phy_reg(pi, HTPHY_RxIQCompB(core));
			}
			/* mark the cache as valid */
			pi->txcal_cache_cookie = tbl_cookie;
		}
	} else {
		if (pi->txcal_cache_cookie == TXCAL_CACHE_VALID &&
		   tbl_cookie != pi->txcal_cache_cookie) {
			PHY_CAL(("wl%d: %s: restore the txcal after scan/roam\n",
				pi->sh->unit, __FUNCTION__));
			/* restore the txcal from cache */
			FOREACH_CORE(pi, core) {
				ab_int[0] = pi->txcal_cache[core].txa;
				ab_int[1] = pi->txcal_cache[core].txb;
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					ab_int, TB_OFDM_COEFFS_AB, core);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					&pi->txcal_cache[core].txd, TB_OFDM_COEFFS_D, core);
				write_radio_reg(pi, RADIO_2059_TX_LOFT_FINE_I(core),
					pi->txcal_cache[core].txei);
				write_radio_reg(pi, RADIO_2059_TX_LOFT_FINE_Q(core),
					pi->txcal_cache[core].txeq);
				write_radio_reg(pi, RADIO_2059_TX_LOFT_COARSE_I(core),
					pi->txcal_cache[core].txfi);
				write_radio_reg(pi, RADIO_2059_TX_LOFT_COARSE_Q(core),
					pi->txcal_cache[core].txfq);
				write_phy_reg(pi, HTPHY_RxIQCompA(core), pi->txcal_cache[core].rxa);
				write_phy_reg(pi, HTPHY_RxIQCompB(core), pi->txcal_cache[core].rxb);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
					IQTBL_CACHE_COOKIE_OFFSET, 16, &pi->txcal_cache_cookie);
			}
			pi->txcal_cache_cookie = 0;
		}
	}
}

void
wlc_phy_table_write_htphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                          const void *data)
{
	htphytbl_info_t tbl;
	/*
	 * PHY_TRACE(("wlc_phy_table_write_htphy, id %d, len %d, offset %d, width %d\n",
	 * 	id, len, offset, width));
	*/
	tbl.tbl_id = id;
	tbl.tbl_len = len;
	tbl.tbl_offset = offset;
	tbl.tbl_width = width;
	tbl.tbl_ptr = data;
	wlc_phy_write_table_htphy(pi, &tbl);
}

void
wlc_phy_table_read_htphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                         void *data)
{
	htphytbl_info_t tbl;
	/*	PHY_TRACE(("wlc_phy_table_read_htphy, id %d, len %d, offset %d, width %d\n",
	 *	id, len, offset, width));
	 */
	tbl.tbl_id = id;
	tbl.tbl_len = len;
	tbl.tbl_offset = offset;
	tbl.tbl_width = width;
	tbl.tbl_ptr = data;
	wlc_phy_read_table_htphy(pi, &tbl);
}


/* initialize the static tables defined in auto-generated wlc_phytbl_ht.c,
 * see htphyprocs.tcl, proc htphy_init_tbls
 * After called in the attach stage, all the static phy tables are reclaimed.
 */
static void
WLBANDINITFN(wlc_phy_static_table_download_htphy)(phy_info_t *pi)
{
	uint idx;

	/* these tables are not affected by phy reset, only power down */
	for (idx = 0; idx < htphytbl_info_sz_rev0; idx++) {
		wlc_phy_write_table_htphy(pi, &htphytbl_info_rev0[idx]);
	}
}

/* initialize all the tables defined in auto-generated wlc_phytbl_ht.c,
 * see htphyprocs.tcl, proc htphy_init_tbls
 *  skip static one after first up
 */
static void
WLBANDINITFN(wlc_phy_tbl_init_htphy)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s, dnld tables = %d\n", pi->sh->unit,
	          __FUNCTION__, pi->phy_init_por));

	/* these tables are not affected by phy reset, only power down */
	if (pi->phy_init_por)
		wlc_phy_static_table_download_htphy(pi);

	/* init volatile tables */
}

void
wlc_phy_write_txmacreg_htphy(wlc_phy_t *ppi, uint16 holdoff, uint16 delay)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	write_phy_reg(pi, HTPHY_TxMacIfHoldOff, holdoff);
	write_phy_reg(pi, HTPHY_TxMacDelay, delay);
}

static void
wlc_phy_detach_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht;

	ASSERT(pi);
	ASSERT(pi->pi_ptr);
	pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	if (pi_ht == NULL)
		return;
	MFREE(pi->sh->osh, pi_ht, sizeof(phy_info_htphy_t));
}

bool
wlc_phy_attach_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht;
	shared_phy_t *sh;
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(pi);
	if (!pi) {
		PHY_ERROR(("wl: wlc_phy_attach_htphy: NULL pi\n"));
		return FALSE;
	}

	sh = pi->sh;

	if ((pi_ht = (phy_info_htphy_t *) MALLOC(sh->osh, sizeof(phy_info_htphy_t))) == NULL) {
		PHY_ERROR(("wl%d: wlc_phy_attach_htphy: out of memory, malloced %d bytes\n",
			sh->unit, MALLOCED(sh->osh)));
		return FALSE;
	}
	bzero((char*)pi_ht, sizeof(phy_info_htphy_t));
	pi->pi_ptr = (void *)pi_ht;

	/* pre_init to ON, register POR default setting */
	pi_ht->ht_rxldpc_override = ON;


	pi->n_preamble_override = AUTO;

	pi->phy_scraminit = AUTO;

	pi->phy_cal_mode = PHY_PERICAL_MPHASE;

	pi->radio_is_on = FALSE;

	for (core = 0; core < PHY_CORE_MAX; core++) {
		pi_ht->txpwrindex_hw_save[core] = 128;
	}

	wlc_phy_txpwrctrl_config_htphy(pi);

	if (!wlc_phy_txpwr_srom_read_htphy(pi))
		goto exit;

#if defined(AP) && defined(RADAR)
	/* wlc_phy_radar_attach_htphy(pi); */
#endif /* defined(AP) && defined(RADAR) */

	/* setup function pointers */
	pi->pi_fptr.init = wlc_phy_init_htphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_htphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_htphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_htphy;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = NULL;
#endif
	pi->pi_fptr.txiqccget = NULL;
	pi->pi_fptr.txiqccset = NULL;
	pi->pi_fptr.txloccget = NULL;
	pi->pi_fptr.radioloftget = NULL;
	pi->pi_fptr.carrsuppr = NULL;
#if defined(WLTEST)
	pi->pi_fptr.rxsigpwr = NULL;
#endif
	pi->pi_fptr.detach = wlc_phy_detach_htphy;

	return TRUE;

exit:
	if (pi_ht != NULL) {
		MFREE(pi->sh->osh, pi_ht, sizeof(phy_info_htphy_t));
		pi->pi_ptr = NULL;
	}
	return FALSE;
}

static void
BCMATTACHFN(wlc_phy_txpwrctrl_config_htphy)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi->hwpwrctrl_capable = TRUE;
	pi->txpwrctrl = PHY_TPC_HW_ON;

	pi->phy_5g_pwrgain = TRUE;
}

void
WLBANDINITFN(wlc_phy_init_htphy)(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 val;
	uint16 clip1_ths[PHY_CORE_MAX];
	uint8 tx_pwr_ctrl_state = PHY_TPC_HW_OFF;
	uint16 core;
	uint32 *tx_pwrctrl_tbl = NULL;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi_ht->deaf_count = 0;

	/* Step 0, initialize tables */
	wlc_phy_tbl_init_htphy(pi);

	/* Load preferred values */
	MOD_PHYREG(pi, HTPHY_BphyControl1, flipiq_adccompout, 0);
	FOREACH_CORE(pi, core) {
		mod_phy_reg(pi, HTPHY_forceFront(core), 0x7ff, 0x7ff);
	}

	/* Reset Ch11 40 MHz spur avoidance WAR flags */

	/***********************************
	 * Rfctrl, Rfseq, and Afectrl setup
	 */

	/* Step 1, power up and reset radio
	 * Step 2, clear force reset to radio
	 * NOT DONE HERE -- already done in wlc_phy_radio2059_reset
	 */

	/* Step 3, write 0x0 to rfctrloverride regs
	 * remove rfctrl signal override
	 */
	FOREACH_CORE(pi, core) {
		write_phy_reg(pi, HTPHY_RfctrlOverride(core), 0);
		write_phy_reg(pi, HTPHY_RfctrlOverrideAux(core), 0);
		write_phy_reg(pi, HTPHY_RfctrlOverrideLpfCT(core), 0);
		write_phy_reg(pi, HTPHY_RfctrlOverrideLpfPU(core), 0);
	}

	/* Step 4, clear bit in RfctrlIntcX to give the control to rfseq
	 * for the antenna for coreX
	 */
	FOREACH_CORE(pi, core) {
		write_phy_reg(pi, HTPHY_RfctrlIntc(core), 0);
	}

	/* Step 8, Write 0x0 to RfseqMode to turn off both CoreActv_override
	 * (to give control to Tx control word) and Trigger_override (to give
	 * control to rfseq)
	 *
	 * Now you are done with all rfseq INIT.
	 */
	and_phy_reg(pi, HTPHY_RfseqMode, ~3);

	/* Step 9, write 0x0 to AfectrlOverride to give control to Auto
	 * control mode.
	 */
	FOREACH_CORE(pi, core) {
		write_phy_reg(pi, HTPHY_AfectrlOverride(core), 0);
	}

	/* gdk: not sure what these do but they are used in wmac */
	write_phy_reg(pi, HTPHY_AfeseqTx2RxPwrUpDownDly20M, 32);
	write_phy_reg(pi, HTPHY_AfeseqTx2RxPwrUpDownDly40M, 32);

	write_phy_reg(pi, HTPHY_TxRealFrameDelay, 184);

	/* Turn on TxCRS extension.
	 * (Need to eventually make the 1.0 be native TxCRSOff (1.0us))
	 */
	write_phy_reg(pi, HTPHY_mimophycrsTxExtension, 200);

	write_phy_reg(pi, HTPHY_payloadcrsExtensionLen, 80);

	/* This number combined with MAC RIFS results in 2.0us RIFS air time */
	write_phy_reg(pi, HTPHY_TxRifsFrameDelay, 48);

	/* LDPC RX settings */
	wlc_phy_update_rxldpc_htphy(pi, ON);

	/* set tx/rx chain */

	wlc_phy_extpa_set_tx_digi_filts_htphy(pi);

	wlc_phy_workarounds_htphy(pi);

	/* Configure Sample Play Buffer's LPF-BW's in RfSeq */
	wlc_phy_init_lpf_sampleplay(pi);

	/* Pulse reset_cca after initing all the tables */
	wlapi_bmac_phyclk_fgc(pi->sh->physhim, ON);

	val = read_phy_reg(pi, HTPHY_BBConfig);
	write_phy_reg(pi, HTPHY_BBConfig, val | BBCFG_RESETCCA);
	write_phy_reg(pi, HTPHY_BBConfig, val & (~BBCFG_RESETCCA));
	wlapi_bmac_phyclk_fgc(pi->sh->physhim, OFF);

	wlapi_bmac_macphyclk_set(pi->sh->physhim, ON);

	/* trigger a rx2tx to get TX lpf bw and gain latched into 205x,
	 * trigger a reset2rx seq
	 */
	wlc_phy_pa_override_htphy(pi, OFF);
	wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RX2TX);
	wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RESET2RX);
	wlc_phy_pa_override_htphy(pi, ON);


	/* example calls to avoid compiler warnings, not used elsewhere yet: */
	wlc_phy_classifier_htphy(pi, 0, 0);
	wlc_phy_clip_det_htphy(pi, 0, clip1_ths);

	/* Initialize the bphy part */
	if (CHSPEC_IS2G(pi->radio_chanspec))
		wlc_phy_bphy_init_htphy(pi);

	/* Load tx gain table */
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		tx_pwrctrl_tbl = tpc_txgain_htphy_rev0_5g;
	} else {
		tx_pwrctrl_tbl = tpc_txgain_htphy_rev0_2g;
	}
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_TXPWRCTL(0), 128, 192, 32, tx_pwrctrl_tbl);

	/* set txgain in case txpwrctrl is disabled */
	wlc_phy_txpwr_fixpower_htphy(pi);

	/* ensure power control is off before starting cals */
	tx_pwr_ctrl_state = pi->txpwrctrl;
	wlc_phy_txpwrctrl_enable_htphy(pi, PHY_TPC_HW_OFF);

	/* Idle TSSI measurement and pwrctrl setup */
	wlc_phy_txpwrctrl_idle_tssi_meas_htphy(pi);
	wlc_phy_txpwrctrl_pwr_setup_htphy(pi);

	/* Set up radio muxes for TSSI
	 * Note: Idle TSSI measurement above may return early w/o
	 *       setting up the muxes for TSSI, see 
	 *       wlc_phy_txpwrctrl_idle_tssi_meas_htphy().
	 */
	wlc_phy_tssi_radio_setup_htphy(pi, 7);

	/* If any rx cores were disabled before htphy_init,
	 *  disable them again since htphy_init enables all rx cores
	 */
	if (pi->sh->phyrxchain != 0x7) {
		wlc_phy_rxcore_setstate_htphy((wlc_phy_t *)pi, pi->sh->phyrxchain);
	}


	/* re-enable (if necessary) tx power control */
	wlc_phy_txpwrctrl_enable_htphy(pi, tx_pwr_ctrl_state);

#if defined(AP) && defined(RADAR)
	/* Initialze Radar detect, on or off */
	wlc_phy_radar_detect_init(pi, pi->sh->radar);
#endif /* defined(AP) && defined(RADAR) */

	/* Set the analog TX_LPF Bandwidth */
	wlc_phy_txlpfbw_htphy(pi);

	/* Spur avoidance WAR */
	/* wlc_phy_spurwar_htphy(pi); */

	/* ACI mode inits */

}

/* enable/disable receiving of LDPC frame */
void
wlc_phy_update_rxldpc_htphy(phy_info_t *pi, bool ldpc)
{
	phy_info_htphy_t *pi_ht;
	uint16 val, bit;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	if (ldpc == pi_ht->ht_rxldpc_override)
		return;

	pi_ht->ht_rxldpc_override = ldpc;
	val = read_phy_reg(pi, HTPHY_HTSigTones);

	bit = HTPHY_HTSigTones_support_ldpc_MASK;
	if (ldpc)
		val |= bit;
	else
		val &= ~bit;
	write_phy_reg(pi, HTPHY_HTSigTones, val);
}

static void
wlc_phy_resetcca_htphy(phy_info_t *pi)
{
	uint16 val;

	/* MAC should be suspended before calling this function */
	ASSERT(0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

	wlapi_bmac_phyclk_fgc(pi->sh->physhim, ON);

	val = read_phy_reg(pi, HTPHY_BBConfig);
	write_phy_reg(pi, HTPHY_BBConfig, val | BBCFG_RESETCCA);
	OSL_DELAY(1);
	write_phy_reg(pi, HTPHY_BBConfig, val & (~BBCFG_RESETCCA));

	wlapi_bmac_phyclk_fgc(pi->sh->physhim, OFF);

	wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RESET2RX);
}

void
wlc_phy_pa_override_htphy(phy_info_t *pi, bool en)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	FOREACH_CORE(pi, core) {
		if (!en) {
			/* switch Off both PA's */
			pi_ht->rfctrlIntc_save[core] = read_phy_reg(pi, HTPHY_RfctrlIntc(core));
			write_phy_reg(pi, HTPHY_RfctrlIntc(core), 0x400);
		} else {
			/* restore Rfctrl override settings */
			write_phy_reg(pi, HTPHY_RfctrlIntc(core), pi_ht->rfctrlIntc_save[core]);
		}
	}
}

void
wlc_phy_anacore_htphy(phy_info_t *pi,  bool on)
{
	uint16 core;

	PHY_TRACE(("wl%d: %s %d\n", pi->sh->unit, __FUNCTION__, on));

	FOREACH_CORE(pi, core) {
		if (on) {
			write_phy_reg(pi, HTPHY_Afectrl(core), 0xcd);
			write_phy_reg(pi, HTPHY_AfectrlOverride(core), 0x0);
		} else {
			write_phy_reg(pi, HTPHY_AfectrlOverride(core), 0x07ff);
			write_phy_reg(pi, HTPHY_Afectrl(core), 0x0fd);
		}
	}
}


void
wlc_phy_rxcore_setstate_htphy(wlc_phy_t *pih, uint8 rxcore_bitmask)
{
	phy_info_t *pi = (phy_info_t*)pih;
	uint16 rfseqCoreActv_DisRx_save, rfseqCoreActv_EnTx_save;
	uint16 rfseqMode_save, sampleDepthCount_save, sampleLoopCount_save;
	uint16 sampleInitWaitCount_save, sampleCmd_save;

	ASSERT((rxcore_bitmask > 0) && (rxcore_bitmask <= 7));

	pi->sh->phyrxchain = rxcore_bitmask;

	if (!pi->sh->clk)
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	/* Save Registers */
	rfseqCoreActv_DisRx_save = READ_PHYREG(pi, HTPHY_RfseqCoreActv2059, DisRx);
	rfseqCoreActv_EnTx_save = READ_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnTx);
	rfseqMode_save = read_phy_reg(pi, HTPHY_RfseqMode);
	sampleDepthCount_save = read_phy_reg(pi, HTPHY_sampleDepthCount);
	sampleLoopCount_save = read_phy_reg(pi, HTPHY_sampleLoopCount);
	sampleInitWaitCount_save = read_phy_reg(pi, HTPHY_sampleInitWaitCount);
	sampleCmd_save = read_phy_reg(pi, HTPHY_sampleCmd);

	/* Indicate to PHY of the Inactive Core */
	MOD_PHYREG(pi, HTPHY_CoreConfig, CoreMask, rxcore_bitmask);
	/* Indicate to RFSeq of the Inactive Core */
	MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnRx, rxcore_bitmask);
	/* Make sure Rx Chain gets shut off in Rx2Tx Sequence */
	MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, DisRx, 7);
	/* Make sure Tx Chain doesn't get turned off during this function */
	MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnTx, 0);
	MOD_PHYREG(pi, HTPHY_RfseqMode, CoreActv_override, 1);

	/* To Turn off the AFE:
	 * TxFrame needs to toggle on and off.
	 * Accomplished by A) turning sample play ON and OFF
	 * Rx2Tx and Tx2Rx Rfseq is executed during A)
	 * To Turn Off the Radio (except LPF) - Do a Rx2Tx
	 * To Turn off the LPF - Do a Tx2Rx
	 */
	write_phy_reg(pi, HTPHY_sampleDepthCount, 0);
	write_phy_reg(pi, HTPHY_sampleLoopCount, 0xffff);
	write_phy_reg(pi, HTPHY_sampleInitWaitCount, 0);
	MOD_PHYREG(pi, HTPHY_sampleCmd, DisTxFrameInSampleplay, 0);
	MOD_PHYREG(pi, HTPHY_sampleCmd, start, 1);

	/* Allow Time For Rfseq to start */
	OSL_DELAY(1);
	/* Allow Time For Rfseq to stop - 1ms timeout */
	SPINWAIT(((read_phy_reg(pi, HTPHY_RfseqStatus0) & 0x1) == 1), 1000);

	MOD_PHYREG(pi, HTPHY_sampleCmd, stop, 1);

	/* Restore Register */
	MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, DisRx, rfseqCoreActv_DisRx_save);
	MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnTx, rfseqCoreActv_EnTx_save);
	write_phy_reg(pi, HTPHY_RfseqMode, rfseqMode_save);
	write_phy_reg(pi, HTPHY_sampleDepthCount, sampleDepthCount_save);
	write_phy_reg(pi, HTPHY_sampleLoopCount, sampleLoopCount_save);
	write_phy_reg(pi, HTPHY_sampleInitWaitCount, sampleInitWaitCount_save);
	write_phy_reg(pi, HTPHY_sampleCmd, sampleCmd_save);

	wlapi_enable_mac(pi->sh->physhim);
}

uint8
wlc_phy_rxcore_getstate_htphy(wlc_phy_t *pih)
{
	uint16 rxen_bits;
	phy_info_t *pi = (phy_info_t*)pih;

	/* Extract the EnRx field of this register */
	rxen_bits = READ_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnRx);

	ASSERT(pi->sh->phyrxchain == rxen_bits);

	return ((uint8) rxen_bits);
}

void
wlc_phy_cal_init_htphy(phy_info_t *pi)
{
}

static void
wlc_phy_workarounds_htphy(phy_info_t *pi)
{
	uint16 rfseq_rx2tx_dacbufpu_rev0[] = {0x10f, 0x10f};
	uint16 dac_control = 0x0002;
	uint16 afectrl_adc_lp20_rev0 = 0x0;
	uint16 afectrl_adc_lp40_rev0 = 0x0;
	uint16 afectrl_adc_ctrl20_rev0 = 0x0;
	uint16 afectrl_adc_ctrl40_rev0 = 0x0;
	uint16 pktgn_lpf_hml_hpc[] = {0x777, 0x111, 0x111,
	                              0x777, 0x111, 0x111,
	                              0x777, 0x111, 0x111};
	uint16 tx2rx_lpf_h_hpc = 0x777;
	uint16 rx2tx_lpf_h_hpc = 0x777;
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));


	/* bphy classifier on for 2.4G vs off for 5G */
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_cck_en, 0);
	} else {
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_cck_en, 1);
	}

	if (!ISSIM_ENAB(pi->sh->sih)) {
		or_phy_reg(pi, HTPHY_IQFlip, HTPHY_IQFlip_adc1_MASK |
		           HTPHY_IQFlip_adc2_MASK | HTPHY_IQFlip_adc3_MASK);
	}

	/* 2. enable PingPongComp "comp swap" to alter which rail gets
	 *    half-sample delay compensation; as of nphy, QT DAC/ADC
	 *    hookup employs same rail delay as true ADC, so comp swap
	 *    no longer conditioned on is_qt.
	 */
	write_phy_reg(pi, HTPHY_PingPongComp, HTPHY_PingPongComp_comp_enable_MASK |
	              HTPHY_PingPongComp_comp_iqswap_MASK);

	wlc_phy_write_txmacreg_htphy((wlc_phy_t *)pi, 0x10, 0x258);

	MOD_PHYREG(pi, HTPHY_NumDatatonesdup40, DupModeShiftEn, 0);

	write_phy_reg(pi, HTPHY_txTailCountValue, 114);

	FOREACH_CORE(pi, core) {
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ,
		                          2, 0x14e + 0x10*core, 16, rfseq_rx2tx_dacbufpu_rev0);
	}

	/* switch ADC mode depending on BW; see AMS for lc_mimo_afe */
	/* power down */
	FOREACH_CORE(pi, core) {
		MOD_PHYREGC(pi, HTPHY_Afectrl, core, adc_pd, 1);
		MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, adc_pd, 1);
		/* force mode */
		if (CHSPEC_IS20(pi->radio_chanspec)) {
			/* switch ADC to low power mode */
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, adc_lp, afectrl_adc_lp20_rev0);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, adc_lp, 1);
			/* use correct control bias current of i/q adc reference buffer */
			wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0x05 + 0x10*core, 16,
			                          &afectrl_adc_ctrl20_rev0);
		} else {
			/*  switch ADC to nominal power mode */
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, adc_lp, afectrl_adc_lp40_rev0);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, adc_lp, 1);
			/* use correct control bias current of i/q adc reference buffer */
			wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0x05 + 0x10*core, 16,
			                          &afectrl_adc_ctrl40_rev0);
		}
		/* remove ADC override */
		MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, adc_pd, 0);
	}

	/* Higher LPF HPC "H" (7) for better settling during gain-settling, better DC Rejection
	 * Lower  LPF HPC "L" (1) for better inner tone EVM during payload decoding;
	 */
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 9, 0x130, 16, pktgn_lpf_hml_hpc);
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, 0x120, 16, &tx2rx_lpf_h_hpc);
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, 0x124, 16, &rx2tx_lpf_h_hpc);

	/* Use 9/18MHz analog LPF BW settings for TX of 20/40MHz frames, respectively. */

	FOREACH_CORE(pi, core) {
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0x00 + 0x10*core, 16,
		                          &dac_control);
	}


	if (HTREV_IS(pi->pubpi.phy_rev, 0)) {
		mod_radio_reg(pi, RADIO_2059_OVR_REG7(0), 0x8, 0x8);
		mod_radio_reg(pi, RADIO_2059_LOGEN_PTAT_RESETS, 0x2, 0x2);
	}

	/* In event of high power spurs/interference that causes crs-glitches, stay
	 * in WAIT_ENERGY_DROP for 1 clk20 instead of default 1 ms. This way, we
	 * get back to CARRIER_SEARCH quickly and will less likely miss actual
	 * packets. PS: this is actually one settings for ACI
	 */
	write_phy_reg(pi, HTPHY_energydroptimeoutLen, 0x2);
}

/* AUTOGENERATED from subband_cust_4331A0.xls */
static void
wlc_phy_subband_cust_htphy(phy_info_t *pi)
{
	uint16 fc;
	if (CHSPEC_CHANNEL(pi->radio_chanspec) > 14) {
		fc = CHAN5G_FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	} else {
		fc = CHAN2G_FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	}

	/* 2G Band Customizations */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {

		/* Default customizations */
		/* CRS MinPwr */
		uint16 bcrsmin  = 0x46;
		/* InitGCode */
		uint16 initGCA0 = 0x0614;
		uint16 initGCA1 = 0x0614;
		uint16 initGCA2 = 0x0614;
		/* InitGain */
		uint16 initgain[] = {0x613f, 0x613f, 0x613f};
		/* Vmid and Av for {RSSI invalid PWRDET TSSI} */
		uint16 vmid0[] = {0x8e, 0x96, 0x96, 0x96};
		uint16 vmid1[] = {0x8f, 0x9f, 0x9f, 0x96};
		uint16 vmid2[] = {0x8f, 0x9f, 0x9f, 0x96};
		uint16 av0[] = {0x02, 0x02, 0x02, 0x00};
		uint16 av1[] = {0x02, 0x02, 0x02, 0x00};
		uint16 av2[] = {0x02, 0x02, 0x02, 0x00};

		MOD_PHYREG(pi, HTPHY_bphycrsminpower0, bphycrsminpower0, bcrsmin);
		write_phy_reg(pi, HTPHY_Core0InitGainCodeB2059, initGCA0);
		write_phy_reg(pi, HTPHY_Core1InitGainCodeB2059, initGCA1);
		write_phy_reg(pi, HTPHY_Core2InitGainCodeB2059, initGCA2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 3, 0x106, 16, initgain);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x08, 16, vmid0);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x18, 16, vmid1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x28, 16, vmid2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x0c, 16, av0);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x1c, 16, av1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x2c, 16, av2);

		/* BW20 customizations */
		if (CHSPEC_IS20(pi->radio_chanspec) == 1) {

			if ((fc >= 2412 && fc <= 2484)) {
				/* CRS MinPwr */
				uint16 crsminu  = 62;
				uint16 crsminl  = 62;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 13;
				uint16 clip1wb1 = 13;
				uint16 clip1wb2 = 13;
				/* LNA1 Gains [dB] */
				int8   lna1gain1[] = {9, 14, 19, 24};
				int8   lna1gain2[] = {9, 14, 19, 24};
				int8   lna1gain3[] = {9, 14, 19, 24};

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059,
				           clip1wbThreshold, clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059,
				           clip1wbThreshold, clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059,
				           clip1wbThreshold, clip1wb2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x8, 8,
				                          lna1gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x8, 8,
				                          lna1gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x8, 8,
				                          lna1gain3);
			}

		/* BW40 customizations */
		} else {

			if ((fc >= 2412 && fc <= 2437)) {
				/* CRS MinPwr */
				uint16 crsminu  = 60;
				uint16 crsminl  = 60;
				/* Match Filter */
				uint16 mfLessAvl = 1;
				uint16 crsPkThresl = 106;
				uint16 mfLessAvu = 0;
				uint16 crsPkThresu = 85;

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_crsControll, mfLessAve, mfLessAvl);
				MOD_PHYREG(pi, HTPHY_crsThreshold2l, peakThresh, crsPkThresl);
				MOD_PHYREG(pi, HTPHY_crsControlu, mfLessAve, mfLessAvu);
				MOD_PHYREG(pi, HTPHY_crsThreshold2u, peakThresh, crsPkThresu);
			} else if ((fc >= 2442 && fc <= 2484)) {
				/* CRS MinPwr */
				uint16 crsminu  = 60;
				uint16 crsminl  = 60;
				/* Match Filter */
				uint16 mfLessAvl = 0;
				uint16 crsPkThresl = 85;
				uint16 mfLessAvu = 1;
				uint16 crsPkThresu = 106;

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_crsControll, mfLessAve, mfLessAvl);
				MOD_PHYREG(pi, HTPHY_crsThreshold2l, peakThresh, crsPkThresl);
				MOD_PHYREG(pi, HTPHY_crsControlu, mfLessAve, mfLessAvu);
				MOD_PHYREG(pi, HTPHY_crsThreshold2u, peakThresh, crsPkThresu);
			}
		}
	}

	/* 5G Band Customizations */
	if (CHSPEC_IS5G(pi->radio_chanspec)) {

		/* Default customizations */
		/* NB Clip  */
		uint16 clipnb0  = 0xff;
		uint16 clipnb1  = 0xff;
		uint16 clipnb2  = 0xff;
		/* Hi Gain  */
		uint16 cliphi0  = 0x9e;
		uint16 cliphi1  = 0x9e;
		uint16 cliphi2  = 0x9e;
		/* MD Gain  */
		uint16 clipmdA0 = 0x82;
		uint16 clipmdA1 = 0x82;
		uint16 clipmdA2 = 0x82;
		uint16 clipmdB0 = 0x24;
		uint16 clipmdB1 = 0x24;
		uint16 clipmdB2 = 0x24;
		/* LO Gain  */
		uint16 cliploA0 = 0x008a;
		uint16 cliploA1 = 0x008a;
		uint16 cliploA2 = 0x008a;
		uint16 cliploB0 = 0x8;
		uint16 cliploB1 = 0x8;
		uint16 cliploB2 = 0x8;
		/* InitGCode */
		uint16 initGCA0 = 0x9e;
		uint16 initGCA1 = 0x9e;
		uint16 initGCA2 = 0x9e;
		/* InitGain */
		uint16 initgain[] = {0x624f, 0x624f, 0x624f};
		/* LNA2 Gains [dB] */
		int8   lna2gain1d[] = {-1, 6, 10, 14};
		int8   lna2gain2d[] = {-1, 6, 10, 14};
		int8   lna2gain3d[] = {-1, 6, 10, 14};
		/* MixTIA Gains */
		int8   mixtia1[] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
		int8   mixtia2[] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
		int8   mixtia3[] = {3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
		/* GainBits */
		int8   gainbits1[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
		int8   gainbits2[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
		int8   gainbits3[] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4};
		/* Vmid and Av for {RSSI invalid PWRDET TSSI} */
		uint16 vmid0[] = {0x8e, 0x96, 0x96, 0x91};
		uint16 vmid1[] = {0x8f, 0x9f, 0x9f, 0x91};
		uint16 vmid2[] = {0x8f, 0x9f, 0x9f, 0x91};
		uint16 av0[] = {0x02, 0x02, 0x02, 0x00};
		uint16 av1[] = {0x02, 0x02, 0x02, 0x00};
		uint16 av2[] = {0x02, 0x02, 0x02, 0x00};

		write_phy_reg(pi, HTPHY_Core0nbClipThreshold, clipnb0);
		write_phy_reg(pi, HTPHY_Core1nbClipThreshold, clipnb1);
		write_phy_reg(pi, HTPHY_Core2nbClipThreshold, clipnb2);
		write_phy_reg(pi, HTPHY_Core0clipHiGainCodeA2059, cliphi0);
		write_phy_reg(pi, HTPHY_Core1clipHiGainCodeA2059, cliphi1);
		write_phy_reg(pi, HTPHY_Core2clipHiGainCodeA2059, cliphi2);
		write_phy_reg(pi, HTPHY_Core0clipmdGainCodeA2059, clipmdA0);
		write_phy_reg(pi, HTPHY_Core1clipmdGainCodeA2059, clipmdA1);
		write_phy_reg(pi, HTPHY_Core2clipmdGainCodeA2059, clipmdA2);
		write_phy_reg(pi, HTPHY_Core0clipmdGainCodeB2059, clipmdB0);
		write_phy_reg(pi, HTPHY_Core1clipmdGainCodeB2059, clipmdB1);
		write_phy_reg(pi, HTPHY_Core2clipmdGainCodeB2059, clipmdB2);
		write_phy_reg(pi, HTPHY_Core0cliploGainCodeA2059, cliploA0);
		write_phy_reg(pi, HTPHY_Core1cliploGainCodeA2059, cliploA1);
		write_phy_reg(pi, HTPHY_Core2cliploGainCodeA2059, cliploA2);
		write_phy_reg(pi, HTPHY_Core0cliploGainCodeB2059, cliploB0);
		write_phy_reg(pi, HTPHY_Core1cliploGainCodeB2059, cliploB1);
		write_phy_reg(pi, HTPHY_Core2cliploGainCodeB2059, cliploB2);
		write_phy_reg(pi, HTPHY_Core0InitGainCodeA2059, initGCA0);
		write_phy_reg(pi, HTPHY_Core1InitGainCodeA2059, initGCA1);
		write_phy_reg(pi, HTPHY_Core2InitGainCodeA2059, initGCA2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 3, 0x106, 16, initgain);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x10, 8, lna2gain1d);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x10, 8, lna2gain2d);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x10, 8, lna2gain3d);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 10, 32, 8, mixtia1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 10, 32, 8, mixtia2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 10, 32, 8, mixtia3);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAINBITS1, 10, 0x20, 8, gainbits1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAINBITS2, 10, 0x20, 8, gainbits2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAINBITS3, 10, 0x20, 8, gainbits3);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x08, 16, vmid0);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x18, 16, vmid1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x28, 16, vmid2);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x0c, 16, av0);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x1c, 16, av1);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 4, 0x2c, 16, av2);

		/* BW20 customizations */
		if (CHSPEC_IS20(pi->radio_chanspec) == 1) {

			if ((fc >= 4920 && fc <= 5230)) {
				/* CRS MinPwr */
				uint16 crsminu  = 58;
				uint16 crsminl  = 58;
				/* RadioNF [1/4dB] */
				uint16 nf0      = 28;
				uint16 nf1      = 28;
				uint16 nf2      = 28;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x19;
				uint16 clip1wb1 = 0x19;
				uint16 clip1wb2 = 0x19;
				/* LNA1 Gains [dB] */
				int8   lna1gain1[] = {12, 18, 22, 26};
				int8   lna1gain2[] = {12, 18, 22, 26};
				int8   lna1gain3[] = {12, 18, 22, 26};
				/* LNA2 Gains [dB] */
				int8   lna2gain1[] = {-1, 6, 10, 14};
				int8   lna2gain2[] = {-1, 6, 10, 14};
				int8   lna2gain3[] = {-1, 6, 10, 14};

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core0, nf0);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core1, nf1);
				MOD_PHYREG(pi, HTPHY_nvcfg1, noisevar_nf_radio_qdb_core2, nf2);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x8, 8,
				                          lna1gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x8, 8,
				                          lna1gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x8, 8,
				                          lna1gain3);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x10, 8,
				                          lna2gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x10, 8,
				                          lna2gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x10, 8,
				                          lna2gain3);
			} else if ((fc >= 5240 && fc <= 5550)) {
				/* CRS MinPwr */
				uint16 crsminu  = 58;
				uint16 crsminl  = 58;
				/* RadioNF [1/4dB] */
				uint16 nf0      = 28;
				uint16 nf1      = 28;
				uint16 nf2      = 28;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x19;
				uint16 clip1wb1 = 0x19;
				uint16 clip1wb2 = 0x19;
				/* LNA1 Gains [dB] */
				int8   lna1gain1[] = {12, 18, 22, 26};
				int8   lna1gain2[] = {12, 18, 22, 26};
				int8   lna1gain3[] = {12, 18, 22, 26};
				/* LNA2 Gains [dB] */
				int8   lna2gain1[] = {-1, 6, 10, 14};
				int8   lna2gain2[] = {-1, 6, 10, 14};
				int8   lna2gain3[] = {-1, 6, 10, 14};

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core0, nf0);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core1, nf1);
				MOD_PHYREG(pi, HTPHY_nvcfg1, noisevar_nf_radio_qdb_core2, nf2);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x8, 8,
				                          lna1gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x8, 8,
				                          lna1gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x8, 8,
				                          lna1gain3);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x10, 8,
				                          lna2gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x10, 8,
				                          lna2gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x10, 8,
				                          lna2gain3);
			} else if ((fc >= 5560 && fc <= 5825)) {
				/* CRS MinPwr */
				uint16 crsminu  = 58;
				uint16 crsminl  = 58;
				/* RadioNF [1/4dB] */
				uint16 nf0      = 28;
				uint16 nf1      = 28;
				uint16 nf2      = 28;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x19;
				uint16 clip1wb1 = 0x19;
				uint16 clip1wb2 = 0x19;
				/* LNA1 Gains [dB] */
				int8   lna1gain1[] = {11, 17, 21, 25};
				int8   lna1gain2[] = {11, 17, 21, 25};
				int8   lna1gain3[] = {11, 17, 21, 25};
				/* LNA2 Gains [dB] */
				int8   lna2gain1[] = {1, 8, 12, 16};
				int8   lna2gain2[] = {1, 8, 12, 16};
				int8   lna2gain3[] = {1, 8, 12, 16};

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core0, nf0);
				MOD_PHYREG(pi, HTPHY_nvcfg0, noisevar_nf_radio_qdb_core1, nf1);
				MOD_PHYREG(pi, HTPHY_nvcfg1, noisevar_nf_radio_qdb_core2, nf2);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x8, 8,
				                          lna1gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x8, 8,
				                          lna1gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x8, 8,
				                          lna1gain3);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN1, 4, 0x10, 8,
				                          lna2gain1);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN2, 4, 0x10, 8,
				                          lna2gain2);
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_GAIN3, 4, 0x10, 8,
				                          lna2gain3);
			}

		/* BW40 customizations */
		} else {

			if ((fc >= 4920 && fc <= 5230)) {
				/* CRS MinPwr */
				uint16 crsminu  = 55;
				uint16 crsminl  = 55;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x14;
				uint16 clip1wb1 = 0x14;
				uint16 clip1wb2 = 0x14;

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
			} else if ((fc >= 5240 && fc <= 5550)) {
				/* CRS MinPwr */
				uint16 crsminu  = 55;
				uint16 crsminl  = 55;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x14;
				uint16 clip1wb1 = 0x14;
				uint16 clip1wb2 = 0x14;

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
			} else if ((fc >= 5560 && fc <= 5825)) {
				/* CRS MinPwr */
				uint16 crsminu  = 58;
				uint16 crsminl  = 58;
				/* Clip1 WB Thresh */
				uint16 clip1wb0 = 0x14;
				uint16 clip1wb1 = 0x14;
				uint16 clip1wb2 = 0x14;

				MOD_PHYREG(pi, HTPHY_crsminpoweru0, crsminpower0, crsminu);
				MOD_PHYREG(pi, HTPHY_crsminpowerl0, crsminpower0, crsminl);
				MOD_PHYREG(pi, HTPHY_Core0clipwbThreshold2059, clip1wbThreshold,
				           clip1wb0);
				MOD_PHYREG(pi, HTPHY_Core1clipwbThreshold2059, clip1wbThreshold,
				           clip1wb1);
				MOD_PHYREG(pi, HTPHY_Core2clipwbThreshold2059, clip1wbThreshold,
				           clip1wb2);
			}
		}
	}
}

void
wlc_phy_switch_radio_htphy(phy_info_t *pi, bool on)
{
	PHY_TRACE(("wl%d: %s %s\n", pi->sh->unit, __FUNCTION__, on ? "ON" : "OFF"));

	if (on) {
		if (! pi->radio_is_on) {
			wlc_phy_radio2059_init(pi);

			/* !!! it could change bw inside */
			wlc_phy_chanspec_set((wlc_phy_t*)pi, pi->radio_chanspec);
		}
		pi->radio_is_on = TRUE;
	} else {
		and_phy_reg(pi, HTPHY_RfctrlCmd, ~HTPHY_RfctrlCmd_chip_pu_MASK);
		pi->radio_is_on = FALSE;
	}
}

static void
wlc_phy_radio2059_reset(phy_info_t *pi)
{
	/* Reset jtag (through por_force toggle) to "reset" radio */
	/* NOTE: por_force is active low and level sensitive! */
	and_phy_reg(pi, HTPHY_RfctrlCmd, ~HTPHY_RfctrlCmd_chip_pu_MASK);
	or_phy_reg(pi, HTPHY_RfctrlCmd, HTPHY_RfctrlCmd_por_force_MASK);

	and_phy_reg(pi, HTPHY_RfctrlCmd, ~HTPHY_RfctrlCmd_por_force_MASK);
	or_phy_reg(pi, HTPHY_RfctrlCmd, HTPHY_RfctrlCmd_chip_pu_MASK);
}

static void
wlc_phy_radio2059_vcocal(phy_info_t *pi)
{
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_EN,         0x01, 0x0);
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_CAL_RESETN, 0x04, 0x0);
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_CAL_RESETN, 0x04, (1 << 2));
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_EN,         0x01, 0x01);

	/* Wait for open loop cal completion and settling */
	OSL_DELAY(300);
}

#define MAX_2059_RCAL_WAITLOOPS 10000

static void
wlc_phy_radio2059_rcal(phy_info_t *pi)
{
	uint16 rcal_reg = 0;
	int i;

	/* Exit if we are running on Quickturn */
	if (ISSIM_ENAB(pi->sh->sih))
		return;

	/* Power up RCAL */
	mod_radio_reg(pi, RADIO_2059_RCAL_CONFIG, 0x1, 0x1);
	OSL_DELAY(10);

	/* Set testmux for rcal */
	mod_radio_reg(pi, RADIO_2059_IQTEST_SEL_PU,  0x1, 0x1);
	mod_radio_reg(pi, RADIO_2059_IQTEST_SEL_PU2, 0x3, 0x2);

	/* Trigger RCAL */
	mod_radio_reg(pi, RADIO_2059_RCAL_CONFIG, 0x2, 0x2);
	OSL_DELAY(100);
	mod_radio_reg(pi, RADIO_2059_RCAL_CONFIG, 0x2, 0x0);

	/* Wait for RCAL to complete */
	for (i = 0; i < MAX_2059_RCAL_WAITLOOPS; i++) {
		rcal_reg = read_radio_reg(pi, RADIO_2059_RCAL_STATUS);
		if (rcal_reg & 0x1) {
			break;
		}
		OSL_DELAY(100);
	}
	ASSERT(rcal_reg & 0x1);

	/* Read the rcal value */
	rcal_reg = read_radio_reg(pi, RADIO_2059_RCAL_STATUS) & 0x3e;

	/* Power down the RCAL */
	mod_radio_reg(pi, RADIO_2059_RCAL_CONFIG, 0x1, 0x0);

	/* Apply rcal value to bandgap setting (get rid of LSB) */
	mod_radio_reg(pi, RADIO_2059_BANDGAP_RCAL_TRIM, 0xf0, rcal_reg << 2);

	PHY_INFORM(("wl%d: %s rcal=%d\n", pi->sh->unit, __FUNCTION__, rcal_reg >> 1));
}

#define MAX_2059_RCCAL_WAITLOOPS 10000
#define NUM_2059_RCCAL_CAPS 3

static void
wlc_phy_radio2059_rccal(phy_info_t *pi)
{
	uint8 master_val[] = {0x61, 0x69, 0x73}; /* bcap, scap, hpc */
	uint8 x1_val[]     = {0x6e, 0x6e, 0x6e}; /* bcap, scap, hpc */
	uint8 trc0_val[]   = {0xe9, 0xd5, 0x99}; /* bcap, scap, hpc */
	uint16 rccal_valid;
	int i, cal;

	/* Exit if we are running on Quickturn */
	if (ISSIM_ENAB(pi->sh->sih))
		return;

	/* Calibrate bcap, scap, and hpc */
	for (cal = 0; cal < NUM_2059_RCCAL_CAPS; cal++) {
		/* Cal setup */
		rccal_valid = 0;
		write_radio_reg(pi, RADIO_2059_RCCAL_MASTER, master_val[cal]);
		write_radio_reg(pi, RADIO_2059_RCCAL_X1, x1_val[cal]);
		write_radio_reg(pi, RADIO_2059_RCCAL_TRC0, trc0_val[cal]);

		/* Start cal */
		write_radio_reg(pi, RADIO_2059_RCCAL_START_R1_Q1_P1, 0x55);

		/* Wait for cal to complete */
		for (i = 0; i < MAX_2059_RCCAL_WAITLOOPS; i++) {
			rccal_valid = read_radio_reg(pi, RADIO_2059_RCCAL_DONE_OSCCAP);
			if (rccal_valid & 0x2) {
				break;
			}
			OSL_DELAY(500);
		}
		ASSERT(rccal_valid & 0x2);

		/* Stop cal */
		write_radio_reg(pi, RADIO_2059_RCCAL_START_R1_Q1_P1, 0x15);
	}
	/* turn RC-cal OFF */
	mod_radio_reg(pi, RADIO_2059_RCCAL_MASTER, 1, 0);

	PHY_INFORM(("wl%d: %s rccal bcap=%d scap=%d hpc=%d\n",
	            pi->sh->unit, __FUNCTION__,
	            read_radio_reg(pi, RADIO_2059_RCCAL_SCAP_VAL),
	            read_radio_reg(pi, RADIO_2059_RCCAL_BCAP_VAL),
	            read_radio_reg(pi, RADIO_2059_RCCAL_HPC_VAL)));
}


static void
wlc_phy_radio2059_init(phy_info_t *pi)
{
	radio_20xx_regs_t *regs_2059_ptr = regs_2059_rev0;
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* "Pre-Init" */
	wlc_phy_radio2059_reset(pi);

	/* Initialize the radio registers */
	wlc_phy_init_radio_regs_allbands(pi, regs_2059_ptr);

	/* "Post-Init" */
	FOREACH_CORE(pi, core) {
		/* Enable PHY direct control ("pin control") */
		/* Enable the xtal_pu_ovr. Thereby the settings in the RF_xtal_config1 
		 * and RF_xtal_config2 regss will be used instead of the xtal being 
		 * controlled by radio_xtal_pu
		 */
		mod_radio_reg(pi, RADIO_2059_XTALPUOVR_PINCTRL(core), 0x3, 0x3);
	}

	/* Reset synthesizer */
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_CAL_RESETN, 0x78, 0x78);
	mod_radio_reg(pi, RADIO_2059_XTAL_CONFIG2,          0x80, 0x80);
	OSL_DELAY(2000);
	mod_radio_reg(pi, RADIO_2059_RFPLL_MISC_CAL_RESETN, 0x78, 0x0);
	mod_radio_reg(pi, RADIO_2059_XTAL_CONFIG2,          0x80, 0x0);

	/* R-cal and RC-cal required after radio reset */
	wlc_phy_radio2059_rcal(pi);
	wlc_phy_radio2059_rccal(pi);

	/* Disable xo_jtag and the spare_xtal_buffer */
	mod_radio_reg(pi, RADIO_2059_RFPLL_MASTER, 0x8, 0x0);
}

/*  lookup radio-chip-specific channel code */
static bool
wlc_phy_chan2freq_htphy(phy_info_t *pi, uint channel, int *f, chan_info_htphy_radio2059_t **t)
{
	uint i;
	chan_info_htphy_radio2059_t *chan_info_tbl = NULL;
	uint32 tbl_len = 0;

	int freq = 0;

	chan_info_tbl = chan_info_htphyrev0_2059v0;
	tbl_len = ARRAYSIZE(chan_info_htphyrev0_2059v0);

	for (i = 0; i < tbl_len; i++) {
		if (chan_info_tbl[i].chan == channel)
			break;
	}

	if (i >= tbl_len) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		           pi->sh->unit, __FUNCTION__, channel));
		ASSERT(i < tbl_len);
		goto fail;
	}
	*t = &chan_info_tbl[i];
	freq = chan_info_tbl[i].freq;

	*f = freq;
	return TRUE;

fail:
	*f = WL_CHAN_FREQ_RANGE_2G;
	return FALSE;
}

/* get the complex freq. if chan==0, use default radio channel */
uint8
wlc_phy_get_chan_freq_range_htphy(phy_info_t *pi, uint channel)
{
	int freq;
	chan_info_htphy_radio2059_t *t = NULL;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (channel == 0)
		channel = CHSPEC_CHANNEL(pi->radio_chanspec);

	if (!wlc_phy_chan2freq_htphy(pi, channel, &freq, &t)) {
		PHY_ERROR(("wl%d: %s: channel invalid\n", pi->sh->unit, __FUNCTION__));
	}

	if (CHSPEC_IS2G(pi->radio_chanspec))
		return WL_CHAN_FREQ_RANGE_2G;

	if ((freq >= BASE_LOW_5G_CHAN) && (freq < BASE_MID_5G_CHAN)) {
		return WL_CHAN_FREQ_RANGE_5GL;
	} else if ((freq >= BASE_MID_5G_CHAN) && (freq < BASE_HIGH_5G_CHAN)) {
		return WL_CHAN_FREQ_RANGE_5GM;
	} else {
		return WL_CHAN_FREQ_RANGE_5GH;
	}
}

static void
wlc_phy_chanspec_radio2059_setup(phy_info_t *pi, const chan_info_htphy_radio2059_t *ci)
{
	uint8 core;

	/* Tuning */
	write_radio_reg(pi,
	                RADIO_2059_VCOCAL_COUNTVAL0, ci->RF_vcocal_countval0);
	write_radio_reg(pi,
	                RADIO_2059_VCOCAL_COUNTVAL1, ci->RF_vcocal_countval1);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_REFMASTER_SPAREXTALSIZE,
	                ci->RF_rfpll_refmaster_sparextalsize);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_LOOPFILTER_R1, ci->RF_rfpll_loopfilter_r1);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_LOOPFILTER_C2, ci->RF_rfpll_loopfilter_c2);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_LOOPFILTER_C1, ci->RF_rfpll_loopfilter_c1);
	write_radio_reg(pi,
	                RADIO_2059_CP_KPD_IDAC, ci->RF_cp_kpd_idac);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_MMD0, ci->RF_rfpll_mmd0);
	write_radio_reg(pi,
	                RADIO_2059_RFPLL_MMD1, ci->RF_rfpll_mmd1);
	write_radio_reg(pi,
	                RADIO_2059_VCOBUF_TUNE, ci->RF_vcobuf_tune);
	write_radio_reg(pi,
	                RADIO_2059_LOGEN_MX2G_TUNE, ci->RF_logen_mx2g_tune);
	write_radio_reg(pi,
	                RADIO_2059_LOGEN_MX5G_TUNE, ci->RF_logen_mx5g_tune);
	write_radio_reg(pi,
	                RADIO_2059_LOGEN_INDBUF2G_TUNE, ci->RF_logen_indbuf2g_tune);

	FOREACH_CORE(pi, core) {
		write_radio_reg(pi, RADIO_2059_LOGEN_INDBUF5G_TUNE(core),
		                TUNING_REG(ci->RF_logen_indbuf5g_tune_core, core));
		write_radio_reg(pi, RADIO_2059_TXMIX2G_TUNE_BOOST_PU(core),
		                TUNING_REG(ci->RF_txmix2g_tune_boost_pu_core, core));
		write_radio_reg(pi, RADIO_2059_PAD2G_TUNE_PUS(core),
		                TUNING_REG(ci->RF_pad2g_tune_pus_core, core));
		write_radio_reg(pi, RADIO_2059_PGA_BOOST_TUNE(core),
		                TUNING_REG(ci->RF_pga_boost_tune_core, core));
		write_radio_reg(pi, RADIO_2059_TXMIX5G_BOOST_TUNE(core),
		                TUNING_REG(ci->RF_txmix5g_boost_tune_core, core));
		write_radio_reg(pi, RADIO_2059_PAD5G_TUNE_MISC_PUS(core),
		                TUNING_REG(ci->RF_pad5g_tune_misc_pus_core, core));
		write_radio_reg(pi, RADIO_2059_LNA2G_TUNE(core),
		                TUNING_REG(ci->RF_lna2g_tune_core, core));
		write_radio_reg(pi, RADIO_2059_LNA5G_TUNE(core),
		                TUNING_REG(ci->RF_lna5g_tune_core, core));
	}


	/* Guard time pre-vco-cal */
	OSL_DELAY(50);

	/* Do a VCO cal after writing the tuning table regs */
	wlc_phy_radio2059_vcocal(pi);
}

static void
wlc_phy_chanspec_setup_htphy(phy_info_t *pi, chanspec_t chanspec, const htphy_sfo_cfg_t *ci)
{
	uint16 val, core;
	uint8 spuravoid = WL_SPURAVOID_OFF;
	uint16 chan = CHSPEC_CHANNEL(chanspec);

	/* Any channel (or sub-band) specific settings go in here */

	/* Set phy Band bit, required to ensure correct band's tx/rx board
	 * level controls are being driven, 0 = 2.4G, 1 = 5G.
	 * Enable/disable BPHY if channel is in 2G/5G band, respectively.
	 */
	if (CHSPEC_IS5G(chanspec)) { /* we are in 5G */
		/* switch BandControl to 2G to make BPHY regs accessible */
		and_phy_reg(pi, HTPHY_BandControl, ~HTPHY_BandControl_currentBand_MASK);
		/* enable force gated clock on */
		val = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, (val | MAC_PHY_FORCE_CLK));
		/* enable bphy resetCCA and put bphy receiver in reset */
		or_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_BB_CONFIG),
		           (BBCFG_RESETCCA | BBCFG_RESETRX));
		/* restore force gated clock */
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, val);
		/* switch BandControl to 5G */
		or_phy_reg(pi, HTPHY_BandControl, HTPHY_BandControl_currentBand_MASK);
	} else { /* we are in 2G */
		/* switch BandControl to 2G */
		and_phy_reg(pi, HTPHY_BandControl, ~HTPHY_BandControl_currentBand_MASK);
		/* enable force gated clock on */
		val = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, (val | MAC_PHY_FORCE_CLK));
		/* disable bphy resetCCA and take bphy receiver out of reset */
		and_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_BB_CONFIG),
		            (uint16)(~(BBCFG_RESETCCA | BBCFG_RESETRX)));
		/* restore force gated clock */
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, val);
	}

	/* set SFO parameters
	 * sfo_chan_center_Ts20 = round([fc-10e6 fc fc+10e6] / 20e6 * 8), fc in Hz
	 *                      = round([$channel-10 $channel $channel+10] * 0.4),
	 *                              $channel in MHz
	 */
	write_phy_reg(pi, HTPHY_BW1a, ci->PHY_BW1a);
	write_phy_reg(pi, HTPHY_BW2, ci->PHY_BW2);
	write_phy_reg(pi, HTPHY_BW3, ci->PHY_BW3);

	/* sfo_chan_center_factor = round(2^17./([fc-10e6 fc fc+10e6]/20e6)), fc in Hz
	 *                        = round(2621440./[$channel-10 $channel $channel+10]),
	 *                                $channel in MHz
	 */
	write_phy_reg(pi, HTPHY_BW4, ci->PHY_BW4);
	write_phy_reg(pi, HTPHY_BW5, ci->PHY_BW5);
	write_phy_reg(pi, HTPHY_BW6, ci->PHY_BW6);


	if (chan == 14) {
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_ofdm_en, 0);
		/* Bit 11 and 6 of BPHY testRegister to '10' */
		or_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_TEST, 0x800);
	} else {
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_ofdm_en,
			HTPHY_ClassifierCtrl_ofdm_en);
		/* Bit 11 and 6 of BPHY testRegister to '00' */
		if (CHSPEC_IS2G(chanspec))
			and_phy_reg(pi, HTPHY_TO_BPHY_OFF + BPHY_TEST, ~0x840);
	}

	/* set txgain in case txpwrctrl is disabled */
	wlc_phy_txpwr_fixpower_htphy(pi);

	/* Set the analog TX_LPF Bandwidth */
	wlc_phy_txlpfbw_htphy(pi);

	/* dynamic spur avoidance, user can override the behavior */
	if (!CHSPEC_IS40(pi->radio_chanspec)) {
		if ((chan == 13) || (chan == 14) || (chan == 153)) {
			spuravoid = WL_SPURAVOID_ON1;
		}
	} else { /* 40 MHz b/w */
		if (chan == 54) {
			spuravoid = WL_SPURAVOID_ON1;
		} else if ((chan == 118) || (chan == 151)) {
			spuravoid = WL_SPURAVOID_ON2;
		}
	}

	if (pi->phy_spuravoid == SPURAVOID_DISABLE) {
		spuravoid = WL_SPURAVOID_OFF;
	} else if (pi->phy_spuravoid == SPURAVOID_FORCEON) {
		spuravoid = WL_SPURAVOID_ON1;
	} else if (pi->phy_spuravoid == SPURAVOID_FORCEON2) {
		spuravoid = WL_SPURAVOID_ON2;
	}


	/* PLL parameter changes */
	wlapi_bmac_core_phypll_ctl(pi->sh->physhim, FALSE);
	si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, spuravoid);
	wlapi_bmac_core_phypll_ctl(pi->sh->physhim, TRUE);

	/* MAX TSF clock setup for chips where PHY & MAC share same PLL */
	wlapi_switch_macfreq(pi->sh->physhim, spuravoid);

	/* Do a soft dreset of the PLL */
	wlapi_bmac_core_phypll_reset(pi->sh->physhim);

	/* Setup resampler */
	MOD_PHYREG(pi, HTPHY_BBConfig, resample_clk160,
	           (spuravoid == WL_SPURAVOID_OFF) ? 0 : 1);
	if (spuravoid != WL_SPURAVOID_OFF) {
		MOD_PHYREG(pi, HTPHY_BBConfig, resamplerFreq,
		           (spuravoid - WL_SPURAVOID_ON1));
	}

	/* Clean up */
	wlc_phy_resetcca_htphy(pi);

	PHY_TRACE(("wl%d: %s spuravoid=%d\n", pi->sh->unit, __FUNCTION__, spuravoid));

	write_phy_reg(pi, HTPHY_NumDatatonesdup40, 0x3830);

	wlc_phy_subband_cust_htphy(pi);


	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		write_radio_reg(pi, RADIO_2059_RFPLL_LOOPFILTER_R1, 0x1f);
		write_radio_reg(pi, RADIO_2059_CP_KPD_IDAC, 0x3f);
		write_radio_reg(pi, RADIO_2059_RFPLL_LOOPFILTER_C1, 0x8);
		write_radio_reg(pi, RADIO_2059_RFPLL_LOOPFILTER_C2, 0x8);
	}

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		FOREACH_CORE(pi, core) {
			write_radio_reg(pi, RADIO_2059_PGA_BOOST_TUNE(core), 0x0);
			write_radio_reg(pi, RADIO_2059_PAD5G_TUNE_MISC_PUS(core), 0x3);
			write_radio_reg(pi, RADIO_2059_TXMIX5G_BOOST_TUNE(core), 0x0);
		}
	}

	/* Spur avoidance WAR */
	wlc_phy_spurwar_htphy(pi);
}

#if defined(AP) && defined(RADAR)
void
wlc_phy_radar_detect_init_htphy(phy_info_t *pi, bool on)
{
}

void
wlc_phy_update_radar_detect_param_htphy(phy_info_t *pi)
{
}

int
wlc_phy_radar_detect_run_htphy(phy_info_t *pi)
{
	return (RADAR_TYPE_NONE);
}
#endif /* defined(AP) && defined(RADAR) */

void
wlc_phy_chanspec_set_htphy(phy_info_t *pi, chanspec_t chanspec)
{
	int freq;
	chan_info_htphy_radio2059_t *t = NULL;

	PHY_TRACE(("wl%d: %s chan = %d\n", pi->sh->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec)));

	if (!wlc_phy_chan2freq_htphy(pi, CHSPEC_CHANNEL(chanspec), &freq, &t))
		return;

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);

	/* Set the phy bandwidth as dictated by the chanspec */
	if (CHSPEC_BW(chanspec) != pi->bw)
		wlapi_bmac_bw_set(pi->sh->physhim, CHSPEC_BW(chanspec));

	/* Set the correct sideband if in 40MHz mode */
	if (CHSPEC_IS40(chanspec)) {
		if (CHSPEC_SB_UPPER(chanspec)) {
			or_phy_reg(pi, HTPHY_RxControl, HTPHY_RxControl_bphy_band_sel_MASK);
			or_phy_reg(pi, HTPHY_ClassifierCtrl2,
			           HTPHY_ClassifierCtrl2_prim_sel_MASK);
		} else {
			and_phy_reg(pi, HTPHY_RxControl, ~HTPHY_RxControl_bphy_band_sel_MASK);
			and_phy_reg(pi, HTPHY_ClassifierCtrl2,
			            (~HTPHY_ClassifierCtrl2_prim_sel_MASK & 0xffff));
		}
	}


	/* band specific 2059 radio inits */

	wlc_phy_chanspec_radio2059_setup(pi, t);
	wlc_phy_chanspec_setup_htphy(pi, chanspec, (const htphy_sfo_cfg_t *)&(t->PHY_BW1a));

#if defined(AP) && defined(RADAR)
	/* update radar detect mode specific params
	 * based on new chanspec
	 */
	wlc_phy_update_radar_detect_param_htphy(pi);
#endif /* defined(AP) && defined(RADAR) */

}


static void
wlc_phy_txlpfbw_htphy(phy_info_t *pi)
{
}

static void
wlc_phy_spurwar_htphy(phy_info_t *pi)
{
}


uint16
wlc_phy_classifier_htphy(phy_info_t *pi, uint16 mask, uint16 val)
{
	uint16 curr_ctl, new_ctl;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Turn on/off classification (bphy, ofdm, and wait_ed), mask and
	 * val are bit fields, bit 0: bphy, bit 1: ofdm, bit 2: wait_ed;
	 * for types corresponding to bits set in mask, apply on/off state
	 * from bits set in val; if no bits set in mask, simply returns
	 * current on/off state.
	 */
	curr_ctl = read_phy_reg(pi, HTPHY_ClassifierCtrl) &
		HTPHY_ClassifierCtrl_classifierSel_MASK;

	new_ctl = (curr_ctl & (~mask)) | (val & mask);

	MOD_PHYREG(pi, HTPHY_ClassifierCtrl, classifierSel, new_ctl);

	return new_ctl;
}

static void
wlc_phy_clip_det_htphy(phy_info_t *pi, uint8 write, uint16 *vals)
{
	uint16 core;

	/* Make clip detection difficult (impossible?) */
	FOREACH_CORE(pi, core) {
		if (write == 0) {
			vals[core] = read_phy_reg(pi, HTPHY_Clip1Threshold(core));
		} else {
			write_phy_reg(pi, HTPHY_Clip1Threshold(core), vals[core]);
		}
	}
}

void
wlc_phy_force_rfseq_htphy(phy_info_t *pi, uint8 cmd)
{
	uint16 trigger_mask, status_mask;
	uint16 orig_RfseqCoreActv;

	switch (cmd) {
	case HTPHY_RFSEQ_RX2TX:
		trigger_mask = HTPHY_RfseqTrigger_rx2tx_MASK;
		status_mask = HTPHY_RfseqStatus0_rx2tx_MASK;
		break;
	case HTPHY_RFSEQ_TX2RX:
		trigger_mask = HTPHY_RfseqTrigger_tx2rx_MASK;
		status_mask = HTPHY_RfseqStatus0_tx2rx_MASK;
		break;
	case HTPHY_RFSEQ_RESET2RX:
		trigger_mask = HTPHY_RfseqTrigger_reset2rx_MASK;
		status_mask = HTPHY_RfseqStatus0_reset2rx_MASK;
		break;
	case HTPHY_RFSEQ_UPDATEGAINH:
		trigger_mask = HTPHY_RfseqTrigger_updategainh_MASK;
		status_mask = HTPHY_RfseqStatus0_updategainh_MASK;
		break;
	case HTPHY_RFSEQ_UPDATEGAINL:
		trigger_mask = HTPHY_RfseqTrigger_updategainl_MASK;
		status_mask = HTPHY_RfseqStatus0_updategainl_MASK;
		break;
	case HTPHY_RFSEQ_UPDATEGAINU:
		trigger_mask = HTPHY_RfseqTrigger_updategainu_MASK;
		status_mask = HTPHY_RfseqStatus0_updategainu_MASK;
		break;
	default:
		PHY_ERROR(("wlc_phy_force_rfseq_htphy: unrecognized command."));
		return;
	}

	orig_RfseqCoreActv = read_phy_reg(pi, HTPHY_RfseqMode);
	or_phy_reg(pi, HTPHY_RfseqMode,
	           (HTPHY_RfseqMode_CoreActv_override_MASK |
	            HTPHY_RfseqMode_Trigger_override_MASK));
	or_phy_reg(pi, HTPHY_RfseqTrigger, trigger_mask);
	SPINWAIT((read_phy_reg(pi, HTPHY_RfseqStatus0) & status_mask), 200000);
	write_phy_reg(pi, HTPHY_RfseqMode, orig_RfseqCoreActv);

	ASSERT((read_phy_reg(pi, HTPHY_RfseqStatus0) & status_mask) == 0);
}

static void
wlc_phy_tssi_radio_setup_htphy(phy_info_t *pi, uint8 core_mask)
{
	uint8  core;

	FOREACH_ACTV_CORE(pi, core_mask, core) {

		/* activate IQ testpins and route through external tssi */
		mod_radio_reg(pi, RADIO_2059_IQTEST_SEL_PU,  0x1, 0x1);
		write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MUX(core), 0x11);
	}
}

static void
wlc_phy_auxadc_sel_htphy(phy_info_t *pi, uint8 core_mask, uint8 signal_type)
{
	uint8  core;
	uint16 reset_mask = HTPHY_RfctrlAux_rssi_wb1a_sel_MASK |
	                    HTPHY_RfctrlAux_rssi_wb1g_sel_MASK |
	                    HTPHY_RfctrlAux_rssi_wb2_sel_MASK |
	                    HTPHY_RfctrlAux_rssi_nb_sel_MASK;

	/* signal_type is HTPHY_AUXADC_SEL_[W1,W2,NB,IQ,OFF] */
	FOREACH_ACTV_CORE(pi, core_mask, core) {

		if (signal_type == HTPHY_AUXADC_SEL_OFF) {
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_i, 0);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_q, 0);

			MOD_PHYREGC(pi, HTPHY_RfctrlOverrideAux, core, rssi_ctrl, 0);

		} else if ((signal_type == HTPHY_AUXADC_SEL_W1) ||
		           (signal_type == HTPHY_AUXADC_SEL_W2) ||
		           (signal_type == HTPHY_AUXADC_SEL_NB)) {
			/* force AFE rssi mux sel to rssi inputs */
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_i, 0);
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_q, 0);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_i, 1);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_q, 1);

			mod_phy_reg(pi, HTPHY_RfctrlAux(core), reset_mask, 0);
			if (signal_type == HTPHY_AUXADC_SEL_W1) {
				if (CHSPEC_IS5G(pi->radio_chanspec)) {
					MOD_PHYREGC(pi, HTPHY_RfctrlAux, core, rssi_wb1a_sel, 1);
				} else {
					MOD_PHYREGC(pi, HTPHY_RfctrlAux, core, rssi_wb1g_sel, 1);
				}
			} else if (signal_type == HTPHY_AUXADC_SEL_W2) {
				MOD_PHYREGC(pi, HTPHY_RfctrlAux, core, rssi_wb2_sel, 1);
			} else /* (signal_type == HTPHY_AUXADC_SEL_NB) */ {
				MOD_PHYREGC(pi, HTPHY_RfctrlAux, core, rssi_nb_sel, 1);
			}
			MOD_PHYREGC(pi, HTPHY_RfctrlOverrideAux, core, rssi_ctrl, 1);

		} else if (signal_type == HTPHY_AUXADC_SEL_TSSI) {
			/* set mux at the afe input level (tssi) */
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_i, 3);
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_q, 3);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_i, 1);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_q, 1);
			/* set up radio muxes (already done in init) */
			wlc_phy_tssi_radio_setup_htphy(pi, 1 << core);

		} else if (signal_type == HTPHY_AUXADC_SEL_IQ) {
			/* force AFE rssi mux sel to iq/pwr_det/temp_sense */
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_i, 2);
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, rssi_select_q, 2);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_i, 1);
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_q, 1);
		}
	}
}

static void
wlc_phy_poll_auxadc_htphy(phy_info_t *pi, uint8 signal_type, int32 *auxadc_buf, uint8 nsamps)
{
	uint core;
	uint16 afectrl_save[PHY_CORE_MAX];
	uint16 afectrlOverride_save[PHY_CORE_MAX];
	uint16 rfctrlAux_save[PHY_CORE_MAX];
	uint16 rfctrlOverrideAux_save[PHY_CORE_MAX];
	uint8 samp = 0;
	uint16 rssi = 0;

	/* Save afectrl/rfctrl registers touched in auxadc_sel */
	FOREACH_CORE(pi, core) {
		afectrl_save[core] = read_phy_reg(pi, HTPHY_Afectrl(core));
		afectrlOverride_save[core] = read_phy_reg(pi, HTPHY_AfectrlOverride(core));
		rfctrlAux_save[core] = read_phy_reg(pi, HTPHY_RfctrlAux(core));
		rfctrlOverrideAux_save[core] = read_phy_reg(pi, HTPHY_RfctrlOverrideAux(core));
	}

	wlc_phy_auxadc_sel_htphy(pi, 7, signal_type);

	FOREACH_CORE(pi, core) {
		auxadc_buf[2*core] = 0;
		auxadc_buf[2*core+1] = 0;
	}

	for (samp = 0; samp < nsamps; samp++) {
		FOREACH_CORE(pi, core) {
			rssi = read_phy_reg(pi, HTPHY_RSSIVal(core));
			/* sign extension 6 to 8 bit */
			auxadc_buf[2*core] += ((int8)((rssi & 0x3f) << 2)) >> 2;
			auxadc_buf[2*core+1] += ((int8)(((rssi >> 8) & 0x3f) << 2)) >> 2;
		}
	}

	/* Restore the saved registers */
	FOREACH_CORE(pi, core) {
		write_phy_reg(pi, HTPHY_Afectrl(core), afectrl_save[core]);
		write_phy_reg(pi, HTPHY_AfectrlOverride(core), afectrlOverride_save[core]);
		write_phy_reg(pi, HTPHY_RfctrlAux(core), rfctrlAux_save[core]);
		write_phy_reg(pi, HTPHY_RfctrlOverrideAux(core), rfctrlOverrideAux_save[core]);
	}
}

int BCMFASTPATH
wlc_phy_rssi_compute_htphy(phy_info_t *pi, wlc_d11rxhdr_t *wlc_rxh)
{
	d11rxhdr_t *rxh = &wlc_rxh->rxhdr;
	int16 rxpwr, rxpwr_core[WL_RSSI_ANT_MAX];
	int i;

	/* mode = 0: rxpwr = max(rxpwr0, rxpwr1)
	 * mode = 1: rxpwr = min(rxpwr0, rxpwr1)
	 * mode = 2: rxpwr = (rxpwr0+rxpwr1)/2
	 */
	rxpwr = 0;
	rxpwr_core[0] = HTPHY_RXPWR_ANT0(rxh);
	rxpwr_core[1] = HTPHY_RXPWR_ANT1(rxh);
	rxpwr_core[2] = HTPHY_RXPWR_ANT2(rxh);
	rxpwr_core[3] = 0;

	/* Sign extend */
	for (i = 0; i < WL_RSSI_ANT_MAX; i ++) {
		if (rxpwr_core[i] > 127)
			rxpwr_core[i] -= 256;
	}


	/* only 3 antennas are valid for now */
	for (i = 0; i < WL_RSSI_ANT_MAX; i ++)
		wlc_rxh->rxpwr[i] = (int8)rxpwr_core[i];
	wlc_rxh->do_rssi_ma = 0;

	/* legacy interface */
	if (pi->sh->rssi_mode == RSSI_ANT_MERGE_MAX) {
		rxpwr = MAX(rxpwr_core[0], rxpwr_core[1]);
		rxpwr = MAX(rxpwr, rxpwr_core[2]);
	} else if (pi->sh->rssi_mode == RSSI_ANT_MERGE_MIN) {
		rxpwr = MIN(rxpwr_core[0], rxpwr_core[1]);
		rxpwr = MIN(rxpwr, rxpwr_core[2]);
	} else if (pi->sh->rssi_mode == RSSI_ANT_MERGE_AVG) {
		int16 qrxpwr;
		rxpwr = (rxpwr_core[0] + rxpwr_core[1] + rxpwr_core[2]);
		rxpwr = (int8)qm_div16(rxpwr, 3, &qrxpwr);
	}
	else
		ASSERT(0);

	return rxpwr;
}

int16
wlc_phy_tempsense_htphy(phy_info_t *pi)
{
	int32 radio_temp[2*PHY_CORE_MAX], radio_temp2[2*PHY_CORE_MAX];
	uint16 syn_tempprocsense_save;
	int16 offset = 0;
	uint16 auxADC_Vmid, auxADC_Av, auxADC_Vmid_save, auxADC_Av_save;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Save radio registers that will be overwritten */
	syn_tempprocsense_save = read_radio_reg(pi, RADIO_2059_TEMPSENSE_CONFIG);


	/* Save current Aux ADC Settings */
	wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xa, 16,
	                         &auxADC_Vmid_save);
	wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xe, 16,
	                         &auxADC_Av_save);

	/* Set the Vmid and Av for the course reading */
	auxADC_Vmid = 0xce;
	auxADC_Av   = 0x3;
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xa, 16,
	                         &auxADC_Vmid);
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xe, 16,
	                         &auxADC_Av);

	/* Config temp sense readout; tempsense pu, clear flip (swap) bit */
	write_radio_reg(pi, RADIO_2059_TEMPSENSE_CONFIG, 0x05);

	/* Do the first temperature measurement and toggle flip bit */
	wlc_phy_poll_auxadc_htphy(pi, HTPHY_AUXADC_SEL_IQ, radio_temp, 1);
	write_radio_reg(pi, RADIO_2059_TEMPSENSE_CONFIG, 0x07);

	/* Do the second temperature measurement and toggle flip bit */
	wlc_phy_poll_auxadc_htphy(pi, HTPHY_AUXADC_SEL_IQ, radio_temp2, 1);

	/* Average readings w/o scaling, then use line-fit at 64 scale */
	/* Convert ADC values to deg C */
	radio_temp[0] = (88 * (radio_temp[1] + radio_temp2[1]) + 4315) / 64;

	/* Restore  Aux ADC Settings */
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xa, 16,
	                         &auxADC_Vmid_save);
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, 0xe, 16,
	                         &auxADC_Av_save);

	/* Restore radio registers */
	write_radio_reg(pi, RADIO_2059_TEMPSENSE_CONFIG, syn_tempprocsense_save);

	PHY_THERMAL(("Tempsense: m1= %3i  m2= %3i  ^C= %3i\n",
	             radio_temp[1], radio_temp2[1], radio_temp[0]));

	offset = (int16) pi->phy_tempsense_offset;

	return ((int16) radio_temp[0] + offset);
}


static void
wlc_phy_get_tx_bbmult(phy_info_t *pi, uint16 *bb_mult, uint16 core)
{
	uint16 tbl_ofdm_offset[] = { 99, 103, 107, 111};

	wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
	                         tbl_ofdm_offset[core], 16,
	                         bb_mult);
}

static void
wlc_phy_set_tx_bbmult(phy_info_t *pi, uint16 *bb_mult, uint16 core)
{
	uint16 tbl_ofdm_offset[] = { 99, 103, 107, 111};
	uint16 tbl_bphy_offset[] = {115, 119, 123, 127};

	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
	                          tbl_ofdm_offset[core], 16,
	                          bb_mult);
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
	                          tbl_bphy_offset[core], 16,
	                          bb_mult);
}

static uint16
wlc_phy_gen_load_samples_htphy(phy_info_t *pi, uint32 f_kHz, uint16 max_val, uint8 dac_test_mode)
{
	uint8 phy_bw, is_phybw40;
	uint16 num_samps, t, bbc;
	fixed theta = 0, rot = 0;
	uint32 tbl_len;
	cint32* tone_buf = NULL;

	/* check phy_bw */
	is_phybw40 = CHSPEC_IS40(pi->radio_chanspec);
	phy_bw     = (is_phybw40 == 1)? 40 : 20;
	tbl_len    = (phy_bw << 3);

	if (dac_test_mode == 1) {
		bbc = read_phy_reg(pi, HTPHY_BBConfig);
		if (bbc & HTPHY_BBConfig_resample_clk160_MASK) {
			phy_bw = ((bbc >> HTPHY_BBConfig_resamplerFreq_SHIFT) & 1) ? 84 : 82;
		} else {
			phy_bw = 80;
		}
		phy_bw = (is_phybw40 == 1) ? (phy_bw << 1) : phy_bw;
		/* use smaller num_samps to prevent overflow the buffer length */
		tbl_len = (phy_bw << 1);
	}

	/* allocate buffer */
	if ((tone_buf = (cint32 *)MALLOC(pi->sh->osh, sizeof(cint32) * tbl_len)) == NULL) {
		PHY_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pi->sh->unit,
		          __FUNCTION__, MALLOCED(pi->sh->osh)));
		return 0;
	}

	/* set up params to generate tone */
	num_samps  = (uint16)tbl_len;
	rot = FIXED((f_kHz * 36)/phy_bw) / 100; /* 2*pi*f/bw/1000  Note: f in KHz */
	theta = 0;			/* start angle 0 */

	/* tone freq = f_c MHz ; phy_bw = phy_bw MHz ; # samples = phy_bw (1us) ; max_val = 151 */
	for (t = 0; t < num_samps; t++) {
		/* compute phasor */
		wlc_phy_cordic(theta, &tone_buf[t]);
		/* update rotation angle */
		theta += rot;
		/* produce sample values for play buffer */
		tone_buf[t].q = (int32)FLOAT(tone_buf[t].q * max_val);
		tone_buf[t].i = (int32)FLOAT(tone_buf[t].i * max_val);
	}

	/* load sample table */
	wlc_phy_loadsampletable_htphy(pi, tone_buf, num_samps);

	if (tone_buf != NULL)
		MFREE(pi->sh->osh, tone_buf, sizeof(cint32) * tbl_len);

	return num_samps;
}

int
wlc_phy_tx_tone_htphy(phy_info_t *pi, uint32 f_kHz, uint16 max_val, uint8 iqmode,
                      uint8 dac_test_mode, bool modify_bbmult)
{
	uint16 num_samps;
	uint16 loops = 0xffff;
	uint16 wait = 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Generate the samples of the tone and load it into the playback buffer */
	if ((num_samps = wlc_phy_gen_load_samples_htphy(pi, f_kHz, max_val, dac_test_mode)) == 0) {
		return BCME_ERROR;
	}

	/* Now, play the samples */
	wlc_phy_runsamples_htphy(pi, num_samps, loops, wait, iqmode, dac_test_mode, modify_bbmult);

	return BCME_OK;
}

static void
wlc_phy_loadsampletable_htphy(phy_info_t *pi, cint32 *tone_buf, uint16 num_samps)
{
	uint16 t;
	uint32* data_buf = NULL;

	/* allocate buffer */
	if ((data_buf = (uint32 *)MALLOC(pi->sh->osh, sizeof(uint32) * num_samps)) == NULL) {
		PHY_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pi->sh->unit,
		           __FUNCTION__, MALLOCED(pi->sh->osh)));
		return;
	}

	/* load samples into sample play buffer */
	for (t = 0; t < num_samps; t++) {
		data_buf[t] = ((((unsigned int)tone_buf[t].i) & 0x3ff) << 10) |
		               (((unsigned int)tone_buf[t].q) & 0x3ff);
	}
	wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_SAMPLEPLAY, num_samps, 0, 32, data_buf);

	if (data_buf != NULL)
		MFREE(pi->sh->osh, data_buf, sizeof(uint32) * num_samps);
}

static void
wlc_phy_init_lpf_sampleplay(phy_info_t *pi)
{
	/* copy BW20/40 entry from rx2tx_lpf_rc_lut to sampleplay */
	uint16 rfseq_splay_offsets[]     = {0x14A, 0x15A, 0x16A};
	uint16 rfseq_lpf_rc_offsets_40[] = {0x149, 0x159, 0x169};
	uint16 rfseq_lpf_rc_offsets_20[] = {0x144, 0x154, 0x164};
	uint16 *rfseq_lpf_rc_offsets = NULL;
	uint16 core, val;

	if (CHSPEC_IS40(pi->radio_chanspec)) {
		rfseq_lpf_rc_offsets = rfseq_lpf_rc_offsets_40;
	} else {
		rfseq_lpf_rc_offsets = rfseq_lpf_rc_offsets_20;
	}
	FOREACH_CORE(pi, core) {
		wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_RFSEQ,
		                         1, rfseq_lpf_rc_offsets[core], 16, &val);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ,
		                          1, rfseq_splay_offsets[core], 16, &val);
	}
}

static void
wlc_phy_runsamples_htphy(phy_info_t *pi, uint16 num_samps, uint16 loops, uint16 wait, uint8 iqmode,
                         uint8 dac_test_mode, bool modify_bbmult)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 bb_mult;
	uint8  core, sample_cmd;
	uint16 orig_RfseqCoreActv;

	/* if bb_mult_save does not exist, save current bb_mult */
	if (pi_ht->bb_mult_save_valid == 0) {
		FOREACH_CORE(pi, core) {
			wlc_phy_get_tx_bbmult(pi, &pi_ht->bb_mult_save[core], core);
		}
		pi_ht->bb_mult_save_valid = 1;
	}

	/* set samp_play -> DAC_out loss to 0dB by setting bb_mult (2.6 format) to
	 * 100/64 for bw = 20MHz
	 *  71/64 for bw = 40Mhz
	 */
	if (modify_bbmult) {
		bb_mult = (CHSPEC_IS20(pi->radio_chanspec) ? 100 : 71);
		FOREACH_CORE(pi, core) {
			wlc_phy_set_tx_bbmult(pi, &bb_mult, core);
		}
	}

	/* configure sample play buffer */
	write_phy_reg(pi, HTPHY_sampleDepthCount, num_samps-1);

	if (loops != 0xffff) { /* 0xffff means: keep looping forever */
		write_phy_reg(pi, HTPHY_sampleLoopCount, loops - 1);
	} else {
		write_phy_reg(pi, HTPHY_sampleLoopCount, loops);
	}
	write_phy_reg(pi, HTPHY_sampleInitWaitCount, wait);

	/* start sample play buffer (in regular mode or iqcal mode) */
	orig_RfseqCoreActv = read_phy_reg(pi, HTPHY_RfseqMode);
	or_phy_reg(pi, HTPHY_RfseqMode, HTPHY_RfseqMode_CoreActv_override_MASK);
	and_phy_reg(pi, HTPHY_sampleCmd, ~HTPHY_sampleCmd_DacTestMode_MASK);
	and_phy_reg(pi, HTPHY_sampleCmd, ~HTPHY_sampleCmd_start_MASK);
	and_phy_reg(pi, HTPHY_iqloCalCmdGctl, 0x3FFF);
	if (iqmode) {
		or_phy_reg(pi, HTPHY_iqloCalCmdGctl, 0x8000);
	} else {
		sample_cmd = HTPHY_sampleCmd_start_MASK;
		sample_cmd |= (dac_test_mode == 1 ? HTPHY_sampleCmd_DacTestMode_MASK : 0);
		or_phy_reg(pi, HTPHY_sampleCmd, sample_cmd);
	}

	/* Wait till the Rx2Tx sequencing is done */
	SPINWAIT(((read_phy_reg(pi, HTPHY_RfseqStatus0) & 0x1) == 1), 1000);

	/* restore mimophyreg(RfseqMode.CoreActv_override) */
	write_phy_reg(pi, HTPHY_RfseqMode, orig_RfseqCoreActv);
}

void
wlc_phy_stopplayback_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 playback_status;
	uint8 core;

	/* check status register */
	playback_status = read_phy_reg(pi, HTPHY_sampleStatus);
	if (playback_status & 0x1) {
		or_phy_reg(pi, HTPHY_sampleCmd, HTPHY_sampleCmd_stop_MASK);
	} else if (playback_status & 0x2) {
		and_phy_reg(pi, HTPHY_iqloCalCmdGctl,
		            (uint16)~HTPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
	} else {
		PHY_CAL(("wlc_phy_stopplayback_htphy: already disabled\n"));
	}
	/* disable the dac_test mode */
	and_phy_reg(pi, HTPHY_sampleCmd, ~HTPHY_sampleCmd_DacTestMode_MASK);

	/* if bb_mult_save does exist, restore bb_mult and undef bb_mult_save */
	if (pi_ht->bb_mult_save_valid != 0) {
		FOREACH_CORE(pi, core) {
			wlc_phy_set_tx_bbmult(pi, &pi_ht->bb_mult_save[core], core);
		}
		pi_ht->bb_mult_save_valid = 0;
	}

	wlc_phy_resetcca_htphy(pi);
}

static void
wlc_phy_txcal_radio_setup_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_txcal_radioregs_t *porig = &(pi_ht->htphy_txcal_radioregs_orig);
	uint16 core;

	/* SETUP: set 2059 into iq/lo cal state while saving off orig state */
	FOREACH_CORE(pi, core) {
		/* save off orig */
		porig->RF_TX_tx_ssi_master[core] =
			read_radio_reg(pi, RADIO_2059_TX_TX_SSI_MASTER(core));
		porig->RF_TX_tx_ssi_mux[core] =
			read_radio_reg(pi, RADIO_2059_TX_TX_SSI_MUX(core));
		porig->RF_TX_tssia[core] =
			read_radio_reg(pi, RADIO_2059_TX_TSSIA(core));
		porig->RF_TX_tssig[core] =
			read_radio_reg(pi, RADIO_2059_TX_TSSIG(core));
		porig->RF_TX_iqcal_vcm_hg[core] =
			read_radio_reg(pi, RADIO_2059_TX_IQCAL_VCM_HG(core));
		porig->RF_TX_iqcal_idac[core] =
			read_radio_reg(pi, RADIO_2059_TX_IQCAL_IDAC(core));
		porig->RF_TX_tssi_misc1[core] =
			read_radio_reg(pi, RADIO_2059_TX_TSSI_MISC1(core));
		porig->RF_TX_tssi_vcm[core] =
			read_radio_reg(pi, RADIO_2059_TX_TSSI_VCM(core));

		/* now write desired values */
		write_radio_reg(pi, RADIO_2059_TX_IQCAL_VCM_HG(core), 0x43);
		write_radio_reg(pi, RADIO_2059_TX_IQCAL_IDAC(core), 0x55);
		write_radio_reg(pi, RADIO_2059_TX_TSSI_VCM(core), 0x00);
		write_radio_reg(pi, RADIO_2059_TX_TSSI_MISC1(core), 0x00);

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MASTER(core), 0x0a);
			write_radio_reg(pi, RADIO_2059_TX_TSSIG(core), 0x00);
			write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MUX(core), 0x4);
			write_radio_reg(pi, RADIO_2059_TX_TSSIA(core), 0x31); /* pad */
			/* write_radio_reg(pi, RADIO_2059_TX_TSSIA, 0x21); */ /* ipa */
		} else {
			write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MASTER(core), 0x06);
			write_radio_reg(pi, RADIO_2059_TX_TSSIA(core), 0x00);
			write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MUX(core), 0x06);
			write_radio_reg(pi, RADIO_2059_TX_TSSIG(core), 0x31); /* pad tapoff */
			/* write_radio_reg(pi, RADIO_2059_TX_TSSIG, 0x21); */ /* ipa */
		}
	} /* for core */
}

static void
wlc_phy_txcal_radio_cleanup_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_txcal_radioregs_t *porig = &(pi_ht->htphy_txcal_radioregs_orig);
	uint16 core;

	/* CLEANUP: restore reg values */
	FOREACH_CORE(pi, core) {
		write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MASTER(core),
		                porig->RF_TX_tx_ssi_master[core]);
		write_radio_reg(pi, RADIO_2059_TX_TX_SSI_MUX(core),
		                porig->RF_TX_tx_ssi_mux[core]);
		write_radio_reg(pi, RADIO_2059_TX_TSSIA(core),
		                porig->RF_TX_tssia[core]);
		write_radio_reg(pi, RADIO_2059_TX_TSSIG(core),
		                porig->RF_TX_tssig[core]);
		write_radio_reg(pi, RADIO_2059_TX_IQCAL_VCM_HG(core),
		                porig->RF_TX_iqcal_vcm_hg[core]);
		write_radio_reg(pi, RADIO_2059_TX_IQCAL_IDAC(core),
		                porig->RF_TX_iqcal_idac[core]);
		write_radio_reg(pi, RADIO_2059_TX_TSSI_MISC1(core),
		                porig->RF_TX_tssi_misc1[core]);
		write_radio_reg(pi, RADIO_2059_TX_TSSI_VCM(core),
		                porig->RF_TX_tssi_vcm[core]);
	} /* for core */
}

static void
wlc_phy_txcal_phy_setup_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_txcal_phyregs_t *porig = &(pi_ht->ht_txcal_phyregs_orig);
	uint16 val;
	uint8  core;

	/*  SETUP: save off orig reg values and configure for cal  */
	FOREACH_CORE(pi, core) {

		/* AUX-ADC ("rssi") selection (could use aux_adc_sel function instead) */
		porig->Afectrl[core] = read_phy_reg(pi, HTPHY_Afectrl(core));
		porig->AfectrlOverride[core] = read_phy_reg(pi, HTPHY_AfectrlOverride(core));
		MOD_PHYREGC(pi, HTPHY_Afectrl,         core, rssi_select_i, 2);
		MOD_PHYREGC(pi, HTPHY_Afectrl,         core, rssi_select_q, 2);
		MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_i, 1);
		MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, rssi_select_q, 1);

		/* Aux-ADC Offset-binary mode (bypass conversion to 2's comp) */
		wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, core*0x10 +3, 16,
		                         &porig->Afectrl_AuxADCmode[core]);
		val = 0;   /* 0 means "no conversion", i.e., use offset binary as is */
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, core*0x10 +3, 16, &val);

		/* Power Down External PA (simply always do 2G & 5G), and set T/R to R */
		porig->RfctrlIntc[core] = read_phy_reg(pi, HTPHY_RfctrlIntc(core));
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, ext_2g_papu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, ext_5g_papu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, tr_sw_tx_pu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, tr_sw_rx_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, override_ext_pa, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, core, override_tr_sw, 1);

		/* Power Down Internal PA */
		porig->RfctrlPU[core] = read_phy_reg(pi, HTPHY_RfctrlPU(core));
		porig->RfctrlOverride[core] = read_phy_reg(pi, HTPHY_RfctrlOverride(core));
		MOD_PHYREGC(pi, HTPHY_RfctrlPU,       core, intpa_pu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, core, intpa_pu, 1);


	} /* for core */

	/* Disable the re-sampler (in case spur avoidance is on) */
	porig->BBConfig = read_phy_reg(pi, HTPHY_BBConfig);
	MOD_PHYREG(pi, HTPHY_BBConfig, resample_clk160, 0);
}


static void
wlc_phy_txcal_phy_cleanup_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_txcal_phyregs_t *porig = &(pi_ht->ht_txcal_phyregs_orig);
	uint8  core;

	/*  CLEANUP: Restore Original Values  */
	FOREACH_CORE(pi, core) {

		/* Restore AUX-ADC Select */
		write_phy_reg(pi, HTPHY_Afectrl(core), porig->Afectrl[core]);
		write_phy_reg(pi, HTPHY_AfectrlOverride(core), porig->AfectrlOverride[core]);

		/* Restore AUX-ADC format */
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_AFECTRL, 1, core*0x10 + 3,
		                          16, &porig->Afectrl_AuxADCmode[core]);

		/* restore ExtPA PU & TR */
		write_phy_reg(pi, HTPHY_RfctrlIntc(core), porig->RfctrlIntc[core]);

		/* restore IntPA PU */
		write_phy_reg(pi, HTPHY_RfctrlPU(core), porig->RfctrlPU[core]);
		write_phy_reg(pi, HTPHY_RfctrlOverride(core), porig->RfctrlOverride[core]);


	} /* for core */

	/* Re-enable re-sampler (in case spur avoidance is on) */
	write_phy_reg(pi, HTPHY_BBConfig, porig->BBConfig);

	wlc_phy_resetcca_htphy(pi);
}

void
wlc_phy_cals_htphy(phy_info_t *pi, uint8 searchmode)
{

	uint8 tx_pwr_ctrl_state;
	uint8 phase_id = pi->cal_info->cal_phase_id;
	uint16 tbl_cookie = TXCAL_CACHE_VALID;
	htphy_cal_result_t *htcal = &pi->cal_info->u.htcal;

	PHY_TRACE(("wl%d: Running HTPHY periodic calibration\n", pi->sh->unit));

	/* -----------------
	 *  Initializations
	 * -----------------
	 */

	/* Exit immediately if we are running on Quickturn */
	if (ISSIM_ENAB(pi->sh->sih)) {
		wlc_phy_cal_perical_mphase_reset(pi);
		return;
	}

	/* skip cal if phy is muted */
	if (PHY_MUTED(pi)) {
		return;
	}


	/*
	 * Search-Mode Sanity Check for Tx-iqlo-Cal
	 *
	 * Notes: - "RESTART" means: start with 0-coeffs and use large search radius
	 *        - "REFINE"  means: start with latest coeffs and only search 
	 *                    around that (faster)
	 *        - here, if channel has changed or no previous valid coefficients
	 *          are available, enforce RESTART search mode (this shouldn't happen
	 *          unless cal driver code is work-in-progress, so this is merely a safety net)
	 */
	if ((pi->radio_chanspec != pi->last_cal_chanspec) ||
	    (htcal->txiqlocal_coeffsvalid == 0)) {
		searchmode = PHY_CAL_SEARCHMODE_RESTART;
	}

	/*
	 * If previous phase of multiphase cal was on different channel, 
	 * then restart multiphase cal on current channel (again, safety net)
	 */
	if ((phase_id > MPHASE_CAL_STATE_INIT)) {
		if (pi->last_cal_chanspec != pi->radio_chanspec) {
			wlc_phy_cal_perical_mphase_restart(pi);
		}
	}


	/* Make the ucode send a CTS-to-self packet with duration set to 10ms. This
	 *  prevents packets from other STAs/AP from interfering with Rx IQcal
	 */

	/* Disable Power control */
	tx_pwr_ctrl_state = pi->txpwrctrl;
	wlc_phy_txpwrctrl_enable_htphy(pi, PHY_TPC_HW_OFF);

	/* Prepare Mac and Phregs */
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);


	/* -------------------
	 *  Calibration Calls
	 * -------------------
	 */

	PHY_NONE(("wlc_phy_cals_htphy: Time=%d, LastTi=%d, SrchMd=%d, PhIdx=%d,"
		" Chan=%d, LastCh=%d, First=%d, vld=%d\n",
		pi->sh->now, pi->last_cal_time, searchmode, phase_id,
		pi->radio_chanspec, pi->last_cal_chanspec,
		pi->first_cal_after_assoc, htcal->txiqlocal_coeffsvalid));

	if (phase_id == MPHASE_CAL_STATE_IDLE) {

		/* 
		 * SINGLE-SHOT Calibrations
		 *
		 *    Call all Cals one after another
		 *
		 *    Notes: 
		 *    - if this proc is called with the phase state in IDLE, 
		 *      we know that this proc was called directly rather
		 *      than via the mphase scheduler (the latter puts us into 
		 *      INIT state); under those circumstances, perform immediate
		 *      execution over all cal tasks 
		 *    - for better code structure, we would use the below mphase code for
		 *      sphase case, too, by utilizing an appropriate outer for-loop
		 */

		/* TO-DO: Ensure that all inits and cleanups happen here */

		/* carry out all phases "en bloc", for comments see the various phases below */
		pi->cal_info->last_cal_time     = pi->sh->now;
		pi->last_cal_chanspec = pi->radio_chanspec;
		wlc_phy_precal_txgain_htphy(pi, htcal->txcal_txgain);
		wlc_phy_cal_txiqlo_htphy(pi, searchmode, FALSE); /* request "Sphase" */
		wlc_phy_cal_rxiq_htphy(pi, FALSE);
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
		                          IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);
		pi->txcal_cache_cookie = 0;
		pi->first_cal_after_assoc = FALSE;

	} else {

		/* 
		 * MULTI-PHASE CAL
		 * 
		 *   Carry out next step in multi-phase execution of cal tasks
		 *
		 */

		switch (phase_id) {
		case CAL_PHASE_INIT:

			/* 
			 *   Housekeeping & Pre-Txcal Tx Gain Adjustment
			 */

			/* remember time and channel of this cal event */
			pi->cal_info->last_cal_time     = pi->sh->now;
			pi->last_cal_chanspec = pi->radio_chanspec;

			/* PreCal Gain Ctrl: 
			 * get tx gain settings (radio, dac, bbmult) to be used throughout
			 * tx iqlo cal; also resets "ladder_updated" flags in pi to 0
			 */
			wlc_phy_precal_txgain_htphy(pi, htcal->txcal_txgain);

			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case CAL_PHASE_TX0:
		case CAL_PHASE_TX1:
		case CAL_PHASE_TX2:
		case CAL_PHASE_TX3:
		case CAL_PHASE_TX4:
		case CAL_PHASE_TX5:
		case CAL_PHASE_TX6:
		case CAL_PHASE_TX7:
		case CAL_PHASE_TX_LAST:

			/* 
			 *   Tx-IQLO-Cal 
			 */

			/* to ensure radar detect is skipped during cals */
			if ((pi->radar_percal_mask & 0x10) != 0) {
				pi->radar_cal_active = TRUE;
			}

			/* execute txiqlo cal's next phase */
			if (wlc_phy_cal_txiqlo_htphy(pi, searchmode, TRUE) != BCME_OK) {
				/* rare case, just reset */
				PHY_ERROR(("wlc_phy_cal_txiqlo_htphy failed\n"));
				wlc_phy_cal_perical_mphase_reset(pi);
				break;
			}

			/* move on */
			if ((pi->cal_info->cal_phase_id == CAL_PHASE_TX_LAST) &&
			    (!PHY_IPA(pi))) {
				pi->cal_info->cal_phase_id++; /* so we skip papd cal for non-ipa */
			}
			pi->cal_info->cal_phase_id++;
			break;

		case CAL_PHASE_PAPDCAL:


			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case CAL_PHASE_RXCAL:
			/*  
			 *   Rx IQ Cal 
			 */
			if ((pi->radar_percal_mask & 0x1) != 0) {
				pi->radar_cal_active = TRUE;
			}

			wlc_phy_cal_rxiq_htphy(pi, FALSE);
			wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1,
				IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);
			pi->txcal_cache_cookie = 0;
			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case CAL_PHASE_RSSICAL:

			/* 
			 *     RSSI Cal & VCO Cal
			 */

			if ((pi->radar_percal_mask & 0x4) != 0) {
			    pi->radar_cal_active = TRUE;
			}

			/* RSSI & VCO cal (prevents VCO/PLL from losing lock with temp delta) */
			/* wlc_phy_rssi_cal_htphy(pi); */
			wlc_phy_radio2059_vcocal(pi);

			/* If this is the first calibration after association then we 
			 * still have to do calibrate the idle-tssi, otherrwise done
			 */
			if (pi->first_cal_after_assoc) {
				pi->cal_info->cal_phase_id++;
			} else {
				wlc_phy_cal_perical_mphase_reset(pi);
			}
			break;

		case CAL_PHASE_IDLETSSI:

			/* 
			 *     Idle TSSI & TSSI-to-dBm Mapping Setup
			 */


			if ((pi->radar_percal_mask & 0x8) != 0)
				pi->radar_cal_active = TRUE;

			/* Idle TSSI determination once right after join/up/assoc */
			wlc_phy_txpwrctrl_idle_tssi_meas_htphy(pi);
			wlc_phy_txpwrctrl_pwr_setup_htphy(pi); /* to write idle tssi */

			/* done with multi-phase cal, reset phase */
			pi->first_cal_after_assoc = FALSE;
			wlc_phy_cal_perical_mphase_reset(pi);
			break;

		default:
			PHY_ERROR(("wlc_phy_cals_phy: Invalid calibration phase %d\n", phase_id));
			ASSERT(0);
			wlc_phy_cal_perical_mphase_reset(pi);
			break;
		}
	}

	/* ----------
	 *  Cleanups
	 * ----------
	 */

	wlc_phy_txpwrctrl_enable_htphy(pi, tx_pwr_ctrl_state);
	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

}

static int
wlc_phy_cal_txiqlo_htphy(phy_info_t *pi, uint8 searchmode, uint8 mphase)
{
	uint8  phy_bw, bw_idx, rd_select = 0, wr_select1 = 0, wr_select2 = 0;
	uint16 tone_ampl;
	uint16 tone_freq;
	int    bcmerror = BCME_OK;
	uint8  num_cmds_total, num_cmds_per_core;
	uint8  cmd_idx, cmd_stop_idx, core, cal_type;
	uint16 *cmds;
	uint16 cmd;
	uint16 coeffs[2];
	uint16 *coeff_ptr;
	uint16 zero = 0;
	uint8  cr, k, num_cores;
	txgain_setting_t orig_txgain[4];
	htphy_cal_result_t *htcal = &pi->cal_info->u.htcal;

	/* -----------
	 *  Constants
	 * -----------
	 */

	/* Table of commands for RESTART & REFINE search-modes
	 *
	 *     This uses the following format (three hex nibbles left to right)
	 *      1. cal_type: 0 = IQ (a/b),   1 = deprecated
	 *                   2 = LOFT digital (di/dq)
	 *                   3 = LOFT analog, fine,   injected at mixer      (ei/eq)
	 *                   4 = LOFT analog, coarse, injected at mixer, too (fi/fq)
	 *      2. initial stepsize (in log2)
	 *      3. number of cal precision "levels"
	 *
	 *     Notes: - functions assumes that order of LOFT cal cmds will be f => e => d, 
	 *              where it's ok to have multiple cmds (say interrupted by IQ) of 
	 *              the same type; this is due to zeroing out of e and/or d that happens
	 *              even during REFINE cal to avoid a coefficient "divergence" (increasing
	 *              LOFT comp over time of different types that cancel each other)
	 *            - final cal cmd should NOT be analog LOFT cal (otherwise have to manually
	 *              pick up analog LOFT settings from best_coeffs and write to radio)
	 */
	uint16 cmds_RESTART[] =
		{ 0x434, 0x334, 0x084, 0x267, 0x056, 0x234};
	uint16 cmds_REFINE[] =
		{ 0x423, 0x334, 0x073, 0x267, 0x045, 0x234};

	/* zeros start coeffs (a,b,di/dq,ei/eq,fi/fq for each core) */
	uint16 start_coeffs_RESTART[] = {0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0};

	/* interval lengths for gain control and correlation segments
	 *   (top/bottom nibbles are for guard and measurement intlvs, resp., in log 2 # samples)
	 */
	uint8 nsamp_gctrl[] = {0x76, 0x87};
	uint8 nsamp_corrs[] = {0x68, 0x79};


	/* -------
	 *  Inits
	 * -------
	 */

	num_cores = pi->pubpi.phy_corenum;

	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);

	/* phy_bw */
	if (CHSPEC_IS40(pi->radio_chanspec)) {
		phy_bw = 40;
		bw_idx = 1;
	} else {
		phy_bw = 20;
		bw_idx = 0;
	}

	/* Put the radio and phy into TX iqlo cal state, including tx gains */
	wlc_phy_txcal_radio_setup_htphy(pi);
	wlc_phy_txcal_phy_setup_htphy(pi);
	wlc_phy_txcal_txgain_setup_htphy(pi, &htcal->txcal_txgain[0], &orig_txgain[0]);

	/* Set IQLO Cal Engine Gain Control Parameters including engine Enable
	 *   iqlocal_en<15> / start_index / thresh_d2 / ladder_length_d2
	 */
	write_phy_reg(pi, HTPHY_iqloCalCmdGctl, 0x8ad9);


	/*
	 *   Retrieve and set Start Coeffs
	 */
	if (pi->cal_info->cal_phase_id > CAL_PHASE_TX0) {
		/* mphase cal and have done at least 1 Tx phase already */
		coeff_ptr = htcal->txiqlocal_interm_coeffs; /* use results from previous phase */
	} else {
		/* single-phase cal or first phase of mphase cal */
		if (searchmode == PHY_CAL_SEARCHMODE_REFINE) {
			/* recal ("refine") */
			coeff_ptr = htcal->txiqlocal_coeffs; /* use previous cal's final results */
		} else {
			/* start from zero coeffs ("restart") */
			coeff_ptr = start_coeffs_RESTART; /* zero coeffs */
		}
		/* copy start coeffs to intermediate coeffs, for pairwise update from here on
		 *    (after all cmds/phases have filled this with latest values, this 
		 *    will be copied to OFDM/BPHY coeffs and to htcal->txiqlocal_coeffs 
		 *    for use by possible REFINE cal next time around)
		 */
		for (k = 0; k < 5*num_cores; k++) {
			htcal->txiqlocal_interm_coeffs[k] = coeff_ptr[k];
		}
	}
	for (core = 0; core < num_cores; core++) {
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 0,
		                                TB_START_COEFFS_AB, core);
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 2,
		                                TB_START_COEFFS_D,  core);
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 3,
		                                TB_START_COEFFS_E,  core);
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 4,
		                                TB_START_COEFFS_F,  core);
	}

	/* 
	 *   Choose Cal Commands for this Phase
	 */
	if (searchmode == PHY_CAL_SEARCHMODE_RESTART) {
		cmds = cmds_RESTART;
		num_cmds_per_core = ARRAYSIZE(cmds_RESTART);
		num_cmds_total    = num_cores * num_cmds_per_core;
	} else {
		cmds = cmds_REFINE;
		num_cmds_per_core = ARRAYSIZE(cmds_REFINE);
		num_cmds_total    = num_cores * num_cmds_per_core;
	}
	if (mphase) {
		/* multi-phase: get next subset of commands (first & last index) */
		cmd_idx = (pi->cal_info->cal_phase_id - CAL_PHASE_TX0) *
			MPHASE_TXCAL_CMDS_PER_PHASE; /* first cmd index in this phase */
		if ((cmd_idx + MPHASE_TXCAL_CMDS_PER_PHASE - 1) < num_cmds_total) {
			cmd_stop_idx = cmd_idx + MPHASE_TXCAL_CMDS_PER_PHASE - 1;
		} else {
			cmd_stop_idx = num_cmds_total - 1;
		}
	} else {
		/* single-phase: execute all commands for all cores */
		cmd_idx = 0;
		cmd_stop_idx = num_cmds_total - 1;
	}

	/* turn on test tone */
	tone_ampl = 250;
	tone_freq = (phy_bw == 20) ? 2500 : 5000;
	if (pi->cal_info->cal_phase_id > CAL_PHASE_TX0) {
		wlc_phy_runsamples_htphy(pi, phy_bw * 8, 0xffff, 0, 1, 0, FALSE);
		bcmerror = BCME_OK;
	} else {
		bcmerror = wlc_phy_tx_tone_htphy(pi, tone_freq, tone_ampl, 1, 0, FALSE);
	}

	PHY_NONE(("wlc_phy_cal_txiqlo_htphy (after inits): SearchMd=%d, MPhase=%d,"
		" CmdIds=(%d to %d)\n",
		searchmode, mphase, cmd_idx, cmd_stop_idx));

	/* ---------------
	 *  Cmd Execution
	 * ---------------
	 */

	if (bcmerror == BCME_OK) { /* in case tone doesn't start (still needed?) */

		/* loop over commands in this cal phase */
		for (; cmd_idx <= cmd_stop_idx; cmd_idx++) {

			/* get command, cal_type, and core */
			core     = cmd_idx / num_cmds_per_core; /* integer divide */
			cmd      = cmds[cmd_idx % num_cmds_per_core] | 0x8000 | (core << 12);
			cal_type = ((cmd & 0x0F00) >> 8);

			/* PHY_CAL(("wlc_phy_cal_txiqlo_htphy: Cmds => cmd_idx=%2d, Cmd=0x%04x,
			 *          cal_type=%d, core=%d\n", cmd_idx, cmd, cal_type, core));
			 */

			/* set up scaled ladders for desired bbmult of current core */
			if (!htcal->txiqlocal_ladder_updated[core]) {
				wlc_phy_cal_txiqlo_update_ladder_htphy(pi,
					htcal->txcal_txgain[core].bbmult);
				htcal->txiqlocal_ladder_updated[core] = TRUE;
			}

			/* set intervals settling and measurement intervals */
			write_phy_reg(pi, HTPHY_iqloCalCmdNnum,
			              (nsamp_corrs[bw_idx] << 8) | nsamp_gctrl[bw_idx]);

			/* if coarse-analog-LOFT cal (fi/fq), always zero out ei/eq and di/dq;
			 * if fine-analog-LOFT   cal (ei/dq), always zero out di/dq 
			 *   - even do this with search-type REFINE, to prevent a "drift"
			 *   - assumes that order of LOFT cal cmds will be f => e => d, where it's
			 *     ok to have multiple cmds (say interrupted by IQ cal) of the same type
			 */
			if ((cal_type == CAL_TYPE_LOFT_ANA_COARSE) ||
			    (cal_type == CAL_TYPE_LOFT_ANA_FINE)) {
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					&zero, TB_START_COEFFS_D, core);
			}
			if (cal_type == CAL_TYPE_LOFT_ANA_COARSE) {
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					&zero, TB_START_COEFFS_E, core);
			}

			/* now execute this command and wait max of ~20ms */
			write_phy_reg(pi, HTPHY_iqloCalCmd, cmd);
			SPINWAIT(((read_phy_reg(pi, HTPHY_iqloCalCmd) & 0xc000) != 0), 20000);
			ASSERT((read_phy_reg(pi, HTPHY_iqloCalCmd) & 0xc000) == 0);

			switch (cal_type) {
			case CAL_TYPE_IQ:
				rd_select  = TB_BEST_COEFFS_AB;
				wr_select1 = TB_START_COEFFS_AB;
				wr_select2 = PI_INTER_COEFFS_AB;
				break;
			case CAL_TYPE_LOFT_DIG:
				rd_select  = TB_BEST_COEFFS_D;
				wr_select1 = TB_START_COEFFS_D;
				wr_select2 = PI_INTER_COEFFS_D;
				break;
			case CAL_TYPE_LOFT_ANA_FINE:
				rd_select  = TB_BEST_COEFFS_E;
				wr_select1 = TB_START_COEFFS_E;
				wr_select2 = PI_INTER_COEFFS_E;
				break;
			case CAL_TYPE_LOFT_ANA_COARSE:
				rd_select  = TB_BEST_COEFFS_F;
				wr_select1 = TB_START_COEFFS_F;
				wr_select2 = PI_INTER_COEFFS_F;
				break;
			default:
				ASSERT(0);
			}
			wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
				coeffs, rd_select,  core);
			wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
				coeffs, wr_select1, core);
			wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
				coeffs, wr_select2, core);

		} /* command loop */

		/* single phase or last tx stage in multiphase cal: apply & store overall results */
		if ((mphase == 0) || (pi->cal_info->cal_phase_id == CAL_PHASE_TX_LAST)) {

			PHY_CAL(("wlc_phy_cal_txiqlo_htphy (mphase = %d, refine = %d):\n",
			         mphase, searchmode == PHY_CAL_SEARCHMODE_REFINE));
			for (cr = 0; cr < num_cores; cr++) {
				/* Save and Apply IQ Cal Results */
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_AB, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_AB, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_OFDM_COEFFS_AB,  cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_BPHY_COEFFS_AB,  cr);

				/* Save and Apply Dig LOFT Cal Results */
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_D, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_D, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_OFDM_COEFFS_D,  cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_BPHY_COEFFS_D,  cr);

				/* Apply Analog LOFT Comp
				 * - unncessary if final command on each core is digital
				 * LOFT-cal or IQ-cal
				 * - then the loft comp coeffs were applied to radio
				 * at the beginning of final command per core
				 * - this is assumed to be the case, so nothing done here
				 */

				/* Save Analog LOFT Comp in PI State */
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_E, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_E, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_F, cr);
				wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_F, cr);

				/* Print out Results */
				PHY_CAL(("\tcore-%d: a/b = (%4d,%4d), d = (%4d,%4d),"
					" e = (%4d,%4d), f = (%4d,%4d)\n", cr,
					htcal->txiqlocal_coeffs[cr*5+0],  /* a */
					htcal->txiqlocal_coeffs[cr*5+1],  /* b */
					(htcal->txiqlocal_coeffs[cr*5+2] & 0xFF00) >> 8, /* di */
					(htcal->txiqlocal_coeffs[cr*5+2] & 0x00FF),      /* dq */
					(htcal->txiqlocal_coeffs[cr*5+3] & 0xFF00) >> 8, /* ei */
					(htcal->txiqlocal_coeffs[cr*5+3] & 0x00FF),      /* eq */
					(htcal->txiqlocal_coeffs[cr*5+4] & 0xFF00) >> 8, /* fi */
					(htcal->txiqlocal_coeffs[cr*5+4] & 0x00FF)));   /* fq */
			} /* for cr */

			/* validate availability of results and store off channel */
			htcal->txiqlocal_coeffsvalid = TRUE;
			pi->last_cal_chanspec = pi->radio_chanspec;
		} /* writing of results */

		/* Switch off test tone */
		wlc_phy_stopplayback_htphy(pi);	/* mimophy_stop_playback */

		/* disable IQ/LO cal */
		write_phy_reg(pi, HTPHY_iqloCalCmdGctl, 0x0000);
	} /* if BCME_OK */


	/* clean Up PHY and radio */
	wlc_phy_txcal_txgain_cleanup_htphy(pi, &orig_txgain[0]);
	wlc_phy_txcal_phy_cleanup_htphy(pi);
	wlc_phy_txcal_radio_cleanup_htphy(pi);


	/*
	 *-----------*
	 *  Cleanup  *
	 *-----------
	 */

	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);

	return bcmerror;
}

static void
wlc_phy_cal_txiqlo_coeffs_htphy(phy_info_t *pi, uint8 rd_wr, uint16 *coeff_vals,
                                uint8 select, uint8 core) {

	/* handles IQLOCAL coefficients access (read/write from/to 
	 * iqloCaltbl and pi State) 
	 *
	 * not sure if reading/writing the pi state coeffs via this appraoch 
	 * is a bit of an overkill
	 */

	/* {num of 16b words to r/w, start offset (ie address), core-to-core block offset} */
	htphy_coeff_access_t coeff_access_info[] = {
		{2, 64, 8},  /* TB_START_COEFFS_AB   */
		{1, 67, 8},  /* TB_START_COEFFS_D    */
		{1, 68, 8},  /* TB_START_COEFFS_E    */
		{1, 69, 8},  /* TB_START_COEFFS_F    */
		{2, 128, 7}, /*   TB_BEST_COEFFS_AB  */
		{1, 131, 7}, /*   TB_BEST_COEFFS_D   */
		{1, 132, 7}, /*   TB_BEST_COEFFS_E   */
		{1, 133, 7}, /*   TB_BEST_COEFFS_F   */
		{2, 96,  4}, /* TB_OFDM_COEFFS_AB    */
		{1, 98,  4}, /* TB_OFDM_COEFFS_D     */
		{2, 112, 4}, /* TB_BPHY_COEFFS_AB    */
		{1, 114, 4}, /* TB_BPHY_COEFFS_D     */
		{2, 0, 5},   /*   PI_INTER_COEFFS_AB */
		{1, 2, 5},   /*   PI_INTER_COEFFS_D  */
		{1, 3, 5},   /*   PI_INTER_COEFFS_E  */
		{1, 4, 5},   /*   PI_INTER_COEFFS_F  */
		{2, 0, 5},   /* PI_FINAL_COEFFS_AB   */
		{1, 2, 5},   /* PI_FINAL_COEFFS_D    */
		{1, 3, 5},   /* PI_FINAL_COEFFS_E    */
		{1, 4, 5}    /* PI_FINAL_COEFFS_F    */
	};		 
	htphy_cal_result_t *htcal = &pi->cal_info->u.htcal;

	uint8 nwords, offs, boffs, k;

	/* get access info for desired choice */
	nwords = coeff_access_info[select].nwords;
	offs   = coeff_access_info[select].offs;
	boffs  = coeff_access_info[select].boffs;

	/* read or write given coeffs */
	if (select <= TB_BPHY_COEFFS_D) { /* START and BEST coeffs in Table */
		if (rd_wr == CAL_COEFF_READ) { /* READ */
			wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_IQLOCAL, nwords,
				offs + boffs*core, 16, coeff_vals);
		} else { /* WRITE */
			wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, nwords,
				offs + boffs*core, 16, coeff_vals);
		}
	} else if (select <= PI_INTER_COEFFS_F) { /* PI state intermediate coeffs */
		for (k = 0; k < nwords; k++) {
			if (rd_wr == CAL_COEFF_READ) { /* READ */
				coeff_vals[k] = htcal->txiqlocal_interm_coeffs[offs +
				                                               boffs*core + k];
			} else { /* WRITE */
				htcal->txiqlocal_interm_coeffs[offs +
				                               boffs*core + k] = coeff_vals[k];
			}
		}
	} else { /* PI state final coeffs */
		for (k = 0; k < nwords; k++) { /* PI state final coeffs */
			if (rd_wr == CAL_COEFF_READ) { /* READ */
				coeff_vals[k] = htcal->txiqlocal_coeffs[offs + boffs*core + k];
			} else { /* WRITE */
				htcal->txiqlocal_coeffs[offs + boffs*core + k] = coeff_vals[k];
			}
		}
	}
}

static void
wlc_phy_cal_txiqlo_update_ladder_htphy(phy_info_t *pi, uint16 bbmult)
{
	uint8  index;
	uint32 bbmult_scaled;
	uint16 tblentry;

	htphy_txiqcal_ladder_t ladder_lo[] = {
	{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
	{25, 0}, {25, 1}, {25, 2}, {25, 3}, {25, 4}, {25, 5},
	{25, 6}, {25, 7}, {35, 7}, {50, 7}, {71, 7}, {100, 7}};

	htphy_txiqcal_ladder_t ladder_iq[] = {
	{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
	{25, 0}, {35, 0}, {50, 0}, {71, 0}, {100, 0}, {100, 1},
	{100, 2}, {100, 3}, {100, 4}, {100, 5}, {100, 6}, {100, 7}};

	for (index = 0; index < 18; index++) {

		/* calculate and write LO cal gain ladder */
		bbmult_scaled = ladder_lo[index].percent * bbmult;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | ladder_lo[index].g_env;
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1, index, 16, &tblentry);

		/* calculate and write IQ cal gain ladder */
		bbmult_scaled = ladder_iq[index].percent * bbmult;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | ladder_iq[index].g_env;
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_IQLOCAL, 1, index+32, 16, &tblentry);
	}
}

static void
wlc_phy_txcal_txgain_setup_htphy(phy_info_t *pi, txgain_setting_t *txcal_txgain,
txgain_setting_t *orig_txgain)
{
	uint16 core;
	uint16 tmp;

	FOREACH_CORE(pi, core) {
		/* store off orig and set new tx radio gain */
		wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, (0x110 + core),
			16, &(orig_txgain[core].rad_gain));
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, (0x110 + core),
			16, &txcal_txgain[core].rad_gain);

		PHY_NONE(("\n radio gain = 0x%x, bbm=%d, dacgn = %d  \n",
			txcal_txgain[core].rad_gain,
			txcal_txgain[core].bbmult,
			txcal_txgain[core].dac_gain));

		/* store off orig and set new dac gain */
		tmp = read_phy_reg(pi, HTPHY_AfeseqInitDACgain);
		switch (core) {
		case 0:
			orig_txgain[0].dac_gain =
				(tmp & HTPHY_AfeseqInitDACgain_InitDACgain0_MASK) >>
				HTPHY_AfeseqInitDACgain_InitDACgain0_SHIFT;
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain0,
			           txcal_txgain[0].dac_gain);
			break;
		case 1:
			orig_txgain[1].dac_gain =
				(tmp & HTPHY_AfeseqInitDACgain_InitDACgain1_MASK) >>
				HTPHY_AfeseqInitDACgain_InitDACgain1_SHIFT;
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain1,
			           txcal_txgain[1].dac_gain);
			break;
		case 2:
			orig_txgain[2].dac_gain =
				(tmp & HTPHY_AfeseqInitDACgain_InitDACgain2_MASK) >>
				HTPHY_AfeseqInitDACgain_InitDACgain2_SHIFT;
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain2,
			           txcal_txgain[2].dac_gain);
			break;
		}

		/* store off orig and set new bbmult gain */
		wlc_phy_get_tx_bbmult(pi, &(orig_txgain[core].bbmult),  core);
		wlc_phy_set_tx_bbmult(pi, &(txcal_txgain[core].bbmult), core);
	}
}

static void
wlc_phy_txcal_txgain_cleanup_htphy(phy_info_t *pi, txgain_setting_t *orig_txgain)
{
	uint8 core;

	FOREACH_CORE(pi, core) {
		/* restore gains: DAC, Radio and BBmult */
		switch (core) {
		case 0:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain0,
			           orig_txgain[0].dac_gain);
			break;
		case 1:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain1,
			           orig_txgain[1].dac_gain);
			break;
		case 2:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain2,
			           orig_txgain[2].dac_gain);
			break;
		}
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, (0x110 + core),
			16, &(orig_txgain[core].rad_gain));
		wlc_phy_set_tx_bbmult(pi, &(orig_txgain[core].bbmult), core);
	}
}

static void
wlc_phy_precal_txgain_htphy(phy_info_t *pi, txgain_setting_t *target_gains)
{
	/*   This function determines the tx gain settings to be
	 *   used during tx iqlo calibration; that is, it sends back
	 *   the following settings for each core:
	 *       - radio gain
	 *       - dac gain
	 *       - bbmult
	 *   This is accomplished by choosing a predefined power-index, or by
	 *   setting gain elements explicitly to predefined values, or by
	 *   doing actual "pre-cal gain control". Either way, the idea is
	 *   to get a stable setting for which the swing going into the
	 *   envelope detectors is large enough for good "envelope ripple"
	 *   while avoiding distortion or EnvDet overdrive during the cal.
	 *
	 *   Note:
	 *       - this function and the calling infrastructure is set up 
	 *         in a way not to leave behind any modified state; this 
	 *         is in contrast to mimophy ("nphy"); in htphy, only the
	 *         desired gain quantities are set/found and set back
	 */

	uint8 core;
	htphy_cal_result_t *htcal = &pi->cal_info->u.htcal;

	/* reset ladder_updated flags so tx-iqlo-cal ensures appropriate recalculation */
	FOREACH_CORE(pi, core) {
		htcal->txiqlocal_ladder_updated[core] = 0;
	}

	/* get target tx gain settings */
	FOREACH_CORE(pi, core) {
		if (1) {
			/* specify tx gain by index (reads from tx power table) */
			int8 target_pwr_idx = 0;
			wlc_phy_get_txgain_settings_by_index(
				pi, &(target_gains[core]), target_pwr_idx);
		} else if (0) {
			/* specify tx gain as hardcoded explicit gain */
			target_gains[core].rad_gain  = 0x000;
			target_gains[core].dac_gain  = 0;
			target_gains[core].bbmult = 64;
		} else {
			/* actual precal gain control */
		}
	}
}

/* see also: proc htphy_rx_iq_comp { {vals {}} {cores {}}} */
static void
wlc_phy_rx_iq_comp_htphy(phy_info_t *pi, uint8 write, phy_iq_comp_t *pcomp, uint8 rx_core)
{
	/* write: 0 - fetch values from phyregs into *pcomp
	 *        1 - deposit values from *pcomp into phyregs
	 *        2 - set all coeff phyregs to 0
	 *
	 * rx_core: specify which core to fetch/deposit
	 */

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(write <= 2);

	/* write values */
	if (write == 0) {
		pcomp->a = read_phy_reg(pi, HTPHY_RxIQCompA(rx_core));
		pcomp->b = read_phy_reg(pi, HTPHY_RxIQCompB(rx_core));
	} else if (write == 1) {
		write_phy_reg(pi, HTPHY_RxIQCompA(rx_core), pcomp->a);
		write_phy_reg(pi, HTPHY_RxIQCompB(rx_core), pcomp->b);
	} else {
		write_phy_reg(pi, HTPHY_RxIQCompA(rx_core), 0);
		write_phy_reg(pi, HTPHY_RxIQCompB(rx_core), 0);
	}
}

/* see also: proc htphy_rx_iq_est { {num_samps 2000} {wait_time ""} } */
void
wlc_phy_rx_iq_est_htphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
                        uint8 wait_time, uint8 wait_for_crs)
{
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Get Rx IQ Imbalance Estimate from modem */
	write_phy_reg(pi, HTPHY_IqestSampleCount, num_samps);
	MOD_PHYREG(pi, HTPHY_IqestWaitTime, waitTime, wait_time);
	MOD_PHYREG(pi, HTPHY_IqestCmd, iqMode, wait_for_crs);

	MOD_PHYREG(pi, HTPHY_IqestCmd, iqstart, 1);

	/* wait for estimate */
	SPINWAIT(((read_phy_reg(pi, HTPHY_IqestCmd) & HTPHY_IqestCmd_iqstart_MASK) != 0), 10000);
	ASSERT((read_phy_reg(pi, HTPHY_IqestCmd) & HTPHY_IqestCmd_iqstart_MASK) == 0);

	if ((read_phy_reg(pi, HTPHY_IqestCmd) & HTPHY_IqestCmd_iqstart_MASK) == 0) {
		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		FOREACH_CORE(pi, core) {
			est[core].i_pwr = (read_phy_reg(pi, HTPHY_IqestipwrAccHi(core)) << 16) |
			        read_phy_reg(pi, HTPHY_IqestipwrAccLo(core));
			est[core].q_pwr = (read_phy_reg(pi, HTPHY_IqestqpwrAccHi(core)) << 16) |
			        read_phy_reg(pi, HTPHY_IqestqpwrAccLo(core));
			est[core].iq_prod = (read_phy_reg(pi, HTPHY_IqestIqAccHi(core)) << 16) |
			        read_phy_reg(pi, HTPHY_IqestIqAccLo(core));
			PHY_NONE(("wlc_phy_rx_iq_est_htphy: core%d "
			         "i_pwr = %u, q_pwr = %u, iq_prod = %d\n",
			         core, est[core].i_pwr, est[core].q_pwr, est[core].iq_prod));
		}
	} else {
		PHY_ERROR(("wlc_phy_rx_iq_est_htphy: IQ measurement timed out\n"));
	}
}

#if defined(BCMDBG) || defined(BCMDBG_DUMP)
/* dump calibration regs/info */
void
wlc_phy_cal_dump_htphy(phy_info_t *pi, struct bcmstrbuf *b)
{

	uint8 core;
	int16  a_reg, b_reg, a_int, b_int;
	uint16 ab_int[2], d_reg, eir, eqr, fir, fqr;

	bcm_bprintf(b, "Tx-IQ/LOFT-Cal:\n");
	FOREACH_CORE(pi, core) {
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ, ab_int,
			TB_OFDM_COEFFS_AB, core);
		wlc_phy_cal_txiqlo_coeffs_htphy(pi, CAL_COEFF_READ, &d_reg,
			TB_OFDM_COEFFS_D, core);
		eir = read_radio_reg(pi, RADIO_2059_TX_LOFT_FINE_I(core));
		eqr = read_radio_reg(pi, RADIO_2059_TX_LOFT_FINE_Q(core));
		fir = read_radio_reg(pi, RADIO_2059_TX_LOFT_COARSE_I(core));
		fqr = read_radio_reg(pi, RADIO_2059_TX_LOFT_COARSE_Q(core));
		bcm_bprintf(b, "   core-%d: a/b: (%4d,%4d), d: (%3d,%3d),"
			" e: (%3d,%3d), f: (%3d,%3d)\n",
			core, (int16) ab_int[0], (int16) ab_int[1],
			(int8)((d_reg & 0xFF00) >> 8), /* di */
			(int8)((d_reg & 0x00FF)),      /* dq */
			(int8)(-((eir & 0xF0) >> 4) + ((eir & 0xF))), /* ei */
			(int8)(-((eqr & 0xF0) >> 4) + ((eqr & 0xF))), /* eq */
			(int8)(-((fir & 0xF0) >> 4) + ((fir & 0xF))), /* fi */
			(int8)(-((fqr & 0xF0) >> 4) + ((fqr & 0xF))));  /* fq */
	}
	bcm_bprintf(b, "Rx-IQ-Cal:\n");
	FOREACH_CORE(pi, core) {
		a_reg = (int16) read_phy_reg(pi, HTPHY_RxIQCompA(core));
		b_reg = (int16) read_phy_reg(pi, HTPHY_RxIQCompB(core));
		a_int = (a_reg >= 512) ? a_reg - 1024 : a_reg; /* s9 format */
		b_int = (b_reg >= 512) ? b_reg - 1024 : b_reg;
		bcm_bprintf(b, "   core-%d: a/b = (%4d,%4d)\n", core, a_int, b_int);
	}
	return;
}
#endif	/*  defined(BCMDBG) || defined(BCMDBG_DUMP) */

/* see also: proc htphy_dac_adc_decouple_WAR80827 {cmd} */
static void
wlc_phy_cal_dac_adc_decouple_war_htphy(phy_info_t *pi, bool do_not_undo)
{

	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_dac_adc_decouple_war_t *pwar = &(pi_ht->ht_dac_adc_decouple_war_info);
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* if not rev0, return immediately, WAR not applicable */
	if (!(HTREV_IS(pi->pubpi.phy_rev, 0))) return;

	if (do_not_undo) {
		/* start WAR */

		/* sanity check to avoid multiple calling to this proc */
		ASSERT(!pwar->is_on);
		pwar->is_on = TRUE;

		/* Disable PAPD Control bits */
		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		FOREACH_CORE(pi, core) {
			/* shift & enable */
			pwar->PapdCalShifts[core] = read_phy_reg(pi, HTPHY_PapdCalShifts(core));
			pwar->PapdEnable[core] = read_phy_reg(pi, HTPHY_PapdEnable(core));
			write_phy_reg(pi, HTPHY_PapdCalShifts(core), 0);
			write_phy_reg(pi, HTPHY_PapdEnable(core), 0);
		}

		/* correlate, epsilon, cal-settle */
		pwar->PapdCalCorrelate = read_phy_reg(pi, HTPHY_PapdCalCorrelate);
		pwar->PapdEpsilonUpdateIterations =
			read_phy_reg(pi, HTPHY_PapdEpsilonUpdateIterations);
		pwar->PapdCalSettle = read_phy_reg(pi, HTPHY_PapdCalSettle);
		write_phy_reg(pi, HTPHY_PapdCalCorrelate, 0xfff);
		write_phy_reg(pi, HTPHY_PapdEpsilonUpdateIterations, 0x1ff);
		write_phy_reg(pi, HTPHY_PapdCalSettle, 0xfff);

		/* engage */
		write_phy_reg(pi, HTPHY_PapdCalStart, 1);

	} else {
		/* stop WAR */
		uint8 sample_play_was_active = 0;

		/* sanity check to avoid multiple calling to this proc */
		ASSERT(pwar->is_on);
		pwar->is_on = FALSE;

		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		FOREACH_CORE(pi, core) {
			/* shift, enable */
			write_phy_reg(pi, HTPHY_PapdCalShifts(core), pwar->PapdCalShifts[core]);
			write_phy_reg(pi, HTPHY_PapdEnable(core), pwar->PapdEnable[core]);
		}
		/* correlate, epsilon, cal-settle, start */
		write_phy_reg(pi, HTPHY_PapdCalCorrelate, pwar->PapdCalCorrelate);
		write_phy_reg(pi, HTPHY_PapdEpsilonUpdateIterations,
			pwar->PapdEpsilonUpdateIterations);
		write_phy_reg(pi, HTPHY_PapdCalSettle, pwar->PapdCalSettle);

		/* ensure that Sample Play is running (already or forcing it here);
		 * that way, the PAPD-cal clk is forced "on" and we can issue a
		 * successful reset
		 */
		if (READ_PHYREG(pi, HTPHY_sampleStatus, NormalPlay) == 0) {
			wlc_phy_tx_tone_htphy(pi, 0, 0, 0, 0, TRUE);
			sample_play_was_active = 0;
		} else {
			sample_play_was_active = 1;
		}

		/* do papd reset */
		MOD_PHYREG(pi, HTPHY_PapdCalAddress, papdReset, 1);
		MOD_PHYREG(pi, HTPHY_PapdCalAddress, papdReset, 0);

		/* turn tone off if we turned it on here */
		if (sample_play_was_active == 0) {
			wlc_phy_stopplayback_htphy(pi);
		}

		/* squash start bit (needed?) */
		write_phy_reg(pi, HTPHY_PapdCalStart, 0);

	}

}

/* see also: proc htphy_rx_iq_est_loopback { {num_samps 2000} {wait_time ""} } */
static void
wlc_phy_rx_iq_est_loopback_htphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
                                 uint8 wait_time, uint8 wait_for_crs)
{

	static const uint16 flip_masks[] = {
		HTPHY_IQFlip_adc1_MASK,
		HTPHY_IQFlip_adc2_MASK,
		HTPHY_IQFlip_adc3_MASK
	};
	static const uint16 flip_shifts[] = {
		HTPHY_IQFlip_adc1_SHIFT,
		HTPHY_IQFlip_adc2_SHIFT,
		HTPHY_IQFlip_adc3_SHIFT
	};

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (HTREV_IS(pi->pubpi.phy_rev, 0)) {
		uint16 flip_val = read_phy_reg(pi, HTPHY_IQFlip);
		uint8 core;

		wlc_phy_cal_dac_adc_decouple_war_htphy(pi, TRUE);
		wlc_phy_rx_iq_est_htphy(pi, est, num_samps, wait_time, wait_for_crs);
		wlc_phy_cal_dac_adc_decouple_war_htphy(pi, FALSE);

		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		FOREACH_CORE(pi, core) {
			if (((flip_val & flip_masks[core]) >> flip_shifts[core]) == 1) {
				uint32 true_qq = est[core].i_pwr;
				est[core].i_pwr = est[core].q_pwr;
				est[core].q_pwr = true_qq;
			}
		}

	} else {
		wlc_phy_rx_iq_est_htphy(pi, est, num_samps, wait_time, wait_for_crs);
	}

}

static int
wlc_phy_calc_ab_from_iq_htphy(phy_iq_est_t *est, phy_iq_comp_t *comp)
{
	int16  iq_nbits, qq_nbits, brsh, arsh;
	int32  iq = est->iq_prod;
	uint32 ii = est->i_pwr;
	uint32 qq = est->q_pwr;
	int32  a, b, temp;

	/* for each core, implement the following floating point
	 * operations quantized to 10 fractional bits:
	 *   a = -iq/ii
	 *   b = -1 + sqrt(qq/ii - a*a)
	 */

	/* ---- BEGIN a,b fixed point computation ---- */

	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);

	/*   a = -iq/ii */
	arsh = 10-(30-iq_nbits);
	if (arsh >= 0) {
		a = (-(iq << (30 - iq_nbits)) + (ii >> (1 + arsh)));
		temp = (int32) (ii >>  arsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return BCME_ERROR;
		}
	} else {
		a = (-(iq << (30 - iq_nbits)) + (ii << (-1 - arsh)));
		temp = (int32) (ii << -arsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return BCME_ERROR;
		}
	}
	a /= temp;

	/*   b = -1 + sqrt(qq/ii - a*a) */
	brsh = qq_nbits-31+20;
	if (brsh >= 0) {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii >>  brsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return BCME_ERROR;
		}
	} else {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii << -brsh);
		if (temp == 0) {
			PHY_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return BCME_ERROR;
		}
	}
	b /= temp;
	b -= a*a;
	b = (int32) wlc_phy_sqrt_int((uint32) b);
	b -= (1 << 10);

	comp->a = a & 0x3ff;
	comp->b = b & 0x3ff;

	/* ---- END a,b fixed point computation ---- */

	return BCME_OK;
}


/* see also: proc htphy_rx_iq_cal_calc_coeffs { {num_samps 0x4000} {cores} } */
static void
wlc_phy_calc_rx_iq_comp_htphy(phy_info_t *pi, uint16 num_samps, uint8 core_mask)
{
	phy_iq_comp_t coeffs[PHY_CORE_MAX];
	phy_iq_est_t  rx_imb_vals[PHY_CORE_MAX];
#if defined(BCMDBG)
	phy_iq_est_t  noise_vals[PHY_CORE_MAX];
	uint16 bbmult_orig[PHY_CORE_MAX], bbmult_zero = 0;
#endif
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* save coefficients */
	ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
	FOREACH_CORE(pi, core) {
		wlc_phy_rx_iq_comp_htphy(pi, 0, &(coeffs[core]), core);
	}

	/* get iq, ii, qq measurements from iq_est */
	wlc_phy_rx_iq_est_loopback_htphy(pi, rx_imb_vals, num_samps, 32, 0);

#if defined(BCMDBG)
	/* take noise measurement (for SNR calc, for information purposes only) */
	FOREACH_CORE(pi, core) {
		wlc_phy_get_tx_bbmult(pi, &(bbmult_orig[core]), core);
		wlc_phy_set_tx_bbmult(pi, &bbmult_zero, core);
	}
	wlc_phy_rx_iq_est_loopback_htphy(pi, noise_vals, num_samps, 32, 0);
	FOREACH_CORE(pi, core) {
		wlc_phy_set_tx_bbmult(pi, &(bbmult_orig[core]), core);
	}
#endif

	/* calculate coeffs on requested cores */
	FOREACH_ACTV_CORE(pi, core_mask, core) {

		/* reset coeffs */
		wlc_phy_rx_iq_comp_htphy(pi, 2, NULL, core);

		/* bounds check estimate info */

		/* calculate coefficients, map to 10 bit and apply */
		wlc_phy_calc_ab_from_iq_htphy(&(rx_imb_vals[core]), &(coeffs[core]));

#if defined(BCMDBG)
		/* Calculate SNR for debug */


		PHY_NONE(("wlc_phy_calc_rx_iq_comp_htphy: core%d => "
			"(S =%9d,  N =%9d,  K =%d)\n",
			core,
			rx_imb_vals[core].i_pwr + rx_imb_vals[core].q_pwr,
			noise_vals[core].i_pwr + noise_vals[core].q_pwr,
			num_samps));
#endif
	}

	/* apply coeffs */
	FOREACH_CORE(pi, core) {
		wlc_phy_rx_iq_comp_htphy(pi, 1, &(coeffs[core]), core);
	}

}

/* see also: proc htphy_rx_iq_cal_radio_config {{setup_or_cleanup "setup"}  {core 0}} */
static void
wlc_phy_rxcal_radio_config_htphy(phy_info_t *pi, bool setup_not_cleanup, uint8 rx_core)
{
	/* nowadays have setup and cleanup in one proc, to ease
	 * consistency between the two
	 */

	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_rxcal_radioregs_t *porig = &(pi_ht->ht_rxcal_radioregs_orig);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (setup_not_cleanup) {
		/*
		 *-------------
		 * Radio Setup
		 *-------------
		 */

		/* sanity check */
		ASSERT(!porig->is_orig);
		porig->is_orig = TRUE;

		/* save off old reg values to the global array */
		porig->RF_TX_txrxcouple_2g_pwrup[rx_core] =
			read_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_PWRUP(rx_core));
		porig->RF_TX_txrxcouple_2g_atten[rx_core] =
			read_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_ATTEN(rx_core));
		porig->RF_TX_txrxcouple_5g_pwrup[rx_core] =
			read_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_PWRUP(rx_core));
		porig->RF_TX_txrxcouple_5g_atten[rx_core] =
			read_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_ATTEN(rx_core));
		porig->RF_rxbb_bias_master[rx_core] =
			read_radio_reg(pi, RADIO_2059_RXBB_BIAS_MASTER(rx_core));
		porig->RF_afe_vcm_cal_master[rx_core] =
			read_radio_reg(pi, RADIO_2059_AFE_VCM_CAL_MASTER(rx_core));
		porig->RF_afe_set_vcm_i[rx_core] =
			read_radio_reg(pi, RADIO_2059_AFE_SET_VCM_I(rx_core));
		porig->RF_afe_set_vcm_q[rx_core] =
			read_radio_reg(pi, RADIO_2059_AFE_SET_VCM_Q(rx_core));

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_PWRUP(rx_core), 0x03);
			write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_ATTEN(rx_core), 0x7f);
		} else {
			write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_PWRUP(rx_core), 0x03);
			write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_ATTEN(rx_core), 0x0f);
		}


		/* Lower bias current into Rx Buffer */
		write_radio_reg(pi, RADIO_2059_RXBB_BIAS_MASTER(rx_core), 0x38);

		mod_radio_reg(pi, RADIO_2059_AFE_VCM_CAL_MASTER(rx_core), 0x2, 0x0);
		write_radio_reg(pi, RADIO_2059_AFE_SET_VCM_I(rx_core), 0x27);
		write_radio_reg(pi, RADIO_2059_AFE_SET_VCM_Q(rx_core), 0x27);

	} else {
		/*
		 *---------------
		 * Radio Cleanup
		 *---------------
		 */

		/* sanity check */
		ASSERT(porig->is_orig);
		porig->is_orig = FALSE;

		write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_PWRUP(rx_core),
		                porig->RF_TX_txrxcouple_2g_pwrup[rx_core]);
		write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_2G_ATTEN(rx_core),
		                porig->RF_TX_txrxcouple_2g_atten[rx_core]);
		write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_PWRUP(rx_core),
		                porig->RF_TX_txrxcouple_5g_pwrup[rx_core]);
		write_radio_reg(pi, RADIO_2059_TX_TXRXCOUPLE_5G_ATTEN(rx_core),
		                porig->RF_TX_txrxcouple_5g_atten[rx_core]);
		write_radio_reg(pi, RADIO_2059_RXBB_BIAS_MASTER(rx_core),
		                porig->RF_rxbb_bias_master[rx_core]);
		write_radio_reg(pi, RADIO_2059_AFE_VCM_CAL_MASTER(rx_core),
		                porig->RF_afe_vcm_cal_master[rx_core]);
		write_radio_reg(pi, RADIO_2059_AFE_SET_VCM_I(rx_core),
		                porig->RF_afe_set_vcm_i[rx_core]);
		write_radio_reg(pi, RADIO_2059_AFE_SET_VCM_Q(rx_core),
		                porig->RF_afe_set_vcm_q[rx_core]);
	}

}

/* see also: proc htphy_rx_iq_cal_phy_config {setup_or_cleanup  core} */
static void
wlc_phy_rxcal_phy_config_htphy(phy_info_t *pi, bool setup_not_cleanup, uint8 rx_core)
{
	/* Notes: 
	 *
	 * (1) nowadays have setup & cleanup in one proc to ease consistency b/w the two
	 *
	 * (2) Reminder on why we turn off the resampler
	 *   - quick reminder of the motivation (on 4322): Turning on SamplePlay
	 *     does/did not automatically enable the resampler clocks in Spur Channels
	 *     (this was a HW bug); therefore, we have two choices: either, 
	 *         (A) disable resampling and accept that the tone freqs will be slightly off
	 *     from what the tone_freq arguments indicate (who cares)
	 *         (B) turn on forcegatedclock when SamplePlay is active to enforce the
	 *    Tx resampler to be active despite the HW bug 
	 *    => Currently, we use (A). On chips >4322, supposedly SamplePlay now enables 
	 *    the right clocks, but we maintain the old approach regardless for now
	 *
	 *   - also note that in the driver we do a resetCCA after this to be on the safe 
	 *     side; may want to revisit this here, too, in case we run into issues
	 */

	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	htphy_rxcal_phyregs_t *porig = &(pi_ht->ht_rxcal_phyregs_orig);
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (setup_not_cleanup) {
		/*
		 *-------------
		 * Phy "Setup"
		 *-------------
		 */

		/* sanity check */
		ASSERT(!porig->is_orig);
		porig->is_orig = TRUE;

		/* Disable the Tx and Rx re-samplers (if enabled)
		 *   - relevant only if spur avoidance is ON;
		 *   - see also comment outsourced to the end of this proc
		 */
		porig->BBConfig = read_phy_reg(pi, HTPHY_BBConfig);
		MOD_PHYREG(pi, HTPHY_BBConfig, resample_clk160, 0);

		/* Save tx gain state */
		FOREACH_CORE(pi, core) {
			wlc_phy_get_tx_bbmult(pi, &(porig->bbmult[core]), core);
		}
		wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_RFSEQ, pi->pubpi.phy_corenum, 0x110, 16,
		                         porig->rfseq_txgain);

		/* AFE Ctrl */
		porig->Afectrl[rx_core] = read_phy_reg(pi, HTPHY_Afectrl(rx_core));
		porig->AfectrlOverride[rx_core] = read_phy_reg(pi, HTPHY_AfectrlOverride(rx_core));
		MOD_PHYREGC(pi, HTPHY_Afectrl, rx_core, adc_pd, 0);
		MOD_PHYREGC(pi, HTPHY_AfectrlOverride, rx_core, adc_pd, 1);

		porig->RfseqCoreActv2059 = read_phy_reg(pi, HTPHY_RfseqCoreActv2059);
		MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, DisRx, 0);
		MOD_PHYREG(pi, HTPHY_RfseqCoreActv2059, EnTx, (1 << rx_core));

		porig->RfctrlIntc[rx_core] = read_phy_reg(pi, HTPHY_RfctrlIntc(rx_core));
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, ext_2g_papu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, ext_5g_papu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, override_ext_pa, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, tr_sw_tx_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, tr_sw_rx_pu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlIntc, rx_core, override_tr_sw, 1);

		/* RfCtrl
		 *   - turn off Internal PA 
		 *   - turn off LNA1 to protect against interference and reduce thermal noise
		 *   - force LPF to Rx Chain
		 *   - force LPF bw
		 *   - NOTE: this also saves off state of possible Tx/Rx gain override states
		 */

		/* save state of Rfctrl override */
		porig->RfctrlCorePU[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlCorePU(rx_core));
		porig->RfctrlOverride[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlOverride(rx_core));
		porig->RfctrlCoreLpfCT[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlCoreLpfCT(rx_core));
		porig->RfctrlOverrideLpfCT[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlOverrideLpfCT(rx_core));
		porig->RfctrlCoreLpfPU[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlCoreLpfPU(rx_core));
		porig->RfctrlOverrideLpfPU[rx_core] =
			read_phy_reg(pi, HTPHY_RfctrlOverrideLpfPU(rx_core));

		MOD_PHYREGC(pi, HTPHY_RfctrlCorePU, rx_core, intpa_pu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlCorePU, rx_core, lna1_pu, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, intpa_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, lna1_pu, 1);

		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfCT, rx_core, lpf_byp_rx, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfCT, rx_core, lpf_byp_tx, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfCT, rx_core, lpf_sel_txrx, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfCT, rx_core, lpf_bw_ctl,
		            (CHSPEC_IS40(pi->radio_chanspec) ? 2 : 0));
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfCT, rx_core, lpf_byp_rx, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfCT, rx_core, lpf_byp_tx, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfCT, rx_core, lpf_sel_txrx, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfCT, rx_core, lpf_bw_ctl, 1);

		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfPU, rx_core, dc_loop_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlCoreLpfPU, rx_core, lpf_rx_buf_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfPU, rx_core, dc_loop_pu, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverrideLpfPU, rx_core, lpf_rx_buf_pu, 1);

		/* temporarily turn off PAPD in case it is enabled */
		porig->PapdEnable[rx_core] = read_phy_reg(pi, HTPHY_PapdEnable(rx_core));
		MOD_PHYREGC(pi, HTPHY_PapdEnable, rx_core, papd_compEnb0, 0);

	} else {
		/*
		 *---------------
		 * Phy "Cleanup"
		 *---------------
		 */

		/* sanity check */
		ASSERT(porig->is_orig);
		porig->is_orig = FALSE;

		/* Re-enable re-sampler (in case spur avoidance is on) */
		write_phy_reg(pi, HTPHY_BBConfig, porig->BBConfig);

		wlc_phy_resetcca_htphy(pi);

		/* restore regs */
		FOREACH_CORE(pi, core) {
			wlc_phy_set_tx_bbmult(pi, &(porig->bbmult[core]), core);
		}
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, pi->pubpi.phy_corenum, 0x110, 16,
		                         porig->rfseq_txgain);
		write_phy_reg(pi, HTPHY_Afectrl(rx_core), porig->Afectrl[rx_core]);
		write_phy_reg(pi, HTPHY_AfectrlOverride(rx_core), porig->AfectrlOverride[rx_core]);
		write_phy_reg(pi, HTPHY_RfseqCoreActv2059, porig->RfseqCoreActv2059);
		write_phy_reg(pi, HTPHY_RfctrlIntc(rx_core), porig->RfctrlIntc[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlCorePU(rx_core), porig->RfctrlCorePU[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlOverride(rx_core), porig->RfctrlOverride[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlCoreLpfCT(rx_core), porig->RfctrlCoreLpfCT[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlOverrideLpfCT(rx_core),
			porig->RfctrlOverrideLpfCT[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlCoreLpfPU(rx_core), porig->RfctrlCoreLpfPU[rx_core]);
		write_phy_reg(pi, HTPHY_RfctrlOverrideLpfPU(rx_core),
			porig->RfctrlOverrideLpfPU[rx_core]);
		write_phy_reg(pi, HTPHY_PapdEnable(rx_core), porig->PapdEnable[rx_core]);

		/* final tasks */
		/* return to rx operation */
		wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RESET2RX);
	}
}

#define HTPHY_RXCAL_TONEAMP 181
#define HTPHY_RXCAL_TONEFREQ_40MHz 4000
#define HTPHY_RXCAL_TONEFREQ_20MHz 2000
#define HTPHY_RXCAL_NUMGAINS 6

typedef struct _htphy_rxcal_txrxgain {
	uint16 lpf_biq1;
	uint16 lpf_biq0;
	uint16 lna2;
	int8 txpwrindex;
} htphy_rxcal_txrxgain_t;

enum {
	HTPHY_RXCAL_GAIN_INIT = 0,
	HTPHY_RXCAL_GAIN_UP,
	HTPHY_RXCAL_GAIN_DOWN
};

/* see also: proc htphy_rx_iq_cal_txrxgain_setup { core } */
static void
wlc_phy_rxcal_gainctrl_htphy(phy_info_t *pi, uint8 rx_core)
{
	/*
	 * joint tx-rx gain control for Rx IQ calibration
	 */

	/* gain candidates tables,
	 * columns are: B1 B0 L2 Tx-Pwr-Idx
	 * rows are monotonically increasing gain
	 */
	htphy_rxcal_txrxgain_t gaintbl_5G[HTPHY_RXCAL_NUMGAINS] =
		{
		{0, 0, 0, 100},
		{0, 0, 0,  50},
		{0, 0, 0,  -1},
		{0, 0, 3,  -1},
		{0, 3, 3,  -1},
		{0, 5, 3,  -1}
		};
	htphy_rxcal_txrxgain_t gaintbl_2G[HTPHY_RXCAL_NUMGAINS] =
	        {
		{0, 0, 0,  10},
		{0, 0, 1,  10},
		{0, 1, 2,  10},
		{0, 1, 3,  10},
		{0, 4, 3,  10},
		{0, 6, 3,  10}
		};
	uint32 gain_bits_tbl_ids[] =
	        {HTPHY_TBL_ID_GAINBITS1, HTPHY_TBL_ID_GAINBITS2, HTPHY_TBL_ID_GAINBITS3};
	uint16 num_samps = 1024;
	phy_iq_est_t est[PHY_CORE_MAX];
	/* threshold for "too high power"(313 mVpk, where clip = 400mVpk in 4322) */
	uint32 i_pwr, q_pwr, curr_pwr, optim_pwr = 0, prev_pwr = 0, thresh_pwr = 10000;
	/* desired_log2_pwr for gain fine adjustment in the end */
	int16 desired_log2_pwr = 13, actual_log2_pwr, delta_pwr;
	bool gainctrl_done = FALSE;
	/* max A-band Tx gain code (does not contain TxGm bits - injected below) */
	uint16 mix_tia_gain_idx, mix_tia_gain = 0;
	int8 optim_gaintbl_index = 0, prev_gaintbl_index = 0;
	int8 curr_gaintbl_index = 3;
	uint8 gainctrl_dirn = HTPHY_RXCAL_GAIN_INIT;
	htphy_rxcal_txrxgain_t *gaintbl;
	uint16 lpf_biq1_gain, lpf_biq0_gain, lna2_gain;
	int16 fine_gain_idx;
	int8 txpwrindex;
	uint16 txgain_max_a = 0x0ff8;
	txgain_setting_t txgain_setting_tmp;
	uint16 txgain_tmp, txgain_txgm_val, txgain_code;
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));


	/* retrieve Rx Mixer/TIA gain from InitGain and via GainBits table */
	mix_tia_gain_idx = READ_PHYREG(pi, HTPHY_Core0InitGainCodeA2059, initmixergainIndex);
	wlc_phy_table_read_htphy(pi, gain_bits_tbl_ids[rx_core], 1, (0x20 + mix_tia_gain_idx), 16,
	                         &mix_tia_gain);

	/* Retrieve Tx Mixer/Gm gain (so can inject below since this needs to be
	 * invariant) -- (extract from Tx gain table)
	 */
	wlc_phy_get_txgain_settings_by_index(pi, &txgain_setting_tmp, 50);
	txgain_tmp = txgain_setting_tmp.rad_gain;
	/* this is the bit-shifted TxGm gain code */
	txgain_txgm_val = txgain_setting_tmp.rad_gain & 0x7000;

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		gaintbl = gaintbl_5G;
	} else {
		gaintbl = gaintbl_2G;
	}

	/*
	 * COARSE ADJUSTMENT: Gain Search Loop
	 */
	do {
		/* set coarse gains */

		/* biq1 should be zero for REV7+ (see above) since it's used for fine AGC */
		/* lna1 is powered down, so code shouldn't matter but implicitly set to 0. */
		lpf_biq1_gain   = gaintbl[curr_gaintbl_index].lpf_biq1;
		lpf_biq0_gain   = gaintbl[curr_gaintbl_index].lpf_biq0;
		lna2_gain       = gaintbl[curr_gaintbl_index].lna2;
		txpwrindex      = gaintbl[curr_gaintbl_index].txpwrindex;

		/* rx */
		write_phy_reg(pi, HTPHY_RfctrlRXGAIN(rx_core),
			(mix_tia_gain << 4) | (lna2_gain << 2));
		write_phy_reg(pi, HTPHY_Rfctrl_lpf_gain(rx_core),
			(lpf_biq1_gain << 4) | lpf_biq0_gain);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, rxgain, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, lpf_gain_biq0, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, lpf_gain_biq1, 1);

		/* tx */
		FOREACH_CORE(pi, core) {
			if (txpwrindex == -1) {
				/* inject default TxGm value from above */
				txgain_code = txgain_max_a | txgain_txgm_val;
				wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ,
					1, 0x110 + core, 16, &txgain_code);
			} else {
				wlc_phy_txpwr_by_index_htphy(pi, (1 << core), txpwrindex);
			}
		}

		/* turn on testtone (this will override bbmult, but that's ok) */
		wlc_phy_tx_tone_htphy(pi, (CHSPEC_IS40(pi->radio_chanspec)) ?
		                      HTPHY_RXCAL_TONEFREQ_40MHz : HTPHY_RXCAL_TONEFREQ_20MHz,
		                      HTPHY_RXCAL_TONEAMP, 0, 0, FALSE);

		wlc_phy_rx_iq_est_loopback_htphy(pi, est, num_samps, 32, 0);
		i_pwr = (est[rx_core].i_pwr + num_samps / 2) / num_samps;
		q_pwr = (est[rx_core].q_pwr + num_samps / 2) / num_samps;
		curr_pwr = i_pwr + q_pwr;
		PHY_NONE(("core %u (gain idx %d): i_pwr = %u, q_pwr = %u, curr_pwr = %d\n",
		         rx_core, curr_gaintbl_index, i_pwr, q_pwr, curr_pwr));


		switch (gainctrl_dirn) {
		case HTPHY_RXCAL_GAIN_INIT:
			if (curr_pwr > thresh_pwr) {
				gainctrl_dirn = HTPHY_RXCAL_GAIN_DOWN;
				prev_gaintbl_index = curr_gaintbl_index;
				curr_gaintbl_index--;
			} else {
				gainctrl_dirn = HTPHY_RXCAL_GAIN_UP;
				prev_gaintbl_index = curr_gaintbl_index;
				curr_gaintbl_index++;
			}
			break;

		case HTPHY_RXCAL_GAIN_UP:
			if (curr_pwr > thresh_pwr) {
				gainctrl_done = TRUE;
				optim_pwr = prev_pwr;
				optim_gaintbl_index = prev_gaintbl_index;
			} else {
				prev_gaintbl_index = curr_gaintbl_index;
				curr_gaintbl_index++;
			}
			break;

		case HTPHY_RXCAL_GAIN_DOWN:
			if (curr_pwr > thresh_pwr) {
				prev_gaintbl_index = curr_gaintbl_index;
				curr_gaintbl_index--;
			} else {
				gainctrl_done = TRUE;
				optim_pwr = curr_pwr;
				optim_gaintbl_index = curr_gaintbl_index;
			}
			break;

		default:
			PHY_ERROR(("Invalid gaintable direction id %d\n", gainctrl_dirn));
			ASSERT(0);
		}

		if ((curr_gaintbl_index < 0) ||
		    (curr_gaintbl_index >= HTPHY_RXCAL_NUMGAINS)) {
			gainctrl_done = TRUE;
			optim_pwr = curr_pwr;
			optim_gaintbl_index = prev_gaintbl_index;
		} else {
			prev_pwr = curr_pwr;
		}

		/* Turn off the tone */
		wlc_phy_stopplayback_htphy(pi);

	} while (!gainctrl_done);

	/* fetch back the chosen gain values resulting from the coarse gain search */
	lpf_biq1_gain   = gaintbl[optim_gaintbl_index].lpf_biq1;
	lpf_biq0_gain   = gaintbl[optim_gaintbl_index].lpf_biq0;
	lna2_gain       = gaintbl[optim_gaintbl_index].lna2;
	txpwrindex      = gaintbl[optim_gaintbl_index].txpwrindex;

	/*
	 * FINE ADJUSTMENT: adjust Biq1 to get on target
	 */

	/* get measured power in log2 domain
	 * (log domain value is larger or equal to lin value)
	 * Compute power delta in log domain
	 */
	actual_log2_pwr = wlc_phy_nbits(optim_pwr);
	delta_pwr = desired_log2_pwr - actual_log2_pwr;

	fine_gain_idx = (int)lpf_biq1_gain + delta_pwr;
	/* Limit Total LPF To 30 dB */
	if (fine_gain_idx + (int)lpf_biq0_gain > 10) {
		lpf_biq1_gain = 10 - lpf_biq0_gain;
	} else {
		lpf_biq1_gain = (uint16) MAX(fine_gain_idx, 0);
	}

	/* set fine gains */

	/* rx */
	write_phy_reg(pi, HTPHY_RfctrlRXGAIN(rx_core), (mix_tia_gain << 4) | (lna2_gain << 2));
	write_phy_reg(pi, HTPHY_Rfctrl_lpf_gain(rx_core), (lpf_biq1_gain << 4) | lpf_biq0_gain);
	MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, rxgain, 1);
	MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, lpf_gain_biq0, 1);
	MOD_PHYREGC(pi, HTPHY_RfctrlOverride, rx_core, lpf_gain_biq1, 1);

	/* tx */
	FOREACH_CORE(pi, core) {
		if (txpwrindex == -1) {
			/* inject default TxGm value from above */
			txgain_code = txgain_max_a | txgain_txgm_val;
			wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, 0x110 + core, 16,
			                          &txgain_code);
		} else {
			wlc_phy_txpwr_by_index_htphy(pi, (1 << core), txpwrindex);
		}
	}

	PHY_NONE(("FINAL: gainIdx=%3d, lna1=OFF, lna2=%3d, mix_tia=%3d, "
	         "lpf0=%3d, lpf1=%3d, txpwridx=%3d\n",
	         optim_gaintbl_index, lna2_gain, mix_tia_gain,
	         lpf_biq0_gain, lpf_biq1_gain, txpwrindex));
}

/* see also: proc htphy_rx_iq_cal {} */
static int
wlc_phy_cal_rxiq_htphy(phy_info_t *pi, bool debug)
{
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Make sure we start in RX state and are "deaf" */
	wlc_phy_deaf_htphy(pi, TRUE);
	wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RESET2RX);

	/* Zero Out coefficients */
	FOREACH_CORE(pi, core) {
		wlc_phy_rx_iq_comp_htphy(pi, 2, NULL, core);
	}

	/* Master Loop */
	FOREACH_CORE(pi, core) {
		wlc_phy_rxcal_phy_config_htphy(pi, TRUE, core);
		wlc_phy_rxcal_radio_config_htphy(pi, TRUE, core);
		wlc_phy_rxcal_gainctrl_htphy(pi, core);
		wlc_phy_tx_tone_htphy(pi, (CHSPEC_IS40(pi->radio_chanspec)) ?
		                      HTPHY_RXCAL_TONEFREQ_40MHz : HTPHY_RXCAL_TONEFREQ_20MHz,
		                      HTPHY_RXCAL_TONEAMP, 0, 0, FALSE);
		wlc_phy_calc_rx_iq_comp_htphy(pi, 0x4000, (1 << core));
		wlc_phy_stopplayback_htphy(pi);
		wlc_phy_rxcal_radio_config_htphy(pi, FALSE, core);
		wlc_phy_rxcal_phy_config_htphy(pi, FALSE, core);
	}

	/* reactivate carrier search */
	wlc_phy_deaf_htphy(pi, FALSE);

	return BCME_OK;
}

/* configure tx filter for SM shaping in ExtPA case */
static void
wlc_phy_extpa_set_tx_digi_filts_htphy(phy_info_t *pi)
{
	int j, type = TXFILT_SHAPING_CCK;
	uint16 addr_offset = HTPHY_ccktxfilt20CoeffStg0A1;

	for (j = 0; j < HTPHY_NUM_DIG_FILT_COEFFS; j++) {
		write_phy_reg(pi, addr_offset+j,
		              HTPHY_txdigi_filtcoeffs[type][j]);
	}
}



/* set txgain in case txpwrctrl is disabled (fixed power) */
static void
wlc_phy_txpwr_fixpower_htphy(phy_info_t *pi)
{
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	FOREACH_CORE(pi, core) {
		wlc_phy_txpwr_by_index_htphy(pi, (1 << core), pi->txpwrindex[core]);
	}
}

void
BCMNMIATTACHFN(wlc_phy_txpwr_apply_htphy)(phy_info_t *pi)
{
}

static bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_htphy)(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint b;

	/* read in antenna-related config */
	pi->antswitch = (uint8) PHY_GETINTVAR(pi, "antswitch");
	pi->aa2g = (uint8) PHY_GETINTVAR(pi, "aa2g");
	pi->aa5g = (uint8) PHY_GETINTVAR(pi, "aa5g");

	/* read in FEM stuff */
	pi->srom_fem2g.tssipos = (uint8)PHY_GETINTVAR(pi, "tssipos2g");
	pi->srom_fem2g.extpagain = (uint8)PHY_GETINTVAR(pi, "extpagain2g");
	pi->srom_fem2g.pdetrange = (uint8)PHY_GETINTVAR(pi, "pdetrange2g");
	pi->srom_fem2g.triso = (uint8)PHY_GETINTVAR(pi, "triso2g");
	pi->srom_fem2g.antswctrllut = (uint8)PHY_GETINTVAR(pi, "antswctl2g");

	pi->srom_fem5g.tssipos = (uint8)PHY_GETINTVAR(pi, "tssipos5g");
	pi->srom_fem5g.extpagain = (uint8)PHY_GETINTVAR(pi, "extpagain5g");
	pi->srom_fem5g.pdetrange = (uint8)PHY_GETINTVAR(pi, "pdetrange5g");
	pi->srom_fem5g.triso = (uint8)PHY_GETINTVAR(pi, "triso5g");
	pi->srom_fem5g.antswctrllut = (uint8)PHY_GETINTVAR(pi, "antswctl5g");

	/* srom_fem2/5g.extpagain changed */


	/* read pwrdet params for each band/subband */
	for (b = 0; b < (CH_2G_GROUP + CH_5G_GROUP); b++) {
		switch (b) {
		case WL_CHAN_FREQ_RANGE_2G: /* 0 */
			/* 2G band */
			pi_ht->max_pwr[0][b] = (int8)PHY_GETINTVAR(pi, "maxp2ga0");
			pi_ht->max_pwr[1][b] = (int8)PHY_GETINTVAR(pi, "maxp2ga1");
			pi_ht->max_pwr[2][b] = (int8)PHY_GETINTVAR(pi, "maxp2ga2");
			pi_ht->pwrdet_a1[0][b] = (int16)PHY_GETINTVAR(pi, "pa2gw0a0");
			pi_ht->pwrdet_a1[1][b] = (int16)PHY_GETINTVAR(pi, "pa2gw0a1");
			pi_ht->pwrdet_a1[2][b] = (int16)PHY_GETINTVAR(pi, "pa2gw0a2");
			pi_ht->pwrdet_b0[0][b] = (int16)PHY_GETINTVAR(pi, "pa2gw1a0");
			pi_ht->pwrdet_b0[1][b] = (int16)PHY_GETINTVAR(pi, "pa2gw1a1");
			pi_ht->pwrdet_b0[2][b] = (int16)PHY_GETINTVAR(pi, "pa2gw1a2");
			pi_ht->pwrdet_b1[0][b] = (int16)PHY_GETINTVAR(pi, "pa2gw2a0");
			pi_ht->pwrdet_b1[1][b] = (int16)PHY_GETINTVAR(pi, "pa2gw2a1");
			pi_ht->pwrdet_b1[2][b] = (int16)PHY_GETINTVAR(pi, "pa2gw2a2");
			break;
		case WL_CHAN_FREQ_RANGE_5GL: /* 1 */
			/* 5G band, low */
			pi_ht->max_pwr[0][b] = (int8)PHY_GETINTVAR(pi, "maxp5gla0");
			pi_ht->max_pwr[1][b] = (int8)PHY_GETINTVAR(pi, "maxp5gla1");
			pi_ht->max_pwr[2][b] = (int8)PHY_GETINTVAR(pi, "maxp5gla2");
			pi_ht->pwrdet_a1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5glw0a0");
			pi_ht->pwrdet_a1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5glw0a1");
			pi_ht->pwrdet_a1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5glw0a2");
			pi_ht->pwrdet_b0[0][b] = (int16)PHY_GETINTVAR(pi, "pa5glw1a0");
			pi_ht->pwrdet_b0[1][b] = (int16)PHY_GETINTVAR(pi, "pa5glw1a1");
			pi_ht->pwrdet_b0[2][b] = (int16)PHY_GETINTVAR(pi, "pa5glw1a2");
			pi_ht->pwrdet_b1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5glw2a0");
			pi_ht->pwrdet_b1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5glw2a1");
			pi_ht->pwrdet_b1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5glw2a2");
			break;
		case WL_CHAN_FREQ_RANGE_5GM: /* 2 */
			/* 5G band, mid */
			pi_ht->max_pwr[0][b] = (int8)PHY_GETINTVAR(pi, "maxp5ga0");
			pi_ht->max_pwr[1][b] = (int8)PHY_GETINTVAR(pi, "maxp5ga1");
			pi_ht->max_pwr[2][b] = (int8)PHY_GETINTVAR(pi, "maxp5ga2");
			pi_ht->pwrdet_a1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5gw0a0");
			pi_ht->pwrdet_a1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5gw0a1");
			pi_ht->pwrdet_a1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5gw0a2");
			pi_ht->pwrdet_b0[0][b] = (int16)PHY_GETINTVAR(pi, "pa5gw1a0");
			pi_ht->pwrdet_b0[1][b] = (int16)PHY_GETINTVAR(pi, "pa5gw1a1");
			pi_ht->pwrdet_b0[2][b] = (int16)PHY_GETINTVAR(pi, "pa5gw1a2");
			pi_ht->pwrdet_b1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5gw2a0");
			pi_ht->pwrdet_b1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5gw2a1");
			pi_ht->pwrdet_b1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5gw2a2");
			break;
		case WL_CHAN_FREQ_RANGE_5GH: /* 3 */
			/* 5G band, high */
			pi_ht->max_pwr[0][b] = (int8)PHY_GETINTVAR(pi, "maxp5gha0");
			pi_ht->max_pwr[1][b] = (int8)PHY_GETINTVAR(pi, "maxp5gha1");
			pi_ht->max_pwr[2][b] = (int8)PHY_GETINTVAR(pi, "maxp5gha2");
			pi_ht->pwrdet_a1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw0a0");
			pi_ht->pwrdet_a1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw0a1");
			pi_ht->pwrdet_a1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw0a2");
			pi_ht->pwrdet_b0[0][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw1a0");
			pi_ht->pwrdet_b0[1][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw1a1");
			pi_ht->pwrdet_b0[2][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw1a2");
			pi_ht->pwrdet_b1[0][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw2a0");
			pi_ht->pwrdet_b1[1][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw2a1");
			pi_ht->pwrdet_b1[2][b] = (int16)PHY_GETINTVAR(pi, "pa5ghw2a2");
			break;
		}
	}


	/* Finished reading from SROM, calculate and apply powers */
	wlc_phy_txpwr_apply_htphy(pi);

	return TRUE;
}

void
wlc_phy_txpower_recalc_target_htphy(phy_info_t *pi)
{
	/* recalc targets -- turns hwpwrctrl off */
	wlc_phy_txpwrctrl_pwr_setup_htphy(pi);

	/* restore power control */
	wlc_phy_txpwrctrl_enable_htphy(pi, pi->txpwrctrl);

}

/* measure idle TSSI by sending 0-magnitude tone */
static void
wlc_phy_txpwrctrl_idle_tssi_meas_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 txgain_save[PHY_CORE_MAX];
	uint16 lpf_gain_save[PHY_CORE_MAX];
	uint16 override_save[PHY_CORE_MAX];
	int32 auxadc_buf[2*PHY_CORE_MAX];
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* BMAC_NOTE: Why is the idle tssi skipped? Is it avoiding doing a cal
	 * on a temporary channel (this is why I would think it would check
	 * SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi)), or is it preventing
	 * any energy emission (this is why I would think it would check
	 * PHY_MUTED()), or both?
	 */
	if (SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi) || PHY_MUTED(pi))
		/* skip idle tssi cal */
		return;


	/* Set TX gain to 0, so that LO leakage does not affect IDLE TSSI */
	FOREACH_CORE(pi, core) {
		txgain_save[core] = read_phy_reg(pi, HTPHY_RfctrlTXGAIN(core));
		lpf_gain_save[core] = read_phy_reg(pi, HTPHY_Rfctrl_lpf_gain(core));
		override_save[core] = read_phy_reg(pi, HTPHY_RfctrlOverride(core));

		write_phy_reg(pi, HTPHY_RfctrlTXGAIN(core), 0);
		MOD_PHYREGC(pi, HTPHY_Rfctrl_lpf_gain, core, lpf_gain_biq0, 0);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, core, txgain, 1);
		MOD_PHYREGC(pi, HTPHY_RfctrlOverride, core, lpf_gain_biq0, 1);
	}

	wlc_phy_tx_tone_htphy(pi, 4000, 0, 0, 0, FALSE);
	OSL_DELAY(20);
	wlc_phy_poll_auxadc_htphy(pi, HTPHY_AUXADC_SEL_TSSI, auxadc_buf, 1);
	wlc_phy_stopplayback_htphy(pi);

	/* Remove TX gain override */
	FOREACH_CORE(pi, core) {
		write_phy_reg(pi, HTPHY_RfctrlOverride(core), override_save[core]);
		write_phy_reg(pi, HTPHY_RfctrlTXGAIN(core), txgain_save[core]);
		write_phy_reg(pi, HTPHY_Rfctrl_lpf_gain(core), lpf_gain_save[core]);
	}

	/* TSSI value is available only on the I component of AUX ADC out */
	FOREACH_CORE(pi, core) {
		pi_ht->idle_tssi[core] = (int8)auxadc_buf[2*core];
		PHY_TXPWR(("wl%d: %s: idle_tssi core%d: %d\n",
		           pi->sh->unit, __FUNCTION__, core, pi_ht->idle_tssi[core]));
	}
}

static void
wlc_phy_txpwrctrl_pwr_setup_htphy(phy_info_t *pi)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	int8   target_pwr_qtrdbm[PHY_CORE_MAX];
	int16  a1[PHY_CORE_MAX], b0[PHY_CORE_MAX], b1[PHY_CORE_MAX];
	uint8  chan_freq_range;
	uint8  iidx = 25;
	uint8  core;
	int32  num, den, pwr_est;
	uint32 tbl_len, tbl_offset;
	uint32 regval[64], idx;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* enable TSSI */
	MOD_PHYREG(pi, HTPHY_TSSIMode, tssiEn, 1);
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlCmd, txPwrCtrl_en, 0);

	/* Get pwrdet params from SROM for current subband */
	chan_freq_range = wlc_phy_get_chan_freq_range_htphy(pi, 0);
	FOREACH_CORE(pi, core) {
		switch (chan_freq_range) {
		case WL_CHAN_FREQ_RANGE_2G:
		case WL_CHAN_FREQ_RANGE_5GL:
		case WL_CHAN_FREQ_RANGE_5GM:
		case WL_CHAN_FREQ_RANGE_5GH:
			a1[core] =  pi_ht->pwrdet_a1[core][chan_freq_range];
			b0[core] =  pi_ht->pwrdet_b0[core][chan_freq_range];
			b1[core] =  pi_ht->pwrdet_b1[core][chan_freq_range];
			PHY_TXPWR(("wl%d: %s: pwrdet core%d: a1=%d b0=%d b1=%d\n",
			           pi->sh->unit, __FUNCTION__, core,
			           a1[core], b0[core], b1[core]));
			break;
		}
	}

	/* target power */
	FOREACH_CORE(pi, core) {
		target_pwr_qtrdbm[core] = (int8) pi->tx_power_max;
	}

	/* determine pos/neg TSSI slope */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi, tssiPosSlope, pi->srom_fem2g.tssipos);
	} else {
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi, tssiPosSlope, pi->srom_fem5g.tssipos);
	}

	/* set power index initial condition */
	FOREACH_CORE(pi, core) {
		wlc_phy_txpwrctrl_set_cur_index_htphy(pi, iidx, core);
	}

	/* expect aux-adc putting out 2's comp format */
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1);

	/* set idle TSSI in 2s complement format (max is 0x1f) */
	FOREACH_CORE(pi, core) {
		wlc_phy_txpwrctrl_set_idle_tssi_htphy(pi, pi_ht->idle_tssi[core], core);
	}

	/* sample TSSI at 6us = 120 samps into packet (120? 240?) */
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlNnum, Ntssi_delay, 240);

	/* average over 8 = 2^3 packets */
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlNnum, Npt_intg_log2, 3);

	/* decouple IQ comp and LOFT comp from Power Control */
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlCmd, use_txPwrCtrlCoefsIQ, 0);
	MOD_PHYREG(pi, HTPHY_TxPwrCtrlCmd, use_txPwrCtrlCoefsLO, 0);

	/* set target powers in 6.2 format (in dBs) */
	FOREACH_CORE(pi, core) {
		wlc_phy_txpwrctrl_set_target_htphy(pi, target_pwr_qtrdbm[core], core);
	}

	/* load estimated power tables (maps TSSI to power in dBm)
	 *    entries in tx power table 0000xxxxxx
	 */
	tbl_len = 64;
	tbl_offset = 0;
	FOREACH_CORE(pi, core) {
		for (idx = 0; idx < tbl_len; idx++) {
			num = 8 * (16 * b0[core] + b1[core] * idx);
			den = 32768 + a1[core] * idx;
			pwr_est = MAX(((4 * num + den/2)/den), -8);
			pwr_est = MIN(pwr_est, 0x7F);
			regval[idx] = (uint32)pwr_est;
		}
		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_TXPWRCTL(core), tbl_len,
		                          tbl_offset, 32, regval);
	}
}

/* report estimated power and adjusted estimated power in quarter dBms */
void
wlc_phy_txpwr_est_pwr_htphy(phy_info_t *pi, uint8 *Pout, uint8 *Pout_act)
{
	uint8 core;
	uint16 val;

	FOREACH_CORE(pi, core) {
		/* Read the Actual Estimated Powers without adjustment */
		val = read_phy_reg(pi, HTPHY_EstPower(core));

		Pout[core] = 0;
		if ((val & HTPHY_EstPower_estPowerValid_MASK)) {
			Pout[core] = (uint8) ((val & HTPHY_EstPower_estPower_MASK)
			                      >> HTPHY_EstPower_estPower_SHIFT);
		}
		/* Read the Adjusted Estimated Powers */
		val = read_phy_reg(pi, HTPHY_TxPwrCtrlStatus(core));

		Pout_act[core] = 0;
		if ((val & HTPHY_TxPwrCtrlStatus_estPwrAdjValid_MASK)) {
			Pout_act[core] = (uint8) ((val & HTPHY_TxPwrCtrlStatus_estPwr_adj_MASK)
			                          >> HTPHY_TxPwrCtrlStatus_estPwr_adj_SHIFT);
		}
	}
}

static void
wlc_phy_get_txgain_settings_by_index(phy_info_t *pi, txgain_setting_t *txgain_settings,
                                     int8 txpwrindex)
{
	uint32 txgain;

	wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_TXPWRCTL(0), 1,
	                         (192 + txpwrindex), 32, &txgain);

	txgain_settings->rad_gain = (txgain >> 16) & ((1<<(32-16+1))-1);
	txgain_settings->dac_gain = (txgain >>  8) & ((1<<(13- 8+1))-1);
	txgain_settings->bbmult   = (txgain >>  0) & ((1<<(7 - 0+1))-1);
}

static bool
wlc_phy_txpwrctrl_ison_htphy(phy_info_t *pi)
{
	uint16 mask = (HTPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
	               HTPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
	               HTPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK);

	return ((read_phy_reg((pi), HTPHY_TxPwrCtrlCmd) & mask) == mask);
}

static void
wlc_phy_txpwrctrl_set_idle_tssi_htphy(phy_info_t *pi, int8 idle_tssi, uint8 core)
{
	/* set idle TSSI in 2s complement format (max is 0x1f) */
	switch (core) {
	case 0:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi, idleTssi0, idle_tssi);
		break;
	case 1:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi, idleTssi1, idle_tssi);
		break;
	case 2:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlIdleTssi1, idleTssi2, idle_tssi);
		break;
	}
}

static void
wlc_phy_txpwrctrl_set_target_htphy(phy_info_t *pi, uint8 pwr_qtrdbm, uint8 core)
{
	/* set target powers in 6.2 format (in dBs) */
	switch (core) {
	case 0:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlTargetPwr, targetPwr0, pwr_qtrdbm);
		break;
	case 1:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlTargetPwr, targetPwr1, pwr_qtrdbm);
		break;
	case 2:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlTargetPwr1, targetPwr2, pwr_qtrdbm);
		break;
	}
}

static uint8
wlc_phy_txpwrctrl_get_cur_index_htphy(phy_info_t *pi, uint8 core)
{
	uint16 tmp;

	tmp = READ_PHYREGC(pi, HTPHY_TxPwrCtrlStatus, core, baseIndex);

	return (uint8)tmp;
}

static void
wlc_phy_txpwrctrl_set_cur_index_htphy(phy_info_t *pi, uint8 idx, uint8 core)
{
	switch (core) {
	case 0:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlCmd, pwrIndex_init, idx);
		break;
	case 1:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlInit, pwrIndex_init1, idx);
		break;
	case 2:
		MOD_PHYREG(pi, HTPHY_TxPwrCtrlInit1, pwrIndex_init2, idx);
		break;
	}
}

uint32
wlc_phy_txpwr_idx_get_htphy(phy_info_t *pi)
{
	uint8 core;
	uint32 pwr_idx[] = {0, 0, 0, 0};
	uint32 tmp = 0;

	if (wlc_phy_txpwrctrl_ison_htphy(pi)) {
		FOREACH_CORE(pi, core) {
			pwr_idx[core] = wlc_phy_txpwrctrl_get_cur_index_htphy(pi, core);
		}
	} else {
		FOREACH_CORE(pi, core) {
			pwr_idx[core] = (pi->txpwrindex[core] & 0xff);
		}
	}
	tmp = (pwr_idx[3] << 24) | (pwr_idx[2] << 16) | (pwr_idx[1] << 8) | pwr_idx[0];

	return tmp;
}

void
wlc_phy_txpwrctrl_enable_htphy(phy_info_t *pi, uint8 ctrl_type)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 mask;
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* check for recognized commands */
	switch (ctrl_type) {
	case PHY_TPC_HW_OFF:
	case PHY_TPC_HW_ON:
		pi->txpwrctrl = ctrl_type;
		break;
	default:
		PHY_ERROR(("wl%d: %s: Unrecognized ctrl_type: %d\n",
			pi->sh->unit, __FUNCTION__, ctrl_type));
		break;
	}

	if (ctrl_type == PHY_TPC_HW_OFF) {
		/* save previous txpwr index if txpwrctl was enabled */
		if (wlc_phy_txpwrctrl_ison_htphy(pi)) {
			FOREACH_CORE(pi, core) {
				pi_ht->txpwrindex_hw_save[core] =
					wlc_phy_txpwrctrl_get_cur_index_htphy(pi, (uint8)core);
			}
		}

		/* Disable hw txpwrctrl */
		mask = HTPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
		       HTPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
		       HTPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK;
		mod_phy_reg(pi, HTPHY_TxPwrCtrlCmd, mask, 0);

	} else {
		/* Enable hw txpwrctrl */
		mask = HTPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
		       HTPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
		       HTPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK;
		mod_phy_reg(pi, HTPHY_TxPwrCtrlCmd, mask, mask);

		FOREACH_CORE(pi, core) {
			/* lower the starting txpwr index for A-Band */
			if (CHSPEC_IS5G(pi->radio_chanspec)) {
				wlc_phy_txpwrctrl_set_cur_index_htphy(pi, 0x32, core);
			}

			/* restore the old txpwr index if they are valid */
			if (pi_ht->txpwrindex_hw_save[core] != 128) {
				wlc_phy_txpwrctrl_set_cur_index_htphy(pi,
					pi_ht->txpwrindex_hw_save[core], core);
			}
		}

	}
}

void
wlc_phy_txpwr_by_index_htphy(phy_info_t *pi, uint8 core_mask, int8 txpwrindex)
{
	uint16 core;
	txgain_setting_t txgain_settings;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Set tx power based on an input "index"
	 * (Emulate what HW power control would use for a given table index)
	 */

	FOREACH_ACTV_CORE(pi, core_mask, core) {

		/* Check txprindex >= 0 */
		if (txpwrindex < 0)
			ASSERT(0); /* negative index not supported */

		/* Read tx gain table */
		wlc_phy_get_txgain_settings_by_index(pi, &txgain_settings, txpwrindex);

		/* Override gains: DAC, Radio and BBmult */
		switch (core) {
		case 0:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain0,
			           txgain_settings.dac_gain);
			break;
		case 1:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain1,
			           txgain_settings.dac_gain);
			break;
		case 2:
			MOD_PHYREG(pi, HTPHY_AfeseqInitDACgain, InitDACgain2,
			           txgain_settings.dac_gain);
			break;
		}

		wlc_phy_table_write_htphy(pi, HTPHY_TBL_ID_RFSEQ, 1, (0x110 + core), 16,
		                          &txgain_settings.rad_gain);

		wlc_phy_set_tx_bbmult(pi, &txgain_settings.bbmult, core);

		PHY_TXPWR(("wl%d: %s: Fixed txpwrindex for core%d is %d\n",
		          pi->sh->unit, __FUNCTION__, core, txpwrindex));

		/* Update the per-core state of power index */
		pi->txpwrindex[core] = txpwrindex;
	}
}


void
wlc_phy_txpower_sromlimit_get_htphy(phy_info_t *pi, uint chan, uint8 *max_pwr, uint8 txp_rate_idx)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint8 core, chan_freq_range;

	uint8 tmp_max_pwr = 32 * 4;
	chan_freq_range = wlc_phy_get_chan_freq_range_htphy(pi, chan);
	FOREACH_CORE(pi, core) {
		switch (chan_freq_range) {
		case WL_CHAN_FREQ_RANGE_2G:
		case WL_CHAN_FREQ_RANGE_5GL:
		case WL_CHAN_FREQ_RANGE_5GM:
		case WL_CHAN_FREQ_RANGE_5GH:
			tmp_max_pwr = MIN(tmp_max_pwr, pi_ht->max_pwr[core][chan_freq_range]);
			break;
		}
	}
	*max_pwr = tmp_max_pwr;
	return;
}

void
wlc_phy_stay_in_carriersearch_htphy(phy_info_t *pi, bool enable)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint16 clip_off[] = {0xffff, 0xffff, 0xffff, 0xffff};

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* MAC should be suspended before calling this function */
	ASSERT(0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

	if (enable) {
		if (pi_ht->deaf_count == 0) {
			pi_ht->classifier_state = wlc_phy_classifier_htphy(pi, 0, 0);
			wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_classifierSel_MASK, 4);
			wlc_phy_clip_det_htphy(pi, 0, pi_ht->clip_state);
			wlc_phy_clip_det_htphy(pi, 1, clip_off);
		}

		pi_ht->deaf_count++;

		wlc_phy_resetcca_htphy(pi);

	} else {
		ASSERT(pi_ht->deaf_count > 0);

		pi_ht->deaf_count--;

		if (pi_ht->deaf_count == 0) {
			wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_classifierSel_MASK,
			pi_ht->classifier_state);
			wlc_phy_clip_det_htphy(pi, 1, pi_ht->clip_state);
		}
	}
}

void
wlc_phy_deaf_htphy(phy_info_t *pi, bool mode)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	if (mode) {
		if (pi_ht->deaf_count == 0)
			wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);
		else
			PHY_ERROR(("%s: Deafness already set\n", __FUNCTION__));
	}
	else {
		if (pi_ht->deaf_count > 0)
			wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);
		else
			PHY_ERROR(("%s: Deafness already cleared\n", __FUNCTION__));
	}
	wlapi_enable_mac(pi->sh->physhim);
}

#ifdef SAMPLE_COLLECT

/* channel to frequency conversion */
static int
wlc_phy_chan2fc(uint channel)
{
	/* go from channel number (such as 6) to carrier freq (such as 2442) */
	if (channel >= 184 && channel <= 228)
		return (channel*5 + 4000);
	else if (channel >= 32 && channel <= 180)
		return (channel*5 + 5000);
	else if (channel >= 1 && channel <= 13)
		return (channel*5 + 2407);
	else if (channel == 14)
		return (2484);
	else
		return -1;
}

#define FILE_HDR_LEN 20 /* words */
/* (FIFO memory is 176kB = 45056 x 32bit) */

int
wlc_phy_sample_collect_htphy(phy_info_t *pi, wl_samplecollect_args_t *collect, uint32 *buf)
{
	uint32 machwcap;
	uint32 tx_fifo_length;
	uint32 phy_ctl, timer, cnt;
	uint16 val;
	uint8 words_per_us_BW20 = 80;
	uint8 words_per_us;
	uint16 fc = (uint16)wlc_phy_chan2fc(CHSPEC_CHANNEL(pi->radio_chanspec));
	uint8 gpio_collection = 0;
	uint32 *ptr;
	phy_info_htphy_t *pi_ht;
	wl_sampledata_t *sample_data;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	ASSERT(pi);
	pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	if (!pi_ht)
		return BCME_ERROR;

	pi_ht = (phy_info_htphy_t *)pi->pi_ptr;

	/* initial return info pointers */
	sample_data = (wl_sampledata_t *)buf;
	ptr = (uint32 *)&sample_data[1];
	bzero((uint8 *)sample_data, sizeof(wl_sampledata_t));
	sample_data->version = htol16(WL_SAMPLEDATA_T_VERSION);
	sample_data->size = htol16(sizeof(wl_sampledata_t));

	/* get TX FIFO length in words */
	machwcap = R_REG(pi->sh->osh, &pi->regs->machwcap);
	tx_fifo_length = (((machwcap >> 3) & 0x1ff) << 7);

	/* compute applicable words_per_us */
	words_per_us = (CHSPEC_IS40(pi->radio_chanspec) ?
		(words_per_us_BW20 << 1) : words_per_us_BW20);

	/* duration(s): length sanity check and mapping from "max" to usec values */
	if (((collect->pre_dur + collect->post_dur) * words_per_us) > tx_fifo_length) {
		PHY_ERROR(("wl%d: %s Error: Bad Duration Option\n", pi->sh->unit, __FUNCTION__));
		return BCME_RANGE;
	}

	/* set phyreg into desired collect mode and enable */
	if (collect->mode == 0) { /* ADC */
		write_phy_reg(pi, HTPHY_AdcDataCollect, 0);
		MOD_PHYREG(pi, HTPHY_AdcDataCollect, adc_sel, collect->mode);
		MOD_PHYREG(pi, HTPHY_AdcDataCollect, adcDataCollectEn, 1);
	} else if (collect->mode < 4) { /* ADC+RSSI */
		write_phy_reg(pi, HTPHY_AdcDataCollect, 0);
		MOD_PHYREG(pi, HTPHY_AdcDataCollect, adc_sel, collect->mode);
		MOD_PHYREG(pi, HTPHY_AdcDataCollect, adcDataCollectEn, 1);
	} else if (collect->mode == 4) { /* GPIO */
		write_phy_reg(pi, HTPHY_AdcDataCollect, 0);
		MOD_PHYREG(pi, HTPHY_AdcDataCollect, gpioSel, 1);
		MOD_PHYREG(pi, HTPHY_gpioSel, gpioSel, collect->gpio_sel);
	} else {
		PHY_ERROR(("wl%d: %s Error: Unsupported Mode\n", pi->sh->unit, __FUNCTION__));
		return BCME_ERROR;
	}
	MOD_PHYREG(pi, HTPHY_AdcDataCollect, downSample, collect->downsamp);

	/* be deaf if requested (e.g. for spur measurement) */
	if (collect->be_deaf) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);
	}

	/* set Tx-FIFO collect start pointer to 0 */
	pi_ht->pstart = 0;
	W_REG(pi->sh->osh, &pi->regs->smpl_clct_strptr, pi_ht->pstart);

	PHY_TRACE(("wl%d: %s Start capture, trigger = %d\n", pi->sh->unit, __FUNCTION__,
		collect->trigger));

	timer = collect->timeout;
	/* immediate trigger */
	if (collect->trigger == TRIGGER_NOW) {
		uint32 curptr;

		/* compute and set stop pointer */
		pi_ht->pstop = (collect->pre_dur + collect->post_dur) * words_per_us;
		if (pi_ht->pstop >= tx_fifo_length-1)
			pi_ht->pstop = tx_fifo_length-1;
		W_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr, pi_ht->pstop);

		/* set Stop bit and Start bit (start capture) */
		phy_ctl = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl | (1 << 4) | (1 << 5));

		/* wait until done */
		do {
			OSL_DELAY(10);
			curptr = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
			timer--;
		} while ((curptr != pi_ht->pstop) && (timer > 0));

		/* clear start/stop bits */
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl & 0xFFCF);

		/* set start/stop pointers for readout */
		pi_ht->pfirst = 0;
		pi_ht->plast = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);

	} else {
		uint32 mac_ctl, dur_1_8th_us;

		/* enable mac and run psm */
		mac_ctl = R_REG(pi->sh->osh, &pi->regs->maccontrol);
		W_REG(pi->sh->osh, &pi->regs->maccontrol, mac_ctl | MCTL_PSM_RUN | MCTL_EN_MAC);

		/* set stop pointer */
		pi_ht->pstop = tx_fifo_length-1;
		W_REG(pi->sh->osh, &pi->regs->smpl_clct_stpptr, pi_ht->pstop);

		/* set up post-trigger duration (expected by ucode in units of 1/8 us) */
		dur_1_8th_us = collect->post_dur << 3;
		W_REG(pi->sh->osh, &pi->regs->tsf_gpt2_ctr_l, dur_1_8th_us & 0x0000FFFF);
		W_REG(pi->sh->osh, &pi->regs->tsf_gpt2_ctr_h, dur_1_8th_us >> 16);

		/* start ucode trigger-based sample collect procedure */
		wlapi_bmac_write_shm(pi->sh->physhim, M_SMPL_COL_BMP, 0x0);
		phy_ctl = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl | (1 << 4));
		if (ISSIM_ENAB(pi->sh->sih)) {
			OSL_DELAY(1000*collect->pre_dur);
		} else {
			OSL_DELAY(collect->pre_dur);
		}
		wlapi_bmac_write_shm(pi->sh->physhim, M_SMPL_COL_BMP, collect->trigger);

		PHY_TRACE(("wl%d: %s Wait for trigger ...\n", pi->sh->unit, __FUNCTION__));

		/* wait until start bit has been cleared - or we time out */
		do {
			OSL_DELAY(10);
			val = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param) & 0x30;
			timer--;
		} while ((val != 0) && (timer > 0));

		/* set first and last pointer indices for readout */
		pi_ht->plast = R_REG(pi->sh->osh, &pi->regs->smpl_clct_curptr);
		if (pi_ht->plast >= (collect->pre_dur + collect->post_dur) * words_per_us) {
			pi_ht->pfirst =
				pi_ht->plast - (collect->pre_dur + collect->post_dur)*words_per_us;
		} else {
			pi_ht->pfirst = (pi_ht->pstop - pi_ht->pstart + 1) +
				pi_ht->plast - (collect->pre_dur + collect->post_dur)*words_per_us;
		}

		/* erase trigger setup */
		wlapi_bmac_write_shm(pi->sh->physhim, M_SMPL_COL_BMP, collect->trigger);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl & 0xFFCF);

		/* restore mac_ctl */
		W_REG(pi->sh->osh, &pi->regs->maccontrol, mac_ctl);
	}

	/* clean up */    
	/* return from deaf if requested */
	if (collect->be_deaf) {
		wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);
		wlapi_enable_mac(pi->sh->physhim);
	}

	/* turn off sample collect config in PHY & MAC */
	write_phy_reg(pi, HTPHY_AdcDataCollect, 0);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, 0x2);

	/* abort if timeout ocurred */
	if (timer == 0) {
		PHY_ERROR(("wl%d: %s Error: Timeout\n", pi->sh->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	PHY_TRACE(("wl%d: %s: Capture successful\n", pi->sh->unit, __FUNCTION__));

	if (pi_ht->pfirst > pi_ht->plast) {
		cnt = pi_ht->pstop - pi_ht->pfirst + 1;
		cnt += pi_ht->plast;
	} else {
		cnt = pi_ht->plast - pi_ht->pfirst;
	}

	sample_data->tag = htol16(WL_SAMPLEDATA_HEADER_TYPE);
	sample_data->length = htol16((WL_SAMPLEDATA_HEADER_SIZE));
	sample_data->flag = 0;		/* first sequence */
	sample_data->flag |= htol32(WL_SAMPLEDATA_MORE_DATA);

	/* store header to buf */
	ptr[0] = 0xACDC2009;
	ptr[1] = 0xFFFF0000 | (FILE_HDR_LEN<<8);
	ptr[2] = cnt;
	ptr[3] = 0xFFFF0000 | (pi->pubpi.phy_rev<<8) | pi->pubpi.phy_type;
	ptr[4] = 0xFFFFFFFF;
	ptr[5] = ((fc / 100) << 24) | ((fc % 100) << 16) | (3 << 8) |
	         (CHSPEC_IS40(pi->radio_chanspec) ? 40 : 20);
	if (collect->mode == 4)
		gpio_collection = 1;
	ptr[6] = (collect->gpio_sel << 24) | (collect->mode << 8) | gpio_collection;
	ptr[7] = 0xFFFF0000 | (collect->downsamp << 8) | collect->trigger;
	ptr[8] = 0xFFFFFFFF;
	ptr[9] = (collect->post_dur << 16) | collect->pre_dur;
	ptr[10] = (read_phy_reg(pi, HTPHY_RxIQCompA(0))) |
	          (read_phy_reg(pi, HTPHY_RxIQCompB(0)) << 16);
	ptr[11] = (read_phy_reg(pi, HTPHY_RxIQCompA(1))) |
	          (read_phy_reg(pi, HTPHY_RxIQCompB(1)) << 16);
	ptr[12] = (read_phy_reg(pi, HTPHY_RxIQCompA(2))) |
	          (read_phy_reg(pi, HTPHY_RxIQCompB(2)) << 16);
	ptr[13] = 0xFFFFFFFF;
	ptr[14] = 0xFFFFFFFF;
	ptr[15] = 0xFFFFFFFF;
	ptr[16] = 0xFFFFFFFF;
	ptr[17] = 0xFFFFFFFF;
	ptr[18] = 0xFFFFFFFF;
	ptr[19] = 0xFFFFFFFF;
	PHY_TRACE(("wl%d: %s: pfirst 0x%x plast 0x%x pstart 0x%x pstop 0x%x\n", pi->sh->unit,
		__FUNCTION__, pi_ht->pfirst, pi_ht->plast, pi_ht->pstart, pi_ht->pstop));
	PHY_TRACE(("wl%d: %s Capture length = %d words\n", pi->sh->unit, __FUNCTION__, cnt));
	return BCME_OK;
}

int
wlc_phy_sample_data_htphy(phy_info_t *pi, wl_sampledata_t *sample_data, void *b)
{
	uint32 data, cnt, bufsize, seq;
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	uint8* head = (uint8 *)b;
	uint32* buf = (uint32 *)(head + sizeof(wl_sampledata_t));

	bufsize = ltoh32(sample_data->length) - sizeof(wl_sampledata_t);
	bufsize = bufsize >> 2;		/* convert to # of words */
	seq = ltoh32(sample_data->flag) & 0xff;		/* get the last sequence number */
	sample_data->flag = htol32(seq++);
	PHY_TRACE(("wl%d: %s: bufsize(words) %d flag 0x%x\n", pi->sh->unit, __FUNCTION__,
		bufsize, sample_data->flag));

	/* store samples to buf */
	cnt = 0;
	W_REG(pi->sh->osh, &pi->regs->tplatewrptr, pi_ht->pfirst << 2);
	while ((cnt < bufsize) && (pi_ht->pfirst != pi_ht->plast)) {
		data = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
		/* write one 4-byte word */
		buf[cnt++] = htol32(data);
		/* wrap around end of fifo if necessary */
		if (pi_ht->pfirst == pi_ht->pstop) {
			W_REG(pi->sh->osh, &pi->regs->tplatewrptr, pi_ht->pstart << 2);
			pi_ht->pfirst = pi_ht->pstart;
			PHY_TRACE(("wl%d: %s TXFIFO wrap around\n", pi->sh->unit, __FUNCTION__));
		} else {
			pi_ht->pfirst++;
		}
	}

	PHY_TRACE(("wl%d: %s: Data fragment completed (pfirst %d plast %d)\n",
		pi->sh->unit, __FUNCTION__, pi_ht->pfirst, pi_ht->plast));
	if (pi_ht->pfirst != pi_ht->plast)
		sample_data->flag |= htol32(WL_SAMPLEDATA_MORE_DATA);

	/* update to # of bytes read */
	sample_data->length = htol16((cnt << 2));
	bcopy((uint8 *)sample_data, head, sizeof(wl_sampledata_t));
	PHY_TRACE(("wl%d: %s: Capture length = %d words\n", pi->sh->unit, __FUNCTION__, cnt));
	return BCME_OK;
}

#endif /* SAMPLE_COLLECT */

#if defined(WLTEST)


void
wlc_phy_bphy_testpattern_htphy(phy_info_t *pi, uint8 testpattern, uint16 rate_reg, bool enable)
{
	uint16 clip_off[] = {0xffff, 0xffff, 0xffff, 0xffff};
	uint16 core;

	if (enable == TRUE) {
		/* Save existing clip_detect state */
		wlc_phy_clip_det_htphy(pi, 0, pi->clip_state);
		/* Turn OFF all clip detections */
		wlc_phy_clip_det_htphy(pi, 1, clip_off);
		/* Save existing classifier state */
		pi->classifier_state = wlc_phy_classifier_htphy(pi, 0, 0);
		/* Turn OFF all classifcation */
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_classifierSel_MASK, 4);
		/* Trigger SamplePlay Start */
		write_phy_reg(pi,  HTPHY_sampleCmd, 0x1);
		/* Save BPHY test and testcontrol registers */
		pi->old_bphy_test = read_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST));
		pi->old_bphy_testcontrol = read_phy_reg(pi, HTPHY_bphytestcontrol);
		if (testpattern == HTPHY_TESTPATTERN_BPHY_EVM) {
			and_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST),
			            ~(TST_TXTEST_ENABLE|TST_TXTEST_RATE|TST_TXTEST_PHASE));
			or_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST),
			           TST_TXTEST_ENABLE | rate_reg);

		} else {
			and_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST), 0xfc00);
			or_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST), 0x0228);
		}
		/* Configure htphy's bphy testcontrol */
		write_phy_reg(pi, HTPHY_bphytestcontrol, 0xF);
		/* Force Rx2Tx */
		wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_RX2TX);

		FOREACH_CORE(pi, core) {
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, dac_pd, 1);
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, dac_pd, 0);
		}

		PHY_CAL(("wl%d: %s: Turning  ON: TestPattern = %3d  "
		         "en = %3d",
		         pi->sh->unit, __FUNCTION__, testpattern, enable));

	} else if (enable == FALSE) {
		/* Turn OFF BPHY testpattern */
		/* Turn off AFE Override */

		FOREACH_CORE(pi, core) {
			MOD_PHYREGC(pi, HTPHY_AfectrlOverride, core, dac_pd, 0);
			MOD_PHYREGC(pi, HTPHY_Afectrl, core, dac_pd, 1);
		}

		/* Force Tx2Rx */
		wlc_phy_force_rfseq_htphy(pi, HTPHY_RFSEQ_TX2RX);
		/* Restore BPHY test and testcontrol registers */
		write_phy_reg(pi, HTPHY_bphytestcontrol, pi->old_bphy_testcontrol);
		write_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST), pi->old_bphy_test);
		/* Turn ON receive packet activity */
		wlc_phy_clip_det_htphy(pi, 1, pi->clip_state);
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_classifierSel_MASK,
		                        pi->classifier_state);

		/* Trigger samplePlay Stop */
		write_phy_reg(pi,  HTPHY_sampleCmd, 0x2);

		PHY_CAL(("wl%d: %s: Turning OFF: TestPattern = %3d  "
		         "en = %3d",
		         pi->sh->unit, __FUNCTION__, testpattern, enable));
	}
}

void
wlc_phy_test_scraminit_htphy(phy_info_t *pi, int8 init)
{
	ASSERT(0);
}

int8
wlc_phy_test_tssi_htphy(phy_info_t *pi, int8 ctrl_type, int8 pwr_offs)
{
	int8 tssi;

	switch (ctrl_type & 0x3) {
	case 0:
	case 1:
	case 2:
		tssi = read_phy_reg(pi, HTPHY_TSSIBiasVal((ctrl_type & 0x3))) & 0xff;
		break;
	default:
		tssi = -127;
	}
	return (tssi);
}

void
wlc_phy_gpiosel_htphy(phy_info_t *pi, uint16 sel)
{
	uint32 mc;

	/* kill OutEn */
	write_phy_reg(pi, HTPHY_gpioLoOutEn, 0x0);
	write_phy_reg(pi, HTPHY_gpioHiOutEn, 0x0);


	/* take over gpio control from cc */
	si_gpiocontrol(pi->sh->sih, 0xffff, 0xffff, GPIO_DRV_PRIORITY);

	/* clear the mac selects, disable mac oe */
	mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);
	mc &= ~MCTL_GPOUT_SEL_MASK;
	W_REG(pi->sh->osh, &pi->regs->maccontrol, mc);
	W_REG(pi->sh->osh, &pi->regs->psm_gpio_oe, 0x0);

	/* set up htphy GPIO sel */
	write_phy_reg(pi, HTPHY_gpioSel, sel);
	write_phy_reg(pi, HTPHY_gpioLoOutEn, 0xffff);
	write_phy_reg(pi, HTPHY_gpioHiOutEn, 0xffff);
}

void
wlc_phy_pavars_get_htphy(phy_info_t *pi, uint16 *buf, uint16 band, uint16 core)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;

	buf[0] = pi_ht->pwrdet_a1[core][band];
	buf[1] = pi_ht->pwrdet_b0[core][band];
	buf[2] = pi_ht->pwrdet_b1[core][band];
}

void
wlc_phy_pavars_set_htphy(phy_info_t *pi, uint16 *buf, uint16 band, uint16 core)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;

	pi_ht->pwrdet_a1[core][band] = buf[0];
	pi_ht->pwrdet_b0[core][band] = buf[1];
	pi_ht->pwrdet_b1[core][band] = buf[2];
}

#endif 


#if defined(BCMDBG) || defined(WLTEST)
int
wlc_phy_freq_accuracy_htphy(phy_info_t *pi, int channel)
{
	phy_info_htphy_t *pi_ht = (phy_info_htphy_t *)pi->pi_ptr;
	int bcmerror = BCME_OK;

	if (channel == 0) {
		wlc_phy_stopplayback_htphy(pi);
		/* restore the old BBconfig, to restore resampler setting */
		write_phy_reg(pi, HTPHY_BBConfig, pi_ht->saved_bbconf);
		wlc_phy_resetcca_htphy(pi);
	} else {
		/* Disable the re-sampler (in case we are in spur avoidance mode) */
		pi_ht->saved_bbconf = read_phy_reg(pi, HTPHY_BBConfig);
		MOD_PHYREG(pi, HTPHY_BBConfig, resample_clk160, 0);
		/* use 151 since that should correspond to nominal tx output power */
		bcmerror = wlc_phy_tx_tone_htphy(pi, 0, 151, 0, 0, TRUE);
	}
	return bcmerror;
}
#endif 
