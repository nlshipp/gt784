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
 * Description: Implementation file
 *
 */
#include "stdafx.h"
#include "wpscli_test_gui.h"
#include "wpscli_test_guiDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

typedef struct td_test_ui_contex {
	Cwpscli_test_guiDlg *test_dlg;
} test_ui_contex;

static long index=0;
static test_ui_contex g_ctxCliRequest;

const char *g_szStatusStr[] = { 
	"BRCM_WPS_EVT_SOFTAP_ASSO_START", 
	"BRCM_WPS_EVT_SOFTAP_ASSO_SUCCESS", 
	"BRCM_WPS_EVT_SOFTAP_ASSO_FAIL", 
	"BRCM_WPS_EVT_STA_SCAN_START",
	"BRCM_WPS_EVT_STA_SCAN_CONTINUE",
	"BRCM_WPS_EVT_STA_SCAN_SUCCESS",
	"BRCM_WPS_EVT_STA_SCAN_FAIL",
	"BRCM_WPS_EVT_STA_ASSO_START",
	"BRCM_WPS_EVT_STA_ASSO_SUCCESS",
	"BRCM_WPS_EVT_STA_ASSO_FAIL",
	"BRCM_WPS_EVT_REG_PROG_START",
	"BRCM_WPS_EVT_REG_PROG_SEND_MSG",
	"BRCM_WPS_EVT_REG_PROG_REV_MSG",
	"BRCM_WPS_EVT_REG_PROG_NO_RESP",
	"BRCM_WPS_EVT_REG_PROG_SUCCESS",
	"BRCM_WPS_EVT_REG_PROG_FAIL",
	"BRCM_WPS_EVT_ABORT",
	"BRCM_WPS_EVT_ABORTED"
};


#define STATUS_STR(status)		g_szStatusStr[status]



// CAboutDlg dialog used for App About
brcm_wpscli_status WpscliAsyncCallback(brcm_wpscli_request_ctx cb_context,
									brcm_wpscli_status status, 
									brcm_wpscli_status_cb_data cb_data);

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// Cwpscli_test_guiDlg dialog



Cwpscli_test_guiDlg::Cwpscli_test_guiDlg(CWnd* pParent /*=NULL*/)
	: CDialog(Cwpscli_test_guiDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	m_bStopScan = FALSE;

	m_eCurrentState = WPS_STATE_IDLE;
}

void Cwpscli_test_guiDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LOG, m_Status);
}

BEGIN_MESSAGE_MAP(Cwpscli_test_guiDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_ENABLE_WPS, &Cwpscli_test_guiDlg::OnBnClickedEnableWps)
	ON_BN_CLICKED(IDC_ADD_ENROLLEE, &Cwpscli_test_guiDlg::OnBnClickedAddEnrollee)
	ON_BN_CLICKED(IDC_CANCEL_WPS, &Cwpscli_test_guiDlg::OnBnClickedCancelWps)
	ON_BN_CLICKED(IDC_STA_ENR_JOIN, &Cwpscli_test_guiDlg::OnBnClickedStaEnrJoin)
	ON_BN_CLICKED(IDC_SCAN_WPS_AP, &Cwpscli_test_guiDlg::OnBnClickedScanWpsAp)
	ON_BN_CLICKED(IDOK, &Cwpscli_test_guiDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// Cwpscli_test_guiDlg message handlers

BOOL Cwpscli_test_guiDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	g_ctxCliRequest.test_dlg = this;

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void Cwpscli_test_guiDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void Cwpscli_test_guiDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR Cwpscli_test_guiDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void Cwpscli_test_guiDlg::OnBnClickedEnableWps()
{
	brcm_wpscli_open(NULL, BRCM_WPSCLI_ROLE_SOFTAP, &g_ctxCliRequest, (brcm_wpscli_status_cb)WpscliAsyncCallback);

	brcm_wpscli_softap_enable_wps();

	brcm_wpscli_close();
}

void Cwpscli_test_guiDlg::OnBnClickedAddEnrollee()
{
	brcm_wpscli_nw_settings nw_settings;
//	uint8 sta_mac[] = {0x00, 0x1e, 0x4c, 0x78, 0x0d, 0x9d};
	uint8 sta_mac[6] = { 0 };

	nw_settings.authType = BRCM_WPS_AUTHTYPE_WPAPSK;
	nw_settings.encrType = BRCM_WPS_ENCRTYPE_TKIP;
	strcpy(nw_settings.nwKey, "password");
	strcpy(nw_settings.ssid, "softap_ssid");
	nw_settings.wepIndex = 0;

	brcm_wpscli_open(NULL, BRCM_WPSCLI_ROLE_SOFTAP, &g_ctxCliRequest, (brcm_wpscli_status_cb)WpscliAsyncCallback);

	brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW, BRCM_WPS_PWD_TYPE_PBC, NULL, &nw_settings, 120, sta_mac);

	brcm_wpscli_close();
}

