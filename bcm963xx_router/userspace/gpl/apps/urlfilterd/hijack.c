#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/netfilter.h>
#include <syslog.h>
#include "filter.h"
#include <libnetfilter_queue/libnetfilter_queue.h>
#if defined(SUPPORT_GPL)
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#endif
 
extern int log_count;
extern int url_count;
extern char circularLog[URL_COUNT][ENTRY_SIZE];
extern time_t  before;
extern int logNeedChange;
#ifdef CUSTOMER_NOT_USED_X
extern char allLanIpList[1024];
#endif
extern unsigned char listtype[8];
extern PURL purl;
extern unsigned int list_count;
#define WEB_PORT 80


#if defined (SSID234_URL_REDIRECT)
char brname[16]={0};
#endif

#if defined (DMP_CAPTIVEPORTAL_1) || defined(SUPPORT_GPL)
static unsigned short
in_cksum (unsigned short *ptr, int nbytes)
{
  register long sum;		/* assumes long == 32 bits */
  u_short oddbyte;
  register u_short answer;	/* assumes u_short == 16 bits */
  /*
   * Our algorithm is simple, using a 32-bit accumulator (sum),
   * we add sequential 16-bit words to it, and at the end, fold back
   * all the carry bits from the top 16 bits into the lower 16 bits.
   */

  sum = 0;
  while (nbytes > 1) {
    sum += *ptr++;
    nbytes -= 2;
  }
  /* mop up an odd byte, if necessary */ if (nbytes == 1) {
    oddbyte = 0;		/* make sure top half is zero */
    *((u_char *) & oddbyte) = *(u_char *) ptr;	/* one byte only */
    sum += oddbyte;
  }

  /*
   * Add back carry outs from top 16 bits to low 16 bits.
   */

  sum = (sum >> 16) + (sum & 0xffff);	/* add high-16 to low-16 */
  sum += (sum >> 16);		/* add carry */
  answer = ~sum;		/* ones-complement, then truncate to 16 bits */
  return (answer);
}


#endif

#if defined(SUPPORT_GPL)

#define NIPQUID(addr)            \
    ((unsigned char *)&addr)[0], \
    ((unsigned char *)&addr)[1], \
    ((unsigned char *)&addr)[2], \
    ((unsigned char *)&addr)[3]


void get_lan_ip(char *addr)
{
	int fd;
	struct ifreq intf;
	
	if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("socket error!\n");
		return;
	}

	strcpy(intf.ifr_name, "br0");
	if(ioctl(fd, SIOCGIFADDR, &intf) != -1)
	{
		strcpy(addr, inet_ntoa(((struct sockaddr_in *)&intf.ifr_addr)->sin_addr));
	}
	close(fd);
	return;
}

#endif

#if defined(SUPPORT_GPL)
extern char RedirectBlockUrl[128];
#endif


#if defined (DMP_CAPTIVEPORTAL_1)
extern char captiveURL[256];
extern char captiveIPAddr[32];
extern int flagCaptiveURL;
extern char captiveAllowList[1024];
#if defined(CUSTOMER_NOT_USED_X)
extern int flagOneTimeRedirect;
extern char oneTimeRedirectIPAdress[32];
extern char oneTimeRedirectURL[256];
#endif


int checkCaptiveAllowList(char *allowList, __be32 ip)
{
	char *p, *pLast, *pTemp;
	char allowIpAddr[32] = {0};
	char allowbuf[1024] = {0};
	char buf[32] = {0};
	int maskNum = 0;
	struct in_addr allowIP;
	__be32 mask;
	__be32 ipNet;
	
	if (strlen(allowList) == 0)
	{
		return 0;
	}
	pLast = NULL;
	sprintf(allowbuf, "%s", allowList);
	p = strtok_r(allowbuf, ",", &pLast);
	while(p != NULL)
	{
		mask = 0xFFFFFFFF;
		sprintf(allowIpAddr, "%s", p);
		if ((pTemp = strchr(allowIpAddr, '/')) != NULL)
		{
			*pTemp = ' ';
			sscanf(allowIpAddr, "%s %d", buf, &maskNum);	
			inet_aton(allowIpAddr, &allowIP);	
			ipNet = htonl(ip);
			mask = mask<<(32-maskNum);
			if ((__be32)(allowIP.s_addr&mask) == (__be32)(ipNet&mask))
			{
				printf("find mask\n");
				return 1;
			}	
		}
		else
		{
			inet_aton(allowIpAddr, &allowIP);

			if (allowIP.s_addr == ip)
			{
				printf("find ip\n");
				return 1;
			}
		}
		p = strtok_r(NULL, ",", &pLast);
	}
	return 0;
}
#endif


