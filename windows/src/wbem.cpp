// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "wbem.h"

#include "MiscUtil.h"
#include "SimpleLog.h"
#include "StrUtil.h"

static void SafeArrayDestroyAll(SAFEARRAY **arr)
{
	SafeArrayDestroyData(*arr);
	SafeArrayDestroyDescriptor(*arr);
	*arr = NULL;
}

static SAFEARRAY *CreateDnsArray(OLECHAR *dns1, OLECHAR *dns2)
{
	int				srvCount = 0;
	SAFEARRAYBOUND	arrayBound;
	SAFEARRAY *		dnsArray = NULL;
	BSTR *			dnsServerAddress;

	if (dns1) ++srvCount;
	if (dns2) ++srvCount;

	if (0 == srvCount)
		return NULL;

	arrayBound.cElements = srvCount;
	arrayBound.lLbound = 0;
	dnsArray = ::SafeArrayCreateEx(VT_BSTR, 1, &arrayBound, NULL);
	if (!dnsArray)
		return NULL;

	::SafeArrayAccessData(dnsArray, reinterpret_cast<LPVOID*>(&dnsServerAddress));

	int index = 0;
	if (dns1)
		dnsServerAddress[index++] = ::SysAllocString(dns1);

	if (dns2) 
		dnsServerAddress[index] = ::SysAllocString(dns2);

	::SafeArrayUnaccessData(dnsArray);
	return dnsArray;
}

static void SetDnsServers(IWbemServices *wmiSvc, int adapterIdx, OLECHAR *dns1, OLECHAR *dns2)
{
	IWbemClassObject *	pObject = NULL;
	IWbemClassObject *	pInstance = NULL;
	IWbemClassObject *	pMethodParam = NULL;
	IWbemClassObject *	pResult = NULL;
	HRESULT				hr;
	VARIANT				vtDnsServers;
	VARIANT				vtRetVal;
	CString				adapterPath;
	BSTR				adapterPathBstr = NULL;
	SAFEARRAY *			dnsServers = NULL;
	int					retVal;

	VariantInit(&vtDnsServers);
	VariantInit(&vtRetVal);

	hr = wmiSvc->GetObject(bstr_t("Win32_NetworkAdapterConfiguration"), 0, NULL, &pObject, NULL);
	if (FAILED(hr))
		goto Exit;

#if 0 // TODO: not sure if I need this, seems to work without it
	hr = pObject->GetMethod(bstr_t("SetDNSServerSearchOrder"), 0, &pMethodParam, NULL);
	if (FAILED(hr))
		goto Exit;

    hr = pMethodParam->SpawnInstance(0, &pInstance);
	if (FAILED(hr))
		goto Exit;
#endif

	dnsServers = CreateDnsArray(dns1, dns2);
	if (!dnsServers)
		goto Exit;

	vtDnsServers.vt = VT_ARRAY | VT_BSTR;
	vtDnsServers.parray = dnsServers;
	hr = pObject->Put(L"DNSServerSearchOrder", 0, &vtDnsServers, 0);
	if (FAILED(hr))
		goto Exit;

	adapterPath.Format(_T("Win32_NetworkAdapterConfiguration.Index='%d'"), adapterIdx);
	adapterPath.SetSysString(&adapterPathBstr);
	hr = wmiSvc->ExecMethod(adapterPathBstr, bstr_t("SetDNSServerSearchOrder"), 0, NULL, pObject, &pResult, NULL);
	if (FAILED(hr))
		goto Exit;

	hr = pResult->Get(L"ReturnValue", 0, &vtRetVal, NULL, 0);
	if (FAILED(hr))
		goto Exit;

	retVal = vtRetVal.intVal;
	slogfmt("Set dns servers on adapter %d, retVal=%d", adapterIdx, retVal);

Exit:
	VariantClear(&vtDnsServers);
	VariantClear(&vtRetVal);
	if (pObject) pObject->Release();
	if (pInstance) pInstance->Release();
	if (pMethodParam) pMethodParam->Release();
	if (pResult) pResult->Release();
	if (dnsServers) SafeArrayDestroyAll(&dnsServers);
	::SysFreeString(adapterPathBstr);
}

static void ClearDnsServers(IWbemServices *pSvc, int adapterIdx)
{
	SetDnsServers(pSvc, adapterIdx, NULL, NULL);
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
		VARIANT vtCaption;
		WCHAR *caption = NULL;

		VariantInit(&vtProp);
		VariantInit(&vtCaption);

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

		hr = pObj->Get(L"Caption", 0, &vtCaption, 0, 0);
		if (FAILED(hr))
			goto Next;
		caption = vtCaption.bstrVal;

		if (ShouldSkipNetworkAdapter(caption))
			goto Next;

		// TODO: skip disabled adapters (probably needs to use
		// Win32_NetworkAdapter.Availability correlated via
		// MACAddress with Win32_NetworkAdapterConfiguration)

#if 0
		VariantClear(&vtProp);
		hr = pObj->Get(L"DNSServerSearchOrder", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Next;
#endif

		VariantClear(&vtProp);
		hr = pObj->Get(L"Index", 0, &vtProp, 0, 0);
		if (FAILED(hr))
			goto Exit;
		assert(vtProp.vt == VT_I4);
		if (vtProp.vt != VT_I4)
			goto Exit;
		int idx = vtProp.lVal;

		SetDnsServers(pSvc, idx, OLESTR("208.67.222.222"), OLESTR("208.67.220.220"));
Next:
		VariantClear(&vtProp);
		VariantClear(&vtCaption);
		pObj->Release();
	}

Exit:
	if (pEnumerator) pEnumerator->Release();
	if (pSvc) pSvc->Release();
	if (pLoc) pLoc->Release();

	CoUninitialize();
	return;
}

