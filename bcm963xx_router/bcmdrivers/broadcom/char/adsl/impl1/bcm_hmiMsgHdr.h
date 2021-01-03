/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiMsgHdr.h                                        */
/*   Purpose         : Header type definition of HMI messages                 */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiMsgHdr.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef HMI_MSG_HDR_H
# define HMI_MSG_HDR_H

# include "bcm_userdef.h"
# include "bcm_layout.h"

# define HMI_MAJOR_VERSION                  BCM_SDK_MAJOR_VERSION
# define HMI_MINOR_VERSION                  BCM_SDK_MINOR_VERSION

# define HMI_RESULT_SUCCESS                 BCM_SUCCESS
# define HMI_RESULT_UNKNOW_APPLICATION_ID   1
# define HMI_RESULT_UNKNOW_COMMAND_ID      -1  
# define HMI_RESULT_WRONG_MESSAGE_FORMAT   -3
# define HMI_RESULT_REQUEST_DATA_NOT_VALID -4
# define HMI_RESULT_CANT_PROCESS_REQUEST   -5

typedef struct HmiMsgHdr HmiMsgHdr;
struct HmiMsgHdr
{
  uint32 sourceRef;  /* This parameter is not used by the BCM6410 device.  It
                        will be transparently sent back in the reply, enabling
                        the Controller to match the reply to the corresponding
                        request that it previously sent. */

  uint32 requestRef; /* This parameter is not used by the BCM6410 device .It
                        will be transparently sent back in the reply, enabling
                        the Controller to match the reply to the corresponding
                        request that it previously sent */

  uint16 applicationId; /* Identification of the application to which the
                           request is being sent. */

  uint16 length; /* Length is the message specific data length */

  uint8  replyAddress[5]; /* This parameter is not used by the application
                             invoked but will be used by the message
                             dispatcher to know where to dispatch the message
                             reply.  This field is not used when the request
                             is received on the memory mapped FIFO.  When the
                             request is received through an in-band message,
                             its value contains the ATM header (5 bytes) that
                             shall be used for all ATM Packet Data Units being
                             part of the reply message.  According to the AAL5
                             encapsulation protocol, bit 1 of the fourth
                             header byte (PTI bit 0) will be overwritten to
                             "0" for all PDU's but the last one. */

  int8  result;  /* This indicates whether the request was correctly
                  * handled by the firmware. */

  uint16 firmwareId; /* Provided to the Controller for information purposes 
                        (on reply message only).  This is a unique identifier
                        of the code version currently running on the BCM6410. */
  uint16 commandId; /* Identification of the command that the application must
                     * perform */
  int16  reserved;
} BCM_PACKING_ATTRIBUTE ;

extern typeLayout HmiMsgHdrLayout[];

#endif
