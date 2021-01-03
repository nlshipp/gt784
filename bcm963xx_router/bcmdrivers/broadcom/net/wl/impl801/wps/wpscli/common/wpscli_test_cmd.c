/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_test_cmd.c,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: WPSCLI library test console program
 *
 */
#include <stdio.h>
#include <string.h>
#include <wpscli_api.h>
#include <ctype.h>

#define WPS_APLIST_BUF_SIZE (BRCM_WPS_MAX_AP_NUMBER*sizeof(brcm_wpscli_ap_entry) + sizeof(brcm_wpscli_ap_list))
const static char ZERO_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if defined(WIN32) || defined(_WIN32_WCE)
#ifdef WIN32
// Compile Win32 Big Window
#define SOFTAP_IF_NAME			"{C87FCF3B-B2AE-4D06-9704-D12A7D5568DC}"  // Desktop PCI multi-band wlan adapter
//#define SOFTAP_IF_NAME			"{08B41032-0F84-4983-9F0A-B2875AB1BF0C}"  // LP-Test/D820 virutal adapter
//#define SOFTAP_IF_NAME			"{5A90D3D4-D239-4290-AA6C-0F4A12EA7EB7}"  // D820 Broadcom 802.11n Network Adapter
//#define SOFTAP_IF_NAME			"{CC1C922D-F81A-4E03-AC87-4A31D79A3A98}"  // Manoj's HP laptop Virtual Adapter
//#define SOFTAP_IF_NAME			"{717BED27-E7FD-4AF7-9648-B6AC49F40031}"  // LP-Test/D820 virutal adapter


//#define WL_ADAPTER_IF_NAME		"{AD332787-7F9B-4EC7-B0E4-E85F532BAC1E}"  // Dell MC5 64-bit Vista
#define WL_ADAPTER_IF_NAME		"{EE502175-BAB0-4BBE-A398-627A8FF3ED91}"  // Desktop PCI Multi-band wlan adapter
//#define WL_ADAPTER_IF_NAME		"{0DC71097-0880-41D9-879F-08CBD6EECB57}"  // USB wlan adapter with wps led
//#define WL_ADAPTER_IF_NAME		"{8B9B98B8-74A6-441E-833F-81A35B781801}"	// USB wlan adapter w/o wps led
//#define WL_ADAPTER_IF_NAME		"{5A90D3D4-D239-4290-AA6C-0F4A12EA7EB7}"  // D820 Broadcom 802.11n Network Adapter
//#define WL_ADAPTER_IF_NAME		"{238F8720-981B-4A52-B20C-7686697B9DD1}"	// Build for Bin Li
#else
// Compile WinMobile
#define WL_ADAPTER_IF_NAME		"BCMSDDHD1" 
#define SOFTAP_IF_NAME			"BCMSDDHD1"  // BCMSDDHD1 is the primary interface
#endif
#else
// Compile Linux
#define SOFTAP_IF_NAME			"wl0.1"
#define WL_ADAPTER_IF_NAME		NULL
#endif


