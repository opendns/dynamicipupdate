// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "StrUtil.h"
#include "wbem.h"

static void SetDnsServers(IWbemServices *pSvc, IWbemClassObject *pObj, OLECHAR *srv1, OLECHAR *srv2)
{
	HRESULT			hr;
	SAFEARRAYBOUND	parrayBound;
	VARIANT			vtProp;
	_variant_t		serverList;
	int				srvCount = 0;
	CString			methodPath;
	BSTR			methodName = SysAllocString(L"SetDNSServerSearchOrder");
	BSTR			methodPathBstr = NULL;
	IWbemClassObject *pAdapterClassObj = NULL;
	IWbemClassObject *pAdapterClassInst = NULL;
	IWbemClassObject *pSetDNSServerSearchOrderIn = NULL;
	IWbemClassObject *pSetDNSServerSearchOrderOut = NULL;

	VariantInit(&vtProp);
	hr = pObj->Get(L"Index", 0, &vtProp, 0, 0);
	if (FAILED(hr))
		goto Exit;

	assert(vtProp.vt == VT_I4);
	if (vtProp.vt != VT_I4)
		goto Exit;

	int idx = vtProp.lVal;

	hr = pSvc->GetObject(
		bstr_t("Win32_NetworkAdapterConfiguration"),
		WBEM_FLAG_RETURN_WBEM_COMPLETE,
		NULL,
		&pAdapterClassObj,
		NULL);
	if (FAILED(hr))
		goto Exit;

	hr = pAdapterClassObj->GetMethod(methodName, 0, &pSetDNSServerSearchOrderIn, NULL);
	if (FAILED(hr))
		goto Exit;

	hr = pAdapterClassObj->SpawnInstance(0, &pAdapterClassInst);
	if (FAILED(hr))
		goto Exit;

	if (srv1)
		++srvCount;
	if (srv2)
		++srvCount;
	if (0 == srvCount)
		return;

	parrayBound.cElements = srvCount;
	parrayBound.lLbound = 0;
	serverList.vt = VT_ARRAY | VT_BSTR;
	if (0 == srvCount) {
		serverList.parray = NULL;
	} else {
		serverList.parray = ::SafeArrayCreateEx(VT_BSTR, 1, &parrayBound, NULL);

		BSTR* dnsServerAddress;
		::SafeArrayAccessData(serverList.parray, reinterpret_cast<LPVOID*>(&dnsServerAddress));
		int index = 0;
		if (srv1)
		{
			dnsServerAddress[index] = ::SysAllocString(srv1);
			index++;
		}
		if (srv2) 
		{
			dnsServerAddress[index] = ::SysAllocString(srv2);
			index++;
		}
		::SafeArrayUnaccessData(serverList.parray);
	}

	hr = pAdapterClassObj->Put(bstr_t("DNSServerSearchOrder"), 0, &serverList, 0);
	if (FAILED(hr))
		goto Exit;

	methodPath.Format("Win32_NetworkAdapterConfiguration.Index='%d'", (int)idx);
	methodPath.SetSysString(&methodPathBstr);
	hr = pSvc->ExecMethod(methodPathBstr, methodName, 0, NULL, pSetDNSServerSearchOrderIn, &pSetDNSServerSearchOrderOut, NULL);
	if (FAILED(hr))
		goto Exit;
Exit:
	if (pAdapterClassObj) pAdapterClassObj->Release();
	if (pAdapterClassInst) pAdapterClassInst->Release();
	if (pSetDNSServerSearchOrderOut) pSetDNSServerSearchOrderOut->Release();
	if (pSetDNSServerSearchOrderIn) pSetDNSServerSearchOrderIn->Release();
	::SysFreeString(methodName);
	::SysFreeString(methodPathBstr);
}

static void ClearDnsServers(IWbemServices *pSvc, IWbemClassObject *pObj)
{
	SetDnsServers(pSvc, pObj, NULL, NULL);
}

