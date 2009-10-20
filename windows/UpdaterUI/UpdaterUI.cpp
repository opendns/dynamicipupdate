// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

#include "base64decode.h"
#include "WTLThread.h"
#include "MiscUtil.h"
#include "resource.h"

#include "CrashHandler.h"
#include "IpUpdatesLog.h"
#include "MainFrm.h"

#include "Prefs.h"
#include "SimpleLog.h"
#include "SingleInstance.h"

extern int run_unit_tests();

int simulatedError = SE_NO_ERROR;
bool g_simulate_upgrade = false;

CAppModule _Module;

#define AUTO_START_KEY_PATH _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run")
#define AUTO_START_KEY_NAME _T("OpenDNS Updater")
#define UNINSTALL_FEEDBACK_URL _T("http://opendns.com/feedback/windows-updater-uninstall")

static void AddToAutoStart()
{
	TCHAR exePath[MAX_PATH];
	DWORD len = GetModuleFileName(NULL, exePath, dimof(exePath));
	if ((len == dimof(exePath)) && (ERROR_INSUFFICIENT_BUFFER == GetLastError())) {
		assert(0);
		return;
	}
	TCHAR *s = TStrCat(_T("\""), exePath, _T("\""), _T(" /autostart"));
	WriteRegStr(HKEY_CURRENT_USER, AUTO_START_KEY_PATH, AUTO_START_KEY_NAME, s, TRUE /* overwrite */);
	free(s);
}

static void RemoveFromAutoStart()
{
	DeleleteReg(HKEY_CURRENT_USER, AUTO_START_KEY_PATH, AUTO_START_KEY_NAME);
}

static void SendErrorNotifMsg(int specialCmd)
{
	HWND hwndExisting = FindWindow(MAIN_WINDOWS_CLASS_NAME, NULL);
	assert(hwndExisting);
	if (!hwndExisting)
		return;
	assert(0 != g_errorNotifMsg);
	::SendMessage(hwndExisting, g_errorNotifMsg, (WPARAM)specialCmd, 0);
}

