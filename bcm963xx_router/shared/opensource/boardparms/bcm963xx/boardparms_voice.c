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
* File Name  : boardparms_voice.c
*
* Description: This file contains the implementation for the BCM63xx board
*              parameter voice access functions.
*
***************************************************************************/

/* ---- Include Files ---------------------------------------------------- */

#include "boardparms_voice.h"

/* ---- Public Variables ------------------------------------------------- */

/* ---- Private Constants and Types -------------------------------------- */

/* Always end the device list in VOICE_BOARD_PARMS with this macro */
#define BP_NULL_DEVICE_MACRO     \
{                                \
   BP_VD_NONE,                   \
   {  0, BP_NOT_DEFINED },       \
   0,                            \
   BP_NOT_DEFINED,               \
   {                             \
      { BP_VOICE_CHANNEL_INACTIVE, BP_VCTYPE_NONE, BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW, BP_VOICE_CHANNEL_NARROWBAND, BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS, BP_VOICE_CHANNEL_ENDIAN_BIG, BP_TIMESLOT_INVALID, BP_TIMESLOT_INVALID }, \
      { BP_VOICE_CHANNEL_INACTIVE, BP_VCTYPE_NONE, BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW, BP_VOICE_CHANNEL_NARROWBAND, BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS, BP_VOICE_CHANNEL_ENDIAN_BIG, BP_TIMESLOT_INVALID, BP_TIMESLOT_INVALID }, \
   }                             \
}

#define BP_NULL_CHANNEL_MACRO             \
{  BP_VOICE_CHANNEL_INACTIVE,             \
   BP_VCTYPE_NONE,                        \
   BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,    \
   BP_VOICE_CHANNEL_NARROWBAND,           \
   BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,   \
   BP_VOICE_CHANNEL_ENDIAN_BIG,           \
   BP_TIMESLOT_INVALID,                   \
   BP_TIMESLOT_INVALID                    \
},


#if defined(_BCM96328_) || defined(CONFIG_BCM96328)

VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88276 =
{
   VOICECFG_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI3226 =
{
   VOICECFG_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890 =
{
   VOICECFG_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_30_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI3217X =
{
   VOICECFG_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_30_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88506 =
{
   VOICECFG_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNG_SI32267 =
{
   VOICECFG_6328AVNG_SI32267_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32267,

         /* SPI control */
         {  
            /* SPI dev id */
            6,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( BP_FLAG_ISI_SUPPORT )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNG_LE88276_NTR =
{
   VOICECFG_6328AVNG_LE88276_NTR_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890HV_Partial =
{
   VOICECFG_6328AVNG_VE890HV_PARTIAL_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,                /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Reset pin */
         BP_GPIO_30_AL,    

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNG_VE890HV =
{
   VOICECFG_6328AVNG_VE890HV_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89136,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
             }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,                /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Reset pin */
         BP_GPIO_30_AL,    

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_31_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNGR_SI32176 =
{
   VOICECFG_6328AVNGR_SI32176_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNGR_LE89116 =
{
   VOICECFG_6328AVNGR_LE89116_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96328AVNGR_SI3217X =
{
   VOICECFG_6328AVNGR_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,       /* Reset line shared to one line only for AVNGR board */

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

static PVOICE_BOARD_PARMS g_VoiceBoardParms[] = 
{
   &voiceBoard_96328AVNG_LE88276,
   &voiceBoard_96328AVNG_SI3226,
   &voiceBoard_96328AVNG_VE890,
   &voiceBoard_96328AVNG_SI3217X,
   &voiceBoard_96328AVNG_LE88506,
#ifdef SI32267ENABLE
   &voiceBoard_96328AVNG_SI32267,
#endif
   &voiceBoard_96328AVNG_LE88276_NTR,
   &voiceBoard_96328AVNG_VE890HV_Partial,
   &voiceBoard_96328AVNG_VE890HV,

   &voiceBoard_96328AVNGR_SI32176,
   &voiceBoard_96328AVNGR_LE89116,
   &voiceBoard_96328AVNGR_SI3217X,
   0
};


#endif

#if defined(_BCM96362_) || defined(CONFIG_BCM96362)

VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_NOSLIC =
{
   VOICECFG_6362ADVNGP5_NOSLIC_STR,   /* szBoardId */
   0,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_VE890 =
{
   VOICECFG_6362ADVNGP5_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE89116 =
{
   VOICECFG_6362ADVNGP5_LE89116_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE89316 =
{
   VOICECFG_6362ADVNGP5_LE89316_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI3217X =
{
   VOICECFG_6362ADVNGP5_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 32176 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI32176 =
{
   VOICECFG_6362ADVNGP5_SI32176_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI32178 =
{
   VOICECFG_6362ADVNGP5_SI32178_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE88276 =
{
   VOICECFG_6362ADVNGP5_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_SI3226 =
{
   VOICECFG_6362ADVNGP5_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96362ADVNGP5_LE88506 =
{
   VOICECFG_6362ADVNGP5_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_6_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_0_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_10_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3217X =
{
   VOICECFG_6362ADVNGR_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32176 =
{
   VOICECFG_6362ADVNGR_SI32176_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32178 =
{
   VOICECFG_6362ADVNGR_SI32178_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3217X_NOFXO =
{
   VOICECFG_6362ADVNGR_SI3217X_NOFXO_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 32176 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890 =
{
   VOICECFG_6362ADVNGR_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               6,
               6
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE89116 =
{
   VOICECFG_6362ADVNGR_LE89116_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE89316 =
{
   VOICECFG_6362ADVNGR_LE89316_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE88506 =
{
   VOICECFG_6362ADVNGR_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_LE88276 =
{
   VOICECFG_6362ADVNGR_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI3226 =
{
   VOICECFG_6362ADVNGR_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },
      
      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32261 =
{
   VOICECFG_6362ADVNGR_SI32261_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },
      
      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32261,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_SI32267 =
{
   VOICECFG_6362ADVNGR_SI32267_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },
      
      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32267,

         /* SPI control */
         {  
            /* SPI dev id */
            3,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( BP_FLAG_ISI_SUPPORT )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890HVP =
{
   VOICECFG_6362ADVNGR_VE890HV_PARTIAL_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,               /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Reset pin */
         BP_GPIO_28_AL,   /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               6,
               6
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNgr_VE890HV =
{
   VOICECFG_6362ADVNGR_VE890HV_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89136,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               6,
               6
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_SI3217X =
{
   VOICECFG_6362ADVNGR2_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_44_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_VE890 =
{
   VOICECFG_6362ADVNGR2_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice3 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_31_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               6,
               6
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_44_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_LE88506 =
{
   VOICECFG_6362ADVNGR2_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_44_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_LE88276 =
{
   VOICECFG_6362ADVNGR2_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_44_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96362ADVNGR2_SI3226 =
{
   VOICECFG_6362ADVNGR2_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   4,             /* numDectLines */
   1,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_29_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },
      
      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_28_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_44_AH,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

static PVOICE_BOARD_PARMS g_VoiceBoardParms[] = 
{
   &voiceBoard_96362ADVNGP5_NOSLIC,
//   &voiceBoard_96362ADVNGP5_VE890,
//   &voiceBoard_96362ADVNGP5_LE89116,  
//   &voiceBoard_96362ADVNGP5_LE89316,  
//   &voiceBoard_96362ADVNGP5_SI3217X,
//   &voiceBoard_96362ADVNGP5_SI32176,  
//   &voiceBoard_96362ADVNGP5_SI32178,  
//   &voiceBoard_96362ADVNGP5_LE88276,
//   &voiceBoard_96362ADVNGP5_SI3226,
//   &voiceBoard_96362ADVNGP5_LE88506,
   &voiceBoard_96362ADVNgr_SI3217X,
   &voiceBoard_96362ADVNgr_SI32176,
   &voiceBoard_96362ADVNgr_SI32178,
   &voiceBoard_96362ADVNgr_SI3217X_NOFXO,   
   &voiceBoard_96362ADVNgr_VE890,
   &voiceBoard_96362ADVNgr_LE89116,
   &voiceBoard_96362ADVNgr_LE89316,
   &voiceBoard_96362ADVNgr_LE88506,
   &voiceBoard_96362ADVNgr_LE88276,
   &voiceBoard_96362ADVNgr_SI3226,   
#ifdef SI32261ENABLE
   &voiceBoard_96362ADVNgr_SI32261,
#endif
#ifdef SI32267ENABLE
   &voiceBoard_96362ADVNgr_SI32267,
#endif
   &voiceBoard_96362ADVNgr_VE890HVP,
   &voiceBoard_96362ADVNgr_VE890HV,
   &voiceBoard_96362ADVNGR2_SI3217X,
   &voiceBoard_96362ADVNGR2_VE890,
   &voiceBoard_96362ADVNGR2_LE88506,
   &voiceBoard_96362ADVNGR2_LE88276,
   &voiceBoard_96362ADVNGR2_SI3226,        
   0
};

#endif

#if defined(_BCM96368_) || defined(CONFIG_BCM96368)

VOICE_BOARD_PARMS voiceBoard_96368MVWG =
{
   VOICECFG_6368MVWG_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   2,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_10_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               4,
               4
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            3,
            /* SPI GPIO */
            BP_GPIO_29_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,

         /* Reset pin */
         BP_GPIO_10_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               5,
               5
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               6,
               6
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_GPIO_3_AH,  BP_GPIO_13_AH } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_VE890 =
{
   VOICECFG_6368MBG_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

{
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_LE89116 =
{
   VOICECFG_6368MBG_LE89116_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_LE89316 =
{
   VOICECFG_6368MBG_LE89316_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_SI3217X =
{
   VOICECFG_6368MBG_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_SI32176 =
{
   VOICECFG_6368MBG_SI32176_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_SI32178 =
{
   VOICECFG_6368MBG_SI32178_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1},
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_LE88276 =
{
   VOICECFG_6368MBG_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_LE88506 =
{
   VOICECFG_6368MBG_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MBG_SI3226 =
{
   VOICECFG_6368MBG_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_3_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3217X =
{
   VOICECFG_6368MVNGR_SI3217X_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI32176 =
{
   VOICECFG_6368MVNGR_SI32176_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI32178 =
{
   VOICECFG_6368MVNGR_SI32178_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3217X_NOFXO =
{
   VOICECFG_6368MVNGR_SI3217X_NOFXO_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32176,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_SILABS_32178,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890 =
{
   VOICECFG_6368MVNGR_VE890_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE89116 =
{
   VOICECFG_6368MVNGR_LE89116_STR,   /* szBoardId */
   1,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE89316 =
{
   VOICECFG_6368MVNGR_LE89316_STR,   /* szBoardId */
   1,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89316,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE88506 =
{
   VOICECFG_6368MVNGR_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_LE88276 =
{
   VOICECFG_6368MVNGR_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_SI3226 =
{
   VOICECFG_6368MVNGR_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890HV_Partial =
{
   VOICECFG_6368MVNGR_VE890HV_PARTIAL_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89116,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,                /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Reset pin */
         BP_GPIO_16_AL,    /* This setting is specific to Zarlink reference card Le71HR8923G for shared 1 reset line. */

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

VOICE_BOARD_PARMS voiceBoard_96368MVNgr_VE890HV =
{
   VOICECFG_6368MVNGR_VE890HV_STR,   /* szBoardId */
   2,             /* numFxsLines */
   1,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_IDECT1,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID
            },
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_DECT,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_NONE,
               BP_VOICE_CHANNEL_WIDEBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice1 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89136,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_16_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* There is no second channel on 89116 so mark it as inactive */
            {  BP_VOICE_CHANNEL_INACTIVE,
               BP_VCTYPE_NONE,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               BP_TIMESLOT_INVALID,
               BP_TIMESLOT_INVALID},
         }
      },

      /* voiceDevice2 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_89336,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_17_AL,

         /* Channel description */
         {
            /* Channel 0 on device 0 */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_DAA,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               2,
               2
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};

static PVOICE_BOARD_PARMS g_VoiceBoardParms[] = 
{
   &voiceBoard_96368MVWG,
   &voiceBoard_96368MBG_LE88276,
   &voiceBoard_96368MBG_LE88506,
   &voiceBoard_96368MBG_VE890,
   &voiceBoard_96368MBG_LE89116,
   &voiceBoard_96368MBG_LE89316,
   &voiceBoard_96368MBG_SI3217X,
   &voiceBoard_96368MBG_SI32176,
   &voiceBoard_96368MBG_SI32178,
   &voiceBoard_96368MBG_SI3226,
   &voiceBoard_96368MVNgr_SI3217X,
   &voiceBoard_96368MVNgr_SI32176,
   &voiceBoard_96368MVNgr_SI32178,
   &voiceBoard_96368MVNgr_SI3217X_NOFXO,   
   &voiceBoard_96368MVNgr_VE890,
//   &voiceBoard_96368MVNgr_LE89116,          /* Temporarily remove */
//   &voiceBoard_96368MVNgr_LE89316,          /* Temporarily remove */
   &voiceBoard_96368MVNgr_VE890HV_Partial,
   &voiceBoard_96368MVNgr_VE890HV,
   &voiceBoard_96368MVNgr_LE88506,
   &voiceBoard_96368MVNgr_LE88276,
   &voiceBoard_96368MVNgr_SI3226, 
   0
};

#endif

#if defined(_BCM96816_) || defined(CONFIG_BCM96816)

VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE9530 =
{
   VOICECFG_LE9530_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_9530,

         /* SPI control */
         {  
            /* SPI dev id */
            0,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         0,

         /* Reset pin */
         BP_NOT_DEFINED,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( BP_FLAG_DSP_APMHAL_ENABLE )

};


VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE88276 =
{
   VOICECFG_LE88276_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88276,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96816PVWM_SI3226 =
{
   VOICECFG_SI3226_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_SILABS_3226,

         /* SPI control */
         {  
            /* SPI dev id */
            2,
            /* SPI GPIO */
            BP_GPIO_28_AL,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};


VOICE_BOARD_PARMS voiceBoard_96816PVWM_LE88506 =
{
   VOICECFG_LE88506_STR,   /* szBoardId */
   2,             /* numFxsLines */
   0,             /* numFxoLines */
   0,             /* numDectLines */
   0,             /* numFailoverRelayPins */

   {
      /* voiceDevice0 parameters */
      {
         /* Device type */
         BP_VD_ZARLINK_88506,

         /* SPI control */
         {  
            /* SPI dev id */
            1,
            /* SPI GPIO */
            BP_NOT_DEFINED,
         },

         /* Reset required (1 for yes, 0 for no) */
         1,

         /* Reset pin */
         BP_GPIO_14_AL,

         /* Channel description */
         {
            /* Channel 0 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               0,
               0
            },
            /* Channel 1 on device */
            {  BP_VOICE_CHANNEL_ACTIVE,
               BP_VCTYPE_SLIC,
               BP_VOICE_CHANNEL_PCMCOMP_MODE_ALAW,
               BP_VOICE_CHANNEL_NARROWBAND,
               BP_VOICE_CHANNEL_SAMPLE_SIZE_16BITS,
               BP_VOICE_CHANNEL_ENDIAN_BIG,
               1,
               1
            },
         }
      },

      /* Always end the device list with BP_NULL_DEVICE_MACRO */
      BP_NULL_DEVICE_MACRO,
   },

   /* Relay control pins */
   { { BP_NOT_DEFINED,  BP_NOT_DEFINED } },

   /* DECT UART control pins */
   { BP_NOT_DEFINED,  BP_NOT_DEFINED },

   /* General-purpose flags */
   ( 0 )

};



static PVOICE_BOARD_PARMS g_VoiceBoardParms[] =
{  &voiceBoard_96816PVWM_LE88276,
   &voiceBoard_96816PVWM_SI3226,
   &voiceBoard_96816PVWM_LE88506,
   &voiceBoard_96816PVWM_LE9530,
   0
};

#endif


static PVOICE_BOARD_PARMS g_pCurrentVoiceBp = 0;

static void bpmemcpy( void* dstptr, const void* srcptr, int size );
static void bpmemcpy( void* dstptr, const void* srcptr, int size )
{
   char* dstp = dstptr;
   const char* srcp = srcptr;
   int i;
   for( i=0; i < size; i++ )
   {
      *dstp++ = *srcp++;
   }
} 

int BpGetVoiceParms( char* pszBoardId, VOICE_BOARD_PARMS* voiceParms )
{
   int nRet = BP_BOARD_ID_NOT_FOUND;
   PVOICE_BOARD_PARMS *ppBp;

   for( ppBp = g_VoiceBoardParms; *ppBp; ppBp++ )
   {
     if( !bpstrcmp((*ppBp)->szBoardId, pszBoardId) )
     {
         g_pCurrentVoiceBp = *ppBp;
         bpmemcpy( voiceParms, g_pCurrentVoiceBp, sizeof(VOICE_BOARD_PARMS) );
         nRet = BP_SUCCESS;
         break;
     }
   }

   return( nRet );
}


/**************************************************************************
* Name       : BpSetVoiceBoardId
*
* Description: This function find the BOARD_PARAMETERS structure for the
*              specified board id string and assigns it to a global, static
*              variable.
*
* Parameters : [IN] pszBoardId - Board id string that is saved into NVRAM.
*
* Returns    : BP_SUCCESS - Success, value is returned.
*              BP_BOARD_ID_NOT_FOUND - Error, board id input string does not
*                  have a board parameters configuration record.
***************************************************************************/
int BpSetVoiceBoardId( char *pszBoardId )
{
    int nRet = BP_BOARD_ID_NOT_FOUND;
    PVOICE_BOARD_PARMS *ppBp;

    for( ppBp = g_VoiceBoardParms; *ppBp; ppBp++ )
    {
        if( !bpstrcmp((*ppBp)->szBoardId, pszBoardId) )
        {
            g_pCurrentVoiceBp = *ppBp;
            nRet = BP_SUCCESS;
            break;
        }
    }

    return( nRet );
} /* BpSetVoiceBoardId */


/**************************************************************************
* Name       : BpGetVoiceBoardId
*
* Description: This function returns the current board id strings.
*
* Parameters : [OUT] pszBoardIds - Address of a buffer that the board id
*                  string is returned in.
*
* Returns    : BP_SUCCESS - Success, value is returned.
***************************************************************************/

int BpGetVoiceBoardId( char *pszBoardId )
{
    int i;

    if (g_pCurrentVoiceBp == 0)
        return -1;

    for (i = 0; i < BP_BOARD_ID_LEN; i++)
        pszBoardId[i] = g_pCurrentVoiceBp->szBoardId[i];

    return 0;
}


/**************************************************************************
* Name       : BpGetVoiceBoardIds
*
* Description: This function returns all of the supported voice board id strings.
*
* Parameters : [OUT] pszBoardIds - Address of a buffer that the board id
*                  strings are returned in.  Each id starts at BP_BOARD_ID_LEN
*                  boundary.
*              [IN] nBoardIdsSize - Number of BP_BOARD_ID_LEN elements that
*                  were allocated in pszBoardIds.
*
* Returns    : Number of board id strings returned.
***************************************************************************/
int BpGetVoiceBoardIds( char *pszBoardIds, int nBoardIdsSize )
{
    PVOICE_BOARD_PARMS *ppBp;
    int i;
    char *src;
    char *dest;

    for( i = 0, ppBp = g_VoiceBoardParms; *ppBp && nBoardIdsSize;
        i++, ppBp++, nBoardIdsSize--, pszBoardIds += BP_BOARD_ID_LEN )
    {
        dest = pszBoardIds;
        src = (*ppBp)->szBoardId;
        while( *src )
            *dest++ = *src++;
        *dest = '\0';
    }

    return( i );
} /* BpGetVoiceBoardIds */
