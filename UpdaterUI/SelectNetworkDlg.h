#ifndef SELECT_NETWORK_DLG_H__
#define SELECT_NETWORK_DLG_H__

#include "resource.h"
#include "JsonApiResponses.h"

class CSelectNetworkDlg : public CDialogImpl<CSelectNetworkDlg>
{
	NetworkInfo *m_networkInfo; // a reference, doesn't own this
	CFont m_fBold;
	CStatic m_staticSelectNetwork;
	CListBox m_listBoxNetworksList;
	CButton m_buttonOk;

public:
	NetworkInfo *m_selectedNetwork;

	enum { IDD = IDD_DIALOG_SELECT_NETWORK };

	BEGIN_MSG_MAP(CSelectNetworkDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER(IDC_LIST_NETWORKS, LBN_SELCHANGE, OnSelectionChanged)
		COMMAND_ID_HANDLER(IDOK, OnButtonOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnButtonCancel)
	END_MSG_MAP()

	CSelectNetworkDlg(NetworkInfo *ni)
	{
		assert(ni);
		m_networkInfo = ni;
		m_selectedNetwork = NULL;
	}

	BOOL OnInitDialog(CWindow /* wndFocus */, LPARAM /* lInitParam */)
	{
		CenterWindow(GetParent());

		CLogFont logFontBold(AtlGetDefaultGuiFont());
		logFontBold.SetBold();
		m_fBold.Attach(logFontBold.CreateFontIndirect());

		m_staticSelectNetwork = GetDlgItem(IDC_STATIC_SELECT_NETWORK);
		m_staticSelectNetwork.SetFont(m_fBold);

		m_listBoxNetworksList = GetDlgItem(IDC_LIST_NETWORKS);
		m_buttonOk = GetDlgItem(IDOK);
		SetOkButtonStatus();

		NetworkInfo *ni = m_networkInfo;
		int total = 0;
		while (ni) {
			if (ni->isDynamic && ni->label) {
				TCHAR *label = StrToTStr(ni->label);
				m_listBoxNetworksList.AddString(label);
				m_listBoxNetworksList.SetItemData(total, (DWORD_PTR)ni);
				free(label);
				total += 1;
			}
			ni = ni->next;
		}
		
		assert(0 != total);
		if (0 == total)
			return TRUE;
		return FALSE;
	}

	LRESULT OnSelectionChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SetOkButtonStatus();
		return 0;
	}

	void SetOkButtonStatus()
	{
		BOOL enable = FALSE;
		int sel = m_listBoxNetworksList.GetCurSel();
		if (-1 != sel)
			enable = TRUE;
		m_buttonOk.EnableWindow(enable);
	}

	LRESULT OnButtonOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		int sel = m_listBoxNetworksList.GetCurSel();
		assert(-1 != sel);
		if (-1 == sel)
			goto Exit;
		m_selectedNetwork = (NetworkInfo*)m_listBoxNetworksList.GetItemData(sel);
Exit:
		EndDialog(wID);
		return 0;
	}

	LRESULT OnButtonCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		assert(!m_selectedNetwork);
		EndDialog(wID);
		return 0;
	}
};

#endif
