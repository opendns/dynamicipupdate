// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PREFS_H__
#define PREFS_H__

#include "StrUtil.h"

// Potential states for a logged in user. They're strings so that we can
// save them in preferences
#define UNS_OK "unsok"  // selected a network or we're using a default network
#define UNS_NO_NETWORKS "unsnonet" // doesn't have any networks
#define UNS_NO_DYNAMIC_IP_NETWORKS "unsnodynip" // has networks but none of them is configured for dynamic ips
#define UNS_NO_NETWORK_SELECTED "unnonetsel"

void PreferencesSave();
void PreferencesLoad();
void PreferencesLoadFromDir(const TCHAR *dir);
void PreferencesFree();

typedef struct Prefs {
	char *name;
	char **value;
	char *defaultValue;
	bool obscured;
} Prefs;

#define PREFS_DEF(M) \
	M(user_name, NULL, false) \
	M(unique_id, NULL, false) \
	M(token, NULL, true) \
	M(hostname, NULL, false) \
	M(user_networks_state, NULL, false) \
	M(send_updates, "1", false)

// g_pref_hostname - NULL means invalid, empty string means default

/* every preference can be accessed as g_${name} global */
#define M(PREF_NAME, PREF_DEFAULT_VALUE, IS_OBSCURED) \
extern char* g_pref_##PREF_NAME;

PREFS_DEF(M)

#undef M

void PrefSetHostname(const char *hostname);
void GenUidIfNotExists();
static inline void SetPrefVal(char **s, const char *newVal)
{
	StrSetCopy(s, newVal);
}

static inline void SetPrefValBool(char **s, BOOL val)
{
	const char *sval = "1";
	if (FALSE == val)
		sval = "0";
	StrSetCopy(s, sval);
}

static inline BOOL GetPrefValBool(char *s)
{
	if (streq("0", s))
		return FALSE;
	return TRUE;
}

#endif

