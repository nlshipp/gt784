/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiLineMsg.h                                       */
/*   Purpose         : Header for all line manager HMI message types          */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 18-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiLineMsg.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef HMI_LINE_MSG_H
# define HMI_LINE_MSG_H
#if 0
# include "bcm_types.h"
#endif
#include "bcm_hmiCoreMsg.h"    /* only for B0/B1 definition */

/* {{{ Application ID */
# define LINE_MANAGER_ID			0x0000
//# define LINE_MANAGER_ID(line) BCM_DEV_LINE_ID(line)

/* }}} */
/* {{{ Available services */
#if 0
# define RESET_LINE                              0x101
# define SET_LINE_TRAFFIC_CONFIGURATION          0x102
# define GET_LINE_TRAFFIC_CONFIGURATION          0x103
# define SET_LINE_PHYSICAL_LAYER_CONFIGURATION   0x104
# define GET_LINE_PHYSICAL_LAYER_CONFIGURATION   0x105
# define SET_LINE_TEST_CONFIGURATION             0x106
# define GET_LINE_TEST_CONFIGURATION             0x107
# define START_LINE                              0x108
# define STOP_LINE                               0x109
#endif
# define GET_LINE_COUNTERS                       0x10A
# define GET_LINE_STATUS                         0x10B
#if 0
# define GET_LINE_FEATURES                       0x10C
# define SET_LINE_PHY_CONFIG_VDSL                0x10D
# define GET_LINE_PHY_CONFIG_VDSL                0x10E
# define READ_EOC_REGISTER                       0x110
# define GET_EOC_READ_RESULT                     0x111
# define WRITE_EOC_REGISTER                      0x112
# define GET_EOC_WRITE_RESULT                    0x113
# define SEND_CLEAR_EOC_MESSAGE                  0x114
# define GET_CLEAR_EOC_MESSAGE                   0x115
# define GET_BAND_PLAN_VDSL                      0x118
# define GET_VDSL_ECHO                           0x119
# define ENTER_LINE_TEST_MODE                    0x120
# define EXIT_LINE_TEST_MODE                     0x121
# define START_LOOP_CHARACTERISATION             0x124
# define GET_LOOP_CHARACTERISATION               0x125
# define START_SIGNAL_GENERATION_AND_MEASUREMENT 0x126
# define GET_SIGNAL_MEASUREMENT                  0x127
# define START_MANUFACTURING_TEST                0x128
# define GET_MANUFACTURING_TEST_RESULT           0x129
# define GET_LINE_AGC_SETTING                    0x12A
# define GET_ECHO                                0x12D
# define GET_SELT                                0x130
# define GET_ECHO_VARIANCE                       0x12E
# define GET_LINE_BIT_ALLOCATION                 0x12F
# define GET_LINE_PERIOD_COUNTERS                0x131
# define SET_LINE_OAM_THRESHOLDS                 0x132
#endif
# define GET_LINE_CPE_VENDOR_INFO                0x133
#if 0
# define GET_LINE_SNR                            0x137
# define SEND_OVERHEAD_MESSAGE                   0x138
# define GET_OVERHEAD_MESSAGE                    0x139
# define GET_LINE_LOOP_DIAG_RESULTS              0x13B
# define GET_LINE_LOOP_DIAG_HLIN                 0x13C
# define GET_LINE_LOOP_DIAG_HLOG                 0x13D
# define GET_LINE_LOOP_DIAG_QLN                  0x13E
# define GET_LINE_LOOP_DIAG_SNR                  0x13F
# define UPDATE_TEST_PARAMETERS                  0x140
# define GET_LINE_HLOG_NE                        0x141
# define GET_LINE_QLN_NE                         0x142
# define GET_LINE_CARRIER_GAIN                   0x143
# define GET_LINE_LINEAR_TSSI                    0x144
# define LINE_INTRODUCE_ERRORS                   0x145
# define LINE_GO_TO_L2                           0x146
# define LINE_GO_TO_L0                           0x147
# define LINE_L2_POWER_TRIM                      0x148
# define LINE_TRIGGER_OLR                        0x149

/* }}} */
/* {{{ ResetLine service */

extern MsgLayout    ResetLineLayout;

/* }}} */
/* {{{ Set- and GetLineTrafficConfiguration services */

typedef struct BearerSpec BearerSpec;
struct BearerSpec
{
  uint32 minBitRate;           /* expressed in Kbit/s */
  uint32 maxBitRate;           /* expressed in Kbit/s */
  uint32 reservedBitRate;      /* expressed in Kbit/s */
  uint8  maxDelay;             /* max delay expressed in msec; 0 means FAST
                                 * channel */
  uint8  INPmin;               /* minimum required impulse noise protection
                                * allowed values are:
                                * 0: no protection
                                * 1: 0.5 DMT symbol  protection
                                * 2: 1   DMT symbol  protection
                                * 3: 2   DMT symbols protection
                                * 4: 4   DMT symbols protection
                                * 5: 8   DMT symbols protection
                                * 6: 16  DMT symbols protection */
  uint8  noINPcheck;           /* if set, no check of VTU-R INP settings in
                                * case VTU-R indicates erasure support */
  uint8  ratio_BC;             /* percentage of excess bandwith */
} BCM_PACKING_ATTRIBUTE ;

#ifdef CO_VCOPE
# define US 0                   /* UpStream link */
# define DS 1                   /* DownStream link */
#else
# define US 1                   /* UpStream link */
# define DS 0                   /* DownStream link */
#endif

# define BCM_DIR_NAME(dir) ((dir) == US ? "US" : "DS")

typedef struct BearerTrafficConfig BearerTrafficConfig;
struct BearerTrafficConfig
{
  BearerSpec  config[2];        /* index 0 is for US */
  int8        txAtmFlags;       /* normally zero; bit mask controlling the
                                 * following:
                                 * - 1<<0 : disable ATM payload scrambling
                                 * - 0<<1 : reserved (set to 0)
                                 * - 0<<2 : reserved (set to 0)
                                 * - 1<<3 : disable HEC regeneration
                                 * - 0<<4 : reserved (set to 0)
                                 * - 0<<5 : reserved (set to 0)
                                 * - 0<<6 : reserved (set to 0)
                                 * - 1<<7 : enable minimum FIFO length */
  int8        rxAtmFlags;       /* normally zero; bit mask controlling the
                                 * following:
                                 * - 1<<0 : disable scrambler
                                 * - 1<<1 : output cells during pre-sync
                                 * - 1<<2 : no cell discard on bad hec
                                 * - 1<<3 : no idle cell discard
                                 * - 1<<4 : force delineation to sync
                                 * - 1<<5 : disable unassigned cell filtering */
  int8        alpha;            /* alpha incorrect cells trigger a SYNC to HUNT
                                 * transition */
  int8        delta;            /* delta correct cells trigger a PRE-SYNC to
                                 * SYNC transition */
  uint32      minL2BitRate;     /* expressed in Kbit/s */
  uint16      L2LowRateSlots;   /* Number of time slots during with the
                                 * meanRate < L2min before switching to L2 
                                 * 0 means autonomous L2 disabled */
  uint8       L2Packet;         /* filling threshold to be reached before
                                 * leaving L2 */
  uint8       isEnabled;        /* 1 => the bearer channel is enabled (in this
                                 * case, this bearer should have been
                                 * correctly configured during the global
                                 * config */ 
} BCM_PACKING_ATTRIBUTE ;

typedef struct LineTrafficConfig LineTrafficConfig;
struct LineTrafficConfig
{
  BearerTrafficConfig bearer[2];
  uint8               tpsTcType; /* 2=PTM, 4=ATM, 6=both */
  uint8               reserved[3];
} BCM_PACKING_ATTRIBUTE ;

typedef LineTrafficConfig  SetLineTrafficConfigurationMsgReq;
extern  MsgLayout          SetLineTrafficConfigurationLayout;


typedef LineTrafficConfig  GetLineTrafficConfigurationMsgRep;
extern  MsgLayout          GetLineTrafficConfigurationLayout;

/* }}} */
/* {{{ Set- and GetLinePhysicalLayerConfiguration services */

# define BCM_TSSI_SHAPE_LEN 16
# define BCM_TSSI_BP_IDX(bp) (((bp)>>7)&0x1FF)
# define BCM_TSSI_BP_LOG(bp) ((bp)&0x7F)
# define BCM_TSSI_BP(idx, log) ((((idx)&0x1FF)<<7) | ((log)&0x7F))

typedef struct TssiShape TssiShape;
struct TssiShape
{
  uint16 breakPoint[BCM_TSSI_SHAPE_LEN]; /* 9 MSB is tone index [0;511]
                                          * 7 LSB is log tssi in -.5dB,
                                          * limited to [0, -62] dB */
  uint8  nrOfBreakPoints;       /* effective number of breakpoints */
  uint8  adsl2ProtocolType;     /* Bitmap: which protocol uses this shape
                                 * bit 0: G.992.3 AnnexA/B/J
                                 * bit 1: G.992.5 AnnexA/B/M
                                 * bit 2: G.992.3 AnnexL */
} BCM_PACKING_ATTRIBUTE ;
  
typedef struct LinkConfig LinkConfig;
struct LinkConfig
{
  int16  targetMarginDB;        /* unit is (0.1 dB) */
  int16  maxAddMarginDB;        /* unit is (0.1 dB) */
  int16  minMarginDB;           /* unit is (0.1 dB) */
  int8   rateAdaptationFlags;   /* bit 0: enable rate adaptation through
                                 *        retrain when margin < minMargin
                                 * bit 1: enable ADSL2 SRA */
  uint8  reserved;
  int16  downShiftNoiseMargin;  /* unit is (0.1 dB) */
  uint16 minimumDownshiftRAInterval;
  int16  upShiftNoiseMargin;    /* unit is (0.1 dB) */
  uint16 minimumUpshiftRAInterval;
  int16  maxTxPower;            /* unit is (0.1 dBm) */
} BCM_PACKING_ATTRIBUTE ;

