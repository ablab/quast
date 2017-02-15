/*
 * $Id: wxt_gui.h,v 1.48.2.8 2016/08/26 04:16:01 sfeam Exp $
 */

/* GNUPLOT - wxt_gui.h */

/*[
 * Copyright 2005,2006   Timothee Lecomte
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
 *
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * GNU General Public License Version 2 or later (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above. If you wish to allow
 * use of your version of this file only under the terms of the GPL and not
 * to allow others to use your version of this file under the above gnuplot
 * license, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the GPL. If you do not
 * delete the provisions above, a recipient may use your version of this file
 * under either the GPL or the gnuplot license.
]*/

/* -----------------------------------------------------
 * The following code uses the wxWidgets library, which is
 * distributed under its own licence (derivated from the LGPL).
 *
 * You can read it at the following address :
 * http://www.wxwidgets.org/licence.htm
 * -----------------------------------------------------*/

/* ------------------------------------------------------
 * This file is the C++ header dedicated to wxt_gui.cpp
 * Everything here is static.
 * ------------------------------------------------------*/

/* ===========================================================
 * includes
 * =========================================================*/

#ifndef GNUPLOT_WXT_H
# define GNUPLOT_WXT_H

/* NOTE : Order of headers inclusion :
 * - wxWidgets headers must be included before Windows.h
 * to avoid conflicts on Unicode macros,
 * - then stdfn.h must be included, to obtain definitions from config.h
 * - then the rest */

/* main wxWidgets header */
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
# include <wx/wx.h>
#endif /* WX_PRECOMP */

/* clipboard functionnality */
#include <wx/dataobj.h>
#include <wx/clipbrd.h>

/* Save File dialog */
#include <wx/filedlg.h>

/* wxImage facility */
#include <wx/image.h>

/* double buffering facility for the drawing context */
#include <wx/dcbuffer.h>

/* system options used with wxMSW to workaround PNG problem in the toolbar */
#include <wx/sysopt.h>

/* wxConfig stuff */
#include <wx/config.h>

/* wxGenericValidator */
#include <wx/valgen.h>

/* wxMemoryInputStream, for the embedded PNG icons */
#include <wx/mstream.h>

/* Debugging support, required to turn off asserts */
#include <wx/debug.h>

/* c++ vectors and lists, used to store gnuplot commands */
#include <vector>
#include <list>

/* suprisingly Cocoa version of wxWidgets does not define _Bool ! */
#ifdef __WXOSX_COCOA__
#define _Bool bool
#endif

extern "C" {
/* for interactive */
# include "plot.h"
/* for stdfn.h, JUSTIFY, encoding, *term definition, color.h */
# include "term_api.h"
/* for do_event declaration */
# include "mouse.h"
/* for rgb functions */
# include "getcolor.h"
/* for paused_for_mouse, PAUSE_BUTTON1 and friends */
# include "command.h"
/* for int_error */
# include "util.h"
}

/* if the gtk headers are available, use them to tweak some behaviours */
#if defined(__WXGTK__)&&defined(HAVE_GTK)
# define USE_GTK
#endif

/* With wxGTK, we use a different cairo surface starting from gtk28 */
#if defined(__WXGTK__)
# define IMAGE_SURFACE
#endif

/* by default, enable IMAGE_SURFACE */
#if !defined(GTK_SURFACE)&&!defined(IMAGE_SURFACE)&&!defined(__WXMSW__)
# define IMAGE_SURFACE
#endif

/* temporarly undef GTK_SURFACE for two reasons :
 * - because of a CAIRO_OPERATOR_SATURATE bug,
 * - because as for now, it is slower than the pure image surface,
 * (multiple copies between video memory and main memory for operations that are
 * not supported by the X server) */
#ifdef GTK_SURFACE
# undef GTK_SURFACE
# define IMAGE_SURFACE
#endif

/* depending on the platform, and mostly because of the Windows terminal which
 * already has its event loop, we may or may not be multithreaded */
