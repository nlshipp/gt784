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
 * Description: Implement timer related OSL APIs  
 *
 */
#include "stdafx.h"

static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
static const __int64 SECS_TO_100NS = 10000000; /* 10^7 */

void wpscli_sleep(unsigned long milli_seconds)
{
	Sleep(milli_seconds);
}

unsigned long wpscli_current_time()
{
	SYSTEMTIME systemTime;
	FILETIME fileTime;
	__int64 UnixTime;

	GetSystemTime( &systemTime );
	SystemTimeToFileTime( &systemTime, &fileTime );


	/* get the full win32 value, in 100ns */
	UnixTime = ((__int64)fileTime.dwHighDateTime << 32) + fileTime.dwLowDateTime;


	/* convert to the Unix epoch */
	UnixTime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);

	UnixTime /= SECS_TO_100NS; /* now convert to seconds */

	return (long)(UnixTime);
}
