/* serverpacket.c
 *
 * Constuct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "packet.h"
#include "debug.h"
#include "dhcpd.h"
#include "options.h"
#include "leases.h"
#include "static_leases.h"

/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
        DEBUG(LOG_INFO, "Forwarding packet to relay");

        return kernel_packet(payload, cur_iface->server, SERVER_PORT,
                        payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
        u_int32_t ciaddr;
        char chaddr[6];
        
        if (force_broadcast) {
                DEBUG(LOG_INFO, "broadcasting packet to client (NAK)");
                ciaddr = INADDR_BROADCAST;
                memcpy(chaddr, MAC_BCAST_ADDR, 6);              
        } else if (payload->ciaddr) {
                DEBUG(LOG_INFO, "unicasting packet to client ciaddr");
                ciaddr = payload->ciaddr;
                memcpy(chaddr, payload->chaddr, 6);
        } else if (ntohs(payload->flags) & BROADCAST_FLAG) {
                DEBUG(LOG_INFO, "broadcasting packet to client (requested)");
                ciaddr = INADDR_BROADCAST;
                memcpy(chaddr, MAC_BCAST_ADDR, 6);              
        } else {
                DEBUG(LOG_INFO, "unicasting packet to client yiaddr");
                ciaddr = payload->yiaddr;
                memcpy(chaddr, payload->chaddr, 6);
        }
        return raw_packet(payload, cur_iface->server, SERVER_PORT, 
                        ciaddr, CLIENT_PORT, chaddr, cur_iface->ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
        int ret;

        if (payload->giaddr)
                ret = send_packet_to_relay(payload);
        else ret = send_packet_to_client(payload, force_broadcast);
        return ret;
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
        memset(packet, 0, sizeof(struct dhcpMessage));
        
        packet->op = BOOTREPLY;
        packet->htype = ETH_10MB;
        packet->hlen = ETH_10MB_LEN;
        packet->xid = oldpacket->xid;
        memcpy(packet->chaddr, oldpacket->chaddr, 16);
        packet->cookie = htonl(DHCP_MAGIC);
        packet->options[0] = DHCP_END;
        packet->flags = oldpacket->flags;
        packet->giaddr = oldpacket->giaddr;
        packet->ciaddr = oldpacket->ciaddr;
        add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
        add_simple_option(packet->options, DHCP_SERVER_ID,
		ntohl(cur_iface->server)); /* expects host order */
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
        packet->siaddr = cur_iface->siaddr;
        if (cur_iface->sname)
                strncpy(packet->sname, cur_iface->sname,
			sizeof(packet->sname) - 1);
        if (cur_iface->boot_file)
                strncpy(packet->file, cur_iface->boot_file,
			sizeof(packet->file) - 1);
}

//add by vincent 2010-12-23
#if defined(AEI_VDSL_CUSTOMER_TELUS)
/*
 * check the ip is in the lease range
 */
static int is_in_range(int vendor, u_int32_t req_align)
{

        if (vendor == 1) {
            if (ntohl(req_align) >= ntohl(cur_iface->vendorClassIdMinAddress) &&
	        ntohl(req_align) <= ntohl(cur_iface->vendorClassIdMaxAddress))
                return 1;
            else
                return 0;
        }
        if (vendor == 2) {
            if (ntohl(req_align) >= ntohl(cur_iface->start) &&
                ntohl(req_align) <= ntohl(cur_iface->end))
                return 1;
        } 
	return 0;
}
#endif

