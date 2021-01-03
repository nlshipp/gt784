/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_layout.h                                           */
/*   Purpose         : layout definitions of some HMI BasicTypes and message  */
/*                     to be used for header construction and byte swapping   */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_layout.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef LAYOUT_H
# define LAYOUT_H

# include "bcm_userdef.h"

# define SIZE_OF_REQ_MSG(Name) sizeof(Name ## MsgReq)
# define SIZE_OF_REP_MSG(Name) sizeof(Name ## MsgRep)

typedef struct typeLayout typeLayout;
struct typeLayout
{
  int occurence;
  int size;
};

typedef struct MsgLayout MsgLayout;
struct MsgLayout
{
  uint16 commandId;
  const char *name;
  typeLayout *requestMsgLayout;
  typeLayout *replyMsgLayout;
};

extern typeLayout emptyLayout[]; /* layout of empty structure */
extern typeLayout uint8Layout[]; /* layout of uint16 */
extern typeLayout uint16Layout[]; /* layout of uint16 */
extern typeLayout uint32Layout[]; /* layout of uint32 */


/* functions declarations */
int sizeOfType      (typeLayout *layout);
int sizeOfRequestMsg(MsgLayout  *layout);
int sizeOfReplyMsg  (MsgLayout  *layout);

#endif
