/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_userdef.h                                          */
/*   Purpose         : User specific definitions                              */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 26-Nov-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_userdef.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef BCM_USER_DEF_H
# define BCM_USER_DEF_H

#if 0
# ifndef NB_LINES
#  define NB_LINES 12
# endif

# ifndef NB_HMIDEV
#  define NB_HMIDEV 6
# endif

/* example of devices' addresses in case of memory mapped access */
# define BASE_ADDRESS  0x10000  /* f.i. first BCM6410 address */
# define OFFSET_ADDRESS 0x8000  /* 32KB is minimum for one BCM6410 */

/* interrupt are not used by default */
# define BCM_USE_POLLING_NOT_INTERRUPT

/* define the basic types */
# include "bcm_BasicTypes.h"
#else
#include "bcmtypes.h"
#endif

/* bcm generic definitions */
# include "bcm_genericDef.h"

#endif
