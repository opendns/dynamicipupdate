// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#if MAIN_FRM == 3
#include "MainFrm3.h"
#include "TypoExceptions.h"

#define TXT_DIV_ACCOUNT _T("OpenDNS account")
#define TXT_DIV_NETWORK_TO_UPDATE _T("Network to update")
#define TXT_DIV_IP_ADDRESS _T("IP address")
#define TXT_DIV_STATUS _T("Using OpenDNS?")
#define TXT_DIV_UPDATE _T("Last updated")

static bool HasDynamicNetworkWithLabel(NetworkInfo *head, const char *label)
{
	while (head) {
		if (head->isDynamic) {
			if (streq(head->label, label))
				return true;
		}
		head = head->next;
	}
	return false;
}

static void LogIpUpdate(char *resp)
{
	slog("sent ip update for\nuser '");
	assert(g_pref_user_name);
	if (g_pref_user_name)
		slog(g_pref_user_name);
	slog("', \nresponse: '");
	if (resp)
		slog(resp);
	slog("'\nurl: ");
	const char *urlTxt = GetIpUpdateUrl();
	if (urlTxt)
		slog(urlTxt);
	free((void*)urlTxt);
	slog("\nhost: ");
	slog(GetIpUpdateHost());
	slog("\n");
}

static TCHAR *GetNetworkName()
{
	char *name = "default";
	if (!strempty(g_pref_hostname))
		name = g_pref_hostname;
	return StrToTStr(name);
}

static TCHAR *AccountName()
{
	char *name = "Not logged in";
	if (!strempty(g_pref_user_name))
		name = g_pref_user_name;
	return StrToTStr(name);
}

CMainFrame::CMainFrame()
{
	m_ipFromDns = IP_UNKNOWN;
	m_ipFromHttp = NULL;
	m_ipUpdateResult = IpUpdateOk;
	m_simulatedError = SE_NO_ERROR;

	m_editErrorMsgRequestedDy = 0;
	m_showUpdateMsgEdit = false;

	m_editUpdateMsgRequestedDy = 0;
	m_showStatusMsgEdit = false;

	m_editFontName = NULL;
	m_minWinDx = 320;
	// this is absolute minimum height of the window's client area
	m_minWinDy = 200;
	m_minUpdateEditDx = m_minStatusEditDx = 320 - 16;
	m_uiState = UI_STATE_VISIBLE;
	m_minutesSinceLastUpdate = 0;
	m_winBgColorBrush = ::CreateSolidBrush(colWinBg);
	m_updaterThread = NULL;
	m_newVersionSetupFilepath = NULL;
	m_forceExitOnClose = false;
	m_hiddenMode = false;
}

CMainFrame::~CMainFrame()
{
	free(m_ipFromHttp);
	free(m_editFontName);
	free(m_newVersionSetupFilepath);
	DeleteObject(m_winBgColorBrush);
	DeleteObject(m_updateBitmap);
}

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg)
{
	return CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg);
}

void CMainFrame::OnClose()
{
	BOOL sendingUpdates = GetPrefValBool(g_pref_send_updates);
	if (CanSendIPUpdates() && sendingUpdates && !IsLeftAltAndCtrlPressed() && !m_forceExitOnClose) {
		SwitchToHiddenState();
		SetMsgHandled(TRUE);
	} else {
		SetMsgHandled(FALSE);
	}
}

void CMainFrame::OnTimer(UINT_PTR /*nIDEvent */)
{
	SubmitTypoExceptionsAsync();
}

LRESULT CMainFrame::OnLButtonDown(UINT /*nFlags*/, CPoint /*point*/)
{
	SetFocus();
	return 0;
}

void CMainFrame::OnGetMinMaxInfo(LPMINMAXINFO lpMMI)
{
	lpMMI->ptMinTrackSize.x = m_minWinDx;
	lpMMI->ptMinTrackSize.y = m_minWinDy;
}

// returns true if the time has changed and we need to update
// the text in UI
bool CMainFrame::GetLastIpUpdateTime()
{
	// optimization: not showing ui => no need to update
	if (m_uiState != UI_STATE_VISIBLE)
		return false;
	int minutesSinceLastUpdate = m_updaterThread->MinutesSinceLastUpdate();
#if 0
	// we don't want to show "0 minutes ago", so show at least "1 minute ago"
	if (0 == minutesSinceLastUpdate)
		minutesSinceLastUpdate = 1;
#endif
	if (minutesSinceLastUpdate == m_minutesSinceLastUpdate)
		return false;
	m_minutesSinceLastUpdate = minutesSinceLastUpdate;
	return true;
}

void CMainFrame::UpdateLastUpdateText()
{
	if (!GetLastIpUpdateTime())
		return;
	UpdateErrorEdit();
}

BOOL CMainFrame::OnIdle()
{
	return FALSE;
}

void CMainFrame::ChangeNetwork(int supressFlags)
{
	StartDownloadNetworks(g_pref_token, supressFlags);
	UpdateErrorEdit();
}

LRESULT CMainFrame::OnSelChange(LPNMHDR /*pnmh*/)
{
	SetFocus();
	return 0;
}

void CMainFrame::OnChangeAccount(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	ChangeAccount();
}

void CMainFrame::OnChangeNetwork(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	ChangeNetwork(0);
}

void CMainFrame::OnSendUpdate(UINT /*uNotifyCode*/, int /*nID*/, CWindow /*wndCtl*/)
{
	m_updaterThread->ForceSendIpUpdate();
	UpdateErrorEdit();
}

void CMainFrame::OnSendUpdatesButtonClicked(UINT /*uNotifyCode*/, int /*nID*/, CWindow wndCtl)
{
	CButton b = wndCtl;
	BOOL checked = b.GetCheck();
	SetPrefValBool(&g_pref_send_updates, checked);
	PreferencesSave();
	UpdateErrorEdit();
}

// sent by rich edit control so that we can know its desired height
LRESULT CMainFrame::OnRequestResize(LPNMHDR pnmh)
{
	REQRESIZE* r = (REQRESIZE*)pnmh;

	if (IDC_EDIT_UPDATE == pnmh->idFrom) {
		m_editUpdateMsgRequestedDy = RectDy(r->rc);
		return 0;
	}

	assert(IDC_EDIT_STATUS == pnmh->idFrom);
	m_editErrorMsgRequestedDy = RectDy(r->rc);
	return 0;
}

bool CMainFrame::IsLink(HWND hwnd)
{
	if (hwnd == m_linkAbout.m_hWnd)
		return true;
	if (hwnd == m_linkLearnSetup.m_hWnd)
		return true;
	return false;
}

bool CMainFrame::IsCheckBoxButton(HWND hwnd)
{
	if (hwnd == m_buttonSendIpUpdates.m_hWnd)
		return true;
	return false;
}

bool CMainFrame::IsStatic(HWND /*hwnd*/)
{
	return false;
}

static void DrawDividerLine(CDCHandle *dc, int y, int x1, int x2, int x3, int x4)
{
	RECT r = {x1, y, x2, y+1};

	if (lineType == ETCHED_LINE)
	{
		dc->DrawEdge(&r, EDGE_ETCHED, BF_TOP);
		r.left = x3;
		r.right = x4;
		dc->DrawEdge(&r, EDGE_ETCHED, BF_TOP);
	} else if (lineType == SOLID_LINE)
	{
		dc->FillSolidRect(&r, colDivLine);
		r.left = x3;
		r.right = x4;
		dc->FillSolidRect(&r, colDivLine);
	} else
		assert(0);
}

void CMainFrame::DrawDivider(CDCHandle dc, const TCHAR *txt, CRect& rect)
{
	CRect      rc;
	GetClientRect(rc);

	HFONT prevFont = dc.SelectFont(m_dividerTextFont);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(colBlack);
	int x = LEFT_MARGIN + DIVIDER_TEXT_LEFT_MARGIN + 12;
	int y = rect.top + DIVIDER_TEXT_TOP_MARGIN;
	dc.TextOut(x, y, txt);

	SIZE textSize;
	dc.GetTextExtent(txt, -1, &textSize);

	int lineY = rect.top + RectDy(rect) / 2 + DIV_LINE_OFF_Y;
	int x1 = LEFT_MARGIN;
	int x2 = LEFT_MARGIN + DIVIDER_TEXT_LEFT_MARGIN + 10;
	int x3 = x2 + 4 + textSize.cx + 2;
	int x4 = rc.right - RIGHT_MARGIN;
	DrawDividerLine(&dc, lineY, x1, x2, x3 ,x4);

	dc.SelectFont(prevFont);
}

