// wpscli_test_gui.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#ifdef POCKETPC2003_UI_MODEL
#include "resourceppc.h"
#endif 

// CWPSCLITestGUIApp:
// See wpscli_test_cmd.cpp for the implementation of this class
//

class CWPSCLITestGUIApp : public CWinApp
{
public:
	CWPSCLITestGUIApp();
	
// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CWPSCLITestGUIApp theApp;
