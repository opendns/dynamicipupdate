// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PREFERENCES_DLG_H__
#define PREFERENCES_DLG_H__

#include "resource.h"

class CPreferencesDlg : public CDialogImpl<CPreferencesDlg>
{

public:
	enum { IDD = IDD_DIALOG_PREFERENCES };

	BEGIN_MSG_MAP(CPreferencesDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_CHECK_SEND_DNS_OMATIC, BN_CLICKED, OnSendDnsOmaticButtonClicked)
		COMMAND_HANDLER_EX(IDC_CHECK_RUN_HIDDEN, BN_CLICKED, OnRunHiddenButtonClicked)
		COMMAND_HANDLER_EX(IDC_CHECK_DISABLE_IP_UPDATES, BN_CLICKED, OnDisableIpUpdatesButtonClicked)
		COMMAND_ID_HANDLER(IDOK, OnButtonOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnButtonCancel)
	END_MSG_MAP()

	~CPreferencesDlg() {}

	BOOL OnInitDialog(CWindow /* wndFocus */, LPARAM /* lInitParam */)
	{
		CenterWindow(GetParent());

		return FALSE;
	}

	void OnSendDnsOmaticButtonClicked(UINT /*uNotifyCode*/, int /*nID*/, CWindow /* wndCtl */)
	{
		//CButton b = wndCtl;
		//BOOL checked = b.GetCheck();
		//SetPrefValBool(&g_pref_send_updates, checked);
	}

	void OnRunHiddenButtonClicked(UINT /*uNotifyCode*/, int /*nID*/, CWindow /* wndCtl */)
	{
	}

	void OnDisableIpUpdatesButtonClicked(UINT /*uNotifyCode*/, int /*nID*/, CWindow /* wndCtl */)
	{
	}

	LRESULT OnButtonOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

	LRESULT OnButtonCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}
};

#endif

