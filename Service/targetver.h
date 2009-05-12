// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

// The following macros define the minimum required platform.  The minimum required platform
// is the earliest version of Windows, Internet Explorer etc. that has the necessary features to run 
// your application.  The macros work by enabling all features available on platform versions up to and 
// including the version specified.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

