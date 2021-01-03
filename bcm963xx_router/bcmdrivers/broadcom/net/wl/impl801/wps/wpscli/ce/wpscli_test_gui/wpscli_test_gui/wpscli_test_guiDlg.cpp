// wpscli_test_guiDlg.cpp : implementation file
//

#include "stdafx.h"
#include "wpscli_test_gui.h"
#include "wpscli_test_guiDlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_test_guiDlg.cpp,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: WPSCLI library test console program
 *
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <eap.h>
#include <wzcsapi.h>
#include <winioctl.h>
#include <Devload.h>

 #define atow(strA,strW,lenW) \
MultiByteToWideChar(CP_ACP,0,strA,-1,strW,lenW)

#define wtoa(strW,strA,lenA) \
WideCharToMultiByte(CP_ACP,0,strW,-1,strA,lenA,NULL,NULL)

#define WPS_APLIST_BUF_SIZE (BRCM_WPS_MAX_AP_NUMBER*sizeof(brcm_wpscli_ap_entry) + sizeof(brcm_wpscli_ap_list))
const static char ZERO_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

WZC_CONTEXT g_WzcContext;
bool g_bWzcContextInitialized = false;
// Compile WinMobile
#define WL_ADAPTER_IF_NAME		"BCMSDDHD1" 
#define SOFTAP_IF_NAME			"BCMSDDHD2"  // BCMSDDHD1 is the primary interface

wchar_t* getAdapterName() { return TEXT(WL_ADAPTER_IF_NAME);}

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

BOOL g_WZCDisabled = FALSE;

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

#define MARGIN 2
class CStoreList
{
public:
	uint8 m_bssid[6];
	char m_ssid[33];
	uint8 m_wep;
	uint16 m_band;
};

enum eWPS_COMBO_MODE {
	STA_CONFIGURE = 0,
	STA_ENROLL = 1,
};

uint8 * get_sta_mac(uint8 mac_buf[])
{
//	uint8 sta_mac[] = {0x00, 0x10, 0x18, 0x96, 0x12, 0x65};  // Dell 65 Vist 64-bit
//	uint8 sta_mac[] = {0x00, 0x1e, 0x4c, 0x78, 0x0d, 0x9d};  // Dell 830 laptop
//	uint8 sta_mac[] = {0x00, 0x1a, 0x73, 0xfd, 0x0b, 0xf7};  // D820 LP-test Vista
//	uint8 sta_mac[] = {0x00, 0x90, 0x4c, 0xd8, 0x04, 0x16};  // USB dongle
//	uint8 sta_mac[] = {0x00, 0x10, 0x18, 0x90, 0x25, 0x50};  // Desktop PCI Multi-band wlan adapter
	uint8 default_mac[] = {0x00, 0x90, 0x4c, 0xc5, 0x03, 0x94};  // Linux desktop
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

// CWPSCLITestGUIDlg dialog

CWPSCLITestGUIDlg::CWPSCLITestGUIDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CWPSCLITestGUIDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_WPSRole = BRCM_WPSCLI_ROLE_STA;  // STA mode by default

}

void CWPSCLITestGUIDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_NETWORKS, m_lstNetworks);
	DDX_Control(pDX, IDC_PIN_STRING, m_edtPinValue);
	DDX_Check(pDX, IDC_PIN_MODE, m_bPinMode);
	DDX_Control(pDX, IDC_EDIT_STATUS, m_edtStatus);
	DDX_Control(pDX, IDC_COMBO_MODE, m_listWPSMode);
}

BEGIN_MESSAGE_MAP(CWPSCLITestGUIDlg, CDialog)
#if defined(_DEVICE_RESOLUTION_AWARE) && !defined(WIN32_PLATFORM_WFSP)
	ON_WM_SIZE()
