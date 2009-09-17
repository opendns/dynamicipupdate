#ifndef IP_UPDATES_HISTORY_DLG_H__
#define IP_UPDATES_HISTORY_DLG_H__

#include "resource.h"

// TODO: sorting by the columns

class CIpUpdatesHistoryDlg : public CDialogImpl<CIpUpdatesHistoryDlg>
{
	IpUpdate *m_ipUpdates; // a reference, doesn't own this

public:
	enum { IDD = IDD_DIALOG_IP_UPDATES_HISTORY};

	BEGIN_MSG_MAP(CSelectNetworkDlg)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnButtonOk)
		COMMAND_ID_HANDLER(IDCANCEL, OnButtonOk) // to make 'close window' button work
		COMMAND_ID_HANDLER(IDC_BUTTON_COPY_TO_CLIPBOARD, OnButtonCopyToClipboard)
	END_MSG_MAP()

	CIpUpdatesHistoryDlg(IpUpdate *ipUpdates)
	{
		m_ipUpdates = ipUpdates;
	}

	BOOL OnInitDialog(CWindow /* wndFocus */, LPARAM /* lInitParam */)
	{
		CListViewCtrl m_ipUpdatesList;
		m_ipUpdatesList = GetDlgItem(IDC_LIST_IP_UPDATES_HISTORY);
		// unfortunately LVS_EX_AUTOSIZECOLUMNS is Vista+ only
		//m_ipUpdatesList.SetExtendedListViewStyle(LVS_EX_AUTOSIZECOLUMNS);
		m_ipUpdatesList.AddColumn(_T("Date"), 0);
		m_ipUpdatesList.AddColumn(_T("IP Address"), 1);
		// TOTAL_WIDTH was determined empirically on XP. Not sure if it will be
		// the same on Vista/7 or on systems with a different fonts/dpis
		static const int TOTAL_WIDTH = 246;
		static const int DATE_COLUMN_WIDTH = 150;
		m_ipUpdatesList.SetColumnWidth(0, DATE_COLUMN_WIDTH);
		m_ipUpdatesList.SetColumnWidth(1, TOTAL_WIDTH - DATE_COLUMN_WIDTH);
		IpUpdate *curr = m_ipUpdates;
		int i = 0;
		while (curr) {
			TCHAR *ipAddr = StrToTStr(curr->ipAddress);
			TCHAR *time = StrToTStr(curr->time);
			if (time && ipAddr) {
				m_ipUpdatesList.AddItem(i, 0, time);
				m_ipUpdatesList.AddItem(i, 1, ipAddr);
			}
			free(ipAddr);
			free(time);
			curr = curr->next;
		}
		return FALSE;
	}

	LRESULT OnButtonOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	};

	LRESULT OnButtonCopyToClipboard(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		EndDialog(wID);
		return 0;
	};

};

#endif

