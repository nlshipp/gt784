#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <syslog.h>

#include "dproxy.h"
#include "dns_decode.h"
//#include "cache.h"
#include "conf.h"
#include "dns_list.h"
#include "dns_construct.h"
#include "dns_io.h"
#include "dns_dyn_cache.h"
#include "dns_probe.h"
/*****************************************************************************/
/*****************************************************************************/
int dns_main_quit;
int dns_sock;
fd_set rfds;

//BRCM
int dns_querysock;
int dns_wanup = 0;
/* CMS message handle */
void *msgHandle=NULL;
int msg_fd;
/* write web log if enableWebLog = 1 */
int enableWebLog = 0 ;

#ifdef CUSTOMER_VERIZON
extern char primary_ns[CMS_IPADDR_LENGTH];
extern char secondary_ns[CMS_IPADDR_LENGTH];
static int sec_flag =0;
static int timeout_flag = 0;
void timeOutReq(dns_request_t * ptr);
void init_time();
void init_sigaction();
void check_dns_timeout();
void set_timeout_flag(int signo);

struct wba_url {
       char url[32];
       char ip[32];
};

#if ACTION_TEC_WBA_DPROXY
int wba_flag = 0;
#endif

#endif


#ifdef CUSTOMER_ATT_DIAGNOSTIC
int diagnostic_flag = 0;
#endif


#ifndef DNS_PROBE
extern time_t dns_recover_time;
#endif
extern void dns_probe_print(void);

#ifdef ACTION_TEC_DNS_CACHE
time_t syn_time=0;
int enable_dns_cache=0;
#endif

#ifdef ACTION_TEC_DNS_CACHE
static int dns_cache_enabled()
{
   time_t t=time(NULL);
   if (t-syn_time>3)
   {
	syn_time=t;
//   	struct stat stats;
    FILE* fp = NULL;
	if (NULL != (fp = fopen("/var/dns_cache", "r")))
  // 	if(stat("/var/dns_cache", &stats) == 0)
	  enable_dns_cache=1;
	else
	  enable_dns_cache=0;
   }
   return enable_dns_cache;
   
}
#endif

/*****************************************************************************/
int is_connected()
{
#if 0 //BRCM
  FILE *fp;
  if(!config.ppp_detect)return 1;

  fp = fopen( config.ppp_device_file, "r" );
  if(!fp)return 0;
  fclose(fp);
  return 1;
#endif
  //BRCM
  return dns_wanup;
}
/*****************************************************************************/
int dns_init()
{
  struct sockaddr_in sa;
  struct in_addr ip;
  int ret;

  /* Clear it out */
  memset((void *)&sa, 0, sizeof(sa));
    
  dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  /* Error */
  if( dns_sock < 0 ){
	 debug_perror("Could not create socket");
	 exit(1);
  } 

  ip.s_addr = INADDR_ANY;
  sa.sin_family = AF_INET;
  memcpy((void *)&sa.sin_addr, (void *)&ip, sizeof(struct in_addr));
  sa.sin_port = htons(PORT);
  
  /* bind() the socket to the interface */
  if (bind(dns_sock, (struct sockaddr *)&sa, sizeof(struct sockaddr)) < 0){
	 debug_perror("dns_init: bind: Could not bind to port");
	 exit(1);
  }


  // BRCM: use a different socket for queries so we can use ephemeral port
  // Our corp DNS server does not like queries with src port 53.
  dns_querysock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  /* Error */
  if( dns_querysock < 0 ){
	 debug_perror("Could not create query socket");
	 exit(1);
  } 

  ip.s_addr = INADDR_ANY;
  sa.sin_family = AF_INET;
  memcpy((void *)&sa.sin_addr, (void *)&ip, sizeof(struct in_addr));
  sa.sin_port = htons(0);
  
  /* bind() the socket to the interface */
  if (bind(dns_querysock, (struct sockaddr *)&sa, sizeof(struct sockaddr)) < 0){
	 debug_perror("dns_init: bind: Could not bind to ephmeral port");
	 exit(1);
  }


  // BRCM: Connect to ssk
  if ((ret = cmsMsg_init(EID_DNSPROXY, &msgHandle)) != CMSRET_SUCCESS) {
  	debug_perror("dns_init: cmsMsg_init() failed");
	exit(1);
  }
  cmsMsg_getEventHandle(msgHandle, &msg_fd);

  dns_main_quit = 0;

  FD_ZERO( &rfds );
  FD_SET( dns_sock, &rfds );
  FD_SET( dns_querysock, &rfds );
  FD_SET( msg_fd, &rfds );

#ifdef DNS_DYN_CACHE
  dns_dyn_hosts_add();
#endif

  //cache_purge( config.purge_time );

  //BCM
  dns_wanup = dns_probe_init();

  return 1;
}


