#include <stdio.h>
#include<malloc.h>
#include<string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


static int sockfd;
static char src_mac[ETH_ALEN];
static unsigned int dest_mac[ETH_ALEN] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};


char *atomac(char *mac_str)
{
	//static char mac[6];
	unsigned int mac_tmp[6];
	int i;

	memset(mac_tmp,0,sizeof(mac_tmp));
	if (!mac_str)
		return NULL;
	if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &mac_tmp[0],
				&mac_tmp[1], &mac_tmp[2], &mac_tmp[3], &mac_tmp[4], &mac_tmp[5]) != 6)
	{
		return NULL;
	}

	for (i=0; i<6; i++)
		dest_mac[i] = (u_char)mac_tmp[i];
	return dest_mac;
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

static inline void dumpHexData(void *head, int len)
{
    int i;
    unsigned char *curPtr = (unsigned char*)head;

    printf("Address : 0x%08X", (unsigned int)curPtr);

    for (i = 0; i < len; ++i) {
        if (i % 4 == 0)
            printf(" ");       
        if (i % 16 == 0)
            printf("\n0x%04X:  ", i);
        printf("%02X ", *curPtr++);
    }

    printf("\n");
}

static int read_mac_from_arp(struct in_addr *device_ip)
{
	int fd;
	int res;
	char buf[1024];
	char mac_buf[20];
	char tmp_ip[16];
	int found=0;

        memset (mac_buf, 0, sizeof (mac_buf));
        memset (tmp_ip, 0, sizeof (tmp_ip));

		if(!device_ip) return -1;

	fd = open("/proc/net/arp", O_RDONLY);
	if (fd<0)   return 0;
	res = read(fd, buf, sizeof(buf)-1);
	close(fd);

	if (res <=0) return 0;	

	//char* pos=0;
	char* pos=NULL;
	char* ptr=buf;
	while ( ( (pos = strchr(ptr, '\n')) || (pos = strchr(ptr, '\r')) )
		&& (ptr-buf<res) ) 
	{
		char *ip,*hw_type, *hw_flags, *mac, *mask, *device;
		*pos = 0;
		ip  = strtok(ptr,  " \t\n");
		if (!strcmp (ip, "IP")) { //skip first  line
			ptr = pos + 1;
			continue;
		}
		
		hw_type = strtok(NULL,  " \t\n");
		hw_flags  = strtok(NULL,  " \t\n");
		mac = strtok(NULL,  " \t\n");
		mask = strtok(NULL,  " \t\n");
		device = strtok(NULL, " \t\n");

		if (!strcmp(device, "ewan0") || strstr(device, "atm") || strstr(device, "ptm") 
		    || !strcmp(device, "eth5") || !strcmp(device, "eth6") || !strcmp(device, "eth7") || !strcmp(device, "eth8"))
		    continue;
		
		printf("\n the ip = %s \n",inet_ntoa(*device_ip));
		if (!strncmp (ip,inet_ntoa(*device_ip),15))
		{
			printf("get ip for mac:%s\n",mac);
			found=1;
			atomac(mac);
			break;
		}

		ptr = pos+1;
	}

	return found;
}

int SendPacket(char *data, int len, struct in_addr *dest_ip_addr)
{
	int ret;
	char *ifName = "br0";
	struct sockaddr_ll socket_address;
	printf("destination address= %s and len= %d\n",inet_ntoa(*dest_ip_addr),len);
	read_mac_from_arp(dest_ip_addr);
	printf("\n finished reading mac address from arp table \n");
	printf("\n the mac address is : %02x:%02x:%02x:%02x:%02x:%02x\n", dest_mac[0],dest_mac[1],dest_mac[2],dest_mac[3],dest_mac[4],dest_mac[5]);
	struct ethhdr raw_eth_hdr;
	char tx_buf[1512];
	
	if((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1)
	{
		printf("ERROR: Could not open Raw Socket");
		return -1;
	}
	printf("\n finished socket cration \n");
	ret = initSocketAddress(&socket_address, ifName);
	printf("\n finished initSocketAddress  \n");
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
	printf("\n great done with testing....   \n");
	close(sockfd);
	return 0;
}

#if defined(SSID234_URL_REDIRECT)
int SendPacketWithDestMac(char *data, int len, unsigned int *dmac,char *brname)
#else
int SendPacketWithDestMac(char *data, int len, unsigned int *dmac)
#endif
{
	int ret;
#if defined(SSID234_URL_REDIRECT)
	char *ifName;
	if (!strcmp(brname,""))
		ifName = "br0";
	else
		ifName = brname;
#else
	char *ifName = "br0";
#endif
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
