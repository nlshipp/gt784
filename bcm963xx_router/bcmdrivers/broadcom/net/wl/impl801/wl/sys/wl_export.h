/*
 * Required functions exported by the port-specific (os-dependent) driver
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: wl_export.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */

#ifndef _wl_export_h_
#define _wl_export_h_

/* misc callbacks */
struct wl_info;
struct wl_if;
struct wlc_if;
extern void wl_init(struct wl_info *wl);
extern uint wl_reset(struct wl_info *wl);
extern void wl_intrson(struct wl_info *wl);
extern uint32 wl_intrsoff(struct wl_info *wl);
extern void wl_intrsrestore(struct wl_info *wl, uint32 macintmask);
extern void wl_event(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern void wl_event_sync(struct wl_info *wl, char *ifname, wlc_event_t *e);
extern int wl_up(struct wl_info *wl);
extern void wl_down(struct wl_info *wl);
extern void wl_dump_ver(struct wl_info *wl, struct bcmstrbuf *b);
extern void wl_txflowcontrol(struct wl_info *wl, bool state, int prio);
extern bool wl_alloc_dma_resources(struct wl_info *wl, uint dmaddrwidth);
extern void wl_event_sendup(struct wl_info *wl, const wlc_event_t *e, uint8 *data, uint32 len);

#ifndef LINUX_WLUSER_POSTMOGRIFY_REMOVAL
/* timer functions */
struct wl_timer;
extern struct wl_timer *wl_init_timer(struct wl_info *wl, void (*fn)(void* arg), void *arg,
                                      const char *name);
extern void wl_free_timer(struct wl_info *wl, struct wl_timer *timer);
extern void wl_add_timer(struct wl_info *wl, struct wl_timer *timer, uint ms, int periodic);
extern bool wl_del_timer(struct wl_info *wl, struct wl_timer *timer);
#endif /* LINUX_WLUSER_POSTMOGRIFY_REMOVAL */

#if defined(DSLCPE_WORKQUEUE_TASK)
/* workqueue functions */
struct wl_task;
struct wl_task *wl_init_workqueue_task(struct wl_info *wl, void (*fn)(void *arg), void *context);
extern void wl_schedule_workqueue_task(struct wl_task *task);
extern bool wl_cancel_workqueue_task(struct wl_task *task);
extern bool wl_free_workqueue_task(struct wl_info *wl, struct wl_task *task);
#endif /* DSLCPE_WORKQUEUE_TASK */

#if defined(DSLCPE) && defined(DSLCPE_DELAYTASK)
/* delayed workqueue functions */
struct wl_delayed_task;
struct wl_delayed_task *wl_init_delayed_task(struct wl_info *wl, void (*fn)(void *arg), void *context);
extern void wl_schedule_delayed_task(struct wl_delayed_task *task, unsigned long delay);
extern bool wl_cancel_delayed_task(struct wl_delayed_task *task);
extern bool wl_free_delayed_task(struct wl_info *wl, struct wl_delayed_task *task);
#endif /* DSLCPE && DSLCPE_DELAYTASK */

/* data receive and interface management functions */
extern void wl_sendup(struct wl_info *wl, struct wl_if *wlif, void *p, int numpkt);
extern char *wl_ifname(struct wl_info *wl, struct wl_if *wlif);
extern struct wl_if *wl_add_if(struct wl_info *wl, struct wlc_if* wlcif, uint unit,
	struct ether_addr *remote);
extern void wl_del_if(struct wl_info *wl, struct wl_if *wlif);

extern void wl_monitor(struct wl_info *wl, wl_rxsts_t *rxsts, void *p);
extern void wl_set_monitor(struct wl_info *wl, int val);

extern uint wl_buf_to_pktcopy(osl_t *osh, void *p, uchar *buf, int len, uint offset);
extern void * wl_get_pktbuffer(osl_t *osh, int len);
extern int wl_set_pktlen(osl_t *osh, void *p, int len);

#if defined(MACOSX) && defined(WL_BSSLISTSORT)
extern bool wl_sort_bsslist(struct wl_info *wl, wlc_bss_info_t **bip);
#else
#define wl_sort_bsslist(a, b) FALSE
#endif

#ifdef LINUX_CRYPTO
extern int wl_tkip_miccheck(struct wl_info *wl, void *p, int hdr_len, bool group_key, int id);
extern int wl_tkip_micadd(struct wl_info *wl, void *p, int hdr_len);
extern int wl_tkip_encrypt(struct wl_info *wl, void *p, int hdr_len);
extern int wl_tkip_decrypt(struct wl_info *wl, void *p, int hdr_len, bool group_key);
extern void wl_tkip_printstats(struct wl_info *wl, bool group_key);
extern int wl_tkip_keyset(struct wl_info *wl, wsec_key_t *key);
#endif /* LINUX_CRYPTO */
#endif	/* _wl_export_h_ */