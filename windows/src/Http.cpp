// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "Http.h"
#include "MiscUtil.h"
#include "StrUtil.h"
#include "WTLThread.h"

#define CONTENT_TYPE_URL_ENCODED_W L"Content-Type: application/x-www-form-urlencoded\r\n"

static void ShowLastError(HttpResult *res)
{
	DWORD error = GetLastError();
	char *err = LastErrorAsStr(error);
	free(err);
	if (0 == error)
		error = (DWORD)-1;
	res->error = error;
}

static bool CreateAndStartThread(LPTHREAD_START_ROUTINE proc, LPVOID procArg)
{
	DWORD dwCreationFlags = 0;
	DWORD dwStackSize = 64*1024;
#ifdef _ATL_MIN_CRT
	DWORD threadId = 0;
	HANDLE hThread = ::CreateThread(NULL, dwStackSize, proc, procArg,
		dwCreationFlags, &threadId);
	if (NULL == hThread)
		return false;
#else
	unsigned threadId;
	uintptr_t hThread = _beginthreadex(NULL, dwStackSize,
		(unsigned (__stdcall*)(void*)) proc, procArg,
		dwCreationFlags, (unsigned*) &threadId);
	if (-1 == hThread)
		return false;
#endif
	return true;
}

bool SetupSessionAndRequest(const WCHAR *host, const WCHAR *url, bool https, 
	const WCHAR *method, HINTERNET *hSession, HINTERNET *hConnect, HINTERNET *hRequest)
{
	*hSession = WinHttpOpen(L"OpenDNS Updater Client",  
					WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
					WINHTTP_NO_PROXY_NAME, 
					WINHTTP_NO_PROXY_BYPASS, 0 );
	if (!*hSession)
		goto Error;

	INTERNET_PORT port = INTERNET_DEFAULT_HTTP_PORT;
	if (https)
		port = INTERNET_DEFAULT_HTTPS_PORT;

	*hConnect = WinHttpConnect(*hSession, host, port, 0);
	if (!*hConnect)
		goto Error;

	DWORD flags = 0;
	if (https)
		flags = WINHTTP_FLAG_SECURE;

	*hRequest = WinHttpOpenRequest(*hConnect, method, url,
					NULL, WINHTTP_NO_REFERER, 
					WINHTTP_DEFAULT_ACCEPT_TYPES, 
					flags);

	if (!*hRequest)
		goto Error;

	return true;
Error:
	return false;
}

static void CloseAllHandles(HINTERNET *hRequest, HINTERNET *hConnect, HINTERNET *hSession)
{
	if (*hRequest)
		WinHttpCloseHandle(*hRequest);
	if (*hConnect)
		WinHttpCloseHandle(*hConnect);
	if (*hSession)
		WinHttpCloseHandle(*hSession);
}

static bool HttpReadAllData(HINTERNET hRequest, MemSegment& data)
{
	BOOL		ok;
	DWORD		dwDownloaded = 0;
	DWORD		dwAvailable = 0;
	char 		buf[1024];
	DWORD		bufSize = dimof(buf);

	do  {
		dwAvailable = 0;
		ok = WinHttpQueryDataAvailable(hRequest, &dwAvailable);
		if (!ok)
			goto Error;

		ok = WinHttpReadData(hRequest, (LPVOID)buf, bufSize, &dwDownloaded);
		if (!ok)
			goto Error;

		if (dwDownloaded > 0)
			data.add(buf, dwDownloaded);

	} while (dwDownloaded > 0);
	return true;
Error:
	return false;
}

HttpResult* HttpGet(const WCHAR *host, const WCHAR *url, bool https)
{
	BOOL		ok;
	HINTERNET	hSession = NULL, hConnect = NULL, hRequest = NULL;

	HttpResult *res = new HttpResult();
	if (!res)
		return NULL;

	ok = SetupSessionAndRequest(host, url, https, L"GET", &hSession, &hConnect, &hRequest);
	if (!ok)
		goto Error;

	ok = WinHttpSendRequest(hRequest,
				WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 
				0, 0);

	if (!ok)
		goto Error;

	ok = WinHttpReceiveResponse(hRequest, NULL);
	if (!ok)
		goto Error;

	ok = HttpReadAllData(hRequest, res->data);
	if (!ok)
		goto Error;

Exit:
	CloseAllHandles(&hRequest, &hConnect, &hSession);
	return res;

Error:
	ShowLastError(res);
	goto Exit;
}