#ifndef WXT_MONOTHREADED
#if defined(__WXGTK__)
# define WXT_MULTITHREADED
#elif defined(__WXMSW__) || defined(__WXMAC__)
# define WXT_MONOTHREADED
#else
# error "wxt does not know if this platform has to be single- or multi-threaded"
#endif
#endif

extern "C" {
/* Windows native backend,
 * redefinition of fprintf, getch...
 * console window */
# ifdef _Windows
#  ifndef _WIN32_WINNT 
#   define _WIN32_WINNT 0x0501
#  endif
#  include <windows.h>
#  include "win/wtext.h"
#  include "win/winmain.h"
# endif

/* for cairo_t */
# include <cairo.h>

#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo/cairo-svg.h>
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo/cairo-pdf.h>
#endif

# ifdef USE_GTK
#  include <gdk/gdk.h>
#  include <gtk/gtk.h>
# endif

# ifdef _Windows
#  include <cairo-win32.h>
# endif

/* to avoid to receive SIGINT in wxWidgets threads,
 * already included unconditionally in plot.c,
 * only needed here when using WXGTK
 * (or at least not needed on Windows) */
# include <signal.h>
}

/* interaction with wxt.trm(wxt_options) : plot number, enhanced state.
 * Communication with gnuplot (wxt_exec_event)
 * Initialization of the library, and checks */
#include "wxt_term.h"
/* drawing facility */
#include "gp_cairo.h"
#include "gp_cairo_helpers.h"

/* ======================================================================
 * declarations
 * ====================================================================*/

#ifdef WXT_MULTITHREADED

#if defined(WX_NEEDS_XINITTHREADS) && defined(X11)
#include <X11/Xlib.h>	/* Magic fix for linking against wxgtk3.0 */
#endif

/* thread class, where the gui loop runs.
 * Not needed with Windows, where the main loop
 * already processes the gui messages */
class wxtThread : public wxThread
{
public:
	wxtThread() : wxThread(wxTHREAD_JOINABLE) {};

	/* thread execution starts in the following */
	void *Entry();
};

/* instance of the thread */
static wxtThread * thread;
#endif /* WXT_MULTITHREADED */

DECLARE_LOCAL_EVENT_TYPE(wxExitLoopEvent, -1)
DEFINE_LOCAL_EVENT_TYPE(wxExitLoopEvent)

DECLARE_LOCAL_EVENT_TYPE(wxCreateWindowEvent, -1)
DEFINE_LOCAL_EVENT_TYPE(wxCreateWindowEvent)

#ifdef USE_MOUSE
DECLARE_LOCAL_EVENT_TYPE(wxStatusTextEvent, -1)
DEFINE_LOCAL_EVENT_TYPE(wxStatusTextEvent)
#endif /* USE_MOUSE */

/* Define a new application type, each gui should derive a class from wxApp */
class wxtApp : public wxApp
{
public:
#if defined(WXT_MULTITHREADED) && defined(WX_NEEDS_XINITTHREADS) && defined(X11)
	/* Magic fix needed by wxgtk3.0 */
        wxtApp() : wxApp() { XInitThreads(); } 
#endif 

	/* This one is called just after wxWidgets initialization */
	bool OnInit();
	/* cleanup on exit */
	int OnExit();
	/* event handler */
	void OnExitLoop( wxCommandEvent &event );
	/* event handler */
	void OnCreateWindow( wxCommandEvent &event );
	/* wrapper for AddPendingEvent or ProcessEvent */
	void SendEvent( wxEvent &event);
private:
	/* any class wishing to process wxWidgets events must use this macro */
	DECLARE_EVENT_TABLE()

	/* load a toolbar icon */
	void LoadPngIcon(const unsigned char *embedded_png, int length, int icon_number);
	/* load a cursor */
	void LoadCursor(wxCursor &cursor, const char* xpm_bits[]);
};

