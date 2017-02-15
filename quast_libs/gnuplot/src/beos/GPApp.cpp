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
#include <Roster.h>
#include <Window.h>
#include <View.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <FilePanel.h>
#include <Path.h>
#include <Entry.h>
#include <TextView.h>
#include <ScrollView.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "GPBitmap.h"
#include "GPView.h"
#include "GPApp.h"
#include "GPWindow.h"

// Application's signature

const char *APP_SIGNATURE				= "application/x-vnd.Xingo-gnuplotViewer";

BRect windowRect(50,50,600,400);

//
// main
//
// The main() function's only real job in a basic BeOS
// application is to create the BApplication object
// and run it.
//
int main(void) {
	GPApp theApp;		// The application object
	theApp.Run();
	return 0;
}

//
// GPApp::GPApp
//
// The constructor for the WEApp class.  This
// will create our window.
//
GPApp::GPApp()
			: BApplication(APP_SIGNATURE) {
	
	window_count = 0;			// No windows yet
	next_untitled_number = 1;	// Next window is "Untitled 1"
	
	// Create the Open file panel
//	openPanel = new BFilePanel;
}


//
// GPApp::MessageReceived
//
// Handle incoming messages.  In particular, handle the
// WINDOW_REGISTRY_ADD and WINDOW_REGISTRY_SUB messages.
//
void GPApp::MessageReceived(BMessage *message) {
	switch(message->what) {
		case WINDOW_REGISTRY_ADD:
			{
				bool need_id = false;
				BMessage reply(WINDOW_REGISTRY_ADDED);
				
				if (message->FindBool("need_id", &need_id) == B_OK) {
					if (need_id) {
						reply.AddInt32("new_window_number", next_untitled_number);
						next_untitled_number++;
					}
					window_count++;
				}
				reply.AddRect("rect", windowRect);
				windowRect.OffsetBy(20,20);
				message->SendReply(&reply);
				break;
			}
		case WINDOW_REGISTRY_SUB:
			window_count--;
			if (!window_count) {
				Quit();
			}
			break;
		case MENU_FILE_OPEN:
//			openPanel->Show();		// Show the file panel
			break;
		default:
			BApplication::MessageReceived(message);
			break;
	}
}

//
// GPApp::RefsReceived
//
// Handle a refs received message.
//
void GPApp::RefsReceived(BMessage *message) {
	entry_ref 	ref;		// The entry_ref to open
	status_t 	err;		// The error code
	int32		ref_num;	// The index into the ref list
	
	// Loop through the ref list and open each one
#if 0
	ref_num = 0;
	do {
		if ((err = message->FindRef("refs", ref_num, &ref)) != B_OK) {
			return;
		}
		new GPWindow(windowRect, &ref);
		ref_num++;
	} while (1);
#endif
}

void GPApp::ReadyToRun(void)
{
    io_thread = spawn_thread(&io_loop, "gnuplot io_loop", B_LOW_PRIORITY, NULL); 
    resume_thread(io_thread); 
}

int32 GPApp::io_loop(void* data)
{
	static plot_struct 	plot_array[MAX_WINDOWS];
	int32 res = 1;
	while(res)
		res = io_task(plot_array);
	return res;
}

int32 GPApp::io_task(plot_struct *plot_array) 
{ 
    char 	buf[256];
	struct 	plot_struct *plot = plot_array;
	FILE	*fp = stdin;
	BMessage msg(bmsgNewCmd);
	int		cnt = 0;

	while (fgets(buf, 256, fp)) {
//		printf("Got : %s", buf);
		switch (*buf) {
			case 'G':		/* enter graphics mode */
			{
				//printf("entering gfx mode\n");
				int plot_number = atoi(buf + 1);	/* 0 if none specified */

				if (plot_number < 0 || plot_number >= MAX_WINDOWS)
					plot_number = 0;

				//printf("plot for window number %d\n", plot_number);
				plot = plot_array + plot_number;
				prepare_plot(plot, plot_number);
				continue;
			}
			case 'E':		/* leave graphics mode / suspend */
			{
//				BMessage msg(bmsgBitmapDirty);
				msg.AddInt32("numcmds",cnt);
				msg.AddPointer("cmds", plot->commands);
				if(plot->window)
					plot->window->PostMessage(&msg);
//				printf("displaying %d cmds, %X at %X\n",cnt,plot->commands[0],&plot->commands[0]);
//				display(plot);
				cnt = 0;
				return 1;
			}
			case 'R':		/* leave x11 mode */
			{
				//printf("leaving gfx mode\n");
				return 0;
			}
			default:
			{
//				msg.AddString("cmd",buf);
//				plot->window->PostMessage(&msg);
				store_command(buf, plot);
				cnt++;
				continue;
			}
		}
	}
	/* get here if fgets fails */
	return (feof(fp) || ferror(fp)) ? 0 : 1;
} 

void GPApp::prepare_plot(plot_struct *plot, int term_number)
{
	int i;

	for (i = 0; i < plot->ncommands; ++i)
		free(plot->commands[i]);
	plot->ncommands = 0;

	if (!plot->posn_flags) {
		/* first time this window has been used - use default or -geometry
		 * settings
		 */
		plot->posn_flags = 1;
		plot->x = 50;
		plot->y = 20;
		plot->width = 400;
		plot->height = 400;
	}

	if (!plot->window) {
		windowRect.Set(plot->x,plot->y,plot->width,plot->height);
		plot->window = new GPWindow(windowRect);
	} else {
		BMessage msg(bmsgClrCmd);
		plot->window->PostMessage(&msg);
	}
}

void GPApp::display(plot_struct *plot)
{
	BMessage msg(bmsgBitmapDirty);
	if(plot->window)
		plot->window->PostMessage(&msg);
}

/* store a command in a plot structure */

void GPApp::store_command(char *buffer, plot_struct *plot)
{
	char *p;
//	BMessage msg(bmsgNewCmd);

//	FPRINTF((stderr, "Store in %d : %s", plot - plot_array, buffer));

	if (plot->ncommands >= plot->max_commands) {
		plot->max_commands = plot->max_commands * 2 + 1;
		plot->commands = (plot->commands)
			? (char **) realloc(plot->commands, plot->max_commands * sizeof(char *))
			: (char **) malloc(sizeof(char *));
	}
	p = (char *) malloc((unsigned) strlen(buffer) + 1);
	if (!plot->commands || !p) {
		fputs("gnuplot: can't get memory. aborted.\n", stderr);
		exit(1);
	}
	plot->commands[plot->ncommands++] = strcpy(p, buffer);

//	msg.AddString("cmd",buffer);
//	plot->window->PostMessage(&msg);
}

