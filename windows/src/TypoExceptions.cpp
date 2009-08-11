// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "StrUtil.h"
#include "MiscUtil.h"
#include "Http.h"
#include "JsonParser.h"
#include "Prefs.h"
#include "JsonApiResponses.h"
#include "SimpleLog.h"

// single-linked list of string/time values
typedef struct StringTimeNode {
	struct StringTimeNode *next;
	char *s;
	time_t t;
} StringTimeNode;

// list of all typo exception names seen since starting the program
static StringTimeNode *g_allTypoExceptions;
static int g_allTypoExceptionsCount;

static StringTimeNode *StringTimeNodeAlloc(char *s, time_t t = 0)
{
	if (!s)
		return NULL;
	StringTimeNode *node = (StringTimeNode*)calloc(1, sizeof(StringTimeNode));
	if (!node)
		return NULL;
	node->s = strdup(s);
	if (!node->s) {
		free(node);
		return NULL;
	}
	if (0 == t)
		time(&t);
	node->t = t;
	return node;
}

static void StringTimeNodeInsert(StringTimeNode **head, StringTimeNode *node)
{
	if (!node)
		return;
	if (!*head) {
		*head = node;
	} else {
		node->next = *head;
		*head = node;
	}
}

static void StringTimeNodeAllocAndInsert(StringTimeNode **head, char *s, time_t t=0)
{
	StringTimeNode *node = StringTimeNodeAlloc(s, t);
	StringTimeNodeInsert(head, node);
}

static void AddToListIfServer(StringTimeNode **head, NETRESOURCE *nr)
{
	if (RESOURCEDISPLAYTYPE_SERVER != nr->dwDisplayType)
		return;

	LPTSTR name = nr->lpRemoteName;
	// netbios names must start with "\\"
	if (*name++ != '\\')
		return;    
	if (*name++ != '\\')
		return;

	char *name2 = TStrToStr(name);
	StringTimeNodeAllocAndInsert(head, name2);
	free(name2);
}

static BOOL GetNetworkServersEnum(StringTimeNode** head, NETRESOURCE *nr)
{
	HANDLE hEnum;
	DWORD cbBuffer = 16384;
	NETRESOURCE *nrLocal;
	DWORD dwResultEnum;
	DWORD cEntries = (DWORD)-1;
	DWORD i;

	DWORD dwResult = WNetOpenEnum(RESOURCE_GLOBALNET, RESOURCETYPE_ANY,
									RESOURCEUSAGE_CONTAINER, nr, &hEnum);

	if (dwResult != NO_ERROR)
		return FALSE;

	nrLocal = (LPNETRESOURCE) GlobalAlloc(GPTR, cbBuffer);
	if (!nrLocal)
		return FALSE;

	for(;;) {
		ZeroMemory(nrLocal, cbBuffer);
		dwResultEnum = WNetEnumResource(hEnum, &cEntries, nrLocal, &cbBuffer);
		if (dwResultEnum != NO_ERROR)
			break;

		for (i = 0; i < cEntries; i++) {
			AddToListIfServer(head, &nrLocal[i]);
			if (RESOURCEUSAGE_CONTAINER == (nrLocal[i].dwUsage & RESOURCEUSAGE_CONTAINER)) {
				GetNetworkServersEnum(head, &nrLocal[i]);
			}
		}
	}

	GlobalFree((HGLOBAL) nrLocal);
	dwResult = WNetCloseEnum(hEnum);

	if (dwResult != NO_ERROR)
		return FALSE;

	return TRUE;
}

static void FreeStringTimeList(StringTimeNode *head)
{
	StringTimeNode *next;
	StringTimeNode *curr = head;
	while (curr) {
		next = curr->next;
		free(curr->s);
		free(curr);
		curr = next;
	}
}

static void *HEAP_ALLOC(SIZE_T x) {
	return HeapAlloc(GetProcessHeap(), 0, x);
}

static void HEAP_FREE(void *x) {
	HeapFree(GetProcessHeap(), 0, x);
}

