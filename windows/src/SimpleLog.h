#ifndef SIMPLE_LOG_H__
#define SIMPLE_LOG_H__

bool SLogInit(const TCHAR *logFileName);
void SLogStop();
void slog(const char *s);
void slognl(const char *s);

#ifdef UNICODE
void slog(const WCHAR *s);
void slognl(const WCHAR *s);
#endif

void slogfmt(const char *fmt, ...);

#endif
