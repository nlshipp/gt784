/*
    Copyright 2000-2010 Broadcom Corporation

    Unless you and Broadcom execute a separate written software license
    agreement governing use of this software, this software is licensed
    to you under the terms of the GNU General Public License version 2
    (the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php,
    with the following added to such license:

        As a special exception, the copyright holders of this software give
        you permission to link this software with independent modules, and to
        copy and distribute the resulting executable under terms of your
        choice, provided that you also meet, for each linked independent
        module, the terms and conditions of the license of that module. 
        An independent module is a module which is not derived from this
        software.  The special exception does not apply to any modifications
        of the software.

    Notwithstanding the above, under no circumstances may you combine this
    software in any way with any other Broadcom software provided under a
    license other than the GPL, without Broadcom's express prior written
    consent.
*/                       

#ifndef __BCMSPIRES_H__
#define __BCMSPIRES_H__ 

#ifndef _CFE_
#include <linux/spi/spi.h> 
#endif

#define SPI_STATUS_OK                (0)
#define SPI_STATUS_INVALID_LEN      (-1)
#define SPI_STATUS_ERR              (-2)

/* legacy and HS controllers can coexist - use bus num to differentiate */
#define LEG_SPI_BUS_NUM  0
#define HS_SPI_BUS_NUM   1

#define LEG_SPI_CLOCK_DEF   2

#if defined(_BCM96816_) || defined(CONFIG_BCM96816) || defined(_BCM96362_) || defined(CONFIG_BCM96362)
#define HS_SPI_PLL_FREQ     400000000
#else
#define HS_SPI_PLL_FREQ     133333333
#endif
#define HS_SPI_CLOCK_DEF    40000000
#define HS_SPI_BUFFER_LEN   512

/* used to specify ctrlState for the interface BcmSpiReserveSlave2 
   SPI_CONTROLLER_STATE_SET is used to differentiate a value of 0 which results in
   the controller using default values and the case where CPHA_EXT, GATE_CLK_SSOFF,
   CLK_POLARITY, and ASYNC_CLOCK all need to be 0 

   SPI MODE sets the values for CPOL and CPHA
   SPI_CONTROLLER_STATE_CPHA_EXT will extend these modes
CPOL = 0 -> base value of clock is 0
CPHA = 0, CPHA_EXT = 0 -> latch data on rising edge, launch data on falling edge
CPHA = 1, CPHA_EXT = 0 -> latch data on falling edge, launch data on rising edge
CPHA = 0, CPHA_EXT = 1 -> latch data on rising edge, launch data on rising edge
CPHA = 1, CPHA_EXT = 1 -> latch data on falling edge, launch data on falling edge

CPOL = 1 -> base value of clock is 1
CPHA = 0, CPHA_EXT = 0 -> latch data on falling edge, launch data on rising edge
CPHA = 1, CPHA_EXT = 0 -> latch data on rising edge, launch data on falling edge
CPHA = 0, CPHA_EXT = 1 -> latch data on falling edge, launch data on falling edge
CPHA = 1, CPHA_EXT = 1 -> latch data on rising edge, launch data on rising edge
*/
#define SPI_CONTROLLER_STATE_SET             (1 << 31)
#define SPI_CONTROLLER_STATE_CPHA_EXT        (1 << 30)
#define SPI_CONTROLLER_STATE_GATE_CLK_SSOFF  (1 << 29)
#define SPI_CONTROLLER_STATE_CLK_POLARITY    (1 << 28)
#define SPI_CONTROLLER_STATE_ASYNC_CLOCK     (1 << 27)

/* set mode and controller state based on CHIP defaults 
   these values do not matter for the legacy controller */
#if defined(CONFIG_BCM96816)
#define SPI_MODE_DEFAULT              SPI_MODE_1
#define SPI_CONTROLLER_STATE_DEFAULT  (SPI_CONTROLLER_STATE_GATE_CLK_SSOFF | SPI_CONTROLLER_STATE_CPHA_EXT)  
#elif (defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328))
#define SPI_MODE_DEFAULT              SPI_MODE_0
#define SPI_CONTROLLER_STATE_DEFAULT  (SPI_CONTROLLER_STATE_GATE_CLK_SSOFF)  
#else
#define SPI_MODE_DEFAULT              SPI_MODE_3
#define SPI_CONTROLLER_STATE_DEFAULT  (0)
#endif

#ifndef _CFE_
int BcmSpiReserveSlave(int busNum, int slaveId, int maxFreq);
int BcmSpiReserveSlave2(int busNum, int slaveId, int maxFreq, int mode, int ctrlState);
int BcmSpiReleaseSlave(int busNum, int slaveId);
int BcmSpiSyncTrans(unsigned char *txBuf, unsigned char *rxBuf, int prependcnt, int nbytes, int busNum, int slaveId);
#endif

int BcmSpi_SetFlashCtrl( int, int, int, int, int );
int BcmSpi_GetMaxRWSize( int );
int BcmSpi_Read(unsigned char *, int, int, int, int, int);
int BcmSpi_Write(unsigned char *, int, int, int, int);

#endif /* __BCMSPIRES_H__ */

