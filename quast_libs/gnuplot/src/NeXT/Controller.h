#import <appkit/appkit.h>


@interface Controller:Object
{
	id activeTerm;
	id keyTerm;
	id NameField;
	NXConnection *myConnection;
	int wcnt;
	int backing;
	id gvList;
	id DefaultSize;				/* textfield matrix */
	id useKeyButton;
	id useBufferedSwitch;
}

- newGnuTerm:sender;
- appDidInit:sender;
- activeTerm;
- setActiveTerm:newView;
- setKeyTerm:newTerm;
- setKeyTitle:sender;
- printPScodeInKey:sender;
- executePScode:(char *)PSstring termTitle:(char *)title;
- termWillClose:sender;
- setDefaultGTSize:sender;
- setUseBuffered:sender;
- closeAll:sender;
- miniaturizeAll:sender;


@end


