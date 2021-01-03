/* vi: set sw=4 ts=4: */
/*
 * Mini nslookup implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Correct default name server display and explicit name server option
 * added by Ben Zeckel <bzeckel@hmc.edu> June 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <stdint.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/inet.h>
#include "busybox.h"

/*
 |  I'm only implementing non-interactive mode;
 |  I totally forgot nslookup even had an interactive mode.
 */
#if defined(AEI_VDSL_SMARTLED)
#include "cms_msg.h"
#include "cms_util.h"
#include "cms_log.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>

#define MAXPACKET 65468
#define __NETDB_MAXADDRS 35
#define __NETDB_MAXALIASES 35
#define pthread_ipaddr_type int 
#define HOST_BUFSIZE 4096
#define ALIGN(p, t) ((char *)(((((long)(p) - 1) / sizeof(t)) + 1) * sizeof(t)))
#define SP(p, t, n) (ALIGN(p, t) + (n) * sizeof(t))

struct __res_state _res = {
	RES_TIMEOUT,               	/* retransmition time interval */
	4,                         	/* number of times to retransmit */
	RES_DEFAULT,				/* options flags */
	1,                         	/* number of name servers */
};

struct	res_data {
	char *buf;
	struct __res_state state;
	int errval;
	int sock;
};

typedef union {
	HEADER hdr;
	unsigned char buf[MAXPACKET];
} querybuf;

typedef union {
	long al;
	char ac;
} align;


/* Performs global initialization. */
struct res_data *globledata = NULL;
static struct __res_state start;
char dns_txt[1024] = {0};
static void *msgHandle=NULL;

struct res_data *_res_init()
{
	struct res_data *data;
	
	/* Initialize thread-specific data for this thread if it hasn't
	 * been done already. */
	if (globledata)
        return globledata;
	if (globledata == NULL) 
		globledata = (struct res_data *) malloc(sizeof(struct res_data));
	if (globledata == NULL)
		return NULL;
		
	globledata->buf = NULL;
	globledata->state = start;
	globledata->errval = NO_RECOVERY;
	globledata->sock = -1;
	
	return globledata;
}

struct __res_state *_res_status()
{
	struct res_data *data;
	
	data = _res_init();
	return (data) ? &data->state : NULL;
}

static int qcomp(const void *arg1, const void *arg2)
{
	const struct in_addr **a1 = (const struct in_addr **) arg1;
	const struct in_addr **a2 = (const struct in_addr **) arg2;
	struct __res_state *state = _res_status();
	
	int pos1, pos2;
	
	for (pos1 = 0; pos1 < state->nsort; pos1++) {
		if (state->sort_list[pos1].addr.s_addr ==
			((*a1)->s_addr & state->sort_list[pos1].mask))
			break;
	}
	for (pos2 = 0; pos2 < state->nsort; pos2++) {
		if (state->sort_list[pos2].addr.s_addr ==
			((*a2)->s_addr & state->sort_list[pos2].mask))
			break;
	}
	return pos1 - pos2;
}

__dn_skipname(comp_dn, eom)
	const u_char *comp_dn, *eom;
{
	register u_char *cp;
	register int n;

	cp = (u_char *)comp_dn;
	while (cp < eom && (n = *cp++)) {
		/*
		 * check for indirection
		 */
		switch (n & INDIR_MASK) {
		  case 0:				/* normal case, n == len */
			cp += n;
			continue;
		  case INDIR_MASK:		/* indirection */
			cp++;
			break;
		  default:				/* illegal type */
			return (-1);
		}
		break;
	}
	if (cp > eom)
		return -1;
	return (cp - comp_dn);
}

u_short
_getshort(msgp)
	register const u_char *msgp;
{
	register u_short u;

	GETSHORT(u, msgp);
	return (u);
}


