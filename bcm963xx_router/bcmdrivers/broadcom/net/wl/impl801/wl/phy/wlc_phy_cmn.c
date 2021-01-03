/*
 * PHY and RADIO specific portion of Broadcom BCM43XX 802.11 Networking Device Driver.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_cmn.c,v 1.2 2010/11/10 03:41:00 dkhoo Exp $
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
#include <bitfuncs.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <proto/802.11.h>
#include <sbchipc.h>
#include <hndpmu.h>
#include <bcmsrom_fmt.h>
#include <sbsprom.h>

#include <d11.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>
#include <wlc_phyreg_abg.h>
#include <wlc_phyreg_n.h>
#include <wlc_phyreg_ht.h>
#include <wlc_phyreg_lp.h>
#include <wlc_phyreg_ssn.h>
#include <wlc_phyreg_lcn.h>
#include <wlc_phytbl_n.h>
#include <wlc_phytbl_ht.h>
#include <wlc_phy_radio.h>
#include <wlc_phy_lcn.h>
#include <wlc_phy_lp.h>
#include <wlc_phy_ssn.h>
#include <bcmwifi.h>
#include <bcmotp.h>

#ifdef WLNOKIA_NVMEM
#include <wlc_phy_noknvmem.h>
#endif /* WLNOKIA_NVMEM */
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  macro, typedef, enum, structure, global variable		*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

uint32 phyhal_msg_level = PHYHAL_ERROR;

/* channel info structure */
typedef struct _chan_info_basic {
	uint16	chan;		/* channel number */
	uint16	freq;		/* in Mhz */
} chan_info_basic_t;

static chan_info_basic_t chan_info_all[] = {
	/* 11b/11g */
/* 0 */		{1,	2412},
/* 1 */		{2,	2417},
/* 2 */		{3,	2422},
/* 3 */		{4,	2427},
/* 4 */		{5,	2432},
/* 5 */		{6,	2437},
/* 6 */		{7,	2442},
/* 7 */		{8,	2447},
/* 8 */		{9,	2452},
/* 9 */		{10,	2457},
/* 10 */	{11,	2462},
/* 11 */	{12,	2467},
/* 12 */	{13,	2472},
/* 13 */	{14,	2484},

#ifdef BAND5G
/* 11a japan high */
/* 14 */	{34,	5170},
/* 15 */	{38,	5190},
/* 16 */	{42,	5210},
/* 17 */	{46,	5230},

/* 11a usa low */
/* 18 */	{36,	5180},
/* 19 */	{40,	5200},
/* 20 */	{44,	5220},
/* 21 */	{48,	5240},
/* 22 */	{52,	5260},
/* 23 */	{56,	5280},
/* 24 */	{60,	5300},
/* 25 */	{64,	5320},

/* 11a Europe */
/* 26 */	{100,	5500},
/* 27 */	{104,	5520},
/* 28 */	{108,	5540},
/* 29 */	{112,	5560},
/* 30 */	{116,	5580},
/* 31 */	{120,	5600},
/* 32 */	{124,	5620},
/* 33 */	{128,	5640},
/* 34 */	{132,	5660},
/* 35 */	{136,	5680},
/* 36 */	{140,	5700},

/* 11a usa high, ref5 only */
/* 37 */	{149,	5745},
/* 38 */	{153,	5765},
/* 39 */	{157,	5785},
/* 40 */	{161,	5805},
/* 41 */	{165,	5825},

/* 11a japan */
/* 42 */	{184,	4920},
/* 43 */	{188,	4940},
/* 44 */	{192,	4960},
/* 45 */	{196,	4980},
/* 46 */	{200,	5000},
/* 47 */	{204,	5020},
/* 48 */	{208,	5040},
/* 49 */	{212,	5060},
/* 50 */	{216,	50800}
#endif /* BAND5G */
};

uint16 ltrn_list[PHY_LTRN_LIST_LEN] = {
	0x18f9, 0x0d01, 0x00e4, 0xdef4, 0x06f1, 0x0ffc,
	0xfa27, 0x1dff, 0x10f0, 0x0918, 0xf20a, 0xe010,
	0x1417, 0x1104, 0xf114, 0xf2fa, 0xf7db, 0xe2fc,
	0xe1fb, 0x13ee, 0xff0d, 0xe91c, 0x171a, 0x0318,
	0xda00, 0x03e8, 0x17e6, 0xe9e4, 0xfff3, 0x1312,
	0xe105, 0xe204, 0xf725, 0xf206, 0xf1ec, 0x11fc,
	0x14e9, 0xe0f0, 0xf2f6, 0x09e8, 0x1010, 0x1d01,
	0xfad9, 0x0f04, 0x060f, 0xde0c, 0x001c, 0x0dff,
	0x1807, 0xf61a, 0xe40e, 0x0f16, 0x05f9, 0x18ec,
	0x0a1b, 0xff1e, 0x2600, 0xffe2, 0x0ae5, 0x1814,
	0x0507, 0x0fea, 0xe4f2, 0xf6e6
};

const uint8 ofdm_rate_lookup[] = {
	    /* signal */
	WLC_RATE_48M, /* 8: 48Mbps */
	WLC_RATE_24M, /* 9: 24Mbps */
	WLC_RATE_12M, /* A: 12Mbps */
	WLC_RATE_6M,  /* B:  6Mbps */
	WLC_RATE_54M, /* C: 54Mbps */
	WLC_RATE_36M, /* D: 36Mbps */
	WLC_RATE_18M, /* E: 18Mbps */
	WLC_RATE_9M   /* F:  9Mbps */
};

#ifdef BCMECICOEX	    /* BTC notifications */
#define NOTIFY_BT_CHL(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_CHANNEL_MASK, ((val) << ECI_OUT_CHANNEL_SHIFT), TRUE)
#define NOTIFY_BT_BW_20(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK, ((ECI_BW_25) << ECI_OUT_BW_SHIFT), TRUE)
#define NOTIFY_BT_BW_40(sih) \
	si_eci_notify_bt((sih), ECI_OUT_BW_MASK, ((ECI_BW_45) << ECI_OUT_BW_SHIFT), TRUE)
#define NOTIFY_BT_NUM_ANTENNA(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_ANTENNA_MASK, ((val) << ECI_OUT_ANTENNA_SHIFT), TRUE)
#define NOTIFY_BT_TXRX(sih, val) \
	si_eci_notify_bt((sih), ECI_OUT_SIMUL_TXRX_MASK, ((val) << ECI_OUT_SIMUL_TXRX_SHIFT, TRUE))
static void wlc_phy_update_bt_chanspec(phy_info_t *pi, chanspec_t chanspec);
#else
#define wlc_phy_update_bt_chanspec(pi, chanspec)
#endif /* BCMECICOEX */

#define PHY_WREG_LIMIT	24	/* number of consecutive phy register write before a readback */
#define PHY_WREG_LIMIT_VENDOR 1	/* num of consec phy reg write before a readback for vendor */

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  local prototype						*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* %%%%%% major operations */
static void wlc_set_phy_uninitted(phy_info_t *pi);
static uint32 wlc_phy_get_radio_ver(phy_info_t *pi);
static void wlc_phy_timercb_phycal(void *arg);

/* %%%%%% Calibration, ACI, noise/rssi mesasurement */
static void wlc_phy_noise_calc(phy_info_t *pi, uint32 *cmplx_pwr, int8 *pwr_ant);
static void wlc_phy_noise_save(phy_info_t *pi, int8 *noise_dbm_ant, int8 *max_noise_dbm);

static void wlc_phy_aci_upd(phy_info_t *pi);
static void wlc_phy_aci_enter(phy_info_t *pi);
static void wlc_phy_aci_exit(phy_info_t *pi);
static void wlc_phy_aci_update_ma(phy_info_t *pi);

LOCATOR_EXTERN bool wlc_phy_interference(phy_info_t *pi, int wanted_mode, bool init);
static void wlc_phy_vco_cal(phy_info_t *pi);
static void wlc_phy_cal_perical_mphase_schedule(phy_info_t *pi, uint delay);
static void wlc_phy_noise_cb(phy_info_t *pi, uint8 channel, int8 noise_dbm);
static void wlc_phy_noise_sample_request(wlc_phy_t *pih, uint8 reason, uint8 ch);

/* %%%%%% temperature-based fallback to 1-Tx */
static void wlc_phy_txcore_temp(phy_info_t *pi);

/* %%%%%% power control */
static void wlc_phy_txpower_reg_limit_calc(phy_info_t *pi, struct txpwr_limits *tp, chanspec_t);
static bool wlc_phy_cal_txpower_recalc_sw(phy_info_t *pi);

/* %%%%%% testing */
#if defined(BCMDBG) || defined(WLTEST)
static int wlc_phy_test_carrier_suppress(phy_info_t *pi, int channel);
static int wlc_phy_test_freq_accuracy(phy_info_t *pi, int channel);
static int wlc_phy_test_evm(phy_info_t *pi, int channel, uint rate, int txpwr);
#endif


static uint32 wlc_phy_rx_iq_est(phy_info_t *pi, uint8 samples, uint8 antsel, uint8 wait_for_crs);
#ifdef SAMPLE_COLLECT
static int wlc_phy_sample_collect(phy_info_t *pi, wl_samplecollect_args_t *collect, void *buff);
static int wlc_phy_sample_data(phy_info_t *pi, wl_sampledata_t *sample_data, void *b);
static int wlc_phy_sample_collect_old(phy_info_t *pi, wl_samplecollect_args_t *collect, void *buff);
#endif

/* %%%%%% radar */
#if defined(AP) && defined(RADAR)
static void wlc_phy_radar_read_table(phy_info_t *pi, radar_work_t *rt, int min_pulses);
static void wlc_phy_radar_generate_tlist(uint32 *inlist, int *outlist, int length, int n);
static void wlc_phy_radar_generate_tpw(uint32 *inlist, int *outlist, int length, int n);
static void wlc_phy_radar_filter_list(int *inlist, int *length, int min_val, int max_val);
static void wlc_shell_sort(int len, int *vector);
static int  wlc_phy_radar_select_nfrequent(int *inlist, int length, int n, int *val,
	int *pos, int *f, int *vlist, int *flist);
static int  wlc_phy_radar_detect_run_aphy(phy_info_t *pi);
static void wlc_phy_radar_detect_init_aphy(phy_info_t *pi, bool on);
static int  wlc_phy_radar_detect_run_nphy(phy_info_t *pi);
static void wlc_phy_radar_detect_init_nphy(phy_info_t *pi, bool on);
static void wlc_phy_radar_attach_nphy(phy_info_t *pi);
#endif /* defined(AP) && defined(RADAR) */

static int wlc_phy_iovar_dispatch_old(phy_info_t *pi, uint32 actionid, void *p, void *a, int vsize,
	int32 int_val, bool bool_val);

static int8 wlc_user_txpwr_antport_to_rfport(phy_info_t *pi, uint chan, uint32 band, uint8 rate);
static void wlc_phy_upd_env_txpwr_rate_limits(phy_info_t *pi, uint32 band);
static int8 wlc_phy_env_measure_vbat(phy_info_t *pi);
static int8 wlc_phy_env_measure_temperature(phy_info_t *pi);

#if defined(WLMCHAN) && defined(BCMDBG)
static void wlc_phydump_chanctx(phy_info_t *phi, struct bcmstrbuf *b);
#endif

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  function implementation   					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

char *
#ifdef BCMDBG
phy_getvar(phy_info_t *pi, const char *name, const char *function)
#else
phy_getvar(phy_info_t *pi, const char *name)
#endif
{
#ifdef _MINOSL_
	return NULL;
#else
	char *vars = pi->vars;
	char *s;
	int len;

#ifdef BCMDBG
	/* Use vars pointing to itself as a flag that wlc_phy_attach is complete.
	 * Can't use NULL because it means there are no vars but we still
	 * need to potentially search NVRAM.
	 */

	if (pi->vars == (char *)&pi->vars)
		PHY_ERROR(("Usage of phy_getvar/phy_getintvar by %s after wlc_phy_attach\n",
			function));
#endif /* BCMDBG */
	ASSERT(pi->vars != (char *)&pi->vars);

	if (!name)
		return NULL;

	len = strlen(name);
	if (len == 0)
		return NULL;

	/* first look in vars[] */
	for (s = vars; s && *s;) {
		if ((bcmp(s, name, len) == 0) && (s[len] == '='))
			return (&s[len+1]);

		while (*s++)
			;
	}

#ifdef LINUX_HYBRID
	/* Don't look elsewhere! */
	return NULL;
#else
	/* Query nvram */
	return (nvram_get(name));

#endif

#endif	/* _MINOSL_ */
}

int
#ifdef BCMDBG
phy_getintvar(phy_info_t *pi, const char *name, const char *function)
#else
phy_getintvar(phy_info_t *pi, const char *name)
#endif
{
#ifdef _MINOSL_
	return 0;
#else
	char *val;

	if ((val = PHY_GETVAR(pi, name)) == NULL)
		return (0);

	return (bcm_strtoul(val, NULL, 0));
#endif	/* _MINOSL_ */
}

/* coordinate with MAC before access PHY register */
void
wlc_phyreg_enter(wlc_phy_t *pih)
{
#ifdef STA
	phy_info_t *pi = (phy_info_t*)pih;
	wlapi_bmac_ucode_wake_override_phyreg_set(pi->sh->physhim);
#endif
}

void
wlc_phyreg_exit(wlc_phy_t *pih)
{
#ifdef STA
	phy_info_t *pi = (phy_info_t*)pih;
	wlapi_bmac_ucode_wake_override_phyreg_clear(pi->sh->physhim);
#endif
}

/* coordinate with MAC before access RADIO register */
void
wlc_radioreg_enter(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;
	wlapi_bmac_mctrl(pi->sh->physhim, MCTL_LOCK_RADIO, MCTL_LOCK_RADIO);

	/* allow any ucode radio reg access to complete */
	OSL_DELAY(10);
}

void
wlc_radioreg_exit(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;
	volatile uint16 dummy;

	/* allow our radio reg access to complete */
	dummy = R_REG(pi->sh->osh, &pi->regs->phyversion);
	pi->phy_wreg = 0;
	wlapi_bmac_mctrl(pi->sh->physhim, MCTL_LOCK_RADIO, 0);
}

/* All radio regs other than idcode are less than 16bits, so
 * {read, write}_radio_reg access the low 16bits only.
 * When reading the idcode use read_radio_id instead.
 */
uint16
read_radio_reg(phy_info_t *pi, uint16 addr)
{
	uint16 data;

	if ((addr == RADIO_IDCODE) && (!ISHTPHY(pi)))
		return 0xffff;

	if (NORADIO_ENAB(pi->pubpi))
		return (NORADIO_IDCODE & 0xffff);

	switch (pi->pubpi.phy_type) {
	case PHY_TYPE_A:
		CASECHECK(PHYTYPE, PHY_TYPE_A);
		addr |= RADIO_2060WW_READ_OFF;
		break;

	case PHY_TYPE_G:
		CASECHECK(PHYTYPE, PHY_TYPE_G);
		addr |= RADIO_2050_READ_OFF;
		break;

	case PHY_TYPE_N:
		CASECHECK(PHYTYPE, PHY_TYPE_N);
		if (NREV_GE(pi->pubpi.phy_rev, 7))
			addr |= RADIO_2057_READ_OFF;
		else
			addr |= RADIO_2055_READ_OFF;  /* works for 2056 too */
		break;

	case PHY_TYPE_HT:
		CASECHECK(PHYTYPE, PHY_TYPE_HT);
		addr |= RADIO_2059_READ_OFF;
		break;

	case PHY_TYPE_LP:
		CASECHECK(PHYTYPE, PHY_TYPE_LP);
		if (BCM2063_ID == LPPHY_RADIO_ID(pi)) {
			addr |= RADIO_2063_READ_OFF;
		} else {
			addr |= RADIO_2062_READ_OFF;
		}
		break;

	case PHY_TYPE_SSN:
		CASECHECK(PHYTYPE, PHY_TYPE_SSN);
		addr |= RADIO_2063_READ_OFF;
		break;
	case PHY_TYPE_LCN:
		CASECHECK(PHYTYPE, PHY_TYPE_LCN);
		addr |= RADIO_2064_READ_OFF;
		break;

	default:
		ASSERT(VALID_PHYTYPE(pi->pubpi.phy_type));
	}

	if ((D11REV_GE(pi->sh->corerev, 24)) ||
	    (D11REV_IS(pi->sh->corerev, 22) && (pi->pubpi.phy_type != PHY_TYPE_SSN))) {
		W_REG(pi->sh->osh, &pi->regs->radioregaddr, addr);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->radioregaddr);
#endif
		data = R_REG(pi->sh->osh, &pi->regs->radioregdata);
	} else {
		W_REG(pi->sh->osh, &pi->regs->phy4waddr, addr);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->phy4waddr);
#endif

#ifdef __ARM_ARCH_4T__
		__asm__(" .align 4 ");
		__asm__(" nop ");
		data = R_REG(pi->sh->osh, &pi->regs->phy4wdatalo);
#else
		data = R_REG(pi->sh->osh, &pi->regs->phy4wdatalo);
#endif

	}
	pi->phy_wreg = 0;	/* clear reg write metering */

	return data;
}

void
write_radio_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	osl_t *osh;
	volatile uint16 dummy;


	if (NORADIO_ENAB(pi->pubpi))
		return;

	osh = pi->sh->osh;

	/* The nphy chips with with corerev 22 have the new i/f, the one with
	 * ssnphy (4319b0) does not.
	 */
	if (BUSTYPE(pi->sh->bustype) == PCI_BUS) {
		if (++pi->phy_wreg >= pi->phy_wreg_limit) {
			(void)R_REG(osh, &pi->regs->maccontrol);
			pi->phy_wreg = 0;
		}
	}
	if ((D11REV_GE(pi->sh->corerev, 24)) ||
	    (D11REV_IS(pi->sh->corerev, 22) && (pi->pubpi.phy_type != PHY_TYPE_SSN))) {

		W_REG(osh, &pi->regs->radioregaddr, addr);
#ifdef __mips__
		(void)R_REG(osh, &pi->regs->radioregaddr);
#endif
		W_REG(osh, &pi->regs->radioregdata, val);
	} else {
		W_REG(osh, &pi->regs->phy4waddr, addr);
#ifdef __mips__
		(void)R_REG(osh, &pi->regs->phy4waddr);
#endif
		W_REG(osh, &pi->regs->phy4wdatalo, val);
	}

	if ((BUSTYPE(pi->sh->bustype) == PCMCIA_BUS) &&
	    (pi->sh->buscorerev <= 3)) {
		dummy = R_REG(osh, &pi->regs->phyversion);
	}
}

static uint32
read_radio_id(phy_info_t *pi)
{
	uint32 id;

	if (NORADIO_ENAB(pi->pubpi))
		return (NORADIO_IDCODE);

	if (D11REV_GE(pi->sh->corerev, 24)) {
		uint32 b0, b1, b2;

		W_REG(pi->sh->osh, &pi->regs->radioregaddr, 0);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->radioregaddr);
#endif
		b0 = (uint32)R_REG(pi->sh->osh, &pi->regs->radioregdata);
		W_REG(pi->sh->osh, &pi->regs->radioregaddr, 1);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->radioregaddr);
#endif
		b1 = (uint32)R_REG(pi->sh->osh, &pi->regs->radioregdata);
		W_REG(pi->sh->osh, &pi->regs->radioregaddr, 2);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->radioregaddr);
#endif
		b2 = (uint32)R_REG(pi->sh->osh, &pi->regs->radioregdata);

		id = ((b0  & 0xf) << 28) | (((b2 << 8) | b1) << 12) | ((b0 >> 4) & 0xf);
	} else {
		W_REG(pi->sh->osh, &pi->regs->phy4waddr, RADIO_IDCODE);
#ifdef __mips__
		(void)R_REG(pi->sh->osh, &pi->regs->phy4waddr);
#endif
		id = (uint32)R_REG(pi->sh->osh, &pi->regs->phy4wdatalo);
		id |= (uint32)R_REG(pi->sh->osh, &pi->regs->phy4wdatahi) << 16;
	}
	pi->phy_wreg = 0;	/* clear reg write metering */
	return id;
}

void
and_radio_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	uint16 rval;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	rval = read_radio_reg(pi, addr);
	write_radio_reg(pi, addr, (rval & val));
}

void
or_radio_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	uint16 rval;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	rval = read_radio_reg(pi, addr);
	write_radio_reg(pi, addr, (rval | val));
}

void
xor_radio_reg(phy_info_t *pi, uint16 addr, uint16 mask)
{
	uint16 rval;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	rval = read_radio_reg(pi, addr);
	write_radio_reg(pi, addr, (rval ^ mask));
}

void
mod_radio_reg(phy_info_t *pi, uint16 addr, uint16 mask, uint16 val)
{
	uint16 rval;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	rval = read_radio_reg(pi, addr);
	write_radio_reg(pi, addr, (rval & ~mask) | (val & mask));
}

void
write_phy_channel_reg(phy_info_t *pi, uint val)
{
	volatile uint16 dummy;

	W_REG(pi->sh->osh, &pi->regs->phychannel, val);

	if ((BUSTYPE(pi->sh->bustype) == PCMCIA_BUS) &&
	    (pi->sh->buscorerev <= 3)) {
		dummy = R_REG(pi->sh->osh, &pi->regs->phyversion);
	}
}

#if defined(BCMDBG) || defined(BCMASSERT_SUPPORT)
static bool
wlc_phy_war41476(phy_info_t *pi)
{
	uint32 mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);

	return ((mc & MCTL_EN_MAC) == 0) || ((mc & MCTL_PHYLOCK) == MCTL_PHYLOCK);
}
#endif /* BCMDBG || BCMASSERT_SUPPORT */

uint16
read_phy_reg(phy_info_t *pi, uint16 addr)
{
	osl_t *osh;
	d11regs_t *regs;

	osh = pi->sh->osh;
	regs = pi->regs;

	W_REG(osh, &regs->phyregaddr, addr);
#ifdef __mips__
	(void)R_REG(osh, &regs->phyregaddr);
#endif

	ASSERT(!(D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) ||
	       wlc_phy_war41476(pi));

	pi->phy_wreg = 0;	/* clear reg write metering */
	return (R_REG(osh, &regs->phyregdata));
}

void
write_phy_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	osl_t *osh;
	d11regs_t *regs;

	osh = pi->sh->osh;
	regs = pi->regs;

#ifdef __mips__
	W_REG(osh, &regs->phyregaddr, addr);
	(void)R_REG(osh, &regs->phyregaddr);
	W_REG(osh, &regs->phyregdata, val);
	if (addr == NPHY_TableAddress)
		(void)R_REG(osh, &regs->phyregdata);
#else
	if (BUSTYPE(pi->sh->bustype) == PCI_BUS) {
		if (++pi->phy_wreg >= pi->phy_wreg_limit) {
			pi->phy_wreg = 0;
			(void)R_REG(osh, &regs->phyversion);
		}
	}
	W_REG(osh, (volatile uint32 *)(uintptr)(&regs->phyregaddr), addr | (val << 16));
#endif /* __mips__ */
}

void
and_phy_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	osl_t *osh;
	d11regs_t *regs;

	osh = pi->sh->osh;
	regs = pi->regs;

	W_REG(osh, &regs->phyregaddr, addr);
#ifdef __mips__
	(void)R_REG(osh, &regs->phyregaddr);
#endif

	ASSERT(!(D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) ||
	       wlc_phy_war41476(pi));

	W_REG(osh, &regs->phyregdata, (R_REG(osh, &regs->phyregdata) & val));
	pi->phy_wreg = 0;	/* clear reg write metering */
}

void
or_phy_reg(phy_info_t *pi, uint16 addr, uint16 val)
{
	osl_t *osh;
	d11regs_t *regs;

	osh = pi->sh->osh;
	regs = pi->regs;

	W_REG(osh, &regs->phyregaddr, addr);
#ifdef __mips__
	(void)R_REG(osh, &regs->phyregaddr);
#endif

	ASSERT(!(D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) ||
	       wlc_phy_war41476(pi));

	W_REG(osh, &regs->phyregdata, (R_REG(osh, &regs->phyregdata) | val));
	pi->phy_wreg = 0;	/* clear reg write metering */
}

void
mod_phy_reg(phy_info_t *pi, uint16 addr, uint16 mask, uint16 val)
{
	osl_t *osh;
	d11regs_t *regs;

	osh = pi->sh->osh;
	regs = pi->regs;

	W_REG(osh, &regs->phyregaddr, addr);
#ifdef __mips__
	(void)R_REG(osh, &regs->phyregaddr);
#endif

	ASSERT(!(D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) ||
	       wlc_phy_war41476(pi));

	W_REG(osh, &regs->phyregdata, ((R_REG(osh, &regs->phyregdata) & ~mask) | (val & mask)));
	pi->phy_wreg = 0;	/* clear reg write metering */
}


static void
WLBANDINITFN(wlc_set_phy_uninitted)(phy_info_t *pi)
{
	int i, j;

	/* Prepare for one-time initializations */
	pi->initialized = FALSE;

	pi->tx_vos = 0xffff;
	pi->nrssi_table_delta = 0x7fffffff;
	pi->rc_cal = 0xffff;
	pi->mintxbias = 0xffff;
	pi->txpwridx = -1;
	if (ISNPHY(pi)) {
		pi->phy_spuravoid = SPURAVOID_DISABLE;

		if NREV_GE(pi->pubpi.phy_rev, 3)
			pi->phy_spuravoid = SPURAVOID_AUTO;

		pi->nphy_papd_skip = 0;
		pi->nphy_papd_epsilon_offset[0] = 0xf588;
		pi->nphy_papd_epsilon_offset[1] = 0xf588;
		pi->nphy_txpwr_idx[0] = 128;
		pi->nphy_txpwr_idx[1] = 128;
		pi->nphy_txpwrindex[0].index_internal = 40;
		pi->nphy_txpwrindex[1].index_internal = 40;
		pi->phy_pabias = 0; /* default means no override */
	} else if (ISHTPHY(pi)) {
		pi->phy_spuravoid = SPURAVOID_AUTO;
		for (i = 0; i < PHY_CORE_MAX; i++)
			pi->txpwrindex[i] = 40;
	} else {
		pi->phy_spuravoid = SPURAVOID_AUTO;
	}
	pi->radiopwr = 0xffff;
	for (i = 0; i < STATIC_NUM_RF; i++) {
		for (j = 0; j < STATIC_NUM_BB; j++) {
			pi->stats_11b_txpower[i][j] = -1;
		}
	}
}

/* returns a pointer to per interface instance data */
shared_phy_t *
BCMATTACHFN(wlc_phy_shared_attach)(shared_phy_params_t *shp)
{
	shared_phy_t *sh;

	/* allocate wlc_info_t state structure */
	if ((sh = (shared_phy_t*) MALLOC(shp->osh, sizeof(shared_phy_t))) == NULL) {
		PHY_ERROR(("wl%d: wlc_phy_shared_state: out of memory, malloced %d bytes\n",
			shp->unit, MALLOCED(shp->osh)));
		return NULL;
	}
	bzero((char*)sh, sizeof(shared_phy_t));

	sh->osh = shp->osh;
	sh->sih = shp->sih;
	sh->physhim = shp->physhim;
	sh->unit = shp->unit;
	sh->corerev = shp->corerev;

	sh->vid = shp->vid;
	sh->did = shp->did;
	sh->chip = shp->chip;
	sh->chiprev = shp->chiprev;
	sh->chippkg = shp->chippkg;
	sh->sromrev = shp->sromrev;
	sh->boardtype = shp->boardtype;
	sh->boardrev = shp->boardrev;
	sh->boardvendor = shp->boardvendor;
	sh->boardflags = shp->boardflags;
	sh->boardflags2 = shp->boardflags2;
	sh->bustype = shp->bustype;
	sh->buscorerev = shp->buscorerev;

	/* create our timers */
	sh->fast_timer = PHY_SW_TIMER_FAST;
	sh->slow_timer = PHY_SW_TIMER_SLOW;
	sh->glacial_timer = PHY_SW_TIMER_GLACIAL;

	/* ACI mitigation mode is auto by default */
	sh->interference_mode = WLAN_AUTO;
	sh->rssi_mode = RSSI_ANT_MERGE_MAX;

	return sh;
}

void
BCMATTACHFN(wlc_phy_shared_detach)(shared_phy_t *phy_sh)
{
	osl_t *osh;

	if (phy_sh) {
		osh = phy_sh->osh;

		/* phy_head must have been all detached */
		if (phy_sh->phy_head) {
			PHY_ERROR(("wl%d: %s non NULL phy_head\n", phy_sh->unit, __FUNCTION__));
			ASSERT(!phy_sh->phy_head);
		}
		MFREE(osh, phy_sh, sizeof(shared_phy_t));
	}
}

/* Figure out if we have a phy for the requested band and attach to it */
wlc_phy_t *
BCMATTACHFN(wlc_phy_attach)(shared_phy_t *sh, void *regs, int bandtype, char *vars)
{
	phy_info_t *pi;
	uint32 sflags = 0;
	uint phyversion;
	int i;
	osl_t *osh;

	osh = sh->osh;

	PHY_TRACE(("wl: %s(%p, %d, %p)\n", __FUNCTION__, regs, bandtype, sh));

	if (D11REV_IS(sh->corerev, 4))
		sflags = SISF_2G_PHY | SISF_5G_PHY;
	else
		sflags = si_core_sflags(sh->sih, 0, 0);

	if (BAND_5G(bandtype)) {
		if ((sflags & (SISF_5G_PHY | SISF_DB_PHY)) == 0) {
			PHY_ERROR(("wl%d: %s: No phy available for 5G\n",
			          sh->unit, __FUNCTION__));
			return NULL;
		}
	}

	if ((sflags & SISF_DB_PHY) && (pi = sh->phy_head)) {
		/* For the second band in dualband phys, just bring the core back out of reset */
		wlapi_bmac_corereset(pi->sh->physhim, pi->pubpi.coreflags);
		pi->refcnt++;
		return &pi->pubpi_ro;
	}

	if ((pi = (phy_info_t *)MALLOC(osh, sizeof(phy_info_t))) == NULL) {
		PHY_ERROR(("wl%d: %s: out of memory, malloced %d bytes", sh->unit,
		          __FUNCTION__, MALLOCED(osh)));
		return NULL;
	}
	bzero((char *)pi, sizeof(phy_info_t));

	/* Assign the default cal info */
	pi->cal_info = &pi->def_cal_info;

#ifdef WLNOKIA_NVMEM
	pi->noknvmem = wlc_phy_noknvmem_attach(osh, pi);
	if (pi->noknvmem == NULL) {
		PHY_ERROR(("wl%d: %s: wlc_phy_noknvmem_attach failed ", sh->unit, __FUNCTION__));
		goto err;
	}
#endif /* WLNOKIA_NVMEM */
	pi->regs = (d11regs_t *)regs;
	pi->sh = sh;
	pi->phy_init_por = TRUE;
	if ((pi->sh->boardvendor == VENDOR_APPLE) &&
	    (pi->sh->boardtype == 0x0093)) {
		pi->phy_wreg_limit = PHY_WREG_LIMIT_VENDOR;
	}
	else {
		pi->phy_wreg_limit = PHY_WREG_LIMIT;
	}

	pi->vars = vars;

	/* set default power output percentage to 100 percent */
	pi->txpwr_percent = 100;

	/* enable initcal by default */
	pi->do_initcal = TRUE;

	/* this will get the value from the SROM */
	pi->phycal_tempdelta = 0;

	if (BAND_2G(bandtype) && (sflags & SISF_2G_PHY)) {
		/* Set the sflags gmode indicator */
		pi->pubpi.coreflags = SICF_GMODE;
	}

	/* get the phy type & revision, enable the core */
	wlapi_bmac_corereset(pi->sh->physhim, pi->pubpi.coreflags);
	phyversion = R_REG(osh, &pi->regs->phyversion);

	pi->pubpi.phy_type = PHY_TYPE(phyversion);
	pi->pubpi.phy_rev = phyversion & PV_PV_MASK;
	/* LCNXN */
	if (pi->pubpi.phy_type == PHY_TYPE_LCNXN) {
		pi->pubpi.phy_type = PHY_TYPE_N;
		pi->pubpi.phy_rev += LCNXN_BASEREV;
	}
	pi->pubpi.phy_corenum = ISHTPHY(pi) ? PHY_CORE_NUM_3 :
		(ISNPHY(pi) ? PHY_CORE_NUM_2 : PHY_CORE_NUM_1);
	pi->pubpi.ana_rev = (phyversion & PV_AV_MASK) >> PV_AV_SHIFT;

	if (!VALID_PHYTYPE(pi->pubpi.phy_type)) {
		PHY_ERROR(("wl%d: %s: invalid phy_type = %d\n",
		          sh->unit, __FUNCTION__, pi->pubpi.phy_type));
		goto err;
	}
	if (BAND_5G(bandtype)) {
		if (!ISAPHY(pi) && !ISNPHY(pi) && !ISLPPHY(pi) && !ISSSLPNPHY(pi) && !ISHTPHY(pi)) {
			PHY_ERROR(("wl%d: %s: invalid phy_type = %d for band 5G\n",
			          sh->unit, __FUNCTION__, pi->pubpi.phy_type));
			goto err;
		}
	} else {
		if (!ISGPHY(pi) && !ISNPHY(pi) && !ISLPPHY(pi) && !ISSSLPNPHY(pi) &&
			!ISLCNPHY(pi) && !ISHTPHY(pi))
		{
			PHY_ERROR(("wl%d: %s: invalid phy_type = %d for band 2G\n",
			          sh->unit, __FUNCTION__, pi->pubpi.phy_type));
			goto err;
		}
	}

	/* read the radio idcode */
	if (ISSIM_ENAB(pi->sh->sih) && !ISHTPHY(pi)) {
		PHY_INFORM(("wl%d: Assuming NORADIO, chip 0x%x pkgopt 0x%x\n", sh->unit,
		           pi->sh->chip, pi->sh->chippkg));
		pi->pubpi.radioid = NORADIO_ID;
		pi->pubpi.radiorev = 5;
	} else {
		uint32 idcode;

		wlc_phy_anacore((wlc_phy_t*)pi, ON);

		idcode = wlc_phy_get_radio_ver(pi);
		pi->pubpi.radioid = (idcode & IDCODE_ID_MASK) >> IDCODE_ID_SHIFT;
		pi->pubpi.radiorev = (idcode & IDCODE_REV_MASK) >> IDCODE_REV_SHIFT;
		pi->pubpi.radiover = (idcode & IDCODE_VER_MASK) >> IDCODE_VER_SHIFT;
		if (!VALID_RADIO(pi, pi->pubpi.radioid)) {
			PHY_ERROR(("wl%d: %s: Unknown radio ID: 0x%x rev 0x%x phy %d, phyrev %d\n",
			          sh->unit, __FUNCTION__, pi->pubpi.radioid, pi->pubpi.radiorev,
			          pi->pubpi.phy_type, pi->pubpi.phy_rev));
			goto err;
		}

		/* make sure the radio is off until we do an "up" */
		wlc_phy_switch_radio((wlc_phy_t*)pi, OFF);
	}

	/* Prepare for one-time initializations */
	wlc_set_phy_uninitted(pi);

	pi->aci_exit_check_period = ISNPHY(pi) ? 15 : 60;
	pi->aci_state = 0;

	/* default channel and channel bandwidth is 20 MHZ */
	pi->bw = WL_CHANSPEC_BW_20;
	pi->radio_chanspec = BAND_2G(bandtype) ? CH20MHZ_CHSPEC(1) : CH20MHZ_CHSPEC(36);
	pi->interf.curr_home_channel = CHSPEC_CHANNEL(pi->radio_chanspec);

	if (ISNPHY(pi)) {
		pi->sh->interference_mode_override = FALSE;
		if (NREV_GE(pi->pubpi.phy_rev, 3)) {
			pi->sh->interference_mode_2G = WLAN_AUTO;
			pi->sh->interference_mode_5G = NON_WLAN;
			if (((CHIPID(pi->sh->chip) == BCM4716_CHIP_ID) ||
				(CHIPID(pi->sh->chip) == BCM4748_CHIP_ID)) &&
				(pi->sh->chippkg == BCM4718_PKG_ID)) {
				pi->sh->interference_mode_2G = WLAN_AUTO_W_NOISE;
			}
			if (BAND_2G(bandtype)) {
				pi->sh->interference_mode =
					pi->sh->interference_mode_2G;
			} else {
				pi->sh->interference_mode =
					pi->sh->interference_mode_5G;
			}
			pi->interf.scanroamtimer = 0;
		} else {
			/* 4321 */
			pi->sh->interference_mode = WLAN_AUTO;
		}
	}

	/* set default rx iq est antenna/samples */
	pi->rxiq_samps = PHY_NOISE_SAMPLE_LOG_NUM_NPHY;
	pi->rxiq_antsel = ANT_RX_DIV_DEF;

	pi->phywatchdog_override = TRUE;

	pi->cal_type_override = PHY_PERICAL_AUTO;

	/* Reset the saved noisevar count */
	pi->nphy_saved_noisevars.bufcount = 0;

	/* minimum reliable txpwr target is 8 dBm/mimo, 10 dbm/legacy  */
	if (ISNPHY(pi) || ISHTPHY(pi))
		pi->min_txpower = PHY_TXPWR_MIN_NPHY;
	else
		pi->min_txpower = PHY_TXPWR_MIN;

	/* Enable both cores by default */
	pi->sh->phyrxchain = 0x3;

#if defined(WLTEST)
	/* Initialize to invalid index values */
	pi->nphy_tbldump_minidx = -1;
	pi->nphy_tbldump_maxidx = -1;
	pi->nphy_phyreg_skipcnt = 0;
#endif

	/* Initialize to -1 to indicate that rx2tx table wasn't modified
	 * to NOP the CLR_RXTX_BIAS entry
	 */
	pi->rx2tx_biasentry = -1;

	/* Parameters for temperature-based fallback to 1-Tx chain */
	wlc_phy_txcore_temp(pi);

	pi->phy_tempsense_offset = 0;

	/* This is the temperature at which the last PHYCAL was done.
	 * Initialize to a very low value.
	 */
	pi->def_cal_info.last_cal_temp = -50;

	/* only NPHY/LPPHY support interrupt based noise measurement */
	pi->phynoise_polling = TRUE;
	if (ISNPHY(pi) || ISLPPHY(pi) || ISLCNPHY(pi) || ISHTPHY(pi))
		pi->phynoise_polling = FALSE;

	/* initialize our txpwr limit to a large value until we know what band/channel
	 * we settle on in wlc_up() set the txpwr user override to the max
	 */
	for (i = 0; i < TXP_NUM_RATES; i++) {
		pi->txpwr_limit[i] = WLC_TXPWR_MAX;
		pi->txpwr_env_limit[i] = WLC_TXPWR_MAX;
		pi->tx_user_target[i] = WLC_TXPWR_MAX;
	}

	/* default radio power */
	pi->radiopwr_override = RADIOPWR_OVERRIDE_DEF;

	/* user specified power is at the ant port */
	pi->user_txpwr_at_rfport = FALSE;

#ifdef WLNOKIA_NVMEM
	/* user specified power is at the ant port */
	pi->user_txpwr_at_rfport = TRUE;
#endif

	if (ISABGPHY(pi)) {
		if (!wlc_phy_attach_abgphy(pi, bandtype))
			return NULL;
	} else if (ISNPHY(pi)) {
		/* only use for NPHY for now */
		if (!(pi->phycal_timer = wlapi_init_timer(pi->sh->physhim,
			wlc_phy_timercb_phycal, pi, "phycal"))) {
			PHY_ERROR(("wlc_timers_init: wlapi_init_timer for phycal_timer failed\n"));
			goto err;
		}
		
#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
		if (!(pi->percal_task = wlapi_init_delayed_task(pi->sh->physhim, (void *)wlc_phy_timercb_phycal, pi))) {
			PHY_ERROR(("wl_init_delayed_task failed\n"));
			if (pi->phycal_timer) {
				wlapi_free_timer(pi->sh->physhim, pi->phycal_timer);
				pi->phycal_timer = NULL;
			}
			return NULL;
		}
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */

		if (!wlc_phy_attach_nphy(pi))
			goto err;

#if defined(AP) && defined(RADAR)
		wlc_phy_radar_attach_nphy(pi);
#endif

	} else if (ISHTPHY(pi)) {

		if (!(pi->phycal_timer = wlapi_init_timer(pi->sh->physhim,
			wlc_phy_timercb_phycal, pi, "phycal"))) {
			PHY_ERROR(("wlc_timers_init: wlapi_init_timer for phycal_timer failed\n"));
			goto err;
		}

		if (!wlc_phy_attach_htphy(pi))
			goto err;

	} else if (ISLPPHY(pi)) {
		if (!wlc_phy_attach_lpphy(pi))
			goto err;

	} else if (ISSSLPNPHY(pi)) {
		if (!wlc_phy_attach_sslpnphy(pi))
			goto err;

	} else if (ISLCNPHY(pi)) {
		if (!wlc_phy_attach_lcnphy(pi))
			goto err;

	} else	{
		/* This is here to complete the preceding if */
		PHY_ERROR(("wlc_phy_attach: unknown phytype\n"));
	}

	/* Good phy, increase refcnt and put it in list */
	pi->refcnt++;
	pi->next = pi->sh->phy_head;
	sh->phy_head = pi;

#ifdef BCMECICOEX
	si_eci_init(pi->sh->sih);
	/* notify BT that there is a single antenna */
	if (CHIPID(pi->sh->chip) == BCM4325_CHIP_ID)
		NOTIFY_BT_NUM_ANTENNA(pi->sh->sih, 0);
#endif /* BCMECICOEX */

	/* Mark that they are not longer available so we can error/assert.  Use a pointer
	 * to self as a flag.
	 */
	pi->vars = (char *)&pi->vars;

	/* Make a public copy of the attach time constant phy attributes */
	bcopy(&pi->pubpi, &pi->pubpi_ro, sizeof(wlc_phy_t));

	return &pi->pubpi_ro;

err:
#ifdef WLNOKIA_NVMEM
	if (pi && pi->noknvmem)
		wlc_phy_noknvmem_detach(sh->osh, pi->noknvmem);
#endif 
	if (pi)
		MFREE(sh->osh, pi, sizeof(phy_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_phy_detach)(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;

	PHY_TRACE(("wl: %s: pi = %p\n", __FUNCTION__, pi));

	if (pih) {
		if (--pi->refcnt) {
			return;
		}

		if (ISABGPHY(pi))
			wlc_phy_detach_abgphy(pi);

		if (pi->phycal_timer) {
			wlapi_free_timer(pi->sh->physhim, pi->phycal_timer);
			pi->phycal_timer = NULL;
		}
#ifdef WLNOKIA_NVMEM
		if (pi->noknvmem)
			wlc_phy_noknvmem_detach(pi->sh->osh, pi->noknvmem);
#endif /* WLNOKIA_NVMEM */
#if defined(PHYCAL_CACHING) || defined(WLMCHAN)
		pi->calcache_on = FALSE;
		wlc_phy_cal_cache_deinit((wlc_phy_t *)pi);
#endif

#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
		if (pi->percal_task) {
			wlapi_free_delayed_task(pi->sh->physhim, pi->percal_task);			
			pi->percal_task = NULL;
		}
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */
		/* Quick-n-dirty remove from list */
		if (pi->sh->phy_head == pi)
			pi->sh->phy_head = pi->next;
		else if (pi->sh->phy_head->next == pi)
			pi->sh->phy_head->next = NULL;
		else
			ASSERT(0);

		if (pi->pi_fptr.detach)
			(pi->pi_fptr.detach)(pi);

		MFREE(pi->sh->osh, pi, sizeof(phy_info_t));
	}
}

bool
wlc_phy_get_phyversion(wlc_phy_t *pih, uint16 *phytype, uint16 *phyrev,
	uint16 *radioid, uint16 *radiover)
{
	phy_info_t *pi = (phy_info_t *)pih;
	*phytype = (uint16)pi->pubpi.phy_type;
	*phyrev = (uint16)pi->pubpi.phy_rev;
	*radioid = pi->pubpi.radioid;
	*radiover = pi->pubpi.radiorev;

	return TRUE;
}

bool
wlc_phy_get_encore(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;
	return pi->pubpi.abgphy_encore;
}

uint32
wlc_phy_get_coreflags(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;
	return pi->pubpi.coreflags;
}

/* Break a lengthy algorithm into smaller pieces using 0-length timer */
static void
wlc_phy_timercb_phycal(void *arg)
{
	phy_info_t *pi = (phy_info_t*)arg;
	uint delay = 5;
#ifdef WLMCHAN
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
#endif

	if (PHY_PERICAL_MPHASE_PENDING(pi)) {

		/* PHY_CAL(("wlc_phy_timercb_phycal: phase_id %d\n", pi->mphase_cal_phase_id)); */

		if (!pi->sh->up) {
			wlc_phy_cal_perical_mphase_reset(pi);
			return;
		}

		if (SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi)) {
			/* delay percal until scan completed */
			PHY_CAL(("wlc_phy_timercb_phycal: scan in progress, delay 1 sec\n"));
			delay = 1000;	/* delay 1 sec */
			/* PHYCAL_CACHING does not interact with mphase */
#ifdef WLMCHAN
			if (!ctx)
#endif
				wlc_phy_cal_perical_mphase_restart(pi);
		} else {
			if (ISNPHY(pi)) {
				wlc_phy_cal_perical_nphy_run(pi, PHY_PERICAL_AUTO);
			} else if (ISHTPHY(pi)) {
				/* pick up the search type from what the scheduler requested 
				 * (INITIAL or INCREMENTAL) and call the calibration
				 */
				wlc_phy_cals_htphy(pi, pi->cal_info->cal_searchmode);
			} else {
				ASSERT(0); /* other phys not expected here */
			}
		}
#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
		if (ISNPHY(pi)) {
			if ( pi->enable_percal_delay ) {
				wlapi_schedule_delayed_task(pi->percal_task, delay);
			}
		}
		else
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */		

		wlapi_add_timer(pi->sh->physhim, pi->phycal_timer, delay, 0);
		return;
	}

	PHY_CAL(("wlc_phy_timercb_phycal: mphase phycal is done\n"));
}

void
wlc_phy_anacore(wlc_phy_t *pih, bool on)
{
	phy_info_t *pi = (phy_info_t*)pih;

	PHY_TRACE(("wl%d: %s %i\n", pi->sh->unit, __FUNCTION__, on));

	if (ISNPHY(pi)) {
		if (on) {
			if (NREV_GE(pi->pubpi.phy_rev, 3)) {
				write_phy_reg(pi, NPHY_AfectrlCore1,     0x0d);
				write_phy_reg(pi, NPHY_AfectrlOverride1, 0x0);
				write_phy_reg(pi, NPHY_AfectrlCore2,     0x0d);
				write_phy_reg(pi, NPHY_AfectrlOverride2, 0x0);
			} else {
				write_phy_reg(pi, NPHY_AfectrlOverride, 0x0);
			}
		} else {
			if (NREV_GE(pi->pubpi.phy_rev, 3)) {
				write_phy_reg(pi, NPHY_AfectrlOverride1, 0x07ff);
				write_phy_reg(pi, NPHY_AfectrlCore1,     0x0fd);
				write_phy_reg(pi, NPHY_AfectrlOverride2, 0x07ff);
				write_phy_reg(pi, NPHY_AfectrlCore2,     0x0fd);
			} else {
				write_phy_reg(pi, NPHY_AfectrlOverride, 0x7fff);
			}
		}
	} else if (ISHTPHY(pi)) {
		wlc_phy_anacore_htphy(pi, on);
	} else if (ISLPPHY(pi)) {
		if (on)
			and_phy_reg(pi, LPPHY_AfeCtrlOvr,
				~(LPPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK));
		else {
			or_phy_reg(pi, LPPHY_AfeCtrlOvrVal,
				LPPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK |
				LPPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK |
				LPPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_MASK);
			or_phy_reg(pi, LPPHY_AfeCtrlOvr,
				LPPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK);
		}
	} else if (ISSSLPNPHY(pi))  {
		if (on) {
			and_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
				~(SSLPNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK |
				SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK |
				SSLPNPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_MASK));
			or_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
				SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK);
		} else  {
			or_phy_reg(pi, SSLPNPHY_AfeCtrlOvrVal,
				SSLPNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK |
				SSLPNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK |
				SSLPNPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_MASK);
			or_phy_reg(pi, SSLPNPHY_AfeCtrlOvr,
				SSLPNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK);
		}
	} else if (ISLCNPHY(pi))  {
		if (on) {
			and_phy_reg(pi, LPPHY_AfeCtrlOvr,
				~(LPPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				LPPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK));
		} else  {
			or_phy_reg(pi, LCNPHY_AfeCtrlOvrVal,
				LCNPHY_AfeCtrlOvrVal_pwdn_adc_ovr_val_MASK |
				LCNPHY_AfeCtrlOvrVal_pwdn_dac_ovr_val_MASK |
				LCNPHY_AfeCtrlOvrVal_pwdn_rssi_ovr_val_MASK);
			or_phy_reg(pi, LCNPHY_AfeCtrlOvr,
				LCNPHY_AfeCtrlOvr_pwdn_adc_ovr_MASK |
				LCNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				LCNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK);
		}
	} else {
		if (on)
			W_REG(pi->sh->osh, &pi->regs->phyanacore, 0x0);
		else
			W_REG(pi->sh->osh, &pi->regs->phyanacore, 0xF4);
	}
}

void
wlc_phy_txcore_temp(phy_info_t *pi)
{
	pi->txcore_temp.disable_temp = (uint8)PHY_GETINTVAR(pi, "tempthresh");
	if ((pi->txcore_temp.disable_temp == 0) || (pi->txcore_temp.disable_temp == 0xf)) {
		/* No values programmed in SROM so use driver default */
		pi->txcore_temp.disable_temp = PHY_CHAIN_TX_DISABLE_TEMP;
	}

	pi->txcore_temp.hysteresis = (uint8)PHY_GETINTVAR(pi, "temps_hysteresis");
	if ((pi->txcore_temp.hysteresis == 0) || (pi->txcore_temp.hysteresis == 0xf)) {
		/* No values programmed in SROM so use driver default */
		pi->txcore_temp.hysteresis = PHY_HYSTERESIS_DELTATEMP;
	}

	pi->txcore_temp.enable_temp =
		pi->txcore_temp.disable_temp - pi->txcore_temp.hysteresis;

	pi->txcore_temp.heatedup = FALSE;

	return;

}

uint32
wlc_phy_clk_bwbits(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;

	uint32 phy_bw_clkbits = 0;

	/* select the phy speed according to selected channel b/w applies to NPHY's only */
	if (pi && (ISNPHY(pi) || ISHTPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi))) {
		switch (pi->bw) {
			case WL_CHANSPEC_BW_10:
				phy_bw_clkbits = SICF_BW10;
				break;
			case WL_CHANSPEC_BW_20:
				phy_bw_clkbits = SICF_BW20;
				break;
			case WL_CHANSPEC_BW_40:
				phy_bw_clkbits = SICF_BW40;
				break;
			default:
				ASSERT(0); /* should never get here */
				break;
		}
	}

	return phy_bw_clkbits;
}

void
WLBANDINITFN(wlc_phy_por_inform)(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	pi->phy_init_por = TRUE;
}

void
wlc_phy_btclock_war(wlc_phy_t *ppi, bool enable)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	pi->btclock_tune = enable;
}

void
wlc_phy_preamble_override_set(wlc_phy_t *ppi, int8 val)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	pi->n_preamble_override = val;
}

int8
wlc_phy_preamble_override_get(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	return pi->n_preamble_override;
}

/* increase the threshold to avoid false edcrs detection, non-11n only */
void
wlc_phy_edcrs_lock(wlc_phy_t *pih, bool lock)
{
	phy_info_t *pi = (phy_info_t *)pih;

	pi->edcrs_threshold_lock = lock;

	/* assertion: -59dB, deassertion: -67dB */
	write_phy_reg(pi, NPHY_ed_crs20UAssertThresh0, 0x46b);
	write_phy_reg(pi, NPHY_ed_crs20UAssertThresh1, 0x46b);
	write_phy_reg(pi, NPHY_ed_crs20UDeassertThresh0, 0x3c0);
	write_phy_reg(pi, NPHY_ed_crs20UDeassertThresh1, 0x3c0);
}

void
wlc_phy_initcal_enable(wlc_phy_t *pih, bool initcal)
{
	phy_info_t *pi = (phy_info_t *)pih;

	pi->do_initcal = initcal;
}

void
wlc_phy_hw_clk_state_upd(wlc_phy_t *pih, bool newstate)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (!pi || !pi->sh)
		return;

	pi->sh->clk = newstate;
}

void
wlc_phy_hw_state_upd(wlc_phy_t *pih, bool newstate)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (!pi || !pi->sh)
		return;

	pi->sh->up = newstate;
}

void
WLBANDINITFN(wlc_phy_init)(wlc_phy_t *pih, chanspec_t chanspec)
{
	uint32	mc;
	initfn_t phy_init = NULL;
	phy_info_t *pi = (phy_info_t *)pih;
#ifdef BCMDBG
	char chbuf[CHANSPEC_STR_LEN];
#endif

	PHY_TRACE(("wl%d: %s chanspec %s\n", pi->sh->unit, __FUNCTION__,
		wf_chspec_ntoa(chanspec, chbuf)));

	/* skip if this function is called recursively(e.g. when bw is changed) */
	if (pi->init_in_progress)
		return;

	pi->init_in_progress = TRUE;

	pi->radio_chanspec = chanspec;

	mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);
	if ((mc & MCTL_EN_MAC) != 0) {
		if (mc == 0xffffffff)
			PHY_ERROR(("wl%d: wlc_phy_init: chip is dead !!!\n", pi->sh->unit));
		else
			PHY_ERROR(("wl%d: wlc_phy_init:MAC running! mc=0x%x\n",
			          pi->sh->unit, mc));
		ASSERT((const char*)"wlc_phy_init: Called with the MAC running!" == NULL);
	}

	ASSERT(pi != NULL);

	/* clear during init. To be set by higher level wlc code */
	pi->cur_interference_mode = INTERFERE_NONE;

	/* init PUB_NOT_ASSOC */
	if (!(pi->measure_hold & PHY_HOLD_FOR_SCAN) &&
		!(pi->interf.aci.nphy.detection_in_progress)) {
		pi->measure_hold |= PHY_HOLD_FOR_NOT_ASSOC;
	}

	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	phy_init = pi->pi_fptr.init;

	if (phy_init == NULL) {
		PHY_ERROR(("wl%d: %s: No phy_init found for phy_type %d, rev %d\n",
		          pi->sh->unit, __FUNCTION__, pi->pubpi.phy_type, pi->pubpi.phy_rev));
		ASSERT(phy_init != NULL);
		return;
	}

	wlc_phy_anacore(pih, ON);

	/* sanitize bw here to avoid later mess. wlc_set_bw will invoke phy_reset,
	 *  but phy_init recursion is avoided by using init_in_progress
	 */
	if (CHSPEC_BW(pi->radio_chanspec) != pi->bw)
		wlapi_bmac_bw_set(pi->sh->physhim, CHSPEC_BW(pi->radio_chanspec));

	/* Reset gain_boost & aci on band-change */
	pi->nphy_gain_boost = TRUE;
	if (ISNPHY(pi)) {
		if (NREV_LT(pi->pubpi.phy_rev, 3)) {
			/* when scanning to different band, don't change aci_state */
			/* but keep phy rev < 3 the same as before */
			pi->aci_state &= ~ACI_ACTIVE;
		}
		/* Reset ACI internals if not scanning and not in aci_detection */
		if (!(SCAN_INPROG_PHY(pi) ||
		pi->interf.aci.nphy.detection_in_progress)) {
			wlc_phy_aci_sw_reset_nphy(pi);
		}
	}

	/* radio on */
	wlc_phy_switch_radio((wlc_phy_t*)pi, ON);

	/* !! kick off the phy init !! */
	(*phy_init)(pi);

	/* Indicate a power on reset isn't needed for future phy init's */
	pi->phy_init_por = FALSE;

	if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))
		wlc_phy_do_dummy_tx(pi, TRUE, OFF);

	/* Save the w/b frequency tracking registers */
	if (ISGPHY(pi)) {
		pi->freqtrack_saved_regs[0] = read_phy_reg(pi, BPHY_COEFFS);
		pi->freqtrack_saved_regs[1] = read_phy_reg(pi, BPHY_STEP);
	}

	/* init txpwr shared memory if sw or ucode are doing tx power control */
	if (!(ISNPHY(pi) || ISHTPHY(pi) || ISSSLPNPHY(pi)))
		wlc_phy_txpower_update_shm(pi);

	/* initialize rx antenna diversity */
	wlc_phy_ant_rxdiv_set((wlc_phy_t *)pi, pi->sh->rx_antdiv);


	/* initialize interference algorithms */
	if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3) && (SCAN_INPROG_PHY(pi))) {
		/* do not reinitialize interference mode, could be scanning */
	} else if (ISHTPHY(pi)) {
	} else {
		wlc_phy_interference(pi, pi->sh->interference_mode, FALSE);
	}

	pi->init_in_progress = FALSE;
}

/*
 * Do one-time phy initializations and calibration.
 *
 * Note: no register accesses allowed; we have not yet waited for PLL
 * since the last corereset.
 */
void
BCMINITFN(wlc_phy_cal_init)(wlc_phy_t *pih)
{
	int i;
	phy_info_t *pi = (phy_info_t *)pih;
	initfn_t cal_init = NULL;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT((R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC) == 0);

	if (ISAPHY(pi))
		pi->txpwridx = DEFAULT_11A_TXP_IDX;


	if (!pi->initialized) {
		/* glitch counter init */
		/* detection is called only if high glitches are observed */
		pi->interf.aci.glitch_ma = ACI_INIT_MA;
		pi->interf.aci.glitch_ma_previous = ACI_INIT_MA;
		pi->interf.aci.pre_glitch_cnt = 0;
		pi->interf.aci.ma_total = MA_WINDOW_SZ * ACI_INIT_MA;
		for (i = 0; i < MA_WINDOW_SZ; i++)
			pi->interf.aci.ma_list[i] = ACI_INIT_MA;

		for (i = 0; i < PHY_NOISE_MA_WINDOW_SZ; i++) {
			pi->interf.noise.ofdm_glitch_ma_list[i] = PHY_NOISE_GLITCH_INIT_MA;
			pi->interf.noise.ofdm_badplcp_ma_list[i] = PHY_NOISE_GLITCH_INIT_MA_BADPlCP;
		}

		pi->interf.noise.ofdm_glitch_ma = PHY_NOISE_GLITCH_INIT_MA;
		pi->interf.noise.ofdm_glitch_ma_previous = PHY_NOISE_GLITCH_INIT_MA;
		pi->interf.noise.bphy_pre_glitch_cnt = 0;
		pi->interf.noise.ofdm_ma_total = PHY_NOISE_GLITCH_INIT_MA * PHY_NOISE_MA_WINDOW_SZ;

		pi->interf.badplcp_ma = PHY_NOISE_GLITCH_INIT_MA_BADPlCP;
		pi->interf.badplcp_ma_previous = PHY_NOISE_GLITCH_INIT_MA_BADPlCP;
		pi->interf.badplcp_ma_total = PHY_NOISE_GLITCH_INIT_MA_BADPlCP * MA_WINDOW_SZ;
		pi->interf.pre_badplcp_cnt = 0;
		pi->interf.bphy_pre_badplcp_cnt = 0;

		pi->interf.noise.ofdm_badplcp_ma = PHY_NOISE_GLITCH_INIT_MA_BADPlCP;
		pi->interf.noise.ofdm_badplcp_ma_previous = PHY_NOISE_GLITCH_INIT_MA_BADPlCP;
		pi->interf.noise.ofdm_badplcp_ma_total =
			PHY_NOISE_GLITCH_INIT_MA_BADPlCP * PHY_NOISE_MA_WINDOW_SZ;

		cal_init = pi->pi_fptr.calinit;
		if (cal_init)
			(*cal_init)(pi);

		pi->initialized = TRUE;
	}
}


int
BCMUNINITFN(wlc_phy_down)(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;
	int callbacks = 0;


	/* all activate phytest should have been stopped */
	ASSERT(pi->phytest_on == FALSE);

	/* cancel phycal timer if exists */
	if (pi->phycal_timer && !wlapi_del_timer(pi->sh->physhim, pi->phycal_timer))
		callbacks++;

#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	if (pi->enable_percal_delay && !wlapi_cancel_delayed_task(pi->percal_task) )
		callbacks++;
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */

	pi->nphy_iqcal_chanspec_2G = 0;
	pi->nphy_iqcal_chanspec_5G = 0;

	return callbacks;
}

static uint32
wlc_phy_get_radio_ver(phy_info_t *pi)
{
	uint32 ver;

	ver = read_radio_id(pi);

	PHY_INFORM(("wl%d: %s: IDCODE = 0x%x\n", pi->sh->unit, __FUNCTION__, ver));
	return ver;
}



/* Do the initial table address write given the phy specific table access register
 * locations, the table ID and offset to start read/write operations
 */
void
wlc_phy_table_addr(phy_info_t *pi, uint tbl_id, uint tbl_offset,
                   uint16 tblAddr, uint16 tblDataHi, uint16 tblDataLo)
{
	PHY_TRACE(("wl%d: %s ID %u offset %u\n", pi->sh->unit, __FUNCTION__, tbl_id, tbl_offset));

	write_phy_reg(pi, tblAddr, (tbl_id << 10) | tbl_offset);

	pi->tbl_data_hi = tblDataHi;
	pi->tbl_data_lo = tblDataLo;

	if ((CHIPID(pi->sh->chip) == BCM43224_CHIP_ID ||
	     CHIPID(pi->sh->chip) == BCM43421_CHIP_ID) &&
	    (pi->sh->chiprev == 1)) {
		pi->tbl_addr = tblAddr;
		pi->tbl_save_id = tbl_id;
		pi->tbl_save_offset = tbl_offset;
	}
}

/* Write the given value to the phy table which has been set up with an earlier call
 * to wlc_phy_table_addr()
 */
void
wlc_phy_table_data_write(phy_info_t *pi, uint width, uint32 val)
{
	ASSERT((width == 8) || (width == 16) || (width == 32));

	PHY_TRACE(("wl%d: %s width %u val %u\n", pi->sh->unit, __FUNCTION__, width, val));

	if ((CHIPID(pi->sh->chip) == BCM43224_CHIP_ID ||
	     CHIPID(pi->sh->chip) == BCM43421_CHIP_ID) &&
	    (pi->sh->chiprev == 1) &&
	    (pi->tbl_save_id == NPHY_TBL_ID_ANTSWCTRLLUT)) {
		read_phy_reg(pi, pi->tbl_data_lo);
		/* roll back the address from the dummy read */
		write_phy_reg(pi, pi->tbl_addr, (pi->tbl_save_id << 10) | pi->tbl_save_offset);
		pi->tbl_save_offset++;
	}

	if (width == 32) {
		/* width is 32-bit */
		write_phy_reg(pi, pi->tbl_data_hi, (uint16)(val >> 16));
		write_phy_reg(pi, pi->tbl_data_lo, (uint16)val);
	} else {
		/* width is 16-bit or 8-bit */
		write_phy_reg(pi, pi->tbl_data_lo, (uint16)val);
	}
}

/* Takes the table name, list of entries, offset to load the table,
 * see xxxphyprocs.tcl, proc xxxphy_write_table
 */
void
wlc_phy_write_table(phy_info_t *pi, const phytbl_info_t *ptbl_info, uint16 tblAddr,
	uint16 tblDataHi, uint16 tblDataLo)
{
	uint    idx;
	uint    tbl_id     = ptbl_info->tbl_id;
	uint    tbl_offset = ptbl_info->tbl_offset;
	uint	tbl_width = ptbl_info->tbl_width;
	const uint8  *ptbl_8b    = (const uint8  *)ptbl_info->tbl_ptr;
	const uint16 *ptbl_16b   = (const uint16 *)ptbl_info->tbl_ptr;
	const uint32 *ptbl_32b   = (const uint32 *)ptbl_info->tbl_ptr;

	ASSERT((tbl_width == 8) || (tbl_width == 16) ||
		(tbl_width == 32));

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	write_phy_reg(pi, tblAddr, (tbl_id << 10) | tbl_offset);

	for (idx = 0; idx < ptbl_info->tbl_len; idx++) {

		if ((CHIPID(pi->sh->chip) == BCM43224_CHIP_ID ||
		     CHIPID(pi->sh->chip) == BCM43421_CHIP_ID) &&
		    (pi->sh->chiprev == 1) &&
		    (tbl_id == NPHY_TBL_ID_ANTSWCTRLLUT)) {
			read_phy_reg(pi, tblDataLo);
			/* roll back the address from the dummy read */
			write_phy_reg(pi, tblAddr, (tbl_id << 10) | (tbl_offset + idx));
		}

		if (tbl_width == 32) {
			/* width is 32-bit */
			write_phy_reg(pi, tblDataHi, (uint16)(ptbl_32b[idx] >> 16));
			write_phy_reg(pi, tblDataLo, (uint16)ptbl_32b[idx]);
		} else if (tbl_width == 16) {
			/* width is 16-bit */
			write_phy_reg(pi, tblDataLo, ptbl_16b[idx]);
		} else {
			/* width is 8-bit */
			write_phy_reg(pi, tblDataLo, ptbl_8b[idx]);
		}
	}
}

void
wlc_phy_read_table(phy_info_t *pi, const phytbl_info_t *ptbl_info, uint16 tblAddr,
	uint16 tblDataHi, uint16 tblDataLo)
{
	uint    idx;
	uint    tbl_id     = ptbl_info->tbl_id;
	uint    tbl_offset = ptbl_info->tbl_offset;
	uint	tbl_width = ptbl_info->tbl_width;
	uint8  *ptbl_8b    = (uint8  *)(uintptr)ptbl_info->tbl_ptr;
	uint16 *ptbl_16b   = (uint16 *)(uintptr)ptbl_info->tbl_ptr;
	uint32 *ptbl_32b   = (uint32 *)(uintptr)ptbl_info->tbl_ptr;

	ASSERT((tbl_width == 8) || (tbl_width == 16) ||
		(tbl_width == 32));

	write_phy_reg(pi, tblAddr, (tbl_id << 10) | tbl_offset);

	for (idx = 0; idx < ptbl_info->tbl_len; idx++) {

		if ((CHIPID(pi->sh->chip) == BCM43224_CHIP_ID ||
		     CHIPID(pi->sh->chip) == BCM43421_CHIP_ID) &&
		    (pi->sh->chiprev == 1)) {
			(void)read_phy_reg(pi, tblDataLo);
			/* roll back the address from the dummy read */
			write_phy_reg(pi, tblAddr, (tbl_id << 10) | (tbl_offset + idx));
		}

		if (tbl_width == 32) {
			/* width is 32-bit */
			ptbl_32b[idx]  =  read_phy_reg(pi, tblDataLo);
			ptbl_32b[idx] |= (read_phy_reg(pi, tblDataHi) << 16);
		} else if (tbl_width == 16) {
			/* width is 16-bit */
			ptbl_16b[idx]  =  read_phy_reg(pi, tblDataLo);
		} else {
			/* width is 8-bit */
			ptbl_8b[idx]   =  (uint8)read_phy_reg(pi, tblDataLo);
		}
	}
}

/* Takes the table name, list of entries, offset to load the table,
 * see xxxphyprocs.tcl, proc xxxphy_write_table
 */
void
wlc_phy_table_write_lpphy(phy_info_t *pi, const lpphytbl_info_t *ptbl_info)
{
	uint    idx;
	uint    tbl_id     = ptbl_info->tbl_id;
	uint    tbl_offset = ptbl_info->tbl_offset;
	uint32	u32temp;

	const uint8  *ptbl_8b    = (const uint8  *)ptbl_info->tbl_ptr;
	const uint16 *ptbl_16b   = (const uint16 *)ptbl_info->tbl_ptr;
	const uint32 *ptbl_32b   = (const uint32 *)ptbl_info->tbl_ptr;

	uint16 tblAddr = LPPHY_TableAddress;
	uint16 tblDataHi = LPPHY_TabledataHi;
	uint16 tblDatalo = LPPHY_TabledataLo;

	ASSERT((ptbl_info->tbl_phywidth == 8) || (ptbl_info->tbl_phywidth == 16) ||
		(ptbl_info->tbl_phywidth == 32));
	ASSERT((ptbl_info->tbl_width == 8) || (ptbl_info->tbl_width == 16) ||
		(ptbl_info->tbl_width == 32));

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_phy_table_lock_lpphy(pi);

	write_phy_reg(pi, tblAddr, (tbl_id << 10) | tbl_offset);

	for (idx = 0; idx < ptbl_info->tbl_len; idx++) {

		/* get the element from the table according to the table width */
		if (ptbl_info->tbl_width == 32) {
			/* tbl width is 32 bit */
			u32temp = (uint32)ptbl_32b[idx];
		} else if (ptbl_info->tbl_width == 16) {
			/* tbl width is 16 bit */
			u32temp = (uint32)ptbl_16b[idx];
		} else {
			/* tbl width is 8 bit */
			u32temp = (uint32)ptbl_8b[idx];
		}

		/* write the element into the phy table according phy address space width */
		if (ptbl_info->tbl_phywidth == 32) {
			/* phy width is 32-bit */
			write_phy_reg(pi, tblDataHi, (u32temp >> 16) & 0xffff);
			write_phy_reg(pi, tblDatalo, u32temp & 0xffff);
		} else if (ptbl_info->tbl_phywidth == 16) {
			/* phy width is 16-bit */
			write_phy_reg(pi, tblDatalo, u32temp & 0xffff);
		} else {
			/* phy width is 8-bit */
			write_phy_reg(pi, tblDatalo, u32temp & 0xffff);
		}
	}
	wlc_phy_table_unlock_lpphy(pi);
}

void
wlc_phy_table_read_lpphy(phy_info_t *pi, const lpphytbl_info_t *ptbl_info)
{
	uint    idx;
	uint    tbl_id     = ptbl_info->tbl_id;
	uint    tbl_offset = ptbl_info->tbl_offset;
	uint32  u32temp;

	uint8  *ptbl_8b    = (uint8  *)(uintptr)ptbl_info->tbl_ptr;
	uint16 *ptbl_16b   = (uint16 *)(uintptr)ptbl_info->tbl_ptr;
	uint32 *ptbl_32b   = (uint32 *)(uintptr)ptbl_info->tbl_ptr;

	uint16 tblAddr = LPPHY_TableAddress;
	uint16 tblDataHi = LPPHY_TabledataHi;
	uint16 tblDatalo = LPPHY_TabledataLo;

	ASSERT((ptbl_info->tbl_phywidth == 8) || (ptbl_info->tbl_phywidth == 16) ||
		(ptbl_info->tbl_phywidth == 32));
	ASSERT((ptbl_info->tbl_width == 8) || (ptbl_info->tbl_width == 16) ||
		(ptbl_info->tbl_width == 32));

	wlc_phy_table_lock_lpphy(pi);

	write_phy_reg(pi, tblAddr, (tbl_id << 10) | tbl_offset);

	for (idx = 0; idx < ptbl_info->tbl_len; idx++) {

		/* get the element from phy according to the phy table element
		 * address space width
		 */
		if (ptbl_info->tbl_phywidth == 32) {
			/* phy width is 32-bit */
			u32temp  =  read_phy_reg(pi, tblDatalo);
			u32temp |= (read_phy_reg(pi, tblDataHi) << 16);
		} else if (ptbl_info->tbl_phywidth == 16) {
			/* phy width is 16-bit */
			u32temp  =  read_phy_reg(pi, tblDatalo);
		} else {
			/* phy width is 8-bit */
			u32temp   =  (uint8)read_phy_reg(pi, tblDatalo);
		}

		/* put the element into the table according to the table element width
		 * Note that phy table width is some times more than necessary while
		 * table width is always optimal.
		 */
		if (ptbl_info->tbl_width == 32) {
			/* tbl width is 32-bit */
			ptbl_32b[idx]  =  u32temp;
		} else if (ptbl_info->tbl_width == 16) {
			/* tbl width is 16-bit */
			ptbl_16b[idx]  =  (uint16)u32temp;
		} else {
			/* tbl width is 8-bit */
			ptbl_8b[idx]   =  (uint8)u32temp;
		}
	}
	wlc_phy_table_unlock_lpphy(pi);
}

/* prevent simultaneous phy table access by driver and ucode */
void
wlc_phy_table_lock_lpphy(phy_info_t *pi)
{
	uint32 mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);

	if (mc & MCTL_EN_MAC) {
		wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  MCTL_PHYLOCK);
		(void)R_REG(pi->sh->osh, &pi->regs->maccontrol);
		OSL_DELAY(5);
	}
}

void
wlc_phy_table_unlock_lpphy(phy_info_t *pi)
{
	wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  0);
}

uint
wlc_phy_init_radio_regs_allbands(phy_info_t *pi, radio_20xx_regs_t *radioregs)
{
	uint i = 0;

	do {
		if (radioregs[i].do_init) {
			write_radio_reg(pi, radioregs[i].address,
			                (uint16)radioregs[i].init);
		}

		i++;
	} while (radioregs[i].address != 0xffff);

	return i;
}

uint
wlc_phy_init_radio_regs(phy_info_t *pi, radio_regs_t *radioregs, uint16 core_offset)
{
	uint i = 0;
	uint count = 0;

	do {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			if (radioregs[i].do_init_a) {
				write_radio_reg(pi, radioregs[i].address | core_offset,
					(uint16)radioregs[i].init_a);
				if (ISNPHY(pi) && (++count % 4 == 0))
					WLC_PHY_WAR_PR51571(pi);
			}
		} else {
			if (radioregs[i].do_init_g) {
				write_radio_reg(pi, radioregs[i].address | core_offset,
					(uint16)radioregs[i].init_g);
				if (ISNPHY(pi) && (++count % 4 == 0))
					WLC_PHY_WAR_PR51571(pi);
			}
		}

		i++;
	} while (radioregs[i].address != 0xffff);

	return i;
}

void
wlc_phy_do_dummy_tx(phy_info_t *pi, bool ofdm, bool pa_on)
{
#define	DUMMY_PKT_LEN	20 /* Dummy packet's length */
	d11regs_t *regs = pi->regs;
	int	i, count;
	uint8	ofdmpkt[DUMMY_PKT_LEN] = {
		0xcc, 0x01, 0x02, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
	};
	uint8	cckpkt[DUMMY_PKT_LEN] = {
		0x6e, 0x84, 0x0b, 0x00, 0x00, 0x00, 0xd4, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
	};
	uint32 *dummypkt;

	ASSERT((R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC) == 0);

	dummypkt = (uint32 *)(ofdm ? ofdmpkt : cckpkt);
	wlapi_bmac_write_template_ram(pi->sh->physhim, 0, DUMMY_PKT_LEN, dummypkt);

	/* set up the TXE transfer */

	W_REG(pi->sh->osh, &regs->xmtsel, 0);
	/* Assign the WEP to the transmit path */
	if (D11REV_GE(pi->sh->corerev, 11))
		W_REG(pi->sh->osh, &regs->wepctl, 0x100);
	else
		W_REG(pi->sh->osh, &regs->wepctl, 0);

	/* Set/clear OFDM bit in PHY control word */
	W_REG(pi->sh->osh, &regs->txe_phyctl, (ofdm ? 1 : 0) | PHY_TXC_ANT_0);
	if (ISNPHY(pi) || ISHTPHY(pi) || ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
		ASSERT(ofdm);
		W_REG(pi->sh->osh, &regs->txe_phyctl1, 0x1A02);
	}

	W_REG(pi->sh->osh, &regs->txe_wm_0, 0);		/* No substitutions */
	W_REG(pi->sh->osh, &regs->txe_wm_1, 0);

	/* Set transmission from the TEMPLATE where we loaded the frame */
	W_REG(pi->sh->osh, &regs->xmttplatetxptr, 0);
	W_REG(pi->sh->osh, &regs->xmttxcnt, DUMMY_PKT_LEN);

	/* Set Template as source, length specified as a count and destination
	 * as Serializer also set "gen_eof"
	 */
	W_REG(pi->sh->osh, &regs->xmtsel, ((8 << 8) | (1 << 5) | (1 << 2) | 2));

	/* Instruct the MAC to not calculate FCS, we'll supply a bogus one */
	W_REG(pi->sh->osh, &regs->txe_ctl, 0);

	if (!pa_on) {
		if (ISNPHY(pi))
			wlc_phy_pa_override_nphy(pi, OFF);
		else if (ISHTPHY(pi)) {
			wlc_phy_pa_override_htphy(pi, OFF);
		}
	}

	/* Start transmission and wait until sendframe goes away */
	/* Set TX_NOW in AUX along with MK_CTLWRD */
	if (ISNPHY(pi) || ISHTPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi))
		W_REG(pi->sh->osh, &regs->txe_aux, 0xD0);
	else if (ISLPPHY(pi))
		W_REG(pi->sh->osh, &regs->txe_aux, ((1 << 6) | (1 << 4)));
	else
		W_REG(pi->sh->osh, &regs->txe_aux, ((1 << 5) | (1 << 4)));

	(void)R_REG(pi->sh->osh, &regs->txe_aux);

	/* Wait for 10 x ack time, enlarge it for vsim of QT */
	i = 0;
	count = ofdm ? 30 : 250;

#ifndef BCMQT_CPU
	if (ISSIM_ENAB(pi->sh->sih)) {
		count *= 100;
	}
#endif
	/* wait for txframe to be zero */
	while ((i++ < count) && (R_REG(pi->sh->osh, &regs->txe_status) & (1 << 7))) {
#if defined(DSLCPE_DELAY)
		OSL_YIELD_EXEC(pi->sh->osh, 10);
#else
		OSL_DELAY(10);
#endif
	}
	if (i >= count)
		PHY_ERROR(("wl%d: %s: Waited %d uS for %s txframe\n",
		          pi->sh->unit, __FUNCTION__, 10 * i, (ofdm ? "ofdm" : "cck")));

	/* Wait for the mac to finish (this is 10x what is supposed to take) */
	i = 0;
	/* wait for txemend */
	while ((i++ < 10) && ((R_REG(pi->sh->osh, &regs->txe_status) & (1 << 10)) == 0)) {
#if defined(DSLCPE_DELAY)
		OSL_YIELD_EXEC(pi->sh->osh, 10);
#else
		OSL_DELAY(10);
#endif
	}
	if (i >= 10)
		PHY_ERROR(("wl%d: %s: Waited %d uS for txemend\n",
		          pi->sh->unit, __FUNCTION__, 10 * i));

	/* Wait for the phy to finish */
	i = 0;
	/* wait for txcrs */
	while ((i++ < 10) && ((R_REG(pi->sh->osh, &regs->ifsstat) & (1 << 8)))) {
#if defined(DSLCPE_DELAY)
		OSL_YIELD_EXEC(pi->sh->osh, 10);
#else
		OSL_DELAY(10);
#endif
	}
	if (i >= 10)
		PHY_ERROR(("wl%d: %s: Waited %d uS for txcrs\n",
		          pi->sh->unit, __FUNCTION__, 10 * i));
	if (!pa_on) {
		if (ISNPHY(pi))
			wlc_phy_pa_override_nphy(pi, ON);
		else if (ISHTPHY(pi)) {
			wlc_phy_pa_override_htphy(pi, ON);
		}
	}
}

static void
wlc_phy_scanroam_cache_cal(phy_info_t *pi, bool set)
{
	if (ISHTPHY(pi))
		wlc_phy_scanroam_cache_cal_htphy(pi, set);
}

void
wlc_phy_hold_upd(wlc_phy_t *pih, mbool id, bool set)
{
	phy_info_t *pi = (phy_info_t *)pih;
	ASSERT(id);

	PHY_TRACE(("%s: id %d val %d old pi->measure_hold 0%x\n", __FUNCTION__, id, set,
		pi->measure_hold));

	PHY_CAL(("wl%d: %s: %s %s flag\n", pi->sh->unit, __FUNCTION__,
		set ? "SET" : "CLR",
		(id == PHY_HOLD_FOR_ASSOC) ? "ASSOC" :
		((id == PHY_HOLD_FOR_SCAN) ? "SCAN" :
		((id == PHY_HOLD_FOR_SCAN) ? "SCAN" :
		((id == PHY_HOLD_FOR_RM) ? "RM" :
		((id == PHY_HOLD_FOR_PLT) ? "PLT" :
		((id == PHY_HOLD_FOR_MUTE) ? "MUTE" :
		((id == PHY_HOLD_FOR_NOT_ASSOC) ? "NOT-ASSOC" : ""))))))));

	if (set) {
		mboolset(pi->measure_hold, id);
	} else {
		mboolclr(pi->measure_hold, id);
	}
	if (id & PHY_HOLD_FOR_SCAN)
		wlc_phy_scanroam_cache_cal(pi, set);
	return;
}

void
wlc_phy_mute_upd(wlc_phy_t *pih, bool mute, mbool flags)
{
	phy_info_t *pi = (phy_info_t *)pih;

	PHY_TRACE(("wlc_phy_mute_upd: flags 0x%x mute %d\n", flags, mute));

	if (mute) {
		mboolset(pi->measure_hold, PHY_HOLD_FOR_MUTE);
	} else {
		mboolclr(pi->measure_hold, PHY_HOLD_FOR_MUTE);
	}

	/* check if need to schedule a phy cal */
	if (!mute && (flags & PHY_MUTE_FOR_PREISM)) {
		pi->cal_info->last_cal_time = (pi->sh->now > pi->sh->glacial_timer) ?
			(pi->sh->now - pi->sh->glacial_timer) : 0;
	}
	return;
}

void
wlc_phy_clear_tssi(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (ISNPHY(pi) || ISHTPHY(pi)) {
		/* NPHY/HTPHY doesn't use sw or ucode powercontrol */
		return;
	} else if (ISAPHY(pi)) {
		wlapi_bmac_write_shm(pi->sh->physhim, M_A_TSSI_0, NULL_TSSI_W);
		wlapi_bmac_write_shm(pi->sh->physhim, M_A_TSSI_1, NULL_TSSI_W);
	} else {
		wlapi_bmac_write_shm(pi->sh->physhim, M_B_TSSI_0, NULL_TSSI_W);
		wlapi_bmac_write_shm(pi->sh->physhim, M_B_TSSI_1, NULL_TSSI_W);
		wlapi_bmac_write_shm(pi->sh->physhim, M_G_TSSI_0, NULL_TSSI_W);
		wlapi_bmac_write_shm(pi->sh->physhim, M_G_TSSI_1, NULL_TSSI_W);
	}
}

static bool
wlc_phy_cal_txpower_recalc_sw(phy_info_t *pi)
{
	bool ret = TRUE;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* NPHY/HTPHY doesn't ever use SW power control */
	if (ISNPHY(pi) || ISHTPHY(pi))
		return FALSE;

	if (ISSSLPNPHY(pi))
		return FALSE;
	if (ISLCNPHY(pi))
		return FALSE;

	if (ISLPPHY(pi)) {
		/* Adjust number of packets for TSSI averaging */
		if (wlc_phy_tpc_isenabled_lpphy(pi)) {
			wlc_phy_tx_pwr_update_npt_lpphy(pi);
		}
		return ret;
	}


	/* No need to do anything if the hw is doing pwrctrl for us */
	if (pi->hwpwrctrl) {

		/* Do nothing if radio pwr is being overridden */
		if (pi->radiopwr_override != RADIOPWR_OVERRIDE_DEF)
			return (ret);

		pi->hwpwr_txcur = wlapi_bmac_read_shm(pi->sh->physhim, M_TXPWR_CUR);
		return (ret);
	} else {
		if (ISABGPHY(pi))
			ret = wlc_phy_cal_txpower_recalc_sw_abgphy(pi);
	}

	return (ret);
}

void
wlc_phy_switch_radio(wlc_phy_t *pih, bool on)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	{
		uint mc;

		mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);
		if (mc & MCTL_EN_MAC) {
			PHY_ERROR(("wl%d: %s: maccontrol 0x%x has EN_MAC set\n",
			          pi->sh->unit, __FUNCTION__, mc));
		}
	}

#ifdef BCMECICOEX	    /* Allow BT on all channels if radio is off */
	if (BCMECISECICOEX_ENAB_PHY(pi) && !on) {
		NOTIFY_BT_CHL(pi->sh->sih, 0);
	}
#endif

	if (ISNPHY(pi)) {
		wlc_phy_switch_radio_nphy(pi, on);

	} else if (ISHTPHY(pi)) {
		wlc_phy_switch_radio_htphy(pi, on);

	} else if (ISLPPHY(pi)) {
		if (on) {
			if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
				and_phy_reg(pi, LPPHY_REV2_RFOverride0,
					~(LPPHY_REV2_RFOverride0_rfpll_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_wrssi_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_nrssi_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_internalrfrxpu_ovr_MASK 	|
					LPPHY_REV2_RFOverride0_internalrftxpu_ovr_MASK));
				and_phy_reg(pi, LPPHY_REV2_rfoverride2,
					~(LPPHY_REV2_rfoverride2_lna_pu_ovr_MASK |
					LPPHY_REV2_rfoverride2_slna_pu_ovr_MASK));
				and_phy_reg(pi, LPPHY_REV2_rfoverride3,
					~LPPHY_REV2_rfoverride3_rfactive_ovr_MASK);
			} else {
				and_phy_reg(pi, LPPHY_RFOverride0,
					~(LPPHY_RFOverride0_rfpll_pu_ovr_MASK 		|
					LPPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
					LPPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
					LPPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
					LPPHY_RFOverride0_internalrftxpu_ovr_MASK));
				and_phy_reg(pi, LPPHY_rfoverride2,
					~(LPPHY_rfoverride2_lna1_pu_ovr_MASK |
					LPPHY_rfoverride2_lna2_pu_ovr_MASK));
			}
		} else {
			if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
				and_phy_reg(pi,  LPPHY_REV2_RFOverrideVal0,
					~(LPPHY_REV2_RFOverrideVal0_rfpll_pu_ovr_val_MASK |
					LPPHY_REV2_RFOverrideVal0_wrssi_pu_ovr_val_MASK 	|
					LPPHY_REV2_RFOverrideVal0_nrssi_pu_ovr_val_MASK 	|
					LPPHY_REV2_RFOverrideVal0_internalrfrxpu_ovr_val_MASK 	|
					LPPHY_REV2_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
				or_phy_reg(pi, LPPHY_RFOverride0,
					LPPHY_REV2_RFOverride0_rfpll_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_wrssi_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_nrssi_pu_ovr_MASK 		|
					LPPHY_REV2_RFOverride0_internalrfrxpu_ovr_MASK 	|
					LPPHY_REV2_RFOverride0_internalrftxpu_ovr_MASK);

				and_phy_reg(pi, LPPHY_REV2_rxlnaandgainctrl1ovrval,
					~(LPPHY_REV2_rxlnaandgainctrl1ovrval_lnapuovr_Val_MASK));
				and_phy_reg(pi, LPPHY_REV2_rfoverride2val,
					~(LPPHY_REV2_rfoverride2val_slna_pu_ovr_val_MASK));
				or_phy_reg(pi, LPPHY_REV2_rfoverride2,
					LPPHY_REV2_rfoverride2_lna_pu_ovr_MASK |
					LPPHY_REV2_rfoverride2_slna_pu_ovr_MASK);
				and_phy_reg(pi, LPPHY_REV2_rfoverride3_val,
					~(LPPHY_REV2_rfoverride3_val_rfactive_ovr_val_MASK));
				or_phy_reg(pi,  LPPHY_REV2_rfoverride3,
					LPPHY_REV2_rfoverride3_rfactive_ovr_MASK);
			} else {
				and_phy_reg(pi,  LPPHY_RFOverrideVal0,
					~(LPPHY_RFOverrideVal0_rfpll_pu_ovr_val_MASK 		|
					LPPHY_RFOverrideVal0_wrssi_pu_ovr_val_MASK 		|
					LPPHY_RFOverrideVal0_nrssi_pu_ovr_val_MASK 		|
					LPPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK 	|
					LPPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
				or_phy_reg(pi, LPPHY_RFOverride0,
					LPPHY_RFOverride0_rfpll_pu_ovr_MASK 			|
					LPPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
					LPPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
					LPPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
					LPPHY_RFOverride0_internalrftxpu_ovr_MASK);

				and_phy_reg(pi, LPPHY_rfoverride2val,
					~(LPPHY_rfoverride2val_lna1_pu_ovr_val_MASK |
					LPPHY_rfoverride2val_lna2_pu_ovr_val_MASK));
				or_phy_reg(pi, LPPHY_rfoverride2,
					LPPHY_rfoverride2_lna1_pu_ovr_MASK |
					LPPHY_rfoverride2_lna2_pu_ovr_MASK);
			}
		}
	} else if (ISSSLPNPHY(pi)) {
		if (on) {
			and_phy_reg(pi, SSLPNPHY_RFOverride0,
				~(SSLPNPHY_RFOverride0_rfpll_pu_ovr_MASK	|
				SSLPNPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
				SSLPNPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
				SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
				SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK));
			and_phy_reg(pi, SSLPNPHY_rfoverride2,
				~(SSLPNPHY_rfoverride2_lna_pu_ovr_MASK |
				SSLPNPHY_rfoverride2_slna_pu_ovr_MASK));
			if (BCMECICOEX_ENAB_PHY(pi))
				and_phy_reg(pi, SSLPNPHY_rfoverride3,
					~SSLPNPHY_rfoverride3_rfactive_ovr_MASK);
		} else {
			and_phy_reg(pi,  SSLPNPHY_RFOverrideVal0,
				~(SSLPNPHY_RFOverrideVal0_rfpll_pu_ovr_val_MASK |
				SSLPNPHY_RFOverrideVal0_wrssi_pu_ovr_val_MASK 	|
				SSLPNPHY_RFOverrideVal0_nrssi_pu_ovr_val_MASK 	|
				SSLPNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK 	|
				SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
			or_phy_reg(pi, SSLPNPHY_RFOverride0,
				SSLPNPHY_RFOverride0_rfpll_pu_ovr_MASK 		|
				SSLPNPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
				SSLPNPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
				SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
				SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK);

			and_phy_reg(pi, SSLPNPHY_rxlnaandgainctrl1ovrval,
				~(SSLPNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_MASK));
			and_phy_reg(pi, SSLPNPHY_rfoverride2val,
				~(SSLPNPHY_rfoverride2val_slna_pu_ovr_val_MASK));
			or_phy_reg(pi, SSLPNPHY_rfoverride2,
				SSLPNPHY_rfoverride2_lna_pu_ovr_MASK |
				SSLPNPHY_rfoverride2_slna_pu_ovr_MASK);
			if (BCMECICOEX_ENAB_PHY(pi)) {
				and_phy_reg(pi, SSLPNPHY_rfoverride3_val,
					~(SSLPNPHY_rfoverride3_val_rfactive_ovr_val_MASK));
				or_phy_reg(pi,  SSLPNPHY_rfoverride3,
					SSLPNPHY_rfoverride3_rfactive_ovr_MASK);
			}
		}
	} else if (ISLCNPHY(pi)) {
		if (on) {
			and_phy_reg(pi, LCNPHY_RFOverride0,
				~(LCNPHY_RFOverride0_rfpll_pu_ovr_MASK	|
				LCNPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
				LCNPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
				LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
				LCNPHY_RFOverride0_internalrftxpu_ovr_MASK));
			and_phy_reg(pi, LCNPHY_rfoverride2,
				~(LCNPHY_rfoverride2_lna_pu_ovr_MASK |
				LCNPHY_rfoverride2_slna_pu_ovr_MASK));
			and_phy_reg(pi, LCNPHY_rfoverride3,
				~LCNPHY_rfoverride3_rfactive_ovr_MASK);
		} else {
			and_phy_reg(pi,  LCNPHY_RFOverrideVal0,
				~(LCNPHY_RFOverrideVal0_rfpll_pu_ovr_val_MASK |
				LCNPHY_RFOverrideVal0_wrssi_pu_ovr_val_MASK 	|
				LCNPHY_RFOverrideVal0_nrssi_pu_ovr_val_MASK 	|
				LCNPHY_RFOverrideVal0_internalrfrxpu_ovr_val_MASK 	|
				LCNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK));
			or_phy_reg(pi, LCNPHY_RFOverride0,
				LCNPHY_RFOverride0_rfpll_pu_ovr_MASK 		|
				LCNPHY_RFOverride0_wrssi_pu_ovr_MASK 		|
				LCNPHY_RFOverride0_nrssi_pu_ovr_MASK 		|
				LCNPHY_RFOverride0_internalrfrxpu_ovr_MASK 	|
				LCNPHY_RFOverride0_internalrftxpu_ovr_MASK);

			and_phy_reg(pi, LCNPHY_rxlnaandgainctrl1ovrval,
				~(LCNPHY_rxlnaandgainctrl1ovrval_lnapuovr_Val_MASK));
			and_phy_reg(pi, LCNPHY_rfoverride2val,
				~(LCNPHY_rfoverride2val_slna_pu_ovr_val_MASK));
			or_phy_reg(pi, LCNPHY_rfoverride2,
				LCNPHY_rfoverride2_lna_pu_ovr_MASK |
				LCNPHY_rfoverride2_slna_pu_ovr_MASK);
			and_phy_reg(pi, LCNPHY_rfoverride3_val,
				~(LCNPHY_rfoverride3_val_rfactive_ovr_val_MASK));
			or_phy_reg(pi,  LCNPHY_rfoverride3,
				LCNPHY_rfoverride3_rfactive_ovr_MASK);
		}
	} else if (ISABGPHY(pi)) {
		wlc_phy_switch_radio_abgphy(pi, on);
	}
}

#ifdef BCMECICOEX
static void
wlc_phy_update_bt_chanspec(phy_info_t *pi, chanspec_t chanspec)
{
	si_t *sih = pi->sh->sih;

	if (BCMECISECICOEX_ENAB_PHY(pi) && !SCAN_INPROG_PHY(pi)) {
		 /* Inform BT about the channel change if we are operating in 2Ghz band */
		if (CHSPEC_IS2G(chanspec)) {
			NOTIFY_BT_CHL(sih, CHSPEC_CHANNEL(chanspec));
			if (CHSPEC_IS40(chanspec))
				NOTIFY_BT_BW_40(sih);
			else
				NOTIFY_BT_BW_20(sih);
		} else {
			NOTIFY_BT_CHL(sih, 0);
		}
	}
}
#endif /* BCMECICOEX */

/* %%%%%% chanspec, reg/srom limit */
uint16
wlc_phy_bw_state_get(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	return pi->bw;
}

void
wlc_phy_bw_state_set(wlc_phy_t *ppi, uint16 bw)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	pi->bw = bw;
}

void
wlc_phy_chanspec_radio_set(wlc_phy_t *ppi, chanspec_t newch)
{
	phy_info_t *pi = (phy_info_t*)ppi;
	pi->radio_chanspec = newch;

}

chanspec_t
wlc_phy_chanspec_get(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	return pi->radio_chanspec;
}

void
wlc_phy_chanspec_set(wlc_phy_t *ppi, chanspec_t chanspec)
{
	phy_info_t *pi = (phy_info_t*)ppi;
	uint16 m_cur_channel;
	chansetfn_t chanspec_set = NULL;
#if defined(WLMCHAN) || defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, chanspec);
#endif

	PHY_TRACE(("wl%d: %s: chanspec %x\n", pi->sh->unit, __FUNCTION__, chanspec));
	ASSERT(!wf_chspec_malformed(chanspec));

	/* Update ucode channel value */
	m_cur_channel = CHSPEC_CHANNEL(chanspec);
	if (CHSPEC_IS5G(chanspec))
		m_cur_channel |= D11_CURCHANNEL_5G;
	if (CHSPEC_IS40(chanspec))
		m_cur_channel |= D11_CURCHANNEL_40;
	PHY_TRACE(("wl%d: %s: m_cur_channel %x\n", pi->sh->unit, __FUNCTION__, m_cur_channel));
	wlapi_bmac_write_shm(pi->sh->physhim, M_CURCHANNEL, m_cur_channel);

	chanspec_set = pi->pi_fptr.chanset;

#if defined(WLMCHAN) || defined(PHYCAL_CACHING)
	/* If a channel context exists, retrieve the multi-phase info from there, else use
	 * the default one
	 */
	/* A context has to be explictly created */
	if (ctx)
		pi->cal_info = &ctx->cal_info;
	else
		pi->cal_info = &pi->def_cal_info;
#endif

	ASSERT(pi->cal_info);
	if (chanspec_set)
		(*chanspec_set)(pi, chanspec);

#ifdef WLMCHAN
	/* Switched the context so restart a pending MPHASE cal, else clear the state */
	if (ctx) {
		if (wlc_phy_cal_cache_restore(pi) == BCME_ERROR)
			PHY_CAL((" %s chanspec 0x%x Failed\n", __FUNCTION__, pi->radio_chanspec));

		/* Calibrate if now > last_cal_time + glacial */

		if (PHY_PERICAL_MPHASE_PENDING(pi)) {
			PHY_CAL(("%s: Restarting calibration for 0x%x phase %d\n",
			         __FUNCTION__, chanspec, pi->cal_info->cal_phase_id));
			/* Delete any exisiting timer just in case */
			wlapi_del_timer(pi->sh->physhim, pi->phycal_timer);
			wlapi_add_timer(pi->sh->physhim, pi->phycal_timer, 0, 0);
		} else if ((pi->phy_cal_mode != PHY_PERICAL_DISABLE) &&
		    (pi->phy_cal_mode != PHY_PERICAL_MANUAL) &&
		    ((pi->sh->now - pi->cal_info->last_cal_time) >= pi->sh->glacial_timer))
			wlc_phy_cal_perical((wlc_phy_t *)pi, PHY_PERICAL_WATCHDOG);
	}
#endif /* WLMCHAN */

	wlc_phy_update_bt_chanspec(pi, chanspec);
}

/* don't use this directly. use wlc_get_band_range whenever possible */
int
wlc_phy_chanspec_freq2bandrange_lpssn(uint freq)
{
	int range = -1;

	if (freq < 2500)
		range = WL_CHAN_FREQ_RANGE_2G;
	else if (freq <= 5320)
		range = WL_CHAN_FREQ_RANGE_5GL;
	else if (freq <= 5700)
		range = WL_CHAN_FREQ_RANGE_5GM;
	else
		range = WL_CHAN_FREQ_RANGE_5GH;

	return range;
}

int
wlc_phy_chanspec_bandrange_get(phy_info_t *pi, chanspec_t chanspec)
{
	int range = -1;
	uint channel = CHSPEC_CHANNEL(chanspec);
	uint freq = wlc_phy_channel2freq(channel);

	if (ISNPHY(pi)) {
		range = wlc_phy_get_chan_freq_range_nphy(pi, channel);
	} else if (ISHTPHY(pi)) {
		range = wlc_phy_get_chan_freq_range_htphy(pi, channel);
	} else if (ISSSLPNPHY(pi) || ISLPPHY(pi) || ISLCNPHY(pi)) {
		range = wlc_phy_chanspec_freq2bandrange_lpssn(freq);
	} else if (ISGPHY(pi)) {
		range = WL_CHAN_FREQ_RANGE_2G;
	} else if (ISAPHY(pi)) {
		range = wlc_get_a_band_range(pi);

		switch (range) {
		case A_LOW_CHANS:
			range = WL_CHAN_FREQ_RANGE_5GL;
			break;

		case A_MID_CHANS:
			range = WL_CHAN_FREQ_RANGE_5GM;
			break;

		case A_HIGH_CHANS:
			range = WL_CHAN_FREQ_RANGE_5GH;
			break;

		default:
			break;
		}
	} else
		ASSERT(0);

	return range;
}

void
wlc_phy_chanspec_ch14_widefilter_set(wlc_phy_t *ppi, bool wide_filter)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	pi->channel_14_wide_filter = wide_filter;

}

/*
 * Converts channel number to channel frequency.
 * Returns 0 if the channel is out of range.
 * Also used by some code in wlc_iw.c
 */
int
wlc_phy_channel2freq(uint channel)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++)
		if (chan_info_all[i].chan == channel)
			return (chan_info_all[i].freq);
	return (0);
}

/* fill out a chanvec_t with all the supported channels for the band. */
void
wlc_phy_chanspec_band_validch(wlc_phy_t *ppi, uint band, chanvec_t *channels)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint i;
	uint channel;

	ASSERT((band == WLC_BAND_2G) || (band == WLC_BAND_5G));

	bzero(channels, sizeof(chanvec_t));

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
		channel = chan_info_all[i].chan;

		/* disable the high band channels [149-165] for srom ver 1 */
		if ((pi->a_band_high_disable) && (channel >= FIRST_REF5_CHANNUM) &&
		    (channel <= LAST_REF5_CHANNUM))
			continue;

		if (((band == WLC_BAND_2G) && (channel <= CH_MAX_2G_CHANNEL)) ||
		    ((band == WLC_BAND_5G) && (channel > CH_MAX_2G_CHANNEL)))
			setbit(channels->vec, channel);
	}
}


/* returns the first hw supported channel in the band */
chanspec_t
wlc_phy_chanspec_band_firstch(wlc_phy_t *ppi, uint band)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint i;
	uint channel;
	chanspec_t chspec;

	ASSERT((band == WLC_BAND_2G) || (band == WLC_BAND_5G));

	for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
		channel = chan_info_all[i].chan;

		/* If 40MHX b/w then check if there is an upper 20Mhz adjacent channel */
		if ((ISNPHY(pi) || ISHTPHY(pi)|| ISSSLPNPHY(pi)) && IS40MHZ(pi)) {
			uint j;
			/* check if the upper 20Mhz channel exists */
			for (j = 0; j < ARRAYSIZE(chan_info_all); j++) {
				if (chan_info_all[j].chan == channel + CH_10MHZ_APART)
					break;
			}
			/* did we find an adjacent channel */
			if (j == ARRAYSIZE(chan_info_all))
				continue;
			/* Convert channel from 20Mhz num to 40 Mhz number */
			channel = UPPER_20_SB(channel);
			chspec = channel | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
			if (band == WLC_BAND_2G)
				chspec |= WL_CHANSPEC_BAND_2G;
			else
				chspec |= WL_CHANSPEC_BAND_5G;
		}
		else
			chspec = CH20MHZ_CHSPEC(channel);

		/* disable the high band channels [149-165] for srom ver 1 */
		if ((pi->a_band_high_disable) && (channel >= FIRST_REF5_CHANNUM) &&
		    (channel <= LAST_REF5_CHANNUM))
			continue;

		if (((band == WLC_BAND_2G) && (channel <= CH_MAX_2G_CHANNEL)) ||
		    ((band == WLC_BAND_5G) && (channel > CH_MAX_2G_CHANNEL)))
			return chspec;
	}

	/* should never come here */
	ASSERT(0);

	/* to avoid warning */
	return (chanspec_t)INVCHANSPEC;
}

/* %%%%%% txpower */

/* user txpower limit: in qdbm units with override flag */
int
wlc_phy_txpower_get(wlc_phy_t *ppi, uint *qdbm, bool *override)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	ASSERT(qdbm != NULL);
	*qdbm = pi->tx_user_target[0];
	if (override != NULL)
		*override = pi->txpwroverride;
	return (0);
}

void
wlc_phy_txpower_target_set(wlc_phy_t *ppi, struct txpwr_limits *txpwr)
{
	bool mac_enabled = FALSE;
	phy_info_t *pi = (phy_info_t*)ppi;

	/* fill the txpwr from the struct to the right offsets */
	/* cck */
	bcopy(&txpwr->cck[0], &pi->tx_user_target[TXP_FIRST_CCK], WLC_NUM_RATES_CCK);

	/* ofdm */
	bcopy(&txpwr->ofdm[0], &pi->tx_user_target[TXP_FIRST_OFDM],
		WLC_NUM_RATES_OFDM);
	bcopy(&txpwr->ofdm_cdd[0], &pi->tx_user_target[TXP_FIRST_OFDM_20_CDD],
		WLC_NUM_RATES_OFDM);

	bcopy(&txpwr->ofdm_40_siso[0], &pi->tx_user_target[TXP_FIRST_OFDM_40_SISO],
		WLC_NUM_RATES_OFDM);
	bcopy(&txpwr->ofdm_40_cdd[0], &pi->tx_user_target[TXP_FIRST_OFDM_40_CDD],
		WLC_NUM_RATES_OFDM);

	/* mcs 20MHz */
	bcopy(&txpwr->mcs_20_siso[0], &pi->tx_user_target[TXP_FIRST_MCS_20_SISO],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_20_cdd[0], &pi->tx_user_target[TXP_FIRST_MCS_20_CDD],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_20_stbc[0], &pi->tx_user_target[TXP_FIRST_MCS_20_STBC],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_20_mimo[0], &pi->tx_user_target[TXP_FIRST_MCS_20_SDM],
		WLC_NUM_RATES_MCS_2_STREAM);

	/* mcs 40MHz */
	bcopy(&txpwr->mcs_40_siso[0], &pi->tx_user_target[TXP_FIRST_MCS_40_SISO],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_40_cdd[0], &pi->tx_user_target[TXP_FIRST_MCS_40_CDD],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_40_stbc[0], &pi->tx_user_target[TXP_FIRST_MCS_40_STBC],
		WLC_NUM_RATES_MCS_1_STREAM);
	bcopy(&txpwr->mcs_40_mimo[0], &pi->tx_user_target[TXP_FIRST_MCS_40_SDM],
		WLC_NUM_RATES_MCS_2_STREAM);

	if (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC)
		mac_enabled = TRUE;

	if (mac_enabled)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	wlc_phy_txpower_recalc_target(pi);
	wlc_phy_cal_txpower_recalc_sw(pi);

	if (mac_enabled)
		wlapi_enable_mac(pi->sh->physhim);
}

/* user txpower limit: in qdbm units with override flag */
int
wlc_phy_txpower_set(wlc_phy_t *ppi, uint qdbm, bool override)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	int i;

	if (qdbm > 127)
		return 5;

	/* No way for user to set maxpower on individual rates yet.
	 * Same max power is used for all rates
	 */
	for (i = 0; i < TXP_NUM_RATES; i++)
		pi->tx_user_target[i] = (uint8)qdbm;

	/* Restrict external builds to 100% Tx Power */
#if defined(WLTEST)
	pi->txpwroverride = override;
#else
	pi->txpwroverride = FALSE;
#endif


	if (pi->sh->up) {
		if (SCAN_INPROG_PHY(pi)) {
			PHY_TXPWR(("wl%d: Scan in progress, skipping txpower control\n",
				pi->sh->unit));
		} else {
			if (ISSSLPNPHY(pi)) {
				wlc_phy_txpower_recalc_target(pi);
				wlc_phy_cal_txpower_recalc_sw(pi);
			} else {
				bool suspend;

				suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) &
					MCTL_EN_MAC));

				if (!suspend)
					wlapi_suspend_mac_and_wait(pi->sh->physhim);

				wlc_phy_txpower_recalc_target(pi);
				wlc_phy_cal_txpower_recalc_sw(pi);

				if (!suspend)
					wlapi_enable_mac(pi->sh->physhim);
			}
		}
	}
	return (0);
}

/* get sromlimit per rate for given channel. Routine does not account for ant gain */
void
wlc_phy_txpower_sromlimit(wlc_phy_t *ppi, uint channel, uint8 *min_pwr, uint8 *max_pwr,
	int txp_rate_idx)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint i;

	*min_pwr = pi->min_txpower * WLC_TXPWR_DB_FACTOR;

	if (ISNPHY(pi)) {
		if (txp_rate_idx < 0)
			txp_rate_idx = TXP_FIRST_CCK;
		wlc_phy_txpower_sromlimit_get_nphy(pi, channel, max_pwr, (uint8)txp_rate_idx);

	} else if (ISHTPHY(pi)) {
		if (txp_rate_idx < 0)
			txp_rate_idx = TXP_FIRST_CCK;
		wlc_phy_txpower_sromlimit_get_htphy(pi, channel, max_pwr, (uint8)txp_rate_idx);

	} else if (ISGPHY(pi) || (channel <= CH_MAX_2G_CHANNEL)) {
		/* until we cook the maxtxpwr value into the channel table,
		 * use the one global B band maxtxpwr
		 */
		if (txp_rate_idx < 0)
			txp_rate_idx = TXP_FIRST_CCK;

		/* legacy phys don't have valid MIMO rate entries
		 * SSLPNPHY does have 8 more entries for MCS 0-7
		 * Cannot re-use nphy logic right away
		 *
		 * SSLPNPHY, LPPHY, ASSERT(txp_rate_idx <= TXP_LAST_OFDM_20_CDD);
		 * other: ASSERT(txp_rate_idx <= TXP_LAST_OFDM);
		 */

		*max_pwr = pi->tx_srom_max_rate_2g[txp_rate_idx];
	} else {
		/* in case we fall out of the channel loop */
		*max_pwr = WLC_TXPWR_MAX;

		if (txp_rate_idx < 0)
			txp_rate_idx = TXP_FIRST_OFDM;
		/* max txpwr is channel dependent */
		for (i = 0; i < ARRAYSIZE(chan_info_all); i++) {
			if (channel == chan_info_all[i].chan) {
				break;
			}
		}
		ASSERT(i < ARRAYSIZE(chan_info_all));

		/* legacy phys don't have valid MIMO rate entries
		 * SSLPNPHY does have 8 more entries for MCS 0-7
		 * Cannot re-use nphy logic right away
		 *
		 *
		 * SSLPNPHY, LPPHY, ASSERT(txp_rate_idx <= TXP_LAST_OFDM_20_CDD);
		 * other: ASSERT(txp_rate_idx <= TXP_LAST_OFDM);
		 */

		if (pi->hwtxpwr) {
			*max_pwr = pi->hwtxpwr[i];
		} else {
			/* When would we get here?  B only? */
			if ((i >= FIRST_MID_5G_CHAN) && (i <= LAST_MID_5G_CHAN))
				*max_pwr = pi->tx_srom_max_rate_5g_mid[txp_rate_idx];
			if ((i >= FIRST_HIGH_5G_CHAN) && (i <= LAST_HIGH_5G_CHAN))
				*max_pwr = pi->tx_srom_max_rate_5g_hi[txp_rate_idx];
			if ((i >= FIRST_LOW_5G_CHAN) && (i <= LAST_LOW_5G_CHAN))
				*max_pwr = pi->tx_srom_max_rate_5g_low[txp_rate_idx];
		}
	}
	PHY_TMP(("%s: chan %d rate idx %d, sromlimit %d\n", __FUNCTION__, channel, txp_rate_idx,
		*max_pwr));
}


void
wlc_phy_txpower_sromlimit_max_get(wlc_phy_t *ppi, uint chan, uint8 *max_txpwr, uint8 *min_txpwr)
{
	phy_info_t *pi = (phy_info_t*)ppi;
	uint8 tx_pwr_max = 0;
	uint8 tx_pwr_min = 255;
	uint8 max_num_rate;
	uint8 maxtxpwr, mintxpwr, rate, pactrl;

	pactrl = 0;
	if (ISGPHY(pi) && (pi->sh->boardflags & BFL_PACTRL))
		pactrl = 3;

	max_num_rate = (ISNPHY(pi) || ISHTPHY(pi)) ? TXP_NUM_RATES :
		((ISSSLPNPHY(pi) || ISLCNPHY(pi))?
		(TXP_LAST_SISO_MCS_20 + 1) : (TXP_LAST_OFDM + 1));

	for (rate = 0; rate < max_num_rate; rate++) {

		wlc_phy_txpower_sromlimit(ppi, chan, &mintxpwr, &maxtxpwr, rate);

		maxtxpwr = (maxtxpwr > pactrl) ? (maxtxpwr - pactrl) : 0;
		/* Subtract 6 (1.5db) to ensure we don't go over
		 * the limit given a noisy power detector
		 */
		maxtxpwr = (maxtxpwr > 6) ? (maxtxpwr - 6) : 0;

		tx_pwr_max = MAX(tx_pwr_max, maxtxpwr);
		tx_pwr_min = MIN(tx_pwr_min, maxtxpwr);
	}
	*max_txpwr = tx_pwr_max;
	*min_txpwr = tx_pwr_min;
}

void
wlc_phy_txpower_boardlimit_band(wlc_phy_t *ppi, uint bandunit, int32 *max_pwr,
	int32 *min_pwr, uint32 *step_pwr)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	int32 local_max;

	if (ISLPPHY(pi)) {
		if (bandunit == 1)
			*max_pwr = pi->tx_srom_max_2g;
		else {
			local_max = pi->tx_srom_max_5g_low;
			if (local_max <  pi->tx_srom_max_5g_mid)
				local_max =  pi->tx_srom_max_5g_mid;
			if (local_max <  pi->tx_srom_max_5g_hi)
				local_max =  pi->tx_srom_max_5g_hi;
			*max_pwr = local_max;
		}
		*min_pwr = 8;
		*step_pwr = 1;
	}
}

uint8
wlc_phy_txpower_get_target_min(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	return pi->tx_power_min;
}

uint8
wlc_phy_txpower_get_target_max(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	return pi->tx_power_max;
}

/* Recalc target power all phys.  This internal/static function needs to be called whenever
 * the chanspec or TX power values (user target, regulatory limits or SROM/HW limits) change.
 * This happens thorough calls to the PHY public API.
 */
void
wlc_phy_txpower_recalc_target(phy_info_t *pi)
{
	uint8 maxtxpwr, mintxpwr, rate, pactrl;
	uint target_chan;
	uint8 tx_pwr_target[TXP_NUM_RATES];
	uint8 tx_pwr_max = 0;
	uint8 tx_pwr_min = 255;
	uint8 tx_pwr_max_rate_ind = 0;
	uint8 max_num_rate;
	uint8 start_rate = 0;
	chanspec_t chspec;
	uint32 band = CHSPEC2WLC_BAND(pi->radio_chanspec);
	initfn_t txpwr_recalc_fn = NULL;

	chspec = pi->radio_chanspec;
	if (CHSPEC_CTL_SB(chspec) == WL_CHANSPEC_CTL_SB_NONE)
		target_chan = CHSPEC_CHANNEL(chspec);
	else if (CHSPEC_CTL_SB(chspec) == WL_CHANSPEC_CTL_SB_UPPER)
		target_chan = UPPER_20_SB(CHSPEC_CHANNEL(chspec));
	else
		target_chan = LOWER_20_SB(CHSPEC_CHANNEL(chspec));

	pactrl = 0;
	if (ISGPHY(pi) && (pi->sh->boardflags & BFL_PACTRL))
		pactrl = 3;

	if (ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
		uint32 offset_mcs, i;

		if (CHSPEC_IS40(pi->radio_chanspec)) {
			offset_mcs = pi->mcs40_po;
			for (i = TXP_FIRST_SISO_MCS_20; i <= TXP_LAST_SISO_MCS_20;  i++) {
				pi->tx_srom_max_rate_2g[i-8] = pi->tx_srom_max_2g -
					((offset_mcs & 0xf) * 2);
				offset_mcs >>= 4;
			}
		} else {
			offset_mcs = pi->mcs20_po;
			for (i = TXP_FIRST_SISO_MCS_20; i <= TXP_LAST_SISO_MCS_20;  i++) {
				pi->tx_srom_max_rate_2g[i-8] = pi->tx_srom_max_2g -
					((offset_mcs & 0xf) * 2);
				offset_mcs >>= 4;
			}
		}
	}

#if WL11N
	max_num_rate = ((ISNPHY(pi) || ISHTPHY(pi)) ? (TXP_NUM_RATES) :
		((ISSSLPNPHY(pi) || ISLCNPHY(pi)) ?
		(TXP_LAST_SISO_MCS_20 + 1) : (TXP_LAST_OFDM + 1)));
#else
	max_num_rate = ((ISNPHY(pi) || ISHTPHY(pi)) ? (TXP_NUM_RATES) :
		(TXP_LAST_OFDM + 1));
#endif
	/* get the per rate limits based on vbat and temp */
	wlc_phy_upd_env_txpwr_rate_limits(pi, band);

	/* Combine user target, regulatory limit, SROM/HW/board limit and power
	 * percentage to get a tx power target for each rate.
	 */
	for (rate = start_rate; rate < max_num_rate; rate++) {
		/* The user target is the starting point for determining the transmit
		 * power.  If pi->txoverride is true, then use the user target as the
		 * tx power target.
		 */

		tx_pwr_target[rate] = pi->tx_user_target[rate];

		/* this basically moves the user target power from rf port to antenna port */
		if (pi->user_txpwr_at_rfport) {
			tx_pwr_target[rate] += wlc_user_txpwr_antport_to_rfport(pi,
				target_chan, band, rate);
		}

#if defined(WLTEST)
		/* Only allow tx power override for internal or test builds. */
		if (!pi->txpwroverride)
#endif
		{
			/* Get board/hw limit */
			wlc_phy_txpower_sromlimit((wlc_phy_t *)pi, target_chan, &mintxpwr,
				&maxtxpwr, rate);

			/* Choose minimum of provided regulatory and board/hw limits */
			maxtxpwr = MIN(maxtxpwr, pi->txpwr_limit[rate]);

			/* Board specific fix reduction */
			maxtxpwr = (maxtxpwr > pactrl) ? (maxtxpwr - pactrl) : 0;

			/* Subtract 6 (1.5db) to ensure we don't go over
			 * the limit given a noisy power detector.  The result
			 * is now a target, not a limit.
			 */

			if (!(ISSSLPNPHY(pi) && SSLPNREV_IS(pi->pubpi.phy_rev, 4)))
				maxtxpwr = (maxtxpwr > 6) ? (maxtxpwr - 6) : 0;

			/* Choose least of user and now combined regulatory/hw
			 * targets.
			 */

			if ((ISSSLPNPHY(pi) && SSLPNREV_IS(pi->pubpi.phy_rev, 4)))
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], maxtxpwr);
			else
				maxtxpwr = MIN(maxtxpwr, tx_pwr_target[rate]);
			if (ISSSLPNPHY(pi))
				wlc_sslpnphy_txpwr_target_adj(pi, tx_pwr_target, rate);

			if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
				tx_pwr_target[rate] = (tx_pwr_target[rate] *
					pi->txpwr_percent) / 100;
			} else {
				/* Apply power output percentage */
				if (pi->txpwr_percent <= 100)
					maxtxpwr = (maxtxpwr * pi->txpwr_percent) / 100;
				/* Enforce min power and save result as power target. */
				tx_pwr_target[rate] = MAX(maxtxpwr, mintxpwr);
			}
		}
		/* check the per rate override based on environment vbat/temp */
		tx_pwr_target[rate] = MIN(tx_pwr_target[rate], pi->txpwr_env_limit[rate]);
		/* Identify the rate amongst the TXP_NUM_RATES rates with the max target power
		 * on this channel. If multiple rates have the max target power, the
		 * lowest rate index among them is stored.
		 */
		if (tx_pwr_target[rate] > tx_pwr_max)
			tx_pwr_max_rate_ind = rate;

		tx_pwr_max = MAX(tx_pwr_max, tx_pwr_target[rate]);
		tx_pwr_min = MIN(tx_pwr_min, tx_pwr_target[rate]);
	}

	/* Now calculate the tx_power_offset and update the hardware... */
	bzero(pi->tx_power_offset, sizeof(pi->tx_power_offset));
	pi->tx_power_max = tx_pwr_max;
	pi->tx_power_min = tx_pwr_min;
	pi->tx_power_max_rate_ind = tx_pwr_max_rate_ind;
	PHY_TMP(("wl%d: %s: channel %d rate - targets - offsets:\n", pi->sh->unit,
		__FUNCTION__, target_chan));
	for (rate = 0; rate < max_num_rate; rate++) {
		/* For swpwrctrl, the offset is OFDM w.r.t. CCK.
		 * For hwpwrctl, other way around
		 */
		pi->tx_power_target[rate] = tx_pwr_target[rate];

		if (!pi->hwpwrctrl || ISNPHY(pi) || ISHTPHY(pi)) {
			pi->tx_power_offset[rate] = pi->tx_power_max - pi->tx_power_target[rate];
		} else {
			pi->tx_power_offset[rate] = pi->tx_power_target[rate] - pi->tx_power_min;
		}
		PHY_TMP(("    %d:    %d    %d\n", rate, pi->tx_power_target[rate],
			pi->tx_power_offset[rate]));
	}

	txpwr_recalc_fn = pi->pi_fptr.txpwrrecalc;
	if (txpwr_recalc_fn)
		(*txpwr_recalc_fn)(pi);
}

#ifdef BCMDBG
void
wlc_phy_txpower_limits_dump(txpwr_limits_t *txpwr)
{
	int i;
	char fraction[4][4] = {"   ", ".25", ".5 ", ".75"};

	printf("CCK                ");
	for (i = 0; i < WLC_NUM_RATES_CCK; i++) {
		printf(" %2d%s", txpwr->cck[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->cck[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz OFDM SISO   ");
	for (i = 0; i < WLC_NUM_RATES_OFDM; i++) {
		printf(" %2d%s", txpwr->ofdm[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->ofdm[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz OFDM CDD    ");
	for (i = 0; i < WLC_NUM_RATES_OFDM; i++) {
		printf(" %2d%s", txpwr->ofdm_cdd[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->ofdm_cdd[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz OFDM SISO   ");
	for (i = 0; i < WLC_NUM_RATES_OFDM; i++) {
		printf(" %2d%s", txpwr->ofdm_40_siso[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->ofdm_40_siso[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz OFDM CDD    ");
	for (i = 0; i < WLC_NUM_RATES_OFDM; i++) {
		printf(" %2d%s", txpwr->ofdm_40_cdd[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->ofdm_40_cdd[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz MCS0-7 SISO ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_20_siso[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_20_siso[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz MCS0-7 CDD  ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_20_cdd[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_20_cdd[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz MCS0-7 STBC ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_20_stbc[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_20_stbc[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("20 MHz MCS8-15 SDM ");
	for (i = 0; i < WLC_NUM_RATES_MCS_2_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_20_mimo[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_20_mimo[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz MCS0-7 SISO ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_40_siso[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_40_siso[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz MCS0-7 CDD  ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_40_cdd[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_40_cdd[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz MCS0-7 STBC ");
	for (i = 0; i < WLC_NUM_RATES_MCS_1_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_40_stbc[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_40_stbc[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("40 MHz MCS8-15 SDM ");
	for (i = 0; i < WLC_NUM_RATES_MCS_2_STREAM; i++) {
		printf(" %2d%s", txpwr->mcs_40_mimo[i]/ WLC_TXPWR_DB_FACTOR,
			fraction[txpwr->mcs_40_mimo[i] % WLC_TXPWR_DB_FACTOR]);
	}
	printf("\n");

	printf("MCS32               %2d%s\n",
		txpwr->mcs32 / WLC_TXPWR_DB_FACTOR,
		fraction[txpwr->mcs32 % WLC_TXPWR_DB_FACTOR]);
}
#endif /* BCMDBG */

/* Translates the regulatory power limit array into an array of length TXP_NUM_RATES,
 * which can match the board limit array obtained using the SROM. Moreover, since the NPHY chips
 * currently do not distinguish between Legacy OFDM and MCS0-7, the SISO and CDD regulatory power
 * limits of these rates need to be combined carefully.
 * This internal/static function needs to be called whenever the chanspec or regulatory TX power
 * limits change.
 */
void
wlc_phy_txpower_reg_limit_calc(phy_info_t *pi, struct txpwr_limits *txpwr, chanspec_t chanspec)
{
	uint8 tmp_txpwr_limit[2*WLC_NUM_RATES_OFDM];
	uint8 *txpwr_ptr1 = NULL, *txpwr_ptr2 = NULL;
	int rate_start_index = 0, rate1, rate2, k;

	/* Obtain the regulatory limits for CCK rates */
	for (rate1 = WL_TX_POWER_CCK_FIRST, rate2 = 0; rate2 < WL_TX_POWER_CCK_NUM;
	     rate1++, rate2++)
		pi->txpwr_limit[rate1] = txpwr->cck[rate2];

	/* Obtain the regulatory limits for Legacy OFDM SISO rates */
	for (rate1 = WL_TX_POWER_OFDM_FIRST, rate2 = 0; rate2 < WL_TX_POWER_OFDM_NUM;
	     rate1++, rate2++)
		pi->txpwr_limit[rate1] = txpwr->ofdm[rate2];

	/* Obtain the regulatory limits for Legacy OFDM and HT-OFDM 11n rates in NPHY chips */
	if (ISNPHY(pi)) {
		/* If NPHY is enabled, then use min of OFDM and MCS_20_SISO values as the regulatory
		 * limit for SISO Legacy OFDM and MCS0-7 rates. Similarly, for 40 MHz SIS0 Legacy
		 * OFDM  and MCS0-7 rates as well as for 20 MHz and 40 MHz CDD Legacy OFDM and
		 * MCS0-7 rates. This is because the current hardware implementation uses common
		 * powers for the 8 Legacy ofdm and 8 mcs0-7 rates, i.e. they share the same power
		 * table. The power table is populated based on the constellation, coding rate, and
		 * transmission mode (SISO/CDD/STBC/SDM). Therefore, care must be taken to match the
		 * constellation and coding rates of the Legacy OFDM and MCS0-7 rates since the 8
		 * Legacy OFDM rates and the 8 MCS0-7 rates do not have a 1-1 correspondence in
		 * these parameters.
		 */

		/* Regulatory limits for Legacy OFDM rates 20 and 40 MHz, SISO and CDD. The
		 * regulatory limits for the corresponding MCS0-7 20 and 40 MHz, SISO and
		 * CDD rates should also be mapped into Legacy OFDM limits and the minimum
		 * of the two limits should be taken for each rate.
		 */
		for (k = 0; k < 4; k++) {
			switch (k) {
			case 0:
				/* 20 MHz Legacy OFDM SISO */
				txpwr_ptr1 = txpwr->mcs_20_siso;
				txpwr_ptr2 = txpwr->ofdm;
				rate_start_index = WL_TX_POWER_OFDM_FIRST;
				break;
			case 1:
				/* 20 MHz Legacy OFDM CDD */
				txpwr_ptr1 = txpwr->mcs_20_cdd;
				txpwr_ptr2 = txpwr->ofdm_cdd;
				rate_start_index = WL_TX_POWER_OFDM20_CDD_FIRST;
				break;
			case 2:
				/* 40 MHz Legacy OFDM SISO */
				txpwr_ptr1 = txpwr->mcs_40_siso;
				txpwr_ptr2 = txpwr->ofdm_40_siso;
				rate_start_index = WL_TX_POWER_OFDM40_SISO_FIRST;
				break;
			case 3:
				/* 40 MHz Legacy OFDM CDD */
				txpwr_ptr1 = txpwr->mcs_40_cdd;
				txpwr_ptr2 = txpwr->ofdm_40_cdd;
				rate_start_index = WL_TX_POWER_OFDM40_CDD_FIRST;
				break;
			}

			for (rate2 = 0; rate2 < WLC_NUM_RATES_OFDM; rate2++) {
				tmp_txpwr_limit[rate2] = 0;
				tmp_txpwr_limit[WLC_NUM_RATES_OFDM+rate2] = txpwr_ptr1[rate2];
			}
			wlc_phy_mcs_to_ofdm_powers_nphy(tmp_txpwr_limit, 0,
			                                WLC_NUM_RATES_OFDM-1, WLC_NUM_RATES_OFDM);
			for (rate1 = rate_start_index, rate2 = 0; rate2 < WLC_NUM_RATES_OFDM;
			     rate1++, rate2++)
				pi->txpwr_limit[rate1] = MIN(txpwr_ptr2[rate2],
				                             tmp_txpwr_limit[rate2]);
		}

		/* Regulatory limits for MCS0-7 rates 20 and 40 MHz, SISO and CDD. The
		 * regulatory limits for the corresponding Legacy OFDM 20 and 40 MHz, SISO and
		 * CDD rates should also be mapped into MCS0-7 limits and the minimum
		 * of the two limits should be taken for each rate.
		 */
		for (k = 0; k < 4; k++) {
			switch (k) {
			case 0:
				/* 20 MHz MCS 0-7 SISO */
				txpwr_ptr1 = txpwr->ofdm;
				txpwr_ptr2 = txpwr->mcs_20_siso;
				rate_start_index = WL_TX_POWER_MCS20_SISO_FIRST;
				break;
			case 1:
				/* 20 MHz MCS 0-7 CDD */
				txpwr_ptr1 = txpwr->ofdm_cdd;
				txpwr_ptr2 = txpwr->mcs_20_cdd;
				rate_start_index = WL_TX_POWER_MCS20_CDD_FIRST;
				break;
			case 2:
				/* 40 MHz MCS 0-7 SISO */
				txpwr_ptr1 = txpwr->ofdm_40_siso;
				txpwr_ptr2 = txpwr->mcs_40_siso;
				rate_start_index = WL_TX_POWER_MCS40_SISO_FIRST;
				break;
			case 3:
				/* 40 MHz MCS 0-7 CDD */
				txpwr_ptr1 = txpwr->ofdm_40_cdd;
				txpwr_ptr2 = txpwr->mcs_40_cdd;
				rate_start_index = WL_TX_POWER_MCS40_CDD_FIRST;
				break;
			}
			for (rate2 = 0; rate2 < WLC_NUM_RATES_OFDM; rate2++) {
				tmp_txpwr_limit[rate2] = 0;
				tmp_txpwr_limit[WLC_NUM_RATES_OFDM+rate2] = txpwr_ptr1[rate2];
			}
			wlc_phy_ofdm_to_mcs_powers_nphy(tmp_txpwr_limit, 0,
			                                WLC_NUM_RATES_OFDM-1, WLC_NUM_RATES_OFDM);
			for (rate1 = rate_start_index, rate2 = 0;
			     rate2 < WLC_NUM_RATES_MCS_1_STREAM; rate1++, rate2++)
				pi->txpwr_limit[rate1] = MIN(txpwr_ptr2[rate2],
				                             tmp_txpwr_limit[rate2]);
		}

		/* Regulatory limits for MCS0-7 rates 20 and 40 MHz, STBC */
		for (k = 0; k < 2; k++) {
			switch (k) {
			case 0:
				/* 20 MHz MCS 0-7 STBC */
				rate_start_index = WL_TX_POWER_MCS20_STBC_FIRST;
				txpwr_ptr1 = txpwr->mcs_20_stbc;
				break;
			case 1:
				/* 40 MHz MCS 0-7 STBC */
				rate_start_index = WL_TX_POWER_MCS40_STBC_FIRST;
				txpwr_ptr1 = txpwr->mcs_40_stbc;
				break;
			}
			for (rate1 = rate_start_index, rate2 = 0;
			     rate2 < WLC_NUM_RATES_MCS_1_STREAM; rate1++, rate2++)
				pi->txpwr_limit[rate1] = txpwr_ptr1[rate2];
		}

		/* Regulatory limits for MCS8-15 rates 20 and 40 MHz, SDM */
		for (k = 0; k < 2; k++) {
			switch (k) {
			case 0:
				/* 20 MHz MCS 8-15 (MIMO) tx power limits */
				rate_start_index = WL_TX_POWER_MCS20_SDM_FIRST;
				txpwr_ptr1 = txpwr->mcs_20_mimo;
				break;
			case 1:
				/* 40 MHz MCS 8-15 (MIMO) tx power limits */
				rate_start_index = WL_TX_POWER_MCS40_SDM_FIRST;
				txpwr_ptr1 = txpwr->mcs_40_mimo;
				break;
			}
			for (rate1 = rate_start_index, rate2 = 0;
			     rate2 < WLC_NUM_RATES_MCS_2_STREAM; rate1++, rate2++)
				pi->txpwr_limit[rate1] = txpwr_ptr1[rate2];
		}

		/* MCS32 tx power limits */
		pi->txpwr_limit[WL_TX_POWER_MCS_32] = txpwr->mcs32;

		/* Set the 40 MHz MCS0 and MCS32 regulatory power limits to the minimum of the
		 * two limits
		 */
		pi->txpwr_limit[WL_TX_POWER_MCS40_CDD_FIRST] =
		        MIN(pi->txpwr_limit[WL_TX_POWER_MCS40_CDD_FIRST],
		            pi->txpwr_limit[WL_TX_POWER_MCS_32]);
		pi->txpwr_limit[WL_TX_POWER_MCS_32] = pi->txpwr_limit[WL_TX_POWER_MCS40_CDD_FIRST];
	} else if (ISHTPHY(pi)) {
	}
}

void
wlc_phy_txpwr_percent_set(wlc_phy_t *ppi, uint8 txpwr_percent)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	pi->txpwr_percent = txpwr_percent;
}

void
wlc_phy_machwcap_set(wlc_phy_t *ppi, uint32 machwcap)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	pi->sh->machwcap = machwcap;
}


void
wlc_phy_runbist_config(wlc_phy_t *ppi, bool start_end)
{
	phy_info_t *pi = (phy_info_t*)ppi;
	uint16 rxc;
	rxc = 0;

	if (start_end == ON) {
		if (!ISNPHY(pi) || ISHTPHY(pi))
			return;

		/* Keep pktproc in reset during bist run */
		if (NREV_IS(pi->pubpi.phy_rev, 3) || NREV_IS(pi->pubpi.phy_rev, 4)) {
			rxc = read_phy_reg(pi, NPHY_RxControl);
			write_phy_reg(pi, NPHY_RxControl,
			      NPHY_RxControl_dbgpktprocReset_MASK | rxc);
		}
	} else {
		if (NREV_IS(pi->pubpi.phy_rev, 3) || NREV_IS(pi->pubpi.phy_rev, 4)) {
			write_phy_reg(pi, NPHY_RxControl, rxc);
		}

		wlc_phy_por_inform(ppi);
	}
}

/* Set tx power limits */
/* BMAC_NOTE: this only needs a chanspec so that it can choose which 20/40 limits
 * to save in phy state. Would not need this if we ether saved all the limits and
 * applied them only when we were on the correct channel, or we restricted this fn
 * to be called only when on the correct channel.
 */
void
wlc_phy_txpower_limit_set(wlc_phy_t *ppi, struct txpwr_limits *txpwr, chanspec_t chanspec)
{
	phy_info_t *pi = (phy_info_t*)ppi;

	wlc_phy_txpower_reg_limit_calc(pi, txpwr, chanspec);

	if ((ISSSLPNPHY(pi) && !(SSLPNREV_IS(pi->pubpi.phy_rev, 3))) || ISLCNPHY(pi)) {
		int i, j;
		for (i = TXP_FIRST_OFDM_20_CDD, j = 0; j < WLC_NUM_RATES_MCS_1_STREAM; i++, j++) {
			if (txpwr->mcs_20_siso[j])
				pi->txpwr_limit[i] = txpwr->mcs_20_siso[j];
			else
				pi->txpwr_limit[i] = txpwr->ofdm[j];
		}
	}

	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	/* wlc_phy_print_txpwr_limits_structure(txpwr); */
	wlc_phy_txpower_recalc_target(pi);
	wlc_phy_cal_txpower_recalc_sw(pi);
	wlapi_enable_mac(pi->sh->physhim);
}

void
wlc_phy_ofdm_rateset_war(wlc_phy_t *pih, bool war)
{
	phy_info_t *pi = (phy_info_t*)pih;

	pi->ofdm_rateset_war = war;
}

void
wlc_phy_bf_preempt_enable(wlc_phy_t *pih, bool bf_preempt)
{
	phy_info_t *pi = (phy_info_t*)pih;

	pi->bf_preempt_4306 = bf_preempt;
}

/*
 * For SW based power control, target power represents CCK and ucode reduced OFDM by the opo.
 * For hw based power control, we lower the target power so it represents OFDM and
 * ucode boosts CCK by the opo.
 */
void
wlc_phy_txpower_update_shm(phy_info_t *pi)
{
	int j;
	if (ISNPHY(pi) || ISHTPHY(pi) || ISSSLPNPHY(pi)) {
		PHY_ERROR(("wlc_phy_update_txpwr_shmem is for legacy phy\n"));
		ASSERT(0);
		return;
	}

	if (!pi->sh->clk)
		return;

	if (pi->hwpwrctrl) {
		uint16 offset;

		/* Rate based ucode power control */
		wlapi_bmac_write_shm(pi->sh->physhim, M_TXPWR_MAX, 63);
		wlapi_bmac_write_shm(pi->sh->physhim, M_TXPWR_N, 1 << NUM_TSSI_FRAMES);

		/* Need to lower the target power to OFDM level, then add boost for CCK. */
		wlapi_bmac_write_shm(pi->sh->physhim, M_TXPWR_TARGET,
			pi->tx_power_min << NUM_TSSI_FRAMES);

		wlapi_bmac_write_shm(pi->sh->physhim, M_TXPWR_CUR, pi->hwpwr_txcur);

		/* CCK */
		if (ISGPHY(pi)) {
			const uint8 ucode_cck_rates[] =
			        { /*	   1,    2,  5.5,   11 Mbps */
					0x02, 0x04, 0x0b, 0x16
				};
			PHY_TXPWR(("M_RATE_TABLE_B:\n"));
			for (j = TXP_FIRST_CCK; j <= TXP_LAST_CCK; j++) {
				offset = wlapi_bmac_rate_shm_offset(pi->sh->physhim,
					ucode_cck_rates[j]);

				wlapi_bmac_write_shm(pi->sh->physhim, offset + 6,
				                   pi->tx_power_offset[j]);
				PHY_TXPWR(("[0x%x] = 0x%x\n", offset + 6, pi->tx_power_offset[j]));

				wlapi_bmac_write_shm(pi->sh->physhim, offset + 14,
				                   -(pi->tx_power_offset[j] / 2));
				PHY_TXPWR(("[0x%x] = 0x%x\n", offset + 14,
				          -(pi->tx_power_offset[j] / 2)));
			}

			if (pi->ofdm_rateset_war) {
				offset = wlapi_bmac_rate_shm_offset(pi->sh->physhim,
					ucode_cck_rates[0]);
				wlapi_bmac_write_shm(pi->sh->physhim, offset + 6, 0);
				PHY_TXPWR(("[0x%x] = 0x%x (ofdm rateset war)\n", offset + 6, 0));
				wlapi_bmac_write_shm(pi->sh->physhim, offset + 14, 0);
				PHY_TXPWR(("[0x%x] = 0x%x (ofdm rateset war)\n", offset + 14, 0));
			}
		}

		/* OFDM */
		PHY_TXPWR(("M_RATE_TABLE_A:\n"));
		for (j = TXP_FIRST_OFDM; j <= TXP_LAST_OFDM; j++) {
			const uint8 ucode_ofdm_rates[] =
			        { /*	   6,   9,    12,   18,   24,   36,   48,   54 Mbps */
					0x0c, 0x12, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
				};
			offset = wlapi_bmac_rate_shm_offset(pi->sh->physhim,
				ucode_ofdm_rates[j - TXP_FIRST_OFDM]);
			wlapi_bmac_write_shm(pi->sh->physhim, offset + 6, pi->tx_power_offset[j]);
			PHY_TXPWR(("[0x%x] = 0x%x\n", offset + 6, pi->tx_power_offset[j]));
			wlapi_bmac_write_shm(pi->sh->physhim, offset + 14,
				-(pi->tx_power_offset[j] / 2));
			PHY_TXPWR(("[0x%x] = 0x%x\n", offset + 14, -(pi->tx_power_offset[j] / 2)));
		}

		wlapi_bmac_mhf(pi->sh->physhim, MHF2, MHF2_HWPWRCTL, MHF2_HWPWRCTL, WLC_BAND_ALL);
	} else {
		int i;

		/* ucode has 2 db granularity when doing sw pwrctrl,
		 * so round up to next 8 .25 units = 2 db.
		 *   HW based power control has .25 db granularity
		 */
		/* Populate only OFDM power offsets, since ucode can only offset OFDM packets */
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++)
			pi->tx_power_offset[i] = (uint8)ROUNDUP(pi->tx_power_offset[i], 8);
		wlapi_bmac_write_shm(pi->sh->physhim, M_OFDM_OFFSET,
			(uint16)((pi->tx_power_offset[TXP_FIRST_OFDM] + 7) >> 3));
	}
}


bool
wlc_phy_txpower_hw_ctrl_get(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	if (ISNPHY(pi)) {
		return pi->nphy_txpwrctrl;
	} else if (ISHTPHY(pi)) {
		return pi->txpwrctrl;
	} else {
		return pi->hwpwrctrl;
	}
}

void
wlc_phy_txpower_hw_ctrl_set(wlc_phy_t *ppi, bool hwpwrctrl)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	bool cur_hwpwrctrl = pi->hwpwrctrl;
	bool suspend;

	/* validate if hardware power control is capable */
	if (!pi->hwpwrctrl_capable) {
		PHY_ERROR(("wl%d: hwpwrctrl not capable\n", pi->sh->unit));
		return;
	}

	PHY_INFORM(("wl%d: setting the hwpwrctrl to %d\n", pi->sh->unit, hwpwrctrl));
	pi->hwpwrctrl = hwpwrctrl;
	pi->nphy_txpwrctrl = hwpwrctrl;
	pi->txpwrctrl = hwpwrctrl;

	/* if power control mode is changed, propagate it */
	if (ISNPHY(pi)) {
		suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);

		/* turn on/off power control */
		wlc_phy_txpwrctrl_enable_nphy(pi, pi->nphy_txpwrctrl);
		if (pi->nphy_txpwrctrl == PHY_TPC_HW_OFF) {
			wlc_phy_txpwr_fixpower_nphy(pi);
		} else {
			/* restore the starting txpwr index */
			mod_phy_reg(pi, NPHY_TxPwrCtrlCmd, NPHY_TxPwrCtrlCmd_pwrIndex_init_MASK,
			            pi->saved_txpwr_idx);
		}

		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	} else if (ISHTPHY(pi)) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

		/* turn on/off power control */
		wlc_phy_txpwrctrl_enable_htphy(pi, pi->txpwrctrl);

		wlapi_enable_mac(pi->sh->physhim);
	} else if (hwpwrctrl != cur_hwpwrctrl) {
		/* save, change and restore tx power control */

		if (ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
			return;

		} else if (ISABGPHY(pi)) {
			wlc_phy_txpower_hw_ctrl_set_abgphy(pi);
		}
	}
}

void
wlc_phy_txpower_ipa_upd(phy_info_t *pi)
{
	/* this should be expanded to work with all new PHY capable of iPA */
	if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->ipa2g_on = (pi->srom_fem2g.extpagain == 2);
		pi->ipa5g_on = (pi->srom_fem5g.extpagain == 2);
	} else {
		pi->ipa2g_on = FALSE;
		pi->ipa5g_on = FALSE;
	}
	PHY_INFORM(("wlc_phy_txpower_ipa_upd: ipa 2g %d, 5g %d\n", pi->ipa2g_on, pi->ipa5g_on));
}

static uint32 wlc_phy_txpower_est_power_nphy(phy_info_t *pi);

/*
 *  reports estimated power and adjusted estimated power in quarter dBms
 *    32-bit output consists of estimated power of cores 0 and 1 in MSbyte and in the 2nd MSbyte,
 *    respectively, and the estimated power cores 0 and 1, adjusted using the power offsets in the
 *    adjPwrLut, are in the 3rd MSbyte and the LSbyte, respectively. When estimated power or
 *    adjusted estimated powerd is not valid, 0x80 is returned for that core
 */
static uint32
wlc_phy_txpower_est_power_nphy(phy_info_t *pi)
{
	int16 tx0_status, tx1_status;
	uint16 estPower1, estPower2;
	uint8 pwr0, pwr1, adj_pwr0, adj_pwr1;
	uint32 est_pwr;

	/* Read the Actual Estimated Powers without adjustment */
	estPower1 = read_phy_reg(pi, NPHY_EstPower1);
	estPower2 = read_phy_reg(pi, NPHY_EstPower2);

	if ((estPower1 & NPHY_EstPower1_estPowerValid_MASK)
	       == NPHY_EstPower1_estPowerValid_MASK) {
		pwr0 = (uint8) (estPower1 & NPHY_EstPower1_estPower_MASK)
		                      >> NPHY_EstPower1_estPower_SHIFT;
	} else {
		pwr0 = 0x80;
	}

	if ((estPower2 & NPHY_EstPower2_estPowerValid_MASK)
	       == NPHY_EstPower2_estPowerValid_MASK) {
		pwr1 = (uint8) (estPower2 & NPHY_EstPower2_estPower_MASK)
		                      >> NPHY_EstPower2_estPower_SHIFT;
	} else {
		pwr1 = 0x80;
	}

	/* Read the Adjusted Estimated Powers */
	tx0_status = read_phy_reg(pi, NPHY_Core0TxPwrCtrlStatus);
	tx1_status = read_phy_reg(pi, NPHY_Core1TxPwrCtrlStatus);

	if ((tx0_status & NPHY_Core1TxPwrCtrlStatus_estPwrValid_MASK)
	       == NPHY_Core1TxPwrCtrlStatus_estPwrValid_MASK) {
		adj_pwr0 = (uint8) (tx0_status & NPHY_Core1TxPwrCtrlStatus_estPwr_MASK)
		                      >> NPHY_Core1TxPwrCtrlStatus_estPwr_SHIFT;
	} else {
		adj_pwr0 = 0x80;
	}
	if ((tx1_status & NPHY_Core2TxPwrCtrlStatus_estPwrValid_MASK)
	       == NPHY_Core2TxPwrCtrlStatus_estPwrValid_MASK) {
		adj_pwr1 = (uint8) (tx1_status & NPHY_Core2TxPwrCtrlStatus_estPwr_MASK)
		                      >> NPHY_Core2TxPwrCtrlStatus_estPwr_SHIFT;
	} else {
		adj_pwr1 = 0x80;
	}

	est_pwr = (uint32) ((pwr0 << 24) | (pwr1 << 16) | (adj_pwr0 << 8) | adj_pwr1);
	return (est_pwr);
}

void
wlc_phy_txpower_get_current(wlc_phy_t *ppi, tx_power_t *power, uint channel)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint rate, num_rates;
	uint8 min_pwr, max_pwr, core;

#if WL_TX_POWER_RATES != TXP_NUM_RATES
#error "tx_power_t struct out of sync with this fn"
#endif

	if (ISNPHY(pi)) {
		power->rf_cores = 2;
		power->flags |= (WL_TX_POWER_F_MIMO);
		if (pi->nphy_txpwrctrl == PHY_TPC_HW_ON)
			power->flags |= (WL_TX_POWER_F_ENABLED | WL_TX_POWER_F_HW);
	} else if (ISHTPHY(pi)) {
		power->rf_cores = pi->pubpi.phy_corenum;
		power->flags |= (WL_TX_POWER_F_MIMO);
		if (pi->txpwrctrl == PHY_TPC_HW_ON)
			power->flags |= (WL_TX_POWER_F_ENABLED | WL_TX_POWER_F_HW);
	} else if (ISSSLPNPHY(pi)) {
		power->rf_cores = 1;
		power->flags |= (WL_TX_POWER_F_SISO);
		if (pi->radiopwr_override == RADIOPWR_OVERRIDE_DEF)
			power->flags |= WL_TX_POWER_F_ENABLED;
		if (pi->hwpwrctrl)
			power->flags |= WL_TX_POWER_F_HW;
	} else if (ISLCNPHY(pi)) {
		power->rf_cores = 1;
		power->flags |= (WL_TX_POWER_F_SISO);
		if (pi->radiopwr_override == RADIOPWR_OVERRIDE_DEF)
			power->flags |= WL_TX_POWER_F_ENABLED;
		if (pi->hwpwrctrl)
			power->flags |= WL_TX_POWER_F_HW;
	} else {
		power->rf_cores = 1;
		if (pi->radiopwr_override == RADIOPWR_OVERRIDE_DEF)
			power->flags |= WL_TX_POWER_F_ENABLED;
		if (pi->hwpwrctrl)
			power->flags |= WL_TX_POWER_F_HW;
	}

	num_rates = ((ISNPHY(pi) || ISHTPHY(pi)) ? (TXP_NUM_RATES) :
		((ISSSLPNPHY(pi) || ISLCNPHY(pi)) ?
		(TXP_LAST_OFDM_20_CDD + 1) : (TXP_LAST_OFDM + 1)));

	for (rate = 0; rate < num_rates; rate++) {
		power->user_limit[rate] = pi->tx_user_target[rate];
		wlc_phy_txpower_sromlimit(ppi, channel, &min_pwr, &max_pwr, rate);
		power->board_limit[rate] = (uint8)max_pwr;
		power->target[rate] = pi->tx_power_target[rate];
	}

	/* fill the est_Pout, max target power, and rate index corresponding to the max
	 * target power fields
	 */
	if (ISNPHY(pi)) {
		uint32 est_pout;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		est_pout = wlc_phy_txpower_est_power_nphy(pi);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);

		/* Store the adjusted  estimated powers */
		power->est_Pout[0] = (est_pout >> 8) & 0xff;
		power->est_Pout[1] = est_pout & 0xff;

		/* Store the actual estimated powers without adjustment */
		power->est_Pout_act[0] = est_pout >> 24;
		power->est_Pout_act[1] = (est_pout >> 16) & 0xff;

		/* if invalid, return 0 */
		if (power->est_Pout[0] == 0x80)
			power->est_Pout[0] = 0;
		if (power->est_Pout[1] == 0x80)
			power->est_Pout[1] = 0;

		/* if invalid, return 0 */
		if (power->est_Pout_act[0] == 0x80)
			power->est_Pout_act[0] = 0;
		if (power->est_Pout_act[1] == 0x80)
			power->est_Pout_act[1] = 0;

		power->est_Pout_cck = 0;

		/* Store the maximum target power among all rates */
		power->tx_power_max[0] = pi->tx_power_max;
		power->tx_power_max[1] = pi->tx_power_max;

		/* Store the index of the rate with the maximum target power among all rates */
		power->tx_power_max_rate_ind[0] = pi->tx_power_max_rate_ind;
		power->tx_power_max_rate_ind[1] = pi->tx_power_max_rate_ind;
	} else if (ISHTPHY(pi)) {
		/* Get power estimates */
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		wlc_phy_txpwr_est_pwr_htphy(pi, power->est_Pout, power->est_Pout_act);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);

		power->est_Pout_cck = 0;

		/* Store the maximum target power among all rates */
		FOREACH_CORE(pi, core) {
			power->tx_power_max[core] = pi->tx_power_max;
		}
	} else if (!pi->hwpwrctrl) {
		/* If sw power control, grab the stashed value */
		if (ISGPHY(pi)) {
			power->est_Pout_cck = pi->txpwr_est_Pout;
			power->est_Pout[0] = pi->txpwr_est_Pout -
				pi->tx_power_offset[TXP_FIRST_OFDM];
		}
		else if (ISAPHY(pi))
			power->est_Pout[0] = pi->txpwr_est_Pout;
	} else if (pi->sh->up) {
		/* If hw (ucode) based, read the hw based estimate in realtime */
		wlc_phyreg_enter(ppi);
		if (ISGPHY(pi)) {
			power->est_Pout_cck = read_phy_reg(pi, BPHY_TX_EST_PWR) & 0xff;
			power->est_Pout[0] =
			        read_phy_reg(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A2) & 0xff;
		} else if (ISLPPHY(pi)) {
			if (wlc_phy_tpc_isenabled_lpphy(pi))
				power->flags |= (WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);
			else
				power->flags &= ~(WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);

			wlc_phy_get_tssi_lpphy(pi, (int8*)&power->est_Pout[0],
				(int8*)&power->est_Pout_cck);
		} else if (ISSSLPNPHY(pi)) {
			/* Store the maximum target power among all rates */
			power->tx_power_max[0] = pi->tx_power_max;
			power->tx_power_max[1] = pi->tx_power_max;

			/* Store the index of the rate with the maximum target power among all
			 * rates.
			 */
			power->tx_power_max_rate_ind[0] = pi->tx_power_max_rate_ind;
			power->tx_power_max_rate_ind[1] = pi->tx_power_max_rate_ind;

			if (wlc_phy_tpc_isenabled_sslpnphy(pi))
				power->flags |= (WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);
			else
				power->flags &= ~(WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);

			wlc_sslpnphy_get_tssi(pi, (int8*)&power->est_Pout[0],
				(int8*)&power->est_Pout_cck);
		} else if (ISLCNPHY(pi)) {
			/* Store the maximum target power among all rates */
			power->tx_power_max[0] = pi->tx_power_max;
			power->tx_power_max[1] = pi->tx_power_max;

			/* Store the index of the rate with the maximum target power among all
			 * rates.
			 */
			power->tx_power_max_rate_ind[0] = pi->tx_power_max_rate_ind;
			power->tx_power_max_rate_ind[1] = pi->tx_power_max_rate_ind;

			if (wlc_phy_tpc_isenabled_lcnphy(pi))
				power->flags |= (WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);
			else
				power->flags &= ~(WL_TX_POWER_F_HW | WL_TX_POWER_F_ENABLED);

			wlc_lcnphy_get_tssi(pi, (int8*)&power->est_Pout[0],
				(int8*)&power->est_Pout_cck);
		} else if (ISAPHY(pi))
			power->est_Pout[0] = read_phy_reg(pi, APHY_RSSI_FILT_A2) & 0xff;
		wlc_phyreg_exit(ppi);
	}
}

#if defined(BCMDBG) || defined(WLTEST)
/* Return the current instantaneous est. power
 * For swpwr ctrl it's based on current TSSI value (as opposed to average)
 * Mainly used by mfg.
 */
static void
wlc_phy_txpower_get_instant(phy_info_t *pi, void *pwr)
{
	tx_inst_power_t *power = (tx_inst_power_t *)pwr;
	/* If sw power control, grab the instant value based on current TSSI Only
	 * If hw based, read the hw based estimate in realtime
	 */
	if (ISLPPHY(pi)) {
		if (!pi->hwpwrctrl)
			return;

		wlc_phy_get_tssi_lpphy(pi, (int8*)&power->txpwr_est_Pout_gofdm,
			(int8*)&power->txpwr_est_Pout[0]);
		power->txpwr_est_Pout[1] = power->txpwr_est_Pout_gofdm;

	} else if (ISSSLPNPHY(pi)) {
		if (!pi->hwpwrctrl)
			return;

		wlc_sslpnphy_get_tssi(pi, (int8*)&power->txpwr_est_Pout_gofdm,
			(int8*)&power->txpwr_est_Pout[0]);
		power->txpwr_est_Pout[1] = power->txpwr_est_Pout_gofdm;

	} else if (ISLCNPHY(pi)) {
		if (!pi->hwpwrctrl)
			return;

		wlc_lcnphy_get_tssi(pi, (int8*)&power->txpwr_est_Pout_gofdm,
			(int8*)&power->txpwr_est_Pout[0]);
		power->txpwr_est_Pout[1] = power->txpwr_est_Pout_gofdm;

	} else if (ISABGPHY(pi)) {
		wlc_phy_txpower_get_instant_abgphy(pi, pwr);
	}

}
#endif 

#if defined(BCMDBG) || defined(WLTEST)
int
wlc_phy_test_init(phy_info_t *pi, int channel, bool txpkt)
{
	if (channel > MAXCHANNEL)
		return BCME_OUTOFRANGECHAN;

	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(channel));

	/* stop any requests from the stack and prevent subsequent thread */
	pi->phytest_on = TRUE;

	if (ISLPPHY(pi)) {

		wlc_phy_init_test_lpphy(pi);

	} else if (ISSSLPNPHY(pi)) {

		wlc_phy_init_test_sslpnphy(pi);

	} else if (ISLCNPHY(pi)) {

		wlc_phy_init_test_lcnphy(pi);

	} else if (ISGPHY(pi)) {
		/* Disable rx */
		pi->tr_loss_ctl = read_phy_reg(pi, BPHY_TR_LOSS_CTL);
		pi->rf_override = read_phy_reg(pi, BPHY_RF_OVERRIDE);
		or_phy_reg(pi, BPHY_RF_OVERRIDE, (uint16)0x8000);
		wlc_synth_pu_war(pi, channel);
	}

	return 0;
}

int
wlc_phy_test_stop(phy_info_t *pi)
{
	if (pi->phytest_on == FALSE)
		return 0;

	/* stop phytest mode */
	pi->phytest_on = FALSE;

	/* For NPHY, phytest register needs to be accessed only via phy and not directly */
	if (ISNPHY(pi)) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			and_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST), 0xfc00);
			if (NREV_GE(pi->pubpi.phy_rev, 3))
				/* BPHY_DDFS_ENABLE is removed in mimophy rev 3 */
				write_phy_reg(pi, NPHY_bphytestcontrol, 0x0);
			else
				write_phy_reg(pi, NPHY_TO_BPHY_OFF + BPHY_DDFS_ENABLE, 0);
		}
	} else if (ISHTPHY(pi)) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			and_phy_reg(pi, (HTPHY_TO_BPHY_OFF + BPHY_TEST), 0xfc00);
			write_phy_reg(pi, HTPHY_bphytestcontrol, 0x0);
		}
	} else if (ISLPPHY(pi)) {
		/* Just ignore the phytest reg, it's not currently used for LPPHY */
	} else if (ISSSLPNPHY(pi)) {
		/* Do nothing */
	} else if (ISLCNPHY(pi)) {
		PHY_TRACE(("%s:***CHECK***\n", __FUNCTION__));
	} else {
		AND_REG(pi->sh->osh, &pi->regs->phytest, 0xfc00);
		write_phy_reg(pi, BPHY_DDFS_ENABLE, 0);
	}

	/* Restore these registers */
	if (ISGPHY(pi)) {
		write_phy_reg(pi, BPHY_TR_LOSS_CTL, pi->tr_loss_ctl);
		write_phy_reg(pi, BPHY_RF_OVERRIDE, pi->rf_override);
	}

	return 0;
}

/*
 * Rate is number of 500 Kb units.
 */
static int
wlc_phy_test_evm(phy_info_t *pi, int channel, uint rate, int txpwr)
{
	d11regs_t *regs = pi->regs;
	uint16 reg = 0;
	int bcmerror = 0;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);


	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		if (ISNPHY(pi))
			write_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST),
			              pi->evm_phytest);
		else if (ISLPPHY(pi)) {
			write_phy_reg(pi, LPPHY_bphyTest, pi->evm_phytest);
			write_phy_reg(pi, LPPHY_ClkEnCtrl, 0);
			wlc_phy_tx_pu_lpphy(pi, 0);
		} else if (ISSSLPNPHY(pi)) {
			write_phy_reg(pi, SSLPNPHY_bphyTest, pi->evm_phytest);
			write_phy_reg(pi, SSLPNPHY_ClkEnCtrl, 0);
			wlc_sslpnphy_tx_pu(pi, 0);
		} else if (ISLCNPHY(pi)) {
			write_phy_reg(pi, LCNPHY_bphyTest, pi->evm_phytest);
			write_phy_reg(pi, LCNPHY_ClkEnCtrl, 0);
			wlc_lcnphy_tx_pu(pi, 0);
		} else 	if (ISHTPHY(pi))
			wlc_phy_bphy_testpattern_htphy(pi, HTPHY_TESTPATTERN_BPHY_EVM, reg, FALSE);
		 else
			W_REG(pi->sh->osh, &regs->phytest, pi->evm_phytest);

		pi->evm_phytest = 0;

		/* Restore 15.6 Mhz nominal fc Tx LPF */
		if (pi->sbtml_gm && (pi->pubpi.radioid == BCM2050_ID)) {
			or_radio_reg(pi, RADIO_2050_TX_CTL0, 0x4);
		}

		if (pi->sh->boardflags & BFL_PACTRL) {
			W_REG(pi->sh->osh, &pi->regs->psm_gpio_out, pi->evm_o);
			W_REG(pi->sh->osh, &pi->regs->psm_gpio_oe, pi->evm_oe);
			OSL_DELAY(1000);
		}
		return 0;
	}

	if (pi->sh->boardflags & BFL_PACTRL) {
		PHY_INFORM(("wl%d: %s: PACTRL boardflag set, clearing gpio 0x%04x\n",
			pi->sh->unit, __FUNCTION__, BOARD_GPIO_PACTRL));
		/* Store initial values */
		pi->evm_o = R_REG(pi->sh->osh, &pi->regs->psm_gpio_out);
		pi->evm_oe = R_REG(pi->sh->osh, &pi->regs->psm_gpio_oe);
		AND_REG(pi->sh->osh, &regs->psm_gpio_out, ~BOARD_GPIO_PACTRL);
		OR_REG(pi->sh->osh, &regs->psm_gpio_oe, BOARD_GPIO_PACTRL);
		OSL_DELAY(1000);
	}

	if ((bcmerror = wlc_phy_test_init(pi, channel, TRUE)))
		return bcmerror;

	if (pi->sbtml_gm && (pi->pubpi.radioid == BCM2050_ID)) {
		/* Disable 15.6 Mhz nominal fc Tx LPF */
		and_radio_reg(pi, RADIO_2050_TX_CTL0, ~4);
	}

	reg = TST_TXTEST_RATE_2MBPS;
	switch (rate) {
	case 2:
		reg = TST_TXTEST_RATE_1MBPS;
		break;
	case 4:
		reg = TST_TXTEST_RATE_2MBPS;
		break;
	case 11:
		reg = TST_TXTEST_RATE_5_5MBPS;
		break;
	case 22:
		reg = TST_TXTEST_RATE_11MBPS;
		break;
	}
	reg = (reg << TST_TXTEST_RATE_SHIFT) & TST_TXTEST_RATE;

	PHY_INFORM(("wlc_evm: rate = %d, reg = 0x%x\n", rate, reg));

	/* Save original contents */
	if (pi->evm_phytest == 0 && !ISHTPHY(pi)) {
		if (ISNPHY(pi))
			pi->evm_phytest = read_phy_reg(pi,
			                               (NPHY_TO_BPHY_OFF + BPHY_TEST));
		else if (ISLPPHY(pi)) {
			pi->evm_phytest = read_phy_reg(pi, LPPHY_bphyTest);
			write_phy_reg(pi, LPPHY_ClkEnCtrl, 0xffff);
		} else if (ISSSLPNPHY(pi)) {
			pi->evm_phytest = read_phy_reg(pi, SSLPNPHY_bphyTest);
			write_phy_reg(pi, SSLPNPHY_ClkEnCtrl, 0xffff);
		} else if (ISLCNPHY(pi)) {
			pi->evm_phytest = read_phy_reg(pi, LCNPHY_bphyTest);
			write_phy_reg(pi, LCNPHY_ClkEnCtrl, 0xffff);
		} else
			pi->evm_phytest = R_REG(pi->sh->osh, &regs->phytest);
	}

	/* Set EVM test mode */
	if (ISHTPHY(pi)) {
		wlc_phy_bphy_testpattern_htphy(pi, NPHY_TESTPATTERN_BPHY_EVM, reg, TRUE);
	} else if (ISNPHY(pi)) {
		and_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST),
		            ~(TST_TXTEST_ENABLE|TST_TXTEST_RATE|TST_TXTEST_PHASE));
		or_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST), TST_TXTEST_ENABLE | reg);
	} else if ISLPPHY(pi) {
		if (LPREV_GE(pi->pubpi.phy_rev, 2))
			wlc_phy_tx_dig_filt_cck_setup_lpphy(pi, TRUE);
		if (pi_lp->lpphy_use_cck_dig_loft_coeffs)
			wlc_phy_set_tx_locc_lpphy(pi, pi_lp->lpphy_cal_results.didq_cck);
		wlc_phy_tx_pu_lpphy(pi, 1);
		or_phy_reg(pi, LPPHY_bphyTest, 0x128);
	} else if ISSSLPNPHY(pi) {
		wlc_sslpnphy_tx_pu(pi, 1);
		or_phy_reg(pi, SSLPNPHY_bphyTest, 0x128);
	} else if ISLCNPHY(pi) {
		wlc_lcnphy_tx_pu(pi, 1);
		or_phy_reg(pi, LCNPHY_bphyTest, 0x128);
	} else {
		AND_REG(pi->sh->osh, &regs->phytest,
		        ~(TST_TXTEST_ENABLE|TST_TXTEST_RATE|TST_TXTEST_PHASE));
		OR_REG(pi->sh->osh, &regs->phytest, TST_TXTEST_ENABLE | reg);
	}
	return 0;
}

static int
wlc_phy_test_carrier_suppress(phy_info_t *pi, int channel)
{
	d11regs_t *regs = pi->regs;
	int bcmerror = 0;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	phy_info_sslpnphy_t *pi_sslpn = pi->u.pi_sslpnphy;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);


	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		if (ISNPHY(pi)) {
			write_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST),
			              pi->car_sup_phytest);
		} else if (ISLPPHY(pi)) {
			/* Disable carrier suppression */
			write_phy_reg(pi, LPPHY_ClkEnCtrl, 0);
			and_phy_reg(pi, LPPHY_bphyTest, pi->car_sup_phytest);

			wlc_phy_tx_pu_lpphy(pi, 0);
		} else if (ISSSLPNPHY(pi)) {
			/* Disable carrier suppression */
			write_phy_reg(pi, SSLPNPHY_ClkEnCtrl, 0);
			and_phy_reg(pi, SSLPNPHY_bphyTest, pi->car_sup_phytest);
			and_phy_reg(pi, SSLPNPHY_sslpnCalibClkEnCtrl, 0xff7f);
			write_phy_reg(pi, SSLPNPHY_BphyControl3, pi_sslpn->sslpnphy_bphyctrl);
			wlc_sslpnphy_tx_pu(pi, 0);
		} else if (ISLCNPHY(pi)) {
			/* release the gpio controls from cc */
			wlc_lcnphy_epa_switch(pi, 0);
			/* Disable carrier suppression */
			wlc_lcnphy_crsuprs(pi, channel);
		} else 	if (ISHTPHY(pi)) {
			wlc_phy_bphy_testpattern_htphy(pi, HTPHY_TESTPATTERN_BPHY_RFCS, 0, FALSE);
		} else
			W_REG(pi->sh->osh, &regs->phytest, pi->car_sup_phytest);

		pi->car_sup_phytest = 0;
		return 0;
	}

	if ((bcmerror = wlc_phy_test_init(pi, channel, TRUE)))
		return bcmerror;

	/* Save original contents */
	if (pi->car_sup_phytest == 0 && !ISHTPHY(pi)) {
	        if (ISNPHY(pi)) {
			pi->car_sup_phytest = read_phy_reg(pi,
			                                   (NPHY_TO_BPHY_OFF + BPHY_TEST));
		} else if (ISLPPHY(pi)) {
			pi->car_sup_phytest = read_phy_reg(pi, LPPHY_bphyTest);
		} else if (ISSSLPNPHY(pi)) {
			pi->car_sup_phytest = read_phy_reg(pi, SSLPNPHY_bphyTest);
			pi_sslpn->sslpnphy_bphyctrl = read_phy_reg(pi, SSLPNPHY_BphyControl3);
		} else if (ISLCNPHY(pi)) {
			pi->car_sup_phytest = read_phy_reg(pi, LCNPHY_bphyTest);
		} else
			pi->car_sup_phytest = R_REG(pi->sh->osh, &regs->phytest);
	}

	/* set carrier suppression test mode */
	if (ISHTPHY(pi)) {
		wlc_phy_bphy_testpattern_htphy(pi, HTPHY_TESTPATTERN_BPHY_RFCS, 0, TRUE);
	} else if (ISNPHY(pi)) {
		and_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST), 0xfc00);
		or_phy_reg(pi, (NPHY_TO_BPHY_OFF + BPHY_TEST), 0x0228);
	} else if (ISLPPHY(pi)) {
		if (LPREV_GE(pi->pubpi.phy_rev, 2))
			wlc_phy_tx_dig_filt_cck_setup_lpphy(pi, TRUE);
		if (pi_lp->lpphy_use_cck_dig_loft_coeffs)
			wlc_phy_set_tx_locc_lpphy(pi, pi_lp->lpphy_cal_results.didq_cck);
		wlc_phy_tx_pu_lpphy(pi, 1);

		/* Enable carrier suppression */
		write_phy_reg(pi, LPPHY_ClkEnCtrl, 0xffff);
		or_phy_reg(pi, LPPHY_bphyTest, 0x228);
	} else if (ISSSLPNPHY(pi)) {
		wlc_sslpnphy_tx_pu(pi, 1);

		/* Enable carrier suppression */
		write_phy_reg(pi, SSLPNPHY_ClkEnCtrl, 0xffff);
		or_phy_reg(pi, SSLPNPHY_bphyTest, 0x228);
	} else if (ISLCNPHY(pi)) {
		/* get the gpio controls to cc */
		wlc_lcnphy_epa_switch(pi, 1);
		wlc_lcnphy_crsuprs(pi, channel);
	} else {
		AND_REG(pi->sh->osh, &regs->phytest, 0xfc00);
		OR_REG(pi->sh->osh, &regs->phytest, 0x0228);
	}

	return 0;
}

static int
wlc_phy_test_freq_accuracy(phy_info_t *pi, int channel)
{
	int bcmerror = 0;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);

	/* channel 0 means this is a request to end the test */
	if (channel == 0) {
		/* Restore original values */
		if (ISNPHY(pi)) {
			if ((bcmerror = wlc_phy_freq_accuracy_nphy(pi, channel)) != BCME_OK)
				return bcmerror;
		} else if (ISHTPHY(pi)) {
			if ((bcmerror = wlc_phy_freq_accuracy_htphy(pi, channel)) != BCME_OK)
				return bcmerror;
		} else if (ISLPPHY(pi)) {
			wlc_phy_stop_tx_tone_lpphy(pi);
			wlc_phy_clear_deaf_lpphy(pi, (bool)1);
		} else if (ISSSLPNPHY(pi)) {
			wlc_sslpnphy_stop_tx_tone(pi);
			wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_stop_tx_tone(pi);
			if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)
				wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_TEMPBASED);
			else
				wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
			wlc_lcnphy_epa_switch(pi, 0);
		} else if (ISABGPHY(pi)) {
			wlc_phy_test_freq_accuracy_prep_abgphy(pi);
		}

		return 0;
	}

	if ((bcmerror = wlc_phy_test_init(pi, channel, FALSE)))
		return bcmerror;

	if (ISNPHY(pi)) {
		if ((bcmerror = wlc_phy_freq_accuracy_nphy(pi, channel)) != BCME_OK)
			return bcmerror;
	} else if (ISHTPHY(pi)) {
		if ((bcmerror = wlc_phy_freq_accuracy_htphy(pi, channel)) != BCME_OK)
			return bcmerror;
	} else if (ISLPPHY(pi)) {
		wlc_phy_set_deaf_lpphy(pi, (bool)1);
		wlc_phy_start_tx_tone_lpphy(pi, 0, LPREV_GE(pi->pubpi.phy_rev, 2) ? 28: 100);
	} else if (ISSSLPNPHY(pi)) {
		wlc_sslpnphy_start_tx_tone(pi, 0, 112, 0);
		wlc_sslpnphy_set_tx_pwr_by_index(pi, (int)20);
	} else if (ISLCNPHY(pi)) {
		/* get the gpio controls to cc */
		wlc_lcnphy_epa_switch(pi, 1);
		wlc_lcnphy_start_tx_tone(pi, 0, 112, 0);
		wlc_lcnphy_set_tx_pwr_by_index(pi, (int)94);
	} else if (ISABGPHY(pi)) {
		wlc_phy_test_freq_accuracy_run_abgphy(pi);
	}

	return 0;
}

#endif 

void
wlc_phy_antsel_type_set(wlc_phy_t *ppi, uint8 antsel_type)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	pi->antsel_type = antsel_type;
}

bool
wlc_phy_test_ison(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	return (pi->phytest_on);
}

bool
wlc_phy_ant_rxdiv_get(wlc_phy_t *ppi, uint8 *pval)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	*pval = pi->sh->rx_antdiv;
	return TRUE;
}

void
wlc_phy_ant_rxdiv_set(wlc_phy_t *ppi, uint8 val)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	bool suspend;
	uint16 bt_shm_addr;

	pi->sh->rx_antdiv = val;

	if (ISHTPHY(pi))
		return;	/* no need to set phy reg for htphy */

	/* update ucode flag for non-4322(phy has antdiv by default) */
	if (!(ISNPHY(pi) && D11REV_IS(pi->sh->corerev, 16))) {
		if (val > ANT_RX_DIV_FORCE_1)
			wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_ANTDIV, MHF1_ANTDIV,
				WLC_BAND_ALL);
		else
			wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_ANTDIV, 0, WLC_BAND_ALL);
	}

	if (ISNPHY(pi))
		return;	/* no need to set phy reg for nphy */

	if (!pi->sh->clk)
		return;

	suspend = (0 == (R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	if (ISLPPHY(pi)) {
		if (val > ANT_RX_DIV_FORCE_1) {
			mod_phy_reg(pi, LPPHY_crsgainCtrl,
				LPPHY_crsgainCtrl_DiversityChkEnable_MASK,
				0x01 << LPPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
			mod_phy_reg(pi, LPPHY_crsgainCtrl,
				LPPHY_crsgainCtrl_DefaultAntenna_MASK,
				((ANT_RX_DIV_START_1 == val) ? 1 : 0) <<
				LPPHY_crsgainCtrl_DefaultAntenna_SHIFT);
		} else {
			mod_phy_reg(pi, LPPHY_crsgainCtrl,
				LPPHY_crsgainCtrl_DiversityChkEnable_MASK,
				0x00 << LPPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
			mod_phy_reg(pi, LPPHY_crsgainCtrl,
				LPPHY_crsgainCtrl_DefaultAntenna_MASK,
				(uint16)val << LPPHY_crsgainCtrl_DefaultAntenna_SHIFT);
		}
	} else if (ISSSLPNPHY(pi)) {
		if (val > ANT_RX_DIV_FORCE_1) {
			if (CHSPEC_IS40(pi->radio_chanspec)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
				SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_MASK,
				0x01 << SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
					SSLPNPHY_Rev2_crsgainCtrl_40_DefaultAntenna_MASK,
					((ANT_RX_DIV_START_1 == val) ? 1 : 0) <<
					SSLPNPHY_Rev2_crsgainCtrl_40_DefaultAntenna_SHIFT);
			} else {
				mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DiversityChkEnable_MASK,
					0x01 << SSLPNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DefaultAntenna_MASK,
					((ANT_RX_DIV_START_1 == val) ? 1 : 0) <<
					SSLPNPHY_crsgainCtrl_DefaultAntenna_SHIFT);
			}
		} else {
			if (CHSPEC_IS40(pi->radio_chanspec)) {
				mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
				SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_MASK,
				0x00 << SSLPNPHY_Rev2_crsgainCtrl_40_DiversityChkEnable_SHIFT);

				mod_phy_reg(pi, SSLPNPHY_Rev2_crsgainCtrl_40,
				SSLPNPHY_Rev2_crsgainCtrl_40_DefaultAntenna_MASK,
				(uint16)val << SSLPNPHY_Rev2_crsgainCtrl_40_DefaultAntenna_SHIFT);
			} else {
				mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DiversityChkEnable_MASK,
					0x00 << SSLPNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
				mod_phy_reg(pi, SSLPNPHY_crsgainCtrl,
					SSLPNPHY_crsgainCtrl_DefaultAntenna_MASK,
					(uint16)val << SSLPNPHY_crsgainCtrl_DefaultAntenna_SHIFT);
			}
		}
				/* Reset radio ctrl and crs gain */
		or_phy_reg(pi, SSLPNPHY_resetCtrl, 0x44);
		write_phy_reg(pi, SSLPNPHY_resetCtrl, 0x80);

		/* Get pointer to the BTCX shm block */
		if ((bt_shm_addr = 2 * wlapi_bmac_read_shm(pi->sh->physhim, M_BTCX_BLK_PTR)))
			wlapi_bmac_write_shm(pi->sh->physhim,
				bt_shm_addr + M_BTCX_DIVERSITY_SAVE, 0);

	} else if (ISLCNPHY(pi)) {
		if (val > ANT_RX_DIV_FORCE_1) {
			mod_phy_reg(pi, LCNPHY_crsgainCtrl,
				LCNPHY_crsgainCtrl_DiversityChkEnable_MASK,
				0x01 << LCNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
			mod_phy_reg(pi, LCNPHY_crsgainCtrl,
				LCNPHY_crsgainCtrl_DefaultAntenna_MASK,
				((ANT_RX_DIV_START_1 == val) ? 1 : 0) <<
				LCNPHY_crsgainCtrl_DefaultAntenna_SHIFT);
		} else {
			mod_phy_reg(pi, LCNPHY_crsgainCtrl,
				LCNPHY_crsgainCtrl_DiversityChkEnable_MASK,
				0x00 << LCNPHY_crsgainCtrl_DiversityChkEnable_SHIFT);
			mod_phy_reg(pi, LCNPHY_crsgainCtrl,
				LCNPHY_crsgainCtrl_DefaultAntenna_MASK,
				(uint16)val << LCNPHY_crsgainCtrl_DefaultAntenna_SHIFT);
		}
	} else if (ISABGPHY(pi)) {
		wlc_phy_ant_rxdiv_set_abgphy(pi, val);
	} else {
		PHY_ERROR(("wl%d: %s: PHY_TYPE= %d is Unsupported ",
		          pi->sh->unit, __FUNCTION__, pi->pubpi.phy_type));
		ASSERT(0);
	}

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	return;
}


void
wlc_phy_interference_set(wlc_phy_t *pih, bool init)
{
	int wanted_mode;
	phy_info_t *pi = (phy_info_t *)pih;

	if (ISHTPHY(pi))
		return;

	if (!ISNPHY(pi))
		return;

	if (pi->sh->interference_mode_override == TRUE) {
		/* keep the same values */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wanted_mode = pi->sh->interference_mode_2G_override;
		} else {
			wanted_mode = pi->sh->interference_mode_5G_override;
		}
	} else {

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			wanted_mode = pi->sh->interference_mode_2G;
		} else {
			wanted_mode = pi->sh->interference_mode_5G;
		}
	}

	if (CHSPEC_CHANNEL(pi->radio_chanspec) != pi->interf.curr_home_channel) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

		wlc_phy_interference(pi, wanted_mode, init);
		pi->sh->interference_mode = wanted_mode;

		wlapi_enable_mac(pi->sh->physhim);
	}
}

/* %%%%%% interference */
LOCATOR_EXTERN bool
wlc_phy_interference(phy_info_t *pi, int wanted_mode, bool init)
{
	if (init) {
		pi->interference_mode_crs_time = 0;
		pi->crsglitch_prev = 0;
		if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
			/* clear out all the state */
			wlc_phy_noisemode_reset_nphy(pi);
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				wlc_phy_acimode_reset_nphy(pi);
			}
		}
	}

	if (ISHTPHY(pi))
		return TRUE;

	/* NPHY 5G, supported for NON_WLAN and INTERFERE_NONE only */
	if (ISGPHY(pi) || ISLPPHY(pi) ||
		(ISNPHY(pi) &&
		(CHSPEC_IS2G(pi->radio_chanspec) ||
		(CHSPEC_IS5G(pi->radio_chanspec) &&
	         (wanted_mode == NON_WLAN || wanted_mode == INTERFERE_NONE))))) {
		if (wanted_mode == INTERFERE_NONE) {	/* disable */

			switch (pi->cur_interference_mode) {
			case WLAN_AUTO:
			case WLAN_AUTO_W_NOISE:
			case WLAN_MANUAL:
				if (ISLPPHY(pi))
					wlc_phy_aci_enable_lpphy(pi, FALSE);
				else if (ISNPHY(pi) &&
					CHSPEC_IS2G(pi->radio_chanspec)) {
					wlc_phy_acimode_set_nphy(pi, FALSE,
						PHY_ACI_PWR_NOTPRESENT);
				} else if (ISGPHY(pi)) {
					wlc_phy_aci_ctl_gphy(pi, FALSE);
				}
				pi->aci_state &= ~ACI_ACTIVE;
				break;
			case NON_WLAN:
				if (ISNPHY(pi)) {
					if (NREV_GE(pi->pubpi.phy_rev, 3) &&
						CHSPEC_IS2G(pi->radio_chanspec)) {
						wlc_phy_acimode_set_nphy(pi,
							FALSE,
							PHY_ACI_PWR_NOTPRESENT);
						pi->aci_state &= ~ACI_ACTIVE;
					}
				} else
				{
					pi->interference_mode_crs = 0;
					if (ISABGPHY(pi))
						wlc_phy_aci_interf_nwlan_set_gphy(pi, FALSE);

				}
				break;
			}
		} else {	/* Enable */
			switch (wanted_mode) {
			case NON_WLAN:
				if (ISNPHY(pi)) {
					if (!NREV_GE(pi->pubpi.phy_rev, 3)) {
						PHY_ERROR(("NON_WLAN not supported for NPHY\n"));
					}
				} else {
					pi->interference_mode_crs = 1;
					if (ISABGPHY(pi)) {
						wlc_phy_aci_interf_nwlan_set_gphy(pi, TRUE);
					}

				}
				break;
			case WLAN_AUTO:
			case WLAN_AUTO_W_NOISE:
				/* fall through */
				if (((pi->aci_state & ACI_ACTIVE) != 0) || ISNPHY(pi))
					break;
				if (ISLPPHY(pi)) {
				  wlc_phy_aci_enable_lpphy(pi, FALSE);
				  break;
				}
				/* FALLTHRU */
			case WLAN_MANUAL:
				if (ISLPPHY(pi))
					wlc_phy_aci_enable_lpphy(pi, TRUE);
				else if (ISNPHY(pi)) {
					if (CHSPEC_IS2G(pi->radio_chanspec)) {
						wlc_phy_acimode_set_nphy(pi, TRUE,
							PHY_ACI_PWR_HIGH);
					}
				} else {
					wlc_phy_aci_ctl_gphy(pi, TRUE);
					break;
				}
			}
		}
	}

	pi->cur_interference_mode = wanted_mode;
	return TRUE;
}

static void
wlc_phy_aci_enter(phy_info_t *pi)
{
	/* There are cases when the glitch count is continuously high
	 * but there is no ACI.  In this case we want to wait 'countdown' secs
	 * between scans
	 */
	ASSERT((pi->aci_state & ACI_ACTIVE) == 0);

	/* If we have glitches see if they are caused by ACI */
	if (pi->interf.aci.glitch_ma > pi->interf.aci.enter_thresh) {

		/* If we find ACI on our channel, go into ACI avoidance */
		if (pi->interf.aci.countdown == 0) {	/* Is it time to check? */
			wlapi_suspend_mac_and_wait(pi->sh->physhim);

			if (ISGPHY(pi) && wlc_phy_aci_scan_gphy(pi)) {
				pi->aci_start_time = pi->sh->now;
				wlc_phy_aci_ctl_gphy(pi, TRUE);
				pi->interf.aci.countdown = 0;
			} else {
				/* Glitch count is high but no ACI, so wait
				 * hi_glitch_delay seconds before checking for ACI again.
				 */
				pi->interf.aci.countdown = pi->interf.aci.glitch_delay + 1;
			}
			wlapi_enable_mac(pi->sh->physhim);
		}
		if (pi->interf.aci.countdown)
			pi->interf.aci.countdown--;
	} else {
		pi->interf.aci.countdown = 0;	/* no glitches so cancel glitchdelay */
	}
}

static void
wlc_phy_aci_exit(phy_info_t *pi)
{
	if (pi->interf.aci.glitch_ma < pi->interf.aci.exit_thresh) {

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		if (!ISGPHY(pi) || !wlc_phy_aci_scan_gphy(pi)) {
			PHY_CAL(("%s: Absence of ACI, exiting ACI\n", __FUNCTION__));
			pi->aci_start_time = 0;
			wlc_phy_aci_ctl_gphy(pi, FALSE);
		} else {
			PHY_CAL(("%s: ACI is present - remain in ACI mode\n", __FUNCTION__));
		}
		wlapi_enable_mac(pi->sh->physhim);
	}
}

/* update aci rx carrier sense glitch moving average */
static void
wlc_phy_aci_update_ma(phy_info_t *pi)
{
	int cur_glitch_cnt;
	uint16 delta;
	uint offset;
	int16 bphy_cur_glitch_cnt = 0;
	uint16 bphy_delta = 0;
	uint16 ofdm_delta = 0;
	uint16 badplcp_delta = 0;
	int16 cur_badplcp_cnt = 0;
	int16 bphy_cur_badplcp_cnt = 0;
	uint16 bphy_badplcp_delta = 0;
	uint16 ofdm_badplcp_delta = 0;

	/* determine delta number of rxcrs glitches */
	offset = M_UCODE_MACSTAT + OFFSETOF(macstat_t, rxcrsglitch);
	cur_glitch_cnt = wlapi_bmac_read_shm(pi->sh->physhim, offset);
	delta = cur_glitch_cnt - pi->interf.aci.pre_glitch_cnt;
	pi->interf.aci.pre_glitch_cnt = cur_glitch_cnt;

	if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {

		/* compute the rxbadplcp  */
		offset = M_UCODE_MACSTAT + OFFSETOF(macstat_t, rxbadplcp);
		cur_badplcp_cnt = wlapi_bmac_read_shm(pi->sh->physhim, offset);
		badplcp_delta = cur_badplcp_cnt - pi->interf.pre_badplcp_cnt;
		pi->interf.pre_badplcp_cnt = cur_badplcp_cnt;

		/* determine delta number of bphy rx crs glitches */
		offset = M_UCODE_MACSTAT + OFFSETOF(macstat_t, bphy_rxcrsglitch);
		bphy_cur_glitch_cnt = wlapi_bmac_read_shm(pi->sh->physhim, offset);
		bphy_delta = bphy_cur_glitch_cnt - pi->interf.noise.bphy_pre_glitch_cnt;
		pi->interf.noise.bphy_pre_glitch_cnt = bphy_cur_glitch_cnt;

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			/* ofdm glitches is what we will be using */
			ofdm_delta = delta - bphy_delta;
		} else {
			ofdm_delta = delta;
		}

		/* compute bphy rxbadplcp */
#ifdef BADPLCP_UCODE_SUPPORT
		offset = M_UCODE_MACSTAT + OFFSETOF(macstat_t, bphy_badplcp);
		bphy_cur_badplcp_cnt = wlapi_bmac_read_shm(pi->sh->physhim, offset);
#else
		bphy_cur_badplcp_cnt = 0;
#endif /* BADPLCP_UCODE_SUPPORT */
		bphy_badplcp_delta = bphy_cur_badplcp_cnt -
			pi->interf.bphy_pre_badplcp_cnt;
		pi->interf.bphy_pre_badplcp_cnt = bphy_cur_badplcp_cnt;

		/* ofdm bad plcps is what we will be using */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			ofdm_badplcp_delta = badplcp_delta - bphy_badplcp_delta;
		} else {
			ofdm_badplcp_delta = badplcp_delta;
		}
	}

	if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
		/* if we aren't suppose to update yet, don't */
		if (pi->interf.scanroamtimer != 0) {
			return;
		}

	}

	/* evict old value */
	pi->interf.aci.ma_total -= pi->interf.aci.ma_list[pi->interf.aci.ma_index];

	/* admit new value */
	pi->interf.aci.ma_total += delta;
	pi->interf.aci.glitch_ma_previous = pi->interf.aci.glitch_ma;
	pi->interf.aci.glitch_ma = pi->interf.aci.ma_total / MA_WINDOW_SZ;

	pi->interf.aci.ma_list[pi->interf.aci.ma_index++] = delta;
	if (pi->interf.aci.ma_index >= MA_WINDOW_SZ)
		pi->interf.aci.ma_index = 0;

	if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->interf.badplcp_ma_total -=
			pi->interf.badplcp_ma_list[pi->interf.badplcp_ma_index];
		pi->interf.badplcp_ma_total += badplcp_delta;
		pi->interf.badplcp_ma_previous = pi->interf.badplcp_ma;
		pi->interf.badplcp_ma =
			pi->interf.badplcp_ma_total / MA_WINDOW_SZ;

		pi->interf.badplcp_ma_list[pi->interf.badplcp_ma_index++] =
			badplcp_delta;
		if (pi->interf.badplcp_ma_index >= MA_WINDOW_SZ)
			pi->interf.badplcp_ma_index = 0;

		pi->interf.noise.ofdm_ma_total -=
			pi->interf.noise.ofdm_glitch_ma_list[pi->interf.noise.ofdm_ma_index];
		pi->interf.noise.ofdm_ma_total += ofdm_delta;
		pi->interf.noise.ofdm_glitch_ma_previous = pi->interf.noise.ofdm_glitch_ma;
		pi->interf.noise.ofdm_glitch_ma =
			pi->interf.noise.ofdm_ma_total / PHY_NOISE_MA_WINDOW_SZ;

		pi->interf.noise.ofdm_glitch_ma_list[pi->interf.noise.ofdm_ma_index++] = ofdm_delta;
		if (pi->interf.noise.ofdm_ma_index >= PHY_NOISE_MA_WINDOW_SZ)
			pi->interf.noise.ofdm_ma_index = 0;


		pi->interf.noise.ofdm_badplcp_ma_total -=
		pi->interf.noise.ofdm_badplcp_ma_list[pi->interf.noise.ofdm_badplcp_ma_index];

		pi->interf.noise.ofdm_badplcp_ma_total += ofdm_badplcp_delta;
		pi->interf.noise.ofdm_badplcp_ma_previous = pi->interf.noise.ofdm_badplcp_ma;
		pi->interf.noise.ofdm_badplcp_ma =
			pi->interf.noise.ofdm_badplcp_ma_total / PHY_NOISE_MA_WINDOW_SZ;

		pi->interf.noise.ofdm_badplcp_ma_list[pi->interf.noise.ofdm_badplcp_ma_index++] =
			ofdm_badplcp_delta;
		if (pi->interf.noise.ofdm_badplcp_ma_index >= PHY_NOISE_MA_WINDOW_SZ)
			pi->interf.noise.ofdm_badplcp_ma_index = 0;
	}

	if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3) &&
		(pi->sh->interference_mode == WLAN_AUTO_W_NOISE ||
		pi->sh->interference_mode == NON_WLAN)) {
		PHY_ACI(("wlc_phy_aci_update_ma: ACI= %s, rxglitch_ma= %d,"
			" badplcp_ma= %d, ofdm_glitch_ma= %d,"
			" ofdm_badplcp_ma= %d, crsminpwr index= %d,"
			" init gain= 0x%x, channel= %d\n",
			(pi->aci_state & ACI_ACTIVE) ? "Active" : "Inactive",
			pi->interf.aci.glitch_ma,
			pi->interf.badplcp_ma,
			pi->interf.noise.ofdm_glitch_ma,
			pi->interf.noise.ofdm_badplcp_ma,
			pi->interf.crsminpwr_index,
			pi->interf.init_gain_core1, CHSPEC_CHANNEL(pi->radio_chanspec)));
	} else {
		PHY_ACI(("wlc_phy_aci_update_ma: ave glitch %d, ACI is %s, delta is %d\n",
		pi->interf.aci.glitch_ma,
		(pi->aci_state & ACI_ACTIVE) ? "Active" : "Inactive", delta));
	}
}

static void
wlc_phy_aci_upd(phy_info_t *pi)
{
	if (ISHTPHY(pi))
		return;

	wlc_phy_aci_update_ma(pi);

	switch (pi->sh->interference_mode) {

		case NON_WLAN:
			/* NON_WLAN NPHY */
			if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
				/* run noise mitigation only */
				wlc_phy_noisemode_upd_nphy(pi);
			}
			break;
		case WLAN_AUTO:

			if (ISGPHY(pi)) {
				/* Attempt to enter ACI mode if not already active */
				if (!(pi->aci_state & ACI_ACTIVE)) {
					wlc_phy_aci_enter(pi);
				} else {
					if (((pi->sh->now - pi->aci_start_time) %
						pi->aci_exit_check_period) == 0) {
						wlc_phy_aci_exit(pi);
					}
				}
			}
			else if (ISLPPHY(pi)) {
				wlc_phy_aci_upd_lpphy(pi);
			}
			else if (ISNPHY(pi)) {
				if (ASSOC_INPROG_PHY(pi))
					break;

				/* 5G band not supported yet */
				if (CHSPEC_IS5G(pi->radio_chanspec))
					break;

				if (PUB_NOT_ASSOC(pi)) {
					/* not associated:  do not run aci routines */
					break;
				}

				/* Attempt to enter ACI mode if not already active */
				/* only run this code if associated */
				if (!(pi->aci_state & ACI_ACTIVE)) {
					if ((pi->sh->now  % NPHY_ACI_CHECK_PERIOD) == 0) {
						PHY_ACI(("Interf Mode 3,"
							" pi->interf.aci.glitch_ma = %d\n",
							pi->interf.aci.glitch_ma));
						if (pi->interf.aci.glitch_ma >=
							pi->interf.aci.enter_thresh) {
							wlc_phy_acimode_upd_nphy(pi);
						}
					}
				} else {
					if (((pi->sh->now - pi->aci_start_time) %
						pi->aci_exit_check_period) == 0)
						wlc_phy_acimode_upd_nphy(pi);
				}
			}
			break;
		case WLAN_AUTO_W_NOISE:
			if (ISNPHY(pi)) {
				/* 5G band not supported yet */
				if (CHSPEC_IS5G(pi->radio_chanspec))
					break;


				/* only do this for 4322 and future revs */
				if (NREV_GE(pi->pubpi.phy_rev, 3)) {
					/* Attempt to enter ACI mode if not already active */
					wlc_phy_aci_noise_upd_nphy(pi);
				}
			}
			break;

		default:
			break;
	}
}

static void
wlc_phy_noise_calc(phy_info_t *pi, uint32 *cmplx_pwr, int8 *pwr_ant)
{
	int8 cmplx_pwr_dbm[PHY_CORE_MAX];
	uint8 i;

	bzero((uint8 *)cmplx_pwr_dbm, sizeof(cmplx_pwr_dbm));
	ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);

	wlc_phy_compute_dB(cmplx_pwr, cmplx_pwr_dbm, pi->pubpi.phy_corenum);

	FOREACH_CORE(pi, i) {
		if (NREV_GE(pi->pubpi.phy_rev, 3))
			cmplx_pwr_dbm[i] += (int8) PHY_NOISE_OFFSETFACT_4322;
		else
		/* assume init gain 70 dB, 128 maps to 1V so 10*log10(128^2*2/128/128/50)+30=16 dBm
		 *  WARNING: if the nphy init gain is ever changed, this formula needs to be updated
		 */
		cmplx_pwr_dbm[i] += (int8)(16 - (15) * 3 - 70);

		pwr_ant[i] = cmplx_pwr_dbm[i];
		PHY_INFORM(("wlc_phy_noise_calc_phy: pwr_ant[%d] = %d\n", i, pwr_ant[i]));
	}

	PHY_INFORM(("%s: samples %d ant %d\n", __FUNCTION__, pi->rxiq_samps, pi->rxiq_antsel));
}

static void
wlc_phy_noise_save(phy_info_t *pi, int8 *noise_dbm_ant, int8 *max_noise_dbm)
{
	uint8 i;

	FOREACH_CORE(pi, i) {
		/* save noise per core */
		pi->phy_noise_win[i][pi->phy_noise_index] = noise_dbm_ant[i];

		/* save the MAX for all cores */
		if (noise_dbm_ant[i] > *max_noise_dbm)
			*max_noise_dbm = noise_dbm_ant[i];
	}
	pi->phy_noise_index = MODINC_POW2(pi->phy_noise_index, PHY_NOISE_WINDOW_SZ);
}

static void
wlc_phy_rx_iq_est_aphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
	uint8 wait_time, uint8 wait_for_crs)
{
	uint16 offset = (ISAPHY(pi)) ? 0 : GPHY_TO_APHY_OFF;

	/* Get Rx IQ Imbalance Estimate from modem */
	write_phy_reg(pi, (offset+APHY_IqestNumSamps), num_samps);
	mod_phy_reg(pi, (offset+APHY_IqestEnWaitTime), APHY_IqEnWaitTime_waitTime_MASK,
		(wait_time << APHY_IqEnWaitTime_waitTime_SHIFT));
	mod_phy_reg(pi, (offset+APHY_IqestEnWaitTime), APHY_IqMode,
		((wait_for_crs) ? APHY_IqMode : 0));
	mod_phy_reg(pi, (offset+APHY_IqestEnWaitTime), APHY_IqStart, APHY_IqStart);

	/* wait for estimate */
	SPINWAIT(((read_phy_reg(pi, offset+APHY_IqestEnWaitTime) & APHY_IqStart) != 0), 10000);
	ASSERT((read_phy_reg(pi, offset+APHY_IqestEnWaitTime) & APHY_IqStart) == 0);

	if ((read_phy_reg(pi, offset+APHY_IqestEnWaitTime) & APHY_IqStart) == 0) {
		est[0].i_pwr = (read_phy_reg(pi, offset+APHY_IqestIpwrAccHi) << 16) |
			read_phy_reg(pi, offset+APHY_IqestIpwrAccLo);
		est[0].q_pwr = (read_phy_reg(pi, offset+APHY_IqestQpwrAccHi) << 16) |
			read_phy_reg(pi, offset+APHY_IqestQpwrAccLo);
		est[0].iq_prod = (read_phy_reg(pi, offset+APHY_IqestIqAccHi) << 16) |
			read_phy_reg(pi, offset+APHY_IqestIqAccLo);
		PHY_CAL(("i_pwr = %u, q_pwr = %u, iq_prod = %d\n",
			est[0].i_pwr, est[0].q_pwr, est[0].iq_prod));
	} else {
		PHY_ERROR(("wlc_phy_rx_iq_est_aphy: IQ measurement timed out\n"));
	}
}

static uint32
wlc_phy_rx_iq_est(phy_info_t *pi, uint8 samples, uint8 antsel, uint8 wait_for_crs)
{
	phy_iq_est_t est[PHY_CORE_MAX];
	uint32 cmplx_pwr[PHY_CORE_MAX];
	int8 noise_dbm_ant[PHY_CORE_MAX];
	uint16 log_num_samps, num_samps;
	uint16 classif_state = 0;
	uint16 org_antsel;
	uint8 wait_time = 32;
	bool sampling_in_progress = (pi->phynoise_state != 0);
	uint8 i;
	uint32 result = 0;

	if (sampling_in_progress) {
		PHY_INFORM(("wlc_phy_rx_iq_est: sampling_in_progress %d\n", sampling_in_progress));
		return 0;
	}

	pi->phynoise_state |= PHY_NOISE_STATE_MON;
	/* choose num_samps to be some power of 2 */
	log_num_samps = samples;
	num_samps = 1 << log_num_samps;

	bzero((uint8 *)est, sizeof(est));
	bzero((uint8 *)cmplx_pwr, sizeof(cmplx_pwr));
	bzero((uint8 *)noise_dbm_ant, sizeof(noise_dbm_ant));

	/* get IQ power measurements */
	if (ISNPHY(pi)) {
		classif_state = wlc_phy_classifier_nphy(pi, 0, 0);
		wlc_phy_classifier_nphy(pi, 3, 0);
		/* get IQ power measurements */
		wlc_phy_rx_iq_est_nphy(pi, est, num_samps, wait_time, wait_for_crs);
		/* restore classifier settings and reenable MAC ASAP */
		wlc_phy_classifier_nphy(pi, NPHY_ClassifierCtrl_classifierSel_MASK,
			classif_state);
	} else if (ISHTPHY(pi)) {
		classif_state = wlc_phy_classifier_htphy(pi, 0, 0);
		wlc_phy_classifier_htphy(pi, 3, 0);
		/* get IQ power measurements */
		wlc_phy_rx_iq_est_htphy(pi, est, num_samps, wait_time, wait_for_crs);
		/* restore classifier settings and reenable MAC ASAP */
		wlc_phy_classifier_htphy(pi, HTPHY_ClassifierCtrl_classifierSel_MASK,
			classif_state);
	} else if (ISLPPHY(pi)) {
		wlc_phy_set_deaf_lpphy(pi, (bool)0);
		noise_dbm_ant[0] = (int8)wlc_phy_rx_signal_power_lpphy(pi, 20);
		wlc_phy_clear_deaf_lpphy(pi, (bool)0);
		PHY_CAL(("channel = %d noise pwr=%d\n",
			(int)CHSPEC_CHANNEL(pi->radio_chanspec), noise_dbm_ant[0]));

		pi->phynoise_state &= ~PHY_NOISE_STATE_MON;
		return (noise_dbm_ant[0] & 0xff);
	} else if (ISSSLPNPHY(pi)) {
		PHY_ERROR(("wlc_phy_rx_iq_est: SSLPNPHY not supported yet\n"));
		pi->phynoise_state &= ~PHY_NOISE_STATE_MON;
		return 0;
	} else if (ISLCNPHY(pi)) {
		PHY_ERROR(("wlc_phy_rx_iq_est: LCNPHY not supported yet\n"));
		pi->phynoise_state &= ~PHY_NOISE_STATE_MON;
		return 0;
	} else {	/* APHY, GPHY */
		uint16 val;
		uint16 bbconfig_reg = APHY_BBCONFIG + ((ISAPHY(pi)) ? 0 : GPHY_TO_APHY_OFF);
		org_antsel = ((val = read_phy_reg(pi, bbconfig_reg)) & PHY_BBC_ANT_MASK);
		mod_phy_reg(pi, bbconfig_reg, PHY_BBC_ANT_MASK, antsel);
		write_phy_reg(pi, bbconfig_reg, val | BBCFG_RESETCCA);
		write_phy_reg(pi, bbconfig_reg, val & (~BBCFG_RESETCCA));
		wlc_phy_rx_iq_est_aphy(pi, est, num_samps, wait_time, wait_for_crs);
		mod_phy_reg(pi, bbconfig_reg, PHY_BBC_ANT_MASK, org_antsel);
		val = read_phy_reg(pi, bbconfig_reg);
		write_phy_reg(pi, bbconfig_reg, val | BBCFG_RESETCCA);
		write_phy_reg(pi, bbconfig_reg, val & (~BBCFG_RESETCCA));
	}

	/* sum I and Q powers for each core, average over num_samps */
	ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
	FOREACH_CORE(pi, i)
		cmplx_pwr[i] = (est[i].i_pwr + est[i].q_pwr) >> log_num_samps;

	/* pi->phy_noise_win per antenna is updated inside */
	wlc_phy_noise_calc(pi, cmplx_pwr, noise_dbm_ant);

	pi->phynoise_state &= ~PHY_NOISE_STATE_MON;

	for (i = pi->pubpi.phy_corenum - 1; i != 0; i--)
		result = (result << 8) | (noise_dbm_ant[i] & 0xff);
	return result;
}

static void
wlc_phy_noise_sample_request(wlc_phy_t *pih, uint8 reason, uint8 ch)
{
	phy_info_t *pi = (phy_info_t*)pih;
	int8 noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
	bool sampling_in_progress = (pi->phynoise_state != 0);
	bool wait_for_intr = TRUE;

	PHY_TMP(("wlc_phy_noise_sample_request: state %d reason %d, channel %d\n",
		pi->phynoise_state, reason, ch));

	if (NORADIO_ENAB(pi->pubpi)) {
		return;
	}

	switch (reason) {
	case PHY_NOISE_SAMPLE_MON:

		pi->phynoise_chan_watchdog = ch;
		pi->phynoise_state |= PHY_NOISE_STATE_MON;

		break;

	case PHY_NOISE_SAMPLE_EXTERNAL:

		pi->phynoise_state |= PHY_NOISE_STATE_EXTERNAL;
		break;

	default:
		ASSERT(0);
		break;
	}

	/* since polling is atomic, sampling_in_progress equals to interrupt sampling ongoing
	 *  In these collision cases, always yield and wait interrupt to finish, where the results
	 *  maybe be sharable if channel matches in common callback progressing.
	 */
	if (sampling_in_progress)
		return;

	/* start test, save the timestamp to recover in case ucode gets stuck */
	pi->phynoise_now = pi->sh->now;

	/* Fixed noise, don't need to do the real measurement */
	if (pi->phy_fixed_noise) {
		if (ISNPHY(pi) || ISHTPHY(pi)) {
			uint8 i;
			FOREACH_CORE(pi, i) {
				pi->phy_noise_win[i][pi->phy_noise_index] =
					PHY_NOISE_FIXED_VAL_NPHY;
			}
			pi->phy_noise_index = MODINC_POW2(pi->phy_noise_index,
				PHY_NOISE_WINDOW_SZ);
			/* legacy noise is the max of antennas */
			noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
		} else {
			/* all other PHY */
			noise_dbm = PHY_NOISE_FIXED_VAL;
		}

		wait_for_intr = FALSE;
		goto done;
	}

	if (ISLPPHY(pi)) {
		/* CQRM always use interrupt since ccx can issue many requests and
		 * suspend_mac can't finish intime
		 */
		if (!pi->phynoise_polling || (reason == PHY_NOISE_SAMPLE_EXTERNAL)) {
			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_0, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_1, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP0, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP1, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP2, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP3, 0);

			OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
		} else {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phy_set_deaf_lpphy(pi, (bool)0);
			noise_dbm = (int8)wlc_phy_rx_signal_power_lpphy(pi, 20);
			wlc_phy_clear_deaf_lpphy(pi, (bool)0);
			wlapi_enable_mac(pi->sh->physhim);
			PHY_CAL(("channel = %d noise pwr=%d\n",
				(int)CHSPEC_CHANNEL(pi->radio_chanspec), noise_dbm));
			wait_for_intr = FALSE;
		}
	} else if (ISSSLPNPHY(pi)) {
		noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
	} else if (ISLCNPHY(pi)) {
		wait_for_intr = FALSE;
		noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
	} else if (ISNPHY(pi) || ISHTPHY(pi)) {
		if (!pi->phynoise_polling || (reason == PHY_NOISE_SAMPLE_EXTERNAL)) {
			/* ucode assumes these shm locations start with 0
			 * and ucode will not touch them in case of sampling expires
			 */
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP0, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP1, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP2, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP3, 0);

			if (ISHTPHY(pi)) {
				wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP4, 0);
				wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_MAP5, 0);
			}

			OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
		} else {

			/* polling mode */
			phy_iq_est_t est[PHY_CORE_MAX];
			uint32 cmplx_pwr[PHY_CORE_MAX];
			int8 noise_dbm_ant[PHY_CORE_MAX];
			uint16 log_num_samps, num_samps, classif_state = 0;
			uint8 wait_time = 32;
			uint8 wait_crs = 0;
			uint8 i;

			bzero((uint8 *)est, sizeof(est));
			bzero((uint8 *)cmplx_pwr, sizeof(cmplx_pwr));
			bzero((uint8 *)noise_dbm_ant, sizeof(noise_dbm_ant));

			/* choose num_samps to be some power of 2 */
			log_num_samps = PHY_NOISE_SAMPLE_LOG_NUM_NPHY;
			num_samps = 1 << log_num_samps;

			/* suspend MAC, get classifier settings, turn it off
			 * get IQ power measurements
			 * restore classifier settings and reenable MAC ASAP
			*/
			wlapi_suspend_mac_and_wait(pi->sh->physhim);

			if (ISNPHY(pi)) {
				classif_state = wlc_phy_classifier_nphy(pi, 0, 0);
				wlc_phy_classifier_nphy(pi, 3, 0);
				wlc_phy_rx_iq_est_nphy(pi, est, num_samps, wait_time, wait_crs);
				wlc_phy_classifier_nphy(pi, NPHY_ClassifierCtrl_classifierSel_MASK,
					classif_state);
			} else if (ISHTPHY(pi)) {
				classif_state = wlc_phy_classifier_htphy(pi, 0, 0);
				wlc_phy_classifier_htphy(pi, 3, 0);
				wlc_phy_rx_iq_est_htphy(pi, est, num_samps, wait_time, wait_crs);
				wlc_phy_classifier_htphy(pi,
					HTPHY_ClassifierCtrl_classifierSel_MASK, classif_state);
			} else {
				ASSERT(0);
			}

			wlapi_enable_mac(pi->sh->physhim);

			/* sum I and Q powers for each core, average over num_samps */
			FOREACH_CORE(pi, i)
				cmplx_pwr[i] = (est[i].i_pwr + est[i].q_pwr) >> log_num_samps;

			/* pi->phy_noise_win per antenna is updated inside */
			wlc_phy_noise_calc(pi, cmplx_pwr, noise_dbm_ant);
			wlc_phy_noise_save(pi, noise_dbm_ant, &noise_dbm);

			wait_for_intr = FALSE;
		}
	} else if (ISAPHY(pi)) {
		if (!pi->phynoise_polling || (reason == PHY_NOISE_SAMPLE_EXTERNAL)) {
			/* For A PHY we must set the radio to return RSSI data to RSSI ADC */
			uint16 reg;

			wlc_phyreg_enter(pih);
			wlc_radioreg_enter(pih);
			/* set temp reading delay to zero */
			reg = read_phy_reg(pi, APHY_RSSI_ADC_CTL);
			reg = (reg & 0xF0FF);
			write_phy_reg(pi, APHY_RSSI_ADC_CTL, reg);

			/* grab the default value for this radio */
			reg = read_radio_reg(pi, RADIO_2060WW_RX_SP_REG1);
			wlc_radioreg_exit(pih);
			wlc_phyreg_exit(pih);

			/* save the radio reg default value for ucode
			 * to restore after measurements
			 */
			wlapi_bmac_write_shm(pi->sh->physhim, M_RF_RX_SP_REG1, reg);

			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_0, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_1, 0);

			OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);

		} else {
			noise_dbm = wlc_phy_noise_sample_aphy_meas(pi);
			wait_for_intr = FALSE;
		}
	} else {
		/* B/GPHY */
		if (!pi->phynoise_polling || (reason == PHY_NOISE_SAMPLE_EXTERNAL)) {
			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_0, 0);
			wlapi_bmac_write_shm(pi->sh->physhim, M_JSSI_1, 0);
			OR_REG(pi->sh->osh, &pi->regs->maccommand, MCMD_BG_NOISE);

		} else {
			noise_dbm = wlc_phy_noise_sample_gphy(pi);
			wait_for_intr = FALSE;
		}
	}

done:
	/* if no interrupt scheduled, populate noise results now */
	if (!wait_for_intr)
		wlc_phy_noise_cb(pi, ch, noise_dbm);

}

void
wlc_phy_noise_sample_request_external(wlc_phy_t *pih)
{
	uint8  channel;

	channel = CHSPEC_CHANNEL(wlc_phy_chanspec_get(pih));

	wlc_phy_noise_sample_request(pih, PHY_NOISE_SAMPLE_EXTERNAL, channel);
}

static void
wlc_phy_noise_cb(phy_info_t *pi, uint8 channel, int8 noise_dbm)
{
	if (!pi->phynoise_state)
		return;

	PHY_TMP(("wlc_phy_noise_cb: state %d noise %d channel %d\n",
		pi->phynoise_state, noise_dbm, channel));

	if (pi->phynoise_state & PHY_NOISE_STATE_MON) {
		if (pi->phynoise_chan_watchdog == channel) {
			pi->sh->phy_noise_window[pi->sh->phy_noise_index] = noise_dbm;
			pi->sh->phy_noise_index = MODINC(pi->sh->phy_noise_index, MA_WINDOW_SZ);
		}
		pi->phynoise_state &= ~PHY_NOISE_STATE_MON;
	}

	if (pi->phynoise_state & PHY_NOISE_STATE_EXTERNAL) {
		pi->phynoise_state &= ~PHY_NOISE_STATE_EXTERNAL;

		wlapi_noise_cb(pi->sh->physhim, channel, noise_dbm);
	}

}

static int8
wlc_phy_noise_read_shmem(phy_info_t *pi)
{
	uint32 cmplx_pwr[PHY_CORE_MAX];
	int8 noise_dbm_ant[PHY_CORE_MAX];
	uint16 lo, hi;
	uint32 cmplx_pwr_tot = 0;
	int8 noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
	uint8 core;

	ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
	bzero((uint8 *)cmplx_pwr, sizeof(cmplx_pwr));
	bzero((uint8 *)noise_dbm_ant, sizeof(noise_dbm_ant));

	/* read SHM, reuse old corerev PWRIND since we are tight on SHM pool */
	FOREACH_CORE(pi, core) {
		lo = wlapi_bmac_read_shm(pi->sh->physhim, M_PWRIND_MAP(2*core));
		hi = wlapi_bmac_read_shm(pi->sh->physhim, M_PWRIND_MAP(2*core+1));
		cmplx_pwr[core] = (hi << 16) + lo;
		cmplx_pwr_tot += cmplx_pwr[core];
		if (cmplx_pwr[core] == 0) {
			noise_dbm_ant[core] = PHY_NOISE_FIXED_VAL_NPHY;
		} else
			cmplx_pwr[core] >>= PHY_NOISE_SAMPLE_LOG_NUM_UCODE;
	}

	if (cmplx_pwr_tot == 0)
		PHY_INFORM(("wlc_phy_noise_sample_nphy_compute: timeout in ucode\n"));
	else
		wlc_phy_noise_calc(pi, cmplx_pwr, noise_dbm_ant);

	wlc_phy_noise_save(pi, noise_dbm_ant, &noise_dbm);

	/* legacy noise is the max of antennas */
	return noise_dbm;

}
/* ucode finished phy noise measurement and raised interrupt */
void
wlc_phy_noise_sample_intr(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;
	int jssi[4];
	uint8 crs_state;
	uint16 jssi_pair, jssi_aux;
	uint8 channel = 0;
	int8 noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;

	if (ISLPPHY(pi)) {
		uint32 cmplx_pwr_tot, cmplx_pwr[PHY_CORE_MAX];
		int8 cmplx_pwr_dbm[PHY_CORE_MAX];
		uint16 lo, hi;
		int32 pwr_offset_dB, gain_dB;
		uint16 status_0, status_1;
		uint16 agc_state, gain_idx;
		uint8 core;

		/* copy of the M_CURCHANNEL, just channel number plus 2G/5G flag */
		jssi_aux = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_AUX);
		channel = jssi_aux & D11_CURCHANNEL_MAX;

		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		bzero((uint8 *)cmplx_pwr, sizeof(cmplx_pwr));
		bzero((uint8 *)cmplx_pwr_dbm, sizeof(cmplx_pwr_dbm));

		/* read SHM, reuse old corerev PWRIND since we are tight on SHM pool */
		FOREACH_CORE(pi, core) {
			lo = wlapi_bmac_read_shm(pi->sh->physhim, M_PWRIND_MAP(2*core));
			hi = wlapi_bmac_read_shm(pi->sh->physhim, M_PWRIND_MAP(2*core+1));
			cmplx_pwr[core] = (hi << 16) + lo;
		}

		cmplx_pwr_tot = (cmplx_pwr[0] + cmplx_pwr[1]) >> 6;

		status_0 = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_0);
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			status_0 = ((status_0 & 0xc000) | ((status_0 & 0x3fff)>>1));
		}
		status_1 = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_1);

		agc_state = (status_0 >> 8) & 0xf;
		gain_idx  = (status_0 & 0xff);

		if ((cmplx_pwr_tot > 0) && ((status_0 & 0xc000) == 0x4000) &&
		    /* Filter out weird statuses -- probably hw bug in status reporting */
		    ((agc_state != 1) || (gain_idx >=  20)))
		    {
			/* Measurement finished o.k */
			wlc_phy_compute_dB(cmplx_pwr, cmplx_pwr_dbm, pi->pubpi.phy_corenum);

			pwr_offset_dB = (read_phy_reg(pi, LPPHY_InputPowerDB) & 0xFF);

			if (pwr_offset_dB > 127)
				pwr_offset_dB -= 256;

			/* 20*log10(1)+30-20*log10(512/0.5) ~= -30 */
			noise_dbm += (int8)(pwr_offset_dB - 30);

			if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
				if ((status_0 & 0x0f00) == 0x0100) {
					gain_dB = (read_phy_reg(pi, LPPHY_HiGainDB) & 0xFF);
					noise_dbm -= (int8)(gain_dB);
				} else {
					/* There's a bug in 4325 where the gainIdx is not
					 * output to debug gpio's so unless the AGC is in
					 * HIGH_GAIN_STATE the actual gain can't be
					 * derived from the debug gpio.
					 */
					noise_dbm = -60;
				}
			} else {
				gain_dB = (gain_idx *3) - 6;
				noise_dbm -= (int8)(gain_dB);
			}

			PHY_INFORM(("lpphy noise is %d dBm ch %d(DBG %04x %04x)\n",
				noise_dbm, channel, status_0, status_1));
		} else {
			/* Measurement finished abnormally */
			PHY_INFORM(("lpphy noise measurement error: %04x %04x ch %d\n",
				status_0, status_1, channel));

			noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;
		}
	} else if (ISSSLPNPHY(pi)) {
		noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;

	} else if (ISLCNPHY(pi)) {
	  noise_dbm = PHY_NOISE_FIXED_VAL_NPHY;

	  wlc_lcnphy_noise_measure(pi);
	} else if (ISNPHY(pi) || ISHTPHY(pi)) {
		/* copy of the M_CURCHANNEL, just channel number plus 2G/5G flag */
		jssi_aux = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_AUX);
		channel = jssi_aux & D11_CURCHANNEL_MAX;
		noise_dbm = wlc_phy_noise_read_shmem(pi);

	} else if (ISABGPHY(pi)) {

		jssi_pair = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_0);
		jssi[0] = (int)(jssi_pair & 0xff);
		jssi[1] = (int)((jssi_pair >> 8) & 0xff);
		if (ISGPHY(pi)) {
			/* ucode grabs 4 samples for GPhy */
			jssi_pair = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_1);
			jssi[2] = (int)(jssi_pair & 0xff);
			jssi[3] = (int)((jssi_pair >> 8) & 0xff);
		}

		jssi_aux = wlapi_bmac_read_shm(pi->sh->physhim, M_JSSI_AUX);
		channel = jssi_aux & D11_CURCHANNEL_MAX;

		crs_state = (jssi_aux >> 8) & 0x1f;

		if (ISAPHY(pi)) {
			/* APhy jssi value is signed */
			if (jssi[0] > 127)
				jssi[0] -= 256;

			/* check for a bad reading, ucode puts valid bit in jssi[1] */
			if (!(jssi[1] & 1)) {	/* bad sample, try again */
				PHY_TMP(("wl%d: wlc_phy_noise_int: sample invalid\n",
					pi->sh->unit));
				/* special failure case for APHY: rerun if hasn't started */
				if (pi->phynoise_state == 0)
					wlc_phy_noise_sample_request(pih, pi->phynoise_state,
						channel);

			} else {
				/* APHY 4306C0s RSSI ADC output needs to be scaled up
				 * to match 4306B0
				 */
				if (AREV_GT(pi->pubpi.phy_rev, 2))
					jssi[0] = jssi[0] * 4;
			}
		}

		PHY_TMP(("wl%d: wlc_phy_noise_int: chan %d crs %d JSSI sample %d\n",
		         pi->sh->unit, channel, crs_state, jssi[0]));

		noise_dbm = wlc_jssi_to_rssi_dbm_abgphy(pi, crs_state, jssi, 4);
	} else {
		PHY_ERROR(("wlc_phy_noise_sample_intr, unsupported phy type\n"));
		ASSERT(0);
	}

	/* rssi dbm computed, invoke all callbacks */
	wlc_phy_noise_cb(pi, channel, noise_dbm);
}

int8
wlc_phy_noise_avg(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;
	int tot = 0;
	int i = 0;

	for (i = 0; i < MA_WINDOW_SZ; i++)
		tot += pi->sh->phy_noise_window[i];

	tot /= MA_WINDOW_SZ;
	return (int8)tot;

}

static uint8 nrssi_tbl_phy[PHY_RSSI_TABLE_SIZE] = {
	00,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63
};

int8 lcnphy_gain_index_offset_for_pkt_rssi[] = {
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

void
wlc_phy_compute_dB(uint32 *cmplx_pwr, int8 *p_cmplx_pwr_dB, uint8 core)
{
	uint8 shift_ct, lsb, msb, secondmsb, i;
	uint32 tmp;

	PHY_INFORM(("wlc_phy_compute_dB: compute_dB for %d cores\n", core));
	for (i = 0; i < core; i++) {
		tmp = cmplx_pwr[i];
		shift_ct = msb = secondmsb = 0;
		while (tmp != 0) {
			tmp = tmp >> 1;
			shift_ct++;
			lsb = (uint8)(tmp & 1);
			if (lsb == 1)
				msb = shift_ct;
		}
		secondmsb = (uint8)((cmplx_pwr[i] >> (msb - 1)) & 1);
		p_cmplx_pwr_dB[i] = (int8)(3*msb + 2*secondmsb);
		PHY_INFORM(("wlc_phy_compute_dB: p_cmplx_pwr_dB[%d] %d\n", i, p_cmplx_pwr_dB[i]));
	}
}

void BCMFASTPATH
wlc_phy_rssi_compute(wlc_phy_t *pih, void *ctx)
{
	wlc_d11rxhdr_t *wlc_rxhdr = (wlc_d11rxhdr_t *)ctx;
	d11rxhdr_t *rxh = &wlc_rxhdr->rxhdr;
	int rssi = ltoh16(rxh->PhyRxStatus_1) & PRXS1_JSSI_MASK;
	uint radioid = pih->radioid;
	phy_info_t *pi = (phy_info_t *)pih;

	if (NORADIO_ENAB(pi->pubpi)) {
		rssi = WLC_RSSI_INVALID;
		wlc_rxhdr->do_rssi_ma = 1;	/* skip calc rssi MA */
		goto end;
	}
	/* intermediate mpdus in a AMPDU do not have a valid phy status */
	if ((pi->sh->corerev >= 11) && !(ltoh16(rxh->RxStatus2) & RXS_PHYRXST_VALID)) {
		rssi = WLC_RSSI_INVALID;
		wlc_rxhdr->do_rssi_ma = 1;	/* skip calc rssi MA */
		goto end;
	}

	if (ISSSLPNPHY(pi)) {
		rssi = wlc_sslpnphy_rssi_compute(pi, rssi, rxh);
	}

	if (ISLCNPHY(pi)) {
		uint8 gidx = (ltoh16(rxh->PhyRxStatus_2) & 0xFC00) >> 10;
		phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

		if (rssi > 127)
			rssi -= 256;

		/* RSSI adjustment */
		rssi = rssi + lcnphy_gain_index_offset_for_pkt_rssi[gidx];
		if ((rssi > -46) && (gidx > 18))
			rssi = rssi + 7;

		/* temperature compensation */
		rssi = rssi + pi_lcn->lcnphy_pkteng_rssi_slope;

		/* 2dB compensation of path loss for 4329 on Ref Boards */
		rssi = rssi + 2;

	}

	if (radioid == BCM2050_ID) {
		if ((ltoh16(rxh->PhyRxStatus_0) & PRXS0_OFDM) == 0) {

			if (pi->sh->boardflags & BFL_ADCDIV) {
				/* Correct for the nrssi slope */
				if (rssi >= PHY_RSSI_TABLE_SIZE)
					rssi = PHY_RSSI_TABLE_SIZE - 1;

				rssi = nrssi_tbl_phy[rssi];
				rssi = (-131 * (31 - rssi) >> 7) - 67;
			} else {
				rssi = (-149 * (31 - rssi) >> 7) - 68;
			}

			if (ISGPHY(pi)) {
				uint16 val;
				/* Only 4306/B0 boards have T/R bit overloaded to also
				 * represent LNA for CCK
				 */
				if (ltoh16(rxh->PhyRxStatus_3) & PRXS3_TRSTATE) {
					rssi += 20;
				}
				val = (ltoh16(rxh->PhyRxStatus_2) & PRXS2_LNAGN_MASK) >>
				        PRXS2_LNAGN_SHIFT;
				switch (val) {
					case 0:	rssi -= -2;	break;
					case 1:	rssi -= 19;	break;
					case 2:	rssi -= 13;	break;
					case 3:	rssi -= 25;	break;
				}
				rssi += 25;
			}
		} else {
			/* Sign extend */
			if (rssi > 127)
				rssi -= 256;
			/* Check T-R/LNA bit */
			/* Adjust the calculated RSSI to match measured RX power
			 * as reported by DVT, to keep reported RSSI within +/-3dB of
			 * measured Rx power. This is needed for 2050 Radio only.
			 */
			if (ltoh16(rxh->PhyRxStatus_3) & PRXS3_TRSTATE)
				rssi += 17;
			else
				rssi -= 4;
		}
	} else if (radioid == BCM2060_ID) {
		/* Sign extend */
		if (rssi > 127)
			rssi -= 256;
	} else if (ISLPPHY(pi)) {
		/* Sign extend */
		if (rssi > 127)
			rssi -= 256;
	} else if (ISSSLPNPHY(pi)) {
		/* do nothing */
	} else if (ISLCNPHY(pi)) {
		/* Sign extend */
		if (rssi > 127)
			rssi -= 256;
	} else if (radioid == BCM2055_ID || radioid == BCM2056_ID || radioid == BCM2057_ID) {
		ASSERT(ISNPHY(pi));
		rssi = wlc_phy_rssi_compute_nphy(pi, wlc_rxhdr);
	} else if (ISHTPHY(pi)) {
		ASSERT(radioid == BCM2059_ID);
		rssi = wlc_phy_rssi_compute_htphy(pi, wlc_rxhdr);
	} else {
		ASSERT((const char *)"Unknown radio" == NULL);
	}
#if defined(WLNOKIA_NVMEM)
	rssi = wlc_phy_upd_rssi_offset(pi, (int8)rssi, CH20MHZ_CHSPEC(WLC_RX_CHANNEL(rxh)));
#endif 

end:
	wlc_rxhdr->rssi = (int8)rssi;
}

/* %%%%%% radar */

void
wlc_phy_radar_detect_enable(wlc_phy_t *pih, bool on)
{
#if defined(AP) && defined(RADAR)
	phy_info_t *pi = (phy_info_t *)pih;

	pi->sh->radar = on;

	if (!pi->sh->up)
		return;

	/* apply radar inits to hardware if we are on the A/LP/N/HTPHY */
	if (ISAPHY(pi) || ISLPPHY(pi) || ISNPHY(pi) || ISHTPHY(pi))
		wlc_phy_radar_detect_init(pi, on);

	return;
#endif /* defined(AP) && defined(RADAR) */
}


int
wlc_phy_radar_detect_run(wlc_phy_t *pih)
{
#if defined(AP) && defined(RADAR)
	phy_info_t *pi = (phy_info_t *)pih;

	if (ISAPHY(pi))
		return wlc_phy_radar_detect_run_aphy(pi);
	else if (ISNPHY(pi))
		return wlc_phy_radar_detect_run_nphy(pi);
	else if (ISHTPHY(pi))
		return wlc_phy_radar_detect_run_htphy(pi);

	ASSERT(0);
#endif /* defined(AP) && defined(RADAR) */
	return (RADAR_TYPE_NONE);
}

void
wlc_phy_radar_detect_mode_set(wlc_phy_t *pih, phy_radar_detect_mode_t mode)
{
#if defined(AP) && defined(RADAR)
	phy_info_t *pi = (phy_info_t *)pih;

	PHY_TRACE(("wl%d: %s, Radar detect mode set done\n", pi->sh->unit, __FUNCTION__));

	if (pi->rdm == mode)
		return;
	else if ((mode != RADAR_DETECT_MODE_FCC) && (mode != RADAR_DETECT_MODE_EU)) {
		PHY_TRACE(("wl%d: bogus radar detect mode: %d\n", pi->sh->unit, mode));
		return;
	} else
		pi->rdm = mode;

	/* Change radar params based on radar detect mode for
	 * both 20Mhz (index 0) and 40Mhz (index 1) aptly
	 * feature_mask bit-11 is FCC-enable
	 * feature_mask bit-12 is EU-enable
	 */
	if (pi->rdm == RADAR_DETECT_MODE_FCC) {
		/* 20MHz */
		pi->rargs[0].radar_args.feature_mask = (pi->rargs[0].radar_args.feature_mask
			& 0xefff) | 0x800;
		pi->rargs[0].radar_args.min_fm_lp = 20;
		if (NREV_GE(pi->pubpi.phy_rev, 6))
			pi->rargs[0].radar_args.st_level_time = 0x1591;
		else
			pi->rargs[0].radar_args.st_level_time = 0x1901;
		pi->rargs[0].radar_args.npulses = 5;
		pi->rargs[0].radar_args.ncontig = 3;

		/* 40MHz */
		pi->rargs[1].radar_args.feature_mask = (pi->rargs[1].radar_args.feature_mask
			& 0xefff) | 0x800;
		pi->rargs[1].radar_args.min_fm_lp = 25;
		if (NREV_GE(pi->pubpi.phy_rev, 6))
			pi->rargs[1].radar_args.st_level_time = 0x1591;
		else
			pi->rargs[1].radar_args.st_level_time = 0x1901;
		pi->rargs[1].radar_args.npulses = 5;
		pi->rargs[1].radar_args.ncontig = 3;
	} else if (pi->rdm == RADAR_DETECT_MODE_EU) {
		/* 20MHz */
		pi->rargs[0].radar_args.feature_mask = (pi->rargs[0].radar_args.feature_mask
			& 0xf7ff) | 0x1000;
		pi->rargs[0].radar_args.min_fm_lp = 40;
		if (NREV_GE(pi->pubpi.phy_rev, 6))
			pi->rargs[0].radar_args.st_level_time = 0x1591;
		else
			pi->rargs[0].radar_args.st_level_time = 0x1901;
		pi->rargs[0].radar_args.npulses = 4;
		pi->rargs[0].radar_args.ncontig = 2;

		/* 40MHz */
		pi->rargs[1].radar_args.feature_mask = (pi->rargs[1].radar_args.feature_mask
			& 0xf7ff) | 0x1000;
		pi->rargs[1].radar_args.min_fm_lp = 80;
		if (NREV_GE(pi->pubpi.phy_rev, 6))
			pi->rargs[1].radar_args.st_level_time = 0x15c1;
		else
			pi->rargs[1].radar_args.st_level_time = 0x1c01;
		pi->rargs[1].radar_args.npulses = 4;
		pi->rargs[1].radar_args.ncontig = 2;
	}
#endif /* defined(AP) && defined(RADAR) */
}

#if defined(AP) && defined(RADAR)

struct {
	char name[7];
	int  pri;
	int  id;
} radar_class[] = {
	{ "ETSI_1", 28544, RADAR_TYPE_ETSI_1 },
	{ "ETSI_2", 11008, RADAR_TYPE_ETSI_2 },
	{ "ETSI_3", 60544, RADAR_TYPE_ETSI_3 },
	{ "ITU_E",  9984, RADAR_TYPE_ITU_E },
	{ "ITU_K",  6528,  RADAR_TYPE_ITU_K },
	{ "",	    0,     0 }
};

/* Sort vector (of length len) into ascending order */
static void
wlc_shell_sort(int len, int *vector)
{
	int i, j, incr;
	int v;

	incr = 4;

	while (incr < len) {
		incr *= 3;
		incr++;
	}

	while (incr > 1) {
		incr /= 3;
		for (i = incr; i < len; i++) {
			v = vector[i];
			j = i;
			while (vector[j-incr] > v) {
				vector[j] = vector[j-incr];
				j -= incr;
				if (j < incr)
					break;
			}
			vector[j] = v;
		}
	}
}

void
wlc_phy_radar_detect_init(phy_info_t *pi, bool on)
{
	PHY_TRACE(("wl%d: %s, RSSI LUT done\n", pi->sh->unit, __FUNCTION__));

	/* update parameter set */
	pi->rparams = IS20MHZ(pi) ? &pi->rargs[0] : &pi->rargs[1];

	if (ISAPHY(pi)) {
		wlc_phy_radar_detect_init_aphy(pi, on);
		return;
	} else if (ISNPHY(pi)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			wlc_phy_radar_detect_init_nphy(pi, on);
		wlc_phy_update_radar_detect_param_nphy(pi);
		return;
	} else if (ISHTPHY(pi)) {
		if (CHSPEC_IS5G(pi->radio_chanspec))
			wlc_phy_radar_detect_init_htphy(pi, on);
		wlc_phy_update_radar_detect_param_htphy(pi);
		return;
	}

	ASSERT(0);
}

/* read_radar_table */
static void
wlc_phy_radar_read_table(phy_info_t *pi, radar_work_t *rt, int min_pulses)
{
	int i;
	int16 w0, w1, w2;

	/* True? Maximum table size is 42 entries */
	bzero(rt->tstart_list, sizeof(rt->tstart_list));
	bzero(rt->width_list, sizeof(rt->width_list));

	rt->length = read_phy_reg(pi, APHY_RADAR_FIFO_CTL) & 0xff;
	if (rt->length < min_pulses)
		return;

	rt->length /= 3;	/* 3 words per pulse */

	for (i = 0; i < rt->length; i++) {	/* Read out the FIFO */
		w0 = read_phy_reg(pi, APHY_RADAR_FIFO);
		w1 = read_phy_reg(pi, APHY_RADAR_FIFO);
		w2 = read_phy_reg(pi, APHY_RADAR_FIFO);
		rt->tstart_list[i] = ((w0 << 8) + (w1 & 0xff)) << 4;
		rt->width_list[i] = w2 & 0xff;
	}
}

/* generate an n-th tier list (difference between nth pulses) */
static void
wlc_phy_radar_generate_tlist(uint32 *inlist, int *outlist, int length, int n)
{
	int i;

	for (i = 0; i < (length - n); i++) {
		outlist[i] = inlist[i + n] - inlist[i];
	}
}

/* generate an n-th tier pw list */
static void
wlc_phy_radar_generate_tpw(uint32 *inlist, int *outlist, int length, int n)
{
	int i;

	for (i = 0; i < (length - n); i++) {
		outlist[i] = inlist[i + n - 1];
	}
}

/* remove outliers from a list */
static void
wlc_phy_radar_filter_list(int *inlist, int *length, int min_val, int max_val)
{
	int i, j;
	j = 0;
	for (i = 0; i < *length; i++) {
		if ((inlist[i] >= min_val) && (inlist[i] <= max_val)) {
			inlist[j] = inlist[i];
			j++;
		}
	}
	*length = j;
}

/*
 * select_nfrequent - crude for now
 * inlist - input array (tier list) that has been sorted into ascending order
 * length - length of input array
 * n - position of interval value/frequency to return
 * value - interval
 * frequency - number of occurrences of interval value
 * vlist - returned interval list
 * flist - returned frequency list
 */
static int
wlc_phy_radar_select_nfrequent(int *inlist, int length, int n, int *value,
	int *position, int *frequency, int *vlist, int *flist)
{
	/*
	 * needs declarations:
		int vlist[RDR_TIER_SIZE];
		int flist[RDR_TIER_SIZE];
	 * from calling routine
	 */
	int i, j, pointer, counter, newvalue, nlength;
	int plist[RDR_NTIER_SIZE];
	int f, v, p;

	vlist[0] = inlist[0];
	plist[0] = 0;
	flist[0] = 1;

	pointer = 0;
	counter = 0;

	for (i = 1; i < length; i++) {	/* find the frequencies */
		newvalue = inlist[i];
		if (newvalue != vlist[pointer]) {
			pointer++;
			vlist[pointer] = newvalue;
			plist[pointer] = i;
			flist[pointer] = 1;
			counter = 0;
		} else {
			counter++;
			flist[pointer] = counter;
		}
	}

	nlength = pointer + 1;

	for (i = 1; i < nlength; i++) {	/* insertion sort */
		f = flist[i];
		v = vlist[i];
		p = plist[i];
		j = i - 1;
		while ((j >= 0) && flist[j] > f) {
			flist[j + 1] = flist[j];
			vlist[j + 1] = vlist[j];
			plist[j + 1] = plist[j];
			j--;
		}
		flist[j + 1] = f;
		vlist[j + 1] = v;
		plist[j + 1] = p;
	}

	if (n < nlength) {
		*value = vlist[nlength - n - 1];
		*position = plist[nlength - n - 1];
		*frequency = flist[nlength - n - 1] + 1;
	} else {
		*value = 0;
		*position = 0;
		*frequency = 0;
	}
	return nlength;
}

/* radar detection */
static void
wlc_phy_radar_attach_nphy(phy_info_t *pi)
{
	/* 20Mhz channel radar thresholds */
	if (NREV_GE(pi->pubpi.phy_rev, 7)) {
		pi->rargs[0].radar_thrs.thresh0_20_lo = 0x6a8;
		pi->rargs[0].radar_thrs.thresh1_20_lo = 0x30;
		pi->rargs[0].radar_thrs.thresh0_20_hi = 0x6a8;
		pi->rargs[0].radar_thrs.thresh1_20_hi = 0x30;
	} else if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->rargs[0].radar_thrs.thresh0_20_lo = 0x698;
		pi->rargs[0].radar_thrs.thresh1_20_lo = 0x6c8;
		pi->rargs[0].radar_thrs.thresh0_20_hi = 0x6a0;
		pi->rargs[0].radar_thrs.thresh1_20_hi = 0x6d0;
	} else {
		pi->rargs[0].radar_thrs.thresh0_20_lo = 0x6a8;
		pi->rargs[0].radar_thrs.thresh1_20_lo = 0x6d0;
		pi->rargs[0].radar_thrs.thresh0_20_hi = 0x69c;
		pi->rargs[0].radar_thrs.thresh1_20_hi = 0x6c6;
	}

	/* 20Mhz channel radar params */
	pi->rargs[0].min_tint = 3000;  /* 0.15ms (6.67 kHz) */
	pi->rargs[0].max_tint = 120000; /* 6ms (167 Hz) - for Finland */
	pi->rargs[0].min_blen = 100000;
	pi->rargs[0].max_blen = 1500000; /* 75 ms */
	pi->rargs[0].sdepth_extra_pulses = 1;
	pi->rargs[0].min_deltat_lp = 19000;
	pi->rargs[0].max_deltat_lp = 41000;
	pi->rargs[0].max_type1_pw = 35;	  /* fcc type1 1*20 + 15 */
	pi->rargs[0].jp2_1_intv = 27780;  /* jp-2-1 1389*20 */
	pi->rargs[0].type1_intv = 28571;  /* fcc type 1 1428.57*20 */
	pi->rargs[0].max_jp1_2_pw = 70;	  /* jp-1-2 2.5*20+20 */
	pi->rargs[0].jp1_2_intv = 76923;  /* jp-1-2 3846.15*20 */
	pi->rargs[0].jp2_3_intv = 80000;  /* jp-2-3 4000*20 */
	pi->rargs[0].max_type2_pw = 150;  /* fcc type 2, 5*20 + 50 */
	pi->rargs[0].min_type2_intv = 3000;
	pi->rargs[0].max_type2_intv = 4600;
	pi->rargs[0].max_type4_pw = 460;  /* fcc type 4, 20*20 + 60 */
	pi->rargs[0].min_type3_4_intv = 4000;
	pi->rargs[0].max_type3_4_intv = 10000;
	pi->rargs[0].radar_args.min_burst_intv_lp = 16000000;
	pi->rargs[0].radar_args.max_burst_intv_lp = 240000000;
	pi->rargs[0].radar_args.nskip_rst_lp = 2;
	pi->rargs[0].radar_args.max_pw_tol = 4;
	pi->rargs[0].radar_args.quant = 128;
	pi->rargs[0].radar_args.npulses = 5;
	pi->rargs[0].radar_args.ncontig = 3;
	pi->rargs[0].radar_args.min_pw = 1;
	pi->rargs[0].radar_args.max_pw = 690;  /* 30us + 15% */
	pi->rargs[0].radar_args.thresh0 = pi->rargs[0].radar_thrs.thresh0_20_lo;
	pi->rargs[0].radar_args.thresh1 = pi->rargs[0].radar_thrs.thresh1_20_lo;
	pi->rargs[0].radar_args.fmdemodcfg = 0x7f09;
	pi->rargs[0].radar_args.autocorr = 0x1e;
	if (NREV_GE(pi->pubpi.phy_rev, 6))
		pi->rargs[0].radar_args.st_level_time = 0x1591;
	else
		pi->rargs[0].radar_args.st_level_time = 0x1901;
	pi->rargs[0].radar_args.t2_min = 0;
	pi->rargs[0].radar_args.npulses_lp = 6;
	pi->rargs[0].radar_args.min_pw_lp = 440;
	pi->rargs[0].radar_args.max_pw_lp = 2300;
	pi->rargs[0].radar_args.min_fm_lp = 20;
	pi->rargs[0].radar_args.max_span_lp = 360000000;
	pi->rargs[0].radar_args.min_deltat = 2300;
	pi->rargs[0].radar_args.max_deltat = 3000000;
	pi->rargs[0].radar_args.version = WL_RADAR_ARGS_VERSION;
	pi->rargs[0].radar_args.fra_pulse_err = 10;
	pi->rargs[0].radar_args.npulses_fra = 3;
	pi->rargs[0].radar_args.npulses_stg2 = 4;
	pi->rargs[0].radar_args.npulses_stg3 = 5;
	pi->rargs[0].radar_args.percal_mask = 0x11;
	pi->rargs[0].radar_args.feature_mask = 0xad03;
	if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->rargs[0].radar_args.blank = 0x6c19;
	} else {
		pi->rargs[0].radar_args.blank = 0x2c19;
	}

/* feature mask bit defitions:
	bit-0:(on) enable the "old" min_deltat filter
	bit-1:(on) enable SKIP LP message
	bit-2:(off) enable Interval output message
	bit-3:(off) output ant0 Intv and PW before filtering
	bit-4:(off) output ant1 Intv and PW before filtering
	bit-5:(off) output staggered reset
	bit-6:(off) output intv and pw even No detection
	bit-7:(off) output quantized intv and pruned pw even No detection
	bit-8:(on) enable detect contiguous pulses only
	bit-9:(off) enable pw(i-1)=tstart(i)-tstart(i-1) when combining pulses
		   with interval < min_deltat or max_pw
	bit-10:(off) 1 selects if interval < max_pw , 0 selects if interval < min_delta
		   when combining short interval pulses
	bit-11:(on) enable fcc radar detection
	bit-12:(on) enable etsi radar detection
	bit-13:(on) for combining pulse use max of pw(i) and pw(i-1) instead of pw(i-1)+pw(i)
	bit-14:(off) print pulse combining debug messages
	bit-15:(off) use 384 of 511 FIFO data
*/

	/* 40Mhz channel radar thresholds */
	if (NREV_GE(pi->pubpi.phy_rev, 7)) {
		pi->rargs[1].radar_thrs.thresh0_40_lo = 0x6a8;
		pi->rargs[1].radar_thrs.thresh1_40_lo = 0x30;
		pi->rargs[1].radar_thrs.thresh0_40_hi = 0x6a8;
		pi->rargs[1].radar_thrs.thresh1_40_hi = 0x30;
	} else if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->rargs[1].radar_thrs.thresh0_40_lo = 0x698;
		pi->rargs[1].radar_thrs.thresh1_40_lo = 0x6c8;
		pi->rargs[1].radar_thrs.thresh0_40_hi = 0x6a0;
		pi->rargs[1].radar_thrs.thresh1_40_hi = 0x6d0;
	} else {
		pi->rargs[1].radar_thrs.thresh0_40_lo = 0x6b8;
		pi->rargs[1].radar_thrs.thresh1_40_lo = 0x6e0;
		pi->rargs[1].radar_thrs.thresh0_40_hi = 0x6b8;
		pi->rargs[1].radar_thrs.thresh1_40_hi = 0x6e0;
	}

	/* 40Mhz channel radar params */
	pi->rargs[1].min_tint = 6000;
	pi->rargs[1].max_tint = 240000;
	pi->rargs[1].min_blen = 200000;
	pi->rargs[1].max_blen = 3000000;
	pi->rargs[1].sdepth_extra_pulses = 1;
	pi->rargs[1].min_deltat_lp = 38000;
	pi->rargs[1].max_deltat_lp = 82000;
	pi->rargs[1].max_type1_pw = 70;		/* fcc type 1 1*40 + 30 */
	pi->rargs[1].jp2_1_intv = 55560;	/* jp-2-1 1389*40 */
	pi->rargs[1].type1_intv = 57143;	/* fcc type 1 1428.57*40 */
	pi->rargs[1].max_jp1_2_pw = 140;	/* jp-1-2 2.5*40+40 */
	pi->rargs[1].jp1_2_intv = 153846;	/* jp-1-2 3846.15*40 */
	pi->rargs[1].jp2_3_intv = 160000;	/* jp-2-3 4000*40 */
	pi->rargs[1].max_type2_pw = 300;    /* fcc type 2, 5*40 + 100 */
	pi->rargs[1].min_type2_intv = 6000;
	pi->rargs[1].max_type2_intv = 9200;
	pi->rargs[1].max_type4_pw = 920;	 /* fcc type 4, 20*40 + 120 */
	pi->rargs[1].min_type3_4_intv = 8000;
	pi->rargs[1].max_type3_4_intv = 20000;
	pi->rargs[1].radar_args.min_burst_intv_lp = 32000000;
	pi->rargs[1].radar_args.max_burst_intv_lp = 480000000;
	pi->rargs[1].radar_args.nskip_rst_lp = 2;
	pi->rargs[1].radar_args.max_pw_tol = 16;
	pi->rargs[1].radar_args.quant = 256;
	pi->rargs[1].radar_args.npulses = 5;
	pi->rargs[1].radar_args.ncontig = 3;
	pi->rargs[1].radar_args.min_pw = 1;
	pi->rargs[1].radar_args.max_pw = 1380; /* 30us + 15% */
	pi->rargs[1].radar_args.thresh0 = pi->rargs[1].radar_thrs.thresh0_40_lo;
	pi->rargs[1].radar_args.thresh1 = pi->rargs[1].radar_thrs.thresh1_40_lo;
	pi->rargs[1].radar_args.fmdemodcfg = 0x7f09;
	pi->rargs[1].radar_args.autocorr = 0x1e;
	if (NREV_GE(pi->pubpi.phy_rev, 6))
		pi->rargs[1].radar_args.st_level_time = 0x1591;
	else
		pi->rargs[1].radar_args.st_level_time = 0x1901;
	pi->rargs[1].radar_args.t2_min = 0;
	pi->rargs[1].radar_args.npulses_lp = 6;
	pi->rargs[1].radar_args.min_pw_lp = 900;
	pi->rargs[1].radar_args.max_pw_lp = 4600;
	pi->rargs[1].radar_args.min_fm_lp = 25;
	pi->rargs[1].radar_args.max_span_lp = 720000000;
	pi->rargs[1].radar_args.min_deltat = 4500; /* determined by max prf */
	pi->rargs[1].radar_args.max_deltat = 6000000; /* how often radar is being probed (150 ms) */
	pi->rargs[1].radar_args.version = WL_RADAR_ARGS_VERSION;
	pi->rargs[1].radar_args.fra_pulse_err = 20;
	pi->rargs[1].radar_args.npulses_fra = 3;
	pi->rargs[1].radar_args.npulses_stg2 = 4;
	pi->rargs[1].radar_args.npulses_stg3 = 5;
	pi->rargs[1].radar_args.percal_mask = 0x11;
	pi->rargs[1].radar_args.feature_mask = 0xad03;
	if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		pi->rargs[1].radar_args.blank = 0x6c32;
	} else {
		pi->rargs[1].radar_args.blank = 0x2c19;
	}
	pi->rparams = IS20MHZ(pi) ? &pi->rargs[0] : &pi->rargs[1];
}

static void
wlc_phy_radar_detect_init_aphy(phy_info_t *pi, bool on)
{
	ASSERT(AREV_GE(pi->pubpi.phy_rev, 3));

	if (on) {
		/* WRITE_APHY_TABLE_ENT(pi, 0xf, 0xd, 0xff); */

		/* empirically refined Radar detect thresh1 */
		write_phy_reg(pi, APHY_RADAR_THRESH1, pi->rparams->radar_args.thresh1);

		/* Requested by dboldy 8/17/04 */
		write_phy_reg(pi, APHY_RADAR_THRESH0, pi->rparams->radar_args.thresh0);

		wlapi_bmac_write_shm(pi->sh->physhim, M_RADAR_REG, pi->rparams->radar_args.thresh1);
	}

	wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_RADARWAR, (on ? MHF1_RADARWAR : 0),
		WLC_BAND_5G);
}

static int
wlc_phy_radar_detect_run_aphy(phy_info_t *pi)
{
	int i, j;
	int tiern_length[RDR_NTIERS];
	int value[RDR_NTIERS], freq[RDR_NTIERS], pos[RDR_NTIERS];
	uint32 *epoch_list;
	int epoch_length = 0;
	int epoch_detected;
	int pulse_interval;
	bool filter_pw = TRUE;
	radar_work_t *rt = &pi->radar_work;
	radar_params_t *rparams = pi->rparams;
	int val_list[RDR_TIER_SIZE];
	int freq_list[RDR_TIER_SIZE];

	if (!pi->rparams->radar_args.npulses) {
		PHY_ERROR(("radar params not initialized\n"));
		return (RADAR_TYPE_NONE);
	}

	pulse_interval = 0;
	wlc_phy_radar_read_table(pi, rt, rparams->radar_args.npulses);

	/*
	 * Reject if too few pulses recorded
	 */
	if (rt->length < rparams->radar_args.npulses) {
		return (RADAR_TYPE_NONE);
	}

	/*
	 * filter based on pulse width
	 */
	if (filter_pw) {
		j = 0;
		for (i = 0; i < rt->length; i++) {
			if ((rt->width_list[i] >= rparams->radar_args.min_pw) &&
				(rt->width_list[i] <= rparams->radar_args.max_pw)) {
				rt->width_list[j] = rt->width_list[i];
				rt->tstart_list[j] = rt->tstart_list[i];
				j++;
			}
		}
		rt->length = j;
	}
	ASSERT(rt->length <= 2*RDR_LIST_SIZE);

	/*
	 * Break pulses into epochs.
	 */
	rt->nepochs = 1;
	rt->epoch_start[0] = 0;
	for (i = 1; i < rt->length; i++) {
		if ((int32)(rt->tstart_list[i] - rt->tstart_list[i-1]) > (int32)rparams->max_blen) {
			rt->epoch_finish[rt->nepochs-1] = i - 1;
			rt->epoch_start[rt->nepochs] = i;
			rt->nepochs++;
		}
	}
	rt->epoch_finish[rt->nepochs - 1] = i;

	/*
	 * Run the detector for each epoch
	 */
	for (i = 0; i < rt->nepochs && !pulse_interval; i++) {
		/*
		 * Generate 0th order tier list (time delta between received pulses)
		 * Quantize and filter delta pulse times delta pulse times are
		 * returned in sorted order from smallest to largest.
		 */

		epoch_list = rt->tstart_list + rt->epoch_start[i];
		epoch_length = rt->epoch_finish[i] - rt->epoch_start[i] + 1;
		if (epoch_length >= RDR_TIER_SIZE) {
			PHY_RADAR(("TROUBLE: NEED TO MAKE TIER_SIZE BIGGER %d\n", epoch_length));
			return (RADAR_TYPE_NONE);
		}
		bzero(rt->tiern_list[0], sizeof(rt->tiern_list[0]));

		/*
		 * generate lists
		 */
		wlc_phy_radar_generate_tlist(epoch_list, rt->tiern_list[0], epoch_length, 1);
		for (j = 0; j < epoch_length; j++) {			/* quantize */
			rt->tiern_list[0][j] = rparams->radar_args.quant * ((rt->tiern_list[0][j] +
			(rparams->radar_args.quant >> 1)) / rparams->radar_args.quant);
		}
		tiern_length[0] = epoch_length;
		wlc_phy_radar_filter_list(rt->tiern_list[0], &(tiern_length[0]), rparams->min_tint,
		                rparams->max_tint);
		wlc_shell_sort(tiern_length[0], rt->tiern_list[0]);	/* sort */

		/*
		 * Detection
		 */

		/* Reject out of hand if the number of filtered pulses is too low */
		if (tiern_length[0] < rparams->radar_args.npulses) {
			continue;
		}

		/* measure most common pulse interval */
		wlc_phy_radar_select_nfrequent(rt->tiern_list[0], tiern_length[0], 0,
			&value[0], &pos[0], &freq[0], val_list, freq_list);

		if (freq[0] >= rparams->radar_args.npulses) {
			/* Paydirt: Equal spaced pulses, no gaps */
			pulse_interval = value[0];
			continue;
		}

		if (freq[0] < rparams->radar_args.ncontig) {
			continue;
		}

		/* Possible match - look for gaps */
		/* Check 2nd most frequent interval only on lowest tier */
		wlc_phy_radar_select_nfrequent(rt->tiern_list[0], tiern_length[0], 1,
			&value[1], &pos[1], &freq[1], val_list, freq_list);
		if ((value[1] == (2 * value[0])) && ((freq[0] + freq[1]) >=
			rparams->radar_args.npulses)) {
			/* twice the interval */
			pulse_interval = value[1];
			continue;
		}

		if ((value[0] == (2 * value[1])) && ((freq[0] + freq[1]) >=
			rparams->radar_args.npulses)) {
			/* half the interval */
			pulse_interval = value[1];
			continue;
		}

		/*
		 * Look for extra pulses
		 */

		/* Generate 2nd lowest tier */
		if (0) {	/* DCL this part is not functional, take out */
					/* to save memory and have RDR_NTIERS=1 */
		bzero(rt->tiern_list[1], sizeof(rt->tiern_list[1]));
		/* generate lists */
		wlc_phy_radar_generate_tlist(epoch_list, rt->tiern_list[1], epoch_length, 2);
		for (j = 0; j < epoch_length; j++) {	/* quantize */
			rt->tiern_list[1][j] = rparams->radar_args.quant * ((rt->tiern_list[1][j] +
				(rparams->radar_args.quant >> 1)) / rparams->radar_args.quant);
		}
		tiern_length[1] = epoch_length;
		wlc_phy_radar_filter_list(rt->tiern_list[1], &(tiern_length[1]),
			rparams->min_tint * 2, rparams->max_tint * 2);
		wlc_shell_sort(tiern_length[1], rt->tiern_list[1]);	/* sort */

		/* Look over multiple frequencies on 2nd lowest tier */
		for (j = 1; (j < rparams->sdepth_extra_pulses) && (j < tiern_length[1]); j++) {
			wlc_phy_radar_select_nfrequent(rt->tiern_list[1], tiern_length[1], j,
				&value[1], &pos[1], &freq[1], val_list, freq_list);
			if ((value[0] == value[1]) && ((freq[0]+freq[1]) >=
			                               rparams->radar_args.npulses)) {
				pulse_interval = value[1];
				continue;
			}
		}
		}
	}

	epoch_detected = i;

	if (pulse_interval) {
		int radar_type = RADAR_TYPE_UNCLASSIFIED;
		const char *radar_type_str = "UNCLASSIFIED";

		for (i = 0; i < radar_class[i].id; i++) {
			if (pulse_interval == radar_class[i].pri) {
				radar_type = radar_class[i].id;
				radar_type_str = (char *)radar_class[i].name;
				break;
			}
		}

		PHY_RADAR(("radar : %s Pulse Interval %d\n", radar_type_str, pulse_interval));

		PHY_RADAR(("Pulse Widths:  "));
		for (i = 0; i < rt->length; i++) {
			PHY_RADAR(("%i ", rt->width_list[i]));
		}
		PHY_RADAR(("\n"));

		PHY_RADAR(("Start Time:  "));
		for (i = 0; i < rt->length; i++) {
			PHY_RADAR(("%i ", rt->tstart_list[i]));
		}
		PHY_RADAR(("\n"));

		PHY_RADAR(("Epoch : nepochs %d, length %d detected %d",
		          rt->nepochs, epoch_length, epoch_detected));

		return (radar_type);
	}

	return (RADAR_TYPE_NONE);
}

static void
wlc_phy_radar_detect_init_nphy(phy_info_t *pi, bool on)
{
	if (on) {
		if (CHSPEC_CHANNEL(pi->radio_chanspec) <= WL_THRESHOLD_LO_BAND) {
			if (CHSPEC_IS40(pi->radio_chanspec)) {
				pi->rparams->radar_args.thresh0 =
					pi->rparams->radar_thrs.thresh0_40_lo;
				pi->rparams->radar_args.thresh1 =
					pi->rparams->radar_thrs.thresh1_40_lo;
			} else {
				pi->rparams->radar_args.thresh0 =
					pi->rparams->radar_thrs.thresh0_20_lo;
				pi->rparams->radar_args.thresh1 =
					pi->rparams->radar_thrs.thresh1_20_lo;
			}
		} else {
			if (CHSPEC_IS40(pi->radio_chanspec)) {
				pi->rparams->radar_args.thresh0 =
					pi->rparams->radar_thrs.thresh0_40_hi;
				pi->rparams->radar_args.thresh1 =
					pi->rparams->radar_thrs.thresh1_40_hi;
			} else {
				pi->rparams->radar_args.thresh0 =
					pi->rparams->radar_thrs.thresh0_20_hi;
				pi->rparams->radar_args.thresh1 =
					pi->rparams->radar_thrs.thresh1_20_hi;
			}
		}
		if (NREV_LT(pi->pubpi.phy_rev, 2))
			write_phy_reg(pi, NPHY_RadarBlankCtrl,
			              pi->rparams->radar_args.blank);
		else
			write_phy_reg(pi, NPHY_RadarBlankCtrl,
			              (pi->rparams->radar_args.blank | (0x0000)));

		if (NREV_LT(pi->pubpi.phy_rev, 3)) {
			write_phy_reg(pi, NPHY_RadarThresh0, pi->rparams->radar_args.thresh0);
			write_phy_reg(pi, NPHY_RadarThresh1, pi->rparams->radar_args.thresh1);
		} else {
			write_phy_reg(pi, NPHY_RadarThresh0,
				(uint16)((int16)pi->rparams->radar_args.thresh0));
			write_phy_reg(pi, NPHY_RadarThresh1,
				(uint16)((int16)pi->rparams->radar_args.thresh1));
			write_phy_reg(pi, NPHY_Radar_t2_min, pi->rparams->radar_args.t2_min);
		}

		write_phy_reg(pi, NPHY_StrAddress2u, pi->rparams->radar_args.st_level_time);
		write_phy_reg(pi, NPHY_StrAddress2l, pi->rparams->radar_args.st_level_time);
		write_phy_reg(pi, NPHY_crsControlu, (uint8)pi->rparams->radar_args.autocorr);
		write_phy_reg(pi, NPHY_crsControll, (uint8)pi->rparams->radar_args.autocorr);
		write_phy_reg(pi, NPHY_FMDemodConfig, pi->rparams->radar_args.fmdemodcfg);

		wlapi_bmac_write_shm(pi->sh->physhim, M_RADAR_REG, pi->rparams->radar_args.thresh1);

		if (NREV_GE(pi->pubpi.phy_rev, 3)) {
			write_phy_reg(pi, NPHY_RadarThresh0R, 0x7a8);
			write_phy_reg(pi, NPHY_RadarThresh1R, 0x7d0);
		} else {
			write_phy_reg(pi, NPHY_RadarThresh0R, 0x7e8);
			write_phy_reg(pi, NPHY_RadarThresh1R, 0x10);
		}

		/* percal_mask to diasable radar detection during selected period cals */
		pi->radar_percal_mask = pi->rparams->radar_args.percal_mask;
		if (NREV_GE(pi->pubpi.phy_rev, 7)) {
			write_phy_reg(pi, NPHY_RadarSearchCtrl, 1);
			write_phy_reg(pi, NPHY_RadarDetectConfig1, 0x3204);
			write_phy_reg(pi, NPHY_RadarT3BelowMin, 0);
			write_phy_reg(pi, NPHY_RadarThresh0, 0x698);
			write_phy_reg(pi, NPHY_RadarThresh1, 0x30);
		}
		else {
			/* Set radar frame search modes */
			write_phy_reg(pi, NPHY_RadarSearchCtrl, 7);
		}
	}

	wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_RADARWAR, (on ? MHF1_RADARWAR : 0), FALSE);

}

static void
wlc_phy_radar_read_table_nphy(phy_info_t *pi, radar_work_t *rt, int min_pulses)
{
	int i;
	int16 w0, w1, w2;
	int max_fifo_size = 255;
	radar_params_t *rparams = pi->rparams;

	if (NREV_GE(pi->pubpi.phy_rev, 3))
		max_fifo_size = 510;

	/* True? Maximum table size is 85 entries for .11n */
	/* Format is different from earlier .11a PHYs */
	bzero(rt->tstart_list_n, sizeof(rt->tstart_list_n));
	bzero(rt->width_list_n, sizeof(rt->width_list_n));
	bzero(rt->fm_list_n, sizeof(rt->fm_list_n));

	if (NREV_GE(pi->pubpi.phy_rev, 3)) {
		rt->nphy_length[0] = read_phy_reg(pi, NPHY_Antenna0_radarFifoCtrl) & 0x3ff;
		rt->nphy_length[1] = read_phy_reg(pi, NPHY_Antenna1_radarFifoCtrl) & 0x3ff;
	} else {
		rt->nphy_length[0] = read_phy_reg(pi, NPHY_Antenna0_radarFifoCtrl) & 0x1ff;
		rt->nphy_length[1] = read_phy_reg(pi, NPHY_Antenna1_radarFifoCtrl) & 0x1ff;
	}
	/* Rev 3: nphy_length is <=510 because words are read/written in multiples of 3 */

	if (rt->nphy_length[0]  > max_fifo_size) {
		PHY_RADAR(("FIFO LENGTH in ant 0 is greater than max_fifo_size\n"));
		rt->nphy_length[0]  = 0;
	}

	if (rt->nphy_length[1]  > max_fifo_size) {
		PHY_RADAR(("FIFO LENGTH in ant 1 is greater than max_fifo_size\n"));
		rt->nphy_length[1]  = 0;
	}

	if ((rt->nphy_length[0] > 5) || (rt->nphy_length[1] > 5))
		PHY_RADAR(("ant 0:%d ant 1:%d\n", rt->nphy_length[0], rt->nphy_length[1]));

	rt->nphy_length[0] /= 3;	/* 3 words per pulse */

	for (i = 0; i < rt->nphy_length[0]; i++) {
		/* Read out FIFO 0 */
		w0 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		w1 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		w2 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		/* USE ONLY 255 of 511 FIFO DATA if feature_mask bit-15 set */
		if ((i < 128 && (rparams->radar_args.feature_mask & 0x8000)) ||
			((rparams->radar_args.feature_mask & 0x8000) == 0)) {
			rt->tstart_list_n[0][i] = ((w0 << 12) + (w1 & 0x0fff)) << 4;
			rt->tstart_list_bin5[0][i] = ((w0 << 12) + (w1 & 0x0fff)) << 4;
			rt->width_list_n[0][i] = ((w2 & 0x00ff) << 4) + ((w1 >> 12) & 0x000f);
			rt->width_list_bin5[0][i] = ((w2 & 0x00ff) << 4) + ((w1 >> 12) & 0x000f);
			rt->fm_list_n[0][i] = (w2 >> 8) & 0x00ff;
		}
	}
	if (rt->nphy_length[0] > 128 && (rparams->radar_args.feature_mask & 0x8000))
		rt->nphy_length[0] = 128;
	rt->nphy_length_bin5[0] = rt->nphy_length[0];

	rt->nphy_length[1] /= 3;	/* 3 words per pulse */

	for (i = 0; i < rt->nphy_length[1]; i++) {	/* Read out FIFO 0 */
		w0 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		w1 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		w2 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		/* USE ONLY 255 of 511 FIFO DATA if feature_mask bit-15 set */
		if ((i < 128 && (rparams->radar_args.feature_mask & 0x8000)) ||
			((rparams->radar_args.feature_mask & 0x8000) == 0)) {
			rt->tstart_list_n[1][i] = ((w0 << 12) + (w1 & 0x0fff)) << 4;
			rt->tstart_list_bin5[1][i] = ((w0 << 12) + (w1 & 0x0fff)) << 4;
			rt->width_list_n[1][i] = ((w2 & 0x00ff) << 4) + ((w1 >> 12) & 0x000f);
			rt->width_list_bin5[1][i] = ((w2 & 0x00ff) << 4) + ((w1 >> 12) & 0x000f);
			rt->fm_list_n[1][i] = (w2 >> 8) & 0x00ff;
		}
	}
	if (rt->nphy_length[1] > 128 && (rparams->radar_args.feature_mask & 0x8000))
		rt->nphy_length[1] = 128;
	rt->nphy_length_bin5[1] = rt->nphy_length[1];
}

static void
wlc_phy_radar_read_table_nphy_rev7(phy_info_t *pi, radar_work_t *rt, int min_pulses)
{
	int i;
	int16 w0, w1, w2, w3;
	int max_fifo_size = 512;
	radar_params_t *rparams = pi->rparams;

	/* Maximum table size is 128 entries for .11n rev7 forward */
	bzero(rt->tstart_list_n, sizeof(rt->tstart_list_n));
	bzero(rt->width_list_n, sizeof(rt->width_list_n));
	bzero(rt->fm_list_n, sizeof(rt->fm_list_n));

	rt->nphy_length[0] = read_phy_reg(pi, NPHY_Antenna0_radarFifoCtrl) & 0x3ff;
	rt->nphy_length[1] = read_phy_reg(pi, NPHY_Antenna1_radarFifoCtrl) & 0x3ff;

	if (rt->nphy_length[0] > max_fifo_size) {
		PHY_RADAR(("FIFO LENGTH in ant 0 is greater than max_fifo_size of %d\n",
			max_fifo_size));
		rt->nphy_length[0]  = 0;
	}

	if (rt->nphy_length[1]  > max_fifo_size) {
		PHY_RADAR(("FIFO LENGTH in ant 1 is greater than max_fifo_size of %d\n",
			max_fifo_size));
		rt->nphy_length[1]  = 0;
	}

	if ((rt->nphy_length[0] > 5) || (rt->nphy_length[1] > 5))
		PHY_RADAR(("ant 0:%d ant 1:%d\n", rt->nphy_length[0], rt->nphy_length[1]));

	rt->nphy_length[0] /= 4;	/* 4 words per pulse */

	for (i = 0; i < rt->nphy_length[0]; i++) {
		/* Read out FIFO 0 */
		w0 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		w1 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		w2 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		w3 = read_phy_reg(pi, NPHY_Antenna0_radarFifoData);
		/* Limit num pulses to 128 if feature_mask bit-15 set */
		if ((i < 128 && (rparams->radar_args.feature_mask & 0x8000)) ||
			((rparams->radar_args.feature_mask & 0x8000) == 0)) {
/*
			rt->tstart_list_n[0][i] = (w0 << 12) + ((w1 & 0x0fff) << 4) + (w3 & 0xf);
*/
			rt->tstart_list_n[0][i] = (w0 << 16) + ((w1 & 0x0fff) << 4) + (w3 & 0xf);
			rt->tstart_list_bin5[0][i] = rt->tstart_list_n[0][i];
			rt->width_list_n[0][i] = ((w3 & 0x10) << 8) + ((w2 & 0x00ff) << 4) +
				((w1 >> 12) & 0x000f);
			rt->width_list_bin5[0][i] = rt->width_list_n[0][i];
			rt->fm_list_n[0][i] = ((w3 & 0x20) << 3) + ((w2 >> 8) & 0x00ff);
		}
	}
	if (rt->nphy_length[0] > 128 && (rparams->radar_args.feature_mask & 0x8000))
		rt->nphy_length[0] = 128;
	rt->nphy_length_bin5[0] = rt->nphy_length[0];

	rt->nphy_length[1] /= 4;	/* 4 words per pulse */

	for (i = 0; i < rt->nphy_length[1]; i++) {	/* Read out FIFO 0 */
		w0 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		w1 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		w2 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		w3 = read_phy_reg(pi, NPHY_Antenna1_radarFifoData);
		/* Limit num pulses to 128 if feature_mask bit-15 set */
		if ((i < 128 && (rparams->radar_args.feature_mask & 0x8000)) ||
			((rparams->radar_args.feature_mask & 0x8000) == 0)) {
/*
			rt->tstart_list_n[1][i] = (w0 << 12) + ((w1 & 0x0fff) << 4) + (w3 & 0xf);
*/
			rt->tstart_list_n[1][i] = (w0 << 16) + ((w1 & 0x0fff) << 4) + (w3 & 0xf);
			rt->tstart_list_bin5[1][i] = rt->tstart_list_n[1][i];
			rt->width_list_n[1][i] = ((w3 & 0x10) << 8) + ((w2 & 0x00ff) << 4) +
				((w1 >> 12) & 0x000f);
			rt->width_list_bin5[1][i] = rt->width_list_n[1][i];
			rt->fm_list_n[1][i] = ((w3 & 0x20) << 3) + ((w2 >> 8) & 0x00ff);
		}
	}
	if (rt->nphy_length[1] > 128 && (rparams->radar_args.feature_mask & 0x8000))
		rt->nphy_length[1] = 128;
	rt->nphy_length_bin5[1] = rt->nphy_length[1];
}

static void
wlc_phy_radar_detect_run_epoch_nphy(phy_info_t *pi, uint i,
	radar_work_t *rt, radar_params_t *rparams,
	uint32 *epoch_list, int epoch_length,
	int fra_t1, int fra_t2, int fra_t3, int fra_err,
	int pw_2us, int pw15us, int pw20us, int pw30us,
	int i250us, int i500us, int i625us, int i5000us,
	int pw2us, int i833us, int i2500us, int i3333us,
	int i938us, int i3066us,
	bool *fra_det_p, bool *stg_det_p, int *pulse_interval_p, int *nconsecq_pulses_p,
	int *detected_pulse_index_p)
{
	int j;
	int k;
	bool radar_detected = FALSE;

	int ndetected_staggered;
	bool fra_det = FALSE;
	bool stg_det = FALSE;
	char stag_det_seq[32];
	int tiern_length[RDR_NTIERS];
	int detected_pulse_index = 0;
	int nconsecq_pulses = 0;
	int nconseq2even, nconseq2odd;
	int nconseq3a, nconseq3b, nconseq3c;
	int skip_cnt;
	int first_interval;
	int tol;
	int min_detected_pw;
	int max_detected_pw;
	int pulse_interval = 0;
	int *freq_list = NULL, *val_list = NULL;

	if ((freq_list = MALLOC(pi->sh->osh, sizeof(uint32) * RDR_NTIER_SIZE)) == NULL)
		goto end;

	if ((val_list = MALLOC(pi->sh->osh, sizeof(uint32) * RDR_NTIER_SIZE)) == NULL)
		goto end;

	min_detected_pw = rparams->radar_args.max_pw;
	max_detected_pw = rparams->radar_args.min_pw;

	detected_pulse_index = 0;
	nconsecq_pulses = 0;
	nconseq2even = 0;
	nconseq2odd = 0;
	nconseq3a = 0;
	nconseq3b = 0;
	nconseq3c = 0;

	/* initialize French radar detection variables */
	snprintf(stag_det_seq, sizeof(stag_det_seq), "%s", "");

	bzero(rt->tiern_list[0], sizeof(rt->tiern_list[0]));
	bzero(rt->tiern_pw[0], sizeof(rt->tiern_pw[0]));

	/*
	 * generate lists
	 */
	wlc_phy_radar_generate_tlist(epoch_list, rt->tiern_list[0], epoch_length, 1);
	wlc_phy_radar_generate_tpw((uint32 *) (rt->width_list + rt->epoch_start[i]),
		rt->tiern_pw[0], epoch_length, 1);
	for (j = 0; j < epoch_length; j++) {			/* quantize */
		rt->tiern_list[0][j] = rparams->radar_args.quant *
			((rt->tiern_list[0][j] + (rparams->radar_args.quant >> 1)) /
			rparams->radar_args.quant);
	}

	/*
	 * Do French radar detection
	 */
	ndetected_staggered = 0; /* need to have npulses_fra in an epoch */

	for (j = 0; j+2 < epoch_length && !fra_det; j++) {
		/*
		PHY_RADAR(("S3 DEBUG: T1=%d, T2=%d, T3=%d\n", rt->tiern_list[0][j],
			rt->tiern_list[0][j+1], rt->tiern_list[0][j+2]));
		*/
		if (ABS(rt->tiern_list[0][j] - fra_t1) < fra_err ||
			ABS(rt->tiern_list[0][j] - fra_t3 - fra_t1) < fra_err) {
			if (ABS(rt->tiern_list[0][j+1] - fra_t2) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t3) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j] -
						fra_t1) < fra_err ? "1 2 3" : "31 2 3"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] -fra_t3 - fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t1) < fra_err ? "1 2 31" : "31 2 31"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t3 - fra_t1 - fra_t2)
						< fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t1) < fra_err ? "1 2 312" : "31 2 312"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			} else if (ABS(rt->tiern_list[0][j+1] - fra_t2 - fra_t3) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t1) < fra_err ? "1 23 1" : "31 23 1"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] -fra_t1 - fra_t2) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t1) < fra_err ? "1 23 12" : "31 23 12"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t1 - fra_t2 - fra_t3)
						< fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t1) < fra_err ? "1 23 123" : "31 23 123"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			}
			if (ndetected_staggered >= rparams->radar_args.npulses_fra) {
				fra_det = 1;
				break;
			}
		} else if (ABS(rt->tiern_list[0][j] - fra_t2) < fra_err ||
			ABS(rt->tiern_list[0][j] - fra_t1 - fra_t2) < fra_err) {
			if (ABS(rt->tiern_list[0][j+1] - fra_t3) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2 < fra_err) ? "2 3 1" : "12 3 1"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t1 -fra_t2) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2) < fra_err ? "2 3 12" : "12 3 12"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t1 - fra_t2
						- fra_t3) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2) < fra_err ? "2 3 123" : "12 3 123"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			} else if (ABS(rt->tiern_list[0][j+1] - fra_t3 - fra_t1) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t2) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2) < fra_err ? "2 31 2" : "12 31 2"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t2 -fra_t3) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2) < fra_err ? "2 31 23" : "12 31 23"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t2 - fra_t3
						- fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t2) < fra_err ? "2 31 231" : "12 31 231"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			}
			if (ndetected_staggered >= rparams->radar_args.npulses_fra) {
				fra_det = 1;
				break;
			}
		} else if (ABS(rt->tiern_list[0][j] - fra_t3) < fra_err ||
			ABS(rt->tiern_list[0][j] - fra_t2 - fra_t3) < fra_err) {
			if (ABS(rt->tiern_list[0][j+1] - fra_t1) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t2) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 1 2" : "23 1 2"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t2 -fra_t3) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 1 23" : "23 1 23"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t2 - fra_t3
						- fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 1 231" : "23 1 231"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			} else if (ABS(rt->tiern_list[0][j+1] - fra_t1 - fra_t2) < fra_err) {
				if (ABS(rt->tiern_list[0][j+2] - fra_t3) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 12 3" : "23 12 3"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t3 -fra_t1) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 12 31" : "23 12 31"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				} else if (ABS(rt->tiern_list[0][j+2] - fra_t3 - fra_t1
						- fra_t2) < fra_err) {
					snprintf(stag_det_seq, sizeof(stag_det_seq), "%s",
						(ABS(rt->tiern_list[0][j]
						- fra_t3) < fra_err ? "3 12 312" : "23 12 312"));
					PHY_RADAR(("S3 DETECT SEQ: %s\n", stag_det_seq));
					ndetected_staggered++;
					j = j + 2;
				}
			}
			if (ndetected_staggered >= rparams->radar_args.npulses_fra) {
				fra_det = 1;
				break;
			}
		} else
			continue;

	}	/* French radar for loop */

	if (fra_det)
		goto end;

	tiern_length[0] = epoch_length;
	wlc_phy_radar_filter_list(rt->tiern_list[0], &(tiern_length[0]), rparams->min_tint,
		rparams->max_tint);

	/* Detect contiguous only pulses */
	detected_pulse_index = 0;
	nconsecq_pulses = 0;
	nconseq2even = 0;
	nconseq2odd = 0;
	nconseq3a = 0;
	nconseq3b = 0;
	nconseq3c = 0;
	skip_cnt = 0;

	tol = rparams->radar_args.quant;
	first_interval = rt->tiern_list[0][0];

	if (rparams->radar_args.feature_mask & 0x100) {   /* detect contiguous pulses only */
		for (j = 0; j < epoch_length - 2; j++) {
			/* contiguous pulse detection */
			if ((rt->tiern_list[0][j] == first_interval ||
				rt->tiern_list[0][j] == (first_interval >> 1) ||
				rt->tiern_list[0][j] == (first_interval << 1)) &&

				/* fcc filters */
				(((pi->rparams->radar_args.feature_mask & 0x800) &&
				(rt->tiern_list[0][j] >= pi->rparams->radar_args.min_deltat) &&

				/* type 2 filter */
				((rt->tiern_pw[0][j] <= pi->rparams->max_type2_pw &&
				rt->tiern_pw[0][j+1] <= pi->rparams->max_type2_pw &&
				rt->tiern_list[0][j] >= pi->rparams->min_type2_intv - tol &&
				rt->tiern_list[0][j] <= pi->rparams->max_type2_intv + tol) ||

				/* type 3,4 filter */
				(rt->tiern_pw[0][j] <= pi->rparams->max_type4_pw &&
				rt->tiern_pw[0][j+1] <= pi->rparams->max_type4_pw &&
				rt->tiern_list[0][j] >= pi->rparams->min_type3_4_intv - tol &&
				rt->tiern_list[0][j] <= pi->rparams->max_type3_4_intv + tol) ||

				/* fcc weather radar filter */
				(rt->tiern_pw[0][j] <= pw_2us &&
				rt->tiern_pw[0][j+1] <= pw_2us &&
				rt->tiern_list[0][j] >= pi->rparams->max_type3_4_intv - tol &&
				rt->tiern_list[0][j] <= i3066us + pw20us) ||

				/* type 1, jp2+1 filter */
				(rt->tiern_pw[0][j] <= pi->rparams->max_type1_pw &&
				rt->tiern_pw[0][j+1] <= pi->rparams->max_type1_pw &&
				rt->tiern_list[0][j] >= pi->rparams->jp2_1_intv - tol &&
				rt->tiern_list[0][j] <= pi->rparams->type1_intv + tol) ||

				/* type jp1-2, jp2-3 filter */
				(rt->tiern_pw[0][j] <= pi->rparams->max_jp1_2_pw &&
				rt->tiern_pw[0][j+1] <= pi->rparams->max_jp1_2_pw &&
				rt->tiern_list[0][j] >= pi->rparams->jp1_2_intv - tol &&
				rt->tiern_list[0][j] <= pi->rparams->jp2_3_intv + tol))) ||

				/* etsi filters */
				((pi->rparams->radar_args.feature_mask & 0x1000) &&

				/* type 1, 2, 5, 6 filter */
				((rt->tiern_pw[0][j] <= pw15us &&
				rt->tiern_pw[0][j+1] <= pw15us &&
				rt->tiern_list[0][j] >= i625us - tol*3 &&
				rt->tiern_list[0][j] <= i5000us + tol) ||

				/* packet based staggered types 4, 5 */
				(rt->tiern_pw[0][j] <= pw2us &&
				rt->tiern_pw[0][j+1] <= pw2us &&
				rt->tiern_list[0][j] >= i833us - tol &&
				rt->tiern_list[0][j] <= i2500us + tol) ||

				/* type 3, 4 filter */
				(rt->tiern_pw[0][j] <= pw30us &&
				rt->tiern_pw[0][j+1] <= pw30us &&
				rt->tiern_list[0][j] >= i250us - tol/2 &&
				rt->tiern_list[0][j] <= i500us + tol))))) {
				nconsecq_pulses++;
				/* check detected pulse for pulse width tollerence */
				if (nconsecq_pulses >= rparams->radar_args.npulses-1) {
					min_detected_pw = rparams->radar_args.max_pw;
					max_detected_pw = rparams->radar_args.min_pw;
					for (k = 0; k <  rparams->radar_args.npulses; k++) {
						if (rt->tiern_pw[0][j-k] <= min_detected_pw)
							min_detected_pw = rt->tiern_pw[0][j-k];
						if (rt->tiern_pw[0][j-k] >= max_detected_pw)
							max_detected_pw = rt->tiern_pw[0][j-k];
					}
					if (max_detected_pw - min_detected_pw <=
						rparams->radar_args.max_pw_tol) {
						pulse_interval = rt->tiern_list[0][j];
						detected_pulse_index = j -
							rparams->radar_args.npulses + 1;
						break;	/* radar detected */
					}
				}
			} else {
				if (rparams->radar_args.feature_mask & 0x80) {
					PHY_RADAR(("IV:%d-%d ", rt->tiern_list[0][j], j));
					PHY_RADAR(("PW:%d,%d-%d ", rt->tiern_pw[0][j],
					rt->tiern_pw[0][j+1], j));
				}
				nconsecq_pulses = 0;
				first_interval = rt->tiern_list[0][j];
			}

			/* staggered 2/3 single filters */
			/* staggered 2 even */
			if (j >= 2 && j % 2 == 0) {
				if (rt->tiern_list[0][j] == rt->tiern_list[0][j-2] &&

					rt->tiern_pw[0][j] <= pw2us &&
					((rt->tiern_list[0][j] >= pi->rparams->max_type3_4_intv
					- tol && rt->tiern_list[0][j] <= i3066us + pw20us &&
					(pi->rparams->radar_args.feature_mask & 0x800)) || /* fcc */
					(rt->tiern_list[0][j] >= i833us - tol &&
					rt->tiern_list[0][j] <= i3333us + tol &&
					(pi->rparams->radar_args.feature_mask & 0x1000))))
					{ /* etsi */
					nconseq2even++;
					/* check detected pulse for pulse width tollerence */
					if (nconseq2even + nconseq2odd >=
						rparams->radar_args.npulses_stg2-1) {
						radar_detected = 1;   /* preset */
						min_detected_pw = rparams->radar_args.max_pw;
						max_detected_pw = rparams->radar_args.min_pw;
						for (k = 0; k < nconseq2even; k++) {
							/*
							PHY_RADAR(("EVEN neven=%d, k=%d pw= %d\n",
							nconseq2even, k,rt->tiern_pw[0][j-2*k]));
							*/
							if (rt->tiern_pw[0][j-2*k] <=
								min_detected_pw)
								min_detected_pw =
								rt->tiern_pw[0][j-2*k];
							if (rt->tiern_pw[0][j-2*k] >=
								max_detected_pw)
								max_detected_pw =
								rt->tiern_pw[0][j-2*k];
						}
						if (max_detected_pw - min_detected_pw >
							rparams->radar_args.max_pw_tol) {
							radar_detected = 0;
						}
						if (nconseq2odd > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq2odd; k++) {
								/*
								PHY_RADAR(("EVEN nodd=%d,"
									"k=%d pw= %d\n",
									nconseq2odd, k,
									rt->tiern_pw[0][j-2*k-1]));
								*/
								if (rt->tiern_pw[0][j-2*k-1] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-2*k-1];
								if (rt->tiern_pw[0][j-2*k-1] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-2*k-1];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (radar_detected) {
							stg_det = 2;
							pulse_interval = rt->tiern_list[0][j];
							detected_pulse_index = j -
							2 * rparams->radar_args.npulses_stg2 + 1;
							break;	/* radar detected */
						}
					}
				} else {
					if (rparams->radar_args.feature_mask & 0x20)
						PHY_RADAR(("EVEN RESET neven=%d, j=%d "
						"intv=%d pw= %d\n",
						nconseq2even, j, rt->tiern_list[0][j],
						rt->tiern_pw[0][j]));
					nconseq2even = 0;
				}
			}

			/* staggered 2 odd */
			if (j >= 3 && j % 2 == 1) {
				if (rt->tiern_list[0][j] == rt->tiern_list[0][j-2] &&
					rt->tiern_pw[0][j] <= pw2us &&
					((rt->tiern_list[0][j] >= pi->rparams->max_type3_4_intv
					- tol && rt->tiern_list[0][j] <= i3066us + pw20us &&
					(pi->rparams->radar_args.feature_mask & 0x800)) || /* fcc */
					(rt->tiern_list[0][j] >= i833us - tol &&
					rt->tiern_list[0][j] <= i3333us + tol &&
					(pi->rparams->radar_args.feature_mask & 0x1000))))
					{ /* etsi */
					nconseq2odd++;
					/* check detected pulse for pulse width tollerence */
					if (nconseq2even + nconseq2odd >=
						rparams->radar_args.npulses_stg2-1) {
						radar_detected = 1;   /* preset */
						min_detected_pw = rparams->radar_args.max_pw;
						max_detected_pw = rparams->radar_args.min_pw;
						for (k = 0; k < nconseq2odd; k++) {
							/*
							PHY_RADAR(("ODD nodd=%d, k=%d pw= %d\n",
							nconseq2odd, k,rt->tiern_pw[0][j-2*k]));
							*/
							if (rt->tiern_pw[0][j-2*k] <=
								min_detected_pw)
								min_detected_pw =
								rt->tiern_pw[0][j-2*k];
							if (rt->tiern_pw[0][j-2*k] >=
								max_detected_pw)
								max_detected_pw =
								rt->tiern_pw[0][j-2*k];
						}
						if (max_detected_pw - min_detected_pw >
							rparams->radar_args.max_pw_tol) {
							radar_detected = 0;
						}
						if (nconseq2even > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq2even; k++) {
								/*
								PHY_RADAR(("ODD neven=%d,"
								" k=%d pw= %d\n",
								nconseq2even, k,
								rt->tiern_pw[0][j-2*k-1]));
								*/
								if (rt->tiern_pw[0][j-2*k-1] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-2*k-1];
								if (rt->tiern_pw[0][j-2*k-1] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-2*k-1];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (radar_detected) {
							stg_det = 2;
							pulse_interval = rt->tiern_list[0][j];
							detected_pulse_index = j -
							2 * rparams->radar_args.npulses_stg2 + 1;
							break;	/* radar detected */
						}
					}
				} else {
					if (rparams->radar_args.feature_mask & 0x20)
						PHY_RADAR(("ODD RESET nodd=%d, j=%d "
						"intv=%d pw= %d\n",
						nconseq2odd, j, rt->tiern_list[0][j],
						rt->tiern_pw[0][j]));
					nconseq2odd = 0;
				}
			}

			/* staggered 3-a */
			if ((j >= 3 && j % 3 == 0) &&
				(pi->rparams->radar_args.feature_mask & 0x1000)) {
				if ((rt->tiern_list[0][j] == rt->tiern_list[0][j-3]) &&
					rt->tiern_pw[0][j] <= pw2us &&
					(rt->tiern_list[0][j] >= i833us - tol &&
					rt->tiern_list[0][j] <= i3333us + tol)) {
					nconseq3a++;
					/* check detected pulse for pulse width tollerence */
					if (nconseq3a + nconseq3b + nconseq3c >=
						rparams->radar_args.npulses_stg3-1) {
						radar_detected = 1;   /* preset */
						min_detected_pw = rparams->radar_args.max_pw;
						max_detected_pw = rparams->radar_args.min_pw;
						for (k = 0; k < nconseq3a; k++) {
							/*
							PHY_RADAR(("3A n3a=%d, k=%d pw= %d\n",
							nconseq3a, k,rt->tiern_pw[0][j-3*k]));
							*/
							if (rt->tiern_pw[0][j-3*k] <=
								min_detected_pw)
								min_detected_pw =
								rt->tiern_pw[0][j-3*k];
							if (rt->tiern_pw[0][j-3*k] >=
								max_detected_pw)
								max_detected_pw =
								rt->tiern_pw[0][j-3*k];
						}
						if (max_detected_pw - min_detected_pw >
							rparams->radar_args.max_pw_tol) {
							radar_detected = 0;
						}
						if (nconseq3c > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3c; k++) {
								/*
								PHY_RADAR(("3A n3c=%d, k=%d "
								"pw= %d\n",
								nconseq3c, k,
								rt->tiern_pw[0][j-3*k-1]));
								*/
								if (rt->tiern_pw[0][j-3*k-1] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
								if (rt->tiern_pw[0][j-3*k-1] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (nconseq3b > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3b; k++) {
								/*
								PHY_RADAR(("3A n3b=%d, k=%d "
								"pw= %d\n",
								nconseq3b, k,
								rt->tiern_pw[0][j-3*k-2]));
								*/
								if (rt->tiern_pw[0][j-3*k-2] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
								if (rt->tiern_pw[0][j-3*k-2] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (radar_detected) {
							stg_det = 3;
							pulse_interval = rt->tiern_list[0][j];
							detected_pulse_index = j -
							3 * rparams->radar_args.npulses_stg3 + 1;
							break;	/* radar detected */
						}
					}
				} else {
					if (rparams->radar_args.feature_mask & 0x20)
						PHY_RADAR(("3A RESET n3a=%d, j=%d intv=%d "
						"pw= %d\n",
						nconseq3a, j, rt->tiern_list[0][j],
						rt->tiern_pw[0][j]));
					nconseq3a = 0;
				}
			}

			/* staggered 3-b */
			if ((j >= 4 && j % 3 == 1) &&
				(pi->rparams->radar_args.feature_mask & 0x1000)) {
				if ((rt->tiern_list[0][j] == rt->tiern_list[0][j-3]) &&
					rt->tiern_pw[0][j] <= pw2us &&
					(rt->tiern_list[0][j] >= i833us - tol &&
					rt->tiern_list[0][j] <= i3333us + tol)) {
					nconseq3b++;
					/* check detected pulse for pulse width tollerence */
					if (nconseq3a + nconseq3b + nconseq3c >=
						rparams->radar_args.npulses_stg3-1) {
						radar_detected = 1;   /* preset */
						min_detected_pw = rparams->radar_args.max_pw;
						max_detected_pw = rparams->radar_args.min_pw;
						for (k = 0; k < nconseq3b; k++) {
							/*
							PHY_RADAR(("3B n3b=%d, k=%d pw= %d\n",
							nconseq3b, k,rt->tiern_pw[0][j-3*k]));
							*/
							if (rt->tiern_pw[0][j-3*k] <=
								min_detected_pw)
								min_detected_pw =
								rt->tiern_pw[0][j-3*k];
							if (rt->tiern_pw[0][j-3*k] >=
								max_detected_pw)
								max_detected_pw =
								rt->tiern_pw[0][j-3*k];
						}
						if (max_detected_pw - min_detected_pw >
							rparams->radar_args.max_pw_tol) {
							radar_detected = 0;
						}
						if (nconseq3a > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3a; k++) {
								/*
								PHY_RADAR(("3B n3a=%d, k=%d "
								"pw= %d\n",
								nconseq3a, k,
								rt->tiern_pw[0][j-3*k-1]));
								*/
								if (rt->tiern_pw[0][j-3*k-1] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
								if (rt->tiern_pw[0][j-3*k-1] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (nconseq3c > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3c; k++) {
								/*
								PHY_RADAR(("3B n3c=%d, k=%d "
								"pw= %d\n",
								nconseq3c, k,
								rt->tiern_pw[0][j-3*k-2]));
								*/
								if (rt->tiern_pw[0][j-3*k-2] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
								if (rt->tiern_pw[0][j-3*k-2] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (radar_detected) {
							stg_det = 3;
							pulse_interval = rt->tiern_list[0][j];
							detected_pulse_index = j -
							3 * rparams->radar_args.npulses_stg3 + 1;
							break;	/* radar detected */
						}
					}
				} else {
					if (rparams->radar_args.feature_mask & 0x20)
						PHY_RADAR(("3B RESET n3b=%d, j=%d intv=%d pw= %d\n",
						nconseq3b, j, rt->tiern_list[0][j],
						rt->tiern_pw[0][j]));
					nconseq3b = 0;
				}
			}

			/* staggered 3-c */
			if ((j >= 5 && j % 3 == 2) &&
				(pi->rparams->radar_args.feature_mask & 0x1000)) {
				if ((rt->tiern_list[0][j] == rt->tiern_list[0][j-3]) &&
					rt->tiern_pw[0][j] <= pw2us &&
					(rt->tiern_list[0][j] >= i833us - tol &&
					rt->tiern_list[0][j] <= i3333us + tol)) {
					nconseq3c++;
					/* check detected pulse for pulse width tollerence */
					if (nconseq3a + nconseq3b + nconseq3c >=
						rparams->radar_args.npulses_stg3-1) {
						radar_detected = 1;   /* preset */
						min_detected_pw = rparams->radar_args.max_pw;
						max_detected_pw = rparams->radar_args.min_pw;
						for (k = 0; k < nconseq3c; k++) {
							/*
							PHY_RADAR(("3C n3c=%d, k=%d pw= %d\n",
							nconseq3c, k,rt->tiern_pw[0][j-3*k]));
							*/
							if (rt->tiern_pw[0][j-3*k] <=
								min_detected_pw)
								min_detected_pw =
								rt->tiern_pw[0][j-3*k];
							if (rt->tiern_pw[0][j-3*k] >=
								max_detected_pw)
								max_detected_pw =
								rt->tiern_pw[0][j-3*k];
						}
						if (max_detected_pw - min_detected_pw >
							rparams->radar_args.max_pw_tol) {
							radar_detected = 0;
						}
						if (nconseq3b > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3b; k++) {
								/*
								PHY_RADAR(("3C n3b=%d, k=%d "
								"pw= %d\n",
								nconseq3b, k,
								rt->tiern_pw[0][j-3*k-1]));
								*/
								if (rt->tiern_pw[0][j-3*k-1] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
								if (rt->tiern_pw[0][j-3*k-1] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-1];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (nconseq3a > 0) {
							min_detected_pw =
								rparams->radar_args.max_pw;
							max_detected_pw =
								rparams->radar_args.min_pw;
							for (k = 0; k < nconseq3a; k++) {
								/*
								PHY_RADAR(("3C n3a=%d, k=%d "
								"pw= %d\n",
								nconseq3a, k,
								rt->tiern_pw[0][j-3*k-2]));
								*/
								if (rt->tiern_pw[0][j-3*k-2] <=
									min_detected_pw)
									min_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
								if (rt->tiern_pw[0][j-3*k-2] >=
									max_detected_pw)
									max_detected_pw =
									rt->tiern_pw[0][j-3*k-2];
							}
							if (max_detected_pw - min_detected_pw >
								rparams->radar_args.max_pw_tol) {
								radar_detected = 0;
							}
						}
						if (radar_detected) {
							stg_det = 3;
							pulse_interval = rt->tiern_list[0][j];
							detected_pulse_index = j -
							3 * rparams->radar_args.npulses_stg3 + 1;
							break;	/* radar detected */
						}
					}
				} else {
					if (rparams->radar_args.feature_mask & 0x20)
						PHY_RADAR(("3C RESET n3c=%d, j=%d intv=%d pw= %d\n",
						nconseq3c, j, rt->tiern_list[0][j],
						rt->tiern_pw[0][j]));
					nconseq3c = 0;
				}
			}

			if (rt->tiern_list[0][j] < pi->rparams->radar_args.min_deltat) {
				/* enable Interval output message if feature_mask bit-2 is set */
				if (rparams->radar_args.feature_mask & 0x4) {
					PHY_RADAR(("%d ", rt->tiern_list[0][j]));
				}
				skip_cnt++;
			}
		}  /* for (j = 0; j < epoch_length - 2; j++) */
/*
		if (skip_cnt > 0) PHY_RADAR(("SKIPPED %d pulses\n", skip_cnt));
*/
		if (pulse_interval != 0)
			goto end;

	} else {	/* Old non-contiguous pulse detection method */
	}   /* conseq_detect */
end:
	*fra_det_p = fra_det;
	*stg_det_p = stg_det;
	*pulse_interval_p = pulse_interval;
	*nconsecq_pulses_p = nconsecq_pulses;
	*detected_pulse_index_p = detected_pulse_index;
	if (freq_list)
		MFREE(pi->sh->osh, freq_list, sizeof(uint32) * RDR_NTIER_SIZE);
	if (val_list)
		MFREE(pi->sh->osh, val_list, sizeof(uint32) * RDR_NTIER_SIZE);
}

static int
wlc_phy_radar_detect_run_nphy(phy_info_t *pi)
{
	int i, j;
	int ant, mlength;
	uint32 *epoch_list;
	int epoch_length = 0;
	int epoch_detected;
	int pulse_interval;
	uint32 tstart;
	int width, fm, valid_lp;
	bool filter_pw = TRUE;
	int32 deltat;
	radar_work_t *rt = &pi->radar_work;
	radar_params_t *rparams = pi->rparams;
	int fra_t1, fra_t2, fra_t3, fra_err;
	bool fra_det;
	bool stg_det;
	int skip_type;
	int min_detected_pw;
	int max_detected_pw;
	int pw_2us, pw15us, pw20us, pw30us;
	int i250us, i500us, i625us, i5000us;
	int pw2us, i833us, i2500us, i3333us;
	int i938us, i3066us;
	int i633us, i658us;
	int nconsecq_pulses;
	int detected_pulse_index;

	/* skip radar detect if doing periodic cal 
	 * (the test-tones utilized during cal can trigger 
	 * radar detect)
	 */
	if (pi->nphy_rxcal_active) {
		pi->nphy_rxcal_active = FALSE;
		return RADAR_TYPE_NONE;
	}

	fra_err = pi->rparams->radar_args.fra_pulse_err;
	if (IS40MHZ(pi)) {
		fra_t1 = FRA_T1_40MHZ;
		fra_t2 = FRA_T2_40MHZ;
		fra_t3 = FRA_T3_40MHZ;
		pw_2us  = 80+12;
		pw15us = 600+90;
		pw20us = 800+120;
		pw30us = 1200+180;
		i250us = 10000;
		i500us = 20000;
		i625us = 25000;
		i633us = 25316;   /* 1580 Hz */
		i658us = 26316;   /* 1520 Hz */
		i938us = 37520;   /* 1066.1 Hz */
		i3066us = 122640; /* 326.2 Hz */
		i5000us = 200000; /* 200 Hz */
		/* staggered */
		pw2us  = 120+18;
		i833us = 33333;   /* 1200 Hz */
		i2500us = 100000;  /* 400 Hz */
		i3333us = 133333; /* 300 Hz */
	} else {
		fra_t1 = FRA_T1_20MHZ;
		fra_t2 = FRA_T2_20MHZ;
		fra_t3 = FRA_T3_20MHZ;
		pw_2us  = 40+6;
		pw15us = 300+45;
		pw20us = 400+60;
		pw30us = 600+90;
		i250us = 5000;    /* 4000 Hz */
		i500us = 10000;   /* 2000Hz */
		i625us = 12500;   /* 1600 Hz */
		i633us = 12658;   /* 1580 Hz */
		i658us = 13158;   /* 1520 Hz */
		i938us = 18760;   /* 1066.1 Hz */
		i3066us = 61320;  /* 326.2 Hz */
		i5000us = 100000; /* 200 Hz */
		/* staggered */
		pw2us  = 60+9;
		i833us = 16667;   /* 1200 Hz */
		i2500us = 50000;  /* 400 Hz */
		i3333us = 66667;  /* 300 Hz */
	}

	/* clear LP buffer if requested, and print LP buffer count history */
	if (pi->dfs_lp_buffer_nphy != 0) {
		pi->dfs_lp_buffer_nphy = 0;
		PHY_RADAR(("DFS LP buffer =  "));
		for (i = 0; i < rt->lp_len_his_idx; i++) {
			PHY_RADAR(("%d, ", rt->lp_len_his[i]));
		}
		PHY_RADAR(("%d; now CLEARED\n", rt->lp_length));
		rt->lp_length = 0;
		rt->lp_cnt = 0;
		rt->lp_skip_cnt = 0;
		rt->lp_len_his_idx = 0;
	}

	/* quantize */
	fra_t1 = rparams->radar_args.quant * ((fra_t1 + (rparams->radar_args.quant >> 1))
		/ rparams->radar_args.quant);
	fra_t2 = rparams->radar_args.quant * ((fra_t2 + (rparams->radar_args.quant >> 1))
		/ rparams->radar_args.quant);
	fra_t3 = rparams->radar_args.quant * ((fra_t3 + (rparams->radar_args.quant >> 1))
		/ rparams->radar_args.quant);

	if (!pi->rparams->radar_args.npulses) {
		PHY_ERROR(("radar params not initialized\n"));
		return RADAR_TYPE_NONE;
	}

	/* intialize variable */
	pulse_interval = 0;

	min_detected_pw = rparams->radar_args.max_pw;
	max_detected_pw = rparams->radar_args.min_pw;

	/* suspend mac before reading phyregs */
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
		wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  MCTL_PHYLOCK);
		(void)R_REG(pi->sh->osh, &pi->regs->maccontrol);
		OSL_DELAY(1);
	}

	if (NREV_GE(pi->pubpi.phy_rev, 7))
		wlc_phy_radar_read_table_nphy_rev7(pi, rt, 1);
	else
		wlc_phy_radar_read_table_nphy(pi, rt, 1);

	if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))
		wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  0);

	/* restart mac after reading phyregs */
	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);


	/* output ant0 fifo data */
	if ((rparams->radar_args.feature_mask & 0x8) && (rt->nphy_length[0] > 0))  {
		/*	Changed to display intervals intead of tstart
			PHY_RADAR(("Start Time:  "));
			for (i = 0; i < rt->nphy_length[0]; i++)
				PHY_RADAR(("%u ", rt->tstart_list_n[i]));
		*/
		PHY_RADAR(("Ant 0: %d pulses, ", rt->nphy_length[0]));
		PHY_RADAR(("\nInterval:  "));
		for (i = 1; i < rt->nphy_length[0]; i++)
			PHY_RADAR(("%d-%d ", rt->tstart_list_n[0][i] -
				rt->tstart_list_n[0][i - 1], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Pulse Widths:  "));
		for (i = 0; i < rt->nphy_length[0]; i++)
			PHY_RADAR(("%i-%d ", rt->width_list_n[0][i], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("FM:  "));
		for (i = 0; i < rt->nphy_length[0]; i++)
			PHY_RADAR(("%d-%d ", rt->fm_list_n[0][i], i));
		PHY_RADAR(("\n"));
	}

	/* output ant1 fifo data */
	if ((rparams->radar_args.feature_mask & 0x10) && (rt->nphy_length[1] > 0))  {
		/*	Changed to display intervals intead of tstart
			PHY_RADAR(("Start Time:  "));
			for (i = 0; i < rt->nphy_length[0]; i++)
				PHY_RADAR(("%u ", rt->tstart_list_n[i]));
		*/
		PHY_RADAR(("Ant 1: %d pulses, ", rt->nphy_length[1]));
		PHY_RADAR(("\nInterval:  "));
		for (i = 1; i < rt->nphy_length[1]; i++)
			PHY_RADAR(("%d-%d ", rt->tstart_list_n[1][i] -
				rt->tstart_list_n[1][i - 1], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Pulse Widths:  "));
		for (i = 0; i < rt->nphy_length[1]; i++)
			PHY_RADAR(("%i-%d ", rt->width_list_n[1][i], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("FM:  "));
		for (i = 0; i < rt->nphy_length[1]; i++)
			PHY_RADAR(("%d-%d ", rt->fm_list_n[1][i], i));
		PHY_RADAR(("\n"));
	}

#ifdef RADAR_DBG
	PHY_RADAR(("Ant 0: %d pulses, ", rt->nphy_length[0]));
	for (i = 0; i < rt->nphy_length[0]; i++) {
		PHY_RADAR(("Tstart = %u,  Width = %d, FM = %d ", rt->tstart_list_n[0][i],
			rt->width_list_n[0][i],
			rt->fm_list_n[0][i]));
	}

	PHY_RADAR(("Ant 1 %d pulses  ", rt->nphy_length[1]));
	for (i = 0; i < rt->nphy_length[1]; i++) {
		PHY_RADAR(("Tstart = %u,  Width = %d, FM = %d ", rt->tstart_list_n[1][i],
			rt->width_list_n[1][i],
			rt->fm_list_n[1][i]));
	}
#endif
	/*
	 * Reject if no pulses recorded
	 */
	if ((rt->nphy_length[0] < 1) && (rt->nphy_length[1] < 1)) {
		return RADAR_TYPE_NONE;
	}

	if (rparams->radar_args.feature_mask & 0x800) {   /* if fcc */

	/* START LONG PULSES (BIN5) DETECTION */

	/* Combine pulses that are adjacent */
	for (ant = 0; ant < RDR_NANTENNAS; ant++) {
		mlength = rt->nphy_length_bin5[ant];
		if (mlength > 1) {
			for (i = 1; i < mlength; i++) {
				deltat = (int32)((int32) rt->tstart_list_bin5[ant][i] -
				                  (int32) rt->tstart_list_bin5[ant][i-1]);

				/* CAUTION: USING MAX_PW_LP HERE FOR BIN5 ONLY */
				if (deltat <= (int32)pi->rparams->radar_args.max_pw_lp) {
					rt->width_list_bin5[ant][i-1] =
						rt->tstart_list_bin5[ant][i] -
						rt->tstart_list_bin5[ant][i-1];
					if (rparams->radar_args.feature_mask & 0x4000) {
						PHY_RADAR(("*%d,%d,%d ",
						rt->tstart_list_bin5[ant][i] -
						rt->tstart_list_bin5[ant][i-1],
						rt->width_list_bin5[ant][i],
						rt->width_list_bin5[ant][i-1]));
					}

					rt->fm_list_n[ant][i-1] = rt->fm_list_n[ant][i-1] +
						rt->fm_list_n[ant][i];
					for (j = i; j < mlength - 1; j++) {
						rt->tstart_list_bin5[ant][j] =
							rt->tstart_list_bin5[ant][j+1];
						rt->width_list_bin5[ant][j] =
							rt->width_list_bin5[ant][j+1];
						rt->fm_list_n[ant][j] = rt->fm_list_n[ant][j+1];
					}
					mlength--;
					rt->nphy_length_bin5[ant]--;
				}	/* if deltat */
			}	/* for mlength loop */
		}	/* mlength > 1 */
	}	/* for ant loop */

	/* Now combine, sort, and remove duplicated pulses from the 2 antennas */
	rt->length = 0;
	for (ant = 0; ant < RDR_NANTENNAS; ant++) {
		for (i = 0; i < rt->nphy_length_bin5[ant]; i++) {
			rt->tstart_list[rt->length] = rt->tstart_list_bin5[ant][i];
			rt->width_list[rt->length] = rt->width_list_bin5[ant][i];
			rt->fm_list[rt->length] = rt->fm_list_n[ant][i];
			rt->length++;
		}
	}

	for (i = 1; i < rt->length; i++) {	/* insertion sort */
		tstart = rt->tstart_list[i];
		width = rt->width_list[i];
		fm = rt->fm_list[i];
		j = i - 1;
		while ((j >= 0) && rt->tstart_list[j] > tstart) {
			rt->tstart_list[j + 1] = rt->tstart_list[j];
			rt->width_list[j + 1] = rt->width_list[j];
			rt->fm_list[j + 1] = rt->fm_list[j];
			j--;
			}
			rt->tstart_list[j + 1] = tstart;
			rt->width_list[j + 1] = width;
			rt->fm_list[j + 1] = fm;
	}

	/* enable the "old" min_deltat filter if feature_mask bit-0 is set */
	if (rparams->radar_args.feature_mask & 0x1) {
	for (i = 1; i < rt->length; i++) {
		deltat = (int32)((int32) rt->tstart_list[i] - (int32) rt->tstart_list[i-1]);
		if (deltat < (int32)pi->rparams->radar_args.min_deltat) {
			for (j = i; j < (rt->length - 1); j++) {
				rt->tstart_list[j] = rt->tstart_list[j+1];
				rt->width_list[j] = rt->width_list[j+1];
				rt->fm_list[j] = rt->fm_list[j+1];
			}
			rt->length--;
		}
	}
	}
#ifdef RADAR_DBG
	/*
	PHY_RADAR(("Combined \n"));
	for (i = 0; i < rt->length; i++) {
		PHY_RADAR(("Tstart = %u,  Width = %d, FM = %d ", rt->tstart_list[i],
			rt->width_list[i], rt->fm_list[i]));
	}
	*/
#endif

	/* remove entries spaced greater than max_deltat */
	if (rt->length > 1) {
		deltat = (int32)((int32) rt->tstart_list[rt->length - 1] -
			(int32) rt->tstart_list[0]);
		i = 0;
		while ((i < (rt->length - 1)) &&
		       (ABS(deltat) > (int32)rparams->radar_args.max_deltat)) {
			i++;
			deltat = (int32)(rt->tstart_list[rt->length - 1] - rt->tstart_list[i]);
		}
		if (i > 0) {
			for (j = i; j < rt->length; j++) {
				rt->tstart_list[j-i] = rt->tstart_list[j];
				rt->width_list[j-i] = rt->width_list[j];
				rt->fm_list[j-i] = rt->fm_list[j];
			}
			rt->length -= i;
		}
	}

	/* First perform bin 5 detection */
	/* add new pulses */
	for (i = 0; i < rt->length; i++) {
		valid_lp = (rt->width_list[i] >= rparams->radar_args.min_pw_lp) &&
			(rt->width_list[i] <= rparams->radar_args.max_pw_lp) &&
			(rt->fm_list[i] >= rparams->radar_args.min_fm_lp);
		if (rt->lp_length > 0) {
			valid_lp = valid_lp &&
				(rt->tstart_list[i] != rt->lp_buffer[rt->lp_length - 1]);
		}

		if (valid_lp) rt->lp_cnt++;
		deltat = (int32)((rt->tstart_list[i] - rt->last_tstart) >> 1);
		if (deltat < 0) {
			deltat =  deltat + 2147483647;
			PHY_RADAR(("Time Value Rollover: tstart = %u, tlast = %u, deltat/2 = %d\n",
				rt->tstart_list[i], rt->last_tstart, deltat));
		}
		if (valid_lp && deltat
				>= (int32)(rparams->radar_args.min_burst_intv_lp/2)) {
			rt->lp_cnt = 0;
		}
		/* skip the pulse if outside of pulse interval range (1-2ms), */
		/* burst to burst interval, or more than 3 pulses in a burst */
		skip_type = 0;
		if ((deltat < (int32)rparams->min_deltat_lp/2) ||
			(deltat >= (int32) rparams->max_deltat_lp/2 &&
			deltat < (int32)rparams->radar_args.min_burst_intv_lp/2) ||
			(deltat > (int32)rparams->radar_args.max_burst_intv_lp/2) ||
			(rt->lp_cnt > 3)) {
			if (valid_lp) {
				if (deltat <
					(int32) rparams->min_deltat_lp/2)
					skip_type = 0;
				if (deltat >=
					(int32) rparams->max_deltat_lp/2 &&
					deltat < (int32)rparams->radar_args.min_burst_intv_lp/2)
					skip_type = 1;
				if (deltat > (int32)rparams->radar_args.max_burst_intv_lp/2)
					skip_type = 2;
				if (rt->lp_cnt > 3) {
					skip_type = 3;
					rt->lp_cnt = 0;
				}
				valid_lp = 0;
				if (rt->lp_cnt > 0)
					rt->lp_cnt--;  /* take back increment due to valid_lp */
				rt->lp_skip_cnt++;
				if (rt->lp_skip_cnt >= rparams->radar_args.nskip_rst_lp) {
					if (rt->lp_len_his_idx < LP_LEN_HIS_SIZE) {
						rt->lp_len_his[rt->lp_len_his_idx] = rt->lp_length;
						rt->lp_len_his_idx++;
					}
					rt->lp_length = 0;
					rt->lp_cnt = 0;
					rt->lp_skip_cnt = 0;
				}
				/* enable SKIP LP message if feature_mask bit-1 is set */
				if (rparams->radar_args.feature_mask & 0x2) {
					PHY_RADAR(("Skipped LP: KTstart=%u KIntv=%u PW=%d FM=%d "
						"nLP=%d Type=%d pulse=%d skip=%d\n",
						(rt->tstart_list[i])/1000, deltat/500,
						rt->width_list[i], rt->fm_list[i], rt->lp_length,
						skip_type, rt->lp_cnt, rt->lp_skip_cnt));
						rt->last_tstart = rt->tstart_list[i];
				}
			}
		}

		if (valid_lp && (rt->lp_length < RDR_LP_BUFFER_SIZE)) {
			rt->lp_skip_cnt = 0;
			PHY_RADAR(("Valid LP: KTstart=%u KIntv=%u PW=%d FM=%d "
				"nLP=%d pulse=%d skip=%d\n",
				(rt->tstart_list[i])/1000, deltat/500,
				rt->width_list[i], rt->fm_list[i], rt->lp_length + 1,
				rt->lp_cnt, rt->lp_skip_cnt));
			rt->lp_buffer[rt->lp_length] = rt->tstart_list[i];
			rt->lp_length++;
			rt->last_tstart = rt->tstart_list[i];
		}
		if (rt->lp_length > RDR_LP_BUFFER_SIZE)
			PHY_RADAR(("WARNING: LP buffer size is too long"));
	}
	/* remove any outside the time max delta_t_lp */
	if (rt->lp_length > 1) {
		deltat = (int32)((rt->lp_buffer[rt->lp_length - 1] - rt->lp_buffer[0]) >> 1);
		if (deltat < 0)
			deltat = deltat + 2147483647;
		i = 0;
		while ((i < (rt->lp_length - 1)) &&
		       (deltat > (int32)rparams->radar_args.max_span_lp/2)) {
			i++;
			deltat = (int32)((rt->lp_buffer[rt->lp_length - 1] - rt->lp_buffer[0])
				>> 1);
			if (deltat < 0)
				deltat = deltat + 2147483647;
		}

		if (i > 0) {
			for (j = i; j < rt->lp_length; j++)
				rt->lp_buffer[j-i] = rt->lp_buffer[j];

			rt->lp_length -= i;
		}
	}
#ifdef RADAR_DBG
	PHY_RADAR(("\n Bin 5 \n"));
	for (i = 0; i < rt->lp_length; i++) {
		PHY_RADAR(("%u  ", rt->lp_buffer[i]));
	}
	PHY_RADAR(("\n"));
#endif
	if (rt->lp_length >= rparams->radar_args.npulses_lp) {
		PHY_RADAR(("Bin 5 Radar Detected \n"));
		if (rt->lp_len_his_idx < LP_LEN_HIS_SIZE) {
			rt->lp_len_his[rt->lp_len_his_idx] = rt->lp_length;
			rt->lp_len_his_idx++;
		}
		rt->lp_length = 0;
		return (RADAR_TYPE_BIN5);
	}
	}	/* end of if fcc */

	/* START SHORT PULSES (NON-BIN5) DETECTION */

	/* Combine pulses that are adjacent */
	for (ant = 0; ant < RDR_NANTENNAS; ant++) {
		mlength = rt->nphy_length[ant];
		if (mlength > 1) {
			for (i = 1; i < mlength; i++) {
				deltat = (int32)(rt->tstart_list_n[ant][i] -
				                  rt->tstart_list_n[ant][i-1]);
				if ((deltat < (int32)pi->rparams->radar_args.min_deltat &&
					((rparams->radar_args.feature_mask & 0x400) == 0x000)) ||
					(deltat <= (int32)pi->rparams->radar_args.max_pw &&
					(rparams->radar_args.feature_mask & 0x400) == 0x400)) {

					if (rparams->radar_args.feature_mask & 0x200 &&
						rt->tstart_list_n[ant][i] -
						rt->tstart_list_n[ant][i-1]
						<= (uint32) rparams->radar_args.max_pw) {
						rt->width_list_n[ant][i-1] =
							rt->tstart_list_n[ant][i] -
							rt->tstart_list_n[ant][i-1];
					} else {
						if (rparams->radar_args.feature_mask & 0x4000) {
							PHY_RADAR(("*%d,%d,%d ",
								rt->tstart_list_n[ant][i] -
								rt->tstart_list_n[ant][i-1],
								rt->width_list_n[ant][i],
								rt->width_list_n[ant][i-1]));
						}
						if (rparams->radar_args.feature_mask & 0x2000) {
							rt->width_list_n[ant][i-1] =
								(rt->width_list_n[ant][i-1] >
								rt->width_list_n[ant][i] ?
								rt->width_list_n[ant][i-1] :
								rt->width_list_n[ant][i]);
						} else {
							rt->width_list_n[ant][i-1] =
								rt->width_list_n[ant][i-1] +
								rt->width_list_n[ant][i];
						}
					}

					for (j = i; j < mlength - 1; j++) {
						rt->tstart_list_n[ant][j] =
							rt->tstart_list_n[ant][j+1];
						rt->width_list_n[ant][j] =
							rt->width_list_n[ant][j+1];
					}
					mlength--;
					rt->nphy_length[ant]--;
				}
			}
		}
	}

	/* Now combine, sort, and remove duplicated pulses from the 2 antennas */
	bzero(rt->tstart_list, sizeof(rt->tstart_list));
	bzero(rt->width_list, sizeof(rt->width_list));
	rt->length = 0;
	for (ant = 0; ant < RDR_NANTENNAS; ant++) {
		for (i = 0; i < rt->nphy_length[ant]; i++) {
			rt->tstart_list[rt->length] = rt->tstart_list_n[ant][i];
			rt->width_list[rt->length] = rt->width_list_n[ant][i];
			rt->length++;
		}
	}

	for (i = 1; i < rt->length; i++) {	/* insertion sort */
		tstart = rt->tstart_list[i];
		width = rt->width_list[i];
		j = i - 1;
		while ((j >= 0) && rt->tstart_list[j] > tstart) {
			rt->tstart_list[j + 1] = rt->tstart_list[j];
			rt->width_list[j + 1] = rt->width_list[j];
			j--;
			}
			rt->tstart_list[j + 1] = tstart;
			rt->width_list[j + 1] = width;
	}

	/* enable the "old" min_deltat filter if feature_mask bit-0 is set */
	if (rparams->radar_args.feature_mask & 0x1) {
	for (i = 1; i < rt->length; i++) {
		deltat = (int32)(rt->tstart_list[i] - rt->tstart_list[i-1]);
		if (deltat < (int32)pi->rparams->radar_args.min_deltat) {
			for (j = i; j < (rt->length - 1); j++) {
				rt->tstart_list[j] = rt->tstart_list[j+1];
				rt->width_list[j] = rt->width_list[j+1];
			}
			rt->length--;
		}
	}
	}
#ifdef RADAR_DBG
	/*
	PHY_RADAR(("Combined \n"));
	for (i = 0; i < rt->length; i++) {
		PHY_RADAR(("Tstart = %u,  Width = %d, FM = %d ", rt->tstart_list[i],
			rt->width_list[i], rt->fm_list[i]));
	}
	*/
#endif

	/* remove entries spaced greater than max_deltat */
	if (rt->length > 1) {
		deltat = (int32)(rt->tstart_list[rt->length - 1] - rt->tstart_list[0]);
		i = 0;
		while ((i < (rt->length - 1)) &&
		       (ABS(deltat) > (int32)rparams->radar_args.max_deltat)) {
			i++;
			deltat = (int32)(rt->tstart_list[rt->length - 1] - rt->tstart_list[i]);
		}
		if (i > 0) {
			for (j = i; j < rt->length; j++) {
				rt->tstart_list[j-i] = rt->tstart_list[j];
				rt->width_list[j-i] = rt->width_list[j];
			}
			rt->length -= i;
		}
	}

	/*
	 * filter based on pulse width
	 */
	if (filter_pw) {
		j = 0;
		for (i = 0; i < rt->length; i++) {
			if ((rt->width_list[i] >= rparams->radar_args.min_pw) &&
				(rt->width_list[i] <= rparams->radar_args.max_pw)) {
				rt->width_list[j] = rt->width_list[i];
				rt->tstart_list[j] = rt->tstart_list[i];
				j++;
			}
		}
		rt->length = j;
	}

	if (NREV_GE(pi->pubpi.phy_rev, 3)) { /* nphy rev >= 3 */
		ASSERT(rt->length <= 2*RDR_LIST_SIZE);
	} else {
		ASSERT(rt->length <= RDR_LIST_SIZE);
	}

	/*
	 * Break pulses into epochs.
	 */
	rt->nepochs = 1;
	rt->epoch_start[0] = 0;
	for (i = 1; i < rt->length; i++) {
		if ((int32)(rt->tstart_list[i] - rt->tstart_list[i-1]) > rparams->max_blen) {
			rt->epoch_finish[rt->nepochs-1] = i - 1;
			rt->epoch_start[rt->nepochs] = i;
			rt->nepochs++;
		}
	}
	if (rt->nepochs > RDR_EPOCH_SIZE)
		PHY_RADAR(("WARNING: number of epochs %d > epoch size\n", rt->nepochs));

	rt->epoch_finish[rt->nepochs - 1] = i;

	fra_det = 0;
	stg_det = 0;

	/*
	 * Run the detector for each epoch
	 */
	for (i = 0; i < rt->nepochs && (pulse_interval == 0) && !fra_det && (stg_det == 0); i++) {

		/*
		 * Generate 0th order tier list (time delta between received pulses)
		 * Quantize and filter delta pulse times delta pulse times are
		 * returned in sorted order from smallest to largest.
		 */
		epoch_list = rt->tstart_list + rt->epoch_start[i];
		epoch_length = (rt->epoch_finish[i] - rt->epoch_start[i] + 1);
		if (epoch_length > RDR_NTIER_SIZE) {
			PHY_RADAR(("TROUBLE: NEED TO MAKE TIER_SIZE BIGGER %d\n", epoch_length));
			return RADAR_TYPE_NONE;
		}

		wlc_phy_radar_detect_run_epoch_nphy(pi, i, rt, rparams, epoch_list, epoch_length,
			fra_t1, fra_t2, fra_t3, fra_err,
			pw_2us, pw15us, pw20us, pw30us,
			i250us, i500us, i625us, i5000us,
			pw2us, i833us, i2500us, i3333us,
			i938us, i3066us,
			&fra_det, &stg_det, &pulse_interval,
			&nconsecq_pulses, &detected_pulse_index);
	}

	if (rparams->radar_args.feature_mask & 0x40) {
		/*	Changed to display intervals intead of tstart
			PHY_RADAR(("Start Time:  "));
			for (i = 0; i < rt->length; i++)
				PHY_RADAR(("%u ", rt->tstart_list[i]));
		*/
		PHY_RADAR(("\nInterval:  "));
		for (i = 1; i < rt->length; i++)
			PHY_RADAR(("%d-%d ", rt->tstart_list[i] - rt->tstart_list[i - 1], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Pulse Widths:  "));
		for (i = 0; i < rt->length; i++)
			PHY_RADAR(("%i-%d ", rt->width_list[i], i));
		PHY_RADAR(("\n"));
	}

	if (rparams->radar_args.feature_mask & 0x80) {
		PHY_RADAR(("\nQuantized Intv: "));
		for (i = 0; i < epoch_length-2; i++)
			PHY_RADAR(("%d-%d ", rt->tiern_list[0][i], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Pruned PW:  "));
		for (i = 0; i <  epoch_length-1; i++)
			PHY_RADAR(("%i-%d ", rt->tiern_pw[0][i], i));
		PHY_RADAR(("\n"));
		PHY_RADAR(("nconsecq_pulses=%d max_pw_delta=%d min_pw=%d max_pw=%d \n",
			nconsecq_pulses, max_detected_pw - min_detected_pw, min_detected_pw,
			max_detected_pw));
	}

	epoch_detected = i;

	if (pulse_interval || (fra_det) || stg_det != 0) {
		int radar_type = RADAR_TYPE_UNCLASSIFIED;
		const char *radar_type_str = "UNCLASSIFIED";

		if (fra_det) {
			radar_type = RADAR_TYPE_FRA;
			radar_type_str = "S3F RADAR";
		} else if (stg_det == 2) {
			radar_type = RADAR_TYPE_STG2;
			radar_type_str = "S2 RADAR";
		} else if (stg_det == 3) {
			radar_type = RADAR_TYPE_STG3;
			radar_type_str = "S3 RADAR";
		} else for (i = 0; i < radar_class[i].id; i++) {
			if (pulse_interval == radar_class[i].pri) {
				radar_type = radar_class[i].id;
				radar_type_str = (char *)radar_class[i].name;
				break;
			}
		}

		if (rparams->radar_args.feature_mask & 0x8) {
			/*	Changed to display intervals intead of tstart
				PHY_RADAR(("Start Time:  "));
				for (i = 0; i < rt->length; i++)
					PHY_RADAR(("%u ", rt->tstart_list[i]));
			*/
			PHY_RADAR(("\nInterval:  "));
			for (i = 1; i < rt->length; i++)
				PHY_RADAR(("%d-%d ",
					rt->tstart_list[i] - rt->tstart_list[i - 1], i));
			PHY_RADAR(("\n"));

			PHY_RADAR(("Pulse Widths:  "));
			for (i = 0; i < rt->length; i++)
				PHY_RADAR(("%i-%d ", rt->width_list[i], i));
			PHY_RADAR(("\n"));
		}

		PHY_RADAR(("\nQuantized Intv: "));
		for (i = 0; i < epoch_length-1; i++)
			PHY_RADAR(("%d-%d ", rt->tiern_list[0][i], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Pruned PW:  "));
		for (i = 0; i <  epoch_length-1; i++)
			PHY_RADAR(("%i-%d ", rt->tiern_pw[0][i], i));
		PHY_RADAR(("\n"));

		PHY_RADAR(("Nepochs=%d len=%d epoch_#=%d; det_idx=%d "
				"max_pw_delta=%d min_pw=%d max_pw=%d \n",
				rt->nepochs, epoch_length, epoch_detected, detected_pulse_index,
				max_detected_pw - min_detected_pw, min_detected_pw,
				max_detected_pw));

		return (radar_type + (min_detected_pw << 4) +  (pulse_interval << 14));
	}

	return (RADAR_TYPE_NONE);
}

#endif /* #if defined(AP) && defined(RADAR) */

/* Increase the loop bandwidth of the PLL in the demodulator.
 * Note that although this allows the demodulator to track the
 * received carrier frequency over a wider bandwidth, it may
 * cause the Rx sensitivity to decrease
 */
void
wlc_phy_freqtrack_start(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (!ISGPHY(pi))
		return;

	wlc_phyreg_enter((wlc_phy_t*)pi);

	write_phy_reg(pi, BPHY_COEFFS, 0xFFEA);
	write_phy_reg(pi, BPHY_STEP, 0x0689);

	wlc_phyreg_exit((wlc_phy_t*)pi);
}


/* Restore the loop bandwidth of the PLL in the demodulator to the original value */
void
wlc_phy_freqtrack_end(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if (!ISGPHY(pi))
		return;

	wlc_phyreg_enter((wlc_phy_t*)pi);

	/* Restore the original values of the PHY registers */
	write_phy_reg(pi, BPHY_COEFFS, pi->freqtrack_saved_regs[0]);
	write_phy_reg(pi, BPHY_STEP, pi->freqtrack_saved_regs[1]);

	wlc_phyreg_exit((wlc_phy_t*)pi);
}

/* VCO calibration proc for 4318 */
static void
wlc_phy_vco_cal(phy_info_t *pi)
{
	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	if ((pi->pubpi.radioid == BCM2050_ID) &&
	    (pi->pubpi.radiorev == 8)) {
		chanspec_t old_chanspec = pi->radio_chanspec;
		wlc_phy_chanspec_set((wlc_phy_t*)pi, CHSPEC_CHANNEL(pi->radio_chanspec) > 7 ?
		CH20MHZ_CHSPEC(1) : CH20MHZ_CHSPEC(13));
		wlc_phy_chanspec_set((wlc_phy_t*)pi, old_chanspec);
	}
}

void
wlc_phy_set_deaf(wlc_phy_t *ppi, bool user_flag)
{
	phy_info_t *pi;
	pi = (phy_info_t*)ppi;

	if (ISLPPHY(pi))
		wlc_phy_set_deaf_lpphy(pi, (bool)1);
	else if (ISSSLPNPHY(pi))
		wlc_sslpnphy_deaf_mode(pi, TRUE);
	else if (ISLCNPHY(pi))
		wlc_lcnphy_deaf_mode(pi, TRUE);
	else if (ISNPHY(pi))
		wlc_nphy_deaf_mode(pi, TRUE);
	else if (ISHTPHY(pi))
		wlc_phy_deaf_htphy(pi, TRUE);
	else {
		PHY_ERROR(("%s: Not yet supported\n", __FUNCTION__));
		ASSERT(0);
	}
}

#if defined(WLTEST) || defined(AP)
static int
wlc_phy_iovar_perical_config(phy_info_t *pi, int32 int_val, int32 *ret_int_ptr,	bool set)
{
	int err = BCME_OK;

	if (!set) {
		if (!ISNPHY(pi) && !ISHTPHY(pi))
			return BCME_UNSUPPORTED;	/* supported for n and ht phy only */

		*ret_int_ptr =  pi->phy_cal_mode;
	} else {
		if (!ISNPHY(pi) && !ISHTPHY(pi))
			return BCME_UNSUPPORTED;	/* supported for n and ht phy only */

		if (int_val == 0) {
			pi->phy_cal_mode = PHY_PERICAL_DISABLE;
		} else if (int_val == 1) {
			pi->phy_cal_mode = PHY_PERICAL_SPHASE;
		} else if (int_val == 2) {
			pi->phy_cal_mode = PHY_PERICAL_MPHASE;
		} else if (int_val == 3) {
			/* this mode is to disable normal periodic cal paths
			 *  only manual trigger(nphy_forcecal) can run it
			 */
			pi->phy_cal_mode = PHY_PERICAL_MANUAL;
		} else {
			err = BCME_RANGE;
			goto end;
		}
		wlc_phy_cal_perical_mphase_reset(pi);
	}
end:
	return err;
}
#endif	

#if defined(BCMDBG) || defined(WLTEST)
static int
wlc_phy_iovar_tempsense(phy_info_t *pi, int32 *ret_int_ptr)
{
	int err = BCME_OK;
	int32 int_val;

	if (ISNPHY(pi)) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		*ret_int_ptr = (int32)wlc_phy_tempsense_nphy(pi);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
	} else if (ISHTPHY(pi)) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		*ret_int_ptr = (int32)wlc_phy_tempsense_htphy(pi);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
	} else if (ISLPPHY(pi)) {
		int_val = wlc_phy_tempsense_lpphy(pi);
		bcopy(&int_val, ret_int_ptr, sizeof(int_val));
	} else if (ISLCNPHY(pi)) {
		if (CHIPID(pi->sh->chip) == BCM4313_CHIP_ID)
			int_val = (int32)wlc_lcnphy_tempsense(pi, 1);
		else
			int_val = wlc_lcnphy_tempsense_degree(pi, 0);
		bcopy(&int_val, ret_int_ptr, sizeof(int_val));
	} else
		err = BCME_UNSUPPORTED;

	return err;
}

#endif	/* defined(BCMDBG) || defined(WLTEST) */

#if defined(WLTEST)
static int
wlc_phy_pkteng_stats_get(phy_info_t *pi, void *a, int alen)
{
	wl_pkteng_stats_t stats;
	uint16 rxstats_base;
	uint16 hi, lo;
	int i;

	if (D11REV_LT(pi->sh->corerev, 11))
		return BCME_UNSUPPORTED;

	if (!pi->sh->up) {
		return BCME_NOTUP;
	}

	PHY_INFORM(("Pkteng Stats Called\n"));

	/* Read with guard against carry */
	do {
		hi = wlapi_bmac_read_shm(pi->sh->physhim, M_PKTENG_FRMCNT_HI);
		lo = wlapi_bmac_read_shm(pi->sh->physhim, M_PKTENG_FRMCNT_LO);
	} while (hi != wlapi_bmac_read_shm(pi->sh->physhim, M_PKTENG_FRMCNT_HI));

	stats.lostfrmcnt = (hi << 16) | lo;

	if (ISLPPHY(pi) || ISNPHY(pi) || ISHTPHY(pi)) {
		stats.rssi = R_REG(pi->sh->osh, &pi->regs->rssi) & 0xff;
		if (stats.rssi > 127)
			stats.rssi -= 256;
		stats.snr = stats.rssi - (ISLPPHY(pi) ? PHY_NOISE_FIXED_VAL :
			PHY_NOISE_FIXED_VAL_NPHY);
	} else if (ISSSLPNPHY(pi)) {
		wlc_sslpnphy_pkteng_stats_get(pi, &stats);
	} else if (ISLCNPHY(pi)) {

		int16 rssi_lcn1, rssi_lcn2, rssi_lcn3, rssi_lcn4;
		int16 snr_a_lcn1, snr_a_lcn2, snr_a_lcn3, snr_a_lcn4;
		int16 snr_b_lcn1, snr_b_lcn2, snr_b_lcn3, snr_b_lcn4;
		uint8 gidx1, gidx2, gidx3, gidx4;
		int8 snr1, snr2, snr3, snr4;
		uint freq;
		phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

		rssi_lcn1 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_0);
		rssi_lcn2 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_1);
		rssi_lcn3 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_2);
		rssi_lcn4 = (int8)wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_3);

		gidx1 = (wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_0) & 0xfe00) >> 9;
		gidx2 = (wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_1) & 0xfe00) >> 9;
		gidx3 = (wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_2) & 0xfe00) >> 9;
		gidx4 = (wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_3) & 0xfe00) >> 9;

		rssi_lcn1 = rssi_lcn1 + lcnphy_gain_index_offset_for_pkt_rssi[gidx1];
		if (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_0)) & 0x0100) &&
		    (gidx1 == 12))
			rssi_lcn1 = rssi_lcn1 - 4;
		if ((rssi_lcn1 > -46) && (gidx1 > 18))
			rssi_lcn1 = rssi_lcn1 + 6;
		rssi_lcn2 = rssi_lcn2 + lcnphy_gain_index_offset_for_pkt_rssi[gidx2];
		if (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_1)) & 0x0100) &&
		    (gidx2 == 12))
			rssi_lcn2 = rssi_lcn2 - 4;
		if ((rssi_lcn2 > -46) && (gidx2 > 18))
			rssi_lcn2 = rssi_lcn2 + 6;
		rssi_lcn3 = rssi_lcn3 + lcnphy_gain_index_offset_for_pkt_rssi[gidx3];
		if (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_2)) & 0x0100) &&
		    (gidx3 == 12))
			rssi_lcn3 = rssi_lcn3 - 4;
		if ((rssi_lcn3 > -46) && (gidx3 > 18))
			rssi_lcn3 = rssi_lcn3 + 6;
		rssi_lcn4 = rssi_lcn4 + lcnphy_gain_index_offset_for_pkt_rssi[gidx4];
		if (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_3)) & 0x0100) &&
		    (gidx4 == 12))
			rssi_lcn4 = rssi_lcn4 - 4;
		if ((rssi_lcn4 > -46) && (gidx4 > 18))
			rssi_lcn4 = rssi_lcn4 + 6;
		stats.rssi = (rssi_lcn1 + rssi_lcn2 + rssi_lcn3 + rssi_lcn4) >> 2;

		/* Channel Correction Factor */
		freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
		if ((freq > 2427) && (freq <= 2467))
			stats.rssi = stats.rssi - 1;
		/* temperature compensation */
		stats.rssi = stats.rssi + pi_lcn->lcnphy_pkteng_rssi_slope;

		/* 2dB compensation of path loss for 4329 on Ref Boards */
		stats.rssi = stats.rssi + 2;
		if (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_1)) & 0x100) &&
		    (stats.rssi > -84) && (stats.rssi < -33))
			stats.rssi = stats.rssi + 3;

		/* SNR */
		snr_a_lcn1 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_A_0);
		snr_b_lcn1 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_B_0);
		snr1 = ((snr_a_lcn1 - snr_b_lcn1)* 3) >> 5;
		if ((stats.rssi < -92) ||
		    (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_0)) & 0x100)))
			snr1 = stats.rssi + 94;

		if (snr1 > 31)
			snr1 = 31;

		snr_a_lcn2 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_A_1);
		snr_b_lcn2 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_B_1);
		snr2 = ((snr_a_lcn2 - snr_b_lcn2)* 3) >> 5;
		if ((stats.rssi < -92) ||
		    (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_1)) & 0x100)))
			snr2 = stats.rssi + 94;

		if (snr2 > 31)
			snr2 = 31;

		snr_a_lcn3 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_A_2);
		snr_b_lcn3 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_B_2);
		snr3 = ((snr_a_lcn3 - snr_b_lcn3)* 3) >> 5;
		if ((stats.rssi < -92) ||
		    (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_2)) & 0x100)))
			snr3 = stats.rssi + 94;

		if (snr3 > 31)
			snr3 = 31;

		snr_a_lcn4 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_A_3);
		snr_b_lcn4 = wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_SNR_B_3);
		snr4 = ((snr_a_lcn4 - snr_b_lcn4)* 3) >> 5;
		if ((stats.rssi < -92) ||
		    (((wlapi_bmac_read_shm(pi->sh->physhim, M_LCN_RSSI_3)) & 0x100)))
			snr4 = stats.rssi + 94;

		if (snr4 > 31)
			snr4 = 31;

		stats.snr = ((snr1 + snr2 + snr3 + snr4)/4);
	} else {
		/* Not available */
		stats.rssi = stats.snr = 0;
	}

#if defined(WLNOKIA_NVMEM)
	/* Nokia NVMEM spec specifies the rssi offsets */
	stats.rssi = wlc_phy_upd_rssi_offset(pi, (int8)stats.rssi, pi->radio_chanspec);
#endif 

	/* rx pkt stats */
	rxstats_base = wlapi_bmac_read_shm(pi->sh->physhim, M_RXSTATS_BLK_PTR);
	for (i = 0; i <= NUM_80211_RATES; i++) {
		stats.rxpktcnt[i] = wlapi_bmac_read_shm(pi->sh->physhim, 2*(rxstats_base+i));
	}
	bcopy(&stats, a,
		(sizeof(wl_pkteng_stats_t) < (uint)alen) ? sizeof(wl_pkteng_stats_t) : (uint)alen);

	return BCME_OK;
}


void
wlc_phy_clear_deaf(wlc_phy_t  *ppi, bool user_flag)
{
	phy_info_t *pi;
	pi = (phy_info_t*)ppi;

	if (ISLPPHY(pi))
		wlc_phy_clear_deaf_lpphy(pi, (bool)1);
	else if (ISSSLPNPHY(pi))
		wlc_sslpnphy_deaf_mode(pi, FALSE);
	else if (ISLCNPHY(pi))
		wlc_lcnphy_deaf_mode(pi, FALSE);
	else if (ISNPHY(pi))
		wlc_nphy_deaf_mode(pi, FALSE);
	else if (ISHTPHY(pi))
		wlc_phy_deaf_htphy(pi, FALSE);
	else {
		PHY_ERROR(("%s: Not yet supported\n", __FUNCTION__));
		ASSERT(0);
	}
}

static int
wlc_phy_table_get(phy_info_t *pi, int32 int_val, void *p, void *a)
{
	int32 tblInfo[4];
	lpphytbl_info_t tab;
	phytbl_info_t tab2;

	if (ISAPHY(pi) || ISGPHY(pi))
		return BCME_UNSUPPORTED;

	/* other PHY */
	bcopy(p, tblInfo, 3*sizeof(int32));

	if (ISLPPHY(pi)) {
		tab.tbl_ptr = &int_val;
		tab.tbl_len = 1;
		tab.tbl_id = (uint32)tblInfo[0];
		tab.tbl_offset = (uint32)tblInfo[1];
		tab.tbl_width = (uint32)tblInfo[2];
		tab.tbl_phywidth = (uint32)tblInfo[2];
		wlc_phy_table_read_lpphy(pi, &tab);
	} else {
		tab2.tbl_ptr = &int_val;
		tab2.tbl_len = 1;
		tab2.tbl_id = (uint32)tblInfo[0];
		tab2.tbl_offset = (uint32)tblInfo[1];
		tab2.tbl_width = (uint32)tblInfo[2];

		if (ISSSLPNPHY(pi)) {
			wlc_sslpnphy_read_table(pi, &tab2);

		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_read_table(pi, &tab2);

		} else if (ISNPHY(pi)) {
			if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
				wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK, MCTL_PHYLOCK);
			}
			wlc_phy_read_table_nphy(pi, &tab2);
			if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
				wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  0);
			}
		} else if (ISHTPHY(pi)) {
			wlc_phy_read_table_htphy(pi, &tab2);
		}
	}
	bcopy(&int_val, a, sizeof(int_val));
	return BCME_OK;
}

static int
wlc_phy_table_set(phy_info_t *pi, int32 int_val, void *a)
{
	int32 tblInfo[4];
	lpphytbl_info_t tab;
	phytbl_info_t tab2;

	if (ISAPHY(pi) || ISGPHY(pi))
		return BCME_UNSUPPORTED;

	bcopy(a, tblInfo, 4*sizeof(int32));

	if (ISLPPHY(pi)) {
		int_val = tblInfo[3];
		tab.tbl_ptr = &int_val;
		tab.tbl_len = 1;
		tab.tbl_id = (uint32)tblInfo[0];
		tab.tbl_offset = (uint32)tblInfo[1];
		tab.tbl_width = (uint32)tblInfo[2];
		tab.tbl_phywidth = (uint32)tblInfo[2];
		wlc_phy_table_write_lpphy(pi, &tab);
	} else {
		int_val = tblInfo[3];
		tab2.tbl_ptr = &int_val;
		tab2.tbl_len = 1;
		tab2.tbl_id = (uint32)tblInfo[0];
		tab2.tbl_offset = (uint32)tblInfo[1];
		tab2.tbl_width = (uint32)tblInfo[2];

		if (ISSSLPNPHY(pi)) {
			wlc_sslpnphy_write_table(pi, &tab2);
		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_write_table(pi, &tab2);
		} else if (ISNPHY(pi)) {
			wlc_phy_write_table_nphy(pi, (&tab2));
		} else if (ISHTPHY(pi)) {
			wlc_phy_write_table_htphy(pi, (&tab2));
		}
	}
	return BCME_OK;
}

static int
wlc_phy_iovar_idletssi(phy_info_t *pi, int32 *ret_int_ptr, bool type)
{
	/* no argument or type = 1 will do full tx_pwr_ctrl_init */
	/* type = 0 will do just idle_tssi_est */
	int err = BCME_OK;
	if (ISLCNPHY(pi))
		*ret_int_ptr = wlc_lcnphy_idle_tssi_est_iovar(pi, type);
	return err;
}

static int
wlc_phy_iovar_txpwrindex_get(phy_info_t *pi, int32 int_val, bool bool_val, int32 *ret_int_ptr)
{
	int err = BCME_OK;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (ISNPHY(pi)) {

		if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
			wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  MCTL_PHYLOCK);
			(void)R_REG(pi->sh->osh, &pi->regs->maccontrol);
			OSL_DELAY(1);
		}

		*ret_int_ptr = wlc_phy_txpwr_idx_get_nphy(pi);

		if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))
			wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  0);

	} else if (ISHTPHY(pi)) {
		*ret_int_ptr = wlc_phy_txpwr_idx_get_htphy(pi);

	} else if (ISLPPHY(pi)) {
		if (wlc_phy_tpc_isenabled_lpphy(pi)) {
			/* Update current power index */
			wlc_phy_tx_pwr_update_npt_lpphy(pi);
			*ret_int_ptr = pi_lp->lpphy_tssi_idx;
		} else
			*ret_int_ptr = pi_lp->lpphy_tx_power_idx_override;
	} else if (ISSSLPNPHY(pi)) {
		*ret_int_ptr = wlc_sslpnphy_txpwr_idx_get(pi);

	} else if (ISLCNPHY(pi))
		*ret_int_ptr = wlc_lcnphy_get_current_tx_pwr_idx(pi);

	return err;
}


static int
wlc_phy_iovar_txpwrindex_set(phy_info_t *pi, void *p)
{
	int err = BCME_OK;
	uint32 txpwridx[PHY_CORE_MAX] = { 0x30 };
	int8 idx, core;
	int8 siso_int_val;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	bcopy(p, txpwridx, PHY_CORE_MAX * sizeof(uint32));
	siso_int_val = (int8)(txpwridx[0] & 0xff);

	if (ISNPHY(pi)) {
		FOREACH_CORE(pi, core) {
			idx = (int8)(txpwridx[core] & 0xff);
			pi->nphy_txpwrindex[core].index_internal = idx;
			wlc_phy_txpwr_index_nphy(pi, (1 << core), idx, TRUE);
		}
	} else if (ISHTPHY(pi)) {
		FOREACH_CORE(pi, core) {
			idx = (int8)(txpwridx[core] & 0xff);
			wlc_phy_txpwr_by_index_htphy(pi, (1 << core), idx);
		}
	} else if (ISLPPHY(pi)) {
		if (-1 == siso_int_val) {
			phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
			wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_HW);
			/* Reset calibration */
			pi_lp->lpphy_full_cal_channel = 0;
			pi->phy_forcecal = TRUE;
		} else if (siso_int_val >= 0) {
			wlc_phy_set_tx_pwr_by_index_lpphy(pi, siso_int_val);
		} else {
			err = BCME_RANGE;
		}
	} else if (ISSSLPNPHY(pi)) {
		phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
		if (-1 == siso_int_val) {
			wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
			/* Reset calibration */
			ph->sslpnphy_full_cal_channel = 0;
			pi->phy_forcecal = TRUE;
		} else if (siso_int_val >= 0) {
			wlc_sslpnphy_set_tx_pwr_by_index(pi, siso_int_val);
		} else {
			err = BCME_RANGE;
		}
	} else if (ISLCNPHY(pi)) {
		phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;

#if defined(WLTEST)
		/* Override power index if NVRAM says so */
		if (pi_lcn->txpwrindex_nvram)
			siso_int_val = pi_lcn->txpwrindex_nvram;
#endif 

		if (-1 == siso_int_val) {
			/* obsolete.  replaced with txpwrctrl */
			wlc_lcnphy_set_tx_pwr_ctrl(pi, LCNPHY_TX_PWR_CTRL_HW);
			/* Reset calibration */
			pi_lcn->lcnphy_full_cal_channel = 0;
			pi->phy_forcecal = TRUE;
		} else if (siso_int_val >= 0) {
			wlc_lcnphy_set_tx_pwr_by_index(pi, siso_int_val);
		} else {
			err = BCME_RANGE;
		}
	}

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	return err;
}

static int
wlc_phy_iovar_vbatsense(phy_info_t *pi, int32 *ret_int_ptr)
{
	int err = BCME_OK;
	int32 int_val;

	if (ISLCNPHY(pi)) {
		int_val = wlc_lcnphy_vbatsense(pi, 0);
		bcopy(&int_val, ret_int_ptr, sizeof(int_val));
	} else
		err = BCME_UNSUPPORTED;

	return err;
}

static int
wlc_phy_iovar_forcecal(phy_info_t *pi, int32 int_val, int32 *ret_int_ptr, int vsize, bool set)
{
	int err = BCME_OK;
	void *a;
	phy_info_sslpnphy_t *pi_ssn = pi->u.pi_sslpnphy;
	phy_info_lcnphy_t *pi_lcn = pi->u.pi_lcnphy;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	uint8 mphase = 0, searchmode = 0;
	a = (int32*)ret_int_ptr;

	if (ISHTPHY(pi)) {
		/* for get with no argument, assume 0x00 */
		if (!set)
			int_val = 0x00;

		/* upper nibble: 0 = sphase,  1 = mphase */
		mphase = (((uint8) int_val) & 0xf0) >> 4;

		/* 0 = RESTART, 1 = REFINE, for Tx-iq/lo-cal */
		searchmode = ((uint8) int_val) & 0xf;

		PHY_CAL(("wlc_phy_iovar_forcecal (mphase = %d, refine = %d)\n",
			mphase, searchmode));

		/* call sphase or schedule mphase cal */
		wlc_phy_cal_perical_mphase_reset(pi);
		if (mphase) {
			pi->cal_info->cal_searchmode = searchmode;
			wlc_phy_cal_perical_mphase_schedule(pi, PHY_PERICAL_NODELAY);
		} else {
			wlc_phy_cals_htphy(pi, searchmode);
		}

	} else if (ISNPHY(pi)) {
		/* for get with no argument, assume 0x00 */
		if (!set)
			int_val = PHY_PERICAL_AUTO;

		if ((int_val == PHY_PERICAL_PARTIAL) ||
		    (int_val == PHY_PERICAL_AUTO) ||
		    (int_val == PHY_PERICAL_FULL)) {
			wlc_phy_cal_perical_mphase_reset(pi);
			pi->cal_type_override = (uint8)int_val;
			wlc_phy_cal_perical_mphase_schedule(pi, PHY_PERICAL_NODELAY);
		} else
			err = BCME_RANGE;

	} else if (ISLPPHY(pi)) {
		/* don't care get or set with argument */

		pi_lp->lpphy_full_cal_channel = 0;
		wlc_phy_periodic_cal_lpphy(pi);
		int_val = 0;
		bcopy(&int_val, a, vsize);

	} else if (ISSSLPNPHY(pi)) {
		if (int_val != 0)
			pi->phy_forcecal = TRUE;
		pi_ssn->sslpnphy_full_cal_channel = 0;

	} else if (ISLCNPHY(pi)) {
		if (!set)
			*ret_int_ptr = 0;

		/* Wl forcecal to perform cal even when no param specified */
		pi_lcn->lcnphy_full_cal_channel = 0;
		wlc_lcnphy_calib_modes(pi, PHY_FULLCAL);
		int_val = 0;
		bcopy(&int_val, a, vsize);
	}

	return err;
}

static int
wlc_phy_iovar_txpwrctrl(phy_info_t *pi, int32 int_val, bool bool_val, int32 *ret_int_ptr,
	bool set)
{
	int err = BCME_OK;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;


	if (!set) {
		if (ISNPHY(pi)) {
			*ret_int_ptr = pi->nphy_txpwrctrl;
		} else if (ISHTPHY(pi)) {
			*ret_int_ptr = pi->txpwrctrl;
		} else if (ISLPPHY(pi)) {
			*ret_int_ptr = wlc_phy_tpc_isenabled_lpphy(pi);
		} else if (ISSSLPNPHY(pi)) {
			*ret_int_ptr = wlc_phy_tpc_isenabled_sslpnphy(pi);
		} else if (ISLCNPHY(pi)) {
			*ret_int_ptr = wlc_phy_tpc_iovar_isenabled_lcnphy(pi);
		}

	} else {
		if (ISNPHY(pi) || ISHTPHY(pi)) {
			if ((int_val != PHY_TPC_HW_OFF) && (int_val != PHY_TPC_HW_ON)) {
				err = BCME_RANGE;
				goto end;
			}

			pi->nphy_txpwrctrl = (uint8)int_val;
			pi->txpwrctrl = (uint8)int_val;

			/* if not up, we are done */
			if (!pi->sh->up)
				goto end;

			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);
			if (ISNPHY(pi))
				wlc_phy_txpwrctrl_enable_nphy(pi, (uint8) int_val);
			else if (ISHTPHY(pi))
				wlc_phy_txpwrctrl_enable_htphy(pi, (uint8) int_val);
			wlc_phyreg_exit((wlc_phy_t *)pi);
			wlapi_enable_mac(pi->sh->physhim);

		} else if (ISLPPHY(pi)) {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);

			wlc_phy_set_tx_pwr_ctrl_lpphy(pi,
				int_val ? LPPHY_TX_PWR_CTRL_HW : LPPHY_TX_PWR_CTRL_SW);
			/* Reset calibration */
			pi_lp->lpphy_full_cal_channel = 0;
			pi->phy_forcecal = TRUE;

			wlc_phyreg_exit((wlc_phy_t *)pi);
			wlapi_enable_mac(pi->sh->physhim);
		} else if (ISSSLPNPHY(pi)) {
			wlc_sslpnphy_iovar_txpwrctrl(pi, int_val);

		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_iovar_txpwrctrl(pi, int_val, ret_int_ptr);
		}
	}

end:
	return err;
}

static int
wlc_phy_iovar_txrx_chain(phy_info_t *pi, int32 int_val, int32 *ret_int_ptr, bool set)
{
	int err = BCME_OK;

	if (ISHTPHY(pi))
		return BCME_UNSUPPORTED;

	if (!set) {
		if (ISNPHY(pi)) {
			*ret_int_ptr = (int)pi->nphy_txrx_chain;
		}
	} else {
		if (ISNPHY(pi)) {
			if ((int_val != AUTO) && (int_val != WLC_N_TXRX_CHAIN0) &&
				(int_val != WLC_N_TXRX_CHAIN1)) {
				err = BCME_RANGE;
				goto end;
			}

			if (pi->nphy_txrx_chain != (int8)int_val) {
				pi->nphy_txrx_chain = (int8)int_val;
				if (pi->sh->up) {
					wlapi_suspend_mac_and_wait(pi->sh->physhim);
					wlc_phyreg_enter((wlc_phy_t *)pi);
					wlc_phy_stf_chain_upd_nphy(pi);
					wlc_phy_force_rfseq_nphy(pi, NPHY_RFSEQ_RESET2RX);
					wlc_phyreg_exit((wlc_phy_t *)pi);
					wlapi_enable_mac(pi->sh->physhim);
				}
			}
		}
	}
end:
	return err;
}

static void
wlc_phy_iovar_bphy_testpattern(phy_info_t *pi, uint8 testpattern, bool enable)
{
	bool existing_enable = FALSE;

	/* WL out check */
	if (pi->sh->up) {
		PHY_ERROR(("wl%d: %s: This function needs to be called after 'wl out'\n",
		          pi->sh->unit, __FUNCTION__));
		return;
	}

	/* confirm band is locked to 2G */
	if (!CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_ERROR(("wl%d: %s: Band needs to be locked to 2G (b)\n",
		          pi->sh->unit, __FUNCTION__));
		return;
	}

	if (NREV_LT(pi->pubpi.phy_rev, 2) || ISHTPHY(pi)) {
		PHY_ERROR(("wl%d: %s: This function is supported only for NPHY PHY_REV > 1\n",
		          pi->sh->unit, __FUNCTION__));
		return;
	}

	if (testpattern == NPHY_TESTPATTERN_BPHY_EVM) {    /* CW CCK for EVM testing */
		existing_enable = (bool) pi->phy_bphy_evm;
	} else if (testpattern == NPHY_TESTPATTERN_BPHY_RFCS) { /* RFCS testpattern */
		existing_enable = (bool) pi->phy_bphy_rfcs;
	} else {
		PHY_ERROR(("Testpattern needs to be between [0 (BPHY_EVM), 1 (BPHY_RFCS)]\n"));
		ASSERT(0);
	}

	if (ISNPHY(pi)) {
		wlc_phy_bphy_testpattern_nphy(pi, testpattern, enable, existing_enable);
	} else {
		PHY_ERROR(("support yet to be added\n"));
		ASSERT(0);
	}

	/* Return state of testpattern enables */
	if (testpattern == NPHY_TESTPATTERN_BPHY_EVM) {    /* CW CCK for EVM testing */
		pi->phy_bphy_evm = enable;
	} else if (testpattern == NPHY_TESTPATTERN_BPHY_RFCS) { /* RFCS testpattern */
		pi->phy_bphy_rfcs = enable;
	}
}

static void
wlc_phy_iovar_scraminit(phy_info_t *pi, int8 scraminit)
{
	pi->phy_scraminit = (int8)scraminit;
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (ISNPHY(pi)) {
		wlc_phy_test_scraminit_nphy(pi, scraminit);
	} else if (ISHTPHY(pi)) {
		wlc_phy_test_scraminit_htphy(pi, scraminit);
	} else {
		PHY_ERROR(("support yet to be added\n"));
		ASSERT(0);
	}

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}

static void
wlc_phy_iovar_force_rfseq(phy_info_t *pi, uint8 int_val)
{
	wlc_phyreg_enter((wlc_phy_t *)pi);
	if (ISNPHY(pi)) {
		wlc_phy_force_rfseq_nphy(pi, int_val);
	} else if (ISHTPHY(pi)) {
		wlc_phy_force_rfseq_htphy(pi, int_val);
	}
	wlc_phyreg_exit((wlc_phy_t *)pi);
}

static void
wlc_phy_iovar_tx_tone(phy_info_t *pi, uint32 int_val)
{
	pi->phy_tx_tone_freq = (uint32) int_val;

	if (pi->phy_tx_tone_freq == 0) {
		if (ISNPHY(pi)) {
			/* Restore back PAPD settings after stopping the tone */
			if (pi->pubpi.phy_rev == LCNXN_BASEREV)
				wlc_phy_papd_enable_nphy(pi, TRUE);
			wlc_phy_stopplayback_nphy(pi);
			wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);
		} else if (ISHTPHY(pi)) {
			wlc_phy_stopplayback_htphy(pi);
			wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);
		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_stop_tx_tone(pi);
		}
	} else {
		if (ISNPHY(pi)) {
			/* use 151 since that should correspond to nominal tx output power */
			/* Can not play tone with papd bit enabled */
			if (pi->pubpi.phy_rev == LCNXN_BASEREV)
				wlc_phy_papd_enable_nphy(pi, FALSE);
			wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);
			wlc_phy_tx_tone_nphy(pi, (uint32)int_val, 151, 0, 0, TRUE); /* play tone */
		} else if (ISHTPHY(pi)) {
			wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);
			wlc_phy_tx_tone_htphy(pi, (uint32)int_val, 151, 0, 0, TRUE); /* play tone */
		} else if (ISLCNPHY(pi)) {
			wlc_lcnphy_set_tx_tone_and_gain_idx(pi);
		}
	}
}

static int8
wlc_phy_iovar_test_tssi(phy_info_t *pi, uint8 val, uint8 pwroffset)
{
	int8 tssi = 0;
	if (ISNPHY(pi)) {
		tssi = wlc_phy_test_tssi_nphy(pi, val, pwroffset);
	} else if (ISHTPHY(pi)) {
		tssi = wlc_phy_test_tssi_htphy(pi, val, pwroffset);
	}
	return tssi;
}

static void
wlc_phy_iovar_rxcore_enable(phy_info_t *pi, int32 int_val, bool bool_val, int32 *ret_int_ptr,
	bool set)
{
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (set) {
		if (ISNPHY(pi)) {
			wlc_phy_rxcore_setstate_nphy((wlc_phy_t *)pi, (uint8) int_val);
		} else if (ISHTPHY(pi)) {
			wlc_phy_rxcore_setstate_htphy((wlc_phy_t *)pi, (uint8) int_val);
		}
	} else {
		if (ISNPHY(pi)) {
			*ret_int_ptr =  (uint32)wlc_phy_rxcore_getstate_nphy((wlc_phy_t *)pi);
		} else if (ISHTPHY(pi)) {
			*ret_int_ptr =  (uint32)wlc_phy_rxcore_getstate_htphy((wlc_phy_t *)pi);
		}
	}

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}

#endif 

/* Dispatch phy iovars */
int
wlc_phy_iovar_dispatch(wlc_phy_t *pih, uint32 actionid, uint16 type, void *p, uint plen, void *a,
	int alen, int vsize)
{
	phy_info_t *pi = (phy_info_t*)pih;
	int32 int_val = 0;
	bool bool_val;
	int err = BCME_OK;
	int32 *ret_int_ptr = (int32 *)a;

	if (plen >= (uint)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	/* bool conversion to avoid duplication below */
	bool_val = int_val != 0;

	switch (actionid) {
#if defined(AP) && defined(RADAR)
	case IOV_GVAL(IOV_RADAR_ARGS):
		bcopy((char*)&pi->rargs[0].radar_args, (char*)a, sizeof(wl_radar_args_t));
		break;

	case IOV_GVAL(IOV_RADAR_ARGS_40MHZ):
		/* any other phy supports 40Mhz channel ? */
		if (!ISNPHY(pi)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		bcopy((char*)&pi->rargs[1].radar_args, (char*)a, sizeof(wl_radar_args_t));
		break;

	case IOV_SVAL(IOV_RADAR_THRS):
		if (ISAPHY(pi) || ISLPPHY(pi) || ISNPHY(pi)) {
			wl_radar_thr_t radar_thr;

			/* len is check done before gets here */
			bzero((char*)&radar_thr, sizeof(wl_radar_thr_t));
			bcopy((char*)p, (char*)&radar_thr, sizeof(wl_radar_thr_t));
			if (radar_thr.version != WL_RADAR_THR_VERSION) {
				err = BCME_VERSION;
				break;
			}
			pi->rargs[0].radar_thrs.thresh0_20_lo = radar_thr.thresh0_20_lo;
			pi->rargs[0].radar_thrs.thresh1_20_lo = radar_thr.thresh1_20_lo;
			pi->rargs[0].radar_thrs.thresh0_20_hi = radar_thr.thresh0_20_hi;
			pi->rargs[0].radar_thrs.thresh1_20_hi = radar_thr.thresh1_20_hi;
			if (ISNPHY(pi)) {
				pi->rargs[1].radar_thrs.thresh0_40_lo = radar_thr.thresh0_40_lo;
				pi->rargs[1].radar_thrs.thresh1_40_lo = radar_thr.thresh1_40_lo;
				pi->rargs[1].radar_thrs.thresh0_40_hi = radar_thr.thresh0_40_hi;
				pi->rargs[1].radar_thrs.thresh1_40_hi = radar_thr.thresh1_40_hi;
			}
			wlc_phy_radar_detect_init(pi, pi->sh->radar);
		} else
			err = BCME_UNSUPPORTED;
		break;

#if defined(BCMDBG) || defined(WLTEST)
	case IOV_SVAL(IOV_RADAR_ARGS):
		if (!pi->sh->up) {
			err = BCME_NOTUP;
			break;
		}

		if (ISAPHY(pi) || ISLPPHY(pi) || ISNPHY(pi)) {
			wl_radar_args_t radarargs;

			/* len is check done before gets here */
			bcopy((char*)p, (char*)&radarargs, sizeof(wl_radar_args_t));
			if (radarargs.version != WL_RADAR_ARGS_VERSION) {
				err = BCME_VERSION;
				break;
			}
			bcopy((char*)&radarargs, (char*)&pi->rargs[0].radar_args,
			      sizeof(wl_radar_args_t));
			/* apply radar inits to hardware if we are on the A/LP/NPHY */
			wlc_phy_radar_detect_init(pi, pi->sh->radar);
		} else
			err = BCME_UNSUPPORTED;

		break;

	case IOV_SVAL(IOV_RADAR_ARGS_40MHZ):
		if (!pi->sh->up) {
			err = BCME_NOTUP;
			break;
		}

		/* any other phy supports 40Mhz channel ? */
		if (ISNPHY(pi)) {
			wl_radar_args_t radarargs;

			/* len check done before gets here */
			bcopy((char*)p, (char*)&radarargs, sizeof(wl_radar_args_t));
			if (radarargs.version != WL_RADAR_ARGS_VERSION) {
				err = BCME_VERSION;
				break;
			}
			bcopy((char*)&radarargs, (char*)&pi->rargs[1].radar_args,
			      sizeof(wl_radar_args_t));
			/* apply radar inits to hardware if we are NPHY */
			wlc_phy_radar_detect_init(pi, pi->sh->radar);
		} else
			err = BCME_UNSUPPORTED;

		break;
#endif 
#endif /* defined(AP) && defined(RADAR) */

#if defined(BCMDBG) || defined(WLTEST)
	case IOV_GVAL(IOV_FAST_TIMER):
		*ret_int_ptr = (int32)pi->sh->fast_timer;
		break;

	case IOV_SVAL(IOV_FAST_TIMER):
		pi->sh->fast_timer = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_SLOW_TIMER):
		*ret_int_ptr = (int32)pi->sh->slow_timer;
		break;

	case IOV_SVAL(IOV_SLOW_TIMER):
		pi->sh->slow_timer = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_GLACIAL_TIMER):
		*ret_int_ptr = (int32)pi->sh->glacial_timer;
		break;

	case IOV_SVAL(IOV_GLACIAL_TIMER):
		pi->sh->glacial_timer = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_PHY_TEMPSENSE):
		wlc_phy_iovar_tempsense(pi, ret_int_ptr);
		break;

#endif /* BCMDBG || WLTEST */

#if defined(BCMDBG) || defined(WLTEST)
	case IOV_GVAL(IOV_TXINSTPWR):
		wlc_phyreg_enter((wlc_phy_t *)pi);
		/* Return the current instantaneous est. power
		 * For swpwr ctrl it's based on current TSSI value (as opposed to average)
		 */
		wlc_phy_txpower_get_instant(pi, a);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		break;
#endif 

#ifdef SAMPLE_COLLECT
	case IOV_GVAL(IOV_PHY_SAMPLE_COLLECT):
	{
		wl_samplecollect_args_t samplecollect_args;

		if (plen < (int)sizeof(wl_samplecollect_args_t)) {
			PHY_ERROR(("plen (%d) < sizeof(wl_samplecollect_args_t) (%d)\n",
				plen, (int)sizeof(wl_samplecollect_args_t)));
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy((char*)p, (char*)&samplecollect_args, sizeof(wl_samplecollect_args_t));
		if (samplecollect_args.version != WL_SAMPLECOLLECT_T_VERSION) {
			PHY_ERROR(("Incompatible version; use %d expected version %d\n",
				samplecollect_args.version, WL_SAMPLECOLLECT_T_VERSION));
			err = BCME_BADARG;
			break;
		}

		if (samplecollect_args.length > alen) {
			PHY_ERROR(("Bad length, length requested > buf len (%d > %d)\n",
				samplecollect_args.length, alen));
			err = BCME_BADLEN;
			break;
		}

		err = wlc_phy_sample_collect(pi, &samplecollect_args, a);
		break;
	}
	case IOV_GVAL(IOV_PHY_SAMPLE_DATA):
	{
		wl_sampledata_t sampledata_args;

		if (plen < (int)sizeof(wl_sampledata_t)) {
			PHY_ERROR(("plen (%d) < sizeof(wl_samplecollect_args_t) (%d)\n",
				plen, (int)sizeof(wl_sampledata_t)));
			err = BCME_BUFTOOSHORT;
			break;
		}
		bcopy((char*)p, (char*)&sampledata_args, sizeof(wl_sampledata_t));
		if (sampledata_args.version != WL_SAMPLEDATA_T_VERSION) {
			PHY_ERROR(("Incompatible version; use %d expected version %d\n",
				sampledata_args.version, WL_SAMPLEDATA_T_VERSION));
			err = BCME_BADARG;
			break;
		}

		if (sampledata_args.length > alen) {
			PHY_ERROR(("Bad length, length requested > buf len (%d > %d)\n",
				sampledata_args.length, alen));
			err = BCME_BADLEN;
			break;
		}

		err = wlc_phy_sample_data(pi, &sampledata_args, a);
		break;
	}
#endif /* SAMPLE_COLLECT */

#if defined(WLTEST)

	case IOV_GVAL(IOV_PHY_TXIQCC):
		{
			int32 iqccValues[2];
			uint16 valuea = 0;
			uint16 valueb = 0;
			txiqccgetfn_t txiqcc_fn = pi->pi_fptr.txiqccget;
			if (txiqcc_fn) {
				(*txiqcc_fn)(pi, &valuea, &valueb);
				iqccValues[0] = valuea;
				iqccValues[1] = valueb;
				bcopy(iqccValues, a, 2*sizeof(int32));
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_TXIQCC):
		{
			int32 iqccValues[2];
			uint16 valuea, valueb;
			txiqccsetfn_t txiqcc_fn = pi->pi_fptr.txiqccset;

			if (txiqcc_fn) {
				bcopy(p, iqccValues, 2*sizeof(int32));
				valuea = (uint16)(iqccValues[0]);
				valueb = (uint16)(iqccValues[1]);
				(*txiqcc_fn)(pi, valuea, valueb);
			}

		}
		break;

	case IOV_GVAL(IOV_PHY_TXLOCC):
		{
			uint16 di0dq0;
			uint8 *loccValues = a;
			txloccgetfn_t txlocc_fn = pi->pi_fptr.txloccget;
			radioloftgetfn_t radio_loft_fn = pi->pi_fptr.radioloftget;

			if ((txlocc_fn) && (radio_loft_fn))
			{
				/* copy the 6 bytes to a */
				di0dq0 = (*txlocc_fn)(pi);
				loccValues[0] = (uint8)(di0dq0 >> 8);
				loccValues[1] = (uint8)(di0dq0 & 0xff);
				(*radio_loft_fn)(pi, &loccValues[2], &loccValues[3],
					&loccValues[4], &loccValues[5]);
			}
		}
		break;

	case IOV_SVAL(IOV_PHY_TXLOCC):
		{
			/* copy 6 bytes from a to radio */
			uint16 di0dq0;
			uint8 *loccValues = p;
			di0dq0 = ((uint16)loccValues[0] << 8) | loccValues[1];
			if (ISLPPHY(pi)) {
				wlc_phy_set_tx_locc_lpphy(pi, di0dq0);
				wlc_phy_set_radio_loft_lpphy(pi, loccValues[2],
					loccValues[3], loccValues[4], loccValues[5]);
				wlc_phy_set_tx_locc_ucode_lpphy(pi, FALSE, di0dq0);
			}
			else if (ISLCNPHY(pi)) {
				wlc_lcnphy_set_tx_locc(pi, di0dq0);
				wlc_lcnphy_set_radio_loft(pi, loccValues[2],
					loccValues[3], loccValues[4], loccValues[5]);
			}
			else if (ISSSLPNPHY(pi)) {
			}
		}
		break;

	case IOV_GVAL(IOV_PHYHAL_MSG):
		*ret_int_ptr = (int32)phyhal_msg_level;
		break;

	case IOV_SVAL(IOV_PHYHAL_MSG):
		phyhal_msg_level = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_PHY_WATCHDOG):
		*ret_int_ptr = (int32)pi->phywatchdog_override;
		break;

	case IOV_SVAL(IOV_PHY_WATCHDOG):
		pi->phywatchdog_override = bool_val;
		break;

	case IOV_SVAL(IOV_PHY_FIXED_NOISE):
		pi->phy_fixed_noise = bool_val;
		break;

	case IOV_GVAL(IOV_PHY_FIXED_NOISE):
		int_val = (int32)pi->phy_fixed_noise;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_GVAL(IOV_PHYNOISE_POLL):
		*ret_int_ptr = (int32)pi->phynoise_polling;
		break;

	case IOV_SVAL(IOV_PHYNOISE_POLL):
		pi->phynoise_polling = bool_val;
		break;

	case IOV_GVAL(IOV_PHY_CAL_DISABLE):
		*ret_int_ptr = (int32)pi->disable_percal;
		break;

	case IOV_SVAL(IOV_PHY_CAL_DISABLE):
		pi->disable_percal = bool_val;
		break;
		
#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	case IOV_GVAL(IOV_ENABLE_PERCAL_DELAY):
		*ret_int_ptr = (int32)pi->enable_percal_delay;
		break;

	case IOV_SVAL(IOV_ENABLE_PERCAL_DELAY):
		pi->enable_percal_delay = bool_val;
		break;
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */

	case IOV_GVAL(IOV_CARRIER_SUPPRESS):
		if (!(ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)))
			return BCME_UNSUPPORTED;	/* lpphy, sslpnphy and lcnphy for now */
		*ret_int_ptr = (pi->carrier_suppr_disable == 0);
		break;

	case IOV_SVAL(IOV_CARRIER_SUPPRESS):
	{
		initfn_t carr_suppr_fn = pi->pi_fptr.carrsuppr;

		if (carr_suppr_fn) {
			pi->carrier_suppr_disable = bool_val;
			if (pi->carrier_suppr_disable) {
				(*carr_suppr_fn)(pi);
			}
			PHY_INFORM(("Carrier Suppress Called\n"));
		}
		else
			return BCME_UNSUPPORTED;

		break;
	}

	case IOV_GVAL(IOV_UNMOD_RSSI):
	{
		int32 input_power_db = 0;
		rxsigpwrfn_t rx_sig_pwr_fn = pi->pi_fptr.rxsigpwr;

		PHY_INFORM(("UNMOD RSSI Called\n"));

		if (!rx_sig_pwr_fn)
			return BCME_UNSUPPORTED;	/* lpphy and sslnphy support only for now */

		if (!pi->sh->up) {
			err = BCME_NOTUP;
			break;
		}

		input_power_db = (*rx_sig_pwr_fn)(pi, -1);

#if defined(WLNOKIA_NVMEM)
		input_power_db = wlc_phy_upd_rssi_offset(pi,
			(int8)input_power_db, pi->radio_chanspec);
#endif 

		*ret_int_ptr = input_power_db;
		break;
	}

	case IOV_GVAL(IOV_PKTENG_STATS):
		wlc_phy_pkteng_stats_get(pi, a, alen);
		break;

	case IOV_GVAL(IOV_PHYTABLE):
		wlc_phy_table_get(pi, int_val, p, a);
		break;

	case IOV_SVAL(IOV_PHYTABLE):
		wlc_phy_table_set(pi, int_val, a);
		break;

	case IOV_SVAL(IOV_ACI_EXIT_CHECK_PERIOD):
		if (int_val == 0)
			err = BCME_RANGE;
		else
			pi->aci_exit_check_period = int_val;
		break;

	case IOV_GVAL(IOV_ACI_EXIT_CHECK_PERIOD):
		int_val = pi->aci_exit_check_period;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_GVAL(IOV_PAVARS): {
		uint16 *outpa = a;
		uint16 inpa[WL_PHY_PAVARS_LEN];
		uint j = 3;	/* PA parameters start from offset 3 */

		bcopy(p, inpa, sizeof(inpa));

		outpa[0] = inpa[0]; /* Phy type */
		outpa[1] = inpa[1]; /* Band range */
		outpa[2] = inpa[2]; /* Chain */

		if (ISHTPHY(pi)) {
			if (inpa[0] != PHY_TYPE_HT) {
				outpa[0] = PHY_TYPE_NULL;
				break;
			}

			if (inpa[2] >= pi->pubpi.phy_corenum)
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
			case WL_CHAN_FREQ_RANGE_5GL:
			case WL_CHAN_FREQ_RANGE_5GM:
			case WL_CHAN_FREQ_RANGE_5GH:
				wlc_phy_pavars_get_htphy(pi, &outpa[j], inpa[1], inpa[2]);
				break;
			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[1]));
				break;
			}
		} else if (ISNPHY(pi)) {
			phy_pwrctrl_t	*pwrctl;

			if (inpa[0] != PHY_TYPE_N) {
				outpa[0] = PHY_TYPE_NULL;
				break;
			}

			if (inpa[2] == 0)
				pwrctl = &pi->nphy_pwrctrl_info[PHY_CORE_0];
			else if (inpa[2] == 1)
				pwrctl = &pi->nphy_pwrctrl_info[PHY_CORE_1];
			else
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
				outpa[j++] = pwrctl->pwrdet_2g_a1;	/* a1 */
				outpa[j++] = pwrctl->pwrdet_2g_b0;	/* b0 */
				outpa[j++] = pwrctl->pwrdet_2g_b1;	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				outpa[j++] = pwrctl->pwrdet_5gl_a1;	/* a1 */
				outpa[j++] = pwrctl->pwrdet_5gl_b0;	/* b0 */
				outpa[j++] = pwrctl->pwrdet_5gl_b1;	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				outpa[j++] = pwrctl->pwrdet_5gm_a1;	/* a1 */
				outpa[j++] = pwrctl->pwrdet_5gm_b0;	/* b0 */
				outpa[j++] = pwrctl->pwrdet_5gm_b1;	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				outpa[j++] = pwrctl->pwrdet_5gh_a1;	/* a1 */
				outpa[j++] = pwrctl->pwrdet_5gh_b0;	/* b0 */
				outpa[j++] = pwrctl->pwrdet_5gh_b1;	/* b1 */
				break;

			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[0]));
				break;
			}
		} else if (ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
			if ((inpa[0] != PHY_TYPE_LP) && (inpa[0] != PHY_TYPE_SSN) &&
				((inpa[0] != PHY_TYPE_LCN))) {
				outpa[0] = PHY_TYPE_NULL;
				break;
			}

			if (inpa[2] != 0)
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
				outpa[j++] = pi->txpa_2g[0];		/* b0 */
				outpa[j++] = pi->txpa_2g[1];		/* b1 */
				outpa[j++] = pi->txpa_2g[2];		/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				outpa[j++] = pi->txpa_5g_low[0];	/* b0 */
				outpa[j++] = pi->txpa_5g_low[1];	/* b1 */
				outpa[j++] = pi->txpa_5g_low[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				outpa[j++] = pi->txpa_5g_mid[0];	/* b0 */
				outpa[j++] = pi->txpa_5g_mid[1];	/* b1 */
				outpa[j++] = pi->txpa_5g_mid[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				outpa[j++] = pi->txpa_5g_hi[0];	/* b0 */
				outpa[j++] = pi->txpa_5g_hi[1];	/* b1 */
				outpa[j++] = pi->txpa_5g_hi[2];	/* a1 */
				break;
			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[0]));
				break;
			}
		} else {
			PHY_ERROR(("Unsupported PHY type!\n"));
			err = BCME_UNSUPPORTED;
		}
	}
	break;

	case IOV_SVAL(IOV_PAVARS): {
		uint16 inpa[WL_PHY_PAVARS_LEN];
		uint j = 3;	/* PA parameters start from offset 3 */

		bcopy(p, inpa, sizeof(inpa));
		if (ISHTPHY(pi)) {
			if (inpa[0] != PHY_TYPE_HT) {
				break;
			}

			if (inpa[2] >= pi->pubpi.phy_corenum)
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
			case WL_CHAN_FREQ_RANGE_5GL:
			case WL_CHAN_FREQ_RANGE_5GM:
			case WL_CHAN_FREQ_RANGE_5GH:
				wlc_phy_pavars_set_htphy(pi, &inpa[j], inpa[1], inpa[2]);
				break;
			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[1]));
				break;
			}
		} else if (ISNPHY(pi)) {
			phy_pwrctrl_t	*pwrctl;

			if (inpa[0] != PHY_TYPE_N)
				break;

			if (inpa[2] == 0)
				pwrctl = &pi->nphy_pwrctrl_info[PHY_CORE_0];
			else if (inpa[2] == 1)
				pwrctl = &pi->nphy_pwrctrl_info[PHY_CORE_1];
			else
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
				pwrctl->pwrdet_2g_a1 = inpa[j++];	/* a1 */
				pwrctl->pwrdet_2g_b0 = inpa[j++];	/* b0 */
				pwrctl->pwrdet_2g_b1 = inpa[j++];	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				pwrctl->pwrdet_5gl_a1 = inpa[j++];	/* a1 */
				pwrctl->pwrdet_5gl_b0 = inpa[j++];	/* b0 */
				pwrctl->pwrdet_5gl_b1 = inpa[j++];	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				pwrctl->pwrdet_5gm_a1 = inpa[j++];	/* a1 */
				pwrctl->pwrdet_5gm_b0 = inpa[j++];	/* b0 */
				pwrctl->pwrdet_5gm_b1 = inpa[j++];	/* b1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				pwrctl->pwrdet_5gh_a1 = inpa[j++];	/* a1 */
				pwrctl->pwrdet_5gh_b0 = inpa[j++];	/* b0 */
				pwrctl->pwrdet_5gh_b1 = inpa[j++];	/* b1 */
				break;
			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[0]));
				break;
			}
		} else if (ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
			if ((inpa[0] != PHY_TYPE_LP) && (inpa[0] != PHY_TYPE_SSN) &&
				(inpa[0] != PHY_TYPE_LCN))
				break;

			if (inpa[2] != 0)
				return BCME_BADARG;

			switch (inpa[1]) {
			case WL_CHAN_FREQ_RANGE_2G:
				pi->txpa_2g[0] = inpa[j++];	/* b0 */
				pi->txpa_2g[1] = inpa[j++];	/* b1 */
				pi->txpa_2g[2] = inpa[j++];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				pi->txpa_5g_low[0] = inpa[j++];	/* b0 */
				pi->txpa_5g_low[1] = inpa[j++];	/* b1 */
				pi->txpa_5g_low[2] = inpa[j++];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				pi->txpa_5g_mid[0] = inpa[j++];	/* b0 */
				pi->txpa_5g_mid[1] = inpa[j++];	/* b1 */
				pi->txpa_5g_mid[2] = inpa[j++];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				pi->txpa_5g_hi[0] = inpa[j++];	/* b0 */
				pi->txpa_5g_hi[1] = inpa[j++];	/* b1 */
				pi->txpa_5g_hi[2] = inpa[j++];	/* a1 */
				break;
			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpa[0]));
				break;
			}
		} else {
			PHY_ERROR(("Unsupported PHY type!\n"));
			err = BCME_UNSUPPORTED;
		}
	}
	break;

	case IOV_GVAL(IOV_POVARS): {
		wl_po_t tmppo;

		/* tmppo has the input phy_type and band */
		bcopy(p, &tmppo, sizeof(wl_po_t));
		if (ISNPHY(pi)) {
			if (tmppo.phy_type != PHY_TYPE_N)  {
				tmppo.phy_type = PHY_TYPE_NULL;
				break;
			}

			switch (tmppo.band) {
			case WL_CHAN_FREQ_RANGE_2G:
				tmppo.cckpo = pi->cck2gpo;
				tmppo.ofdmpo = pi->ofdm2gpo;
				bcopy(&pi->mcs2gpo, &tmppo.mcspo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				tmppo.ofdmpo = pi->ofdm5glpo;
				bcopy(&pi->mcs5glpo, &tmppo.mcspo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				tmppo.ofdmpo = pi->ofdm5gpo;
				bcopy(&pi->mcs5gpo, &tmppo.mcspo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				tmppo.ofdmpo = pi->ofdm5ghpo;
				bcopy(&pi->mcs5ghpo, &tmppo.mcspo, 8*sizeof(uint16));
				break;

			default:
				PHY_ERROR(("bandrange %d is out of scope\n", tmppo.band));
				err = BCME_BADARG;
				break;
			}

			if (!err)
				bcopy(&tmppo, a, sizeof(wl_po_t));
		} else if (ISLCNPHY(pi)) {
			if (tmppo.phy_type != PHY_TYPE_LCN) {
				tmppo.phy_type = PHY_TYPE_NULL;
				break;
			}

			switch (tmppo.band) {
			case WL_CHAN_FREQ_RANGE_2G:
				tmppo.cckpo = pi->cck2gpo;
				tmppo.ofdmpo = pi->ofdm2gpo;
				bcopy(&pi->mcs2gpo, &tmppo.mcspo, 8*sizeof(uint16));

				break;

			default:
				PHY_ERROR(("bandrange %d is out of scope\n", tmppo.band));
				err = BCME_BADARG;
				break;
			}

			if (!err)
				bcopy(&tmppo, a, sizeof(wl_po_t));
		} else {
			PHY_ERROR(("Unsupported PHY type!\n"));
			err = BCME_UNSUPPORTED;
		}
	}
	break;

	case IOV_SVAL(IOV_POVARS): {
		wl_po_t inpo;

		bcopy(p, &inpo, sizeof(wl_po_t));

		if (ISNPHY(pi)) {
			if (inpo.phy_type != PHY_TYPE_N)
				break;

			switch (inpo.band) {
			case WL_CHAN_FREQ_RANGE_2G:
				pi->cck2gpo = inpo.cckpo;
				pi->ofdm2gpo = inpo.ofdmpo;
				bcopy(inpo.mcspo, pi->mcs2gpo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				pi->ofdm5glpo = inpo.ofdmpo;
				bcopy(inpo.mcspo, pi->mcs5glpo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				pi->ofdm5gpo = inpo.ofdmpo;
				bcopy(inpo.mcspo, pi->mcs5gpo, 8*sizeof(uint16));
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
				pi->ofdm5ghpo = inpo.ofdmpo;
				bcopy(inpo.mcspo, pi->mcs5ghpo, 8*sizeof(uint16));
				break;

			default:
				PHY_ERROR(("bandrange %d is out of scope\n", inpo.band));
				err = BCME_BADARG;
				break;
			}

			if (!err)
				wlc_phy_txpwr_apply_nphy(pi);
		} else {
			PHY_ERROR(("Unsupported PHY type!\n"));
			err = BCME_UNSUPPORTED;
		}
	}
	break;

	case IOV_GVAL(IOV_PHY_TXPWRINDEX):
		wlc_phy_iovar_txpwrindex_get(pi, int_val, bool_val, ret_int_ptr);
		break;

	case IOV_SVAL(IOV_PHY_TXPWRINDEX):
		err = wlc_phy_iovar_txpwrindex_set(pi, p);
		break;

	case IOV_GVAL(IOV_PHY_IDLETSSI):
		wlc_phy_iovar_idletssi(pi, ret_int_ptr, TRUE);
		break;

	case IOV_SVAL(IOV_PHY_IDLETSSI):
		wlc_phy_iovar_idletssi(pi, ret_int_ptr, bool_val);
		break;

	case IOV_GVAL(IOV_PHY_VBATSENSE):
		wlc_phy_iovar_vbatsense(pi, ret_int_ptr);
		break;

	case IOV_GVAL(IOV_PHY_TXPWRCTRL):
		wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_PHY_TXPWRCTRL):
		err = wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_PHY_FORCECAL):
		err = wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, FALSE);
		break;

	case IOV_SVAL(IOV_PHY_FORCECAL):
		err = wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, TRUE);
		break;

	case IOV_GVAL(IOV_PHY_TXRX_CHAIN):
		wlc_phy_iovar_txrx_chain(pi, int_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_PHY_TXRX_CHAIN):
		err = wlc_phy_iovar_txrx_chain(pi, int_val, ret_int_ptr, TRUE);
		break;

	case IOV_SVAL(IOV_PHY_BPHY_EVM):
		wlc_phy_iovar_bphy_testpattern(pi, NPHY_TESTPATTERN_BPHY_EVM, (bool) int_val);
		break;

	case IOV_GVAL(IOV_PHY_BPHY_RFCS):
		*ret_int_ptr = pi->phy_bphy_rfcs;
		break;

	case IOV_SVAL(IOV_PHY_BPHY_RFCS):
		wlc_phy_iovar_bphy_testpattern(pi, NPHY_TESTPATTERN_BPHY_RFCS, (bool) int_val);
		break;

	case IOV_GVAL(IOV_PHY_SCRAMINIT):
		*ret_int_ptr = pi->phy_scraminit;
		break;

	case IOV_SVAL(IOV_PHY_SCRAMINIT):
		wlc_phy_iovar_scraminit(pi, pi->phy_scraminit);
		break;

	case IOV_SVAL(IOV_PHY_RFSEQ):
		wlc_phy_iovar_force_rfseq(pi, (uint8)int_val);
		break;

	case IOV_GVAL(IOV_PHY_TX_TONE):
		*ret_int_ptr = pi->phy_tx_tone_freq;
		break;

	case IOV_SVAL(IOV_PHY_TX_TONE):
		wlc_phy_iovar_tx_tone(pi, (uint32)int_val);
		break;

	case IOV_GVAL(IOV_PHY_TEST_TSSI):
		*((uint*)a) = wlc_phy_iovar_test_tssi(pi, (uint8)int_val, 0);
		break;

	case IOV_GVAL(IOV_PHY_TEST_TSSI_OFFS):
		*((uint*)a) = wlc_phy_iovar_test_tssi(pi, (uint8)int_val, 12);
		break;

	case IOV_SVAL(IOV_PHY_5G_PWRGAIN):
		pi->phy_5g_pwrgain = bool_val;
		break;

	case IOV_GVAL(IOV_PHY_5G_PWRGAIN):
		*ret_int_ptr = (int32)pi->phy_5g_pwrgain;
		break;

	case IOV_SVAL(IOV_PHY_ENABLERXCORE):
		wlc_phy_iovar_rxcore_enable(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_PHY_ENABLERXCORE):
		wlc_phy_iovar_rxcore_enable(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_GVAL(IOV_PHYCAL_TEMPDELTA):
		*ret_int_ptr = (int32)pi->phycal_tempdelta;
		break;

	case IOV_SVAL(IOV_PHYCAL_TEMPDELTA):
		pi->phycal_tempdelta = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_PHY_ACTIVECAL):
		*ret_int_ptr = (int32)((pi->cal_info->cal_phase_id !=
			MPHASE_CAL_STATE_IDLE)? 1 : 0);
		break;
#endif 

#ifdef WLTEST
	case IOV_GVAL(IOV_PHY_FEM2G): {
		bcopy(&pi->srom_fem2g, a, sizeof(srom_fem_t));
		break;
	}

	case IOV_SVAL(IOV_PHY_FEM2G): {
		bcopy(p, &pi->srom_fem2g, sizeof(srom_fem_t));
		/* srom_fem2g.extpagain changed after attach time */
		wlc_phy_txpower_ipa_upd(pi);
		break;
	}

	case IOV_GVAL(IOV_PHY_FEM5G): {
		bcopy(&pi->srom_fem5g, a, sizeof(srom_fem_t));
		break;
	}

	case IOV_SVAL(IOV_PHY_FEM5G): {
		bcopy(p, &pi->srom_fem5g, sizeof(srom_fem_t));
		/* srom_fem5g.extpagain changed after attach time */
		wlc_phy_txpower_ipa_upd(pi);
		break;
	}

	case IOV_GVAL(IOV_PHY_MAXP): {
		if (ISNPHY(pi)) {
			uint8*	maxp = (uint8*)a;

			maxp[0] = pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_2g;
			maxp[1] = pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_2g;
			maxp[2] = pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gm;
			maxp[3] = pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gm;
			maxp[4] = pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gl;
			maxp[5] = pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gl;
			maxp[6] = pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gh;
			maxp[7] = pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gh;
		}
		break;
	}
	case IOV_SVAL(IOV_PHY_MAXP): {
		if (ISNPHY(pi)) {
			uint8*	maxp = (uint8*)p;

			pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_2g = maxp[0];
			pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_2g = maxp[1];
			pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gm = maxp[2];
			pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gm = maxp[3];
			pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gl = maxp[4];
			pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gl = maxp[5];
			pi->nphy_pwrctrl_info[PHY_CORE_0].max_pwr_5gh = maxp[6];
			pi->nphy_pwrctrl_info[PHY_CORE_1].max_pwr_5gh = maxp[7];
		}
		break;
	}
#endif /* WLTEST */

	case IOV_GVAL(IOV_PHY_RXIQ_EST):
	{
		if (ISSSLPNPHY(pi))
		{
			return BCME_UNSUPPORTED;        /* lpphy support only for now */
		}

		if (!pi->sh->up) {
			err = BCME_NOTUP;
			break;
		}

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		/* get IQ power measurements */
		*ret_int_ptr = wlc_phy_rx_iq_est(pi, pi->rxiq_samps, pi->rxiq_antsel, 0);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;
	}

	case IOV_SVAL(IOV_PHY_RXIQ_EST):
		if (ISSSLPNPHY(pi))
		{
			return BCME_UNSUPPORTED;        /* lpphy support only for now */
		}

		if (!ISLPPHY(pi)) {
			uint8 samples, antenna;
			samples = int_val >> 8;
			antenna = int_val & 0xff;
			if (samples < 10 || samples > 15) {
				err = BCME_RANGE;
				break;
			}

			if ((antenna != ANT_RX_DIV_FORCE_0) && (antenna != ANT_RX_DIV_FORCE_1) &&
			    (antenna != ANT_RX_DIV_DEF)) {
				err = BCME_RANGE;
				break;
			}
			pi->rxiq_samps = samples;
			pi->rxiq_antsel = antenna;
		}
		break;

	case IOV_GVAL(IOV_NUM_STREAM):
		if (ISNPHY(pi)) {
			int_val = 2;
		} else if (ISHTPHY(pi)) {
			int_val = 3;
		} else if (ISAPHY(pi) || ISGPHY(pi) || ISLPPHY(pi) || ISSSLPNPHY(pi) ||
			ISLCNPHY(pi)) {
			int_val = 1;
		} else {
			int_val = -1;
		}
		bcopy(&int_val, a, vsize);
		break;

	case IOV_GVAL(IOV_BAND_RANGE):
		int_val = wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec);
		bcopy(&int_val, a, vsize);
		break;

	case IOV_SVAL(IOV_MIN_TXPOWER):
		pi->min_txpower = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_MIN_TXPOWER):
		int_val = pi->min_txpower;
		bcopy(&int_val, a, sizeof(int_val));
		break;
#ifdef PHYMON
	case IOV_GVAL(IOV_PHYCAL_STATE): {
		if (alen < (int)sizeof(wl_phycal_state_t)) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (ISNPHY(pi))
			err = wlc_phycal_state_nphy(pi, a, alen);
		else
			err = BCME_UNSUPPORTED;

		break;
	}
#endif /* PHYMON */

#if defined(WLTEST) || defined(AP)
	case IOV_GVAL(IOV_PHY_PERICAL):
		wlc_phy_iovar_perical_config(pi, int_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_PHY_PERICAL):
		wlc_phy_iovar_perical_config(pi, int_val, ret_int_ptr, TRUE);
		break;
#endif 

	case IOV_GVAL(IOV_PHY_PAPD_DEBUG):
		if (ISSSLPNPHY(pi))
		{
			wlc_sslpnphy_iovar_papd_debug(pi, a);
		}
		break;

	case IOV_GVAL(IOV_NOISE_MEASURE):
#if LCNCONF
		if (ISLCNPHY(pi))
		  wlc_lcnphy_noise_measure_start(pi);
#endif
		int_val = 0;
		bcopy(&int_val, a, sizeof(int_val));
		break;
	case IOV_GVAL(IOV_NOISE_MEASURE_DISABLE):
#if LCNCONF
		if (ISLCNPHY(pi))
		  wlc_lcnphy_noise_measure_disable(pi, 0, (uint32*)&int_val);
		else
#endif
		   int_val = 0;
		bcopy(&int_val, a, sizeof(int_val));
		break;
	case IOV_SVAL(IOV_NOISE_MEASURE_DISABLE):
#if LCNCONF
		if (ISLCNPHY(pi))
		  wlc_lcnphy_noise_measure_disable(pi, int_val, NULL);
#endif
		break;


	default:
		if (BCME_UNSUPPORTED == wlc_phy_iovar_dispatch_old(pi, actionid, p, a, vsize,
			int_val, bool_val))
			err = BCME_UNSUPPORTED;
	}

	return err;
}

int
wlc_phy_iovar_dispatch_old(phy_info_t *pi, uint32 actionid, void *p, void *a, int vsize,
	int32 int_val, bool bool_val)
{
	int err = BCME_OK;
	int32 *ret_int_ptr;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ret_int_ptr = (int32 *)a;

	switch (actionid) {
#if NCONF
#if defined(BCMDBG)
	case IOV_SVAL(IOV_NPHY_INITGAIN):
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		wlc_phy_setinitgain_nphy(pi, (uint16) int_val);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;

	case IOV_SVAL(IOV_NPHY_HPVGA1GAIN):
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		wlc_phy_sethpf1gaintbl_nphy(pi, (int8) int_val);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;

	case IOV_SVAL(IOV_NPHY_TX_TEMP_TONE): {
		uint16 orig_BBConfig;
		uint16 m0m1;
		nphy_txgains_t target_gain;

		if ((uint32)int_val > 0) {
			pi->phy_tx_tone_freq = (uint32) int_val;
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);

			/* Save the bbmult values,since it gets overwritten by mimophy_tx_tone() */
			wlc_phy_table_read_nphy(pi, 15, 1, 87, 16, &m0m1);

			/* Disable the re-sampler (in case we are in spur avoidance mode) */
			orig_BBConfig = read_phy_reg(pi, NPHY_BBConfig);
			mod_phy_reg(pi, NPHY_BBConfig, NPHY_BBConfig_resample_clk160_MASK, 0);

			/* read current tx gain and use as target_gain */
			target_gain = wlc_phy_get_tx_gain_nphy(pi);

			PHY_ERROR(("Tx gain core 0: target gain: ipa = %d,"
			         " pad = %d, pga = %d, txgm = %d, txlpf = %d\n",
			         target_gain.ipa[0], target_gain.pad[0], target_gain.pga[0],
			         target_gain.txgm[0], target_gain.txlpf[0]));

			PHY_ERROR(("Tx gain core 1: target gain: ipa = %d,"
			         " pad = %d, pga = %d, txgm = %d, txlpf = %d\n",
			         target_gain.ipa[0], target_gain.pad[1], target_gain.pga[1],
			         target_gain.txgm[1], target_gain.txlpf[1]));

			/* play a tone for 10 secs and then stop it and return */
			wlc_phy_tx_tone_nphy(pi, (uint32)int_val, 250, 0, 0, FALSE);

			/* Now restore the original bbmult values */
			wlc_phy_table_write_nphy(pi, 15, 1, 87, 16, &m0m1);
			wlc_phy_table_write_nphy(pi, 15, 1, 95, 16, &m0m1);

			OSL_DELAY(10000000);
			wlc_phy_stopplayback_nphy(pi);

			/* Restore the state of the re-sampler
			   (in case we are in spur avoidance mode)
			*/
			write_phy_reg(pi, NPHY_BBConfig, orig_BBConfig);

			wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);
			wlc_phyreg_exit((wlc_phy_t *)pi);
			wlapi_enable_mac(pi->sh->physhim);
		}
		break;
	}
	case IOV_SVAL(IOV_NPHY_CAL_RESET):
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		wlc_phy_cal_reset_nphy(pi, (uint32) int_val);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;

	case IOV_GVAL(IOV_NPHY_EST_TONEPWR):
	case IOV_GVAL(IOV_PHY_EST_TONEPWR): {
		int32 dBm_power[2];
		uint16 orig_BBConfig;
		uint16 m0m1;

		if (ISNPHY(pi)) {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);

			/* Save the bbmult values, since it gets overwritten
			   by mimophy_tx_tone()
			*/
			wlc_phy_table_read_nphy(pi, 15, 1, 87, 16, &m0m1);

			/* Disable the re-sampler (in case we are in spur avoidance mode) */
			orig_BBConfig = read_phy_reg(pi, NPHY_BBConfig);
			mod_phy_reg(pi, NPHY_BBConfig, NPHY_BBConfig_resample_clk160_MASK, 0);
			pi->phy_tx_tone_freq = (uint32) 4000;

			/* play a tone for 10 secs */
			wlc_phy_tx_tone_nphy(pi, (uint32)4000, 250, 0, 0, FALSE);

			/* Now restore the original bbmult values */
			wlc_phy_table_write_nphy(pi, 15, 1, 87, 16, &m0m1);
			wlc_phy_table_write_nphy(pi, 15, 1, 95, 16, &m0m1);

			OSL_DELAY(10000000);
			wlc_phy_est_tonepwr_nphy(pi, dBm_power, 128);
			wlc_phy_stopplayback_nphy(pi);

			/* Restore the state of the re-sampler
			   (in case we are in spur avoidance mode)
			*/
			write_phy_reg(pi, NPHY_BBConfig, orig_BBConfig);

			wlc_phyreg_exit((wlc_phy_t *)pi);
			wlapi_enable_mac(pi->sh->physhim);

			int_val = dBm_power[0]/4;
			bcopy(&int_val, a, vsize);
			break;
		} else {
			err = BCME_UNSUPPORTED;
			break;
		}
	}

	case IOV_GVAL(IOV_NPHY_RFSEQ_TXGAIN): {
		uint16 rfseq_tx_gain[2];
		wlc_phy_table_read_nphy(pi, NPHY_TBL_ID_RFSEQ, 2, 0x110, 16, rfseq_tx_gain);
		int_val = (((uint32) rfseq_tx_gain[1] << 16) | ((uint32) rfseq_tx_gain[0]));
		bcopy(&int_val, a, vsize);
		break;
	}

	case IOV_SVAL(IOV_PHY_SPURAVOID):
		if ((int_val != SPURAVOID_DISABLE) && (int_val != SPURAVOID_AUTO) &&
		    (int_val != SPURAVOID_FORCEON) && (int_val != SPURAVOID_FORCEON2)) {
			err = BCME_RANGE;
			break;
		}

		pi->phy_spuravoid = (int8)int_val;
		break;

	case IOV_GVAL(IOV_PHY_SPURAVOID):
		int_val = pi->phy_spuravoid;
		bcopy(&int_val, a, vsize);
		break;
#endif /* defined(BCMDBG) */

#if defined(WLTEST)
	case IOV_GVAL(IOV_NPHY_CAL_SANITY):
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		*ret_int_ptr = (uint32)wlc_phy_cal_sanity_nphy(pi);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;

	case IOV_GVAL(IOV_NPHY_BPHY_EVM):
	case IOV_GVAL(IOV_PHY_BPHY_EVM):

		*ret_int_ptr = pi->phy_bphy_evm;
		break;

	case IOV_SVAL(IOV_NPHY_BPHY_EVM):
		wlc_phy_iovar_bphy_testpattern(pi, NPHY_TESTPATTERN_BPHY_EVM, (bool) int_val);
		break;

	case IOV_GVAL(IOV_NPHY_BPHY_RFCS):
		*ret_int_ptr = pi->phy_bphy_rfcs;
		break;

	case IOV_SVAL(IOV_NPHY_BPHY_RFCS):
		wlc_phy_iovar_bphy_testpattern(pi, NPHY_TESTPATTERN_BPHY_RFCS, (bool) int_val);
		break;

	case IOV_GVAL(IOV_NPHY_SCRAMINIT):
		*ret_int_ptr = pi->phy_scraminit;
		break;

	case IOV_SVAL(IOV_NPHY_SCRAMINIT):
		wlc_phy_iovar_scraminit(pi, pi->phy_scraminit);
		break;

	case IOV_SVAL(IOV_NPHY_RFSEQ):
		wlc_phy_iovar_force_rfseq(pi, (uint8)int_val);
		break;

	case IOV_GVAL(IOV_NPHY_TXIQLOCAL): {
		nphy_txgains_t target_gain;
		uint8 tx_pwr_ctrl_state;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);

		/* read current tx gain and use as target_gain */
		target_gain = wlc_phy_get_tx_gain_nphy(pi);
		tx_pwr_ctrl_state = pi->nphy_txpwrctrl;
		wlc_phy_txpwrctrl_enable_nphy(pi, PHY_TPC_HW_OFF);

		/* want outer (0,1) ants so T/R works properly for CB2 2x3 switch, */
		if (pi->antsel_type == ANTSEL_2x3)
			wlc_phy_antsel_init((wlc_phy_t *)pi, TRUE);

		wlc_phy_cal_txiqlo_nphy(pi, target_gain, TRUE, FALSE);

		wlc_phy_txpwrctrl_enable_nphy(pi, tx_pwr_ctrl_state);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		*ret_int_ptr = 0;
		break;
	}
	case IOV_SVAL(IOV_NPHY_RXIQCAL): {
		nphy_txgains_t target_gain;
		uint8 tx_pwr_ctrl_state;


		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);

		/* read current tx gain and use as target_gain */
		target_gain = wlc_phy_get_tx_gain_nphy(pi);
		tx_pwr_ctrl_state = pi->nphy_txpwrctrl;
		wlc_phy_txpwrctrl_enable_nphy(pi, PHY_TPC_HW_OFF);

		wlc_phy_cal_rxiq_nphy(pi, target_gain, 0, (bool)int_val);

		wlc_phy_txpwrctrl_enable_nphy(pi, tx_pwr_ctrl_state);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		int_val = 0;
		bcopy(&int_val, a, vsize);
		break;
	}
	case IOV_GVAL(IOV_NPHY_RXCALPARAMS):
		*ret_int_ptr = pi->nphy_rxcalparams;
		break;

	case IOV_SVAL(IOV_NPHY_RXCALPARAMS):
		pi->nphy_rxcalparams = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_NPHY_TXPWRCTRL):
		wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_NPHY_TXPWRCTRL):
		err = wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_NPHY_RSSISEL):
		*ret_int_ptr = pi->nphy_rssisel;
		break;

	case IOV_SVAL(IOV_NPHY_RSSISEL):
		pi->nphy_rssisel = (uint8)int_val;

		if (!pi->sh->up)
			break;

		if (pi->nphy_rssisel < 0) {
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_phy_rssisel_nphy(pi, RADIO_MIMO_CORESEL_OFF, 0);
			wlc_phyreg_exit((wlc_phy_t *)pi);
		} else {
			int32 rssi_buf[4];
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_phy_poll_rssi_nphy(pi, (uint8)int_val, rssi_buf, 1);
			wlc_phyreg_exit((wlc_phy_t *)pi);
		}
		break;

	case IOV_GVAL(IOV_NPHY_RSSICAL): {
		/* if down, return the value, if up, run the cal */
		if (!pi->sh->up) {
			int_val = pi->rssical_nphy;
			bcopy(&int_val, a, vsize);
			break;
		}

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		/* run rssi cal */
		wlc_phy_rssi_cal_nphy(pi);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		int_val = pi->rssical_nphy;
		bcopy(&int_val, a, vsize);
		break;
	}

	case IOV_SVAL(IOV_NPHY_RSSICAL): {
		pi->rssical_nphy = bool_val;
		break;
	}

	case IOV_SVAL(IOV_NPHY_DFS_LP_BUFFER): {
		pi->dfs_lp_buffer_nphy = bool_val;
		break;
	}

	case IOV_GVAL(IOV_NPHY_GPIOSEL):
	case IOV_GVAL(IOV_PHY_GPIOSEL):
		*ret_int_ptr = pi->phy_gpiosel;
		break;

	case IOV_SVAL(IOV_NPHY_GPIOSEL):
	case IOV_SVAL(IOV_PHY_GPIOSEL):
		pi->phy_gpiosel = (uint16) int_val;

		if (!pi->sh->up)
			break;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);
		if (ISNPHY(pi))
			wlc_phy_gpiosel_nphy(pi, (uint16)int_val);
		else if (ISHTPHY(pi))
			wlc_phy_gpiosel_htphy(pi, (uint16)int_val);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);
		break;

	case IOV_GVAL(IOV_NPHY_TX_TONE):
		*ret_int_ptr = pi->phy_tx_tone_freq;
		break;

	case IOV_SVAL(IOV_NPHY_TX_TONE):
		wlc_phy_iovar_tx_tone(pi, (uint32)int_val);
		break;

	case IOV_SVAL(IOV_NPHY_GAIN_BOOST):
		pi->nphy_gain_boost = bool_val;
		break;

	case IOV_GVAL(IOV_NPHY_GAIN_BOOST):
		*ret_int_ptr = (int32)pi->nphy_gain_boost;
		break;

	case IOV_SVAL(IOV_NPHY_ELNA_GAIN_CONFIG):
		pi->nphy_elna_gain_config = (int_val != 0) ? TRUE : FALSE;
		break;

	case IOV_GVAL(IOV_NPHY_ELNA_GAIN_CONFIG):
		*ret_int_ptr = (int32)pi->nphy_elna_gain_config;
		break;

	case IOV_GVAL(IOV_NPHY_TEST_TSSI):
		*((uint*)a) = wlc_phy_iovar_test_tssi(pi, (uint8)int_val, 0);
		break;

	case IOV_GVAL(IOV_NPHY_TEST_TSSI_OFFS):
		*((uint*)a) = wlc_phy_iovar_test_tssi(pi, (uint8)int_val, 12);
		break;

	case IOV_SVAL(IOV_NPHY_5G_PWRGAIN):
		pi->phy_5g_pwrgain = bool_val;
		break;

	case IOV_GVAL(IOV_NPHY_5G_PWRGAIN):
		*ret_int_ptr = (int32)pi->phy_5g_pwrgain;
		break;

	case IOV_GVAL(IOV_NPHY_PERICAL):
		wlc_phy_iovar_perical_config(pi, int_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_NPHY_PERICAL):
		wlc_phy_iovar_perical_config(pi, int_val, ret_int_ptr, TRUE);
		break;

	case IOV_SVAL(IOV_NPHY_FORCECAL):
		err = wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, TRUE);
		break;

	case IOV_GVAL(IOV_NPHY_ACI_SCAN):
		if (SCAN_INPROG_PHY(pi)) {
			PHY_ERROR(("Scan in Progress, can execute %s\n", __FUNCTION__));
			*ret_int_ptr = -1;
		} else {
			if (pi->cur_interference_mode == INTERFERE_NONE) {
				PHY_ERROR(("interference mode is off\n"));
				*ret_int_ptr = -1;
				break;
			}

			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			*ret_int_ptr = wlc_phy_aci_scan_nphy(pi);
			wlapi_enable_mac(pi->sh->physhim);
		}
		break;

	case IOV_SVAL(IOV_NPHY_ENABLERXCORE):
		wlc_phy_iovar_rxcore_enable(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_NPHY_ENABLERXCORE):
		wlc_phy_iovar_rxcore_enable(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_NPHY_PAPDCALTYPE):
		pi->nphy_papd_cal_type = (int8) int_val;
		break;

	case IOV_GVAL(IOV_NPHY_PAPDCAL):
		pi->nphy_force_papd_cal = TRUE;
		int_val = 0;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_SVAL(IOV_NPHY_SKIPPAPD):
		if ((int_val != 0) && (int_val != 1)) {
			err = BCME_RANGE;
			break;
		}

		pi->nphy_papd_skip = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_NPHY_PAPDCALINDEX):
		*ret_int_ptr = (pi->nphy_papd_cal_gain_index[0] << 8) |
			pi->nphy_papd_cal_gain_index[1];
		break;

	case IOV_SVAL(IOV_NPHY_CALTXGAIN): {
		uint8 tx_pwr_ctrl_state;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);

		pi->nphy_cal_orig_pwr_idx[0] =
			(uint8) ((read_phy_reg(pi, NPHY_Core0TxPwrCtrlStatus) >> 8) & 0x7f);
		pi->nphy_cal_orig_pwr_idx[1] =
			(uint8) ((read_phy_reg(pi, NPHY_Core1TxPwrCtrlStatus) >> 8) & 0x7f);

		tx_pwr_ctrl_state = pi->nphy_txpwrctrl;
		wlc_phy_txpwrctrl_enable_nphy(pi, PHY_TPC_HW_OFF);

		wlc_phy_cal_txgainctrl_nphy(pi, int_val, TRUE);

		wlc_phy_txpwrctrl_enable_nphy(pi, tx_pwr_ctrl_state);
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);

		break;
	}

	case IOV_GVAL(IOV_NPHY_TEMPTHRESH):
	case IOV_GVAL(IOV_PHY_TEMPTHRESH):
		*ret_int_ptr = (int32) pi->txcore_temp.disable_temp;
		break;

	case IOV_SVAL(IOV_NPHY_TEMPTHRESH):
	case IOV_SVAL(IOV_PHY_TEMPTHRESH):
		pi->txcore_temp.disable_temp = (uint8) int_val;
		pi->txcore_temp.enable_temp =
		    pi->txcore_temp.disable_temp - pi->txcore_temp.hysteresis;
		break;

	case IOV_GVAL(IOV_NPHY_TEMPOFFSET):
	case IOV_GVAL(IOV_PHY_TEMPOFFSET):
		*ret_int_ptr = (int32) pi->phy_tempsense_offset;
		break;

	case IOV_SVAL(IOV_NPHY_TEMPOFFSET):
	case IOV_SVAL(IOV_PHY_TEMPOFFSET):
		pi->phy_tempsense_offset = (int8) int_val;
		break;

	case IOV_GVAL(IOV_NPHY_VCOCAL):
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phy_radio205x_vcocal_nphy(pi);
		wlapi_enable_mac(pi->sh->physhim);
		*ret_int_ptr = 0;
		break;

	case IOV_GVAL(IOV_NPHY_TBLDUMP_MINIDX):
		*ret_int_ptr = (int32)pi->nphy_tbldump_minidx;
		break;

	case IOV_SVAL(IOV_NPHY_TBLDUMP_MINIDX):
		pi->nphy_tbldump_minidx = (int8) int_val;
		break;

	case IOV_GVAL(IOV_NPHY_TBLDUMP_MAXIDX):
		*ret_int_ptr = (int32)pi->nphy_tbldump_maxidx;
		break;

	case IOV_SVAL(IOV_NPHY_TBLDUMP_MAXIDX):
		pi->nphy_tbldump_maxidx = (int8) int_val;
		break;

	case IOV_SVAL(IOV_NPHY_PHYREG_SKIPDUMP):
		if (pi->nphy_phyreg_skipcnt < 127) {
			pi->nphy_phyreg_skipaddr[pi->nphy_phyreg_skipcnt++] = (uint) int_val;
		}
		break;

	case IOV_GVAL(IOV_NPHY_PHYREG_SKIPDUMP):
		*ret_int_ptr = (pi->nphy_phyreg_skipcnt > 0) ?
			(int32) pi->nphy_phyreg_skipaddr[pi->nphy_phyreg_skipcnt-1] : 0;
		break;

	case IOV_SVAL(IOV_NPHY_PHYREG_SKIPCNT):
		pi->nphy_phyreg_skipcnt = (int8) int_val;
		break;

	case IOV_GVAL(IOV_NPHY_PHYREG_SKIPCNT):
		*ret_int_ptr = (int32)pi->nphy_phyreg_skipcnt;
		break;
#endif 
#endif /* NCONF */

#if defined(WLTEST)

#if LPCONF
	case IOV_SVAL(IOV_LPPHY_TX_TONE):
		pi->phy_tx_tone_freq = int_val;
		if (pi->phy_tx_tone_freq == 0) {
			wlc_phy_stop_tx_tone_lpphy(pi);
			wlc_phy_clear_deaf_lpphy(pi, (bool)1);
			wlc_phyreg_exit((wlc_phy_t *)pi);
			wlapi_enable_mac(pi->sh->physhim);
			pi->phywatchdog_override = TRUE;
		} else {
			pi->phywatchdog_override = FALSE;
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_phy_set_deaf_lpphy(pi, (bool)1);
			wlc_phy_start_tx_tone_lpphy(pi, int_val,
				LPREV_GE(pi->pubpi.phy_rev, 2) ? 28: 100); /* play tone */
		}
		break;

	case IOV_SVAL(IOV_LPPHY_PAPDCALTYPE):
		pi_lp->lpphy_papd_cal_type = (int16) int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_PAPDCAL):
		pi_lp->lpphy_force_papd_cal = TRUE;
		int_val = 0;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_GVAL(IOV_LPPHY_TXIQLOCAL):
	case IOV_GVAL(IOV_LPPHY_RXIQCAL):
		pi->phy_forcecal = TRUE;

		*ret_int_ptr = 0;
		break;

	case IOV_GVAL(IOV_LPPHY_FULLCAL):	/* conver to SVAL with parameters like NPHY ? */
		wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, FALSE);
		break;

	case IOV_GVAL(IOV_LPPHY_TXPWRCTRL):
		wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_LPPHY_TXPWRCTRL):
		err = wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_LPPHY_PAPD_SLOW_CAL):
		*ret_int_ptr = (uint32)pi_lp->lpphy_papd_slow_cal;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_SLOW_CAL): {
		pi_lp->lpphy_papd_slow_cal = bool_val;
		break;
	}

	case IOV_GVAL(IOV_LPPHY_PAPD_RECAL_MIN_INTERVAL):
		*ret_int_ptr = (int32)pi_lp->lpphy_papd_recal_min_interval;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_RECAL_MIN_INTERVAL):
		pi_lp->lpphy_papd_recal_min_interval = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_PAPD_RECAL_MAX_INTERVAL):
		*ret_int_ptr = (int32)pi_lp->lpphy_papd_recal_max_interval;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_RECAL_MAX_INTERVAL):
		pi_lp->lpphy_papd_recal_max_interval = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_PAPD_RECAL_GAIN_DELTA):
		*ret_int_ptr = (int32)pi_lp->lpphy_papd_recal_gain_delta;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_RECAL_GAIN_DELTA):
		pi_lp->lpphy_papd_recal_gain_delta = (uint32)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_PAPD_RECAL_ENABLE):
		*ret_int_ptr = (uint32)pi_lp->lpphy_papd_recal_enable;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_RECAL_ENABLE):
		pi_lp->lpphy_papd_recal_enable = bool_val;
		break;

	case IOV_GVAL(IOV_LPPHY_PAPD_RECAL_COUNTER):
		*ret_int_ptr = (int32)pi_lp->lpphy_papd_recal_counter;
		break;

	case IOV_SVAL(IOV_LPPHY_PAPD_RECAL_COUNTER):
		pi_lp->lpphy_papd_recal_counter = (uint32)int_val;
		break;

	case IOV_SVAL(IOV_LPPHY_CCK_DIG_FILT_TYPE):
		pi_lp->lpphy_cck_dig_filt_type = (int16)int_val;
		if (LPREV_GE(pi->pubpi.phy_rev, 2))
			wlc_phy_tx_dig_filt_cck_setup_lpphy(pi, TRUE);
		break;

	case IOV_GVAL(IOV_LPPHY_CCK_DIG_FILT_TYPE):
		int_val = (uint32)pi_lp->lpphy_cck_dig_filt_type;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_OFDM_DIG_FILT_TYPE):
		pi_lp->lpphy_ofdm_dig_filt_type = (int16)int_val;
		if (LPREV_GE(pi->pubpi.phy_rev, 2))
			wlc_phy_tx_dig_filt_ofdm_setup_lpphy(pi, TRUE);
		break;

	case IOV_GVAL(IOV_LPPHY_OFDM_DIG_FILT_TYPE):
		int_val = (uint32)pi_lp->lpphy_ofdm_dig_filt_type;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_TXRF_SP_9_OVR):
		pi_lp->lpphy_txrf_sp_9_override = int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_TXRF_SP_9_OVR):
		int_val = pi_lp->lpphy_txrf_sp_9_override;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_OFDM_ANALOG_FILT_BW_OVERRIDE):
		pi->ofdm_analog_filt_bw_override = (int16)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_OFDM_ANALOG_FILT_BW_OVERRIDE):
		int_val = pi->ofdm_analog_filt_bw_override;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_CCK_ANALOG_FILT_BW_OVERRIDE):
		pi->cck_analog_filt_bw_override = (int16)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_CCK_ANALOG_FILT_BW_OVERRIDE):
		int_val = pi->cck_analog_filt_bw_override;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_OFDM_RCCAL_OVERRIDE):
		pi->ofdm_rccal_override = (int16)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_OFDM_RCCAL_OVERRIDE):
		int_val = pi->ofdm_rccal_override;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_CCK_RCCAL_OVERRIDE):
		pi->cck_rccal_override = (int16)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_CCK_RCCAL_OVERRIDE):
		int_val = pi->cck_rccal_override;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_GVAL(IOV_LPPHY_TXPWRINDEX):	/* depreciated by PHY_TXPWRINDEX */
		wlc_phy_iovar_txpwrindex_get(pi, int_val, bool_val, ret_int_ptr);
		break;

	case IOV_SVAL(IOV_LPPHY_TXPWRINDEX):	/* depreciated by PHY_TXPWRINDEX */
		err = wlc_phy_iovar_txpwrindex_set(pi, p);
		break;

	case IOV_GVAL(IOV_LPPHY_CRS):
		*ret_int_ptr = (0 == (read_phy_reg(pi, LPPHY_crsgainCtrl) &
		LPPHY_crsgainCtrl_crseddisable_MASK));
		break;

	case IOV_SVAL(IOV_LPPHY_CRS):
		if (int_val)
			wlc_phy_clear_deaf_lpphy(pi, (bool)1);
		else
			wlc_phy_set_deaf_lpphy(pi, (bool)1);
		break;

	case IOV_SVAL(IOV_LPPHY_ACI_ON_THRESH):
	        pi_lp->lpphy_aci.on_thresh = (int)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_ON_THRESH):
		int_val = pi_lp->lpphy_aci.on_thresh;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_OFF_THRESH):
		pi_lp->lpphy_aci.off_thresh = (int)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_OFF_THRESH):
		int_val = pi_lp->lpphy_aci.off_thresh;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_ON_TIMEOUT):
		pi_lp->lpphy_aci.on_timeout = (int)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_ON_TIMEOUT):
		int_val = pi_lp->lpphy_aci.on_timeout;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_OFF_TIMEOUT):
		pi_lp->lpphy_aci.off_timeout = (int)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_OFF_TIMEOUT):
		int_val = pi_lp->lpphy_aci.off_timeout;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_GLITCH_TIMEOUT):
		pi_lp->lpphy_aci.glitch_timeout = (int)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_GLITCH_TIMEOUT):
		int_val = pi_lp->lpphy_aci.glitch_timeout;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_CHAN_SCAN_CNT):
		pi_lp->lpphy_aci.chan_scan_cnt = (int32)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_CHAN_SCAN_CNT):
		int_val = pi_lp->lpphy_aci.chan_scan_cnt;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_CHAN_SCAN_PWR_THRESH):
		pi_lp->lpphy_aci.chan_scan_pwr_thresh = (int32)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_CHAN_SCAN_PWR_THRESH):
		int_val = pi_lp->lpphy_aci.chan_scan_pwr_thresh;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_CHAN_SCAN_CNT_THRESH):
		pi_lp->lpphy_aci.chan_scan_cnt_thresh = (int32)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_CHAN_SCAN_CNT_THRESH):
		int_val = pi_lp->lpphy_aci.chan_scan_cnt_thresh;
		bcopy(&int_val, a, vsize);
		break;
	case IOV_SVAL(IOV_LPPHY_ACI_CHAN_SCAN_TIMEOUT):
		pi_lp->lpphy_aci.chan_scan_timeout = (int32)int_val;
		wlc_phy_aci_init_lpphy(pi, FALSE);
		break;
	case IOV_GVAL(IOV_LPPHY_ACI_CHAN_SCAN_TIMEOUT):
		int_val = pi_lp->lpphy_aci.chan_scan_timeout;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_SVAL(IOV_LPPHY_NOISE_SAMPLES):
		pi_lp->lpphy_noise_samples = (uint16)int_val;
		break;
	case IOV_GVAL(IOV_LPPHY_NOISE_SAMPLES):
		int_val = pi_lp->lpphy_noise_samples;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_GVAL(IOV_LPPHY_PAPDEPSTBL):
	{
		lpphytbl_info_t tab;
		uint32 papdepstbl[PHY_PAPD_EPS_TBL_SIZE_LPPHY];

		/* Preset PAPD eps table */
		tab.tbl_len = PHY_PAPD_EPS_TBL_SIZE_LPPHY;
		tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
		tab.tbl_offset = 0;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;
		tab.tbl_ptr = &papdepstbl[0];

		/* read the table */
		wlc_phy_table_read_lpphy(pi, &tab);
		bcopy(&papdepstbl[0], a, PHY_PAPD_EPS_TBL_SIZE_LPPHY*sizeof(uint32));
	}
	break;

	case IOV_GVAL(IOV_LPPHY_IDLE_TSSI_UPDATE_DELTA_TEMP):
		int_val = (int)pi_lp->lpphy_idle_tssi_update_delta_temp;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_SVAL(IOV_LPPHY_IDLE_TSSI_UPDATE_DELTA_TEMP):
		pi_lp->lpphy_idle_tssi_update_delta_temp = (int16)int_val;
		break;
#endif /* LPCONF */

#if SSLPNCONF
	case IOV_SVAL(IOV_SSLPNPHY_TX_TONE):
		pi->phy_tx_tone_freq = int_val;
		if (pi->phy_tx_tone_freq == 0) {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_sslpnphy_stop_tx_tone(pi);
			wlc_phyreg_exit((wlc_phy_t *)pi);

			wlapi_bmac_macphyclk_set(pi->sh->physhim, ON);

			wlapi_enable_mac(pi->sh->physhim);
			pi->phywatchdog_override = TRUE;
		} else {
			pi->phywatchdog_override = FALSE;
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phyreg_enter((wlc_phy_t *)pi);
			wlc_sslpnphy_start_tx_tone(pi, int_val, 112, 0); /* play tone */
			wlc_sslpnphy_set_tx_pwr_by_index(pi, (int)60);
			wlc_phyreg_exit((wlc_phy_t *)pi);

			wlapi_bmac_macphyclk_set(pi->sh->physhim, OFF);

			wlapi_enable_mac(pi->sh->physhim);
		}
		break;

	case IOV_GVAL(IOV_SSLPNPHY_PAPDCAL):
		pi->u.pi_sslpnphy->sslpnphy_full_cal_channel = 0;
		/* fall through */
	case IOV_GVAL(IOV_SSLPNPHY_TXIQLOCAL):
	case IOV_GVAL(IOV_SSLPNPHY_RXIQCAL):
		pi->phy_forcecal = TRUE;

		*ret_int_ptr = 0;
		break;

	case IOV_GVAL(IOV_SSLPNPHY_TXPWRCTRL):
		wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, FALSE);
		break;

	case IOV_SVAL(IOV_SSLPNPHY_TXPWRCTRL):
		err = wlc_phy_iovar_txpwrctrl(pi, int_val, bool_val, ret_int_ptr, TRUE);
		break;

	case IOV_GVAL(IOV_SSLPNPHY_TXPWRINDEX):
		wlc_phy_iovar_txpwrindex_get(pi, int_val, bool_val, ret_int_ptr);
		break;

	case IOV_SVAL(IOV_SSLPNPHY_TXPWRINDEX):
		err = wlc_phy_iovar_txpwrindex_set(pi, p);
		break;

	case IOV_GVAL(IOV_SSLPNPHY_CRS):
		*ret_int_ptr = (0 == (read_phy_reg(pi, SSLPNPHY_crsgainCtrl) &
		SSLPNPHY_crsgainCtrl_crseddisable_MASK));
		break;

	case IOV_SVAL(IOV_SSLPNPHY_CRS):
		if (int_val)
			wlc_sslpnphy_deaf_mode(pi, FALSE);
		else
			wlc_sslpnphy_deaf_mode(pi, TRUE);
		break;

	case IOV_GVAL(IOV_SSLPNPHY_CARRIER_SUPPRESS):
		*ret_int_ptr = (pi->carrier_suppr_disable == 0);
		break;

	case IOV_GVAL(IOV_SSLPNPHY_UNMOD_RSSI):
	{
		int32 input_power_db = 0;

		if (!pi->sh->up) {
			err = BCME_NOTUP;
			break;
		}
#if SSLPNCONF
		input_power_db = wlc_sslpnphy_rx_signal_power(pi, -1);
#endif
#if defined(WLNOKIA_NVMEM)
		input_power_db = wlc_phy_upd_rssi_offset(pi,
			(int8)input_power_db, pi->radio_chanspec);
#endif 
		*ret_int_ptr = input_power_db;
		break;
	}

	case IOV_SVAL(IOV_SSLPNPHY_CARRIER_SUPPRESS):
	{
		pi->carrier_suppr_disable = bool_val;
		if (pi->carrier_suppr_disable) {
			wlc_phy_carrier_suppress_sslpnphy(pi);
		}
		break;
	}

	case IOV_SVAL(IOV_SSLPNPHY_NOISE_SAMPLES):
		pi->u.pi_sslpnphy->sslpnphy_noise_samples = (uint16)int_val;
		break;
	case IOV_GVAL(IOV_SSLPNPHY_NOISE_SAMPLES):
		int_val = pi->u.pi_sslpnphy->sslpnphy_noise_samples;
		bcopy(&int_val, a, vsize);
		break;

	case IOV_GVAL(IOV_SSLPNPHY_PAPARAMS):
		{
			int32 paparams[3];
			uint freq;
			freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
			switch (wlc_phy_chanspec_freq2bandrange_lpssn(freq)) {
			case WL_CHAN_FREQ_RANGE_2G:
				/* 2.4 GHz */
				paparams[0] = (int32)pi->txpa_2g[0];		/* b0 */
				paparams[1] = (int32)pi->txpa_2g[1];		/* b1 */
				paparams[2] = (int32)pi->txpa_2g[2];		/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				/* 5 GHz low */
				paparams[0] = (int32)pi->txpa_5g_low[0];	/* b0 */
				paparams[1] = (int32)pi->txpa_5g_low[1];	/* b1 */
				paparams[2] = (int32)pi->txpa_5g_low[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				/* 5 GHz middle */
				paparams[0] = (int32)pi->txpa_5g_mid[0];	/* b0 */
				paparams[1] = (int32)pi->txpa_5g_mid[1];	/* b1 */
				paparams[2] = (int32)pi->txpa_5g_mid[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
			default:
				/* 5 GHz high */
				paparams[0] = (int32)pi->txpa_5g_hi[0];		/* b0 */
				paparams[1] = (int32)pi->txpa_5g_hi[1];		/* b1 */
				paparams[2] = (int32)pi->txpa_5g_hi[2];		/* a1 */
				break;
			}
			bcopy(&paparams, a, 3*sizeof(int32));
		}
		break;

	case IOV_SVAL(IOV_SSLPNPHY_PAPARAMS):
		{
			int32 paparams[3];
			int32 a1, b0, b1;
			int32 tssi, pwr;
			phytbl_info_t tab;
			uint freq;

			bcopy(p, paparams, 3*sizeof(int32));
			b0 = paparams[0];
			b1 = paparams[1];
			a1 = paparams[2];

			tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
			tab.tbl_width = 32;	/* 32 bit wide	*/
			/* Convert tssi to power LUT */
			tab.tbl_ptr = &pwr; /* ptr to buf */
			tab.tbl_len = 1;        /* # values   */
			tab.tbl_offset = 0; /* estPwrLuts */
			for (tssi = 0; tssi < 64; tssi++) {
				pwr = wlc_sslpnphy_tssi2dbm(tssi, a1, b0, b1);
				wlc_sslpnphy_write_table(pi,  &tab);
				tab.tbl_offset++;
			}

			freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
			switch (wlc_phy_chanspec_freq2bandrange_lpssn(freq)) {
			case WL_CHAN_FREQ_RANGE_2G:
				/* 2.4 GHz */
				pi->txpa_2g[0] = (int16)paparams[0];		/* b0 */
				pi->txpa_2g[1] = (int16)paparams[1];		/* b1 */
				pi->txpa_2g[2] = (int16)paparams[2];		/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GL:
				/* 5 GHz low */
				pi->txpa_5g_low[0] = (int16)paparams[0];	/* b0 */
				pi->txpa_5g_low[1] = (int16)paparams[1];	/* b1 */
				pi->txpa_5g_low[2] = (int16)paparams[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GM:
				/* 5 GHz middle */
				pi->txpa_5g_mid[0] = (int16)paparams[0];	/* b0 */
				pi->txpa_5g_mid[1] = (int16)paparams[1];	/* b1 */
				pi->txpa_5g_mid[2] = (int16)paparams[2];	/* a1 */
				break;

			case WL_CHAN_FREQ_RANGE_5GH:
			default:
				/* 5 GHz high */
				pi->txpa_5g_hi[0] = (int16)paparams[0];		/* b0 */
				pi->txpa_5g_hi[1] = (int16)paparams[1];		/* b1 */
				pi->txpa_5g_hi[2] = (int16)paparams[2];		/* a1 */
				break;
			}
		}
		break;

	case IOV_GVAL(IOV_SSLPNPHY_FULLCAL):	/* conver to SVAL with parameters like NPHY ? */
		wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, FALSE);
		break;

	case IOV_SVAL(IOV_SSLPNPHY_FULLCAL):
		err = wlc_phy_iovar_forcecal(pi, int_val, ret_int_ptr, vsize, TRUE);
		break;
#endif /* SSLPNCONF */
#if LCNCONF
	case IOV_GVAL(IOV_LCNPHY_PAPDEPSTBL):
	{
		lpphytbl_info_t tab;
		uint32 papdepstbl[PHY_PAPD_EPS_TBL_SIZE_LCNPHY];

		/* Preset PAPD eps table */
		tab.tbl_len = PHY_PAPD_EPS_TBL_SIZE_LCNPHY;
		tab.tbl_id = LCNPHY_TBL_ID_PAPDCOMPDELTATBL;
		tab.tbl_offset = 0;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;
		tab.tbl_ptr = &papdepstbl[0];

		/* read the table */
		wlc_phy_table_read_lpphy(pi, &tab);
		bcopy(&papdepstbl[0], a, PHY_PAPD_EPS_TBL_SIZE_LCNPHY*sizeof(uint32));
	}
	break;

#endif /* LCNCONF */
#endif 

#if LPCONF
	case IOV_GVAL(IOV_LPPHY_TEMPSENSE):
		int_val = wlc_phy_tempsense_lpphy(pi);
		bcopy(&int_val, a, sizeof(int_val));
		break;
#endif
	case IOV_GVAL(IOV_LPPHY_CAL_DELTA_TEMP):
		int_val = pi_lp->lpphy_cal_delta_temp;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_CAL_DELTA_TEMP):
		pi_lp->lpphy_cal_delta_temp = (int8)int_val;
		break;
#if LPCONF
	case IOV_GVAL(IOV_LPPHY_VBATSENSE):
		int_val = wlc_phy_vbatsense_lpphy(pi);
		bcopy(&int_val, a, sizeof(int_val));
		break;
#endif

#if LPCONF
	case IOV_SVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_TEMPSENSE):
		pi_lp->lpphy_rx_gain_temp_adj_tempsense = (int8)int_val;
		break;

	case IOV_GVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_TEMPSENSE):
		int_val = (int32)pi_lp->lpphy_rx_gain_temp_adj_tempsense;
		bcopy(&int_val, a, sizeof(int_val));
		break;

	case IOV_SVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_THRESH):
	  {
	    uint32 thresh = (uint32)int_val;
	    pi_lp->lpphy_rx_gain_temp_adj_thresh[0] = (thresh & 0xff);
	    pi_lp->lpphy_rx_gain_temp_adj_thresh[1] = ((thresh >> 8) & 0xff);
	    pi_lp->lpphy_rx_gain_temp_adj_thresh[2] = ((thresh >> 16) & 0xff);
	    wlc_phy_rx_gain_temp_adj_lpphy(pi);
	  }
	  break;

	case IOV_GVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_THRESH):
	  {
	    uint32 thresh;
	    thresh = (uint32)pi_lp->lpphy_rx_gain_temp_adj_thresh[0];
	    thresh |= ((uint32)pi_lp->lpphy_rx_gain_temp_adj_thresh[1])<<8;
	    thresh |= ((uint32)pi_lp->lpphy_rx_gain_temp_adj_thresh[2])<<16;
	    bcopy(&thresh, a, sizeof(thresh));
	  }
	  break;

	case IOV_SVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_METRIC):
		pi_lp->lpphy_rx_gain_temp_adj_metric = (int8)(int_val & 0xff);
		pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric = (int8)((int_val >> 8) & 1);
		wlc_phy_rx_gain_temp_adj_lpphy(pi);
		break;

	case IOV_GVAL(IOV_LPPHY_RX_GAIN_TEMP_ADJ_METRIC):
		int_val = (int32)pi_lp->lpphy_rx_gain_temp_adj_metric;
		int_val |= (int32)((pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric << 8) & 0x100);
		bcopy(&int_val, a, sizeof(int_val));
		break;
#endif /* LPCONF */
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* %%%%%% IOCTL */
int
wlc_phy_ioctl(wlc_phy_t *pih, int cmd, int len, void *arg, bool *ta_ok)
{
	phy_info_t *pi = (phy_info_t *)pih;
	int bcmerror = 0;
	int val, *pval;
	bool bool_val;

	/* default argument is generic integer */
	pval = arg ? (int*)arg : NULL;

	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));
	else
		val = 0;

	/* bool conversion to avoid duplication below */
	bool_val = val != 0;

	switch (cmd) {

	case WLC_GET_PHY_NOISE:
		*pval = wlc_phy_noise_avg(pih);
		break;

	case WLC_RESTART:
		/* Reset calibration results to uninitialized state in order to
		 * trigger recalibration next time wlc_init() is called.
		 */
		if (pi->sh->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}
		wlc_set_phy_uninitted(pi);
		break;

#if defined(BCMDBG)
	case WLC_GET_RADIOREG:
		*ta_ok = TRUE;

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlc_phyreg_enter(pih);
		wlc_radioreg_enter(pih);
		if ((val == RADIO_IDCODE) && (!ISHTPHY(pi)))
			*pval = read_radio_id(pi);
		else
			*pval = read_radio_reg(pi, (uint16)val);
		wlc_radioreg_exit(pih);
		wlc_phyreg_exit(pih);
		break;

	case WLC_SET_RADIOREG:
		*ta_ok = TRUE;

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlc_phyreg_enter(pih);
		wlc_radioreg_enter(pih);
		write_radio_reg(pi, (uint16)val, (uint16)(val >> NBITS(uint16)));
		wlc_radioreg_exit(pih);
		wlc_phyreg_exit(pih);
		break;
#endif 

#if defined(BCMDBG)
	case WLC_GET_TX_PATH_PWR:

		*pval = (read_radio_reg(pi, RADIO_2050_PU_OVR) & 0x84) ? 1 : 0;
		break;

	case WLC_SET_TX_PATH_PWR:

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlc_phyreg_enter(pih);
		wlc_radioreg_enter(pih);
		if (bool_val) {
			/* Enable overrides */
			write_radio_reg(pi, RADIO_2050_PU_OVR,
				0x84 | (read_radio_reg(pi, RADIO_2050_PU_OVR) &
				0xf7));
		} else {
			/* Disable overrides */
			write_radio_reg(pi, RADIO_2050_PU_OVR,
				read_radio_reg(pi, RADIO_2050_PU_OVR) & ~0x84);
		}
		wlc_radioreg_exit(pih);
		wlc_phyreg_exit(pih);
		break;
#endif /* BCMDBG */

#if defined(BCMDBG) || defined(WLTEST)
	case WLC_GET_PHYREG:
		*ta_ok = TRUE;

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlc_phyreg_enter(pih);

		if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
			wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  MCTL_PHYLOCK);
			(void)R_REG(pi->sh->osh, &pi->regs->maccontrol);
			OSL_DELAY(1);
		}

		*pval = read_phy_reg(pi, (uint16)val);

		if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))
			wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  0);

		wlc_phyreg_exit(pih);
		break;

	case WLC_SET_PHYREG:
		*ta_ok = TRUE;

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlc_phyreg_enter(pih);
		write_phy_reg(pi, (uint16)val, (uint16)(val >> NBITS(uint16)));
		wlc_phyreg_exit(pih);
		break;
#endif 

#if defined(BCMDBG) || defined(WLTEST)
	case WLC_GET_TSSI: {

		if (!pi->sh->clk) {
			bcmerror = BCME_NOCLK;
			break;
		}

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter(pih);
		*pval = 0;
		switch (pi->pubpi.phy_type) {
		case PHY_TYPE_LP:
			CASECHECK(PHYTYPE, PHY_TYPE_LP);
			{
			int8 ofdm_pwr = 0, cck_pwr = 0;

			wlc_phy_get_tssi_lpphy(pi, &ofdm_pwr, &cck_pwr);
			*pval =  ((uint16)ofdm_pwr << 8) | (uint16)cck_pwr;
			break;

			}
		case PHY_TYPE_SSN:
			CASECHECK(PHYTYPE, PHY_TYPE_SSN);
			{
				int8 ofdm_pwr = 0, cck_pwr = 0;

				wlc_sslpnphy_get_tssi(pi, &ofdm_pwr, &cck_pwr);
				*pval =  ((uint16)ofdm_pwr << 8) | (uint16)cck_pwr;
				break;
			}
		case PHY_TYPE_LCN:
			PHY_TRACE(("%s:***CHECK***\n", __FUNCTION__));
			CASECHECK(PHYTYPE, PHY_TYPE_LCN);
			{
				int8 ofdm_pwr = 0, cck_pwr = 0;

				wlc_lcnphy_get_tssi(pi, &ofdm_pwr, &cck_pwr);
				*pval =  ((uint16)ofdm_pwr << 8) | (uint16)cck_pwr;
				break;
			}
		case PHY_TYPE_N:
			CASECHECK(PHYTYPE, PHY_TYPE_N);
			{
			*pval = (read_phy_reg(pi, NPHY_TSSIBiasVal1) & 0xff) << 8;
			*pval |= (read_phy_reg(pi, NPHY_TSSIBiasVal2) & 0xff);
			break;
			}
		case PHY_TYPE_A:
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			*pval = (read_phy_reg(pi, APHY_TSSI_STAT) & 0xff) << 8;
			break;
		case PHY_TYPE_G:
			CASECHECK(PHYTYPE, PHY_TYPE_G);
			*pval = (read_phy_reg(pi,
				(GPHY_TO_APHY_OFF + APHY_TSSI_STAT)) & 0xff) << 8;
			*pval |= (read_phy_reg(pi, BPHY_TSSI) & 0xff);
			break;
		}

		wlc_phyreg_exit(pih);
		wlapi_enable_mac(pi->sh->physhim);
		break;
	}

	case WLC_GET_ATTEN: {
		atten_t *atten = (atten_t *)pval;

		if (!ISGPHY(pi)) {
			bcmerror = BCME_BADBAND;
			break;
		}
		wlc_get_11b_txpower(pi, atten);
		break;
	}

	case WLC_SET_ATTEN: {
		atten_t *atten = (atten_t *)pval;

		if (!pi->sh->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		if ((atten->auto_ctrl != WL_ATTEN_APP_INPUT_PCL_OFF) &&
			(atten->auto_ctrl != WL_ATTEN_PCL_ON) &&
			(atten->auto_ctrl != WL_ATTEN_PCL_OFF)) {
			bcmerror = BCME_BADARG;
			break;
		}

		if (!ISGPHY(pi)) {
			bcmerror = BCME_BADBAND;
			break;
		}

		wlc_phyreg_enter(pih);
		wlc_radioreg_enter(pih);
		wlc_phy_set_11b_txpower(pi, atten);
		wlc_radioreg_exit(pih);
		wlc_phyreg_exit(pih);
		break;
	}

	case WLC_GET_PWRIDX:
		if (!ISAPHY(pi)) {
			bcmerror = BCME_BADBAND;
			break;
		}

		*pval = pi->txpwridx;
		break;

	case WLC_SET_PWRIDX:	/* set A band radio/baseband power index */
		if (!pi->sh->up) {
			bcmerror = BCME_NOTUP;
			break;
		}

		if (!ISAPHY(pi)) {
			bcmerror = BCME_BADBAND;
			break;
		}

		if ((val < WL_PWRIDX_LOWER_LIMIT) || (val > WL_PWRIDX_UPPER_LIMIT)) {
			bcmerror = BCME_RANGE;
			break;
		}

		{
		bool override;
		override = (val == WL_PWRIDX_PCL_ON) ? FALSE : TRUE;
		wlc_set_11a_txpower(pi, (int8)val, override);
		}
		break;

	case WLC_LONGTRAIN:
		{
		longtrnfn_t long_train_fn = NULL;

		if (pi->sh->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}

		long_train_fn = pi->pi_fptr.longtrn;
		if (long_train_fn)
			bcmerror = (*long_train_fn)(pi, val);
		else
			PHY_ERROR(("WLC_LONGTRAIN: unsupported phy type"));

			break;
		}

	case WLC_EVM:
		if (pi->sh->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}

		bcmerror = wlc_phy_test_evm(pi, val, *(((uint *)arg) + 1), *(((int *)arg) + 2));
		break;

	case WLC_FREQ_ACCURACY:
#if !SSLPNCONF
		/* SSLPNCONF transmits a few frames before running PAPD Calibration
		 * it does papd calibration each time it enters a new channel
		 * We cannot be down for this reason
		 */
		if (pi->sh->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}
#endif

		bcmerror = wlc_phy_test_freq_accuracy(pi, val);
		break;

	case WLC_CARRIER_SUPPRESS:
#if !SSLPNCONF
		if (pi->sh->up) {
			bcmerror = BCME_NOTDOWN;
			break;
		}
#endif
		bcmerror = wlc_phy_test_carrier_suppress(pi, val);
		break;
#endif 


	case WLC_GET_INTERFERENCE_MODE:
		*pval = pi->sh->interference_mode;
		if (pi->aci_state & ACI_ACTIVE)
			*pval |= AUTO_ACTIVE;
		break;

	case WLC_SET_INTERFERENCE_MODE:
		if (val < INTERFERE_NONE || val > WLAN_AUTO_W_NOISE) {
			bcmerror = BCME_RANGE;
			break;
		}

		if (pi->sh->interference_mode == val)
			break;

		/* push to sw state */
		pi->sh->interference_mode = val;

		if (!pi->sh->up)
			break;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);

		/* turn interference mode to off before entering another mode */
		if (val != INTERFERE_NONE)
			wlc_phy_interference(pi, INTERFERE_NONE, TRUE);

		if (!wlc_phy_interference(pi, pi->sh->interference_mode, TRUE))
			bcmerror = BCME_BADOPTION;

		wlapi_enable_mac(pi->sh->physhim);
		break;

	case WLC_GET_INTERFERENCE_OVERRIDE_MODE:
		if (!ISNPHY(pi)) {
			break;
		}

		if (!NREV_GE(pi->pubpi.phy_rev, 3)) {
			break;
		}

		if (pi->sh->interference_mode_override == FALSE) {
			*pval = INTERFERE_OVRRIDE_OFF;
		} else {
			*pval = pi->sh->interference_mode;
		}
		break;

	case WLC_SET_INTERFERENCE_OVERRIDE_MODE:
		if (!ISNPHY(pi)) {
			break;
		}

		if (!NREV_GE(pi->pubpi.phy_rev, 3)) {
			break;
		}

		if (val < INTERFERE_OVRRIDE_OFF || val > WLAN_AUTO_W_NOISE) {
			bcmerror = BCME_RANGE;
			break;
		}

		if (val == INTERFERE_OVRRIDE_OFF) {
			/* this is a reset */
			pi->sh->interference_mode_override = FALSE;
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				pi->sh->interference_mode =
					pi->sh->interference_mode_2G;
			} else {
				pi->sh->interference_mode =
					pi->sh->interference_mode_5G;
			}
		} else {

			pi->sh->interference_mode_override = TRUE;

			if (pi->sh->interference_mode == val)
				break;

			/* push to sw state */
			if (NREV_GE(pi->pubpi.phy_rev, 3)) {
				pi->sh->interference_mode_2G_override = val;
				if (val != 0) {
					pi->sh->interference_mode_5G_override = NON_WLAN;
				} else {
					pi->sh->interference_mode_5G_override = 0;
				}

				if (CHSPEC_IS2G(pi->radio_chanspec)) {
					pi->sh->interference_mode =
						pi->sh->interference_mode_2G_override;
				} else {
					pi->sh->interference_mode =
						pi->sh->interference_mode_5G_override;
				}
			} else {
				pi->sh->interference_mode = val;
			}
		}


		if (!pi->sh->up)
			break;

		wlapi_suspend_mac_and_wait(pi->sh->physhim);

		/* turn interference mode to off before entering another mode */
		if (val != INTERFERE_NONE)
			wlc_phy_interference(pi, INTERFERE_NONE, TRUE);

		if (!wlc_phy_interference(pi, pi->sh->interference_mode, TRUE))
			bcmerror = BCME_BADOPTION;

		wlapi_enable_mac(pi->sh->physhim);
		break;

	default:
		bcmerror = BCME_UNSUPPORTED;
	}

	return bcmerror;
}

/* WARNING: check (radioid != NORADIO_ID) before doing any radio related calibrations */
#ifdef AEI_VDSL_CUSTOMER_NCS
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)
#define WHOLE_N 1
#define FRACTION_N 20
#endif

void
wlc_phy_watchdog(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;
	bool delay_phy_cal = FALSE;	/* avoid more than one phy cal at same
					 * pi->sh->now instant
					 */
#ifdef AEI_VDSL_CUSTOMER_NCS
	int a = avenrun[0] + (FIXED_1/200);
#endif

	if (ISLCNPHY(pi))
	  wlc_lcnphy_noise_measure_stop(pi);

	pi->sh->now++;

	if (!pi->phywatchdog_override)
		return;

	/* defer interference checking, scan and update if RM is progress */
	if (!SCAN_RM_IN_PROGRESS(pi) &&
	    (ISNPHY(pi) || ISGPHY(pi) || ISLPPHY(pi))
#ifdef AEI_VDSL_CUSTOMER_NCS
	&&  LOAD_INT(a)<WHOLE_N && LOAD_FRAC(a)<FRACTION_N	
#endif
	) {
		/* interf.scanroamtimer counts transient time coming out of scan */
		if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
			if (pi->interf.scanroamtimer != 0)
				pi->interf.scanroamtimer -= 1;
		}
		wlc_phy_aci_upd(pi);
	} else {
		if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 3)) {
			/* in a scan or radio meas, don't update moving average
			 * when we first come out of scan or roam
			*/
			pi->interf.scanroamtimer = 2;
		}
	}

	if ((pi->sh->now % pi->sh->fast_timer) == 0) {
		wlc_phy_update_bt_chanspec(pi, pi->radio_chanspec);

		if (ISLPPHY(pi) && (BCM2062_ID == LPPHY_RADIO_ID(pi))) {
			wlc_phy_radio_2062_check_vco_cal(pi);
		}
	}

	/* update phy noise moving average only if no scan or rm in progress */
	if (!(SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi))) {
		wlc_phy_noise_sample_request((wlc_phy_t*)pi, PHY_NOISE_SAMPLE_MON,
			CHSPEC_CHANNEL(pi->radio_chanspec));
	}

	/* reset phynoise state if ucode interrupt doesn't arrive for so long */
	if (pi->phynoise_state && (pi->sh->now - pi->phynoise_now) > 5) {
		PHY_TMP(("wlc_phy_watchdog: ucode phy noise sampling overdue\n"));
		pi->phynoise_state = 0;
	}

	/* fast_timer driven event */
	if ((!pi->phycal_txpower) ||
	    ((pi->sh->now - pi->phycal_txpower) >= pi->sh->fast_timer)) {
		/* SW power control: Keep attempting txpowr recalc until successfully */
		if (!SCAN_INPROG_PHY(pi) && wlc_phy_cal_txpower_recalc_sw(pi)) {
			pi->phycal_txpower = pi->sh->now;
		}
	}

	/* abort if no radio */
	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* abort if cal should be blocked(e.g. not in home channel) */
	if ((SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi) || ASSOC_INPROG_PHY(pi)))
		return;

#ifdef WLNOKIA_NVMEM
	if (pi->sh->now % pi->sh->glacial_timer) {
		int8 vbat = wlc_phy_env_measure_vbat(pi);
		int8 temp = wlc_phy_env_measure_temperature(pi);
		if (wlc_phy_noknvmem_env_check(pi, vbat, temp))
			wlc_phy_txpower_recalc_target(pi);
	}

#endif /* WLNOKIA_NVMEM */

	if (ISGPHY(pi)) {
		/* glacial_timer driven event */
		if (!pi->disable_percal &&
		    (pi->sh->now - pi->phycal_mlo) >= pi->sh->glacial_timer) {
			if ((GREV_GT(pi->pubpi.phy_rev, 1))) {
				wlc_phy_cal_measurelo_gphy(pi);
				delay_phy_cal = TRUE;
			}
		}

		/* slow_timer driven event */
		if ((pi->sh->now % pi->sh->slow_timer) == 0) {
			/* NOTE: wlc_phy_cal_txpower_stats_clr_gphy is to be called *after*
			 * wlc_gphy_measurelo in the case where both are called in
			 * the same pi->sh->now instant, otherwise the periodic
			 * calibration will skip all attenuator settings.
			 */
			if (!pi->hwpwrctrl)
				wlc_phy_cal_txpower_stats_clr_gphy(pi);
		}

		if (((pi->sh->now - pi->phycal_noffset) >= pi->sh->slow_timer) &&
		    (pi->sh->boardflags & BFL_ADCDIV) && (pi->pubpi.radiorev == 8) &&
		    !delay_phy_cal) {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phy_cal_radio2050_nrssioffset_gmode1(pi);
			wlapi_enable_mac(pi->sh->physhim);
			delay_phy_cal = TRUE;
		}

		if (((pi->sh->now - pi->phycal_nslope) >= pi->sh->slow_timer) &&
		    (pi->sh->boardflags & BFL_ADCDIV) && !delay_phy_cal) {
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
			wlc_phy_cal_radio2050_nrssislope(pi);
			wlc_phy_vco_cal(pi);
			wlapi_enable_mac(pi->sh->physhim);
			delay_phy_cal = TRUE;
		}
	}

	if ((ISNPHY(pi)) && !pi->disable_percal && !delay_phy_cal) {


		/* 1) check to see if new cal needs to be activated */
		if ((pi->phy_cal_mode != PHY_PERICAL_DISABLE) &&
		    (pi->phy_cal_mode != PHY_PERICAL_MANUAL) &&
		    ((pi->sh->now - pi->cal_info->last_cal_time) >= pi->sh->glacial_timer))
			wlc_phy_cal_perical((wlc_phy_t *)pi, PHY_PERICAL_WATCHDOG);

		wlc_phy_txpwr_papd_cal_nphy(pi);

		wlc_phy_radio205x_check_vco_cal_nphy(pi);
	}

	if (ISHTPHY(pi) && !pi->disable_percal)	{

		/* do we really want to look at disable_percal? we have an enable flag,
		 * so isn't this redundant? (nphy does this, but seems weird)
		 */

		/* check to see if a cal needs to be run */
		if ((pi->phy_cal_mode != PHY_PERICAL_DISABLE) &&
		    (pi->phy_cal_mode != PHY_PERICAL_MANUAL) &&
		    ((pi->sh->now - pi->cal_info->last_cal_time) >= pi->sh->glacial_timer)) {
			PHY_CAL(("wlc_phy_watchdog: watchdog fired (en=%d, now=%d,"
				"prev_time=%d, glac=%d)\n",
				pi->phy_cal_mode, pi->sh->now,
				pi->cal_info->last_cal_time,  pi->sh->glacial_timer));

			wlc_phy_cal_perical((wlc_phy_t *)pi, PHY_PERICAL_WATCHDOG);
		}
	}


	if (ISLPPHY(pi)) {
		phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

		if (pi->phy_forcecal ||
			(pi_lp->lpphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec)) ||
			((pi->sh->now - pi->phy_lastcal) >= pi->sh->glacial_timer)) {
			if (!(pi->carrier_suppr_disable || pi->disable_percal)) {
				wlc_phy_periodic_cal_lpphy(pi);
			}
		}

		/* recal if max interval has elapsed, or min interval has elapsed and
		tx gain index has changed by more than some threshold
		*/
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			if ((pi_lp->lpphy_force_papd_cal == TRUE) ||
			    (pi_lp->lpphy_papd_recal_enable &&
			     (((pi->sh->now - pi_lp->lpphy_papd_last_cal >=
			        pi_lp->lpphy_papd_recal_max_interval) &&
			       (pi_lp->lpphy_papd_recal_max_interval != 0)) ||
			      (wlc_phy_tpc_isenabled_lpphy(pi) &&
			       (pi_lp->lpphy_papd_recal_min_interval != 0) &&
			       ((uint32)ABS(wlc_phy_get_current_tx_pwr_idx_lpphy(pi) -
			                    pi_lp->lpphy_papd_tx_gain_at_last_cal) >=
			        pi_lp->lpphy_papd_recal_gain_delta) &&
			       (pi->sh->now - pi_lp->lpphy_papd_last_cal >=
			        pi_lp->lpphy_papd_recal_min_interval))))) {
				if (!(pi->carrier_suppr_disable || pi->disable_percal)) {
					wlc_phy_papd_cal_txpwr_lpphy(pi,
						pi_lp->lpphy_force_papd_cal);
				}
			}
		}
	}

	if (ISSSLPNPHY(pi)) {
		if (pi->phy_forcecal ||
		    ((pi->sh->now - pi->phy_lastcal) >= pi->sh->glacial_timer)) {
			if (!(SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi) ||
				ASSOC_INPROG_PHY(pi) || pi->carrier_suppr_disable ||
				pi->pkteng_in_progress || pi->disable_percal)) {
					wlc_sslpnphy_periodic_cal(pi);
					/* wlc_sslpnphy_periodic_cal_top(pi); causes driver crash */
			}
		}
	}
	if (ISLCNPHY(pi)) {
		if (pi->phy_forcecal ||
			((pi->sh->now - pi->phy_lastcal) >= pi->sh->glacial_timer)) {
			if (!(SCAN_RM_IN_PROGRESS(pi) || ASSOC_INPROG_PHY(pi) ||
				pi->disable_percal))
				wlc_lcnphy_calib_modes(pi, LCNPHY_PERICAL_TEMPBASED_TXPWRCTRL);
			if (!(SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi) ||
				ASSOC_INPROG_PHY(pi) || pi->carrier_suppr_disable ||
				pi->pkteng_in_progress || pi->disable_percal))
				wlc_lcnphy_calib_modes(pi, PHY_PERICAL_WATCHDOG);
		}

		if (!(SCAN_RM_IN_PROGRESS(pi) ||
		      PLT_INPROG_PHY(pi) ||
		      ASSOC_INPROG_PHY(pi) ||
		      pi->carrier_suppr_disable ||
		      pi->pkteng_in_progress))
		  wlc_lcnphy_noise_measure(pi);

	}
}

#ifdef STA

#ifdef PR43338WAR
void
wlc_phy_11bap_reset(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;

	if (pi->war_b_ap) {
		pi->war_b_ap = FALSE;

		if (ISGPHY(pi))
			mod_phy_reg(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
				APHY_CTHR_CRS1_ENABLE, pi->war_b_ap_cthr_save);
		else if (ISNPHY(pi)) {

		}
	}
}

void
wlc_phy_11bap_restrict(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t*)pih;

	if (!pi->war_b_ap) {
		pi->war_b_ap = TRUE;

		if (ISGPHY(pi)) {
			pi->war_b_ap_cthr_save = APHY_CTHR_CRS1_ENABLE;
			mod_phy_reg(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
				APHY_CTHR_CRS1_ENABLE, 0);
			/* TODO, block change to APHY_CTHR_STHR_SHDIN for the bit */
		}
	}
}
#endif	/* PR43338WAR */
#endif /* STA */

void
wlc_phy_BSSinit(wlc_phy_t *pih, bool bonlyap, int rssi)
{
	phy_info_t *pi = (phy_info_t*)pih;
	uint i;
	uint k;

	if (bonlyap) {
	}

	/* watchdog idle phy noise */
	for (i = 0; i < MA_WINDOW_SZ; i++) {
		pi->sh->phy_noise_window[i] = (int8)(rssi & 0xff);
	}
	if (ISLCNPHY(pi)) {
		for (i = 0; i < MA_WINDOW_SZ; i++)
			pi->sh->phy_noise_window[i] = PHY_NOISE_FIXED_VAL_LCNPHY;
	}
	pi->sh->phy_noise_index = 0;

	if ((pi->sh->interference_mode == WLAN_AUTO) &&
	     (pi->aci_state & ACI_ACTIVE)) {
		/* Reset the clock to check again after the moving average buffer has filled
		 */
		pi->aci_start_time = pi->sh->now + MA_WINDOW_SZ;
	}

	for (i = 0; i < PHY_NOISE_WINDOW_SZ; i++) {
		for (k = WL_ANT_IDX_1; k < WL_ANT_RX_MAX; k++)
			pi->phy_noise_win[k][i] = PHY_NOISE_FIXED_VAL_NPHY;
	}
	pi->phy_noise_index = 0;
}


/* Convert epsilon table value to complex number */
void
wlc_phy_papd_decode_epsilon(uint32 epsilon, int32 *eps_real, int32 *eps_imag)
{
	if ((*eps_imag = (epsilon>>13)) > 0xfff)
		*eps_imag -= 0x2000; /* Sign extend */
	if ((*eps_real = (epsilon & 0x1fff)) > 0xfff)
		*eps_real -= 0x2000; /* Sign extend */
}

/* Atan table for cordic >> num2str(atan(1./(2.^[0:17]))/pi*180,8) */
static const fixed AtanTbl[] = {
	2949120,
	1740967,
	919879,
	466945,
	234379,
	117304,
	58666,
	29335,
	14668,
	7334,
	3667,
	1833,
	917,
	458,
	229,
	115,
	57,
	29
};

void
wlc_phy_cordic(fixed theta, cint32 *val)
{
	fixed angle, valtmp;
	unsigned iter;
	int signx = 1;
	int signtheta;

	val[0].i = CORDIC_AG;
	val[0].q = 0;
	angle    = 0;

	/* limit angle to -180 .. 180 */
	signtheta = (theta < 0) ? -1 : 1;
	theta = ((theta+FIXED(180)*signtheta)% FIXED(360))-FIXED(180)*signtheta;

	/* rotate if not in quadrant one or four */
	if (FLOAT(theta) > 90) {
		theta -= FIXED(180);
		signx = -1;
	} else if (FLOAT(theta) < -90) {
		theta += FIXED(180);
		signx = -1;
	}

	/* run cordic iterations */
	for (iter = 0; iter < CORDIC_NI; iter++) {
		if (theta > angle) {
			valtmp = val[0].i - (val[0].q >> iter);
			val[0].q = (val[0].i >> iter) + val[0].q;
			val[0].i = valtmp;
			angle += AtanTbl[iter];
		} else {
			valtmp = val[0].i + (val[0].q >> iter);
			val[0].q = -(val[0].i >> iter) + val[0].q;
			val[0].i = valtmp;
			angle -= AtanTbl[iter];
		}
	}

	/* re-rotate quadrant two and three points */
	val[0].i = val[0].i*signx;
	val[0].q = val[0].q*signx;
}

#if defined(PHYCAL_CACHING) || defined(WLMCHAN)
int
wlc_phy_cal_cache_init(wlc_phy_t *ppi)
{
	return 0;
}

void
wlc_phy_cal_cache_deinit(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	ch_calcache_t *ctx = pi->calcache;

	while (ctx) {
		pi->calcache = ctx->next;
		MFREE(pi->sh->osh, ctx,
		      sizeof(ch_calcache_t));
		ctx = pi->calcache;
	}

	pi->calcache = NULL;

	/* No more per-channel contexts, switch in the default one */
	pi->cal_info = &pi->def_cal_info;
	/* Reset the parameters */
	pi->cal_info->last_cal_temp = -50;
	pi->cal_info->last_cal_time = 0;
}
#endif /* defined(PHYCAL_CACHING) || defined(WLMCHAN) */

#ifdef PHYCAL_CACHING
void
wlc_phy_cal_cache_set(wlc_phy_t *ppi, bool state)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	pi->calcache_on = state;
	if (!state)
		wlc_phy_cal_cache_deinit(ppi);
}

bool
wlc_phy_cal_cache_get(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	return pi->calcache_on;
}
#endif /* PHYCAL_CACHING */

void
wlc_phy_cal_perical_mphase_reset(phy_info_t *pi)
{
	phy_cal_info_t *cal_info = pi->cal_info;

#ifdef WLMCHAN
	PHY_CAL(("wlc_phy_cal_perical_mphase_reset chanspec 0x%x ctx: %p\n", pi->radio_chanspec,
	           wlc_phy_get_chanctx(pi, pi->radio_chanspec)));
#else
	PHY_CAL(("wlc_phy_cal_perical_mphase_reset\n"));
#endif

	wlapi_del_timer(pi->sh->physhim, pi->phycal_timer);

#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	if ( pi->enable_percal_delay )
		wlapi_cancel_delayed_task(pi->percal_task);
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */
	pi->cal_type_override = PHY_PERICAL_AUTO;
	cal_info->cal_phase_id = MPHASE_CAL_STATE_IDLE;
	cal_info->txcal_cmdidx = 0; /* needed in nphy only */
}

static void
wlc_phy_cal_perical_mphase_schedule(phy_info_t *pi, uint delay)
{
	/* for manual mode, let it run */
	if ((pi->phy_cal_mode != PHY_PERICAL_MPHASE) &&
	    (pi->phy_cal_mode != PHY_PERICAL_MANUAL))
		return;

	PHY_CAL(("wlc_phy_cal_perical_mphase_schedule\n"));

	/* use timer to wait for clean context since this may be called in the middle of nphy_init
	 */
	wlapi_del_timer(pi->sh->physhim, pi->phycal_timer);
#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	if ( pi->enable_percal_delay) 
		wlapi_cancel_delayed_task(pi->percal_task);
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */

	pi->cal_info->cal_phase_id = MPHASE_CAL_STATE_INIT;

#if defined(DSLCPE) && defined(DSLCPE_PERCAL_DELAY)
	if ( pi->enable_percal_delay) {
		wlapi_schedule_delayed_task(pi->percal_task, delay);
	}
 	else
#endif /* DSLCPE && DSLCPE_PERCAL_DELAY */
	wlapi_add_timer(pi->sh->physhim, pi->phycal_timer, delay, 0);
}

/* policy entry */
void
wlc_phy_cal_perical(wlc_phy_t *pih, uint8 reason)
{
	int16 nphy_currtemp = 0;
	int16 delta_temp = 0;
	bool  suppress_cal = FALSE;
	phy_info_t *pi = (phy_info_t*)pih;
#if defined(WLMCHAN) || defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
#endif

	if (!ISNPHY(pi) && !ISHTPHY(pi))
		return;

	if ((pi->phy_cal_mode == PHY_PERICAL_DISABLE) ||
	    (pi->phy_cal_mode == PHY_PERICAL_MANUAL))
		return;

	/* NPHY_IPA : disable PAPD cal for following calibration at least 4322A1? */

	PHY_CAL(("wlc_phy_cal_perical: reason %d chanspec 0x%x\n", reason,
	         pi->radio_chanspec));

	/* perical is enable: either singlephase only, or mphase is allowed
	 *  dispatch to s-phase or m-phase based on reasons
	 */
	switch (reason) {
	case PHY_PERICAL_DRIVERUP:	/* always single phase ? */
		break;

	case PHY_PERICAL_PHYINIT:
		if (pi->phy_cal_mode == PHY_PERICAL_MPHASE) {
#ifdef WLMCHAN
			if (ctx) {
				/* Switched the context so restart a pending MPHASE cal or
				 * restore stored calibration
				 */
				ASSERT(ctx->chanspec == pi->radio_chanspec);

				/* If it was pending last time, just restart it */
				if (PHY_PERICAL_MPHASE_PENDING(pi)) {
					/* Delete any exisiting timer just in case */
					PHY_CAL(("%s: Restarting calibration for 0x%x phase %d\n",
					         __FUNCTION__, ctx->chanspec,
					         pi->cal_info->cal_phase_id));
					wlapi_del_timer(pi->sh->physhim, pi->phycal_timer);
					wlapi_add_timer(pi->sh->physhim, pi->phycal_timer, 0, 0);
				} else if (wlc_phy_cal_cache_restore(pi) != BCME_ERROR)
					break;
			} else
#endif /* WLMCHAN */
			if (PHY_PERICAL_MPHASE_PENDING(pi)) {
				wlc_phy_cal_perical_mphase_reset(pi);
			}

			pi->cal_info->cal_searchmode = PHY_CAL_SEARCHMODE_RESTART;
			/* schedule mphase cal */
			wlc_phy_cal_perical_mphase_schedule(pi, PHY_PERICAL_INIT_DELAY);
		}
		break;

	case PHY_PERICAL_JOIN_BSS:
	case PHY_PERICAL_START_IBSS:
	case PHY_PERICAL_UP_BSS:

		/* for these, want single phase cal to ensure immediately 
		 *  clean Tx/Rx so auto-rate fast-start is promising
		 */
		if ((pi->phy_cal_mode == PHY_PERICAL_MPHASE) &&
			PHY_PERICAL_MPHASE_PENDING(pi)) {
			wlc_phy_cal_perical_mphase_reset(pi);
		}

		/* Always do idle TSSI measurement at the end of NPHY cal
		   while starting/joining a BSS/IBSS
		*/
		pi->first_cal_after_assoc = TRUE;

		pi->cal_type_override = PHY_PERICAL_FULL; /* not used in htphy */


		if (pi->phycal_tempdelta) {
			pi->cal_info->last_cal_temp = wlc_phy_tempsense_nphy(pi);
		}
#if defined(PHYCAL_CACHING) || defined(WLMCHAN)
		if (ctx) {
			ASSERT(ISHTPHY(pi)==0); /* caching not defined for htphy */
			PHY_CAL(("wl%d: %s: Attemping to restore cals on JOIN...\n",
			          pi->sh->unit, __FUNCTION__));
			if (wlc_phy_cal_cache_restore(pi) == BCME_ERROR)
				wlc_phy_cal_perical_nphy_run(pi, PHY_PERICAL_FULL);
		}
		else
#endif /* PHYCAL_CACHING */
			if (ISNPHY(pi)) {
				wlc_phy_cal_perical_nphy_run(pi, PHY_PERICAL_FULL);
			} else if (ISHTPHY(pi)) {
				wlc_phy_cals_htphy(pi, PHY_CAL_SEARCHMODE_RESTART);
			}
		break;

	case PHY_PERICAL_WATCHDOG:
		if (pi->phycal_tempdelta && ISNPHY(pi)) {
			nphy_currtemp = wlc_phy_tempsense_nphy(pi);
			delta_temp =
			    (nphy_currtemp > pi->cal_info->last_cal_temp)?
			    nphy_currtemp - pi->cal_info->last_cal_temp :
			    pi->cal_info->last_cal_temp - nphy_currtemp;

			/* Only do WATCHDOG triggered (periodic) calibration if
			   the channel hasn't changed and if the temperature delta
			   is above the specified threshold
			*/
			if ((delta_temp < (int16)pi->phycal_tempdelta) &&
			    (pi->cal_info->u.ncal.txiqlocal_chanspec == pi->radio_chanspec)) {
				suppress_cal = TRUE;
			} else {
				pi->cal_info->last_cal_temp = nphy_currtemp;
			}
		}

		if (!suppress_cal) {

			/* if mphase is allowed, do it, otherwise, fall back to single phase */
			if (pi->phy_cal_mode == PHY_PERICAL_MPHASE) {
				/* only schedule if it's not in progress */
				if (!PHY_PERICAL_MPHASE_PENDING(pi)) {
					pi->cal_info->cal_searchmode = PHY_CAL_SEARCHMODE_REFINE;
					wlc_phy_cal_perical_mphase_schedule(pi,
						PHY_PERICAL_WDOG_DELAY);
				}
			} else if (pi->phy_cal_mode == PHY_PERICAL_SPHASE) {
				if (ISNPHY(pi)) {
					wlc_phy_cal_perical_nphy_run(pi, PHY_PERICAL_AUTO);
				} else if (ISHTPHY(pi)) {
					wlc_phy_cals_htphy(pi, PHY_CAL_SEARCHMODE_RESTART);
				}
			} else {
				ASSERT(0);
			}
		}
		break;

	default:
		ASSERT(0);
		break;
	}
}

void
wlc_phy_cal_perical_mphase_restart(phy_info_t *pi)
{
	PHY_CAL(("wlc_phy_cal_perical_mphase_restart\n"));
	pi->cal_info->cal_phase_id = MPHASE_CAL_STATE_INIT;
	pi->cal_info->txcal_cmdidx = 0;
}

uint8
wlc_phy_nbits(int32 value)
{
	int32 abs_val;
	uint8 nbits = 0;

	abs_val = ABS(value);
	while ((abs_val >> nbits) > 0) nbits++;

	return nbits;
}

uint32
wlc_phy_sqrt_int(uint32 value)
{
	uint32 root = 0, shift = 0;

	/* Compute integer nearest to square root of input integer value */
	for (shift = 0; shift < 32; shift += 2) {
		if (((0x40000000 >> shift) + root) <= value) {
			value -= ((0x40000000 >> shift) + root);
			root = (root >> 1) | (0x40000000 >> shift);
		} else {
			root = root >> 1;
		}
	}

	/* round to the nearest integer */
	if (root < value) ++root;

	return root;
}

void
wlc_phy_stf_chain_init(wlc_phy_t *pih, uint8 txchain, uint8 rxchain)
{
	phy_info_t *pi = (phy_info_t*)pih;

	pi->sh->hw_phytxchain = txchain;
	pi->sh->hw_phyrxchain = rxchain;
	pi->sh->phytxchain = txchain;
	pi->sh->phyrxchain = rxchain;
}

void
wlc_phy_stf_chain_set(wlc_phy_t *pih, uint8 txchain, uint8 rxchain)
{
	phy_info_t *pi = (phy_info_t*)pih;

	PHY_TRACE(("wlc_phy_stf_chain_set, new phy chain tx %d, rx %d", txchain, rxchain));

	pi->sh->phytxchain = txchain;

	if (ISNPHY(pi)) {
		wlc_phy_rxcore_setstate_nphy(pih, rxchain);
	}
}

void
wlc_phy_stf_chain_get(wlc_phy_t *pih, uint8 *txchain, uint8 *rxchain)
{
	phy_info_t *pi = (phy_info_t*)pih;

	*txchain = pi->sh->phytxchain;
	*rxchain = pi->sh->phyrxchain;
}

uint8
wlc_phy_stf_chain_active_get(wlc_phy_t *pih)
{
	int16 nphy_currtemp;
	uint8 active_bitmap;
	phy_info_t *pi = (phy_info_t*)pih;

	/* upper nibble is for rxchain, lower nibble is for txchain */
	active_bitmap = (pi->txcore_temp.heatedup)? 0x31 : 0x33;

	/* Watchdog override is a master switch to kill all PHY related
	 *  periodic activities in the driver
	 */
	if (!pi->phywatchdog_override)
		return active_bitmap;

	if (NREV_IS(pi->pubpi.phy_rev, 6) ||
	    (NREV_GE(pi->pubpi.phy_rev, 7) && (pi->pubpi.radiorev == 5) &&
	     (pi->pubpi.radiover == 0x1))) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		nphy_currtemp = wlc_phy_tempsense_nphy(pi);
		wlapi_enable_mac(pi->sh->physhim);

		if (!pi->txcore_temp.heatedup) {
			if (nphy_currtemp >= pi->txcore_temp.disable_temp) {
				active_bitmap &= 0xFD;
				pi->txcore_temp.heatedup = TRUE;
			}
		} else {
			if (nphy_currtemp <= pi->txcore_temp.enable_temp) {
				active_bitmap |= 0x2;
				pi->txcore_temp.heatedup = FALSE;
			}
		}
	}

	return active_bitmap;
}

int8
wlc_phy_stf_ssmode_get(wlc_phy_t *pih, chanspec_t chanspec)
{
	phy_info_t *pi = (phy_info_t*)pih;
	uint8 siso_mcs_id, cdd_mcs_id;

	PHY_TRACE(("wl%d: %s: chanspec %x\n", pi->sh->unit, __FUNCTION__, chanspec));

	/* For simplicity, only MCS0/Legacy OFDM 6 Mbps is checked to choose between CDD and SISO */
	siso_mcs_id = (CHSPEC_IS40(chanspec)) ? TXP_FIRST_MCS_40_SISO : TXP_FIRST_MCS_20_SISO;
	cdd_mcs_id = (CHSPEC_IS40(chanspec)) ? TXP_FIRST_MCS_40_CDD : TXP_FIRST_MCS_20_CDD;

	/* criteria to choose stf mode */

	/* the "+3dbm (12 0.25db units)" is to account for the fact that with CDD, tx occurs
	 * on both chains
	 */
	if (pi->tx_power_target[siso_mcs_id] > (pi->tx_power_target[cdd_mcs_id] + 12))
		return PHY_TXC1_MODE_SISO;
	else
		return PHY_TXC1_MODE_CDD;
}

#ifdef SAMPLE_COLLECT

/*  A function to do sample collect
	Parameters for NPHY (with NREV < 7) are ----
	coll_us: collection time (in us).
	cores: 0, 1 or -1 for using core 0, 1 or both in 40MHz (ignored in 20MHz).
	Parameters for NPHY (with NREV >= 7) are ----
	mode: sample collect type.
	trigger: sample collect trigger bitmap.
	post_dur: post-trigger collection time (in us).
	For NPHY with (NREV < 7), we have the following
	If BW=20MHz, both cores are sampled simultaneously and the returned buffer
	has the structure [(uint16)num_bytes, rxpower(core0), rxpower(core1),
	I0(core0), Q0(core0), I0(core1), Q0(core1),...].
	If BW=40MHz, cores are sampled sequentially and the returned buffer has the
	structure [(uint16)num_bytes(core0), rxpower(core0), rxpower(core1),
	I0(core0), Q0(core0),...,(uint16)num_bytes(core1), rxpower(core0), rxpower(core1),
	I0(core1), Q0(core1),...].
	In 20MHz the sample frequency is 20MHz and in 40MHz the sample frequency is 40MHz.
*/
static int
wlc_phy_sample_collect(phy_info_t *pi, wl_samplecollect_args_t *collect, void *buff)
{
	int ret;

	/* driver must be "out" (not up but chip is alive) */
	if (pi->sh->up)
		return BCME_NOTDOWN;
	if (!pi->sh->clk)
		return BCME_NOCLK;

	if (ISHTPHY(pi)) {
		ret = wlc_phy_sample_collect_htphy(pi, collect, (uint32 *)buff);
		return ret;
	} else if (ISNPHY(pi)) {
		if (NREV_LT(pi->pubpi.phy_rev, 7)) {
			ret = wlc_phy_sample_collect_old(pi, collect, buff);
		} else {
			ret = wlc_phy_sample_collect_nphy(pi, collect, (uint32 *)buff);
			return ret;
		}
	}

	return BCME_UNSUPPORTED;
}

static int
wlc_phy_sample_data(phy_info_t *pi, wl_sampledata_t *sample_data, void *b)
{
	int ret;

	/* driver must be "out" (not up but chip is alive) */
	if (pi->sh->up)
		return BCME_NOTDOWN;
	if (!pi->sh->clk)
		return BCME_NOCLK;

	if (ISHTPHY(pi)) {
		ret = wlc_phy_sample_data_htphy(pi, sample_data, b);
		return ret;
	} else if (ISNPHY(pi) && NREV_GE(pi->pubpi.phy_rev, 7)) {
		ret = wlc_phy_sample_data_nphy(pi, sample_data, b);
		return ret;
	}

	return BCME_UNSUPPORTED;
}

static int
wlc_phy_sample_collect_old(phy_info_t *pi, wl_samplecollect_args_t *collect, void *buff)
{
	int cores;
	uint8 coll_us;
	uint16 *ptr = (uint16 *) buff;
	uint samp_freq_MHz, byte_per_samp, samp_count, byte_count;
	int start_core, end_core, core;
	uint16 mask, val, coll_type = 1;
	uint16 crsctl, crsctlu, crsctll;

	osl_t *osh;
	osh = pi->sh->osh;

	cores = collect->cores;
	coll_us = collect->coll_us;

	if (cores > 1)
		return BCME_RANGE;

	if (IS40MHZ(pi)) {
		samp_freq_MHz = 40;
		byte_per_samp = 2;
		if (cores < 0) {
			start_core = 0;
			end_core = 1;
		} else {
			start_core = end_core = cores;
		}
	} else {
		samp_freq_MHz = 20;
		byte_per_samp = 4;
		start_core = end_core = 0;
	}
	samp_count = coll_us * samp_freq_MHz;
	byte_count = samp_count * byte_per_samp;

	if (byte_count > WLC_SAMPLECOLLECT_MAXLEN) {
		PHY_ERROR(("wl%d: %s: Not sufficient memory for this collect, byte_count: %d\n",
			pi->sh->unit, __FUNCTION__, byte_count));
		return BCME_ERROR;
	}

	wlapi_ucode_sample_init(pi->sh->physhim);

	wlc_phy_sample_collect_start_nphy(pi, coll_us, &crsctl, &crsctlu, &crsctll);

	/* RUN it */

	/* The RxMacifMode phyreg is passed to ucode to start the collect */
	mask = NPHY_RxMacifMode_SampleCore_MASK	| NPHY_RxMacifMode_PassThrough_MASK;

	/* Clear the macintstatus bit that indicates ucode timeout */
	W_REG(osh, &pi->regs->macintstatus, (1 << 24));

	for (core = start_core; core <= end_core; ptr += 1 + (ptr[0] >> 1), core++) {
		val = (core << NPHY_RxMacifMode_SampleCore_SHIFT) |
			(coll_type << NPHY_RxMacifMode_PassThrough_SHIFT);
		val |= (read_phy_reg(pi, NPHY_RxMacifMode) & ~mask);

		/* Use SHM 0 for phyreg value (will be overwritten by the samples */
		wlapi_bmac_write_shm(pi->sh->physhim, 0, val);

		/* Give the command and wait */
		OR_REG(osh, &pi->regs->maccommand, MCMD_SAMPLECOLL);
		SPINWAIT(((R_REG(osh, &pi->regs->maccommand) & MCMD_SAMPLECOLL) != 0), 50000);

		if ((R_REG(osh, &pi->regs->maccommand) & MCMD_SAMPLECOLL) != 0) {
			PHY_ERROR(("wl%d: %s: Failed to finish sample collect\n",
				pi->sh->unit, __FUNCTION__));
			return BCME_ERROR;
		}

		/* Verify that the sample collect didn't timeout in the ucode */
		if (R_REG(osh, &pi->regs->macintstatus) & (1 << 24)) {
			PHY_ERROR(("wl%d: %s: Sample Collect failed after 10 attempts\n",
				pi->sh->unit, __FUNCTION__));
			return BCME_ERROR;
		}

		/* RXE_RXCNT is stored in S_RSV1 */
		W_REG(osh, &pi->regs->objaddr, OBJADDR_SCR_SEL + S_RSV1);
		ptr[0] = R_REG(osh, &pi->regs->objdata) & ~0x1;
		/* Hardcode the peak-rxpower for the specific rx-gain */
		ptr[1] = 0xB8B8;	/* 2's complement -72dBm */
		wlapi_copyfrom_objmem(pi->sh->physhim, 0, &ptr[2], ptr[0], OBJADDR_SHM_SEL);

		ptr[0] += 2;
	}

	wlc_phy_sample_collect_end_nphy(pi, crsctl, crsctlu, crsctll);

	return BCME_OK;
}
#endif	/* SAMPLE_COLLECT */


#ifdef WLTEST
void
wlc_phy_boardflag_upd(wlc_phy_t *pih)
{
	phy_info_t *pi = (phy_info_t *)pih;

	if NREV_GE(pi->pubpi.phy_rev, 3) {
		/* Check if A-band spur WAR should be enabled for this board */
		if (pi->sh->boardflags2 & BFL2_SPUR_WAR) {
			PHY_ERROR(("%s: aband_spurwar on\n", __FUNCTION__));
			pi->nphy_aband_spurwar_en = TRUE;
		} else {
			PHY_ERROR(("%s: aband_spurwar off\n", __FUNCTION__));
			pi->nphy_aband_spurwar_en = FALSE;
		}
	}

	if NREV_GE(pi->pubpi.phy_rev, 6) {
		/* Check if extra G-band spur WAR for 40 MHz channels 3 through 10
		 * should be enabled for this board
		 */
		if (pi->sh->boardflags2 & BFL2_2G_SPUR_WAR) {
			PHY_ERROR(("%s: gband_spurwar2 on\n", __FUNCTION__));
			pi->nphy_gband_spurwar2_en = TRUE;
		} else {
			PHY_ERROR(("%s: gband_spurwar2 off\n", __FUNCTION__));
			pi->nphy_gband_spurwar2_en = FALSE;
		}
	}

	pi->nphy_txpwrctrl = PHY_TPC_HW_OFF;
	pi->txpwrctrl = PHY_TPC_HW_OFF;
	pi->phy_5g_pwrgain = FALSE;

	if (pi->sh->boardvendor == VENDOR_APPLE &&
	    (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))) {

		pi->nphy_txpwrctrl =  PHY_TPC_HW_ON;
		pi->phy_5g_pwrgain = TRUE;

	} else if ((pi->sh->boardflags2 & BFL2_TXPWRCTRL_EN) &&
		NREV_GE(pi->pubpi.phy_rev, 2) && (pi->sh->sromrev >= 4)) {

		pi->nphy_txpwrctrl = PHY_TPC_HW_ON;

	} else if ((pi->sh->sromrev >= 4) && (pi->sh->boardflags2 & BFL2_5G_PWRGAIN)) {
		pi->phy_5g_pwrgain = TRUE;
	}
}
#endif /* WLTEST */

/* %%%%%% dump */

#if defined(BCMDBG) || defined(BCMDBG_DUMP)


/* Below wlc_phydump_xx should be generic, applicable to all PHYs.
 * each PHY section should have ISXPHY macro checking
 */
void
wlc_phydump_measlo(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	int padmix_en = 0;
	int j, z, bb_attn, rf_attn;
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISGPHY(pi))
		return;

	bcm_bprintf(b, "bb_list_size= %d, rf_list_size= %d\n *RF* ", pi->bb_list_size,
		pi->rf_list_size);

	for (j = 0; j < pi->bb_list_size; j++)
		bcm_bprintf(b, "  %d:i,q  ", pi->bb_attn_list[j]);

	bcm_bprintf(b, "\n");

	for (z = 0; z < pi->rf_list_size; z++) {
		rf_attn   = (int)((char)PHY_GET_RFATTN(pi->rf_attn_list[z]));
		padmix_en = (int)((char)PHY_GET_PADMIX(pi->rf_attn_list[z]));
		bcm_bprintf(b, "%2d %1d", rf_attn, padmix_en);

		for (j = 0; j < pi->bb_list_size; j++) {
			int i, q;
			bb_attn = pi->bb_attn_list[j];
			i = (int)((char)pi->gphy_locomp_iq[PHY_GET_RFGAINID(rf_attn, padmix_en,
				pi->rf_max)][bb_attn].i);
			q = (int)((char)pi->gphy_locomp_iq[PHY_GET_RFGAINID(rf_attn, padmix_en,
				pi->rf_max)][bb_attn].q);
			bcm_bprintf(b, " : %3d %2d", i, q);
		}
		bcm_bprintf(b, "\n");
	}
}

void
wlc_phydump_phycal(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	nphy_iq_comp_t rxcal_coeffs;
	int16 txcal_ofdm_coeffs[8];
	int16 txcal_bphy_coeffs[8];
	uint16 radio_regs[8] = { 0 };
	uint16 rccal_val[2] = { 0 };
	int ci, cq;
	int ei, eq, fi, fq;
	int time_elapsed;
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!pi->sh->up)
		return;

	if (!ISNPHY(pi) && !(ISHTPHY(pi)))
		return;

	/* for HTPHY, branch out to htphy phycal dump routine */
	if (ISHTPHY(pi)) {
		wlc_phy_cal_dump_htphy(pi, b);
	} else {
		/* for NPHY, carry out the following steps */
		time_elapsed = pi->sh->now - pi->cal_info->last_cal_time;
		if (time_elapsed < 0)
			time_elapsed = 0;

		bcm_bprintf(b, "time since last cal: %d (sec), mphase_cal_id: %d\n\n",
			time_elapsed, pi->cal_info->cal_phase_id);

		bcm_bprintf(b, "Pre-calibration txpwr indices: %d, %d\n",
			pi->nphy_cal_orig_pwr_idx[0], pi->nphy_cal_orig_pwr_idx[1]);

		bcm_bprintf(b, "Tx Calibration pwr indices: %d, %d\n",
			pi->nphy_txcal_pwr_idx[0], pi->nphy_txcal_pwr_idx[1]);

		if (NREV_GE(pi->pubpi.phy_rev, 7)) {
			bcm_bprintf(b, "Calibration target txgain Core 0: txlpf=%d,"
				" txgm=%d, pga=%d, pad=%d, ipa=%d\n",
				pi->nphy_cal_target_gain.txlpf[0],
				pi->nphy_cal_target_gain.txgm[0],
				pi->nphy_cal_target_gain.pga[0],
				pi->nphy_cal_target_gain.pad[0],
				pi->nphy_cal_target_gain.ipa[0]);
		} else {
			bcm_bprintf(b, "Calibration target txgain Core 0: txgm=%d, pga=%d,"
				" pad=%d, ipa=%d\n",
				pi->nphy_cal_target_gain.txgm[0],
				pi->nphy_cal_target_gain.pga[0],
				pi->nphy_cal_target_gain.pad[0],
				pi->nphy_cal_target_gain.ipa[0]);
		}

		if (NREV_GE(pi->pubpi.phy_rev, 7)) {
			bcm_bprintf(b, "Calibration target txgain Core 1: txlpf=%d,"
				" txgm=%d, pga=%d, pad=%d, ipa=%d\n",
				pi->nphy_cal_target_gain.txlpf[1],
				pi->nphy_cal_target_gain.txgm[1],
				pi->nphy_cal_target_gain.pga[1],
				pi->nphy_cal_target_gain.pad[1],
				pi->nphy_cal_target_gain.ipa[1]);
		} else {
			bcm_bprintf(b, "Calibration target txgain Core 1: txgm=%d,"
				" pga=%d, pad=%d, ipa=%d\n",
				pi->nphy_cal_target_gain.txgm[1],
				pi->nphy_cal_target_gain.pga[1],
				pi->nphy_cal_target_gain.pad[1],
				pi->nphy_cal_target_gain.ipa[1]);
		}

		bcm_bprintf(b, "Calibration RFSEQ table txgains : 0x%x, 0x%x\n\n",
			pi->nphy_cal_orig_tx_gain[0],
			pi->nphy_cal_orig_tx_gain[1]);

		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phyreg_enter((wlc_phy_t *)pi);

		if (pi->phyhang_avoid)
			wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);

		/* Read Rx calibration co-efficients */
		wlc_phy_rx_iq_coeffs_nphy(pi, 0, &rxcal_coeffs);

		if (NREV_GE(pi->pubpi.phy_rev, 3)) {
			radio_regs[0] =	read_radio_reg(pi,
				RADIO_2056_TX_LOFT_FINE_I | RADIO_2056_TX0);
			radio_regs[1] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_FINE_Q | RADIO_2056_TX0);
			radio_regs[2] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_FINE_I | RADIO_2056_TX1);
			radio_regs[3] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_FINE_Q | RADIO_2056_TX1);
			radio_regs[4] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_COARSE_I | RADIO_2056_TX0);
			radio_regs[5] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_COARSE_Q | RADIO_2056_TX0);
			radio_regs[6] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_COARSE_I | RADIO_2056_TX1);
			radio_regs[7] = read_radio_reg(pi,
				RADIO_2056_TX_LOFT_COARSE_Q | RADIO_2056_TX1);
			/* read rccal value */
			rccal_val[0] = read_radio_reg(pi,
				RADIO_2056_RX_RXLPF_RCCAL_LPC | RADIO_2056_RX0);
			rccal_val[1] = read_radio_reg(pi,
				RADIO_2056_RX_RXLPF_RCCAL_LPC | RADIO_2056_RX1);
		} else {
			/* Read the LOFT compensation (Ci/Cq) values */
			radio_regs[0] = read_radio_reg(pi, RADIO_2055_CORE1_TX_VOS_CNCL);
			radio_regs[1] = read_radio_reg(pi, RADIO_2055_CORE2_TX_VOS_CNCL);
		}

		/* Read OFDM Tx calibration co-efficients */
		wlc_phy_table_read_nphy(pi, NPHY_TBL_ID_IQLOCAL, 8, 80, 16, txcal_ofdm_coeffs);

		/* Read BPHY Tx calibration co-efficients */
		wlc_phy_table_read_nphy(pi, NPHY_TBL_ID_IQLOCAL, 8, 88, 16, txcal_bphy_coeffs);

		if (pi->phyhang_avoid)
			wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);

		/* reg access is done, enable the mac */
		wlc_phyreg_exit((wlc_phy_t *)pi);
		wlapi_enable_mac(pi->sh->physhim);

		bcm_bprintf(b, "Tx IQ/LO cal coeffs for OFDM PHY:\n");

		txcal_ofdm_coeffs[0] &= 0x3ff;
		txcal_ofdm_coeffs[1] &= 0x3ff;
		txcal_ofdm_coeffs[2] &= 0x3ff;
		txcal_ofdm_coeffs[3] &= 0x3ff;

		if (txcal_ofdm_coeffs[0] > 511)
			txcal_ofdm_coeffs[0] -= 1024;

		if (txcal_ofdm_coeffs[1] > 511)
			txcal_ofdm_coeffs[1] -= 1024;

		if (txcal_ofdm_coeffs[2] > 511)
			txcal_ofdm_coeffs[2] -= 1024;

		if (txcal_ofdm_coeffs[3] > 511)
			txcal_ofdm_coeffs[3] -= 1024;

		bcm_bprintf(b, "A0=%d, B0=%d, A1=%d, B1=%d\n",
			txcal_ofdm_coeffs[0],
			txcal_ofdm_coeffs[1],
			txcal_ofdm_coeffs[2],
			txcal_ofdm_coeffs[3]);

		if (NREV_GE(pi->pubpi.phy_rev, 3)) {
			ei = (int)(radio_regs[0] & 0xf) - (int)((radio_regs[0] & 0xf0) >> 4);
			eq = (int)(radio_regs[1] & 0xf) - (int)((radio_regs[1] & 0xf0) >> 4);
			fi = (int)(radio_regs[4] & 0xf) - (int)((radio_regs[4] & 0xf0) >> 4);
			fq = (int)(radio_regs[5] & 0xf) - (int)((radio_regs[5] & 0xf0) >> 4);
			bcm_bprintf(b, "Core 0: LOFT_FINE_I=0x%0x, LOFT_FINE_Q=0x%0x\n",
				radio_regs[0], radio_regs[1]);
			bcm_bprintf(b, "Core 0: LOFT_COARSE_I=0x%0x, LOFT_COARSE_Q=0x%0x\n",
				radio_regs[4], radio_regs[5]);
			bcm_bprintf(b, "Core 0: ei=%d, eq=%d, fi=%d, fq=%d\n",
				ei, eq, fi, fq);

			ei = (int)(radio_regs[2] & 0xf) - (int)((radio_regs[2] & 0xf0) >> 4);
			eq = (int)(radio_regs[3] & 0xf) - (int)((radio_regs[3] & 0xf0) >> 4);
			fi = (int)(radio_regs[6] & 0xf) - (int)((radio_regs[6] & 0xf0) >> 4);
			fq = (int)(radio_regs[7] & 0xf) - (int)((radio_regs[7] & 0xf0) >> 4);
			bcm_bprintf(b, "Core 1: LOFT_FINE_I=0x%0x, LOFT_FINE_Q=0x%0x\n",
				radio_regs[2], radio_regs[3]);
			bcm_bprintf(b, "Core 1: LOFT_COARSE_I=0x%0x, LOFT_COARSE_Q=0x%0x\n",
				radio_regs[6], radio_regs[7]);
			bcm_bprintf(b, "Core 1: ei=%d, eq=%d, fi=%d, fq=%d\n",
				ei, eq, fi, fq);
		} else {
			if (radio_regs[0] & 0xC0)
				ci = (radio_regs[0] & 0xC0) >> 6;
			else
				ci = ((radio_regs[0] & 0x30) >> 4) * -1;

			if (radio_regs[0] & 0x0C)
				cq = (radio_regs[0] & 0x0C) >> 3;
			else
				cq = (radio_regs[0] & 0x03) * -1;

			bcm_bprintf(b, "CORE1_VOS_CNCL=0x%0x, Ci0=%d, Cq0=%d\n",
				radio_regs[0], ci, cq);

			if (radio_regs[1] & 0xC0)
				ci = (radio_regs[1] & 0xC0) >> 6;
			else
				ci = ((radio_regs[1] & 0x30) >> 4) * -1;

			if (radio_regs[1] & 0x0C)
				cq = (radio_regs[1] & 0x0C) >> 3;
			else
				cq = (radio_regs[1] & 0x03) * -1;

			bcm_bprintf(b, "CORE2_VOS_CNCL=0x%0x, Ci1=%d, Cq1=%d\n",
				radio_regs[1], ci, cq);
		}

		bcm_bprintf(b, "Di0=%d, Dq0=%d, Di1=%d, Dq1=%d, m0=%d, m1=%d\n\n\n",
			(int8) ((txcal_ofdm_coeffs[5] & 0xFF00) >> 8),
			(int8) (txcal_ofdm_coeffs[5] & 0x00FF),
			(int8) ((txcal_ofdm_coeffs[6] & 0xFF00) >> 8),
			(int8) (txcal_ofdm_coeffs[6] & 0x00FF),
			(int8) ((txcal_ofdm_coeffs[7] & 0xFF00) >> 8),
			(int8) (txcal_ofdm_coeffs[7] & 0x00FF));

		bcm_bprintf(b, "Tx IQ/LO cal coeffs for BPHY:\n");

		txcal_bphy_coeffs[0] &= 0x3ff;
		txcal_bphy_coeffs[1] &= 0x3ff;
		txcal_bphy_coeffs[2] &= 0x3ff;
		txcal_bphy_coeffs[3] &= 0x3ff;

		if (txcal_bphy_coeffs[0] > 511)
			txcal_bphy_coeffs[0] -= 1024;

		if (txcal_bphy_coeffs[1] > 511)
			txcal_bphy_coeffs[1] -= 1024;

		if (txcal_bphy_coeffs[2] > 511)
			txcal_bphy_coeffs[2] -= 1024;

		if (txcal_bphy_coeffs[3] > 511)
			txcal_bphy_coeffs[3] -= 1024;

		bcm_bprintf(b, "A0=%d, B0=%d, A1=%d, B1=%d\n",
			txcal_bphy_coeffs[0],
			txcal_bphy_coeffs[1],
			txcal_bphy_coeffs[2],
			txcal_bphy_coeffs[3]);

		bcm_bprintf(b, "Di0=%d, Dq0=%d, Di1=%d, Dq1=%d, m0=%d, m1=%d\n\n\n",
			(int8) ((txcal_bphy_coeffs[5] & 0xFF00) >> 8),
			(int8) (txcal_bphy_coeffs[5] & 0x00FF),
			(int8) ((txcal_bphy_coeffs[6] & 0xFF00) >> 8),
			(int8) (txcal_bphy_coeffs[6] & 0x00FF),
			(int8) ((txcal_bphy_coeffs[7] & 0xFF00) >> 8),
			(int8) (txcal_bphy_coeffs[7] & 0x00FF));

		bcm_bprintf(b, "Rx IQ/LO cal coeffs:\n");

		/* Rx calibration coefficients are 10-bit signed integers */
		if (rxcal_coeffs.a0 > 511)
			rxcal_coeffs.a0 -= 1024;

		if (rxcal_coeffs.b0 > 511)
			rxcal_coeffs.b0 -= 1024;

		if (rxcal_coeffs.a1 > 511)
			rxcal_coeffs.a1 -= 1024;

		if (rxcal_coeffs.b1 > 511)
			rxcal_coeffs.b1 -= 1024;

		bcm_bprintf(b, "a0=%d, b0=%d, a1=%d, b1=%d\n\n",
			rxcal_coeffs.a0, rxcal_coeffs.b0, rxcal_coeffs.a1, rxcal_coeffs.b1);

		if (NREV_GE(pi->pubpi.phy_rev, 3)) {
			bcm_bprintf(b, "RC CAL value: %d, %d\n", rccal_val[0], rccal_val[1]);
		}
	}

#ifdef WLMCHAN
	{
		if (!ISNPHY(pi))
			return;

		wlc_phydump_chanctx(pi, b);
	}
#endif

}


/* Dump rssi values from aci scans */
void
wlc_phydump_aci(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	int channel, index;
	int val;
	char *ptr;
	phy_info_t *pi = (phy_info_t *)ppi;

	val = pi->sh->interference_mode;
	if (pi->aci_state & ACI_ACTIVE)
		val |= AUTO_ACTIVE;

	if (val & AUTO_ACTIVE)
		bcm_bprintf(b, "ACI is Active\n");
	else {
		bcm_bprintf(b, "ACI is Inactive\n");
		return;
	}

	for (channel = 0; channel < ACI_LAST_CHAN; channel++) {
		bcm_bprintf(b, "Channel %d : ", channel + 1);
		for (index = 0; index < 50; index++) {
			ptr = (char *)(pi->interf.aci.rssi_buf) +
				channel * (ACI_SAMPLES + 1) + index;
			if (*ptr == (char)-1)
				break;
			bcm_bprintf(b, "%d ", *ptr);
		}
		bcm_bprintf(b, "\n");
	}
}

void
wlc_phydump_papd(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint32 val, i, j;
	int32 eps_real, eps_imag;
	phy_info_t *pi = (phy_info_t *)ppi;

	eps_real = eps_imag = 0;

	if (!pi->sh->up)
		return;

	if (!ISNPHY(pi))
		return;


	if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12)) {
		wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK,  MCTL_PHYLOCK);
		(void)R_REG(pi->sh->osh, &pi->regs->maccontrol);
		OSL_DELAY(1);
	}


	bcm_bprintf(b, "papd eps table:\n [core 0]\t\t[core 1] \n");
	for (j = 0; j < 64; j++) {
		for (i = 0; i < 2; i++) {
			wlc_phy_table_read_nphy(pi, ((i == 0) ? NPHY_TBL_ID_EPSILONTBL0 :
				NPHY_TBL_ID_EPSILONTBL1), 1, j, 32, &val);
			wlc_phy_papd_decode_epsilon(val, &eps_real, &eps_imag);
			bcm_bprintf(b, "{%d\t%d}\t\t", eps_real, eps_imag);
		}
		bcm_bprintf(b, "\n");
	}
	bcm_bprintf(b, "\n\n");

	if (D11REV_IS(pi->sh->corerev, 11) || D11REV_IS(pi->sh->corerev, 12))
		wlapi_bmac_mctrl(pi->sh->physhim, MCTL_PHYLOCK, 0);
}

void
wlc_phydump_noise(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint32 i, idx, antidx;
	int32 tot;
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!pi->sh->up)
		return;

	bcm_bprintf(b, "History and average of latest %d noise values:\n",
		PHY_NOISE_WINDOW_SZ);

	FOREACH_CORE(pi, antidx) {
		tot = 0;
		bcm_bprintf(b, "Ant%d: [", antidx);

		idx = pi->phy_noise_index;
		for (i = 0; i < PHY_NOISE_WINDOW_SZ; i++) {
			bcm_bprintf(b, "%4d", pi->phy_noise_win[antidx][idx]);
			tot += pi->phy_noise_win[antidx][idx];
			idx = MODINC_POW2(idx, PHY_NOISE_WINDOW_SZ);
		}
		bcm_bprintf(b, "]");

		tot /= PHY_NOISE_WINDOW_SZ;
		bcm_bprintf(b, " [%4d]\n", tot);
	}
}

void
wlc_phydump_state(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	char fraction[4][4] = {"  ", ".25", ".5", ".75"};
	int i;
	phy_info_t *pi = (phy_info_t *)ppi;

#define QDB_FRAC(x)	(x) / WLC_TXPWR_DB_FACTOR, fraction[(x) % WLC_TXPWR_DB_FACTOR]

	bcm_bprintf(b, "phy_type %d phy_rev %d ana_rev %d radioid 0x%x radiorev 0x%x\n",
	               pi->pubpi.phy_type, pi->pubpi.phy_rev, pi->pubpi.ana_rev,
	               pi->pubpi.radioid, pi->pubpi.radiorev);

	bcm_bprintf(b, "hw_power_control %d encore %d\n",
	               pi->hwpwrctrl, pi->pubpi.abgphy_encore);

	bcm_bprintf(b, "Power targets: ");

	bcm_bprintf(b, "\n\tCCK: ");
	for (i = TXP_FIRST_CCK; i < TXP_NUM_RATES; i++) {
		bcm_bprintf(b, "%2d%s ", QDB_FRAC(pi->tx_power_target[i]));
		if (i == TXP_LAST_CCK)	bcm_bprintf(b, "\n\tOFDM 20MHz SISO: ");
		if (i == TXP_LAST_OFDM)	bcm_bprintf(b, "\n\tOFDM 20MHz CDD: ");
		if (i == TXP_LAST_OFDM_20_CDD)	bcm_bprintf(b, "\n\tmcs0-7 20MHz SISO: ");
		if (i == TXP_LAST_MCS_20_SISO)	bcm_bprintf(b, "\n\tmcs0-7 20MHz CDD: ");
		if (i == TXP_LAST_MCS_20_CDD)	bcm_bprintf(b, "\n\tmcs0-7 20MHz STBC: ");
		if (i == TXP_LAST_MCS_20_STBC)	bcm_bprintf(b, "\n\tmcs8-15 20MHz SDM: ");
		if (i == TXP_LAST_MCS_20_SDM)	bcm_bprintf(b, "\n\tOFDM 40MHz SISO: ");
		if (i == TXP_LAST_OFDM_40_SISO)	bcm_bprintf(b, "\n\tOFDM 40MHz CDD: ");
		if (i == TXP_LAST_OFDM_40_CDD)	bcm_bprintf(b, "\n\tmcs0-7 40MHz SISO: ");
		if (i == TXP_LAST_MCS_40_SISO)	bcm_bprintf(b, "\n\tmcs0-7 40MHz CDD: ");
		if (i == TXP_LAST_MCS_40_CDD)	bcm_bprintf(b, "\n\tmcs0-7 40MHz STBC: ");
		if (i == TXP_LAST_MCS_40_STBC)	bcm_bprintf(b, "\n\tmcs8-15 40MHz SDM: ");
		if (i == TXP_LAST_MCS_40_SDM)	bcm_bprintf(b, "\n\tmcs32: ");
	}
	bcm_bprintf(b, "\n");

	if (ISNPHY(pi)) {
		bcm_bprintf(b, "antsel_type %d\n", pi->antsel_type);
		bcm_bprintf(b, "ipa2g %d ipa5g %d\n", pi->ipa2g_on, pi->ipa5g_on);

	} else if (ISLPPHY(pi) || ISSSLPNPHY(pi) || ISLCNPHY(pi)) {
		return;
	} else if (ISAPHY(pi)) {
		bcm_bprintf(b, "itssi low 0x%x, itssi mid 0x%x, itssi high 0x%x\n",
		               pi->idle_tssi[A_LOW_CHANS], pi->idle_tssi[A_MID_CHANS],
		               pi->idle_tssi[A_HIGH_CHANS]);

		bcm_bprintf(b, "Power limits low: ");
		for (i = 0; i <= TXP_LAST_OFDM; i++)
			bcm_bprintf(b, "%d ", pi->tx_srom_max_rate_5g_low[i]);

		bcm_bprintf(b, "\nPower limits mid: ");
		for (i = 0; i <= TXP_LAST_OFDM; i++)
			bcm_bprintf(b, "%d ", pi->tx_srom_max_rate_5g_mid[i]);

		bcm_bprintf(b, "\nPower limits hi: ");
		for (i = 0; i <= TXP_LAST_OFDM; i++)
			bcm_bprintf(b, "%d ", pi->tx_srom_max_rate_5g_hi[i]);
	} else {
		bcm_bprintf(b, "itssi 0x%x\n", pi->idle_tssi[G_ALL_CHANS]);
		bcm_bprintf(b, "Power limits: ");
		for (i = 0; i <= TXP_LAST_OFDM; i++)
			bcm_bprintf(b, "%d ", pi->tx_srom_max_rate_2g[i]);
	}

	bcm_bprintf(b, "\ninterference_mode %d intf_crs %d intf_crs_time %d crsglitch_prev %d\n",
		pi->sh->interference_mode, pi->interference_mode_crs,
		pi->interference_mode_crs_time, pi->crsglitch_prev);

}

void
wlc_phydump_lnagain(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	int core;
	uint16 lnagains[2][4];
	uint16 mingain[2];
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISNPHY(pi))
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);

	/* Now, read the gain table */
	for (core = 0; core < 2; core++) {
		wlc_phy_table_read_nphy(pi, core, 4, 8, 16, &lnagains[core][0]);
	}

	mingain[0] =
		(read_phy_reg(pi, NPHY_Core1MinMaxGain) &
		NPHY_CoreMinMaxGain_minGainValue_MASK) >>
		NPHY_CoreMinMaxGain_minGainValue_SHIFT;
	mingain[1] =
		(read_phy_reg(pi, NPHY_Core2MinMaxGain) &
		NPHY_CoreMinMaxGain_minGainValue_MASK) >>
		NPHY_CoreMinMaxGain_minGainValue_SHIFT;

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	bcm_bprintf(b, "Core 0: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
		lnagains[0][0], lnagains[0][1], lnagains[0][2], lnagains[0][3]);
	bcm_bprintf(b, "Core 1: 0x%02x, 0x%02x, 0x%02x, 0x%02x\n\n",
		lnagains[1][0], lnagains[1][1], lnagains[1][2], lnagains[1][3]);
	bcm_bprintf(b, "Min Gain: Core 0=0x%02x,   Core 1=0x%02x\n\n",
		mingain[0], mingain[1]);
}

void
wlc_phydump_initgain(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint8 ctr;
	uint16 regval[2], tblregval[4];
	uint16 lna_gain[2], hpvga1_gain[2], hpvga2_gain[2];
	uint16 tbl_lna_gain[4], tbl_hpvga1_gain[4], tbl_hpvga2_gain[4];
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISNPHY(pi))
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);

	regval[0] = read_phy_reg(pi, NPHY_Core1InitGainCode);
	regval[1] = read_phy_reg(pi, NPHY_Core2InitGainCode);

	wlc_phy_table_read_nphy(pi, 7, pi->pubpi.phy_corenum, 0x106, 16, tblregval);

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	lna_gain[0] = (regval[0] & NPHY_CoreInitGainCode_initLnaIndex_MASK) >>
		NPHY_CoreInitGainCode_initLnaIndex_SHIFT;
	hpvga1_gain[0] = (regval[0] & NPHY_CoreInitGainCode_initHpvga1Index_MASK) >>
		NPHY_CoreInitGainCode_initHpvga1Index_SHIFT;
	hpvga2_gain[0] = (regval[0] & NPHY_CoreInitGainCode_initHpvga2Index_MASK) >>
		NPHY_CoreInitGainCode_initHpvga2Index_SHIFT;


	lna_gain[1] = (regval[1] & NPHY_CoreInitGainCode_initLnaIndex_MASK) >>
		NPHY_CoreInitGainCode_initLnaIndex_SHIFT;
	hpvga1_gain[1] = (regval[1] & NPHY_CoreInitGainCode_initHpvga1Index_MASK) >>
		NPHY_CoreInitGainCode_initHpvga1Index_SHIFT;
	hpvga2_gain[1] = (regval[1] & NPHY_CoreInitGainCode_initHpvga2Index_MASK) >>
		NPHY_CoreInitGainCode_initHpvga2Index_SHIFT;

	for (ctr = 0; ctr < 4; ctr++) {
		tbl_lna_gain[ctr] = (tblregval[ctr] >> 2) & 0x3;
	}

	for (ctr = 0; ctr < 4; ctr++) {
		tbl_hpvga1_gain[ctr] = (tblregval[ctr] >> 4) & 0xf;
	}

	for (ctr = 0; ctr < 4; ctr++) {
		tbl_hpvga2_gain[ctr] = (tblregval[ctr] >> 8) & 0x1f;
	}

	bcm_bprintf(b, "Core 0 INIT gain: HPVGA2=%d, HPVGA1=%d, LNA=%d\n",
		hpvga2_gain[0], hpvga1_gain[0], lna_gain[0]);
	bcm_bprintf(b, "Core 1 INIT gain: HPVGA2=%d, HPVGA1=%d, LNA=%d\n",
		hpvga2_gain[1], hpvga1_gain[1], lna_gain[1]);
	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "INIT gain table:\n");
	bcm_bprintf(b, "----------------\n");
	for (ctr = 0; ctr < 4; ctr++) {
		bcm_bprintf(b, "Core %d: HPVGA2=%d, HPVGA1=%d, LNA=%d\n",
			ctr, tbl_hpvga2_gain[ctr], tbl_hpvga1_gain[ctr], tbl_lna_gain[ctr]);
	}

}

void
wlc_phydump_hpf1tbl(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint8 ctr, core;
	uint16 gain[2][NPHY_MAX_HPVGA1_INDEX+1];
	uint16 gainbits[2][NPHY_MAX_HPVGA1_INDEX+1];
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISNPHY(pi))
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, TRUE);

	/* Read from the HPVGA1 gaintable */
	wlc_phy_table_read_nphy(pi, 0, NPHY_MAX_HPVGA1_INDEX, 16, 16, &gain[0][0]);
	wlc_phy_table_read_nphy(pi, 1, NPHY_MAX_HPVGA1_INDEX, 16, 16, &gain[1][0]);
	wlc_phy_table_read_nphy(pi, 2, NPHY_MAX_HPVGA1_INDEX, 16, 16, &gainbits[0][0]);
	wlc_phy_table_read_nphy(pi, 3, NPHY_MAX_HPVGA1_INDEX, 16, 16, &gainbits[1][0]);

	if (pi->phyhang_avoid)
		wlc_phy_stay_in_carriersearch_nphy(pi, FALSE);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	for (core = 0; core < 2; core++) {
		bcm_bprintf(b, "Core %d gain: ", core);
		for (ctr = 0; ctr <= NPHY_MAX_HPVGA1_INDEX; ctr++)  {
			bcm_bprintf(b, "%2d ", gain[core][ctr]);
		}
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "\n");
	for (core = 0; core < 2; core++) {
		bcm_bprintf(b, "Core %d gainbits: ", core);
		for (ctr = 0; ctr <= NPHY_MAX_HPVGA1_INDEX; ctr++)  {
			bcm_bprintf(b, "%2d ", gainbits[core][ctr]);
		}
		bcm_bprintf(b, "\n");
	}
}

void
wlc_phydump_lpphytbl0(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint16 val16;
	lpphytbl_info_t tab;
	int i;
	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISLPPHY(pi))
		return;

	for (i = 0; i < 108; i ++) {
		tab.tbl_ptr = &val16;	/* ptr to buf */
		tab.tbl_len = 1;	/* # values */
		tab.tbl_id = 0;		/* iqloCaltbl */
		tab.tbl_offset = i;	/* tbl offset */
		tab.tbl_width = 16;	/* 16 bit wide */
		tab.tbl_phywidth = 16; /* width of table element in phy address space */
		wlc_phy_table_read_lpphy(pi, &tab);
		bcm_bprintf(b, "%i:\t%04x\n", i, val16);
	}
}

void
wlc_phydump_chanest(wlc_phy_t *ppi, struct bcmstrbuf *b)
{
	uint8 num_rx, num_sts, num_tones;
	uint32 ch;
	uint16 ch_re_ma, ch_im_ma;
	uint8 ch_re_si, ch_im_si;
	int16 ch_re, ch_im;
	int8 ch_exp;
	uint8 k, r, t, fftk;

	phy_info_t *pi = (phy_info_t *)ppi;

	if (!ISHTPHY(pi))
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	/* Go deaf to prevent PHY channel writes while doing reads */
	wlc_phy_stay_in_carriersearch_htphy(pi, TRUE);

	num_rx = (uint8)pi->pubpi.phy_corenum;
	num_sts = 4;
	num_tones = (CHSPEC_IS40(pi->radio_chanspec) ? 128 : 64);

	for (k = 0; k < num_tones; k++) {
		for (r = 0; r < num_rx; r++) {
			for (t = 0; t < num_sts; t++) {
				wlc_phy_table_read_htphy(pi, HTPHY_TBL_ID_CHANEST(r), 1,
				                         t*128 + k, 32, &ch);
				ch_re_ma  = ((ch >> 18) & 0x7ff);
				ch_re_si  = ((ch >> 29) & 0x001);
				ch_im_ma  = ((ch >>  6) & 0x7ff);
				ch_im_si  = ((ch >> 17) & 0x001);
				ch_exp    = ((int8)((ch << 2) & 0xfc)) >> 2;

				ch_re = (ch_re_si > 0) ? -ch_re_ma : ch_re_ma;
				ch_im = (ch_im_si > 0) ? -ch_im_ma : ch_im_ma;

				fftk = ((k < num_tones/2) ? (k + num_tones/2) : (k - num_tones/2));

				bcm_bprintf(b, "chan(%d, %d, %d) = %d*2^%d + i*%d*2^%d;\n",
				            r+1, t+1, fftk+1, ch_re, ch_exp, ch_im, ch_exp);
			}
		}
	}

	/* Return from deaf */
	wlc_phy_stay_in_carriersearch_htphy(pi, FALSE);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}
#endif /* BCMDBG || BCMDBG_DUMP */

const uint8 *
wlc_phy_get_ofdm_rate_lookup(void)
{
	return ofdm_rate_lookup;
}

/* LPCONF || SSLPNCONF */
void
wlc_phy_radio_2063_vco_cal(phy_info_t *pi)
{
	uint8 calnrst;

	/* Power up VCO cal clock */
	mod_radio_reg(pi, RADIO_2063_PLL_SP_1, 1 << 6, 0);

	calnrst = read_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST) & 0xf8;
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x04);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x06);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x07);
#if defined(DSLCPE_DELAY)
	OSL_YIELD_EXEC(pi->sh->osh, 300);
#else
	OSL_DELAY(300);
#endif

	/* Power down VCO cal clock */
	mod_radio_reg(pi, RADIO_2063_PLL_SP_1, 1 << 6, 1 << 6);
}
void
wlc_lcnphy_epa_switch(phy_info_t *pi, bool mode)
{
	if ((CHIPID(pi->sh->chip) == BCM4313_CHIP_ID) &&
		(pi->sh->boardflags & BFL_FEM)) {
		if (mode) {
			uint16 txant = 0;
			txant = wlapi_bmac_get_txant(pi->sh->physhim);
			if (txant == 1) {
				MOD_PHY_REG(pi, LCNPHY, RFOverrideVal0, ant_selp_ovr_val, 1);
				MOD_PHY_REG(pi, LCNPHY, RFOverride0, ant_selp_ovr, 1);
			}
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpiocontrol),
				~0x0, 0x0);
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpioout),
				0x40, 0x40);
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpioouten),
				0x40, 0x40);
		} else {
			MOD_PHY_REG(pi, LCNPHY, RFOverride0, ant_selp_ovr, 0);
			MOD_PHY_REG(pi, LCNPHY, RFOverrideVal0, ant_selp_ovr_val, 0);
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpioout),
				0x40, 0x00);
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, gpioouten),
				0x40, 0x0);
			si_corereg(pi->sh->sih, SI_CC_IDX,
				OFFSETOF(chipcregs_t, gpiocontrol), ~0x0, 0x40);
		}
	}
}

static int8
wlc_user_txpwr_antport_to_rfport(phy_info_t *pi, uint chan, uint32 band, uint8 rate)
{
	int8 offset = 0;

	if (!pi->user_txpwr_at_rfport)
		return offset;
#ifdef WLNOKIA_NVMEM
	offset += wlc_phy_noknvmem_antport_to_rfport_offset(pi, chan, band, rate);
#endif 
	return offset;
}

/* vbat measurement hook. VBAT is in volts Q4.4 */
static int8
wlc_phy_env_measure_vbat(phy_info_t *pi)
{
	if (ISLCNPHY(pi))
		return wlc_lcnphy_vbatsense(pi, 0);
	else
		return 0;
}

/* temperature measurement hook */
static int8
wlc_phy_env_measure_temperature(phy_info_t *pi)
{
	if (ISLCNPHY(pi))
		return wlc_lcnphy_tempsense_degree(pi, 0);
	else
		return 0;
}


/* get the per rate limits based on vbat and temp */
static void
wlc_phy_upd_env_txpwr_rate_limits(phy_info_t *pi, uint32 band)
{
	uint8 i;
	int8 temp, vbat;

	for (i = 0; i < TXP_NUM_RATES; i++)
		pi->txpwr_env_limit[i] = WLC_TXPWR_MAX;

	vbat = wlc_phy_env_measure_vbat(pi);
	temp = wlc_phy_env_measure_temperature(pi);

#ifdef WLNOKIA_NVMEM
	/* check the nvram settings and see if there is support for vbat/temp based rate limits */
	wlc_phy_noknvmem_env_txpwrlimit_upd(pi, vbat, temp, band);
#endif 
}

void
wlc_phy_ldpc_override_set(wlc_phy_t *ppi, bool ldpc)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	if (ISHTPHY(pi))
		wlc_phy_update_rxldpc_htphy(pi, ldpc);
	return;
}

void
wlc_phy_tkip_rifs_war(wlc_phy_t *ppi, uint8 rifs)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	if (ISNPHY(pi)) {
		wlc_phy_nphy_tkip_rifs_war(pi, rifs);
	}
}

void
wlc_phy_get_pwrdet_offsets(phy_info_t *pi, int8 *cckoffset, int8 *ofdmoffset)
{
	*cckoffset = 0;
	*ofdmoffset = 0;
#ifdef WLNOKIA_NVMEM
	if (ISLCNPHY(pi))
		wlc_phy_noknvmem_get_pwrdet_offsets(pi, cckoffset, ofdmoffset);
#endif /* WLNOKIA_NVMEM */
}

uint32
wlc_phy_qdiv_roundup(uint32 dividend, uint32 divisor, uint8 precision)
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
/* update the cck power detector offset */
int8
wlc_phy_upd_rssi_offset(phy_info_t *pi, int8 rssi, chanspec_t chanspec)
{

#ifdef WLNOKIA_NVMEM
	if (rssi != WLC_RSSI_INVALID)
		rssi = wlc_phy_noknvmem_modify_rssi(pi, rssi, chanspec);
#endif 
	return rssi;
}

bool
wlc_phy_txpower_ipa_ison(wlc_phy_t *ppi)
{
	phy_info_t *pi = (phy_info_t *)ppi;

	if (ISNPHY(pi))
		return (wlc_phy_n_txpower_ipa_ison(pi));
	else
		return 0;
}

#if defined(WLMCHAN) || defined(PHYCAL_CACHING)
int
wlc_phy_create_chanctx(wlc_phy_t *ppi, chanspec_t chanspec)
{
	ch_calcache_t *ctx;
	phy_info_t *pi = (phy_info_t *)ppi;

	/* Check for existing */
	if (wlc_phy_get_chanctx(pi, chanspec))
		return 0;

	if (!(ctx = (ch_calcache_t *)MALLOC(pi->sh->osh, sizeof(ch_calcache_t)))) {
		PHY_ERROR(("%s: out of memory %d\n", __FUNCTION__, MALLOCED(pi->sh->osh)));
		return BCME_NOMEM;
	}
	bzero(ctx, sizeof(ch_calcache_t));

	ctx->chanspec = chanspec;
	ctx->cal_info.last_cal_temp = -50;
	ctx->cal_info.txcal_numcmds = pi->def_cal_info.txcal_numcmds;

	/* Add it to the list */
	ctx->next = pi->calcache;

	/* For the first context, switch out the default context */
	if (pi->calcache == NULL &&
	    (pi->radio_chanspec == chanspec))
		pi->cal_info = &ctx->cal_info;

	pi->calcache = ctx;
	return 0;
}

void
wlc_phy_destroy_chanctx(wlc_phy_t *ppi, chanspec_t chanspec)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	ch_calcache_t *ctx = pi->calcache, *rem = pi->calcache;

	while (rem) {
		if (rem->chanspec == chanspec) {
			if (rem == pi->calcache)
				pi->calcache = rem->next;
			else
				ctx->next = rem->next;

			/* If the current cal_info points to the one being removed
			 * then switch NULL it
			 */
			if (pi->cal_info == &rem->cal_info)
				pi->cal_info = NULL;

			MFREE(pi->sh->osh, rem,
			      sizeof(ch_calcache_t));
			rem = NULL;
			break;
		}
		ctx = rem;
		rem = rem->next;
	}

	/* Set the correct context if one exists, otherwise,
	 * switch in the default one
	 */
	if (pi->cal_info == NULL) {
		ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
		if (!ctx) {
			pi->cal_info = &pi->def_cal_info;
			/* Reset the parameters */
			pi->cal_info->last_cal_temp = -50;
			pi->cal_info->last_cal_time = 0;
		} else
			pi->cal_info = &ctx->cal_info;
	}
}

ch_calcache_t *
wlc_phy_get_chanctx(phy_info_t *phi, chanspec_t chanspec)
{
	ch_calcache_t *ctx = phi->calcache;
	while (ctx) {
		if (ctx->chanspec == chanspec)
			return ctx;
		ctx = ctx->next;
	}
	return NULL;
}

#ifdef BCMDBG
static void
wlc_phydump_chanctx(phy_info_t *phi, struct bcmstrbuf *b)
{
	ch_calcache_t *ctx = phi->calcache;
	bcm_bprintf(b, "Current chanspec: 0x%x\n", phi->radio_chanspec);
	while (ctx) {
		if (ISNPHY(phi)) {
			bcm_bprintf(b, "%sContext found for chanspec: 0x%x\n",
			            (ctx->valid)? "Valid ":"",
			            ctx->chanspec);
			wlc_phydump_cal_cache_nphy(phi, ctx, b);
		}
		ctx = ctx->next;
	}
}
#endif /* BCMDBG */
#endif /* WLMCHAN */
