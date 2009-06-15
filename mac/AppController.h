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
}

- (IBAction)login:(id)sender;
@end