int Run(bool show)
{
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;
	CString appDataDir = AppDataDir();
	// to avoid jarring resizing of the window at startup, create a window with
	// a size close to what the calculated size will be in most frequent case.
	// 328x290 was chosen empirically by measuring the window on XP
	RECT r = { CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT+328, CW_USEDEFAULT+290 };
	if (wndMain.CreateEx(NULL, r) == NULL)
		return 0;

	if (GetPrefValBool(g_pref_run_hidden)) {
		show = false;
	}

	wndMain.m_simulatedError = (SimulatedError)simulatedError;
	if (show)
		wndMain.SwitchToVisibleState();
	else
		wndMain.SwitchToHiddenState();

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

void SendAutoUpdateCheck(VersionUpdateCheckType type)
{
	char *updateUrl = GetUpdateUrl(PROGRAM_VERSION, type);
	// we don't care about update url here
	free(updateUrl);
}

static void SkipUtf8Bom(char **data)
{
	unsigned char *d = (unsigned char*)*data;
	if ((0xef == d[0]) &&
		(0xbb == d[1]) &&
		(0xbf == d[2])) {
			*data = (char*)d+3;
	}
}

static void OldSettingLineSplit(char *line, char **nameOut, char **valOut)
{
	StrSkipWs(&line);
	*nameOut = line;
	char *val = (char*)StrFindChar(line, '=');
	if (!val)
		return;
	*val++ = 0;
	StrStripWsRight(line);
	StrSkipWs(&val);
	*valOut = val;
	StrStripWsRight(val);
}

static char *g_imported_user_name;
static char *g_imported_password;
static char *g_imported_hostname;

// very similar to CMainFrame::OnDownloadNetworks()
static void VerifyHostname(char *hostName)
{
	HttpResult *httpResult = NULL;
	char *jsonTxt = NULL;
	JsonEl *json = NULL;
	NetworkInfo *ni = NULL;
	NetworkInfo *dynamicNetwork = NULL;

	CString params = ApiParamsNetworksGet(g_pref_token);
	const char *paramsTxt = TStrToStr(params);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	httpResult = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);		
	free((void*)paramsTxt);
	if (!httpResult ||  !httpResult->IsValid())
		return;

	jsonTxt = (char*)httpResult->data.getData(NULL);
	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Exit;

	WebApiStatus status = GetApiStatus(json);
	if (WebApiStatusSuccess != status)
		goto Exit;

	ni = ParseNetworksGetJson(json);
	if (!ni)
		goto Exit;
	size_t networksCount = ListLengthGeneric(ni);
	assert(0 != networksCount);
	if (0 == networksCount)
		goto Exit;

	size_t dynamicNetworksCount = DynamicNetworksCount(ni);
	if (0 == dynamicNetworksCount)
		goto NoDynamicNetworks;

	NetworkInfo *network = FindDynamicWithLabel(ni, hostName);
	if (network) {
		SetPrefVal(&g_pref_user_networks_state, UNS_OK);
		SetPrefVal(&g_pref_network_id, network->networkId);
		PrefSetHostname(network->label);
		goto Exit;
	}

	dynamicNetwork = FindFirstDynamic(ni);
	assert(dynamicNetwork);
	if (!dynamicNetwork)
		goto Exit;

	if (1 == dynamicNetworksCount) {
		PrefSetHostname(dynamicNetwork->label);
		SetPrefVal(&g_pref_network_id, dynamicNetwork->networkId);
		SetPrefVal(&g_pref_user_networks_state, UNS_OK);
		goto Exit;
	}

	goto NoNetworkSelected;

Exit:
	NetworkInfoFreeList(ni);
	JsonElFree(json);
	free(jsonTxt);
	delete httpResult;
	return;

NoDynamicNetworks:
	dynamicNetwork = MakeFirstNetworkDynamic(ni);
	if (dynamicNetwork != NULL) {
		PrefSetHostname(dynamicNetwork->label);
		SetPrefVal(&g_pref_network_id, dynamicNetwork->networkId);
		SetPrefVal(&g_pref_user_networks_state, UNS_OK);
		goto Exit;
	}
	SetPrefVal(&g_pref_user_networks_state, UNS_NO_DYNAMIC_IP_NETWORKS);
	SetPrefVal(&g_pref_network_id, NULL);
	SetPrefVal(&g_pref_hostname, NULL);
	goto Exit;

NoNetworkSelected:
	SetPrefVal(&g_pref_user_networks_state, UNS_NO_NETWORK_SELECTED);
	SetPrefVal(&g_pref_network_id, NULL);
	SetPrefVal(&g_pref_hostname, NULL);
	goto Exit;
}

static bool VerifyUserNamePassword(char *userName, char *pwd)
{
	bool res = false;
	HttpResult *httpResult = NULL;
	char *jsonTxt = NULL;
	JsonEl *json = NULL;

	CString params = ApiParamsSignIn(userName, pwd);
	const char *paramsTxt = TStrToStr(params);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	httpResult = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
	free((void*)paramsTxt);

	if (!httpResult || !httpResult->IsValid())
		goto Error;

	jsonTxt = (char*)httpResult->data.getData(NULL);
	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Error;
	WebApiStatus status = GetApiStatus(json);
	if (WebApiStatusSuccess != status)
		goto Error;

	char *tokenTxt = GetApiResponseToken(json);
	if (!tokenTxt)
		goto Error;

	SetPrefVal(&g_pref_user_name, userName);
	SetPrefVal(&g_pref_token, tokenTxt);

	res = true;
Error:
	free(jsonTxt);
	delete httpResult;
	return res;
}

static void VerifyImportedSettings()
{
	char *pwd = NULL;
	if (strempty(g_imported_user_name))
		return;
	if (strempty(g_imported_password))
		return;
	pwd = b64decode(g_imported_password);
	if (!pwd)
		return;

	if (!VerifyUserNamePassword(g_imported_user_name, pwd))
		goto Error;

	VerifyHostname(g_imported_hostname);

Error:
	free(pwd);
}

