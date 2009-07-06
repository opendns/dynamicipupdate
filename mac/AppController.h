// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

typedef enum {
    IpUpdateOk,
    IpUpdateNotYours,
    IpUpdateBadAuth,
    IpUpdateNotAvailable,
    IpUpdateNoHost
} IpUpdateResult;

enum {
    SupressOneNetworkMsgFlag   = 0x1,
    SuppressNoDynamicIpMsgFlag = 0x2,
    SupressNoNetworksMsgFlag   = 0x4,
    SupressAll = SupressOneNetworkMsgFlag | SuppressNoDynamicIpMsgFlag | SupressNoNetworksMsgFlag
};

enum {
    ERR_UNKNOWN_METHOD			= 1003,
    ERR_BAD_USERNAME_PWD		= 1004,
    ERR_BAD_TOKEN				= ERR_BAD_USERNAME_PWD,
    // if there are no networks in networks_get
    ERR_NETWORK_DOESNT_EXIST	= 4008
};

extern NSString * PREF_ACCOUNT;
extern NSString * PREF_TOKEN;
extern NSString * PREF_HOSTNAME;
extern NSString * PREF_SEND_UPDATES;
extern NSString * PREF_USER_NETWORKS_STATE;
extern NSString * PREF_UNIQUE_ID;

extern NSString * UNS_OK;
extern NSString * UNS_NO_NETWORKS;
extern NSString * UNS_NO_DYNAMIC_IP_NETWORKS;
extern NSString * UNS_NO_NETWORK_SELECTED;

@interface AppController : NSObject {

    // stuff related to login window
    IBOutlet NSWindow *			windowLogin_;

    IBOutlet NSTextField *		editOpenDnsAccount_;
    IBOutlet NSTextField *		editOpenDnsPassword_;
    
    IBOutlet NSProgressIndicator *	progressLogin_;
    IBOutlet NSTextField *		textLoginProgress_;
    IBOutlet NSTextField *		textLoginError_;

    IBOutlet NSButton *                 buttonLogin_;
    IBOutlet NSButton *                 buttonQuitCancel_;

    // stuff related to select network window
    IBOutlet NSWindow *                 windowSelectNetwork_;
    IBOutlet NSTableView *              tableNetworksList_;
    IBOutlet NSButton *                 buttonSelect_;

    // stuff related to status window
    IBOutlet NSWindow *                 windowStatus_;
    IBOutlet NSTextField *              textAccount_;
    IBOutlet NSTextField *              textHostname_;
    IBOutlet NSTextField *              textIpAddress_;
    IBOutlet NSTextField *              textUsingOpenDns_;
    IBOutlet NSTextField *              textLastUpdated_;
    IBOutlet NSButton *                 buttonChangeAccount_;
    IBOutlet NSButton *                 buttonChangeNetwork_;
    IBOutlet NSButton *                 buttonUpdateNow_;
    IBOutlet NSButton *                 buttonCheckSendUpdates_;

    // menu-related stuff
    IBOutlet NSMenu *                   menu_;
    NSStatusItem *                      statusItem_;
    NSImage *                           menuIcon_;

    // program state
    IpUpdateResult                      ipUpdateResult_;
    NSString *                          currentIpAddressFromDns_;
    NSString *                          ipAddressFromHttp_;
    BOOL                                exitIpChangeThread_;
    BOOL                                usingOpenDns_;
    BOOL                                forceNextUpdate_;

    NSDate *                            nextIpUpdate_;
    NSDate *                            lastIpUpdateTime_;
    int                                 minutesSinceLastIpUpdate_;
}

- (IBAction)login:(id)sender;
- (IBAction)loginQuitOrCancel:(id)sender;
- (IBAction)selectNetworkCancel:(id)sender;
- (IBAction)selectNetworkClick:(id)sender;
- (IBAction)selectNetworkDoubleClick:(id)sender;
- (IBAction)showStatusWindow:(id)sender;
- (IBAction)statusChangeAccount:(id)sender;
- (IBAction)statusChangeNetwork:(id)sender;
- (IBAction)statusUpdateNow:(id)sender;

@end
