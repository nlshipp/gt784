/*
 * Interface definitions for DES.
 * Copied from des-ka9q-1.0-portable. a public domain DES implementation.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: des.h,v 1.1 2010/08/05 21:58:58 ywu Exp $
 */

#ifndef _DES_H_
#define _DES_H_

typedef unsigned long DES_KS[16][2];	/* Single-key DES key schedule */

void BCMROMFN(deskey)(DES_KS, unsigned char *, int);

void BCMROMFN(des)(DES_KS, unsigned char *);

#endif /* _DES_H_ */
