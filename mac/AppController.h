// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

@interface AppController : NSObject {

	IBOutlet NSWindow *				windowLogin_;
	IBOutlet NSMenu *				menu_;

	IBOutlet NSTextField *			editOpenDnsAccount_;
	IBOutlet NSTextField *			editOpenDnsPassword_;
	
	IBOutlet NSProgressIndicator *	progressLogin_;
	IBOutlet NSTextField *			textLoginProgress_;

	IBOutlet NSButton *				buttonLogin_;

	NSStatusItem *					statusItem_;
	NSImage *						menuIcon_;
	
	BOOL							exitIpChangeThread_;
}

- (IBAction)login:(id)sender;
@end
