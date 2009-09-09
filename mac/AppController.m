// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppController.h"
#import "SBJSON.h"
#import "GDataHTTPFetcher.h"
#import "ApiKey.h"
#import "LoginItemsAE.h"
#import "NSDictionary+Networks.h"
#import <Sparkle/Sparkle.h>
#include <netdb.h>

#define LOGIN_ABOUT_URL @"http://www.opendns.com/software/mac/dynip/about/"
#define STATUS_ABOUT_URL LOGIN_ABOUT_URL
#define LEARN_MORE_IP_ADDRESS_TAKEN_URL @"http://www.opendns.com/software/mac/dynip/ip-taken/"
#define LEARN_MORE_IP_MISMATCH_URL @"http://www.opendns.com/software/mac/dynip/ip-differs/"
#define SETUP_OPENDNS_URL @"http://www.opendns.com/software/mac/dynip/setup-opendns/"
#define MANAGE_SETTINGS_URL @"https://www.opendns.com/dashboard"

#define API_HOST @"https://api.opendns.com/v1/"
#define IP_UPDATE_HOST @"https://updates.opendns.com"

#define NO_ERROR 0

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
- (void)hideErrorMessage;
- (void)forceHideErrorMessage;
- (void)showErrorMessage:(NSString*)msg;

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

- (void)showErrorInKeyWindow:(NSString*)error additionalText:(NSString*)s {
    NSWindow *window = [NSApp keyWindow];
    NSBeginAlertSheet(error, 
                      @"OK", nil, nil, 
                      window,
                      nil, // delegate
                      nil, //@selector(sheetDidEndShouldDelete:returnCode:contextInfo:),
                      nil, //@selector(endAlertSheet:returnCode:contextInfo:),
                      nil, // context info
                      s);
    
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
    if (NO == [prefs boolForKey:PREF_SEND_UPDATES])
        return NO;
    NSString *networkState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    return [networkState isEqualToString:UNS_OK];
}

- (BOOL)shouldSendPeriodicUpdate {
    if (![self canSendIPUpdates])
        return NO;

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
        NSRange spacePos = [s rangeOfString:@" "];
        unsigned pos = spacePos.location;
        if (pos != NSNotFound) {
            [ipAddressFromHttp_ release];
            ipAddressFromHttp_ = [[s substringFromIndex:pos+1] retain];
        }
    }

    if (ipUpdateResult == ipUpdateResult_)
         return;

    ipUpdateResult_ = ipUpdateResult;
    [self updateStatusWindow];
    if (ipUpdateResult != IpUpdateOk)
        [self showStatusWindow:self];
}

- (void)ipUpdateFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error {
    // TODO: maybe this is a good place to detect that there is no network?
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

- (void)ipAddressCheckAndPeriodicIpUpdate:(NSTimer*)timer {
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
		[self forceNextIpUpdate];
        updateStatus = YES;
    }

    if ([self shouldSendPeriodicUpdate]) {
        [self sendPeriodicUpdate];
    }
    if ([self updateLastIpUpdateTime])
        updateStatus = YES;
    
    if (updateStatus)
        [self updateStatusWindow];

	[NSTimer scheduledTimerWithTimeInterval:TIME_INTERVAL_ONE_MINUTE target:self selector:@selector(ipAddressCheckAndPeriodicIpUpdate:) userInfo:nil repeats:NO];
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

- (NSString*)uniqueId {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *uuid = [prefs objectForKey:PREF_UNIQUE_ID];
    if (!uuid) {
        CFUUIDRef uuidRef = CFUUIDCreate(kCFAllocatorDefault);
        CFStringRef sref = CFUUIDCreateString(kCFAllocatorDefault, uuidRef);
        CFRelease(uuidRef);
        uuid = (NSString*)sref;
        [uuid autorelease];
        [prefs setObject:uuid forKey:PREF_UNIQUE_ID];
    }
    return uuid;
}

- (NSData*)apiHostPost:(NSString*)postData {
    NSURL *url = [NSURL URLWithString:API_HOST];
    NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:url];
    [request setHTTPMethod:@"POST"];
    [request setHTTPBody:[postData dataUsingEncoding:NSUTF8StringEncoding]];
    NSURLResponse *resp = nil;
    NSError *err = nil;
    NSData *result = [NSURLConnection sendSynchronousRequest:request 
                                           returningResponse:&resp 
                                                       error:&err];
    if (err)
        return nil;
    return result;
}

