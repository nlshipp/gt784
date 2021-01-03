/*
 * Debug messages
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: tutrace.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 */

#if defined(WIN32) || defined(WINCE)
#include <windows.h>
#endif
#if !defined(WIN32)
#include <stdarg.h>
#endif
#include <stdio.h>
#if defined(__linux__) || defined(__ECOS)
#include <time.h>
#endif
#include "tutrace.h"

static WPS_TRACEMSG_OUTPUT_FN traceMsgOutputFn = NULL;

void
wps_set_traceMsg_output_fn(WPS_TRACEMSG_OUTPUT_FN fn)
{
	traceMsgOutputFn = fn;
}

#ifdef _TUDEBUGTRACE
void
print_traceMsg(int level, char *lpszFile,
                   int nLine, char *lpszFormat, ...)
{
	char szTraceMsg[2000];
	char *lpszPfx;
	int cbMsg;
	va_list lpArgv;

#ifdef _WIN32_WCE
	TCHAR szMsgW[2000];
#endif

	if (!(TUTRACELEVEL & level)) {
		return;
	}
	if (traceMsgOutputFn != NULL)
		lpszPfx = "WPS: %s(%d):";
	else
		lpszPfx = "%s(%d):";

	/* Format trace msg prefix */
	cbMsg = sprintf(szTraceMsg, lpszPfx, lpszFile, nLine);

	/* Append trace msg to prefix. */
	va_start(lpArgv, lpszFormat);
	cbMsg = vsprintf(szTraceMsg + cbMsg, lpszFormat, lpArgv);
	va_end(lpArgv);

	if (traceMsgOutputFn != NULL) {
		traceMsgOutputFn(((TUTRACELEVEL & TUERR) != 0), szTraceMsg);
	} else {
#ifndef _WIN32_WCE
	#ifdef WIN32
		OutputDebugString(szTraceMsg);
	#else /* Linux */
		fprintf(stdout, szTraceMsg);
	#endif /* WIN32 */
#else /* _WIN32_WCE */
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szTraceMsg, -1,
			szMsgW, strlen(szTraceMsg)+1);
		RETAILMSG(1, (szMsgW));
#endif /* _WIN32_WCE */
	}
}

#endif /* _TUDEBUGTRACE */