/* IDs for gnuplot commands */
typedef enum wxt_gp_command_t {
	command_color = 1,
	command_linetype,
	command_linestyle,
	command_move,
	command_vector,
	command_put_text,
	command_enhanced_init,
	command_enhanced_open,
	command_enhanced_writec,
	command_enhanced_flush,
	command_enhanced_finish,
	command_set_font,
	command_justify,
	command_point,
	command_pointsize,
	command_linewidth,
	command_text_angle,
	command_fillbox,
	command_filled_polygon,
	command_image,
	command_layer,
	command_hypertext
#ifdef EAM_BOXED_TEXT
	,command_boxed_text
#endif
	,command_dashtype
} wxt_gp_command_t;

/* base structure for storing gnuplot commands */
typedef struct gp_command {
	enum wxt_gp_command_t command;
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
	unsigned int x3;
	unsigned int y3;
	unsigned int x4;
	unsigned int y4;
	int integer_value;
	int integer_value2;
	double double_value;
	double double_value2;
	t_dashtype *dashpattern;
	char *string;
	gpiPoint *corners;
	enum JUSTIFY mode;
	rgb_color color;
	unsigned int * image;
} gp_command;

/* declare a type for our list of gnuplot commands */
typedef std::list<gp_command> command_list_t;

/* panel class : this is the space between the toolbar
 * and the status bar, where the plot is actually drawn. */
class wxtPanel : public wxPanel
{
public :
	/* constructor*/
	wxtPanel( wxWindow* parent, wxWindowID id, const wxSize& size );

	/* event handlers (these functions should _not_ be virtual)*/
	void OnPaint( wxPaintEvent &event );
	void OnEraseBackground( wxEraseEvent &event );
	void OnMouseLeave( wxMouseEvent &event );
	void OnSize( wxSizeEvent& event );
	void OnMotion( wxMouseEvent& event );
	void OnLeftDown( wxMouseEvent& event );
	void OnLeftUp( wxMouseEvent& event );
	void OnMiddleDown( wxMouseEvent& event );
	void OnMiddleUp( wxMouseEvent& event );
	void OnRightDown( wxMouseEvent& event );
	void OnRightUp( wxMouseEvent& event );
	void OnMouseWheel( wxMouseEvent& event );
	void OnKeyDownChar( wxKeyEvent& event );

	void UpdateModifiers( wxMouseEvent& event );
	void RaiseConsoleWindow();
	void DrawToDC( wxDC& dc, wxRegion& region );
	void Draw();

	void wxt_settings_queue(TBOOLEAN antialiasing,
					TBOOLEAN oversampling,
					int hinting_setting);
	void wxt_settings_apply();

	/* list of commands sent by gnuplot */
	command_list_t command_list;
	/* mutex protecting this list */
	wxMutex command_list_mutex;
	/* method to clear the command list, free the allocated memory */
	void ClearCommandlist();

#ifdef USE_MOUSE
	/* mouse and zoom events datas */
	bool wxt_zoombox;
	int mouse_x, mouse_y;
	int zoom_x1, zoom_y1;
	wxString zoom_string1, zoom_string2;
	bool wxt_ruler;
	double wxt_ruler_x, wxt_ruler_y;
	bool wxt_ruler_lineto;
	/* modifier_mask for wxKeyEvents */
	int modifier_mask;
#endif

	/* cairo context creation */
	void wxt_cairo_create_context();
	void wxt_cairo_free_context();
	/* platform-dependant cairo context creation */
	int wxt_cairo_create_platform_context();
	void wxt_cairo_free_platform_context();

#ifdef IMAGE_SURFACE
	void wxt_cairo_create_bitmap();
#endif

	/* functions used to process the command list */
	void wxt_cairo_refresh();
	void wxt_cairo_exec_command(gp_command command);
	void wxt_cairo_draw_hypertext();

	/* the plot structure, defined in gp_cairo.h */
	plot_struct plot;

