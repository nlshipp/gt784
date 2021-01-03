/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiMsgHdr.c                                        */
/*   Purpose         : Definition of layout variables for hmiMsgHdr type      */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiMsgHdr.c,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#include "bcm_userdef.h"
#include "bcm_hmiMsgHdr.h"

typeLayout HmiMsgHdrLayout[] =
{
  { 2, sizeof(uint32) },
  { 2, sizeof(uint16) },
  { 6, sizeof(uint8)  },        /* uint8 and int8 have same size */
  { 3, sizeof(int16)  },        /* uint16 and int16 have same size */
  {0, 0}
};
