/*
 * WPS GPIO functions
 *
 * Copyright 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <board.h>
#include <bcmnvram.h>
#include <cms_boardioctl.h>
#include <cms.h>
#include <cms_eid.h>
#include <cms_core.h>
#include <cms_msg.h>
#include <wps_gpio.h>
#include "Wsc_dslcpe_led.h"

#if defined(AEI_VDSL_CUSTOMER_NCS)

/*
 * Turn the UNIT TEST On to see the WPS LED behaviour.
 * Unit test only.
 *
#define CUSTOMER_QWEST_WPS_LED_UNIT_TEST
 */
#if defined(CUSTOMER_QWEST_WPS_LED_UNIT_TEST)
#include <pthread.h>
#endif

static pthread_t g_ledThreadHandle;
static void *ledThreadProc(void *p_data);
static void wscLedSet(int led_action, int led_blink_type, int led_event, int led_status);
int wps_led_kill_timer_init(int timeout,void (*timer)());
void wps_led_kill_timer();
#endif //(defined(AEI_VDSL_CUSTOMER_NCS))


extern char **environ;
static wps_blinktype_t current_blinktype = WPS_BLINKTYPE_STOP;
static struct timespec led_change_time;
static int board_fp = 0;

char *wps_get_conf(char *name);
int get_ifname_unit(const char* ifname, int *unit, int *subunit);

void dslcpe_restart_wlan(void)
{
   static void *msgHandle = NULL;
   static char buf[sizeof(CmsMsgHeader) + 32]={0};
   CmsMsgHeader *msg=(CmsMsgHeader *) buf;

   char ptr[8];
   int unit;
   CmsRet ret = CMSRET_INTERNAL_ERROR;

   printf(">>>>> Restarting wireless <<<<<\n");
   strcpy(ptr, nvram_safe_get("wl_unit"));
   unit = 0;

   unit=ptr[0]-0x30;

   if (unit < 0)
      unit = 0;

   if ((ret = cmsMsg_init(EID_WLWPS, &msgHandle)) != CMSRET_SUCCESS)
   {
      printf("could not initialize msg, ret=%d", ret);
      return;
   }

   sprintf((char *)(msg + 1), "Modify:%d", unit + 1);

   msg->dataLength = 32;
   msg->type = CMS_MSG_WLAN_CHANGED;
   msg->src = EID_WLWPS;
   msg->dst = EID_WLMNGR;
   msg->flags_event = 1;
   msg->flags_request = 0;

   if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
      printf("could not send BOUNDIFNAME_CHANGED msg to ssk, ret=%d", ret);
   else
      printf("message CMS_MSG_WLAN_CHANGED sent successfully");

   cmsMsg_cleanup(&msgHandle);
   return;
 }

int bcmSystemEx (char *command)
{
   int pid = 0, status = 0;

   if ( command == 0 )
      return 1;
   pid = fork();
   if ( pid == -1 )
      return -1;

   if ( pid == 0 ) {
      char *argv[4];
      argv[0] = "sh";
      argv[1] = "-c";
      argv[2] = command;
      argv[3] = 0;
      execve("/bin/sh", argv, environ);
      exit(127);
   }

   /* wait for child process return */
   do {
      if ( waitpid(pid, &status, 0) == -1 ) {
         if ( errno != EINTR )
            return -1;
      } else
         return status;
   } while ( 1 );

   return status;
}

int wps_gpio_btn_init()
{
    board_fp = open("/dev/brcmboard", O_RDWR);

    if (board_fp <= 0)
    {
        printf("Open /dev/brcmboard failed!\n");
        return -1;
    }

    memset(&led_change_time, 0, sizeof(led_change_time));
    return 0;
}

void wps_gpio_btn_cleanup()
{
    if (board_fp > 0)
#if defined(AEI_VDSL_CUSTOMER_NCS)
        wscLedSet(WSC_LED_OFF, 0, 0, 0);
#endif
        close(board_fp);
    return;
}