# define PSD_NORMAL 0
# define PSD_PLUS   1
# define PSD_REACH  2

# define PHY_STANDARD_DS_PSD_NORMAL (-400)
# define PHY_STANDARD_DS_PSD_PLUS   (-400)
# define PHY_STANDARD_DS_PSD_REACH  (-370)

# define PHY_STANDARD_US_PSD_NORMAL (-380)
# define PHY_STANDARD_US_PSD_PLUS   (-380)
# define PHY_STANDARD_US_PSD_REACH  (-364)

# define PHY_US_PSD_NORMAL(phyConfig) (PHY_STANDARD_US_PSD_NORMAL + \
         phyConfig.protocolSpecificLinkConfig[US].maxTxDeltaPsd[PSD_NORMAL])
# define PHY_US_PSD_PLUS(phyConfig)   (PHY_STANDARD_US_PSD_PLUS + \
         phyConfig.protocolSpecificLinkConfig[US].maxTxDeltaPsd[PSD_PLUS])
# define PHY_US_PSD_REACH(phyConfig)  (PHY_STANDARD_US_PSD_REACH + \
         phyConfig.protocolSpecificLinkConfig[US].maxTxDeltaPsd[PSD_REACH])

# define PHY_DS_PSD_NORMAL(phyConfig) (PHY_STANDARD_DS_PSD_NORMAL + \
         phyConfig.protocolSpecificLinkConfig[DS].maxTxDeltaPsd[PSD_NORMAL])
# define PHY_DS_PSD_PLUS(phyConfig)   (PHY_STANDARD_DS_PSD_PLUS + \
         phyConfig.protocolSpecificLinkConfig[DS].maxTxDeltaPsd[PSD_PLUS])
# define PHY_DS_PSD_REACH(phyConfig)  (PHY_STANDARD_DS_PSD_REACH + \
         phyConfig.protocolSpecificLinkConfig[DS].maxTxDeltaPsd[PSD_REACH])

typedef struct SpecificLinkConfig SpecificLinkConfig;
struct SpecificLinkConfig
{
  int16 maxTxDeltaPsd[3];       /* delta PSD w.r.t. standard, defining the 
                                 * max PSD to apply: (unit is 0.1 dBm/Hz)
                                 * 0: normal (ADSL2)
                                 * 1: ADSL2PLUS (992.5)
                                 * 2: READSL (992.3 AnnexL) */
} BCM_PACKING_ATTRIBUTE ;
#endif /* #if 0 */

# define PHY_OPTION_DM_ENABLED (1<<4)

# define BCM_LOOPBACK_NONE        0
# define BCM_LOOPBACK_HYBRID      1
# define BCM_LOOPBACK_AFE         2
# define BCM_LOOPBACK_DSP         3
# define BCM_LOOPBACK_ATM         4
# define BCM_LOOPBACK_VDSL_ECHO   6
# define BCM_LOOPBACK_VDSL_HYBRID 7
# define BCM_LOOPBACK_VDSL_AFE    8

# define BCM_LOOPBACK_NAME(loopback) \
  ((loopback) == BCM_LOOPBACK_NONE        ? "Disabled"          : \
  ((loopback) == BCM_LOOPBACK_HYBRID      ? "Hybrid"            : \
  ((loopback) == BCM_LOOPBACK_AFE         ? "Digital_AFE_Chip"  : \
  ((loopback) == BCM_LOOPBACK_DSP         ? "Digital_DSP_Chip"  : \
  ((loopback) == BCM_LOOPBACK_ATM         ? "Digital_ATM"       : \
  ((loopback) == BCM_LOOPBACK_VDSL_ECHO   ? "Echo_VDSL"         : \
  ((loopback) == BCM_LOOPBACK_VDSL_HYBRID ? "Hybrid_VDSL"       : \
  ((loopback) == BCM_LOOPBACK_VDSL_AFE    ? "Digital_AFE_VDSL"  : \
  "Unknown_Loopback"))))))))

#if 0
typedef struct PRMconfig PRMconfig;
struct PRMconfig
{
  uint8  prmMode;               /* 0: no PRM 
                                 * 1: PRM @ 12kHz
                                 * 2: PRM @ 16kHz */
  uint8  reserved[3];
  uint32 prmPower;              /* 4000 limits the AGC to 12 dB */
} BCM_PACKING_ATTRIBUTE ;

typedef struct PhysicalLayerConfig PhysicalLayerConfig;
struct PhysicalLayerConfig
{
  LinkConfig linkConfig[2];     /* index 0 is for US */
  SpecificLinkConfig protocolSpecificLinkConfig[2]; /* index 0 is for US */
  int16 maxRxPower;             /* Max receive power (ADSL2 only) */
  uint8 fixedPSD;               /* if this flag is set, the PSD is kept to
                                 * min(PSDmax, TXDSP_REF_PSD)
                                 * bit0: flag for all protocol
                                 * bit1: flag for ADSL1 protocols only
                                 * bit2: flag for ADSL2 protocols only */ 
  uint8 powerAdaptivity;        /* used when fixedPSD == 0
                                 * 0: rate Adaptivity (get Max rate)
                                 * 1: power adaptivity  (get Minimal power) */
  uint8 optionFlags;            /* Bit[0]: Set this flag to enable spectrum 
                                 *         overlapping. 
                                 * Bit[2]: NTR support - Annex A/B only
                                 *         0 - NTR support enabled
                                 *         1 - NTR support disabled
                                 * Bit[4]: Diagnostic mode (ADSL2 only)
                                 *         0 - disabled
                                 *         1 - enabled
                                 * Bit[5]: Force use of low-pass Filter
                                 */
  uint8 loopbackMode;           /* Type of physical loopback used:
                                 *   0 means no loopback
                                 *   1 is hybrid loopback
                                 *   2 is external digital loopback
                                 *   3 is internal digital loopback
                                 *   4 is ATM loopback
                                 */
  uint8 carrierMaskUp[8];       /* Upstream carrier mask
                                 * Each bit corresponds to a tone, a 1 means
                                 * tone is present, a 0 means tone is masked. 
                                 * carrierMaskUp[i] LSB is mapped on tone i*8+0,
                                 * carrierMaskUp[i] MSB is mapped on tone i*8+7
                                 */
  uint8 carrierMaskDn[64];      /* Downstream carrier mask */
  uint8 actFailTimeout;         /* Timeout after which an activation failure
                                 * can be raised. Only affects the failures
                                 * 'HIGH_BIT_RATE', 'COMM_PROBLEM'
                                 * and 'NO_PEER'.
                                 * Unit is 10s. (i.e. actFailTimeout=10 means
                                 * 100s timeout). Values below 30s are clamped
                                 * upward to 30s. 
                                 */
  uint8 actFailThreshold[2];    /* Threshold on the activation failure count
                                 * that needs to be reached when the
                                 * activation timer expires to declare the
                                 * corresponding failure:
                                 * [0] -> threshold on 'HIGH_BIT_RATE'
                                 * [1] -> threshold on 'COMM_PROBLEM'
                                 * Note that a threshold equal to 0 means that
                                 * no timer is used: a failure is raised
                                 * immediately.
                                 * If the failure is already present, the
                                 * threhold used is half the configured value,
                                 * such as to have an hysteresis on the
                                 * persistency check */
  uint8 reservedA;
  uint8 adslPlusTxFilterId;     /* Additional Tx Filter setting for G.992.5: 
                                 * 0 or BCM6420: default value according to
                                 *    TxFilterConfig from core configuration.
                                 * [1..15] and BCM6421: use pre-defined filter
                                 *    for ADSL2+ from the cabinet deployement.
                                 *    The high-pass filter cutoff tone is equal
                                 *    to 130+10*adslPlusTxFilterId. */
  uint8 reservedB;
  uint8 automodePIalpha;        /* automoding alpha performance index */
  uint8 automodePIbeta;         /* automoding beta performance index */
  TssiShape customTssiShape;    /* custom TSSI shape */
  uint16 L0Time;                /* Minimum L0 time interval between L2 exit
                                 * and next L2 entry */
  uint16 L2Time;                /* Minimum L2 time interval between L2 entry
                                 * and first trim, and between trims */
  uint8 L2Atpr;                 /* Maximum Aggregate Transmit power reduction
                                 * per L2 trim. Range 0..L2Atpr */
  uint8 L2Atprt;                /* Maximum total Aggregate Transmit power
                                 * reduction in L2 mode. Range 0..15 */
  int8  hsTxPSD[3];             /* PSD level of G.hs carrier set
                                 * byte 0: A43 
                                 * byte 1: B43
                                 * byte 2: A43c/B43c */
  uint8 reservedC[3];
  PRMconfig prmConfig;
} BCM_PACKING_ATTRIBUTE ;

typedef PhysicalLayerConfig SetLinePhysicalLayerConfigurationMsgReq;
extern  MsgLayout           SetLinePhysicalLayerConfigurationLayout;


typedef PhysicalLayerConfig GetLinePhysicalLayerConfigurationMsgRep;
extern  MsgLayout           GetLinePhysicalLayerConfigurationLayout;

/* }}} */
/* {{{ Set- and GetLinePhyConfigVDSL services */

typedef struct BreakPoint BreakPoint;
struct BreakPoint
{
  uint16 toneIndex;
  int16  psd;                  /* unit is (0.1 dB) */
} BCM_PACKING_ATTRIBUTE ;

# define BCM_VDSL_NB_PSD_BP_DN 32
typedef struct PsdDescriptorDn PsdDescriptorDn;
struct PsdDescriptorDn
{
  uint8      nrOfBreakPoints;
  uint8      bypassOutbandPsdFilter;
  BreakPoint bp[BCM_VDSL_NB_PSD_BP_DN];
} BCM_PACKING_ATTRIBUTE ;

