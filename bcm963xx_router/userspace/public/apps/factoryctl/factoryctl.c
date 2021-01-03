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
#include "cms_boardioctl.h"
#define SUPPORT_RESET_SET 1

void usage(UINT32 exitCode)
{
#if defined(SUPPORT_RESET_SET)
   char *str="[reset|set|get]";
#else
   char *str="[get]";
#endif
   printf("usage: factoryctl [serialnum|fwversion|wpakey|wpspin] %s [value]\n",str);
   printf("[serialnum|fwversion|wpakey|wpspin] must be specified.\n");
   printf("%s must be specified.\n",str);
   printf("[value] must be specified when set it.\n");
   exit(exitCode);
}


void processFactoryFWVersion(const char *action,const char *value,int flag)
{
   char ffwversion[48]={0};
   if (!cmsUtl_strcmp(action, "get"))
   {
      devCtl_boardIoctl(BOARD_IOCTL_GET_FW_VERSION, 0, ffwversion, sizeof(ffwversion), flag, NULL);
      printf("Return Value = %s\n",ffwversion);
   }
#if defined(SUPPORT_RESET_SET)
   else if (!cmsUtl_strcmp(action, "reset"))
   {
      devCtl_boardIoctl(BOARD_IOCTL_RESET_FW_VERSION, 0, ffwversion, sizeof(ffwversion), flag, NULL);
      printf("Reset is successful!\n");
   }
   else if (!cmsUtl_strcmp(action, "set"))
   {
      if(value!=NULL && strlen(value) > 0){
         strncpy(&ffwversion[0],value,48-1);
         devCtl_boardIoctl(BOARD_IOCTL_SET_FW_VERSION, 0, ffwversion, sizeof(ffwversion), flag, NULL);
         printf("Set is successful!\n");
      }else {
	usage(1);
      }
   }
#endif
   else
   {
      usage(1);
   }

}


int main(int argc, char *argv[])
{

   if (argc < 3)
   {
      usage(1);
   }

   if (!cmsUtl_strcmp(argv[1], "fwversion"))
   {
      processFactoryFWVersion(argv[2],argv[3],0);
   }else if(!cmsUtl_strcmp(argv[1], "serialnum"))
   {
      processFactoryFWVersion(argv[2],argv[3],1);
   }else if(!cmsUtl_strcmp(argv[1], "wpspin"))
   {
      processFactoryFWVersion(argv[2],argv[3],2);
   }else if(!cmsUtl_strcmp(argv[1], "wpakey"))
   {
      processFactoryFWVersion(argv[2],argv[3],3);
   }else
   {
      usage(1);
   }

   return 0;
}
