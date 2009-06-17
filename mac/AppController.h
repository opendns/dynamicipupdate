// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@interface AppController : NSObject {

	IBOutlet NSWindow *				windowLogin;
	IBOutlet NSMenu *				menu;

	IBOutlet NSTextField *			editOpenDnsAccount;
	IBOutlet NSTextField *			editOpenDnsPassword;
	
	IBOutlet NSProgressIndicator *	progressLogin;
	IBOutlet NSTextField *			textLoginProgress;

	IBOutlet NSButton *				buttonLogin;

	NSStatusItem *					statusItem_;
	NSImage *						menuIcon;
	
	BOOL							exitIpChangeThread_;
}

- (IBAction)login:(id)sender;
@end
