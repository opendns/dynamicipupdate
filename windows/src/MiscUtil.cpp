// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include <tlhelp32.h>

#include "MiscUtil.h"

#include "Prefs.h"
#include "StrUtil.h"

UINT g_errorNotifMsg = 0;

bool FileOrDirExists(const TCHAR *fileName)
{
    DWORD fileAttr = GetFileAttributes(fileName);
    if (0xFFFFFFFF == fileAttr)
        return false;
    return true;
}

char *FileReadAll(const TCHAR *filePath, uint64_t *fileSizeOut)
{
    DWORD       size, sizeRead;
    char *      data = NULL;
    int         f_ok;

    HANDLE h = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL,  
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    size = GetFileSize(h, NULL);
    if (-1 == size)
        goto Exit;

    /* allocate one byte more and 0-terminate just in case it's a text
       file we'll want to treat as C string. Doesn't hurt for binary
       files */
    data = (char*)malloc(size + 1);
    if (!data)
        goto Exit;
    data[size] = 0;

    f_ok = ReadFile(h, data, size, &sizeRead, NULL);
    if (!f_ok) {
        free(data);
        data = NULL;
    } else {
        assert(sizeRead == size);
    }
    if (fileSizeOut)
        *fileSizeOut = (uint64_t)size;
Exit:
    CloseHandle(h);
    return data;
}

char *MyGetWindowTextA(HWND hwnd)
{
	int len = ::GetWindowTextLength(hwnd);
	if (len <= 0)
		return NULL;
	char *buf = (char*)malloc(len + 1);
	::GetWindowTextA(hwnd, buf, len+1);
	buf[len] = 0;
	return buf;
}

WCHAR *MyGetWindowTextW(HWND hwnd)
{
	int len = ::GetWindowTextLength(hwnd);
	if (len <= 0)
		return NULL;
	WCHAR *buf = (WCHAR*)malloc(sizeof(WCHAR) * (len+1));
	::GetWindowTextW(hwnd, buf, len+1);
	buf[len] = 0;
	return buf;
}

// caller has to free() the result
TCHAR *MyGetWindowText(HWND hwnd)
{
#ifdef UNICODE
	return MyGetWindowTextW(hwnd);
#else
	return MyGetWindowTextA(hwnd);
#endif
}

BOOL FileWriteAll(const TCHAR *filePath, const char *data, uint64_t dataLen)
{
    HANDLE h = CreateFile(filePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,  
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,  NULL); 
    if (h == INVALID_HANDLE_VALUE)
        return FALSE;

    DWORD       size;
    BOOL f_ok = WriteFile(h, data, (DWORD)dataLen, &size, NULL);
    assert(!f_ok || ((DWORD)dataLen == size));
    CloseHandle(h);
    return f_ok;
}

#define APP_DATA_SUBDIR _T("OpenDNS Updater")
CString AppDataDir()
{
	TCHAR		dir[MAX_PATH];
	SHGetSpecialFolderPath(NULL, dir, CSIDL_APPDATA, TRUE);
	CString dirOut(dir);
	dirOut += PATH_SEP_STR APP_DATA_SUBDIR;
	const TCHAR *path = dirOut;
	if (!FileOrDirExists(path)) {
		BOOL ok = CreateDirectory(path, NULL);
		assert(ok);
		UNUSED_VAR(ok);
	}
	return dirOut;
}

CString OldClientCommonAppDataDir()
{
	TCHAR		dir[MAX_PATH];
	SHGetSpecialFolderPath(NULL, dir, CSIDL_COMMON_APPDATA, TRUE);
	CString dirOut(dir);
	dirOut += PATH_SEP_STR APP_DATA_SUBDIR;
	return dirOut;
}
CString OldSettingsFileName()
{
	CString fileName = OldClientCommonAppDataDir();
	fileName += PATH_SEP_STR _T("settings.ini");
	return fileName;
}

CString OldSettingsFileName2()
{
	CString fileName = OldClientCommonAppDataDir();
	fileName += PATH_SEP_STR _T("networks.ini");
	return fileName;
}

#define SETTINGS_FILE_NAME _T("settings.dat")

