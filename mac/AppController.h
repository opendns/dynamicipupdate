#import <Cocoa/Cocoa.h>

@interface AppController : NSObject {

	IBOutlet NSMenu *				menu;

	IBOutlet NSTextField *			editOpenDnsAccount;
	IBOutlet NSTextField *			editOpenDnsPassword;
	
	IBOutlet NSProgressIndicator *	progressLogin;
	IBOutlet NSTextField *			textLoginProgress;

	NSStatusItem *					statusItem;
	NSImage *						menuIcon;
}

- (IBAction)login:(id)sender;

@end
