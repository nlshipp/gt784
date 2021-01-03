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

#ifndef __CMS_LED_H__
#define __CMS_LED_H__


/*!\file cms_led.h
 * \brief Header file for the CMS LED API.
 *  This is in the cms_util library.
 *
 */

#include "cms.h"


void cmsLed_setWanConnected(void);

void cmsLed_setWanDisconnected(void);

void cmsLed_setWanFailed(void);

#ifdef SUPPORT_GPL
#ifdef AEI_VDSL_SMARTLED
void setInetLedTrafficBlink(int state);
#endif

void cmsLed_setPowerGreen(void);

void cmsLed_setPowerAmber(void);

void cmsLed_setEthWanConnected(void);

void cmsLed_setEthWanDisconnected(void);

#if defined(SUPPORT_DSL_BONDING) && defined(CUSTOMER_NOT_USED_X)
void cmsLed_setWanLineStatus(int state);
#endif

#endif


#endif /* __CMS_LED_H__ */
