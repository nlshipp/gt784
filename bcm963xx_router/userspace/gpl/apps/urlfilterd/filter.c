#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <syslog.h>
#include "filter.h"

#if defined (DMP_CAPTIVEPORTAL_1)
#if defined(CUSTOMER_NOT_USED_X) || defined(SUPPORT_GPL)
#include "cms.h"
#include "cms_util.h"
#include "cms_msg.h"
#endif
#endif

#if defined(SUPPORT_GPL)
#include <signal.h>
#endif


#ifdef CUSTOMER_NOT_USED_X
char allLanIpList[1024] = {0};
#endif
unsigned char listtype[8]={0};
PURL purl = NULL;
unsigned int list_count = 0;

#if defined(SUPPORT_GPL)
char RedirectBlockUrl[128] = "\0";
#endif

#if defined (DMP_CAPTIVEPORTAL_1)
void *msgHandle=NULL;  
int flagCaptiveURL = 0;
char captiveURL[BUFLEN_256] = {0};
char captiveIPAddr[32] = {0};
char captiveAllowList[1024] = {0};
#define capURLFile "/var/captive_url"
#define capAllowListFile  "/var/captive_allowlist"
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)
#define webActiveLogFile  "/var/webActiveLogFile"
#endif

#if defined(CUSTOMER_NOT_USED_X)
int flagOneTimeRedirect = 0;
char oneTimeRedirectURL[256] = {0};
char oneTimeRedirectIPAdress[32];
#define oneTimeCapURLFile "/var/oneTimeRedirectURL"



CmsRet send_msg_to_set_oneTimeRedirectURLFlag()
{
	CmsRet ret = CMSRET_SUCCESS;
	CmsMsgHeader send_Msg = EMPTY_MSG_HEADER;

	send_Msg.type = CMS_MSG_SET_ONE_TIME_REDIRECT_URL_FLAG;
	send_Msg.src = EID_URLFILTERD;
	send_Msg.dst = EID_SSK;

	if ((ret = cmsMsg_send(msgHandle, &send_Msg)) != CMSRET_SUCCESS)
	{

		printf("Failed to send message (ret=%d)", ret);
	}

	return ret;
}
#endif

void getCaptiveAllowList()
{
	FILE *fp = NULL;
	
	if ((fp = fopen(capAllowListFile, "r")) != NULL)
	{
		fgets (captiveAllowList, sizeof(captiveAllowList)-1, fp);
		if (strstr(captiveAllowList, "None"))
		{
			captiveAllowList[0] = '\0';
		}
		printf("captiveAllowList %s\n", captiveAllowList);
		fclose(fp);	
	}
}
void getCaptiveURLandIPAddr(char *fileName, char *url, char *ipAddr, int *flag)
{
	FILE *fp = NULL;
	char buf[256] = {0};
	char *pTemp = NULL;
	
	if ((fp=fopen(fileName, "r")) != NULL)
	{
#if defined(CUSTOMER_NOT_USED_X)
		fgets(buf,sizeof(buf),fp);
#else
		fscanf(fp, "%s", buf);
#endif
		printf("fileName %s buf %s\n", fileName, buf);
		if (strcmp(buf, "None") != 0)
		{
#if !defined(CUSTOMER_NOT_USED_X)		
			pTemp = strchr(buf, '_');
			*pTemp = ' ';
#endif			
			*flag = 1;
			sscanf(buf, "%s %s", url, ipAddr);
			printf("url %s ip %s\n", url, ipAddr);
		}
		fclose(fp);
	}
}
#endif



#define BUFSIZE 2048

typedef enum
{
	PKT_ACCEPT,
	PKT_DROP,
}pkt_decision_enum;

struct nfq_handle *h;
struct nfq_q_handle *qh;


#if defined(CUSTOMER_NOT_USED_X)
void add_entry(char *website, 
               char *folder,
               char *lanIP
               )
#else
void add_entry(char *website, char *folder)
#endif
{
   PURL new_entry, current, prev;

   new_entry = (PURL) malloc (sizeof(URL));

   strcpy(new_entry->website, website);
   strcpy(new_entry->folder, folder);

#if defined(CUSTOMER_NOT_USED_X)
   strncpy(new_entry->lanIP, lanIP, 15);
#endif

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

void get_url_info()
{
   char temp[MAX_WEB_LEN + MAX_FOLDER_LEN], *temp1, *temp2, web[MAX_WEB_LEN], folder[MAX_FOLDER_LEN];
			
   FILE *f = fopen("/var/url_list", "r");
   if (!f)
   {
       fprintf(stderr, "/var/url_list is not existed.\n");
       return;
   }

   while (fgets(temp,96, f) != '\0')
   {
#if defined(CUSTOMER_NOT_USED_X)
      char lanIP[16] = {0};
      char *pos = NULL;
	char *pe = NULL;
      if ( (pos = strchr (temp, ';')) != NULL) 
      {
          *pos++ = '\0';
                
          if ( *pos == '\0')
              strcpy(lanIP, "\0");
          else {
		pe = strrchr(pos, '\n');
		if(pe)
		 	*pe = '\0';
	   }
              strcpy(lanIP, pos);		
      }
      strcat(allLanIpList, lanIP);
      strcat(allLanIpList, " ");
#endif
      if (temp[0]=='h' && temp[1]=='t' && temp[2]=='t' && temp[3]=='p' && temp[4]==':' && temp[5]=='/' && temp[6]=='/')
      {
         temp1 = temp + 7;	
      }
      else
      {
         temp1 = temp;	
      }

      if (*temp1=='w' && *(temp1+1)=='w' && *(temp1+2)=='w' && *(temp1+3)=='.')
      {
         temp1 = temp1 + 4;
      }

      if (temp2 = strchr (temp1, '\n'))
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
#if defined(CUSTOMER_NOT_USED_X)
      add_entry(web, 
              folder,
              lanIP
              );
#else
      add_entry(web, folder);
#endif
      list_count ++;
   }

   fclose(f);

   return;
}

#if defined(SUPPORT_GPL)
time_t before;
int log_count = 0;
int url_count = -1;
int logNeedChange=0;
char circularLog[URL_COUNT][ENTRY_SIZE];
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)
int webActiveLogEnable =0;
#endif

