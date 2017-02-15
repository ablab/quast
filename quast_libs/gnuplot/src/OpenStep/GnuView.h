#import <AppKit/AppKit.h>

#define NEXT_XMAX 7200
#define NEXT_YMAX 5040

@interface GnuView:NSView
{
	NSString *PSstring;
}

- (void) drawRect:(NSRect)rect;
- initWithFrame:(NSRect)rects;
- executePS:(NSString *) PStext;
- (void) dealloc;


@end