void get_lanip (char *ip, int len)
{
  FILE *fp;
  char buffer[256] = {0};
  char host_name[128] = {0};
  char ip_address[32] = {0};
  int  i =0;

  fp = fopen("/var/hosts", "r");
  if (!fp)
     return;

  memset (buffer, 0, sizeof(buffer));
  while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) 
  {  
     i++;
     memset(ip_address, 0, sizeof(ip_address));
     memset(host_name, 0, sizeof(host_name));    
     sscanf(buffer, "%s %s", ip_address, host_name);
     if (i == 2)
     {
         if ( (strlen (host_name) > 0) && (strlen(ip_address) > 0) )
         {
             strncpy (ip, ip_address, len);
         }
         break;
     }
     memset (buffer, 0, sizeof(buffer));
  }
  fclose (fp);
  return;
}

/*****************************************************************************/
void dns_handle_new_query(dns_request_t *m)
{
  struct in_addr in;
  int retval = -1;
  dns_dyn_list_t *dns_entry;
 
  /*BRCM: support IPv4 only*/
  if( m->message.question[0].type == A /*|| m->message.question[0].type == AAA*/){
      /* standard query */
     //retval = cache_lookup_name( m->cname, m->ip );

#ifdef ACTION_TEC_DNS_CACHE
      if(!dns_cache_enabled())
          retval = 0;
      else{
#endif
      
#ifdef DNS_DYN_CACHE
      if ((dns_entry = dns_dyn_find(m))) {
        strcpy(m->ip, inet_ntoa(dns_entry->addr));
        m->ttl = abs(dns_entry->expiry - time(NULL));
        retval = 1;
      }
      else
        retval = 0;
#endif

#ifdef ACTION_TEC_DNS_CACHE
      }
#endif


  }else if( m->message.question[0].type == PTR ) {
      /* reverse lookup */
      //retval = cache_lookup_ip( m->ip, m->cname );

#ifdef DNS_DYN_CACHE
      if ((dns_entry = dns_dyn_find(m))) {
        strcpy(m->cname, dns_entry->cname);
        m->ttl = abs(dns_entry->expiry - time(NULL));
        retval = 1;
      }
      else 
        retval = 0;
#endif

  }
  else if( m->message.question[0].type == AAA){ /* IPv6 */
      retval = 0;
  }
  else //BRCM
  {
      retval = 0;
  }

#ifdef CUSTOMER_ACTIONTEC
  if(retval == 0){
  
	    FILE *fp;
	    char hostname[40] = {0};
	    char hostip[80] = {0};
	    char tmp_buf[256] = {0};
	    #define HOSTFILE "/var/hosts"
	    
	    if(fp = fopen(HOSTFILE, "r"))
	    {
		while(fgets(tmp_buf, sizeof(tmp_buf), fp))
		{
			sscanf(tmp_buf, "%s %s \n",hostip, hostname);
			if(!strcmp(hostname, m->cname))
			{
			    strcpy(m->ip, hostip);
			    retval = 1;
			    break;
			}
			else
			{
			    retval = 0;
			}
		}
                fclose (fp);
	    }
  
  
  
  }

#endif


#if defined(CUSTOMER_VERIZON)  && defined(ACTION_TEC_WBA_DPROXY)
  do{
	  char flag[4]={0,0,0,0};
	  //char *verizon_reg_url[2][]={
          struct wba_url verizon_reg_url[] ={ 
          {"activatemydsl.verizon.net", "206.46.232.55"},
          {"activatemyfios.verizon.net", "206.46.232.56"},
          {"wbadownload.verizon.net", "206.46.232.57"},
          {"activate.verizon.net", "206.46.232.49"},
          {"netservices.verizon.net", "206.46.232.44"},
          {"sso.verizon.net", "206.46.232.47"},
          {"dcshm.verizon.net", "206.46.230.68"},
          {"dcs.verizon.net", "206.46.230.132"},
          {"noaccess.verizon.net", "206.46.230.148"}
	  };
	  cmsPsp_get("wba_flag", flag, 4);
          //printf("--flag=[%d] %d\n", flag[0], atoi(flag[0]));
          wba_flag = 0;
	  if(flag[0] == 0){/*WBA status is disable*/
          wba_flag = 1;
		  int ii;
		  int count = sizeof(verizon_reg_url)/sizeof(struct wba_url);
                  //printf("--count=%d\n", count);
		  for(ii = 0 ;ii < count; ii++){
			  if( strcmp(m->cname,verizon_reg_url[ii].url) == 0)
                          {
                              strcpy(m->ip, verizon_reg_url[ii].ip);
                              //printf("--return ip as %s\n", verizon_reg_url[ii].ip);
                              retval = 1;
				  break;
                          }
		  }
#if 1 
		  if(ii == count ){ /*have not found one matched*/
			strcpy(m->ip, "206.46.232.55");
                        //printf("--return verizon ip 206.46.232.55\n");
			retval = 1;
		  }
#endif
	  }
  }while(0);
#endif


  debug(".......... %s ---- %s\n", m->cname, m->ip );
  //printf("--.......... %s ---- %s retval=%d\n", m->cname, m->ip, retval );
 
#ifdef CUSTOMER_ATT_DIAGNOSTIC
  diagnostic_flag = 1;  // set to 1 just for demo purpose
#endif
  
  switch( retval )
  {
     case 0:

     debug("config.name_server=%s is_connected=%d", config.name_server, is_connected());
     inet_aton( config.name_server, &in );

     if( is_connected()){
        debug("Adding to list-> id: %d\n", m->message.header.id);
        dns_list_add(m);
        /* relay the query untouched */
        debug("sending query out on dns_querysock\n");
     }else{
#ifdef CUSTOMER_ATT_DIAGNOSTIC
       char lan_ip[64];
       memset (lan_ip, 0, sizeof(lan_ip));
       get_lanip(lan_ip, sizeof(lan_ip)-1);
       if (strlen (lan_ip) > 0)
          strcpy(m->ip, lan_ip);
       else
          strcpy(m->ip, "192.168.0.1");
       diagnostic_flag = 1;
       dns_construct_reply( m );
       dns_write_packet( dns_sock, m->src_addr, m->src_port, m );
       return;
#endif
        debug("wan not up, No DNS information: send to magic ppp addr.\n");
     }
     dns_write_packet( dns_querysock, in, PORT, m );
     break;

     case 1:
        dns_construct_reply( m );
        dns_write_packet( dns_sock, m->src_addr, m->src_port, m );
        debug("Cache hit\n");
        break;
     default:
        debug("Unknown query type: %d\n", m->message.question[0].type );
   }
}
/*****************************************************************************/
void dns_handle_request(dns_request_t *m)
{
  struct in_addr in;
  dns_request_t *ptr = NULL;

  /* request may be a new query or a answer from the upstream server */
  ptr = dns_list_find_by_id(m);

  if( ptr != NULL ){
      debug("Found query in list; id 0x%04x flags 0x%04x\n ip %s --- cname %s\n", 
             m->message.header.id, m->message.header.flags.flags, m->ip, m->cname);

      /* message may be a response */
      if( m->message.header.flags.flags & 0x8000)
      {
          dns_write_packet( dns_sock, ptr->src_addr, ptr->src_port, m );
          debug("Replying with answer from %s, id 0x%04x\n", inet_ntoa( m->src_addr), m->message.header.id);
          dns_list_unarm_requests_after_this( ptr );  // BRCM
          dns_list_remove( ptr );         
      #if 0 //BRCM: Don't write to cache for saving device resource.
          if( m->message.header.flags.f.rcode == 0 ){ // lookup was succesful
              debug("Cache append: %s ----> %s\n", m->cname, m->ip );
              cache_name_append( m->cname, m->ip );
          }
      #endif
#ifdef DNS_DYN_CACHE
          if( m->message.question[0].type != AAA) /* No cache for IPv6 */
		  {
#ifdef ACTION_TEC_DNS_CACHE
			  if(dns_cache_enabled())
#endif
				dns_dyn_cache_add(m);
		  }
#endif
          //BRCM
          dns_probe_activate(m->src_addr.s_addr);
      }
      else
      {
         debug("Duplicate query to %s, send again\n", m->cname);
         inet_aton( config.name_server, &in );
         dns_write_packet( dns_querysock, in, PORT, m );
      }
  }
  else
  {
      dns_handle_new_query( m );
  }

  debug("dns_handle_request: done\n\n");
}

