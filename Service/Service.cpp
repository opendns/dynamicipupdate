// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include <conio.h>

#include "Errors.h"
#include "CrashHandler.h"
#include "DnsCheckThread.h"
#include "JsonParser.h"
#include "JsonApiResponses.h"
#include "MiscUtil.h"
#include "Prefs.h"
#include "SampleApiResponses.h"
#include "SendIPUpdate.h"
#include "ServiceManager.h"
#include "SimpleLog.h"
#include "StrUtil.h"
#include "UnitTests.h"

extern int run_unit_tests();

/*
Service controller. Command line arguments:
  install - installs the service
  remove - removes the service
  debug - run in debug mode
  ut or unittests - run unittests

If run without arguments, starts the service.
*/

static HANDLE g_serviceStopEvent;
static SERVICE_STATUS_HANDLE g_serviceHandle;
static ULONGLONG g_lastIpUpdateTimeInMs;
static ULONGLONG g_lastSoftwareUpgradeTimeInMs;

static SERVICE_DESCRIPTION description = { 
	_T("OpenDNS Dynamic IP Client. See http://www.opendns.com/support/service for details.")
};

// convenient to use in the debugger 
bool g_forceStop = false;
bool g_paused = false;

static bool g_debugMode = false;

static const ULONGLONG THREE_HRS_IN_MS =  3*60*60*1000;
static const ULONGLONG ONE_DAY_IN_MS   = 24*60*60*1000;

static void LaunchGuiWithParam(TCHAR *param)
{
	HANDLE userToken;
	TCHAR *userName = NULL;
	TCHAR *domainName = NULL;
	TCHAR* guiExePath = FindGuiExePath();
	if (!guiExePath) {
		slog("LaunchGuiWithParam(): FindGuiExePath() failed\n");
		return;
	}
	userToken = GetExplorerOwnerToken(&userName, &domainName);
	free(userName);
	free(domainName);
	if (INVALID_HANDLE_VALUE == userToken) {
		slog("LaunchGuiWithParam(): GetUserToken() failed\n");
		return;
	}
	slog("Executing: ");
	slog(guiExePath);
	if (param)
		slog(param);
	slog(" \n");

	STARTUPINFO          startupInfo = {0};
	PROCESS_INFORMATION  processInfo = {0};
	startupInfo.cb = sizeof(STARTUPINFO);
	startupInfo.wShowWindow = SW_SHOW;
	TCHAR *cmdLine = TStrCat(_T("\""), guiExePath, _T("\" "), param);
	BOOL ok = CreateProcessAsUser(userToken,
		NULL, cmdLine,
		NULL, NULL, FALSE,
		DETACHED_PROCESS,
		NULL, NULL,
		&startupInfo, &processInfo);
	if (!ok)
		slog("LaunchGuiWithParam(): CreateProcessAsUser() failed\n");
	free(cmdLine);
    if (processInfo.hThread)
		CloseHandle(processInfo.hThread);
    if (processInfo.hProcess)
		CloseHandle(processInfo.hProcess);

	CloseHandle(userToken);
}

static bool ShouldSendPeriodicUpdate()
{
	ULONGLONG currTimeInMs = GetTickCount();

	// the time wraps-around every 49.7 days.
	if (currTimeInMs < g_lastIpUpdateTimeInMs) {
		// going backwards in time - it must be wrap around
		g_lastIpUpdateTimeInMs = GetTickCount();
		slog("Detected GetTickCount() wrap-around\n");
	}

	ULONGLONG nextUpdateTimeInMs = g_lastIpUpdateTimeInMs + THREE_HRS_IN_MS;
	if (currTimeInMs > nextUpdateTimeInMs)
		return true;
	return false;
}

static IpUpdateResult g_prevIpUpdateResult = IpUpdateOk;

static void HandleIPUpdateResponse(char *resp)
{
	if (!resp)
		return;

	if (IpUpdateOk != g_prevIpUpdateResult) {
		// If previously wasn't ok, then siently ignore.
		// We don't want to flood the user with too many
		// messages
		return;
	}

	g_prevIpUpdateResult = IpUpdateResultFromString(resp);
	if (IpUpdateNotYours == g_prevIpUpdateResult) {
		LaunchGuiWithParam(CMD_ARG_NOT_YOURS);
		return;
	}

	if (IpUpdateBadAuth == g_prevIpUpdateResult) {
		LaunchGuiWithParam(CMD_ARG_BAD_AUTH);
		return;
	}
}

