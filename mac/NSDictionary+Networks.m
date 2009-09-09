// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "NSDictionary+Networks.h"

static BOOL isDynamicNetwork(NSDictionary *network) {
    id isDynamic = [network objectForKey:@"dynamic"];
    if (!isDynamic)
        return NO;
    BOOL val = [isDynamic boolValue];
    return val;    
}

static BOOL isLabledDynamicNetwork(NSDictionary *network) {
    if (!isDynamicNetwork(network))
        return NO;
    id label = [network objectForKey:@"label"];
    if ([label isKindOfClass:[NSString class]])
        return YES;
    return NO;
}

NSArray *labeledDynamicNetworks(NSDictionary *networksDict) {
    NSArray *networks = [networksDict allValues];
    NSMutableArray *res = [NSMutableArray arrayWithCapacity:8];
    unsigned count = [networks count];
    for (unsigned i = 0; i < count; i++) {
        NSDictionary *network = [networks objectAtIndex:i];
        if (isLabledDynamicNetwork(network)) {
            [res addObject:network];
        }
    }
    return res;    
}

@implementation NSDictionary (DynamicNetworks)

- (BOOL)isNetworkDynamic:(NSDictionary*)network {
    id isDynamic = [network objectForKey:@"dynamic"];
    if (!isDynamic)
        return NO;
    BOOL val = [isDynamic boolValue];
    return val;
}

- (unsigned)dynamicNetworksCount {
    unsigned dynamicCount = 0;
    NSArray *networks = [self allValues];
    unsigned count = [networks count];
    for (unsigned i = 0; i < count; i++) {
        NSDictionary *network = [networks objectAtIndex:i];
        if ([self isNetworkDynamic:network])
            dynamicCount += 1;
    }
    return dynamicCount;
}

- (NSDictionary*)findFirstDynamicNetwork {
    NSArray *networks = [self allValues];
    unsigned count = [networks count];
    for (unsigned i = 0; i < count; i++) {
        NSDictionary *network = [networks objectAtIndex:i];
        if ([self isNetworkDynamic:network])
            return network;
    }
    return nil;
}

- (NSDictionary*)dynamicNetworkWithLabel:(NSString*)aLabel {
    NSArray *networks = [self allValues];
    unsigned count = [networks count];
    for (unsigned i = 0; i < count; i++) {
        NSDictionary *network = [networks objectAtIndex:i];
        if ([self isNetworkDynamic:network]) {
            NSString* label = [network objectForKey:@"label"];
            if (!label || ![label isKindOfClass:[NSString class]])
                continue;
            if ([label isEqualToString:aLabel])
                return network;
        }
    }
    return nil;
}

- (NSDictionary*)dynamicNetworkAtIndex:(unsigned)idx {
    NSArray *networks = [self allValues];
    unsigned count = [networks count];
    int currIdx = 0;
    for (unsigned i = 0; i < count; i++) {
        NSDictionary *network = [networks objectAtIndex:i];
        if ([self isNetworkDynamic:network]) {
            if (idx == currIdx)
                return network;
            else
                currIdx += 1;
        }
    }
    return nil;
}

- (NSDictionary*)firstNetwork {
    NSArray *networks = [self allValues];
    unsigned count = [networks count];
    if (0 == count)
        return nil;
    return [networks objectAtIndex:0];    
}
@end

