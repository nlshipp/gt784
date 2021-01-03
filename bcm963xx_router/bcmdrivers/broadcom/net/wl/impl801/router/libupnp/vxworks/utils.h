/*
 * Utils include file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: utils.h,v 1.1 2010/08/05 21:58:59 ywu Exp $
 */

#ifndef __LIBUPNP_VX_UTILS_H__
#define __LIBUPNP_VX_UTILS_H__

int snprintf(char *str, size_t count, const char *fmt, ...);
int vsnprintf(char *str, size_t count, const char *fmt, va_list args);

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, int n);
char *strsep(char **stringp, const char *delim);
char *strdup(const char *s);

#endif /* __LIBUPNP_VX_UTILS_H__ */