#endif
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_ENROLL, &CWPSCLITestGUIDlg::OnBnClickedEnroll)
	ON_BN_CLICKED(IDC_PIN_MODE, &CWPSCLITestGUIDlg::OnBnClickedPinMode)
	ON_BN_CLICKED(IDC_REFRESH, &CWPSCLITestGUIDlg::OnBnClickedRefresh)
	ON_BN_CLICKED(IDCANCEL, &CWPSCLITestGUIDlg::OnBnClickedCancel)
	ON_NOTIFY(NM_KILLFOCUS, IDC_NETWORKS, &CWPSCLITestGUIDlg::OnNMKillfocusNetworks)
	ON_NOTIFY(NM_SETFOCUS, IDC_NETWORKS, &CWPSCLITestGUIDlg::OnNMSetfocusNetworks)
	ON_EN_SETFOCUS(IDC_PIN_STRING, &CWPSCLITestGUIDlg::OnEnSetfocusPinString)
	ON_EN_CHANGE(IDC_PIN_STRING, &CWPSCLITestGUIDlg::OnEnChangePinString)
	ON_CBN_SELCHANGE(IDC_COMBO_MODE, &CWPSCLITestGUIDlg::OnCbnSelchangeComboMode)

END_MESSAGE_MAP()

int ConfigureAdapter(PWCHAR pAdapter, DWORD dwCommand)
{
	HANDLE ndis;
	TCHAR multi[100];
	int len = 0;

	if (pAdapter)
		len = _tcslen(pAdapter);

	if (len == 0 || len > 80)
		return 0;

	ndis = CreateFile(DD_NDIS_DEVICE_NAME, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if (ndis == INVALID_HANDLE_VALUE) {
		return 0;
	}

	len++;
	memcpy(multi, pAdapter, len * sizeof(TCHAR));
	memcpy(&multi[len], TEXT("NDISUIO\0"), 9 * sizeof(TCHAR));
	len += 9;

	if (!DeviceIoControl(ndis, dwCommand,
		multi, len * sizeof(TCHAR), NULL, 0, NULL, NULL)) {
			CloseHandle(ndis);
			return FALSE;
	}

	CloseHandle(ndis);
	return 1;
}

typedef DWORD (*PFN_WZCQueryContext)(LPWSTR pSrvAddr,DWORD dwInFlags,PWZC_CONTEXT pContext,LPDWORD pdwOutFlags);
typedef DWORD (*PFN_WZCSetContext)(LPWSTR pSrvAddr,DWORD dwInFlags,PWZC_CONTEXT pContext,LPDWORD pdwOutFlags);
#define WZC_DRIVER TEXT("Drivers\\BuiltIn\\ZeroConfig")

int ConfigureWZCInterface(BOOL bEnable)
{
	DWORD dwStatus = 0;
	WZC_CONTEXT WzcContext = {0};
	HINSTANCE hWZCLib = NULL;
	DWORD dwTimerInFlags = 0x00;
	PFN_WZCQueryContext       pfnWZCQueryContext = 0;
	PFN_WZCSetContext         pfnWZCSetContext = 0;

	hWZCLib = LoadLibraryW(L"wzcsapi.dll");
	if (hWZCLib != NULL) {
		pfnWZCQueryContext     = (PFN_WZCQueryContext)GetProcAddress(hWZCLib,L"WZCQueryContext");
		pfnWZCSetContext     = (PFN_WZCSetContext)GetProcAddress(hWZCLib,L"WZCSetContext");

		if ((pfnWZCQueryContext == NULL)       ||
			(pfnWZCSetContext == NULL)) {
				FreeLibrary(hWZCLib);
				return 0;
		}

		dwStatus = pfnWZCQueryContext(NULL, dwTimerInFlags, &WzcContext, NULL);
		if (dwStatus == ERROR_SUCCESS)
		{
			if (!bEnable)
			{
				memcpy(&g_WzcContext, &WzcContext, sizeof(WZC_CONTEXT));
				g_bWzcContextInitialized = true;
				WzcContext.tmTr = TMMS_INFINITE;
				WzcContext.tmTp =  TMMS_INFINITE;
				WzcContext.tmTc = TMMS_INFINITE;
				WzcContext.tmTf =  TMMS_INFINITE;
			}
			else
			{
				if (!g_bWzcContextInitialized || 
						TMMS_INFINITE == g_WzcContext.tmTr ||
						TMMS_INFINITE == g_WzcContext.tmTp ||
						TMMS_INFINITE == g_WzcContext.tmTc ||
						TMMS_INFINITE == g_WzcContext.tmTp)
				{
					g_WzcContext.tmTr = 3000;
					g_WzcContext.tmTp =  2000;
					g_WzcContext.tmTc = 0x70000000;
					g_WzcContext.tmTf =  60000;
				}
				memcpy(&WzcContext, &g_WzcContext, sizeof(WZC_CONTEXT));
			}
			dwStatus = pfnWZCSetContext(NULL, dwTimerInFlags, &WzcContext, NULL);
		}

		FreeLibrary(hWZCLib);
	}
	return (dwStatus == ERROR_SUCCESS ? 1 : 0);
}


bool wps_configure_wzcsvc(bool bEnable)
{
	int bRet = 0;
	HKEY   hkey;
	DWORD dwErrRpt = -1, dwDisable = 0;
	bool bRegSuccess = false;
	DWORD  dwDisposition = 0,dwType = 0, dwSize = 0;
	LONG ret = RegOpenKeyEx(HKEY_LOCAL_MACHINE, WZC_DRIVER, 0, 0, &hkey);
	if (ret == ERROR_SUCCESS) {
		RegCloseKey(hkey);
		bRet = 1;
	}
	else {
		return TRUE; /* Do not do anything */
	}

	bRet = ConfigureWZCInterface(bEnable);

	if (bRet)
	{
		g_WZCDisabled = !bEnable;
	}
	ConfigureAdapter(getAdapterName(), IOCTL_NDIS_REBIND_ADAPTER);
	Sleep(2000);
	return bRet;
}

BOOL CWPSCLITestGUIDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	int index=0;

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	AdjustControls();
	CString strPin(_T("14412783"));
	m_edtPinValue.SetLimitText(8);

	CenterWindow();
	m_edtPinValue.ShowWindow(SW_SHOW);
	m_edtPinValue.SetWindowTextW(strPin);
	wtoa(strPin.GetBuffer(0), m_pin, 79);
	
		m_lstNetworks.InsertColumn(0, _T("SSID"));
	m_lstNetworks.InsertColumn(1, _T("BSSID"));

	CRect rect;
	m_lstNetworks.GetWindowRect(&rect);
	m_lstNetworks.SetColumnWidth(0, (int)(rect.Width()/4));
	m_lstNetworks.SetColumnWidth(1, (int)(3*rect.Width()/4));

	m_lstNetworks.SetExtendedStyle(m_lstNetworks.GetExtendedStyle() | LVS_EX_FULLROWSELECT | LVS_SHOWSELALWAYS );

	m_listWPSMode.InsertString(index++, _T("WPS Registrar"));
	m_listWPSMode.InsertString(index++, _T("WPS Enrollee"));

	m_listWPSMode.SetCurSel(STA_ENROLL);

	UpdateControls(STA_ENROLL);

	UpdateStatus(_T("Pin Mode"));
	
	bWpsCancelled = FALSE;
//	Test();
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CWPSCLITestGUIDlg::OnBnClickedEnroll()
{
//	GetDlgItem(IDC_ENROLL)->EnableWindow(FALSE);	
	if (m_WPSRole == BRCM_WPSCLI_ROLE_STA)
	{
		test_sta_side();
	}
	else
	{
		test_softap_side();
	}
}

void CWPSCLITestGUIDlg::OnBnClickedPinMode()
{
	UpdateData(TRUE);
	UpdatePinMode(m_bPinMode);
}

void CWPSCLITestGUIDlg::UpdatePinMode(BOOL bPinMode)
{
	if (!bPinMode)
	{
		UpdateStatus(_T("PBC Mode"));
		//m_edtPinValue.SetWindowTextW(_T(""));
		m_edtPinValue.ShowWindow(SW_HIDE);
	}
	else
	{
		//wchar_t szChar[200];
		//atow(m_pin,szChar,199);
		UpdateStatus(_T("Pin Mode"));
		//m_edtPinValue.SetWindowTextW(szChar);
		m_edtPinValue.ShowWindow(SW_SHOW);
	}
}

void CWPSCLITestGUIDlg::OnBnClickedRefresh()
{
	Refresh();
	UpdateControls(m_listWPSMode.GetCurSel());
}

void CWPSCLITestGUIDlg::Refresh()
{
	CString strPin;
	uint32 nAP = 0;
	wchar_t bssidString[100];
	brcm_wpscli_status status;
	int k;
	int i=20;
	int j=0;
	char buf[WPS_APLIST_BUF_SIZE] = { 0 };
	uint32 ap_total;
	brcm_wpscli_ap_entry *ap;

	CWaitCursor c;
	UpdateData(TRUE);

	m_edtPinValue.GetWindowTextW(strPin);
	strPin.TrimLeft();
	strPin.TrimRight();
	wtoa(strPin.GetBuffer(0), m_pin, 79);

	Clear();
	UpdateData(FALSE);

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
			k=0;
			for(i=0; i<ap_total; i++) {
				ap = &(((brcm_wpscli_ap_list *)buf)->ap_entries[i]);
				printf("[%u] %s\n",i+1,ap->ssid);

				if (!m_bPinMode && ap->pwd_type == BRCM_WPS_PWD_TYPE_PIN)
				{
					continue;
				}
				if (m_bPinMode && ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC)
				{
					continue;
				}

				CString str(ap->ssid);
				CStoreList* pStore = new CStoreList();
				strcpy(pStore->m_ssid, ap->ssid);
				for(int j=0;j<6;j++)
					pStore->m_bssid[j] = ap->bssid[j];
				wsprintf(bssidString,_T("%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x"),
					pStore->m_bssid[0], pStore->m_bssid[1], pStore->m_bssid[2], 
					pStore->m_bssid[3], pStore->m_bssid[4], pStore->m_bssid[5], 
					pStore->m_bssid[6]);
				pStore->m_wep = ap->wsec;
				pStore->m_band = ap->band;
				m_lstNetworks.InsertItem(k, str);
				m_lstNetworks.SetItemText(k, 1, bssidString);
				m_lstNetworks.SetItemData(k, (DWORD_PTR)(pStore));
				k++;
			}
		}
		else
		{
			printf("Failed to get wps ap list. status=%s\n", STATUS_STR[status]);
			AfxMessageBox(_T("Failed to get wps ap list"));
		}
	}
	else {
		printf("No WPS AP is found. status=%s\n", STATUS_STR[status]);
		AfxMessageBox(_T("No WPS AP is found"));
	}