/* Called from wps_monitor only */
wps_btnpress_t wps_gpio_btn_pressed()
{
    #define SES_EVENTS 0x00000001

    int trigger = SES_EVENTS;
    int poll_ret  = 0;
    int timeout = 0;

    wps_btnpress_t btn_event = WPS_NO_BTNPRESS;
    struct pollfd gpiofd          = {0};
    BOARD_IOCTL_PARMS IoctlParms  = {0};

    if (board_fp < 0) 
        goto failure;

    IoctlParms.result = -1;
    IoctlParms.string = (char *)&trigger;
    IoctlParms.strLen = sizeof(trigger);

    if (ioctl(board_fp, BOARD_IOCTL_SET_TRIGGER_EVENT, &IoctlParms) < 0)
        goto failure;

    if(IoctlParms.result < 0)
        goto failure;

    timeout = 0;
    gpiofd.fd = board_fp;
    gpiofd.events |= POLLIN;

    poll_ret = poll(&gpiofd, 1, timeout);

    if (poll_ret < 0)
        goto failure;
   
    if(poll_ret > 0)
    {  
        /* button pressed, check short or long push */
        int count = 0, lbpcount = 0, val = 0, len = 0;
        struct timeval time;
        lbpcount = (WPS_LONG_PRESSTIME * 1000000) / WPS_BTNSAMPLE_PERIOD;

        while ((len = read(board_fp, (char*)&val, sizeof(val))) > 0)
        {
            time.tv_sec = 0;
            time.tv_usec = WPS_BTNSAMPLE_PERIOD;
            select(0, NULL, NULL, NULL, &time);

            if (val & trigger)
                count++;

            if(count >= lbpcount)
                break;
        }

        if (count < lbpcount)
        {
            btn_event = WPS_SHORT_BTNPRESS;
            nvram_set("wps_sta_pin", "00000000");
        }
        else
        {
            btn_event = WPS_LONG_BTNPRESS;
            nvram_set("wps_sta_pin", "00000000");
        }

        /* Physical Button is only used for wl0 */
        nvram_set("wl_unit", "0");
#if defined(AEI_VDSL_CUSTOMER_NCS)
        wscLedSet(WSC_LED_BLINK, kLedStateUserWpsInProgress,
                  WSC_EVENTS_PROC_WAITING, 0);
#endif
        nvram_set("wps_proc_status", "1"); /* set LED blink after PBC is pressed */ 
        printf("WPS Button is Pressed!\n");
    }
            
failure :
  return btn_event;
}

/*If WPS LED from 43XX GPIO, enable compiler flag WPS_LED_FROM_43XX*/
//#define WPS_LED_FROM_43XX

static void wscLedSet(int led_action, int led_blink_type, int led_event, int led_status)
{
#ifdef WPS_LED_FROM_43XX
    switch(led_action)
    {
        case WSC_LED_BLINK:
            if (led_blink_type == kLedStateUserWpsSessionOverLap)
            {
                /*
                 * Overlap uses two frequency patterns; we use two LED assignments,
                 * one for each frequency:
                 * LED[0]: 1000 ms/500 ms
                 * LED[1]: 100 ms/100 ms
                 */
                bcmSystemEx("wlctl led_blinkslow 0x01f403e8");
                bcmSystemEx("wlctl ledbh 0 15");

                bcmSystemEx("wlctl led_blinkcustom 0x00640064");
                bcmSystemEx("wlctl ledbh 1 18");
            }
            else if (led_blink_type == kLedStateUserWpsInProgress)
            {
                bcmSystemEx("wlctl ledbh 0 0");

                bcmSystemEx("wlctl led_blinkcustom 0x00c80064");
                bcmSystemEx("wlctl ledbh 1 18");
            }
            else if (led_blink_type == kLedStateUserWpsError)
            {
                bcmSystemEx("wlctl ledbh 0 0");

                bcmSystemEx("wlctl led_blinkcustom 0x00640064");
                bcmSystemEx("wlctl ledbh 1 18");
            }
            break;

        case WSC_LED_ON:
            bcmSystemEx("wlctl ledbh 0 0");
            bcmSystemEx("wlctl ledbh 1 1");
            break;

        case WSC_LED_OFF:
            /*
             * If the LED is shared, then we restore it to default:
             * LED[0]: ACTIVITY and LED[1] B-RADIO.
             */
            bcmSystemEx("wlctl ledbh 0 2");
            bcmSystemEx("wlctl ledbh 1 5");
            break;
    }
#else
    /* Control 63xx GPIO LED */
    BOARD_IOCTL_PARMS IoctlParms = {0};
    if (board_fp <= 0)
        return;

    led_action &= WSC_LED_MASK;
    led_action |= ((led_status      << WSC_STATUS_OFFSET) & WSC_STATUS_MASK);
    led_action |= ((led_event       << WSC_EVENT_OFFSET)  & WSC_EVENT_MASK);
    led_action |= ((led_blink_type  << WSC_BLINK_OFFSET)  & WSC_BLINK_MASK);

    IoctlParms.result = -1;
    IoctlParms.string = (char *)&led_action;
    IoctlParms.strLen = sizeof(led_action);
    ioctl(board_fp, BOARD_IOCTL_SET_SES_LED, &IoctlParms);
#endif

    clock_gettime(CLOCK_REALTIME, &led_change_time);
    return;
}

