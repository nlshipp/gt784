/*
 * Microcode declarations for Broadcom 802.11abg
 * Networking Adapter Device Driver.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: d11ucode.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */

/* ucode and inits structure */
typedef struct d11init {
	uint16	addr;
	uint16	size;
	uint32	value;
} d11init_t;

/* ucode and inits */
extern CONST uint32 d11ucode4[];
extern CONST uint d11ucode4sz;
extern CONST uint32 d11ucode5[];
extern CONST uint d11ucode5sz;
#if defined(MBSS)
extern CONST uint32 d11ucode9[];
extern CONST uint d11ucode9sz;
#endif 
extern CONST uint32 d11ucode11[], d11ucode_2w11[];
extern CONST uint d11ucode11sz, d11ucode_2w11sz;
extern CONST uint32 d11ucode13[], d11ucode_2w13[];
extern CONST uint d11ucode13sz, d11ucode_2w13sz;
extern CONST uint32 d11ucode14[];
extern CONST uint d11ucode14sz;
extern CONST uint32 d11ucode15[], d11ucode_2w15[];
extern CONST uint d11ucode15sz, d11ucode_2w15sz;
extern CONST uint32 d11ucode16_lp[];
extern CONST uint d11ucode16_lpsz;
extern CONST uint8 d11ucode16_sslpn[];
extern CONST uint d11ucode16_sslpnsz;
extern CONST uint8 d11ucode16_sslpn_nobt[];
extern CONST uint d11ucode16_sslpn_nobtsz;
extern CONST uint32 d11ucode16_mimo[];
extern CONST uint d11ucode16_mimosz;
extern CONST uint32 d11ucode17[];
extern CONST uint d11ucode17sz;
extern CONST uint32 d11ucode19_lp[];
extern CONST uint d11ucode19_lpsz;
extern CONST uint8 d11ucode19_sslpn[];
extern CONST uint d11ucode19_sslpnsz;
extern CONST uint8 d11ucode19_sslpn_nobt[];
extern CONST uint d11ucode19_sslpn_nobtsz;
extern CONST uint32 d11ucode19_mimo[];
extern CONST uint d11ucode19_mimosz;
extern CONST uint32 d11ucode20_lp[];
extern CONST uint d11ucode20_lpsz;
extern CONST uint8 d11ucode20_sslpn[];
extern CONST uint d11ucode20_sslpnsz;
extern CONST uint8 d11ucode20_sslpn_nobt[];
extern CONST uint d11ucode20_sslpn_nobtsz;
extern CONST uint32 d11ucode20_mimo[];
extern CONST uint d11ucode20_mimosz;
extern CONST uint8 d11ucode21_sslpn_nobt[];
extern CONST uint8 d11ucode21_sslpn[];
extern CONST uint d11ucode21_sslpn_nobtsz;
extern CONST uint d11ucode21_sslpnsz;
extern CONST uint32 d11ucode22_mimo[];
extern CONST uint d11ucode22_mimosz;
extern CONST uint8 d11ucode22_sslpn[];
extern CONST uint d11ucode22_sslpnsz;
extern CONST uint32 d11ucode24_mimo[];
extern CONST uint d11ucode24_mimosz;
extern CONST uint8 d11ucode24_lcn[];
extern CONST uint d11ucode24_lcnsz;
extern CONST uint32 d11ucode25_mimo[];
extern CONST uint d11ucode25_mimosz;
extern CONST uint8 d11ucode25_lcn[];
extern CONST uint d11ucode25_lcnsz;
extern CONST uint32 d11ucode26_mimo[];
extern CONST uint d11ucode26_mimosz;
extern CONST uint32 d11ucode30_mimo[];
extern CONST uint d11ucode30_mimosz;

extern CONST uint32 d11pcm4[];
extern CONST uint d11pcm4sz;
extern CONST uint32 d11pcm5[];
extern CONST uint d11pcm5sz;