END:
	brcm_wpscli_close();	 
}

void CWPSCLITestGUIDlg::Clear()
{
	for (int i=0;i < m_lstNetworks.GetItemCount();i++)
	{
		CStoreList* pStore = (CStoreList*)(m_lstNetworks.GetItemData(i));
		if (pStore)
			delete pStore;
	}
	m_lstNetworks.DeleteAllItems();
}

void CWPSCLITestGUIDlg::UpdateStatus(LPCTSTR pszStatus)
{
    m_edtStatus.SetWindowTextW(pszStatus);
}

void CWPSCLITestGUIDlg::OnBnClickedCancel()
{
}

LONG CWPSCLITestGUIDlg::AdjustControls()
{
	int i;
	CRect rectDlg;
	CRect rect;
	GetWindowRect(&rectDlg);
	ScreenToClient(&rectDlg);

	CWnd *pWnd = 0;
	int iBottomRetry = 0;

    //cancel window
	pWnd = GetDlgItem(IDCANCEL);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	int iWidth = rect.Width();
    rect.right = rectDlg.right-MARGIN;
	rect.left = rect.right - iWidth;
	int iModeRight = rect.left - MARGIN;
	pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
	InvalidateRect(&rect);

	//mode window
	pWnd = GetDlgItem(IDC_COMBO_MODE);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.right = iModeRight;
	iBottomRetry = rect.bottom;
	pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
	InvalidateRect(&rect);

	//Pin String
	pWnd = GetDlgItem(IDC_PIN_STRING);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.right = rectDlg.right-MARGIN;;
	iBottomRetry += rect.Height() + 5 * MARGIN ;
	pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
	InvalidateRect(&rect);

	//Push Buttons
	int iHeight = 0;
	int TopButtons = 0;
	for (i=0;i<2;i++)
	{
		if (i == 0)
		{
			//scan
			pWnd = GetDlgItem(IDC_REFRESH);
		}
		else if (i == 1)
		{
			//enroll
			pWnd = GetDlgItem(IDC_ENROLL);
		}

		pWnd->GetWindowRect(&rect);
		ScreenToClient(&rect);
		iHeight = rect.Height();
		rect.bottom = rectDlg.bottom-MARGIN;
		rect.top = rect.bottom - iHeight;
		TopButtons = rect.top;
		pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
		InvalidateRect(&rect);
	}

	//edit status
	int TopStatus = 0;
	pWnd = GetDlgItem(IDC_EDIT_STATUS);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.right = rectDlg.right-MARGIN;
	iHeight = rect.Height();
	rect.bottom = TopButtons;
	rect.top = rect.bottom - iHeight;
	TopStatus = rect.top;
	pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
	InvalidateRect(&rect);

	//scan list
	pWnd = GetDlgItem(IDC_NETWORKS);
	pWnd->GetWindowRect(&rect);
	ScreenToClient(&rect);
	rect.right = rectDlg.right-MARGIN;
	iHeight = rect.Height();
	rect.bottom = TopStatus;
	rect.top = iBottomRetry;
	pWnd->MoveWindow(rect.left, rect.top, rect.Width(), rect.Height());
	InvalidateRect(&rect);

	UpdateControls(m_listWPSMode.GetCurSel());
	UpdateWindow();

    return 0;
}

