/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
 *
 ************************************************************************/

#ifdef AEI_VDSL_CUSTOMER_NCS
#include "fcntl.h"
#endif

#include "cms.h"
#include "cms_util.h"
#include "cms_boardioctl.h"
#include "adsldrv.h"

/*
 * See:
 * bcmdrivers/opensource/include/bcm963xx/board.h
 * bcmdrivers/opensource/char/board/bcm963xx/impl1/board.c and bcm63xx_led.c
 */

void cmsLed_setWanConnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateOn, NULL);
}


void cmsLed_setWanDisconnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateOff, NULL);
}


void cmsLed_setWanFailed(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedWanData, kLedStateFail, NULL);
}

#ifdef AEI_VDSL_CUSTOMER_NCS
static int boardIoctl(int boardFd, int board_ioctl, BOARD_IOCTL_ACTION action,
                      char *string, int strLen, int offset)
{
    BOARD_IOCTL_PARMS IoctlParms;
    
    IoctlParms.string = string;
    IoctlParms.strLen = strLen;
    IoctlParms.offset = offset;
    IoctlParms.action = action;

    ioctl(boardFd, board_ioctl, &IoctlParms);

    return (IoctlParms.result);
}

void sysGPIOCtrl(int gpio, GPIO_STATE_t state)
{
    int boardFd;

    if ((boardFd = open("/dev/brcmboard", O_RDWR)) == -1)
        printf("Unable to open device /dev/brcmboard.\n");

    boardIoctl(boardFd, BOARD_IOCTL_SET_GPIO, 0, "", (int)gpio, (int)state);
    close(boardFd);
}

#ifdef AEI_VDSL_SMARTLED
void setInetLedTrafficBlink(int state)
{
    int boardFd;

    if ((boardFd = open("/dev/brcmboard", O_RDWR)) == -1)
        printf("Unable to open device /dev/brcmboard.\n");

    boardIoctl(boardFd, BOARD_IOCTL_SET_INET_TRAFFIC_BLINK, 0, "", state, 1);
    close(boardFd);
}
#endif

void cmsLed_setPowerGreen(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedPower, kLedStateOn, NULL);
}

void cmsLed_setPowerAmber(void)
{
    sysGPIOCtrl(24, kGpioActive);
    sysGPIOCtrl(25, kGpioActive);
}

void cmsLed_setEthWanConnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedSecAdsl, kLedStateOn, NULL);
}

void cmsLed_setEthWanDisconnected(void)
{
   devCtl_boardIoctl(BOARD_IOCTL_LED_CTRL, 0, NULL, kLedSecAdsl, kLedStateOff, NULL);
}

#if defined(SUPPORT_DSL_BONDING) && defined(AEI_VDSL_CUSTOMER_QWEST_Q2000)
void cmsLed_setWanLineStatus(int state)
{
    int boardFd;

    if ((boardFd = open("/dev/bcmadsl0", O_RDWR)) == -1)
        printf("Unable to open device /dev/bcmadsl0.\n");
    else
        ioctl(boardFd,ADSLIOCTL_SET_DSL_WAN_STATUS,state);
    close(boardFd);
}
#endif
#endif

