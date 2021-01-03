/*
 * Linux port of wl command line utility
 *
 * Copyright 2007, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id: wlu_linux_dslcpe.c,v 1.1 2010/08/05 21:59:00 ywu Exp $
 */
#include "wlu_linux_dslcpe.h"

int
main(int argc, char **argv)
{
	return wl_libmain(argc, argv);
}