struct hostent *_res_parse_answer(querybuf *answer, int anslen, int iquery,
								  struct hostent *result, char *buf,
								  int bufsize, int *errval)
{
	struct res_data *data = _res_init();
	register HEADER *hp;
	register u_char *cp;
	register int n;
	u_char *eom;
	char *aliases[__NETDB_MAXALIASES], *addrs[__NETDB_MAXADDRS];
	char *bp = buf, **ap = aliases, **hap = addrs;
	int type, class, ancount, qdcount, getclass = C_ANY, iquery_done = 0;
	
	eom = answer->buf + anslen;
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = buf;
	cp = answer->buf + sizeof(HEADER);

	/* Read in the hostname if this is an address lookup. */
	if (qdcount) {
		if (iquery) {
			if ((n = dn_expand((u_char *) answer->buf,
							   (u_char *) eom, (u_char *) cp, (u_char *) bp,
							   bufsize - (bp - buf))) < 0) {
				*errval = NO_RECOVERY;
				return ((struct hostent *) NULL);
			}
			cp += n + QFIXEDSZ;
			result->h_name = bp;
			bp += strlen(bp) + 1;
		} else {
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
		}
		while (--qdcount > 0)
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
	} else if (iquery) {
		*errval = (hp->aa) ? HOST_NOT_FOUND : TRY_AGAIN;
		return ((struct hostent *) NULL);
	}

	/* Read in the answers. */
	*ap = NULL;
	*hap = NULL;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand((u_char *) answer->buf, (u_char *) eom,
						   (u_char *) cp, (u_char *) bp,
						   bufsize - (bp - buf))) < 0)
			break;
		cp += n;
		type = _getshort(cp);
		cp += sizeof(u_short);
		class = _getshort(cp);
		cp += sizeof(u_short) + sizeof(pthread_ipaddr_type);
		n = _getshort(cp);
		cp += sizeof(u_short);
		if (type == T_CNAME) {
			cp += n;
			if (ap >= aliases + __NETDB_MAXALIASES - 1)
				continue;
			*ap++ = bp;
			bp += strlen(bp) + 1;
			continue;
		}
		if (iquery && type == T_PTR) {
			if ((n = dn_expand((u_char *) answer->buf, (u_char *) eom,
							   (u_char *) cp, (u_char *) bp,
							   bufsize - (bp - buf))) < 0)
				break;
			cp += n;
			result->h_name = bp;
			bp += strlen(bp) + 1;
			iquery_done = 1;
			break;
		}
		
		if (iquery || type == 0x0010)	{
			//printf("expected answer type %d, size %d\n", type, n);
			strncpy(dns_txt, cp, n);
			dns_txt[n] = '\0';
			printf("dns txt %s\n", dns_txt);
			cp += n;
			//iquery_done = 1;
			//continue;
		}
		
		if (hap > addrs) {
			if (n != result->h_length) {
				cp += n;
				continue;
			}
			if (class != getclass) {
				cp += n;
				continue;
			}
		} else {
			result->h_length = n;
			getclass = class;
			result->h_addrtype = (class == C_IN) ? AF_INET : AF_UNSPEC;
			if (!iquery) {
				result->h_name = bp;
				bp += strlen(bp) + 1;
			}
		}
		
		bp = ALIGN(bp, char *);
		//bp = ALIGN(bp, pthread_ipaddr_type);
		if (bp + n >= buf + bufsize) {
			errno = ERANGE;
			return NULL;
		}
		memcpy(bp, cp, n);
		cp += n;
		if (hap >= addrs + __NETDB_MAXADDRS - 1)
			continue;
		*hap++ = bp;
		bp += n;
		cp += n;
	}

	if (hap > addrs || iquery_done) {
		*ap++ = NULL;
		*hap++ = NULL;
		if (data->state.nsort)
			qsort(addrs, hap - addrs, sizeof(struct in_addr), qcomp);
		if (SP(bp, char *, (hap - addrs) + (ap - aliases)) > buf + bufsize) {
			errno = ERANGE;
			return NULL;
		}
		result->h_addr_list = (char **) ALIGN(bp, char *);
		memcpy(result->h_addr_list, addrs, (hap - addrs) * sizeof(char *));
		result->h_aliases = result->h_addr_list + (hap - addrs);
		memcpy(result->h_aliases, aliases, (ap - aliases) * sizeof(char *));
		return result;
	} else {
		*errval = TRY_AGAIN;
		return NULL;
	}
}

static struct hostent *fake_hostent(const char *hostname, struct in_addr addr,
									struct hostent *result, char *buf,
									int bufsize, int *errval)
{
	int len = strlen(hostname);
	char *name, *addr_ptr;

	if (SP(SP(SP(buf, char, len + 1), addr, 1), char *, 3) > buf + bufsize) {
		errno = ERANGE;
		return NULL;
	}

	/* Copy faked name and address into buffer. */
	strcpy(buf, hostname);
	name = buf;
	buf = ALIGN(buf + len + 1, addr);
	*((struct in_addr *) buf) = addr;
	addr_ptr = buf;
	buf = ALIGN(buf + sizeof(addr), char *);
	((char **) buf)[0] = addr_ptr;
	((char **) buf)[1] = NULL;
	((char **) buf)[2] = NULL;

	result->h_name = name;
	result->h_aliases = ((char **) buf) + 2;
	result->h_addrtype = AF_INET;
	result->h_length = sizeof(addr);
	result->h_addr_list = (char **) buf;

	return result;
}

