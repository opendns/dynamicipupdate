// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UPDATER_THREAD_H__
#define UPDATER_THREAD_H__

/* This thread does 2 things, at 1 min intervals:
 - resolves myip.opendns.com to get current ip address of this computer
 - detects if we're using OpenDNS dns servers: if myip.opendns.com returns
   NX record, we're *not* using OpenDNS dns servers
*/
#include "WTLThread.h"
#include "DnsQuery.h"
#include "MiscUtil.h"
#include "SimpleLog.h"
#include "SendIPUpdate.h"

extern bool g_simulate_upgrade;

static const ULONGLONG THREE_HRS_IN_MS =  3*60*60*1000;
static const ULONGLONG ONE_DAY_IN_MS   = 24*60*60*1000;

class UpdaterThreadObserver
{
public:
	virtual void OnIpCheckResult(IP4_ADDRESS myNewIP) = 0;
	virtual void OnIpUpdateResult(char *ipUpdateResult) = 0;
	virtual void OnNewVersionAvailable(TCHAR *setupFilePath) = 0;
};

// special values for IP4_ADDRESS
enum {
	// we haven't had a chance to dns resolve "myip.opends.com"
	IP_UNKNOWN  = 0,
	// we resolved "myip.opendns.com" but got NX record. That means
	// we're not using OpenDNS dns server
	IP_NOT_USING_OPENDNS = 1,
	// we try to resolve "myip.opendns.com" but got generic dns error
	// this usually indicates network connection problems
	IP_DNS_RESOLVE_ERROR = 2
};

static inline bool RealIpAddress(IP4_ADDRESS ipAddr)
{
	if (ipAddr > IP_DNS_RESOLVE_ERROR)
		return true;
	return false;
}

class UpdaterThread : public CThread
{
public:
	UpdaterThreadObserver *	m_updaterObserver;
	HANDLE				m_event;
	bool				m_stop;
	ULONGLONG			m_lastIpUpdateTimeInMs;
	ULONGLONG			m_lastSoftwareUpgradeTimeInMs;
	bool				m_forceNextIpUpdate;
	bool				m_forceNextSoftwareUpdate;

	UpdaterThread(UpdaterThreadObserver *updaterObserver) :
		m_updaterObserver(updaterObserver),
		m_stop(false)
	{
		m_lastIpUpdateTimeInMs = 0;
		m_lastSoftwareUpgradeTimeInMs = 0;
		m_forceNextIpUpdate = false;
		m_forceNextSoftwareUpdate = false;

		// we shouldn't need more stack than 64k
		// TODO: this doesn't seem to change stack size from default 1MB
		// as tested by StackHungry() function. No idea why.
		DWORD stackSize = 64*1024;
		m_hThread = CreateThread(NULL, stackSize, (LPTHREAD_START_ROUTINE) _ThreadProcThunk<UpdaterThread>,
			this, 0, &m_dwThreadId);
	}

	IP4_ADDRESS GetMyIp()
	{
		IP4_ADDRESS myIp;
		int res = dns_query("myip.opendns.com", &myIp);
		if (DNS_QUERY_OK == res)
			return myIp;
		if (DNS_QUERY_NO_A_RECORD == res)
			return IP_NOT_USING_OPENDNS;
		assert(DNS_QUERY_ERROR == res);
		return IP_DNS_RESOLVE_ERROR;
	}

	void UpdateCurrentIp(IP4_ADDRESS myIp)
	{
		m_updaterObserver->OnIpCheckResult(myIp);
	}

	int MinutesSinceLastUpdate() {
		if (!CanSendIPUpdates())
			return -1;

		ULONGLONG currTimeInMs = GetTickCount();

		// the time wraps-around every 49.7 days.
		if (currTimeInMs < m_lastIpUpdateTimeInMs) {
			// going backwards in time - it must be wrap around
			m_lastIpUpdateTimeInMs = currTimeInMs;
			slog("Detected GetTickCount() wrap-around\n");
		}

		ULONGLONG timePassedInMs =  currTimeInMs - m_lastIpUpdateTimeInMs;
		ULONGLONG timePassedInMin = timePassedInMs / (60 * 1000);
		return (int)timePassedInMin;
	}

