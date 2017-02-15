#import <AppKit/AppKit.h>
#import "Controller.h"
#import "GnuView.h"

@interface GnuViewController:NSObject
{
    id window;
    id gnuView; // GnuView instance
    id activateButton;
    id controller; // Controller instance
}

- init;
- window;
- (BOOL)windowShouldClose:(id)sender;
#warning OK - NotificationConversion: windowDidBecomeMain:(NSNotification *)notification is an NSWindow notification method (used to be a delegate method); delegates of NSWindow are automatically set to observe this notification; subclasses of NSWindow do not automatically receive this notification
- (void)windowDidBecomeMain:(NSNotification *)notification;
- gnuView;
- activatePushed:sender;
- deactivate:sender;

@end
