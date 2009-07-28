// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "StrUtil.h"

// single-linked list of string/time values
typedef struct StringTimeNode {
	struct StringTimeNode *next;
	char *s;
	time_t t;
} StringTimeNode;

StringTimeNode *AllocStringTimeNode(char *s)
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
	time(&node->t);
	return node;
}

static void InsertStringTimeNode(StringTimeNode **head, StringTimeNode *node)
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

static void AllocAndInsertStringTimeNode(StringTimeNode **head, char *s)
{
	StringTimeNode *node = AllocStringTimeNode(s);
	InsertStringTimeNode(head, node);
	free(s);
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
	AllocAndInsertStringTimeNode(head, name2); // name2 freed inside
}

static BOOL GetNetworkServersEnum(StringTimeNode** head, NETRESOURCE *nr)
{
	HANDLE hEnum;
	DWORD cbBuffer = 16384;
	NETRESOURCE *nrLocal;
	DWORD dwResultEnum;
	DWORD cEntries = 0;
	DWORD i;

	DWORD dwResult = WNetOpenEnum(RESOURCE_GLOBALNET,
									RESOURCETYPE_ANY,
									RESOURCEUSAGE_CONTAINER,
									nr,
									&hEnum);

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

void FreeStringTimeList(StringTimeNode *head)
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

StringTimeNode* GetNetworkServers() 
{
	StringTimeNode *head = NULL;
	NETRESOURCE* nr = NULL;
	GetNetworkServersEnum(&head, nr);
	return head;
}

static void *HEAP_ALLOC(SIZE_T x) {
	return HeapAlloc(GetProcessHeap(), 0, x);
}

static void HEAP_FREE(void *x) {
	HeapFree(GetProcessHeap(), 0, x);
}

void GetDNSPrefixes(StringTimeNode **head)
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
				AllocAndInsertStringTimeNode(head, dnsSuffix2); // dnsSuffix2 freed inside
			}
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	HEAP_FREE(pAddresses);
	return;
}

// result must be freed by FreeStringTimeList()
StringTimeNode* GetTypoExceptions()
{
	StringTimeNode *list = NULL;
	GetDNSPrefixes(&list);
	GetNetworkServersEnum(&list, NULL);
	return list;
}

