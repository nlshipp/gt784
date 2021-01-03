/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiMsgDef.h                                        */
/*   Purpose         : definition of some macros to automatically define the  */
/*                     message layout variable, which is: <Name>Layout        */
/*                     where Name is the name of the service                  */
/*                     This is a structure of type MsgLayout                  */
/*                     It contains the command Id of the service, and 2       */
/*                     pointers to the layout variables of the service        */
/*                     request and reply underlying data                      */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 18-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiMsgDef.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef HMI_MSG_DEF_H
# define HMI_MSG_DEF_H

# define concat(a,b) a ## b

/*********************************************** */
/* Macros for message layout variable definition */
/*********************************************** */

/* macro for message having no reply nor request payload */
#define HMI_MSG_NPL_LAYOUT(Name, CommandId) \
   MsgLayout concat(Name,Layout) = \
   { CommandId, #Name, emptyLayout, emptyLayout};

/* macro for message having no reply payload */
#define HMI_MSG_REQ_LAYOUT(Name, CommandId, RequestType) \
   MsgLayout concat(Name,Layout) = \
   { CommandId, #Name, concat(RequestType,Layout), emptyLayout};

/* macro for message having no request payload */
#define HMI_MSG_REP_LAYOUT(Name, CommandId, ReplyType) \
   MsgLayout concat(Name,Layout) = \
   { CommandId, #Name, emptyLayout, concat(ReplyType,Layout)};

/* macro for message having both request and reply payloads */
#define HMI_MSG_ALL_LAYOUT(Name, CommandId, RequestType, ReplyType) \
   MsgLayout concat(Name,Layout) = \
   { CommandId, #Name, concat(RequestType,Layout), concat(ReplyType,Layout)};

#endif