extern CONST d11init_t d11b0g0initvals4[];
extern CONST d11init_t d11b0g0bsinitvals4[];
extern CONST d11init_t d11a0g0initvals4[];
extern CONST d11init_t d11a0g0bsinitvals4[];
extern CONST d11init_t d11b0g0initvals5[];
extern CONST d11init_t d11b0g0bsinitvals5[];
extern CONST d11init_t d11a0g0initvals5[];
extern CONST d11init_t d11a0g1initvals5[];
extern CONST d11init_t d11a0g0bsinitvals5[];
extern CONST d11init_t d11a0g1bsinitvals5[];
#if defined(MBSS)
extern CONST d11init_t d11b0g0initvals9[];
extern CONST d11init_t d11b0g0bsinitvals9[];
extern CONST d11init_t d11a0g0initvals9[];
extern CONST d11init_t d11a0g1initvals9[];
extern CONST d11init_t d11a0g0bsinitvals9[];
extern CONST d11init_t d11a0g1bsinitvals9[];
#endif 
extern CONST d11init_t d11n0initvals11[];
extern CONST d11init_t d11n0bsinitvals11[];
extern CONST d11init_t d11lp0initvals13[];
extern CONST d11init_t d11lp0bsinitvals13[];
extern CONST d11init_t d11lp0initvals14[];
extern CONST d11init_t d11lp0bsinitvals14[];
extern CONST d11init_t d11b0g0initvals13[];
extern CONST d11init_t d11b0g0bsinitvals13[];
extern CONST d11init_t d11a0g1initvals13[];
extern CONST d11init_t d11a0g1bsinitvals13[];
extern CONST d11init_t d11lp0initvals15[];
extern CONST d11init_t d11lp0bsinitvals15[];
extern CONST d11init_t d11n0initvals16[];
extern CONST d11init_t d11n0bsinitvals16[];
extern CONST d11init_t d11sslpn0initvals16[];
extern CONST d11init_t d11sslpn0bsinitvals16[];
extern CONST d11init_t d11lp0initvals16[];
extern CONST d11init_t d11lp0bsinitvals16[];
extern CONST d11init_t d11sslpn2initvals17[];
extern CONST d11init_t d11sslpn2bsinitvals17[];
extern CONST d11init_t d11n2initvals19[];
extern CONST d11init_t d11n2bsinitvals19[];
extern CONST d11init_t d11sslpn2initvals19[];
extern CONST d11init_t d11sslpn2bsinitvals19[];
extern CONST d11init_t d11lp2initvals19[];
extern CONST d11init_t d11lp2bsinitvals19[];
extern CONST d11init_t d11n1initvals20[];
extern CONST d11init_t d11n1bsinitvals20[];
extern CONST d11init_t d11sslpn1initvals20[];
extern CONST d11init_t d11sslpn1bsinitvals20[];
extern CONST d11init_t d11lp1initvals20[];
extern CONST d11init_t d11lp1bsinitvals20[];
extern CONST d11init_t d11sslpn3initvals21[];
extern CONST d11init_t d11sslpn3bsinitvals21[];
extern CONST d11init_t d11n0initvals22[];
extern CONST d11init_t d11n0bsinitvals22[];
extern CONST d11init_t d11sslpn4initvals22[];
extern CONST d11init_t d11sslpn4bsinitvals22[];
extern CONST d11init_t d11n0initvals24[];
extern CONST d11init_t d11n0bsinitvals24[];
extern CONST d11init_t d11lcn0initvals24[];
extern CONST d11init_t d11lcn0bsinitvals24[];
extern CONST d11init_t d11n0initvals25[];
extern CONST d11init_t d11n0bsinitvals25[];
extern CONST d11init_t d11n16initvals30[];
extern CONST d11init_t d11n16bsinitvals30[];
extern CONST d11init_t d11lcn0initvals25[];
extern CONST d11init_t d11lcn0bsinitvals25[];
extern CONST d11init_t d11ht0initvals26[];
extern CONST d11init_t d11ht0bsinitvals26[];

