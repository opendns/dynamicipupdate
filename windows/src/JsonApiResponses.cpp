// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "JsonParser.h"
#include "JsonApiResponses.h"

#include "MiscUtil.h"
#include "StrUtil.h"

WebApiStatus GetApiStatus(JsonEl *json)
{
	JsonEl *statusEl = GetMapElByName(json, "status");
	if (!statusEl)
		return WebApiStatusUnknown;
	JsonElString *elString = JsonElAsString(statusEl);
	if (!elString)
		return WebApiStatusUnknown;
	char *s = elString->stringVal;
	if (streq("success", s))
		return WebApiStatusSuccess;
	if (streq("failure", s))
		return WebApiStatusFailure;
	return WebApiStatusUnknown;
}

// returns the string value of "token" element or NULL if there was
// an error ("token" element doesn't exist, is not a string)
char *GetApiResponseToken(JsonEl *json)
{
	// TODO: should get response.token but *.token works for us too
	JsonEl *el = GetMapElByName(json, "token");
	return JsonElAsStringVal(el);
}

bool GetApiError(JsonEl *json, long *errOut)
{
	JsonEl *el = GetMapElByName(json, "error");
	return JsonElAsIntegerVal(el, errOut);
}

char *GetApiErrorMessage(JsonEl *json)
{
	JsonEl *el = GetMapElByName(json, "error_message");
	return JsonElAsStringVal(el);
}

static void NetworkInfoFree(NetworkInfo *ni)
{
	if (!ni)
		return;
	free(ni->networkId);
	free(ni->ipAddress);
	free(ni->label);
	free(ni);
}

void NetworkInfoFreeList(NetworkInfo *head)
{
	NetworkInfo *curr = head;
	while (curr) {
		NetworkInfo *next = curr->next;
		NetworkInfoFree(curr);
		curr = next;
	}
}

static NetworkInfo* NetworkInfoFromJson(JsonElMapData *json)
{
	char *networkId = json->key;
	int isDynamic = FALSE;
	char *label = NULL;
	char *ipAddress = NULL;

	JsonElMap *networkInfoMap = JsonElAsMap(json->val);
	if (!networkInfoMap)
		return NULL;
	JsonElMapData *networkInfoMapEl = networkInfoMap->firstVal;

	while (networkInfoMapEl) {
		char *key = networkInfoMapEl->key;
		if (streq(key, "dynamic")) {
			JsonElBool *boolEl = JsonElAsBool(networkInfoMapEl->val);
			if (!boolEl)
				return NULL;
			isDynamic = boolEl->boolVal;
		} else if (streq(key, "label")) {
			label = JsonElAsStringVal(networkInfoMapEl->val);
		} else if (streq(key, "ip_address")) {
			ipAddress = JsonElAsStringVal(networkInfoMapEl->val);
		} else {
			assert(0);
			return NULL;
		}
		networkInfoMapEl = networkInfoMapEl->next;
	}

	if (!ipAddress)
		return NULL;
	NetworkInfo *res = SA(NetworkInfo);
	if (!res)
		return NULL;
	res->next = NULL;
	res->networkId = StrDupSafe(networkId);
	res->ipAddress = StrDupSafe(ipAddress);
	res->label = StrDupSafe(label);
	res->isDynamic = isDynamic;
	return res;
}

NetworkInfo *ParseNetworksGetJson(JsonEl *json)
{
	JsonEl *el = GetMapElByName(json, "response");
	if (!el)
		return NULL;
	JsonElMap *networksMap = JsonElAsMap(el);
	if (!networksMap)
		return NULL;

	NetworkInfo *head = NULL;
	JsonElMapData *currNetwork = networksMap->firstVal;
	while (currNetwork) {
		NetworkInfo *ni = NetworkInfoFromJson(currNetwork);
		if (!ni)
			goto Error;
		ni->next = head;
		head = ni;
		currNetwork = currNetwork->next;
	}
	head = ReverseListGeneric(head);
	return head;

Error:
	NetworkInfoFreeList(head);
	return NULL;
}

size_t DynamicNetworksCount(NetworkInfo *head, bool onlyLabeled)
{
	size_t count = 0;
	while (head) {
		if (head->isDynamic) {
			if (head->label || !onlyLabeled)
				++count;
		}
		head = head->next;
	}
	return count;
}

NetworkInfo *FindDynamicWithLabel(NetworkInfo *head, char *label)
{
	if (!label)
		return NULL;
	while (head) {
		if (head->isDynamic) {
			if (strieq(head->label, label))
				return head;
		}
		head = head->next;
	}
	return NULL;
}

NetworkInfo *FindFirstDynamic(NetworkInfo *head)
{
	while (head) {
		if (head->isDynamic)
			return head;
		head = head->next;
	}
	return NULL;
}
