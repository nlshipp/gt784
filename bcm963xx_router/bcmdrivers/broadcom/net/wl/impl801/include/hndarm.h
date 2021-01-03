/*
 * HND SiliconBackplane ARM core software interface.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: hndarm.h,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#ifndef _hndarm_h_
#define _hndarm_h_

#include <sbhndarm.h>

extern void *hndarm_armr;
extern uint32 hndarm_rev;


extern void si_arm_init(si_t *sih);

extern void enable_arm_ints(uint32 which);
extern void disable_arm_ints(uint32 which);

extern uint32 get_arm_cyclecount(void);
extern void set_arm_cyclecount(uint32 ticks);

extern uint32 get_arm_inttimer(void);
extern void set_arm_inttimer(uint32 ticks);

extern uint32 get_arm_intmask(void);
extern void set_arm_intmask(uint32 ticks);

extern uint32 get_arm_intstatus(void);
extern void set_arm_intstatus(uint32 ticks);

extern void arm_wfi(si_t *sih);
extern void arm_jumpto(void *addr);

#endif /* _hndarm_h_ */
