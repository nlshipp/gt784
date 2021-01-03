/*
 * WPS GPIO functions
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_gpio.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#include <typedefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <signal.h>
#include <assert.h>
#include <bcmgpio.h>
#include <bcmnvram.h>

#include <wps.h>
#include <wps_gpio.h>
#include <wps_wl.h>



#ifdef __ECOS
#include <sys/select.h>
#include <sys/time.h>
#define CLOCK_REALTIME	0
#endif

extern int clock_gettime();

#define WPS_GPIO_BUTTON_VALUE	"wps_button"
#define WPS_GPIO_LED_VALUE	"wps_led"
#define WPS_GPIO_LED_Y_VALUE	"wps_led_y"
#define WPS_GPIO_LED_R_VALUE	"wps_led_r"
#define WPS_GPIO_LED_G_VALUE	"wps_led_g"

#define WPS_NV_BTN_ASSERTLVL	"wps_button_assertlvl"
#define WPS_NV_LED_ASSERTLVL	"wps_led_assertlvl"
#define WPS_NV_LED_Y_ASSERTLVL	"wps_led_y_assertlvl"
#define WPS_NV_LED_R_ASSERTLVL	"wps_led_r_assertlvl"
#define WPS_NV_LED_G_ASSERTLVL	"wps_led_g_assertlvl"

#define WPS_GPIO_LED_DEF_NAME	"wps_led_def"

static int wps_btn_gpio = BCMGPIO_UNDEFINED;
static int wps_led_gpio = BCMGPIO_UNDEFINED;
static int wps_led_y_gpio = BCMGPIO_UNDEFINED;
static int wps_led_r_gpio = BCMGPIO_UNDEFINED;
static int wps_led_g_gpio = BCMGPIO_UNDEFINED;

static int wps_btn_assertlvl = WPS_BTN_ASSERTLVL;
static int wps_led_assertlvl = WPS_LED_ASSERTLVL;
static int wps_led_y_assertlvl = WPS_LED_ASSERTLVL;
static int wps_led_r_assertlvl = WPS_LED_ASSERTLVL;
static int wps_led_g_assertlvl = WPS_LED_ASSERTLVL;

static int wps_led_count = 0;
static int wps_led_gpio_active = BCMGPIO_UNDEFINED;
static int wps_led_assertlvl_active = WPS_LED_ASSERTLVL;


#ifdef _TUDEBUGTRACE
#define WPS_GPIO(fmt, arg...)	printf(fmt, ##arg)
#define WPS_ERROR(fmt, arg...) printf(fmt, ##arg)
#else
#define WPS_GPIO(fmt, arg...)
#define WPS_ERROR(fmt, arg...)
#endif

typedef struct wps_led_s {
	uint blinkticks1; /* total number of on/off ticks */
	uint blinkloops1; /* total number of on/off loop */
	uint16 blinkon1; /* # ticks on */
	uint16 blinkoff1; /* # ticks off */
	uint16 looptimes1; /* loop of stage1 */
	uint blinkticks2; /* total number of on/off ticks */
	uint blinkloops2; /* total number of on/off loop */
	uint16 blinkon2; /* # ticks on */
	uint16 blinkoff2; /* # ticks off */
	uint16 looptimes2; /* loop of stage2 */
	int stopstage; /* recycle flag */
} wps_led_t;

wps_led_t wps_led;

static void wps_gpio_led_off(int gpio_active, int assertlvl_active);

int wps_gpio_btn_init()
{
	char *value;

	/* Determine the GPIO pins for the WPS Button */

	wps_btn_gpio = bcmgpio_getpin(WPS_GPIO_BUTTON_VALUE, BCMGPIO_UNDEFINED);

	if (wps_btn_gpio != BCMGPIO_UNDEFINED) {
		WPS_GPIO("Using pin %d for wps button input\n", wps_btn_gpio);

		if (bcmgpio_connect(wps_btn_gpio, BCMGPIO_DIRN_IN) != 0) {
			WPS_ERROR("Error connecting GPIO %d to wps button\n",
				wps_btn_gpio);
			return -1;
		}

		value = nvram_get(WPS_NV_BTN_ASSERTLVL);
		if (value) {
			wps_btn_assertlvl = atoi(value) ? 1 : 0;
			WPS_GPIO("Using assertlvl %d for wps button\n",
				wps_btn_assertlvl);
		}
	}

	return 0;
}

