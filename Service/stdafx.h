// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>
#include <atldef.h>

#include <stdio.h>
#include <tchar.h>

#include <Winhttp.h>
#include <shlwapi.h>
#include <ShlObj.h>

#if 0
// I wish I knew why I have to define SECURITY_WIN32
#define SECURITY_WIN32
#include <Security.h>
#pragma comment(lib, "Secur32.lib")
#endif

#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include "pstdint.h"

#pragma comment(lib, "dnsapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")