/*****************************************************************************/
static void processCmsMsg(void)
{
  CmsMsgHeader *msg;
  CmsRet ret;

  while( (ret = cmsMsg_receiveWithTimeout(msgHandle, &msg, 0)) == CMSRET_SUCCESS) {
    switch(msg->type) {
    case CMS_MSG_SET_LOG_LEVEL:
      cmsLog_debug("got set log level to %d", msg->wordData);
      cmsLog_setLevel(msg->wordData);
      if ((ret = cmsMsg_sendReply(msgHandle, msg, CMSRET_SUCCESS)) != CMSRET_SUCCESS) {
        cmsLog_error("send response for msg 0x%x failed, ret=%d", msg->type, ret);
      }
      break;
    case CMS_MSG_SET_LOG_DESTINATION:
      cmsLog_debug("got set log destination to %d", msg->wordData);
      cmsLog_setDestination(msg->wordData);
      if ((ret = cmsMsg_sendReply(msgHandle, msg, CMSRET_SUCCESS)) != CMSRET_SUCCESS) {
        cmsLog_error("send response for msg 0x%x failed, ret=%d", msg->type, ret);
      }
      break;
    case CMS_MSG_DNSPROXY_RELOAD:
      cmsLog_debug("received CMS_MSG_DNSPROXY_RELOAD\n");
      /* Reload config file */
#ifdef DNS_DYN_CACHE
      dns_dyn_hosts_add();
#endif
      dns_wanup = dns_probe_init();

      /*
       * During CDRouter dhcp scaling tests, this message is sent a lot to dnsproxy.
       * To make things more efficient/light weight, the sender of the message does
       * not expect a reply.
       */
      break;

#ifdef SUPPORT_DEBUG_TOOLS

    case CMS_MSG_DNSPROXY_DUMP_STATUS:
       printf("\n============= Dump dnsproxy status=====\n");
       printf("config.name_server=%s config.domain=%s dns_wanup=%d\n",
              config.name_server, config.domain_name, dns_wanup);
       dns_probe_print();
       dns_list_print();
       dns_dyn_print();
       break;

    case CMS_MSG_DNSPROXY_DUMP_STATS:
       printf("stats dump not implemented yet\n");
       break;

#endif /* SUPPORT_DEBUG_TOOLS */
    
	 /*
	  * change the web activity log status 
	  */
	 case CMS_MSG_DNSPROXY_DISABLE_WEBLOG:
	   enableWebLog = 1 ;       
       break;
 
    default:
      cmsLog_error("unrecognized msg 0x%x", msg->type);
      break;
    }
    CMSMEM_FREE_BUF_AND_NULL_PTR(msg);
  }
  
  if (ret == CMSRET_DISCONNECTED) {
    cmsLog_error("lost connection to smd, exiting now.");
    dns_main_quit = 1;
  }
}


