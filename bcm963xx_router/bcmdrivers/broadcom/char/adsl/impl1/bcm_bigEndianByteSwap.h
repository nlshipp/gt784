/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_bigEndianByteSwap.h                                */
/*   Purpose         : Header for the big endian byte swap function           */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 13-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_bigEndianByteSwap.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef BIG_ENDIAN_BYTE_SWAP_H
# define BIG_ENDIAN_BYTE_SWAP_H

# include "bcm_userdef.h"
# include "bcm_layout.h"

#define BCM_BIG_ENDIAN

# ifdef BCM_BIG_ENDIAN
void bigEndianByteSwap(uint8 *, typeLayout *);
# else
/* avoid the call to an empty function, just define an empty macro */
#  define bigEndianByteSwap(a,b) BCM_UNUSED_PARAMETER(b->size)
# endif

#endif
