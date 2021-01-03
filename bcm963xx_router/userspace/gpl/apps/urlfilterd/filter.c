#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <syslog.h>
#include "filter.h"

#define BUFSIZE 2048

typedef enum
{
	PKT_ACCEPT,
	PKT_DROP,
#if defined(ACTION_TEC_WBA)
	PKT_REDIRECT
#endif
}pkt_decision_enum;

struct nfq_handle *h;
struct nfq_q_handle *qh;
char listtype[8];

void add_entry(char *website, char *folder)
{
	PURL new_entry, current, prev;
	new_entry = (PURL) malloc (sizeof(URL));
	strcpy(new_entry->website, website);
	strcpy(new_entry->folder, folder);
	new_entry->next = NULL;

	if (purl == NULL)
	{
		purl = new_entry;
	}
	else 
	{
		current = purl;
		while (current) 
		{
			prev = current;
			current = current->next;
		}
		prev->next = new_entry;
	}
}

int get_url_info()
{
	char temp[MAX_WEB_LEN + MAX_FOLDER_LEN], *temp1, *temp2, web[MAX_WEB_LEN], folder[MAX_FOLDER_LEN];
			
	FILE *f = fopen("/var/url_list", "r");
	while (fgets(temp,96, f) != '\0')
	{
		if (temp[0]=='h' && temp[1]=='t' && temp[2]=='t' && 
			temp[3]=='p' && temp[4]==':' && temp[5]=='/' && temp[6]=='/')
		{
			temp1 = temp + 7;	
		}
		else
		{
			temp1 = temp;	
		}

		if ((*temp1=='w') && (*(temp1+1)=='w') && (*(temp1+2)=='w') && (*(temp1+3)=='.'))
		{
			temp1 = temp1 + 4;
		}

		if ((temp2 = strchr(temp1, '\n')))
		{
			*temp2 = '\0';
		}
		       
		sscanf(temp1, "%[^/]", web);		
		temp1 = strchr(temp1, '/');
		if (temp1 == NULL)
		{
			strcpy(folder, "\0");
		}
		else
		{
			strcpy(folder, ++temp1);		
		}
		add_entry(web, folder);
		list_count ++;
	}

	fclose(f);

	return 0;
}

/* disable belowing codes since we will not write log in this module */
#undef CUSTOMER_ACTIONTEC
#ifdef CUSTOMER_ACTIONTEC	
#include <time.h>
#include <unistd.h>
#define URL_COUNT 100
#define IPQ_TIMEOUT 10000000
#define LOG_TIMEOUT IPQ_TIMEOUT/1000000
#define ENTRY_SIZE 256
#define BUFSIZE 2048
#define URL_COUNT 100
#define IPQ_TIMEOUT 10000000
#define LOG_TIMEOUT IPQ_TIMEOUT/1000000
#define ENTRY_SIZE 256

int url_count = -1;
int log_count=0;
char circularLog[URL_COUNT][ENTRY_SIZE];
time_t before;
int logNeedChange = 0;
static int timeIsUp(time_t time)
{
    if ((time - before) > LOG_TIMEOUT)
    {
       before=time;
       return 1;
    }
    else
       return 0;
}

static void writeLog()
{
    FILE *webLog;
    int i = 0;

    if (!logNeedChange || (url_count == -1))
        return;
    webLog = fopen("/var/webActivityLog", "w");

    if (!webLog)
    {
         fprintf(stderr, "/var/webActivityLog is created now.\n");
         return;
    }

    for (i=0;i<log_count;i++)
    {
        if ((url_count-i)>=0)
            fputs(circularLog[url_count-i], webLog);
        else
            fputs(circularLog[URL_COUNT + (url_count-i)], webLog);
    }
    fclose(webLog);
    logNeedChange=0;
}
#endif

#if defined(ACTION_TEC_WBA)
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
struct psuedohdr  {
  struct in_addr source_address;
  struct in_addr dest_address;
  unsigned char place_holder;
  unsigned char protocol;
  unsigned short length;
} psuedohdr;