void CWPSCLITestGUIDlg::OnDestroy()
{
	if (g_WZCDisabled)
	{
		wps_configure_wzcsvc(1);
		Sleep(500);
	}

	CDialog::OnDestroy();
}


void CWPSCLITestGUIDlg::UpdateControls(int iSel)
{
    eWPS_COMBO_MODE nMode = static_cast<eWPS_COMBO_MODE>(iSel);
	CWnd* pWndMode = GetDlgItem(IDC_ENROLL);
//	pWndMode->EnableWindow(FALSE);
	CWnd* pWnd = GetDlgItem(IDC_REFRESH);
	pWnd->EnableWindow(TRUE);

	switch(nMode)
	{
		case STA_CONFIGURE:
			pWndMode->SetWindowTextW(_T("Registrar"));
			m_WPSRole = BRCM_WPSCLI_ROLE_SOFTAP;  
			pWnd->EnableWindow(FALSE);
			break;
		case STA_ENROLL:
			pWndMode->SetWindowTextW(_T("Enroll"));
			m_WPSRole = BRCM_WPSCLI_ROLE_STA;  
			break;
		default:
			break;
	}

	UpdateData(FALSE);
	UpdatePinMode(m_bPinMode);
}

void CWPSCLITestGUIDlg::OnNMKillfocusNetworks(NMHDR *pNMHDR, LRESULT *pResult)
{

}