const char *ENCR_STR[] = {"None", "WEP", "TKIP", "AES"};
const char *AUTH_STR[] = {"OPEN", "SHARED", "WPA-PSK", "WPA2-PSK"};
const char *STATUS_STR[] = {
	// Generic WPS library errors
	"WPS_STATUS_SUCCESS",
	"WPS_STATUS_SYSTEM_ERR",					// generic error not belonging to any other definition
	"WPS_STATUS_OPEN_ADAPTER_FAIL",		// failed to open/init wps adapter
	"WPS_STATUS_ABORTED",					// user cancels the connection
	"WPS_STATUS_INVALID_NULL_PARAM",		// invalid NULL parameter passed in
	"WPS_STATUS_NOT_ENOUGH_MEMORY",			// more memory is required to retrieve data
	"WPS_STATUS_INVALID_NW_SETTINGS",			// Invalid network settings
	"WPS_STATUS_WINDOW_NOT_OPEN",		

	// WPS protocol related errors
	"WPS_STATUS_PROTOCOL_SUCCESS",				
	"WPS_STATUS_PROTOCOL_INIT_FAIL",				
	"WPS_STATUS_PROTOCOL_INIT_SUCCESS",				
	"WPS_STATUS_PROTOCOL_START_EXCHANGE",
	"WPS_STATUS_PROTOCOL_CONTINUE",				
	"WPS_STATUS_PROTOCOL_SEND_MEG",
	"WPS_STATUS_PROTOCOL_WAIT_MSG",
	"WPS_STATUS_PROTOCOL_RECV_MSG",
	"WPS_STATUS_PROTOCOL_FAIL_TIMEOUT",			// timeout and fails in M1-M8 negotiation
	"WPS_STATUS_PROTOCOL_FAIL_MAX_EAP_RETRY",				// don't retry any more because of EAP timeout as AP gives up already 
	"WPS_STATUS_PROTOCOL_FAIL_OVERLAP",					// PBC session overlap
	"WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN",		// fails in protocol processing stage because of unmatched pin number
	"WPS_STATUS_PROTOCOL_FAIL_EAP",				// fails because of EAP failure
	"WPS_STATUS_PROTOCOL_FAIL_UNEXPECTED_NW_CRED",				// after wps negotiation, unexpected network credentials are received
	"WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG",				// after wps negotiation, unexpected network credentials are received

	// WL handler related status code
	"WPS_STATUS_SET_BEACON_IE_FAIL",
	"WPS_STATUS_DEL_BEACON_IE_FAIL",
	"WPS_STATUS_IOCTL_SET_FAIL",			// failed to set iovar
	"WPS_STATUS_IOCTL_GET_FAIL",			// failed to get iovar

	// WLAN related status code
	"WPS_STATUS_WLAN_INIT_FAIL",
	"WPS_STATUS_WLAN_SCAN_START",
	"WPS_STATUS_WLAN_NO_ANY_AP_FOUND",
	"WPS_STATUS_WLAN_NO_WPS_AP_FOUND",
	"WPS_STATUS_WLAN_CONNECTION_START",					// preliminary association failed
	"WPS_STATUS_WLAN_CONNECTION_ATTEMPT_FAIL",					// preliminary association failed
	"WPS_STATUS_WLAN_CONNECTION_LOST",					// preliminary association lost during registration
	"WPS_STATUS_WLAN_CONNECTION_DISCONNECT",

	// Packet dispatcher related erros
	"WPS_STATUS_PKTD_INIT_FAIL",
	"WPS_STATUS_PKTD_SYSTEM_FAIL",				// Generic packet dispatcher related errors not belonging any other definition 
	"WPS_STATUS_PKTD_SEND_PKT_FAIL",				// failed to send eapol packet
	"WPS_STATUS_PKTD_NO_PKT",				
	"WPS_STATUS_PKTD_NOT_EAP_PKT"			// received packet is not eap packet
};

void print_help()
{
	printf("Usage: \n");
	printf("  wpscli_test_cmd softap      [start wps in SoftAP mode]\n");
	printf("  wpscli_test_cmd sta         [start wps in STA mode (default option)]\n");
}

unsigned char* ConverMacAddressStringIntoByte(const char *pszMACAddress, unsigned char* pbyAddress)
{
	const char cSep = '-';
	int iConunter;

	for (iConunter = 0; iConunter < 6; ++iConunter)
	{
		unsigned int iNumber = 0;
		char ch;

		//Convert letter into lower case.
		ch = tolower(*pszMACAddress++);

		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
		{
			return NULL;
		}

		iNumber = isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
		ch = tolower (*pszMACAddress);

		if ((iConunter < 5 && ch != cSep) || 
			(iConunter == 5 && ch != '\0' && !isspace (ch)))
		{
			++pszMACAddress;

			if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
			{
				return NULL;
			}

			iNumber <<= 4;
			iNumber += isdigit (ch) ? (ch - '0') : (ch - 'a' + 10);
			ch = *pszMACAddress;

			if (iConunter < 5 && ch != cSep)
			{
				return NULL;
			}
		}
		/* Store result.  */
		pbyAddress[iConunter] = (unsigned char) iNumber;
		/* Skip cSep.  */
		++pszMACAddress;
	}
	return pbyAddress;
}

