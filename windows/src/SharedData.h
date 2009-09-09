// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHARED_DATA_H__
#define SHARED_DATA_H__

#include "SharedMem.h"

/* Data shared via shared memory between the service and UI */

typedef struct {
	IP4_ADDRESS currentIpAddress;
} ServiceStateData;

struct ServiceStateDataName {
	static const char *name() { return "opendns_ipupdater_servicestate"; }
};

typedef SharedMemT<ServiceStateData, ServiceStateDataName> ServiceStateSharedData;

#endif
