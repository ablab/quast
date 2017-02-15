#import "gnuviewController.h"
#import "Controller.h"

@implementation gnuviewController

- window
{
	return window;
}

- gnuView
{
	return gnuView;
}

- windowWillClose:sender
{
	[sender setDelegate: nil];
	[controller termWillClose:self];

	return self;
}

- windowDidBecomeMain:sender
{
	[controller setKeyTerm:self];

	return self;
}

- activatePushed:sender
{
	if ([sender state] == 1) {
		[controller setActiveTerm: self];
		[sender setTitle:"--- ACTIVE ---"];
	}
	else {
		[controller setActiveTerm:nil];
		[sender setTitle:"Activate"];
	}

	return self;
}

- deactivate:sender
{
	[activateButton setState:0];
	[activateButton setTitle:"Activate"];

	return self;
}

- GVactivate:sender
{
	[activateButton setState:1];
	[activateButton setTitle:"--- ACTIVE ---"];

	return self;
}

- largerPushed:sender
{
	NXRect oldFrame;
	double factor;

	[[window contentView] getFrame:&oldFrame];

	factor = 1.3;

	[window sizeWindow:oldFrame.size.width*factor:oldFrame.size.height*factor];
	[[window contentView] display];

	return self;
}

- smallerPushed:sender
{
	NXRect oldFrame;
	double factor;

	[[window contentView] getFrame:&oldFrame];

	factor = 1.3;

	[window sizeWindow:oldFrame.size.width/factor:oldFrame.size.height/factor];
	[[window contentView] display];
	return self;
}



@end