HttpResult* HttpGetWithBasicAuth(const WCHAR *host, const WCHAR *url, const WCHAR *userName, const WCHAR *pwd, bool https)
{
	BOOL		ok;
	HINTERNET	hSession = NULL, hConnect = NULL, hRequest = NULL;

	HttpResult *res = new HttpResult();
	if (!res)
		return NULL;

	ok = SetupSessionAndRequest(host, url, https, L"GET", &hSession, &hConnect, &hRequest);
	if (!ok)
		goto Error;
	

	ok = WinHttpSetCredentials(hRequest, WINHTTP_AUTH_TARGET_SERVER, WINHTTP_AUTH_SCHEME_BASIC, userName, pwd, NULL);
	if (!ok)
		goto Error;

	ok = WinHttpSendRequest(hRequest,
				WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				WINHTTP_NO_REQUEST_DATA, 0, 
				0, 0);

	if (!ok)
		goto Error;

	ok = WinHttpReceiveResponse(hRequest, NULL);
	if (!ok)
		goto Error;

	ok = HttpReadAllData(hRequest, res->data);
	if (!ok)
		goto Error;

Exit:
	CloseAllHandles(&hRequest, &hConnect, &hSession);
	return res;

Error:
	ShowLastError(res);
	goto Exit;
}

// TODO: should report non-200 results as NULL?
HttpResult* HttpGetWithBasicAuth(const char *host, const char *url, const char *userName, const char *pwd, bool https)
{
	WCHAR *host2 = StrToWstrSimple(host);
	WCHAR *url2 = StrToWstrSimple(url);
	WCHAR *userName2 = StrToWstrSimple(userName);
	WCHAR *pwd2 = StrToWstrSimple(pwd);
	HttpResult *res = NULL;
	if (host2 && url2 && userName2 && pwd2)
		res = HttpGetWithBasicAuth(host2, url2, userName2, pwd2, https);
	free(host2);
	free(url2);
	free(userName2);
	free(pwd2);
	return res;
}

HttpResult* HttpGet(const WCHAR *url)
{
	bool https = false;
	if (WStrStartsWithI(url, L"https://")) {
		https = true;
		url += 8; // skip https://
	} else if (WStrStartsWithI(url, L"http://")) {
		url += 7; // skip http://
	} else {
		// url must start with http:// or https://
		return NULL;
	}

	const WCHAR *urlPart = WStrFindChar(url, L'/');
	if (!urlPart)
		return NULL;
	int hostLen = urlPart - url;
	if (0 == hostLen)
		return NULL;
	WCHAR* host = WStrDupN(url, hostLen);
	if (!host)
		return NULL;
	HttpResult *res = HttpGet((const WCHAR*)host, urlPart, https);
	free(host);
	return res;
}

HttpResult* HttpGet(const char *url)
{
	WCHAR *url2 = StrToWstrSimple(url);
	HttpResult *res = NULL;
	if (url2)
		res = HttpGet(url2);
	free(url2);
	return res;
}

HttpResult* HttpGet(const char *host, const char *url, bool https)
{
	WCHAR *host2 = StrToWstrSimple(host);
	WCHAR *url2 = StrToWstrSimple(url);
	HttpResult *res = NULL;
	if (host2 && url2)
		res = HttpGet(host2, url2, https);
	free(host2);
	free(url2);
	return res;
}

