/* script.c
 *
 * Functions to call the DHCP client notification scripts 
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

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "options.h"
#include "dhcpd.h"
#include "dhcpc.h"
#include "packet.h"
#include "options.h"
#include "debug.h"
#ifdef AEI_CONTROL_LAYER
#include "ctl_log.h"
#endif

// brcm
static char local_ip[32]="";
static char router_ip[32]="";
static char dns_ip[128]="";
static char subnet_ip[32]="";

#ifdef AEI_SUPPORT_6RD
// 6rd option
static int n_ipv6rd_ipv4_mask_len = 0;
static char s_ipv6rd_bripv4_addr[32] = "";
static int n_ipv6rd_prefix_len = 32;
static char s_ipv6rd_prefix[128] = "";
#endif


#include "cms.h"
extern void sendEventMessage(UBOOL8 assigned, const char *ip, const char *mask, const char *gateway, const char *nameserver);

/* get a rough idea of how long an option will be (rounding up...) */
static int max_option_length(char *option, struct dhcp_option *type)
{
	int size = 0;
	
	switch (type->flags & TYPE_MASK) {
	case OPTION_IP:
	case OPTION_IP_PAIR:
		size = (option[OPT_LEN - 2] / 4) * sizeof("255.255.255.255 ");
		break;
	case OPTION_STRING:
		size = option[OPT_LEN - 2] + 1;
		break;
	case OPTION_BOOLEAN:
		size = option[OPT_LEN - 2] * sizeof("yes ");
		break;
	case OPTION_U8:
		size = option[OPT_LEN - 2] * sizeof("255 ");
		break;
	case OPTION_U16:
		size = (option[OPT_LEN - 2] / 2) * sizeof("65535 ");
		break;
	case OPTION_S16:
		size = (option[OPT_LEN - 2] / 2) * sizeof("-32768 ");
		break;
	case OPTION_U32:
		size = (option[OPT_LEN - 2] / 4) * sizeof("4294967295 ");
		break;
	case OPTION_S32:
		size = (option[OPT_LEN - 2] / 4) * sizeof("-2147483684 ");
		break;
	}
	
	return size;
}


/* Fill dest with the text of option 'option'. */
static void fill_options(char *dest, unsigned char *option, struct dhcp_option *type_p)
{
	int type, optlen;
	u_int16_t val_u16;
	int16_t val_s16;
	u_int32_t val_u32;
	int32_t val_s32;
	int len = option[OPT_LEN - 2];
        // brcm
	char tmp[128]="";

	dest += sprintf(dest, "%s=", type_p->name);

	type = type_p->flags & TYPE_MASK;
	optlen = option_lengths[type];
	for(;;) {
		switch (type) {
		case OPTION_IP:	/* Works regardless of host byte order. */
			dest += sprintf(dest, "%d.%d.%d.%d",
					option[0], option[1],
					option[2], option[3]);
		        // brcm
		        sprintf(tmp, "%d.%d.%d.%d",
				option[0], option[1],
				option[2], option[3]);
			if (!strcmp(type_p->name, "dns")) {
			    // cwu
             if (strlen(dns_ip) > 0)
			    {
			        strcat(dns_ip, ",");
             }
             strcat(dns_ip, tmp);
			}
			if (!strcmp(type_p->name, "router"))
			    strcpy(router_ip, tmp);
			if (!strcmp(type_p->name, "subnet"))
			    strcpy(subnet_ip, tmp);
 			break;
		case OPTION_IP_PAIR:
			dest += sprintf(dest, "%d.%d.%d.%d, %d.%d.%d.%d",
					option[0], option[1],
					option[2], option[3],
					option[4], option[5],
					option[6], option[7]);
			break;
		case OPTION_BOOLEAN:
			dest += sprintf(dest, *option ? "yes" : "no");
			break;
		case OPTION_U8:
			dest += sprintf(dest, "%u", *option);
			break;
		case OPTION_U16:
			memcpy(&val_u16, option, 2);
			dest += sprintf(dest, "%u", ntohs(val_u16));
			break;
		case OPTION_S16:
			memcpy(&val_s16, option, 2);
			dest += sprintf(dest, "%d", ntohs(val_s16));
			break;
		case OPTION_U32:
			memcpy(&val_u32, option, 4);
			dest += sprintf(dest, "%lu", (unsigned long) ntohl(val_u32));
			break;
		case OPTION_S32:
			memcpy(&val_s32, option, 4);
			dest += sprintf(dest, "%ld", (long) ntohl(val_s32));
			break;
		case OPTION_STRING:
			memcpy(dest, option, len);
			dest[len] = '\0';
			return;	 /* Short circuit this case */
		}
		option += optlen;
		len -= optlen;
		if (len <= 0) break;
		*(dest++) = ' ';
	}
}


static char *find_env(const char *prefix, char *defaultstr)
{
	extern char **environ;
	char **ptr;
	const int len = strlen(prefix);

	for (ptr = environ; *ptr != NULL; ptr++) {
		if (strncmp(prefix, *ptr, len) == 0)
		return *ptr;
	}
	return defaultstr;
}