#if defined(DMP_CAPTIVEPORTAL_1)
/* ~/work/pk5000/pk_new/Micro_cvs/rpm/BUILD/iptables-1.2.6a/ipq_act.c */
struct psuedohdr  {
  struct in_addr source_address;
  struct in_addr dest_address;
  unsigned char place_holder;
  unsigned char protocol;
  unsigned short length;
} psuedohdr;

unsigned short trans_check(unsigned char proto,
                           char *packet,
                           int length,
                           struct in_addr source_address,
                           struct in_addr dest_address)
{
  char *psuedo_packet;
  unsigned short answer;

  psuedohdr.protocol = proto;
  psuedohdr.length = htons(length);
  psuedohdr.place_holder = 0;

  psuedohdr.source_address = source_address;
  psuedohdr.dest_address = dest_address;

  if((psuedo_packet = malloc(sizeof(psuedohdr) + length)) == NULL)  {
    perror("malloc");
    exit(1);
  }

  memcpy(psuedo_packet,&psuedohdr,sizeof(psuedohdr));
  memcpy((psuedo_packet + sizeof(psuedohdr)),
         packet,length);

  answer = (unsigned short)in_cksum((unsigned short *)psuedo_packet,
                                    (length + sizeof(psuedohdr)));
  free(psuedo_packet);
  return answer;
}

