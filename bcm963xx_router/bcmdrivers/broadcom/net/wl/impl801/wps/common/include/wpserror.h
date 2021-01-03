/*
 * WPS ERROR code definition
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpserror.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 */

#ifndef _WPS_ERROR_
#define _WPS_ERROR_


#define WPS_MESSAGE_PROCESSING_ERROR 19

/* Generic */
#define WPS_BASE                    0x1000
#define WPS_SUCCESS                 WPS_BASE+1
#define WPS_ERR_OUTOFMEMORY         WPS_BASE+2
#define WPS_ERR_SYSTEM              WPS_BASE+3
#define WPS_ERR_NOT_INITIALIZED     WPS_BASE+4
#define WPS_ERR_INVALID_PARAMETERS  WPS_BASE+5
#define WPS_ERR_BUFFER_TOO_SMALL    WPS_BASE+6
#define WPS_ERR_NOT_IMPLEMENTED     WPS_BASE+7
#define WPS_ERR_ALREADY_INITIALIZED WPS_BASE+8
#define WPS_ERR_GENERIC             WPS_BASE+9
#define WPS_ERR_FILE_OPEN           WPS_BASE+10
#define WPS_ERR_FILE_READ           WPS_BASE+11
#define WPS_ERR_FILE_WRITE          WPS_BASE+12

#define WPS_SEND_MSG_CONT           WPS_BASE+13
#define WPS_SEND_MSG_SUCCESS        WPS_BASE+14
#define WPS_SEND_MSG_ERROR          WPS_BASE+15
#define WPS_SEND_MSG_IDRESP         WPS_BASE+16
#define WPS_SEND_FAIL_CONT          WPS_BASE+17
#define WPS_CONT                    WPS_BASE+18
#define WPS_ERR_ADAPTER_NONEXISTED  WPS_BASE+19
#define WPS_ERR_ADDCLIENT_PINFAIL   WPS_BASE+20
#define WPS_ERR_CONFIGAP_PINFAIL    WPS_BASE+21
#define WPS_ERR_OPEN_SESSION        WPS_BASE+22

/* CQueue */
#define CQUEUE_BASE               0x2000
#define CQUEUE_ERR_INTERNAL       CQUEUE_BASE+1
#define CQUEUE_ERR_IPC            CQUEUE_BASE+2

/* State machine */
#define SM_BASE                   0x3000
#define SM_ERR_INVALID_PTR        SM_BASE+1
#define SM_ERR_WRONG_STATE        SM_BASE+2
#define SM_ERR_MESSAGE_DATA       SM_BASE+3

#define MC_BASE                   0x4000
#define MC_ERR_CFGFILE_CONTENT          MC_BASE+1
#define MC_ERR_CFGFILE_OPEN             MC_BASE+2
#define MC_ERR_STACK_ALREADY_STARTED    MC_BASE+3
#define MC_ERR_STACK_NOT_STARTED        MC_BASE+4
#define MC_ERR_VALUE_UNCHANGED          MC_BASE+5

/* Transport */
#define TRANS_BASE                 0x5000

#define TRUFD_BASE                 0x5100
#define TRUFD_ERR_DRIVE_REMOVED    TRUFD_BASE+1
#define TRUFD_ERR_FILEOPEN         TRUFD_BASE+2
#define TRUFD_ERR_FILEREAD         TRUFD_BASE+3
#define TRUFD_ERR_FILEWRITE        TRUFD_BASE+4
#define TRUFD_ERR_FILEDELETE       TRUFD_BASE+5

#define TRNFC_BASE                 0x5200
#define TRNFC_ERR_NO_TAG           TRNFC_BASE+1
#define TRNFC_ERR_NO_READER        TRNFC_BASE+2

#define TREAP_BASE                 0x5300
#define TREAP_ERR_SENDRECV         TREAP_BASE+1

#define TRWLAN_BASE                0x5400
#define TRWLAN_ERR_SENDRECV        TRWLAN_BASE+1

#define TRIP_BASE                       0x5500
#define TRIP_ERR_SENDRECV               TRIP_BASE+1
#define TRIP_ERR_NETWORK                TRIP_BASE+2
#define TRIP_ERR_NOT_MONITORING         TRIP_BASE+3
#define TRIP_ERR_ALREADY_MONITORING     TRIP_BASE+4
#define TRIP_ERR_INVALID_SOCKET         TRIP_BASE+5

#define TRUPNP_BASE                 0x5600
#define TRUPNP_ERR_SENDRECV         TRUPNP_BASE+1

/* RegProtocol */
#define RPROT_BASE                 0x6000
#define RPROT_ERR_REQD_TLV_MISSING RPROT_BASE+1
#define RPROT_ERR_CRYPTO           RPROT_BASE+2
#define RPROT_ERR_INCOMPATIBLE     RPROT_BASE+3
#define RPROT_ERR_INVALID_VALUE    RPROT_BASE+4
#define RPROT_ERR_NONCE_MISMATCH   RPROT_BASE+5
#define RPROT_ERR_WRONG_MSGTYPE    RPROT_BASE+6
#define RPROT_ERR_MULTIPLE_M2      RPROT_BASE+7
#define RPROT_ERR_AUTH_ENC_FLAG    RPROT_BASE+8
#define WPS_PASSWORD_AUTH_ERROR 18

/* Portability */
#define PORTAB_BASE                 0x7000
#define PORTAB_ERR_SYNCHRONIZATION  PORTAB_BASE+1
#define PORTAB_ERR_THREAD           PORTAB_BASE+2
#define PORTAB_ERR_EVENT            PORTAB_BASE+3
#define PORTAB_ERR_WAIT_ABANDONED   PORTAB_BASE+4
#define PORTAB_ERR_WAIT_TIMEOUT     PORTAB_BASE+5

#endif /* _WPS_ERROR_ */