static int sockfd;
static char src_mac[ETH_ALEN];
static unsigned int dest_mac[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static unsigned short in_cksum (unsigned short *ptr, int nbytes)
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

static int initSocketAddress(struct sockaddr_ll* socket_address, char *ifName)
{
    struct ifreq ifr;
    int i;

   /* RAW communication */
    socket_address->sll_family = AF_PACKET; /* TX */
    socket_address->sll_protocol = 0; /* BIND */ /* FIXME: htons(ETH_P_ALL) ??? */

    memset(&ifr, 0x00, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));
    if(ioctl(sockfd, SIOCGIFINDEX, &ifr) < 0)
    {
        printf("Fail to get ifindex\n");
        return -1;
    }
    socket_address->sll_ifindex = ifr.ifr_ifindex; /* BIND, TX */

    socket_address->sll_hatype = 0; /* RX */
    socket_address->sll_pkttype = PACKET_OTHERHOST; /* RX */

    socket_address->sll_halen = ETH_ALEN; /* TX */

    /* MAC */
    for(i = 0; i < ETH_ALEN; i++)
    {
        socket_address->sll_addr[i] = dest_mac[i]; /* TX */
    }
    socket_address->sll_addr[6] = 0x00;/*not used*/
    socket_address->sll_addr[7] = 0x00;/*not used*/

    /* Get source MAC of the Interface we want to bind to */
    memset(&ifr, 0x00, sizeof(ifr));
    strncpy(ifr.ifr_name, ifName, sizeof(ifr.ifr_name));
    if(ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        printf("Fail to get hw addr\n");
        return -1;
    }

    for(i = 0; i < ETH_ALEN; i++)
    {
        src_mac[i] = (unsigned char)ifr.ifr_hwaddr.sa_data[i];
    }

    //printf("Binding to %s: ifindex <%d>, protocol <0x%04X>...\n",
    //       ifName, socket_address->sll_ifindex, socket_address->sll_protocol);

    /* Bind to Interface */
    if(bind(sockfd, (struct sockaddr*)socket_address, sizeof(struct sockaddr_ll)) < 0)
    {
        printf("Binding error\n");
        return -1;
    }

    //printf("Done!\n");

    return 0;
}
static int SendPacketWithDestMac(char *data, int len, unsigned int *dmac)
{
	int ret;
	char *ifName = "br0";
	struct sockaddr_ll socket_address;
	
        memcpy(dest_mac, dmac, sizeof(dest_mac));
	//printf("\n finished reading mac address from arp table \n");
	//printf("\n the mac address is : %02x:%02x:%02x:%02x:%02x:%02x\n", dest_mac[0],dest_mac[1],dest_mac[2],dest_mac[3],dest_mac[4],dest_mac[5]);
	struct ethhdr raw_eth_hdr;
	char tx_buf[1512];
	
	if((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
	{
		printf("ERROR: Could not open Raw Socket");
		return -1;
	}
	//printf("\n finished socket cration \n");
	ret = initSocketAddress(&socket_address, ifName);
	//printf("\n finished initSocketAddress  \n");
	memset(&raw_eth_hdr,0,sizeof(struct ethhdr));
	memset(tx_buf,0,sizeof(tx_buf));

	int i;
	for (i=0; i<6; i++)
	{
		raw_eth_hdr.h_dest[i] = (u_char)dest_mac[i];
		raw_eth_hdr.h_source[i] = (u_char)src_mac[i];
	}
	raw_eth_hdr.h_proto = 0x0800;
	memcpy(tx_buf,&raw_eth_hdr,14);
	memcpy(tx_buf+sizeof(raw_eth_hdr),data,len);

	if ( sendto(sockfd, (void *)tx_buf, len+sizeof(raw_eth_hdr), 0, (struct sockaddr*)&socket_address, sizeof(socket_address)) < 0)
	{
		printf("\n sending data on Raw socket is failed \n");

	}
	//printf("\n great done with testing....   \n");
	close(sockfd);
	return 0;
}
static int send_redirect (struct nfq_q_handle *qh, int id, struct nfq_data * payload)
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
	char *capurl = "https://activate.verizon.net/launch/welcome";

	memset (buf_redirect, 0, sizeof (buf_redirect));

	data_len = nfq_get_payload(payload, &p_indata);
	if( data_len == -1 )
	{
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
		sprintf (buf_redirect, 
                         "HTTP/1.1 302 Moved Temporarily\r\n%s\r\n%s\r\n%s\r\n%s%s%s\r\n\r\n",
                         "Content-Length: 0",
                         "Pragma: no-cache",
                         "Cache-Control: private, max-age=0, no-cache",
                         "Location: ",
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
                
                SendPacketWithDestMac(p_outdata, data_len, dmac);
                
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
#if defined(ACTION_TEC_WBA)
static int pkt_decision(struct nfq_q_handle *qh, int id, struct nfq_data * payload)
#else
static int pkt_decision(struct nfq_data * payload)
#endif
{
	char *data;
	char *match, *folder, *url;
	PURL current;
	int payload_offset, data_len;
	struct iphdr *iph;
	struct tcphdr *tcp;
#ifdef CUSTOMER_ACTIONTEC	
	char ip_addr[16];
	char web_site[64];
	char *ps = NULL;
	char *pe = NULL;
	char currTime[64];
	struct tm *tmp;
	time_t now;
#define WEB_PORT 80
#endif /* CUSTOMER_ACTIONTEC */
#if defined(ACTION_TEC_WBA)
	int ret = PKT_ACCEPT;
#endif

	match = folder = url = NULL;

	data_len = nfq_get_payload(payload, &data);
	if( data_len == -1 )
	{
//printf("data_len == -1!!!!!!!!!!!!!!!, EXIT\n");
		exit(1);
	}
//printf("data_len=%d ", data_len);

	iph = (struct iphdr *)data;
	tcp = (struct tcphdr *)(data + (iph->ihl<<2));

	payload_offset = ((iph->ihl)<<2) + (tcp->doff<<2);
//   payload_len = (unsigned int)ntohs(iph->tot_len) - ((iph->ihl)<<2) - (tcp->doff<<2);
	match = (char *)(data + payload_offset);

	do{
	if(strstr(match, "GET ") == NULL && strstr(match, "POST ") == NULL && strstr(match, "HEAD ") == NULL)
	{
//printf("****NO HTTP INFORMATION!!!\n");
#if defined(ACTION_TEC_WBA)
		ret = PKT_ACCEPT;
		break;
#else
		return PKT_ACCEPT;
#endif
	}

#ifdef CUSTOMER_ACTIONTEC
	if(tcp->dest == htons (WEB_PORT))
	{
		ps = strstr(match, "Host: ");
		if(NULL != ps)
		{
			ps += 6;
			pe = strchr(ps, '\r');

			memset(web_site,0,sizeof(web_site));
			if((sizeof(web_site)-1) > (pe - ps))
			  strncpy(web_site, ps, (pe - ps));
			else
			  strncpy(web_site, ps, sizeof(web_site)-1);

			strncpy(ip_addr, inet_ntoa(*(struct in_addr *) &iph->saddr),sizeof(ip_addr)-1);

			now = time(NULL);
			tmp = localtime(&now);
			memset(currTime, 0, sizeof(currTime));
			strftime(currTime, sizeof(currTime), "%m/%d/%Y|%I:%M:%S%p|", tmp);

			if (log_count < URL_COUNT)
			  log_count++;

			if (++url_count == URL_COUNT )
			  url_count=0;

			snprintf(circularLog[url_count],ENTRY_SIZE - 1,"%s%s|%s|\n", currTime,ip_addr, web_site);
			logNeedChange=1;

			if (timeIsUp(now))
			{
				writeLog();
			}
		}
	}
#endif
#define CUSTOMER_ACTIONTEC

//printf("####payload = %s\n\n", match);
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
//printf("####This page is blocked by Exclude list!");
#if defined(ACTION_TEC_WBA)
					ret = PKT_DROP;
					break;
#else
					return PKT_DROP;
#endif
				}
				else 
				{
//printf("###Website hits but folder no hit in Exclude list! packets pass\n");
#if defined(ACTION_TEC_WBA)
					ret = PKT_ACCEPT;
					break;
#else
					return PKT_ACCEPT;
#endif

				}
			}
			else 
			{
				if ( (folder != NULL) || (current->folder[0] == '\0') )
				{
//printf("####This page is accepted by Include list!,line=%d",__LINE__);
#if defined(ACTION_TEC_WBA)
					ret = PKT_ACCEPT;
					break;
#else
					return PKT_ACCEPT;
#endif
				}
				else 
				{
//printf("####Website hits but folder no hit in Include list!, packets drop,line=%d\n",__LINE__);
#if defined(ACTION_TEC_WBA)
					ret = PKT_DROP;
					break;
#else
					return PKT_DROP;
#endif
				}
			}
		}
	}

	if (url == NULL) 
	{
		if (strcmp(listtype, "Exclude") == 0) 
		{
//printf("~~~~No Url hits!! This page is accepted by Exclude list! line=%d\n",__LINE__);
#if defined(ACTION_TEC_WBA)
			ret = PKT_ACCEPT;
			break;
#else
			return PKT_ACCEPT;
#endif
		}
		else 
		{
//printf("~~~~No Url hits!! This page is blocked by Include list! line=%d\n",__LINE__);
#if defined(ACTION_TEC_WBA)
			ret = PKT_DROP;
			break;
#else
			return PKT_DROP;
#endif
		}
	}
	}while(0);

//printf("~~~None of rules can be applied!! Traffic is allowed!!\n");
#if defined(ACTION_TEC_WBA)
//printf("~~~~ret=%d, line=%d\n",ret,__LINE__);
	if(ret == PKT_DROP){
		if ( (strcmp(listtype, "Include") == 0) && !strstr(match, "HEAD ") ){ /*WBA status is disable,urlfilter will get parameter 'Include'*/ 
		/*have not credentials yet,so redirect to verizon registration site*/
			send_redirect (qh, id, payload);
			return PKT_REDIRECT;
		}
		else /*should be normal web site block*/
			return ret;
	}
	return ret;
#else
	return PKT_ACCEPT;
#endif
}


