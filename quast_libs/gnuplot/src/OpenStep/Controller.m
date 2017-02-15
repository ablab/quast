#import "Controller.h"
#import "GnuViewController.h"
#import "GnuView.h"

@implementation Controller

- (id) activeTerm
{
  return activeTerm;
}

- (void) applicationDidFinishLaunching:(NSNotification *) notification
{
    // NSApplication *theApplication = [notification object];
    NSDictionary *gnutermDefaults;
    NSPrintInfo *prInfo;

#if 0
    gnutermDefaults = [NSDictionary dictionaryWithObjectsAndKeys:  @"400", @"Width", @"300", @"Height",  @"Buffered", @"Backing", NULL];
#endif

    activeTerm = keyTerm = [self newGnuTerm:self];

    myConnection = [NSConnection defaultConnection];
    [myConnection retain];
    [myConnection setRootObject:self];
    if([myConnection registerName:@"gnuplotServer"] == NO) {
        NSLog(@"Error registering %s\n", "gnuplotServer");
    }

#warning PrintingConversion:  The current PrintInfo object now depends on context. '[NSPrintInfo sharedPrintInfo]' used to be '[NSApp printInfo]'. This might want to be [[NSPrintOperation currentOperation] printInfo] or possibly [[PageLayout new] printInfo].
    prInfo = [NSPrintInfo sharedPrintInfo];
//        prInfo = [[NSPrintOperation currentOperation] printInfo];
#warning PrintingConversion: May be able to remove some of the [prInfo setXXXXMargin:] calls
    [prInfo setLeftMargin:0];

    [prInfo setRightMargin:0];

    [prInfo setTopMargin:0];

    [prInfo setBottomMargin:50];
    [prInfo setOrientation:NSLandscapeOrientation];
    [prInfo setHorizontallyCentered:YES];
    [prInfo setVerticallyCentered:YES];

#if 0
    /* Get user Preferences */
    [[NSUserDefaults standardUserDefaults] registerDefaults: gnutermDefaults];  // TODO this may override existing values

    [[DefaultSize cellAtIndex:0] setStringValue:[[NSUserDefaults standardUserDefaults] objectForKey:@"Width"]];
    [[DefaultSize cellAtIndex:1] setStringValue:[[NSUserDefaults standardUserDefaults] objectForKey:@"Height"]];

    if ([[[NSUserDefaults standardUserDefaults] objectForKey:@"Backing"] isEqualToString:@"Buffered"]) {
        backing = NSBackingStoreBuffered;
        [useBufferedSwitch setState:YES];
    }
    else {
        backing = NSBackingStoreRetained;
        [useBufferedSwitch setState:NO];
    }

    gvList = [NSMutableArray array];
    [gvList retain];
#endif
}

- newGnuTerm:sender
{
    NSWindow *win;
    NSRect frame;

    if ([NSBundle loadNibNamed:@"gnuview.nib" owner:self] == NO) {
        return nil;
    }

    if (win = [activeTerm window]) {
        frame = [win frame];
        (&frame)->origin.x += offset;
        (&frame)->origin.y -= offset;
        if ( (offset += 24.0) > 100.0)
            offset = 0.0;

        [win setTitle:[NSString stringWithFormat: @"gnuplot %d", ++gnuviewNum]];

        [win setFrame:frame display:YES];
        [win makeKeyAndOrderFront:nil];

        return activeTerm;
    }
    return nil;
}


- (void) setActiveTerm:(id) newTerm
{
	if (activeTerm != nil) [activeTerm deactivate:self];

	activeTerm = newTerm;
}

- (void) printPScodeInKey:sender
{
    [[keyTerm gnuView] print:sender];
}

- (void) setKeyTerm: (id) newTerm
{
	keyTerm = newTerm;

	[NameField setStringValue:[[keyTerm window] title]];
	[NameField selectText:self];
}

- (void) setKeyTitle:sender
{
	[[keyTerm window] setTitle:[NameField stringValue]];
	[[NameField window] performClose:self];
}

- (void) executePScode:(NSString *) PSstring termTitle:(NSString *) title;
{
        NSLog(@"Request for window: %@\n", title);

	if (activeTerm == nil) [self newGnuTerm:self];

	[[activeTerm gnuView] executePS:PSstring];
}



@end
