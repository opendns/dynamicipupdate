#ifndef STR_UTIL_H__
#define STR_UTIL_H__

/* DOS is 0xd 0xa */
#define DOS_NEWLINE "\x0d\x0a"
/* Mac is single 0xd */
#define MAC_NEWLINE "\x0d"
/* Unix is single 0xa (10) */
#define UNIX_NEWLINE "\x0a"
#define UNIX_NEWLINE_C 0xa

#define PATH_SEP_CHAR _T('\\')
#define PATH_SEP_STR _T("\\")

void *memdup(const void *m, size_t len);
size_t tstrlen(const TCHAR *s);
bool strempty(const char *s);
bool streq(const char *s1, const char *s2);
bool strieq(const char *s1, const char *s2);
bool tstreq(const TCHAR *s1, const TCHAR *s2);
bool tstrieq(const TCHAR *s1, const TCHAR *s2);
char *StrDupSafe(const char *s);
char *strdupn(const char *s, size_t len);
WCHAR *WStrDupN(const WCHAR *s, size_t len);
TCHAR *tstrdup(const TCHAR *s);
TCHAR *TStrCat(const TCHAR *s1, const TCHAR *s2 = NULL, const TCHAR *s3 = NULL, const TCHAR *s4 = NULL);
int  StrStartsWithI(const char *str, const char *txt);
int  WStrStartsWithI(const WCHAR *str, const WCHAR *txt);
char *StrHexEncode(const char *s);
char *StrHexDecode(const char *s);
void strobf(unsigned char *s);
char *StrObfuscate(const char *s);
char *StrDeobfuscate(const char *s);
void TStrFree(const TCHAR **s);
char *TStrToStr(const TCHAR *s);
WCHAR *TStrToWStr(const TCHAR *s);
TCHAR* StrToTStr(const char *s);
WCHAR *StrToWstrSimple(const char *str);
char *WstrToUtf8(const WCHAR *s);
void StrSetCopy(char **s, const char *newVal);

const char *StrFindChar(const char *txt, char c);
const char *StrFindLastChar(const char *txt, char c);
int TStrFind(TCHAR *s, TCHAR *sub);
const WCHAR* WStrFindChar(const WCHAR *s, WCHAR c);

bool TStrEndsWithI(TCHAR *s, TCHAR *sub);
bool TStrContains(TCHAR *s, TCHAR *sub);
void TStrRemoveAnchorTags(TCHAR *s);
char *StrUrlEncode(const char *str);
BOOL PathStripLastComponentInPlace(TCHAR *s);
TCHAR LastTChar(TCHAR *s);
char *StrNormalizeNewline(const char *txt, const char *replace);
char *StrSplitIter(char **txt, char c);
void StrStripWsBoth(char *txt);
void StrStripWsRight(char *txt);
void StrSkipWs(char **txtInOut);
int CharIsWs(char c);

#endif