static void LogIpUpdate(char *resp)
{
	slog("sent ip update for user '");
	assert(g_pref_user_name);
	if (g_pref_user_name)
		slog(g_pref_user_name);
	slog("', response: '");
	if (resp)
		slog(resp);
	slog("' ");
	slog(" url: ");
	const char *urlTxt = GetIpUpdateUrl();
	if (urlTxt)
		slog(urlTxt);
	free((void*)urlTxt);
	slog(" host: ");
	slog(GetIpUpdateHost());
	slog("\n");
}

static void SendIPUpdateFromService()
{
	g_lastIpUpdateTimeInMs = GetTickCount();
	if (g_paused)
		return;

	char *resp = SendIpUpdate();
	LogIpUpdate(resp);
	HandleIPUpdateResponse(resp);
	free(resp);
}

static void SendPeriodicIPUpdate()
{
	if (ShouldSendPeriodicUpdate())
		SendIPUpdateFromService();
}

static bool ShouldCheckForSoftwareUpgrade()
{
	ULONGLONG currTimeInMs = GetTickCount();

	// the time wraps-around every 49.7 days.
	if (currTimeInMs < g_lastSoftwareUpgradeTimeInMs) {
		// going backwards in time - it must be wrap around
		g_lastSoftwareUpgradeTimeInMs = GetTickCount();
		slog("Detected GetTickCount() wrap-around\n");
	}

	ULONGLONG nextUpdateTimeInMs = g_lastSoftwareUpgradeTimeInMs + ONE_DAY_IN_MS;
	if (currTimeInMs > nextUpdateTimeInMs)
		return true;
	return false;
}

static void CheckForSoftwareUpgrade(bool simulateUpgrade=false)
{
	g_lastSoftwareUpgradeTimeInMs = GetTickCount();
	slog("CheckForSoftwareUpgrade()\n");

	TCHAR *url2 = NULL;
	const TCHAR *version = PROGRAM_VERSION;
	if (simulateUpgrade)
		version = PROGRAM_VERSION_SIMULATE_UPGRADE;
	char *url = GetUpdateUrl(version, UpdateCheckVersionCheck);
	if (!url)
		return;
	free(url);

	// found an update - launch the UI asking it to check for an upgrade
	// because a service can't show any UI

	// TODO: make it less obnoxious if a user didn't choose to
	// upgrade
	slog("launching ui with /upgradecheck\n");
	LaunchGuiWithParam(CMD_ARG_UPGRADE_CHECK);
}

static void PeriodicCheckForSoftwareUpgrade()
{
	if (ShouldCheckForSoftwareUpgrade())
		CheckForSoftwareUpgrade();
}

class ServiceDnsEventsObserver : public DnsEventsObserver
{
	IP4_ADDRESS m_prevIP;
public:
	ServiceDnsEventsObserver()
	{
		m_prevIP = IP_UNKNOWN;
	}

	virtual void MyIpChanged(IP4_ADDRESS myNewIP)
	{
		if (myNewIP == m_prevIP)
			return;

		bool wasPrevOk = RealIpAddress(m_prevIP);
		m_prevIP = myNewIP;
		if (RealIpAddress(myNewIP))
			SendIPUpdateFromService();

		// notify the user via launching UI if we're not using
		// OpenDNS servers
		if (myNewIP != IP_NOT_USING_OPENDNS)
			return;

		// if it was bad before, don't bother
		if (!wasPrevOk)
			return;
		LaunchGuiWithParam(CMD_ARG_NOT_USING_UPENDNS);
	}
};

#if 0
static CString LogUniqueFileName(const TCHAR *dir)
{
	CString fileNameBase = dir;
	const TCHAR *fnb = fileNameBase;
	int i = 0;
	CString fileName;
	while (i <= 999) {
		fileName.Format(_T("%s\\service-log-%03d.txt"), fnb, i);
		if (!FileOrDirExists(fileName)) {
			return fileName;
		}
		i++;
	}
	// if all names taken, overwrite the first
	fileName.Format(_T("%s\\log-%3d.txt"), fnb, 0);
	return fileName;
}
#endif

