#import "AppController.h"

@interface AppController (Private)
- (void)setButtonLoginStatus;
- (BOOL)isButtonLoginEnabled;
@end

@implementation AppController

- (void)awakeFromNib
{
	statusItem = [[[NSStatusBar systemStatusBar] 
				   statusItemWithLength:NSSquareStatusItemLength]
				  retain];
	[statusItem setHighlightMode:YES];
	[statusItem setEnabled:YES];
	[statusItem setToolTip:@"OpenDNS Updater"];
	[statusItem setMenu:menu]; 
	NSBundle *bundle = [NSBundle bundleForClass:[self class]]; 
	NSString *path = [bundle pathForResource:@"menuicon" ofType:@"tif"]; 
	menuIcon= [[NSImage alloc] initWithContentsOfFile:path]; 
	[statusItem setImage:menuIcon]; 
	[menuIcon release]; 

	NSUserDefaults * prefs = [NSUserDefaults standardUserDefaults];
	
	NSString *account = [prefs objectForKey: @"account"];
	NSString *token = [prefs objectForKey: @"token"];
	if (account && token) {
		// TODO: this is meant to hide the window
		//[windowLogin orderOut: nil];
	}
	[self setButtonLoginStatus];
}

-(void)dealloc
{
	[statusItem release];
	[super dealloc];
}

- (BOOL)isButtonLoginEnabled
{
	NSString *account = [editOpenDnsAccount stringValue];
	NSString *password = [editOpenDnsPassword stringValue];
	if (!account || (0 == [account length]))
		return NO;
	if (!password || (0 == [password length]))
		return NO;
	return YES;
}

- (void)setButtonLoginStatus
{
	[buttonLogin setEnabled:[self isButtonLoginEnabled]];
}

- (void)controlTextDidChange:(NSNotification*)aNotification
{
	[self setButtonLoginStatus];
}

-(IBAction)login:(id)sender
{
	if (![self isButtonLoginEnabled])
		return;
	[buttonLogin setEnabled: NO];
	[progressLogin setHidden: NO];
	[progressLogin startAnimation: nil];
	[textLoginProgress setHidden: NO];
}

@end
