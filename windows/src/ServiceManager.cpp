#include "stdafx.h"

#include "Errors.h"
#include "ServiceManager.h"
#include "StrUtil.h"
#include "MiscUtil.h"

void CloseManagerService(SC_HANDLE *manager, SC_HANDLE *service)
{
	if (*manager) {
		CloseServiceHandle(*manager);
		*manager = NULL;
	}
	if (*service) {
		CloseServiceHandle(*service);
		*service = NULL;
	}
}

ServiceStatus GetUpdaterServiceStatus()
{
	ServiceManager sc;
	int err = sc.Open(SC_MANAGER_CONNECT, SERVICE_QUERY_STATUS);
	if (err == ErrOpenServiceFail)
		return ServiceStatusNotInstalled;
	if (err != ErrNoError)
		return ServiceStatusUnknown;
	DWORD status;
	bool ok = sc.GetStatus(status);
	if (!ok)
		return ServiceStatusUnknown;

	static const unsigned char statusMap[] = {
		SERVICE_STOPPED, ServiceStatusNotRunning,
		SERVICE_START_PENDING, ServiceStatusRunning,
		SERVICE_STOP_PENDING, ServiceStatusStopPending,
		SERVICE_RUNNING, ServiceStatusRunning,
		SERVICE_CONTINUE_PENDING, ServiceStatusRunning,
		SERVICE_PAUSE_PENDING, ServiceStatusNotRunning,
		SERVICE_PAUSED, ServiceStatusNotRunning
	};
	static const int els = sizeof(statusMap) / (sizeof(statusMap[0]) * 2);
	for (int i=0; i<els; i++) {
		if (statusMap[i*2] == (unsigned char)status)
			return (ServiceStatus)statusMap[(i*2)+1];
	}
	assert(0);
	return ServiceStatusUnknown;
}

TCHAR* FindServiceExePath()
{
	// first look for SERVICE_EXE_NAME in the same directory as ourselves
	// (the case of installed program)
	TCHAR buf[MAX_PATH + 1];
	DWORD len = GetModuleFileName(NULL, buf, dimof(buf));
	if ((len == dimof(buf)) && (ERROR_INSUFFICIENT_BUFFER == GetLastError())) {
		assert(0);
		return NULL;
	}

	BOOL ok = PathStripLastComponentInPlace(buf);
	assert(ok);
	if (!ok)
		return NULL;

	TCHAR *fullName = TStrCat(buf, PATH_SEP_STR, SERVICE_EXE_NAME);
	BOOL exists = FileOrDirExists(fullName);
	if (exists)
		return fullName;
	free(fullName);

	// now try ../../Service/Debug/${SERVICE_EXE_NAME}
	// (the case for running develepement build)
	ok = PathStripLastComponentInPlace(buf);
	if (!ok)
		return NULL;
	ok = PathStripLastComponentInPlace(buf);
	if (!ok)
		return NULL;
#ifdef DEBUG
	fullName = TStrCat(buf, _T("\\Service\\Debug\\"), SERVICE_EXE_NAME);
#else
	fullName = TStrCat(buf, _T("\\Service\\Release\\"), SERVICE_EXE_NAME);
#endif
	if (!fullName)
		return NULL;
	exists = FileOrDirExists(fullName);
	if (exists)
		return fullName;
	free(fullName);
	return NULL;
}

// TODO: not the best place for it. Move to MiscUtil.cpp?
TCHAR* FindGuiExePath()
{
	// first look for GUI_EXE_NAME in the same directory as ourselves
	// (the case of installed program)
	TCHAR buf[MAX_PATH + 1];
	DWORD len = GetModuleFileName(NULL, buf, dimof(buf));
	if ((len == dimof(buf)) && (ERROR_INSUFFICIENT_BUFFER == GetLastError())) {
		assert(0);
		return NULL;
	}

	BOOL ok = PathStripLastComponentInPlace(buf);
	assert(ok);
	if (!ok)
		return NULL;

	TCHAR *fullName = TStrCat(buf, PATH_SEP_STR, GUI_EXE_NAME);
	BOOL exists = FileOrDirExists(fullName);
	if (exists)
		return fullName;
	free(fullName);

	// now try ..\..\UpdaterUI\Debug\${GUI_EXE_NAME}
	// (the case for running develepement build)
	ok = PathStripLastComponentInPlace(buf);
	if (!ok)
		return NULL;
	ok = PathStripLastComponentInPlace(buf);
	if (!ok)
		return NULL;
#ifdef DEBUG
	fullName = TStrCat(buf, _T("\\UpdaterUI\\Debug\\"), GUI_EXE_NAME);
#else
	fullName = TStrCat(buf, _T("\\UpdaterUI\\Release\\"), GUI_EXE_NAME);
#endif
	if (!fullName)
		return NULL;
	exists = FileOrDirExists(fullName);
	if (exists)
		return fullName;
	free(fullName);
	return NULL;
}

void InstallService()
{
	TCHAR* serviceExePath = FindServiceExePath();
	if (!serviceExePath) {
		MessageBox(NULL, _T("Error: couldn't install the service."), _T("Error"), MB_OK);
		return;
	}
	const bool isHidden = true;
	ExecWithParams(serviceExePath, _T("install"), isHidden);
	free(serviceExePath);
}

void RemoveService()
{
	TCHAR* serviceExePath = FindServiceExePath();
	if (!serviceExePath) {
		MessageBox(NULL, _T("Error: couldn't remove the service."), _T("Error"), MB_OK);
		return;
	}
	const bool isHidden = true;
	ExecWithParams(serviceExePath, _T("remove"), isHidden);
	free(serviceExePath);
}

void StartServiceIfNotStarted()
{
	ServiceStatus status = GetUpdaterServiceStatus();
	if (ServiceStatusNotInstalled == status)
		InstallService();
	status = GetUpdaterServiceStatus();
	if (ServiceStatusRunning != status)
		StartUpdateService();
}

void WaitForServiceStopped()
{
	int tries = 5;
	while (tries > 0) {
		ServiceStatus status = GetUpdaterServiceStatus();
		if ((ServiceStatusRunning == status) || (ServiceStatusStopPending == status)) {
			--tries;
			Sleep(100); // 100ms = 1/10 of a second
			continue;
		}
		return;
	}
}

void StopServiceIfNotStopped()
{
	ServiceStatus status = GetUpdaterServiceStatus();
	if (ServiceStatusRunning == status) {
		StopUpdateService();
		WaitForServiceStopped();
	}
}

void StopAndStartServiceIfNeeded()
{
	StopServiceIfNotStopped();
	if (CanSendIPUpdates())
		StartServiceIfNotStarted();
}