static int
wps_gpio_led_multi_color_init(char *nv_wps_led, int *gpio_pin,
	char *nv_led_assertlvl, int *led_assertlvl)
{
	char *value;

	*gpio_pin = bcmgpio_getpin(nv_wps_led, BCMGPIO_UNDEFINED);
	if (*gpio_pin != BCMGPIO_UNDEFINED) {
		WPS_GPIO("Using pin %d for %s output\n", *gpio_pin, nv_wps_led);

		if (bcmgpio_connect(*gpio_pin, BCMGPIO_DIRN_OUT) != 0) {
			WPS_GPIO("Error connecting GPIO %d to %s\n",
				*gpio_pin, nv_wps_led);
			*gpio_pin = BCMGPIO_UNDEFINED;
			return -1;
		}

		value = nvram_get(nv_led_assertlvl);
		if (value) {
			*led_assertlvl = atoi(value) ? 1 : 0;
			WPS_GPIO("Using assertlvl %d for %s\n",
				*led_assertlvl, nv_wps_led);
		}
	}

	return 0;
}

static void
wps_gpio_led_multi_color_cleanup(int *gpio_pin)
{
	if (*gpio_pin != BCMGPIO_UNDEFINED) {
		bcmgpio_disconnect(*gpio_pin);
		*gpio_pin = BCMGPIO_UNDEFINED;
	}
}

static void
wps_gpio_led_multi_color_set_active(wps_blinktype_t blinktype)
{
	unsigned long led_mask;
	unsigned long value;
	int new_gpio_active, new_assertlvl_active;
	int old_gpio_active = BCMGPIO_UNDEFINED;
	int old_assertlvl_active = WPS_LED_ASSERTLVL;

	new_gpio_active = wps_led_gpio;
	new_assertlvl_active = wps_led_assertlvl;

	switch ((int)blinktype) {
	case WPS_BLINKTYPE_INPROGRESS: /* Multi-color yellow */
		if (wps_led_y_gpio != BCMGPIO_UNDEFINED) {
			new_gpio_active = wps_led_y_gpio;
			new_assertlvl_active = wps_led_y_assertlvl;
		}
		break;
	case WPS_BLINKTYPE_ERROR: /* Multi-color red */
	case WPS_BLINKTYPE_OVERLAP: /* Multi-color red */
		if (wps_led_r_gpio != BCMGPIO_UNDEFINED) {
			new_gpio_active = wps_led_r_gpio;
			new_assertlvl_active = wps_led_r_assertlvl;
		}
		break;
	case WPS_BLINKTYPE_SUCCESS: /* Multi-color green */
		if (wps_led_g_gpio != BCMGPIO_UNDEFINED) {
			new_gpio_active = wps_led_g_gpio;
			new_assertlvl_active = wps_led_g_assertlvl;
		}
		break;
	default:
		new_gpio_active = BCMGPIO_UNDEFINED;
		new_assertlvl_active = WPS_LED_ASSERTLVL;
		break;
	}

	if (wps_led_gpio_active != new_gpio_active) {
		old_gpio_active = wps_led_gpio_active;
		old_assertlvl_active = wps_led_assertlvl_active;
	}

	/* change active led first */
	wps_led_gpio_active = new_gpio_active;
	wps_led_assertlvl_active = new_assertlvl_active;

	/* then off old active led */
	if (old_gpio_active == BCMGPIO_UNDEFINED)
		return;

	led_mask = ((unsigned long)1 << old_gpio_active);
	value = ((unsigned long) ~old_assertlvl_active << old_gpio_active);
	value &= led_mask;
	bcmgpio_out(led_mask, value);

	return;
}