void CWPSCLITestGUIDlg::OnNMSetfocusNetworks(NMHDR *pNMHDR, LRESULT *pResult)
{
	UpdateControls(m_listWPSMode.GetCurSel());
}

void CWPSCLITestGUIDlg::OnEnSetfocusPinString()
{
	CWnd* pWndMode = GetDlgItem(IDC_ENROLL);
//	pWndMode->EnableWindow(FALSE);
}


void CWPSCLITestGUIDlg::OnEnChangePinString()
{
	CString strPin;

	UpdateData(TRUE);
	m_edtPinValue.GetWindowTextW(strPin);
	strPin.TrimLeft();
	strPin.TrimRight();
	wtoa(strPin.GetBuffer(0), m_pin, 79);

}

void CWPSCLITestGUIDlg::OnCbnSelchangeComboMode()
{
	Clear();
	int iSel = m_listWPSMode.GetCurSel();

	UpdateControls(iSel);
}



void CWPSCLITestGUIDlg::test_sta_side()
{
	brcm_wpscli_status status;
	brcm_wpscli_nw_settings nw_cred;
	CStoreList* pStore = 0;
	bool bContinue= true;
	CWaitCursor c1;

	int i=20;
	unsigned long pin = strtoul(m_pin,NULL,0);

	if (m_bPinMode && strlen(m_pin) == 0)
	{
		AfxMessageBox(_T("No pin defined."), MB_ICONSTOP);
		bContinue = false;
	} 
	else
	{
		POSITION pos = m_lstNetworks.GetFirstSelectedItemPosition();
		if (pos == NULL)
		{
			AfxMessageBox(_T("No APs selected."), MB_ICONSTOP);
			bContinue = false;
		}
		else
		{
			while (pos)
			{
				int nItem = m_lstNetworks.GetNextSelectedItem(pos);
				pStore = (CStoreList*)(m_lstNetworks.GetItemData(nItem));
			}
		}
	}

	if (!bContinue || !pStore)
		return;

	status = brcm_wpscli_open(WL_ADAPTER_IF_NAME, BRCM_WPSCLI_ROLE_STA, NULL, NULL);
	if(status != WPS_STATUS_SUCCESS) {
		printf("Failed to initialize wps library. status=%s\n", STATUS_STR[status]);
		goto END;
	}

	wps_configure_wzcsvc(0);

	UpdateStatus(_T("Started enrollment"));
	if(!m_bPinMode)
	{
		// Continue to make connection
		status = brcm_wpscli_sta_start_wps(pStore->m_ssid,
			pStore->m_wep,
			pStore->m_bssid,
			0, 0,
			BRCM_WPS_MODE_STA_ENR_JOIN_NW, 
			BRCM_WPS_PWD_TYPE_PBC, 
			NULL,
			180,
			&nw_cred);
	}
	else
	{
		status = brcm_wpscli_sta_start_wps(pStore->m_ssid,
			pStore->m_wep,
			pStore->m_bssid,
			0, 0,
			BRCM_WPS_MODE_STA_ENR_JOIN_NW, 
			BRCM_WPS_PWD_TYPE_PIN, 
			m_pin,
			180,
			&nw_cred);
	}

	wps_configure_wzcsvc(1);
	UpdateStatus(_T("Enrollment over"));

	// Print out the result
	if(status == WPS_STATUS_SUCCESS)
	{
		char szCred[1024];
		printf("\nWPS negotiation is successful!\n");
		printf("\nWPS AP Credentials:\n");
		printf("SSID: %s\n", nw_cred.ssid); 
		printf("Key Mgmt type: %s\n", AUTH_STR[nw_cred.authType]);
		printf("Encryption type: %s\n", ENCR_STR[nw_cred.encrType]);
		printf("Network key: %s\n", nw_cred.nwKey);
		printf("\n\n");

		sprintf(szCred, "WPS AP Credentials:\nSSID = %s\nKey Mgmt type is %s\nKey : %s\nEncryption : %s",
			nw_cred.ssid, AUTH_STR[nw_cred.authType], ENCR_STR[nw_cred.encrType], nw_cred.nwKey);

		CString str(szCred);
		AfxMessageBox(str);
	}
	else
	{
		printf("WPS negotiation is failed. status=%s\n", STATUS_STR[status]);
		AfxMessageBox(_T("WPS negotiation is failed"));
	}

END:
	brcm_wpscli_close();
}

