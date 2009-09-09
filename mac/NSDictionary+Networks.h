// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

NSArray *labeledDynamicNetworks(NSDictionary *networksDict);

@interface NSDictionary (DynamicNetworks)

- (BOOL)isNetworkDynamic:(NSDictionary*)network;
- (unsigned)dynamicNetworksCount;
- (NSDictionary*)findFirstDynamicNetwork;
- (NSDictionary*)dynamicNetworkAtIndex:(unsigned)idx;
- (NSDictionary*)dynamicNetworkWithLabel:(NSString*)aLabel;
- (NSDictionary*)firstNetwork;

@end