CString SettingsFileName()
{
	CString fileName = AppDataDir();
	fileName += PATH_SEP_STR SETTINGS_FILE_NAME;
	return fileName;
}

CString SettingsFileNameInDir(const TCHAR *dir)
{
	CString fileName = dir;
	fileName += PATH_SEP_STR SETTINGS_FILE_NAME;
	return fileName;
}

static bool IsValidAutoUpdateType(const char *type)
{
	// install, uninstall or check
	if (streq(type, "i"))
		return true;
	if (streq(type, "u"))
		return true;
	if (streq(type, "c"))
		return true;
	return false;
}

// assumes <name> and <val> are already properly url-encoded
static void AddUrlParam(CString& s, const char *name, const char *val)
{
	if (!s.IsEmpty())
		s += "&";
	s += name;
	s += "=";
	s += val;
}


static CString CommonUrlPart(CString url)
{
	CString s = url;
	assert(g_pref_unique_id);
	s += "&i=";
	s += g_pref_unique_id;

	if (!strempty(g_pref_user_name))
	{
		s += "&u=";
		s += g_pref_user_name;
	}

	char country[32] = {0};
	GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, country, sizeof(country)-1);	
	s += "&c=";
	s += country;

	char lang[32] = {0};
	GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, sizeof(lang)-1);
	s += "&l=";
	s += lang;

	OSVERSIONINFO osver;
	osver.dwOSVersionInfoSize = sizeof(osver);
	if (GetVersionEx(&osver)) {
		s += "&osver=";
		char num[32];
		itoa(osver.dwMajorVersion, num, 10);
		s += num;
		s += ".";
		itoa(osver.dwMinorVersion, num, 10);
		s += num;
	}
	return s;
}

CString AutoUpdateUrl(const TCHAR *version, const char *type)
{
	assert(IsValidAutoUpdateType(type));
	CString s = AUTO_UPDATE_URL;

	s += "?v=";
	s += version;

	s += "&t=";
	s += type;
	return CommonUrlPart(s);
}

CString CrashDumpUrl(const TCHAR *version)
{
	CString s = CRASH_DUMP_URL;
	s += "?v=";
	s += version;
	s += "&app=updaterwin";
	return CommonUrlPart(s);
}

CString ApiParamsSignIn(const char* userName, const char* password)
{
	char *userNameUrlEncoded = StrUrlEncode(userName);
	char *passwordUrlEncoded = StrUrlEncode(password);
	CString url;
	AddUrlParam(url, "api_key", API_KEY);
	AddUrlParam(url, "method", "account_signin");
	AddUrlParam(url, "username", userNameUrlEncoded);
	AddUrlParam(url, "password", passwordUrlEncoded);
	free(userNameUrlEncoded);
	free(passwordUrlEncoded);
	return url;
}

CString ApiParamsNetworksGet(const char *token)
{
	char *tokenEncoded = StrUrlEncode(token);
	CString url;
	AddUrlParam(url, "api_key", API_KEY);
	AddUrlParam(url, "method", "networks_get");
	AddUrlParam(url, "token", tokenEncoded);
	free(tokenEncoded);
	return url;
}

CString ApiParamsNetworkDynamicSet(const char *token, const char *networkId, bool makeDynamic)
{
	CString url;
	AddUrlParam(url, "api_key", API_KEY);
	AddUrlParam(url, "method", "network_dynamic_set");
	char *tokenEncoded = StrUrlEncode(token);
	AddUrlParam(url, "token", tokenEncoded);
	free(tokenEncoded);
	AddUrlParam(url, "network_id", networkId);
	if (makeDynamic)
		AddUrlParam(url, "setting", "on");
	else
		AddUrlParam(url, "setting", "off");
	return url;
}

char* LastErrorAsStr(DWORD err)
{
	char *msgBuf = NULL;
	char *copy;
	if (-1 == err)
		err = GetLastError();
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, 0,
		(LPSTR) &msgBuf, 0, NULL);
	if (!msgBuf) {
		return WinHttpErrorAsStr(err);
	}
	copy = _strdup(msgBuf);
	LocalFree(msgBuf);
	return copy;
}

