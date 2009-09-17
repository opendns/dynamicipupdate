#ifndef IP_UPDATES_HISTORY_DLG_H__
#define IP_UPDATES_HISTORY_DLG_H__

#include "resource.h"

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
		CListBox m_ipUpdatesList;
		m_ipUpdatesList = GetDlgItem(IDC_LIST_IP_UPDATES_HISTORY);
		IpUpdate *curr = m_ipUpdates;
		while (curr) {
			TCHAR *ipAddr = StrToTStr(curr->ipAddress);
			TCHAR *time = StrToTStr(curr->time);
			if (time && ipAddr) {
				TCHAR *s = TStrCat(time, _T(" "), ipAddr);
				m_ipUpdatesList.AddString(s);
				free(s);
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

