// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "dnsquery.h"
#include "MiscUtil.h"
#include "StrUtil.h"

int dns_query(const char *nameAscii, IP4_ADDRESS *ip4ut)
{
	PDNS_RECORD records, cursor;
	TCHAR *name = StrToTStr(nameAscii);
	AutoFree nameAutoFree(name);
	DNS_STATUS dnsStatus;
	dnsStatus = DnsQuery(name, DNS_TYPE_A, 
			DNS_QUERY_BYPASS_CACHE | DNS_QUERY_TREAT_AS_FQDN, 
			NULL, &records, NULL);

	if (DNS_ERROR_RCODE_NAME_ERROR == dnsStatus)
		return DNS_QUERY_NO_A_RECORD;

	if (ERROR_SUCCESS != dnsStatus)
		return DNS_QUERY_ERROR;

	for (cursor = records; cursor != NULL; cursor = cursor->pNext) {

		if (cursor->Flags.S.Section != DnsSectionAnswer)
			continue;

		if (DnsNameCompare(cursor->pName, name)) {
			if (cursor->wType == DNS_TYPE_A) {
				IP4_ADDRESS a;
				INLINE_HTONL(a, cursor->Data.A.IpAddress);
				*ip4ut = a;
				break;
			} else 	if (cursor->wType == DNS_TYPE_CNAME) {
				name = cursor->Data.CNAME.pNameHost;
			}
		}
	}
	DnsRecordListFree(records, DnsFreeRecordList);
	if (NULL == cursor)
		return DNS_QUERY_NO_A_RECORD;
	return DNS_QUERY_OK;
}
