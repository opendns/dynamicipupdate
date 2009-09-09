// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DNS_QUERY_H__
#define DNS_QUERY_H__

#include <windns.h>
enum {
	DNS_QUERY_OK,
	DNS_QUERY_NO_A_RECORD,
	DNS_QUERY_ERROR
};

int dns_query(const char *nameAscii, IP4_ADDRESS *ip4ut);

#endif
