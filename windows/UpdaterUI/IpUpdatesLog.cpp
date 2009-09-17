#include "stdafx.h"

#include "IpUpdatesLog.h"
#include "MiscUtil.h"
#include "StrUtil.h"

/*
TODO:
* redo parsing so that it parses from the beginning of the string
* finish writing out the truncated log
*/

// Linked list of ip updates. Newest are at the front.
IpUpdate *				g_ipUpdates = NULL;

// name of the file where we persist ip updates history
static const TCHAR *	gIpUpdatesLogFileName;
static FILE *			gIpUpdatesLogFile;

static void FreeIpUpdate(IpUpdate *ipUpdate)
{
	assert(ipUpdate);
	if (!ipUpdate)
		return;
	free(ipUpdate->ipAddress);
	free(ipUpdate->time);
	free(ipUpdate);
}

static void FreeIpUpdatesFromElement(IpUpdate *curr)
{
	while (curr) {
		IpUpdate *next = curr->next;
		FreeIpUpdate(curr);
		curr = next;
	}
}

static void InsertIpUpdate(const char *ipAddress, const char *time)
{
	assert(ipAddress);
	assert(time);
	if (!ipAddress || !time)
		return;
	IpUpdate *ipUpdate = SA(IpUpdate);
	if (!ipUpdate)
		return;
	ipUpdate->ipAddress = strdup(ipAddress);
	ipUpdate->time = strdup(time);
	if (!ipUpdate->ipAddress || !ipUpdate->time) {
		FreeIpUpdate(ipUpdate);
	}
}

static inline bool is_newline_char(char c)
{
	return (c == '\r') || (c == '\n');
}

// at this point <*dataEnd> points at the end of the log line, which is in
// format:
// $ipaddr $time\r\n
static bool ExtractIpAddrAndTime(char **dataEndInOut, uint64_t *dataSizeInOut, char **ipAddrOut, char **timeOut)
{
	char *dataEnd = *dataEndInOut;
	char *ipAddressStart = dataEnd;
	char *timeStart = NULL;
	uint64_t dataSizeLeft = *dataSizeInOut;

	// skip "\r\n" at the end and replace them with 0, so that $time will be
	// a zero-terminated string
	while (dataSizeLeft > 0) {
		if (!is_newline_char(*ipAddressStart))
			break;

		dataSizeLeft--;
		*ipAddressStart-- = 0;
	}

	// find the end of previous line, which is the beginning of $ipaddr
	while (dataSizeLeft > 0) {
		if (is_newline_char(*ipAddressStart)) {
			// we went back one too far, so correct that
			++ipAddressStart;
			++dataSizeLeft;
			break;
		}
		dataSizeLeft--;
		ipAddressStart--;
	}

	// the first space separates $ipaddr from $time
	timeStart = ipAddressStart;
	while (*timeStart && (*timeStart != ' ')) {
		++timeStart;
	}

	if (*timeStart != ' ') {
		// didn't find a space - something's wrong
		return false;
	}
	// change space to 0 to make ipaddress zero-terminated string
	*timeStart++ = 0;

	*ipAddrOut = ipAddressStart;
	*timeOut = timeStart;
	*dataEndInOut = ipAddressStart;
	*dataSizeInOut = dataSizeLeft;
	return true;
}

// load up to IP_UPDATES_HISTORY_MAX latest entries from ip updates log
// return true if there is more than IP_UPDATES_HISTORY_MAX entries in the log
// 
static int ParseIpLogHistory(char *data, uint64_t dataSize)
{
	char *ipAddr = NULL;
	char *time = NULL;
	int entries = 0;
	char *currentDataEnd = data + dataSize - 1;
	uint64_t dataSizeLeft = dataSize;
	while (dataSizeLeft != 0) {
		ExtractIpAddrAndTime(&currentDataEnd, &dataSizeLeft, &ipAddr, &time);
		InsertIpUpdate(ipAddr, time);
		++entries;
	}
	return entries;
}

static void LogIpUpdateEntry(FILE *log, const char *ipAddress, const char *time)
{
	assert(log && ipAddress && time);
	if (!log || !ipAddress || !time)
		return;

	size_t slen = strlen(ipAddress);
	fwrite(ipAddress, slen, 1, log);
	fwrite(" ", 1, 1, log);

	slen = strlen(time);
	fwrite(time, slen, 1, log);
	fwrite("\r\n", 2, 1, log);
	fflush(log);
}

// overwrite the history log file with the current history in g_ipUpdates
// The assumption is that we've limited 
static void WriteIpLogHistory(const TCHAR *logFileName)
{
	FILE *log = _tfopen(logFileName, _T("wb"));

	fclose(log);
}

static void RemoveLogEntries(int max)
{
	IpUpdate *curr = g_ipUpdates;
	IpUpdate **currPtr = NULL;
	while (curr && max > 0) {
		currPtr = &curr->next;
		curr = curr->next;
		--max;
	}
	if (!curr || !currPtr)
		return;
	FreeIpUpdatesFromElement(*currPtr);
	*currPtr = NULL;
}

static void LoadAndParseHistory(const TCHAR *logFileName)
{
	uint64_t dataSize;
	char *data = FileReadAll(logFileName, &dataSize);
	if (!data)
		return;
	int entries = ParseIpLogHistory(data, dataSize);
	if (entries > IP_UPDATES_HISTORY_MAX) {
		RemoveLogEntries(IP_UPDATES_HISTORY_MAX);
		WriteIpLogHistory(logFileName);
	}
	free(data);
}

void LoadIpUpdatesHistory(const TCHAR *logFileName)
{
	assert(!gIpUpdatesLogFileName);
	assert(!gIpUpdatesLogFile);

	LoadAndParseHistory(logFileName);

	gIpUpdatesLogFileName = tstrdup(logFileName);
	gIpUpdatesLogFile = _tfopen(logFileName, _T("ab"));
}

void LogIpUpdate(const char *ipAddress)
{
	char timeBuf[256];
	__time64_t ltime;
	struct tm *today;
	today = _localtime64(&ltime);
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", today);
	assert(gIpUpdatesLogFile);
	LogIpUpdateEntry(gIpUpdatesLogFile, ipAddress, timeBuf);
}

static void CloseIpUpdatesLog()
{
	TStrFree(&gIpUpdatesLogFileName);
	if (gIpUpdatesLogFile) {
		fclose(gIpUpdatesLogFile);
		gIpUpdatesLogFile = NULL;
	}
}

void FreeIpUpdatesHistory()
{
	CloseIpUpdatesLog();
	FreeIpUpdatesFromElement(g_ipUpdates);
	g_ipUpdates = NULL;
}