uint8 * get_sta_mac(uint8 mac_buf[])
{
//	uint8 sta_mac[] = {0x00, 0x10, 0x18, 0x96, 0x12, 0x65};  // Dell 65 Vist 64-bit
//	uint8 sta_mac[] = {0x00, 0x1e, 0x4c, 0x78, 0x0d, 0x9d};  // Dell 830 laptop
//	uint8 sta_mac[] = {0x00, 0x1a, 0x73, 0xfd, 0x0b, 0xf7};  // D820 LP-test Vista
//	uint8 sta_mac[] = {0x00, 0x90, 0x4c, 0xd8, 0x04, 0x16};  // USB dongle
//	uint8 sta_mac[] = {0x00, 0x10, 0x18, 0x90, 0x25, 0x50};  // Desktop PCI Multi-band wlan adapter
	uint8 default_mac[] = {0x00, 0x90, 0x4c, 0xc6, 0x00, 0xff};  // Linux desktop
	char str_mac[256] = { 0 };
	uint8 *sta_mac = NULL;

GET_MAC:
	sprintf(str_mac, "%02x-%02x-%02x-%02x-%02x-%02x", default_mac[0], default_mac[1], 
		default_mac[2], default_mac[3], default_mac[4], default_mac[5]);

	printf("\nPlease enter MAC address of STA to request enrollment (default: %s): ", str_mac);

	// Get input string
	gets(str_mac);
	if(strlen(str_mac) == 0) {
		memcpy(mac_buf, default_mac, 6);  // use the default mac address
	}
	else { 
		sta_mac = ConverMacAddressStringIntoByte(str_mac, mac_buf);  // convert mac format from input string to bytes 
		if(sta_mac == NULL) {
			printf("\nWrong format of MAC address, try again.\n");
			goto GET_MAC;
		}
	}

	return mac_buf;
}

void test_softap_side(const char *pin)
{
	brcm_wpscli_status status;
	brcm_wpscli_nw_settings nw_settings;
	char c;
	uint8 mac_buf[6] = { 0 };
	uint8 sta_mac[6] = { 0 };

	nw_settings.authType = BRCM_WPS_AUTHTYPE_WPAPSK;
	nw_settings.encrType = BRCM_WPS_ENCRTYPE_TKIP;
	strcpy(nw_settings.nwKey, "password");
	strcpy(nw_settings.ssid, "vista_softap");
	nw_settings.wepIndex = 0;


	// Get mac address of STA to enroll
//	sta_mac = get_sta_mac(mac_buf);

	printf("Calling brcm_wpscli_open\n");

	status = brcm_wpscli_open(SOFTAP_IF_NAME, BRCM_WPSCLI_ROLE_SOFTAP, NULL, NULL);
	if(status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_open failed. System error!\n");
		goto SOFTAP_END;
	}

SOFTAP_ESTART:
	printf("Adding WPS enrollee and waiting for enrollee to connect...\n");

	if(pin == NULL || strlen(pin) == 0)
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW, BRCM_WPS_PWD_TYPE_PBC, NULL, &nw_settings, 60, sta_mac);
	else
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW, BRCM_WPS_PWD_TYPE_PIN, pin, &nw_settings, 60, sta_mac);

	if(status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_softap_start_wps failed. System error!\n");
		brcm_wpscli_softap_close_session();
		goto SOFTAP_END;
	}

//	status = brcm_wpscli_softap_process_protocol(sta_mac);

	if(status == WPS_STATUS_SUCCESS)
		printf("\n\nWPS negotiation is successful!\n");
	else
		printf("\n\nWPS negotiation is failed. status=%d!\n", status);

	printf("\n\n");

	// Close wps windows session
	brcm_wpscli_softap_close_session();

	printf("\n\nDo you want to process another enrollment request (y/n)?: ");
	fflush(stdin);
	c=getchar();
	if(c=='y' || c=='Y')
		goto SOFTAP_ESTART;

SOFTAP_END:
	brcm_wpscli_close();

	printf("\n\nWPS registrar is exitting....\n");
}

