/******************************************************************************/
/*                 Copyright C 1998-2004 Broadcom Corporation                 */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corporation.  It may not be reproduced, used, sold or          */
/* transferred to any third party without the prior written consent of        */
/* Broadcom Corporation.   All rights reserved.                               */
/******************************************************************************/
/*   FileName        : bcm_genericDef.h                                       */
/*   Purpose         : some BCM6410 related parameters                        */
/*   Limitations     : None                                                   */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 22-Jun-2004                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_genericDef.h,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/

#ifndef BCM_GENERIC_DEF_H
# define BCM_GENERIC_DEF_H

/* {{{ SDK version */
# define BCM_SDK_MAJOR_VERSION 8
# define BCM_SDK_MINOR_VERSION 0
# define BCM_SDK_FIX_VERSION   0
/* }}} */

#if 0
/* {{{ BCM6410/6411/6510 related stuff */
# ifdef BCM_USE_BCM6510
#  if !defined(BCM_VDSL_ON_6510)
#    define BCM_DEV_NB_LINES     16
#  else
/* #    if ( !defined(VCOPE_RELAYCTL) && !defined(VCOPE_VDSLCTL) ) || defined(CO_VCOPE) */
#    if ( !defined(VCOPE_RELAYCTL) && !defined(VCOPE_VDSLCTL) )
#      if ( !defined(BCM_DEV_NB_LINES) )
#        define BCM_DEV_NB_LINES     6
#      endif
#    else
#      define BCM_DEV_NB_LINES     2
#    endif
#  endif
# elif !defined(BCM_ADSLPLUS_ON_6410)
#  define BCM_DEV_NB_LINES     12
# else
#  define BCM_DEV_NB_LINES      6
# endif
# define BCM_DEV_NB_BONDGRP    (BCM_DEV_NB_LINES/2)
# define NB_BCM_DEV            ((NB_LINES+BCM_DEV_NB_LINES-1)/BCM_DEV_NB_LINES)
# ifndef NB_HMIDEV
#  define NB_HMIDEV            NB_BCM_DEV
# endif

# if (NB_HMIDEV<NB_BCM_DEV) 
#  error "inconsistent configuration of NB_HMIDEV and NB_LINES"
# endif

/* number of supported lines/bondgrp per device and per core */
# ifdef BCM_SUPPORT_FOR_MIX_LINES_PER_DEV_NB
/* if we want to support a mix of devices running different number of lines
 * (like vdsl/adsl), we need to make the supported number of lines per device
 * dynamic, dependant on what the fw reports for a particular device */
#  define BCM_DEV_SUP_LINES(devId) \
          (getDevPtr(devId)->fwVersion.nrOfLinesSupportedInFirmwareVersion)
# else
/* if we dont need to support a mix, we keep the macro static */
#  define BCM_DEV_SUP_LINES(devId)   BCM_DEV_NB_LINES
# endif
# define BCM_CORE_SUP_LINES(devId)   (BCM_DEV_SUP_LINES(devId)/BCM_DEV_NB_CORES)
# define BCM_DEV_SUP_BONDGRP(devId)  (BCM_DEV_SUP_LINES(devId)/2)
# define BCM_CORE_SUP_BONDGRP(devId) (BCM_DEV_SUP_BONDGRP(devId)/BCM_DEV_NB_CORES)

# define BCM_DEV_LINE_ID(line)      ((line) % BCM_DEV_NB_LINES)
# define BCM_LINE_2_DEV(line)       ((line) / BCM_DEV_NB_LINES)
# define BCM_CORE_2_DEV(coreId)     ((coreId)/2)
# define BCM_DEV_BONDGRP_ID(group)  ((group) % BCM_DEV_NB_BONDGRP)
# define BCM_BONDGRP_2_DEV(group)   ((group) / BCM_DEV_NB_BONDGRP)
# ifndef BCM6410_SPECIAL_CORE_MAPPING
#  define BCM_CORE_LINE_ID(line)    ((line) % (BCM_DEV_NB_LINES/2))
#  define BCM_LINE_2_CORE(line)     ((line) / (BCM_DEV_NB_LINES/2))
#  define BCM_BONDGRP_2_CORE(group) ((group) / (BCM_DEV_NB_BONDGRP/2))
# endif
/* }}} */

