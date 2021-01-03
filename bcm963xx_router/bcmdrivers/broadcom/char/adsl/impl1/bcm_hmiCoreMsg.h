/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_hmiCoreMsg.h                                       */
/*   Purpose         : Header for all core manager HMI message types          */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 17-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_hmiCoreMsg.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/
#ifndef HMI_CORE_MSG_H
# define HMI_CORE_MSG_H

# include "bcm_userdef.h"
# include "bcm_layout.h"

/* {{{ Application ID */

# define CORE_MANAGER_ID                0x100

/* }}} */
/* {{{ Available services */

# define SET_DEVICE_CONFIGURATION       0x0001
# define GET_DEVICE_CONFIGURATION       0x0002
# define GET_LINE_INTERRUPT_STATUS      0x0003
# define CHANGE_LINE_INTERRUPT_MASK     0x0004
# define RESET_DEVICE                   0x0005     
# define GET_FIRMWARE_VERSION_INFO      0x0007
# define WRITE_GPIO                     0x0008
# define READ_GPIO                      0x0009
# define SET_OAM_PERIOD_ELAPSED_TIME    0x000C
# ifndef CO_VCOPE
# define SET_OEM_ID                     0x000D
# endif
# define SET_PTM_VC                     0x000E
# define DETECT_MODEM                   0x0021
# define SET_IN_BAND_IP_CONFIGURATION   0x0022
# define GET_IN_BAND_IP_CONFIGURATION   0x0023
# define GET_IN_BAND_LAYER_2_CONFIG     0x0024
# define GET_IN_BAND_STACK_STATUS       0x0025
# define SET_LED_PATTERN                0x0027
# define SET_TX_FILTER                  0x0028
# define SET_BCM6510_AFE_CONNECT        0x0029

#if 0
/* }}} */

/* {{{ Set- and GetDeviceConfiguration service */
typedef struct BearerConfig BearerConfig;
struct BearerConfig
{
  int32  utopiaAddr;            /* -1 indicates not linked to utopia */
  uint32 STMslotBitMap;         /* bit map for STM time slot */
} BCM_PACKING_ATTRIBUTE ;

# define B0 0                   /* bearer 0 */
# define B1 1                   /* bearer 1 */
# define BCM_BEARER_NAME(bearer) ((bearer) == B0 ? "B0" : "B1")

typedef struct LineConfig LineConfig;
struct LineConfig
{
  BearerConfig bearerConfig[2]; /* the second bearer may not be used if the
                                   first one is not connected. */
} BCM_PACKING_ATTRIBUTE ;

/* For interop reasons, the countryCode, providerCode and vendorInfo are no
 * longer available on the HMI -- they're frozen */
# define BCM_COUNTRY_CODE 181
# define BCM_PROVIDER_CODE "BDCM"
# define BCM_VENDOR_INFO 19796

typedef struct LineDriverPowerModes LineDriverPowerModes;
struct LineDriverPowerModes
{
  uint8  powerdownMode;         /* line driver setting used to put the line
                                 * driver in power down mode */
  uint8  activationMode;        /* line driver setting used during the
                                 * activation (until politeness has been
                                 * applied) */
  uint8  powerModeMaxtones256;  /* showtime LD setting if max tone < 256 */
  uint8  powerModeMaxtones512;  /* showtime LD setting if max tone < 512 */
  uint8  powerModeMaxtones1024; /* showtime LD setting if max tone < 1024 */
  uint8  powerModeMaxtones2048; /* showtime LD setting if max tone < 2048 */
  uint8  powerModeMaxtones4096; /* showtime LD setting if max tone < 4096 */
  uint8  lineTestPowerMode;     /* Line driver power mode to be used
                                 * while the line is in 'TEST MODE' state */
} BCM_PACKING_ATTRIBUTE ;

