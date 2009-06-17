// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "AppController.h"
#import "SBJSON.h"
#import "GDataHTTPFetcher.h"

#include <netdb.h>

@interface AppController (Private)
- (void)setButtonLoginStatus;
- (BOOL)isButtonLoginEnabled;
@end

@implementation AppController

- (void)getMyIp {
	char **addrs;
	struct hostent *he = gethostbyname("myip.opendns.com");
	// TODO: notify the user that we don't support ipv6?
	if (AF_INET != he->h_addrtype)
		return;
	if (4 != he->h_length)
		return;
	addrs = he->h_addr_list;
	while (*addrs) {
		unsigned char *a = (unsigned char*)*addrs++;
		NSString *addrTxt = [NSString stringWithFormat:@"%d.%d.%d.%d", a[0], a[1], a[2], a[3]];
	}
}

- (void)ipChangeThread {
	NSAutoreleasePool* myAutoreleasePool = [[NSAutoreleasePool alloc] init];
	
	while (!exitIpChangeThread_) {
		
		
	}

	[myAutoreleasePool release];
}

- (void)awakeFromNib {
	statusItem_ = [[[NSStatusBar systemStatusBar] 
				   statusItemWithLength:NSSquareStatusItemLength]
				  retain];
	[statusItem_ setHighlightMode:YES];
	[statusItem_ setEnabled:YES];
	[statusItem_ setToolTip:@"OpenDNS Updater"];
	[statusItem_ setMenu:menu]; 
	NSBundle *bundle = [NSBundle bundleForClass:[self class]]; 
	NSString *path = [bundle pathForResource:@"menuicon" ofType:@"tif"]; 
	menuIcon= [[NSImage alloc] initWithContentsOfFile:path]; 
	[statusItem_ setImage:menuIcon]; 
	[menuIcon release]; 

	NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];
	
	NSString *account = [prefs objectForKey: @"account"];
	NSString *token = [prefs objectForKey: @"token"];
	if (!account || !token) {
		[NSApp activateIgnoringOtherApps:YES];
		[windowLogin makeKeyAndOrderFront:self];
	}
	[self setButtonLoginStatus];
	
	[self getMyIp];
}

-(void)dealloc {
	[statusItem_ release];
	[super dealloc];
}

- (BOOL)isButtonLoginEnabled {
	NSString *account = [editOpenDnsAccount stringValue];
	NSString *password = [editOpenDnsPassword stringValue];
	if (!account || (0 == [account length]))
		return NO;
	if (!password || (0 == [password length]))
		return NO;
	return YES;
}

- (void)setButtonLoginStatus {
	[buttonLogin setEnabled:[self isButtonLoginEnabled]];
}

- (void)controlTextDidChange:(NSNotification*)aNotification {
	[self setButtonLoginStatus];
}

- (void)myFetcher:(GDataHTTPFetcher *)fetcher finishedWithData:(NSData *)retrievedData
{
	[progressLogin stopAnimation: nil];
}

- (void)myFetcher:(GDataHTTPFetcher *)fetcher failedWithError:(NSError *)error
{
	[progressLogin stopAnimation: nil];
}

- (IBAction)login:(id)sender {
	if (![self isButtonLoginEnabled])
		return;
	[buttonLogin setEnabled: NO];
	[progressLogin setHidden: NO];
	[progressLogin startAnimation: nil];
	[textLoginProgress setHidden: NO];
	NSString *urlString = @"http://google.com/";
	NSURL *url = [NSURL URLWithString:urlString];
	NSURLRequest *request = [NSURLRequest requestWithURL:url];
	GDataHTTPFetcher* fetcher = [GDataHTTPFetcher httpFetcherWithRequest:request];
	[fetcher beginFetchWithDelegate:self
	               didFinishSelector:@selector(myFetcher:finishedWithData:)
	                 didFailSelector:@selector(myFetcher:failedWithError:)];

}

@end