static CString LogFileName(const TCHAR *dir)
{
	CString fileName = dir;
	fileName += "\\service-log.txt";
	return fileName;
}

static void usage()
{
	fprintf(stdout, "usage: OpenDNSDynamicIpService.exe [install|remove]\n");
}

static void log(char *s)
{
	if (g_debugMode)
		fprintf(stderr, "%s", s);
}

static int remove_service()
{
	slog("remove_service()\n");

	ServiceManager sm;
	int err = sm.Open(SC_MANAGER_CREATE_SERVICE, SERVICE_STOP | DELETE);
	if (err != ErrNoError)
		return err;

	err = sm.Stop();
	if (err != ErrNoError)
		return err;

	BOOL ok = sm.Delete();
	if (!ok)
		return ErrDeleteServiceFail;
	return ErrNoError;
}

static int install_service()
{
	int err = ErrNoError;
	SC_HANDLE manager = NULL, service = NULL;
	TCHAR exePath[MAX_PATH];

	slog("install_service()\n");
	DWORD res = GetModuleFileName(NULL, exePath, MAX_PATH);
	if (0 == res)
		return ErrGetModuleFileNameFail;

	manager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (NULL == manager) {
		err = ErrOpenSCManagerFail;
		goto Exit;
	}

	service = CreateService(manager, OPENDNS_UI_UPDATER_SERVICE_NAME, NULL,
		SERVICE_CHANGE_CONFIG | SERVICE_START,
		SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
		SERVICE_ERROR_NORMAL, exePath, NULL, NULL,
		_T("Tcpip\0"), NULL, NULL);

	if (NULL == service) {
		err = ErrCreateServiceFail;
		goto Exit;
	}

	BOOL ok = ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &description);
	if (!ok) {
		err = ErrChangeServiceConfig2Fail;
		goto Exit;
	}

	ok = StartService(service, 0, NULL);
	if (!ok) {
		err = ErrStartServiceFail;
		goto Exit;
	}

Exit:
	CloseManagerService(&manager, &service);
	return err;
}

#define ONE_SECOND_IN_MS 1000
#define ONE_MINUTE_IN_MS 60*1000

static void StopIfQPressed()
{
	int keyPressed = _kbhit();
	if (!keyPressed)
		return;
	int k = _getch();
	if ('q' != k)
		return;

	log("Quitting since pressed 'q'\n");
	g_forceStop = true;
}

static void LogWhyCantSendIPUpdates()
{
	slog("Can't send IP updates becasue: ");
	if (strempty(g_pref_user_name)) {
		slog("user name is empty\n");
		return;
	}
	if (strempty(g_pref_token)) {
		slog("token is empty\n");
		return;
	}
	if (!streq(UNS_OK, g_pref_user_networks_state)) {
		slog("state is not 'unsok' but '");
		slog(g_pref_user_networks_state);
		slog("'\n");
		return;
	}
	slog("wait, I can send IP updates\n");
}

#ifdef LOG_ALIVE
#define ALIVE_PERIOD 10
#endif

static void RunUntilAskedToQuit(HANDLE stopHandle)
{
	ServiceDnsEventsObserver dnsObserver;
	DnsCheckThread *dnsCheckThread = new DnsCheckThread(&dnsObserver);
	bool stop = false;
	DWORD res;
#ifdef LOG_ALIVE
	int aliveCount = ALIVE_PERIOD;
#endif
	while (!stop && !g_forceStop) {
		res = WaitForSingleObject(stopHandle, ONE_MINUTE_IN_MS);
		if (WAIT_TIMEOUT != res)
			stop = true;
		if (g_debugMode)
			StopIfQPressed();
		SendPeriodicIPUpdate();
		PeriodicCheckForSoftwareUpgrade();
#ifdef LOG_ALIVE
		--aliveCount;
		if (0 == aliveCount) {
			aliveCount = ALIVE_PERIOD;
			slog("still alive\n");
		}
#endif
	}
	dnsCheckThread->Stop(true);
}

static void run_in_debug_mode()
{
	g_debugMode = true;
	slog("Running in debug mode\n");
	log("Running in debug mode. Press 'q' to quit\n");
	RunUntilAskedToQuit(CreateEvent(NULL, TRUE, FALSE, 0)); 
}