void test_sta_side(const char *pin)
{
	brcm_wpscli_status status;
	uint8 bssid[6] = { 0 };
	char ssid[32] = { 0 };
	uint8 wep = 1;
	uint16 band = 0;
	char option[10] = { 0 };
	int retries = 3;
	int i=20;
	uint32 nAP=0;
	char buf[WPS_APLIST_BUF_SIZE] = { 0 };
	uint32 ap_total;
	brcm_wpscli_ap_entry *ap;
	brcm_wpscli_nw_settings nw_cred;
	char c;
	brcm_wpscli_role role = BRCM_WPSCLI_ROLE_STA;  // STA mode by default

	status = brcm_wpscli_open(WL_ADAPTER_IF_NAME, BRCM_WPSCLI_ROLE_STA, NULL, NULL);
	if(status != WPS_STATUS_SUCCESS) {
		printf("Failed to initialize wps library. status=%s\n", STATUS_STR[status]);
		goto END;
	}

	printf("Searching WPS APs...\n");
	status = brcm_wpscli_sta_search_wps_ap(&nAP);

	if(status == WPS_STATUS_SUCCESS) {
		printf("WPS APs are found and there are %d of them:\n", nAP);

		// Get the list of wps APs
		status = brcm_wpscli_sta_get_wps_ap_list((brcm_wpscli_ap_list *)buf, 
												sizeof(brcm_wpscli_ap_entry)*BRCM_WPS_MAX_AP_NUMBER, 
												&ap_total);
		if(status == WPS_STATUS_SUCCESS) 
		{
			for(i=0; i<ap_total; i++) {
				ap = &(((brcm_wpscli_ap_list *)buf)->ap_entries[i]);
				printf("[%u] %s\n",i+1,ap->ssid);
			}

			printf("-------------------------------------------------------\n");
			printf("\nPlease enter the AP number you wish to connect to or [x] to quit: ");
			c=getchar();
			if(c=='x' || c=='X')
				goto END;

			ap = &(((brcm_wpscli_ap_list *)buf)->ap_entries[c-'1']);
			
			printf("\nConnecting to WPS AP [%s] and negotiating WPS protocol in %s mode\n", 
				   ap->ssid,
				   ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC? "PBC" : "PIN");

			if(ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC)
			{
				// Continue to make connection
				status = brcm_wpscli_sta_start_wps(ap->ssid,
										ap->wsec,
										ap->bssid,
										0, 0,
										BRCM_WPS_MODE_STA_ENR_JOIN_NW, 
										BRCM_WPS_PWD_TYPE_PBC, 
										NULL,
										120,
										&nw_cred);
			}
			else
			{
				status = brcm_wpscli_sta_start_wps(ap->ssid,
										ap->wsec,
										ap->bssid,
										0, 0,
										BRCM_WPS_MODE_STA_ENR_JOIN_NW, 
										BRCM_WPS_PWD_TYPE_PIN, 
										pin,
										120,
										&nw_cred);
			}

			// Print out the result
			if(status == WPS_STATUS_SUCCESS)
			{
				printf("\nWPS negotiation is successful!\n");
				printf("\nWPS AP Credentials:\n");
				printf("SSID: %s\n", nw_cred.ssid); 
				printf("Key Mgmt type: %s\n", AUTH_STR[nw_cred.authType]);
				printf("Encryption type: %s\n", ENCR_STR[nw_cred.encrType]);
				printf("Network key: %s\n", nw_cred.nwKey);
				printf("\n\n");
			}
			else
				printf("WPS negotiation is failed. status=%s\n", STATUS_STR[status]);
		}
		else
		{
			printf("Failed to get wps ap list. status=%s\n", STATUS_STR[status]);
		}
	}
	else {
		printf("No WPS AP is found. status=%s\n", STATUS_STR[status]);
	}

END:
	brcm_wpscli_close();
}


int main(int argc, char* argv[])
{
	char c;
	char pin[80] = { 0 };
	char role_sel = '0';
	brcm_wpscli_role role = BRCM_WPSCLI_ROLE_STA;  // STA mode by default

	printf("**************** WPSCLI Test ****************************\n");
	printf("1. Run WPS for SoftAP side as Registrar.\n");
	printf("2. Run WPS for STA side as Enrollee.\n");
	printf("e. Exit the test.\n");
	
	printf("\n");
	while(1) {
		printf("Please make selection: ");
		role_sel = getchar();
		if(role_sel == '1') {
			role = BRCM_WPSCLI_ROLE_SOFTAP;
			break;
		}
		else if(role_sel == '2') {
			role = BRCM_WPSCLI_ROLE_STA;
			break;
		}
		else if(role_sel == 'e' || role_sel == 'E')
			return 0;
		
		fflush(stdin);
	}

	printf("\n*********************************************\n");
	if(role == BRCM_WPSCLI_ROLE_STA)
		printf("WPS - STA Enrollee APP Broadcom Corp.\n");
	else
		printf("WPS - SoftAP Registrar APP Broadcom Corp.\n");
	printf("*********************************************\n");

	printf("\nIf you have a pin, enter it now, otherwise press ENTER: ");
	fflush(stdin);
	do{
	   fgets(pin, 80, stdin);
	}while(pin[0] == '\n'); 

	fflush(stdin);
	pin[strlen(pin)-1] = '\0';
	if(role == BRCM_WPSCLI_ROLE_STA)
		test_sta_side(pin);
	else
		test_softap_side(pin);

	printf("\nPress any key to exit...");
	fflush(stdin);
	c = getchar();

	return 0;
}
