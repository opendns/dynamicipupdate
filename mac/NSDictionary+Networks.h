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
