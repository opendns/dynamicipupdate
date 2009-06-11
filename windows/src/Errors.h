// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ERRORS_H__
#define ERRORS_H__

enum {
	ErrNoError = 0,
	ErrOpenSCManagerFail,
	ErrOpenServiceFail,
	ErrControlServiceStopFail,
	ErrDeleteServiceFail,
	ErrGetModuleFileNameFail,
	ErrCreateServiceFail,
	ErrChangeServiceConfig2Fail,
	ErrStartServiceFail,
	ErrStartServiceCtrlDispatcherFail,
};

#endif
