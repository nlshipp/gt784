/*
<:copyright-broadcom 
 
 Copyright (c) 2007 Broadcom Corporation 
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
/***********************************************************************/
/*                                                                     */
/*   MODULE:  6816_common.h                                             */
/*   DATE:    02/01/08                                                 */
/*   PURPOSE: Define addresses of major hardware components of         */
/*            BCM6816                                                  */
/*                                                                     */
/***********************************************************************/
#ifndef __BCM6816_MAP_COMMON_H
#define __BCM6816_MAP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define PERF_BASE            0xb0000000  /* chip control registers */
#define TIMR_BASE            0xb0000040  /* timer registers */
#define NAND_INTR_BASE       0xb0000070
#define GPIO_BASE            0xb0000080  /* gpio registers */
#define UART_BASE            0xb0000100  /* uart registers */
#define UART1_BASE           0xb0000120  /* uart registers */
#define I2C_BASE             0xb0000180
#define OTP_BASE             0xb0000400
#define UBUS_STAT_BASE       0xb0000500
#define SPI_BASE             0xb0000800  /* SPI master controller registers */
#define HSSPIM_BASE          0xb0001000
#define MISC_BASE            0xb0001800
#define NAND_REG_BASE        0xb0002000  /* NAND control registers */
#define MPI_BASE             0xb00020A0  /* MPI control registers */
#define PCI_BASE             0xb0002100  /* PCI control registers */
#define NAND_CACHE_BASE      0xb0002200
#define USB_CTL_BASE         0xb0002400  /* USB 2.0 device control registers */
#define USB_EHCI_BASE        0x10002500  /* USB host registers */
#define USB_OHCI_BASE        0x10002600  /* USB host registers */
#define USBH_CFG_BASE        0xb0002700
#define IPSEC_BASE           0xb0002800
#define DDR_BASE             0xb0003000  /* Memory control registers */
#define GPON_BASE            0xb0004000
#define APM_BASE             0xb0008000
#define PCM_BASE             0xb0008200
#define APM_HVG_BASE         0xb0008300
#define APM_IUDMA_BASE       0xb0008800
#define BMU_BASE             0xb0009000  /* fff9D000-fff9Dfff */
#define USB_DMA_BASE         0xb000c000  /* USB 2.0 device DMA regiseters */
#define GPON_DMA_BASE        0xb000c800
#define IPSEC_DMA_BASE       0xb000d000
#define SWITCH_DMA_BASE      0xb000d800
#define SWITCH_DMA_CONFIG    0xb000da00
#define SWITCH_DMA_STATE     0xb000dc00
#define APM_MEM_BASE         0xb0010000
#define MOCA_MEM_BASE        0xb0d00000
#define MOCA_IO_BASE         0xb0d80000
#define SWITCH_BASE          0xb0e00000
#define PCIE_MEM65K_BASE     0xb0e40000
#define PCIE_MEM1M_BASE      0xb0f00000

/*
#####################################################################
# System PLL Control Register
#####################################################################
*/

/*
#####################################################################
# GPIO Control Registers
#####################################################################
*/
#define GPIO_SWREG_CONFIG       0x1c
#define GPIO_LIN_VREG_ADJ_SHIFT 0x0
#define GPIO_LIN_VREG_ADJ_MASK  (0xf<<GPIO_LIN_VREG_ADJ_SHIFT)
#define GPIO_SW_VREG_SEL_SHIFT	0x8
#define GPIO_SW_VREG_SEL_MASK	(0xf<<GPIO_SW_VREG_SEL_SHIFT)

/*
#####################################################################
# Miscellaneous Registers
#####################################################################
*/
#define MISC_MEMC_CONTROL		0x10
#define MISC_STRAP_BUS			0x14
#define MISC_STRAP_BUS_MIPS_PLL_FVCO_SHIFT      27