/* macros definition for some known line driver settings */
# define BCM_SET_EL1537_POWER_MODES(mode) \
  mode.powerdownMode               = 3; \
  mode.activationMode              = 2; \
  mode.powerModeMaxtones256        = 2; \
  mode.powerModeMaxtones512        = 2; \
  mode.powerModeMaxtones1024       = 2; \
  mode.powerModeMaxtones2048       = 2; \
  mode.powerModeMaxtones4096       = 2; \
  mode.lineTestPowerMode           = 2

# define BCM_SET_EL1517_POWER_MODES(mode) \
  mode.powerdownMode               = 2; \
  mode.activationMode              = 0; \
  mode.powerModeMaxtones256        = 0; \
  mode.powerModeMaxtones512        = 0; \
  mode.powerModeMaxtones1024       = 0; \
  mode.powerModeMaxtones2048       = 0; \
  mode.powerModeMaxtones4096       = 0; \
  mode.lineTestPowerMode           = 0

# define BCM_SET_EL1548_POWER_MODES(mode) \
  mode.powerdownMode               = 3; \
  mode.activationMode              = 0; \
  mode.powerModeMaxtones256        = 0; \
  mode.powerModeMaxtones512        = 0; \
  mode.powerModeMaxtones1024       = 0; \
  mode.powerModeMaxtones2048       = 0; \
  mode.powerModeMaxtones4096       = 0; \
  mode.lineTestPowerMode           = 0

# define BCM_SET_AD8390_POWER_MODES(mode) \
  mode.powerdownMode               = 1; \
  mode.activationMode              = 3; \
  mode.powerModeMaxtones256        = 3; \
  mode.powerModeMaxtones512        = 3; \
  mode.powerModeMaxtones1024       = 3; \
  mode.powerModeMaxtones2048       = 3; \
  mode.powerModeMaxtones4096       = 3; \
  mode.lineTestPowerMode           = 3

# ifdef BCM_VDSL_ON_6510
#  define BCM_SET_DEFAULT_POWER_MODES BCM_SET_EL1517_POWER_MODES
# elif defined(BCM_IPDSLAM_V2)
#  define BCM_SET_DEFAULT_POWER_MODES BCM_SET_AD8390_POWER_MODES
# else
#  define BCM_SET_DEFAULT_POWER_MODES BCM_SET_EL1537_POWER_MODES
# endif

typedef struct SantoriniConfig SantoriniConfig;
struct SantoriniConfig
{
  LineConfig lineConfig[BCM_DEV_NB_LINES];
  uint8  enableHPIreplyInterrupt;  /* every HMI reply will generate an int */
  uint8  OAMstatisticsCountMode;   /* modify the behavior of ES, SES and UAS
                                    * counters when not in Running Activation
                                    * nor Running Showtime:
                                    * bit1:0 0: incremented each seccond
                                    *        1: incremented when no LPR failure
                                    *        2: remains frozen 
                                    * bit2: fullInitCount counts re-init */
  int16  txPowerCalibration;       /* Fine tune transmit power (1/256 dB) */
  int16  rxAfeGains[4];            /* Fine tune received power (1/256 dB) */
  int16  agcGains[7];              /* Fine tune AGC gains (1/256 dB) */
  LineDriverPowerModes lineDriverPowerModes;
  uint8  txPowerOptimizationIlv;   /* Allow power boost when INP = 2 DMT
                                    * Unit is 1/256 dB */
  uint8  enableUtopiaFlushing;     /* Controls the Utopia flushing mode for
                                    * the line not in showtime:
                                    * 0: disabled
                                    * 1: enabled */
  uint8  ESmaskingMode;            /* Controls LOSS, ES and SES during UAS:
                                    * 0: all UAS counted as LOSS, ES and SES
                                    * 1: LOSS, ES, SES inhibited during UAS */
  uint8  reservedA[3];
  int16  rxCrestFactor;            /* Crest factor of the input signal. Unit
                                    * is 1/256. 0 means default settings */
  uint8  reservedB[8];
  uint16 hwMaxOutputPower;         /* HW limitation on the max transmit power
                                    * unit is 1/256 dBm */
  uint8  reservedC;
  uint8  autoMsgConfig;            /* Autonomous message configuration:
                                    * 0: send ATM in-band autonomous messages
                                    * 1: send IP in-band autonomous messages
                                    * 2: Don't send autonomous messages */
  uint16 autoMsgDestPort;          /* Autonomous message dest. UDP port */
  uint32 autoMsgDestIPaddr;        /* Autonomous message dest. UDP IP address */
  int32  controlUtopiaAddr;        /* utopia port used for in-band control
                                    * -1 disable in-band control,
                                    * -2 keep the current port used for
                                    *    in-band download */
  uint8  utopia16bitBusSelect;     /* flag indicating whether the utopia bus
                                      * has has to be configured as an 8 bits
                                      * bus or a 16 bits bus:
                                      * 0: 8  bits utopia bus
                                      * 1: 16 bits utopia bus
                                      * 2: keep setting used for in-band
                                      *    download */
  uint8  repetitionRate;           /* autonomous repetition rate (seconds) */
  uint8  autoMsgATMheader[5];      /* ATM header for autonomous messages */
  uint8  flags;                    /* bits 0-6: reserved, set to 0
                                    * bit 7: enable long DS Utopia queue */
  uint32 autonomousRef;            /* device reference identification to be
                                    * used in autonomous message header */
} BCM_PACKING_ATTRIBUTE ;

