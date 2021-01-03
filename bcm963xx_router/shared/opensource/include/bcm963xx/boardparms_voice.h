/*
    Copyright 2000-2010 Broadcom Corporation

    Unless you and Broadcom execute a separate written software license
    agreement governing use of this software, this software is licensed
    to you under the terms of the GNU General Public License version 2
    (the “GPL”), available at http://www.broadcom.com/licenses/GPLv2.php,
    with the following added to such license:

        As a special exception, the copyright holders of this software give
        you permission to link this software with independent modules, and to
        copy and distribute the resulting executable under terms of your
        choice, provided that you also meet, for each linked independent
        module, the terms and conditions of the license of that module. 
        An independent module is a module which is not derived from this
        software.  The special exception does not apply to any modifications
        of the software.

    Notwithstanding the above, under no circumstances may you combine this
    software in any way with any other Broadcom software provided under a
    license other than the GPL, without Broadcom's express prior written
    consent.
*/                       

/**************************************************************************
 * File Name  : boardparms_voice.h
 *
 * Description: This file contains definitions and function prototypes for
 *              the BCM63xx voice board parameter access functions.
 *
 ***************************************************************************/

#if !defined(_BOARDPARMS_VOICE_H)
#define _BOARDPARMS_VOICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <boardparms.h>

#define SI32261ENABLE    /* Temporary #def until fully supported */
#define SI32267ENABLE   /* Temporary #def until fully supported */

/* Board string defines */

#define VOICECFG_NOSLIC_STR            "NOSLIC"
#define VOICECFG_LE88276_STR           "LE88276"
#define VOICECFG_LE88506_STR           "LE88506"
#define VOICECFG_LE9530_STR            "LE9530"
#define VOICECFG_VE890_STR             "VE890"
#define VOICECFG_SI32178_STR           "SI32178"
#define VOICECFG_SI3217X_STR           "SI3217X"
#define VOICECFG_SI3226_STR            "SI3226"
#define VOICECFG_6328AVNG_LE88276_NTR_STR         "LE88276-NTR"
#define VOICECFG_6328AVNG_SI32267_STR             "AVNG_SI32267"
#define VOICECFG_6328AVNG_VE890HV_PARTIAL_STR     "AVNG_VE890HVP"
#define VOICECFG_6328AVNG_VE890HV_STR             "AVNG_VE890HV"

#define VOICECFG_6328AVNGR_SI32176_STR         "AVNGR_SI32176"
#define VOICECFG_6328AVNGR_LE89116_STR         "AVNGR_LE89116"
#define VOICECFG_6328AVNGR_SI3217X_STR         "AVNGR_SI3217X"

#define VOICECFG_6368MVWG_STR                     "MVWG"
#define VOICECFG_6368MBG_LE88276_STR              "MBG_LE88276"
#define VOICECFG_6368MBG_LE88506_STR              "MBG_LE88506"
#define VOICECFG_6368MBG_VE890_STR                "MBG_VE890"
#define VOICECFG_6368MBG_LE89116_STR              "MBG_LE89116"
#define VOICECFG_6368MBG_LE89316_STR              "MBG_LE89316"
#define VOICECFG_6368MBG_SI3217X_STR              "MBG_SI3217X"
#define VOICECFG_6368MBG_SI32176_STR              "MBG_SI32176"
#define VOICECFG_6368MBG_SI32178_STR              "MBG_SI32178"
#define VOICECFG_6368MBG_SI3226_STR               "MBG_SI3226"

#define VOICECFG_6362ADVNGP5_NOSLIC_STR           "ADVNGP5_NOSLIC"
#define VOICECFG_6362ADVNGP5_SI3217X_STR          "ADVNGP5_SI3217X"
#define VOICECFG_6362ADVNGP5_SI32176_STR          "ADVNGP5_SI32176"
#define VOICECFG_6362ADVNGP5_SI32178_STR          "ADVNGP5_SI32178"
#define VOICECFG_6362ADVNGP5_VE890_STR            "ADVNGP5_VE890"
#define VOICECFG_6362ADVNGP5_LE89116_STR          "ADVNGP5_LE89116"
#define VOICECFG_6362ADVNGP5_LE89316_STR          "ADVNGP5_LE89316"
#define VOICECFG_6362ADVNGP5_LE88506_STR          "ADVNGP5_LE88506"
#define VOICECFG_6362ADVNGP5_LE88276_STR          "ADVNGP5_LE88276"
#define VOICECFG_6362ADVNGP5_SI3226_STR           "ADVNGP5_SI3226"

