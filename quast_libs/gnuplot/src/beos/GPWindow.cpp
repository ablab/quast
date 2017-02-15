/*[
 * Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
]*/

#include <Application.h>
#include <Messenger.h>
#include <Message.h>
#include <Alert.h>
#include <Roster.h>
#include <Window.h>
#include <View.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuField.h>
#include <Entry.h>
#include <Path.h>
#include <Box.h>
#include <MenuItem.h>
#include <TextView.h>
#include <FilePanel.h>
#include <ScrollView.h>
#include <OutlineListView.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "GPApp.h"
#include "GPBitmap.h"
#include "GPView.h"
#include "GPWindow.h"

// Constructs the window we'll be drawing into.
//
GPWindow::GPWindow(BRect frame)
			: BWindow(frame, " ", B_TITLED_WINDOW, 0) {
	_InitWindow();
	Show();
}


// Create a window from a file.
//
GPWindow::GPWindow(BRect frame, plot_struct *plot)
			: BWindow(frame, "Untitled ", B_TITLED_WINDOW, 0) {
	
	_InitWindow();
	
	Show();
}


void GPWindow::_InitWindow(void) {
	BRect r, rtool, plotframe;
	BMenu *menu;
	BMenuItem *item;

	 
	// Initialize variables
	
	savemessage = NULL;			// No saved path yet
	r = rtool = plotframe = Bounds();
//	rtool = Bounds();
//	plotframe = Bounds();
	
	// Add the menu bar
	menubar = new BMenuBar(r, "menu_bar");

	// Add File menu to menu bar
	menu = new BMenu("File");
	menu->AddItem(new BMenuItem("New", new BMessage(MENU_FILE_NEW), 'N'));
	
	menu->AddItem(item=new BMenuItem("Open" B_UTF8_ELLIPSIS, new BMessage(MENU_FILE_OPEN), 'O'));
	item->SetTarget(be_app);
	menu->AddItem(new BMenuItem("Close", new BMessage(MENU_FILE_CLOSE), 'W'));
	menu->AddSeparatorItem();
	menu->AddItem(saveitem=new BMenuItem("Save", new BMessage(MENU_FILE_SAVE), 'S'));
	saveitem->SetEnabled(false);
	menu->AddItem(new BMenuItem("Save as" B_UTF8_ELLIPSIS, new BMessage(MENU_FILE_SAVEAS)));
					
	menu->AddSeparatorItem();
	menu->AddItem(item=new BMenuItem("Page Setup" B_UTF8_ELLIPSIS, new BMessage(MENU_FILE_PAGESETUP)));
	item->SetEnabled(false);
	menu->AddItem(item=new BMenuItem("Print" B_UTF8_ELLIPSIS, new BMessage(MENU_FILE_PRINT), 'P'));
	item->SetEnabled(false);

	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Quit", new BMessage(MENU_FILE_QUIT), 'Q'));
	menubar->AddItem(menu);
	
	// Attach the menu bar to the window
	AddChild(menubar);
	
	// Add the plot view
//	plotframe.left +=rtool.right+5;
	plotframe.top = menubar->Bounds().bottom+2;
//	plotframe.right -= B_V_SCROLL_BAR_WIDTH;
//	plotframe.bottom -= B_H_SCROLL_BAR_HEIGHT;
	
	BRect plotrect = plotframe;
	plotrect.OffsetTo(B_ORIGIN);
	r.InsetBy(3.0,3.0);
	
	plotview = new GPView(plotframe, B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_PULSE_NEEDED, NULL);
//	plotview = new BView(plotframe, "test", B_FOLLOW_ALL_SIDES, B_WILL_DRAW|B_PULSE_NEEDED);
	AddChild(plotview);
	//help menu
	menu = new BMenu("Help");
	menu->AddItem(new BMenuItem("Help",new BMessage(MENU_HELP_REQUESTED)));
	menu->AddItem(new BMenuItem("About...",new BMessage(MENU_HELP_ABOUT)));
	
	menubar->AddItem(menu);
	
	// Create the save filepanel for this window
	
	savePanel = new BFilePanel(B_SAVE_PANEL, new BMessenger(this), NULL, B_FILE_NODE, false);

	// Tell the application that there's one more window
	// and get the number for this untitled window.
	Register(true);
	Minimize(false);		// So Show() doesn't really make it visible
}


