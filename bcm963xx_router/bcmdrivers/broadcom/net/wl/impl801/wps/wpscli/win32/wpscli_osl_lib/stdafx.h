/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: stdafx.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Precompile header 
 *
 */
#pragma once

#ifndef _WIN32_WINNT		  // Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <wlioctl.h>
#include <typedefs.h>
#include <tutrace.h>
#include "wpscli_osl.h"
#include "wpscli_osl_common.h"
