/******************************************************************************/
/*                       Copyright 2001 Broadcom Corporation                  */
/*                                                                            */
/* This material is the confidential trade secret and proprietary information */
/* of Broadcom Corp.  It may not be reproduced, used, sold or transferred to  */
/* any third party without the prior written consent of Broadcom Corp.        */
/* All rights reserved.                                                       */
/******************************************************************************/
/*   FileName        : bcm_bigEndianByteSwap.c                                */
/*   Purpose         : big endian swap bytes functions                        */
/*   Limitations     : Max object size is 8 bytes                             */
/*   Author          : Emmanuel Hanssens                                      */
/*   Creation Date   : 13-Sep-2001                                            */
/*   History         : Compiled by CVS                                        */
/******************************************************************************/
/*   $Id: bcm_bigEndianByteSwap.c,v 1.1.1.1 2010/06/14 22:47:41 tliu Exp $  */
/******************************************************************************/

#include "bcm_bigEndianByteSwap.h"
//#include "bcm_errorHandler.h"

#ifdef BCM_BIG_ENDIAN
/* This function works on an array of byte, according to a layout information
 * which delimits the different fields (short, int, long) to be processed */
void bigEndianByteSwap(uint8 *data, typeLayout *layout)
{
  uint8 tmpBytes[8];            /* max 64 bits supported */
  int i = 0, j, k;

  while (layout[i].occurence != 0) 
  {
    if (layout[i].size > 8) {
      //FATAL_ERROR("Does not support object sizes larger than 8 bytes");
      printk("%s: Does not support object sizes larger than 8 bytes", __FUNCTION__);	  
      return;
    }
    else

    if (layout[i].size == 1)
      data+=layout[i].occurence;

    else
    {
      for (j=0; j<layout[i].occurence; j++) 
      {
        for (k=0; k<layout[i].size; k++) 
          tmpBytes[k] = data[k];
        
        for (k=0; k<layout[i].size; k++) 
          data[k] = tmpBytes[layout[i].size-k-1];

        data+=layout[i].size;
      }
    }
    i++;
  }
}
#endif

