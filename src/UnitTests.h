// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UNIT_TESTS_H__
#define UNIT_TESTS_H__

void recordUnitTestOk();
void recordUnitTestFailed();
int unitTestsTotal();
int unitTestsFailed();

#define utassert(ok) \
	assert(ok); \
	if (ok) \
		recordUnitTestOk(); \
	else \
		recordUnitTestFailed();

#endif