	/* destructor*/
	~wxtPanel();

private:
	/* any class wishing to process wxWidgets events must use this macro */
	DECLARE_EVENT_TABLE()

	bool settings_queued;
	TBOOLEAN antialiasing_queued;
	TBOOLEAN oversampling_queued;
	int hinting_queued;
	wxMutex mutex_queued;

#ifdef USE_MOUSE
	/* watches for time between mouse clicks */
	wxStopWatch left_button_sw;
	wxStopWatch right_button_sw;
	wxStopWatch middle_button_sw;
#endif /*USE_MOUSE*/

	/* cairo surfaces, which depends on the implementation */
#if defined(GTK_SURFACE)
	GdkPixmap *gdkpixmap;
#elif defined(__WXMSW__)
	HDC hdc;
	HBITMAP hbm;
#else /* generic 'image' surface */
	unsigned int *data32;
	wxBitmap* cairo_bitmap;
#endif
};


/* class implementing the configuration dialog */
class wxtConfigDialog : public wxDialog
{
  public :
	/* constructor*/
	wxtConfigDialog(wxWindow* parent);

	void OnRendering( wxCommandEvent& event );
	void OnButton( wxCommandEvent& event );
	void OnClose( wxCloseEvent& event );

	/* destructor*/
	~wxtConfigDialog() {};
  private:
	/* any class wishing to process wxWidgets events must use this macro */
	DECLARE_EVENT_TABLE()

	/* these two elements are enabled/disabled dynamically */
	wxSlider *slider;
	wxStaticText *text_hinting;

	/* settings */
	bool raise_setting;
	bool persist_setting;
	bool ctrl_setting;
	bool toggle_setting;
	bool redraw_setting;
	/* rendering_setting :
	 * 0 = no antialiasing, no oversampling
	 * 1 = antialiasing, no oversampling
	 * 2 = antialiasing and oversampling
	 * Note that oversampling without antialiasing makes no sense */
	int rendering_setting;
	int hinting_setting;
};


/* Define a new frame type: this is our main frame */
class wxtFrame : public wxFrame
{
public:
	/* constructor*/
	wxtFrame( const wxString& title, wxWindowID id );

	/* event handlers (these functions should _not_ be virtual)*/
	void OnClose( wxCloseEvent& event );
	void OnSize( wxSizeEvent& event );
	void OnCopy( wxCommandEvent& event );
	void OnExport( wxCommandEvent& event );
#ifdef USE_MOUSE
	void OnReplot( wxCommandEvent& event );
	void OnToggleGrid( wxCommandEvent& event );
	void OnZoomPrevious( wxCommandEvent& event );
	void OnZoomNext( wxCommandEvent& event );
	void OnAutoscale( wxCommandEvent& event );
	void OnSetStatusText( wxCommandEvent& event );
#endif /*USE_MOUSE*/
	void OnConfig( wxCommandEvent& event );
	void OnHelp( wxCommandEvent& event );

	/* wrapper for AddPendingEvent or ProcessEvent */
	void SendEvent( wxEvent &event);

	/* destructor*/
	~wxtFrame();

	wxtPanel * panel;
	bool config_displayed;
	wxToolBar * toolbar;

private:
	wxtConfigDialog * config_dialog;

	/* any class wishing to process wxWidgets events must use this macro */
	DECLARE_EVENT_TABLE()
};

/* IDs for the controls and the menu commands, and for wxEvents */
enum {
/* start at wxID_HIGHEST to avoid collisions */
Toolbar_CopyToClipboard = wxID_HIGHEST,
Toolbar_ExportToFile,
Toolbar_Replot,
Toolbar_ToggleGrid,
Toolbar_ZoomPrevious,
Toolbar_ZoomNext,
Toolbar_Autoscale,
Toolbar_Config,
Toolbar_Help,
Config_Rendering,
Config_OK,
Config_APPLY,
Config_CANCEL
};

