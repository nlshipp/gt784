/*
 * Linux OS Independent Layer
 *
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: linux_osl_dslcpe.h,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#ifndef _linux_osl_dslcpe_h_
#define _linux_osl_dslcpe_h_


#ifdef DSLCPE_SDIO_EBIDMA
    #define SDIOH_R_REG(r) ( \
        sizeof(*(r)) == sizeof(uint8) ? readb((volatile uint8*)(r)) : \
        sizeof(*(r)) == sizeof(uint16) ? readw((volatile uint16*)(r)) : \
        readl((volatile uint32*)(r)) \
    )

    #define SDIOH_W_REG(r, v) do { \
        switch (sizeof(*(r))) { \
        case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
        case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
        case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
        } \
    } while (0)

    #define HOST_R_REG			SDIOH_R_REG
    #define HOST_W_REG			SDIOH_W_REG
    #define	HOST_AND_REG(r, v)	HOST_W_REG((r), HOST_R_REG(r) & (v))
    #define	HOST_OR_REG(r, v)	HOST_W_REG((r), HOST_R_REG(r) | (v))
    #undef R_SM
    #undef W_SM
    #define R_SM				HOST_R_REG
    #define W_SM				HOST_W_REG
#endif

#undef PKTPRIO
#undef PKTSETPRIO
#define	PKTPRIO(skb)			osl_pktprio((skb))
#define	PKTSETPRIO(skb, x)		osl_pktsetprio((skb), (x))
extern uint osl_pktprio(void *skb);
extern void osl_pktsetprio(void *skb, uint x);
extern int  osl_pktq_len(osl_t *osh);
extern bool osl_pktq_wmark_low_drop(osl_t *osh);
extern bool osl_pktq_wmark_high_drop(osl_t *osh);
extern void osl_pktpreallocinc(osl_t *osh, void *skb);

#define PRIO_LOC_NFMARK 16
#define PKT_PREALLOCINC(osh, skb) osl_pktpreallocinc((osh), (skb))	

#endif	/* _linux_osl_dslcpe_h_ */
