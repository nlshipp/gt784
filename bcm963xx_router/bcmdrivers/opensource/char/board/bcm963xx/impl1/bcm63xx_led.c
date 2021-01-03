/*
<:copyright-gpl 
 Copyright 2002 Broadcom Corp. All Rights Reserved. 
 
 This program is free software; you can distribute it and/or modify it 
 under the terms of the GNU General Public License (Version 2) as 
 published by the Free Software Foundation. 
 
 This program is distributed in the hope it will be useful, but WITHOUT 
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or 
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License 
 for more details. 
 
 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA. 
:>
*/
/***************************************************************************
 * File Name  : bcm63xx_led.c
 *
 * Description: 
 *    This file contains bcm963xx board led control API functions. 
 *
 ***************************************************************************/

/* Includes. */
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <asm/uaccess.h>

#include <bcm_map_part.h>
#include <board.h>
#include <boardparms.h>

#define k125ms              (HZ / 8)   // 125 ms
#define kFastBlinkCount     0          // 125ms
#define kSlowBlinkCount     1          // 250ms

#define kLedOff             0
#define kLedOn              1

#define kLedGreen           0
#define kLedRed             1

// uncomment // for debug led
// #define DEBUG_LED 1

typedef int (*BP_LED_FUNC) (unsigned short *);

typedef struct {
    BOARD_LED_NAME ledName;
    BP_LED_FUNC bpFunc;
    BP_LED_FUNC bpFuncFail;
} BP_LED_INFO, *PBP_LED_INFO;

typedef struct {
    short ledGreenGpio;             // GPIO # for LED
    short ledRedGpio;               // GPIO # for Fail LED
    BOARD_LED_STATE ledState;       // current led state
    short blinkCountDown;           // Count for blink states
} LED_CTRL, *PLED_CTRL;

static BP_LED_INFO bpLedInfo[] =
{
    {kLedAdsl, BpGetAdslLedGpio, BpGetAdslFailLedGpio},
    {kLedSecAdsl, BpGetSecAdslLedGpio, BpGetSecAdslFailLedGpio},
    {kLedHpna, BpGetHpnaLedGpio, NULL},
    {kLedWanData, BpGetWanDataLedGpio, BpGetWanErrorLedGpio},

#if defined(AEI_VDSL_CUSTOMER_NCS) && defined(CONFIG_BCM96328)
//#if defined(AEI_ADSL_CUSTOMER_QWEST_L5000)
    {kLedSes, BpGetWirelessSesLedGpio, BpGetWirelessSesLedGpiofail},
#else
    {kLedSes, BpGetWirelessSesLedGpio, BpGetWirelessFailSesLedGpio},
#endif
    {kLedVoip, BpGetVoipLedGpio, NULL},
    {kLedVoip1, BpGetVoip1LedGpio, BpGetVoip1FailLedGpio},
    {kLedVoip2, BpGetVoip2LedGpio, BpGetVoip2FailLedGpio},
    {kLedPots, BpGetPotsLedGpio, NULL},
    {kLedDect, BpGetDectLedGpio, NULL},
    {kLedGpon, BpGetGponLedGpio, BpGetGponFailLedGpio},
    {kLedMoCA, BpGetMoCALedGpio, BpGetMoCAFailLedGpio},
#ifdef AEI_VDSL_CUSTOMER_NCS
    {kLedUsb, BpGetUsbLedGpio, NULL},
    {kLedPower, BpGetBootloaderPowerOnLedGpio, BpGetBootloaderStopLedGpio},
#endif
    {kLedEnd, NULL, NULL}
};

// global variables:
static struct timer_list gLedTimer;
static PLED_CTRL gLedCtrl = NULL;
static int gTimerOn = FALSE;
static unsigned int gLedRunningCounter = 0;

void (*ethsw_led_control)(unsigned long ledMask, int value) = NULL;
EXPORT_SYMBOL(ethsw_led_control);

static spinlock_t brcm_ledlock;
static void ledTimerExpire(void);

//**************************************************************************************
// LED operations
//**************************************************************************************

