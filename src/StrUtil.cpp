#include "stdafx.h"

#include "StrUtil.h"

void *memdup(const void *m, size_t len)
{
	if (!m)
		return NULL;
	void *res = malloc(len);
	if (!res)
		return NULL;
	memcpy(res, m, len);
	return res;
}

bool strempty(const char *s)
{
	if (!s)
		return true;
	if (!*s)
		return true;
	return false;
}

bool streq(const char *s1, const char *s2)
{
	if (!s1 && !s2)
		return true;
	if (!s1 || !s2)
		return false;
	return 0 == strcmp(s1, s2);
}

bool strieq(const char *s1, const char *s2)
{
	if (!s1 && !s2)
		return true;
	if (!s1 || !s2)
		return false;
	return 0 == stricmp(s1, s2);
}

bool tstreq(const TCHAR *s1, const TCHAR *s2)
{
	return 0 == StrCmp(s1, s2);
}

bool tstrieq(const TCHAR *s1, const TCHAR *s2)
{
	return 0 == StrCmpI(s1, s2);
}

size_t tstrlen(const TCHAR *s)
{
#ifdef UNICODE
	return wcslen(s);
#else
	return strlen(s);
#endif
}

char *StrDupSafe(const char *s)
{
	if (!s)
		return NULL;
	return strdup(s);
}

TCHAR *tstrdup(const TCHAR *s)
{
	if (!s)
		return NULL;
	int slen = tstrlen(s);
	return (TCHAR*)memdup(s, sizeof(TCHAR) * (slen + 1));
}

char *strdupn(const char *s, size_t len)
{
	size_t slen = strlen(s);
	if (slen < len)
		len = slen;
	char *res = (char*)malloc(len+1);
	memmove(res, s, len);
	res[len] = 0;
	return res;
}

WCHAR *WStrDupN(const WCHAR *s, size_t len)
{
	size_t slen = wcslen(s);
	if (slen < len)
		len = slen;
	WCHAR *res = (WCHAR*)malloc((len+1)*sizeof(WCHAR));
	memmove(res, s, len*sizeof(WCHAR));
	res[len] = 0;
	return res;
}

static void TStrAppend(TCHAR **s, const TCHAR *s2)
{
	assert(s);
	if (!s2)
		return;
	size_t s2len = tstrlen(s2);
	if (!s2len)
		return;
	TCHAR *tmp = *s;
	memcpy(tmp, s2, s2len * sizeof(TCHAR));
	tmp += s2len;
	*s = tmp;
}

TCHAR *TStrCat(const TCHAR *s1, const TCHAR *s2, const TCHAR *s3, const TCHAR *s4)
{
	size_t totalLen = 0;
	if (s1)
		totalLen += tstrlen(s1);
	if (s2)
		totalLen += tstrlen(s2);
	if (s3)
		totalLen += tstrlen(s3);
	if (s4)
		totalLen += tstrlen(s4);
	if (0 == totalLen)
		return NULL;
	TCHAR *res = (TCHAR*)malloc(sizeof(TCHAR)*(totalLen+1));
	if (!res)
		return NULL;
	TCHAR *tmp = res;
	TStrAppend(&tmp, s1);
	TStrAppend(&tmp, s2);
	TStrAppend(&tmp, s3);
	TStrAppend(&tmp, s4);
	*tmp = 0;
	return res;
}

/* A brain-dead way to convert ascii to Unicode, only works correctly
   for pure ascii strings without any funky characters.
   Caller has to free() the result. */
WCHAR *StrToWstrSimple(const char *s)
{
	if (!s)
		return NULL;
	size_t slen = strlen(s);
	WCHAR *res = (WCHAR*)malloc(sizeof(WCHAR) * (slen+1));
	if (!res)
		return NULL;
	WCHAR *tmp = res;
	char c = 1; // anything but 0
	while (c) {
		c = *s++;
		*tmp++ = c;
	}
	return res;
}

char *WstrToUtf8(const WCHAR *s)
{
	size_t slen = wcslen(s);
	int len = WideCharToMultiByte(CP_UTF8, 0, s, slen, NULL, 0, NULL, NULL);
	char *res = (char*)malloc(len+1);
	if (!res) return NULL;
	WideCharToMultiByte(CP_UTF8, 0, s, slen, res, len, NULL, NULL);
	res[len] = 0;
	return res;
}

/* Caller needs to free() the result */
WCHAR *Utf8ToWstr(const char *utf8)
{
    WCHAR *res;
    int requiredBufSize = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    res = (WCHAR*)malloc(requiredBufSize * sizeof(WCHAR));
    if (!res)
        return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, res, requiredBufSize);
    return res;
}

