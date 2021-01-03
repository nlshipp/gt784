/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_test_gui.h,v 1.1 2010/08/05 21:59:03 ywu Exp $
 *
 * Description: main header file for the PROJECT_NAME application
 *
 */
#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// Cwpscli_test_guiApp:
// See wpscli_test_gui.cpp for the implementation of this class
//

class Cwpscli_test_guiApp : public CWinApp
{
public:
	Cwpscli_test_guiApp();

// Overrides
	public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern Cwpscli_test_guiApp theApp;
