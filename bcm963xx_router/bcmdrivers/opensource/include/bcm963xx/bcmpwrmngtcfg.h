/***********************************************************************
//
//  Copyright (c) 2008  Broadcom Corporation
//  All Rights Reserved
//  No portions of this material may be reproduced in any form without the
//  written permission of:
//          Broadcom Corporation
//          5300 California Avenue 
//          Irvine, California 92617 
//  All information contained in this document is Broadcom Corporation
//  company private, proprietary, and trade secret.
//
************************************************************************/
#ifndef _BCMPWRMNGTCFG_H
#define _BCMPWRMNGTCFG_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************
 * Constant Definitions
 ***************************************************************************/
#define PWRMNGT_ENABLE                         1
#define PWRMNGT_DISABLE                        0

/* MIPS CPU Valid Speeds */
#define PWRMNGT_MIPS_CPU_SPEED_MIN_VAL         0
#define PWRMNGT_MIPS_CPU_SPEED_MAX_VAL         256

/* Return status values. */
typedef enum PwrMngtStatus
{
    PWRMNGTSTS_SUCCESS = 0,
    PWRMNGTSTS_INIT_FAILED,
    PWRMNGTSTS_ERROR,
    PWRMNGTSTS_STATE_ERROR,
    PWRMNGTSTS_PARAMETER_ERROR,
    PWRMNGTSTS_ALLOC_ERROR,
    PWRMNGTSTS_NOT_SUPPORTED,
    PWRMNGTSTS_TIMEOUT,
} PWRMNGT_STATUS;

typedef unsigned int ui32;

/* Masks defined here are used in selecting the required parameter from Mgmt
 * application.
 */
#if defined(CHIP_6368)
#define PWRMNGT_CFG_PARAM_ALL_MASK               0x0000FFFF
#define PWRMNGT_CFG_DEF_PARAM_MASK               0x0000FFFF  /* Parameters for which the defaults exist */
#elif defined(CHIP_6816) || defined(CONFIG_BCM96816)
#define PWRMNGT_CFG_PARAM_ALL_MASK               0x0003FFFF
#define PWRMNGT_CFG_DEF_PARAM_MASK               0x0003FFFF  /* Parameters for which the defaults exist */
#else
#define PWRMNGT_CFG_PARAM_ALL_MASK               0x00000FFF
#define PWRMNGT_CFG_DEF_PARAM_MASK               0x00000FFF  /* Parameters for which the defaults exist */
#endif

typedef struct _PwrMngtConfigParams {
#define PWRMNGT_CFG_PARAM_CPUSPEED_MASK            0x00000001
   ui32                  cpuspeed;
#define PWRMNGT_CFG_PARAM_CPU_R4K_WAIT_MASK        0x00000002
   ui32                  cpur4kwait;
#define PWRMNGT_CFG_PARAM_MEM_SELF_REFRESH_MASK    0x00000004
   ui32                  dramSelfRefresh;
#define PWRMNGT_CFG_PARAM_MEM_ETH_APD_MASK 0x00000008
   ui32                  ethAutoPwrDwn;
} PWRMNGT_CONFIG_PARAMS, *PPWRMNGT_CONFIG_PARAMS ;


typedef struct
{
   ui32                     ethRegisteredCnt;
   ui32                     ethConnectedCnt;
   ui32                     USBRegisteredCnt;
   ui32                     USBConnectedCnt;
   PWRMNGT_STATUS           status; 
}PWRMNGT_IF_STATUS, *PPWRMNGT_IF_STATUS ;


/***************************************************************************
 * Function Prototypes
 ***************************************************************************/

#ifdef __cplusplus
}
#endif
#endif /* _BCMPWRMNGTCGG_H */