static int send_redirect (struct nfq_q_handle *qh, int id, struct nfq_data * payload, char *capurl)
{
	struct iphdr *iph =  NULL;
        struct udphdr *udp = NULL;
        struct tcphdr *tcp = NULL;
        char *data = NULL;
        char *p_indata = NULL;
        char *p_outdata = NULL;
        struct in_addr saddr, daddr;
	char buf_redirect[1024];
        __u32	old_seq;
	unsigned short old_port;
        int data_len = 0;
	int org_len = 0;
	int redirect_flag = 0;
        int status= 0;
        int tcp_doff = 0;
        struct nfqnl_msg_packet_hw *hw = NULL;
        unsigned int dmac[6] = {0};
        int i = 0;
#if defined(CUSTOMER_NOT_USED_X)||defined(CUSTOMER_NOT_USED_X)
	char loc[20]= {0};
#endif
        memset (buf_redirect, 0, sizeof (buf_redirect));

	data_len = nfq_get_payload(payload, &p_indata);
	if( data_len == -1 )
	{
                //printf("data_len == -1!!!!!!!!!!!!!!!, EXIT\n");
		exit(1);
	}

        hw = nfq_get_packet_hw(payload);

        for (i = 0; i < 6; i++){
                dmac[i] = hw->hw_addr[i];
        }

	iph = (struct iphdr *)p_indata;
	tcp = (struct tcphdr *)(p_indata + (iph->ihl<<2));
        data = (char *)(p_indata + iph->ihl * 4 + tcp->doff * 4 );
        org_len = data_len - iph->ihl * 4 - tcp->doff * 4;
        
        //if (!schedule_access(hw_addr))
        {

#if defined(CUSTOMER_NOT_USED_X)||defined(CUSTOMER_NOT_USED_X)
		if(strstr(capurl,"http://") || strstr(capurl,"https://"))
			memcpy(loc,"Location: ", sizeof("Location: "));
		else
			memcpy(loc,"Location: http://", sizeof("Location: http://"));
#endif
		sprintf (buf_redirect, 
                         "HTTP/1.1 302 Moved Temporarily\r\n%s\r\n%s\r\n%s\r\n%s%s%s\r\n\r\n",
                         "Content-Length: 0",
                         "Pragma: no-cache",
                         "Cache-Control: private, max-age=0, no-cache",
#if defined(CUSTOMER_NOT_USED_X)|| defined(CUSTOMER_NOT_USED_X)
                         loc,
#else
			 "Location: http://",
#endif
                         capurl,
                         "");
                
                redirect_flag = 1; 
        }
        
        if (redirect_flag){
                p_outdata = p_indata;
                memset (p_outdata + iph->ihl * 4 + tcp->doff * 4, 0, org_len);
                
                tcp->doff = 5;
                tcp_doff = tcp->doff; 
                memcpy (p_outdata + iph->ihl * 4 + tcp->doff * 4, buf_redirect, strlen (buf_redirect));

                data_len = strlen (buf_redirect) + iph->ihl * 4 + tcp->doff * 4;

                iph = (struct iphdr *)p_outdata;
                tcp = (struct tcphdr *)(p_outdata + (iph->ihl<<2));
                data = (char *)(p_outdata + iph->ihl * 4 + tcp->doff * 4);
                
                saddr.s_addr = iph->saddr;
                daddr.s_addr = iph->daddr;
                iph->saddr = daddr.s_addr;
                iph->daddr = saddr.s_addr;
                
                //change ip header check sum
                iph->tot_len = htons (data_len);
                iph->check = 0;
                iph->check = in_cksum((unsigned short *)iph, iph->ihl * 4); 
                
                //change tcp checksum
                
                memset ((char *)tcp+12, 0, 2);
                tcp->res1 = 0;
                tcp->doff = tcp_doff; //set the header len(options may set it other than 5)
                tcp->psh = 1; 
                tcp->ack = 1;
                tcp->fin = 1;
                
                old_seq = tcp->seq;
                tcp->seq= ntohl (htonl (tcp->ack_seq));
                tcp->ack_seq = ntohl (htonl (old_seq) + org_len );
                
                old_port = tcp->dest;
                tcp->dest = tcp->source;
                tcp->source = old_port;
                
                tcp->check = 0;
                tcp->check = trans_check(IPPROTO_TCP, (char *)tcp, data_len - sizeof(struct iphdr), daddr, saddr);

#if defined(SSID234_URL_REDIRECT)                
                SendPacketWithDestMac(p_outdata, data_len, dmac,brname);
#else
                SendPacketWithDestMac(p_outdata, data_len, dmac);
#endif
                
                status = nfq_set_verdict(qh, id, NF_DROP, data_len, p_outdata);
                
                if (status < 0)
                        printf("send redirect error\n");
                
                return status;
        }       
        
        status = nfq_set_verdict(qh, id, NF_ACCEPT, data_len, p_indata);
        if (status < 0)
                printf("send redirect error\n");
        
	return status;
}
#endif

#if defined (SSID234_URL_REDIRECT)
void urltrim(char *url_redirect)
{
    int size = 0;

    if (!url_redirect)
        return;
    size = strlen(url_redirect);
    if(!size)
        return;

    while(1)
    {
        size = strlen(url_redirect);
        if (!size)
            break;
        if(url_redirect[size-1]!='/')
            break;
        else
            url_redirect[size-1]='\0';
    }
}
#endif