#define VOICECFG_6362ADVNGR_SI3217X_STR           "ADVNGR_SI3217X"
#define VOICECFG_6362ADVNGR_SI32176_STR           "ADVNGR_SI32176"
#define VOICECFG_6362ADVNGR_SI32178_STR           "ADVNGR_SI32178"
#define VOICECFG_6362ADVNGR_SI3217X_NOFXO_STR     "ADVNGR_SI3217XN"
#define VOICECFG_6362ADVNGR_VE890_STR             "ADVNGR_VE890"
#define VOICECFG_6362ADVNGR_LE89116_STR           "ADVNGR_LE89116"
#define VOICECFG_6362ADVNGR_LE89316_STR           "ADVNGR_LE89316"
#define VOICECFG_6362ADVNGR_LE88506_STR           "ADVNGR_LE88506"
#define VOICECFG_6362ADVNGR_LE88276_STR           "ADVNGR_LE88276"
#define VOICECFG_6362ADVNGR_SI3226_STR            "ADVNGR_SI3226"
#define VOICECFG_6362ADVNGR_SI32261_STR           "ADVNGR_SI32261"
#define VOICECFG_6362ADVNGR_SI32267_STR           "ADVNGR_SI32267"
#define VOICECFG_6362ADVNGR_VE890HV_PARTIAL_STR   "ADVNGR_VE890HVP"
#define VOICECFG_6362ADVNGR_VE890HV_STR           "ADVNGR_VE890HV"

#define VOICECFG_6362ADVNGR2_SI3217X_STR          "ADVNGR2_SI3217X"
#define VOICECFG_6362ADVNGR2_VE890_STR            "ADVNGR2_VE890"
#define VOICECFG_6362ADVNGR2_LE88506_STR          "ADVNGR2_LE88506"
#define VOICECFG_6362ADVNGR2_LE88276_STR          "ADVNGR2_LE88276"
#define VOICECFG_6362ADVNGR2_SI3226_STR           "ADVNGR2_SI3226"

#define VOICECFG_6368MVNGR_SI3217X_STR            "MVNGR_SI3217X"
#define VOICECFG_6368MVNGR_SI32176_STR            "MVNGR_SI32176"
#define VOICECFG_6368MVNGR_SI32178_STR            "MVNGR_SI32178"
#define VOICECFG_6368MVNGR_SI3217X_NOFXO_STR      "MVNGR_SI3217XN"
#define VOICECFG_6368MVNGR_VE890_STR              "MVNGR_VE890"
#define VOICECFG_6368MVNGR_LE89116_STR            "MVNGR_LE89116"
#define VOICECFG_6368MVNGR_LE89316_STR            "MVNGR_LE89316"
#define VOICECFG_6368MVNGR_LE88506_STR            "MVNGR_LE88506"
#define VOICECFG_6368MVNGR_LE88276_STR            "MVNGR_LE88276"
#define VOICECFG_6368MVNGR_SI3226_STR             "MVNGR_SI3226"
#define VOICECFG_6368MVNGR_VE890HV_PARTIAL_STR    "MVNGR_VE890HVP"
#define VOICECFG_6368MVNGR_VE890HV_STR            "MVNGR_VE890HV"


/* Maximum number of devices in the system (on the board).
** Devices can refer to DECT, SLAC/SLIC, or SLAC/DAA combo. */
#define BP_MAX_VOICE_DEVICES           5 

/* Maximum numbers of channels per SLAC. */
#define BP_MAX_CHANNELS_PER_DEVICE     2

/* Maximum number of voice channels in the system.
** This represents the sum of all channels available on the devices in the system */
#define BP_MAX_VOICE_CHAN              (BP_MAX_VOICE_DEVICES * BP_MAX_CHANNELS_PER_DEVICE)