// turn led on and set the ledState
static void setLed (PLED_CTRL pLed, unsigned short led_state, unsigned short led_type)
{
    short led_gpio;
    unsigned short gpio_state;

    if (led_type == kLedRed)
        led_gpio = pLed->ledRedGpio;
    else
        led_gpio = pLed->ledGreenGpio;

    if (led_gpio == -1)
        return;

    if (((led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOn)) || 
        (!(led_gpio & BP_ACTIVE_LOW) && (led_state == kLedOff)))
        gpio_state = 0;
    else
        gpio_state = 1;

#if defined(CONFIG_BCM96328)
    /* Enable LED controller to drive this GPIO */
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->GPIOMode |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM96362)
    /* Enable LED controller to drive this GPIO */
    if (!(led_gpio & BP_GPIO_SERIAL))
        GPIO->LEDCtrl |= GPIO_NUM_TO_MASK(led_gpio);
#endif

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
#if defined(AEI_VDSL_CUSTOMER_NCS) && defined(CONFIG_BCM96328)
//#if defined(AEI_ADSL_CUSTOMER_QWEST_L5000)
	if((led_gpio & 0xff) <= 23){ /*0~23 normal LED,it's controlled by ledMode*/
		LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
		if( gpio_state )
			LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
		else
			LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
	}
	else{ 
		/*GPIO 29 : WPS LED GREEN,,,GPIO 31 : WPS LED RED
		 * GPIO->GPIOio controls on&off for WPS LED ,bit[29],bit[31]
		 * GPIO->GPIODir controls Enable&Disable for WPS LED,bit[29],bit[31]
		 * */
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
	}
#else
    LED->ledMode &= ~(LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    if( gpio_state )
        LED->ledMode |= (LED_MODE_OFF << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
    else
        LED->ledMode |= (LED_MODE_ON << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#endif
#else
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        if( gpio_state )
            GPIO->SerialLed |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->SerialLed &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        if( gpio_state )
            GPIO->GPIOio |= GPIO_NUM_TO_MASK(led_gpio);
        else
            GPIO->GPIOio &= ~GPIO_NUM_TO_MASK(led_gpio);
    }
#endif
}

// toggle the LED
static void ledToggle(PLED_CTRL pLed)
{
    short led_gpio;

    led_gpio = pLed->ledGreenGpio;

    if (led_gpio == -1)
        return;

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
#if defined(AEI_VDSL_CUSTOMER_NCS) && defined(CONFIG_BCM96328)
//#if defined(AEI_ADSL_CUSTOMER_QWEST_L5000)
	if(led_gpio <= 23){ /*0~23 normal LED,it's controlled by ledMode*/
    	LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
	}
	else{ 
		/*GPIO 29 : WPS LED GREEN,,,GPIO 31 : WPS LED RED
		 * GPIO->GPIOio controls on&off for WPS LED ,bit[29],bit[31]
		 * GPIO->GPIODir controls Enable&Disable for WPS LED,bit[29],bit[31]
		 * */
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
    	GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
	}
#else
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#endif
#else
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        GPIO->SerialLed ^= GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
    }
#endif
}

#ifdef AEI_VDSL_CUSTOMER_NCS
static void ledToggleRed(PLED_CTRL pLed)
{
    short led_gpio;

    led_gpio = pLed->ledRedGpio;

    if (led_gpio == -1)
        return;

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
    LED->ledMode ^= (LED_MODE_MASK << GPIO_NUM_TO_LED_MODE_SHIFT(led_gpio));
#else
    if (led_gpio & BP_GPIO_SERIAL) {
        while (GPIO->SerialLedCtrl & SER_LED_BUSY);
        GPIO->SerialLed ^= GPIO_NUM_TO_MASK(led_gpio);
    }
    else {
        GPIO->GPIODir |= GPIO_NUM_TO_MASK(led_gpio);
        GPIO->GPIOio ^= GPIO_NUM_TO_MASK(led_gpio);
    }
#endif
}
#endif 

// led timer.  Will return if timer is already on

// led timer.  Will return if timer is already on
static void ledTimerStart(void)
{
    if (gTimerOn)
        return;

#if defined(DEBUG_LED)
    printk("led: add_timer\n");
#endif

    init_timer(&gLedTimer);
    gLedTimer.function = (void*)ledTimerExpire;
    gLedTimer.expires = jiffies + k125ms;        // timer expires in ~100ms
    add_timer (&gLedTimer);
    gTimerOn = TRUE;
} 


