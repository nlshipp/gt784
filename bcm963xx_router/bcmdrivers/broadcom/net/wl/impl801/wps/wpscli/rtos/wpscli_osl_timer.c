/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl_timer.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: RTOS timer functions
 *
 */

unsigned long get_current_time(void);
void WpsSleep(unsigned int milliseconds);

unsigned long wpscli_current_time(void)
{
	return get_current_time();
}

void wpscli_sleep(unsigned long milli_seconds)
{
	WpsSleep(milli_seconds);
}