typedef SantoriniConfig    SetDeviceConfigurationMsgReq;
extern  MsgLayout          SetDeviceConfigurationLayout;


typedef SantoriniConfig    GetDeviceConfigurationMsgRep;
extern  MsgLayout          GetDeviceConfigurationLayout;

/* }}} */
/* {{{ GetLineInterruptStatus service */

# define LINE_INT_IDLE_NOT_CONFIGURED_STATE    (1<<0)
# define LINE_INT_LINE_UP                      (1<<1)
# define LINE_INT_LINE_DOWN                    (1<<2)
# define LINE_INT_INIT_FAIL                    (1<<3)
# ifdef BCM_DONT_USE_EXTENDED_OAM_THRESHOLDS
#  define LINE_INT_NE_ES_15MIN_THRESHOLD       (1<<4)
#  define LINE_INT_NE_SES_15MIN_THRESHOLD      (1<<5)
#  define LINE_INT_NE_UAS_15MIN_THRESHOLD      (1<<6)
#  define LINE_INT_FE_ES_15MIN_THRESHOLD       (1<<7)
#  define LINE_INT_FE_SES_15MIN_THRESHOLD      (1<<8)
#  define LINE_INT_FE_UAS_15MIN_THRESHOLD      (1<<9)
#  define LINE_INT_NE_ES_24HOUR_THRESHOLD      (1<<10)
#  define LINE_INT_NE_SES_24HOUR_THRESHOLD     (1<<11)
#  define LINE_INT_NE_UAS_24HOUR_THRESHOLD     (1<<12)
#  define LINE_INT_FE_ES_24HOUR_THRESHOLD      (1<<13)
#  define LINE_INT_FE_SES_24HOUR_THRESHOLD     (1<<14)
#  define LINE_INT_FE_UAS_24HOUR_THRESHOLD     (1<<15)
# else
#  define LINE_INT_RE_INIT                     (1<<4)
#  define LINE_INT_NE_OAM_15MIN_THRESHOLD      (1<<6)
#  define LINE_INT_FE_OAM_15MIN_THRESHOLD      (1<<9)
#  define LINE_INT_NE_OAM_24HOUR_THRESHOLD     (1<<12)
#  define LINE_INT_FE_OAM_24HOUR_THRESHOLD     (1<<15)
# endif
# define LINE_INT_FAILURE_STATE_CHANGE         (1<<16)
# define LINE_INT_EOC_READ_REGISTER_READY      (1<<17)
# define LINE_INT_OVERHEAD_TRANSACTION_DONE    (1<<18)
# define LINE_INT_CLEAR_EOC_RECEIVED           (1<<19)
# define LINE_INT_CPE_DYING_GASP               (1<<20)
# define LINE_INT_AUTO_RESET                   (1<<21)
# define LINE_INT_CLEAR_EOC_TRANSMITTED        (1<<22)
# define LINE_INT_LINE_ENTER_TRAINING          (1<<23)
# define LINE_INT_LINE_LD_COMPLETE             (1<<24)
# define LINE_INT_15MIN_PERIOD                 (1<<25)
# define LINE_INT_24HOUR_PERIOD                (1<<26)
# define LINE_INT_BONDING_RATE_CHANGED         (1<<27)
# define LINE_INT_L2_STATE_CHANGED             (1<<28)
# define LINE_INT_SRA_RATE_CHANGED             (1<<29)
# define LINE_INT_ATUC_NOT_VDSL                (1<<30)