void CMainFrame::DrawErrorText(CDCHandle *dc, int x, int y, const TCHAR *txt)
{
	HFONT fontPrev = dc->SelectFont(m_dividerTextFont); // reusing, it's about being bold
	COLORREF colPrev = dc->SetTextColor(colRed2);
	dc->TextOut(x, y, txt);
	dc->SetTextColor(colPrev);
	dc->SelectFont(fontPrev);
}

BOOL CMainFrame::OnEraseBkgnd(CDCHandle dc)
{
	CRect		rc;
	TCHAR *		txt;
	int			x, y;

	GetClientRect(rc);
	{
		// Double-buffering. In our case we don't draw enough to make a difference
		CMemoryDC dc(dc, rc);
		CDCHandle dc2(dc);

		// paint top bar in a bluish gradient
		TRIVERTEX        vert[2] ;
		GRADIENT_RECT    gRect;
		vert[0].x      = 0;
		vert[0].y      = 0;
		vert[0].Red    = 0x6b00;
		vert[0].Green  = 0x7900;
		vert[0].Blue   = 0xde00;
		vert[0].Alpha  = 0x0000;

		vert[1].x      = RectDx(rc);
		vert[1].y      = TOP_BAR_DY; 
		vert[1].Red    = 0x8400;
		vert[1].Green  = 0xa600;
		vert[1].Blue   = 0xef00;
		vert[1].Alpha  = 0x0000;

		gRect.UpperLeft  = 0;
		gRect.LowerRight = 1;
		GradientFill(dc, vert, 2, &gRect, 1, GRADIENT_FILL_RECT_H);

		// paint solid background everywhere except in top bar
		//rc.MoveToY(TOP_BAR_DY);
		rc.MoveToY(0);
		dc2.FillSolidRect(rc, colWinBg);

		DrawDivider(dc2, TXT_DIV_ACCOUNT, m_txtAccountRect);
		if (IsLoggedIn())
			DrawDivider(dc2, TXT_DIV_NETWORK_TO_UPDATE, m_txtNetworkRect);
		DrawDivider(dc2, TXT_DIV_IP_ADDRESS, m_txtIpAddressRect);
		DrawDivider(dc2, TXT_DIV_STATUS, m_txtStatusRect);
		if (ShowLastUpdated())
			DrawDivider(dc2, TXT_DIV_UPDATE, m_txtUpdateRect);

		HFONT prevFont = dc.SelectFont(m_textFont);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(colBlack);
		x = LEFT_MARGIN + DIVIDER_TEXT_LEFT_MARGIN;

		// Draw account
		y = m_txtAccountRect.bottom + DIVIDER_Y_SPACING + 6;
		if (IsLoggedIn()) {
			txt = AccountName();
			dc.TextOut(x, y, txt);
			free(txt);
		} else {
			DrawErrorText(&dc2, x, y, _T("Not logged in"));
		}

		// Draw network name
		if (IsLoggedIn()) {
			y = m_txtNetworkRect.bottom + DIVIDER_Y_SPACING + 6;
			if (NoNetworksConfigured()) {
				DrawErrorText(&dc2, x, y, _T("No networks"));
			} else if (NetworkNotSelected()) {
				DrawErrorText(&dc2, x, y, _T("Network not selected"));
			} else if (NoDynamicNetworks()) {
				DrawErrorText(&dc2, x, y, _T("No dynamic network"));
			} else {
				txt = GetNetworkName();
				dc.TextOut(x, y, txt);
				free(txt);
			}
		}

		// Draw IP address
		y = m_txtIpAddressRect.bottom + DIVIDER_Y_SPACING + 6;
		txt = IpAddress();
		dc.TextOut(x, y, txt);
		free(txt);

		// Draw "Yes"/"No" (for 'Using OpenDNS?' part)
		y = m_txtStatusRect.bottom + DIVIDER_Y_SPACING + 6;
		if (IsUsingOpenDns())
			dc.TextOut(x, y, _T("Yes"));
		else
			DrawErrorText(&dc2, x, y, _T("No. Learn how to"));

		// Draw last updated time (e.g. "5 minutes ago")
		if (ShowLastUpdated()) {
			y = m_txtUpdateRect.bottom + DIVIDER_Y_SPACING + 6;
			BOOL sendUpdates = GetPrefValBool(g_pref_send_updates);
			if (sendUpdates) {
				txt = LastUpdateTxt();
				dc.TextOut(x, y, txt);
				free(txt);
			} else {
				DrawErrorText(&dc2, x, y, _T("Updates disabled"));
			}
		}

		dc.SelectFont(prevFont);

		if (m_showStatusMsgEdit) {
			// draw frame around edit box
			RECT editRect;
			m_editErrorMsg.GetWindowRect(&editRect);
			editRect.top -= 6;
			editRect.bottom += 6;
			editRect.left -= 4;
			editRect.right += 4;
			ScreenToClient(&editRect);
			dc.FillSolidRect(&editRect, colEditFrame);

			editRect.top += 1;
			editRect.bottom -= 1;
			editRect.left += 1;
			editRect.right -= 1;
			dc.FillSolidRect(&editRect, colEditBg);
		}

		if (m_showUpdateMsgEdit) {
			// draw frame around edit box
			RECT editRect;
			m_editUpdateMsg.GetWindowRect(&editRect);
			editRect.top -= 6;
			editRect.bottom += 6;
			editRect.left -= (4 + UPDATE_BITMAP_DX_TOTAL);
			editRect.right += 4;
			ScreenToClient(&editRect);
			dc.FillSolidRect(&editRect, colEditUpdateFrame);

			editRect.top += 1;
			editRect.bottom -= 1;
			editRect.left += 1;
			editRect.right -= 1;
			dc.FillSolidRect(&editRect, colEditUpdateBg);

			// draw the 'update available' bitmap
			{
				HDC			bitmapDC;
				BITMAP		bm;
				HBITMAP 	hOldBitmap;
				GetObject(m_updateBitmap, sizeof(BITMAP), &bm);
				int			bmpX = editRect.left + 2 + UPDATE_BITMAP_MARGIN_X_LEFT;
				int			bmpY = editRect.bottom - bm.bmHeight - 3;
				bitmapDC = CreateCompatibleDC(dc);
				hOldBitmap = (HBITMAP)SelectObject(bitmapDC, m_updateBitmap);

				::BitBlt(dc, bmpX, bmpY, bm.bmWidth, bm.bmHeight, bitmapDC, 0, 0, SRCCOPY);

				SelectObject(bitmapDC, hOldBitmap);
				DeleteObject(bitmapDC);
			}
		}


#if 0
		// draw top bar text
		HFONT prevFont = dc.SelectFont(m_topBarFont);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(colWhite);
		dc.TextOut(m_topBarX, m_topBarY, TOP_BAR_TXT);
		dc.SelectFont(prevFont);
#endif
	}
	return 1;
}

HBRUSH CMainFrame::OnCtlColorStatic(CDCHandle dc, CWindow wnd)
{
	HWND hwnd = wnd;
	// TODO: could probabably do IsLink() and IsStatic() by
	// comparing WINDOWINFO.atomWindowType (obtained via GetWindowInfo())
	// with ATOM corresponding to syslink and static classes found
	// with GetClassInfo()
	if (IsLink(hwnd)) {
		dc.SetBkColor(colWinBg);
		return 0;
	} else if (IsStatic(hwnd)) {
		//dc.SetBkColor(colWinBg);
		dc.SetTextColor(colBlack);
		dc.SetBkMode(TRANSPARENT);
	} else if (IsCheckBoxButton(hwnd)) {
		return m_winBgColorBrush;
	} else {
		SetMsgHandled(false);
		return 0;
	}

	return (HBRUSH)::GetStockObject(NULL_BRUSH);
}

