// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "Prefs.h"
#include "StrUtil.h"
#include "MiscUtil.h"
#include "JsonParser.h"
#include "yajl_gen.h"

/* every preference can be accessed as g_${name} global */
#define M(PREF_NAME, PREF_DEFAULT_VALUE, IS_OBSCURED) \
char* g_pref_##PREF_NAME;

PREFS_DEF(M)

#undef M

/* define preferences array */
#define M(PREF_NAME, PREF_DEFAULT_VALUE, IS_OBSCURED) \
{ #PREF_NAME, &g_pref_##PREF_NAME, PREF_DEFAULT_VALUE, IS_OBSCURED },

Prefs g_prefs[] = {
	PREFS_DEF(M)
};

#undef M

static char *DupPrefValue(char *value, bool obscured)
{
	if (!value)
		return NULL;
	if (obscured)
		return StrDeobfuscate(value);
	else
		return strdup(value);
}

static bool PrefsLoad(const TCHAR *prefsFileName)
{
	ASSERT_RUN_ONCE();
	char *prefsAsJsonTxt = FileReadAll(prefsFileName);
	// if failed to read, pretend it's an empty hash
	// so that we go through the loop below that
	// sets default values
	if (!prefsAsJsonTxt)
		prefsAsJsonTxt = strdup("{}");

	JsonEl *json = ParseJsonToDoc(prefsAsJsonTxt);
	assert(json);
	if (!json) {
		free(prefsAsJsonTxt);
		return false;
	}

	// TODO: linear search is inefficient, but given small amount of data,
	// we don't care. Could be improved by sorting map values by keys
	// upon reading and doing a binary search or making it a real
	// hash table
	for (int i=0; i < dimof(g_prefs); i++) {
		Prefs *p = &(g_prefs[i]);
		char * name = p->name;
		char * value = *(p->value);
		assert(!value);
		JsonEl *valueEl = GetMapElByName(json, name);
		value = JsonElAsStringVal(valueEl);
		if (!value)
			value = p->defaultValue;
		value = DupPrefValue(value, p->obscured);
		*(p->value) = value;
	}

	free(prefsAsJsonTxt);
	JsonElFree(json);
	return true;
}

static bool IsPrefValEq(const char *s1, const char *s2)
{
	if (!s1 || !s2)
		return s1 == s2;
	return streq(s1, s2);
}

static void PrefsSave(const TCHAR *fileName)
{
	const char *buf = NULL;
	unsigned int bufLen;
	yajl_gen_status status;

	static const yajl_gen_config conf = {
		1, /* beautify */
		"  ", /* indent string */
	};
	yajl_gen h = yajl_gen_alloc(&conf);
	if (!h)
		goto Error;

	status = yajl_gen_map_open(h);
	if (yajl_gen_status_ok != status)
		goto Error;

	for (int i=0; i < dimof(g_prefs); i++) {
		Prefs *p = &(g_prefs[i]);
		char * name = p->name;
		char * value = *(p->value);
		char * defaultValue = p->defaultValue;
		char * valueSaved = NULL;
		// optimization: don't serialize prefs whose value is the
		// same as default value
		if (IsPrefValEq(value, defaultValue))
			continue;
		assert(value);
		if (!value)
			value = "";
		status = yajl_gen_string(h, (const unsigned char*)name, strlen(name));
		if (yajl_gen_status_ok != status)
			goto Error;
		if (p->obscured)
			valueSaved = StrObfuscate(value);
		else
			valueSaved = value;
		status = yajl_gen_string(h, (const unsigned char*)valueSaved, strlen(valueSaved));
		if (value != valueSaved)
			free(valueSaved);
		if (yajl_gen_status_ok != status)
			goto Error;
	}
	status = yajl_gen_map_close(h);
	if (yajl_gen_status_ok != status)
		goto Error;
	status = yajl_gen_get_buf(h, (const unsigned char **)&buf, &bufLen);
	if (yajl_gen_status_ok != status)
		goto Error;

	FileWriteAll(fileName, buf, bufLen);
Exit:
	if (h)
		yajl_gen_free(h);
	return;
Error:
	assert(0);
	goto Exit;
}

// Should be called only once, at the end. Prefs should not be accessed
// after this call.
void PreferencesFree()
{
	for (int i=0; i < dimof(g_prefs); i++) {
		Prefs *p = &(g_prefs[i]);
		char * value = *(p->value);
		free(value);
		p->value = reinterpret_cast<char**>(-1); /* so that we crash trying to access it */
	}
}

void GenUidIfNotExists()
{
	if (NULL != g_pref_unique_id)
		return;

	GUID guid;
	HRESULT hr = CoCreateGuid(&guid);
	if (S_OK != hr) {
		// can it ever happen? we could do some fallback here by generating
		// a random number
		assert(0);
		return;
	}

	char buf[64];
	sprintf(buf, "%08X%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X", guid.Data1,
		guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1],
		guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5],
		guid.Data4[6], guid.Data4[7]);

	SetPrefVal(&g_pref_unique_id, buf);
	PreferencesSave();
}

void PreferencesLoad()
{
	PrefsLoad(SettingsFileName());
}

void PreferencesLoadFromDir(const TCHAR *dir)
{
	PrefsLoad(SettingsFileNameInDir(dir));
}

void PreferencesSave()
{
	PrefsSave(SettingsFileName());
}

// Special handling to make caller's life easier: consider a NULL hostname
// to be a default hostname and represent it as empty string ("")
void PrefSetHostname(const char *hostname)
{
	if (!hostname)
		hostname = "";
	SetPrefVal(&g_pref_hostname, hostname);
}
