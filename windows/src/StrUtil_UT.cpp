#include "stdafx.h"

#include "StrUtil.h"

#include "UnitTests.h"

static void strmisc_ut()
{
	utassert(streq(NULL, NULL));
	utassert(!streq(NULL, ""));
	utassert(!streq("", NULL));
	utassert(streq("", ""));
	utassert(!streq(" ", ""));
	utassert(!streq("", " "));
	utassert(streq("abcz", "abcz"));
}

static void strobf_ut()
{
	unsigned char orig[256];
	unsigned char obfdeobf[256];
	for (int i=0; i<256; i++) {
		orig[i] = (unsigned char)i;
	}
	memcpy(obfdeobf, orig, 256);
	strobf(obfdeobf);
	strobf(obfdeobf);
	int cmpres = memcmp(orig, obfdeobf, 256);
	utassert(0 == cmpres);
}

static void StrObfuscateHelper(const char *orig)
{
	char *obfuscated = StrObfuscate(orig);
	char *deobfuscated = StrDeobfuscate(obfuscated);
	utassert(streq(orig, deobfuscated));
	free(obfuscated);
	free(deobfuscated);
}

static void StrObfuscate_ut()
{
	StrObfuscateHelper("3E104FBAA978B41BA0391C71CC209654");
	StrObfuscateHelper("0123456789abcdefghijklmnopqrstuvwzABCDEFGHIJKLMNOPQRSTVWZ@!#@#$%&^%#$^");
}

#define PATH_1 _T("c:\\foo\\bar\\t.txt")

static void PathStripLastComponentInPlace_ut()
{
	BOOL ok;
	TCHAR buf[128];
	memcpy(buf, PATH_1, sizeof(PATH_1));
	ok = PathStripLastComponentInPlace(buf);
	utassert(ok);
	utassert(tstreq(buf, _T("c:\\foo\\bar")));
	ok = PathStripLastComponentInPlace(buf);
	utassert(ok);
	utassert(tstreq(buf, _T("c:\\foo")));
	ok = PathStripLastComponentInPlace(buf);
	utassert(ok);
	utassert(tstreq(buf, _T("c:")));
	ok = PathStripLastComponentInPlace(buf);
	utassert(!ok);
	utassert(tstreq(buf, _T("c:")));
	buf[0] = 0;
	ok = PathStripLastComponentInPlace(buf);
	utassert(!ok);
}

void strutil_ut_all()
{
	strmisc_ut();
	strobf_ut();
	StrObfuscate_ut();
	PathStripLastComponentInPlace_ut();
}
