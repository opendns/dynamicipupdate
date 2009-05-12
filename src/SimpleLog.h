#ifndef SIMPLE_LOG_H__
#define SIMPLE_LOG_H__

bool SLogInit(const TCHAR *logFileName);
void SLogStop();
void slog(const char *s);
#ifdef UNICODE
void slog(const TCHAR *s);
#endif
void slogfmt(const char *fmt, ...);

#endif