static struct hostent *gethostbyname_txt_r(const char *hostname, struct hostent *result,
								char *buf, int bufsize, int *errval)
{
	struct in_addr addr;
	querybuf qbuf;
	const char *p;
	int n;

	/* Default failure condition is not a range error and not recoverable. */
	errno = 0;
	*errval = NO_RECOVERY;
	
	/* Check for all-numeric hostname with no trailing dot. */
	if (isdigit(hostname[0])) {
		p = hostname;
		while (*p && (isdigit(*p) || *p == '.'))
			p++;
		if (!*p && p[-1] != '.') {
			/* Looks like an IP address; convert it. */
			if (inet_aton(hostname, &addr) == -1) {
				*errval = HOST_NOT_FOUND;
				return NULL;
			}
			return fake_hostent(hostname, addr, result, buf, bufsize, errval);
		}
	}
	
	/* Do the search. */
	//n = res_search(hostname, C_IN, T_A, qbuf.buf, sizeof(qbuf));
	n = res_search(hostname, C_IN, 0x0010, qbuf.buf, sizeof(qbuf));
	if (n >= 0)
		return _res_parse_answer(&qbuf, n, 0, result, buf, bufsize, errval);	
	else
		return NULL;
}

static struct hostent *gethostbyname_txt(const char *hostname)
{
	struct res_data *data = _res_init();

	if (!data)
		return NULL;
	if (!data->buf) {
		data->buf = malloc(sizeof(struct hostent) + HOST_BUFSIZE);
		if (!data->buf) {
			errno = 0;
			data->errval = NO_RECOVERY;
			return NULL;
		}
	}
	return gethostbyname_txt_r(hostname, (struct hostent *) data->buf,
						   data->buf + sizeof(struct hostent), HOST_BUFSIZE,
						   &data->errval);
}

void sendTxtRecordForNslookup(char *dnsTxt)
{
   char buf[sizeof(CmsMsgHeader) + 512]={0};
   CmsMsgHeader *msg=(CmsMsgHeader *) buf;
   char *txtRecord = (char*)(msg+1);
   int strLength = 0;
   CmsRet ret;
	printf("++++++++++++dnsTxt %s\n",dnsTxt);
   if (dnsTxt)
   {
	  strLength = sprintf(txtRecord, "%s", dnsTxt);
   }
   else
   {
      strLength = sprintf(txtRecord, "%s", "NSLookUp_NoResponse");
   }	
   msg->type = CMS_MSG_NSLOOKUP_TXT_RDATA;
   msg->src = EID_NSLOOKUP;
   msg->dst = EID_SSK;
   msg->flags_event = 1;
   msg->dataLength = strLength;


   if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
   {
      cmsLog_error("could not send out CMS_MSG_NSLOOKUP_STATE_CHANGED to SSK, txtRecord : %s", txtRecord);
   }
   else
   {
      cmsLog_debug("sent out CMS_MSG_NSLOOKUP_TXT_RDATA to SSK txtRecord : %s", txtRecord);
   }
   cmsMsg_cleanup(&msgHandle);
   return;
}

void led_cleanup()
{
	if (globledata)
	{
		if (globledata->buf != NULL)
		{
			free(globledata->buf);
		}
		free(globledata);
	}
}
#endif
/* only works for IPv4 */
static int addr_fprint(char *addr)
{
	uint8_t split[4];
	uint32_t ip;
	uint32_t *x = (uint32_t *) addr;

	ip = ntohl(*x);
	split[0] = (ip & 0xff000000) >> 24;
	split[1] = (ip & 0x00ff0000) >> 16;
	split[2] = (ip & 0x0000ff00) >> 8;
	split[3] = (ip & 0x000000ff);
	printf("%d.%d.%d.%d", split[0], split[1], split[2], split[3]);
	return 0;
}

/* takes the NULL-terminated array h_addr_list, and
 * prints its contents appropriately
 */
