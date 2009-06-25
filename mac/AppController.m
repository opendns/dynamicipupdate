// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppController.h"
#import "SBJSON.h"
#import "GDataHTTPFetcher.h"
#import "ApiKey.h"

#include <netdb.h>

#define API_HOST @"https://api.opendns.com/v1/"

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
- (void)ipAddressChanged:(NSString *)newIpAddress;
- (void)ipChangeThread;
- (void)setButtonLoginStatus;
- (BOOL)isButtonLoginEnabled;
- (BOOL)shouldSendPeriodicUpdate;
- (void)sendPeriodicUpdate;
- (NSString*)apiSignInStringForAccount:(NSString*)userName withPassword:(NSString*)password;
- (NSString*)apiGetNetworksStringForToken:(NSString*)token;
- (void)showLoginError;
- (void)showLoginWindow;
- (void)downloadNetworks:(NSString*)token suppressUI:(BOOL)suppressUI;
- (BOOL)noNetworksConfigured;
- (BOOL)isLoggedIn;
- (void)updateStatusWindow;
@end

@interface NSDictionary (DynamicNetworks)

- (BOOL)isNetworkDynamic:(NSDictionary*)network;
- (unsigned)dynamicNetworksCount;
- (NSDictionary*)findFirstDynamicNetwork;
- (NSDictionary*)dynamicNetworkAtIndex:(unsigned)idx;
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

- (void)ipAddressChanged:(NSString *)newIpAddress {
	[currentIpAddress_ release];
	currentIpAddress_ = [newIpAddress copy];
    if (newIpAddress) {
        usingOpenDns_ = YES;
    } else {
        usingOpenDns_ = NO;
    }
    [self updateStatusWindow];
}

- (BOOL)canSendIPUpdates {
    if (![self isLoggedIn])
        return NO;
    NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
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

- (void)sendPeriodicUpdate {

    // schedule next ip update 3 hours from now
    [nextIpUpdate_ release];
    nextIpUpdate_ = [[NSDate dateWithTimeIntervalSinceNow:TIME_INTERVAL_3HR] retain];
}

- (void)ipChangeThread {
	NSAutoreleasePool* myAutoreleasePool = [[NSAutoreleasePool alloc] init];
	NSString *currIp = nil;

	while (!exitIpChangeThread_) {
		NSString *newIp = [self getMyIp];
		if (!NSStringsEqual(newIp, currIp)) {
			[currIp release];
			currIp = [newIp copy];
			[self performSelectorOnMainThread:@selector(ipAddressChanged:) withObject:currIp waitUntilDone:NO];
		}
        
		if ([self shouldSendPeriodicUpdate]) {
			[self sendPeriodicUpdate];
		}

		NSDate *inOneMinute = [[NSDate date] addTimeInterval:TIME_INTERVAL_ONE_MINUTE];
		[NSThread sleepUntilDate:inOneMinute];
        [myAutoreleasePool drain];
	}
	[currIp release];
	[myAutoreleasePool release];
}

- (void)awakeFromNib {
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
    // schedule first update as soon as possible
    nextIpUpdate_ = [[NSDate date] retain];

	[NSThread detachNewThreadSelector:@selector(ipChangeThread)
							 toTarget:(id)self
						   withObject:(id)nil];
    
	NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
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

- (BOOL)noNetworksConfigured {
	NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
    NSString *networksState = [prefs objectForKey:PREF_USER_NETWORKS_STATE];
    if ([networksState isEqualToString:UNS_NO_NETWORKS])
        return YES;
    return NO;
}

- (void)updateStatusWindow {
	NSUserDefaults *prefs = [NSUserDefaults standardUserDefaults];
	NSString *account = [prefs objectForKey:PREF_ACCOUNT];
    if ([self isLoggedIn]) {
        [textAccount_ setTitleWithMnemonic:account];
    } else {
        // TODO: change 'Change account' to 'Login' ?
        // TODO: or not allow showing this window if not logged in?
        [textAccount_ setTitleWithMnemonic:@"Not logged in"];
    }

	NSString *hostname = [prefs objectForKey:PREF_HOSTNAME];
    if (0 == [hostname length])
        hostname = @"default";
    [textHostname_ setTitleWithMnemonic:hostname];

    if (currentIpAddress_)
        [textIpAddress_ setTitleWithMnemonic:currentIpAddress_];
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

    // TODO: show last updated time
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
        [self showStatusWindow:self];
		goto Exit;
	}

    NSArray *dynamicNetworks = labeledDynamicNetworks(networks);
	NSTableDataSourceDynamicNetworks *dataSource = [[NSTableDataSourceDynamicNetworks alloc] initWithNetworks:dynamicNetworks];
	[tableNetworksList_ setDataSource:dataSource];
    [tableNetworksList_ setTarget:self];
    [tableNetworksList_ setAction:@selector(selectNetworkClick:)];
    [tableNetworksList_ setDoubleAction:@selector(selectNetworkDoubleClick:)];
	[tableNetworksList_ reloadData];
	[self showNetworksWindow];
	goto Exit;
Error:
	NSLog(@"Error");
Exit:
	return;

NoNetworks:
	[prefs setObject:UNS_NO_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
	[prefs setObject:@"" forKey:PREF_HOSTNAME];
	return;

NoDynamicNetworks:
	[prefs setObject:UNS_NO_DYNAMIC_IP_NETWORKS forKey:PREF_USER_NETWORKS_STATE];
	[prefs setObject:@"" forKey:PREF_HOSTNAME];
	return;
}

- (void)getNetworksFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error {
	// TODO: implement me
}

- (void)downloadNetworks:(NSString*)token suppressUI:(BOOL)suppressUI{
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

- (void)loginFetcher:(GDataHTTPFetcher *)fetcher finishedWithData:(NSData *)retrievedData {
	[progressLogin_ stopAnimation: nil];
	NSString *s = [[NSString alloc] initWithData:retrievedData encoding:NSUTF8StringEncoding];
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
	NSString *token = [response objectForKey:@"token"];
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
	NSURLRequest *request = [NSURLRequest requestWithURL:url];
	GDataHTTPFetcher* fetcher = [GDataHTTPFetcher httpFetcherWithRequest:request];
	[fetcher setPostData:[apiString dataUsingEncoding:NSUTF8StringEncoding]];
	[fetcher beginFetchWithDelegate:self
	               didFinishSelector:@selector(loginFetcher:finishedWithData:)
	                 didFailSelector:@selector(loginFetcher:failedWithError:)];
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
	[prefs setObject:UNS_NO_NETWORK_SELECTED forKey:PREF_USER_NETWORKS_STATE];
	[prefs setObject:@"" forKey:PREF_HOSTNAME];
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

}

@end
