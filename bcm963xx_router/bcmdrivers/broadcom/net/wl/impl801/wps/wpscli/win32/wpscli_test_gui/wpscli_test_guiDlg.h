/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_test_guiDlg.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: GUI diaglog header
 *
 */
#pragma once
#include "afxwin.h"
#include "wpscli_api.h"

enum WPS_STATE {
	WPS_STATE_IDLE,
	WPS_STATE_SCANNING,
	WPS_STATE_ASSOCIATING,
	WPS_STATE_PROTOCOL
};


// Cwpscli_test_guiDlg dialog
class Cwpscli_test_guiDlg : public CDialog
{
// Construction
public:
	Cwpscli_test_guiDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_WPSCLI_TEST_GUI_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedEnableWps();
	afx_msg void OnBnClickedAddEnrollee();
	afx_msg void OnBnClickedCancelWps();
	afx_msg void OnBnClickedStaEnrJoin();
	afx_msg void OnBnClickedScanWpsAp();

	void AddLog(const CString& strLog);
	void AddLog(const char *cszLog);
	void WpsEnd();
	void on_wps_evt_sta_wps_ap_found(brcm_wpscli_status wps_err, brcm_wpscli_status_cb_data cb_data);

	CListBox m_Status;

	BOOL m_bStopScan;

	WPS_STATE m_eCurrentState;
public:
	afx_msg void OnBnClickedOk();
};
