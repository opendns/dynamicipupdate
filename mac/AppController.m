// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppController.h"
#import "SBJSON.h"
#import "GDataHTTPFetcher.h"
#import "ApiKey.h"
#import "LoginItemsAE.h"

#include <netdb.h>

#define API_HOST @"https://api.opendns.com/v1/"
#define IP_UPDATE_HOST @"https://updates.opendns.com"

#define TIME_INTERVAL_ONE_MINUTE 60.0
#define TIME_INTERVAL_3HR 60.0*60.0*3.0

NSString * PREF_ACCOUNT = @"account";
NSString * PREF_TOKEN = @"token";
NSString * PREF_HOSTNAME = @"hostname";
NSString * PREF_SEND_UPDATES = @"sendUpdates";
NSString * PREF_USER_NETWORKS_STATE = @"networksState";
NSString * PREF_UNIQUE_ID = @"uniqueId";

// selected a network or we're using a default network
NSString * UNS_OK = @"unsok";
// doesn't have any networks
NSString * UNS_NO_NETWORKS = @"unsnonet";
// has networks but none of them is configured for dynamic ips
NSString * UNS_NO_DYNAMIC_IP_NETWORKS = @"unsnodynip";
// has networks but didn't select any yet
NSString * UNS_NO_NETWORK_SELECTED = @"unnonetsel";

@interface AppController (Private)

- (NSString *)getMyIp;
- (void)ipChangeThread;
- (void)setButtonLoginStatus;
- (BOOL)isButtonLoginEnabled;
- (NSString*)apiSignInStringForAccount:(NSString*)userName withPassword:(NSString*)password;
- (NSString*)apiGetNetworksStringForToken:(NSString*)token;
- (void)showLoginError;
- (void)showLoginWindow;
- (void)downloadNetworks:(NSString*)token suppressUI:(BOOL)suppressUI;
- (BOOL)noNetworksConfigured;
- (BOOL)isLoggedIn;
- (void)updateStatusWindow;
- (NSString*)tokenFromSignInJsonResponse:(NSData*)data;

@end

@interface NSDictionary (DynamicNetworks)

- (BOOL)isNetworkDynamic:(NSDictionary*)network;
- (unsigned)dynamicNetworksCount;
- (NSDictionary*)findFirstDynamicNetwork;
- (NSDictionary*)dynamicNetworkAtIndex:(unsigned)idx;
- (NSDictionary*)dynamicNetworkWithLabel:(NSString*)aLabel;

@end

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