// led timer expire kicks in about ~100ms and perform the led operation according to the ledState and
// restart the timer according to ledState
static void ledTimerExpire(void)
{
    int i;
    PLED_CTRL pCurLed;
    unsigned long flags;

    gTimerOn = FALSE;

    for (i = 0, pCurLed = gLedCtrl; i < kLedEnd; i++, pCurLed++)
    {
#if defined(DEBUG_LED)
        printk("< %s >led[%d]: Mask=0x%04x, State = %d, blcd=%d\n", __FUNCTION__, i, pCurLed->ledGreenGpio, pCurLed->ledState, pCurLed->blinkCountDown);
#endif
        spin_lock_irqsave(&brcm_ledlock, flags);        // LEDs can be changed from ISR
        switch (pCurLed->ledState)
        {
        case kLedStateOn:
        case kLedStateOff:
             pCurLed->blinkCountDown = 0;            // reset the blink count down
             spin_unlock_irqrestore(&brcm_ledlock, flags);
             break;
        case kLedStateFail:
            pCurLed->blinkCountDown--;

            if (pCurLed->blinkCountDown == 0) {
#ifndef AEI_VDSL_CUSTOMER_QWEST
                setLed (pCurLed, kLedOff, kLedRed);
#endif
                spin_unlock_irqrestore(&brcm_ledlock, flags);
            }
            else  {
                spin_unlock_irqrestore(&brcm_ledlock, flags);
                ledTimerStart();
            }
            break;

        case kLedStateSlowBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kSlowBlinkCount;
                ledToggle(pCurLed);
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart();
            break;

#ifdef AEI_VDSL_CUSTOMER_NCS
        case kLedStateRedSlowBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kSlowBlinkCount;
                ledToggleRed(pCurLed);
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart();
            break;
#endif

        case kLedStateFastBlinkContinues:
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart();
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = (((gLedRunningCounter++)&1)? kFastBlinkCount: kSlowBlinkCount);
                ledToggle(pCurLed);
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart(); 
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            if (pCurLed->blinkCountDown-- == 0)
            {
                pCurLed->blinkCountDown = kFastBlinkCount;
                ledToggle(pCurLed);
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart();
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */        
            if (pCurLed->blinkCountDown-- == 0)
            {
                if(((++gLedRunningCounter)%10) == 0) {
                    pCurLed->blinkCountDown = 4;
                    setLed(pCurLed, kLedOff, kLedGreen);
                    pCurLed->ledState = kLedStateUserWpsSessionOverLap;
                }
                else
                {
                    pCurLed->blinkCountDown = kFastBlinkCount;
                    ledToggle(pCurLed);
                }
            }
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            ledTimerStart();                 
            break;        

        default:
            spin_unlock_irqrestore(&brcm_ledlock, flags);
            printk("Invalid state = %d\n", pCurLed->ledState);
        }
    }
}

void __init boardLedInit(void)
{
    PBP_LED_INFO pInfo;
    ETHERNET_MAC_INFO EnetInfo;
    unsigned short i;
    short gpio;

    spin_lock_init(&brcm_ledlock);

#if !(defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362))
    /* Set blink rate for hardware LEDs. */
    GPIO->LEDCtrl &= ~LED_INTERVAL_SET_MASK;
    GPIO->LEDCtrl |= LED_INTERVAL_SET_80MS;
#endif

    gLedCtrl = (PLED_CTRL) kmalloc((kLedEnd * sizeof(LED_CTRL)), GFP_KERNEL);

    if( gLedCtrl == NULL ) {
        printk( "LED memory allocation error.\n" );
        return;
    }

    /* Initialize LED control */
    for (i = 0; i < kLedEnd; i++) {
        gLedCtrl[i].ledGreenGpio = -1;
        gLedCtrl[i].ledRedGpio = -1;
        gLedCtrl[i].ledState = kLedStateOff;
        gLedCtrl[i].blinkCountDown = 0;            // reset the blink count down
    }

    for( pInfo = bpLedInfo; pInfo->ledName != kLedEnd; pInfo++ ) {
        if( pInfo->bpFunc && (*pInfo->bpFunc) (&gpio) == BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledGreenGpio = gpio;
        }
        if( pInfo->bpFuncFail && (*pInfo->bpFuncFail)(&gpio)==BP_SUCCESS )
        {
            gLedCtrl[pInfo->ledName].ledRedGpio = gpio;
        }

#ifdef AEI_VDSL_CUSTOMER_NCS
        if (pInfo->ledName == kLedPower)
            continue;        
#endif

        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedGreen);
        setLed(&gLedCtrl[pInfo->ledName], kLedOff, kLedRed);
    }

    if( BpGetEthernetMacInfo( &EnetInfo, 1 ) == BP_SUCCESS ) {
        if( EnetInfo.usGpioLedPhyLinkSpeed != BP_NOT_DEFINED ) {
            /* The internal Ethernet PHY has a GPIO for 10/100 link speed. */
            gLedCtrl[kLedEphyLinkSpeed].ledGreenGpio = EnetInfo.usGpioLedPhyLinkSpeed;
            setLed(&gLedCtrl[kLedEphyLinkSpeed], kLedOff, kLedGreen);
        }
    }

#if defined(DEBUG_LED)
    for (i=0; i < kLedEnd; i++)
        printk("initLed: led[%d]: Gpio=0x%04x, FailGpio=0x%04x\n", i, gLedCtrl[i].ledGreenGpio, gLedCtrl[i].ledRedGpio);
#endif

}