FILE* g_file;

/*****************************************************************************/
int dns_main_loop()
{
    struct timeval tv, *ptv;
    fd_set active_rfds;
    int next_probe_time = 0;
    int retval;
    dns_request_t m;
    dns_request_t *ptr, *next;	
#ifdef CUSTOMER_VERIZON
    struct in_addr in;
    int dns_sock_flag = 0;
#endif
    /* open log file */
    g_file=fopen("/var/webActivityLog", "w");

#ifdef CUSTOMER_VERIZON
    sleep(10);
    init_sigaction();
#endif
    while( !dns_main_quit )
    {
      /* set the one second time out */
      //BRCM: set timeout to the earliest pending request's timeout or
      //next probe time (if in probing procedure). If there
      //is no pending requests and not in probing procedure, timeout will
      //be 0, causing select() to wait forever until received packets on
      //any sockets.
#ifndef CUSTOMER_VERIZON      
      int next_request_time = dns_list_next_time();
      if (next_request_time) {
         if (next_request_time < next_probe_time || !next_probe_time) {
            debug("use next_request_time = %d", next_request_time);
            tv.tv_sec = next_request_time;
         } else {
            tv.tv_sec = next_probe_time;
         }
      } else {
         tv.tv_sec = next_probe_time;
      }

      if (tv.tv_sec == 0) { /* To wait indefinitely */
         ptv = NULL;
         debug("\n\n=============select will wait indefinitely============");
      } else {
        tv.tv_usec = 0;
        ptv = &tv;
        debug("select timeout = %lu seconds", tv.tv_sec);
     }
#else
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    ptv = &tv;
    debug("select timeout = %lu seconds",tv.tv_sec);
#endif
      /* now copy the main rfds in the active one as it gets modified by select*/
      active_rfds = rfds;
      	
      retval = select( FD_SETSIZE, &active_rfds, NULL, NULL, ptv );

      if (retval){
         debug("received something");
         if (FD_ISSET(msg_fd, &active_rfds)) { /* message from ssk */
            debug("received cms message");
            processCmsMsg();

         } else if (FD_ISSET(dns_sock, &active_rfds)) { /* DNS message */
            debug("received DNS message (LAN side)\n");
            /* data is now available */
#ifdef CUSTOMER_VERIZON			
			if(!dns_sock_flag){
				dns_sock_flag = 1;	
	    	    init_time();
			}		  
#endif			
            bzero(&m, sizeof(dns_request_t));
            //BRCM
            //dns_read_packet( dns_sock, &m );
            if (dns_read_packet(dns_sock, &m) == 0) 
                dns_handle_request( &m );

         } else if (FD_ISSET(dns_querysock, &active_rfds)) {
            debug("received DNS response (WAN side)\n");
            bzero(&m, sizeof(dns_request_t));
            if (dns_read_packet(dns_querysock, &m) == 0 && !dns_probe_response(&m))
				dns_handle_request( &m );
         }
      } else { /* select time out */
      	   time_t now = time(NULL);
		   int doSwitch=0;
#ifndef  CUSTOMER_VERIZON	   
          debug("select timed out, next_request_time=%d next_probe_time=%d dns_recover_time=%d",
                next_request_time, next_probe_time, dns_recover_time);
#endif	

         /*
          * There could be several reasons for select timeout.
          * a) It is time for switching back to primary server.
          * b) A query timed out, but we know from other dns responses that the
          *    dns server is actually up. The UDP request or response from this
          *    query was simply lost.  Do not switch dns servers.  Do not send an error
          *    reply to the LAN client, just let it timeout.  This is what the LAN
          *    client would experience if no dnsproxy was here.
          * c) A query timed out, and we suspect the primary server is down.
          *    Switch to the secondary DNS server.
          */

         if (now >= dns_recover_time) {
            dns_probe_switchback();
         }
		 ptr = dns_request_list;
	 while (ptr) {
		 next = ptr->next;
		 if (ptr->expiry <= now) {
			debug("removing expired request %p\n", ptr);
		 
			if (ptr->switch_on_timeout)
			{
				doSwitch = 1;
				 /*
				   * I don't see the point of sending an error reply to
				   * the LAN client.  Why not just let it timeout.  It will
				   * resend the request anyways, right?  Since the original
				   * dproxy code did it, I'll leave it here.  mwang 8/31/09.
				*/
		 
			   	dns_construct_error_reply(ptr);
				dns_write_packet( dns_sock, ptr->src_addr, ptr->src_port, ptr );
					   
			 }
			 dns_list_remove(ptr);
		  }
		  ptr = next;
	 }
#ifndef CUSTOMER_VERIZON	 
#ifndef DNS_PROBE
         if (doSwitch) {
			 dns_probe_set_recover_time();
	  }	
#endif
#endif
         /* purge cache */
#if 0 //BRCM
         purge_time--;
         if( !purge_time ){
             cache_purge( config.purge_time );
             purge_time = PURGE_TIMEOUT;
         }
#endif
      } /* if (retval) */
      //BRCM
#ifdef CUSTOMER_VERIZON	  
          if(timeout_flag)
			 check_dns_timeout();
#endif			 
          next_probe_time = dns_probe();
    }  /* while (!dns_main_quit) */

    /* close web log file */
    fclose(g_file);
   return 0;
}

