/***********************************************************************
 *
 *  Copyright (c) 2011  Actiontec Electronics Inc.
 *  All Rights Reserved
 *
 *  This file is to store all functions that developed by Actiontec Electronics
 *  in addition to routines provided by Broadcom. All additional routines that 
 *  are missing from dal_dsl.h file will locate in this file. 
 *
 ************************************************************************/

#ifndef __AEI_WP_H__
#define __AEI_WP_H__
#include "cms_core.h"
#include "cms_msg.h"

#define WIRELESS_SETTINGS_XML_FILENAME   "wirelesssettings.xml"
#define WIRELESSSYNC_SUCCESSFUL_FILENAME "wirelesssync_successful.xml"
#define WIRELESSSYNC_FAILURE_FILENAME    "wirelesssync_Failure.xml"
#define FIRMWARE_LINKS_FILENAME          "firmwarelinks.xml"

#define WIRELESSSYNC   "/var/"WIRELESS_SETTINGS_XML_FILENAME
#define FIRMWARELINK   "/var/"FIRMWARE_LINKS_FILENAME
#define MAX_WP_HOSTS   32
#include "rut_main.h"

UBOOL8  AEI_isWPClient(int portnum);
UBOOL8  AEI_is_WP_client(UINT32 pcaddr,saved_lease *lease);
UBOOL8  AEI_is_Enabled_WP();

typedef enum {
    AEI_WP_WIRLESSCHANGING_NOTIFY=0,
    AEI_WP_FIRMWAREUPGRADE_NOTIFY
} AEI_WP_NOTIFY_TYPE;

typedef enum {
    AEI_WP_INIT=0,
    AEI_WP_SYNCING,
    AEI_WP_SUCCESSFUL,
    AEI_WP_FAIL
} AEI_WP_STATE_TYPE;

typedef enum {
    AEI_WP_WIRELESS_SETTINGS_XML_FILENAME=0,
    AEI_WP_WIRELESSSYNC_SUCCESSFUL_FILENAME,
    AEI_WP_WIRELESSSYNC_FAILURE_FILENAME,
    AEI_WP_FIRMWARE_LINKS_FILENAME,
    AEI_WP_NOT_MATCH_FILE
} AEI_WP_FILENAME_FLAG;

void  AEI_set_WP_state(UINT32 pcaddr, AEI_WP_STATE_TYPE state,saved_lease *lease);

void AEI_wirelessChanging_notify();

void AEI_firmwareupgrade_notify();

void AEI_generate_FirmwareUpgrade(char *productType,char *version,char *link);

#define AEI_WP_WIRLESSCHANGING_NOTIFY_FILENAME  "wirelesschanging_notify.html"
#define AEI_WP_FIRMWAREUPGRADE_NOTIFY_FILENAME  "firmwareupgrade_notify.html"

#endif
