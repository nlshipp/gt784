/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
<:license-private
 *
 ************************************************************************/


#include "cms.h"
#include "cms_util.h"


void usage(UINT32 exitCode)
{
   printf("usage: ledctl [WAN] [on|off|red]\n");
   printf("    WAN is the only led that is controllable by this app right now.\n");
   printf("    WAN must be specified.\n");
   printf("    one of on, off, or red must be specified.\n");

   exit(exitCode);
}


void processWanLed(const char *state)
{
   if (!cmsUtl_strcmp(state, "on"))
   {
      cmsLed_setWanConnected();
   }
   else if (!cmsUtl_strcmp(state, "off"))
   {
      cmsLed_setWanDisconnected();
   }
   else if (!cmsUtl_strcmp(state, "red"))
   {
      cmsLed_setWanFailed();
   }
   else
   {
      usage(1);
   }

}


int main(int argc, char *argv[])
{

   if (argc != 3)
   {
      usage(1);
   }

   if (!cmsUtl_strcmp(argv[1], "WAN"))
   {
      processWanLed(argv[2]);
   }
   else
   {
      usage(1);
   }

   return 0;
}
