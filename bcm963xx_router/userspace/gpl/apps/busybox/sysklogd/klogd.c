/* vi: set sw=4 ts=4: */
/*
 * Mini klogd implementation for busybox
 *
 * Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>.
 * Changes: Made this a standalone busybox module which uses standalone
 * 					syslog() client interface.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>		/* for our signal() handlers */
#include <string.h>		/* strncpy() */
#include <errno.h>		/* errno and friends */
#include <unistd.h>
#include <ctype.h>
#include <sys/syslog.h>
#include <sys/klog.h>

#include "busybox.h"

static void klogd_signal(int sig)
{
	klogctl(7, NULL, 0);
	klogctl(0, 0, 0);
	/* logMessage(0, "Kernel log daemon exiting."); */
	syslog(LOG_NOTICE, "Kernel log daemon exiting.");
	exit(EXIT_SUCCESS);
}

#if defined(CUSTOMER_FWlOG)
// FILE SIZE is 1000K 
#define FILE_SIZE      (1000 * 1000) 
#define FW_LOG_HEAD       "[firewall_log]"
#define FW_LOG_PATH       "/var/log/firewall_log"
static int check_logtime(time_t latest_t, char * timeStr)
{
    struct tm logTm;
    time_t log_t;
    char buf[32] = {0};
    char col[32] = {0};
      
    sscanf(timeStr, "%s %s", buf, col);
    memset(&logTm, 0, sizeof(struct tm));
    strptime(buf, "%m/%d/%Y|%I:%M:%S%p|", &logTm);
    log_t = mktime(&logTm);
    // just show 7days' log
    if ((latest_t-log_t) <60*60*24*7)
        return 0;
    else
        return -1;
}

typedef struct buf_node_s 
{
    struct buf_node_s * next;
    char   *data;
}buf_node_t;

buf_node_t *p_head = NULL;
buf_node_t *p_tail = NULL;
static int write_file()
{
    FILE * fp = NULL;
    buf_node_t *node = p_head;
    time_t log_t = time(NULL);

    if ((fp=fopen(FW_LOG_PATH, "a+"))==NULL)
    {
        printf("klod: open %s failed\n", FW_LOG_PATH);
        return -1;
    }

    if (p_head == NULL)
        goto EXT_SUCC;
    if (p_head == p_tail)
    {
        if (check_logtime(log_t, p_head->data) == 0)
            fprintf(fp, "%s", node->data);
        goto EXT_SUCC;
    }

    do{
        // check log time 
        if (check_logtime(log_t, p_head->data) == 0)
            fprintf(fp, "%s", node->data);
        node = node->next;
    }while (node!=p_head);
        
EXT_SUCC:
    fclose(fp);
    return 0;
}



static void free_node(buf_node_t *node)
{
    free(node->data);
    free(node);
}

// add element on the tail of the list
static int add_node(buf_node_t *node, int *p_total_len)
{
    if (p_head == NULL)
    {
        p_head = node;
        p_tail = node;
        p_head->next = p_tail;
        *p_total_len = strlen(node->data);
    }
    else
    {
        node->next = p_head;
        p_tail->next = node;
        p_tail = node;
        *p_total_len = *p_total_len + strlen(node->data);
    }

    return 0;
}

// just delete the last element of the list
static void del_node(int *p_total_len)
{
    if (p_head == NULL)
        return ;
    if (p_head->next == p_head)
    {
        free_node(p_head);
        *p_total_len = 0;
        p_head = NULL;
        p_tail = NULL;
        return;
    }

    *p_total_len = *p_total_len - strlen(p_head->data);
    p_tail->next = p_head->next;
    free_node(p_head);
    p_head = p_tail->next;
}