void Cwpscli_test_guiDlg::OnBnClickedCancelWps()
{
	brcm_wpscli_abort();
}

void Cwpscli_test_guiDlg::AddLog(const CString& strLog)
{
	m_Status.InsertString(index++, strLog);
}

void Cwpscli_test_guiDlg::AddLog(const char *cszLog)
{
	m_Status.InsertString(index++, CString(cszLog));
}

void Cwpscli_test_guiDlg::WpsEnd()
{
	brcm_wpscli_close();
}

void Cwpscli_test_guiDlg::OnBnClickedStaEnrJoin()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	char buf[BRCM_WPS_MAX_AP_NUMBER*sizeof(brcm_wpscli_ap_entry) + sizeof(brcm_wpscli_ap_list)] = { 0 };
	uint32 ap_total;
	brcm_wpscli_ap_entry *ap;
	int i;
	brcm_wpscli_nw_settings nw_cred;
	uint8 wsec = 2;

	brcm_wpscli_open(NULL, BRCM_WPSCLI_ROLE_STA, &g_ctxCliRequest, (brcm_wpscli_status_cb)WpscliAsyncCallback);

	// Get the list of wps APs
	status = brcm_wpscli_sta_get_wps_ap_list((brcm_wpscli_ap_list *)buf, 
											sizeof(brcm_wpscli_ap_entry)*BRCM_WPS_MAX_AP_NUMBER, 
											&ap_total);
	if(status == WPS_STATUS_SUCCESS) 
	{
		for(i=0; i<ap_total; i++)
		{
			ap = &(((brcm_wpscli_ap_list *)buf)->ap_entries[i]);

			// Continue to make connection
			if(ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC)
			{
				m_bStopScan = TRUE;
				brcm_wpscli_sta_start_wps(ap->ssid,
										wsec,
										ap->bssid,
										0, 0,
										BRCM_WPS_MODE_STA_ENR_JOIN_NW, 
										BRCM_WPS_PWD_TYPE_PBC, 
										NULL,
										120,
										&nw_cred);
				break;
			}
		}
	}

	brcm_wpscli_close();
}

brcm_wpscli_status WpscliAsyncCallback(brcm_wpscli_request_ctx cb_context,
									brcm_wpscli_status cb_status, 
									brcm_wpscli_status_cb_data cb_data)
{
	char szLog[1024] = { 0 };

	Cwpscli_test_guiDlg *pDlg = ((test_ui_contex *)cb_context)->test_dlg;

	sprintf(szLog, "%s", STATUS_STR(cb_status));

	if(cb_context)
	{
		Cwpscli_test_guiDlg *dlg = ((test_ui_contex *)cb_context)->test_dlg;
		if(dlg)
			dlg->m_Status.InsertString(index++, CString(szLog));
	}

	return WPS_STATUS_SUCCESS;
}


void Cwpscli_test_guiDlg::OnBnClickedScanWpsAp()
{
	brcm_wpscli_open(NULL, BRCM_WPSCLI_ROLE_STA, &g_ctxCliRequest, (brcm_wpscli_status_cb)WpscliAsyncCallback);

	uint32 number;

	brcm_wpscli_sta_search_wps_ap(&number);

	brcm_wpscli_close();
}

void Cwpscli_test_guiDlg::OnBnClickedOk()
{
	brcm_wpscli_close();

	OnOK();
}