#ifdef AEI_VDSL_CUSTOMER_QWEST
void custom_dns_option(struct dhcpMessage *packet, struct option_set *curr)
{
    unsigned char data[256];
    char mac_addr[20] = {0};
    struct ip_list *p;
    int len = 0;
    
    memset(data, 0, sizeof(data));

    if (cur_iface->dns_proxy_ip != 0 && !IS_EMPTY_STRING(cur_iface->dns_passthru_chaddr))
    {
        cmsUtl_macNumToStr(packet->chaddr, mac_addr);

        if (strcasecmp(cur_iface->dns_passthru_chaddr, mac_addr) == 0)
        {
            for (p = cur_iface->dns_srv_ips; p; p = p->next)
            {
                if (p->ip != cur_iface->dns_proxy_ip &&
                    sizeof(u_int32_t) <= sizeof(data) - len)
                {
                    memcpy(data + len, &p->ip, sizeof(u_int32_t));
                    len += sizeof(u_int32_t);
                }
            }
        }
        else
        {
            memcpy(data, &cur_iface->dns_proxy_ip, sizeof(u_int32_t));
            len += sizeof(u_int32_t);
        }
    }
    else
    {
        if (cur_iface->dns_proxy_ip != 0)
        {
            memcpy(data, &cur_iface->dns_proxy_ip, sizeof(u_int32_t));
            len += sizeof(u_int32_t);
        }

        for (p = cur_iface->dns_srv_ips; p; p = p->next)
        {
            if (p->ip != cur_iface->dns_proxy_ip &&
                sizeof(u_int32_t) <= sizeof(data) - len)
            {
                memcpy(data + len, &p->ip, sizeof(u_int32_t));
                len += sizeof(u_int32_t);
            }
        }
    }

    if (curr->data)
        free(curr->data);

    curr->data = malloc(len + 2);
    curr->data[OPT_CODE] = DHCP_DNS_SERVER;
    curr->data[OPT_LEN] = len;
    memcpy(curr->data + 2, data, len);
}
#endif

#ifdef AEI_VDSL_TR098_TELUS
static UBOOL8 wantOption(struct dhcpMessage *oldpacket,int option)
{
    int i, len;
    char *param_list = NULL;
    UBOOL8 found = FALSE;
    
    if (oldpacket)
    {
        param_list = get_option(oldpacket, DHCP_PARAM_REQ);
        if (param_list)
        {
            len = *(param_list - 1);
            for (i = 0; i < len; i++)
            {
                if (*(param_list + i) == option)
                {
                    found = TRUE;
                    break;
                }
            }
        }
    }
    return found;
}

static UBOOL8 is_IPTVSTB(struct dhcpMessage *oldpacket)
{   
    int i; 
    UBOOL8 stb = FALSE;
    int len = 0;
    char vendorid[256] = {0};

    if (oldpacket)
    {
        unsigned char *vid = get_option(oldpacket, DHCP_VENDOR);

        if (vid)
        {
           len = *(vid - 1);
           memcpy(vendorid, vid, (size_t)len);
 
           for (i = 0; i < VENDOR_CLASS_ID_TAB_SIZE; i++)
           {
               if (cur_iface->opt67WarrantVids[i] 
#if defined (FUNCTION_ACTIONTEC_QOS)
                   && !wstrcmp(cur_iface->opt67WarrantVids[i], vendorid)
#endif
                   )
               {
                   stb = TRUE;
                   break;
               }
           }
        }
    }
    return stb;
}
#endif

#ifdef AEI_VDSL_CUSTOMER_TELUS

#define NIPQUID(addr) \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]

UBOOL8 is_stb(const char *vid)
{
    int i;
    UBOOL8 stb = FALSE;

    if (vid && *vid)
    {
        for (i = 0; i < VENDOR_CLASS_ID_TAB_SIZE; i++)
        {
            if (cur_iface->stbVids[i] 
#if defined(FUNCTION_ACTIONTEC_QOS)
                && !wstrcmp(cur_iface->stbVids[i], vid)
#endif
                )
            {
                stb = TRUE;
                break;
            }
        }
    }

    return stb;
}