TCHAR* StrToTStr(const char *s)
{
#ifdef UNICODE
	return Utf8ToWstr(s);
#else
	return _strdup(s);
#endif
}

// caller needs to free() the result
char *TStrToStr(const TCHAR *s)
{
#ifdef UNICODE
	return WstrToUtf8(s);
#else
	return strdup(s);
#endif
}

WCHAR *TStrToWStr(const TCHAR *s)
{
#ifdef UNICODE
	return wcsdup(s);
#else
	return Utf8ToWstr(s);
#endif
}

/* return true if 'str' starts with 'txt', NOT case-sensitive */
int StrStartsWithI(const char *str, const char *txt)
{
    if (!str && !txt)
        return TRUE;
    if (!str || !txt)
        return FALSE;

    if (0 == _strnicmp(str, txt, strlen(txt)))
        return TRUE;
    return FALSE;
}

int WStrStartsWithI(const WCHAR *str, const WCHAR *txt)
{
    if (!str && !txt)
        return TRUE;
    if (!str || !txt)
        return FALSE;

    if (0 == _wcsnicmp(str, txt, wcslen(txt)))
        return TRUE;
    return FALSE;
}

#define  HEX_NUMBERS "0123456789ABCDEF"
static void CharToHex(unsigned char c, char* buffer)
{
    buffer[0] = HEX_NUMBERS[c / 16];
    buffer[1] = HEX_NUMBERS[c % 16];
}

char *StrHexEncode(const char *s)
{
	if (!s)
		return NULL;
	size_t slen = strlen(s);
	char *res = (char*)malloc( slen*2 + 1);
	char *tmp = res;
	while (*s) {
		unsigned char c = (unsigned char)*s;
		CharToHex(c, tmp);
		tmp += 2;
		++s;
	}
	*tmp = 0;
	return res;
}

int hexValFromChar(char hexChar)
{
	if ((hexChar >= '0') && (hexChar <= '9'))
		return hexChar - '0';
	if ((hexChar >= 'a') && (hexChar <= 'f'))
		return hexChar - 'a' + 10;
	if ((hexChar >= 'A') && (hexChar <= 'F'))
		return hexChar - 'A' + 10;
	return -1;
}

unsigned char FromHexChar(char hexChar1, char hexChar2)
{
	int hex1 = hexValFromChar(hexChar1);
	int hex2 = hexValFromChar(hexChar2);
	int val = hex1 * 16 + hex2;
	return (unsigned char)val;
}

char *StrHexDecode(const char *s)
{
	if (!s)
		return NULL;
	size_t slen = strlen(s);
	if (0 != slen % 2)
		return NULL;
	char *res = (char*)malloc((slen / 2) + 1);
	if (!res)
		return NULL;
	char *tmp = res;
	while (*s) {
		unsigned char hexChar1 = *s++;
		unsigned char hexChar2 = *s++;
		unsigned char val = FromHexChar(hexChar1, hexChar2);
		*tmp++ = (char)val;
	}
	*tmp = 0;
	return res;
}

void strobf(unsigned char *s)
{
	while (*s) {
		*s++ ^= 0xab;
	}
}

char *StrObfuscate(const char *s)
{
	if (!s)
		return NULL;
	char *s2 = strdup(s);
	strobf((unsigned char*)s2);
	char *res = StrHexEncode(s2);
	free(s2);
	return res;
}

char *StrDeobfuscate(const char *s)
{
	if (!s)
		return NULL;
	char *res = StrHexDecode(s);
	strobf((unsigned char*)res);
	return res;
}

/* Find character 'c' in string 'txt'.
   Return pointer to this character or NULL if not found */
const char *StrFindChar(const char *txt, char c)
{
    while (*txt != c) {
        if (0 == *txt)
            return NULL;
        ++txt;
    }
    return txt;
}

int str_contains(const char *str, char c)
{
    const char *pos = StrFindChar(str, c);
    if (!pos)
        return FALSE;
    return TRUE;
}

#define CHAR_URL_DONT_ENCODE   "-_.!~*'()"

int char_needs_url_encode(char c)
{
    if ((c >= 'a') && (c <= 'z'))
        return FALSE;
    if ((c >= 'A') && (c <= 'Z'))
        return FALSE;
    if ((c >= '0') && (c <= '9'))
        return FALSE;
    if (str_contains(CHAR_URL_DONT_ENCODE, c))
        return FALSE;
    return TRUE;
}