# define BCM_VDSL_NB_PSD_BP_UP 16
typedef struct PsdDescriptorUp PsdDescriptorUp;
struct PsdDescriptorUp
{
  uint8      nrOfBreakPoints;
  uint8      reserved;
  BreakPoint bp[BCM_VDSL_NB_PSD_BP_UP];
} BCM_PACKING_ATTRIBUTE ;

/* usefull macro to add a breakpoint to an existing PSD */
# define BCM_ADD_BREAKPOINT(list, idx, level)                              \
      do {(list)->bp[(list)->nrOfBreakPoints].toneIndex = (uint16) (idx);  \
          (list)->bp[(list)->nrOfBreakPoints].psd       = (int16) (level); \
          (list)->nrOfBreakPoints++;} while (0)

typedef struct ToneGroup ToneGroup;
struct ToneGroup
{
  uint16 endTone;
  uint16 startTone;
} BCM_PACKING_ATTRIBUTE ;

# define BCM_VDSL_NB_TONE_GROUPS 3 
typedef struct BandPlanDescriptor BandPlanDescriptor;
struct BandPlanDescriptor
{
  uint8     noOfToneGroups;
  uint8     reserved;
  ToneGroup toneGroups[BCM_VDSL_NB_TONE_GROUPS];
} BCM_PACKING_ATTRIBUTE ;

/* usefull macro to add a band to an existing bandplan */
# define BCM_ADD_BAND(bp, start, stop)                        \
      do {(bp)->toneGroups[(bp)->noOfToneGroups].startTone = (uint16) (start); \
          (bp)->toneGroups[(bp)->noOfToneGroups].endTone   = (uint16) (stop);  \
          (bp)->noOfToneGroups++;} while (0)

# define BCM_VDSL_NB_NOTCH_BANDS 8 
typedef struct NotchDescriptor NotchDescriptor;
struct NotchDescriptor
{
  uint8     noOfToneGroups;
  uint8     reserved;
  ToneGroup toneGroups[BCM_VDSL_NB_NOTCH_BANDS];
} BCM_PACKING_ATTRIBUTE ;

/* usefull macro to add a rfi band in kHz to an existing rfi description */
# define BCM_ADD_RFI_NOTCH(rfi, startFreq, stopFreq) \
  BCM_ADD_BAND(rfi, (startFreq)/4.3125, (stopFreq)/4.3125+1)

# define BCM_VDSL_NB_RFI_BANDS 16 
typedef struct RfiDescriptor RfiDescriptor;
struct RfiDescriptor
{
  uint8     noOfToneGroups;
  uint8     reserved;
  ToneGroup toneGroups[BCM_VDSL_NB_RFI_BANDS];
} BCM_PACKING_ATTRIBUTE ;

typedef struct PboSetting PboSetting;
struct PboSetting
{
  int16 a;
  int16 b;
} BCM_PACKING_ATTRIBUTE ;

typedef struct RefPsdDescriptor RefPsdDescriptor;
struct RefPsdDescriptor
{
  uint8      noOfToneGroups;
  uint8      reserved;
  PboSetting pbo[BCM_VDSL_NB_TONE_GROUPS];
  
} BCM_PACKING_ATTRIBUTE ;
  
/* usefull macro to add a pbo setting to an existing ref psd */
# define BCM_ADD_PBO_BAND(psd, valA, valB)                  \
  do {(psd)->pbo[(psd)->noOfToneGroups].a = (int16) (valA); \
      (psd)->pbo[(psd)->noOfToneGroups].b = (int16) (valB); \
      (psd)->noOfToneGroups++;} while (0)

typedef struct PhysicalLayerConfigVDSL PhysicalLayerConfigVDSL;
struct PhysicalLayerConfigVDSL
{
  PsdDescriptorDn    limitingMaskDn;
  PsdDescriptorUp    limitingMaskUp;
  BandPlanDescriptor bandPlanDn;
  BandPlanDescriptor bandPlanUp;
  NotchDescriptor    inBandNotches;
  RfiDescriptor      rfiNotches;
  RefPsdDescriptor   pboPsdUp;
  int16              forceElectricalLength; /* if !=-1, enforce US PBO
                                             * corresponding to a line whose
                                             * attenuation at 1MHz equals this
                                             * number (unit is .1 dB) */
  int16              maxTxPowerDn; /* unit is 0.1 dB */
} BCM_PACKING_ATTRIBUTE ;

typedef PhysicalLayerConfigVDSL SetLinePhyConfigVDSLMsgReq;
extern  MsgLayout               SetLinePhyConfigVDSLLayout;

typedef PhysicalLayerConfigVDSL GetLinePhyConfigVDSLMsgRep;
extern  MsgLayout               GetLinePhyConfigVDSLLayout;

/* }}} */
/* {{{ Set- and GetLineTestConfiguration services */

# define LINE_MODE_MASK_992_1_A   (1UL<< LINE_SEL_PROT_992_1_A  )
# define LINE_MODE_MASK_992_1_B   (1UL<< LINE_SEL_PROT_992_1_B  )
# define LINE_MODE_MASK_992_1_C   (1UL<< LINE_SEL_PROT_992_1_C  )
# define LINE_MODE_MASK_992_2_A   (1UL<< LINE_SEL_PROT_992_2_A  )
# define LINE_MODE_MASK_992_2_C   (1UL<< LINE_SEL_PROT_992_2_C  )
# define LINE_MODE_MASK_992_1_H   (1UL<< LINE_SEL_PROT_992_1_H  )
# define LINE_MODE_MASK_992_1_I   (1UL<< LINE_SEL_PROT_992_1_I  )
# define LINE_MODE_MASK_993_2     (1UL<< LINE_SEL_PROT_993_2    )
# define LINE_MODE_MASK_991_2_A   (1UL<< LINE_SEL_PROT_991_2_A  )
# define LINE_MODE_MASK_991_2_B   (1UL<< LINE_SEL_PROT_991_2_B  )
# define LINE_MODE_MASK_992_3_A   (1UL<< LINE_SEL_PROT_992_3_A  )
# define LINE_MODE_MASK_992_3_B   (1UL<< LINE_SEL_PROT_992_3_B  )
# define LINE_MODE_MASK_992_3_I   (1UL<< LINE_SEL_PROT_992_3_I  )
# define LINE_MODE_MASK_992_3_M   (1UL<< LINE_SEL_PROT_992_3_M  )
# define LINE_MODE_MASK_992_4_A   (1UL<< LINE_SEL_PROT_992_4_A  )
# define LINE_MODE_MASK_992_4_I   (1UL<< LINE_SEL_PROT_992_4_I  )
# define LINE_MODE_MASK_992_3_L1  (1UL<< LINE_SEL_PROT_992_3_L1 )
# define LINE_MODE_MASK_992_3_L2  (1UL<< LINE_SEL_PROT_992_3_L2 )
# define LINE_MODE_MASK_992_5_A   (1UL<< LINE_SEL_PROT_992_5_A  )
# define LINE_MODE_MASK_992_5_B   (1UL<< LINE_SEL_PROT_992_5_B  )
# define LINE_MODE_MASK_992_5_I   (1UL<< LINE_SEL_PROT_992_5_I  )
# define LINE_MODE_MASK_992_5_M   (1UL<< LINE_SEL_PROT_992_5_M  )
# define LINE_MODE_MASK_SADSL     (1UL<< LINE_SEL_PROT_SADSL    )
# define LINE_MODE_MASK_ANSI      (1UL<< LINE_SEL_PROT_ANSI     )
# define LINE_MODE_MASK_ETSI      (1UL<< LINE_SEL_PROT_ETSI     )

typedef struct TestConfigMessage TestConfigMessage;
struct TestConfigMessage
{
  uint32 disableProtocolBitmap; /* Bitmap to select allowed modes (see macros
                                 * LINE_MODE_MASK_XXX here above */
  uint32 reservedA;
  uint8  disableCoding;         /* bit0: trellis coding (0=enable, 1=disable)
                                 * bit1: Reed-Solomon coding (same) */
  uint8  restartMode;           /* Possible states are:
                                 * 0 - Default behavior.
                                 * 1 - When the Running Showtime state is 
				 *     left due to the presence of ten 
	                         *     consecutive SES, the modem goes to 
				 *     the Idle Configured state instead of 
				 *     going autonomously to the Running 
				 *     Activation state.
				 * 2 - Do not leave Running Showtime state 
				 *     in case of ten consecutive SES. When 
				 *     this option is active, the only way
				 *     to leave the Running Showtime state 
				 *     is to send the StopLine message. */
  uint8  disableShalfOption;    /* bit0: disable S=0.5 for ADSL1 modes
                                 * bit1: ignore GSPN CPE reporting of S=0.5
                                 *       support in R-MSG1 */
  uint8  maxFramingMode;        /* Possible states are:
				 * 0 - Default behavior: supports framing modes 
				 *     one, two and three.
				 * 1 - reserved.
				 * 2 - reserved.
				 * 3 - Maximum framing mode is three: modem
				 *     will support framing modes one, two and
				 *     three. */
  uint8  disableBitSwap;        /* bit0: US, bit1: DS */
  uint8  disableHighBitRateInGlite; /* bit0: US, bit1: DS */
  uint8  disableBitRateLimitation; /* VDSL2: Disable rate limitation */
  uint8  disableUS1bitConst;    /* ADSL2: Disable 1 bit constellation in US */
  uint8  reservedC;
  uint8  disableWindowing;      /* Disable the transmit windowing */
  uint8  disableShowtimeExit;   /* Disable exit from showtime if CPE doesn't
                                 * reply to GetCounter overhead message */
  uint8  disableNitro;          /* Disable BCM Nitro feature */
  uint8  disableUpdateTestParameters; /* Disable automatic test params update
                                 * (must be done manually) */
  uint8  msgMin[2];             /* Set the MSG min value, unit is kbs
                                 * index 0 is for US */
  uint8  disableLargeD;         /* Disable proprietary D selection via Nsif */
  uint32 maxL2BitRate[2];       /* Only applied when overruling L2 min/max.
                                 * Index 0 is for B0 */
  uint8  L2TestConfig;          /* bit0: disable autonomous L2 entry/exit
                                 * bit1: disable power trim
                                 * bit2: overrule L2 min/max rate calcul */
  uint8  L2MinAtpr;             /* Minimum Power Reduction at L2 entry. Valid
                                 * range: [0..15]. Will be clamped at
                                 * L2Atpr.*/
  uint8  forceSegmentation;     /* Force ovh messages with size > 10 to be
                                 * segmented (both request/response) */
  uint8  relaxInpMinCheck;      /* Always accept INPminDS=3 even if INPmin>3 */
  uint8  strictPerCheck;        /* when set, limit SRA such as to respect the
                                 * 15 to 20ms PER constraint */
  uint8  disableVdslUsPbo;      /* Disable VDSL US power back off, forcing the
                                 * CPE to use the max US PSD as template */
  uint8  dynamicInterleaver[2]; /* Enable support of ilv depth dynamic change
                                 * index 0 is for US */
  
} BCM_PACKING_ATTRIBUTE ;