static void set_service_status(DWORD state)
{
	SERVICE_STATUS status;

	status.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
		status.dwCurrentState = state;
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP,
		status.dwWin32ExitCode = NO_ERROR,
		status.dwCheckPoint = 0;
	status.dwWaitHint = 0;
	SetServiceStatus(g_serviceHandle, &status);
}

static void WINAPI service_handler(unsigned long control)
{
	if (SERVICE_CONTROL_STOP == control) {
		slog("service_handler() control=SERVICE_CONTROL_STOP\n");
		set_service_status(SERVICE_STOP_PENDING);
		SetEvent(g_serviceStopEvent);
	} else if (SERVICE_CONTROL_PAUSE == control) {
		slog("service_handler() control=SERVICE_CONTROL_PAUSE\n");
		g_paused = true;
	} else if (SERVICE_CONTROL_CONTINUE == control) {
		slog("service_handler() control=SERVICE_CONTROL_CONTINUE\n");
		g_paused = false;
	}
}

static void WINAPI service_main(unsigned long /*argc*/, TCHAR ** /*argv*/)
{
	slog("service_main()\n");
	g_serviceHandle = RegisterServiceCtrlHandler(OPENDNS_UI_UPDATER_SERVICE_NAME, service_handler);
	if (!g_serviceHandle) {
		slog("service_main(): RegisterServiceCtrlHandler() failed\n");
		return;
	}

	set_service_status(SERVICE_START_PENDING);
	g_serviceStopEvent = CreateEvent(NULL, TRUE, FALSE, 0);
	if (NULL == g_serviceStopEvent) {
		slog("service_main(): CreateEvent() failed\n");
		return;
	}

	set_service_status(SERVICE_RUNNING);
	g_lastIpUpdateTimeInMs = GetTickCount();
	RunUntilAskedToQuit(g_serviceStopEvent);
	set_service_status(SERVICE_STOPPED);
}

static int start_service()
{
	const SERVICE_TABLE_ENTRY service_table[] = {
		{ OPENDNS_UI_UPDATER_SERVICE_NAME,  service_main},
		{ NULL, NULL }
	};

	slog("start_service()\n");
	BOOL ok = StartServiceCtrlDispatcher(service_table);
	if (!ok) {
		slog("start_service(), StartServiceCtrlDispatcher() failed\n");
		return ErrStartServiceCtrlDispatcherFail;
	}

	return ErrNoError;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int err = ErrNoError;

	if (argc > 2) {
		usage();
		return 1;
	}

	TCHAR *commonDir = ReadRegStr(HKEY_LOCAL_MACHINE, COMMON_DATA_DIR_REG_KEY_PATH, COMMON_DATA_DIR_REG_KEY_NAME);
	if (!commonDir) {
		// we really need that
		return 1;
	}
	InstallCrashHandler(commonDir, SERVICE_EXE_NAME_WITHOUT_EXE);

	PreferencesLoadFromDir(commonDir);

	SLogInit(LogFileName(commonDir));
	slog("-------- starting\n");

	if (1 == argc) {
		// if we don't have unique_id, don't even start. It should have been generated
		// by UI at installation time so something must have gone wrong if we don't
		// have it
		assert(g_pref_unique_id);
		if (!g_pref_unique_id) {
			slog("no user name, exiting\n");
			goto Exit;
		}

		if (!g_pref_user_name) {
			slog("no user name, exiting\n");
			goto Exit;
		}
		slogfmt("user_name: %s\n", g_pref_user_name);

		if (!g_pref_token) {
			slog("no token, exiting\n");
			goto Exit;
		}

		if (!CanSendIPUpdates()) {
			LogWhyCantSendIPUpdates();
			slog("Can't send ip updates, exiting\n");
			goto Exit;
		}

		err = start_service();
	} else {
		TCHAR* cmd = argv[1];
		if (tstreq(cmd, _T("install")))
			err = install_service();
		else if (tstreq(cmd, _T("remove")))
			err = remove_service();
		else if (tstreq(cmd, _T("debug")))
			run_in_debug_mode();
		else if (tstreq(cmd, _T("unittests")))
			err = run_unit_tests();
		else if (tstreq(cmd, _T("ut")))
			err = run_unit_tests();
	}

Exit:
	PreferencesFree();

	slog("finished\n");
	SLogStop();
	return err;
}
