// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SIGN_IN_DLG_H__
#define SIGN_IN_DLG_H__

#include "resource.h"
#include "JsonParser.h"
#include "JsonApiResponses.h"
#include "Prefs.h"
#include "StrUtil.h"
#include "MiscUtil.h"

enum {
	WM_HTTP_SIGN_IN = WM_APP + 32,
};

class CSignInDlg : public CDialogImpl<CSignInDlg>
{
	CFont m_fBold;
	CFont m_fBig;

	CStatic m_staticSignIn;
	CStatic m_staticUsername;
	CStatic m_staticPwd;
	CEdit m_editUsername;
	CEdit m_editPwd;
	CButton m_buttonSignIn;

	bool m_checkingUsernamePassword;

	char *m_username;
	char *m_pwd;

public:
	enum { IDD = IDD_DIALOG_SIGNIN };

	BEGIN_MSG_MAP(CSignInDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		MESSAGE_HANDLER_EX(WM_HTTP_SIGN_IN, OnSignIn)
		COMMAND_CODE_HANDLER(EN_CHANGE, OnEditChanged)
		NOTIFY_HANDLER(IDC_SYSLINK_FORGOT_PASSWORD, NM_CLICK, OnSysLinkForgotPassword) 
		NOTIFY_HANDLER(IDC_SYSLINK_CREATE_ACCOUNT, NM_CLICK, OnSysLinkCreateAccount) 
		COMMAND_ID_HANDLER(IDOK, OnButtonSignIn)
		COMMAND_ID_HANDLER(IDCANCEL, OnButtonCancel)
	END_MSG_MAP()

	~CSignInDlg()
	{
		FreeUsernamePwd();
	}

	void FreeUsernamePwd()
	{
		free(m_username);
		m_username = NULL;
		free(m_pwd);
		m_pwd = NULL;
	}

	LRESULT OnSysLinkForgotPassword(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	{
		LaunchUrl(FORGOT_PASSWORD_URL);
		return 0;
	}

	LRESULT OnSysLinkCreateAccount(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
	{
		LaunchUrl(CREATE_ACCOUNT_URL);		
		return 0;
	}
	
	BOOL OnInitDialog(CWindow /* wndFocus */, LPARAM /* lInitParam */)
	{
		m_checkingUsernamePassword = false;
		m_username = NULL;
		m_pwd = NULL;

		CenterWindow(GetParent());

		CLogFont logFontBig(AtlGetDefaultGuiFont());
		logFontBig.SetBold();
		logFontBig.SetHeight(12);
		m_fBig.Attach(logFontBig.CreateFontIndirect());

		m_staticSignIn = GetDlgItem(IDC_STATIC_SIGNIN);
		m_staticSignIn.SetFont(m_fBig);

		CLogFont logFontBold(AtlGetDefaultGuiFont());
		logFontBold.SetBold();
		m_fBold.Attach(logFontBold.CreateFontIndirect());

		m_staticUsername = GetDlgItem(IDC_STATIC_USERNAME);
		m_staticUsername.SetFont(m_fBold);

		m_staticPwd = GetDlgItem(IDC_STATIC_PASSWORD);
		m_staticPwd.SetFont(m_fBold);

		m_editUsername = GetDlgItem(IDC_EDIT_USERNAME);
		m_editPwd = GetDlgItem(IDC_EDIT_PASSWORD);

		m_buttonSignIn = GetDlgItem(IDOK);
		SetSignInButtonStatus();

		m_editUsername.SetFocus();

		return FALSE;
	}

	void SetSignInButtonStatus()
	{
		BOOL enableSignIn = FALSE;
		if (!m_checkingUsernamePassword) {
			int pwdLen = m_editPwd.GetWindowTextLength();
			int usernameLen = m_editUsername.GetWindowTextLength();
			if (pwdLen > 0 && usernameLen > 0)
				enableSignIn = TRUE;
		}
		m_buttonSignIn.EnableWindow(enableSignIn);
	}

	LRESULT OnEditChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hwndCtrl*/, BOOL& /*bHandled*/)
	{
		SetSignInButtonStatus();
		return 0;
	}

	LRESULT OnSignIn(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/)
	{
		bool endDialog = false;
		char *jsonTxt = NULL;
		JsonEl *json = NULL;
		HttpResult *ctx = (HttpResult*)wParam;
		assert(ctx);
		if (!ctx || !ctx->IsValid()) {
			slognl("OnSignIn() - ctx not valid");
			goto Error;
		}

		DWORD dataSize;
		jsonTxt = (char*)ctx->data.getData(&dataSize);
		json = ParseJsonToDoc(jsonTxt);
		if (!json) {
			if (jsonTxt) {
				slogfmt("OnSignIn() failed to parse json: '%s'\n", jsonTxt);
			} else {
				slognl("OnSignIn() failed to parse json: <NULL>");
			}
			goto Error;
		}

		WebApiStatus status = GetApiStatus(json);
		if (WebApiStatusSuccess != status) {
			if (WebApiStatusFailure == status) {
				long err;
				bool ok = GetApiError(json, &err);
				if (ok && (ERR_BAD_USERNAME_PWD == err))
					goto BadUsernamePwd;
			}
			slogfmt("OnSignIn() bad json status: %d, json: '%s'\n", (int)status, jsonTxt);
			goto Error;
		}

		char *tokenTxt = GetApiResponseToken(json);
		if (!tokenTxt) {
			slogfmt("OnSignIn() no token, json: '%s'\n", jsonTxt);
			goto Error;
		}

		// we got the token, so username and password are good
		// so save the info in preferences
		SetPrefVal(&g_pref_user_name, m_username);
		SetPrefVal(&g_pref_token, tokenTxt);
		PreferencesSave();
		endDialog = true;
Exit:
		m_checkingUsernamePassword = false;
		delete ctx;
		free(jsonTxt);
		JsonElFree(json);
		if (endDialog)
			EndDialog(IDOK);
		else
			SetSignInButtonStatus();
		return 0;

Error:
		MessageBox(_T("There was an error verifying username and password"), MAIN_FRAME_TITLE);
		goto Exit;

BadUsernamePwd:
		MessageBox(_T("Not a valid username or password"), MAIN_FRAME_TITLE);
		goto Exit;
	}

	void StartCheckUsernamePassword(char *userName, char *pwd)
	{
		m_checkingUsernamePassword = true;
		SetSignInButtonStatus();

		CString params = ApiParamsSignIn(userName, pwd);
		const char *paramsTxt = TStrToStr(params);
		const char *apiHost = GetApiHost();
		bool apiHostIsHttps = IsApiHostHttps();
		HttpPostAsync(apiHost, API_URL, paramsTxt, apiHostIsHttps, m_hWnd, WM_HTTP_SIGN_IN);
		free((void*)paramsTxt);
	}

	LRESULT OnButtonSignIn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		FreeUsernamePwd();
		m_username = MyGetWindowTextA(m_editUsername);
		m_pwd = MyGetWindowTextA(m_editPwd);
		if (m_username && m_pwd)
			StartCheckUsernamePassword(m_username, m_pwd);
		return 0;
	}

	LRESULT OnButtonCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	}

};

#endif