/*****************************************************************************/
void debug_perror( char * msg ) {
	debug( "%s : %s\n" , msg , strerror(errno) );
}

#if 0 //BRCM: Mask the debug() function. It's redefined by cmsLog_debug()
#ifdef DPROXY_DEBUG
/*****************************************************************************/
void debug(char *fmt, ...)
{
#define MAX_MESG_LEN 1024
  
  va_list args;
  char text[ MAX_MESG_LEN ];
  
  sprintf( text, "[ %d ]: ", getpid());
  va_start (args, fmt);
  vsnprintf( &text[strlen(text)], MAX_MESG_LEN - strlen(text), fmt, args);	 
  va_end (args);
  
  printf(text);
#if 0 //BRCM 
  if( config.debug_file[0] ){
	 FILE *fp;
	 fp = fopen( config.debug_file, "a");
	 if(!fp){
		syslog( LOG_ERR, "could not open log file %m" );
		return;
	 }
	 fprintf( fp, "%s", text);
	 fclose(fp);
  }
  

  /** if not in daemon-mode stderr was not closed, use it. */
  if( ! config.daemon_mode ) {
	 fprintf( stderr, "%s", text);
  }
#endif
}

#endif
#endif
/*****************************************************************************
 * print usage informations to stderr.
 * 
 *****************************************************************************/
