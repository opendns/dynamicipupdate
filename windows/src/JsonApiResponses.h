// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JSON_API_RESPONSES_H__
#define JSON_API_RESPONSES_H__

#include "JsonParser.h"

typedef enum {
	WebApiStatusUnknown = -1,
	WebApiStatusSuccess = 1,
	WebApiStatusFailure = 2
} WebApiStatus;

enum {
	ERR_UNKNOWN_METHOD			= 1003,
	ERR_BAD_USERNAME_PWD		= 1004,
	ERR_BAD_TOKEN				= ERR_BAD_USERNAME_PWD,
	// if there are no networks in networks_get
	ERR_NETWORK_DOESNT_EXIST	= 4008
};

typedef struct NetworkInfo NetworkInfo;

// a linked list of information about networks, as returned from networks_get call
struct NetworkInfo {
	// make 'next' the first field for perf
	NetworkInfo *next;
	char *networkId;
	int isDynamic;
	char *label;
	char *ipAddress;
};

WebApiStatus GetApiStatus(JsonEl *json);
char *GetApiResponseToken(JsonEl *json);
bool GetApiError(JsonEl *json, long *errOut);
char *GetApiErrorMessage(JsonEl *json);
NetworkInfo *ParseNetworksGetJson(JsonEl *json);
void NetworkInfoFreeList(NetworkInfo *head);
size_t DynamicNetworksCount(NetworkInfo *head, bool onlyLabeled=false);
NetworkInfo *FindDynamicWithLabel(NetworkInfo *head, char *label);
NetworkInfo *FindFirstDynamic(NetworkInfo *head);
#endif