static int fill_loopList( const char * const target ) 
{
    static total_len = 0;
    int data_len = 0;
    char src_ip[32] = {0};
    char dst_ip[32] = {0};
    char col[32] = {0}; 
    char record[256] = {0};
    buf_node_t *node;
    char currTime[64];
    struct tm *tmp;
	time_t now;

    sscanf(target, "%s %s %s %s %s %s", col, col, col, src_ip, dst_ip, col);

    now = time(NULL);
    tmp = localtime(&now);
	memset(currTime, 0, sizeof(currTime));
	strftime(currTime, sizeof(currTime), "%m/%d/%Y|%I:%M:%S%p|", tmp);
    sprintf(record, "%s %s %s\n",  currTime, src_ip, dst_ip);

    data_len = strlen(record);
    
    if ((node = (buf_node_t *)malloc(sizeof(buf_node_t))) == NULL)
    { 
        printf("klogd: alloc memory for firewall log failed\n");        
        return -1; 
    }
    if ((node->data = (char *)malloc(data_len + 1)) == NULL)
    {
        printf("klogd: alloc memory for firewall log failed\n");        
        free (node);
        return -1; 
    }

    strcpy(node->data, record);
    node->data[data_len] = 0;
    if ((total_len + data_len )< FILE_SIZE)
    {
        add_node(node ,&total_len); 
    }
    else
    {
        del_node(&total_len);
        add_node(node, &total_len);
    }

    return 0;
}

static void save_fwLog(const char * const log_buffer)
{
    if (strstr(log_buffer, FW_LOG_HEAD) )
    {
        time_t nowTm = time(NULL);
        static time_t last_writeTm = 0;

        fill_loopList(log_buffer);
        // we will write file every 5 sec
        if ((nowTm - last_writeTm) >= 5)
        {
            write_file();
            last_writeTm = time(NULL);
        }
    }
}
#endif

static void doKlogd(const int console_log_level) __attribute__ ((noreturn));
static void doKlogd(const int console_log_level)
{
	int priority = LOG_INFO;
	char log_buffer[4096];
	int i, n, lastc;
	char *start;

	openlog("kernel", 0, LOG_KERN);

	/* Set up sig handlers */
	signal(SIGINT, klogd_signal);
	signal(SIGKILL, klogd_signal);
	signal(SIGTERM, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP." */
	klogctl(1, NULL, 0);

	/* Set level of kernel console messaging.. */
	if (console_log_level != -1)
		klogctl(8, NULL, console_log_level);

	syslog(LOG_NOTICE, "klogd started: " BB_BANNER);

	while (1) {
		/* Use kernel syscalls */
		memset(log_buffer, '\0', sizeof(log_buffer));
		n = klogctl(2, log_buffer, sizeof(log_buffer));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: Error return from sys_sycall: %d - %m.\n", errno);
			exit(EXIT_FAILURE);
		}
#if defined(CUSTOMER_FWlOG)
        save_fwLog(log_buffer);
#endif

		/* klogctl buffer parsing modelled after code in dmesg.c */
		start = &log_buffer[0];
		lastc = '\0';
		for (i = 0; i < n; i++) {
			if (lastc == '\0' && log_buffer[i] == '<') {
				priority = 0;
				i++;
				while (isdigit(log_buffer[i])) {
					priority = priority * 10 + (log_buffer[i] - '0');
					i++;
				}
				if (log_buffer[i] == '>')
					i++;
				start = &log_buffer[i];
			}
			if (log_buffer[i] == '\n') {
				log_buffer[i] = '\0';	/* zero terminate this message */
				syslog(priority, "%s", start);
				start = &log_buffer[i + 1];
				priority = LOG_INFO;
			}
			lastc = log_buffer[i];
		}
	}
}

extern int klogd_main(int argc, char **argv)
{
	/* no options, no getopt */
	int opt;
	int doFork = TRUE;
	unsigned char console_log_level = -1;

#ifdef CUSTOMER_ACTIONTEC
    signed sessionPid;
#endif

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "c:n")) > 0) {
		switch (opt) {
		case 'c':
			if ((optarg == NULL) || (optarg[1] != '\0')) {
				bb_show_usage();
			}
			/* Valid levels are between 1 and 8 */
			console_log_level = *optarg - '1';
			if (console_log_level > 7) {
				bb_show_usage();
			}
			console_log_level++;

			break;
		case 'n':
			doFork = FALSE;
			break;
		default:
			bb_show_usage();
		}
	}

	if (doFork) {
#if defined(__uClinux__)
		vfork_daemon_rexec(0, 1, argc, argv, "-n");
#else /* __uClinux__ */
		if (daemon(0, 1) < 0)
			bb_perror_msg_and_die("daemon");
#endif /* __uClinux__ */
	}

#ifdef CUSTOMER_ACTIONTEC
    /*
    * detach from the terminal so we don't catch the user typing control-c
    */
    if ((sessionPid = setsid()) == -1)
        printf("Could not detach from terminal\n");
#endif 

	doKlogd(console_log_level);

	return EXIT_SUCCESS;
}

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