/* url-encode 'str'. Returns NULL in case of error. Caller needs to free()
   the result */
char *StrUrlEncode(const char *str)
{
    char *          encoded;
    char *          result;
    int             res_len = 0;
    const char *    tmp = str;

    /* calc the size of the string after url encoding */
    while (*tmp) {
        if (char_needs_url_encode(*tmp))
            res_len += 3;
        else
            ++res_len;
        tmp++;
    }
    if (0 == res_len)
        return NULL;

    encoded = (char*)malloc(res_len+1);
    if (!encoded)
        return NULL;

    result = encoded;
    tmp = str;
    while (*tmp) {
        if (char_needs_url_encode(*tmp)) {
            *encoded++ = '%';
            CharToHex(*tmp, encoded);
            encoded += 2;
        } else {
            if (' ' == *tmp)
                *encoded++ = '+';
            else
                *encoded++ = *tmp;
        }
        tmp++;
    }
    *encoded = 0;
    return result;
}

void TStrFree(const TCHAR **s)
{
	free((void*)*s);
	*s = NULL;
}

void StrSetCopy(char **s, const char *newVal)
{
	free(*s);
	if (newVal)
		*s = strdup(newVal);
	else
		*s = NULL;
}

BOOL tstreqn(TCHAR *s1, TCHAR *s2, int len)
{
	if (len <= 0)
		return FALSE;
	while (len > 0) {
		TCHAR c1 = *s1++;
		TCHAR c2 = *s2++;
		if (!c1 || !c2)
			return FALSE;
		if (c1 != c2)
			return FALSE;
		--len;
	}
	return TRUE;
}

int TStrFind(TCHAR *s, TCHAR *sub)
{
	size_t slen = tstrlen(s);
	size_t sublen = tstrlen(sub);
	TCHAR c = *sub++;
	TCHAR c2;
	for (size_t i=0; sublen + i <= slen; i++)
	{
		c2 = *s++;
		if (c != c2)
			continue;
		if (1 == sublen)
			return 1;
		if (tstreqn(s, sub, sublen-1))
			return i;
	}
	return -1;
}

const WCHAR* WStrFindChar(const WCHAR *s, WCHAR c)
{
	if (!s)
		return NULL;
	while (*s) {
		WCHAR c2 = *s;
		if (c == c2)
			return s;
		s++;
	}
	return NULL;
}

bool TStrEndsWithI(TCHAR *s, TCHAR *sub)
{
	if (!s || !sub)
		return false;
	size_t lenSub = tstrlen(sub);
	size_t lenS = tstrlen(s);
	if (lenSub > lenS)
		return false;
	TCHAR *sEnd = s + lenS - lenSub;
	return tstrieq(sEnd, sub);
}

// return true if string 's' contains string 'sub'
bool TStrContains(TCHAR *s, TCHAR *sub)
{
	return -1 != TStrFind(s, sub);
}

void TStrRemove(TCHAR *s, int start, int len)
{
	int slen = tstrlen(s);
	if (slen < (start+len))
		return;
	memmove(s + start, s + start + len, (slen-start-len+1) * sizeof(TCHAR));
}

int TStrFindAnchor(TCHAR *s, int *lenOut)
{
	int start = TStrFind(s, _T("<a"));
	if (-1 == start)
		return -1;
	int end = TStrFind(s + start + 2, _T(">"));
	if (-1 == end)
		return -1;
	*lenOut = 2 + end;
	return start;
}

// Removes '<a.*>' and '</a>' in-place from s
void TStrRemoveAnchorTags(TCHAR *s)
{
	int pos, len;
	TCHAR *tmp = s;
	for (;;) {
		pos = TStrFind(tmp, _T("</a>"));
		if (-1 == pos)
			break;
		TStrRemove(tmp, pos, 4);
		tmp += pos;
	}

	tmp = s;
	for (;;) {
		pos = TStrFindAnchor(tmp, &len);
		if (-1 == pos)
			break;
		TStrRemove(tmp, pos, len);
		tmp += pos;
	}
}

BOOL PathStripLastComponentInPlace(TCHAR *s)
{
	TCHAR *lastSepPos = NULL;
	while (*s) {
		if (*s == PATH_SEP_CHAR)
			lastSepPos = s;
		++s;
	}
	if (lastSepPos) {
		*lastSepPos = 0;
		return true;
	}
	return false;
}

TCHAR LastTChar(TCHAR *s)
{
	if (!s || !*s)
		return 0;
	size_t slen = tstrlen(s);
	return s[slen-1];
}