typedef TestConfigMessage SetLineTestConfigurationMsgReq;
extern  MsgLayout         SetLineTestConfigurationLayout;


typedef TestConfigMessage GetLineTestConfigurationMsgRep;
extern  MsgLayout         GetLineTestConfigurationLayout;

/* }}} */
/* {{{ StartLine service */

extern MsgLayout    StartLineLayout;

/* }}} */
/* {{{ StopLine service */

extern MsgLayout    StopLineLayout;

/* }}} */
/* {{{ GetLineCounters service */
#endif /* #if 0 */

typedef struct DerivedSecCounters DerivedSecCounters;
struct DerivedSecCounters 
{
  uint32   FECS; /* FEC second */
  uint32   LOSS; /* LOS second */
  uint32   ES;   /* errored second: CRC-8 error or LOS or SEF (or LPR) */
  uint32   SES;  /* severely errored second : idem as above but CRC-8 > 18 */
  uint32   UAS;  /* unavailable second : more then 10 consecutive SES */
  uint32   AS;   /* available second = elapsed time - UAS_L */
  uint32   LOFS; /* LOF second */
  uint32   LPRS; /* LPR second - FE only (reserved for NE) */
} BCM_PACKING_ATTRIBUTE ;

typedef struct AdslAnomNeFeCounters AdslAnomNeFeCounters;
struct AdslAnomNeFeCounters 
{
  uint32   CV;                     /* CRC-8 count */
  uint32   FEC;                    /* FEC corrections */
  uint32   uncorrectableCodeword;  /* codewords that could not be corrected */
} BCM_PACKING_ATTRIBUTE ;

typedef struct AdslAnomCounters AdslAnomCounters;
struct AdslAnomCounters 
{
  AdslAnomNeFeCounters ne;         /* Near-End anomaly counters */
  AdslAnomNeFeCounters fe;         /* Far-End anomaly counters */
} BCM_PACKING_ATTRIBUTE ;

# define BCM_IDLE_CELLS(count) \
  (count.totalCells - count.totalUserCells - count.overflowCells)

typedef struct AtmPerfNeFeCounters AtmPerfNeFeCounters;
struct AtmPerfNeFeCounters
{
  uint32   totalCells;
  uint32   totalUserCells;
  uint32   hecErrors;
  uint32   overflowCells;
  uint32   idleCellBitErrors;
} BCM_PACKING_ATTRIBUTE ;

typedef struct AtmPerfCounters AtmPerfCounters;
struct AtmPerfCounters 
{
  AtmPerfNeFeCounters ne;
  AtmPerfNeFeCounters fe;
} BCM_PACKING_ATTRIBUTE ;

typedef struct PtmPerfCounters PtmPerfCounters;
struct PtmPerfCounters
{
  uint32 TC_0_CV;                  /* Priority 0 code violation */
  uint32 TC_1_CV;                  /* Priority 1 code violation */
};

# define NE 0
# define FE 1
# define BCM_SIDE_NAME(side) ((side) == NE ? "NE" : "FE")

/* PtmPerfCounters     ptmPerfCounts[2];      index 0 is for B0 */
typedef struct LineCounters LineCounters;
struct LineCounters
{
  DerivedSecCounters  derivedCounts[2];     /* index 0 is for NE */
  AdslAnomCounters    adslAnomalyCounts[2]; /* index 0 is for B0 */
  AtmPerfCounters     atmPerfCounts[2];     /* index 0 is for B0 */
  uint16 failedFullInitCount;      /* incremented each time an initialization
                                    * failure is reported in current period */
  uint16 fullInitCount;            /* incremented each time showtime is
                                    * entered */
  uint16 elapsedTimeSinceLastRead; /* number of seconds since
                                    * last read action */
  uint8  suspectFlagNE;            /* the suspect flag is activated either
                                    * - the first time PM is read after a
                                    *   Santorini reset or line reset.
                                    * - when the elapsed time between two
                                    *   reads is bigger than 1 hour. */
  uint8  suspectFlagFE;            /* the suspect flag is activated either
                                    * - the first time PM is read after a
                                    *   Santorini reset or line reset.
                                    * - when the elapsed time between two
                                    *   reads is bigger than 1 hour.
                                    * - the FE statistics are not reliable
                                    *   because of a IB were not reliable at
                                    *   some point in time during the last
                                    *   monitoring period.
                                    */
  uint32 upTime;                   /* nr of seconds already in showtime */
} BCM_PACKING_ATTRIBUTE ;

typedef LineCounters   GetLineCountersMsgRep;
extern  MsgLayout      GetLineCountersLayout;

/* }}} */
/* {{{ GetLineStatus service */



typedef struct BearerParams BearerParams;
struct BearerParams
{
  uint32 attainableBitRate;     /* maximum bit rate on the line */
  uint32 actualBitRate;         /* current bit rate */
  uint8  delayIlv;
  uint8  lp;                    /* Maps bearer to a Latency path */
  uint16 B;                     /* Nominal number of octets from this bearer
                                 * per MUX data frame */
} BCM_PACKING_ATTRIBUTE ;

typedef struct Rational16 Rational16;
struct Rational16
{
  uint16 num;                   /* numerator */
  uint16 denom;                 /* denominator */
} BCM_PACKING_ATTRIBUTE ;

typedef struct LatencyPathConfig LatencyPathConfig;
struct LatencyPathConfig
{
  Rational16    S;              /* S = number of PMD symbols over which the
                                 * FEC data frame spans (=1 for G.dmt fast
                                 * path, <=1 for ADSL2 fast path) */
  uint16        D;              /* interleaving depth: =1 for fast path */
  uint16        N;              /* RS codeword size */
  uint8         I;              /* VDSL only : Interleaver block length */
  uint8         R;              /* RS codeword overhead bytes */
  uint16        L;              /* Number of bits per symbol */        
  uint16        B[2];           /* Nominal number of bearer octets per MUX data
                                 * frame.  Index 0 is for Bearer B0 */
  uint8         M;              /* Number of MUX data frames per FEC data
                                 * frame (codeword) */
  uint8         T;              /* Number of MUX data frames per sync octet */
  uint8         G;              /* Number of overhead octets in superframe */
  uint8         U;              /* Number of overhead subframes in superframe */
  uint8         F;              /* Number of overhead frames in superframe */
  uint8         codingType;     /* codingType used for this latency
                                 * 0: No coding
                                 * 1: Trellis coding
                                 * 2: Reed-Solomon coding
                                 * 3: Concatenated (trellis and RS) coding
                                 */
} BCM_PACKING_ATTRIBUTE ;

# define LP0 0
# define LP1 1

# define LINE_STATUS_FAILURE_UAS               (1<< 1)
# define LINE_STATUS_FAILURE_NO_SHOWTIME       (1<< 2)
# define LINE_STATUS_FAILURE_LCD_B0            (1<< 6)
# define LINE_STATUS_FAILURE_LCD_B1            (1<< 7)
# define LINE_STATUS_FAILURE_LOS               (1<<12)
# define LINE_STATUS_FAILURE_LOF               (1<<13)
# define LINE_STATUS_FAILURE_NCD_B0            (1<<14)
# define LINE_STATUS_FAILURE_NCD_B1            (1<<15)
# define LINE_STATUS_FAILURE_LPR               (1<<18)
# define LINE_STATUS_FAILURE_CONFIG_ERROR      (1<<24)
# define LINE_STATUS_FAILURE_HIGH_BIT_RATE     (1<<25)
# define LINE_STATUS_FAILURE_COMM_PROBLEM      (1<<26)
# define LINE_STATUS_FAILURE_NO_PEER_DETECTED  (1<<27)
# define LINE_STATUS_FAILURE_LOM               (1<<28)
# define LINE_STATUS_FAILURE_L3_CPE            (1<<29)

typedef struct LinkStatus LinkStatus;
struct LinkStatus
{
  int16  loopAttenuation;
  int16  signalAttenuation;
  int16  snrMargin;
  int16  outputPower;
  int16  refPsd;                /* ADSL2 only: nominal PSD - power cutback */
  uint8  reservedA[2];
  uint32 actualLineBitRate;     /* ATM net data rate of all bearers plus all
                                 * overhead information */
  BearerParams bearer[2];       /* index 0 is for B0 */
  uint8  MSGc;                  /* nb octets in message-based portion of the
                                 * overhead framing structure (ADSL2 only) */
  uint8  MSGlp;                 /* ID of latency path carrying the
                                 * message-based overhead */
  uint8  reservedB[2];
  LatencyPathConfig latencyPathConfig[2]; /* Index 0 is for LP0 */
  uint32 currentFailures;       /* current failure state */
  uint32 changedFailures;       /* set of failures changed since last failures
                                 * read */
} BCM_PACKING_ATTRIBUTE ;

# define LINE_TRAFFIC_ATM        4
# define LINE_TRAFFIC_PTM        2
# define LINE_TRAFFIC_INACTIVE   1