/*
 * callback function for handling packets
 */
static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg,
	      struct nfq_data *nfa, void *data)
{
	struct nfqnl_msg_packet_hdr *ph;
	int decision, id=0;

	ph = nfq_get_msg_packet_hdr(nfa);
	if (ph)
	{
		id = ntohl(ph->packet_id);
	}

   /* check if we should block this packet */

#if defined(ACTION_TEC_WBA)
	decision = pkt_decision(qh, id, nfa);
#else
	decision = pkt_decision(nfa);
#endif
	if( decision == PKT_ACCEPT)
	{
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
	}
#if defined(ACTION_TEC_WBA)
	else if (decision == PKT_REDIRECT)
	{
		return 0;
	}
#endif
	else
	{
		return nfq_set_verdict(qh, id, NF_DROP, 0, NULL);
	}
}


/*
 * Open a netlink connection and returns file descriptor
 */
int netlink_open_connection(void *data)
{
	struct nfnl_handle *nh;

//printf("opening library handle\n");
	h = nfq_open();
	if (!h) 
	{
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

//printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET) < 0) 
	{
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		exit(1);
	}

//printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET) < 0) 
	{
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

//printf("binding this socket to queue '0'\n");
	qh = nfq_create_queue(h,  0, &cb, NULL);
	if (!qh) 
	{
		fprintf(stderr, "error during nfq_create_queue()\n");
		exit(1);
	}

//printf("setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) 
	{
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	nh = nfq_nfnlh(h);
	return nfnl_fd(nh);
}


int main(int argc, char **argv)
{
	int fd, rv;
	char buf[BUFSIZE]; 

	strcpy(listtype, argv[1]);
	get_url_info();
	memset(buf, 0, sizeof(buf));

	/* open a netlink connection to get packet from kernel */
	fd = netlink_open_connection(NULL);

	while ((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0) 
	{
//printf("pkt received\n");
		nfq_handle_packet(h, buf, rv);
		memset(buf, 0, sizeof(buf));
	}

//printf("unbinding from queue 0\n");
	nfq_destroy_queue(qh);
	nfq_close(h);

	return 0;
}