static NSArray *labeledDynamicNetworks(NSDictionary *networksDict) {
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

@end

@interface NSTableDataSourceDynamicNetworks : NSObject {
    NSArray *networks_;
}

- (id)initWithNetworks:(NSArray*)networks;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
@end

@implementation NSTableDataSourceDynamicNetworks

- (id)initWithNetworks:(NSArray*)networks {
    networks_ = [networks retain];
    return self;
}

- (void)dealloc {
    [networks_ release];
    [super dealloc];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex {
    NSDictionary * network = [networks_ objectAtIndex:(unsigned)rowIndex];
    NSString *hostname = [network objectForKey:@"label"];
    return hostname;
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView {
    return (int)[networks_ count];
}

@end

static BOOL NSStringsEqual(NSString *s1, NSString *s2) {
    if (!s1 && !s2)
        return YES;
    if (!s1 || !s2)
        return NO;
    return [s1 isEqualToString:s2];
}

@implementation AppController

+ (void)initialize {
    NSMutableDictionary *defaultValues = [NSMutableDictionary dictionary];
    [defaultValues setObject:[NSNumber numberWithBool:YES] forKey:PREF_SEND_UPDATES];
    [[NSUserDefaults standardUserDefaults] registerDefaults: defaultValues];
}

- (NSString*)apiSignInStringForAccount:(NSString*)account withPassword:(NSString*)password {
    NSString *accountEncoded = [account stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSString *passwordEncoded = [password stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSString *url = [NSString stringWithFormat:@"api_key=%@&method=account_signin&username=%@&password=%@", API_KEY, accountEncoded, passwordEncoded];
    return url;
}

- (NSString*)apiGetNetworksStringForToken:(NSString*)token {
    NSString *tokenEncoded = [token stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSString *url = [NSString stringWithFormat:@"api_key=%@&method=networks_get&token=%@", API_KEY, tokenEncoded];
    return url;
}

- (NSString*)apiNetworksDynamicSetForNetwork:(NSString*)networkId withToken:token {
    NSString *tokenEncoded = [token stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    NSString *url = [NSString stringWithFormat:@"api_key=%@&method=network_dynamic_set&token=%@&network_id=%@&setting=on", API_KEY, tokenEncoded, networkId];
    return url;    
}

- (NSString*)apiIpUpdateForToken:(NSString*)token andHostname:(NSString*)hostname {
    NSString *tokenEncoded = [token stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
    if (!hostname)
        hostname = @"";
    NSString *url = [NSString stringWithFormat:@"%@/nic/update?token=%@&api_key=%@&v=2&hostname=%@", IP_UPDATE_HOST, tokenEncoded, API_KEY, hostname];
    return url;    
}

- (NSString *)getMyIp {
    char **addrs;
    struct hostent *he = gethostbyname("myip.opendns.com");
    if (!he)
        return nil;

    // TODO: notify the user that we don't support ipv6?
    if (AF_INET != he->h_addrtype)
            return nil;
    if (4 != he->h_length)
            return nil;
    addrs = he->h_addr_list;
    while (*addrs) {
        unsigned char *a = (unsigned char*)*addrs++;
        // TODO: could by more efficient by comparing old vs. new as bytes
        // and only creating NSString when are different
        NSString *addrTxt = [NSString stringWithFormat:@"%d.%d.%d.%d", a[0], a[1], a[2], a[3]];
        return addrTxt;
    }
    return nil;
}

- (BOOL)canSendIPUpdates {
    if (![self isLoggedIn])
        return NO;
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSNumber *shouldSend = [prefs objectForKey:PREF_SEND_UPDATES];
    if (NO == [shouldSend boolValue])
        return NO;
    NSString *networkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    return [networkState isEqualToString:UNS_OK];
}

- (BOOL)shouldSendPeriodicUpdate {
    if (![self canSendIPUpdates])
        return NO;

    if (forceNextUpdate_) {
        forceNextUpdate_ = NO;
        return YES;
    }

    NSDate *now = [NSDate date];
    if ([now compare:nextIpUpdate_] == NSOrderedAscending)
        return NO;

    return YES;
}

// update minutesSinceLastIpUpdate_. Return YES if changed
- (BOOL)updateLastIpUpdateTime {
    if (!lastIpUpdateTime_)
        return NO;
    NSTimeInterval seconds = -[lastIpUpdateTime_ timeIntervalSinceNow];
    int currMinutesSinceLastUpdate = (int)(seconds / 60.0);
    if (currMinutesSinceLastUpdate == minutesSinceLastIpUpdate_)
        return NO;
    minutesSinceLastIpUpdate_ = currMinutesSinceLastUpdate;
    return YES;
}

- (void)scheduleNextIpUpdate {
    // schedule next ip update 3 hours from now
    [nextIpUpdate_ release];
    nextIpUpdate_ = [[NSDate dateWithTimeIntervalSinceNow:TIME_INTERVAL_3HR] retain];
    [lastIpUpdateTime_ release];
    lastIpUpdateTime_ = [[NSDate date] retain];
}

- (IpUpdateResult)ipUpdateResultFromString:(NSString*)s {
    
    if ([s hasPrefix:@"The service is not available"])
        return IpUpdateNotAvailable;
    if ([s hasPrefix:@"good"])
        return IpUpdateOk;
    if ([s hasPrefix:@"!yours"])
        return IpUpdateNotYours;
    if ([s hasPrefix:@"badauth"])
        return IpUpdateBadAuth;
    if ([s hasPrefix:@"nohost"])
        return IpUpdateBadAuth;
    assert(0);
    return IpUpdateOk;
}

- (void)ipUpdateFetcher:(GDataHTTPFetcher *)fetcher finishedWithData:(NSData *)retrievedData {
    NSString *s = [[NSString alloc] initWithData:retrievedData encoding:NSUTF8StringEncoding];

    IpUpdateResult ipUpdateResult = [self ipUpdateResultFromString:s];
    if (IpUpdateNotAvailable == ipUpdateResult)
            return;

    // TODO: this might happen if a user made a network non-dynamic behind our back
    // not sure what to do in this case - re-download networks?
    if (IpUpdateNoHost == ipUpdateResult)
        return;

    if ((IpUpdateOk == ipUpdateResult) || (IpUpdateNotYours == ipUpdateResult)) {
        /* TODO: port this
        const char *ip = StrFindChar(ipUpdateRes, ' ');
        if (ip)
            m_ipFromHttp = StrToTStr(ip+1);
         */
    }
    
    if (ipUpdateResult == ipUpdateResult_)
         return;

    ipUpdateResult_ = ipUpdateResult;
    [self updateStatusWindow];
    if (ipUpdateResult != IpUpdateOk)
        [self showStatusWindow:self];
}

- (void)ipUpdateFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error {
    // silently ignore
    NSLog(@"ip update failed");
}

- (void)sendPeriodicUpdate {
    NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];
    NSString *hostname = [prefs objectForKey:PREF_HOSTNAME];
    NSString *token = [prefs objectForKey:PREF_TOKEN];
    NSString *urlString = [self apiIpUpdateForToken:token andHostname:hostname];
    NSURL *url = [NSURL URLWithString:urlString];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    GDataHTTPFetcher* fetcher = [GDataHTTPFetcher httpFetcherWithRequest:request];
    [fetcher beginFetchWithDelegate:self
                  didFinishSelector:@selector(ipUpdateFetcher:finishedWithData:)
                    didFailSelector:@selector(ipUpdateFetcher:failedWithError:)];

    [self scheduleNextIpUpdate];
}

- (void)ipAddressCheckAndPeriodicIpUpdate:(id)dummy {
    BOOL updateStatus = NO;
    NSString *newIp = [self getMyIp];
    if (!NSStringsEqual(newIp, currentIpAddressFromDns_)) {
        [currentIpAddressFromDns_ release];
        currentIpAddressFromDns_ = [newIp copy];
        if (newIp) {
            usingOpenDns_ = YES;
        } else {
            usingOpenDns_ = NO;
        }
        forceNextUpdate_ = YES;
        updateStatus = YES;
    }
    
    if ([self shouldSendPeriodicUpdate]) {
        [self sendPeriodicUpdate];
    }
    if ([self updateLastIpUpdateTime])
        updateStatus = YES;
    
    if (updateStatus)
        [self updateStatusWindow];
}

- (void)ipChangeThread {
    NSAutoreleasePool* myAutoreleasePool = [[NSAutoreleasePool alloc] init];
    while (!exitIpChangeThread_) {
        [self performSelectorOnMainThread:@selector(ipAddressCheckAndPeriodicIpUpdate:) withObject:nil waitUntilDone:YES];
        NSDate *inOneMinute = [[NSDate date] addTimeInterval:TIME_INTERVAL_ONE_MINUTE];
        [NSThread sleepUntilDate:inOneMinute];
        [myAutoreleasePool drain];
    }
    [myAutoreleasePool release];
}

// equivalent of  echo "${s}" | openssl enc -bf -d -pass pass:"NojkPqnbK8vwmaJWVnwUq" -salt -a
- (NSString*)decryptString:(NSString*)s {
    NSTask *task;
    task = [[NSTask alloc] init];
    [task setLaunchPath: @"/bin/sh"];
    NSString *shellArg = [NSString stringWithFormat:@"echo \"%@\" | openssl enc -bf -d -pass pass:\"NojkPqnbK8vwmaJWVnwUq\" -salt -a", s];
    NSArray *arguments;
    arguments = [NSArray arrayWithObjects: @"-c", shellArg, nil];
    [task setArguments: arguments];
    NSPipe *pipe = [NSPipe pipe];
    [task setStandardOutput: pipe];
    NSFileHandle *file = [pipe fileHandleForReading];
    [task launch];
    NSData *data = [file readDataToEndOfFile];
    NSString *string = [[NSString alloc] initWithData: data
                                   encoding: NSUTF8StringEncoding];
    string = [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
    return string;
}

- (NSString*)generateUniqueId {
    CFUUIDRef uuidRef = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef sref = CFUUIDCreateString(kCFAllocatorDefault, uuidRef);
    CFRelease(uuidRef);
    NSString *s = (NSString*)sref;
    return [s autorelease];
}

- (void)generateUniqueIdIfNotExists {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *uuid = [prefs objectForKey:PREF_UNIQUE_ID];
    if (uuid)
        return;
    uuid = [self generateUniqueId];
    [prefs setObject:uuid forKey:PREF_UNIQUE_ID];
}

#if 0
NetworkInfo *MakeFirstNetworkDynamic(NetworkInfo *ni)
{
    JsonEl *json = NULL;
    HttpResult *httpRes = NULL;
    char *jsonTxt = NULL;

    char *networkId = ni->internalId;
    CString params = ApiParamsNetworkDynamicSet(g_pref_token, networkId, true);
    const char *paramsTxt = TStrToStr(params);
    const char *apiHost = GetApiHost();
    bool apiHostIsHttps = IsApiHostHttps();
    httpRes = HttpPost(apiHost, API_URL, paramsTxt, apiHostIsHttps);
    free((void*)paramsTxt);
    if (!httpRes || !httpRes->IsValid())
        goto Error;
    
    DWORD dataSize;
    jsonTxt = (char *)httpRes->data.getData(&dataSize);
    if (!jsonTxt)
        goto Error;
    
    json = ParseJsonToDoc(jsonTxt);
    if (!json)
        goto Error;
    WebApiStatus status = GetApiStatus(json);
    if (WebApiStatusSuccess != status)
        goto Error;
    
Exit:
    JsonElFree(json);
    delete httpRes;
    return ni;
Error:
    ni = NULL;
    goto Exit;
}
#endif

- (NSData*)apiHostPost:(NSString*)apiString {
    NSURL *url = [NSURL URLWithString:API_HOST];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:[apiString dataUsingEncoding:NSUTF8StringEncoding]];
    NSURLResponse *resp = nil;
    NSError *err = nil;
    NSData *result = [NSURLConnection sendSynchronousRequest:request 
                                           returningResponse:&resp 
                                                       error:&err];
    if (err)
        return nil;
    return result;
}

- (NSDictionary*)makeFirstNetworkDynamic:(NSDictionary*)networks withToken:(NSString*)token {
    NSDictionary *dynamicNetwork = [networks findFirstDynamicNetwork];
    assert(!dynamicNetwork);
    if (dynamicNetwork)
        return dynamicNetwork;

    NSString *internalId = [dynamicNetwork objectForKey:@"internalId"];
    if (!internalId || !([internalId isKindOfClass:[NSString class]]) || (0 == [internalId length]))
        return nil;

    NSString *apiString = [self apiNetworksDynamicSetForNetwork:internalId withToken:token];
    NSData *result = [self apiHostPost:apiString];
    if (!result)
        return nil;
    return nil;
}

// very similar to downloadNetworks and getNetworksFetcher:
- (void)verifyHostname:(NSString*)hostname withToken:(NSString*)token {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSDictionary *dynamicNetwork = nil;
    NSString *apiString = [self apiGetNetworksStringForToken:token];
    NSData *result = [self apiHostPost:apiString];
    if (!result)
        return;

    NSString *s = [[NSString alloc] initWithData:result encoding:NSUTF8StringEncoding];
    SBJSON *parser = [[[SBJSON alloc] init] autorelease];
    id json = [parser objectWithString:s];
    if (![json isKindOfClass:[NSDictionary class]])
        return;

    NSString *s2 = [json objectForKey:@"status"];
    if ([s2 isEqualToString:@"failure"]) {
        NSNumber *n = [json objectForKey:@"error"];
        int err = [n intValue];
        if (ERR_NETWORK_DOESNT_EXIST == err)
            goto NoNetworks;
        return;
    }
    if (![s2 isEqualToString:@"success"])
        return;

    NSDictionary *networks = [json objectForKey:@"response"];
    if (!networks || (0 == [networks count]))
        goto NoNetworks;
    
    unsigned dynamicCount = [networks dynamicNetworksCount];
    if (0 == dynamicCount)
        goto NoDynamicNetworks;

    dynamicNetwork = [networks dynamicNetworkWithLabel:hostname];
    if (!dynamicNetwork)
        dynamicNetwork = [networks findFirstDynamicNetwork];
    assert(dynamicNetwork);
    if (!dynamicNetwork)
        goto NoDynamicNetworks;

SetDynamicNetwork:
    hostname = [dynamicNetwork objectForKey:@"label"];
    if (!hostname || ![hostname isKindOfClass:[NSString class]])
        hostname = @"";
    
    [prefs setObject:hostname forKey:PREF_HOSTNAME];
    [prefs setObject:UNS_OK forKey:PREF_USER_NETWORKS_STATE];
    return;

NoNetworks:
    [prefs setObject:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
    return;

NoDynamicNetworks:
    dynamicNetwork = [self makeFirstNetworkDynamic:networks withToken:token];
    if (dynamicNetwork)
        goto SetDynamicNetwork;
    
    [prefs setObject:UNS_NO_DYNAMIC_IP_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
    return;
}

- (void)importOldSettings {
    NSDictionary *settings;
    NSData *settingsData;
    NSString *errorString = nil;
    NSString *path = [@"~/Library/Preferences/com.zweisoft.OpenDNSUpdater.plist" stringByExpandingTildeInPath];
    NSPropertyListFormat format;
    settingsData = [NSData dataWithContentsOfFile:path];
    settings = [NSPropertyListSerialization 
                propertyListFromData:settingsData
                mutabilityOption:NSPropertyListImmutable
                format:&format
                errorDescription:&errorString];
    NSString *username = [settings objectForKey:@"Username"];
    if (!username || (0 == [username length]))
        return;
    NSString *pwd = [settings objectForKey:@"VString"];
    if (!pwd || (0 == [pwd length]))
        return;
    pwd = [self decryptString:pwd];
    NSString *hostname = [settings objectForKey:@"Hostname"];
    //NSLog(@"Old settings: %@, %@, %@", username, pwd, hostname);

    NSString *apiString = [self apiSignInStringForAccount:username withPassword:pwd];
    NSData *result = [self apiHostPost:apiString];
    if (!result)
        return;

    NSString *token = [self tokenFromSignInJsonResponse:result];
    if (!token)
        return;
    
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs setObject:username forKey:PREF_ACCOUNT];
    [prefs setObject:token forKey:PREF_TOKEN];

    [self verifyHostname:hostname withToken:token];
    
    // delete the old preferences file, so that we don't import it multiple times
#if 0
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeFileAtPath:path handler:nil];
#endif
}

// If we're not a login startup item or if we are but at a different path
// (e.g. if the user moved the binary), add our binary as a startup login item
// 10.5 has APIs for that, but no point using them if we have to support
// 10.4 anyway
- (void)makeStartAtLogin {
    NSString *filePath = [[NSBundle mainBundle] bundlePath];
    NSString *fileName = [filePath lastPathComponent];
    NSURL * url = [NSURL fileURLWithPath:filePath];

    CFArrayRef loginItems = NULL;
    OSStatus status = LIAECopyLoginItems (&loginItems);
    if (status != noErr)
        goto Exit;

    NSEnumerator * enumerator = [(NSArray *) loginItems objectEnumerator];
    NSDictionary * loginItemDict;
    
    NSURL *existingUrl;
    NSString *existingPath;
    NSString *existingFileName;
    unsigned alreadyExistsAtIndex = NSNotFound;

    while ((loginItemDict = [enumerator nextObject]))
    {
        existingUrl = [loginItemDict objectForKey:(NSString *) kLIAEURL];
        existingPath = [existingUrl path];
        existingFileName = [existingPath lastPathComponent];
        if ([fileName isEqualToString:existingFileName])
        {
            alreadyExistsAtIndex = [(NSArray *) loginItems indexOfObjectIdenticalTo:loginItemDict];
            break;
        }
    }

    if (alreadyExistsAtIndex != NSNotFound) {
        // if it's exactly the same, nothing futher to do
        if ([existingUrl isEqualTo:url])
            goto Exit;
        LIAERemove(alreadyExistsAtIndex);
    }

    Boolean hideIt = true; // not sure if it makes a difference
    LIAEAddURLAtEnd((CFURLRef)url, hideIt);

Exit:
    if (loginItems)
        CFRelease(loginItems);
}

- (void)awakeFromNib {
    [self generateUniqueIdIfNotExists];
    [self makeStartAtLogin];
    [self importOldSettings];
    statusItem_ = [[[NSStatusBar systemStatusBar] 
                               statusItemWithLength:NSSquareStatusItemLength]
                              retain];
    [statusItem_ setHighlightMode:YES];
    [statusItem_ setEnabled:YES];
    [statusItem_ setToolTip:@"OpenDNS Updater"];
    [statusItem_ setMenu:menu_]; 

    NSBundle *bundle = [NSBundle bundleForClass:[self class]]; 
    NSString *path = [bundle pathForResource:@"menuicon" ofType:@"tif"]; 
    menuIcon_= [[NSImage alloc] initWithContentsOfFile:path]; 
    [statusItem_ setImage:menuIcon_]; 
    [menuIcon_ release]; 

    exitIpChangeThread_ = NO;
    forceNextUpdate_ = NO;
    ipUpdateResult_ = IpUpdateOk;
    // schedule first update as soon as possible
    nextIpUpdate_ = [[NSDate date] retain];

    [NSThread detachNewThreadSelector:@selector(ipChangeThread)
                             toTarget:(id)self
                           withObject:(id)nil];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs addObserver:self
            forKeyPath:PREF_SEND_UPDATES
               options:NSKeyValueObservingOptionNew
               context:nil];

    NSString *token = [prefs objectForKey: PREF_TOKEN];
    if (![self isLoggedIn]) {
        [self showLoginWindow];
        return;
    }

    if ([self noNetworksConfigured]) {
        [self downloadNetworks:token suppressUI:YES];
        return;
    }
    [self showStatusWindow:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString:PREF_SEND_UPDATES]) {
        [self updateStatusWindow];
        return;
    }
}

- (BOOL)isLoggedIn {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *account = [prefs objectForKey:PREF_ACCOUNT];
    if (!account || (0 == [account length]))
        return NO;
    NSString *token = [prefs objectForKey:PREF_TOKEN];
    if (!token || (0 == [token length]))
        return NO;
    return YES;
}

- (NSString*)formatNum:(int) num withPostfix:(NSString*)postfix {
    if (1 == num) {
        return [NSString stringWithFormat:@"%d %@", num, postfix];
    } else {
        return [NSString stringWithFormat:@"%d %@s", num, postfix];
    }
}

- (NSString*)lastUpdateText {
    int hours = minutesSinceLastIpUpdate_ / 60;
    int minutes = minutesSinceLastIpUpdate_ % 60;
    NSString *s = @"";
    if (hours > 0) {
        s = [self formatNum:hours withPostfix:@"hr"];
        s = [s stringByAppendingString:@" "];
    }
    s = [s stringByAppendingString:[self formatNum:minutes withPostfix:@"minute"]];
    return [s stringByAppendingString:@" ago."];
}

- (BOOL)noNetworksConfigured {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *networkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    return [networkState isEqualToString:UNS_NO_NETWORKS];
}

- (BOOL)noDynamicNetworks {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *networkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    return [networkState isEqualToString:UNS_NO_DYNAMIC_IP_NETWORKS];
}

- (BOOL)networkNotSelected {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *networkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    return [networkState isEqualToString:UNS_NO_NETWORK_SELECTED];
}

- (void)updateStatusWindow {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    BOOL sendingUpdates = [[prefs objectForKey:PREF_SEND_UPDATES] boolValue];
    NSString *account = [prefs objectForKey:PREF_ACCOUNT];
    if ([self isLoggedIn]) {
        [textAccount_ setTitleWithMnemonic:account];
    } else {
        // TODO: change 'Change account' to 'Login' ?
        // TODO: or not allow showing this window if not logged in?
        [textAccount_ setTitleWithMnemonic:@"Not logged in"];
    }

    // TODO: what to do with networks if not logged in?
    if ([self noNetworksConfigured]) {
        [textHostname_ setTextColor:[NSColor redColor]];
        [textHostname_ setTitleWithMnemonic:@"No networks"];
        [buttonChangeNetwork_ setTitle:@"Refresh network list"];
    } else if ([self noDynamicNetworks]) {
        [textHostname_ setTextColor:[NSColor redColor]];
        [textHostname_ setTitleWithMnemonic:@"No dynamic network"];
        [buttonChangeNetwork_ setTitle:@"Select network"];
    } else if ([self networkNotSelected]) {
        [textHostname_ setTextColor:[NSColor redColor]];
        [textHostname_ setTitleWithMnemonic:@"Network not selected"];
        [buttonChangeNetwork_ setTitle:@"Select network"];
    } else {
        [textHostname_ setTextColor:[NSColor blackColor]];
        NSString *hostname = [prefs objectForKey:PREF_HOSTNAME];
        if (0 == [hostname length])
            hostname = @"default";
        [textHostname_ setTitleWithMnemonic:hostname];
        [buttonChangeNetwork_ setTitle:@"Change network"];
    }

    if (currentIpAddressFromDns_)
        [textIpAddress_ setTitleWithMnemonic:currentIpAddressFromDns_];
    else {
        // TODO: show a different text in red?
        [textIpAddress_ setTitleWithMnemonic:@""];
    }

    if (usingOpenDns_) {
        [textUsingOpenDns_ setTextColor:[NSColor blackColor]];
        [textUsingOpenDns_ setTitleWithMnemonic:@"Yes"];
    } else {
        [textUsingOpenDns_ setTextColor:[NSColor redColor]];
        [textUsingOpenDns_ setTitleWithMnemonic:@"No"];
    }

    if (sendingUpdates) {
        [textLastUpdated_ setTextColor:[NSColor blackColor]];
        [textLastUpdated_ setTitleWithMnemonic:[self lastUpdateText]];
        [buttonUpdateNow_ setEnabled:YES];
    } else {
        [textLastUpdated_ setTextColor:[NSColor redColor]];
        [textLastUpdated_ setTitleWithMnemonic:@"Updates disabled"];
        [buttonUpdateNow_ setEnabled:NO];
    }
}

- (void)showStatusWindow:(id)sender {
    [windowLogin_ orderOut:self];
    [windowSelectNetwork_ orderOut:self];
    [self updateStatusWindow];
    [NSApp activateIgnoringOtherApps:YES];
    [windowStatus_ makeKeyAndOrderFront:self];
}

- (void)showLoginWindow {
    if ([self isLoggedIn]) {
        [buttonQuitCancel_ setTitle:@"Cancel"];
    } else {
        [buttonQuitCancel_ setTitle:@"Quit"];
    }
    [editOpenDnsAccount_ setTitleWithMnemonic:@""];
    [editOpenDnsPassword_ setTitleWithMnemonic:@""];
    [progressLogin_ setHidden:YES];
    [textLoginProgress_ setHidden:YES];
    [textLoginError_ setHidden:YES];

    [windowSelectNetwork_ orderOut:self];
    [windowStatus_ orderOut:self];
    [NSApp activateIgnoringOtherApps:YES];
    [windowLogin_ makeKeyAndOrderFront:self];	
}

- (void)showNetworksWindow {
    [windowLogin_ orderOut:self];
    [windowStatus_ orderOut:self];
    [NSApp activateIgnoringOtherApps:YES];
    [windowSelectNetwork_ makeKeyAndOrderFront:self];
}

// don't bother doing anything, it's not being called by runtime anyway
- (void)dealloc {
    [super dealloc];
}

- (BOOL)isButtonLoginEnabled {
    NSString *account = [editOpenDnsAccount_ stringValue];
    NSString *password = [editOpenDnsPassword_ stringValue];
    if (!account || (0 == [account length]))
        return NO;
    if (!password || (0 == [password length]))
        return NO;
    return YES;
}

- (void)setButtonLoginStatus {
    [buttonLogin_ setEnabled:[self isButtonLoginEnabled]];
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
    [self setButtonLoginStatus];
}

- (void)getNetworksFetcher:(GDataHTTPFetcher *)fetcher finishedWithData:(NSData *)retrievedData {
    BOOL suppressUI = NO;
    NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];

    if (nil != [fetcher userData])
        suppressUI = YES;

    NSString *s = [[NSString alloc] initWithData:retrievedData encoding:NSUTF8StringEncoding];
    SBJSON *parser = [[[SBJSON alloc] init] autorelease];
    id json = [parser objectWithString:s];
    if (![json isKindOfClass:[NSDictionary class]])
        goto Error;

    NSString *s2 = [json objectForKey:@"status"];
    if ([s2 isEqualToString:@"failure"]) {
        NSNumber *n = [json objectForKey:@"error"];
        int err = [n intValue];
        if (ERR_NETWORK_DOESNT_EXIST == err)
            goto NoNetworks;
        goto Error;
    }
    if (![s2 isEqualToString:@"success"])
        goto Error;

    NSDictionary *networks = [json objectForKey:@"response"];
    if (!networks)
        goto NoNetworks;

    if (0 == [networks count])
        goto NoNetworks;
	
    unsigned dynamicCount = [networks dynamicNetworksCount];
    if (0 == dynamicCount)
            goto NoDynamicNetworks;
    NSDictionary *dynamicNetwork = [networks findFirstDynamicNetwork];
    if (1 == dynamicCount) {
        NSString *hostname = [dynamicNetwork objectForKey:@"label"];
        if (!hostname || ![hostname isKindOfClass:[NSString class]])
            hostname = @"";

        [prefs setObject:hostname forKey:PREF_HOSTNAME];
        [prefs setObject:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
        goto ShowStatusWindow;
    }

    NSArray *dynamicNetworks = labeledDynamicNetworks(networks);
    NSTableDataSourceDynamicNetworks *dataSource = [[NSTableDataSourceDynamicNetworks alloc] initWithNetworks:dynamicNetworks];
    [tableNetworksList_ setDataSource:dataSource];
    [tableNetworksList_ setTarget:self];
    [tableNetworksList_ setAction:@selector(selectNetworkClick:)];
    [tableNetworksList_ setDoubleAction:@selector(selectNetworkDoubleClick:)];
    [tableNetworksList_ reloadData];
    [self showNetworksWindow];
    return;

Error:
    NSLog(@"Error");
    return;

NoNetworks:
    [prefs setObject:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
    goto ShowStatusWindow;

NoDynamicNetworks:
    [prefs setObject:UNS_NO_DYNAMIC_IP_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
ShowStatusWindow:
    [self updateStatusWindow];
    [self showStatusWindow:nil];
    return;
}

- (void)getNetworksFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error {
    // TODO: implement me
}

- (void)downloadNetworks:(NSString*)token suppressUI:(BOOL)suppressUI {
    NSString *apiString = [self apiGetNetworksStringForToken:token];
    NSURL *url = [NSURL URLWithString:API_HOST];
    NSURLRequest *request = [NSURLRequest requestWithURL:url];
    GDataHTTPFetcher* fetcher = [GDataHTTPFetcher httpFetcherWithRequest:request];
    [fetcher setPostData:[apiString dataUsingEncoding:NSUTF8StringEncoding]];
    if (suppressUI) {
        // it's only significant that it's there
        [fetcher setUserData:[NSString stringWithString:@"suppress"]];
    }
    [fetcher beginFetchWithDelegate:self
                  didFinishSelector:@selector(getNetworksFetcher:finishedWithData:)
                    didFailSelector:@selector(getNetworksFetcher:failedWithError:)];
}

// extract token from json response to signin method. Returns nil on error.
- (NSString*)tokenFromSignInJsonResponse:(NSData*)jsonData {
    NSString *token = nil;
    NSString *s = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    SBJSON *parser = [[[SBJSON alloc] init] autorelease];
    id json = [parser objectWithString:s];
    if (![json isKindOfClass:[NSDictionary class]])
        goto Error;
    
    NSString *s2 = [json objectForKey:@"status"];
    if (![s2 isEqualToString:@"success"])
        goto Error;
    NSDictionary *response = [json objectForKey:@"response"];
    if (!response)
        goto Error;
    token = [response objectForKey:@"token"];
Error:
    return token;
}

- (void)loginFetcher:(GDataHTTPFetcher *)fetcher finishedWithData:(NSData *)retrievedData {
    [progressLogin_ stopAnimation: nil];
    NSString *token = [self tokenFromSignInJsonResponse:retrievedData];
    if (!token)
        goto Error;
    NSString *account = [editOpenDnsAccount_ stringValue];
    [[NSUserDefaults standardUserDefaults] setObject:token forKey:PREF_TOKEN];
    [[NSUserDefaults standardUserDefaults] setObject:account forKey:PREF_ACCOUNT];
    [self downloadNetworks:token suppressUI:YES];
    return;
Error:
    [self showLoginError];	
}

- (void)loginFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error {
    [self showLoginError];
}

- (void)showLoginError {
    [progressLogin_ stopAnimation: nil];
    [progressLogin_ setHidden:YES];
    [textLoginProgress_ setHidden:YES];
    [textLoginError_ setHidden:NO];
}

- (IBAction)login:(id)sender {
    if (![self isButtonLoginEnabled])
        return;
    [buttonLogin_ setEnabled: NO];
    [progressLogin_ setHidden: NO];
    [progressLogin_ startAnimation: nil];
    [textLoginProgress_ setHidden: NO];
    NSString *account = [editOpenDnsAccount_ stringValue];
    NSString *password = [editOpenDnsPassword_ stringValue];

    NSString *apiString = [self apiSignInStringForAccount:account withPassword:password];
    NSURL *url = [NSURL URLWithString:API_HOST];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];

#if 1
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:[apiString dataUsingEncoding:NSUTF8StringEncoding]];
    NSURLResponse *resp = nil;
    NSError *err = nil;
    NSData *result = [NSURLConnection sendSynchronousRequest:request 
                                           returningResponse:&resp 
                                                       error:&err];

    if (err) {
        [self loginFetcher:nil failedWithError:err];
    } else {
        [self loginFetcher:nil finishedWithData:result];
    }
#else
    GDataHTTPFetcher* fetcher = [GDataHTTPFetcher httpFetcherWithRequest:request];
    [fetcher setPostData:[apiString dataUsingEncoding:NSUTF8StringEncoding]];
    [fetcher beginFetchWithDelegate:self
                  didFinishSelector:@selector(loginFetcher:finishedWithData:)
                    didFailSelector:@selector(loginFetcher:failedWithError:)];
#endif
}

- (void)applicationWillTerminate:(NSNotification*)aNotification {
    exitIpChangeThread_ = YES;
}

- (IBAction)loginQuitOrCancel:(id)sender {
    if ([self isLoggedIn])
        [self showStatusWindow:self];
    else
        [NSApp terminate:self];
}

- (IBAction)loginWindowAbout:(id)sender {
    // TODO: implement me
}

- (IBAction)selectNetworkCancel:(id)sender {
    NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];
    NSString *currNetworkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    if (![currNetworkState isEqualToString:UNS_OK]) {
        [prefs setObject:UNS_NO_NETWORK_SELECTED forKey:PREF_USER_NETWORKS_STATE];
    }
    
    [self updateStatusWindow];
    [self showStatusWindow:self];
}

- (IBAction)selectNetworkClick:(id)sender {
    [buttonSelect_ setEnabled:YES];
}

- (IBAction)selectNetworkDoubleClick:(id)sender {
    NSTableDataSourceDynamicNetworks *dataSource = [tableNetworksList_ dataSource];
    int row = [tableNetworksList_ selectedRow];
    NSTableColumn *tableColumn = [tableNetworksList_ tableColumnWithIdentifier:@"1"];
    NSString *hostname = [dataSource tableView:sender objectValueForTableColumn:tableColumn row:row];
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs setObject:UNS_OK forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:hostname forKey:PREF_HOSTNAME];
    [self showStatusWindow:self];
}

- (IBAction)statusChangeAccount:(id)sender {
    [self showLoginWindow];
}

- (IBAction)statusChangeNetwork:(id)sender {
    NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];
    NSString *token = [prefs objectForKey:PREF_TOKEN];
    [self downloadNetworks:token suppressUI:NO];
}

- (IBAction)statusUpdateNow:(id)sender {
    forceNextUpdate_ = YES;
    [lastIpUpdateTime_ release];
    lastIpUpdateTime_ = [[NSDate date] retain];
    if ([self updateLastIpUpdateTime])
        [self updateStatusWindow];
}

@end
