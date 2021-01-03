/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiLineMsg.c                                       */
/*   Purpose         : Definition of layout variables for all line manager    */
/*                     HMI message types and related principal types          */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 18-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiLineMsg.c,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/

#include "bcm_userdef.h"
#include "bcm_hmiLineMsg.h"
#include "bcm_hmiMsgDef.h"

#if 0
/* {{{ layout variable of principal types */
typeLayout LineTrafficConfigLayout[] =
{
  { 3,   sizeof(uint32) },
  { 4,   sizeof(uint8)  },
  { 3,   sizeof(uint32) },
  { 4+4, sizeof(uint8)  },
  { 1,   sizeof(uint32) },
  { 1,   sizeof(uint16) },
  { 2,   sizeof(int8)   },
  { 3,   sizeof(uint32) },
  { 4,   sizeof(uint8)  },
  { 3,   sizeof(uint32) },
  { 4+4, sizeof(uint8)  },
  { 1,   sizeof(uint32) },
  { 1,   sizeof(uint16) },
  { 2+4, sizeof(int8)   },
  { 0,   0              }
};

typeLayout PhysicalLayerConfigLayout[] =
{
  { 3,                  sizeof(int16) },
  { 2,                  sizeof(int8)  },
  { 5+3,                sizeof(int16) },
  { 2,                  sizeof(int8)  },
  { 5+2*3+1,            sizeof(int16) },
  { 84,                 sizeof(uint8) },
  { BCM_TSSI_SHAPE_LEN, sizeof(uint16)},
  { 2,                  sizeof(int8)  },
  { 2,                  sizeof(uint16)},
  { 12,                 sizeof(int8)  },
  { 1,                  sizeof(uint32)},
  { 0,                  0             }
};

