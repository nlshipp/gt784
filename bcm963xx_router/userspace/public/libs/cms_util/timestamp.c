/***********************************************************************
//
//  Copyright (c) 2006  Broadcom Corporation
//  All Rights Reserved
//  No portions of this material may be reproduced in any form without the
//  written permission of:
//          Broadcom Corporation
//          16215 Alton Parkway
//          Irvine, California 92619
//  All information contained in this document is Broadcom Corporation
//  company private, proprietary, and trade secret.
//
************************************************************************/


#include "cms.h"
#include "cms_tms.h"
#include "oal.h"

void cmsTms_get(CmsTimestamp *tms)
{
   oalTms_get(tms);
}

void cmsTms_delta(const CmsTimestamp *newTms,
                  const CmsTimestamp *oldTms,
                  CmsTimestamp *deltaTms)
{
   if (newTms->sec >= oldTms->sec)
   {
      if (newTms->nsec >= oldTms->nsec)
      {
         /* no roll-over in the sec and nsec fields, straight subtract */
         deltaTms->nsec = newTms->nsec - oldTms->nsec;
         deltaTms->sec = newTms->sec - oldTms->sec;
      }
      else
      {
         /* no roll-over in the sec field, but roll-over in nsec field */
         deltaTms->nsec = (NSECS_IN_SEC - oldTms->nsec) + newTms->nsec;
         deltaTms->sec = newTms->sec - oldTms->sec - 1;
      }
   }
   else
   {
      if (newTms->nsec >= oldTms->nsec)
      {
         /* roll-over in the sec field, but no roll-over in the nsec field */
         deltaTms->nsec = newTms->nsec - oldTms->nsec;
         deltaTms->sec = (MAX_UINT32 - oldTms->sec) + newTms->sec + 1; /* +1 to account for time spent during 0 sec */
      }
      else
      {
         /* roll-over in the sec and nsec fields */
         deltaTms->nsec = (NSECS_IN_SEC - oldTms->nsec) + newTms->nsec;
         deltaTms->sec = (MAX_UINT32 - oldTms->sec) + newTms->sec;
      }
   }
}

UINT32 cmsTms_deltaInMilliSeconds(const CmsTimestamp *newTms,
                                  const CmsTimestamp *oldTms)
{
   CmsTimestamp deltaTms;
   UINT32 ms;

   cmsTms_delta(newTms, oldTms, &deltaTms);

   if (deltaTms.sec > MAX_UINT32 / MSECS_IN_SEC)
   {
      /* the delta seconds is larger than the UINT32 return value, so return max value */
      ms = MAX_UINT32;
   }
   else
   {
      ms = deltaTms.sec * MSECS_IN_SEC;

      if ((MAX_UINT32 - ms) < (deltaTms.nsec / NSECS_IN_MSEC))
      {
         /* overflow will occur when adding the nsec, return max value */
         ms = MAX_UINT32;
      }
      else
      {
         ms += deltaTms.nsec / NSECS_IN_MSEC;
      }
   }

   return ms;
}


void cmsTms_addMilliSeconds(CmsTimestamp *tms, UINT32 ms)
{
   UINT32 addSeconds;
   UINT32 addNano;

   addSeconds = ms / MSECS_IN_SEC;
   addNano = (ms % MSECS_IN_SEC) * NSECS_IN_MSEC;

   tms->sec += addSeconds;
   tms->nsec += addNano;

   /* check for carry-over in nsec field */
   if (tms->nsec > NSECS_IN_SEC)
   {
      /* we can't have carried over by more than 1 second */
      tms->sec++;
      tms->nsec -= NSECS_IN_SEC;
   }

   return;
}


CmsRet cmsTms_getXSIDateTime(UINT32 t, char *buf, UINT32 bufLen)
{
   return (oalTms_getXSIDateTime(t, buf, bufLen));
}

#if defined(AEI_VDSL_CUSTOMER_NCS)
CmsRet cmsTms_getGUIDateTime(char *buf, UINT32 bufLen)
{
   return (oalTms_getGUIDateTime(buf, bufLen));
}
#endif

#ifdef DMP_PERIODICSTATSBASE_1
/* xsiTime1 minus xsiTime2, return the result in seconds */
int cmsTms_subXSIDateTime(const char *xsiTime1, const char *xsiTime2, UINT32 *result)
{
    struct tm tm1, tm2;
    time_t sec1, sec2;

    if (xsiTime1 == NULL || xsiTime2 == NULL || result == NULL)
        return -1;

    if (strptime(xsiTime1, "%Y-%m-%dT%H:%M:%S", &tm1) == NULL ||
        strptime(xsiTime2, "%Y-%m-%dT%H:%M:%S", &tm2) == NULL )
        return -1;

    sec1 = mktime(&tm1);
    sec2 = mktime(&tm2);

    *result = sec1 - sec2;

    return 0;
}
#endif /* DMP_PERIODICSTATSBASE_1 */