// returns true if we get valid ip address from both
// http query and dns query and they are not the same
// (it does happen)
bool CMainFrame::DnsVsHttpIpMismatch()
{
	if (!RealIpAddress(m_ipFromDns) || !m_ipFromHttp)
		return false;
	return (0 != m_ipFromDnsStr.Compare(m_ipFromHttp));
}

TCHAR *CMainFrame::LastUpdateTxt()
{
	if (m_minutesSinceLastUpdate < 0)
		return NULL;

	CString s;
	TCHAR *timeTxt = FormatUpdateTime(m_minutesSinceLastUpdate);
	if (timeTxt) {
		s += timeTxt;
		free(timeTxt);
	}
	s += " ago.";
	const TCHAR* res = s;
	return tstrdup(res);
}

TCHAR* CMainFrame::IpAddress()
{
	IP4_ADDRESS a = m_ipFromDns;
	if (!RealIpAddress(a))
		return NULL;

	CString s;
	s.Format(_T("%u.%u.%u.%u"), (a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);
	const TCHAR *res = s;
	return tstrdup(res);
}

bool CMainFrame::IsLoggedIn()
{
	if (SE_NOT_LOGGED_IN == m_simulatedError)
		return false;
	if (strempty(g_pref_user_name))
		return false;
	if (strempty(g_pref_token))
		return false;
	return true;
}

bool CMainFrame::IsUsingOpenDns()
{
	if (IP_NOT_USING_OPENDNS == m_ipFromDns)
		return false;
	if (SE_NOT_USING_OPENDNS == m_simulatedError)
		return false;
	return true;
}

// return true if the user has 
bool CMainFrame::NoNetworksConfigured()
{
	if (streq(UNS_NO_NETWORKS, g_pref_user_networks_state))
		return true;
	if (SE_NO_NETWORKS_CONFIGURED == m_simulatedError)
		return true;
	return false;
}


bool CMainFrame::NoDynamicNetworks()
{
	if (streq(UNS_NO_DYNAMIC_IP_NETWORKS, g_pref_user_networks_state))
		return true;
	if (SE_NO_DYNAMIC_IP_NETWORKS == m_simulatedError)
		return true;
	return false;
}

bool CMainFrame::ShowLastUpdated()
{
	return IsLoggedIn() && !NetworkNotSelected() && !NoNetworksConfigured() && !NoDynamicNetworks();
}

bool CMainFrame::NoInternetConnectivity()
{
	if (IP_DNS_RESOLVE_ERROR == m_ipFromDns)
		return true;
	if (SE_NO_INTERNET == m_simulatedError)
		return true;
	return false;
}

bool CMainFrame::NetworkNotSelected()
{
	if (streq(UNS_NO_NETWORK_SELECTED, g_pref_user_networks_state))
		return true;
	if (SE_NO_NETWORK_SELECTED == m_simulatedError)
		return true;
	return false;
}

void CMainFrame::BuildUpdateEditRtf(RtfTextInfo& ti)
{
	m_showUpdateMsgEdit = false;
	if (!m_newVersionSetupFilepath)
		return;

	m_showUpdateMsgEdit = true;
	ti.Init(m_editFontName, EDIT_FONT_SIZE);
	ti.StartFgCol(RtfTextInfo::ColBlack);
	ti.StartBoldStyle();
	ti.StartCentered();
	ti.AddTxt(_T("New version is available! "));
	ti.AddLink(_T("Install now"), LINK_INSTALL_NEW_VERSION);
	ti.AddTxt(_T("."));
	ti.EndStyle();
	ti.EndStyle();
	ti.EndCol();
	ti.End();
}

void CMainFrame::BuildStatusEditRtf(RtfTextInfo& ti)
{
	CString s;
	m_showStatusMsgEdit = false;

	ti.Init(m_editFontName, EDIT_FONT_SIZE);

	// show error scenarios at the end, in bold red
	ti.StartFgCol(RtfTextInfo::ColRed);
	ti.StartBoldStyle();

	if (NoInternetConnectivity()) {
		m_showStatusMsgEdit = true;
		ti.AddParasIfNeeded();
		ti.AddTxt("Looks like there's no internet connectivity.");
	}

	if (IsLoggedIn()) {
		if (NoNetworksConfigured()) {
			m_showStatusMsgEdit = true;
			ti.AddParasIfNeeded();
			ti.AddTxt("You don't have any networks. First, ");
			ti.AddLink("add a network", LINK_CONFIGURE_NETWORKS);
			ti.AddTxt(" in your OpenDNS account. Then ");
			ti.AddLink("refresh network list.", LINK_SELECT_NETWORK);
		} else if (NoDynamicNetworks()) {
			m_showStatusMsgEdit = true;
			ti.AddParasIfNeeded();
			ti.AddTxt("None of your networks is configured for dynamic IP. First, ");
			ti.AddLink("configure a network", LINK_CONFIGURE_NETWORKS);
			ti.AddTxt(" for dynamic IP in your OpenDNS account. Then ");
			ti.AddLink("select a network", LINK_SELECT_NETWORK);
		}
	}

	if ((IpUpdateNotYours == m_ipUpdateResult) || (SE_IP_NOT_YOURS == m_simulatedError)) {
		m_showStatusMsgEdit = true;
		ti.AddParasIfNeeded();
		ti.AddTxt(_T("Your IP address is taken by another user. "));
		ti.AddLink(_T("Learn more."), LINK_LEARN_MORE_IP_TAKEN);
	}

	if ((IpUpdateBadAuth == m_ipUpdateResult) || (SE_BAD_AUTH == m_simulatedError)) {
		m_showStatusMsgEdit = true;
		ti.AddParasIfNeeded();
		ti.AddTxt(_T("Your authorization token is invalid."));
	}

	bool ipMismatch = DnsVsHttpIpMismatch();
	if (ipMismatch) {
		m_showStatusMsgEdit = true;
		ti.AddParasIfNeeded();
		ti.AddTxt(_T("Your OpenDNS filtering settings might not work due to DNS IP address ("));
		ti.AddTxt(m_ipFromDnsStr);
		ti.AddTxt(_T(") and HTTP IP address ("));
		ti.AddTxt(m_ipFromHttp);
		ti.AddTxt(_T(") mismatch. "));
		ti.AddLink(_T("Learn more."), LINK_LEARN_MORE_IP_MISMATCH);
	}

	ti.EndStyle();
	ti.EndCol();

	if (g_showDebug) {
		m_showStatusMsgEdit = true;
		if (UsingDevServers()) {
			ti.AddParasIfNeeded();
			ti.AddTxt("Using dev api servers ");
		} else {
			ti.AddParasIfNeeded();
			ti.AddTxt("Using production api servers ");
		}
		ti.AddLink("(toggle)", LINK_TOGGLE_DEV_PRODUCTION);
		ti.AddTxt(".");
		ti.AddPara();

		ti.AddLink("Send IP update", LINK_SEND_IP_UPDATE);
		ti.AddTxt(" ");
		ti.AddLink("Crash me", LINK_CRASH_ME);
	} else {
		if (UsingDevServers()) {
			m_showStatusMsgEdit = true;
			ti.AddTxt("Using dev api servers.");
		}
	}

	ti.End();
}

void CMainFrame::UpdateUpdateEdit(bool doLayout)
{
	BuildUpdateEditRtf(m_rtiUpdate);
	const TCHAR *s = m_rtiUpdate.text;
#ifdef UNICODE
	// Don't know why I have to do this, but SetWindowText() with unicode
	// doesn't work (rtf codes are not being recognized)
	const char *sUtf = WstrToUtf8(s);
	m_editUpdateMsg.SetTextEx((LPCTSTR)sUtf, ST_DEFAULT, CP_UTF8);
#else
	m_editUpdateMsg.SetWindowText(s);
#endif
	SetRtfLinks(&m_editUpdateMsg, &m_rtiUpdate);

	m_editUpdateMsg.SetSelNone();
	m_editUpdateMsg.RequestResize();
	if (doLayout)
		PostMessage(WMAPP_DO_LAYOUT);
}

void CMainFrame::UpdateErrorEdit(bool doLayout)
{
	GetLastIpUpdateTime();
	BuildStatusEditRtf(m_rtiError);
	const TCHAR *s = m_rtiError.text;
#ifdef UNICODE
	// Don't know why I have to do this, but SetWindowText() with unicode
	// doesn't work (rtf codes are not being recognized)
	const char *sUtf = WstrToUtf8(s);
	m_editErrorMsg.SetTextEx((LPCTSTR)sUtf, ST_DEFAULT, CP_UTF8);
#else
	m_editErrorMsg.SetWindowText(s);
#endif
	SetRtfLinks(&m_editErrorMsg, &m_rtiError);

	m_editErrorMsg.SetSelNone();
	m_editErrorMsg.RequestResize();
	if (doLayout)
		PostMessage(WMAPP_DO_LAYOUT);
}

void CMainFrame::SetRtfLinks(CRichEditCtrl *edit, RtfTextInfo *rti)
{
	RtfLinkInfo *link = rti->firstLink;
	while (link) {
		LONG start = link->start;
		LONG end = link->end;
		edit->SetSel(start, end);
		CHARFORMAT2 cf;
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_LINK;
		cf.dwEffects = CFE_LINK;
		edit->SetCharFormat(cf, SCF_SELECTION);
		link = link->next;
	}
}

LRESULT CMainFrame::OnLinkStatusEdit(LPNMHDR pnmh)
{
	ENLINK *e = (ENLINK *)pnmh;
	if (e->msg != WM_LBUTTONDOWN)
		return 0;

	CHARRANGE chr = e->chrg;
	LONG start = chr.cpMin;
	LONG end = chr.cpMax;
	RtfLinkId linkId;
	BOOL found = m_rtiError.FindLinkFromRange(start, end, linkId);
	if (!found)
	    found = m_rtiUpdate.FindLinkFromRange(start, end, linkId);
	assert(found);
	if (!found)
		return 0;
	if (LINK_CHANGE_ACCOUNT == linkId) {
		ChangeAccount();
	} else if (LINK_CHANGE_NETWORK == linkId) {
		ChangeNetwork(0);
	} else if (LINK_SETUP_OPENDNS == linkId) {
		LaunchUrl(SETUP_OPENDNS_URL);
	} else if (LINK_SELECT_NETWORK == linkId) {
		ChangeNetwork(0);
	} else if (LINK_CONFIGURE_NETWORKS == linkId) {
		LaunchUrl(GetDashboardUrl());
	} else if (LINK_TOGGLE_DEV_PRODUCTION == linkId) {
		if (UsingDevServers())
			UseDevServers(false);
		else
			UseDevServers(true);
		UpdateErrorEdit();
	} else if (LINK_SEND_IP_UPDATE == linkId) {
		m_updaterThread->ForceSendIpUpdate();
		UpdateErrorEdit();
	} else if (LINK_CRASH_ME == linkId) {
		CrashMe();
	} else if (LINK_INSTALL_NEW_VERSION == linkId) {
		LaunchUrl(m_newVersionSetupFilepath);
		free(m_newVersionSetupFilepath);
		m_newVersionSetupFilepath = NULL;
	} else if (LINK_LEARN_MORE_IP_MISMATCH == linkId) {
		LaunchUrl(LEARN_MORE_IP_MISMATCH_URL);
	} else if (LINK_LEARN_MORE_IP_TAKEN == linkId) {
		LaunchUrl(LEARN_MORE_IP_ADDRESS_TAKEN_URL);
	} else
		assert(0);
	SetFocus();
	return 0;
}

void CMainFrame::ChangeAccount()
{
	CSignInDlg dlg;
	INT_PTR nRet = dlg.DoModal();
	if (IDCANCEL == nRet) {
		// nothing has changed
		return;
	}
	StartDownloadNetworks(g_pref_token, SupressAll);
	UpdateErrorEdit();
}

// Calculate @rectOut for a divider line with text @txt, starting at position @y,
// given current widht of client area is @clientDx.
// Returns minimum required size to display text
int CMainFrame::SizeDividerLineText(TCHAR *txt, int y, int clientDx, CRect& rectOut)
{
	CUITextSizer textSizer;
	textSizer.SetWindow(m_hWnd); // doesn't matter which window
	textSizer.SetFont(m_dividerTextFont);
	textSizer.SetText(txt);
	SIZE s = textSizer.GetIdealSize2();
	rectOut.top = y;
	int yMargins = DIVIDER_TEXT_TOP_MARGIN + DIVIDER_TEXT_BOTTOM_MARGIN;
	rectOut.bottom = y + s.cy + yMargins;

	rectOut.left = LEFT_MARGIN;
	rectOut.right = clientDx - RIGHT_MARGIN;

	int xMargins = DIVIDER_TEXT_LEFT_MARGIN + DIVIDER_TEXT_LEFT_MARGIN + LEFT_MARGIN + RIGHT_MARGIN;
	int minDx = s.cx + xMargins;
	return minDx;
}

// I want all three bugttons on the left be the same size, so this will
// calculate common dx/dy of the buttons. Dy should be always the same,
// dx is the biggest of them
void CMainFrame::SizeButtons(int& dxOut, int& dyOut)
{
	int dxMax, dy;
	int dxTmp, dyTmp;
	TCHAR *txt;

	CUITextSizer textSizer;
	textSizer.SetWindow(m_hWnd); // doesn't matter which window

	txt = MyGetWindowText(m_buttonChangeAccount);
	textSizer.GetIdealSize(txt, m_buttonsFont, dxMax, dy);
	free(txt);

	if (m_buttonChangeConfigureNetwork.IsWindowVisible()) {
		txt = MyGetWindowText(m_buttonChangeConfigureNetwork);
		textSizer.GetIdealSize(txt, m_buttonsFont, dxTmp, dyTmp);
		free(txt);
		assert(dy == dyTmp);
		if (dxTmp > dxMax)
			dxMax = dxTmp;
	}

	if (m_buttonUpdate.IsWindowVisible()) {
		txt = MyGetWindowText(m_buttonUpdate);
		textSizer.GetIdealSize(txt, m_buttonsFont, dxTmp, dyTmp);
		free(txt);
		assert(dy == dyTmp);
		if (dxTmp > dxMax)
			dxMax = dxTmp;
	}

	dxOut = dxMax + 32;
	dyOut = dy + 12;
}

void CMainFrame::DoLayout()
{
	int x, y;
	int btnDx;
	int minDx = m_minStatusEditDx + LEFT_MARGIN + RIGHT_MARGIN;
	int dxLine;
	SIZE s;
	TCHAR *txt;

	BOOL ok;
	RECT clientRect;
	ok = GetClientRect(&clientRect);
	if (!ok) return;
	int clientDx = RectDx(clientRect);
	int clientDy = RectDy(clientRect);

	if (IsLoggedIn()) {
		m_buttonChangeAccount.SetWindowText(_T("Change account"));
		m_buttonChangeConfigureNetwork.ShowWindow(SW_SHOW);
		m_buttonSendIpUpdates.ShowWindow(SW_SHOW);
	} else {
		m_buttonChangeAccount.SetWindowText(_T("Log in"));
		m_buttonChangeConfigureNetwork.ShowWindow(SW_HIDE);
		m_buttonSendIpUpdates.ShowWindow(SW_HIDE);
	}

	if (m_showStatusMsgEdit)
		m_editErrorMsg.ShowWindow(SW_SHOW);
	else
		m_editErrorMsg.ShowWindow(SW_HIDE);

	if (m_showUpdateMsgEdit)
		m_editUpdateMsg.ShowWindow(SW_SHOW);
	else
		m_editUpdateMsg.ShowWindow(SW_HIDE);

	BOOL sendUpdates = GetPrefValBool(g_pref_send_updates);
	if (sendUpdates)
		m_buttonUpdate.EnableWindow(TRUE);
	else
		m_buttonUpdate.EnableWindow(FALSE);

	if (NoNetworksConfigured())
		m_buttonChangeConfigureNetwork.SetWindowText(_T("Refresh network list"));
	else if (NetworkNotSelected() || NoDynamicNetworks())
		m_buttonChangeConfigureNetwork.SetWindowText(_T("Select network"));
	else
		m_buttonChangeConfigureNetwork.SetWindowText(_T("Change network"));

	if (ShowLastUpdated())
		m_buttonUpdate.ShowWindow(SW_SHOW);
	else
		m_buttonUpdate.ShowWindow(SW_HIDE);

	SizeButtons(btnDx, m_btnDy);

	// position "Send IP updates" check-box in the bottom right corner
	CUICheckBoxButtonSizer sendIpUpdatsSizer(m_buttonSendIpUpdates);
	static const int BTN_SEND_UPDATES_RIGHT_MARGIN = 8;
	static const int BTN_SEND_UPDATES_BOTTOM_MARGIN = 4;
	RECT pos;
	CalcFixedPositionBottomRight(clientDx, clientDy, BTN_SEND_UPDATES_RIGHT_MARGIN, BTN_SEND_UPDATES_BOTTOM_MARGIN, &sendIpUpdatsSizer, pos);
	m_buttonSendIpUpdates.MoveWindow(&pos);
	int buttonDy = RectDy(pos);

	CUILinkSizer linkSizer;
	CUITextSizer textSizer;
	textSizer.SetWindow(m_hWnd); // doesn't matter which window
	CUIButtonSizer buttonSizer;

	// position "About" link in the bottom left corner
	x = LEFT_MARGIN;
	linkSizer.SetWindow(m_linkAbout);
	linkSizer.SetFont(m_defaultGuiFont);
	s = linkSizer.GetIdealSize2();
	static const int LINK_ABOUT_BOTTOM_MARGIN = 5;
	m_linkAbout.MoveWindow(x, clientDy - s.cy - LINK_ABOUT_BOTTOM_MARGIN, s.cx, s.cy);

#if 0
	// position title in the middle of top bar
	RECT topBarRect = {0, 0, clientDx, TOP_BAR_DY};
	m_topBarRect = topBarRect;
	textSizer.SetFont(m_topBarFont);
	textSizer.SetText(TOP_BAR_TXT);
	s = textSizer.GetIdealSize2();
	//y = (TOP_BAR_DY - s.cy) / 2;
	m_topBarY = 4;
	m_topBarX = (clientDx - s.cx) / 2;
	dxLine = s.cx + 2 * LEFT_MARGIN;
	if (dxLine > minDx)
		minDx = dxLine;
#endif

	// Position "OpenDNS account" divider line
	y = Y_START;
	dxLine = SizeDividerLineText(TXT_DIV_ACCOUNT, y, clientDx, m_txtAccountRect);
	if (dxLine > minDx)
		minDx = dxLine;
	y += m_txtAccountRect.Height();

	// position account name text and "Change account" button
	y += DIVIDER_Y_SPACING;
	dxLine = LEFT_MARGIN + RIGHT_MARGIN;
	//buttonSizer.GetIdealSize(&m_buttonChangeAccount, btnDx, m_btnDy);
	x = clientDx - RIGHT_MARGIN - btnDx;
	m_buttonChangeAccount.MoveWindow(x, y, btnDx, m_btnDy);
	dxLine += btnDx;
	y += m_btnDy;
	if (dxLine > minDx)
		minDx = dxLine;

	// position "Network to update" divider line
	if (IsLoggedIn()) {
		y += DIVIDER_Y_SPACING;
		dxLine = SizeDividerLineText(TXT_DIV_NETWORK_TO_UPDATE, y, clientDx, m_txtNetworkRect);
		if (dxLine > minDx)
			minDx = dxLine;
		y += m_txtNetworkRect.Height();

		// position network name and "Change network"/"Configure network" button
		y += DIVIDER_Y_SPACING;
		dxLine = LEFT_MARGIN + RIGHT_MARGIN;
		//buttonSizer.GetIdealSize(&m_buttonChangeConfigureNetwork, btnDx, m_btnDy);
		x = clientDx - RIGHT_MARGIN - btnDx;
		m_buttonChangeConfigureNetwork.MoveWindow(x, y, btnDx, m_btnDy);
		dxLine += btnDx;
		if (dxLine > minDx)
			minDx = dxLine;
		y += m_btnDy;
	}

	// position "IP address" divider line
	y += DIVIDER_Y_SPACING;
	dxLine = SizeDividerLineText(TXT_DIV_IP_ADDRESS, y, clientDx, m_txtIpAddressRect);
	if (dxLine > minDx)
		minDx = dxLine;
	y += m_txtIpAddressRect.Height();

	// position IP address text
	y += DIVIDER_Y_SPACING;
	txt = IpAddress();
	if (txt) {
		textSizer.SetText(txt);
		textSizer.SetFont(m_dividerTextFont);
		s = textSizer.GetIdealSize2();
		free(txt);
	}
	y += m_btnDy;

	// position "Status" divider line
	y += DIVIDER_Y_SPACING;
	dxLine = SizeDividerLineText(TXT_DIV_STATUS, y, clientDx, m_txtStatusRect);
	if (dxLine > minDx)
		minDx = dxLine;
	y += m_txtStatusRect.Height();

	// position "Using OpenDNS: " + "Yes"/"No" line
	y += DIVIDER_Y_SPACING;
	textSizer.SetFont(m_dividerTextFont);
	if (IsUsingOpenDns()) {
		textSizer.SetText(_T("Yes"));
		m_linkLearnSetup.ShowWindow(SW_HIDE);
	} else {
		textSizer.SetText(_T("No. Learn how to setup OpenDNS.  "));
		s = textSizer.GetIdealSize2();
		minDx = max(minDx, s.cx + LEFT_MARGIN + RIGHT_MARGIN);
		textSizer.SetText(_T("No. Learn how to "));
		s = textSizer.GetIdealSize2();
		int linkX = LEFT_MARGIN + DIVIDER_TEXT_LEFT_MARGIN + s.cx;
		int linkY = m_txtStatusRect.bottom + DIVIDER_Y_SPACING + 6;

		linkSizer.SetWindow(m_linkLearnSetup);
		linkSizer.SetFont(m_dividerTextFont);
		s = linkSizer.GetIdealSize2();
		m_linkLearnSetup.MoveWindow(linkX, linkY, s.cx, s.cy);
		m_linkLearnSetup.ShowWindow(SW_SHOW);
	}

	y += m_btnDy;

	// position "Update" divider line
	if (ShowLastUpdated()) {
		y += DIVIDER_Y_SPACING;
		dxLine = SizeDividerLineText(TXT_DIV_UPDATE, y, clientDx, m_txtUpdateRect);
		if (dxLine > minDx)
			minDx = dxLine;
		y += m_txtUpdateRect.Height();

		// position "Last updated: " text and "Update now" button
		y += DIVIDER_Y_SPACING;
		dxLine = LEFT_MARGIN + RIGHT_MARGIN;
		//buttonSizer.GetIdealSize(&m_buttonUpdate, btnDx, m_btnDy);
		x = clientDx - RIGHT_MARGIN - btnDx;
		m_buttonUpdate.MoveWindow(x, y, btnDx, m_btnDy);
		y += m_btnDy;
		dxLine += btnDx;
		txt = LastUpdateTxt();
		if (txt) {
			textSizer.SetFont(m_defaultGuiFont);
			s = textSizer.GetIdealSize2();
			dxLine += s.cx + 16;
			free(txt);
		}
		if (dxLine > minDx)
			minDx = dxLine;
	}

	if (m_showStatusMsgEdit || m_showUpdateMsgEdit) {
		y += DIVIDER_LINE_Y_OFF;
		y += EDIT_BOX_Y_OFF;
	}

	// position status edit box
	if (m_showStatusMsgEdit) {
		m_editErrorMsg.MoveWindow(EDIT_MARGIN_X, y, m_editErrorMsgDx, m_editErrorMsgRequestedDy);
		y += m_editErrorMsgRequestedDy;
		y += DIVIDER_LINE_Y_OFF;
		y += EDIT_BOX_Y_OFF;
	}

	if (m_showUpdateMsgEdit) {
		m_editUpdateMsg.MoveWindow(EDIT_MARGIN_X + UPDATE_BITMAP_DX_TOTAL, y, m_editUpdateMsgDx, m_editUpdateMsgRequestedDy);
		y += m_editUpdateMsgRequestedDy;
		y += DIVIDER_LINE_Y_OFF;
		y += EDIT_BOX_Y_OFF;
	}

	if (!m_showUpdateMsgEdit && !m_showStatusMsgEdit)
		y+= 18;

	int minDy = y + buttonDy + 8;

	// resize the window if the current size is smaller than
	// what's needed to display content
	int newClientDx = clientDx;
	if (minDx > clientDx)
		newClientDx = minDx;
	int newClientDy = clientDy;
	if (minDy > newClientDy)
		newClientDy = minDy;

	if ((newClientDx != clientDx) || (newClientDy != clientDy))
	{
		ResizeClient(newClientDx, newClientDy);
		RECT r = {0, 0, newClientDx, newClientDy};
		DWORD winStyle = GetStyle();
		BOOL hasMenu = FALSE;
		::AdjustWindowRect(&r, winStyle, hasMenu);
		m_minWinDx = RectDx(r);
		Invalidate();
	}

	RECT r = {0, 0, 0, minDy};
	DWORD winStyle = GetStyle();
	BOOL hasMenu = FALSE;
	::AdjustWindowRect(&r, winStyle, hasMenu);
	m_minWinDy = RectDy(r);
}

LRESULT CMainFrame::OnLayout(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	DoLayout();
	Invalidate();
	return 0;
}

void CMainFrame::OnIpCheckResult(IP4_ADDRESS myIp)
{
	if (m_ipFromDns == myIp) {
		// since this is called every minute, we use this
		// to check if we should update display of "last updated" time
		if (GetLastIpUpdateTime())
			PostMessage(WMAPP_UPDATE_STATUS);
		return;
	}
	m_ipFromDns = myIp;
	IP4_ADDRESS a = m_ipFromDns;
	m_ipFromDnsStr.Format(_T("%u.%u.%u.%u"), (a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);

	// This is called on dns thread so trigger update of
	// the ui on ui thread
	PostMessage(WMAPP_UPDATE_STATUS);
	if (RealIpAddress(myIp)) {
		// on ip change force sending ip update to update
		// possible error state
		m_updaterThread->ForceSendIpUpdate();
	} else {
		if (IP_NOT_USING_OPENDNS == myIp) {
			PostMessage(WMAPP_SWITCH_TO_VISIBLE);
		}
	}
}

LRESULT CMainFrame::OnLinkAbout(LPNMHDR /*pnmh*/)
{
	/* Show debug info when clicking on a link while pressing left alt key */
	if (IsLeftAltPressed()) {
		if (g_showDebug)
			g_showDebug = false;
		else
			g_showDebug = true;
		UpdateErrorEdit();
		return 0;
	}
	LaunchUrl(ABOUT_URL);
	return 0;
}

LRESULT CMainFrame::OnLinkLearnSetupOpenDns(LPNMHDR /*pnmh*/)
{
	LaunchUrl(SETUP_OPENDNS_URL);
	return 0;
}

void CMainFrame::OnIpUpdateResult(char *ipUpdateRes)
{
	IpUpdateResult ipUpdateResult =	IpUpdateResultFromString(ipUpdateRes);
	LogIpUpdate(ipUpdateRes);
	free(m_ipFromHttp);
	m_ipFromHttp = NULL;

	if (IpUpdateNotAvailable == ipUpdateResult)
		return;

	// TODO: this might happen if a user made a network non-dynamic behind our back
	// not sure what to do in this case - re-download networks?
	if (IpUpdateNoHost == ipUpdateResult)
		return;

	if ((IpUpdateOk == ipUpdateResult) || (IpUpdateNotYours == ipUpdateResult)) {
		const char *ip = StrFindChar(ipUpdateRes, ' ');
		if (ip)
			m_ipFromHttp = StrToTStr(ip+1);
	}

	if (ipUpdateResult == m_ipUpdateResult)
		return;

	m_ipUpdateResult = ipUpdateResult;
	if (ipUpdateResult != IpUpdateOk)
		PostMessage(WMAPP_SWITCH_TO_VISIBLE);
	PostMessage(WMAPP_UPDATE_STATUS);
}

void CMainFrame::OnExit(UINT /*uCode*/, int /*nID*/, HWND /*hWndCtl*/)
{
	m_forceExitOnClose = TRUE;
	PostMessage(WM_CLOSE);
}

void CMainFrame::OnToggleWindow(UINT /*uCode*/, int /*nID*/, HWND /*hWndCtl*/)
{
	if (UI_STATE_HIDDEN == m_uiState)
		SwitchToVisibleState();
	else
		SwitchToHiddenState();
}

void CMainFrame::OnRunHidden(UINT /*uCode*/, int /*nID*/, HWND /*hWndCtl*/)
{
	m_hiddenMode = !m_hiddenMode;
	// when enabling hidden mode, it acts as a command and hides the window
	// when disabling hidden mode, it acts as an off button. It's a bit weird
	if (m_hiddenMode)
		SwitchToHiddenState();
	else
		UISetCheck(IDM_RUN_HIDDEN, m_hiddenMode);
}

LRESULT CMainFrame::OnNewVersion(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/)
{
	m_newVersionSetupFilepath = (TCHAR*)wParam;
	SwitchToVisibleState();
	return 0;
}

LRESULT CMainFrame::OnSwitchToVisible(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	SwitchToVisibleState();
	return 0;
}

LRESULT CMainFrame::OnNotifyIcon(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
	m_notifyIcon.OnTrayNotification(wParam, lParam);
	return 0;
}

void CMainFrame::OnNewVersionAvailable(TCHAR *setupFilePath)
{
	PostMessage(WMAPP_NEW_VERSION, (WPARAM)setupFilePath);
}

void CMainFrame::OnSize(UINT nType, CSize /*size*/)
{
	if (SIZE_MINIMIZED == nType)
		return;
	RECT clientRect;
	BOOL ok = GetClientRect(&clientRect);
	if (!ok) return;
	int clientDx = RectDx(clientRect);
	m_editErrorMsgDx = clientDx - EDIT_MARGIN_X * 2;
	m_editErrorMsg.RequestResize();

	m_editUpdateMsgDx = m_editErrorMsgDx - UPDATE_BITMAP_DX_TOTAL;
	m_editUpdateMsg.RequestResize();
	PostMessage(WMAPP_DO_LAYOUT);
}

void CMainFrame::StartDownloadNetworks(char *token, int supressFlags)
{
	CString params = ApiParamsNetworksGet(token);
	const char *paramsTxt = TStrToStr(params);
	// TODO: could do it async but probably not worth it
	//HttpPostAsync(API_HOST, API_URL, paramsTxt, API_IS_HTTPS, m_hWnd, WM_HTTP_DOWNLOAD_NETOWRKS);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	HttpResult *httpRes = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
	free((void*)paramsTxt);
	OnDownloadNetworks(0, (WPARAM)httpRes, (LPARAM)supressFlags);
}

static BOOL IsBitSet(int flags, int bit)
{
	if (flags & bit)
		return true;
	return false;
}

NetworkInfo *MakeFirstNetworkDynamic(NetworkInfo *ni)
{
	JsonEl *json = NULL;
	HttpResult *httpRes = NULL;
	char *jsonTxt = NULL;
	NetworkInfo *dynamicNetwork = FindFirstDynamic(ni);
	assert(!dynamicNetwork);
	if (dynamicNetwork)
		return dynamicNetwork;
	char *networkId = ni->networkId;
	CString params = ApiParamsNetworkDynamicSet(g_pref_token, networkId, true);
	const char *paramsTxt = TStrToStr(params);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	httpRes = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
	free((void*)paramsTxt);
	if (!httpRes || !httpRes->IsValid())
		goto Error;

	DWORD dataSize;
	jsonTxt = (char *)httpRes->data.getData(&dataSize);
	if (!jsonTxt)
		goto Error;

	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Error;
	WebApiStatus status = GetApiStatus(json);
	if (WebApiStatusSuccess != status)
		goto Error;

Exit:
	JsonElFree(json);
	delete httpRes;
	return ni;
Error:
	ni = NULL;
	goto Exit;
}

LRESULT CMainFrame::OnDownloadNetworks(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
	NetworkInfo *ni = NULL;
	char *jsonTxt = NULL;
	JsonEl *json = NULL;
	NetworkInfo *selectedNetwork = NULL;
	int supressFlags = (int)lParam;
	BOOL supressOneNetworkMsg = IsBitSet(supressFlags, SupressOneNetworkMsgFlag);
	BOOL suppressNoDynamicIpMsg = IsBitSet(supressFlags, SuppressNoDynamicIpMsgFlag);
	BOOL supressNoNetworks = IsBitSet(supressFlags, SupressNoNetworksMsgFlag);

	HttpResult *ctx = (HttpResult*)wParam;
	assert(ctx);
	if (!ctx || !ctx->IsValid())
		goto Error;

	DWORD dataSize;
	jsonTxt = (char*)ctx->data.getData(&dataSize);
	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Error;
	WebApiStatus status = GetApiStatus(json);

	if (WebApiStatusSuccess != status) {
		if (WebApiStatusFailure == status) {
			long err;
			bool ok = GetApiError(json, &err);
			if (!ok)
				goto Error;
			if (ERR_NETWORK_DOESNT_EXIST == err)
				goto NoNetworks;
			if (ERR_BAD_TOKEN == err)
				goto BadToken;
			goto Error;
		}
	}
	ni = ParseNetworksGetJson(json);
	size_t networksCount = ListLengthGeneric(ni);
	assert(0 != networksCount);
	if (0 == networksCount)
		goto Error;

	size_t dynamicNetworksCount = DynamicNetworksCount(ni);
	if (0 == dynamicNetworksCount)
		goto NoDynamicNetworks;

	NetworkInfo *dynamicNetwork = FindFirstDynamic(ni);
	assert(dynamicNetwork);
	if (!dynamicNetwork)
		goto Error;

	if (1 == dynamicNetworksCount) {
		if (!supressOneNetworkMsg)
			MessageBox(_T("Only one network configured for dynamic IP updates. Using that network."), MAIN_FRAME_TITLE);
		PrefSetHostname(dynamicNetwork->label);
		SetPrefVal(&g_pref_network_id, dynamicNetwork->networkId);
		SetPrefVal(&g_pref_user_networks_state, UNS_OK);
		goto Exit;
	}

	selectedNetwork = SelectNetwork(ni);
	if (!selectedNetwork) {
		// if cancelled selection but has a network already
		// selected, keep the old network
		if (!streq(UNS_OK, g_pref_user_networks_state))
			goto NoNetworkSelected;
		if (strempty(g_pref_hostname))
			goto NoNetworkSelected;
		// rare but possible case: currently selected network
		// is not on the list of downloaded network (e.g. user
		// changed networks on website)
		if (!HasDynamicNetworkWithLabel(ni, g_pref_hostname))
			goto NoNetworkSelected;
		goto Exit;
	}

NetworkSelected:
	PrefSetHostname(selectedNetwork->label);
	SetPrefVal(&g_pref_network_id, selectedNetwork->networkId);
	SetPrefVal(&g_pref_user_networks_state, UNS_OK);

Exit:
	NetworkInfoFreeList(ni);
	JsonElFree(json);
	delete ctx;
	// prefs changed so save them
	PreferencesSave();
	m_updaterThread->ForceSendIpUpdate();
	// hack: sometimes cursor gets hidden so set
	// it to standard cursor to ensure it's visible
	HCURSOR curs = LoadCursor(NULL, IDC_ARROW);
	SetCursor(curs);
	return 0;

NoNetworkSelected:
	//MessageBox(_T("You need to select a network for Dynamic IP Update."), MAIN_FRAME_TITLE);
	SetPrefVal(&g_pref_user_networks_state, UNS_NO_NETWORK_SELECTED);
	SetPrefVal(&g_pref_network_id, NULL);
	SetPrefVal(&g_pref_hostname, NULL);
	goto Exit;

NoDynamicNetworks:
	selectedNetwork = MakeFirstNetworkDynamic(ni);
	if (selectedNetwork != NULL)
		goto NetworkSelected;
	if (!suppressNoDynamicIpMsg)
		MessageBox(_T("You don't have any networks enabled for Dynamic IP Update. Enable Dynamic IP Updates in your OpenDNS account"), MAIN_FRAME_TITLE); 
	SetPrefVal(&g_pref_user_networks_state, UNS_NO_DYNAMIC_IP_NETWORKS);
	SetPrefVal(&g_pref_network_id, NULL);
	SetPrefVal(&g_pref_hostname, NULL);
	goto Exit;

NoNetworks:
	if (!supressNoNetworks)
		MessageBox(_T("You don't have any networks configured. You need to configure a network in your OpenDNS account"), MAIN_FRAME_TITLE);
	SetPrefVal(&g_pref_user_networks_state, UNS_NO_NETWORKS);
	SetPrefVal(&g_pref_network_id, NULL);
	SetPrefVal(&g_pref_hostname, NULL);
	goto Exit;

BadToken:
	// TODO: this should never happen, not sure what the user can do
	// should we just nuke username/pwd/token in preferences?
	MessageBox(_T("Not a valid token"), MAIN_FRAME_TITLE);
	goto Exit;

Error:
	MessageBox(_T("There was an error downloading networks"), MAIN_FRAME_TITLE);
	goto Exit;
}

NetworkInfo *CMainFrame::SelectNetwork(NetworkInfo *ni)
{
	CSelectNetworkDlg dlg(ni);
	INT_PTR nRet = dlg.DoModal();
	if (IDOK != nRet)
		return NULL;
	NetworkInfo *selected = dlg.m_selectedNetwork;
	assert(selected);
	return selected;
}

int CMainFrame::OnCreate(LPCREATESTRUCT /* lpCreateStruct */)
{
	SetMenu(NULL);

	// remove WS_CLIPCHILDREN style to make transparent
	// static controls work
	ModifyStyle(WS_CLIPCHILDREN, 0);

	SetWindowText(MAIN_FRAME_TITLE);

#if 0
	m_defaultFont = AtlGetDefaultGuiFont();
#else
	HDC dc = GetWindowDC();
	CLogFont logFontDefault(AtlGetDefaultGuiFont());
	//_tcscpy_s(logFontDefault.lfFaceName, dimof(logFontDefault.lfFaceName), "Tahoma");
	//_tcscpy_s(logFontDefault.lfFaceName, dimof(logFontDefault.lfFaceName), "Comic Sans MS");
	logFontDefault.SetBold();
	logFontDefault.SetHeight(DEFAULT_FONT_SIZE, dc);
	m_defaultFont.Attach(logFontDefault.CreateFontIndirect());

	CLogFont logFontEditFont(AtlGetDefaultGuiFont());
	if (EDIT_FONT_NAME)
		_tcscpy_s(logFontEditFont.lfFaceName, dimof(logFontEditFont.lfFaceName), EDIT_FONT_NAME);
	logFontEditFont.SetBold();
	logFontEditFont.SetHeight(EDIT_FONT_SIZE, dc);
	m_statusEditFont.Attach(logFontEditFont.CreateFontIndirect());
	m_editFontName = tstrdup(logFontEditFont.lfFaceName);
	ReleaseDC(dc);

#endif

	CLogFont lf(m_defaultFont);
	//m_editFontName = tstrdup(_T("Tahoma"));
	//m_editFontName = tstrdup(_T("Times New Roman"));
	//m_editFontName = tstrdup(_T("Arial"));
	//m_editFontName = tstrdup(_T("Trebuchet MS"));
	//m_editFontName = tstrdup(_T("Fixedsys"));

	m_topBarFont = m_defaultFont;

	// values inside r don't matter - things get positioned in DoLayout()
	RECT r = {10, 10, 20, 20};

	m_defaultGuiFont = AtlGetDefaultGuiFont();

	CLogFont logFontButtons(AtlGetDefaultGuiFont());
	//_tcscpy_s(logFontButtons.lfFaceName, dimof(logFontButtons.lfFaceName), "Comic Sans MS");
	//logFontButtons.SetBold();
	m_buttonsFont.Attach(logFontButtons.CreateFontIndirect());

	CLogFont logFontDivider(AtlGetDefaultGuiFont());
	//_tcscpy_s(logFontDivider.lfFaceName, dimof(logFontDivider.lfFaceName), "Comic Sans MS");
	//logFontDivider.SetHeight(DEFAULT_FONT_SIZE, dc);
	logFontDivider.SetBold();
	m_dividerTextFont.Attach(logFontDivider.CreateFontIndirect());

	CLogFont logFontText(AtlGetDefaultGuiFont());
	//_tcscpy_s(logFontText.lfFaceName, dimof(logFontText.lfFaceName), "Comic Sans MS");
	//logFontText.SetBold();
	m_textFont.Attach(logFontText.CreateFontIndirect());

	// TODO: tried using LWS_TRANSPARENT and/or WS_EX_TRANSPARENT but they don't
	// seem to work as expected (i.e. create transparent background for the link
	// control)
	m_linkAbout.Create(m_hWnd, r, _T("<a>About this program</a>"), WS_CHILD | WS_VISIBLE);
	m_linkAbout.SetFont(m_defaultGuiFont);
	m_linkAbout.SetDlgCtrlID(IDC_LINK_ABOUT);

	m_linkLearnSetup.Create(m_hWnd, r, _T("<a>setup OpenDNS.</a>"), WS_CHILD | WS_VISIBLE);
	m_linkLearnSetup.SetFont(m_dividerTextFont);
	m_linkLearnSetup.SetDlgCtrlID(IDC_LINK_LEARN_SETUP_OPENDNS);

	m_buttonSendIpUpdates.Create(m_hWnd, r, _T("Send background IP updates"), WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX);
	m_buttonSendIpUpdates.SetFont(m_buttonsFont);
	//m_buttonSendIpUpdates.SetFont(m_topBarFont);
	m_buttonSendIpUpdates.SetDlgCtrlID(IDC_CHECK_SEND_UPDATES);
	BOOL sendingUpdates = GetPrefValBool(g_pref_send_updates);
	m_buttonSendIpUpdates.SetCheck(sendingUpdates);

	m_buttonChangeAccount.Create(m_hWnd, r, _T("Change account"),  WS_CHILD | WS_VISIBLE);
	m_buttonChangeAccount.SetFont(m_buttonsFont);
	m_buttonChangeAccount.SetDlgCtrlID(IDC_BUTTON_CHANGE_ACCOUNT);
	//m_buttonChangeAccount.ShowWindow(SW_HIDE);

	m_buttonChangeConfigureNetwork.Create(m_hWnd, r, _T("Change network"),  WS_CHILD | WS_VISIBLE);
	m_buttonChangeConfigureNetwork.SetFont(m_buttonsFont);
	//m_buttonChangeConfigureNetwork.SetFont(m_topBarFont);
	m_buttonChangeConfigureNetwork.SetDlgCtrlID(IDC_BUTTON_CHANGE_NETWORK);
	//m_buttonChangeAccount.ShowWindow(SW_HIDE);

	m_buttonUpdate.Create(m_hWnd, r, _T("Update now"),  WS_CHILD | WS_VISIBLE);
	// TODO: long text here will not size the window
	//m_buttonUpdate.Create(m_hWnd, r, _T("Update now because this is a long text"),  WS_CHILD | WS_VISIBLE);
	m_buttonUpdate.SetFont(m_buttonsFont);
	m_buttonUpdate.SetDlgCtrlID(IDC_BUTTON_SEND_IP_UPDATE);
	//m_buttonChangeAccount.ShowWindow(SW_HIDE);

	m_editErrorMsg.Create(m_hWnd, r, _T(""), WS_CHILD | WS_VISIBLE | ES_MULTILINE);
	m_editErrorMsg.SetReadOnly(TRUE);
	m_editErrorMsg.SetEventMask(ENM_REQUESTRESIZE | ENM_LINK | ENM_SELCHANGE);
	//m_editErrorMsg.SetEventMask(ENM_REQUESTRESIZE | ENM_LINK);
	m_editErrorMsg.SetBackgroundColor(colEditBg);
	m_editErrorMsg.SetDlgCtrlID(IDC_EDIT_STATUS);

	m_editUpdateMsg.Create(m_hWnd, r, _T(""), WS_CHILD | WS_VISIBLE | ES_MULTILINE);
	m_editUpdateMsg.SetReadOnly(TRUE);
	m_editUpdateMsg.SetEventMask(ENM_REQUESTRESIZE | ENM_LINK | ENM_SELCHANGE);
	m_editUpdateMsg.SetBackgroundColor(colEditUpdateBg);
	m_editUpdateMsg.SetDlgCtrlID(IDC_EDIT_UPDATE);

	if (strempty(g_pref_user_name) ||
		strempty(g_pref_token))
	{
		CSignInDlg dlg;
		dlg.DoModal();
	}

	HINSTANCE hinst = ATL::_AtlBaseModule.GetResourceInstance();
	m_updateBitmap = (HBITMAP)::LoadImage(hinst,  MAKEINTRESOURCE(IDR_UPDATE_BMP), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_DEFAULTSIZE);
	m_hIconOk = CTrayNotifyIcon::LoadIcon(IDR_SYSTRAY_OK);
	m_hIconErr = CTrayNotifyIcon::LoadIcon(IDR_SYSTRAY_ERR);

	m_notifyIcon.Create(this, IDR_MENU1, _T(""), m_hIconOk, WMAPP_NOTIFY_ICON);
	m_notifyIcon.SetTooltipText(_T("OpenDNS Updater v") PROGRAM_VERSION);

	m_updaterThread = new UpdaterThread(this);
	if (IsLoggedIn() && strempty(g_pref_user_networks_state))
		ChangeNetwork(SupressAll);

	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	m_updaterThread->ForceSendIpUpdate();
	m_updaterThread->ForceSoftwareUpdateCheck();
	OnTimer(0);
	UINT ONE_HOUR_IN_MS = 60*60*1000;
	SetTimer(1, ONE_HOUR_IN_MS);
	return 0;
}

LRESULT CMainFrame::OnUpdateStatus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	UpdateErrorEdit();
	return 0;
}

void CMainFrame::SwitchToVisibleState()
{
	ShowWindow(SW_SHOW);
	m_uiState = UI_STATE_VISIBLE;
	HMENU menu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU2));
	m_notifyIcon.SetMenu(menu);
	m_notifyIcon.SetDefaultMenuItem(1, TRUE);
	if (m_notifyIcon.IsHidden())
		m_notifyIcon.Show();
	UISetCheck(IDM_RUN_HIDDEN, m_hiddenMode);
	UpdateUpdateEdit();
	UpdateErrorEdit();
}

void CMainFrame::SwitchToHiddenState()
{
	ShowWindow(SW_HIDE);
	m_uiState = UI_STATE_HIDDEN;
	HMENU menu = LoadMenu(NULL, MAKEINTRESOURCE(IDR_MENU1));
	m_notifyIcon.SetMenu(menu);
	m_notifyIcon.SetDefaultMenuItem(1, TRUE);
	if (m_hiddenMode && !m_notifyIcon.IsHidden())
		m_notifyIcon.Hide();
	UISetCheck(IDM_RUN_HIDDEN, m_hiddenMode);
}

LRESULT CMainFrame::OnErrorNotif(UINT /*uMsg*/, WPARAM specialCmd, LPARAM /*lParam*/)
{
	slog("CMainFrame::OnErrorNotif(): "); 
	if (SPECIAL_CMD_SHOW == specialCmd) {
		SwitchToVisibleState();
		slog("SPECIAL_CMD_SHOW\n");
	}
	else {
		char buf[32];
		slog("unknown specialCmd=");
		itoa((int)specialCmd, buf, 10);
		slog(buf);
		slog("\n");
		assert(0);
	}
	return 0;
}

#endif
