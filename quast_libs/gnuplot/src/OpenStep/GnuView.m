#import "GnuView.h"

@implementation GnuView

static float xsize= NEXT_XMAX/10+50;
static float ysize= NEXT_YMAX/10+50;

static int printing;
static void setprintsize();

- initWithFrame:(NSRect)rects
{
    [super initWithFrame:rects];

    PSstring = @"";
    [self display];

    return self;
}


/* This is here to fix NeXT bug # 21973:  failure to free D.O. memory */
/* Note:  personally I don't think this fixes it. */
- (void)dealloc
{
    [super dealloc];
    [PSstring release];
}

- executePS:(NSString *) PStext
{
    if (PSstring)
	[PSstring release];
    PSstring = PStext;
    [PSstring retain];

    [[self window] makeKeyAndOrderFront:self];
    [self display];

    return self;
}

- (void) drawRect:(NSRect) rect
{
    DPSContext d;

    d = DPSGetCurrentContext();

    if (!printing) { /* Clear Screen */
	PSsetgray(NSWhite);
	NSRectFill([self bounds]);
	[self setBoundsSize:NSMakeSize(xsize, ysize)];    /* scale to gnuplot coords */
    }
    else {
	setprintsize();
    }
    DPSWritePostScript(d, [PSstring cString], [PSstring length]);
    DPSFlushContext(d);
}


- (void)print:(id)sender
{
	printing = 1;
	[super print:sender];
	printing = 0;
}

static void setprintsize()
{
    DPSContext d;
    NSSize paperSize;
    id prInfo;

    d = DPSGetCurrentContext();
#warning PrintingConversion:  The current PrintInfo object now depends on context. '[NSPrintInfo sharedPrintInfo]' used to be '[NSApp printInfo]'. This might want to be [[NSPrintOperation currentOperation] printInfo] or possibly [[PageLayout new] printInfo].

    prInfo = [NSPrintInfo sharedPrintInfo];
    paperSize = [prInfo paperSize];

    DPSPrintf(d, "grestore\ngrestore\ngrestore\n");

    if ([prInfo orientation] == NSLandscapeOrientation) {
        DPSPrintf(d, "-90 rotate\n");
        DPSPrintf(d, "%g 0 translate\n", -1.0 * paperSize.width);
        DPSPrintf(d, "0 %g translate\n", paperSize.height/20);
    }
    else {
        DPSPrintf(d, "%g %g scale\n", paperSize.width/paperSize.height, paperSize.height/paperSize.width);
    }

    DPSPrintf(d, "gsave\ngsave\n");

    DPSFlushContext(d);

    return;
}

@end