HttpResult* HttpPost(const WCHAR *host, const WCHAR *url, const char *params, bool https)
{
	BOOL		ok;
	HINTERNET	hSession = NULL, hConnect = NULL, hRequest = NULL;

	HttpResult *res = new HttpResult();
	if (!res)
		return NULL;

	ok = SetupSessionAndRequest(host, url, https, L"POST", &hSession, &hConnect, &hRequest);
	if (!ok)
		goto Error;

	const WCHAR *headers = CONTENT_TYPE_URL_ENCODED_W;
	DWORD headersLen = wcslen(headers);
	DWORD flags = WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE;
	ok = WinHttpAddRequestHeaders(hRequest, headers, headersLen, flags);
	if (!ok)
		goto Error;

	DWORD paramsLen = strlen(params);
	ok = WinHttpSendRequest(hRequest,
				WINHTTP_NO_ADDITIONAL_HEADERS, 0,
				(LPVOID)params, paramsLen, 
				paramsLen, 0);
	if (!ok)
		goto Error;

	ok = WinHttpReceiveResponse(hRequest, NULL);
	if (!ok)
		goto Error;

	ok = HttpReadAllData(hRequest, res->data);
	if (!ok)
		goto Error;

Exit:
	CloseAllHandles(&hRequest, &hConnect, &hSession);
	return res;

Error:
	ShowLastError(res);
	goto Exit;
}

HttpResult* HttpPost(const char *host, const char *url, const char *params, bool https)
{
	WCHAR *host2 = StrToWstrSimple(host);
	WCHAR *url2 = StrToWstrSimple(url);
	HttpResult *res = NULL;
	if (host2 && url2)
		res = HttpPost(host2, url2, params, https);
	free(host2);
	free(url2);
	return res;
}

HttpResult* HttpPostData(const WCHAR *host, const WCHAR *url, void *data, DWORD dataSize, bool https)
{
#if 0
	inet = _wininet.InternetOpenA(CLIENTNAME "/" UT_VERSION_ID, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0 );
	if (inet == NULL) { err = "InternetOpen failed"; goto error1; }

	conn = _wininet.InternetConnectA(inet, CRASHREPORTHOST, INTERNET_DEFAULT_HTTP_PORT, "", "", INTERNET_SERVICE_HTTP, 0, 0);
#endif
	BOOL		ok;
	HINTERNET	hSession = NULL, hConnect = NULL, hRequest = NULL;

	HttpResult *res = new HttpResult();
	if (!res)
		return NULL;

	ok = SetupSessionAndRequest(host, url, https, L"POST", &hSession, &hConnect, &hRequest);
	if (!ok)
		goto Error;

	ok = WinHttpSendRequest(hRequest, L"Content-type: application/binary", (DWORD)-1, data, dataSize, dataSize, NULL);
	if (!ok)
		goto Error;

	ok = WinHttpReceiveResponse(hRequest, NULL);
	if (!ok)
		goto Error;

	ok = HttpReadAllData(hRequest, res->data);
	if (!ok)
		goto Error;

Exit:
	CloseAllHandles(&hRequest, &hConnect, &hSession);
	return res;

Error:
	ShowLastError(res);
	goto Exit;
}

HttpResult* HttpPostData(const char *host, const char *url, void *data, DWORD dataSize, bool https)
{
	WCHAR *host2 = StrToWstrSimple(host);
	WCHAR *url2 = StrToWstrSimple(url);
	HttpResult *res = NULL;
	if (host2 && url2)
		res = HttpPostData(host2, url2, data, dataSize, https);
	free(host2);
	free(url2);
	return res;
}

class HttpPostThreadData
{
public:
	char *m_host;
	char *m_url;
	char *m_params;
	bool m_https;
	HWND m_hwndToNotify;
	UINT m_msg;

	~HttpPostThreadData() {
		free(m_host);
		free(m_url);
		free(m_params);
	}
};

static DWORD WINAPI HttpPostThread(LPVOID arg)
{
	HttpPostThreadData *data = (HttpPostThreadData*)arg;
	HttpResult *result = HttpPost(data->m_host, data->m_url, data->m_params, data->m_https);
	PostMessage(data->m_hwndToNotify, data->m_msg, (WPARAM)result, 0);
	delete data;
	return 0;
}

bool HttpPostAsync(const char *host, const char *url, const char *params, bool https, HWND hwndToNotify, UINT msg)
{
	HttpPostThreadData *data = new HttpPostThreadData();
	if (!data)
		return false;
	data->m_host = strdup(host);
	data->m_url = strdup(url);
	data->m_params = strdup(params);
	data->m_https = https;
	data->m_hwndToNotify = hwndToNotify;
	data->m_msg = msg;
	return CreateAndStartThread(HttpPostThread, (LPVOID)data);
}

