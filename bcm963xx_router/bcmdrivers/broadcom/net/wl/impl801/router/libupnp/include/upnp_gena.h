/*
 * Broadcom UPnP library GENA include file
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp_gena.h,v 1.1 2010/08/05 21:58:59 ywu Exp $
 */
#ifndef  __LIBUPNP_GENA_H__
#define  __LIBUPNP_GENA_H__

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* Constants */
#define GENA_TIMEOUT            30      /* GENA check interval */
#define GENA_SUBTIME            1800    /* default subscription time */
#define GENA_MAX_HEADER         512
#define GENA_MAX_BODY           4096
#define GENA_MAX_URL            256

/*
 * Functions
 */
UPNP_SCBRCHAIN *get_subscriber_chain(UPNP_CONTEXT *context, UPNP_SERVICE *service);
UPNP_STATE_VAR *find_event_var(UPNP_CONTEXT *context, UPNP_SERVICE *service, char *name);
UPNP_EVENT *get_event(UPNP_CONTEXT *context, UPNP_STATE_VAR *state_var);
void gena_event_alarm(UPNP_CONTEXT *context, char *service_name,
	int num, char *headers[], char *ipaddr);

void gena_notify_complete(UPNP_CONTEXT *, UPNP_SERVICE *);
void gena_notify(UPNP_CONTEXT *, UPNP_SERVICE *, char *, char *);
int gena_process(UPNP_CONTEXT *);
void gena_timeout(UPNP_CONTEXT *);
int gena_init(UPNP_CONTEXT *);
int gena_shutdown(UPNP_CONTEXT *);


#ifdef __cplusplus
}
#endif

#endif /* __LIBUPNP_GENA_H__ */