/* Replace all posible versions (Unix, Windows, Mac) of newline character
   with 'replace'. Returns newly allocated string with normalized newlines
   or NULL if error.
   Caller needs to free() the result */
char *StrNormalizeNewline(const char *txt, const char *replace)
{
    size_t          replace_len;
    char            c;
    char *          result;
    const char *    tmp;
    char *          tmp_out;
    size_t          result_len = 0;

    replace_len = strlen(replace);
    tmp = txt;
    for (;;) {
        c = *tmp++;
        if (!c)
            break;
        if (0xa == c) {
            /* a single 0xa => Unix */
            result_len += replace_len;
        } else if (0xd == c) {
            if (0xa == *tmp) {
                /* 0xd 0xa => dos */
                result_len += replace_len;
                ++tmp;
            }
            else {
                /* just 0xd => Mac */
                result_len += replace_len;
            }
        } else
            ++result_len;
    }

    if (0 == result_len)
        return NULL;

    result = (char*)malloc(result_len+1);
    if (!result)
        return NULL;
    tmp_out = result;
    for (;;) {
        c = *txt++;
        if (!c)
            break;
        if (0xa == c) {
            /* a single 0xa => Unix */
            memcpy(tmp_out, replace, replace_len);
            tmp_out += replace_len;
        } else if (0xd == c) {
            if (0xa == *txt) {
                /* 0xd 0xa => dos */
                memcpy(tmp_out, replace, replace_len);
                tmp_out += replace_len;
                ++txt;
            }
            else {
                /* just 0xd => Mac */
                memcpy(tmp_out, replace, replace_len);
                tmp_out += replace_len;
            }
        } else
            *tmp_out++ = c;
    }

    *tmp_out = 0;
    return result;
}

/* split a string '*txt' at the border character 'c'. Something like python's
   string.split() except called iteratively.
   Returns a copy of the string (must be free()d by the caller).
   Returns NULL to indicate there's no more items. */
char *StrSplitIter(char **txt, char c)
{
    const char *tmp;
    const char *pos;
    char *result;

    tmp = (const char*)*txt;
    if (!tmp)
        return NULL;

    pos = StrFindChar(tmp, c);
    if (pos) {
         result = strdupn(tmp, (int)(pos-tmp));
         *txt = (char*)pos+1;
    } else {
        result = strdup(tmp);
        *txt = NULL; /* next iteration will return NULL */
    }
    return result;
}

#define WHITE_SPACE_CHARS " \n\t\r"

/* Strip all 'to_strip' characters from the beginning of the string.
   Does stripping in-place */
void StrStripLeft(char *txt, const char *to_strip)
{
    char *new_start = txt;
    char c;
    if (!txt || !to_strip)
        return;
    for (;;) {
        c = *new_start;
        if (0 == c)
            break;
        if (!str_contains(to_strip, c))
            break;
        ++new_start;
    }

    if (new_start != txt) {
        memmove(txt, new_start, strlen(new_start)+1);
    }
}

/* Strip white-space characters from the beginning of the string.
   Does stripping in-place */
void StrStripWsLeft(char *txt)
{
    StrStripLeft(txt, WHITE_SPACE_CHARS);
}

void StrStripRight(char *txt, const char *to_strip)
{
    char * new_end;
    char   c;
    if (!txt || !to_strip)
        return;
    if (0 == *txt)
        return;
    /* point at the last character in the string */
    new_end = txt + strlen(txt) - 1;
    for (;;) {
        c = *new_end;
        if (!str_contains(to_strip, c))
            break;
        if (txt == new_end)
            break;
        --new_end;
    }
    if (str_contains(to_strip, *new_end))
        new_end[0] = 0;
    else
        new_end[1] = 0;
}

void StrStripWsRight(char *txt)
{
    StrStripRight(txt, WHITE_SPACE_CHARS);
}

void StrStripBoth(char *txt, const char *to_strip)
{
    StrStripLeft(txt, to_strip);
    StrStripRight(txt, to_strip);
}

void StrStripWsBoth(char *txt)
{
    StrStripWsLeft(txt);
    StrStripWsRight(txt);
}

int CharIsWs(char c)
{
    switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return TRUE;
    }
    return FALSE;
}

/* Given a pointer to a string in '*txt', skip past whitespace in the string
   and put the result in '*txt' */
void StrSkipWs(char **txtInOut)
{
    char *cur;
    if (!txtInOut)
        return;
    cur = *txtInOut;
    if (!cur)
        return;
    while (CharIsWs(*cur)) {
        ++cur;
    }
    *txtInOut = cur;
}