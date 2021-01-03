/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiCoreMsg.c                                       */
/*   Purpose         : Definition of layout variables for all core manager    */
/*                     HMI message types and related principal types          */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiCoreMsg.c,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/

#include "bcm_userdef.h"
#include "bcm_hmiCoreMsg.h"
#include "bcm_hmiMsgDef.h"

#if 0
/* {{{ static BCM6510 configurations */
#ifdef BCM6510_AFE_CONFIG

const SetBCM6510afeConnectMsgReq defaultBCM6510afeConfig[] =
{ 
# ifndef BCM6510_AFE_CONFIG_CUSTOM
  {{5, 0,2, 9, 8,2, 9,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_16 */
  {{5, 0,0,11,12,2, 5,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_32A(0) */
  {{5, 0,6, 9, 4,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_32A(1) */
  {{5,14,2, 5, 2,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_32B(0) */
  {{5,14,6, 9, 2,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_32B(1) */
  {{5, 0,0,11,12,0, 3,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48A(0) */
  {{5, 0,4,11, 8,0, 7,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48A(1) */
  {{5, 0,8,11, 4,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48A(2) */
  {{5,14,0, 3, 2,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48B(0) */
  {{5,14,4, 7, 2,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48B(1) */
  {{5,14,8,11, 2,0,11,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_48B(2) */
  {{5, 0,0, 0, 2,0,11,0,2,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_12 */
  {{5, 0,0, 5, 6,4,13,0,0,0,0,0,0,0,0,0}}, /* BCM6510_AFE_CONFIG_6411 */
# else
  BCM6510_AFE_CONFIG_CUSTOM,
# endif
};

#endif
/* }}} */
/* {{{ layout variable of principal types */

typeLayout SantoriniConfigLayout[] =
{
  { BCM_DEV_NB_LINES*2*2, sizeof(int32)},
  { 2,                    sizeof(uint8)  },
  { 12,                   sizeof(uint16) },
  { 8+6,                  sizeof(uint8)  },
  { 1,                    sizeof(uint16) },
  { 8,                    sizeof(uint8)  },
  { 1,                    sizeof(uint16) },
  { 2,                    sizeof(uint8)  },
  { 1,                    sizeof(uint16) },
  { 2,                    sizeof(int32)  },
  { 8,                    sizeof(uint8)  },
  { 1,                    sizeof(uint32) },
  { 0,                    0              }
};

typeLayout LineInterruptLayout[] =
{
  { BCM_DEV_NB_LINES*2+2, sizeof(uint32) },
  { 0,      0 }
};

typeLayout SantChangeLineIMRLayout[] =
{
  { BCM_DEV_NB_LINES*2, sizeof(uint32) },
  { 0,    0 }
};
#endif /* #if 0 */
typeLayout FirmwareVersionInfoLayout[] =
{
  { 1,  sizeof(uint32) },
  { 1,  sizeof(uint16) },
  { 2,  sizeof(uint8)  },
  { 2,  sizeof(uint16) },
  { 64, sizeof(uint8)  },
  { 4,  sizeof(uint8)  },
  { 0,  0              }
};
#if 0
typeLayout GPoutDataLayout[] =
{
  { 2,  sizeof(uint32) },
  { 0,  0              }
};

typeLayout GPinDataLayout[] =
{
  { 1,  sizeof(uint32) },
  { 0,  0              }
};

typeLayout TimingCalibrationResultLayout[] =
{
  { 1,  sizeof(uint32) },
  { 0,  0              }
};

typeLayout OAMperiodElapsedTimeLayout[] =
{
  { 3, sizeof(uint32) },
  { 0, 0              }
};

#ifndef CO_VCOPE
typeLayout SetOEMidMsgReqLayout[] =
{
  { 121, sizeof(uint8) },
  { 0,   0             }
};
#endif

typeLayout SetPtmVcMsgReqLayout[] =
{
  { 4*2+4*2, sizeof(uint16) },
  { 0,       0              }
};

typeLayout SNMPdetectLayout[] =
{
  { 2,  sizeof(uint32) },
  { 0,  0              }
};

typeLayout PstackIPconfigLayout[] =
{
  { 3,  sizeof(uint32) },
  { 4,  sizeof(uint8)  },
  { 0,  0              }
};

typeLayout PstackL2configLayout[] =
{
  { 5,  sizeof(uint16) },
  { 8,  sizeof(uint8)  },
  { 0,  0              }
};

typeLayout StackStatusLayout[] =
{
  { 1,  sizeof(int32)  },
  { 0,  0              }
};

typeLayout LedPatternLayout[] =
{
  { 8*4, sizeof(uint8)  },
  { 1,   sizeof(uint32) },
  { 4,   sizeof(uint8)  },
  { 0,   0              }
};

typeLayout SetTxFilterMsgReqLayout[] =
{
  { BCM_TX_FILTER_LEN+1, sizeof(uint8)},
  { 0, 0 }
};

#ifdef BCM6510_AFE_CONFIG
typeLayout SetBCM6510afeConnectMsgReqLayout[] =
{
  { 16, sizeof(uint8)  },
  { 0, 0 }
};
#endif

/* }}} */
/* {{{ message layout using the macros in hmiMsgDef.h */

HMI_MSG_REQ_LAYOUT(SetDeviceConfiguration, SET_DEVICE_CONFIGURATION,
                   SantoriniConfig)

HMI_MSG_REP_LAYOUT(GetDeviceConfiguration, GET_DEVICE_CONFIGURATION,
                   SantoriniConfig)

HMI_MSG_REP_LAYOUT(GetLineInterruptStatus, GET_LINE_INTERRUPT_STATUS,
                   LineInterrupt)

HMI_MSG_REQ_LAYOUT(ChangeLineInterruptMask, CHANGE_LINE_INTERRUPT_MASK,
                   SantChangeLineIMR)

HMI_MSG_NPL_LAYOUT(ResetDevice, RESET_DEVICE)
#endif /* #if 0 */
HMI_MSG_REP_LAYOUT(GetFirmwareVersionInfo, GET_FIRMWARE_VERSION_INFO,
                   FirmwareVersionInfo)
#if 0
HMI_MSG_REQ_LAYOUT(WriteGPIO, WRITE_GPIO, GPoutData)

HMI_MSG_REP_LAYOUT(ReadGPIO, READ_GPIO, GPinData)

HMI_MSG_REQ_LAYOUT(SetOAMperiodElapsedTime, SET_OAM_PERIOD_ELAPSED_TIME,
                   OAMperiodElapsedTime)

#ifndef CO_VCOPE
HMI_MSG_REQ_LAYOUT(SetOEMid, SET_OEM_ID, SetOEMidMsgReq)
#endif

HMI_MSG_REQ_LAYOUT(SetPtmVc, SET_PTM_VC, SetPtmVcMsgReq)

HMI_MSG_ALL_LAYOUT(DetectModem, DETECT_MODEM, SNMPdetect, SNMPdetect)

HMI_MSG_REQ_LAYOUT(SetInBandIPconfiguration, SET_IN_BAND_IP_CONFIGURATION,
                   PstackIPconfig)

HMI_MSG_REP_LAYOUT(GetInBandIPconfiguration, GET_IN_BAND_IP_CONFIGURATION,
                   PstackIPconfig)

HMI_MSG_REP_LAYOUT(GetInBandLayer2Config, GET_IN_BAND_LAYER_2_CONFIG,
                   PstackL2config)

HMI_MSG_REP_LAYOUT(GetInBandStackStatus, GET_IN_BAND_STACK_STATUS,
                   StackStatus)

HMI_MSG_REQ_LAYOUT(SetLedPattern, SET_LED_PATTERN, LedPattern)

HMI_MSG_REQ_LAYOUT(SetTxFilter, SET_TX_FILTER, SetTxFilterMsgReq)

#ifdef BCM6510_AFE_CONFIG
HMI_MSG_REQ_LAYOUT(SetBCM6510afeConnect, SET_BCM6510_AFE_CONNECT,
                   SetBCM6510afeConnectMsgReq)
#endif
#endif /* #if 0 */

/* }}} */