static void ParseSettingsFromOldClient(char *data)
{
	SkipUtf8Bom(&data);
	data = StrNormalizeNewline(data, UNIX_NEWLINE);
	char *line;
	char *tmp = data;
	char *name, *val;
	for(;;) {
		line = StrSplitIter(&tmp, UNIX_NEWLINE_C);
		if (!line)
			break;
		OldSettingLineSplit(line, &name, &val);
		if (streq(name, "username") && !strempty(val)) {
			g_imported_user_name = strdup(val);
			slog("imported username='");
			slognl(val);
		} else if (streq(name, "password") && !strempty(val)) {
			g_imported_password = strdup(val);
			slog("imported password\n");
		} else if (streq(name, "hostname") && !strempty(val)) {
			g_imported_hostname = strdup(val);
			slog("imported hostname='");
			slognl(val);
		}
		free(line);
	}
	free(data);
	VerifyImportedSettings();
	free(g_imported_user_name);
	free(g_imported_password);
	free(g_imported_hostname);
}

static void DeleteSettingsFromOldClient()
{
	CString file = OldSettingsFileName();
	::DeleteFile(file);
	file = OldSettingsFileName2();
	::DeleteFile(file);
}

static void DeleteOldClientExecutable()
{
	TCHAR		dir[MAX_PATH];
	SHGetSpecialFolderPath(NULL, dir, CSIDL_PROGRAM_FILES, TRUE);
	CString file(dir);
	file += _T("\\OpenDNS Updater\\OpenDNS Updater.exe");
	::DeleteFile(file);
}

static void ImportSettingsFromOldClient()
{
	if (!strempty(g_pref_user_name))
		return; // we're already configured

	CString oldSettingsFileName = OldSettingsFileName();
	uint64_t dataSize;
	char *data = FileReadAll(oldSettingsFileName, &dataSize);
	if (!data)
		return;
	ParseSettingsFromOldClient(data);
	free(data);
}

static void DoInstallStep()
{
	ImportSettingsFromOldClient();
	DeleteSettingsFromOldClient();
	DeleteOldClientExecutable();
	AddToAutoStart();
	SendAutoUpdateCheck(UpdateCheckInstall);
}

static void DoUninstallStep()
{
	RemoveFromAutoStart();
	SendAutoUpdateCheck(UpdateCheckUninstall);
	if (g_pref_user_name) {
		char *usernameEncoded = StrUrlEncode(g_pref_user_name);
		CString url = UNINSTALL_FEEDBACK_URL;
		url += "?account=";
		url += usernameEncoded;
		LaunchUrl(url);
		free(usernameEncoded);
	} else {
		LaunchUrl(UNINSTALL_FEEDBACK_URL);
	}
}

void SubmitAndDeleteCrashDump(const TCHAR *filePath)
{
	CString url;
	uint64_t fileSize;
	char *fileData = FileReadAll(filePath, &fileSize);
	if (!fileData)
		goto Exit;

	//char *host = "127.0.0.1";
	char *host = "opendnsupdate.appspot.com";
	url = CrashDumpUrl(PROGRAM_VERSION);
	char *url2 = TStrToStr(url);
	DWORD dataSize = (DWORD)fileSize;
	HttpResult *httpResult = HttpPostData(host, url2, fileData, dataSize, false /*https*/);
	if (httpResult && httpResult->IsValid()) {
		char *res = (char*)httpResult->data.getData(NULL);
		slog("Sent crashdump. Response: ");
		slognl(res);
		free(res);
	}
	free(url2);
Exit:
	DeleteFile(filePath);
}

void SendCrashDumps()
{
	WIN32_FIND_DATA fileData;
	HANDLE h;
	CString crashDumpFilePattern = AppDataDir();
	crashDumpFilePattern += _T("\\*.dmp");
	h = FindFirstFile(crashDumpFilePattern, &fileData);
	if (INVALID_HANDLE_VALUE == h)
		return;
	BOOL ok = TRUE;
	while (ok) {
		TCHAR *fileName = fileData.cFileName;
		assert(TStrEndsWithI(fileName, _T(".dmp")));
		CString filePath = AppDataDir() + _T("\\") + fileName;
		SubmitAndDeleteCrashDump(filePath);
		ok = FindNextFile(h, &fileData);
	}
	FindClose(h);
}

