// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "JsonParser.h"
#include "Http.h"
#include "StrUtil.h"

#define PROGRAM_VERSION  _T("1.0")

#define AUTO_UPDATE_HOST _T("opendnsupdate.appspot.com")
#define AUTO_UPDATE_URL "/updatecheck/dynamicipwin"

enum VersionUpdateCheckType {
	UpdateCheckInstall,
	UpdateCheckUninstall,
	UpdateCheckVersionCheck
};

// mark unused variables to reduce compiler warnings
#define UNUSED_VAR( x )  (x) = (x)

static CString CommonUrlPart(CString url)
{
	CString s = url;
	char num[32];

	s += "&i=";
	s += "1.3"; // a dummy unique id to denote check from 13Updater.exe

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
	CString s = AUTO_UPDATE_URL;

	s += "?v=";
	s += version;

	s += "&t=";
	s += type;
	return CommonUrlPart(s);
}

bool FileOrDirExists(const TCHAR *fileName)
{
    DWORD fileAttr = GetFileAttributes(fileName);
    if (0xFFFFFFFF == fileAttr)
        return false;
    return true;
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

TCHAR *DownloadUpdateIfNotDownloaded(const char *url)
{
	const TCHAR *filePathStr = NULL;
	const char *fileName = StrFindLastChar(url, '/');
	if (!fileName)
		return NULL;
	++fileName;
	CString filePath = AppDataDir();
	filePath += PATH_SEP_STR;
	filePath += fileName;

	filePathStr = filePath;
	if (FileOrDirExists(filePathStr))
		return tstrdup(filePathStr);

	HttpResult *httpResult = HttpGet(url);
	if (!httpResult || !httpResult->IsValid())
		goto Error;

	DWORD size;
	void *s = httpResult->data.getData(&size);
	if (!s)
		goto Error;

	BOOL ok = FileWriteAll(filePathStr, (const char*)s, size);
	if (!ok)
		goto Error;

	return tstrdup(filePathStr);
Error:
	free((void*)filePathStr);
	return NULL;
}

// sends auto-update check. Returns url of the new version to download
// if an update is available or NULL if update is not available
// (or there was an error getting the upgrade info)
// Caller needs to free() the result.
char *GetUpdateUrl(const TCHAR *version, VersionUpdateCheckType type)
{
	char *downloadUrl = NULL;
	JsonEl *json = NULL;

	char *typeStr = "c";
	if (UpdateCheckInstall == type)
		typeStr = "i";
	else if (UpdateCheckUninstall == type)
		typeStr = "u";
	else if (UpdateCheckVersionCheck == type)
		typeStr = "c";
	else
		assert(0);

	CString url = AutoUpdateUrl(version, typeStr);
	HttpResult *res = HttpGet(AUTO_UPDATE_HOST, url, false /* https */);
	if (!res || !res->IsValid())
		return NULL;
	char *s = (char *)res->data.getData(NULL);
	json = ParseJsonToDoc(s);
	JsonEl *upgradeAvailable = GetMapElByName(json, "upgrade");
	JsonElBool *upgradeAvailableBool = JsonElAsBool(upgradeAvailable);
	if (!upgradeAvailableBool || !upgradeAvailableBool->boolVal)
		goto Exit;
	JsonEl *updateUrl = GetMapElByName(json, "download");
	if (!updateUrl)
		goto Exit;
	downloadUrl = StrDupSafe(JsonElAsStringVal(updateUrl));
Exit:
	JsonElFree(json);
	free(s);
	return downloadUrl;
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

#define DIRECT_URL _T("http://www.opendns.com/download/windows/dynamicip")

int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPTSTR /*cmdLine*/, int /* nCmdShow */)
{
    char *url = NULL;
    TCHAR *filePath = NULL;
    int res;

    url = GetUpdateUrl(_T("1.3"), UpdateCheckVersionCheck);
    if (!url) {
	res = ::MessageBox(NULL, _T("Couldn't determine the download. You can install upgrade directly by downloading http://www.opendns.com/download/windows/dynamicip. Upgrade?"), _T("Error"), MB_YESNO);
	if (IDYES == res) {
	    LaunchUrl(DIRECT_URL);
	}
	goto Exit;
    }
    filePath = DownloadUpdateIfNotDownloaded(url);
    if (!filePath) {
	res = ::MessageBox(NULL, _T("Couldn't download the upgrade. You can install upgrade directly by downloading http://www.opendns.com/download/windows/dynamicip. Upgrade?"), _T("Error"), MB_YESNO);
	if (IDYES == res) {
	    LaunchUrl(DIRECT_URL);
	}
	goto Exit;
    }

    LaunchUrl(filePath);

Exit:
    free(filePath);
    free(url);
    return 0;
}
