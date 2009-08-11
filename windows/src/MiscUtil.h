// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MISC_UTIL_H__
#define MISC_UTIL_H__

#include "ApiKey.h"

#define ABOUT_URL _T("http://www.opendns.com/software/windows/dynip/about/")
#define LEARN_MORE_IP_ADDRESS_TAKEN_URL _T("http://www.opendns.com/software/windows/dynip/ip-taken/")
#define LEARN_MORE_IP_MISMATCH_URL _T("http://www.opendns.com/software/windows/dynip/ip-differs/")
#define SETUP_OPENDNS_URL _T("http://www.opendns.com/software/windows/dynip/setup-opendns/")

#define FORGOT_PASSWORD_URL _T("https://www.opendns.com/dashboard/signin/")
#define CREATE_ACCOUNT_URL _T("https://www.opendns.com/start/create_account/")

extern UINT g_errorNotifMsg;

#define PROGRAM_VERSION  _T("2.0b12")

// PROGRAM_VERSION_SIMULATE_UPGRADE must be lower than PROGRAM_VERSION
#define PROGRAM_VERSION_SIMULATE_UPGRADE _T("0.8")

#define MAIN_WINDOWS_CLASS_NAME _T("OpenDNS_IP_Update_Client_Wnd_Class")

#define MAIN_FRAME_TITLE _T("OpenDNS Updater v") PROGRAM_VERSION
#define API_URL "/v1/"

#define AUTO_UPDATE_HOST _T("opendnsupdate.appspot.com")
#define AUTO_UPDATE_URL "/updatecheck/dynamicipwin"

#define CRASH_DUMP_URL "/crashsubmit"

#define COMMON_DATA_DIR_REG_KEY_PATH  _T("SOFTWARE\\OpenDNS Updater")
#define COMMON_DATA_DIR_REG_KEY_NAME  _T("Common Dir")

#define APP_UNIQUE_GUID _T("3ffe9397-f387-46bc-b5d0-d798c2a596b9")

#define SERVICE_EXE_NAME _T("OpenDNSDynamicIpService.exe")
#define SERVICE_EXE_NAME_WITHOUT_EXE _T("OpenDNSDynamicIpService")

#define GUI_EXE_NAME _T("OpenDNSUpdater.exe")
#define GUI_EXE_NAME_WITHOUT_EXE _T("OpenDNSUpdater")

// names of cmd-line arguments that services uses when launching
// ui to inform it about some condition
#define CMD_ARG_NOT_USING_UPENDNS _T("/notusingopendns")
#define CMD_ARG_UPGRADE_CHECK _T("/upgradecheck")
#define CMD_ARG_NOT_YOURS _T("/notyours")
#define CMD_ARG_BAD_AUTH _T("/badauth")
#define CMD_ARG_SEND_CRASHDUMP _T("/sendcrashdump")

// SA == Struct Alloc
#define SA(struct_name) (struct_name*)malloc(sizeof(struct_name))

// mark unused variables to reduce compiler warnings
#define UNUSED_VAR( x )  (x) = (x)

#ifndef NDEBUG
#define ASSERT_RUN_ONCE() \
	static bool was_run__ = false; \
	assert(!was_run__); \
	was_run__ = true
#else
#define ASSERT_RUN_ONCE() \
	do {} while (false)
#endif

#define dimof(x) (sizeof(x)/sizeof(x[0]))

enum {
	SPECIAL_CMD_NONE = 0,
	SPECIAL_CMD_INSTALL,
	SPECIAL_CMD_UNINSTALL,
	SPECIAL_CMD_SHOW,
	SPECIAL_CMD_SEND_CRASHDUMPS
};

class AutoFree {
	void *m_tofree;
public:
	AutoFree(void *tofree) : m_tofree(tofree) {}
	AutoFree(char *tofree) : m_tofree((void*)tofree) {}
	AutoFree(const char *tofree) : m_tofree((void*)tofree) {}
	AutoFree(const WCHAR *tofree) : m_tofree((void*)tofree) {} 
	~AutoFree() { free(m_tofree); }
};

char *MyGetWindowTextA(HWND hwnd);
WCHAR *MyGetWindowTextW(HWND hwnd);
TCHAR *MyGetWindowText(HWND hwnd);

bool FileOrDirExists(const TCHAR *fileName);

char *FileReadAll(const TCHAR *filePath, uint64_t *fileSizeOut=NULL);
BOOL FileWriteAll(const TCHAR *filePath, const char *buf, uint64_t bufLen);
CString AppDataDir();
CString SettingsFileName();
CString SettingsFileNameInDir(const TCHAR *dir);
CString OldSettingsFileName();
CString OldSettingsFileName2();
CString AutoUpdateUrl(const TCHAR *version, const char *type);
CString CrashDumpUrl(const TCHAR *version);

CString ApiParamsSignIn(const char* userName, const char* password);
CString ApiParamsNetworksGet(const char *token);
CString ApiParamsUpdateIp(const char *ip, const char *network);
CString ApiParamsNetworkDynamicSet(const char *token, const char *networkId, bool makeDynamic);
CString ApiParamsNetworkTypoExceptionsAdd(const char *token, const char *network_id, const char *typoExceptionsList);
CString ApiParamsNetworkTypoExceptionsRemove(const char *token, const char *network_id, const char *typoExceptionsList);
CString ApiParamsNetworkGet(const char *token);

char* LastErrorAsStr(DWORD err=-1);
char* WinHttpErrorAsStr(DWORD error);
void SeeLastError(DWORD err=-1);
void LaunchUrl(const TCHAR *url);
void ExecWithParams(const TCHAR *exe, const TCHAR *params, bool hidden);
TCHAR *ReadRegStr(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName);
void WriteRegStr(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName, const TCHAR *value, BOOL overwrite=FALSE);
void DeleleteReg(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName);
//TCHAR *GetWindowsUserName();
void UseDevServers(bool useDevServers);
bool UsingDevServers();
const char *GetApiHost();
const char *GetIpUpdateHost();
const char *GetIpUpdateUrl(BOOL addApiKey);
bool IsApiHostHttps();
const TCHAR *GetDashboardUrl();
bool CanSendIPUpdates();
void RegisterErrorNotifMsg();
DWORD GetExplorerProcessId();
BOOL GetUserNameDomainFromSid(PSID sid, TCHAR **userNameOut, TCHAR **userDomainOut);
BOOL GetUserNameDomainFromToken(HANDLE token, TCHAR **userNameOut, TCHAR **userDomainOut);
HANDLE GetExplorerOwnerToken(TCHAR **userNameOut, TCHAR **userDomainOut);
bool IsLeftAltPressed();
bool IsLeftCtrlPressed();
bool IsLeftAltAndCtrlPressed();
TCHAR *FormatUpdateTime(int minutes);
void DeleteOldInstallers();

static inline int RectDx(const RECT r)
{
	int dx = r.right - r.left;
	return dx;
}

static inline int RectDy(const RECT r)
{
	int dy = r.bottom - r.top;
	return dy;
}

#endif