static void do_mark(UBOOL8 mark)
{
    char cmd[256];
    struct ip_list *ip;

    for (ip = cur_iface->stb_ip_list; ip; ip = ip->next)
    {
        if (mark)
        {
            sprintf(cmd, "iptables -I INPUT 1 -i br0 -p UDP --dport 53 -s %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
            sprintf(cmd, "iptables -I FORWARD 1 -s %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
            sprintf(cmd, "iptables -I FORWARD 1 -d %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
        }
        else
        {
            sprintf(cmd, "iptables -D INPUT -i br0 -p UDP --dport 53 -s %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
            sprintf(cmd, "iptables -D FORWARD -s %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
            sprintf(cmd, "iptables -D FORWARD -d %u.%u.%u.%u -j ACCEPT", NIPQUID(ip->ip));
            system(cmd);
        }
    }
}

static UBOOL8 find_ip(u_int32_t ip)
{
    UBOOL8 found = FALSE;
    struct ip_list *item;

    for (item = cur_iface->stb_ip_list; item; item = item->next)
    {
        if (item->ip == ip)
        {
            found = TRUE;
            break;
        }
    }

    return found;
}

static void clear_stb_list(void)
{
    struct ip_list *ip, *pre, *next;
    struct dhcpOfferedAddr *lease;
    UBOOL8 clear = TRUE;

    ip = cur_iface->stb_ip_list;
    pre = next = NULL;

    while (ip)
    {
        clear = TRUE;

        lease = find_lease_by_yiaddr(ip->ip);
        if (lease && lease->is_stb && !lease_expired(lease))
            clear = FALSE;

        next = ip->next;
        if (clear)
        {
            if (!pre)
                cur_iface->stb_ip_list = next;
            else
                pre->next = next;

            free(ip);
            ip = next;
        }
        else
        {
            pre = ip;
            ip = next;
        }
    }
}

void add_stb_list(u_int32_t ip)
{
    struct ip_list *item;

    if (!find_ip(ip))
    {
        item = calloc(1, sizeof(struct ip_list));
        if (item)
        {
            item->ip = ip;
            item->next = cur_iface->stb_ip_list;
            cur_iface->stb_ip_list = item;
        }
    }
}

void update_stb_mark(void)
{
    do_mark(FALSE);
    clear_stb_list();
    do_mark(TRUE);
}

void mark_stb(const char *vid, u_int32_t ip)
{
    do_mark(FALSE);

    if (is_stb(vid))
        add_stb_list(ip);

    clear_stb_list();

    do_mark(TRUE);
}
#endif 


/* send a DHCP OFFER to a DHCP DISCOVER */
int sendOffer(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;
        struct dhcpOfferedAddr *lease = NULL;
        u_int32_t req_align, lease_time_align = cur_iface->lease;
        char *req, *lease_time;
        struct option_set *curr;
        struct in_addr addr;
	//For static IP lease
	uint32_t static_lease_ip;

        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        //brcm end
		
	#if defined(AEI_VDSL_CUSTOMER_NCS) //add william 2012-1-11
	    /* */
        int vendor;
        char vendorid[VENDOR_CLASS_ID_STR_SIZE + 1];
        vendorid[0] = '\0';
        /* */

        vendor = is_vendor_equipped(oldpacket, vendorid);
	
        switch (vendor) {
        case 1:
            /* client discover match in the server config */
#if 0 //debug
            LOG(LOG_ERR, "(debug message) DHCP server equipped the"
                " vendorClassId:'%s' MAC=%02X:%02X:%02X:%02X:%02X:%02X\n"
                "Ignore this client DHCP DISCOVERY in the local server.",
                vendorid,
                oldpacket->chaddr[0],
                oldpacket->chaddr[1],
                oldpacket->chaddr[2],
                oldpacket->chaddr[3],
                oldpacket->chaddr[4],
                oldpacket->chaddr[5],
                oldpacket->chaddr[6]);
#endif
            return 1;
        default:
            /*
             * == 0 client discover not match in the server config
             * == 2 client send a DHCP discover not include option-60
             */
            break;
        }
#endif		

        init_packet(&packet, oldpacket, DHCPOFFER);
        
	//For static IP lease
	static_lease_ip = getIpByMac(cur_iface->static_leases,
		oldpacket->chaddr);

	if(!static_lease_ip) {
        	/* the client is in our lease/offered table */
#if defined(AEI_VDSL_CUSTOMER_NCS)
                if ((lease = find_lease_by_chaddr(oldpacket->chaddr)) && (!lease_expired(lease))) {
#else
                if ((lease = find_lease_by_chaddr(oldpacket->chaddr))) {
#endif
#if defined(AEI_VDSL_CUSTOMER_NCS)
#else
                	if (!lease_expired(lease)) 
#endif
                        	lease_time_align = lease->expires - time(0);
                	packet.yiaddr = lease->yiaddr;
        	/* Or the client has a requested ip */
        	} else if ((req = get_option(oldpacket, DHCP_REQUESTED_IP)) &&

			/* Don't look here (ugly hackish thing to do) */
			memcpy(&req_align, req, 4) && 

			/* and the ip is in the lease range */
//add by vincent 2010-12-23
//#if defined(AEI_VDSL_CUSTOMER_TELUS)
//			is_in_range(vendor, req_align) &&
//#else
			ntohl(req_align) >= ntohl(cur_iface->start) &&
			ntohl(req_align) <= ntohl(cur_iface->end) && 
//#endif

			/* and its not already taken/offered */
			((!(lease = find_lease_by_yiaddr(req_align)) ||

			/* or its taken, but expired */
			lease_expired(lease)))) {
				packet.yiaddr = req_align; 

		/* otherwise, find a free IP */
        	} else {
                	packet.yiaddr = find_address(0);
                
                	/* try for an expired lease */
			if (!packet.yiaddr) packet.yiaddr = find_address(1);
        	}
        
        	if(!packet.yiaddr) {
                	LOG(LOG_WARNING, "no IP addresses to give -- "
				"OFFER abandoned");
                	return -1;
        	}
        
        	if (!add_lease(packet.chaddr, packet.yiaddr,
			server_config.offer_time)) {
                	LOG(LOG_WARNING, "lease pool is full -- "
				"OFFER abandoned");
                	return -1;
        	}               

        	if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
                	memcpy(&lease_time_align, lease_time, 4);
                	lease_time_align = ntohl(lease_time_align);
                	if (lease_time_align > cur_iface->lease) 
                        	lease_time_align = cur_iface->lease;
        	}

        	/* Make sure we aren't just using the lease time from the
		 * previous offer */
        	if (lease_time_align < server_config.min_lease) 
                	lease_time_align = cur_iface->lease;

	} else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}

#ifdef AEI_VDSL_CUSTOMER_TELUS
	mark_stb(NULL, packet.yiaddr);
#endif
                
        add_simple_option(packet.options, DHCP_LEASE_TIME, lease_time_align);

        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
		{	
#ifdef AEI_VDSL_CUSTOMER_QWEST
			if (curr->data[OPT_CODE] == DHCP_DNS_SERVER)
				custom_dns_option(&packet, curr);
#endif

#ifdef AEI_VDSL_TR098_TELUS
			if (curr->data[OPT_CODE] == DHCP_BOOT_FILE)
			{			
					int i, len;
					char vendorid[256];
					char *vid;
					char *param_list;
					UBOOL8 isStb = FALSE;
					UBOOL8 sendOpt67 = FALSE;
						
					memset(vendorid, 0, sizeof(vendorid));
						
					vid = get_option(oldpacket, DHCP_VENDOR);
					
					if (vid != NULL)
					{
						len = *(vid - 1);
						memcpy(vendorid, vid, (size_t)len);

						for (i = 0; i < VENDOR_CLASS_ID_TAB_SIZE; i++)
						{						
							if (cur_iface->opt67WarrantVids[i] 
#if defined(FUNCTION_ACTIONTEC_QOS)
                                && !wstrcmp(cur_iface->opt67WarrantVids[i], vendorid)
#endif
                                )
								{
									isStb = TRUE;
									break;
								}
						}
					}

					if (isStb)
					{
						param_list = get_option(oldpacket, DHCP_PARAM_REQ);
						if (param_list)
						{
							len = *(param_list - 1);
							for (i = 0; i < len; i++)
							{
								if (*(param_list + i) == DHCP_BOOT_FILE)
								{
									sendOpt67 = TRUE;
									break;
								}
							}
						}
					}

					if (sendOpt67)
						add_option_string(packet.options, curr->data);
						
					curr = curr->next;
					continue;
			}
#endif

            add_option_string(packet.options, curr->data);
		}

            curr = curr->next;
        }
		

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPDISCOVER from client has device identity, send back gateway identity */
        if ((req = get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY,VIinfo) != -1)
            add_option_string(packet.options,VIinfo);
        }
        //brcm end

        addr.s_addr = packet.yiaddr;
        LOG(LOG_INFO, "sending OFFER of %s", inet_ntoa(addr));
        return send_packet(&packet, 0);
}


int sendNAK(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;

        init_packet(&packet, oldpacket, DHCPNAK);
        
        DEBUG(LOG_INFO, "sending NAK");
        return send_packet(&packet, 1);
}


int sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr)
{
        struct dhcpMessage packet;
        struct option_set *curr;
        struct dhcpOfferedAddr *offerlist;
        char *lease_time, *vendorid, *userclsid;
        char length = 0;
        u_int32_t lease_time_align = cur_iface->lease;
        struct in_addr addr;
        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        char *req;
        int saveVIoptionNeeded = 0;
        //brcm end

        init_packet(&packet, oldpacket, DHCPACK);
        packet.yiaddr = yiaddr;
        
        if ((lease_time = get_option(oldpacket, DHCP_LEASE_TIME))) {
                memcpy(&lease_time_align, lease_time, 4);
                lease_time_align = ntohl(lease_time_align);
                if (lease_time_align > cur_iface->lease) 
                        lease_time_align = cur_iface->lease;
                else if (lease_time_align < server_config.min_lease) 
                        lease_time_align = cur_iface->lease;
        }
        
        add_simple_option(packet.options, DHCP_LEASE_TIME, lease_time_align);
        
        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
		{
#ifdef AEI_VDSL_CUSTOMER_QWEST
			if (curr->data[OPT_CODE] == DHCP_DNS_SERVER)
				custom_dns_option(&packet, curr);
#endif

#ifdef AEI_VDSL_TR098_TELUS			
			if (curr->data[OPT_CODE] == DHCP_BOOT_FILE)
			{
				if (is_IPTVSTB(oldpacket) && wantOption(oldpacket,DHCP_BOOT_FILE))
				{
					add_option_string(packet.options, curr->data);
				}

				curr = curr->next;
				continue;
			}
#endif

			add_option_string(packet.options, curr->data);
		}

                curr = curr->next;
        }

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPRequest from client has device identity, send back gateway identity,
           and save the device identify */
        if ((req = get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY,VIinfo) != -1)
          {
            add_option_string(packet.options,VIinfo);
          }
          saveVIoptionNeeded = 1;
        }
        //brcm end

        addr.s_addr = packet.yiaddr;
        LOG(LOG_INFO, "sending ACK to %s", inet_ntoa(addr));

        if (send_packet(&packet, 0) < 0) 
                return -1;

        add_lease(packet.chaddr, packet.yiaddr, lease_time_align);
        offerlist = find_lease_by_chaddr(packet.chaddr);
        if (saveVIoptionNeeded)
        {
           saveVIoption(req,offerlist);
        }
        vendorid = get_option(oldpacket, DHCP_VENDOR);
        userclsid = get_option(oldpacket, DHCP_USER_CLASS_ID);
        memset(offerlist->classid, 0, sizeof(offerlist->classid));
        memset(offerlist->vendorid, 0, sizeof(offerlist->vendorid));
        if( vendorid != NULL){
 	     length = *(vendorid - 1);
	     memcpy(offerlist->vendorid, vendorid, (size_t)length);
	     offerlist->vendorid[length] = '\0';
        }

        if( userclsid != NULL){
 	     length = *(userclsid - 1);
	     memcpy(offerlist->classid, userclsid, (size_t)length);
	     offerlist->classid[length] = '\0';
        }

