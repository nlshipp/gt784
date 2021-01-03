/*
<:copyright-broadcom 
 
 Copyright (c) 2002 Broadcom Corporation 
 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          16215 Alton Parkway 
          Irvine, California 92619 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/****************************************************************************
 *
 * AdslCoreMap.h -- Definitions for ADSL core hardware
 *
 * Description:
 *	Definitions for ADSL core hardware
 *
 *
 * Authors: Ilya Stomakhin
 *
 * $Revision: 1.3 $
 *
 * $Id: AdslCoreMap.h,v 1.3 2011/05/11 08:28:57 ydu Exp $
 *
 * $Log: AdslCoreMap.h,v $
 * Revision 1.3  2011/05/11 08:28:57  ydu
 * update dsl driver to A2x023i, VDSL2 Bonding datapump to Av4bC035b
 * ADSL2 Bonding datapump to A2pbC033a, VDSL2 datapump to A2pv6C035b
 *
 * Revision 1.1  2004/04/08 21:24:49  ilyas
 * Initial CVS checkin. Version A2p014
 *
 ****************************************************************************/

#if defined(__KERNEL__)
#include <linux/version.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#include <linux/autoconf.h>
#else
#include <linux/config.h>
#endif
#endif
#include "softdsl/AdslCoreDefs.h"

#if defined(CONFIG_BCM96348) || defined(CONFIG_BCM96358)
#define XDSL_ENUM_BASE		0xFFFE3000
#elif defined(CONFIG_BCM96338)
#define XDSL_ENUM_BASE		0xFFFE1000
#elif defined(CONFIG_BCM96368)
#undef XDSL_ENUM_BASE
#define XDSL_ENUM_BASE		0xB0F56000	/* DHIF */
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
#define XDSL_ENUM_BASE		0xB0D56000	/* DHIF */
#elif defined(CONFIG_BCM963268)
#define XDSL_ENUM_BASE		0xB0756000	/* DHIF */
#else
#error  No definition for XDSL_ENUM_BASE
#endif

#ifndef HOST_LMEM_BASE
 #if defined(CONFIG_BCM96368)
 #define HOST_LMEM_BASE          0xB0F80000
 #elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
 #define HOST_LMEM_BASE          0xB0D80000
 #elif defined(CONFIG_BCM963268)
 #define HOST_LMEM_BASE          0xB0780000
 #else
 #define HOST_LMEM_BASE          0xFFF00000
 #endif
#endif

#if defined(CONFIG_BCM96358)
#define ATM_REG_BASE		0xFFFE2000
#elif defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338)
#define ATM_REG_BASE		0xFFFE4000
#endif

#if defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338) || defined(CONFIG_BCM96358)

#ifndef ADSL_ENUM_BASE
#define ADSL_ENUM_BASE	XDSL_ENUM_BASE	/* Prevent compile error in ATM driver under Linux3.xx releases */
#endif

#define RCVINT_BIT              0x00010000
#define XMTINT_BIT              0x01000000
#define MSGINT_BIT             0x00000020
#define MSGINT_MASK_BIT	MSGINT_BIT

/* Backplane Enumeration Space Addresses */
#define ADSL_CTL                (0x000 / 4)
#define ADSL_STATUS             (0x004 / 4)
#define ADSL_INTMASK_I          (0x024 / 4)
#define ADSL_INTMASK_F          (0x02c / 4)
#define ADSL_INTSTATUS_I        (0x020 / 4)
#define ADSL_INTSTATUS_F        (0x028 / 4)
#define ADSL_HOSTMESSAGE        (0x300 / 4)

/* DMA Regs, offset from backplane enumeration space address */

#define XMT_CTL_INTR            (0x200 / 4)
#define XMT_ADDR_INTR           (0x204 / 4)
#define XMT_PTR_INTR            (0x208 / 4)
#define XMT_STATUS_INTR         (0x20c / 4)
#define RCV_CTL_INTR            (0x210 / 4)
#define RCV_ADDR_INTR           (0x214 / 4)
#define RCV_PTR_INTR            (0x218 / 4)
#define RCV_STATUS_INTR         (0x21c / 4)
#define XMT_CTL_FAST            (0x220 / 4)
#define XMT_ADDR_FAST           (0x224 / 4)
#define XMT_PTR_FAST            (0x228 / 4)
#define XMT_STATUS_FAST         (0x22c / 4)
#define RCV_CTL_FAST            (0x230 / 4)
#define RCV_ADDR_FAST           (0x234 / 4)
#define RCV_PTR_FAST            (0x238 / 4)
#define RCV_STATUS_FAST         (0x23c / 4)

#define ADSL_CORE_RESET         (0xf98 / 4)
#define ADSL_MIPS_RESET         (0x000 / 4)

#define US_DESCR_TABLE_ADDR		0xA0004000
#define DS_DESCR_TABLE_ADDR		0xA0005000
#define DS_STATUS_SIZE			16

#define HDMA_INIT_FLAG_ADDR		0xA0006000
#define US_CURR_DSCR_ADDR		0xA0006004
#define US_LAST_DSCR_ADDR		0xA0006008
#define DS_CURR_DSCR_ADDR		0xA000600c
#define DS_LAST_DSCR_ADDR		0xA0006010

#else

/* Common for: CONFIG_BCM96368, CONFIG_BCM96362, CONFIG_BCM96328, CONFIG_BCM963268 */
#define MSGINT_BIT			0x00000100
#define MSGINT_MASK_BIT		0x00000001

#define ADSL_Core2HostMsg		(0x034 / 4)
#define ADSL_INTMASK_I		(0x03c / 4)
#define ADSL_INTSTATUS_I		ADSL_INTMASK_I
#define ADSL_HOSTMESSAGE	(0x038 / 4)

#endif /* defined(CONFIG_BCM96348) || defined(CONFIG_BCM96338) || defined(CONFIG_BCM96358) */