# define LINE_STATUS_IDL_NCON    0
# define LINE_STATUS_IDL_CONF    1
# define LINE_STATUS_RUN_ACTI    2
# define LINE_STATUS_RUN_INIT    3
# define LINE_STATUS_RUN_SHOW    4
# define LINE_STATUS_TST_MODE    5
# define LINE_STATUS_RUN_LD_INIT 6
# define LINE_STATUS_IDL_LD_DONE 7
# define LINE_STATUS_RUN_SHOW_L2 8

# define BCM_LINE_STATE_NAME(state) \
  ((state) == LINE_STATUS_IDL_NCON    ? "Idle"                          : \
  ((state) == LINE_STATUS_IDL_CONF    ? "Configured"                    : \
  ((state) == LINE_STATUS_RUN_ACTI    ? "Running_Activation"            : \
  ((state) == LINE_STATUS_RUN_INIT    ? "Running_Initialisation"        : \
  ((state) == LINE_STATUS_RUN_SHOW    ? "Running_Showtime_L0"           : \
  ((state) == LINE_STATUS_TST_MODE    ? "Test_Mode"                     : \
  ((state) == LINE_STATUS_RUN_LD_INIT ? "Running_Loop_Diagnostics_Init" : \
  ((state) == LINE_STATUS_IDL_LD_DONE ? "Idle_Loop_Diagnostics_Done"    : \
  ((state) == LINE_STATUS_RUN_SHOW_L2 ? "Running_Showtime_L2"           : \
  "Unknown_State" )))))))))

# define BCM_LINE_INIT(state) \
  ((state) == LINE_STATUS_RUN_ACTI || (state) == LINE_STATUS_RUN_INIT)
# define BCM_LINE_SHOWTIME(state) \
  ((state) == LINE_STATUS_RUN_SHOW || (state) == LINE_STATUS_RUN_SHOW_L2)
# define BCM_LINE_RUNNING(state) \
   (BCM_LINE_INIT(state) || BCM_LINE_SHOWTIME(state))

# define LINE_SEL_PROT_NONE      -1
# define LINE_SEL_PROT_992_1_A    0
# define LINE_SEL_PROT_992_1_B    1
# define LINE_SEL_PROT_992_1_C    2
# define LINE_SEL_PROT_992_2_A    3
# define LINE_SEL_PROT_992_2_C    4
# define LINE_SEL_PROT_992_1_H    5
# define LINE_SEL_PROT_992_1_I    6
# define LINE_SEL_PROT_993_2      7
# define LINE_SEL_PROT_991_2_A    8
# define LINE_SEL_PROT_991_2_B    9
# define LINE_SEL_PROT_992_3_A   16
# define LINE_SEL_PROT_992_3_B   17
# define LINE_SEL_PROT_992_3_I   18
# define LINE_SEL_PROT_992_3_M   19
# define LINE_SEL_PROT_992_4_A   20
# define LINE_SEL_PROT_992_4_I   21
# define LINE_SEL_PROT_992_3_L1  22
# define LINE_SEL_PROT_992_3_L2  23
# define LINE_SEL_PROT_992_5_A   24
# define LINE_SEL_PROT_992_5_B   25
# define LINE_SEL_PROT_992_5_I   26
# define LINE_SEL_PROT_992_5_M   27
# define LINE_SEL_PROT_SADSL     29
# define LINE_SEL_PROT_ANSI      30
# define LINE_SEL_PROT_ETSI      31