char* WinHttpErrorAsStr(DWORD error)
{
	char *msgBuf = NULL;
	char *copy;
	DWORD err = GetLastError();
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
		GetModuleHandleA("winhttp.dll"), error, 0,
		(LPSTR) &msgBuf, 0, NULL);
	if (!msgBuf) {
		err = GetLastError();
		// 317 - can't find message
		return NULL;
	}
	copy = _strdup(msgBuf);
	LocalFree(msgBuf);
	return copy;
}

void SeeLastError(DWORD err)
{
	char *s = LastErrorAsStr(err);
	free(s);
}

void LaunchUrl(const TCHAR *url)
{
	SHELLEXECUTEINFO sei;
	BOOL             res;

	if (NULL == url)
		return;

	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize  = sizeof(sei);
	sei.fMask   = SEE_MASK_FLAG_NO_UI;
	sei.lpVerb  = TEXT("open");
	sei.lpFile  = url;
	sei.nShow   = SW_SHOWNORMAL;

	res = ShellExecuteEx(&sei);
}

void ExecWithParams(const TCHAR *exe, const TCHAR *params, bool hidden)
{
	SHELLEXECUTEINFO sei;
	BOOL             res;

	if (NULL == exe)
		return;

	ZeroMemory(&sei, sizeof(sei));
	sei.cbSize  = sizeof(sei);
	sei.fMask   = SEE_MASK_FLAG_NO_UI;
	sei.lpVerb  = NULL;
	sei.lpFile  = exe;
	sei.lpParameters = params;
	if (hidden)
		sei.nShow = SW_HIDE;
	else
		sei.nShow   = SW_SHOWNORMAL;
	res = ShellExecuteEx(&sei);
}

void WriteRegStr(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName, const TCHAR *value,  BOOL overwrite)
{
    HKEY keyTmp = NULL;
    if (!overwrite) {
        TCHAR *val = ReadRegStr(keySub, keyPath, keyName);
        if (val) {
            free(val);
            return;
        }
    }

    LONG res = RegCreateKeyEx(keySub, keyPath, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &keyTmp, NULL);
    if (ERROR_SUCCESS != res) {
        SeeLastError();
        goto Exit;
    }
    DWORD cbValue = (tstrlen(value)+1)*sizeof(TCHAR);
    res = RegSetValueEx(keyTmp, keyName, 0, REG_SZ, (const BYTE*)value, cbValue);
    if (ERROR_SUCCESS != res)
        SeeLastError();
Exit:
    RegCloseKey(keyTmp);
}

TCHAR *ReadRegStr(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName)
{
	TCHAR *result = NULL;
	HKEY keyTmp = NULL;
	LONG res = RegOpenKeyEx(keySub, keyPath, 0, KEY_QUERY_VALUE, &keyTmp);
	if (ERROR_SUCCESS != res) {
		SeeLastError();
		goto Exit;
	}
	TCHAR buf[512];
	DWORD cbBuf = sizeof(buf);
	DWORD type;
	res = RegQueryValueEx(keyTmp, keyName, 0, &type, (LPBYTE)buf, &cbBuf);
	// TODO: handle ERROR_MORE_DATA
	if (ERROR_SUCCESS != res) {
		SeeLastError();
		goto Exit;
	}
	assert(REG_SZ == type);
	result = tstrdup(buf);
Exit:
    RegCloseKey(keyTmp);
	return result;
}

void DeleleteReg(HKEY keySub, const TCHAR *keyPath, const TCHAR *keyName)
{
	HKEY keyTmp = NULL;
	LONG res = RegOpenKeyEx(keySub, keyPath, 0, KEY_SET_VALUE, &keyTmp);
	if (ERROR_SUCCESS != res)
		goto Error;
	res = RegDeleteValue(keyTmp, keyName);
	if (ERROR_SUCCESS != res)
		goto Error;
Exit:
    RegCloseKey(keyTmp);
	return;
Error:
	SeeLastError(res);
	goto Exit;
}

#if 0
// caller needs to free the result
TCHAR *GetWindowsUserName()
{
	TCHAR buf[1024];
	ULONG sizeNeeded = dimof(buf);
	BOOL ok = GetUserNameEx(NameSamCompatible, buf, &sizeNeeded);
	if (!ok)
		return NULL;
	return tstrdup(buf);
}
#endif

