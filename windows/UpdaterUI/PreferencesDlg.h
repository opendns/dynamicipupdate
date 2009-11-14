// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PREFERENCES_DLG_H__
#define PREFERENCES_DLG_H__

#include "resource.h"

class CPreferencesDlg : public CDialogImpl<CPreferencesDlg>
{
	static const COLORREF colWinBg = RGB(0xf7, 0xfb, 0xff);

public:
	enum { IDD = IDD_DIALOG_PREFERENCES };

	BEGIN_MSG_MAP(CPreferencesDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MSG_WM_ERASEBKGND(OnEraseBkgnd)
		MSG_WM_CTLCOLORSTATIC(OnCtlColorStatic)
		COMMAND_ID_HANDLER(IDOK, OnButtonOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnButtonCancel)
	END_MSG_MAP()

	~CPreferencesDlg() {}

	void SetCheckValue(int ctrlId, char *prefName)
	{
		BOOL val = GetPrefValBool(prefName);
		CButton b = GetDlgItem(ctrlId);
		b.SetCheck(val);
	}

	void SetCheckValueInverted(int ctrlId, char *prefName)
	{
		BOOL val = !GetPrefValBool(prefName);
		CButton b = GetDlgItem(ctrlId);
		b.SetCheck(val);
	}

	void SetPrefFromCheckValue(int ctrlId, char **prefName)
	{
		CButton b =  GetDlgItem(ctrlId);
		BOOL checked = b.GetCheck();
		SetPrefValBool(prefName, checked);
	}

	void SetPrefFromCheckValueInverted(int ctrlId, char **prefName)
	{
		CButton b =  GetDlgItem(ctrlId);
		BOOL checked = b.GetCheck();
		SetPrefValBool(prefName, !checked);
	}

	BOOL OnInitDialog(CWindow /* wndFocus */, LPARAM /* lInitParam */)
	{
		CenterWindow(GetParent());
		SetCheckValue(IDC_CHECK_SEND_DNS_OMATIC, g_pref_dns_o_matic);
		SetCheckValue(IDC_CHECK_RUN_HIDDEN, g_pref_run_hidden);
		SetCheckValue(IDC_CHECK_DONT_NOTIFY_ABOUT_ERRORS, g_pref_disable_nagging);
		SetCheckValueInverted(IDC_CHECK_DISABLE_IP_UPDATES, g_pref_send_updates);
		return FALSE;
	}

	BOOL OnEraseBkgnd(CDCHandle dc)
	{
		CRect		rc;
		GetClientRect(rc);
		dc.FillSolidRect(rc, colWinBg);
		return 1;
	}

	HBRUSH OnCtlColorStatic(CDCHandle dc, CWindow wnd)
	{
		HBRUSH br = CommonOnCtlColorStatic(dc, wnd);
		if (0 == br)
			SetMsgHandled(false);
		return br;
	}

	void UpdatePrefsValues()
	{
		SetPrefFromCheckValue(IDC_CHECK_SEND_DNS_OMATIC, &g_pref_dns_o_matic);
		SetPrefFromCheckValue(IDC_CHECK_RUN_HIDDEN, &g_pref_run_hidden);
		SetPrefFromCheckValue(IDC_CHECK_DONT_NOTIFY_ABOUT_ERRORS, &g_pref_disable_nagging);
		SetPrefFromCheckValueInverted(IDC_CHECK_DISABLE_IP_UPDATES, &g_pref_send_updates);
	}

	LRESULT OnButtonOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		UpdatePrefsValues();
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