/* put all the paramaters into an environment */
static char **fill_envp(struct dhcpMessage *packet)
{
	int num_options = 0;
	int i, j;
	unsigned char *addr;
	char **envp;
	unsigned char *temp;
	char over = 0;

	if (packet == NULL)
		num_options = 0;
	else {
		for (i = 0; options[i].code; i++)
			if (get_option(packet, options[i].code))
				num_options++;
		if (packet->siaddr) num_options++;
		if ((temp = get_option(packet, DHCP_OPTION_OVER)))
			over = *temp;
		if (!(over & FILE_FIELD) && packet->file[0]) num_options++;
		if (!(over & SNAME_FIELD) && packet->sname[0]) num_options++;		
	}
	
	envp = malloc((num_options + 5) * sizeof(char *));
	envp[0] = malloc(strlen("interface=") + strlen(client_config.interface) + 1);
	sprintf(envp[0], "interface=%s", client_config.interface);
	envp[1] = malloc(sizeof("ip=255.255.255.255"));
	envp[2] = find_env("PATH", "PATH=/bin:/usr/bin:/sbin:/usr/sbin");
	envp[3] = find_env("HOME", "HOME=/");

	if (packet == NULL) {
		envp[4] = NULL;
		return envp;
	}

	addr = (unsigned char *) &packet->yiaddr;
	sprintf(envp[1], "ip=%d.%d.%d.%d",
		addr[0], addr[1], addr[2], addr[3]);
	// brcm
	sprintf(local_ip, "%d.%d.%d.%d",
		addr[0], addr[1], addr[2], addr[3]);
	strcpy(dns_ip, "");

#ifdef AEI_SUPPORT_6RD
	// cleanup
	n_ipv6rd_ipv4_mask_len = 0;
	memset( s_ipv6rd_bripv4_addr, 0, sizeof(s_ipv6rd_bripv4_addr));
	n_ipv6rd_prefix_len = 0; //32
	memset( s_ipv6rd_prefix, 0, sizeof(s_ipv6rd_prefix));
#endif
	for (i = 0, j = 4; options[i].code; i++) {
		if ((temp = get_option(packet, options[i].code))) {
#ifdef AEI_SUPPORT_6RD
			//ctllog_debug( "code=0X%02x,len=%d",temp[-2],temp[-1]);
			if(temp[OPT_CODE-2] == DHCP_6RD ) {
				char ipv6prefix[16]="";
				char ipv4BR[4]="";

				n_ipv6rd_ipv4_mask_len = temp[0];
				n_ipv6rd_prefix_len = temp[1];
				//ctllog_debug( "temp[0]=0X%02x\n",temp[0]);
				//ctllog_debug( "6rd_netmask_len=%d\n", n_ipv6rd_ipv4_mask_len);
				//ctllog_debug( "6rd_ipv6_prefix_len=%d\n", n_ipv6rd_prefix_len);
				memcpy( ipv6prefix, &temp[2], sizeof(ipv6prefix));
				memcpy( ipv4BR, &temp[18], sizeof(ipv4BR));
				if( inet_ntop( AF_INET6,
						(const void*)ipv6prefix,
						s_ipv6rd_prefix,
						sizeof(s_ipv6rd_prefix)))
				{
					//ctllog_debug( "6rd_ipv6_prefix=%s\n", s_ipv6rd_prefix );
				} else {
					ctllog_error( "Cann't translate binary(ipv6rd prefix) to asc\n");
				}

				if( inet_ntop( AF_INET,
						(const void*)ipv4BR,
						s_ipv6rd_bripv4_addr,
						sizeof(s_ipv6rd_bripv4_addr)))
				{
					//ctllog_debug("6rd_ipv4_br=%s\n",s_ipv6rd_bripv4_addr);
				} else {
					ctllog_error("Cann't translate binary(br ipv4 addr) to asc\n");
				}
			} else
#endif
			{
			envp[j] = malloc(max_option_length(temp, &options[i]) + 
					strlen(options[i].name) + 2);
			fill_options(envp[j], temp, &options[i]);
			j++;
		}
	}
	}
	if (packet->siaddr) {
		envp[j] = malloc(sizeof("siaddr=255.255.255.255"));
		addr = (unsigned char *) &packet->yiaddr;
		sprintf(envp[j++], "siaddr=%d.%d.%d.%d",
			addr[0], addr[1], addr[2], addr[3]);
	}
	if (!(over & FILE_FIELD) && packet->file[0]) {
		/* watch out for invalid packets */
		packet->file[sizeof(packet->file) - 1] = '\0';
		envp[j] = malloc(sizeof("boot_file=") + strlen(packet->file));
		sprintf(envp[j++], "boot_file=%s", packet->file);
	}
	if (!(over & SNAME_FIELD) && packet->sname[0]) {
		/* watch out for invalid packets */
		packet->sname[sizeof(packet->sname) - 1] = '\0';
		envp[j] = malloc(sizeof("sname=") + strlen(packet->sname));
		sprintf(envp[j++], "sname=%s", packet->sname);
	}	
	envp[j] = NULL;

	printf( "A: local ipv4 addr: %s\n", local_ip );
   sendEventMessage(TRUE, local_ip, subnet_ip, router_ip, dns_ip);
	printf( "B: local ipv4 addr: %s\n", local_ip );
#if defined(AEI_CONTROL_LAYER) && defined(AEI_SUPPORT_6RD)
	sendEventMessageWith6RD(TRUE, local_ip, subnet_ip, router_ip, dns_ip,
				n_ipv6rd_ipv4_mask_len,
				s_ipv6rd_bripv4_addr,
				n_ipv6rd_prefix_len,
				s_ipv6rd_prefix);
#endif

	return envp;
}


/* Call a script with a par file and env vars */
void run_script(struct dhcpMessage *packet, const char *name)
{
	char **envp;

   if (!strcmp(name, "bound"))
   {
      envp = fill_envp(packet);
      free(*envp);
   }

}