static int addr_list_fprint(char **h_addr_list)
{
	int i, j;
	char *addr_string = (h_addr_list[1])
		? "Addresses: " : "Address:   ";

	printf("%s ", addr_string);
	for (i = 0, j = 0; h_addr_list[i]; i++, j++) {
		addr_fprint(h_addr_list[i]);

		/* real nslookup does this */
		if (j == 4) {
			if (h_addr_list[i + 1]) {
				printf("\n          ");
			}
			j = 0;
		} else {
			if (h_addr_list[i + 1]) {
				printf(", ");
			}
		}

	}
	printf("\n");
	return 0;
}

/* print the results as nslookup would */
static struct hostent *hostent_fprint(struct hostent *host, const char *server_host)
{
	if (host) {
		printf("%s     %s\n", server_host, host->h_name);
		addr_list_fprint(host->h_addr_list);
	} else {
		printf("*** Unknown host\n");
	}
	return host;
}

/* changes a c-string matching the perl regex \d+\.\d+\.\d+\.\d+
 * into a uint32_t
 */
static uint32_t str_to_addr(const char *addr)
{
	uint32_t split[4];
	uint32_t ip;

	sscanf(addr, "%d.%d.%d.%d",
		   &split[0], &split[1], &split[2], &split[3]);

	/* assuming sscanf worked */
	ip = (split[0] << 24) |
		(split[1] << 16) | (split[2] << 8) | (split[3]);

	return htonl(ip);
}

/* gethostbyaddr wrapper */
static struct hostent *gethostbyaddr_wrapper(const char *address)
{
	struct in_addr addr;

	addr.s_addr = str_to_addr(address);
	return gethostbyaddr((char *) &addr, 4, AF_INET);	/* IPv4 only for now */
}

/* lookup the default nameserver and display it */
static inline void server_print(void)
{
	struct sockaddr_in def = _res.nsaddr_list[0];
	char *ip = inet_ntoa(def.sin_addr);

	hostent_fprint(gethostbyaddr_wrapper(ip), "Server:");
	printf("\n");
}

/* alter the global _res nameserver structure to use
   an explicit dns server instead of what is in /etc/resolv.h */
static inline void set_default_dns(char *server)
{
	struct in_addr server_in_addr;

	if(inet_aton(server,&server_in_addr))
	{
		_res.nscount = 1;
		_res.nsaddr_list[0].sin_addr = server_in_addr;
	}
}

/* naive function to check whether char *s is an ip address */
static int is_ip_address(const char *s)
{
	while (*s) {
		if ((isdigit(*s)) || (*s == '.')) {
			s++;
			continue;
		}
		return 0;
	}
	return 1;
}

/* ________________________________________________________________________ */
int nslookup_main(int argc, char **argv)
{
	struct hostent *host;

	/*
	* initialize DNS structure _res used in printing the default
	* name server and in the explicit name server option feature.
	*/

	res_init();
#if defined(AEI_VDSL_SMARTLED)
	cmsLog_init(EID_NSLOOKUP);
   	cmsLog_setLevel(DEFAULT_LOG_LEVEL);
        cmsMsg_init(EID_NSLOOKUP, &msgHandle);
#endif
	/*
	* We allow 1 or 2 arguments.
	* The first is the name to be looked up and the second is an
	* optional DNS server with which to do the lookup.
	* More than 3 arguments is an error to follow the pattern of the
	* standard nslookup
	*/

	if (argc < 2 || *argv[1]=='-' || argc > 3)
		bb_show_usage();
	else if(argc == 3)
		set_default_dns(argv[2]);

	server_print();
	if (is_ip_address(argv[1])) {
		host = gethostbyaddr_wrapper(argv[1]);
	} else {
#if defined (AEI_VDSL_SMARTLED)
		host = gethostbyname_txt(argv[1]);
#else
		host = xgethostbyname(argv[1]);
#endif
	}
#if !defined (AEI_VDSL_SMARTLED)
	hostent_fprint(host, "Name:  ");
#endif
	if (host) {
#if defined (AEI_VDSL_SMARTLED)
		sendTxtRecordForNslookup(dns_txt);
		led_cleanup();	
#endif
		return EXIT_SUCCESS;
	}
#if defined (AEI_VDSL_SMARTLED)
	sendTxtRecordForNslookup(NULL);
	led_cleanup();	
#endif
	return EXIT_FAILURE;
}

/* $Id: nslookup.c,v 1.2 2010/09/21 05:34:52 xxia Exp $ */