void usage(char * program , char * message ) {
  fprintf(stderr,"%s\n" , message );
  fprintf(stderr,"usage : %s [-c <config-file>] [-d] [-h] [-P]\n", program );
  fprintf(stderr,"\t-c <config-file>\tread configuration from <config-file>\n");
  fprintf(stderr,"\t-d \t\trun in debug (=non-daemon) mode.\n");
  fprintf(stderr,"\t-D \t\tDomain Name\n");
  fprintf(stderr,"\t-P \t\tprint configuration on stdout and exit.\n");
  fprintf(stderr,"\t-v \t\tset verbosity, where num==0 is LOG_ERROR, 1 is LOG_NOTICE, all others is LOG_DEBUG\n");
  fprintf(stderr,"\t-h \t\tthis message.\n");
}
/*****************************************************************************
 * get commandline options.
 * 
 * @return 0 on success, < 0 on error.
 *****************************************************************************/
int get_options( int argc, char ** argv ) 
{
  char c = 0;
  int not_daemon = 0;
  int want_printout = 0;
  char * progname = argv[0];
  SINT32 logLevelNum;
  CmsLogLevel logLevel=DEFAULT_LOG_LEVEL;
  //UBOOL8 useConfiguredLogLevel=TRUE;

  cmsLog_init(EID_DNSPROXY);

  conf_defaults();
#if 1 
  while( (c = getopt( argc, argv, "cD:dhPv:")) != EOF ) {
    switch(c) {
	 case 'c':
  		conf_load(optarg);
		break;
	 case 'd':
		not_daemon = 1;
		break;
     case 'D':
        strcpy(config.domain_name, optarg);
        break;
	 case 'h':
		usage(progname,"");
		return -1;
	 case 'P':
		want_printout = 1;
		break;
         case 'v':
         	logLevelNum = atoi(optarg);
         	if (logLevelNum == 0)
         	{
            		logLevel = LOG_LEVEL_ERR;
         	}
         	else if (logLevelNum == 1)
         	{
            		logLevel = LOG_LEVEL_NOTICE;
         	}
         	else
         	{
            		logLevel = LOG_LEVEL_DEBUG;
         	}
         	cmsLog_setLevel(logLevel);
         	//useConfiguredLogLevel = FALSE;
         	break;
	 default:
		usage(progname,"");
		return -1;
    }
  }
#endif

#if 0  
  /** unset daemon-mode if -d was given. */
  if( not_daemon ) {
	 config.daemon_mode = 0;
  }
  
  if( want_printout ) {
	 conf_print();
	 exit(0);
  }
#endif 

  return 0;
}
/*****************************************************************************/
void sig_hup (int signo)
{
  signal(SIGHUP, sig_hup); /* set this for the next sighup */
  conf_load (config.config_file);
}
/*****************************************************************************/
int main(int argc, char **argv)
{

  /* get commandline options, load config if needed. */
  if(get_options( argc, argv ) < 0 ) {
	  exit(1);
  }

  signal(SIGHUP, sig_hup);

  dns_init();

//BRCM: Don't fork a task again!
#if 0 
  if (config.daemon_mode) {
    /* Standard fork and background code */
    switch (fork()) {
	 case -1:	/* Oh shit, something went wrong */
		debug_perror("fork");
		exit(-1);
	 case 0:	/* Child: close off stdout, stdin and stderr */
		close(0);
		close(1);
		close(2);
		break;
	 default:	/* Parent: Just exit */
		exit(0);
    }
  }
#endif
  dns_main_loop();

  return 0;
}