/* Max number of GPIO pins used for controling PSTN failover relays
** Note: the number of PSTN failover relays can be larger if multiple
** relays are controlled by single GPIO */
#define BP_MAX_RELAY_PINS              2

#define BP_TIMESLOT_INVALID            0xFF

/* General-purpose flag definitions (rename as appropriate) */
#define BP_FLAG_DSP_APMHAL_ENABLE            (1 << 0)
#define BP_FLAG_ISI_SUPPORT                  (1 << 1)
#define BP_FLAG_MODNAME_TESTNAME2            (1 << 2)
#define BP_FLAG_MODNAME_TESTNAME3            (1 << 3)
#define BP_FLAG_MODNAME_TESTNAME4            (1 << 4)
#define BP_FLAG_MODNAME_TESTNAME5            (1 << 5)
#define BP_FLAG_MODNAME_TESTNAME6            (1 << 6)
#define BP_FLAG_MODNAME_TESTNAME7            (1 << 7)
#define BP_FLAG_MODNAME_TESTNAME8            (1 << 8)

/* 
** Device-specific definitions 
*/
typedef enum
{
   BP_VD_NONE = -1,
   BP_VD_IDECT1,  /* Do not move this around, otherwise rebuild dect_driver.bin */
   BP_VD_EDECT1,  
   BP_VD_SILABS_3050,
   BP_VD_SILABS_3215,
   BP_VD_SILABS_3216,
   BP_VD_SILABS_3217,
   BP_VD_SILABS_32176,
   BP_VD_SILABS_32178,
   BP_VD_SILABS_3226,
   BP_VD_SILABS_32261,
   BP_VD_SILABS_32267,
   BP_VD_ZARLINK_88010,
   BP_VD_ZARLINK_88221,
   BP_VD_ZARLINK_88276,
   BP_VD_ZARLINK_88506,
   BP_VD_ZARLINK_89010,
   BP_VD_ZARLINK_89116,
   BP_VD_ZARLINK_89316,
   BP_VD_ZARLINK_9530,
   BP_VD_ZARLINK_89136,
   BP_VD_ZARLINK_89336,
   BP_VD_MAX,
} BP_VOICE_DEVICE_TYPE;


/* 
** Channel-specific definitions 
*/

typedef enum
{
   BP_VOICE_CHANNEL_ACTIVE,
   BP_VOICE_CHANNEL_INACTIVE,
} BP_VOICE_CHANNEL_STATUS;

typedef enum
{
   BP_VCTYPE_NONE = -1,
   BP_VCTYPE_SLIC,
   BP_VCTYPE_DAA,
   BP_VCTYPE_DECT
} BP_VOICE_CHANNEL_TYPE;

typedef enum
{
   BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
   BP_VOICE_CHANNEL_SAMPLE_SIZE_8BITS,
} BP_VOICE_CHANNEL_SAMPLE_SIZE;

typedef enum
{
   BP_VOICE_CHANNEL_NARROWBAND,
   BP_VOICE_CHANNEL_WIDEBAND,
} BP_VOICE_CHANNEL_FREQRANGE;


typedef enum
{
   BP_VOICE_CHANNEL_ENDIAN_BIG,
   BP_VOICE_CHANNEL_ENDIAN_LITTLE,
} BP_VOICE_CHANNEL_ENDIAN_MODE;

typedef enum
{
   BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
   BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
   BP_VOICE_CHANNEL_PCMCOMP_MODE_ULAW,
} BP_VOICE_CHANNEL_PCMCOMP_MODE;


typedef struct
{
   unsigned int  status;        /* active/inactive */
   unsigned int  type;          /* SLIC/DAA/DECT */
   unsigned int  pcmCompMode;   /* u-law/a-law (applicable for 8-bit samples) */
   unsigned int  freqRange;     /* narrowband/wideband */
   unsigned int  sampleSize;    /* 8-bit / 16-bit */
   unsigned int  endianMode;    /* big/little */
   unsigned int  rxTimeslot;    /* Receive timeslot for the channel */
   unsigned int  txTimeslot;    /* Sending timeslot for the channel */

} BP_VOICE_CHANNEL;