/*
#####################################################################
# Memory Control Registers
#####################################################################
*/
#define DDR_CTL_CNFG                             0x000 
#define DDR_CTL_CSST                             0x004 
#define DDR_CTL_CSEND                            0x008 
#define DDR_CTL_ROW00_0                          0x010 
#define DDR_CTL_ROW00_1                          0x014 
#define DDR_CTL_ROW01_0                          0x018 
#define DDR_CTL_ROW01_1                          0x01c 
#define DDR_CTL_ROW20_0                          0x030 
#define DDR_CTL_ROW20_1                          0x034 
#define DDR_CTL_ROW21_0                          0x038 
#define DDR_CTL_ROW21_1                          0x03c 
#define DDR_CTL_COL00_0                          0x050 
#define DDR_CTL_COL00_1                          0x054 
#define DDR_CTL_COL01_0                          0x058 
#define DDR_CTL_COL01_1                          0x05c 
#define DDR_CTL_COL20_0                          0x070 
#define DDR_CTL_COL20_1                          0x074 
#define DDR_CTL_COL21_0                          0x078 
#define DDR_CTL_COL21_1                          0x07c 
#define DDR_CTL_BNK10                            0x090 
#define DDR_CTL_BNK32                            0x094 
#define DDR_CTL_DCMD                             0x100 
#define DDR_CTL_DMODE_0                          0x104 
#define DDR_CTL_DMODE_1                          0x108 
#define DDR_CTL_CLKS                             0x10c 
#define DDR_CTL_ODT                              0x110 
#define DDR_CTL_TIM1_0                           0x114 
#define DDR_CTL_TIM1_1                           0x118 
#define DDR_CTL_TIM2                             0x11c 
#define DDR_CTL_CTL_CRC                          0x120 
#define DDR_CTL_DOUT_CRC                         0x124 
#define DDR_CTL_DIN_CRC                          0x128 
#define PHY_CONTROL_REGS_REVISION                0x200 
#define PHY_CONTROL_REGS_CLK_PM_CTRL             0x204 
#define PHY_CONTROL_REGS_PLL_STATUS              0x210 
#define PHY_CONTROL_REGS_PLL_CONFIG              0x214 
#define PHY_CONTROL_REGS_PLL_PRE_DIVIDER         0x218 
#define PHY_CONTROL_REGS_PLL_DIVIDER             0x21c 
#define PHY_CONTROL_REGS_PLL_CONTROL1            0x220 
#define PHY_CONTROL_REGS_PLL_CONTROL2            0x224 
#define PHY_CONTROL_REGS_PLL_SS_EN               0x228 
#define PHY_CONTROL_REGS_PLL_SS_CFG              0x22c 
#define PHY_CONTROL_REGS_STATIC_VDL_OVERRIDE     0x230 
#define PHY_CONTROL_REGS_DYNAMIC_VDL_OVERRIDE    0x234 
#define PHY_CONTROL_REGS_IDLE_PAD_CONTROL        0x238 
#define PHY_CONTROL_REGS_ZQ_PVT_COMP_CTL         0x23c 
#define PHY_CONTROL_REGS_DRIVE_PAD_CTL           0x240 
#define PHY_BYTE_LANE_0_REVISION                 0x300 
#define PHY_BYTE_LANE_0_VDL_CALIBRATE            0x304 
#define PHY_BYTE_LANE_0_VDL_STATUS               0x308 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_0           0x310 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_1           0x314 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_2           0x318 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_3           0x31c 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_4           0x320 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_5           0x324 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_6           0x328 
#define PHY_BYTE_LANE_0_VDL_OVERRIDE_7           0x32c 
#define PHY_BYTE_LANE_0_READ_CONTROL             0x330 
#define PHY_BYTE_LANE_0_READ_FIFO_STATUS         0x334 
#define PHY_BYTE_LANE_0_READ_FIFO_CLEAR          0x338 
#define PHY_BYTE_LANE_0_IDLE_PAD_CONTROL         0x33c 
#define PHY_BYTE_LANE_0_DRIVE_PAD_CTL            0x340 
#define PHY_BYTE_LANE_0_CLOCK_PAD_DISABLE        0x344 
#define PHY_BYTE_LANE_0_WR_PREAMBLE_MODE         0x348 
#define PHY_BYTE_LANE_1_REVISION                 0x400 
#define PHY_BYTE_LANE_1_VDL_CALIBRATE            0x404 
#define PHY_BYTE_LANE_1_VDL_STATUS               0x408 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_0           0x410 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_1           0x414 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_2           0x418 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_3           0x41c 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_4           0x420 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_5           0x424 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_6           0x428 
#define PHY_BYTE_LANE_1_VDL_OVERRIDE_7           0x42c 
#define PHY_BYTE_LANE_1_READ_CONTROL             0x430 
#define PHY_BYTE_LANE_1_READ_FIFO_STATUS         0x434 
#define PHY_BYTE_LANE_1_READ_FIFO_CLEAR          0x438 
#define PHY_BYTE_LANE_1_IDLE_PAD_CONTROL         0x43c 
#define PHY_BYTE_LANE_1_DRIVE_PAD_CTL            0x440 
#define PHY_BYTE_LANE_1_CLOCK_PAD_DISABLE        0x444 
#define PHY_BYTE_LANE_1_WR_PREAMBLE_MODE         0x448 
#define PHY_BYTE_LANE_2_REVISION                 0x500 
#define PHY_BYTE_LANE_2_VDL_CALIBRATE            0x504 
#define PHY_BYTE_LANE_2_VDL_STATUS               0x508 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_0           0x510 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_1           0x514 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_2           0x518 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_3           0x51c 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_4           0x520 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_5           0x524 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_6           0x528 
#define PHY_BYTE_LANE_2_VDL_OVERRIDE_7           0x52c 
#define PHY_BYTE_LANE_2_READ_CONTROL             0x530 
#define PHY_BYTE_LANE_2_READ_FIFO_STATUS         0x534 
#define PHY_BYTE_LANE_2_READ_FIFO_CLEAR          0x538 
#define PHY_BYTE_LANE_2_IDLE_PAD_CONTROL         0x53c 
#define PHY_BYTE_LANE_2_DRIVE_PAD_CTL            0x540 
#define PHY_BYTE_LANE_2_CLOCK_PAD_DISABLE        0x544 
#define PHY_BYTE_LANE_2_WR_PREAMBLE_MODE         0x548 
#define PHY_BYTE_LANE_3_REVISION                 0x600 
#define PHY_BYTE_LANE_3_VDL_CALIBRATE            0x604 
#define PHY_BYTE_LANE_3_VDL_STATUS               0x608 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_0           0x610 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_1           0x614 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_2           0x618 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_3           0x61c 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_4           0x620 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_5           0x624 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_6           0x628 
#define PHY_BYTE_LANE_3_VDL_OVERRIDE_7           0x62c 
#define PHY_BYTE_LANE_3_READ_CONTROL             0x630 
#define PHY_BYTE_LANE_3_READ_FIFO_STATUS         0x634 
#define PHY_BYTE_LANE_3_READ_FIFO_CLEAR          0x638 
#define PHY_BYTE_LANE_3_IDLE_PAD_CONTROL         0x63c 
#define PHY_BYTE_LANE_3_DRIVE_PAD_CTL            0x640 
#define PHY_BYTE_LANE_3_CLOCK_PAD_DISABLE        0x644 
#define PHY_BYTE_LANE_3_WR_PREAMBLE_MODE         0x648 
#define DDR_CTL_GCFG                             0x800 
#define DDR_CTL_LBIST_CFG                        0x804 
#define DDR_CTL_LBIST_SEED                       0x808 
#define DDR_CTL_ARB                              0x80c 
#define DDR_CTL_PI_GCF                           0x810 
#define DDR_CTL_PI_UBUS_CTL                      0x814 
#define DDR_CTL_PI_MIPS_CTL                      0x818 
#define DDR_CTL_PI_DSL_MIPS_CTL                  0x81c 
#define DDR_CTL_PI_DSL_PHY_CTL                   0x820 
#define DDR_CTL_PI_UBUS_ST                       0x824 
#define DDR_CTL_PI_MIPS_ST                       0x828 
#define DDR_CTL_PI_DSL_MIPS_ST                   0x82c 
#define DDR_CTL_PI_DSL_PHY_ST                    0x830 
#define DDR_CTL_PI_UBUS_SMPL                     0x834 
#define DDR_CTL_TESTMODE                         0x838 
#define DDR_CTL_TEST_CFG1                        0x83c 
#define DDR_CTL_TEST_PAT                         0x840 
#define DDR_CTL_TEST_COUNT                       0x844 
#define DDR_CTL_TEST_CURR_COUNT                  0x848 
#define DDR_CTL_TEST_ADDR_UPDT                   0x84c 
#define DDR_CTL_TEST_ADDR                        0x850 
#define DDR_CTL_TEST_DATA0                       0x854 
#define DDR_CTL_TEST_DATA1                       0x858 
#define DDR_CTL_TEST_DATA2                       0x85c 
#define DDR_CTL_TEST_DATA3                       0x860 