// led ctrl.  Maps the ledName to the corresponding ledInfoPtr and perform the led operation
void boardLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
    unsigned long flags;
    PLED_CTRL pLed;

    spin_lock_irqsave(&brcm_ledlock, flags);     // LED can be changed from ISR

    if( (int) ledName < kLedEnd ) {

        pLed = &gLedCtrl[ledName];

        // If the state is kLedStateFail and there is not a failure LED defined
        // in the board parameters, change the state to kLedStateSlowBlinkContinues.
        if( ledState == kLedStateFail && pLed->ledRedGpio == -1 )
		 ledState = kLedStateOff;
        // Save current LED state
        pLed->ledState = ledState;

        // Start from LED Off and turn it on later as needed
        setLed (pLed, kLedOff, kLedGreen);
        setLed (pLed, kLedOff, kLedRed);

        // Disable HW control for WAN Data LED. 
        // It will be enabled only if LED state is On
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
        if (ledName == kLedWanData)
            LED->ledHWDis |= GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#elif defined(CONFIG_BCM96368)
        if (ledName == kLedWanData) {
            GPIO->AuxLedCtrl |= AUX_HW_DISAB_2;
            if (pLed->ledGreenGpio & BP_ACTIVE_LOW)
                GPIO->AuxLedCtrl |= (LED_STEADY_ON << AUX_MODE_SHFT_2);
            else
                GPIO->AuxLedCtrl &= ~(LED_STEADY_ON << AUX_MODE_SHFT_2);
        }
#endif

        switch (ledState) {
        case kLedStateOn:
            // Enable SAR to control INET LED
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362)
            if (ledName == kLedWanData)
                LED->ledHWDis &= ~GPIO_NUM_TO_MASK(pLed->ledGreenGpio);
#elif defined(CONFIG_BCM96368)
            if (ledName == kLedWanData) {
                GPIO->AuxLedCtrl &= ~AUX_HW_DISAB_2;
                if (pLed->ledGreenGpio & BP_ACTIVE_LOW)
                    GPIO->AuxLedCtrl &= ~(LED_STEADY_ON << AUX_MODE_SHFT_2);
                else
                    GPIO->AuxLedCtrl |= (LED_STEADY_ON << AUX_MODE_SHFT_2);
            }
#endif
            setLed (pLed, kLedOn, kLedGreen);
            break;

        case kLedStateOff:
            break;

        case kLedStateFail:
            setLed (pLed, kLedOn, kLedRed);
            pLed->blinkCountDown = 1000 * kSlowBlinkCount; 
            ledTimerStart();
            break;

        case kLedStateSlowBlinkContinues:
            pLed->blinkCountDown = kSlowBlinkCount;
            ledTimerStart();
            break;

#ifdef AEI_VDSL_CUSTOMER_NCS
        case kLedStateRedSlowBlinkContinues:
            pLed->blinkCountDown = kSlowBlinkCount;
            ledTimerStart();
            break;
#endif

        case kLedStateFastBlinkContinues:
            pLed->blinkCountDown = kFastBlinkCount;
            ledTimerStart();
            break;

        case kLedStateUserWpsInProgress:          /* 200ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            ledTimerStart();
            break;             

        case kLedStateUserWpsError:               /* 100ms on, 100ms off */
            pLed->blinkCountDown = kFastBlinkCount;
            gLedRunningCounter = 0;
            ledTimerStart();
            break;        

        case kLedStateUserWpsSessionOverLap:      /* 100ms on, 100ms off, 5 times, off for 500ms */
            pLed->blinkCountDown = kFastBlinkCount;
            ledTimerStart();
            break;        

        default:
            printk("Invalid led state\n");
        }
    }
    spin_unlock_irqrestore(&brcm_ledlock, flags);
}