int pkt_decision(struct nfq_q_handle *qh, int id, struct nfq_data * payload)
{
	char *data;
	char *match, *folder, *url;
	PURL current;
	int payload_offset, data_len;
	struct iphdr *iph;
	struct tcphdr *tcp;
	int status;


	match = folder = url = NULL;

	data_len = nfq_get_payload(payload, &data);
	if( data_len == -1 ){
                printf("data_len == -1!!!!!!!!!!!!!!!, EXIT\n");
		exit(1);
	}
        
        iph = (struct iphdr *)data;
	tcp = (struct tcphdr *)(data + (iph->ihl<<2));
        
	payload_offset = ((iph->ihl)<<2) + (tcp->doff<<2);
        //   payload_len = (unsigned int)ntohs(iph->tot_len) - ((iph->ihl)<<2) - (tcp->doff<<2);
	match = (char *)(data + payload_offset);
        
        if (tcp->dest == htons(WEB_PORT)){
                if(strstr(match, "GET ") == NULL && strstr(match, "POST ") == NULL && strstr(match, "HEAD ") == NULL){
                        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
                }
        }

#if defined (SSID234_URL_REDIRECT)
	char mac_str[128]={0};
	char url_redirect[256]={0};
        char *pUrl = NULL;
	struct nfqnl_msg_packet_hw *hw = NULL;
	hw = nfq_get_packet_hw(payload);
        if ( tcp->dest == htons (WEB_PORT) && (strstr(match,"GET / HTTP/")||
                                               strstr(match,"get / HTTP/")||
                                               strstr(match,"GET /"))||
                                               strstr(match,"get /") ) 
        {
	    sprintf(mac_str,"%02x:%02x:%02x:%02x:%02x:%02x",hw->hw_addr[0],hw->hw_addr[1],hw->hw_addr[2],
							hw->hw_addr[3],hw->hw_addr[4],hw->hw_addr[5]);
   	    processSSID234UrlRedirect(mac_str,url_redirect,sizeof(url_redirect)-1,brname);
	    if (strcmp(url_redirect,"")){
                urltrim(url_redirect);
                pUrl = strstr(url_redirect,"http://");
                if(pUrl){
                    pUrl = pUrl + 7;
                    if  (strstr(match, pUrl) == NULL) {
                  	 status = send_redirect (qh, id, payload, url_redirect);
                         return status;
                    }
                }else{
                    if  (strstr(match, url_redirect) == NULL) {
                        status = send_redirect (qh, id, payload, url_redirect);
                        return status;
                    }
                }
            }
            
	}
#endif


#if defined (DMP_CAPTIVEPORTAL_1)
#if defined(CUSTOMER_NOT_USED_X)
	if ((flagOneTimeRedirect == 1) && (tcp->dest == htons (WEB_PORT)))
        {
                if(strstr(match, "GET / HTTP/") || strstr(match, "get HTTP/")) 
		{        
			flagOneTimeRedirect = 2;
			send_msg_to_set_oneTimeRedirectURLFlag();
			if ((strstr(match, oneTimeRedirectURL) == NULL) && (strstr(oneTimeRedirectIPAdress, inet_ntoa(*(struct in_addr *) &iph->daddr)) == NULL))
			{
                                status = send_redirect (qh, id, payload, oneTimeRedirectURL);
				return status;
			}
		}	
	}
	else if ((flagOneTimeRedirect == 2) && (tcp->dest == htons (WEB_PORT)))
	{               
		if ((strstr(match, "GET / HTTP/") || strstr(match, "get / HTTP/")))	
		{           
			if ((strstr(match, oneTimeRedirectURL) == NULL) && (strstr(oneTimeRedirectIPAdress, inet_ntoa(*(struct in_addr *) &iph->daddr)) == NULL))
			{              
				flagOneTimeRedirect = 0;
				if ((flagCaptiveURL == 1) && (!checkCaptiveAllowList(captiveAllowList, iph->daddr)))
				{            
					if ((strstr(match, captiveURL) == NULL) && (strstr(captiveIPAddr, inet_ntoa(*(struct in_addr *) &iph->daddr)) == NULL))
					{    
                                                status = send_redirect (qh, id, payload, captiveURL);
						return status;
					}
				}
			}
		}	
	}
	else	
#endif
	{              
		if ((flagCaptiveURL == 1) && (tcp->dest == htons (WEB_PORT)) && (!checkCaptiveAllowList(captiveAllowList, iph->daddr)))
		{               
			if(strstr(match, "GET /") || strstr(match, "get /")) 
			{        
				if ((strstr(match, captiveURL) == NULL) && (strstr(captiveIPAddr, inet_ntoa(*(struct in_addr *) &iph->daddr)) == NULL))
				{
                                        status = send_redirect (qh, id, payload, captiveURL);
					flagCaptiveURL = 1;
					return status;
				}
			}
		}
		else if ((flagCaptiveURL == 2) && (tcp->dest == htons (WEB_PORT)) && (!checkCaptiveAllowList(captiveAllowList, iph->daddr)))
                {
			if(strstr(match, "GET / HTTP/") || strstr(match, "get / HTTP/")) 
			{
				if ((strstr(match, captiveURL) == NULL) && (strstr(captiveIPAddr, inet_ntoa(*(struct in_addr *) &iph->daddr)) == NULL))
				{
                                        status = send_redirect (qh, id, payload, captiveURL);
					flagCaptiveURL = 1;
					return status;
				}
			}
		}

	}	
#endif


#if defined(SUPPORT_GPL)
		time_t now;
		struct tm *tmp;
		char currTime[64];
		char ip_addr[64]="";
		char web_site[64]="";
		char *temp_start = NULL;
		char *temp_end = NULL;
		char *cpy_start = NULL;
		char *cpy_end = NULL;
		ip_addr[0]=web_site[0]=currTime[0]='\0';

		temp_start = strstr(match, "Host: ");
		if(!temp_start)
		{
		    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		}

		cpy_start = temp_start + 6;
                temp_end = strchr(cpy_start, '\n');
                if(!temp_end) 
		    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

		cpy_end = temp_end - 1;
                if(cpy_end-cpy_start <= 0 )
		    return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

		memset(web_site,0,sizeof(web_site));

                if ((sizeof(web_site)-1) > (cpy_end-cpy_start))
                    strncpy(web_site, cpy_start, (cpy_end-cpy_start));
                else 
                    strncpy(web_site, cpy_start,sizeof(web_site)-1);

		strncpy(ip_addr, inet_ntoa(*(struct in_addr *) &iph->saddr),sizeof(ip_addr)-1);

		now = time(NULL);
		tmp = localtime(&now);
		memset(currTime, 0, sizeof(currTime));
		strftime(currTime, sizeof(currTime), "%m/%d/%Y|%I:%M:%S:%p|", tmp);

		if(log_count < URL_COUNT)
		    log_count++;
		if(++url_count == URL_COUNT)
		    url_count=0;

		snprintf(circularLog[url_count], ENTRY_SIZE-1, "%s%s|%s|\n", currTime, ip_addr, web_site);
		logNeedChange = 1;
		if(timeIsUp(now))
		{
		    writeLog();
		}
#endif
              
        for (current = purl; current != NULL; current = current->next)
	{               
		if (current->folder[0] != '\0')
		{               
			folder = strstr(match, current->folder);
		}

		if ( (url = strstr(match, current->website)) != NULL ) 
		{               
			if (strcmp(listtype, "Exclude") == 0) 
			{             
				if ( (folder != NULL) || (current->folder[0] == '\0') )
                                {               
#if defined(CUSTOMER_NOT_USED_X)
                                        if (strstr(current->lanIP, "all") != NULL){ //block all PCs
                                                //printf("All####This page is blocked by Exclude list!, into send_redirect\n");
                                                return send_redirect(qh, id, payload, RedirectBlockUrl);
                                        } else {//block specific PCs
                                                struct in_addr lanIP;
                                                inet_aton(current->lanIP, &lanIP);
                                                if (lanIP.s_addr == iph->saddr){
                                                        //printf("IP####This page is blocked by Exclude list!, into send_redirect\n");
                                                        return send_redirect(qh, id, payload, RedirectBlockUrl);
                                                }
                                        }
                                        //printf("All####This page is blocked by Exclude list!, into DROPPP\n");
#elif defined(CUSTOMER_NOT_USED_X)
                                        return send_redirect(qh, id, payload, RedirectBlockUrl);
#else
                                        return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
#endif
                                }
				else 
                                {
                                        //printf("###Website hits but folder no hit in Exclude list! packets pass\n");
                                        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
				}
			}
			else 
			{
				if ( (folder != NULL) || (current->folder[0] == '\0') )
				{
                                        //printf("####This page is accepted by Include list!");
					return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
				}
				else 
				{
                                        //printf("####Website hits but folder no hit in Include list!, packets drop\n");
					return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
				}
			}
		}
	}

	if (url == NULL) 
	{
		if (strcmp(listtype, "Exclude") == 0) 
		{
                        //printf("~~~~No Url hits!! This page is accepted by Exclude list!\n");
			return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		}
		else 
		{
                        //printf("~~~~No Url hits!! This page is blocked by Include list!\n");
			return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
		}
	}

        //printf("~~~None of rules can be applied!! Traffic is allowed!!\n");
	return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

}