/*
#####################################################################
# MPI Control Registers
#####################################################################
*/
#define CS0BASE         0x00
#define CS0CNTL         0x04

/*
# CSxBASE settings
#   Size in low 4 bits
#   Base Address for match in upper 24 bits
#
# TBD - Is it 8K to 256M or 4K to 128M?
#
*/
#define EBI_SIZE_8K         0
#define EBI_SIZE_16K        1
#define EBI_SIZE_32K        2
#define EBI_SIZE_64K        3
#define EBI_SIZE_128K       4
#define EBI_SIZE_256K       5
#define EBI_SIZE_512K       6
#define EBI_SIZE_1M         7
#define EBI_SIZE_2M         8
#define EBI_SIZE_4M         9
#define EBI_SIZE_8M         10
#define EBI_SIZE_16M        11
#define EBI_SIZE_32M        12
#define EBI_SIZE_64M        13
#define EBI_SIZE_128M       14
#define EBI_SIZE_256M       15

/* CSxCNTL settings */
/* TBD. Verify these definitions on BCM6816 */
#define EBI_ENABLE          0x00000001  /* .. enable this range */
#define EBI_WAIT_STATES     0x0000000e  /* .. mask for wait states */
#define ZEROWT              0x00000000  /* ..  0 WS */
#define ONEWT               0x00000002  /* ..  1 WS */
#define TWOWT               0x00000004  /* ..  2 WS */
#define THREEWT             0x00000006  /* ..  3 WS */
#define FOURWT              0x00000008  /* ..  4 WS */
#define FIVEWT              0x0000000a  /* ..  5 WS */
#define SIXWT               0x0000000c  /* ..  6 WS */
#define SEVENWT             0x0000000e  /* ..  7 WS */
#define EBI_WORD_WIDE       0x00000010  /* .. 16-bit peripheral, else 8 */
#define EBI_TS_TA_MODE      0x00000080  /* .. use TS/TA mode */
#define EBI_TS_SEL          0x00000100  /* .. drive tsize, not bs_b */
#define EBI_FIFO            0x00000200  /* .. enable fifo */
#define EBI_RE              0x00000400  /* .. Reverse Endian */

