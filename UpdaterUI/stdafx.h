// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// Set minium Windows version supported
#define WINVER		0x0500
#define _WIN32_WINNT	0x0501
#define _WIN32_IE	0x0501
#define _RICHEDIT_VER	0x0300

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>
#include <atlcrack.h>
#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>
#include <atlctrlw.h>
#include <atlmisc.h>

#include <Winhttp.h>
#include <shlwapi.h>

#if 0
// I wish I knew why I have to define SECURITY_WIN32
#define SECURITY_WIN32
#include <Security.h>
#pragma comment(lib, "Secur32.lib")
#endif

#include <assert.h>
#include "pstdint.h"

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")

