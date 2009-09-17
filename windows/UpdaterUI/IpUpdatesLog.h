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

CString IpUpdatesLogFileName();
void LoadIpUpdatesHistory();
void LogIpUpdate(const char *ipAddress);
void FreeIpUpdatesHistory();
char *IpUpdatesAsText(IpUpdate *head, size_t *sizeOut);

#endif

