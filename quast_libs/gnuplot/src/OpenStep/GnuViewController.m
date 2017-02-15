#import "GnuViewController.h"
#import "Controller.h"

@implementation GnuViewController

- init
{
        [activateButton setState:1];
        [activateButton setTitle:@"--- ACTIVE ---"];

        return self;
}

- window
{
	return window;
}

- gnuView
{
	return gnuView;
}

- (BOOL)windowShouldClose:(id)sender
{
	[sender setDelegate:nil];
	if ([controller activeTerm] == self) {
		[controller setActiveTerm:nil];
	}
	return YES;
}

#warning OK - check delegate NotificationConversion: windowDidBecomeMain:(NSNotification *)notification is an NSWindow notification method (used to be a delegate method); delegates of NSWindow are automatically set to observe this notification; subclasses of NSWindow do not automatically receive this notification
- (void)windowDidBecomeMain:(NSNotification *)notification
{
    [controller setKeyTerm:self];
}

- activatePushed:sender
{
	if ([sender state] == 1) {
		[controller setActiveTerm: self];
		[sender setTitle:@"--- ACTIVE ---"];
	}
	else {
		[controller setActiveTerm:nil];
		[sender setTitle:@"Activate"];
	}

	return self;
}

- deactivate:sender
{
	[activateButton setState:0];
	[activateButton setTitle:@"Activate"];

	return self;
}


@end
