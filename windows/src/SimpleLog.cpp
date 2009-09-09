#include "stdafx.h"

#include "SimpleLog.h"

#include "MiscUtil.h"
#include "StrUtil.h"

static const TCHAR *	gLogFileName;
static FILE *			gLogFile;
bool					gLogToDebugger = true;
static CRITICAL_SECTION gLogCs;

bool SLogInit(const TCHAR *logFileName)
{
#ifdef DEBUG
	static bool wasHere = false;
	assert(!wasHere);
	wasHere = true;
#endif
	InitializeCriticalSection(&gLogCs);
	gLogFileName = tstrdup(logFileName);
	//gLogFile = _tfopen(logFileName, _T("wb"));
	gLogFile = _tfopen(logFileName, _T("ab"));
	return true;
}

void SLogStop()
{
	TStrFree(&gLogFileName);
	if (gLogFile) {
		fclose(gLogFile);
		gLogFile = NULL;
	}
	DeleteCriticalSection(&gLogCs);
}

// TODO: add critical section to protect logging?
void slog(const char *s)
{
	if (gLogToDebugger)
		OutputDebugStringA(s);
	if (!gLogFile)
		return;

	EnterCriticalSection(&gLogCs);
	size_t slen = strlen(s);
	fwrite(s, slen, 1, gLogFile);
	fflush(gLogFile);
	LeaveCriticalSection(&gLogCs);
}

void slognl(const char *s)
{
	slog(s);
	slog("\n");
}

#ifdef UNICODE
void slog(const WCHAR *s)
{
	if (!s)
		return;
	char *s2 = TStrToStr(s);
	if (!s2)
		return;
	slog(s2);
	free(s2);
}

void slognl(const WCHAR *s)
{
	slog(s);
	slog("\n");
}

#endif

void slogfmt(const char *fmt, ...)
{
    char        bufStatic[256];
    char  *     buf;
    int         bufCchSize;

    va_list args;
    va_start(args, fmt);

    buf = &(bufStatic[0]);
    bufCchSize = sizeof(bufStatic);
    int len = vsnprintf(buf, bufCchSize, fmt, args);
    
    if (len >= bufCchSize)
    {
        bufCchSize = len + 1;
        buf = (char *)malloc(bufCchSize*sizeof(char));
        if (NULL == buf)
            goto Exit;
        
        len = vsnprintf(buf, bufCchSize, fmt, args);
    }
    slog(buf);

Exit:
    if (buf != &(bufStatic[0]))
        free(buf);

    va_end(args);
}

