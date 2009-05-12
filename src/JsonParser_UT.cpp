// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "StrUtil.h"
#include "MiscUtil.h"
#include "JsonParser.h"
#include "JsonApiResponses.h"
#include "SampleApiResponses.h"

#include "UnitTests.h"

typedef struct ListNode {
	struct ListNode *next;
	int val;
} ListNode;

static ListNode *GenListNode(int val)
{
	ListNode *el = SA(ListNode);
	el->val = val;
	return el;
}

static void FreeList(ListNode *head)
{
	ListNode *curr = head;
	while (curr) {
		ListNode *next = curr->next;
		free(curr);
		curr = next;
	}
}

static ListNode *GenList(int elCount)
{
	ListNode *head = NULL;
	for (int i=1; i<=elCount; i++)
	{
		ListNode *el = GenListNode(i);
		el->next = head;
		head = el;
	}
	return head;
}

static void CheckListSorted(ListNode *head)
{
	if (!head)
		return;
	int lastVal = head->val;
	utassert(1 == lastVal);
	ListNode *curr = head->next;
	while (curr) {
		int newVal = curr->val;
		utassert(newVal == lastVal + 1);
		lastVal =  newVal;
		curr = curr->next;
	}
}

static void CheckListReverseSorted(ListNode *head)
{
	if (!head)
		return;
	int lastVal = head->val;
	ListNode *curr = head->next;
	while (curr) {
		int newVal = curr->val;
		utassert(newVal == lastVal-1);
		lastVal =  newVal;
		curr = curr->next;
	}
	utassert(lastVal == 1);
}

static void ReverseListGeneric_ut()
{
	ListNode *lnNull = NULL;
	ListNode *ln2;
	utassert(0 == ListLengthGeneric(lnNull));
	ListNode *ln = ReverseListGeneric(lnNull);
	utassert(NULL == ln);

	ln = GenList(1);
	utassert(1 == ListLengthGeneric(ln));
	ln2 = ReverseListGeneric(ln);
	utassert(1 == ListLengthGeneric(ln2));
	utassert(ln == ln2);
	FreeList(ln);

	ln = GenList(5);
	utassert(5 == ListLengthGeneric(ln));
	CheckListReverseSorted(ln);
	ln = ReverseListGeneric(ln);
	utassert(5 == ListLengthGeneric(ln));
	CheckListSorted(ln);
	FreeList(ln);
}

static void auth_ok_parsing_ut()
{
	JsonEl *json = ParseJsonToDoc(AUTH_OK);
	utassert(json);
	if (!json) return;
	WebApiStatus status = GetApiStatus(json);
	utassert(WebApiStatusSuccess == status);

	// test the shortcut way
	JsonEl *token = GetMapElByName(json, "token");
	utassert(token);
	if (!token) return;
	JsonElString *tokenString = JsonElAsString(token);
	utassert(tokenString);
	if (!tokenString) return;
	utassert(streq("DCE15D01E430D8C96D3920FB8F64185C", tokenString->stringVal));

	// test non-shortcut way
	JsonEl *response = GetMapElByName(json, "response");
	utassert(response);
	if (!response) return;
	utassert(JsonElAsMap(response));
	token = GetMapElByName(response, "token");
	utassert(token);
	if (!token) return;
	tokenString = JsonElAsString(token);
	utassert(tokenString);
	if (!tokenString) return;
	utassert(streq("DCE15D01E430D8C96D3920FB8F64185C", tokenString->stringVal));

	char *tokenTxt = GetApiResponseToken(json);
	utassert(tokenTxt);
	if (!tokenTxt) return;
	utassert(streq("DCE15D01E430D8C96D3920FB8F64185C", tokenTxt));

	long err;
	bool ok = GetApiError(json, &err);
	utassert(!ok);

	char *s = GetApiErrorMessage(json);
	utassert(NULL == s);
	JsonElFree(json);
}

