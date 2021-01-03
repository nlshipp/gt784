/*
 * HND SiliconBackplane MIPS/ARM hardware description.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: sbhndcpu.h,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#ifndef _sbhndcpu_h_
#define _sbhndcpu_h_

#if defined(mips)
#include <mips33_core.h>
#include <mips74k_core.h>
#elif defined(__arm__) || defined(__thumb__) || defined(__thumb2__)
#include <sbhndarm.h>
#endif

#endif /* _sbhndcpu_h_ */
