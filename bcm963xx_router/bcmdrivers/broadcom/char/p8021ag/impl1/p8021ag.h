/*
<:copyright-broadcom 
 
 Copyright (c) 2007 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          5300 California Avenue
          Irvine, California 92617 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***************************************************************************
 * File Name  : p8021ag.h
 *
 * Description: This file contains the definitions and structures for the
 *              IEEE P802.1ag driver.
 ***************************************************************************/

#if !defined(_P8021AG_H_)
#define _P8021AG_H_

/* Defines. */

/* Linktrace Message multicast addresses. */
#define LTM_MAC_L0                          {0x01,0x80,0xc2,0x00,0x00,0x08}
#define LTM_MAC_L1                          {0x01,0x80,0xc2,0x00,0x00,0x09}
#define LTM_MAC_L2                          {0x01,0x80,0xc2,0x00,0x00,0x0a}
#define LTM_MAC_L3                          {0x01,0x80,0xc2,0x00,0x00,0x0b}
#define LTM_MAC_L4                          {0x01,0x80,0xc2,0x00,0x00,0x0c}
#define LTM_MAC_L5                          {0x01,0x80,0xc2,0x00,0x00,0x0d}
#define LTM_MAC_L6                          {0x01,0x80,0xc2,0x00,0x00,0x0e}
#define LTM_MAC_L7                          {0x01,0x80,0xc2,0x00,0x00,0x0f}

/* Macro to set maintenance domain level in CFM_COMMON_HDR ucMdLevelVersion. */
#define CFM_MD_LEVEL(V)                     ((unsigned char) (V) << 5)
#define CFM_VERSION_MASK                    0x1f
#define CFM_MD_LEVEL_MASK                   0xe0

/* Values for CFM_COMMON_HDR ucOpCode. */
#define CFM_CCM                             1
#define CFM_LBR                             2
#define CFM_LBM                             3
#define CFM_LTR                             4
#define CFM_LTM                             5

/* Values for CFM_COMMON_HDR ucFlags. */
#define CFM_FLAG_LT_USE_FDB_ONLY            0x80
#define CFM_FLAG_LTR_FWD_YES                0x40
#define CFM_FLAG_LTR_TERMINAL_MEP           0x20

/* Values for CFM_COMMON_HDR ucFirstTlvOffset. */
#define CFM_FTO_LBM                         4
#define CFM_FTO_LBR                         4
#define CFM_FTO_LTM                         17
#define CFM_FTO_LTR                         6

/* Offsets and macros for TLV handling. */
#define TLV_TYPE_OFS                        0
#define TLV_LEN_OFS                         1
#define TLV_START_DATA                      3

#define TLV_GET_LEN(P)                                                  \
    (((unsigned short)(*(unsigned char *) ((P) + TLV_LEN_OFS)) << 8) |  \
     ((unsigned short)(*(unsigned char *) ((P) + TLV_LEN_OFS + 1))))

#define TLV_SET_LEN(P,L)                                                \
    do                                                                  \
    {                                                                   \
        *((unsigned char *) (P) + TLV_LEN_OFS) =                        \
            (unsigned char) ((unsigned short) (L) >> 8);                \
        *((unsigned char *) (P) + TLV_LEN_OFS + 1) =                    \
            (unsigned char) ((unsigned short) (L) & 0xff);              \
    } while(0)

/* Values for TLV type. */
#define TLV_TYPE_END                        0
#define TLV_TYPE_SENDER_ID                  1
#define TLV_TYPE_PORT_STATUS                2
#define TLV_TYPE_DATA                       3
#define TLV_TYPE_INTERFACE_STATUS           4
#define TLV_TYPE_REPLY_INGRESS              5
#define TLV_TYPE_REPLY_EGRESS               6
#define TLV_TYPE_LTM_EGRESS_ID              7
#define TLV_TYPE_LTR_EGRESS_ID              8

/* Values for TLV length. */
#define TLV_LEN_LTM_EGRESS_ID               8
#define TLV_LEN_LTR_EGRESS_ID               16

/* LBM and LBR offsets. */
#define LB_TRANS_ID_OFS                     4
#define LB_HDR_PLUS_FIRST_TLV_OFS           (sizeof(CFM_COMMON_HDR)+CFM_FTO_LBM)

/* LTM offsets. */
#define LTM_TRANS_ID_OFS                    4
#define LTM_TTL_OFS                         8
#define LTM_ORIG_MAC_ADDR_OFS               9
#define LTM_TARG_MAC_ADDR_OFS               15
#define LTM_HDR_PLUS_FIRST_TLV_OFS          (sizeof(CFM_COMMON_HDR)+CFM_FTO_LTM)

/* LTR offsets. */
#define LTR_TRANS_ID_OFS                    4
#define LTR_TTL_OFS                         8
#define LTR_RELAY_OFS                       9
#define LTR_HDR_PLUS_FIRST_TLV_OFS          (sizeof(CFM_COMMON_HDR)+CFM_FTO_LTR)

/* Values for Relay Action field. */
#define RLY_HIT                             1
#define RLY_FDB                             2
#define RLY_MPDB                            3

/* Typedefs. */
typedef struct
{
    unsigned char ucMdLevelVersion;
    unsigned char ucOpCode;
    unsigned char ucFlags;
    unsigned char ucFirstTlvOffset;
} CFM_COMMON_HDR, *PCFM_COMMON_HDR;

extern int (*p8021ag_hook)(struct sk_buff *skb);
#endif // _P8021AG_H_

