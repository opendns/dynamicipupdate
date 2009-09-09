// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICE_MANAGER_H__
#define SERVICE_MANAGER_H__

#define OPENDNS_UI_UPDATER_SERVICE_NAME _T("OpenDNS Dynamic IP")

enum ServiceStatus {
	ServiceStatusUnknown,
	ServiceStatusNotInstalled,
	ServiceStatusNotRunning,
	ServiceStatusStopPending,
	ServiceStatusRunning
};

void CloseManagerService(SC_HANDLE *manager, SC_HANDLE *service);

class ServiceManager
{
public:
	SC_HANDLE manager;
	SC_HANDLE service;

	ServiceManager() :
	    manager(NULL)
	  , service(NULL)
		{}

	~ServiceManager() {
		CloseManagerService(&manager, &service);
	}

	int Open(DWORD managerRights, DWORD serviceRights) {
		manager = OpenSCManager(NULL, NULL, managerRights);
		if (NULL == manager)
			return ErrOpenSCManagerFail;

		service = OpenService(manager, OPENDNS_UI_UPDATER_SERVICE_NAME, serviceRights);
		if (NULL == service)
			return ErrOpenServiceFail;
		return ErrNoError;
	}

	int OpenForStop() {
		return Open(SC_MANAGER_CONNECT,  SERVICE_STOP);
	}

	int OpenForStart() {
		return Open(SC_MANAGER_CONNECT,  SERVICE_START);
	}

	int Stop() {
		if (!service || !manager) {
			int err = OpenForStop();
			if (ErrNoError != err)
				return err;
		}

		assert(service && manager);
		SERVICE_STATUS status;
		BOOL ok = ControlService(service, SERVICE_CONTROL_STOP, &status);
		if (ok)
			return ErrNoError;
		DWORD lastError = GetLastError();
		if (ERROR_SERVICE_NOT_ACTIVE == lastError)
			return ErrNoError;
		return ErrControlServiceStopFail;
	}

	int Start() {
		if (!service || !manager) {
			int err = OpenForStart();
			if (ErrNoError != err)
				return err;
		}
		BOOL ok = StartService(service, 0, NULL);
		if (ok)
			return ErrNoError;
		return ErrStartServiceFail;
	}

	BOOL Delete() {
		return DeleteService(service);
	}

	bool GetStatus(DWORD& state) {
		SERVICE_STATUS status;
		BOOL ok = QueryServiceStatus(service, &status);
		if (!ok)
			return false;
		state = status.dwCurrentState;
		return true;
	}
};

static inline int StartUpdateService()
{
	ServiceManager sc;
	return sc.Start();
}

static inline int StopUpdateService()
{
	ServiceManager sc;
	return sc.Stop();
}

ServiceStatus GetUpdaterServiceStatus();
TCHAR* FindServiceExePath();
TCHAR* FindGuiExePath();
void InstallService();
void RemoveService();
void StartServiceIfNotStarted();
void StopServiceIfNotStopped();
void StopAndStartServiceIfNeeded();

#endif
