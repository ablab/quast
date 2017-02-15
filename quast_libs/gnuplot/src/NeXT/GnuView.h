#import <appkit/appkit.h>

						/* This is the coordinate system defined in next.trm.						*/
#define NEXT_XMAX 640
#define NEXT_YMAX 480

@interface GnuView:View
{
	char *PSstring;
}

- drawSelf:(const NXRect *) rects : (int) rectCount;
- initFrame: (NXRect *)rects;
- executePS:(char *)PStext;
- free;


@end