static void FindSimulatedError(TCHAR *cmdLine)
{
	int pos = TStrFind(cmdLine, _T("/simerr"));
	if (-1 == pos)
		return;
	
	TCHAR *errNoStr = cmdLine + pos + dimof(_T("/simerr"));
	simulatedError = _ttoi(errNoStr);
}

static CString LogFileName(const TCHAR *dir)
{
	CString fileName = dir;
	fileName += "\\log.txt";
	return fileName;
}

static bool IsApiKeyCharValid(char c)
{
	static char *validKeyChars = "0123456789ABCDEF";
	char *tmp = validKeyChars;
	char c2 = *tmp++;
	while (c2) {
		if (c == c2)
			return true;
		c2 = *tmp++;
	}
	return false;
}

static bool IsApiKeyValid(char *apiKey)
{
	char c = *apiKey++;
	while (c) {
		if (!IsApiKeyCharValid(c))
			return false;
		c = *apiKey++;
	}
	return true;
}

#if 0
static void SetDnsServers(IWbemServices *pSvc, IWbemClassObject *pObj, OLECHAR *srv1, OLECHAR *srv2)
{
	HRESULT			hr;
	SAFEARRAYBOUND	parrayBound;
	VARIANT			vtProp;
	_variant_t		serverList;
	int				srvCount = 0;
	CString			methodPath;
	BSTR			methodName = SysAllocString(L"SetDNSServerSearchOrder");
	BSTR			methodPathBstr = NULL;
	IWbemClassObject *pAdapterClassObj = NULL;
	IWbemClassObject *pAdapterClassInst = NULL;
	IWbemClassObject *pSetDNSServerSearchOrderIn = NULL;
	IWbemClassObject *pSetDNSServerSearchOrderOut = NULL;

	VariantInit(&vtProp);
	hr = pObj->Get(L"Index", 0, &vtProp, 0, 0);
	if (FAILED(hr))
		goto Exit;

	assert(vtProp.vt == VT_I4);
	if (vtProp.vt != VT_I4)
		goto Exit;

	int idx = vtProp.lVal;

	hr = pSvc->GetObject(
		bstr_t("Win32_NetworkAdapterConfiguration"),
		WBEM_FLAG_RETURN_WBEM_COMPLETE,
		NULL,
		&pAdapterClassObj,
		NULL);
	if (FAILED(hr))
		goto Exit;

	hr = pAdapterClassObj->GetMethod(methodName, 0, &pSetDNSServerSearchOrderIn, NULL);
	if (FAILED(hr))
		goto Exit;

	hr = pAdapterClassObj->SpawnInstance(0, &pAdapterClassInst);
	if (FAILED(hr))
		goto Exit;

	if (srv1)
		++srvCount;
	if (srv2)
		++srvCount;
	if (0 == srvCount)
		return;

	parrayBound.cElements = srvCount;
	parrayBound.lLbound = 0;
	serverList.vt = VT_ARRAY | VT_BSTR;
	if (0 == srvCount) {
		serverList.parray = NULL;
	} else {
		serverList.parray = ::SafeArrayCreateEx(VT_BSTR, 1, &parrayBound, NULL);

		BSTR* dnsServerAddress;
		::SafeArrayAccessData(serverList.parray, reinterpret_cast<LPVOID*>(&dnsServerAddress));
		int index = 0;
		if (srv1)
		{
			dnsServerAddress[index] = ::SysAllocString(srv1);
			index++;
		}
		if (srv2) 
		{
			dnsServerAddress[index] = ::SysAllocString(srv2);
			index++;
		}
		::SafeArrayUnaccessData(serverList.parray);
	}

	hr = pAdapterClassObj->Put(bstr_t("DNSServerSearchOrder"), 0, &serverList, 0);
	if (FAILED(hr))
		goto Exit;

	methodPath.Format("Win32_NetworkAdapterConfiguration.Index='%d'", (int)idx);
	methodPath.SetSysString(&methodPathBstr);
	hr = pSvc->ExecMethod(methodPathBstr, methodName, 0, NULL, pSetDNSServerSearchOrderIn, &pSetDNSServerSearchOrderOut, NULL);
	if (FAILED(hr))
		goto Exit;
Exit:
	if (pAdapterClassObj) pAdapterClassObj->Release();
	if (pAdapterClassInst) pAdapterClassInst->Release();
	if (pSetDNSServerSearchOrderOut) pSetDNSServerSearchOrderOut->Release();
	if (pSetDNSServerSearchOrderIn) pSetDNSServerSearchOrderIn->Release();
	::SysFreeString(methodName);
	::SysFreeString(methodPathBstr);
}