static void GetDNSPrefixes(StringTimeNode **head)
{
	DWORD dwRetVal = 0;

	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	ULONG family = AF_UNSPEC;

	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 0;

	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;

	outBufLen = sizeof(IP_ADAPTER_ADDRESSES);
	pAddresses = (IP_ADAPTER_ADDRESSES *)HEAP_ALLOC(outBufLen);
	if (pAddresses == NULL)
		return;

	// Make an initial call to GetAdaptersAddresses to get the 
	// size needed into the outBufLen variable
	ULONG ret = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
	if (ERROR_BUFFER_OVERFLOW == ret) {
		HEAP_FREE(pAddresses);
		pAddresses = (IP_ADAPTER_ADDRESSES *) HEAP_ALLOC(outBufLen);
	}

	if (pAddresses == NULL)
		return;

	dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

	if (dwRetVal == NO_ERROR) {
		pCurrAddresses = pAddresses;
		while (pCurrAddresses) {
			WCHAR *dnsSuffix = pCurrAddresses->DnsSuffix;
			if (dnsSuffix && *dnsSuffix) {
				char *dnsSuffix2 = WstrToUtf8(dnsSuffix);
				StringTimeNodeAllocAndInsert(head, dnsSuffix2);
				free(dnsSuffix2);
			}
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	HEAP_FREE(pAddresses);
	return;
}

// result must be freed by FreeStringTimeList()
static StringTimeNode* GetTypoExceptions()
{
	StringTimeNode *head = NULL;
	GetDNSPrefixes(&head);
	GetNetworkServersEnum(&head, NULL);
	return head;
}

static char *GetNamesAsCommaSeparatedString(StringTimeNode *head)
{
	CString s;
	StringTimeNode *curr = head;
	while (curr) {
		s += curr->s;
		if (curr->next)
			s += ",";
		curr = curr->next;
	}
	char *res = TStrToStr(s);
	return res;
}

static BOOL SubmitAddedTypoExceptions(StringTimeNode *added)
{
	HttpResult *httpRes = NULL;
	JsonEl *json = NULL;
	char *jsonTxt = NULL;
	BOOL res = TRUE;

	if (!added)
		return FALSE;

	char *toAdd = GetNamesAsCommaSeparatedString(added);
	CString params = ApiParamsNetworkTypoExceptionsAdd(g_pref_token, g_pref_network_id, toAdd);
	const char *paramsTxt = TStrToStr(params);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	httpRes = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
	free((void*)paramsTxt);
	if (!httpRes || !httpRes->IsValid())
		goto Error;

	DWORD dataSize;
	jsonTxt = (char *)httpRes->data.getData(&dataSize);
	if (!jsonTxt)
		goto Error;

	// we log the server's json response if there was an error, but ignore
	// the error otherwise
	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Exit;
	WebApiStatus status = GetApiStatus(json);
	if (WebApiStatusSuccess != status)
		slog(jsonTxt);

Exit:
	JsonElFree(json);
	delete httpRes;
	return res;
Error:
	res = FALSE;
	goto Exit;
}

static BOOL SubmitExpiredTypoExceptions(StringTimeNode *expired)
{
	HttpResult *httpRes = NULL;
	JsonEl *json = NULL;
	char *jsonTxt = NULL;
	BOOL res = TRUE;

	if (!expired)
		return FALSE;

	char *toDelete = GetNamesAsCommaSeparatedString(expired);
	CString params = ApiParamsNetworkTypoExceptionsRemove(g_pref_token, g_pref_network_id, toDelete);
	const char *paramsTxt = TStrToStr(params);
	const char *apiHost = GetApiHost();
	bool apiHostIsHttps = IsApiHostHttps();
	httpRes = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
	free((void*)paramsTxt);
	if (!httpRes || !httpRes->IsValid())
		goto Error;

	DWORD dataSize;
	jsonTxt = (char *)httpRes->data.getData(&dataSize);
	if (!jsonTxt)
		goto Error;

	// we log the server's json response if there was an error, but ignore
	// the error otherwise
	json = ParseJsonToDoc(jsonTxt);
	if (!json)
		goto Exit;
	WebApiStatus status = GetApiStatus(json);
	if (WebApiStatusSuccess != status)
		slog(jsonTxt);

Exit:
	JsonElFree(json);
	delete httpRes;
	return res;
Error:
	res = FALSE;
	goto Exit;
}

static BOOL StringTimeNodeExists(StringTimeNode *head, char *s)
{
	StringTimeNode *curr = head;
	while (curr) {
		if (strieq(curr->s, s))
			return TRUE;
		curr = curr->next;
	}
	return FALSE;
}

// Note: this is slow (O(n*m) where n=len(all), m=len(current)) but it probably
// doesn't matter. Could be faster if we sorted names or used hash table
static StringTimeNode *StringTimeNodeListGetAdded(StringTimeNode *all, StringTimeNode *current)
{
	StringTimeNode *added = NULL;
	StringTimeNode *curr = current;
	while (curr) {
		if (!StringTimeNodeExists(all, curr->s)) {
			StringTimeNodeAllocAndInsert(&added, curr->s);
		}
		curr = curr->next;
	}
	return added;
}

static BOOL StringTimeNodeIsExpired(StringTimeNode *n)
{
	time_t curr;
	time(&curr);
	const time_t TWO_WEEKS_IN_SECONDS = 60*60*24*14;
	time_t expirationTime = n->t + TWO_WEEKS_IN_SECONDS;
	if (curr > expirationTime)
		return TRUE;
	return FALSE;
}

static StringTimeNode *StringTimeNodeListGetExpired(StringTimeNode *head)
{
	StringTimeNode *expiredList = NULL;
	StringTimeNode *curr = head;
	while (curr) {
		if (StringTimeNodeIsExpired(curr)) {
			StringTimeNodeAllocAndInsert(&expiredList, curr->s, curr->t);
		}
		curr = head->next;
	}
	return expiredList;
}

static void StringTimeNodeListAdd(StringTimeNode **head, StringTimeNode *toAdd)
{
	StringTimeNode *curr = toAdd;
	while (curr) {
		StringTimeNodeAllocAndInsert(head, curr->s, curr->t);
		curr = curr->next;
	}
}

static void StringTimeNodeListRemoveExpired(StringTimeNode **head)
{
	// TODO: if I was smarter, I would do in-place removal but I'm not so I'll
	// do something more expensive but simpler: rebuild a list without expired
	// nodes. It should happen very rarely.
	StringTimeNode *newHead = NULL;
	StringTimeNode *curr = *head;
	while (curr) {
		if (!StringTimeNodeIsExpired(curr)) {
			StringTimeNodeAllocAndInsert(&newHead, curr->s, curr->t);
		}
		curr = curr->next;
	}
	FreeStringTimeList(*head);
	*head = newHead;
}

static void UpdateTypoExceptionsCount() {
	StringTimeNode *curr = g_allTypoExceptions;
	int count = 0;
	while (curr) {
		++count;
		curr = curr->next;
	}
	g_allTypoExceptionsCount = count;
}

DWORD WINAPI SubmitTypoExceptionsThread(LPVOID /*lpParam*/) 
{
	StringTimeNode *currentList = GetTypoExceptions();
	StringTimeNode *added = StringTimeNodeListGetAdded(g_allTypoExceptions, currentList);
	StringTimeNode *expired = StringTimeNodeListGetExpired(g_allTypoExceptions);

	BOOL addedOk = SubmitAddedTypoExceptions(added);
	BOOL expiredOk = SubmitExpiredTypoExceptions(expired);

	if (addedOk) {
		StringTimeNodeListAdd(&g_allTypoExceptions, added);
	}

	if (expiredOk) {
		StringTimeNodeListRemoveExpired(&g_allTypoExceptions);
	}

	UpdateTypoExceptionsCount();

	FreeStringTimeList(currentList);
	FreeStringTimeList(added);
	FreeStringTimeList(expired);
	return 0;
}

void SubmitTypoExceptionsAsync() 
{
	if (!CanSendIPUpdates())
		return;

	DWORD stackSize = 64*1024;
	DWORD threadId;
	::CreateThread(NULL, stackSize, SubmitTypoExceptionsThread, 0, 0, &threadId);
}

// Note: for thread safety this cannot be recalculated from g_allTypoExceptions
// since g_allTypoExceptions might be being modified in SubmitTypoExceptionsThread()
int TypoExceptionsCount()
{
	return g_allTypoExceptionsCount;
}

