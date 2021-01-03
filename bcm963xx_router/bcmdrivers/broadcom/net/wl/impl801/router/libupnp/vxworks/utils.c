/*
 * Utils
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: utils.c,v 1.1 2010/08/05 21:58:59 ywu Exp $
 */

#include "vxWorks.h"
#include "sockLib.h"
#include "inetLib.h"
#include "stdlib.h"
#include "memLib.h"
#include "stdio.h"
#include "ioLib.h"
#include "hostLib.h"
#include "ctype.h"


/*
 * The strcasecmp() function compares the two strings s1 and s2,
 * ignor­ing the case of the characters. It returns an integer less
 * than, equal to, or greater than zero if s1 is found, respectively,
 * to be less than, to match, or be greater than s2.
 */
int
strcasecmp(const char *t1, const char *t2)
{
	while (tolower(*t1) == tolower(*t2)) {
		t2++;
		if (*t1++ == '\0')
			return 0;
	}
	return (tolower(*t1) - tolower(*t2));
}

/*
 * The strncasecmp() function is similar, except it only compares the
 * first n characters of s1.
 */
int
strncasecmp(const char *t1, const char *t2, int n)
{
	while (n > 0 && tolower(*t1) == tolower(*t2)) {
		t2++;
		if (--n == 0 || *t1++ == '\0')
			return (0);
	}
	return (n > 0 ? tolower(*t1) - tolower(*t2) : 0);
}

char *
strdup(const char *s)
{
	return strcpy(malloc(strlen(s)+1), s);
}


/*
 * If *stringp is NULL, the strsep() function returns NULL and does
 * nothing else. Otherwise, this function finds the first token in the
 * string *stringp, where tokens are delimited by symbols in the string
 * delim. This token is terminated with a `\0' character
 * (by overwrit­ing the delimiter) and *stringp is updated to point
 * past the token.  In case no delimiter was found, the token is taken
 * to be the entire string *stringp, and *stringp is made NULL.
 */
char *
strsep(char **stringp, const char *delim)
{
	char *tok, *p;

	if (stringp == NULL || *stringp == NULL)
		return NULL;
	tok = *stringp;
	p = strpbrk(*stringp, delim);
	if (p) {
		*p = '\0';
		*stringp = p+1;
	}
	else {
		*stringp = NULL;
	}

	return tok;
}
