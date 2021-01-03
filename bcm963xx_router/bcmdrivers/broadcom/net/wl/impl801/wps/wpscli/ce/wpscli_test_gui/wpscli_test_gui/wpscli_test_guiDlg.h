// wpscli_test_guiDlg.h : header file
//

#pragma once

#include <wpscli_api.h>

// CWPSCLITestGUIDlg dialog
class CWPSCLITestGUIDlg : public CDialog
{
// Construction
public:
	CWPSCLITestGUIDlg(CWnd* pParent = NULL);	// standard constructor
	void UpdateStatus(LPCTSTR pszStatus);
// Dialog Data
	enum { IDD = IDD_WPSCLI_TEST_CMD_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy(); 
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedEnroll();

private:
	afx_msg void OnBnClickedPinMode();
	afx_msg void OnBnClickedRefresh();

	void test_sta_side();
	void test_softap_side();
	void Refresh();
	void Clear();
	LONG AdjustControls();

	CEdit m_edtPinValue;
	CEdit m_edtStatus;
	BOOL m_bPinMode;
	char m_pin[80];
	BOOL bWpsCancelled;
	CListCtrl m_lstNetworks;
	afx_msg void OnBnClickedCancel();
	CComboBox m_listWPSMode;
	void UpdateControls(int iSel);
	void UpdatePinMode(BOOL bPinMode);
	brcm_wpscli_role m_WPSRole;
	afx_msg void OnNMKillfocusNetworks(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnNMSetfocusNetworks(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnEnSetfocusPinString();
	afx_msg void OnEnChangePinString();
	afx_msg void OnCbnSelchangeComboMode();
};
