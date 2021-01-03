/* leases.h */
#ifndef _LEASES_H
#define _LEASES_H
#if defined(AEI_VDSL_WP) && defined(AEI_VDSL_DHCP_LEASE)
#include "aei_cms.h"
#endif


struct dhcpOfferedAddr {
	u_int8_t chaddr[16];
	u_int32_t yiaddr;	/* network order */
	u_int32_t expires;	/* host order */
	char hostname[64];
	// BRCM
#if defined(SUPPORT_GPL)
        int icon;               /* get device identifier according to option 12/60 */
#endif

#ifdef SUPPORT_GPL
		u_int32_t is_stb;
#endif

#if defined (SUPPORT_GPL)
		char clientid[256]; /* Client Identifier DHCP option (Option 61) */
#endif	

   char vendorid[256]; /* vendor class id, option 60, rfc 2132, max length is 255.  We add 1 for the null. */
   char classid[256];  /* user class id, option 77, rfc 3004, max length is 255.  We add 1 for the null. */
   char oui[8];
   char serialNumber[64];
   char productClass[64];
#if defined(AEI_VDSL_WP) && defined(AEI_VDSL_DHCP_LEASE)
    u_int32_t isWP;
    char WPProductType[AEI_WP_PRODUCT_TYPE_LEN];
    char WPFirmwareVersion[AEI_WP_FIRMWARE_LEN];
    char WPProtocolVersion[AEI_WP_PROTOCOL_LEN];
#endif

};


void clear_lease(u_int8_t *chaddr, u_int32_t yiaddr);
struct dhcpOfferedAddr *add_lease(u_int8_t *chaddr, u_int32_t yiaddr, unsigned long lease);
int lease_expired(struct dhcpOfferedAddr *lease);
int lease_time_remaining(const struct dhcpOfferedAddr *lease);
struct dhcpOfferedAddr *oldest_expired_lease(void);
struct dhcpOfferedAddr *find_lease_by_chaddr(u_int8_t *chaddr);
struct dhcpOfferedAddr *find_lease_by_yiaddr(u_int32_t yiaddr);
u_int32_t find_address(int check_expired);
struct iface_config_t *find_iface_by_ifname(const char *name);
int check_ip(u_int32_t addr);


#endif