/* TODO: This structure could possibly be turned into union if needed */
typedef struct
{
   int                  spiDevId;               /* SPI device id */
   unsigned short       spiGpio;                /* SPI GPIO (if used for SPI control) */
} BP_VOICE_SPI_CONTROL;

typedef struct
{
   unsigned short       relayGpio[BP_MAX_RELAY_PINS];
} BP_PSTN_RELAY_CONTROL;

typedef struct
{
   unsigned short       dectUartGpioTx;
   unsigned short       dectUartGpioRx;
} BP_DECT_UART_CONTROL;

typedef struct
{
   unsigned int         voiceDeviceType;        /* Specific type of device (Le88276, Si32176, etc.) */
   BP_VOICE_SPI_CONTROL spiCtrl;                /* SPI control through dedicated SPI pin or GPIO */
   int                  requiresReset;          /* Does the device requires reset (through GPIO) */
   unsigned short       resetGpio;              /* Reset GPIO */
   BP_VOICE_CHANNEL     channel[BP_MAX_CHANNELS_PER_DEVICE];   /* Device channels */

} BP_VOICE_DEVICE;


/*
** Main structure for defining the board parameters and used by boardHal
** for proper initialization of the DSP and devices (SLACs, DECT, etc.)
*/
typedef struct VOICE_BOARD_PARMS
{
   char                    szBoardId[BP_BOARD_ID_LEN];   /* board id string */
   unsigned int            numFxsLines;            /* Number of FXS lines in the system */
   unsigned int            numFxoLines;            /* Number of FXO lines in the system */
   unsigned int            numDectLines;           /* Number of DECT lines in the system */
   unsigned int            numFailoverRelayPins;   /* Number of GPIO pins controling PSTN failover relays */
   BP_VOICE_DEVICE         voiceDevice[BP_MAX_VOICE_DEVICES];  /* Voice devices in the system */
   BP_PSTN_RELAY_CONTROL   pstnRelayCtrl;          /* Control for PSTN failover relays */
   BP_DECT_UART_CONTROL    dectUartControl;        /* Control for external DECT UART */
   unsigned int            flags;                  /* General-purpose flags */
} VOICE_BOARD_PARMS, *PVOICE_BOARD_PARMS;

/* --- End of voice-specific structures and enums --- */

int BpGetVoiceParms( char* pszBoardId, VOICE_BOARD_PARMS* voiceParms );
int BpSetVoiceBoardId( char *pszBoardId );
int BpGetVoiceBoardId( char *pszBoardId );
int BpGetVoiceBoardIds( char *pszBoardIds, int nBoardIdsSize );

/* Variable externs */

#if defined(_BCM96328_) || defined(CONFIG_BCM96328)

extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI3226;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI32267;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88276_NTR;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890HV_Partial;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890HV;

extern VOICE_BOARD_PARMS voiceBoard_96328AVNGR_SI32176;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNGR_LE89116;
extern VOICE_BOARD_PARMS voiceBoard_96328AVNGR_SI3217X;

#endif

#if defined(_BCM96362_) || defined(CONFIG_BCM96362)

extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_NOSLIC;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE89116;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE89316;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI32176;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI32178;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI3226;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE88506; 

extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32176;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32178;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3217X_NOFXO;   
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE89116;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE89316;     

extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3226;  
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32261;      
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32267;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890HVP;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890HV;

extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_SI3226;

#endif

#if defined(_BCM96368_) || defined(CONFIG_BCM96368)

extern VOICE_BOARD_PARMS voiceBoard_96368MVWG;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_LE89116;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_LE89316;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_SI32176;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_SI32178;
extern VOICE_BOARD_PARMS voiceBoard_96368MBG_SI3226;

extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3217X;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI32176;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI32178;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3217X_NOFXO;   
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE89116;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE89316;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3226; 
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890HV_Partial;
extern VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890HV;

#endif


#if defined(_BCM96816_) || defined(CONFIG_BCM96816)

extern VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE88276;
extern VOICE_BOARD_PARMS voiceBoard_96816PVWM_SI3226;
extern VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE88506;
extern VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE9530;

#endif

#ifdef __cplusplus
}
#endif

#endif /* _BOARDPARMS_VOICE_H */