#ifdef CUSTOMER_VERIZON

void init_time()
{
	struct itimerval value;
	value.it_value.tv_sec = 2;
	value.it_value.tv_usec = 0;
	value.it_interval = value.it_value;
	setitimer(ITIMER_REAL, &value, NULL);
}

void init_sigaction()
{
	struct sigaction tact;
	tact.sa_handler = set_timeout_flag;
	tact.sa_flags = 0;
	sigemptyset(&tact.sa_mask);
	sigaction(SIGALRM, &tact, NULL);
}

void set_timeout_flag(int signo)
{
	if(!timeout_flag)
		timeout_flag = 1;
}
void check_dns_timeout()
{
	dns_request_t *ptr,*next;
        time_t now = time(NULL);

	ptr = dns_request_list;

	while(ptr)
	{
	   next = ptr->next;
	   if(!(ptr->message.header.flags.flags & 0x8000)){
	   	if(strcmp(inet_ntoa( ptr->src_addr),"127.0.0.1")){
		   debug("Replying with answer from %s, id 0x%04x\n", inet_ntoa( ptr->src_addr), ptr->message.header.id);	   
		   if(now <= ptr->expiry){
			if(now == ptr->timeout){
				ptr->timeout = now + DNS_SERVER_TIMEOUT;
				timeOutReq(ptr);
			}else
				debug("timeout value is %d",(ptr->timeout -now));
		    }
		   else{
				debug("removing expired request %p\n", ptr);
			        if (ptr->switch_on_timeout){
				        dns_construct_error_reply(ptr);
						dns_write_packet( dns_sock, ptr->src_addr, ptr->src_port, ptr );
				  }
				dns_list_remove(ptr);
		  }
	    }
	   }
	   ptr = next;
	}
	timeout_flag = 0;
}
//time out and send request again
void timeOutReq(dns_request_t *ptr)
{
    time_t now = time(NULL);
    int doSwitch=0;
    int count = 0;
    struct in_addr in;
    if((ptr->pri_try_count + ptr->sec_try_count)>=(DNS_PROBE_MAX_TRY*DNS_SERVER_TIMEOUT))
    {	//try 6 times ,then remove the request		
		debug("removing try timeout request %p\n", ptr);
		if (ptr->switch_on_timeout){
		        dns_construct_error_reply(ptr);
				dns_write_packet( dns_sock, ptr->src_addr, ptr->src_port, ptr );
		  }
		 dns_list_remove(ptr);	
		 return;
      }

	if (primary_ns[0] &&
		 !strcmp(config.name_server, primary_ns)){
		if(ptr->pri_try_count >= DNS_PROBE_MAX_TRY){
	        dns_probe_set_recover_time();
		}
		else {
	        ptr->pri_try_count++;
	        printf("Duplicate query to %s, send again %d\n", ptr->cname,ptr->pri_try_count);
		     //strcpy(config.name_server,secondary_ns);
	        inet_aton( config.name_server, &in );
	        dns_write_packet( dns_querysock, in, PORT, ptr );	 
		}
	}
    if (secondary_ns[0] &&
	   !strcmp(config.name_server, secondary_ns)){
		if(ptr->sec_try_count < DNS_PROBE_MAX_TRY){
			ptr->sec_try_count++;
		    printf("Duplicate query to %s, send secondary again %d\n", ptr->cname,ptr->sec_try_count);
		    inet_aton( config.name_server, &in );
		    dns_write_packet( dns_querysock, in, PORT,ptr );    
		}
	} 
}
#endif
