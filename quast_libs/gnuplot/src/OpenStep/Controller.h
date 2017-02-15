#import <AppKit/AppKit.h>

@interface Controller:NSObject
{
	id activeTerm;
	id keyTerm;
	id NameField;
	NSConnection *myConnection;
	float offset;
	int gnuviewNum;
}

- newGnuTerm:sender;
- (void) applicationDidFinishLaunching:(NSNotification *) notification;
- activeTerm;
- (void) setActiveTerm:newView;
- (void) setKeyTerm:newTerm;
- (void) setKeyTitle:sender;
- (void) printPScodeInKey:sender;
- (void) executePScode:(NSString *) PSstring termTitle:(NSString *) title;

@end