int timeIsUp(time_t time)
{
    if ((time - before) > LOG_TIMEOUT)
    {
       before=time;
       return 1;
    }
    else
       return 0;
}
void readLog()
{
    FILE *webLog;
    int i = 0;
    char line[ENTRY_SIZE] = {0};
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)    
    if(webActiveLogEnable == 0)
        return;
#endif
    webLog = fopen("/var/webActivityLog", "r");
    if (!webLog)
    {
         fprintf(stderr, "can't read /var/webActivityLog\n");
         return;
    }
    while (fgets(line, ENTRY_SIZE, webLog))
    {
        log_count ++;
        url_count++;
        //printf("circularLog[%d]=%s\n",url_count,circularLog[url_count]);

    }
    if(log_count >URL_COUNT)
    {
        url_count = URL_COUNT -1;
        log_count = URL_COUNT;
    }
    fseek(webLog, 0, SEEK_SET);   
   // printf("log count=%d, url_count=%d\n",log_count,url_count);
    i=url_count;
    while (fgets(circularLog[i--], ENTRY_SIZE, webLog))
    {
    }    
#if 0    
    for(i=0; i< log_count; i++)
    {
        printf("circularLog[%d]=%s\n",i,circularLog[i]);
    }
#endif    
    fclose(webLog);
}

void writeLog()
{
    FILE *webLog;
    int i = 0;
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)    
    if(webActiveLogEnable == 0)
        return;
#endif
    if (!logNeedChange || (url_count == -1))
        return;
    webLog = fopen("/var/webActivityLog", "w");

    if (!webLog)
    {
         fprintf(stderr, "/var/webActivityLog is created now.\n");
         return;
    }
    //printf("log_count =%d, url_count=%d\n",log_count, url_count);
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
	return pkt_decision(qh, id, nfa);
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
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)

void getWebActiveInfo()
{
    int isFile=0;
    if((isFile = access(webActiveLogFile,F_OK)) == 0)
        webActiveLogEnable=1;
}
#endif
int main(int argc, char **argv)
{
	int fd, rv;
	char buf[BUFSIZE]; 

#if defined (DMP_CAPTIVEPORTAL_1)
	/*check if OneTimeRedircetURL is opened*/	
	FILE *fpRedirect = NULL;
	char cmd[BUFLEN_128];
	char *pTemp = NULL;
        char lan_ip[16] = "\0";
#endif


#if defined(SUPPORT_GPL)
	/*ignore SIGINT*/
	signal(SIGINT, SIG_IGN);

	if(argc<2) 
		strcpy(listtype,"Exclude");
	else
#endif
        strcpy(listtype, argv[1]);
	get_url_info();
	memset(buf, 0, sizeof(buf));


#if defined (DMP_CAPTIVEPORTAL_1)
	cmsMsg_init(EID_URLFILTERD, &msgHandle);
	cmsLog_init(EID_URLFILTERD);

	getCaptiveURLandIPAddr(capURLFile, captiveURL, captiveIPAddr, &flagCaptiveURL);
#if defined(CUSTOMER_NOT_USED_X)
	getCaptiveURLandIPAddr(oneTimeCapURLFile, oneTimeRedirectURL, oneTimeRedirectIPAdress, &flagOneTimeRedirect);
#endif        
        get_lan_ip(lan_ip);
        memset(RedirectBlockUrl, 0, sizeof(RedirectBlockUrl));
#if defined(CUSTOMER_NOT_USED_X)
        sprintf(RedirectBlockUrl, "%s", lan_ip);
#else
        sprintf(RedirectBlockUrl, "%s/captiveportal_pageblocked.html", lan_ip);
#endif
        //printf("redirect url%s\n", RedirectBlockUrl);
	getCaptiveAllowList();	
#endif
#if defined(AEI_VDSL_CUSTOMER_WEBACTIVELOG_SWITCH)
       getWebActiveInfo();
#endif
        readLog();
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