/* Called from wps_monitor every 10ms */
void wps_gpio_led_blink_timer()
{
    struct timespec now;
    time_t seconds;

    if (current_blinktype == WPS_BLINKTYPE_STOP)
        return;

    clock_gettime(CLOCK_REALTIME, &now);
    seconds = now.tv_sec - led_change_time.tv_sec;
    
#ifdef WPS_LED_FROM_43XX
    /*
     * If the LED is shared, then we restore it to ACTIVITY/B-RADIO
     * after five minutes.
     */
#else
    /*
     * If the LED is not shared, then we restore it only when the
     * blinktype is WPS_BLINKTYPE_SUCCESS.
     */
    if (current_blinktype == WPS_BLINKTYPE_SUCCESS)
#endif
    {
#if defined(AEI_VDSL_CUSTOMER_TELUS)  //hk_telus     
	if (seconds > 30)
#else
        if (seconds > 300)
#endif			
        {
#if !defined(AEI_VDSL_CUSTOMER_QWEST)  && !defined(AEI_VDSL_CUSTOMER_CENTURYLINK) && !defined(AEI_ADSL_CUSTOMER_TDS_GT784WN)//hk_ctl//hk_wps_q2000
            printf("stop LED\n");
            wscLedSet(WSC_LED_OFF, 0, 0, 0);
#endif
            current_blinktype = WPS_BLINKTYPE_STOP;
        }
    }
#if defined(AEI_VDSL_CUSTOMER_QWEST)  || defined(AEI_VDSL_CUSTOMER_TELUS) || defined(AEI_VDSL_CUSTOMER_CENTURYLINK) //hk_ctl//hk_wps_q2000  //hk_telus      
	if (current_blinktype == WPS_BLINKTYPE_ERROR) {
#if defined(AEI_VDSL_CUSTOMER_TELUS)  //hk_telus     		
		if (seconds > 30) 
#else
		if (seconds > 120) 
#endif			
		{
			wscLedSet(WSC_LED_OFF, 0, 0, 0);
#if !defined(AEI_VDSL_CUSTOMER_TELUS)  //hk_telus  			
            		printf("<WPS_GREEN>\n");
            		wscLedSet(WSC_LED_ON, kLedStateOn, WSC_EVENTS_PROC_SUCC, 0);	
#endif					
			current_blinktype = WPS_BLINKTYPE_STOP;
		}
	}
#endif
    return;
}

/* Called from wps_monitor */
int wps_gpio_led_init()
{
#if defined(CUSTOMER_QWEST_WPS_LED_UNIT_TEST)
    pthread_create(&g_ledThreadHandle, NULL,
                   ledThreadProc, NULL);
    while (1)
        usleep(6000000);
#endif 

    wscLedSet(WSC_LED_OFF, 0, 0, 0);
    current_blinktype = WPS_BLINKTYPE_STOP;
    return 0;
}

/* Called from wps_monitor */
void wps_gpio_led_cleanup(void)
{
    wscLedSet(WSC_LED_OFF, 0, 0, 0);
    current_blinktype = WPS_BLINKTYPE_STOP;
}

/* Called from wps_monitor to change led blinking pattern */
void wps_gpio_led_blink(wps_blinktype_t blinktype)
{
#if defined(AEI_VDSL_CUSTOMER_NCS) 
    switch ((int)blinktype)
    {
        case WPS_BLINKTYPE_STOP:
            printf("<WPS_STOP>\n");
            wscLedSet(WSC_LED_OFF, 0, 0, 0);
            break;

        case WPS_BLINKTYPE_INPROGRESS:
            printf("<WPS_INPROGRESS>\n");
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsInProgress , WSC_EVENTS_PROC_WAITING, 0);
            break;

        case WPS_BLINKTYPE_ERROR:
            printf("<WPS_ERROR>\n");
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsError, WSC_EVENTS_PROC_FAIL, 1);
            break;

        case WPS_BLINKTYPE_OVERLAP:
            printf("<WPS_OVERLAP>\n");
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsSessionOverLap, WSC_EVENTS_PROC_PBC_OVERLAP, 0);
            break;

        case WPS_BLINKTYPE_SUCCESS:
            printf("<WPS_SUCCESS>\n");
            wscLedSet(WSC_LED_ON, kLedStateOn, WSC_EVENTS_PROC_SUCC, 0);
            break;

        default:
            break;
    }

    current_blinktype = blinktype;
