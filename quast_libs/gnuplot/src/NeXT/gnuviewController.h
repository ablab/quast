
#import <appkit/appkit.h>

@interface gnuviewController:Object
{
    id window;
	id gnuView;
	id activateButton;
	id controller;
}


- window;
- windowWillClose:sender;
- windowDidBecomeMain:sender;
- gnuView;
- activatePushed:sender;
- deactivate:sender;
- GVactivate:sender;
- largerPushed:sender;
- smallerPushed:sender;


@end
