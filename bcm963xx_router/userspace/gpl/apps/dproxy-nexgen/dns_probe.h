#ifndef _DNS_PROBE_H_
#define _DNS_PROBE_H_

/* Passive mode macros */
#ifndef CUSTOMER_VERIZON
#define DNS_RECOVER_TIMEOUT 30
#else
#define DNS_RECOVER_TIMEOUT 120
#endif
extern int dns_probe(void);
extern int dns_probe_init(void);
extern void dns_probe_switchback(void);
extern int dns_probe_response(dns_request_t *m);
extern int dns_probe_activate(uint32_t name_server);
#ifndef DNS_PROBE
extern void dns_probe_set_recover_time(void);
#endif

#endif /* _DNS_PROBE_H_ */
