/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl_xp.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: Description: Handling OID on Windows XP
 *
 */
#include "stdafx.h"
#include <epictrl.h>

BOOL SetOid_Xp(HANDLE irh, ULONG oid, void* data, ULONG nbytes)
{
	if(ir_setinformation((HANDLE)(void *)irh, oid, data, &nbytes) == NO_ERROR )
		return TRUE;

	return FALSE;
}

BOOL QueryOid_Xp(HANDLE irh, ULONG oid, void* results, ULONG nbytes)
{
	if(!irh)
		return FALSE;

	if(ir_queryinformation(irh, oid, results, &nbytes) == NO_ERROR)
		return TRUE;

	return FALSE;
}