# define BCM_SEL_PROTOCOL_NAME(prot) \
  ((prot) == LINE_SEL_PROT_NONE      ? "NONE"             : \
  ((prot) == LINE_SEL_PROT_992_1_A   ? "G.992.1_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_992_1_B   ? "G.992.1_Annex_B"  : \
  ((prot) == LINE_SEL_PROT_992_1_C   ? "G.992.1_Annex_C"  : \
  ((prot) == LINE_SEL_PROT_992_2_A   ? "G.992.2_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_992_2_C   ? "G.992.2_Annex_C"  : \
  ((prot) == LINE_SEL_PROT_992_1_H   ? "G.992.1_Annex_H"  : \
  ((prot) == LINE_SEL_PROT_992_1_I   ? "G.992.1_Annex_I"  : \
  ((prot) == LINE_SEL_PROT_993_2     ? "G.993.2 (VDSL2)"  : \
  ((prot) == LINE_SEL_PROT_991_2_A   ? "G.991.2_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_991_2_B   ? "G.991.2_Annex_B"  : \
  ((prot) == LINE_SEL_PROT_992_3_A   ? "G.992.3_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_992_3_B   ? "G.992.3_Annex_B"  : \
  ((prot) == LINE_SEL_PROT_992_3_I   ? "G.992.3_Annex_I"  : \
  ((prot) == LINE_SEL_PROT_992_3_M   ? "G.992.3_Annex_M"  : \
  ((prot) == LINE_SEL_PROT_992_4_A   ? "G.992.4_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_992_4_I   ? "G.992.4_Annex_I"  : \
  ((prot) == LINE_SEL_PROT_992_3_L1  ? "G.992.3_Annex_L1" : \
  ((prot) == LINE_SEL_PROT_992_3_L2  ? "G.992.3_Annex_L2" : \
  ((prot) == LINE_SEL_PROT_992_5_A   ? "G.992.5_Annex_A"  : \
  ((prot) == LINE_SEL_PROT_992_5_B   ? "G.992.5_Annex_B"  : \
  ((prot) == LINE_SEL_PROT_992_5_I   ? "G.992.5_Annex_I"  : \
  ((prot) == LINE_SEL_PROT_992_5_M   ? "G.992.5_Annex_M"  : \
  ((prot) == LINE_SEL_PROT_SADSL     ? "SADSL"            : \
  ((prot) == LINE_SEL_PROT_ANSI      ? "ANSI"             : \
  ((prot) == LINE_SEL_PROT_ETSI      ? "ETSI"             : \
  "Unknown_Protocol" ))))))))))))))))))))))))))

# define BCM_IS_SEL_PROT_ADSL2_MAIN(lineStatus) \
  ((lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_A || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_B || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_M)
# define BCM_IS_SEL_PROT_ADSL2_PLUS(lineStatus) \
  ((lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_A || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_B || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_M)
# define BCM_IS_SEL_PROT_ADSL2_REACH(lineStatus) \
  ((lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_L1 || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_L2)
# define BCM_IS_SEL_PROT_VDSL2(lineStatus) \
  ((lineStatus).selectedProtocol == LINE_SEL_PROT_993_2)

# define BCM_IS_SEL_PROT_ADSL2(lineStatus) \
   (BCM_IS_SEL_PROT_ADSL2_MAIN(lineStatus) || \
    BCM_IS_SEL_PROT_ADSL2_PLUS(lineStatus) || \
    BCM_IS_SEL_PROT_ADSL2_REACH(lineStatus))

/* double DS is also set for VDSL2, where tones are grouped by 8 -> also 512 */
# define BCM_IS_SEL_PROT_DOUBLE_DS(lineStatus) \
   (BCM_IS_SEL_PROT_ADSL2_PLUS(lineStatus) || \
    BCM_IS_SEL_PROT_VDSL2(lineStatus))

# define BCM_IS_SEL_PROT_DOUBLE_US(lineStatus) \
  ((lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_M || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_M || \
   (lineStatus).selectedProtocol == LINE_SEL_PROT_SADSL   )

# define BCM_NB_ADSL_TONE_DS      512
# define BCM_NB_ADSL_TONE_US       64
# define BCM_NB_VDSL_TONE        4096

# define BCM_ACTIVE_CARRIER_US(lineStatus) \
  (((lineStatus).selectedProtocol == LINE_SEL_PROT_ETSI    || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_992_1_B || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_B || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_B || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_992_3_M || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_992_5_M || \
    (lineStatus).selectedProtocol == LINE_SEL_PROT_SADSL   ) ? \
   BCM_NB_ADSL_TONE_US : BCM_NB_ADSL_TONE_US/2)

# define BCM_ACTIVE_CARRIER_DS(lineStatus) \
  (BCM_IS_SEL_PROT_DOUBLE_DS(lineStatus) ? \
   BCM_NB_ADSL_TONE_DS : BCM_NB_ADSL_TONE_DS/2)

typedef struct LineStatus LineStatus;
struct LineStatus
{
  uint8   state;                /* 4 LSB of LineMgr state */
  int8    selectedProtocol;     /* from HsNegotiation */
  uint8   lastRetrainCause;     /* bits[3:0] give the cause of the last init */
                                /* failure, according to G.997.1.
                                 * Possible values are:
                                 * 1 - wrong profile (can't occur in our system)
                                 * 2 - requested bit rate cannot be achieved
                                 * 3 - communication problem (CRC or time-out
                                 *     error)
                                 * 4 - no ATU-R detected
                                 * bits[7:4] give extra information in case of
                                 * a communication problem:
                                 * 0 - timeout during initialisation
                                 * 1 - CRC error during initialisation message
                                 * 2 - Unavailable Time decleared during
                                 *     showtime at NE
                                 * 3 - Dynamic Rate Adaptation triggered 
                                 *     retrain at NE
                                 * 4 - Unavailable Time decleared during
                                 *     showtime at FE
                                 * 5 - Dynamic Rate Adaptation triggered 
                                 *     retrain at FE
                                 * 6 - Far-End line counters unavailable (ADSL2)
                                 */
  uint8   loopbackMode;         /* copied from the physicalConfiguration */
  uint8   tpsTcType[2];         /* TPS-TC type.  Index 0 is for bearer B0.
                                 * Possible values:
                                 * 1 - Inactive bearer
                                 * 2 - PTM (VDSL2 only)
                                 * 4 - ATM */
  uint8   reserved[2];
  LinkStatus linkStatus[2];     /* index 0 is for US */  
} BCM_PACKING_ATTRIBUTE ;

typedef LineStatus   GetLineStatusMsgRep;
extern  MsgLayout    GetLineStatusLayout;

/* }}} */
/* {{{ GetLineFeatures service */

#if 0
typedef struct ProtocolType ProtocolType;
struct ProtocolType {
  uint32 adsl1;
  uint32 adsl2;
  uint32 vdsl;
} BCM_PACKING_ATTRIBUTE ;

typedef struct StandardsBitMap StandardsBitMap;
struct StandardsBitMap 
{
  uint32 bitMap;                /* Near-End supported standards */
  uint32 reserved;
} BCM_PACKING_ATTRIBUTE ;

typedef struct LineFeatures LineFeatures;
struct LineFeatures {
  StandardsBitMap supportedStandards[2]; /* index 0 is for NE */
  StandardsBitMap enabledStandards;      /* standards enabled as per test
                                          * config */
  ProtocolType    supportedOptions[2];   /* index 0 is for NE */
  ProtocolType    enabledOptions[2];     /* options enabled as per test config 
                                          * index 0 is for US */
} BCM_PACKING_ATTRIBUTE ;

typedef LineFeatures GetLineFeaturesMsgRep;
extern  MsgLayout    GetLineFeaturesLayout;

/* }}} */
/* {{{ ReadEocRegister service */

typedef struct EocRegisterId EocRegisterId;
struct EocRegisterId
{
  uint8 registerId;
} BCM_PACKING_ATTRIBUTE ;

typedef EocRegisterId  ReadEocRegisterMsgReq;
extern  MsgLayout      ReadEocRegisterLayout;

/* }}} */
/* {{{ GetEocReadResult service */

typedef struct EocReadResult EocReadResult;
struct EocReadResult
{
  uint8 eocReadStatus;
  uint8 length;
  uint8 eocData[32];
} BCM_PACKING_ATTRIBUTE ;

typedef EocReadResult  GetEocReadResultMsgRep;
extern  MsgLayout      GetEocReadResultLayout;

/* }}} */
/* {{{ WriteEocRegister service */

typedef struct EocRegister EocRegister;
struct EocRegister
{
  uint8 registerId;
  uint8 length;
  uint8 eocData[32];
} BCM_PACKING_ATTRIBUTE ;

typedef EocRegister  WriteEocRegisterMsgReq;
extern  MsgLayout    WriteEocRegisterLayout;

/* }}} */
/* {{{ GetEocWriteResult service */

typedef struct EocWriteResult EocWriteResult;
struct EocWriteResult
{
  uint8 eocWriteStatus;
  uint8 length;
} BCM_PACKING_ATTRIBUTE ;

typedef EocWriteResult GetEocWriteResultMsgRep;
extern  MsgLayout      GetEocWriteResultLayout;

/* }}} */
/* {{{ Send- and GetClearEocMessage services */

typedef struct ClearEocMessage ClearEocMessage;
struct ClearEocMessage
{
  uint16 length;
  uint8  message[486];
} BCM_PACKING_ATTRIBUTE ;

typedef ClearEocMessage  SendClearEocMessageMsgReq;
extern  MsgLayout        SendClearEocMessageLayout;

typedef ClearEocMessage  GetClearEocMessageMsgRep;
extern  MsgLayout        GetClearEocMessageLayout;

/* }}} */
/* {{{ GetBandPlanVDSL service */

typedef struct GetBandPlanVDSLMsgRep GetBandPlanVDSLMsgRep;
struct GetBandPlanVDSLMsgRep {
  BandPlanDescriptor bandPlanDn;
  BandPlanDescriptor bandPlanUp;
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout    GetBandPlanVDSLLayout;

/* }}} */
/* {{{ EnterLineTestMode service */

typedef struct TestModeSelection TestModeSelection;
struct TestModeSelection
{
  uint8 filterMode;             /* indicates type of high-pass filter for ADSL
                                 * signals (0: Annex-A, 1: Annex-B */
} BCM_PACKING_ATTRIBUTE ;

typedef TestModeSelection  EnterLineTestModeMsgReq;
extern  MsgLayout          EnterLineTestModeLayout;

/* }}} */
/* {{{ ExitLineTestMode service */

extern MsgLayout    ExitLineTestModeLayout;

/* }}} */
/* {{{ StartLoopCharacterisation service */

typedef struct StartLoopCharacterisationMsgReq StartLoopCharacterisationMsgReq;
struct StartLoopCharacterisationMsgReq
{
  uint8 measurementPeriod;      /* effective period = 10*2^measurementPeriod */
  uint8 agcSetting;             /* 0: variable AGC setting
                                 * 1-6: fixed AGC at (-12+agcSetting*6) dB */
} BCM_PACKING_ATTRIBUTE ;

extern MsgLayout    StartLoopCharacterisationLayout;

/* }}} */
/* {{{ GetLoopCharacterisation service */

typedef struct LoopCharacteristics LoopCharacteristics;
struct LoopCharacteristics
{
  uint32 testStatus;            /* 0: test not yet started
                                 * 1: test in progress
                                 * 2: test completed */
} BCM_PACKING_ATTRIBUTE ;

typedef LoopCharacteristics GetLoopCharacteristicsMsgRep;
extern  MsgLayout           GetLoopCharacteristicsLayout;

/* }}} */
/* {{{ StartSignalGenerationAndMeasurement service */

# define TESTSIGNAL_TYPE_REVERB        3
# define TESTSIGNAL_TYPE_MEDLEY        4
# define TESTSIGNAL_TYPE_QUIET         5
# define TESTSIGNAL_TYPE_TRAINING_VDSL 6
# define TESTSIGNAL_TYPE_MEDLEY_VDSL   7
# define TESTSIGNAL_TYPE_QUIET_VDSL    8
# define TESTSIGNAL_TYPE_PERIODIC_VDSL 9

typedef struct SignalGeneration SignalGeneration;
struct SignalGeneration
{
  uint8    signalType;          /* see TESTSIGNAL_TYPE_... */
  uint8    crestControl;        /* control crest factor of signal 3&6 */
  int16    signalPsdAdsl; 
  uint32   dutyCycleON;
  uint32   dutyCycleOFF;
  uint8    carrierMaskAdsl[64];
  uint8    filterConfig;        /* bit0: bypass decimator
                                 * bit1: enable extra Tx LPF */
  uint8    measurementLength;   /* signal measurement over a length equal to
                                 * 2^measurementlength */
  uint8    reservedA;
  uint8    adslPlusTxFilterId;  /* Additional Tx Filter setting for G.992.5 */ 
  TssiShape customTssiShapeAdsl; /* custom TSSI shape for ADSL signals */
  BandPlanDescriptor bandPlanDn; /* band plan DS for VDSL signals */
  BandPlanDescriptor bandPlanUp; /* band plan US for VDSL signals */
  PsdDescriptorDn    limitingMaskDn;
  int16   maxTxPowerVdsl;       /* max Tx Power for VDSL signals. 
                                 * Unit is 1/256 dB */
  int8    reservedB[2];
} BCM_PACKING_ATTRIBUTE ;

typedef SignalGeneration  StartSignalGenerationAndMeasurementMsgReq;
extern  MsgLayout         StartSignalGenerationAndMeasurementLayout;

/* }}} */
/* {{{ GetSignalMeasurement service */

typedef struct GetSignalMeasurementMsgReq GetSignalMeasurementMsgReq;
struct GetSignalMeasurementMsgReq
{
  uint8 partId;                 /* part of the measurement vector to retrieve,
                                 * in chunks of 64 tones */
  uint8 toneGroupingFactor;     /* Speed up factor: only retrieves tones
                                 * multiple of (1<<toneGroupingFactor) */
} BCM_PACKING_ATTRIBUTE ;

typedef struct SignalMeasurement SignalMeasurement;
struct SignalMeasurement
{
  uint32 testStatus;            /* 0: test not yet started
                                 * 1: test in progress
                                 * 2: test completed */
  uint32 testDuration;          /* Duration in seconds of the peak
                                 * noise measurement (reported below) */
  /* all PSD values are expressed in dBm referred to 100Ohm line / Tonebin */
  int16   peakPsd[64];          /*  max (x^2+y^2)/T,
                                 *  where T = peakMeasurementTime seconds */
  int16   totalPsd[64];         /*  sum(x^2+y^2)/T,  where T = 1 sec.      */
  int16   signalPsd[64];        /* (sum(x)^2 sum(y)^2)/T, where T = 1 sec */
  uint8   partId;               /*  acknowledgement of partId in request */
  uint8   reserved[3];
} BCM_PACKING_ATTRIBUTE ;

typedef SignalMeasurement GetSignalMeasurementMsgRep;
extern  MsgLayout         GetSignalMeasurementLayout;

/* }}} */
/* {{{ StartManufacturingTest service */

typedef struct ManufTestConfig ManufTestConfig;
struct ManufTestConfig
{
  uint8  loopbackMode;          /* see physical configuration */
  int8   crestControl;          /* control the crest factor of the signal */
                                /* used for the high crest THD test */
  uint16 duration;              /* in second */
  uint8  loopbackToneStart;
  uint8  loopbackToneStop;
  uint8  skipAFEtest;
  uint8  reserved;
} BCM_PACKING_ATTRIBUTE ;

typedef ManufTestConfig StartManufacturingTestMsgReq;
extern  MsgLayout       StartManufacturingTestLayout;

/* }}} */
/* {{{ GetManufacturingTestResult service */

typedef struct ManufacturingTestResult ManufacturingTestResult;
struct ManufacturingTestResult
{
  uint32 testStatus;            /* 0: test not yet started
                                 * 1: test in progress
                                 * 2: test completed */
  uint32 loopbackRate;
  uint32 loopbackBits;
  uint32 loopbackErrors;
  int32  THRLmean;
  int32  THRLdip;
  int32  THDandNoise[3];        /* THD and Noise measrured in US band.
                                 * Index 0 is for VDSL US0 band or ADSL mode
                                 * Index 1 is for VDSL US1 band
                                 * Index 2 is for VDSL US2 band
                                 * Unit is 0.1 dB */
  int32  THDdeltaHighCrest[3];  /* THD increase due to the high crest signal */
                                /* used. Expressed as a delta in 0.1 dB unit */
                                /* with respect to the THD result provided */
                                /* in the same test. */
  int32  agc;
} BCM_PACKING_ATTRIBUTE ;

typedef ManufacturingTestResult GetManufacturingTestResultMsgRep;
extern  MsgLayout               GetManufacturingTestResultLayout;

/* }}} */
/* {{{ GetLineAGCsetting service */

typedef struct AGCsetting AGCsetting;
struct AGCsetting
{
  int16 agc;                    /* Receiver gain used during last test */
} BCM_PACKING_ATTRIBUTE ;

typedef AGCsetting GetLineAGCsettingMsgRep;
extern  MsgLayout  GetLineAGCsettingLayout;

/* }}} */
/* {{{ GetEcho service */

typedef struct EchoPartId EchoPartId;
struct EchoPartId
{
  uint32 partId;
} BCM_PACKING_ATTRIBUTE ;

typedef struct AccurateEcho AccurateEcho;
struct AccurateEcho
{
  int32 partialEchoResponse[64];
} BCM_PACKING_ATTRIBUTE ;

typedef EchoPartId   GetEchoMsgReq;
typedef AccurateEcho GetEchoMsgRep;
extern  MsgLayout    GetEchoLayout;

/* }}} */
/* {{{ GetVDSLEcho service */

typedef struct VDSLechoPartId VDSLechoPartId;
struct VDSLechoPartId
{
  int16 partId;
} BCM_PACKING_ATTRIBUTE ;

typedef struct AccurateVDSLecho AccurateVDSLecho;
struct AccurateVDSLecho
{
  int32 testStatus;
  int16 numRxTones;
  int16 numTxTones;
  int16 partialVDSLechoMant[128];
  int8  partialVDSLechoExp[64];
} BCM_PACKING_ATTRIBUTE ;

typedef VDSLechoPartId   GetVDSLechoMsgReq;
typedef AccurateVDSLecho GetVDSLechoMsgRep;
extern  MsgLayout        GetVDSLechoLayout;
/* }}} */
/* {{{ GetSelt service */

typedef struct SeltPartId SeltPartId;
struct SeltPartId
{
  uint32 partId;
} BCM_PACKING_ATTRIBUTE ;

typedef struct SeltResult SeltResult;
struct SeltResult
{
  int32 seltValue[64];
} BCM_PACKING_ATTRIBUTE;

typedef SeltPartId GetSeltMsgReq;
typedef SeltResult GetSeltMsgRep;
extern  MsgLayout  GetSeltLayout;

/* }}} */
/* {{{ GetEchoVariance service */

struct AccurateEchoVariance
{
  int16 varValues[128];
} BCM_PACKING_ATTRIBUTE ;

typedef EchoPartId   GetEchoVarianceMsgReq;
typedef struct AccurateEchoVariance GetEchoVarianceMsgRep;
extern  MsgLayout    GetEchoVarianceLayout;

/* }}} */
/* {{{ GetLineBitAllocation service */

typedef struct LineBitAllocAndSnrReq LineBitAllocAndSnrReq;
struct LineBitAllocAndSnrReq
{
  uint8 reserved;
  uint8 partId;                 /* bits [6:0]: specifies the toneBlock.
                                 *      Data for tones toneBlock*488 to
                                 *      toneBlock*488+487 will be retrieved.
                                 * bit 7: 0 for US, 1 for DS */
} BCM_PACKING_ATTRIBUTE ;

typedef struct GetLineBitAllocationMsgRep GetLineBitAllocationMsgRep;
struct GetLineBitAllocationMsgRep
{
  uint8  bitAllocTable[488];
} BCM_PACKING_ATTRIBUTE ;

typedef LineBitAllocAndSnrReq GetLineBitAllocationMsgReq;
extern  MsgLayout GetLineBitAllocationLayout;

/* }}} */
/* {{{ GetLineSnr service */

/* request part uses LineBitAllocAndSnrReq, with each toneBlock having 244 */
/* tones (and only US is supported) */
typedef struct GetLineSnrMsgRep GetLineSnrMsgRep;
struct GetLineSnrMsgRep
{
  uint16 snr[244];
} BCM_PACKING_ATTRIBUTE ;

typedef LineBitAllocAndSnrReq GetLineSnrMsgReq;
extern  MsgLayout             GetLineSnrLayout;

/* }}} */
/* {{{ GetLinePeriodCounters service */

# define PERIOD_CURRENT_15_MIN    0
# define PERIOD_PREVIOUS_15_MIN   1
# define PERIOD_CURRENT_24_HOURS  2
# define PERIOD_PREVIOUS_24_HOURS 3

# define BCM_COUNTER_PERIOD_NAME(period) \
  ((period) == PERIOD_CURRENT_15_MIN  ?   "Current  15 min" : \
  ((period) == PERIOD_PREVIOUS_15_MIN ?   "Previous 15 min" : \
  ((period) == PERIOD_CURRENT_24_HOURS ?  "Current  24 hrs" : \
  ((period) == PERIOD_PREVIOUS_24_HOURS ? "Previous  24 hrs" : \
  "unknown"))))

typedef struct PeriodIdentification PeriodIdentification;
struct PeriodIdentification
{
  uint32   periodIdentification;   /* Flag indicating which OAM report period
                                    * 0: current  15 min
                                    * 1: previous 15 min
                                    * 2: current  24 hour
                                    * 3: previous 24 hour
                                    */
} BCM_PACKING_ATTRIBUTE ;

typedef struct PeriodCounters PeriodCounters;
struct PeriodCounters
{
  DerivedSecCounters  derivedCounts[2];     /* index 0 is for NE */
  AdslAnomCounters    adslAnomalyCounts[2]; /* index 0 is for B0 */
  AtmPerfCounters     atmPerfCounts[2];     /* index 0 is for B0 */
  PtmPerfCounters     ptmPerfCounts[2];     /* index 0 is for B0 */

  uint16 failedFullInitCount;      /* incremented each time an initialization
                                    * failure is reported in current period */
  uint16 fullInitCount;            /* incremented each time showtime is
                                    * entered */
  uint16 reserved;
  uint8  suspectFlagNE;            /* the suspect flag is activated either
                                    * - the first time PM is read after a
                                    *   Santorini reset or line reset.
                                    * - when the elapsed time between two
                                    *   reads is bigger than 1 hour. */
  uint8  suspectFlagFE;            /* the suspect flag is activated either
                                    * - the first time PM is read after a
                                    *   Santorini reset or line reset.
                                    * - when the elapsed time between two
                                    *   reads is bigger than 1 hour.
                                    * - the FE statistics are not reliable
                                    *   because of a IB were not reliable at
                                    *   some point in time during the last
                                    *   monitoring period.
                                    */
  uint32 upTime;                   /* nr of seconds already in showtime */
  uint32 periodIdentification;
  uint32 elapsedTimeIn15min;
  uint32 elapsedTimeIn24hour;
} BCM_PACKING_ATTRIBUTE ;

typedef PeriodIdentification GetLinePeriodCountersMsgReq;
typedef PeriodCounters       GetLinePeriodCountersMsgRep;
extern  MsgLayout            GetLinePeriodCountersLayout;

/* }}} */
/* {{{ SetLineOAMthresholds service */

# define BCM_ES_MASK        (1<<0)
# define BCM_SES_MASK       (1<<1)
# define BCM_UAS_MASK       (1<<2)

typedef struct OAMthresholds OAMthresholds;
struct OAMthresholds
{
  uint32  ESthreshold;
  uint32  SESthreshold;
  uint32  UASthreshold;
} BCM_PACKING_ATTRIBUTE ;

typedef struct NewThresholds NewThresholds;
struct NewThresholds
{
  OAMthresholds   threshold15min[2]; /* NE-FE */
  OAMthresholds   threshold24hour[2]; /* NE-FE */
} BCM_PACKING_ATTRIBUTE ;

typedef NewThresholds SetLineOAMthresholdsMsgReq;
extern  MsgLayout     SetLineOAMthresholdsLayout;
#endif /* #if 0 */

/* }}} */
/* {{{ GetLineCPEvendorInfo service */

/* vendor info is MSByte first as per standard */
# define BCM_GET_VENDOR_INFO(info) \
  (((info) [0]<<8) + (info)[1])

/* country code: if first byte != 255, ignore second byte */
# define BCM_GET_COUNTRY_CODE(code) \
  ((code[0]) == 0xFF ? (code)[1] : (code)[0])

typedef struct VendorId VendorId;
struct VendorId
{
  uint8  countryCode[2];
  uint8  providerCode[4];
  uint8  vendorInfo[2];
} BCM_PACKING_ATTRIBUTE ;

typedef struct CPEparamsInfo CPEparamsInfo;
struct CPEparamsInfo
{
  uint32 minBitRate;
  uint32 maxBitRate;
  uint8  maxDelay;
  uint8  INPmin;
  uint8  msgMin;
  uint8  reserved;
} BCM_PACKING_ATTRIBUTE ;

typedef struct CPEvendorInfo CPEvendorInfo;
struct CPEvendorInfo
{
  VendorId  hsVendorId;
  uint16    t1_413_vendorInfo;
  uint8     availInfo;          /* bitmap indicating which info is available
                                 * bit [0] : hs available
                                 * bit [1] : eocVendorId avail
                                 */
  uint8     reserved;
  CPEparamsInfo params[2];      /* index 0 is for US */
} BCM_PACKING_ATTRIBUTE ;

typedef CPEvendorInfo GetLineCPEvendorInfoMsgRep;
extern  MsgLayout     GetLineCPEvendorInfoLayout;

#if 0
/* }}} */
/* {{{ Send- and GetOverheadMessage service */

#define MAX_OVERHEAD_MESSAGE_BUFFER 256
typedef struct OverheadMessage OverheadMessage;
struct OverheadMessage
{
  int16  length;
  uint8  message[MAX_OVERHEAD_MESSAGE_BUFFER];
} BCM_PACKING_ATTRIBUTE ;

typedef OverheadMessage SendOverheadMessageMsgReq;
extern  MsgLayout       SendOverheadMessageLayout;

typedef struct OverheadMessagePartId OverheadMessagePartId;
struct OverheadMessagePartId
{
  uint8 partId;                 /* 0: get bytes    0 -  255
                                 * 1: get bytes  256 -  511
                                 * 2: get bytes  512 -  767
                                 * 3: get bytes  768 - 1023
                                 * 4: get bytes 1024 - 1279
                                 * 5: get bytes 1280 - 1535
                                 * 6: get bytes 1536 - 1791
                                 * 7: get bytes 1792 - 2047
                                 */
} BCM_PACKING_ATTRIBUTE ;

typedef OverheadMessagePartId GetOverheadMessageMsgReq;
typedef OverheadMessage       GetOverheadMessageMsgRep;
extern  MsgLayout             GetOverheadMessageLayout;

/* }}} */
/* {{{ GetLineLoopDiagResults service */

typedef struct LoopDiagResults LoopDiagResults;
struct LoopDiagResults
{
  uint32 attainableBitRate;
  uint16 loopAttenuation;
  uint16 signalAttenuation;
  int16  snrMargin;
  int16  actualTxPowerFE;
} BCM_PACKING_ATTRIBUTE ;

typedef struct GetLineLoopDiagResultsMsgRep GetLineLoopDiagResultsMsgRep;
struct GetLineLoopDiagResultsMsgRep
{
  LoopDiagResults ne;
  LoopDiagResults fe;
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout GetLineLoopDiagResultsLayout;

/* }}} */
/* {{{ GetLineLoopDiagHlin service */

typedef struct GetLineLoopDiagHlinMsgReq GetLineLoopDiagHlinMsgReq;
struct GetLineLoopDiagHlinMsgReq
{
  uint8 direction;              /* 0 = NE, 1 = FE */
  uint8 partId;                 /* 0 for NE, 0-7 in case of FE */
} BCM_PACKING_ATTRIBUTE ;

typedef struct LineHlin LineHlin;
struct LineHlin
{
  uint16       hlinScale;
  ComplexInt16 hlin[64];
} BCM_PACKING_ATTRIBUTE ;

typedef LineHlin       GetLineLoopDiagHlinMsgRep;
extern  MsgLayout      GetLineLoopDiagHlinLayout;

/* }}} */
/* {{{ GetLineLoopDiagHlog service */

typedef struct GetLineLoopDiagHlogMsgReq GetLineLoopDiagHlogMsgReq;
struct GetLineLoopDiagHlogMsgReq
{
  uint8 partId;                 /* 0-3 */
} BCM_PACKING_ATTRIBUTE ;

typedef uint16 LineHlogNE[64];
typedef uint16 LineHlogFEpart[128];

typedef struct LineHlog LineHlog;
struct LineHlog
{
  LineHlogNE     hlogNE;        /* Near end Hlog */
  LineHlogFEpart hlogFE;        /* Far end Hlog (1/4 part) */
} BCM_PACKING_ATTRIBUTE ;

typedef LineHlog       GetLineLoopDiagHlogMsgRep;
extern  MsgLayout      GetLineLoopDiagHlogLayout;

/* }}} */
/* {{{ GetLineLoopDiagQln service */

typedef struct GetLineLoopDiagQlnMsgReq GetLineLoopDiagQlnMsgReq;
struct GetLineLoopDiagQlnMsgReq
{
  uint8 partId;                 /* 0-1 */
} BCM_PACKING_ATTRIBUTE ;

typedef uint8 LineQlnNE[64];
typedef uint8 LineQlnFEpart[256];

typedef struct LineQln LineQln;
struct LineQln
{
  LineQlnNE     qlnNE;          /* Near end QLN */
  LineQlnFEpart qlnFE;          /* Far end QLN (1/2 part) */
} BCM_PACKING_ATTRIBUTE ;

typedef LineQln        GetLineLoopDiagQlnMsgRep;
extern  MsgLayout      GetLineLoopDiagQlnLayout;

/* }}} */
/* {{{ GetLineLoopDiagSnr service */

typedef struct GetLineLoopDiagSnrMsgReq GetLineLoopDiagSnrMsgReq;
struct GetLineLoopDiagSnrMsgReq
{
  uint8 partId;                 /* 0-1 */
} BCM_PACKING_ATTRIBUTE ;

typedef uint8 LineSnrNE[64];
typedef uint8 LineSnrFEpart[256];

typedef struct LineSnr LineSnr;
struct LineSnr
{
  LineSnrNE     snrNE;          /* Near end SNR */
  LineSnrFEpart snrFE;          /* Far end SNR (1/2 part) */
} BCM_PACKING_ATTRIBUTE ;

typedef LineSnr        GetLineLoopDiagSnrMsgRep;
extern  MsgLayout      GetLineLoopDiagSnrLayout;

/* }}} */
/* {{{ UpdateTestParameters service */

# define BCM_TEST_PARAM_SHOW_STATUS 0
# define BCM_TEST_PARAM_INVALIDATE  1

typedef struct UpdateTestParametersMsgReq UpdateTestParametersMsgReq;
struct UpdateTestParametersMsgReq
{
  uint8 command;                /* 0: read status of test parameters
                                 * 1: invalidate test parameters */
} BCM_PACKING_ATTRIBUTE ;

typedef struct UpdateTestParametersMsgRep UpdateTestParametersMsgRep;
struct UpdateTestParametersMsgRep
{
  uint8 status;                 /* 0: test parameters are present
                                 * 1: test parameters are invalidated */
} BCM_PACKING_ATTRIBUTE ;
extern MsgLayout    UpdateTestParametersLayout;

/* }}} */
/* {{{ GetLineHlogNE service */

typedef struct GetLineHlogNEMsgRep GetLineHlogNEMsgRep;
struct GetLineHlogNEMsgRep
{
  LineHlogNE hlogNE;
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout      GetLineHlogNELayout;

/* }}} */
/* {{{ GetLineQlnNE service */

typedef struct GetLineQlnNEMsgRep GetLineQlnNEMsgRep;
struct GetLineQlnNEMsgRep
{
  LineQlnNE qlnNE;
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout      GetLineQlnNELayout;

/* }}} */
/* {{{ GetLineCarrierGains service */

typedef struct GetLineCarrierGainsMsgReq GetLineCarrierGainsMsgReq;
struct GetLineCarrierGainsMsgReq
{
  uint8 partId;                 /* bits [6:0]: specifies the toneBlock.  
                                 *      Data for tones toneBlock*244 to
                                 *      toneBlock*244+243 will be retrieved.
                                 * bit 7: 0 for US, 1 for DS */
} BCM_PACKING_ATTRIBUTE ;

typedef struct LineCarrierGains LineCarrierGains;
struct LineCarrierGains
{
  uint16     gi[244];           /* Unit is 1/512 dB */
} BCM_PACKING_ATTRIBUTE ;

typedef LineCarrierGains GetLineCarrierGainsMsgRep;
extern  MsgLayout        GetLineCarrierGainsLayout;

/* }}} */
/* {{{ GetLineLinearTssi service */

typedef struct GetLineLinearTssiMsgReq GetLineLinearTssiMsgReq;
struct GetLineLinearTssiMsgReq
{
  uint8 partId;                 /* 0-3 */
} BCM_PACKING_ATTRIBUTE ;

typedef uint16 LineTssiUS[64];  /* unit is 1/32768 */
typedef uint16 LineTssiDSpart[128];

typedef struct LineLinearTssi LineLinearTssi;
struct LineLinearTssi
{
  LineTssiUS     tssiUS;            /* US linear TSSI values */
  LineTssiDSpart tssiDS;            /* DS linear TSSI values (1/4 part) */
} BCM_PACKING_ATTRIBUTE ;

typedef LineLinearTssi GetLineLinearTssiMsgRep;
extern  MsgLayout      GetLineLinearTssiLayout;

/* }}} */
/* {{{ LineIntroduceErrors service */

typedef struct LineIntroduceErrorsMsgReq LineIntroduceErrorsMsgReq;
struct LineIntroduceErrorsMsgReq
{
  uint8 errorType;
  uint8 numberOfErrors;
  uint8 latencyPath;
  uint8 bearer;
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout LineIntroduceErrorsLayout;

/* }}} */
/* {{{ LineGoToL2 service */

extern MsgLayout    LineGoToL2Layout;

/* }}} */
/* {{{ LineGoToL0 service */

extern MsgLayout    LineGoToL0Layout;

/* }}} */
/* {{{ LineL2PowerTrim service */

typedef struct LineL2PowerTrimMsgReq LineL2PowerTrimMsgReq;
struct LineL2PowerTrimMsgReq
{
  uint8 deltaPsd;
} BCM_PACKING_ATTRIBUTE ;

extern MsgLayout    LineL2PowerTrimLayout;

/* }}} */
/* {{{ LineTriggerOLR service */

typedef struct BitSwapEntry BitSwapEntry;
struct BitSwapEntry
{
  uint8 toneIdx;                /* US tone index */
  uint8 deltaBi;                /* bit loading change, range [-1;1] */
  uint8 deltaGi;                /* gain change in dB, range [-3;3] */
} BCM_PACKING_ATTRIBUTE;

typedef struct LineTriggerOLRMsgReq LineTriggerOLRMsgReq;
struct LineTriggerOLRMsgReq
{
  uint8        olrType;         /* 0: bitswap, 1: SRA */
  uint8        reserved;
  int16        sraDelta;        /* change in SRA L */
  BitSwapEntry bitswapEntry[2]; /* 2 tones to modify by bitswap */
} BCM_PACKING_ATTRIBUTE ;

extern MsgLayout    LineTriggerOLRLayout;

/* }}} */

#endif /* #if 0 */

#endif