/*
#####################################################################
# UART Control Registers
#####################################################################
*/
/* TBD. Verify these definitions on BCM6816 */
#define UART0CONTROL     0x01
#define UART0CONFIG      0x02
#define UART0RXTIMEOUT   0x03
#define UART0BAUD        0x04
#define UART0FIFOCFG     0x0a
#define UART0INTMASK     0x10
#define UART0INTSTAT     0x12
#define UART0DATA        0x17

#define BRGEN            0x80   /* Control register bit defs */
#define TXEN             0x40
#define RXEN             0x20
#define LOOPBK           0x10
#define TXPARITYEN       0x08
#define TXPARITYEVEN     0x04
#define RXPARITYEN       0x02
#define RXPARITYEVEN     0x01

#define XMITBREAK        0x40   /* Config register */
#define BITS5SYM         0x00
#define BITS6SYM         0x10
#define BITS7SYM         0x20
#define BITS8SYM         0x30
#define ONESTOP          0x07
#define TWOSTOP          0x0f

#define DELTAIP         0x0001
#define TXUNDERR        0x0002
#define TXOVFERR        0x0004
#define TXFIFOTHOLD     0x0008
#define TXREADLATCH     0x0010
#define TXFIFOEMT       0x0020
#define RXUNDERR        0x0040
#define RXOVFERR        0x0080
#define RXTIMEOUT       0x0100
#define RXFIFOFULL      0x0200
#define RXFIFOTHOLD     0x0400
#define RXFIFONE        0x0800
#define RXFRAMERR       0x1000
#define RXPARERR        0x2000
#define RXBRK           0x4000

#ifdef __cplusplus
}
#endif

#endif
