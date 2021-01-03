/*
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: si_dslcpe.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pci_core.h>
#include <pcie_core.h>
#include <nicpci.h>
#include <bcmnvram.h>
#include <bcmsrom.h>
#include <hndtcam.h>
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsocram.h>
#ifdef BCMECICOEX
#include <bcmotp.h>
#endif /* BCMECICOEX */
#include <hndpmu.h>
#ifdef BCMSPI
#include <spid.h>
#endif /* BCMSPI */

#include "siutils_priv.h"

/* DSLCPE headers */
#include <board.h>
#include <boardparms.h>


#ifdef SI_SPROM_PROBE 
#include <bcmsrom_fmt.h>
extern int wl_probe_srom(si_t *sih, osl_t *osh, void *curmap);
extern int BCMATTACHFN(init_srom_sw_map)(si_t *sih, uint chipId, void *buf, uint nbytes);

char wl_srom_sw_map[SROM_MAX] = {0};
extern int wl_srom_present;
#endif /* SI_SPROM_PROBE */

#ifdef SI_ENUM_BASE_VARIABLE
void si_enum_base_init(si_t *sih, uint bustype)
{
	si_info_t *sii = SI_INFO(sih);
	if (bustype == PCI_BUS) {
		if ((OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_VID, sizeof(uint32)) >> 16)
		   == BCM6362_D11N_ID) {
			SI_ENUM_BASE = 0x10004000; //sii->pub.si_enum_base = 0x10004000;
		} else {
			SI_ENUM_BASE = 0x18000000; //sii->pub.si_enum_base = 0x18000000;
		}
	}
}
#endif /* SI_ENUM_BASE_VARIABLE */

static bool si_is_rom_exist(si_t *sih)
{
	/* this deals with chip design, not board/strap settings */
	/* chips without WLAN OTP or SROM return FALSE */
	switch (CHIPID(sih->chip)) {	
		case BCM6362_CHIP_ID:	
			return FALSE;
		default:
			return TRUE;
	}		
}
#ifdef SI_SPROM_PROBE 
void si_sprom_init(si_t *sih)
{
	si_info_t *sii = SI_INFO(sih);
	unsigned short wlflags = 0;
	
	sii->pub.wl_srom_present = wl_probe_srom(&sii->pub, sii->osh, sii->curmap);
	
	/* override by boardparams flag, purpose is to intentionally use OTP but not srom map */
	if ((BpGetWirelessFlags(&wlflags) == BP_SUCCESS) && (wlflags&BP_WLAN_USE_OTP) && si_is_rom_exist(sih)) {
		sii->pub.wl_srom_present = TRUE;
	}
	
	/* wl_srom_present is used for SDIO interface. This temporary change is to 
	*  eliminate compiling error for SDIO interface(such as 6338). 
	* WOMBO handing for SDIO may need to be re-organize. Both for SDIO and PCI
	* interface should use the same variable
	*/
	wl_srom_present = sii->pub.wl_srom_present;

	if (!sii->pub.wl_srom_present) {
		printk("wl:srom not detected,"
			 "using main memory mapped srom info(wombo board)\n");
		/* read in srom files during attach time */
		if (BUSTYPE(sii->pub.bustype) == SDIO_BUS)
			init_srom_sw_map(&sii->pub, sih->chip,
				wl_srom_sw_map, sizeof(wl_srom_sw_map));
		else
			init_srom_sw_map(&sii->pub, sih->chip,
				sii->pub.wl_srom_sw_map, sizeof(sii->pub.wl_srom_sw_map));
	}
}
#endif /* SI_SPROM_PROBE */
