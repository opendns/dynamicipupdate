#import "AppController.h"

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
}

-(void)dealloc
{
	[statusItem release];
	[super dealloc];
}

- (IBAction)login:(id)sender
{
	
}

@end