- (NSDictionary*)dictionaryFromJson:(NSData*)jsonData {
    if (!jsonData)
        return nil;
    NSString *s = [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
    SBJSON *parser = [[[SBJSON alloc] init] autorelease];
    id json = [parser objectWithString:s];
    if (![json isKindOfClass:[NSDictionary class]])
        return nil;
    return json;    
}

// 0 means no error, -1 means no json, others are error numbers given in "error"
// field of json response from our api
- (int)apiResponseError:(NSDictionary*)json {
    if (!json)
        return -1;
    
    NSString *s = [json objectForKey:@"status"];
    if ([s isEqualToString:@"success"])
        return NO_ERROR;
    
    if (![s isEqualToString:@"failure"]) {
        assert(0);
        return -1;
    }
    NSNumber *n = [json objectForKey:@"error"];
    int err = [n intValue];
    assert(NO_ERROR != err);
    return err;
}

- (NSDictionary*)makeFirstNetworkDynamic:(NSDictionary*)networks withToken:(NSString*)token {
    NSDictionary *network = [networks findFirstDynamicNetwork];
    assert(!network);
    if (network)
        return network;

    network = [networks firstNetwork];
    assert(network);
    if (!network)
        return nil;

    NSString *internalId = [network objectForKey:@"internalId"];
    if (!internalId || !([internalId isKindOfClass:[NSString class]]) || (0 == [internalId length]))
        return nil;

    NSString *apiString = [self apiNetworksDynamicSetForNetwork:internalId withToken:token];
    NSData *jsonData = [self apiHostPost:apiString];
    NSDictionary *json = [self dictionaryFromJson:jsonData];
    int err = [self apiResponseError:json];
    if (NO_ERROR != err)
        return nil;
    return network;
}

- (void)setPref:(id)prefValue forKey:(NSString*)key {
    [[NSUserDefaults standardUserDefaults] setObject:prefValue forKey:key];
}

// very similar to downloadNetworks and downloadNetworks:
- (void)verifyHostname:(NSString*)hostname withToken:(NSString*)token {
    NSDictionary *dynamicNetwork = nil;
    NSString *apiString = [self apiGetNetworksStringForToken:token];
    NSData *jsonData = [self apiHostPost:apiString];
    NSDictionary *json = [self dictionaryFromJson:jsonData];
    int err = [self apiResponseError:json];
    if (ERR_NETWORK_DOESNT_EXIST == err)
        goto NoNetworks;
    if (NO_ERROR != err)
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
    
    [self setPref:hostname forKey:PREF_HOSTNAME];
    [self setPref:UNS_OK forKey:PREF_USER_NETWORKS_STATE];
    return;

NoNetworks:
    [self setPref:@"" forKey:PREF_HOSTNAME];
    [self setPref:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    return;

NoDynamicNetworks:
    dynamicNetwork = [self makeFirstNetworkDynamic:networks withToken:token];
    if (dynamicNetwork)
        goto SetDynamicNetwork;
    
    [self setPref:@"" forKey:PREF_HOSTNAME];
    [self setPref:UNS_NO_DYNAMIC_IP_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    return;
}

- (void)importOldSettings {
    // nothing to do if we already have account/token
    if ([self isLoggedIn])
        return;

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
    NSData *jsonData = [self apiHostPost:apiString];
    if (!jsonData)
        return;

    NSString *token = [self tokenFromSignInJsonResponse:jsonData];
    if (!token)
        return;
    
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    [prefs setObject:username forKey:PREF_ACCOUNT];
    [prefs setObject:token forKey:PREF_TOKEN];

    [self verifyHostname:hostname withToken:token];
    
    // delete the old preferences file, so that we don't import it multiple times
    NSFileManager *fileManager = [NSFileManager defaultManager];
    [fileManager removeFileAtPath:path handler:nil];
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
    OSStatus status = LIAECopyLoginItems(&loginItems);
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

- (BOOL) apiKeyValid {
	NSCharacterSet *validApiKeyChars = [NSCharacterSet characterSetWithCharactersInString:@"0123456789ABCDEF"];
	NSCharacterSet *invalidApiKeyChars = [validApiKeyChars invertedSet];
	NSString *apiKey = API_KEY;
	NSRange invalidCharsRange = [apiKey rangeOfCharacterFromSet:invalidApiKeyChars];
	if (NSNotFound == invalidCharsRange.location)
		return YES;
	return NO;
}

- (void)awakeFromNib {
	//BOOL sendsSystemProfile = [updater_ sendsSystemProfile];
	// TODO: should not be necessary, because I do have SUSendProfileInfo
	// boolean value set to YES in info.plist. For whatever reason, it
	// doesn't work
	[updater_ setSendsSystemProfile:YES];
    [textError_ setHorizontallyResizable:NO];
    [textError_ setVerticallyResizable:YES];
    
    statusItem_ = [[[NSStatusBar systemStatusBar] 
                               statusItemWithLength:NSSquareStatusItemLength]
                              retain];
    [statusItem_ setHighlightMode:YES];
    [statusItem_ setEnabled:YES];
    [statusItem_ setToolTip:@"OpenDNS Updater"];
    [statusItem_ setMenu:menu_]; 

    NSFont *font = [buttonChangeAccount_ font];
    [textError_ setFont:font];

    NSBundle *bundle = [NSBundle bundleForClass:[self class]]; 
    NSString *path = [bundle pathForResource:@"menuicon" ofType:@"tif"]; 
    menuIcon_= [[NSImage alloc] initWithContentsOfFile:path]; 
    [statusItem_ setImage:menuIcon_]; 
    [menuIcon_ release]; 

}

// schedule first update as soon as possible
-(void)forceNextIpUpdate {
	if (nextIpUpdate_)
		[nextIpUpdate_ release];
	nextIpUpdate_ = [[NSDate date] retain];
}

-(void)applicationDidFinishLaunching:(NSNotification*)aNotification {

	if (![self apiKeyValid]) {
		[self showInvaliApiKeyWindow];
		return;
	}
	
    [self makeStartAtLogin];
    [self importOldSettings];

    ipUpdateResult_ = IpUpdateOk;
	[self forceNextIpUpdate];
    [self forceHideErrorMessage];

    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    // observe changing of PREF_SEND_UPDATES so that we can synchronize
    // 'Update now' button (disable/enable depending on the value of PREF_SEND_UPDATES)
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

	[self ipAddressCheckAndPeriodicIpUpdate:nil];
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
    if (0 == minutesSinceLastIpUpdate_) {
        return @"Less than a minute ago.";
    }
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

- (BOOL)dnsVsHttpIpMismatch {
    if (!currentIpAddressFromDns_ || !ipAddressFromHttp_)
        return NO;
    if ([currentIpAddressFromDns_ isEqualToString:ipAddressFromHttp_])
        return NO;
    return YES;
}

- (void)updateStatusWindow {
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    BOOL sendingUpdates = [[prefs objectForKey:PREF_SEND_UPDATES] boolValue];
    NSString *account = [prefs objectForKey:PREF_ACCOUNT];

    if ([self isLoggedIn]) {
        [textAccount_ setTextColor:[NSColor blackColor]];
        [textAccount_ setTitleWithMnemonic:account];
        [buttonChangeAccount_ setTitle:@"Change account"];
    } else {
        [textAccount_ setTextColor:[NSColor redColor]];
        [textAccount_ setTitleWithMnemonic:@"Not signed in"];
        [buttonChangeAccount_ setTitle:@"Sign in"];
    }

    BOOL updateNowAlreadyDisabled = NO;
    if (![self isLoggedIn]) {
        [textHostname_ setTextColor:[NSColor redColor]];
        [textHostname_ setTitleWithMnemonic:@"Not signed in"];
        [buttonChangeNetwork_ setEnabled:NO];
    } else {
        [buttonChangeNetwork_ setEnabled:YES];
        // default button size for most texts. The value is taken from IB 
        NSRect buttonFrame = NSMakeRect(210, 3, 143, 32);
        if ([self noNetworksConfigured]) {
            [textHostname_ setTextColor:[NSColor redColor]];
            [textHostname_ setTitleWithMnemonic:@"No networks"];
            [buttonChangeNetwork_ setTitle:@"Refresh network list"];
            // this one is bigger
            buttonFrame = NSMakeRect(186, 3, 166, 32);

            updateNowAlreadyDisabled = YES;
            [textLastUpdated_ setTextColor:[NSColor redColor]];
            [textLastUpdated_ setTitleWithMnemonic:@"No networks"];
            [buttonUpdateNow_ setEnabled:NO];
        } else if ([self noDynamicNetworks]) {
            [textHostname_ setTextColor:[NSColor redColor]];
            [textHostname_ setTitleWithMnemonic:@"No dynamic IP networks"];
            [buttonChangeNetwork_ setTitle:@"Select network"];
            updateNowAlreadyDisabled = YES;
            [textLastUpdated_ setTextColor:[NSColor redColor]];
            [textLastUpdated_ setTitleWithMnemonic:@"No dynamic IP networks"];
            [buttonUpdateNow_ setEnabled:NO];
        } else if ([self networkNotSelected]) {
            [textHostname_ setTextColor:[NSColor redColor]];
            [textHostname_ setTitleWithMnemonic:@"No network selected"];
            [buttonChangeNetwork_ setTitle:@"Select network"];

            updateNowAlreadyDisabled = YES;
            [textLastUpdated_ setTextColor:[NSColor redColor]];
            [textLastUpdated_ setTitleWithMnemonic:@"No network selected"];
            [buttonUpdateNow_ setEnabled:NO];
        } else {
            [textHostname_ setTextColor:[NSColor blackColor]];
            NSString *hostname = [prefs objectForKey:PREF_HOSTNAME];
            if (0 == [hostname length])
                hostname = @"default";
            [textHostname_ setTitleWithMnemonic:hostname];
            [buttonChangeNetwork_ setTitle:@"Change network"];
        }
        [buttonChangeNetwork_ setFrame:buttonFrame];
    }

    if (currentIpAddressFromDns_) {
        [textIpAddress_ setTitleWithMnemonic:currentIpAddressFromDns_];
		NSString *menuTitle = [NSString stringWithFormat:@"IP: %@", currentIpAddressFromDns_];
		[menuItemIpAddr_ setTitle:menuTitle];
	}
    else {
		// TODO: show a different text in red?
        [textIpAddress_ setTitleWithMnemonic:@""];
		[menuItemIpAddr_ setTitle:@"IP: unavailable"];
    }

    if (usingOpenDns_) {
        [textUsingOpenDns_ setTextColor:[NSColor blackColor]];
        [textUsingOpenDns_ setTitleWithMnemonic:@"Yes"];
    } else {
        [textUsingOpenDns_ setTextColor:[NSColor redColor]];
        [textUsingOpenDns_ setTitleWithMnemonic:@"No"];
    }

    if (![self isLoggedIn]) {
        [textLastUpdated_ setTextColor:[NSColor redColor]];
        [textLastUpdated_ setTitleWithMnemonic:@"Not signed in"];
        [buttonUpdateNow_ setEnabled:NO];
        return;
    }

    if (!sendingUpdates) {
        [textLastUpdated_ setTextColor:[NSColor redColor]];
        [textLastUpdated_ setTitleWithMnemonic:@"Updates disabled"];
        [buttonUpdateNow_ setEnabled:NO];
        return;
    }

    if (!updateNowAlreadyDisabled) {
        [textLastUpdated_ setTextColor:[NSColor blackColor]];
        [textLastUpdated_ setTitleWithMnemonic:[self lastUpdateText]];
        [buttonUpdateNow_ setEnabled:YES];
    }

    // build up error message to be shown at the bottom, if there are
    // any error conditions
    NSMutableString *errorMsg = [NSMutableString stringWithCapacity:128];
    BOOL needNewline = NO;
    // TODO: no network connectivity error

    if ([self isLoggedIn]) {
        if ([self noNetworksConfigured]) {
            if (needNewline) [errorMsg appendString:@"\n\n"];
            needNewline = YES;
            [errorMsg appendString:@"You don't have any networks. First, add a network in your OpenDNS account. Then refresh network list."];
        } else if ([self noDynamicNetworks]) {
            if (needNewline) [errorMsg appendString:@"\n\n"];
            needNewline = YES;
            [errorMsg appendString:@"None of your networks is configured for dynamic IP updates. First, configure a network for dynamic IP updates in your OpenDNS account. Then select a network."];
        } else if ([self networkNotSelected]) {
            if (needNewline) [errorMsg appendString:@"\n\n"];
            needNewline = YES;
            [errorMsg appendString:@"You need to select one of your networks."];
        }
    }

    if (IpUpdateNotYours == ipUpdateResult_) {
        if (needNewline) [errorMsg appendString:@"\n\n"];
        needNewline = YES;
        [errorMsg appendFormat:@"Your IP address is taken by another user. Learn more at %@", LEARN_MORE_IP_ADDRESS_TAKEN_URL];
    }
    
    if (IpUpdateBadAuth == ipUpdateResult_) {
        // this should never happen
        if (needNewline) [errorMsg appendString:@"\n\n"];
        needNewline = YES;
        [errorMsg appendString:@"Your authorization token is invalid."];
    }

    if ([self dnsVsHttpIpMismatch]) {
        if (needNewline) [errorMsg appendString:@"\n\n"];
        needNewline = YES;
        [errorMsg appendFormat:@"Your OpenDNS settings might not work due to DNS IP address (%@) and HTTP IP address (%@) mismatch. Learn more at %@", currentIpAddressFromDns_, ipAddressFromHttp_, LEARN_MORE_IP_MISMATCH_URL];
    }
    
    if (needNewline) [errorMsg appendString:@"\n"];

    if ([errorMsg length] > 0) {
        [self showErrorMessage:errorMsg];
    } else {
        [self hideErrorMessage];
    }
}

- (void)hiliteAndActivateURLs:(NSTextView*)textView
{
    NSTextStorage* textStorage=[textView textStorage];
    NSString* string=[textStorage string];
    NSRange searchRange=NSMakeRange(0, [string length]);
    NSRange foundRange;
    
    [textStorage beginEditing];
    do {
        foundRange=[string rangeOfString:@"http://" options:0 range:searchRange];
        
        if (foundRange.length > 0) {
            NSURL* theURL;
            NSDictionary* linkAttributes;
            NSRange endOfURLRange;
            
            searchRange.location=foundRange.location+foundRange.length;
            searchRange.length = [string length]-searchRange.location;
            
            endOfURLRange=[string rangeOfCharacterFromSet:
                           [NSCharacterSet whitespaceAndNewlineCharacterSet]
                                                  options:0 
                                                    range:searchRange];
            
            if (endOfURLRange.length==0)
                endOfURLRange.location=[string length]-1;
            
            foundRange.length = endOfURLRange.location-foundRange.location+1;
            theURL=[NSURL URLWithString:[string substringWithRange:foundRange]];			
            linkAttributes= [NSDictionary dictionaryWithObjectsAndKeys: theURL, NSLinkAttributeName,
                             [NSNumber numberWithInt:NSSingleUnderlineStyle], NSUnderlineStyleAttributeName,
                             [NSColor blueColor], NSForegroundColorAttributeName,
                             NULL];
            
            [textStorage addAttributes:linkAttributes range:foundRange];
        }
    } while (foundRange.length!=0);
    
    [textStorage endEditing];
}

- (void)setTextView:(NSTextView*)textView string:(NSString*)aString
{
    NSTextStorage *ts = [textView textStorage];
    [ts beginEditing];
    NSString *s = [ts string];
    unsigned len = [s length];
    NSRange r = NSMakeRange(0, len);
    [ts deleteCharactersInRange:r];
    
    NSAttributedString *attrString =
    [[NSAttributedString alloc] initWithString:aString
                                    attributes:nil];
    [ts setAttributedString:attrString];
    [ts endEditing];
    [self hiliteAndActivateURLs:textView];
}

- (BOOL)errorMessageVisible {
    return ![scrollViewError_ isHidden];
}

- (void)forceHideErrorMessage {
    [scrollViewError_ setHidden:YES];
    NSRect contentFrame = [[windowStatus_ contentView] frame];
    int oldHeight = contentFrame.size.height;
    int newHeight = 367;
    contentFrame.size.height = newHeight;
    int heightDiff = newHeight - oldHeight;
    NSRect newWinFrame = [windowStatus_ frameRectForContentRect:contentFrame];
    NSRect winFrame = [windowStatus_ frame];
    newWinFrame.origin = winFrame.origin;
    newWinFrame.origin.y -= heightDiff;
    [windowStatus_ setFrame:newWinFrame display:YES animate:YES];
}

- (void)hideErrorMessage {
    if ([self errorMessageVisible])
        [self forceHideErrorMessage];
}

- (void)showErrorMessage:(NSString*)msg {
    NSRect frame;
    [self setTextView:textError_ string:msg];
    [textError_ setTextColor:[NSColor redColor]];
    
    // figure out how big the NSTextView for the error message needs to be
    NSLayoutManager *layoutManager = [textError_ layoutManager];
    NSTextContainer *textContainer = [textError_ textContainer];
    // force layout (so that usedRectForTextContainer: works)
    [layoutManager glyphRangeForTextContainer:textContainer];
    NSRect textSize = [layoutManager usedRectForTextContainer:textContainer];
    int textHeight = textSize.size.height;
    int textViewHeight = textHeight + 4;

    NSRect contentFrame = [[windowStatus_ contentView] frame];
    int oldHeight = contentFrame.size.height;
    int newHeight = 367 + textViewHeight + 8;
    int heightDiff = newHeight - oldHeight;
 
    contentFrame.size.height = newHeight;
    NSRect newWinFrame = [windowStatus_ frameRectForContentRect:contentFrame];
    NSRect winFrame = [windowStatus_ frame];
    newWinFrame.origin = winFrame.origin;
    newWinFrame.origin.y -= heightDiff;
    [windowStatus_ setFrame:newWinFrame display:YES animate:YES];

    frame = [boxLastUpdated_ frame];
    int lastUpdateY = frame.origin.y;

    frame = [scrollViewError_ frame];
    frame.size.height = textViewHeight;
    frame.origin.y = lastUpdateY - textViewHeight - 12;
    [scrollViewError_ setFrame:frame];
    
    [scrollViewError_ setHidden:NO];
}

- (void)showInvaliApiKeyWindow {
    [windowLogin_ orderOut:self];
    [windowSelectNetwork_ orderOut:self];
    [windowStatus_ orderOut:self];
    [NSApp activateIgnoringOtherApps:YES];
    [windowInvalidApiKey_ makeKeyAndOrderFront:self];
}

- (void)showStatusWindow:(id)sender {
    [windowLogin_ orderOut:self];
    [windowSelectNetwork_ orderOut:self];
	[windowInvalidApiKey_ orderOut:self];
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

    [windowSelectNetwork_ orderOut:self];
    [windowStatus_ orderOut:self];
	[windowInvalidApiKey_ orderOut:self];
    [NSApp activateIgnoringOtherApps:YES];
    [windowLogin_ makeKeyAndOrderFront:self];
    [self setButtonLoginStatus];
    [windowLogin_ makeFirstResponder:editOpenDnsAccount_];
}

- (void)showNetworksWindow {
    [windowLogin_ orderOut:self];
    [windowStatus_ orderOut:self];
	[windowInvalidApiKey_ orderOut:self];
    [NSApp activateIgnoringOtherApps:YES];
    [windowSelectNetwork_ makeKeyAndOrderFront:self];
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

- (void)showLoginError {
    [self showErrorInKeyWindow:@"Login failed" additionalText:@"Please double-check your username and password."];
}

- (void)showGenericNetworkError {
    [self showErrorInKeyWindow:@"Network error" additionalText:@"Error downloading data"];    
}

- (void)setButtonLoginStatus {
    [buttonLogin_ setEnabled:[self isButtonLoginEnabled]];
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
    [self setButtonLoginStatus];
}

- (void)downloadNetworks:(NSString*)token suppressUI:(BOOL)suppressUI {
    NSDictionary *dynamicNetwork = nil;
    NSString *hostname = nil;
    NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];

    NSString *apiString = [self apiGetNetworksStringForToken:token];
    NSData *jsonData = [self apiHostPost:apiString];
    if (!jsonData) {
        [self showGenericNetworkError];
        return;
    }

    NSDictionary *json = [self dictionaryFromJson:jsonData];
    int err = [self apiResponseError:json];
    if (ERR_NETWORK_DOESNT_EXIST == err)
        goto NoNetworks;
    if (NO_ERROR != err)
        goto Error;
    
    NSDictionary *networks = [json objectForKey:@"response"];
    if (!networks)
        goto NoNetworks;

    if (0 == [networks count])
        goto NoNetworks;
    
    unsigned dynamicCount = [networks dynamicNetworksCount];
    if (0 == dynamicCount)
        goto NoDynamicNetworks;
    
    dynamicNetwork = [networks findFirstDynamicNetwork];
    if (1 == dynamicCount) {
        if (!suppressUI) {
            [self showErrorInKeyWindow:@"No other networks available" 
                        additionalText:@"You only have one network configured for dynamic IP updates."];
        }
        goto SetDynamicNetwork;
    }

    NSArray *dynamicNetworks = labeledDynamicNetworks(networks);
    if (0 == [dynamicNetworks count]) {
        // if we have more than one dynamic networks but none of them has a
        // label, use the first dynamic network found above.
        // TODO: not sure if updates will work for that case.
        goto SetDynamicNetwork;
    }
    NSTableDataSourceDynamicNetworks *dataSource = [[NSTableDataSourceDynamicNetworks alloc] initWithNetworks:dynamicNetworks];
    [tableNetworksList_ setDataSource:dataSource];
    [tableNetworksList_ setTarget:self];
    [tableNetworksList_ setAction:@selector(selectNetworkClick:)];
    [tableNetworksList_ setDoubleAction:@selector(selectNetworkDoubleClick:)];
    [tableNetworksList_ reloadData];
    // set current state as network not selected, so that when user clicks
    // 'Cancel' button or hides the window, we end up in the correct state
    if (suppressUI) {
        // hack: only when suppressing ui, which really means we're coming
        // from main window, not the first-time login sequence
        [self setPref:UNS_NO_NETWORK_SELECTED forKey:PREF_USER_NETWORKS_STATE];
    }
    [self showNetworksWindow];
    return;

Error:
    [self showErrorInKeyWindow:@"Error downloading information"
                additionalText:@""];
    return;
    
NoNetworks:
    if (!suppressUI) {
        [self showErrorInKeyWindow:@"You have no networks" 
                    additionalText:@"Add a network and enable dynamic IP updates in your OpenDNS account."];
    }
    [prefs setObject:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
    goto ShowStatusWindow;
    
NoDynamicNetworks:
    dynamicNetwork = [self makeFirstNetworkDynamic:networks withToken:token];
    if (dynamicNetwork)
        goto SetDynamicNetwork;
    if (!suppressUI) {
        [self showErrorInKeyWindow:@"No networks enabled for dynamic IP updates" 
                    additionalText:@"Make sure at least one network is enabled for dynamic IP updates in your OpenDNS account."];
    }

    [prefs setObject:UNS_NO_DYNAMIC_IP_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
    [prefs setObject:@"" forKey:PREF_HOSTNAME];
    return;
    
SetDynamicNetwork:
    hostname = [dynamicNetwork objectForKey:@"label"];
    if (!hostname || ![hostname isKindOfClass:[NSString class]])
        hostname = @"";
    [prefs setObject:hostname forKey:PREF_HOSTNAME];
    [prefs setObject:UNS_OK forKey:PREF_USER_NETWORKS_STATE];
    
ShowStatusWindow:
	[self forceNextIpUpdate];
	[self ipAddressCheckAndPeriodicIpUpdate:nil];
    [self updateStatusWindow];
    [self showStatusWindow:nil];
    return;
}

// extract token from json response to signin method. Returns nil on error.
- (NSString*)tokenFromSignInJsonResponse:(NSData*)jsonData {
    NSDictionary *json = [self dictionaryFromJson:jsonData];
    int err = [self apiResponseError:json];
    if (NO_ERROR != err)
        return nil;
    NSDictionary *response = [json objectForKey:@"response"];
    if (!response)
        return nil;
    return [response objectForKey:@"token"];
}

- (IBAction)login:(id)sender {
    if (![self isButtonLoginEnabled])
        return;
    [buttonLogin_ setEnabled: NO];
    NSString *account = [editOpenDnsAccount_ stringValue];
    NSString *password = [editOpenDnsPassword_ stringValue];

    NSString *apiString = [self apiSignInStringForAccount:account withPassword:password];
    NSData *jsonData = [self apiHostPost:apiString];
    if (!jsonData) {
        [self showGenericNetworkError];
        return;
    }
    NSString *token = [self tokenFromSignInJsonResponse:jsonData];
    if (!token) {
        [self showLoginError];
        return;
    }
    [self setPref:token forKey:PREF_TOKEN];
    [self setPref:account forKey:PREF_ACCOUNT];
    [self downloadNetworks:token suppressUI:YES];
}

- (IBAction)loginQuitOrCancel:(id)sender {
    if ([self isLoggedIn])
        [self showStatusWindow:self];
    else
        [NSApp terminate:self];
}

- (IBAction)loginWindowAbout:(id)sender {
    [[NSWorkspace sharedWorkspace]
                 openURL:[NSURL URLWithString:LOGIN_ABOUT_URL]];
}

- (IBAction)manageMySettings:(id)sender {
    [[NSWorkspace sharedWorkspace]
     openURL:[NSURL URLWithString:MANAGE_SETTINGS_URL]];    
}

- (IBAction)statusWindowAbout:(id)sender {
    [[NSWorkspace sharedWorkspace]
     openURL:[NSURL URLWithString:STATUS_ABOUT_URL]];
}

- (IBAction)selectNetworkCancel:(id)sender {
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
	[self forceNextIpUpdate];
	[self ipAddressCheckAndPeriodicIpUpdate:nil];
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
	[self forceNextIpUpdate];
	[self ipAddressCheckAndPeriodicIpUpdate:nil];
    if ([self updateLastIpUpdateTime])
        [self updateStatusWindow];
}

- (NSArray *)feedParametersForUpdater:(SUUpdater *)updater
                 sendingSystemProfile:(BOOL)sendingProfile {

    NSString *uniqueId = [self uniqueId];
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys: 
                          @"uniqueId", @"key",
						  uniqueId, @"value",
                          @"uniqueId", @"displayKey",
						  uniqueId, @"displayValue",
                          nil];
    NSArray *arr = [NSArray arrayWithObject:dict];
    return arr;
}

@end