# ifdef BCM_DONT_USE_EXTENDED_OAM_THRESHOLDS
/* some usefull aggregates when not using extended oam */
#  define LINE_INT_NE_15MIN_THRESHOLD  (LINE_INT_NE_ES_15MIN_THRESHOLD   | \
                                        LINE_INT_NE_SES_15MIN_THRESHOLD  | \
                                        LINE_INT_NE_UAS_15MIN_THRESHOLD)
#  define LINE_INT_FE_15MIN_THRESHOLD  (LINE_INT_FE_ES_15MIN_THRESHOLD   | \
                                        LINE_INT_FE_SES_15MIN_THRESHOLD  | \
                                        LINE_INT_FE_UAS_15MIN_THRESHOLD)
#  define LINE_INT_NE_24HOUR_THRESHOLD (LINE_INT_NE_ES_24HOUR_THRESHOLD  | \
                                        LINE_INT_NE_SES_24HOUR_THRESHOLD | \
                                        LINE_INT_NE_UAS_24HOUR_THRESHOLD)
#  define LINE_INT_FE_24HOUR_THRESHOLD (LINE_INT_FE_ES_24HOUR_THRESHOLD  | \
                                        LINE_INT_FE_SES_24HOUR_THRESHOLD | \
                                        LINE_INT_FE_UAS_24HOUR_THRESHOLD)
# elif defined(BCM_USE_INIT_THRESHOLDS)
#  define LINE_INT_NE_15MIN_THRESHOLD  (LINE_INT_INIT_FAIL |\
                                        LINE_INT_RE_INIT   |\
                                        LINE_INT_NE_OAM_15MIN_THRESHOLD)
#  define LINE_INT_FE_15MIN_THRESHOLD   LINE_INT_FE_OAM_15MIN_THRESHOLD 
#  define LINE_INT_NE_24HOUR_THRESHOLD (LINE_INT_INIT_FAIL |\
                                        LINE_INT_RE_INIT   |\
                                        LINE_INT_NE_OAM_24HOUR_THRESHOLD)
#  define LINE_INT_FE_24HOUR_THRESHOLD  LINE_INT_FE_OAM_24HOUR_THRESHOLD

# else
#  define LINE_INT_NE_15MIN_THRESHOLD   LINE_INT_NE_OAM_15MIN_THRESHOLD
#  define LINE_INT_FE_15MIN_THRESHOLD   LINE_INT_FE_OAM_15MIN_THRESHOLD 
#  define LINE_INT_NE_24HOUR_THRESHOLD  LINE_INT_NE_OAM_24HOUR_THRESHOLD
#  define LINE_INT_FE_24HOUR_THRESHOLD  LINE_INT_FE_OAM_24HOUR_THRESHOLD
# endif

typedef struct LineInterrupt LineInterrupt;
struct LineInterrupt
{
  uint32 interruptSummary;      /* one bit per line bit 0 refers to line 0 */
  uint32 reserved;
  uint32 interruptStatus[BCM_DEV_NB_LINES]; /* Status Register */
  uint32 interruptMask[BCM_DEV_NB_LINES];   /* Mask Register */
} BCM_PACKING_ATTRIBUTE ;