#else
    switch ((int)blinktype) 
    {
        case WPS_BLINKTYPE_STOP:
            wscLedSet(WSC_LED_OFF, 0, 0, 0);
            break;

        case WPS_BLINKTYPE_INPROGRESS:
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsInProgress , WSC_EVENTS_PROC_WAITING, 0);
            break;

        case WPS_BLINKTYPE_ERROR:
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsError, WSC_EVENTS_PROC_FAIL, 0);
            break;

        case WPS_BLINKTYPE_OVERLAP:
            wscLedSet(WSC_LED_BLINK, kLedStateUserWpsSessionOverLap, WSC_EVENTS_PROC_PBC_OVERLAP, 0);
            break;

        case WPS_BLINKTYPE_SUCCESS:
            wscLedSet(WSC_LED_ON, kLedStateOn, WSC_EVENTS_PROC_SUCC, 0);
            break;

        default:
            break;
    }

    current_blinktype = blinktype;
#endif
    return;
}

#if defined(AEI_VDSL_CUSTOMER_NCS)
/* Turn Led On */
static inline void wscLedOn(int event, int status)
{
    wscLedSet(WSC_LED_ON, 0, event, status);
    return;
}

/* Turn Led Off */
static inline void wscLedOff(int event, int status)
{
    wscLedSet(WSC_LED_OFF, 0, event, status);
    return;
}

/*Turn LED Blink*/
static inline void wscLedBlink(ses_blinktype_t blinktype,
                               int event, int status)
{
    wscLedSet(WSC_LED_BLINK, blinktype, event, status);
    return;
}

#if defined(CUSTOMER_QWEST_WPS_LED_UNIT_TEST)
/*
 * The Unit test thread function, Loops to show all the LED behaviours
 * (Tested ok)
 */
static void *ledThreadProc(void *p_data)
{
    char curr_state;
    int state =0, status=0;

    printf("-->ledThreadProc started (WPS LED Unit test only)\n");
    while (1) {
        usleep(3030303);
        //curr_state = (char)(random() % 10);
	state = state % 10;
        curr_state = (char)(state++);
	if (status) status = 0;
	else status = 1;

	printf("\n\n");
        switch (curr_state) {
            case WSC_PROC_IDLE:
                printf("-->WPS IDEL (GREEN)\n");
                wscLedOn(WSC_EVENTS_PROC_IDLE, 0);
                break;
            case WSC_PROC_WAITING:
                printf("-->WPS WAITING (Flashing Orange, RED+GREEN)\n");
                wscLedBlink(0, WSC_EVENTS_PROC_WAITING, 0);
                break;
            case WSC_PROC_SUCC:
                printf("-->WPS SUCC (GREEN)\n");
                wscLedOn(WSC_EVENTS_PROC_SUCC, 0);
                break;
            case WSC_PROC_TIMEOUT:
                printf("-->WPS TIMEOUT (not work ???)\n");
                wscLedBlink(0, WSC_EVENTS_PROC_TIMEOUT, 0);
                break;
            case WSC_PROC_FAIL:
                printf("-->WPS FAIL (RED), status = %d\n", status);
                /*wscLedBlink(0, WSC_EVENTS_PROC_FAIL, 0);*/
                wscLedOn(WSC_EVENTS_PROC_FAIL, status);
                break;
            case WSC_PROC_M2_SENT:
                printf("-->WPS M2 (not work ???)\n");
                wscLedBlink(0, WSC_EVENTS_PROC_M2_SENT, 0);
                break;
            case WSC_PROC_M7_SENT:
                printf("-->WPS M7 (not work ???)\n");
                wscLedBlink(0, WSC_EVENTS_PROC_M7_SENT, 0);
                break;
            case WSC_PROC_MSG_DONE:
                printf("-->WPS DONE (not work ???)\n");
                wscLedOn(WSC_EVENTS_PROC_MSG_DONE, 0);
                break;
            case WSC_PROC_PBC_OVERLAP:
                printf("-->WPS OVERLAP (same as WAITING ???), status = %d\n", status);
                wscLedBlink(0, WSC_EVENTS_PROC_PBC_OVERLAP, status);
                break;
            case WSC_PROC_OFF: /* == 9 */
                printf("-->WPS OFF (LED OFF)\n");
                wscLedOff(0, 0);
                break;
            default:
                printf("-->default\n");
                break;
            } /* end switch */
    } /* end while */
}
#endif //(Unit-test)

#endif //(defined(AEI_VDSL_CUSTOMER_NCS))

/*End of file */