// We don't want to change the DNS settings of VirtualBox/VMWare etc. virtual ethernet adapters
// I wish I knew a better heurstic to filter non-physical adapters
static BOOL ShouldSkipNetworkAdapter(WCHAR *caption)
{
	// virtual adapter from VirtualBox
	if (-1 != WStrFind(caption, L"VirtualBox"))
		return TRUE;
	// virtual adapter from coLinux (http://colinux.wikia.com/wiki/TAP-Win32_Adapter_V8_%28coLinux%29)
	if (-1 != WStrFind(caption, L"TAP-Win32"))
		return TRUE;
	// virtual adapter from VMWare
	if (-1 != WStrFind(caption, L"VMware"))
		return TRUE;
	return FALSE;
}

void SetOpenDnsServersOnAllAdapters()
{
	IWbemLocator *pLoc = NULL;
	IWbemServices *pSvc = NULL;
	IEnumWbemClassObject* pEnumerator = NULL;

	HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED); 
	if (FAILED(hr))
		return;

	hr =  CoInitializeSecurity(
		NULL, 
		-1,                          // COM authentication
		NULL,                        // Authentication services
		NULL,                        // Reserved
		RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
		RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
		NULL,                        // Authentication info
		EOAC_NONE,                   // Additional capabilities 
		NULL                         // Reserved
	);

	if (FAILED(hr))
		goto Exit;

	hr = CoCreateInstance(
		CLSID_WbemLocator,             
		0, 
		CLSCTX_INPROC_SERVER, 
		IID_IWbemLocator, (LPVOID *) &pLoc);
 
	if (FAILED(hr))
		goto Exit;

	// Connect to the root\cimv2 namespace with
	// the current user and obtain pointer pSvc
	// to make IWbemServices calls.
	hr = pLoc->ConnectServer(
		 bstr_t(L"ROOT\\CIMV2"), // Object path of WMI namespace
		 NULL,					  // User name. NULL = current user
		 NULL,					  // User password. NULL = current
		 0, 					  // Locale. NULL indicates current
		 NULL,					  // Security flags.
		 0, 					  // Authority (e.g. Kerberos)
		 0, 					  // Context object 
		 &pSvc					  // pointer to IWbemServices proxy
		 );

	if (FAILED(hr))
		goto Exit;

	// Set security levels on the proxy
	hr = CoSetProxyBlanket(
		pSvc,						// Indicates the proxy to set
		RPC_C_AUTHN_WINNT,			// RPC_C_AUTHN_xxx
		RPC_C_AUTHZ_NONE,			// RPC_C_AUTHZ_xxx
		NULL,						// Server principal name 
		RPC_C_AUTHN_LEVEL_CALL,		// RPC_C_AUTHN_LEVEL_xxx 
		RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
		NULL,						// client identity
		EOAC_NONE					// proxy capabilities 
	);

	if (FAILED(hr))
		goto Exit;

	hr = pSvc->ExecQuery(
		bstr_t("WQL"), 
		bstr_t("SELECT * FROM Win32_NetworkAdapterConfiguration"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, 
		NULL,
		&pEnumerator);

	if (FAILED(hr))
		goto Exit;

	if (!pEnumerator)
		goto Exit;

	IWbemClassObject *pObj;
	ULONG result = 0;

	for (;;)
	{
		VARIANT vtProp;
		WCHAR *s;
		VariantInit(&vtProp);
		hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &result);
		if (0 == result)
			break;

		hr = pObj->Get(L"IPEnabled", 0, &vtProp, 0, 0);
		if (FAILED(hr) || !vtProp.boolVal)
			goto Next;

#if 0
		VariantClear(&vtProp);
		hr = pObj->Get(L"SettingID", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
		s = vtProp.bstrVal;
#endif

		VariantClear(&vtProp);
		hr = pObj->Get(L"Caption", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
		s = vtProp.bstrVal;

		if (ShouldSkipNetworkAdapter(s))
			goto Next;

#if 0
		VariantClear(&vtProp);
		hr = pObj->Get(L"DNSServerSearchOrder", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
#endif

		SetDnsServers(pSvc, pObj, OLESTR("208.67.222.222"), OLESTR("208.67.220.220 "));
Next:
		VariantClear(&vtProp);
		pObj->Release();
	}

Exit:
	if (pEnumerator) pEnumerator->Release();
	if (pSvc) pSvc->Release();
	if (pLoc) pLoc->Release();

	CoUninitialize();
	return;
}