/* array of toolbar icons */
#define ICON_NUMBER 8
static wxBitmap* toolBarBitmaps[ICON_NUMBER];

/* frames icons in the window manager */
static wxIconBundle icon;

/* mouse cursors */
static wxCursor wxt_cursor_cross;
static wxCursor wxt_cursor_right;
static wxCursor wxt_cursor_rotate;
static wxCursor wxt_cursor_size;

/* wxt_abort_init is set to true if there is an error when
 * wxWidgets is initialized, for example if the X server is unreachable.
 * If there has been an error, we should not try to initialize again,
 * because the following try-out will return that it succeeded,
 * although this is false. */
static bool wxt_abort_init = false;

/* Sometimes, terminal functions are called although term->init() has not been called before.
 * It's the case when you hit 'set terminal wxt' twice.
 * So, we check for earlier initialisation.
 * External module, such as cairo, can check for status!=0, instead of using the enums defined here.
 * This is used to process interrupt (ctrl-c) */
enum {
STATUS_OK = 0,
STATUS_UNINITIALIZED,
STATUS_INCONSISTENT,
STATUS_INTERRUPT_ON_NEXT_CHECK,
STATUS_INTERRUPT
};
static int wxt_status = STATUS_UNINITIALIZED;

/* wxt_handling_persist is set to true after a child process is created for the
 * "persist-effect", and starts handling events directly without having two
 * separate threads. */
static bool wxt_handling_persist = false;

/* structure to store windows and their ID
 * also used to pass titles and waiting condition to the GUI thread on
 * window creation */
typedef struct wxt_window_t {
	wxWindowID id;
	wxtFrame * frame;
	wxString title;
	wxMutex * mutex;
	wxCondition * condition;
	int axis_mask;
	wxt_axis_state_t axis_state[4];
} wxt_window_t;

/* list of already created windows */
static std::vector<wxt_window_t> wxt_window_list;

/* given a window number, return the window structure */
static wxt_window_t* wxt_findwindowbyid(wxWindowID);

/* pointers to currently active instances */
static wxt_window_t *wxt_current_window;
static wxtPanel *wxt_current_panel;
static plot_struct *wxt_current_plot;

/* push a command to the commands list */
static void wxt_command_push(gp_command command);

#ifdef USE_MOUSE
/* routine to send an event to gnuplot
 * returns true if the event has really been processed - it will
 * not if the window is not the current one. */
static bool wxt_exec_event(int type, int mx, int my, int par1, int par2, wxWindowID id);
static void wxt_check_for_toggle(unsigned int x, unsigned int y);
static void wxt_check_for_anchors(unsigned int x, unsigned int y);

/* process one event, returns true if it ends the pause */
static bool wxt_process_one_event(struct gp_event_t *);

# ifdef WXT_MULTITHREADED
/* set of pipe file descriptors for event communication with the core */
int wxt_event_fd = -1;
int wxt_sendevent_fd = -1;
# endif /* WXT_MULTITHREADED */
#endif /*USE_MOUSE*/

/* helpers to handle the issues of the default Raise() and Lower() methods */
static void wxt_raise_window(wxt_window_t* window, bool force);
static void wxt_lower_window(wxt_window_t* window);

/* cleanup  on exit : close all created windows, delete thread if necessary */
static void wxt_cleanup();

/* helpers for gui mutex handling : they do nothing in WXMSW */
static void wxt_MutexGuiEnter();
static void wxt_MutexGuiLeave();

/* interrupt stuff */
static void (*original_siginthandler) (int);
static void wxt_sigint_handler(int WXUNUSED(sig));
static void wxt_sigint_return();
static void wxt_sigint_check();
static void wxt_sigint_init();
static void wxt_sigint_restore();
static int wxt_sigint_counter = 0;

/* cleanup at exit, and handle 'persist' setting */
void wxt_atexit();

#endif /*gnuplot_wxt_h*/
