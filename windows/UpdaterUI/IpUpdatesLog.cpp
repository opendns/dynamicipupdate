#include "stdafx.h"

#include "IpUpdatesLog.h"
#include "MiscUtil.h"
#include "StrUtil.h"

// When we reach that many updates, we'll trim the ip updates log
#define IP_UPDATES_PURGE_LIMIT 1000

// After we reach IP_UPDATES_PURGE_LIMIT, we'll only write out
// IP_UPDATES_AFTER_PURGE_SIZE items, so that we don't have to purge every
// time once we reach purge limit
#define IP_UPDATES_SIZE_AFTER_PURGE 500

// Linked list of ip updates. Newest are at the front.
IpUpdate *				g_ipUpdates = NULL;

// name of the file where we persist ip updates history
static const TCHAR *	gIpUpdatesLogFileName;
static FILE *			gIpUpdatesLogFile;

CString IpUpdatesLogFileName()
{
	CString fileName = AppDataDir();
	fileName += _T("\\ipupdateslog.txt");
	return fileName;
}

static void str_append(char **dstInOut, char *s)
{
	char *dst = *dstInOut;
	size_t len = strlen(s);
	memcpy(dst, s, len);
	dst += len;
	*dstInOut = dst;
}

char *IpUpdatesAsText(IpUpdate *head, size_t *sizeOut)
{
	IpUpdate *curr = head;
	char *s = NULL;
	size_t sizeNeeded = 0;

	while (curr) {
		if (curr->ipAddress && curr->time) {
			sizeNeeded += strlen(curr->ipAddress);
			sizeNeeded += strlen(curr->time);
			sizeNeeded += 3; // space + '\r\n'
		}
		curr = curr->next;
	}

	s = (char*)malloc(sizeNeeded+1); // +1 for terminating zero

	char *tmp = s;
	curr = head;
	while (curr) {
		if (curr->ipAddress && curr->time) {
			str_append(&tmp, curr->ipAddress);
			str_append(&tmp, " ");
			str_append(&tmp, curr->time);
			str_append(&tmp, "\r\n");
		}
		curr = curr->next;
	}
	*tmp = 0;

	*sizeOut = sizeNeeded + 1;
	return s;
}

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
		return;
	}

	ipUpdate->next = g_ipUpdates;
	g_ipUpdates = ipUpdate;
}

static inline bool is_newline_char(char c)
{
	return (c == '\r') || (c == '\n');
}

// at this point <*dataStartInOut> points at the beginning of the log file,
// which consists of lines in format:
// $ipaddr $time\r\n
static bool ExtractIpAddrAndTime(char **dataStartInOut, uint64_t *dataSizeLeftInOut, char **ipAddrOut, char **timeOut)
{
	char *curr = *dataStartInOut;
	uint64_t dataSizeLeft = *dataSizeLeftInOut;
	char *time = NULL;
	char *ipAddr = curr;

	// first space separates $ipaddr from $time
	while ((dataSizeLeft > 0) && (*curr != ' ')) {
		--dataSizeLeft;
		++curr;
	}

	// didn't find the space => something's wrong
	if (0 == dataSizeLeft)
		return false;

	assert(*curr == ' ');
	// replace space with 0 to make ipAddr a null-terminated string
	*curr = 0;
	--dataSizeLeft;
	++curr;

	time = curr;

	// find "\r\n' at the end
	while ((dataSizeLeft > 0) && !is_newline_char(*curr)) {
		--dataSizeLeft;
		++curr;
	}

	// replace '\r\n' with 0, to make time a null-terminated string
	while ((dataSizeLeft > 0) && is_newline_char(*curr)) {
		*curr++ = 0;
		--dataSizeLeft;
	}

	*ipAddrOut = ipAddr;
	*timeOut = time;
	*dataSizeLeftInOut = dataSizeLeft;
	*dataStartInOut = curr;
	return true;
}

// load up to IP_UPDATES_HISTORY_MAX latest entries from ip updates log
// When we finish g_ipUpdates is a list of ip updates with the most recent
// at the beginning of the list
static void ParseIpLogHistory(char *data, uint64_t dataSize)
{
	char *ipAddr = NULL;
	char *time = NULL;
	while (dataSize != 0) {
		bool ok = ExtractIpAddrAndTime(&data, &dataSize, &ipAddr, &time);
		if (!ok) {
			assert(0);
			break;
		}
		InsertIpUpdate(ipAddr, time);
	}
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

static void LoadAndParseHistory(const TCHAR *logFileName)
{
	uint64_t dataSize;
	char *data = FileReadAll(logFileName, &dataSize);
	if (!data)
		return;
	ParseIpLogHistory(data, dataSize);
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
	_time64(&ltime);
	today = _localtime64(&ltime);
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M", today);
	assert(gIpUpdatesLogFile);
	LogIpUpdateEntry(gIpUpdatesLogFile, ipAddress, timeBuf);
	InsertIpUpdate(ipAddress, timeBuf);
}

static void CloseIpUpdatesLog()
{
	if (gIpUpdatesLogFile) {
		fclose(gIpUpdatesLogFile);
		gIpUpdatesLogFile = NULL;
	}
}

static inline void FreeIpUpdatesLogName()
{
	TStrFree(&gIpUpdatesLogFileName);
}

// reverse g_ipUpdates and return size of the list
size_t ReverseIpUpdateList()
{
	size_t size = 0;
	IpUpdate *newHead = NULL;
	IpUpdate *curr = g_ipUpdates;
	while (curr) {
		IpUpdate *next = curr->next;
		curr->next = newHead;
		newHead = curr;
		curr = next;
		size++;
	}
	g_ipUpdates = newHead;
	return size;
}

static void WriteIpLogHistory(const TCHAR *logFileName, IpUpdate *head)
{
	FILE *log = _tfopen(logFileName, _T("wb"));
	IpUpdate *curr = head;
	while (curr) {
		LogIpUpdateEntry(log, curr->ipAddress, curr->time);
		curr = curr->next;
	}
	fclose(log);
}

// if we have more than IP_UPDATES_HISTORY_MAX entries, over-write
// the log with only the recent entries
static void OverwriteLogIfReachedLimit()
{
	// in order to (possibly) write out log entries, we have to write from
	// the end, so we need to reverse the list. We combine this with
	// calculating the size because we need to know the size to decide
	// if we need to overwrite the log
	size_t size = ReverseIpUpdateList();
	if (size < IP_UPDATES_PURGE_LIMIT)
		return;

	// skip the entries we don't want to write out
	assert(size > IP_UPDATES_SIZE_AFTER_PURGE);
	size_t toSkip = size - IP_UPDATES_SIZE_AFTER_PURGE;
	IpUpdate *curr = g_ipUpdates;
	while (curr && (toSkip != 0)) {
		curr = curr->next;
		toSkip--;
	}

	// write the rest to the new log
	WriteIpLogHistory(gIpUpdatesLogFileName, curr);
}

void FreeIpUpdatesHistory()
{
	CloseIpUpdatesLog();
	OverwriteLogIfReachedLimit();
	FreeIpUpdatesLogName();
	FreeIpUpdatesFromElement(g_ipUpdates);
	g_ipUpdates = NULL;
}

