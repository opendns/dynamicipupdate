// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "UnitTests.h"

static int g_unitTestsTotal;
static int g_unitTestsFailed;

void recordUnitTestOk()
{
	fprintf(stderr, ".");
	++g_unitTestsTotal;
}

void recordUnitTestFailed()
{
	fprintf(stderr, "-");
	++g_unitTestsTotal;
	++g_unitTestsTotal;
}

int unitTestsTotal()
{
	return g_unitTestsTotal;
}

int unitTestsFailed()
{
	return g_unitTestsFailed;
}