#define API_HOST_DEV "api.dev6.sfo.opendns.com"
#define IP_UPDATE_HOST_DEV "website.dev6.sfo.opendns.com"
#define DASHBOARD_URL_DEV _T("http://website.dev6.sfo.opendns.com/dashboard/networks/")
#define API_IS_HTTPS_DEV false

#define API_HOST "api.opendns.com"
#define IP_UPDATE_HOST "updates.opendns.com"
#define DASHBOARD_URL _T("http://www.opendns.com/dashboard/networks/")
#define API_IS_HTTPS true

static bool g_useDevServers = false;

void UseDevServers(bool useDevServers)
{
	g_useDevServers = useDevServers;
}

bool UsingDevServers()
{
	return g_useDevServers;
}

const char *GetApiHost()
{
	if (g_useDevServers)
		return API_HOST_DEV;
	else
		return API_HOST;
}

const char *GetIpUpdateHost()
{
	if (g_useDevServers)
		return IP_UPDATE_HOST_DEV;
	else
		return IP_UPDATE_HOST;
}

const char *GetIpUpdateUrl()
{
	CString url = "/nic/update?token=";
	assert(g_pref_token);
	url += g_pref_token;
	url += "&api_key=";
	url += API_KEY;
	url += "&v=2";
	url += "&hostname=";
	if (g_pref_hostname)
		url += g_pref_hostname;
	const char *urlTxt = (const char*)TStrToStr(url);
	return urlTxt;
}

bool IsApiHostHttps()
{
	if (g_useDevServers)
		return API_IS_HTTPS_DEV;
	else
		return API_IS_HTTPS;
}

const TCHAR *GetDashboardUrl()
{
	if (g_useDevServers)
		return DASHBOARD_URL_DEV;
	else
		return DASHBOARD_URL;
}

bool CanSendIPUpdates()
{
	if (strempty(g_pref_user_name))
		return false;
	if (strempty(g_pref_token))
		return false;
	if (!streq(UNS_OK, g_pref_user_networks_state))
		return false;
	return true;
}

void RegisterErrorNotifMsg()
{
	assert(0 == g_errorNotifMsg); // only call me once
	// reusing wnd class name as messge name, becuase there's no
	// reason not to
	g_errorNotifMsg = RegisterWindowMessage(MAIN_WINDOWS_CLASS_NAME);
}

DWORD GetExplorerProcessId()
{
    PROCESSENTRY32 processEntry;
	BOOL ok;
    DWORD explorerProcessId = 0;
	HANDLE hSnap = INVALID_HANDLE_VALUE;
	DWORD sessId = WTSGetActiveConsoleSessionId();
	if (GetSystemMetrics(SM_REMOTESESSION)) {
		DWORD currProcessId = GetCurrentProcessId();
		DWORD newSessId = 0xFFFFFFFF;
		ProcessIdToSessionId(currProcessId, &newSessId);
		sessId = newSessId;
	}

	hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hSnap)
		goto Exit;

	processEntry.dwSize = sizeof(processEntry);
	if (!Process32First(hSnap, &processEntry))
		goto Exit;
	for (;;) {
		TCHAR *procExe = processEntry.szExeFile;
		if (tstreq(procExe, _T("explorer.exe"))) {
			DWORD explorerSessId = 0;
			ok = ProcessIdToSessionId(processEntry.th32ProcessID, &explorerProcessId);
			if (ok && (sessId == explorerSessId)) {
				explorerProcessId = processEntry.th32ProcessID;
				break;
			}
		}
		if (!Process32Next(hSnap, &processEntry))
			break;
	}

Exit:
	CloseHandle(hSnap);
	return explorerProcessId;
}

BOOL GetUserNameDomainFromSid(PSID sid, TCHAR **userNameOut, TCHAR **userDomainOut)
{
	TCHAR userName[128] = {0};
	TCHAR domainName[128] = {0};
	DWORD cchUserName = dimof(userName)-1;
	DWORD cchDomainName = dimof(domainName)-1;
	SID_NAME_USE sidNameUse;
	BOOL ok = LookupAccountSid(NULL, sid, 
		&(userName[0]), &cchUserName,
		&(domainName[0]), &cchDomainName,
		&sidNameUse);
	if (!ok)
		return FALSE;

	*userNameOut = tstrdup(userName);
	*userDomainOut = tstrdup(domainName);
	return TRUE;
}