typedef LineInterrupt GetLineInterruptStatusMsgRep;
extern  MsgLayout     GetLineInterruptStatusLayout;

/* }}} */
/* {{{ ChangeLineInterruptMask service */

typedef struct SantChangeLineIMR SantChangeLineIMR;
struct SantChangeLineIMR
{
  /* Bit array specifying for which interrupts the newMask must be taken into
   * account */
  uint32 concernedInterrupts[BCM_DEV_NB_LINES];
  /* Bit array giving the new mask for the concernedInterrupts. */
  uint32 newMask[BCM_DEV_NB_LINES];
} BCM_PACKING_ATTRIBUTE ;

typedef SantChangeLineIMR   ChangeLineInterruptMaskMsgReq;
extern  MsgLayout           ChangeLineInterruptMaskLayout;

/* }}} */
/* {{{ ResetDevice service */

extern MsgLayout  ResetDeviceLayout;
#endif /* #if 0 */

/* }}} */
/* {{{ GetFirmwareVersionInfo service */

typedef struct FirmwareVersionInfo FirmwareVersionInfo;
 struct FirmwareVersionInfo
{
  uint32 hwChipId;
  uint16 fwMajorVersion;
  uint8  fwMinorVersion;
  uint8  fwFixVersion;
  uint16 hmiMajorVersion;
  uint16 hmiMinorVersion;
  uint8  info[64];
  uint8  nrOfLinesSupportedInFirmwareVersion;
  uint8  reserved[3];
} BCM_PACKING_ATTRIBUTE ;

typedef FirmwareVersionInfo  GetFirmwareVersionInfoMsgRep;
extern  MsgLayout            GetFirmwareVersionInfoLayout;

#if 0
/* }}} */
/* {{{ WriteGPIO service */
typedef struct GPoutData GPoutData;
struct GPoutData {
  uint32 newState;                 /* 32-bits bitmap */
  uint32 dontMask;                 /* ~mask to apply to map */
} BCM_PACKING_ATTRIBUTE ;

typedef GPoutData WriteGPIOMsgReq;
extern  MsgLayout WriteGPIOLayout;

/* }}} */
/* {{{ ReadGPIO service */

typedef struct GPinData GPinData;
struct GPinData {
  uint32 state;                    /* 32-bits bitmap */
} BCM_PACKING_ATTRIBUTE ;

typedef GPinData  ReadGPIOMsgRep;
extern  MsgLayout ReadGPIOLayout;

/* }}} */
/* {{{ SetOAMperiodElapsedTime service */

typedef struct OAMperiodElapsedTime OAMperiodElapsedTime;
struct OAMperiodElapsedTime
{
  uint32 elapsedTime;
  uint32 numberOfSecIn15min;
  uint32 numberOf15minIn24hour;
} BCM_PACKING_ATTRIBUTE ;

typedef OAMperiodElapsedTime SetOAMperiodElapsedTimeMsgReq;
extern  MsgLayout            SetOAMperiodElapsedTimeLayout;
/* }}} */
#ifndef CO_VCOPE
/* {{{ SetOEMid service */