static void ClearDnsServers(IWbemServices *pSvc, IWbemClassObject *pObj)
{
	SetDnsServers(pSvc, pObj, NULL, NULL);
}

// We don't want to change the DNS settings of VirtualBox/VMWare etc. virtual ethernet adapters
// I wish I knew a better heurstic to filter non-physical adapters
static BOOL ShouldSkipNetworkAdapter(WCHAR *caption)
{
	// virtual adapter from VirtualBox
	if (-1 != WStrFind(caption, L"VirtualBox"))
		return TRUE;
	// virtual adapter from coLinux (http://colinux.wikia.com/wiki/TAP-Win32_Adapter_V8_%28coLinux%29)
	if (-1 != WStrFind(caption, L"TAP-Win32"))
		return TRUE;
	// virtual adapter from VMWare
	if (-1 != WStrFind(caption, L"VMware"))
		return TRUE;
	return FALSE;
}

void SetOpenDnsServersOnAllAdapters()
{
	IWbemLocator *pLoc = NULL;
	IWbemServices *pSvc = NULL;
	IEnumWbemClassObject* pEnumerator = NULL;

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED); 
	if (FAILED(hr))
		return;

	hr =  CoInitializeSecurity(
		NULL, 
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
	);

	if (FAILED(hr))
		goto Exit;

	hr = CoCreateInstance(
		CLSID_WbemLocator,             
		0, 
		CLSCTX_INPROC_SERVER, 
		IID_IWbemLocator, (LPVOID *) &pLoc);
 
	if (FAILED(hr))
		goto Exit;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hr = pLoc->ConnectServer(
		 bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		 NULL,					  // User name. NULL = current user
		 NULL,					  // User password. NULL = current
		 0, 					  // Locale. NULL indicates current
		 NULL,					  // Security flags.
		 0, 					  // Authority (e.g. Kerberos)
		 0, 					  // Context object 
		 &pSvc					  // pointer to IWbemServices proxy
		 );

	if (FAILED(hr))
		goto Exit;

	// Set security levels on the proxy
	hr = CoSetProxyBlanket(
		pSvc,						// Indicates the proxy to set
		RPC_C_AUTHN_WINNT,			// RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,			// RPC_C_AUTHZ_xxx
		NULL,						// Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,		// RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,						// client identity
		EOAC_NONE					// proxy capabilities 
	);

	if (FAILED(hr))
		goto Exit;

	hr = pSvc->ExecQuery(
		bstr_t("WQL"), 
		bstr_t("SELECT * FROM Win32_NetworkAdapterConfiguration"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
		NULL,
		&pEnumerator);

	if (FAILED(hr))
		goto Exit;

	if (!pEnumerator)
		goto Exit;

	IWbemClassObject *pObj;
	ULONG result = 0;

	for (;;)
	{
		VARIANT vtProp;
		WCHAR *s;
		VariantInit(&vtProp);
		hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &result);
		if (0 == result)
			break;

		hr = pObj->Get(L"IPEnabled", 0, &vtProp, 0, 0);
		if (FAILED(hr) || !vtProp.boolVal)
			goto Next;

#if 0
		VariantClear(&vtProp);
		hr = pObj->Get(L"SettingID", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
		s = vtProp.bstrVal;
#endif

		VariantClear(&vtProp);
		hr = pObj->Get(L"Caption", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
		s = vtProp.bstrVal;

		if (ShouldSkipNetworkAdapter(s))
			goto Next;

#if 0
		VariantClear(&vtProp);
		hr = pObj->Get(L"DNSServerSearchOrder", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
#endif

		SetDnsServers(pSvc, pObj, OLESTR("208.67.222.222"), OLESTR("208.67.220.220 "));
Next:
		VariantClear(&vtProp);
		pObj->Release();
	}

Exit:
	if (pEnumerator) pEnumerator->Release();
	if (pSvc) pSvc->Release();
	if (pLoc) pLoc->Release();

	CoUninitialize();
	return;
}

int WINAPI _tWinMain(HINSTANCE  /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPTSTR /* cmdLine */, int /* nCmdShow */)
{
	SetOpenDnsServersOnAllAdapters();
}
#else
int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR cmdLine, int /* nCmdShow */)
{
	int nRet = 0;
	int specialCmd = SPECIAL_CMD_NONE;

#ifdef DEBUG
	int failedCount = run_unit_tests();
	if (0 != failedCount) {
		return failedCount;
	}
#endif

	if (!IsApiKeyValid(API_KEY)) {
		::MessageBox(NULL, _T("Invalid API key"), _T("Error"), MB_OK);
		return 1;
	}

	CString appDataDir = AppDataDir();
	InstallCrashHandler(appDataDir, GUI_EXE_NAME_WITHOUT_EXE);

	LoadIpUpdatesHistory();

	SLogInit(LogFileName(appDataDir));
	slognl("-------- starting");

	if (-1 != TStrFind(cmdLine, _T("/devserver"))) {
		UseDevServers(true);
	}

	bool wasAutoStart = TStrContains(cmdLine, _T("/autostart"));

	if (TStrContains(cmdLine, _T("/install")))
		specialCmd = SPECIAL_CMD_INSTALL;

	if (TStrContains(cmdLine, _T("/uninstall")))
		specialCmd = SPECIAL_CMD_UNINSTALL;

	if (TStrContains(cmdLine, _T("/simupgradecheck")))
		g_simulate_upgrade = true;
	
	if (TStrContains(cmdLine, CMD_ARG_SEND_CRASHDUMP))
		specialCmd = SPECIAL_CMD_SEND_CRASHDUMPS;

	FindSimulatedError(cmdLine);

	HRESULT hRes = ::CoInitialize(NULL);
	ATLASSERT(SUCCEEDED(hRes));
	static const DWORD allClasses = 0xffff;
	AtlInitCommonControls(allClasses);

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	LPCTSTR richEditLibName = CRichEditCtrl::GetLibraryName();
	HINSTANCE hInstRich = ::LoadLibrary(richEditLibName);
	ATLASSERT(hInstRich != NULL);
	if (hInstRich == NULL) {
		TCHAR buf[512];
		_sntprintf(buf, dimof(buf), _T("No rich edit control '%s' found"), richEditLibName);
		::MessageBox(NULL, buf, _T("Error"), MB_OK);
		goto Exit;
	}

	RegisterErrorNotifMsg();
	BOOL isAlreadyRunning = IsInstancePresent(APP_UNIQUE_GUID);
	if (isAlreadyRunning && (SPECIAL_CMD_NONE == specialCmd)) {
		HWND hwndExisting = FindWindow(MAIN_WINDOWS_CLASS_NAME, NULL);
		if (!hwndExisting) {
			slognl("Didn't find a window");
			goto Exit;
		}
		SendErrorNotifMsg(SPECIAL_CMD_SHOW);
		BringWindowToTop(hwndExisting);
		goto Exit;
	}

	PreferencesLoad();

	// just exit if we auto-started and we won't send updates
	BOOL sendingUpdates = GetPrefValBool(g_pref_send_updates);
	if (wasAutoStart && !sendingUpdates)
			goto Exit;

	GenUidIfNotExists();

	if (SPECIAL_CMD_INSTALL == specialCmd) {
		DoInstallStep();
		PreferencesSave();
		goto Exit;
	}

	if (SPECIAL_CMD_UNINSTALL == specialCmd) {
		DoUninstallStep();
		goto Exit;
	}

	if (SPECIAL_CMD_SEND_CRASHDUMPS == specialCmd) {
		SendCrashDumps();
		goto Exit;
	}

	AddToAutoStart();
	bool showWindow = true;
	if (wasAutoStart)
		showWindow = false;

	nRet = Run(showWindow);

	PreferencesSave();
Exit:
	PreferencesFree();
	slognl("finished");
	SLogStop();
	FreeIpUpdatesHistory();

	::FreeLibrary(hInstRich);
	_Module.Term();
	::CoUninitialize();

	return nRet;
}
#endif