	bool ShouldSendPeriodicUpdate()
	{
		if (!CanSendIPUpdates())
			return false;

		if (m_forceNextIpUpdate) {
			m_forceNextIpUpdate = false;
			return true;
		}

		ULONGLONG currTimeInMs = GetTickCount();

		// the time wraps-around every 49.7 days.
		if (currTimeInMs < m_lastIpUpdateTimeInMs) {
			// going backwards in time - it must be wrap around
			m_lastIpUpdateTimeInMs = GetTickCount();
			slog("Detected GetTickCount() wrap-around\n");
		}

		ULONGLONG nextUpdateTimeInMs = m_lastIpUpdateTimeInMs + THREE_HRS_IN_MS;
		if (currTimeInMs > nextUpdateTimeInMs)
			return true;
		return false;
	}

	void SendPeriodicUpdate()
	{
		m_lastIpUpdateTimeInMs = GetTickCount();
		char *resp = ::SendIpUpdate();
		if (NULL == resp)
			return;
		m_updaterObserver->OnIpUpdateResult(resp);
		free(resp);
	}

	bool ShouldCheckForSoftwareUpgrade()
	{
		ULONGLONG currTimeInMs = GetTickCount();

		if (m_forceNextSoftwareUpdate) {
			m_forceNextSoftwareUpdate = false;
			return true;
		}

		// the time wraps-around every 49.7 days.
		if (currTimeInMs < m_lastSoftwareUpgradeTimeInMs) {
			// going backwards in time - it must be wrap around
			m_lastSoftwareUpgradeTimeInMs = GetTickCount();
			slog("Detected GetTickCount() wrap-around\n");
		}

		ULONGLONG nextUpdateTimeInMs = m_lastSoftwareUpgradeTimeInMs + ONE_DAY_IN_MS;
		if (currTimeInMs > nextUpdateTimeInMs)
			return true;
		return false;
	}

	void CheckForSoftwareUpgrade(bool simulateUpgrade=false)
	{
		m_lastSoftwareUpgradeTimeInMs = GetTickCount();
		slog("CheckForSoftwareUpgrade()\n");

		const TCHAR *version = PROGRAM_VERSION;
		if (simulateUpgrade)
			version = PROGRAM_VERSION_SIMULATE_UPGRADE;
		char *url = GetUpdateUrl(version, UpdateCheckVersionCheck);
		if (!url) {
			DeleteOldInstallers();
			return;
		}

		TCHAR *filePath = DownloadUpdateIfNotDownloaded(url);
		free(url);
		if (filePath)
			m_updaterObserver->OnNewVersionAvailable(filePath);
	}

#if 0
	// a function that uses lots of stack space, to test
	// if our stack limits take place. Calculates a value
	// just so that it doesn't get optimized out in release build
	#define STACK_SIZE 768*1024
	int StackHungry()
	{
		static int last_total;
		char sh[STACK_SIZE];
		int total = 0;
		for (int i=0; i<STACK_SIZE; i++)
		{
			sh[i] = i % 0xff;
			total += sh[i];
		}
		last_total = total;
		return total;
	}
#endif

	void ForceSendIpUpdate()
	{
		m_lastIpUpdateTimeInMs = GetTickCount();
		m_forceNextIpUpdate = true;
		SetEvent(m_event);
	}

	void ForceSoftwareUpdateCheck()
	{
		m_forceNextSoftwareUpdate = true;
		SetEvent(m_event);
	}

	void Stop(bool wait=false)
	{
		m_stop = true;
		SetEvent(m_event);
		if (wait)
			Join();
	}

	DWORD Run()
	{
		const DWORD ONE_MINUTE_IN_MS = 1000*60;

		m_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (NULL == m_event)
			return 1;

		while (!m_stop)
		{
			IP4_ADDRESS myIp = GetMyIp();
			UpdateCurrentIp(myIp);

			if (ShouldSendPeriodicUpdate())
				SendPeriodicUpdate();

			if (ShouldCheckForSoftwareUpgrade())
				CheckForSoftwareUpgrade(g_simulate_upgrade);

			// int k = StackHungry();
			WaitForSingleObject(m_event, ONE_MINUTE_IN_MS);
		}
		CloseHandle(m_event);
		return 0;
	}
};

#endif
