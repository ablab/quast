#import "GnuView.h"


@implementation GnuView

static NXCoord xsize= NEXT_XMAX;
static NXCoord ysize= NEXT_YMAX;

static int printing;
static void setprintsize();

- initFrame: (NXRect *)rects
{
    [super initFrame:rects];

	PSstring = NULL;
	[self display];

	return self;
}


	/* This is here to fix NeXT bug # 21973:  failure to free D.O. memory */
	/* Note:  personally I don't think this fixes it. */
- free
{
	if (PSstring) free(PSstring);
	[super free];

	return self;

}

- executePS:(char *) PStext
{
	if (PSstring) free(PSstring);
	PSstring = PStext;

	[window makeKeyAndOrderFront:self];

	[self display];

	return self;
}


- drawSelf:(const NXRect *) rects : (int) rectCount
{
    DPSContext d;

    d = DPSGetCurrentContext();

	if (!printing) {
							/* Clear Screen */
		PSsetgray(NX_WHITE);
		NXRectFill(&bounds);
							/* scale to gnuplot coords */
		[self setDrawSize:xsize:ysize];
	}
	else {
		setprintsize();
	}

	if (PSstring) DPSWritePostScript(d, PSstring, strlen(PSstring));

    DPSFlushContext(d);

	return self;
}


- printPSCode: sender
{
	printing = 1;
	[super printPSCode:sender];
	printing = 0;

	return self;
}

static void setprintsize()
{
	DPSContext d;
	NXRect *paperRect;
	float width, height;
	id prInfo;
	float xscale, yscale;

    d = DPSGetCurrentContext();
	prInfo = [NXApp printInfo];
	paperRect = (NXRect *) [prInfo paperRect];

	width = paperRect->size.width;
	height = paperRect->size.height;

							/* Leave margins on paper */

	DPSPrintf(d, "grestore\ngrestore\ngrestore\n");


	if ([prInfo orientation] == NX_LANDSCAPE) {
		DPSPrintf(d, "-90 rotate\n");
		DPSPrintf(d, "%g 0 translate\n", -1 * paperRect->size.width);
		DPSPrintf(d, "0 %g translate\n", paperRect->size.height/100);

		xscale = width/NEXT_XMAX*0.95;
		yscale = height/NEXT_YMAX*0.9;
		DPSPrintf(d, "%g %g scale\n", xscale, yscale);
	}
	else {
		xscale = width/NEXT_XMAX*0.95;
		yscale = height/NEXT_YMAX*0.95;
		DPSPrintf(d, "%g %g scale\n", xscale, yscale);
		DPSPrintf(d, "0 %g translate\n", paperRect->size.height/100);
	}

	DPSPrintf(d, "gsave\ngsave\n");

    DPSFlushContext(d);

	return;
}



@end