#ifdef AEI_VDSL_CUSTOMER_TELUS
	if (is_stb(offerlist->vendorid))
		offerlist->is_stb = TRUE;

	mark_stb(offerlist->vendorid, packet.yiaddr);
#endif

#if defined (AEI_VDSL_TR098_TELUS)
       getClientIDOption(oldpacket, offerlist); 
#endif

        return 0;
}


int send_inform(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;
        struct option_set *curr;
        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        char *req;
        //brcm end


        init_packet(&packet, oldpacket, DHCPACK);
        
        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
		{
#ifdef AEI_VDSL_CUSTOMER_QWEST
			if (curr->data[OPT_CODE] == DHCP_DNS_SERVER)
				custom_dns_option(&packet, curr);
#endif

#ifdef AEI_VDSL_TR098_TELUS                     
			if (curr->data[OPT_CODE] == DHCP_BOOT_FILE)
			{
				if (is_IPTVSTB(oldpacket) && wantOption(oldpacket,DHCP_BOOT_FILE))
				{
					add_option_string(packet.options, curr->data);
				}

				curr = curr->next;
				continue;
			}
#endif

			add_option_string(packet.options, curr->data);
		}

                curr = curr->next;
        }

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPRequest from client has device identity, send back gateway identity,
           and save the device identify */
        if ((req = get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY,VIinfo) != -1)
            add_option_string(packet.options,VIinfo);
        }
        //brcm end

        return send_packet(&packet, 0);
}
