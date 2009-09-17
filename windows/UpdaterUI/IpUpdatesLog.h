#ifndef IP_UPDATES_LOG_H__
#define IP_UPDATES_LOG_H__

// Maximum number of ip updates we remember. The idea being that it doesn't make
// sense to show the user more than that. We purge the oldest updates.
#define IP_UPDATES_HISTORY_MAX 100

typedef struct IpUpdate {
	struct IpUpdate *	next;
	char *				time;
	char *				ipAddress;
} IpUpdate;

extern IpUpdate *g_ipUpdates;

void LoadIpUpdatesHistory(const TCHAR *logFileName);
void LogIpUpdate(const char *ipAddress);
void FreeIpUpdatesHistory();

#endif