typeLayout PhyConfigVDSLLayout[] =
{
  { 2,                           sizeof(uint8)  }, /* limitingMaskDn */
  { BCM_VDSL_NB_PSD_BP_DN*2,     sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* limitingMaskUp */
  { BCM_VDSL_NB_PSD_BP_UP*2,     sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* bandPlanDn */
  { BCM_VDSL_NB_TONE_GROUPS*2,   sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* bandPlanUp */
  { BCM_VDSL_NB_TONE_GROUPS*2,   sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* inBandNotches */
  { BCM_VDSL_NB_NOTCH_BANDS*2,   sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* rfiNotches */
  { BCM_VDSL_NB_RFI_BANDS*2,     sizeof(uint16) },
  { 2,                           sizeof(uint8)  }, /* pboPsdUp */
  { BCM_VDSL_NB_TONE_GROUPS*2+2, sizeof(int16)  },
  { 0,                           0              }
};

typeLayout TestConfigMessageLayout[] =
{
  { 2,  sizeof(uint32) },
  { 16, sizeof(uint8)  },
  { 2,  sizeof(uint32) },
  { 8,  sizeof(uint8)  },
  { 0,  0              }
};

#endif /* #if 0 */

/*{ 2*2,   sizeof(uint32) },     PtmPerfCounters */
typeLayout LineCountersLayout[] =
{
  { 2*8,   sizeof(uint32) },    /* DerivedSecCounters */
  { 2*2*3, sizeof(uint32) },    /* AdslAnomCounters */
  { 2*2*5, sizeof(uint32) },    /* AtmPerfCounters */
  { 3,     sizeof(uint16) },
  { 2,     sizeof(uint8)  },
  { 1,     sizeof(uint32) },
  { 0,     0              }
};


typeLayout LineStatusLayout[] =
{
  { 8,     sizeof(int8)   },
  { 5,     sizeof(int16)  },    /* LinkStatus[US] at 8 */
  { 2,     sizeof(uint8)  },
  { 1,     sizeof(uint32) },
  { 2,     sizeof(uint32) },    /* BearerParams[B0] at 24 */
  { 2,     sizeof(int8)   },
  { 1,     sizeof(uint16) },
  { 2,     sizeof(uint32) },    /* BearerParams[B1] at 36 */
  { 2,     sizeof(int8)   },
  { 1,     sizeof(uint16) },
  { 2+2,   sizeof(int8)   },    /* MSGx at 48 */
  { 4,     sizeof(uint16) },    /* LatencyPathConfig[LP0] at 50 */
  { 2,     sizeof(uint8)  },
  { 3,     sizeof(uint16) },
  { 6,     sizeof(uint8)  },
  { 4,     sizeof(uint16) },    /* LatencyPathOptions[LP1] at 72 */
  { 2,     sizeof(uint8)  },
  { 3,     sizeof(uint16) },
  { 6,     sizeof(uint8)  },
  { 2,     sizeof(uint32) },    /* failures at 96 */
  { 5,     sizeof(int16)  },    /* LinkStatus[DS] at 104 */
  { 2,     sizeof(uint8)  },
  { 1,     sizeof(uint32) },
  { 2,     sizeof(uint32) },    /* BearerParams[B0] at 120 */
  { 2,     sizeof(int8)   },
  { 1,     sizeof(uint16) },
  { 2,     sizeof(uint32) },    /* BearerParams[B1] at 132 */
  { 2,     sizeof(int8)   },
  { 1,     sizeof(uint16) },
  { 2+2,   sizeof(int8)   },    /* MSGx at 144 */
  { 4,     sizeof(uint16) },    /* LatencyPathConfig[LP0] at 146 */
  { 2,     sizeof(uint8)  },
  { 3,     sizeof(uint16) },
  { 6,     sizeof(uint8)  },
  { 4,     sizeof(uint16) },    /* LatencyPathOptions[LP1] at 168 */
  { 2,     sizeof(uint8)  },
  { 3,     sizeof(uint16) },
  { 6,     sizeof(uint8)  },
  { 2,     sizeof(uint32) },    /* failures at 192 */
  { 0,     0              }
};

#if 0
typeLayout LineFeaturesLayout[] =
{
  { 2*2+2+2*3+2*3, sizeof(uint32) },
  { 0,             0              }
};
    
typeLayout EocRegisterIdLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout EocReadResultLayout[] =
{
  { 34, sizeof(uint8) },
  { 0 , 0             }
};

typeLayout EocRegisterLayout[] =
{
  { 34, sizeof(uint8) },
  { 0,  0             }
};

typeLayout EocWriteResultLayout[] =
{
  { 2, sizeof(uint8) },
  { 0, 0             }
};

typeLayout ClearEocMessageLayout[] =
{
  { 1,   sizeof(uint16) },
  { 486, sizeof(uint8)  },
  { 0,   0              }
};

typeLayout GetBandPlanVDSLMsgRepLayout [] =
{
  { 2,                           sizeof(uint8)  },
  { BCM_VDSL_NB_TONE_GROUPS*2,   sizeof(uint16) },
  { 2,                           sizeof(uint8)  },
  { BCM_VDSL_NB_TONE_GROUPS*2,   sizeof(uint16) },
  { 0,                           0              }
};

typeLayout TestModeSelectionLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LoopCharacteristicsLayout[] =
{
  { 1, sizeof(uint32) },
  { 0, 0              }
};

typeLayout StartLoopCharacterisationMsgReqLayout[] =
{
  { 2, sizeof(uint8) },
  { 0, 0             }
};

typeLayout SignalGenerationLayout[] =
{
  { 2,                         sizeof(uint8)  },
  { 1,                         sizeof(int16)  },
  { 2,                         sizeof(uint32) },
  { 68,                        sizeof(uint8)  },
  { BCM_TSSI_SHAPE_LEN,        sizeof(uint16) }, /* customTssiShapeAdsl */
  { 2+2,                       sizeof(int8)   },
  { BCM_VDSL_NB_TONE_GROUPS*2, sizeof(uint16) },
  { 2,                         sizeof(uint8)  },
  { BCM_VDSL_NB_TONE_GROUPS*2, sizeof(uint16) },
  { 2,                         sizeof(uint8)  }, /* limitingMaskDn */
  { BCM_VDSL_NB_PSD_BP_DN*2,   sizeof(uint16) },
  { 1,                         sizeof(uint16) },
  { 2,                         sizeof(uint8)  },
  { 0,                         0              }
};

typeLayout GetSignalMeasurementMsgReqLayout[] =
{
  { 2,   sizeof(uint8)  },
  { 0,   0              }
};

typeLayout SignalMeasurementLayout[] =
{
  { 2,   sizeof(uint32) },
  { 192, sizeof(int16)  },
  { 4,   sizeof(uint8)  },
  { 0,   0              }
};

typeLayout ManufTestConfigLayout[] =
{
  { 2, sizeof(uint8)  },
  { 1, sizeof(uint16) },
  { 4, sizeof(uint8)  },
  { 0, 0              }
};

typeLayout ManufacturingTestResultLayout[] =
{
  { 13,  sizeof(uint32) },      /* uint32 and int32 have same size */
  { 0,   0              }
};

typeLayout AGCsettingLayout[] =
{
  { 1, sizeof(int16) },
  { 0, 0             }
};

typeLayout EchoPartIdLayout[] =
{
  { 1, sizeof(uint32) },
  { 0, 0              }
};

typeLayout AccurateEchoLayout[] =
{
  { 64, sizeof(int32) },
  { 0,  0             }
};

typeLayout VDSLechoPartIdLayout[] =
{
  { 1,   sizeof(int32) },
  { 130, sizeof(int16) },
  { 64,  sizeof(int8)  },
  { 0,   0             }
};

typeLayout AccurateVDSLechoLayout[] =
{
  { 1, sizeof(int32) },
  { 0,  0             }
};

typeLayout SeltPartIdLayout[] =
{
  { 1, sizeof(uint32) },
  { 0, 0              }
};

typeLayout SeltResultLayout[] =
{
  { 64, sizeof(int32) },
  { 0,  0             }
};


typeLayout AccurateEchoVarianceLayout[] =
{
  { 128, sizeof(int16) },
  { 0,   0             }
};

typeLayout LineBitAllocAndSnrReqLayout[] =
{
  { 2,  sizeof(uint8) },
  { 0,  0             }
};

typeLayout GetLineBitAllocationMsgRepLayout[] =
{
  { 488, sizeof(uint8) },
  { 0,   0             }
};

typeLayout GetLineSnrMsgRepLayout[] =
{
  { 244, sizeof(uint16) },
  { 0,   0              }
};

typeLayout PeriodIdentificationLayout[] =
{
  { 1, sizeof(uint32) },
  { 0, 0              }
};

typeLayout PeriodCountersLayout[] =
{
  { 2*8,   sizeof(uint32) },    /* DerivedSecCounters */
  { 2*2*3, sizeof(uint32) },    /* AdslAnomCounters */
  { 2*2*5, sizeof(uint32) },    /* AtmPerfCounters */
  { 2*2,   sizeof(uint32) },    /* PtmPerfCounters */
  { 3,     sizeof(uint16) },
  { 2,     sizeof(uint8)  },
  { 4,     sizeof(uint32) },
  { 0,     0              }
};

typeLayout NewThresholdsLayout[] =
{
  { 4*3, sizeof(uint32) },
  { 0,   0              }
};
#endif /* #if 0 */

typeLayout CPEvendorInfoLayout[] =
{
  { 8, sizeof(uint8)  },
  { 1, sizeof(uint16) },
  { 2, sizeof(uint8)  },
  { 2, sizeof(uint32) },
  { 4, sizeof(uint8)  },
  { 2, sizeof(uint32) },
  { 4, sizeof(uint8)  },
  { 0, 0              }
};

#if 0
typeLayout OverheadMessageLayout[] =
{
  { 1,                           sizeof(uint16) },
  { MAX_OVERHEAD_MESSAGE_BUFFER, sizeof(uint8)  },
  { 0,                           0              }
};

typeLayout OverheadMessagePartIdLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout GetLineLoopDiagResultsMsgRepLayout[] =
{
  { 1, sizeof(uint32) },
  { 4, sizeof(uint16) },
  { 1, sizeof(uint32) },
  { 4, sizeof(uint16) },
  { 0, 0              }
};

typeLayout GetLineLoopDiagHlinMsgReqLayout[] =
{
  { 2, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineHlinLayout[] =
{
  { 1+64*2, sizeof(uint16) },
  { 0,      0              }
};

typeLayout GetLineLoopDiagHlogMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineHlogLayout[] =
{
  { 64+128, sizeof(uint16) },
  { 0,      0              }
};

typeLayout GetLineLoopDiagQlnMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineQlnLayout[] =
{
  { 64+256, sizeof(uint8) },
  { 0,      0             }
};

typeLayout GetLineLoopDiagSnrMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineSnrLayout[] =
{
  { 64+256, sizeof(uint8) },
  { 0,      0             }
};

typeLayout UpdateTestParametersMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout UpdateTestParametersMsgRepLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineHlogNELayout[] =
{
  { 64, sizeof(uint16) },
  { 0,  0              }
};

typeLayout LineQlnNELayout[] =
{
  { 64, sizeof(uint8) },
  { 0,  0             }
};

typeLayout GetLineCarrierGainsMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineCarrierGainsLayout[] =
{
  { 244, sizeof(uint16) },
  { 0,   0              }
};

typeLayout GetLineLinearTssiMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineLinearTssiLayout[] =
{
  { 64+128, sizeof(uint16) },
  { 0,      0              }
};

typeLayout LineIntroduceErrorsMsgReqLayout[] =
{
  { 4, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineL2PowerTrimMsgReqLayout[] =
{
  { 1, sizeof(uint8) },
  { 0, 0             }
};

typeLayout LineTriggerOLRMsgReqLayout[] =
{
  { 2, sizeof(uint8) },
  { 1, sizeof(int16) },
  { 6, sizeof(uint8) },
  { 0, 0             }
};

/* }}} */
/* {{{ message layout using the macros in hmiMsgDef.h */

HMI_MSG_NPL_LAYOUT(ResetLine, RESET_LINE)

HMI_MSG_REQ_LAYOUT(SetLineTrafficConfiguration,
                   SET_LINE_TRAFFIC_CONFIGURATION,
                   LineTrafficConfig)

HMI_MSG_REP_LAYOUT(GetLineTrafficConfiguration,
                   GET_LINE_TRAFFIC_CONFIGURATION,
                   LineTrafficConfig)

HMI_MSG_REQ_LAYOUT(SetLinePhysicalLayerConfiguration,
                   SET_LINE_PHYSICAL_LAYER_CONFIGURATION,
                   PhysicalLayerConfig)

HMI_MSG_REP_LAYOUT(GetLinePhysicalLayerConfiguration,
                   GET_LINE_PHYSICAL_LAYER_CONFIGURATION,
                   PhysicalLayerConfig)

HMI_MSG_REQ_LAYOUT(SetLinePhyConfigVDSL, SET_LINE_PHY_CONFIG_VDSL,
                   PhyConfigVDSL)

HMI_MSG_REP_LAYOUT(GetLinePhyConfigVDSL, GET_LINE_PHY_CONFIG_VDSL,
                   PhyConfigVDSL)

HMI_MSG_REQ_LAYOUT(SetLineTestConfiguration,
                   SET_LINE_TEST_CONFIGURATION,
                   TestConfigMessage)

HMI_MSG_REP_LAYOUT(GetLineTestConfiguration,
                   GET_LINE_TEST_CONFIGURATION,
                   TestConfigMessage)

HMI_MSG_NPL_LAYOUT(StartLine, START_LINE)

HMI_MSG_NPL_LAYOUT(StopLine, STOP_LINE)

#endif /* #if 0 */
HMI_MSG_REP_LAYOUT(GetLineCounters, GET_LINE_COUNTERS,
                   LineCounters)

HMI_MSG_REP_LAYOUT(GetLineStatus, GET_LINE_STATUS,
                   LineStatus)
                   
#if 0
HMI_MSG_REP_LAYOUT(GetLineFeatures, GET_LINE_FEATURES,
                   LineFeatures)

HMI_MSG_REQ_LAYOUT(ReadEocRegister, READ_EOC_REGISTER,
                   EocRegisterId)

HMI_MSG_REP_LAYOUT(GetEocReadResult, GET_EOC_READ_RESULT,
                   EocReadResult)

HMI_MSG_REQ_LAYOUT(WriteEocRegister, WRITE_EOC_REGISTER,
                   EocRegister)

HMI_MSG_REP_LAYOUT(GetEocWriteResult, GET_EOC_WRITE_RESULT,
                   EocWriteResult)

HMI_MSG_REQ_LAYOUT(SendClearEocMessage, SEND_CLEAR_EOC_MESSAGE,
                   ClearEocMessage)

HMI_MSG_REP_LAYOUT(GetClearEocMessage, GET_CLEAR_EOC_MESSAGE,
                   ClearEocMessage)

HMI_MSG_REP_LAYOUT(GetBandPlanVDSL, GET_BAND_PLAN_VDSL,
                   GetBandPlanVDSLMsgRep)

HMI_MSG_REQ_LAYOUT(EnterLineTestMode, ENTER_LINE_TEST_MODE,
                   TestModeSelection)

HMI_MSG_NPL_LAYOUT(ExitLineTestMode, EXIT_LINE_TEST_MODE)

HMI_MSG_REQ_LAYOUT(StartLoopCharacterisation, START_LOOP_CHARACTERISATION,
                   StartLoopCharacterisationMsgReq)

HMI_MSG_REP_LAYOUT(GetLoopCharacteristics,GET_LOOP_CHARACTERISATION ,
                   LoopCharacteristics)

HMI_MSG_REQ_LAYOUT(StartSignalGenerationAndMeasurement,
                   START_SIGNAL_GENERATION_AND_MEASUREMENT,
                   SignalGeneration)

HMI_MSG_ALL_LAYOUT(GetSignalMeasurement, GET_SIGNAL_MEASUREMENT,
                   GetSignalMeasurementMsgReq, SignalMeasurement)

HMI_MSG_REQ_LAYOUT(StartManufacturingTest, START_MANUFACTURING_TEST,
                   ManufTestConfig)

HMI_MSG_REP_LAYOUT(GetManufacturingTestResult, GET_MANUFACTURING_TEST_RESULT,
                   ManufacturingTestResult)

HMI_MSG_REP_LAYOUT(GetLineAGCsetting, GET_LINE_AGC_SETTING,
                   AGCsetting)

HMI_MSG_ALL_LAYOUT(GetEcho, GET_ECHO,
                   EchoPartId, AccurateEcho)

HMI_MSG_ALL_LAYOUT(GetVDSLecho, GET_VDSL_ECHO,
                   VDSLechoPartId, AccurateVDSLecho)

HMI_MSG_ALL_LAYOUT(GetSelt, GET_SELT,
                   SeltPartId, SeltResult)

HMI_MSG_ALL_LAYOUT(GetEchoVariance, GET_ECHO_VARIANCE,
                   EchoPartId, AccurateEchoVariance)

HMI_MSG_ALL_LAYOUT(GetLineBitAllocation, GET_LINE_BIT_ALLOCATION,
                   LineBitAllocAndSnrReq, GetLineBitAllocationMsgRep)

HMI_MSG_ALL_LAYOUT(GetLineSnr, GET_LINE_SNR,
                   LineBitAllocAndSnrReq, GetLineSnrMsgRep)

HMI_MSG_ALL_LAYOUT(GetLinePeriodCounters, GET_LINE_PERIOD_COUNTERS,
                   PeriodIdentification, PeriodCounters)

HMI_MSG_REQ_LAYOUT(SetLineOAMthresholds, SET_LINE_OAM_THRESHOLDS,
                   NewThresholds)
#endif /* #if 0 */
HMI_MSG_REP_LAYOUT(GetLineCPEvendorInfo, GET_LINE_CPE_VENDOR_INFO,
                   CPEvendorInfo)
#if 0
HMI_MSG_REQ_LAYOUT(SendOverheadMessage, SEND_OVERHEAD_MESSAGE, OverheadMessage)

HMI_MSG_ALL_LAYOUT(GetOverheadMessage, GET_OVERHEAD_MESSAGE,
                   OverheadMessagePartId, OverheadMessage)

HMI_MSG_REP_LAYOUT(GetLineLoopDiagResults, GET_LINE_LOOP_DIAG_RESULTS,
                   GetLineLoopDiagResultsMsgRep)

HMI_MSG_ALL_LAYOUT(GetLineLoopDiagHlin, GET_LINE_LOOP_DIAG_HLIN,
                   GetLineLoopDiagHlinMsgReq, LineHlin)

HMI_MSG_ALL_LAYOUT(GetLineLoopDiagHlog, GET_LINE_LOOP_DIAG_HLOG,
                   GetLineLoopDiagHlogMsgReq, LineHlog)

HMI_MSG_ALL_LAYOUT(GetLineLoopDiagQln, GET_LINE_LOOP_DIAG_QLN,
                   GetLineLoopDiagQlnMsgReq, LineQln)

HMI_MSG_ALL_LAYOUT(GetLineLoopDiagSnr, GET_LINE_LOOP_DIAG_SNR,
                   GetLineLoopDiagSnrMsgReq, LineSnr)

HMI_MSG_ALL_LAYOUT(UpdateTestParameters, UPDATE_TEST_PARAMETERS,
                   UpdateTestParametersMsgReq, UpdateTestParametersMsgRep)

HMI_MSG_REP_LAYOUT(GetLineHlogNE, GET_LINE_HLOG_NE, LineHlogNE)

HMI_MSG_REP_LAYOUT(GetLineQlnNE, GET_LINE_QLN_NE, LineQlnNE)

HMI_MSG_ALL_LAYOUT(GetLineCarrierGains, GET_LINE_CARRIER_GAIN,
                   GetLineCarrierGainsMsgReq, LineCarrierGains)

HMI_MSG_ALL_LAYOUT(GetLineLinearTssi, GET_LINE_LINEAR_TSSI,
                   GetLineLinearTssiMsgReq, LineLinearTssi)

HMI_MSG_REQ_LAYOUT(LineIntroduceErrors, LINE_INTRODUCE_ERRORS,
                   LineIntroduceErrorsMsgReq)

HMI_MSG_NPL_LAYOUT(LineGoToL2, LINE_GO_TO_L2)

HMI_MSG_NPL_LAYOUT(LineGoToL0, LINE_GO_TO_L0)

HMI_MSG_REQ_LAYOUT(LineL2PowerTrim, LINE_L2_POWER_TRIM, LineL2PowerTrimMsgReq)

HMI_MSG_REQ_LAYOUT(LineTriggerOLR, LINE_TRIGGER_OLR, LineTriggerOLRMsgReq)
#endif /* #if 0 */

/* }}} */
