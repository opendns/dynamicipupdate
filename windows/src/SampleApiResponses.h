// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLE_API_RESPONSES_H__
#define SAMPLE_API_RESPONSES_H__

#define AUTH_OK "{\"status\":\"success\",\"response\":{\"token\":\"DCE15D01E430D8C96D3920FB8F64185C\"}}"
#define BAD_API_KEY "{\"status\":\"failure\",\"error\":1002,\"error_message\":\"Unknown API key\"}"
#define BAD_PWD "{\"status\":\"failure\",\"error\":1004,\"error_message\":\" -- bad username\\/password\"}"

#define ONE_NETWORK_NOT_DYNAMIC "{\"status\":\"success\",\"response\":{\"668257\":{\"dynamic\":false,\"label\":null,\"ip_address\":\"67.215.69.50\"}}}"
#define ONE_NETWORK_DYNAMIC "{\"status\":\"success\",\"response\":{\"668258\":{\"dynamic\":true,\"label\":null,\"ip_address\":\"67.215.69.51\"}}}"
#define MULTIPLE_NETWORKS "{\"status\":\"success\",\"response\":{\"668261\":{\"dynamic\":false,\"label\":null,\"ip_address\":\"67.215.69.57\"},\"668262\":{\"dynamic\":false,\"label\":null,\"ip_address\":\"67.215.69.59\"},\"668263\":{\"dynamic\":false,\"label\":null,\"ip_address\":\"67.215.69.60\"},\"668260\":{\"dynamic\":true,\"label\":\"home\",\"ip_address\":\"67.216.69.55\"},\"668259\":{\"dynamic\":true,\"label\":\"office\",\"ip_address\":\"67.215.69.52\"}}}"

#endif