/* {{{ HMI fifos geometries */
# define BCM_PAGE_SIZE      0x4000            /* 16kB page */
# define BCM_MAX_PAGE       31                /* 5 bit page */
# define MAX_HMI_FIFO_SIZE  (BCM_PAGE_SIZE/2) /* 2 fifo's  */
# define MAX_HMI_PACKET_SIZE 512
# ifndef MAX_BOOT_PACKET_SIZE
#  define MAX_BOOT_PACKET_SIZE MAX_HMI_PACKET_SIZE
# endif
/* }}} */

#  define BCM_QA_TIMEOUT_MS   1000

/* {{{ booter and firmware time out */
# ifndef BOOTER_TIMEOUT_MS
#  define BOOTER_TIMEOUT_MS   1000
# endif
# ifndef FIRMWARE_TIMEOUT_MS
#  define FIRMWARE_TIMEOUT_MS 4000
# endif
/* QA time out */
# ifndef BCM_QA_TIMEOUT_MS
#  define BCM_QA_TIMEOUT_MS 100
# endif
/* }}} */

#ifndef CONN_NAME
#define CONN_NAME "ANON"
#endif

/* {{{ print and debug info */
# ifdef NO_PRINT_INFO
#  define NO_STDOUT_PRINT
#  define NO_STDERR_PRINT
# else
#  define BCM_LOG_ENABLED
# endif
/* }}} */

/* BCM6410 addresses */
#define FP_PAGING_REG        0x4000
#define FP_ENABLE_REG        0x4008
#define FP_RQST_HOST_INT_REG 0x4018
#define FP_PROM_STATUS_REG   0x42B8
#define FP_CHIP_ID_REG       0x4250
#define FP_SELF_TEST_ADDR    16

#define VCOPE_STATUS_ADDRESS 0x8FFF        /* Address used for VCOPE status */
#endif /* #if 0 */

/* {{{ some generic macros */ 
# define BCM_SUCCESS 0
# define BCM_FAILURE 2

# define BCM_UNUSED_PARAMETER(a) (a = a)

# ifndef TRUE
#  define TRUE 1
# endif
# ifndef FALSE
#  define FALSE 0
# endif

# ifndef min
#  define min(a,b) ((a)<(b)? (a):(b))
# endif
# ifndef max
#  define max(a,b) ((a)>(b)? (a):(b))
# endif

/* range check for parameters */
# ifndef IN_RANGE
#  define IN_RANGE(data, min, max) ((data) >= (min) && (data) <= (max))
# endif
/* }}} */

#if 0
/* {{{ line driver usage */
# ifdef BCM_IPDSLAM_V2
#  define BCM_SPECIAL_LINE_DRIVER_SETTING BCM_SET_AD8390_POWER_MODES
# endif
/* }}} */

/* {{{ default calibration */
# ifndef BCM_TX_POWER_CALIBRATION
#  ifdef BCM_USE_3_3_VOLT_ANALOG
#   define BCM_TX_POWER_CALIBRATION  -68
#  elif defined(BCM_IPDSLAM_V2)
#   define BCM_TX_POWER_CALIBRATION  -256
#  else
#   define BCM_TX_POWER_CALIBRATION  0
#  endif
# endif

# ifndef BCM_RX_POWER_CALIBRATION
#  define BCM_RX_POWER_CALIBRATION  256
# endif

# ifndef BCM_SELT_CALIB_DELAY
#  define BCM_SELT_CALIB_DELAY 41.8
# endif
# ifndef BCM_SELT_CALIB_GAIN
#  define BCM_SELT_CALIB_GAIN 191.4
# endif
/* }}} */

/* {{{ default BCM6510 afe config -- only necessary if using BCM6510 */
# if defined(BCM_USE_BCM6510) && !defined(BCM6510_AFE_CONFIG)
#  define BCM6510_AFE_CONFIG BCM6510_AFE_CONFIG_16
# endif
/* }}} */
#endif /* #if 0 */
/* {{{ packing attribute may be defined for some processors */
# ifndef BCM_PACKING_ATTRIBUTE
#  define BCM_PACKING_ATTRIBUTE
# endif
/* }}} */
#if 0
/* {{{ if different external line mapping is required -> define those macros */
# ifndef EXT2BCM_LINE_NB
#  define EXT2BCM_LINE_NB(lineId) lineId
# endif
# ifndef BCM2EXT_LINE_NB
#  define BCM2EXT_LINE_NB(lineId) lineId
# endif
/* }}} */
#endif /* #if 0 */

#endif