static void
wps_gpio_led_multi_color_stop()
{
	switch (wps_led_count) {
	case 0:
		if (wps_led_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_off(wps_led_gpio, wps_led_assertlvl);
		break;

	case 2:
	case 3:
		/* do multi-color cleanup */
		if (wps_led_y_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_off(wps_led_y_gpio, wps_led_y_assertlvl);
		if (wps_led_r_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_off(wps_led_r_gpio, wps_led_r_assertlvl);
		if (wps_led_g_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_off(wps_led_g_gpio, wps_led_g_assertlvl);
		break;

	default:
		break;
	}

	return;
}


int wps_gpio_led_init()
{
	char *wps_led_def;

	/* reset wps_led structure */
	memset(&wps_led, 0, sizeof(wps_led));

	/* Determine the GPIO pins for the WPS led */
	/*
	 * WPS led configuration rules:
	 * 1. if "wps_led_y" && "wps_led_r" && "wps_led_g" are valid then "wps_led" is unused.
	 * 2. if two of ("wps_led_y", "wps_led_r", "wps_led_g") are valid then wps_led_def is used
	 *    to indicate the default wps led.
	 * 3. if none of ("wps_led_y", "wps_led_r", "wps_led_g") are valid then "wps_led" is used.
	 * 4. if one of the initializations failed, return -1.
	 */
	wps_led_count = 0;
	if ((wps_led_y_gpio = bcmgpio_getpin(WPS_GPIO_LED_Y_VALUE, BCMGPIO_UNDEFINED))
		!= BCMGPIO_UNDEFINED)
		wps_led_count++;
	if ((wps_led_r_gpio = bcmgpio_getpin(WPS_GPIO_LED_R_VALUE, BCMGPIO_UNDEFINED))
		!= BCMGPIO_UNDEFINED)
		wps_led_count++;
	if ((wps_led_g_gpio = bcmgpio_getpin(WPS_GPIO_LED_G_VALUE, BCMGPIO_UNDEFINED))
		!= BCMGPIO_UNDEFINED)
		wps_led_count++;

	switch (wps_led_count) {
	case 0:
		wps_led_gpio = bcmgpio_getpin(WPS_GPIO_LED_VALUE, BCMGPIO_UNDEFINED);
		if (wps_led_gpio == BCMGPIO_UNDEFINED) {
			WPS_GPIO("No default wps led configuration\n");
			return -1;
		} else {
			return wps_gpio_led_multi_color_init(WPS_GPIO_LED_VALUE, &wps_led_gpio,
				WPS_NV_LED_ASSERTLVL, &wps_led_assertlvl);
		}
	case 2:
		/* does default wps led match one of two */
		wps_led_def = nvram_get(WPS_GPIO_LED_DEF_NAME);
		if (wps_led_def == NULL) {
			WPS_GPIO("No default wps led configuration\n");
			return -1;
		} else {
			wps_led_gpio = atoi(wps_led_def);
			if (wps_led_y_gpio != wps_led_gpio &&
			    wps_led_r_gpio != wps_led_gpio &&
			    wps_led_g_gpio != wps_led_gpio) {
				WPS_GPIO("Default wps led configuration does not match"
					"any multi-color led\n");
				return -1;
			}
			WPS_GPIO("Using pin %d for wps led default\n", wps_led_gpio);
		}
		/* do multi-color init */
		if (wps_led_y_gpio != BCMGPIO_UNDEFINED &&
			wps_gpio_led_multi_color_init(WPS_GPIO_LED_Y_VALUE, &wps_led_y_gpio,
			WPS_NV_LED_Y_ASSERTLVL, &wps_led_y_assertlvl) != 0) {
			return -1;
		} else if (wps_led_r_gpio != BCMGPIO_UNDEFINED &&
			wps_gpio_led_multi_color_init(WPS_GPIO_LED_R_VALUE, &wps_led_r_gpio,
			WPS_NV_LED_R_ASSERTLVL, &wps_led_r_assertlvl) != 0) {
			if (wps_led_y_gpio != BCMGPIO_UNDEFINED)
				wps_gpio_led_multi_color_cleanup(&wps_led_y_gpio);
			return -1;
		} else if (wps_led_g_gpio != BCMGPIO_UNDEFINED &&
			wps_gpio_led_multi_color_init(WPS_GPIO_LED_G_VALUE, &wps_led_g_gpio,
			WPS_NV_LED_G_ASSERTLVL, &wps_led_g_assertlvl) != 0) {
			if (wps_led_y_gpio != BCMGPIO_UNDEFINED)
				wps_gpio_led_multi_color_cleanup(&wps_led_y_gpio);
			if (wps_led_r_gpio != BCMGPIO_UNDEFINED)
				wps_gpio_led_multi_color_cleanup(&wps_led_r_gpio);
			return -1;
		}
		break;
	case 3:
		if (wps_gpio_led_multi_color_init(WPS_GPIO_LED_Y_VALUE, &wps_led_y_gpio,
			WPS_NV_LED_Y_ASSERTLVL, &wps_led_y_assertlvl) != 0) {
			return -1;
		} else if (wps_gpio_led_multi_color_init(WPS_GPIO_LED_R_VALUE, &wps_led_r_gpio,
			WPS_NV_LED_R_ASSERTLVL, &wps_led_r_assertlvl) != 0) {
			wps_gpio_led_multi_color_cleanup(&wps_led_y_gpio);
			return -1;
		} else if (wps_gpio_led_multi_color_init(WPS_GPIO_LED_G_VALUE, &wps_led_g_gpio,
			WPS_NV_LED_G_ASSERTLVL, &wps_led_g_assertlvl) != 0) {
			wps_gpio_led_multi_color_cleanup(&wps_led_y_gpio);
			wps_gpio_led_multi_color_cleanup(&wps_led_r_gpio);
			return -1;
		}
		/*
		 * set default wps led to wps_led_y_gpio, we use wps_led_gpio
		 * to indicate wps gpio led init successed.
		 */
		wps_led_gpio = wps_led_y_gpio;
		break;

	default:
		WPS_GPIO("Wrong wps led NV configuration, count %d\n", wps_led_count);
			return -1;
	}

	return 0;
}

void wps_gpio_btn_cleanup()
{
	if (wps_btn_gpio != BCMGPIO_UNDEFINED) {
		bcmgpio_disconnect(wps_btn_gpio);
		wps_btn_gpio = BCMGPIO_UNDEFINED;
	}
}

void wps_gpio_led_cleanup()
{
	switch (wps_led_count) {
	case 0:
		if (wps_led_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_multi_color_cleanup(&wps_led_gpio);
		break;

	case 2:
	case 3:
		/* do multi-color cleanup */
		if (wps_led_y_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_multi_color_cleanup(&wps_led_y_gpio);
		if (wps_led_r_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_multi_color_cleanup(&wps_led_r_gpio);
		if (wps_led_g_gpio != BCMGPIO_UNDEFINED)
			wps_gpio_led_multi_color_cleanup(&wps_led_g_gpio);
		break;

	default:
		break;
	}
}

wps_btnpress_t wps_gpio_btn_pressed()
{
	unsigned long btn_mask;
	unsigned long value;
	struct timeval time;
	bool long_btnpress = FALSE;
	static bool first_time = TRUE;
	struct timespec start_ts, end_ts;

	if (wps_btn_gpio == BCMGPIO_UNDEFINED)
		return WPS_NO_BTNPRESS;

	btn_mask = ((unsigned long)1 << wps_btn_gpio);

	bcmgpio_in(btn_mask, &value);
	value >>= wps_btn_gpio;

	if (value == wps_btn_assertlvl) {
		clock_gettime(CLOCK_REALTIME, &start_ts);

		do {
			time.tv_sec = 1;
			time.tv_usec = 0;
			select(0, NULL, NULL, NULL, &time);

			bcmgpio_in(btn_mask, &value);
			value >>= wps_btn_gpio;

			clock_gettime(CLOCK_REALTIME, &end_ts);
			if ((end_ts.tv_sec - start_ts.tv_sec) >= WPS_LONG_PRESSTIME) {
				long_btnpress = TRUE;
				break;
			}
		} while (value == wps_btn_assertlvl);

		if (first_time)
			return WPS_NO_BTNPRESS;

		if (long_btnpress == FALSE)
			return WPS_SHORT_BTNPRESS;
		else
			return WPS_LONG_BTNPRESS;
	}
	else {
		first_time = FALSE;
		return WPS_NO_BTNPRESS;
	}
}

static void wps_gpio_led_on()
{
	unsigned long led_mask;
	unsigned long value;

	if (wps_led_gpio_active == BCMGPIO_UNDEFINED)
		return;

	led_mask = ((unsigned long)1 << wps_led_gpio_active);
	value = ((unsigned long) wps_led_assertlvl_active << wps_led_gpio_active);

	bcmgpio_out(led_mask, value);
}

static void wps_gpio_led_off(int gpio_active, int assertlvl_active)
{
	unsigned long led_mask;
	unsigned long value;

	if (gpio_active == BCMGPIO_UNDEFINED)
		return;

	led_mask = ((unsigned long)1 << gpio_active);
	value = ((unsigned long) ~assertlvl_active << gpio_active);
	value &= led_mask;

	bcmgpio_out(led_mask, value);
}

/* call wps_gpio_led_blink_timer in each WPS_LED_BLINK_TIME_UNIT */
void wps_gpio_led_blink_timer()
{
	if (wps_led_gpio == BCMGPIO_UNDEFINED ||
	    wps_led_gpio_active == BCMGPIO_UNDEFINED)
		return;

	/* first stage check */
	if (wps_led.blinkticks1) {
		if (wps_led.blinkticks1 > wps_led.blinkoff1) {
			wps_gpio_led_on();
		} else {
			wps_gpio_led_off(wps_led_gpio_active, wps_led_assertlvl_active);
		}

		/* first stage finished, check second stage */
		if (--wps_led.blinkticks1 == 0) {
			/* check loop */
			if (wps_led.blinkloops1 && --wps_led.blinkloops1) {
				/* need to loop again */
				wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
				return;
			}

			if (wps_led.stopstage == 1) {
				/* clear all blinktick number to stop blink */
				wps_led.blinkticks1 = 0;
				wps_led.blinkticks2 = 0;
				return; /* break after first stage */
			}
		}
	}

	if (wps_led.blinkticks1 == 0 && wps_led.blinkticks2) {
		if (wps_led.blinkticks2 > wps_led.blinkoff2) {
			wps_gpio_led_on();
		} else {
			wps_gpio_led_off(wps_led_gpio_active, wps_led_assertlvl_active);
		}

		/* second stage finished */
		if (--wps_led.blinkticks2 == 0) {
			/* check loop */
			if (wps_led.blinkloops2 && --wps_led.blinkloops2) {
				/* need to loop again */
				wps_led.blinkticks2 = wps_led.blinkon2 + wps_led.blinkoff2;
				return;
			}

			if (wps_led.stopstage == 2) {
				/* clear all blinktick number to stop blink */
				wps_led.blinkticks1 = 0;
				wps_led.blinkticks2 = 0;
				return; /* break after second satge */
			}
		}
	}

	if (wps_led.blinkticks1 == 0 &&
		wps_led.blinkticks2 == 0 &&
		wps_led.stopstage == 0) {
		/* recycle blinkticks */
		wps_led.blinkticks2 = wps_led.blinkon2 + wps_led.blinkoff2;
		wps_led.blinkloops2 = wps_led.looptimes2;
		wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
		wps_led.blinkloops1 = wps_led.looptimes1;
	}

	return;
}

void wps_gpio_led_blink(wps_blinktype_t blinktype)
{
	if (wps_led_gpio == BCMGPIO_UNDEFINED)
		return;

	switch ((int)blinktype) {
	case WPS_BLINKTYPE_INPROGRESS: /* Multi-color yellow */
		memset(&wps_led, 0, sizeof(wps_led));
		wps_led.blinkon1 = 200 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkoff1 = 100 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
		break;

	case WPS_BLINKTYPE_ERROR: /* Multi-color red */
		memset(&wps_led, 0, sizeof(wps_led));
		wps_led.blinkon1 = 100 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkoff1 = 100 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
		break;

	case WPS_BLINKTYPE_OVERLAP: /* Multi-color red */
		memset(&wps_led, 0, sizeof(wps_led));
		wps_led.blinkon1 = 100 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkoff1 = 100 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
		wps_led.looptimes1 = 5;
		wps_led.blinkloops1 = wps_led.looptimes1;
		wps_led.blinkon2 = 0;
		wps_led.blinkoff2 = 500 / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkticks2 = wps_led.blinkon2 + wps_led.blinkoff2;
		break;

	case WPS_BLINKTYPE_SUCCESS: /* Multi-color green */
		memset(&wps_led, 0, sizeof(wps_led));
		wps_led.blinkon1 = (1000 * 300)  / WPS_LED_BLINK_TIME_UNIT;
		wps_led.blinkoff1 = 1;
		wps_led.blinkticks1 = wps_led.blinkon1 + wps_led.blinkoff1;
		wps_led.stopstage = 1;
		break;

	case WPS_BLINKTYPE_STOP_MULTI: /* clear all multi-color leds */
		memset(&wps_led, 0, sizeof(wps_led));
		wps_gpio_led_multi_color_stop();
		return; /* return now */

	default:
		memset(&wps_led, 0, sizeof(wps_led));
		wps_gpio_led_off(wps_led_gpio_active, wps_led_assertlvl_active);
		break;
	}

	wps_gpio_led_multi_color_set_active(blinktype);

	return;
}