void CWPSCLITestGUIDlg::test_softap_side()
{
	brcm_wpscli_status status;
	brcm_wpscli_nw_settings nw_settings;
	uint8 mac_buf[6] = { 0 };
	uint8 sta_mac[6] = { 0 };

	CWaitCursor c1;

	unsigned long pin = strtoul(m_pin,NULL,0);

	if (m_bPinMode && strlen(m_pin) == 0)
	{
		AfxMessageBox(_T("No pin defined."), MB_ICONSTOP);
		return;
	} 


	//hardcode - can be put in dialog box.
	nw_settings.authType = BRCM_WPS_AUTHTYPE_OPEN;
	nw_settings.encrType = BRCM_WPS_ENCRTYPE_NONE;
	strcpy(nw_settings.nwKey, "You Got it - Key");
	strcpy(nw_settings.ssid, "You Got it - SSID");
	nw_settings.wepIndex = 0;

	// Get mac address of STA to enroll
	//memcpy(&sta_mac, &pStore->m_bssid[0], sizeof(sta_mac));

	printf("Calling brcm_wpscli_open\n");

	status = brcm_wpscli_open(SOFTAP_IF_NAME, BRCM_WPSCLI_ROLE_SOFTAP, NULL, NULL);
	if(status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_open failed. System error!\n");
		goto SOFTAP_END;
	}

	printf("Adding WPS enrollee and waiting for enrollee to connect...\n");
	UpdateStatus(_T("Started registration"));

	if(!m_bPinMode)
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW, BRCM_WPS_PWD_TYPE_PBC, NULL, &nw_settings, 180, sta_mac);
	else
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW, BRCM_WPS_PWD_TYPE_PIN, m_pin, &nw_settings, 180, sta_mac);

	if(status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_softap_start_wps failed. System error!\n");
		brcm_wpscli_softap_close_session();
		goto SOFTAP_END;
	}

//	status = brcm_wpscli_softap_process_protocol(sta_mac);
	UpdateStatus(_T("Registration over"));

	if(status == WPS_STATUS_SUCCESS)
		AfxMessageBox(_T("WPS negotiation is successful!"));
	else
		AfxMessageBox(_T("WPS negotiation is failed"));

	// Close wps windows session
	brcm_wpscli_softap_close_session();

SOFTAP_END:
	brcm_wpscli_close();
	UpdateStatus(_T("Registration over"));
}