typedef struct SetOEMidMsgReq SetOEMidMsgReq;
struct SetOEMidMsgReq
{
  uint8 len;
  uint8 data[120];
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout SetOEMidLayout;

/* }}} */
#endif
/* {{{ SetPtmVc service */

typedef struct VpiVci VpiVci;
struct VpiVci
{
  uint16 vpi;
  uint16 vci;
};

typedef struct SetPtmVcMsgReq SetPtmVcMsgReq;
struct SetPtmVcMsgReq
{
  VpiVci ptmVcTx[2][2];         /* index 0 is for bearer, 1 for stream */
  VpiVci ptmVcRx[2][2];         /* index 0 is for bearer, 1 for stream */
} BCM_PACKING_ATTRIBUTE ;

extern  MsgLayout SetPtmVcLayout;

/* }}} */
/* {{{ DetectModem service */

typedef struct SNMPdetect SNMPdetect;
struct SNMPdetect 
{
  uint32 number;
  uint32 requester;
} BCM_PACKING_ATTRIBUTE ;

typedef SNMPdetect DetectModemMsgReq;
typedef SNMPdetect DetectModemMsgRep;
extern  MsgLayout  DetectModemLayout;

/* }}} */
/* {{{ Set- and GetInBandIPconfiguration service */

typedef struct PstackIPconfig PstackIPconfig;
struct PstackIPconfig
{
  uint32 ipAddr; 
  uint32 subnetMask; 
  uint32 defGateway;
  uint8  tos;
  uint8  reserved[3];
} BCM_PACKING_ATTRIBUTE ;

typedef PstackIPconfig     SetInBandIPconfigurationMsgReq;
extern  MsgLayout          SetInBandIPconfigurationLayout;


typedef PstackIPconfig     GetInBandIPconfigurationMsgRep;
extern  MsgLayout          GetInBandIPconfigurationLayout;

/* }}} */
/* {{{ GetInBandLayer2Config service */

typedef struct PstackL2config PstackL2config;
struct PstackL2config
{
  uint16 reservedA;
  uint16 tx_ll_vpi;
  uint16 tx_ll_vci;
  uint16 rx_ll_vpi;
  uint16 rx_ll_vci;
  uint8  ll_flags;
  uint8  mac[6];
  uint8  reservedB;
} BCM_PACKING_ATTRIBUTE ;

typedef PstackL2config GetInBandLayer2ConfigMsgRep;
extern  MsgLayout      GetInBandLayer2ConfigLayout;

/* }}} */
/* {{{ GetInBandStackStatus service */

typedef struct StackStatus StackStatus;
struct StackStatus {
  int32 status;
} BCM_PACKING_ATTRIBUTE ;

typedef StackStatus  GetInBandStackStatusMsgRep;
extern  MsgLayout    GetInBandStackStatusLayout;

/* }}} */
/* {{{ SetLedPattern service */

struct LedPattern
{
  uint8   idlePattern[8];
  uint8   activatePattern[8];
  uint8   trainingPattern[8];
  uint8   showtimePattern[8];
  uint32  elapsedTime;
  uint8   enable;
  uint8   reserved[3];
} BCM_PACKING_ATTRIBUTE ;

typedef struct LedPattern SetLedPatternMsgReq;
extern  MsgLayout         SetLedPatternLayout;

/* }}} */
/* {{{ SetTxFilter service */

# define BCM_TX_FILTER_LEN 49
typedef struct SetTxFilterMsgReq SetTxFilterMsgReq;
struct SetTxFilterMsgReq 
{
  uint8 filterId;
  uint8 filterDescription[BCM_TX_FILTER_LEN];
} BCM_PACKING_ATTRIBUTE ;

extern MsgLayout SetTxFilterLayout;

/* }}} */
/* {{{ SetBCM6510afeConnect service */
# ifdef BCM6510_AFE_CONFIG

typedef struct SetBCM6510afeConnectMsgReq SetBCM6510afeConnectMsgReq;
struct SetBCM6510afeConnectMsgReq
{
  uint8 data[16];
} BCM_PACKING_ATTRIBUTE ;

extern MsgLayout SetBCM6510afeConnectLayout;

/* static configurations */
extern const SetBCM6510afeConnectMsgReq defaultBCM6510afeConfig[];

/* define some macro for standard configurations */
#  define BCM6510_AFE_CONFIG_16(devId)   0
#  define BCM6510_AFE_CONFIG_32A(devId)  (1+(devId)%2)
#  define BCM6510_AFE_CONFIG_32B(devId)  (3+(devId)%2)
#  define BCM6510_AFE_CONFIG_48A(devId)  (5+(devId)%3)
#  define BCM6510_AFE_CONFIG_48B(devId)  (8+(devId)%3)
#  define BCM6510_AFE_CONFIG_12(devId)   11
#  define BCM6510_AFE_CONFIG_6411(devId) 12
# endif
/* }}} */
#endif /* #if 0 */
#endif 