extern CONST d11init_t d11waken0initvals12[];
extern CONST d11init_t d11waken0bsinitvals12[];
extern CONST uint32 d11aeswakeucode12[];
extern CONST uint32 d11wakeucode12[];
extern CONST uint d11wakeucode12sz;
extern CONST uint d11aeswakeucode12sz;
extern CONST d11init_t d11wakelp0initvals15[];
extern CONST d11init_t d11wakelp0bsinitvals15[];
extern CONST uint32 d11aeswakeucode15[];
extern CONST uint32 d11wakeucode15[];
extern CONST uint d11wakeucode15sz;
extern CONST uint d11aeswakeucode15sz;
extern CONST d11init_t d11waken0initvals16[];
extern CONST d11init_t d11waken0bsinitvals16[];
extern CONST uint32 d11aeswakeucode16_lp[];
extern CONST uint32 d11aeswakeucode16_sslpn[];
extern CONST uint32 d11aeswakeucode16_mimo[];
extern CONST uint32 d11wakeucode16_lp[];
extern CONST uint32 d11wakeucode16_sslpn[];
extern CONST uint32 d11wakeucode16_mimo[];
extern CONST uint d11wakeucode16_lpsz;
extern CONST uint d11wakeucode16_sslpnsz;
extern CONST uint d11wakeucode16_mimosz;
extern CONST uint d11aeswakeucode16_lpsz;
extern CONST uint d11aeswakeucode16_sslpnsz;
extern CONST uint d11aeswakeucode16_mimosz;


#ifdef SAMPLE_COLLECT
extern CONST uint32 d11sampleucode16[];
extern CONST uint d11sampleucode16sz;
#endif	/* defined(SAMPLE_COLLECT) */

/* BOM info for each ucode file */
extern CONST uint32 d11ucode_ge24_bommajor;
extern CONST uint32 d11ucode_ge24_bomminor;
extern CONST uint32 d11ucode_gt15_bommajor;
extern CONST uint32 d11ucode_gt15_bomminor;
extern CONST uint32 d11ucode_le15_bommajor;
extern CONST uint32 d11ucode_le15_bomminor;
extern CONST uint32 d11ucode_2w_bommajor;
extern CONST uint32 d11ucode_2w_bomminor;
extern CONST uint32 d11wakeucode_bommajor;
extern CONST uint32 d11wakeucode_bomminor;
extern CONST uint32 locator_ucode_bommajor;
extern CONST uint32 locator_ucode_bomminor;

#ifdef WLP2P
extern CONST uint32 d11p2pucode_bommajor;
extern CONST uint32 d11p2pucode_bomminor;
extern CONST uint32 d11p2pucode16_lp[];
extern CONST uint d11p2pucode16_lpsz;
extern CONST uint8 d11p2pucode16_sslpn[];
extern CONST uint d11p2pucode16_sslpnsz;
extern CONST uint8 d11p2pucode16_sslpn_nobt[];
extern CONST uint d11p2pucode16_sslpn_nobtsz;
extern CONST uint32 d11p2pucode16_mimo[];
extern CONST uint d11p2pucode16_mimosz;
extern CONST uint32 d11p2pucode17_mimo[];
extern CONST uint d11p2pucode17_mimosz;
extern CONST uint8 d11p2pucode19_sslpn[];
extern CONST uint d11p2pucode19_sslpnsz;
extern CONST uint8 d11p2pucode19_sslpn_nobt[];
extern CONST uint d11p2pucode19_sslpn_nobtsz;
extern CONST uint8 d11p2pucode20_sslpn[];
extern CONST uint d11p2pucode20_sslpnsz;
extern CONST uint8 d11p2pucode20_sslpn_nobt[];
extern CONST uint d11p2pucode20_sslpn_nobtsz;
extern CONST uint8 d11p2pucode21_sslpn[];
extern CONST uint d11p2pucode21_sslpnsz;
extern CONST uint8 d11p2pucode21_sslpn_nobt[];
extern CONST uint d11p2pucode21_sslpn_nobtsz;
extern CONST uint8 d11p2pucode22_sslpn[];
extern CONST uint d11p2pucode22_sslpnsz;
extern CONST uint32 d11p2pucode22_mimo[];
extern CONST uint d11p2pucode22_mimosz;
extern CONST uint32 d11p2pucode24_mimo[];
extern CONST uint d11p2pucode24_mimosz;
extern CONST uint8 d11p2pucode24_lcn[];
extern CONST uint d11p2pucode24_lcnsz;
extern CONST uint32 d11p2pucode25_mimo[];
extern CONST uint d11p2pucode25_mimosz;
extern CONST uint8 d11p2pucode25_lcn[];
extern CONST uint d11p2pucode25_lcnsz;
extern CONST uint32 d11p2pucode26_mimo[];
extern CONST uint d11p2pucode26_mimosz;
extern CONST uint8 d11p2pucode26_lcn[];
extern CONST uint d11p2pucode26_lcnsz;
#endif /* WLP2P */
