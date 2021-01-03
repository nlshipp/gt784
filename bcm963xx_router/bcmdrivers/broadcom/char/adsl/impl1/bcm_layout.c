/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_layout.c                                           */
/*   Purpose         : usefull functions for layout types                     */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_layout.c,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/

#include "bcm_userdef.h"
#include "bcm_layout.h"
#include "bcm_hmiMsgHdr.h"

int sizeOfType(typeLayout *layout)
{
  int i = 0;
  int size = 0;
  
  while (layout[i].occurence != 0) 
  {
    size += layout[i].occurence * layout[i].size;
    i++;
  }
  
  return size;
}

int sizeOfRequestMsg(MsgLayout *layout)
{
  return (sizeOfType(layout->requestMsgLayout));
}

int sizeOfReplyMsg(MsgLayout *layout)
{
  return (sizeOfType(layout->replyMsgLayout));
}

/* empty layout structure for message having no request/reply payload */
typeLayout emptyLayout[]  = { { 0, 0 } };
typeLayout uint8Layout[] = { { 1, sizeof(uint8)}, { 0, 0 } };
typeLayout uint16Layout[] = { { 1, sizeof(uint16)}, { 0, 0 } };
typeLayout uint32Layout[] = { { 1, sizeof(uint32)}, { 0, 0 } };