static void bad_api_key_parsing_ut()
{
	JsonEl *json = ParseJsonToDoc(BAD_API_KEY);
	utassert(json);
	if (!json) return;
	WebApiStatus status = GetApiStatus(json);
	utassert(WebApiStatusFailure == status);

	char *tokenTxt = GetApiResponseToken(json);
	utassert(!tokenTxt);

	long err;
	bool ok = GetApiError(json, &err);
	utassert(ok);
	utassert(err == 1002);
	JsonElFree(json);
}

static void bad_pwd_parsing_ut()
{
	JsonEl *json = ParseJsonToDoc(BAD_PWD);
	utassert(json);
	if (!json) return;
	WebApiStatus status = GetApiStatus(json);
	utassert(WebApiStatusFailure == status);

	char *tokenTxt = GetApiResponseToken(json);
	utassert(!tokenTxt);

	long err;
	bool ok = GetApiError(json, &err);
	utassert(ok);
	utassert(err == ERR_BAD_USERNAME_PWD);
	JsonElFree(json);
}

static void check_network_info(NetworkInfo *ni, char *expectedInternalId, char *expectedIpAddress, char *expectedLabel, int expectedIsDynamic)
{
	utassert(streq(expectedInternalId, ni->internalId));
	utassert(streq(expectedIpAddress, ni->ipAddress));
	utassert(streq(expectedLabel, ni->label));
	utassert(expectedIsDynamic == ni->isDynamic);
}

static void one_network_not_dynamic_ut()
{
	JsonEl *json = ParseJsonToDoc(ONE_NETWORK_NOT_DYNAMIC);
	utassert(json);
	if (!json) return;
	NetworkInfo *ni = ParseNetworksGetJson(json);
	utassert(ni);
	if (!ni) return;
	utassert(ni->next == NULL);
	check_network_info(ni, "668257", "67.215.69.50", NULL, FALSE);
	utassert(0 == DynamicNetworksCount(ni));
	NetworkInfoFreeList(ni);
	JsonElFree(json);
}

static void one_network_dynamic_ut()
{
	JsonEl *json = ParseJsonToDoc(ONE_NETWORK_DYNAMIC);
	utassert(json);
	if (!json) return;
	NetworkInfo *ni = ParseNetworksGetJson(json);
	utassert(ni);
	if (!ni) return;
	utassert(ni->next == NULL);
	check_network_info(ni, "668258", "67.215.69.51", NULL, TRUE);
	utassert(1 == DynamicNetworksCount(ni));
	NetworkInfoFreeList(ni);
	JsonElFree(json);
}

static void multiple_networks_ut()
{
	JsonEl *json = ParseJsonToDoc(MULTIPLE_NETWORKS);
	utassert(json);
	if (!json) return;
	NetworkInfo *first = ParseNetworksGetJson(json);
	NetworkInfo *ni = first;
	utassert(ni);
	if (!ni) return;
	check_network_info(ni, "668261", "67.215.69.57", NULL, FALSE);
	ni = ni->next;
	utassert(ni);
	if (!ni) return;
	check_network_info(ni, "668262", "67.215.69.59", NULL, FALSE);
	ni = ni->next;
	utassert(ni);
	if (!ni) return;
	check_network_info(ni, "668263", "67.215.69.60", NULL, FALSE);
	ni = ni->next;
	utassert(ni);
	if (!ni) return;
	check_network_info(ni, "668260", "67.216.69.55", "home", TRUE);
	ni = ni->next;
	utassert(ni);
	if (!ni) return;
	check_network_info(ni, "668259", "67.215.69.52", "office", TRUE);
	utassert(ni->next == NULL);
	utassert(2 == DynamicNetworksCount(first));
	NetworkInfoFreeList(first);
	JsonElFree(json);
}

void json_parser_ut_all()
{
	ReverseListGeneric_ut();
	auth_ok_parsing_ut();
	bad_api_key_parsing_ut();
	bad_pwd_parsing_ut();
	one_network_not_dynamic_ut();
	one_network_dynamic_ut();
	multiple_networks_ut();
	// TODO: a negative test for networks parsing
}