BOOL GetUserNameDomainFromToken(HANDLE token, TCHAR **userNameOut, TCHAR **userDomainOut)
{
	BOOL ok;
	TOKEN_OWNER *tokenOwner = NULL;
	DWORD tokenSize = 0;
	GetTokenInformation(token, TokenOwner, NULL, 0, &tokenSize);
	if (0 == tokenSize)
		return FALSE;
	tokenOwner = (TOKEN_OWNER*)malloc(tokenSize);
	if (!tokenOwner)
		return FALSE;
	DWORD returnSize;
	ok = GetTokenInformation(token, TokenOwner, tokenOwner, tokenSize, &returnSize);
	if (!ok)
		return FALSE;
	ok =  GetUserNameDomainFromSid(tokenOwner->Owner, userNameOut, userDomainOut);
	free(tokenOwner);
	return ok;
}

// returns INVALID_HANDLE_VALUE on error
HANDLE GetExplorerOwnerToken(TCHAR **userNameOut, TCHAR **userDomainOut)
{
	HANDLE procHandle = NULL;
	HANDLE tokenHandle = NULL;
	HANDLE result = INVALID_HANDLE_VALUE;
	BOOL ok;
	DWORD procId = GetExplorerProcessId();
	if (0 == procId)
		goto Exit;
	procHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, procId);
	if (NULL == procHandle)
		goto Exit;
	ok = OpenProcessToken(procHandle, MAXIMUM_ALLOWED, &tokenHandle);
	if (!ok)
		goto Exit;

	ok = DuplicateTokenEx(tokenHandle, TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &result);
	if (!ok)
		goto Exit;

	ok = GetUserNameDomainFromToken(tokenHandle, userNameOut, userDomainOut);
	if (!ok) {
		CloseHandle(result);
		result = INVALID_HANDLE_VALUE;
		goto Exit;
	}
Exit:
	CloseHandle(tokenHandle);
	CloseHandle(procHandle);
	return result;
}

bool IsLeftAltPressed()
{
	SHORT keyState = GetAsyncKeyState(VK_LMENU);
	if (keyState & 0x8000)
		return true;
	return false;
}

bool IsLeftCtrlPressed()
{
	SHORT keyState = GetAsyncKeyState(VK_LCONTROL);
	if (keyState & 0x8000)
		return true;
	return false;
}

bool IsLeftAltAndCtrlPressed()
{
	return IsLeftAltPressed() && IsLeftCtrlPressed();
}

static TCHAR *TBufAppend(TCHAR *start, TCHAR *end, TCHAR *toAppend)
{
	if (start >= end)
		return start;

	size_t len = tstrlen(toAppend);
	size_t maxLen = end - start - 1;
	if (len > maxLen)
		len = maxLen;
	memmove(start, toAppend, len * sizeof(TCHAR));
	start += len;
	start[0] = 0;
	return start;
}

static TCHAR *FormatNum(TCHAR *cur, TCHAR *end, int num, TCHAR *prefix)
{
	TCHAR numBuf[16];
	_itot(num, numBuf, 10);
	cur = TBufAppend(cur, end, numBuf);
	cur = TBufAppend(cur, end, _T(" "));
	cur = TBufAppend(cur, end, prefix);
	if (1 != num)
		cur = TBufAppend(cur, end, _T("s"));
	return cur;
}

TCHAR *FormatUpdateTime(int minutes)
{
	TCHAR buf[256];

	int hours = minutes / 60;
	minutes = minutes % 60);
	assert(minutes < 60);
	assert(minutes >= 0);

	buf[0] = 0;
	TCHAR *cur = &(buf[0]);
	TCHAR *end = cur + dimof(buf) - 1;

	if (hours > 0) {
		cur = FormatNum(cur, end, hours, _T("hr"));
		cur = TBufAppend(cur, end, _T(" "));
	}
	cur = FormatNum(cur, end, minutes, _T("minute"));
	return tstrdup(buf);
}