//
// GPWindow::FrameResized
//
// Adjust the size of the BTextView's text rectangle
// when the window is resized.
//
void GPWindow::FrameResized(float width, float height) {
	BRect plotrect = plotview->Bounds();
	
	plotrect.right = plotrect.left + (width - 3.0);
//	plotview->SetTextRect(plotrect);
}


//
// GPWindow::~GPWindow
//
// Destruct the window.  This calls Unregister().
//
GPWindow::~GPWindow() {
	Unregister();
	if (savemessage) {
		delete savemessage;
	}
	delete savePanel;
}


//
// GPWindow::MessageReceived
//
// Called when a message is received by our
// application.
//
void GPWindow::MessageReceived(BMessage *message) {

	switch(message->what) {

		case WINDOW_REGISTRY_ADDED:
			{
				char s[22];
				BRect rect;
				if (message->FindInt32("new_window_number", &window_id) == B_OK) {
					if (!savemessage) {		// if it's untitled
						sprintf(s, "File%ld.html", window_id);
						SetTitle(s);
					}
				}
				if (message->FindRect("rect", &rect) == B_OK) {
					MoveTo(rect.LeftTop());
					ResizeTo(rect.Width(), rect.Height());
				}
				Minimize(false);
			}
			break;
			
		case MENU_FILE_NEW:
			{
				BRect r;
				r = Frame();
				new GPWindow(r);
			}
			break;
		
		case MENU_FILE_CLOSE:
			Quit();
			break;
		case MENU_FILE_QUIT:
			be_app->PostMessage(B_QUIT_REQUESTED);
			break;
		case MENU_FILE_SAVEAS:
			savePanel->Show();
			break;
		case MENU_FILE_SAVE:
			Save(NULL);
			break;
		case B_SAVE_REQUESTED:
			Save(message);
			break;
		
		case MENU_HELP_REQUESTED:{
		int arg_c;
		char **_arg;
		arg_c=1;
		_arg = (char **)malloc(sizeof(char *) * (arg_c+ 1));
		_arg[0]="/boot/home/peojects/WebEditor/help.html";
		_arg[1]=NULL;
		be_roster->Launch("application/x-vnd.Be-NPOS",arg_c,_arg,NULL);
		free(_arg);
		}	
		break; 
		
		case MENU_HELP_ABOUT:{
			BAlert		*alert;
			alert= new BAlert("About ", "WebEditor for BeOS™\n©François Jouen 1999.\ne-mail:jouen@epeire.univ-rouen.fr", "OK");
			alert->Go();
	
		}	
		break;				
					
//		case B_MOUSE_UP:
//			printf("gor something\n");
//		break;				

		case bmsgNewCmd:
		case bmsgClrCmd:
		case bmsgBitmapDirty:
		case bmsgBitmapResize:
//			printf("gor something\n");
			plotview->MessageReceived(message);
		break;				
					
		default:
			BWindow::MessageReceived(message);
			break;
	}
}


//
// GPWindow::Register
//
// Since MessageWorld can have multiple windows and
// we need to know when there aren't any left so the
// application can be shut down, this function is used
// to tell the application that a new window has been
// opened.
//
// If the need_id argument is true, we'll specify true
// for the "need_id" field in the message we send; this
// will cause the application to send back a
// WINDOW_REGISTRY_ADDED message containing the window's
// unique ID number.  If this argument is false, we won't
// request an ID.
//
void GPWindow::Register(bool need_id) {
	BMessenger messenger(APP_SIGNATURE);
	BMessage message(WINDOW_REGISTRY_ADD);
	
	message.AddBool("need_id", need_id);
	messenger.SendMessage(&message, this);
}


//
// GPWindow::Unregister
//
// Unregisters a window.  This tells the application that
// one fewer windows are open.  The application will
// automatically quit if the count goes to zero because
// of this call.
//
void GPWindow::Unregister(void) {
	BMessenger messenger(APP_SIGNATURE);
	
	messenger.SendMessage(new BMessage(WINDOW_REGISTRY_SUB));
}


//
// GPWindow::QuitRequested
//
// Here we just give permission to close the window.
//
bool GPWindow::QuitRequested() {
	return true;
}


//
// GPWindow::Save
//
// Save the contents of the window.  The message specifies
// where to save it (see BFilePanel in the Storage Kit chapter
// of the Be Book).
//
status_t GPWindow::Save(BMessage *message) {
	entry_ref ref;		// For the directory to save into
	status_t err = B_OK;		// For the return code

	return err;
}
