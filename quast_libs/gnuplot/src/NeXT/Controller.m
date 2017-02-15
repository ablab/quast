#import <defaults/defaults.h>
#import "Controller.h"
#import "gnuviewController.h"
#import "GnuView.h"


@implementation Controller

- (id) activeTerm
{
	return activeTerm;
}


- appDidInit:sender
{
	id prInfo;
	NXRect *paperRect;

	static NXDefaultsVector GnuTermDefaults = {
		{"Width", "400"},
		{"Height", "300"},
		{"Backing", "Buffered"},
		{NULL}
	};


	activeTerm = keyTerm = nil;

	myConnection = [NXConnection registerRoot:self withName:"gnuplotServer"];
	[myConnection runFromAppKit];

	prInfo = [NXApp printInfo];
	paperRect = (NXRect *) [prInfo paperRect];
	[prInfo setOrientation:NX_LANDSCAPE andAdjust:YES];
	[prInfo setHorizCentered:YES];
	[prInfo setVertCentered:YES];

								/* Get user Preferences */
	NXRegisterDefaults("GnuTerm", GnuTermDefaults);

	[DefaultSize setStringValue:NXGetDefaultValue("GnuTerm","Width") at:0];
	[DefaultSize setStringValue:NXGetDefaultValue("GnuTerm","Height") at:1];

	if (!strcmp(NXGetDefaultValue("GnuTerm","Backing"), "Buffered")) {
		backing = NX_BUFFERED;
		[useBufferedSwitch setState:YES];
	}
	else {
		backing = NX_RETAINED;
		[useBufferedSwitch setState:NO];
	}

	gvList = [[List new] initCount:10];

	return self;
}

- newGnuTerm:sender
{
	NXRect frame;

	if ([gvList indexOf:keyTerm] != NX_NOT_IN_LIST) {
		[[keyTerm window] getFrame: &frame];
		NX_X(&frame) += 24;
		NX_Y(&frame) -= 24;
	}
	else {
		NX_WIDTH(&frame) = 	atof(NXGetDefaultValue("GnuTerm","Width"));
		NX_HEIGHT(&frame) = atof(NXGetDefaultValue("GnuTerm","Height"));
		NX_X(&frame) = 200;
		NX_Y(&frame) = 350;
	}

	if ([NXApp loadNibSection: "gnuview.nib" owner: self] == nil) {
		return nil;
	}
//	fprintf(stderr,"newGnuTerm: %g x %g\n",NX_WIDTH(&frame),NX_HEIGHT(&frame));

	[[activeTerm window] setBackingType:backing];
	[[activeTerm window] placeWindowAndDisplay: &frame];
	[self setKeyTerm:activeTerm];

	[gvList addObject:activeTerm];
	++wcnt;

	return activeTerm;
}


- setActiveTerm:(id) newTerm
{
	if (activeTerm != nil) [activeTerm deactivate:self];

	activeTerm = newTerm;
	[activeTerm GVactivate:self];

	return self;
}

- printPScodeInKey:sender
{
    [[keyTerm gnuView] printPSCode:sender];

    return self;


}

- setKeyTerm:newTerm
{
	keyTerm = newTerm;

	[NameField setStringValue:[[keyTerm window] title]];
	[NameField selectText:self];

	return self;
}

- setKeyTitle:sender
{
	[[keyTerm window] setTitle:[NameField stringValue]];
	[[NameField window] performClose:self];
	return self;
}

-  executePScode:(char *)PSstring termTitle:(char *)title
{
	int i, cnt;
	id test;
	char buf[50];

	//fprintf(stderr, "Request for window: %s\n", title);

	if (*title) {
										/* If the window exists, use it */
		cnt = [gvList count];
		for (i=0; i < cnt; ++i) {
			test = [gvList objectAt:i];
			if ( !strcmp([[test window] title], title)) {
				if (test != activeTerm) [self setActiveTerm:test];
				break;
			}
		}
										/* O.K., it doesn't exist, what now? */
		if (i == cnt) {
			[self newGnuTerm:self];
			[[activeTerm window] setTitle: title];
		}
	}
	else {
		if (activeTerm == nil) {
			[self newGnuTerm:self];
			sprintf(buf, "gnuplot %d", wcnt);
			[[activeTerm window] setTitle: buf];
		}
	}


	[[activeTerm window] makeKeyAndOrderFront: nil];
	[[activeTerm gnuView] executePS:PSstring];

	return activeTerm;
}

- termWillClose:sender
{
	[gvList removeObject:sender];
	if (activeTerm == sender) activeTerm =nil;

	return self;
}

- setDefaultGTSize:sender
{
	NXRect frame;

	if (sender == useKeyButton) {
		fprintf(stderr, "useKey\n");
		if ([gvList indexOf:keyTerm] != NX_NOT_IN_LIST) {
			[[keyTerm window] getFrame: &frame];
			[DefaultSize setFloatValue:NX_WIDTH(&frame) at:0];
			[DefaultSize setFloatValue:NX_HEIGHT(&frame) at:1];
		}
	}

	NXWriteDefault("GnuTerm", "Width",  [DefaultSize stringValueAt:0]);
	NXWriteDefault("GnuTerm", "Height", [DefaultSize stringValueAt:1]);


	fprintf(stderr, "setDefaultGTSize: %s x %s\n",
		[DefaultSize stringValueAt:0],[DefaultSize stringValueAt:1]);

	return self;
}

- setUseBuffered:sender
{
	if ([sender state] == YES) {
		backing = NX_BUFFERED;
		NXWriteDefault("GnuTerm", "Backing", "Buffered");
	}
	else {
		backing = NX_RETAINED;
		NXWriteDefault("GnuTerm", "Backing", "Retained");
	}
	[[activeTerm window] setBackingType:backing];


	return self;
}


- closeAll:sender
{
	while([gvList count]) [[[gvList objectAt:0] window] performClose:self];
	return self;
}
- miniaturizeAll:sender
{
	int i, cnt;

	cnt = [gvList count];

	for (i=0; i < cnt; ++i)
		[[[gvList objectAt:i] window] performMiniaturize:self];

	return self;
}

@end
