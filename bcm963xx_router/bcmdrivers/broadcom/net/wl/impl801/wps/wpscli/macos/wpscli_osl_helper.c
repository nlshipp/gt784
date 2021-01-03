/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl_helper.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Implement Linux OSL helper functions
 *
 */
#include <wpscli_osl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <tutrace.h>
#include <sys/timeb.h>
#include <sys/time.h>

void wpscli_rand_init()
{

	struct timeb tm;
	uint rnd_seed;

	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Entered.\n"));

	/* Seed the random number generator */
	ftime(&tm);
	rnd_seed = tm.millitm;
	srandom(rnd_seed);

	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Exiting.\n"));
}

uint16 wpscli_htons(uint16 v)
{
	return htons(v);
}

uint32 wpscli_htonl(uint32 v)
{
	return htonl(v);
}

uint16 wpscli_ntohs(uint16 v)
{
	return ntohs(v);
}

uint32 wpscli_ntohl(uint32 v)
{
	return ntohl(v);
}

void WpsDebugOutput(char *fmt, ...)
{
#ifndef TARGETENV_android
	char *args;
	char szString[LOG_BUFFER_SIZE];

	va_start(args, fmt);
	vsprintf(szString, fmt, args);
	printf(szString);
	printf("\n");
#endif
}

#ifdef _TUDEBUGTRACE
void wpscli_print_buf(char *text, unsigned char *buff, int buflen)
{
	int i;
	printf("\n%s : %d\n", text, buflen);
	for (i = 0; i < buflen; i++) {
		printf("%02X ", buff[i]);
		if (!((i+1)%16))
			printf("\n");
	}
	printf("\n");
}
#endif
