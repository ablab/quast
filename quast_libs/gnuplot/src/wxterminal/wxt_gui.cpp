/*
 * $Id: wxt_gui.cpp,v 1.128.2.27 2016/08/26 04:16:01 sfeam Exp $
 */

/* GNUPLOT - wxt_gui.cpp */

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
 * Alternatively, the contents of this file, apart from one portion
 * that originates from other gnuplot files and is designated as such,
 * may be used under the terms of the GNU General Public License
 * Version 2 or later (the "GPL"), in which case the provisions of GPL
 * are applicable instead of those above. If you wish to allow
 * use of your version of the appropriate portion of this file only
 * under the terms of the GPL and not to allow others to use your version
 * of this file under the above gnuplot license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not
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
 * This file implements in C++ the functions which are called by wxt.trm :
 *
 * It depends on the generic cairo functions,
 * declared in gp_cairo.h for all the drawing work.
 *
 * Here is the interactive part :
 * - rescaling according to the window's size,
 * - mouse support (cursor position, zoom, rotation, ruler, clipboard...),
 * - a toolbar to give additionnal capabilities (similar to the OS/2 terminal),
 * - multiple plot windows.
 *
 * ------------------------------------------------------*/

/* PORTING NOTES
 * Since it uses wxWidgets and Cairo routines, this code is mostly cross-platform.
 * However some details have to be implemented or tweaked for each platform :
 *
 * 1) A generic 'image' surface is implemented as the destination surface
 * for cairo drawing. But for optimal results, cairo should draw to a native
 * surface corresponding to the graphical system.
 * Examples :
 * - a gdkpixmap when compiling for wxGTK (currently disabled because of a bug in CAIRO_OPERATOR_SATURATE),
 * - a HDC when compiling for wxMSW
 * - [insert your contribution here !]
 *
 * 2) You have to be careful with the gui main loop.
 * As far as I understand :
 * Some platforms (Windows ?) require that it is in the main thread.
 * When compiling for Windows (wxMSW), the text window already implements it, so we
 * don't have to do it, but wxWidgets still have to be initialized correctly.
 * When compiling for Unix (wxGTK), we don't have one, so we launch it in a separate thread.
 * For new platforms, it is necessary to figure out whether to configure for single
 * or multi-threaded operation.
 */


/* define DEBUG here to have debugging messages in stderr */
#include "wxt_gui.h"

/* frame icon composed of three icons of different resolutions */
#include "bitmaps/xpm/icon16x16.xpm"
#include "bitmaps/xpm/icon32x32.xpm"
#include "bitmaps/xpm/icon64x64.xpm"
/* cursors */
#include "bitmaps/xpm/cross.xpm"
#include "bitmaps/xpm/right.xpm"
#include "bitmaps/xpm/rotate.xpm"
#include "bitmaps/xpm/size.xpm"
/* Toolbar icons
 * Those are embedded PNG icons previously converted to an array.
 * See bitmaps/png/README for details */
#include "bitmaps/png/clipboard_png.h"
#include "bitmaps/png/replot_png.h"
#include "bitmaps/png/grid_png.h"
#include "bitmaps/png/previouszoom_png.h"
#include "bitmaps/png/nextzoom_png.h"
#include "bitmaps/png/autoscale_png.h"
#include "bitmaps/png/config_png.h"
#include "bitmaps/png/help_png.h"

/* standard icon art from wx (used only for "Export to file" */
#include <wx/artprov.h>

extern "C" {
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
}

/* Interactive toggle control variables
 */
static int wxt_cur_plotno = 0;
static TBOOLEAN wxt_in_key_sample = FALSE;
static TBOOLEAN wxt_in_plot = FALSE;
static TBOOLEAN wxt_zoom_command = FALSE;
#ifdef USE_MOUSE
typedef struct {
	unsigned int left;
	unsigned int right;
	unsigned int ytop;
	unsigned int ybot;
	TBOOLEAN hidden;
} wxtBoundingBox;
wxtBoundingBox *wxt_key_boxes = NULL;
int wxt_max_key_boxes = 0;

/* Hypertext tracking variables */
typedef struct {
	unsigned int x;
	unsigned int y;
	unsigned int size;
} wxtAnchorPoint;
wxtAnchorPoint *wxt_anchors = NULL;
int wxt_n_anchors = 0;
int wxt_max_anchors = 0;
TBOOLEAN pending_href = FALSE;
const char *wxt_display_hypertext = NULL;
wxtAnchorPoint wxt_display_anchor = {0,0,0};

#else
#define wxt_update_key_box(x,y)
#define wxt_update_anchors(x,y,size)
#endif

#if defined(WXT_MONOTHREADED) && !defined(_Windows)
static int yield = 0;	/* used in wxt_waitforinput() */
#endif

char *wxt_enhanced_fontname = NULL;

#ifdef __WXMAC__
#include <ApplicationServices/ApplicationServices.h>
#endif

/* ---------------------------------------------------------------------------
 * event tables and other macros for wxWidgets
 * --------------------------------------------------------------------------*/

/* the event tables connect the wxWidgets events with the functions (event
 * handlers) which process them. It can be also done at run-time, but for the
 * simple menu events like this the static method is much simpler.
 */

BEGIN_EVENT_TABLE( wxtApp, wxApp )
	EVT_COMMAND( wxID_ANY, wxCreateWindowEvent, wxtApp::OnCreateWindow )
	EVT_COMMAND( wxID_ANY, wxExitLoopEvent, wxtApp::OnExitLoop )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE( wxtFrame, wxFrame )
	EVT_CLOSE( wxtFrame::OnClose )
	EVT_SIZE( wxtFrame::OnSize )
	EVT_TOOL( Toolbar_ExportToFile, wxtFrame::OnExport )
	/* Clipboard widget (should consolidate this with Export to File) */
	EVT_TOOL( Toolbar_CopyToClipboard, wxtFrame::OnCopy )
#ifdef USE_MOUSE
	EVT_TOOL( Toolbar_Replot, wxtFrame::OnReplot )
	EVT_TOOL( Toolbar_ToggleGrid, wxtFrame::OnToggleGrid )
	EVT_TOOL( Toolbar_ZoomPrevious, wxtFrame::OnZoomPrevious )
	EVT_TOOL( Toolbar_ZoomNext, wxtFrame::OnZoomNext )
	EVT_TOOL( Toolbar_Autoscale, wxtFrame::OnAutoscale )
	EVT_COMMAND( wxID_ANY, wxStatusTextEvent, wxtFrame::OnSetStatusText )
#endif /*USE_MOUSE*/
	EVT_TOOL( Toolbar_Config, wxtFrame::OnConfig )
	EVT_TOOL( Toolbar_Help, wxtFrame::OnHelp )
END_EVENT_TABLE()

BEGIN_EVENT_TABLE( wxtPanel, wxPanel )
	EVT_LEAVE_WINDOW( wxtPanel::OnMouseLeave )
	EVT_PAINT( wxtPanel::OnPaint )
	EVT_ERASE_BACKGROUND( wxtPanel::OnEraseBackground )
	EVT_SIZE( wxtPanel::OnSize )
#ifdef USE_MOUSE
	EVT_MOTION( wxtPanel::OnMotion )
	EVT_LEFT_DOWN( wxtPanel::OnLeftDown )
	EVT_LEFT_UP( wxtPanel::OnLeftUp )
	EVT_MIDDLE_DOWN( wxtPanel::OnMiddleDown )
	EVT_MIDDLE_UP( wxtPanel::OnMiddleUp )
	EVT_RIGHT_DOWN( wxtPanel::OnRightDown )
	EVT_RIGHT_UP( wxtPanel::OnRightUp )
	EVT_MOUSEWHEEL( wxtPanel::OnMouseWheel )
	EVT_CHAR( wxtPanel::OnKeyDownChar )
#endif /*USE_MOUSE*/
END_EVENT_TABLE()

BEGIN_EVENT_TABLE( wxtConfigDialog, wxDialog )
	EVT_CLOSE( wxtConfigDialog::OnClose )
	EVT_CHOICE( Config_Rendering, wxtConfigDialog::OnRendering )
	EVT_COMMAND_RANGE( Config_OK, Config_CANCEL,
		wxEVT_COMMAND_BUTTON_CLICKED, wxtConfigDialog::OnButton )
END_EVENT_TABLE()

#ifdef WXT_MULTITHREADED
/* ----------------------------------------------------------------------------
 *   gui thread
 * ----------------------------------------------------------------------------*/

/* What really happens in the thread
 * Just before it returns, wxEntry will call a whole bunch of wxWidgets-cleanup functions */
void *wxtThread::Entry()
{
	FPRINTF((stderr,"secondary thread entry\n"));

	/* don't answer to SIGINT in this thread - avoids LONGJMP problems */
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	/* gui loop */
	wxTheApp->OnRun();

	/* Workaround for a deadlock when the main thread will Wait() for this one.
	 * This issue comes from the fact that our gui main loop is not in the
	 * main thread as wxWidgets was written for. */
	wxt_MutexGuiLeave();

	FPRINTF((stderr,"secondary thread finished\n"));
	return NULL;
}
#endif /* WXT_MULTITHREADED */



/* ----------------------------------------------------------------------------
 *  `Main program' equivalent: the program execution "starts" here
 * ----------------------------------------------------------------------------*/

/* Create a new application object */
IMPLEMENT_APP_NO_MAIN(wxtApp)

bool wxtApp::OnInit()
{
#ifdef __WXMAC__
	ProcessSerialNumber PSN;
	GetCurrentProcess(&PSN);
	TransformProcessType(&PSN, kProcessTransformToForegroundApplication);
#endif

	/* Usually wxWidgets apps create their main window here.
	 * However, in the context of multiple plot windows, the same code is written in wxt_init().
	 * So, to avoid duplication of the code, we do only what is strictly necessary.*/

	FPRINTF((stderr, "OnInit\n"));

	/* initialize frames icons */
	icon.AddIcon(wxIcon(icon16x16_xpm));
	icon.AddIcon(wxIcon(icon32x32_xpm));
	icon.AddIcon(wxIcon(icon64x64_xpm));

	/* we load the image handlers, needed to copy the plot to clipboard, and to load icons */
	::wxInitAllImageHandlers();

#ifdef __WXMSW__
	/* allow the toolbar to display properly png icons with an alpha channel */
	wxSystemOptions::SetOption(wxT("msw.remap"), 0);
#endif /* __WXMSW__ */

	/* load toolbar icons */
	LoadPngIcon(clipboard_png, sizeof(clipboard_png), 0);
	LoadPngIcon(replot_png, sizeof(replot_png), 1);
	LoadPngIcon(grid_png, sizeof(grid_png), 2);
	LoadPngIcon(previouszoom_png, sizeof(previouszoom_png), 3);
	LoadPngIcon(nextzoom_png, sizeof(nextzoom_png), 4);
	LoadPngIcon(autoscale_png, sizeof(autoscale_png), 5);
	LoadPngIcon(config_png, sizeof(config_png), 6);
	LoadPngIcon(help_png, sizeof(help_png), 7);

	/* load cursors */
	LoadCursor(wxt_cursor_cross, cross);
	LoadCursor(wxt_cursor_right, right);
	LoadCursor(wxt_cursor_rotate, rotate);
	LoadCursor(wxt_cursor_size, size);

	/* Initialize the config object */
	/* application and vendor name are used by wxConfig to construct the name
	 * of the config file/registry key and must be set before the first call
	 * to Get() */
	SetVendorName(wxT("gnuplot"));
	SetAppName(wxT("gnuplot-wxt"));
	wxConfigBase *pConfig = wxConfigBase::Get();
	/* this will force writing back of the defaults for all values
	 * if they're not present in the config - this can give the user an idea
	 * of all possible settings */
	pConfig->SetRecordDefaults();

	FPRINTF((stderr, "OnInit finished\n"));

	return true; /* means that process must continue */
}

/* load an icon from a PNG file embedded as a C array */
void wxtApp::LoadPngIcon(const unsigned char *embedded_png, int length, int icon_number)
{
	wxMemoryInputStream pngstream(embedded_png, length);
#ifdef __WXOSX_COCOA__
	/* 16x16 bitmaps on wxCocoa cause blurry toolbar images, resize them to 24x24 */
	toolBarBitmaps[icon_number] = new wxBitmap(wxImage(pngstream, wxBITMAP_TYPE_PNG).Resize(wxSize(24, 24), wxPoint(4, 4)));
#else
	toolBarBitmaps[icon_number] = new wxBitmap(wxImage(pngstream, wxBITMAP_TYPE_PNG));
#endif
}

/* load a cursor */
void wxtApp::LoadCursor(wxCursor &cursor, const char* xpm_bits[])
{
	int hotspot_x, hotspot_y;
	wxBitmap cursor_bitmap = wxBitmap(xpm_bits);
	wxImage cursor_image = cursor_bitmap.ConvertToImage();
	/* XPM spec : first string is :
	 * width height ncolors charperpixel hotspotx hotspoty */
	sscanf(xpm_bits[0], "%*d %*d %*d %*d %d %d", &hotspot_x, &hotspot_y);
	cursor_image.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, hotspot_x);
	cursor_image.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, hotspot_y);
	cursor = wxCursor(cursor_image);
}

/* cleanup on exit
 * In a pure wxWidgets app, the returned int is the exit status of the app.
 * Here it is not used. */
int wxtApp::OnExit()
{
	FPRINTF((stderr,"wxtApp::OnExit\n"));
	/* clean up: Set() returns the active config object as Get() does, but unlike
	 * Get() it doesn't try to create one if there is none (definitely not what
	 * we want here!) */
	delete wxConfigBase::Set((wxConfigBase *) NULL);
	return 0;
}

/* will gently terminate the gui thread */
void wxtApp::OnExitLoop( wxCommandEvent& WXUNUSED(event) )
{
	FPRINTF((stderr,"wxtApp::OnExitLoop\n"));
	wxTheApp->ExitMainLoop();
}

void wxtApp::OnCreateWindow( wxCommandEvent& event )
{
    /* retrieve the pointer to wxt_window_t */
	wxt_window_t *window = (wxt_window_t*) event.GetClientData();

	FPRINTF((stderr,"wxtApp::OnCreateWindow\n"));
	window->frame = new wxtFrame( window->title, window->id );
	window->frame->Show(true);
	FPRINTF((stderr,"new plot window opened\n"));
	/* make the panel able to receive keyboard input */
	window->frame->panel->SetFocus();
	/* set the default crosshair cursor */
	window->frame->panel->SetCursor(wxt_cursor_cross);
	/* creating the true context (at initialization, it may be a fake one).
	 * Note : the frame must be shown for this to succeed */
	if (!window->frame->panel->plot.success)
		window->frame->panel->wxt_cairo_create_context();

	/* tell the other thread we have finished */
	wxMutexLocker lock(*(window->mutex));
	window->condition->Broadcast();
}

/* wrapper for AddPendingEvent or ProcessEvent */
void wxtApp::SendEvent( wxEvent &event)
{
#ifdef WXT_MULTITHREADED
	AddPendingEvent(event);
#else /* !WXT_MULTITHREADED */
	ProcessEvent(event);
#endif /* !WXT_MULTITHREADED */
}

/* ---------------------------------------------------------------------------
 * Frame : the main windows (one for each plot)
 * ----------------------------------------------------------------------------*/

/* frame constructor*/
wxtFrame::wxtFrame( const wxString& title, wxWindowID id )
	: wxFrame((wxFrame *)NULL, id, title, wxPoint(wxt_posx, wxt_posy), wxDefaultSize, wxDEFAULT_FRAME_STYLE|wxWANTS_CHARS)
{
	FPRINTF((stderr,"wxtFrame constructor\n"));

	/* used to check for panel initialization */
	panel = NULL;

	/* initialize the state of the configuration dialog */
	config_displayed = false;

	/* set up the window icon, in several resolutions */
	SetIcons(icon);

	/* set up the status bar, and fill it with an empty
	 * string. It will be immediately overriden by gnuplot. */
	CreateStatusBar();
	SetStatusText( wxT("") );

	/* set up the toolbar */
	toolbar = CreateToolBar();

	/* With wxMSW, default toolbar size is only 16x15. */
	// toolbar->SetToolBitmapSize(wxSize(16,16));

	toolbar->AddTool(Toolbar_CopyToClipboard, wxT("Copy"),
				wxArtProvider::GetBitmap(wxART_PASTE, wxART_TOOLBAR),
				wxT("Copy plot to clipboard"));
	toolbar->AddTool(Toolbar_ExportToFile, wxT("Export"),
				wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS, wxART_TOOLBAR),
				wxT("Export plot to file"));
#ifdef USE_MOUSE
#ifdef __WXOSX_COCOA__
	/* wx 2.9 Cocoa bug & crash workaround for Lion, which does not have toolbar separators anymore */
	toolbar->AddStretchableSpace();
#else
	toolbar->AddSeparator();
#endif
	toolbar->AddTool(Toolbar_Replot, wxT("Replot"),
				*(toolBarBitmaps[1]), wxT("Replot"));
	toolbar->AddTool(Toolbar_ToggleGrid, wxT("Toggle grid"),
				*(toolBarBitmaps[2]),wxNullBitmap,wxITEM_NORMAL, wxT("Toggle grid"));
	toolbar->AddTool(Toolbar_ZoomPrevious, wxT("Previous zoom"),
				*(toolBarBitmaps[3]), wxT("Apply the previous zoom settings"));
	toolbar->AddTool(Toolbar_ZoomNext, wxT("Next zoom"),
				*(toolBarBitmaps[4]), wxT("Apply the next zoom settings"));
	toolbar->AddTool(Toolbar_Autoscale, wxT("Autoscale"),
				*(toolBarBitmaps[5]), wxT("Apply autoscale"));
#endif /*USE_MOUSE*/
#ifdef __WXOSX_COCOA__
	/* wx 2.9 Cocoa bug & crash workaround for Lion, which does not have toolbar separators anymore */
	toolbar->AddStretchableSpace();
#else
	toolbar->AddSeparator();
#endif
	toolbar->AddTool(Toolbar_Config, wxT("Terminal configuration"),
				*(toolBarBitmaps[6]), wxT("Open configuration dialog"));
	toolbar->AddTool(Toolbar_Help, wxT("Help"),
				*(toolBarBitmaps[7]), wxT("Open help dialog"));
	toolbar->Realize();

	FPRINTF((stderr,"wxtFrame constructor 2\n"));

	SetClientSize( wxSize(wxt_width, wxt_height) );

	/* build the panel, which will contain the visible device context */
	panel = new wxtPanel( this, this->GetId(), this->GetClientSize() );

	/* setting minimum height and width for the window */
	SetSizeHints(100, 100);

	FPRINTF((stderr,"wxtFrame constructor 3\n"));
}


wxtFrame::~wxtFrame()
{
	/* Automatically remove frame from window list. */
	std::vector<wxt_window_t>::iterator wxt_iter;

	for(wxt_iter = wxt_window_list.begin();
		wxt_iter != wxt_window_list.end(); wxt_iter++) {
		if (this == wxt_iter->frame) {
			wxt_window_list.erase(wxt_iter);
			break;
		}
	}
}


/* toolbar event : Export to file
 * We will create a file dialog, using platform-independant wxWidgets functions
 */
void wxtFrame::OnExport( wxCommandEvent& WXUNUSED( event ) )
{
	static int userFilterIndex = 0;
	static wxString saveDir;

	if (saveDir.IsEmpty())
		saveDir = wxGetCwd();

	wxFileDialog exportFileDialog (this, wxT("Exported File Format"),
		saveDir, wxT(""),
		wxT("PNG files (*.png)|*.png|PDF files (*.pdf)|*.pdf|SVG files (*.svg)|*.svg"),
		wxFD_SAVE|wxFD_OVERWRITE_PROMPT);

	exportFileDialog.SetFilterIndex(userFilterIndex);

	if (exportFileDialog.ShowModal() == wxID_CANCEL)
		return;

	/* wxID_OK:  User wants to save to a file. */

	saveDir = exportFileDialog.GetDirectory();

	wxString fullpathFilename = exportFileDialog.GetPath();
	wxString fileExt = fullpathFilename.AfterLast ('.');

	cairo_status_t ierr;
	cairo_surface_t *surface;
	cairo_t* save_cr;

	switch (exportFileDialog.GetFilterIndex()) {
	case 0 :
		/* Save as PNG file. */
		surface = cairo_get_target(panel->plot.cr);
		ierr = cairo_surface_write_to_png(surface, fullpathFilename.mb_str(wxConvUTF8));
		if (ierr != CAIRO_STATUS_SUCCESS)
			fprintf(stderr,"error writing PNG file: %s\n", cairo_status_to_string(ierr));
		break;

	case 1 :
		/* Save as PDF file. */
		save_cr = panel->plot.cr;
		cairo_save(save_cr);
		surface = cairo_pdf_surface_create(
			fullpathFilename.mb_str(wxConvUTF8),
			panel->plot.device_xmax, panel->plot.device_ymax);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo error: could not create surface for file %s.\n", (const char *)fullpathFilename.mb_str());
			cairo_surface_destroy(surface);
			break;
		}
		panel->plot.cr = cairo_create(surface);
		cairo_surface_destroy(surface);

		cairo_scale(panel->plot.cr,
			1./(double)panel->plot.oversampling_scale,
			1./(double)panel->plot.oversampling_scale);
		panel->wxt_cairo_refresh();

		cairo_show_page(panel->plot.cr);
		cairo_surface_finish(surface);
		panel->plot.cr = save_cr;
		cairo_restore(panel->plot.cr);
		break;

	case 2 :
#ifdef CAIRO_HAS_SVG_SURFACE
		/* Save as SVG file. */
		save_cr = panel->plot.cr;
		cairo_save(save_cr);
		surface = cairo_svg_surface_create(
			fullpathFilename.mb_str(wxConvUTF8),
			panel->plot.device_xmax, panel->plot.device_ymax);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
			fprintf(stderr, "Cairo error: could not create surface for file %s.\n", (const char *)fullpathFilename.mb_str());
			cairo_surface_destroy(surface);
			break;
		}
		panel->plot.cr = cairo_create(surface);
		cairo_surface_destroy(surface);

		cairo_scale(panel->plot.cr,
			1./(double)panel->plot.oversampling_scale,
			1./(double)panel->plot.oversampling_scale);
		panel->wxt_cairo_refresh();

		cairo_show_page(panel->plot.cr);
		cairo_surface_finish(surface);
		panel->plot.cr = save_cr;
		cairo_restore(panel->plot.cr);
		break;
#endif

	default :
		fprintf(stderr, "Can't save in that file type.\n");
		break;
	}

	/* Save user environment selections. */
	userFilterIndex = exportFileDialog.GetFilterIndex();
}

/* toolbar event : Copy to clipboard
 * We will copy the panel to a bitmap, using platform-independant wxWidgets functions */
void wxtFrame::OnCopy( wxCommandEvent& WXUNUSED( event ) )
{
	FPRINTF((stderr,"Copy to clipboard\n"));
	int width = panel->plot.device_xmax, height = panel->plot.device_ymax;
	wxBitmap cp_bitmap(width,height);
	wxMemoryDC cp_dc;
	wxClientDC dc(panel);

	cp_dc.SelectObject(cp_bitmap);
	cp_dc.Blit(0,0,width,height,&dc,0,0);
	cp_dc.SelectObject(wxNullBitmap);

	wxTheClipboard->UsePrimarySelection(false);
	/* SetData clears the clipboard */
	if ( wxTheClipboard->Open() ) {
		wxTheClipboard->SetData(new wxBitmapDataObject(cp_bitmap));
		wxTheClipboard->Close();
	}
	wxTheClipboard->Flush();
}

#ifdef USE_MOUSE
/* toolbar event : Replot */
void wxtFrame::OnReplot( wxCommandEvent& WXUNUSED( event ) )
{
	wxt_exec_event(GE_keypress, 0, 0, 'e' , 0, this->GetId());
}

/* toolbar event : Toggle Grid */
void wxtFrame::OnToggleGrid( wxCommandEvent& WXUNUSED( event ) )
{
	wxt_exec_event(GE_keypress, 0, 0, 'g', 0, this->GetId());
}

/* toolbar event : Previous Zoom in history */
void wxtFrame::OnZoomPrevious( wxCommandEvent& WXUNUSED( event ) )
{
	wxt_exec_event(GE_keypress, 0, 0, 'p', 0, this->GetId());
}

/* toolbar event : Next Zoom in history */
void wxtFrame::OnZoomNext( wxCommandEvent& WXUNUSED( event ) )
{
	wxt_exec_event(GE_keypress, 0, 0, 'n', 0, this->GetId());
}

/* toolbar event : Autoscale */
void wxtFrame::OnAutoscale( wxCommandEvent& WXUNUSED( event ) )
{
	wxt_exec_event(GE_keypress, 0, 0, 'a', 0, this->GetId());
}

/* set the status text from the event data */
void wxtFrame::OnSetStatusText( wxCommandEvent& event )
{
	SetStatusText(event.GetString());
}
#endif /*USE_MOUSE*/

/* toolbar event : Config */
void wxtFrame::OnConfig( wxCommandEvent& WXUNUSED( event ) )
{
	/* if we have already opened a dialog, just raise it */
	if (config_displayed) {
		config_dialog->Raise();
		return;
	}

	/* otherwise, open a dialog */
	config_displayed = true;
	config_dialog = new wxtConfigDialog(this);
	config_dialog->Show(true);
}


/* toolbar event : Help */
void wxtFrame::OnHelp( wxCommandEvent& WXUNUSED( event ) )
{
	wxMessageBox( wxString(wxT("You are using an interactive terminal ")
		wxT("based on wxWidgets for the interface, Cairo ")
		wxT("for the drawing facilities, and Pango for the text layouts.\n")
		wxT("Please note that toolbar icons in the terminal ")
		wxT("don't reflect the whole range of mousing ")
		wxT("possibilities in the terminal.\n")
		wxT("Hit 'h' in the plot window ")
		wxT("and a help message for mouse commands ")
		wxT("will appear in the gnuplot console.\n")
		wxT("See also 'help mouse'.\n")),
		wxT("wxWidgets terminal help"), wxOK | wxICON_INFORMATION, this );
}

/* called on Close() (menu or window manager) */
void wxtFrame::OnClose( wxCloseEvent& event )
{
	FPRINTF((stderr,"OnClose\n"));
	if ( event.CanVeto() && !wxt_handling_persist) {
		/* Default behaviour when Quit is clicked, or the window cross X */
		event.Veto();
		this->Hide();
#ifdef USE_MOUSE
		/* Pass it through mouse handling to check for "bind Close" */
		wxt_exec_event(GE_reset, 0, 0, 0, 0, this->GetId());
#endif /* USE_MOUSE */
	}
	else /* in "persist" mode */ {
		/* declare the iterator */
		std::vector<wxt_window_t>::iterator wxt_iter;

		for(wxt_iter = wxt_window_list.begin();
				wxt_iter != wxt_window_list.end(); wxt_iter++)
		{
			if (this == wxt_iter->frame) {
				wxt_window_list.erase(wxt_iter);
				break;
			}
		}
		this->Destroy();
	}

#if defined(_Windows) && !defined(WGP_CONSOLE)
	/* Close text window if this was the last plot window. */
	WinPersistTextClose();
#endif
}

/* when the window is resized,
 * resize the panel to fit in the frame.
 * If the tool widget setting for "redraw on resize" is set, replot in new size.
 * FIXME : Loses all but most recent component of a multiplot.
 */
void wxtFrame::OnSize( wxSizeEvent& event )
{
	FPRINTF((stderr,"frame OnSize\n"));

	/* Under Windows the frame receives an OnSize event before being completely initialized.
	 * So we must check for the panel to be properly initialized before.*/
	if (panel)
		panel->SetSize( this->GetClientSize() );
#ifdef __WXOSX_COCOA__
	/* wx 2.9 Cocoa bug workaround, that does not adjust layout for status bar on resize */
	PositionStatusBar();
#endif

	/* Note: On some platforms OnSize() might get called before the settings have been initialized in wxt_init(). */
	if (wxt_redraw == yes)
		wxt_exec_event(GE_replot, 0, 0, 0 , 0, this->GetId());
}

/* wrapper for AddPendingEvent or ProcessEvent */
void wxtFrame::SendEvent( wxEvent &event)
{
#ifdef WXT_MULTITHREADED
	AddPendingEvent(event);
#else /* !WXT_MULTITHREADED */
	ProcessEvent(event);
#endif /* !WXT_MULTITHREADED */
}

/* ---------------------------------------------------------------------------
 * Panel : the space used for the plot, between the toolbar and the statusbar
 * ----------------------------------------------------------------------------*/

/* panel constructor
 * Note : under Windows, wxDefaultPosition makes the panel hide the toolbar */
wxtPanel::wxtPanel( wxWindow *parent, wxWindowID id, const wxSize& size )
	: wxPanel( parent,  id,  wxPoint(0,0) /*wxDefaultPosition*/, size, wxWANTS_CHARS )
{
	FPRINTF((stderr,"panel constructor\n"));

	/* initialisations */
	gp_cairo_initialize_plot(&plot);
	GetSize(&(plot.device_xmax),&(plot.device_ymax));
	plot.polygons_saturate = TRUE;

	settings_queued = false;

#ifdef USE_MOUSE
	mouse_x = 0;
	mouse_y = 0;
	wxt_zoombox = false;
	zoom_x1 = 0;
	zoom_y1 = 0;
	zoom_string1 = wxT("");
	zoom_string2 = wxT("");

	wxt_ruler = false;
	wxt_ruler_x = 0;
	wxt_ruler_y = 0;

	modifier_mask = 0;
#endif /*USE_MOUSE*/

#if defined(GTK_SURFACE)
	gdkpixmap = NULL;
#elif defined(__WXMSW__)
	hdc = NULL;
	hbm = NULL;
#else /* IMAGE_SURFACE */
	cairo_bitmap = NULL;
	data32 = NULL;
#endif /* SURFACE */

	FPRINTF((stderr,"panel constructor4\n"));

	/* create the device context to be drawn */
	wxt_cairo_create_context();

	FPRINTF((stderr,"panel constructor5\n"));

#ifdef IMAGE_SURFACE
	wxt_cairo_create_bitmap();
#endif
	FPRINTF((stderr,"panel constructor6\n"));
}


/* destructor */
wxtPanel::~wxtPanel()
{
	FPRINTF((stderr,"panel destructor\n"));
	wxt_cairo_free_context();

	/* clear the command list, free the allocated memory */
	ClearCommandlist();
}

/* temporary store new settings values to be applied for the next plot */
void wxtPanel::wxt_settings_queue(TBOOLEAN antialiasing,
					TBOOLEAN oversampling,
					int hinting_setting)
{
	mutex_queued.Lock();
	settings_queued = true;
	antialiasing_queued = antialiasing;
	oversampling_queued = oversampling;
	hinting_queued = hinting_setting;
	mutex_queued.Unlock();
}

/* apply queued settings */
void wxtPanel::wxt_settings_apply()
{
	mutex_queued.Lock();
	if (settings_queued) {
		plot.antialiasing = antialiasing_queued;
		plot.oversampling = oversampling_queued;
		plot.hinting = hinting_queued;
		settings_queued = false;
	}
	mutex_queued.Unlock();
}

/* clear the command list, free the allocated memory */
void wxtPanel::ClearCommandlist()
{
	command_list_mutex.Lock();

	command_list_t::iterator iter; /*declare the iterator*/

	/* run through the list, and free allocated memory */
	for(iter = command_list.begin(); iter != command_list.end(); ++iter) {
		if ( iter->command == command_put_text ||
			iter->command == command_hypertext ||
			iter->command == command_set_font)
			delete[] iter->string;
		if (iter->command == command_filled_polygon)
			delete[] iter->corners;
		if (iter->command == command_image)
			free(iter->image);
		if (iter->command == command_dashtype)
			free(iter->dashpattern);
	}

	command_list.clear();
	command_list_mutex.Unlock();
}


/* method called when the panel has to be painted
 * -> Refresh(), window dragged, dialogs over the window, etc. */
void wxtPanel::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
	/* Constructor of the device context */
	wxPaintDC dc(this);
	DrawToDC(dc, GetUpdateRegion());
}

/* same as OnPaint, but can be directly called by a user function */
void wxtPanel::Draw()
{
	wxClientDC dc(this);
	wxBufferedDC buffered_dc(&dc, wxSize(plot.device_xmax, plot.device_ymax));
	wxRegion region(0, 0, plot.device_xmax, plot.device_ymax);
	DrawToDC(buffered_dc, region);
}

/* copy the plot to the panel, draw zoombow and ruler needed */
void wxtPanel::DrawToDC(wxDC &dc, wxRegion &region)
{
	wxPen tmp_pen;

	/* TODO extend the region mechanism to surfaces other than GTK_SURFACE */
#ifdef GTK_SURFACE
	wxRegionIterator upd(region);
	int vX,vY,vW,vH; /* Dimensions of client area in pixels */

	while (upd) {
		vX = upd.GetX();
		vY = upd.GetY();
		vW = upd.GetW();
		vH = upd.GetH();

		FPRINTF((stderr,"OnPaint %d,%d,%d,%d\n",vX,vY,vW,vH));
		/* Repaint this rectangle */
		if (gdkpixmap)
			gdk_draw_drawable(dc.GetWindow(),
				dc.m_penGC,
				gdkpixmap,
				vX,vY,
				vX,vY,
				vW,vH);
		++upd;
	}
#elif defined(__WXMSW__)
	// Need to flush to make sure the bitmap is fully drawn.
	cairo_surface_flush(cairo_get_target(plot.cr));
	BitBlt((HDC) dc.GetHDC(), 0, 0, plot.device_xmax, plot.device_ymax, hdc, 0, 0, SRCCOPY);
#else
	dc.DrawBitmap(*cairo_bitmap, 0, 0, false);
#endif

	/* fill in gray when the aspect ratio conservation has let empty space in the panel */
	if (plot.device_xmax*plot.ymax > plot.device_ymax*plot.xmax) {
		dc.SetPen( *wxTRANSPARENT_PEN );
		dc.SetBrush( wxBrush( wxT("LIGHT GREY"), wxSOLID ) );
		dc.DrawRectangle((int) (plot.xmax/plot.oversampling_scale*plot.xscale),
				0,
				plot.device_xmax - (int) (plot.xmax/plot.oversampling_scale*plot.xscale),
				plot.device_ymax);
	} else if (plot.device_xmax*plot.ymax < plot.device_ymax*plot.xmax) {
		dc.SetPen( *wxTRANSPARENT_PEN );
		dc.SetBrush( wxBrush( wxT("LIGHT GREY"), wxSOLID ) );
		dc.DrawRectangle(0,
				(int) (plot.ymax/plot.oversampling_scale*plot.yscale),
				plot.device_xmax,
				(int) (plot.device_ymax - plot.ymax/plot.oversampling_scale*plot.yscale));
	}

#ifdef USE_MOUSE
	if (wxt_zoombox) {
		tmp_pen = wxPen(wxT("black"), 1, wxSOLID);
		tmp_pen.SetCap( wxCAP_ROUND );
		dc.SetPen( tmp_pen );
#ifndef __WXOSX_COCOA__
		/* wx 2.9 Cocoa bug workaround, which has no logical functions support */
		dc.SetLogicalFunction( wxINVERT );
#endif
		dc.DrawLine( zoom_x1, zoom_y1, mouse_x, zoom_y1 );
		dc.DrawLine( mouse_x, zoom_y1, mouse_x, mouse_y );
		dc.DrawLine( mouse_x, mouse_y, zoom_x1, mouse_y );
		dc.DrawLine( zoom_x1, mouse_y, zoom_x1, zoom_y1 );
		dc.SetPen( *wxTRANSPARENT_PEN );
		dc.SetBrush( wxBrush( wxT("LIGHT BLUE"), wxSOLID ) );
		dc.SetLogicalFunction( wxAND );
		dc.DrawRectangle( zoom_x1, zoom_y1, mouse_x -zoom_x1, mouse_y -zoom_y1);
		dc.SetLogicalFunction( wxCOPY );

		dc.SetFont( wxFont( (int) plot.fontsize, wxFONTFAMILY_DEFAULT,
			wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false,
			wxString(plot.fontname, wxConvLocal) ) );

		dc.DrawText( zoom_string1.BeforeFirst(wxT('\r')),
			zoom_x1, zoom_y1 - term->v_char/plot.oversampling_scale);
		dc.DrawText( zoom_string1.AfterFirst(wxT('\r')),
			zoom_x1, zoom_y1);

		dc.DrawText( zoom_string2.BeforeFirst(wxT('\r')),
			mouse_x, mouse_y - term->v_char/plot.oversampling_scale);
		dc.DrawText( zoom_string2.AfterFirst(wxT('\r')),
			mouse_x, mouse_y);

		/* if we have to redraw the zoombox, it is with another size,
		 * so it will be issued later and we can disable it now */
		wxt_zoombox = false;
	}

	if (wxt_ruler) {
		tmp_pen = wxPen(wxT("black"), 1, wxSOLID);
		tmp_pen.SetCap(wxCAP_BUTT);
		dc.SetPen( tmp_pen );
#ifndef __WXOSX_COCOA__
		/* wx 2.9 Cocoa bug workaround, which has no logical functions support */
		dc.SetLogicalFunction( wxINVERT );
#endif
		dc.CrossHair( (int)wxt_ruler_x, (int)wxt_ruler_y );
		dc.SetLogicalFunction( wxCOPY );
	}

	if (wxt_ruler && wxt_ruler_lineto) {
		tmp_pen = wxPen(wxT("black"), 1, wxSOLID);
		tmp_pen.SetCap(wxCAP_BUTT);
		dc.SetPen( tmp_pen );
#ifndef __WXOSX_COCOA__
		/* wx 2.9 Cocoa bug workaround, which has no logical functions support */
		dc.SetLogicalFunction( wxINVERT );
#endif
		dc.DrawLine((int)wxt_ruler_x, (int)wxt_ruler_y, mouse_x, mouse_y);
		dc.SetLogicalFunction( wxCOPY );
	}
#endif /*USE_MOUSE*/
}

/* avoid flickering under win32 */
void wxtPanel::OnEraseBackground( wxEraseEvent &WXUNUSED(event) )
{
}

/* avoid leaving cross cursor when leaving window on Mac */
void wxtPanel::OnMouseLeave( wxMouseEvent &WXUNUSED(event) )
{
#ifdef __WXMAC__
	::wxSetCursor(wxNullCursor);
#endif
}

/* when the window is resized */
void wxtPanel::OnSize( wxSizeEvent& event )
{
	/* don't do anything if term variables are not initialized */
	if (plot.xmax == 0 || plot.ymax == 0)
		return;

	/* update window size, and scaling variables */
	GetSize(&(plot.device_xmax),&(plot.device_ymax));

	double new_xscale, new_yscale;

	new_xscale = ((double) plot.device_xmax)*plot.oversampling_scale/((double) plot.xmax);
	new_yscale = ((double) plot.device_ymax)*plot.oversampling_scale/((double) plot.ymax);

	/* We will keep the aspect ratio constant */
	if (new_yscale < new_xscale) {
		plot.xscale = new_yscale;
		plot.yscale = new_yscale;
	} else {
		plot.xscale = new_xscale;
		plot.yscale = new_xscale;
	}
	FPRINTF((stderr,"panel OnSize %d %d %lf %lf\n",
		plot.device_xmax, plot.device_ymax, plot.xscale,plot.yscale));

	/* create a new cairo context of the good size */
	wxt_cairo_create_context();
	/* redraw the plot with the new scaling */
	wxt_cairo_refresh();
}

#ifdef USE_MOUSE
/* when the mouse is moved over the panel */
void wxtPanel::OnMotion( wxMouseEvent& event )
{
	/* Get and store mouse position for _put_tmp_text() and key events (ruler) */
	mouse_x = event.GetX();
	mouse_y = event.GetY();
	int xnow = (int)gnuplot_x( &plot, mouse_x );
	int ynow = (int)gnuplot_y( &plot, mouse_y );
	bool buttondown = event.LeftIsDown() || event.RightIsDown() || event.MiddleIsDown();

	UpdateModifiers(event);

	/* update the ruler_lineto thing */
	if (wxt_ruler && wxt_ruler_lineto)
		Draw();

	/* informs gnuplot */
	wxt_exec_event(GE_motion,
		xnow, ynow,
		0, 0, this->GetId());

	/* Check to see if the mouse is over a hypertext anchor point */
	if (wxt_n_anchors > 0 && !buttondown)
		wxt_check_for_anchors(xnow, ynow);
}

/* mouse "click" event */
void wxtPanel::OnLeftDown( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);
	if (wxt_toggle)
		wxt_check_for_toggle(x, y);

	wxt_exec_event(GE_buttonpress, x, y, 1, 0, this->GetId());
}

/* mouse "click" event */
void wxtPanel::OnLeftUp( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);

	if ( wxt_exec_event(GE_buttonrelease, x, y, 1,
				(int) left_button_sw.Time(), this->GetId()) ) {
		/* start a watch to send the time elapsed between up and down */
		left_button_sw.Start();
	}
}

/* mouse "click" event */
void wxtPanel::OnMiddleDown( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);

	wxt_exec_event(GE_buttonpress, x, y, 2, 0, this->GetId());
}

/* mouse "click" event */
void wxtPanel::OnMiddleUp( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);

	if ( wxt_exec_event(GE_buttonrelease, x, y, 2,
				(int) middle_button_sw.Time(), this->GetId()) ) {
		/* start a watch to send the time elapsed between up and down */
		middle_button_sw.Start();
	}
}

/* mouse "click" event */
void wxtPanel::OnRightDown( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);

	wxt_exec_event(GE_buttonpress, x, y, 3, 0, this->GetId());
}

/* mouse "click" event */
void wxtPanel::OnRightUp( wxMouseEvent& event )
{
	int x,y;
	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);

	if ( wxt_exec_event(GE_buttonrelease, x, y, 3,
				(int) right_button_sw.Time(), this->GetId()) ) {
		/* start a watch to send the time elapsed between up and down */
		right_button_sw.Start();
	}
}

/* mouse wheel event */
void wxtPanel::OnMouseWheel( wxMouseEvent& event )
{
    int mouse_button;
    int x,y;

	x = (int) gnuplot_x( &plot, event.GetX() );
	y = (int) gnuplot_y( &plot, event.GetY() );

	UpdateModifiers(event);
	mouse_button = (event.GetWheelRotation() > 0 ? 4 : 5);
#if wxCHECK_VERSION(2, 9, 0)
	/* GetWheelAxis: 0 is the Y axis, 1 is the X axis. */
	if (event.GetWheelAxis() > 0)
	    mouse_button += 2;
#endif
	wxt_exec_event(GE_buttonpress, x, y, mouse_button, 0, this->GetId());
}

/* the state of the modifiers is checked each time a key is pressed instead of
 * tracking the press and release events of the modifiers keys, because the
 * window manager catches some combinations, like ctrl+F1, and thus we do not
 * receive a release event in this case */
void wxtPanel::UpdateModifiers( wxMouseEvent& event )
{
	int current_modifier_mask = 0;

	/* retrieve current modifier mask from the wxEvent */
	current_modifier_mask |= (event.AltDown() ? (1<<2) : 0);
	current_modifier_mask |= (event.ControlDown() ? (1<<1) : 0);
	current_modifier_mask |= (event.ShiftDown() ? (1) : 0);

	/* update if changed */
	if (modifier_mask != current_modifier_mask) {
		modifier_mask = current_modifier_mask;
		wxt_exec_event(GE_modifier, 0, 0, modifier_mask, 0, this->GetId());
	}
}

/* a key has been pressed, modifiers have already been handled.
 * We receive keycodes here, and we send corresponding events to gnuplot main thread */
void wxtPanel::OnKeyDownChar( wxKeyEvent &event )
{
	int keycode = event.GetKeyCode();
	int gp_keycode;

	/* this is the same code as in UpdateModifiers(), but the latter method cannot be
	 * used here because wxKeyEvent and wxMouseEvent are different classes, both of them
	 * derive from wxEvent, but wxEvent does not have the necessary AltDown() and friends */
	int current_modifier_mask = 0;

	/* retrieve current modifier mask from the wxEvent */
	current_modifier_mask |= (event.AltDown() ? (1<<2) : 0);
	current_modifier_mask |= (event.ControlDown() ? (1<<1) : 0);
	current_modifier_mask |= (event.ShiftDown() ? (1) : 0);

	/* update if changed */
	if (modifier_mask != current_modifier_mask) {
		modifier_mask = current_modifier_mask;
		wxt_exec_event(GE_modifier, 0, 0, modifier_mask, 0, this->GetId());
	}

#define WXK_GPKEYCODE(wxkey,kcode) case wxkey : gp_keycode=kcode; break;

	if (keycode<256) {
		switch (keycode) {

#ifndef DISABLE_SPACE_RAISES_CONSOLE
		case WXK_SPACE :
			if ((wxt_ctrl==yes && event.ControlDown())
				|| wxt_ctrl!=yes) {
				RaiseConsoleWindow();
				return;
			} else {
				gp_keycode = ' ';
				break;
			}
#endif /* DISABLE_SPACE_RAISES_CONSOLE */

		case 'q' :
		/* ctrl+q does not send 113 but 17 */
		/* WARNING : may be the same for other combinations */
		case 17 :
			if ((wxt_ctrl==yes && event.ControlDown())
				|| wxt_ctrl!=yes) {
				/* closes terminal window */
				this->GetParent()->Close(false);
				return;
			} else {
				gp_keycode = 'q';
				break;
			}
		WXK_GPKEYCODE(WXK_BACK,GP_BackSpace);
		WXK_GPKEYCODE(WXK_TAB,GP_Tab);
		WXK_GPKEYCODE(WXK_RETURN,GP_Return);
		WXK_GPKEYCODE(WXK_ESCAPE,GP_Escape);
		WXK_GPKEYCODE(WXK_DELETE,GP_Delete);
		default : gp_keycode = keycode; break; /* exact solution */
		}
	} else {
		switch( keycode ) {
		WXK_GPKEYCODE(WXK_PAUSE,GP_Pause);
		WXK_GPKEYCODE(WXK_SCROLL,GP_Scroll_Lock);
		WXK_GPKEYCODE(WXK_INSERT,GP_Insert);
		WXK_GPKEYCODE(WXK_HOME,GP_Home);
		WXK_GPKEYCODE(WXK_LEFT,GP_Left);
		WXK_GPKEYCODE(WXK_UP,GP_Up);
		WXK_GPKEYCODE(WXK_RIGHT,GP_Right);
		WXK_GPKEYCODE(WXK_DOWN,GP_Down);
		WXK_GPKEYCODE(WXK_PAGEUP,GP_PageUp);
		WXK_GPKEYCODE(WXK_PAGEDOWN,GP_PageDown);
		WXK_GPKEYCODE(WXK_END,GP_End);
		WXK_GPKEYCODE(WXK_NUMPAD_SPACE,GP_KP_Space);
		WXK_GPKEYCODE(WXK_NUMPAD_TAB,GP_KP_Tab);
		WXK_GPKEYCODE(WXK_NUMPAD_ENTER,GP_KP_Enter);
		WXK_GPKEYCODE(WXK_NUMPAD_F1,GP_KP_F1);
		WXK_GPKEYCODE(WXK_NUMPAD_F2,GP_KP_F2);
		WXK_GPKEYCODE(WXK_NUMPAD_F3,GP_KP_F3);
		WXK_GPKEYCODE(WXK_NUMPAD_F4,GP_KP_F4);

		WXK_GPKEYCODE(WXK_NUMPAD_INSERT,GP_KP_Insert);
		WXK_GPKEYCODE(WXK_NUMPAD_END,GP_KP_End);
		WXK_GPKEYCODE(WXK_NUMPAD_DOWN,GP_KP_Down);
		WXK_GPKEYCODE(WXK_NUMPAD_PAGEDOWN,GP_KP_Page_Down);
		WXK_GPKEYCODE(WXK_NUMPAD_LEFT,GP_KP_Left);
		WXK_GPKEYCODE(WXK_NUMPAD_BEGIN,GP_KP_Begin);
		WXK_GPKEYCODE(WXK_NUMPAD_RIGHT,GP_KP_Right);
		WXK_GPKEYCODE(WXK_NUMPAD_HOME,GP_KP_Home);
		WXK_GPKEYCODE(WXK_NUMPAD_UP,GP_KP_Up);
		WXK_GPKEYCODE(WXK_NUMPAD_PAGEUP,GP_KP_Page_Up);

		WXK_GPKEYCODE(WXK_NUMPAD_DELETE,GP_KP_Delete);
		WXK_GPKEYCODE(WXK_NUMPAD_EQUAL,GP_KP_Equal);
		WXK_GPKEYCODE(WXK_NUMPAD_MULTIPLY,GP_KP_Multiply);
		WXK_GPKEYCODE(WXK_NUMPAD_ADD,GP_KP_Add);
		WXK_GPKEYCODE(WXK_NUMPAD_SEPARATOR,GP_KP_Separator);
		WXK_GPKEYCODE(WXK_NUMPAD_SUBTRACT,GP_KP_Subtract);
		WXK_GPKEYCODE(WXK_NUMPAD_DECIMAL,GP_KP_Decimal);
		WXK_GPKEYCODE(WXK_NUMPAD_DIVIDE,GP_KP_Divide);
		WXK_GPKEYCODE(WXK_NUMPAD0,GP_KP_0);
		WXK_GPKEYCODE(WXK_NUMPAD1,GP_KP_1);
		WXK_GPKEYCODE(WXK_NUMPAD2,GP_KP_2);
		WXK_GPKEYCODE(WXK_NUMPAD3,GP_KP_3);
		WXK_GPKEYCODE(WXK_NUMPAD4,GP_KP_4);
		WXK_GPKEYCODE(WXK_NUMPAD5,GP_KP_5);
		WXK_GPKEYCODE(WXK_NUMPAD6,GP_KP_6);
		WXK_GPKEYCODE(WXK_NUMPAD7,GP_KP_7);
		WXK_GPKEYCODE(WXK_NUMPAD8,GP_KP_8);
		WXK_GPKEYCODE(WXK_NUMPAD9,GP_KP_9);
		WXK_GPKEYCODE(WXK_F1,GP_F1);
		WXK_GPKEYCODE(WXK_F2,GP_F2);
		WXK_GPKEYCODE(WXK_F3,GP_F3);
		WXK_GPKEYCODE(WXK_F4,GP_F4);
		WXK_GPKEYCODE(WXK_F5,GP_F5);
		WXK_GPKEYCODE(WXK_F6,GP_F6);
		WXK_GPKEYCODE(WXK_F7,GP_F7);
		WXK_GPKEYCODE(WXK_F8,GP_F8);
		WXK_GPKEYCODE(WXK_F9,GP_F9);
		WXK_GPKEYCODE(WXK_F10,GP_F10);
		WXK_GPKEYCODE(WXK_F11,GP_F11);
		WXK_GPKEYCODE(WXK_F12,GP_F12);
		default : return; /* probably not ideal */
		}
	}

	/* only send char events to gnuplot if we are the active window */
	if ( wxt_exec_event(GE_keypress, (int) gnuplot_x( &plot, mouse_x ),
				(int) gnuplot_y( &plot, mouse_y ),
				gp_keycode, 0, this->GetId()) ) {
		FPRINTF((stderr,"sending char event\n"));
	}

	/* The following wxWidgets keycodes are not mapped :
	 *	WXK_ALT, WXK_CONTROL, WXK_SHIFT,
	 *	WXK_LBUTTON, WXK_RBUTTON, WXK_CANCEL, WXK_MBUTTON,
	 *	WXK_CLEAR, WXK_MENU,
	 *	WXK_NUMPAD_PRIOR, WXK_NUMPAD_NEXT,
	 *	WXK_CAPITAL, WXK_PRIOR, WXK_NEXT, WXK_SELECT,
	 *	WXK_PRINT, WXK_EXECUTE, WXK_SNAPSHOT, WXK_HELP,
	 *	WXK_MULTIPLY, WXK_ADD, WXK_SEPARATOR, WXK_SUBTRACT,
	 *	WXK_DECIMAL, WXK_DIVIDE, WXK_NUMLOCK, WXK_WINDOWS_LEFT,
	 *	WXK_WINDOWS_RIGHT, WXK_WINDOWS_MENU, WXK_COMMAND
	 * The following gnuplot keycodes are not mapped :
	 *	GP_Linefeed, GP_Clear, GP_Sys_Req, GP_Begin
	 */
}

/* --------------------------------------------------------
 * Bookkeeping for clickable hot spots and hypertext anchors
 * --------------------------------------------------------*/

/* Initialize boxes starting from i */
static void wxt_initialize_key_boxes(int i)
{
	for (; i<wxt_max_key_boxes; i++) {
		wxt_key_boxes[i].left = wxt_key_boxes[i].ybot = INT_MAX;
		wxt_key_boxes[i].right = wxt_key_boxes[i].ytop = 0;
	}
}
static void wxt_initialize_hidden(int i)
{
	for (; i<wxt_max_key_boxes; i++)
		wxt_key_boxes[i].hidden = FALSE;
}


/* Update the box enclosing the key sample for the current plot
 * so that later we can detect mouse clicks in that area
 */
static void wxt_update_key_box( unsigned int x, unsigned int y )
{
	if (wxt_max_key_boxes <= wxt_cur_plotno) {
		wxt_max_key_boxes = wxt_cur_plotno + 10;
		wxt_key_boxes = (wxtBoundingBox *)realloc(wxt_key_boxes,
				wxt_max_key_boxes * sizeof(wxtBoundingBox));
		wxt_initialize_key_boxes(wxt_cur_plotno);
		wxt_initialize_hidden(wxt_cur_plotno);
	}
	wxtBoundingBox *bb = &(wxt_key_boxes[wxt_cur_plotno]);
	y = term->ymax - y;
	if (x < bb->left)  bb->left = x;
	if (x > bb->right) bb->right = x;
	if (y < bb->ybot)  bb->ybot = y;
	if (y > bb->ytop)  bb->ytop = y;
}

/* Keep a list of hypertext anchor points so that we can
 * detect if the mouse is hovering over one of them.
 * Called when we build the display list, not when we execute it.
 */
static void wxt_update_anchors( unsigned int x, unsigned int y, unsigned int size )
{
	if (wxt_n_anchors >= wxt_max_anchors) {
		wxt_max_anchors += 10;
		wxt_anchors = (wxtAnchorPoint *)realloc(wxt_anchors,
				wxt_max_anchors * sizeof(wxtAnchorPoint));
	}
	wxt_anchors[wxt_n_anchors].x = x;
	wxt_anchors[wxt_n_anchors].y = y;
	wxt_anchors[wxt_n_anchors].size = size;
	wxt_n_anchors++;
}

/* Called from wxtPanel::OnLeftDown
 * If the mouse click was on top of a key sample then toggle the
 * corresponding plot on/off
 */
static void wxt_check_for_toggle(unsigned int x, unsigned int y)
{
	int i;
	for (i=1; i<=wxt_cur_plotno && i<wxt_max_key_boxes; i++) {
		if (wxt_key_boxes[i].left == INT_MAX)
			continue;
		if (x < wxt_key_boxes[i].left)
			continue;
		if (x > wxt_key_boxes[i].right)
			continue;
		if (y < wxt_key_boxes[i].ybot)
			continue;
		if (y > wxt_key_boxes[i].ytop)
			continue;
		wxt_key_boxes[i].hidden = !wxt_key_boxes[i].hidden;
		wxt_current_panel->wxt_cairo_refresh();

	}
}

/* Called from wxtPanel::OnMotion
 * If the mouse is hovering over a hypertext anchor point,
 * trigger a refresh so that the text box will be drawn.
 */
static void wxt_check_for_anchors(unsigned int x, unsigned int y)
{
	int i;
	TBOOLEAN refresh = FALSE;
	for (i=0; i<wxt_n_anchors; i++) {
		if ((abs((int)x - (int)wxt_anchors[i].x) < wxt_anchors[i].size)
		&&  (abs((int)y - (int)wxt_anchors[i].y) < wxt_anchors[i].size)) {
			refresh = TRUE;
		}
	}
	/* FIXME: Surely we can be more clever than refreshing every time! */
	if (refresh)
		wxt_current_panel->wxt_cairo_refresh();
}

#endif /*USE_MOUSE*/

#ifndef DISABLE_SPACE_RAISES_CONSOLE
/* ====license information====
 * The following code originates from other gnuplot files,
 * and is not subject to the alternative license statement.
 */


/* FIXME : this code should be deleted, and the feature removed or handled differently,
 * because it is highly platform-dependant, is not reliable because
 * of a lot of factors (WINDOWID not set, multiple tabs in gnome-terminal, mechanisms
 * to prevent focus stealing) and is inconsistent with global bindings mechanism ) */
void wxtPanel::RaiseConsoleWindow()
{
#ifdef USE_GTK
	char *window_env;
	unsigned long windowid = 0;
	/* retrieve XID of gnuplot window */
	window_env = getenv("WINDOWID");
	if (window_env)
		sscanf(window_env, "%lu", &windowid);

/* NOTE: This code uses DCOP, a KDE3 mechanism that no longer exists in KDE4 */
	char *ptr = getenv("KONSOLE_DCOP_SESSION"); /* Try KDE's Konsole first. */
	if (ptr) {
		/* We are in KDE's Konsole, or in a terminal window detached from a Konsole.
		* In order to active a tab:
		* 1. get environmental variable KONSOLE_DCOP_SESSION: it includes konsole id and session name
		* 2. if
		*	$WINDOWID is defined and it equals
		*	    `dcop konsole-3152 konsole-mainwindow#1 getWinID`
		*	(KDE 3.2) or when $WINDOWID is undefined (KDE 3.1), then run commands
		*    dcop konsole-3152 konsole activateSession session-2; \
		*    dcop konsole-3152 konsole-mainwindow#1 raise
		* Note: by $WINDOWID we mean gnuplot's text console WINDOWID.
		* Missing: focus is not transferred unless $WINDOWID is defined (should be fixed in KDE 3.2).
		*
		* Implementation and tests on KDE 3.1.4: Petr Mikulik.
		*/
		char *konsole_name = NULL;
		char *cmd = NULL;
		/* use 'while' to easily break out (aka catch exception) */
		while (1) {
			char *konsole_tab;
			unsigned long w;
			FILE *p;
			ptr = strchr(ptr, '(');
			/* the string for tab nb 4 looks like 'DCOPRef(konsole-2866, session-4)' */
			if (!ptr) break;
			konsole_name = strdup(ptr+1);
			konsole_tab = strchr(konsole_name, ',');
			if (!konsole_tab) break;
			*konsole_tab++ = 0;
			ptr = strchr(konsole_tab, ')');
			if (ptr) *ptr = 0;
			cmd = (char*) malloc(strlen(konsole_name) + strlen(konsole_tab) + 64);
			sprintf(cmd, "dcop %s konsole-mainwindow#1 getWinID 2>/dev/null", konsole_name);
			/* is  2>/dev/null  portable among various shells? */
			p = popen(cmd, "r");
			if (p) {
				fscanf(p, "%lu", &w);
				pclose(p);
			}
			if (windowid) { /* $WINDOWID is known */
			if (w != windowid) break;
		/* `dcop getWinID`==$WINDOWID thus we are running in a window detached from Konsole */
			} else {
				windowid = w;
				/* $WINDOWID has not been known (KDE 3.1), thus set it up */
			}
			sprintf(cmd, "dcop %s konsole activateSession %s", konsole_name, konsole_tab);
			system(cmd);
		}
		if (konsole_name) free(konsole_name);
		if (cmd) free(cmd);
	}
/* NOTE: End of DCOP/KDE3 code (doesn't work in KDE4) */
	/* now test for GNOME multitab console */
	/* ... if somebody bothers to implement it ... */
	/* we are not running in any known (implemented) multitab console */

	if (windowid) {
		gdk_window_raise(gdk_window_foreign_new(windowid));
		gdk_window_focus(gdk_window_foreign_new(windowid), GDK_CURRENT_TIME);
	}
#endif /* USE_GTK */

#ifdef WIN32
	WinRaiseConsole();
#endif

#ifdef OS2
	/* we assume that the console window is managed by PM, not by a X server */
	HSWITCH hSwitch = 0;
	SWCNTRL swGnu;
	HWND hw;
	/* get details of command-line window */
	hSwitch = WinQuerySwitchHandle(0, getpid());
	WinQuerySwitchEntry(hSwitch, &swGnu);
	hw = WinQueryWindow(swGnu.hwnd, QW_BOTTOM);
	WinSetFocus(HWND_DESKTOP, hw);
	WinSwitchToProgram(hSwitch);
#endif /* OS2 */
}

/* ====license information====
 * End of the non-relicensable portion.
 */
#endif /* DISABLE_SPACE_RAISES_CONSOLE */


/* ------------------------------------------------------
 * Configuration dialog
 * ------------------------------------------------------*/

/* configuration dialog : handler for a close event */
void wxtConfigDialog::OnClose( wxCloseEvent& WXUNUSED( event ) )
{
	wxtFrame *parent = (wxtFrame *) GetParent();
	parent->config_displayed = false;
	this->Destroy();
}

/* configuration dialog : handler for a button event */
void wxtConfigDialog::OnButton( wxCommandEvent& event )
{
	TBOOLEAN antialiasing;
	TBOOLEAN oversampling;

	wxConfigBase *pConfig = wxConfigBase::Get();
	Validate();
	TransferDataFromWindow();

	wxtFrame *parent = (wxtFrame *) GetParent();

	switch (event.GetId()) {
	case Config_OK :
		Close(true);
		/* continue */
	case Config_APPLY :
		/* changes are applied immediately */
		wxt_raise = raise_setting?yes:no;
		wxt_persist = persist_setting?yes:no;
		wxt_ctrl = ctrl_setting?yes:no;
		wxt_toggle = toggle_setting?yes:no;
		wxt_redraw = redraw_setting?yes:no;

		switch (rendering_setting) {
		case 0 :
			antialiasing = FALSE;
			oversampling = FALSE;
			break;
		case 1 :
			antialiasing = TRUE;
			oversampling = FALSE;
			break;
		case 2 :
		default :
			antialiasing = TRUE;
			oversampling = TRUE;
			break;
		}

		/* we cannot apply the new settings right away, because it would mess up
		 * the plot in case of a window resize.
		 * Instead, we queue the settings until the next plot. */
		parent->panel->wxt_settings_queue(antialiasing, oversampling, hinting_setting);

		if (!pConfig->Write(wxT("raise"), raise_setting))
			wxLogError(wxT("Cannot write raise"));
		if (!pConfig->Write(wxT("persist"), persist_setting))
			wxLogError(wxT("Cannot write persist"));
		if (!pConfig->Write(wxT("ctrl"), ctrl_setting))
			wxLogError(wxT("Cannot write ctrl"));
		if (!pConfig->Write(wxT("toggle"), toggle_setting))
			wxLogError(wxT("Cannot write toggle"));
		if (!pConfig->Write(wxT("redraw"), redraw_setting))
			wxLogError(wxT("Cannot write redraw_setting"));
		if (!pConfig->Write(wxT("rendering"), rendering_setting))
			wxLogError(wxT("Cannot write rendering_setting"));
		if (!pConfig->Write(wxT("hinting"), hinting_setting))
			wxLogError(wxT("Cannot write hinting_setting"));
		break;
	case Config_CANCEL :
	default :
		Close(true);
		break;
	}
}

/* Configuration dialog constructor */
wxtConfigDialog::wxtConfigDialog(wxWindow* parent)
	: wxDialog(parent, -1, wxT("Terminal configuration"), wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
/* 	wxStaticBox *sb = new wxStaticBox( this, wxID_ANY, wxT("&Explanation"),
		wxDefaultPosition, wxDefaultSize );
	wxStaticBoxSizer *wrapping_sizer = new wxStaticBoxSizer( sb, wxVERTICAL );
	wxStaticText *text1 = new wxStaticText(this, wxID_ANY,
		wxT("Options remembered between sessions, ")
		wxT("overriden by `set term wxt <options>`.\n\n"),
		wxDefaultPosition, wxSize(300, wxDefaultCoord));
	wrapping_sizer->Add(text1,wxSizerFlags(0).Align(0).Expand().Border(wxALL) );*/

	wxConfigBase *pConfig = wxConfigBase::Get();
	pConfig->Read(wxT("raise"),&raise_setting);
	pConfig->Read(wxT("persist"),&persist_setting);
	pConfig->Read(wxT("ctrl"),&ctrl_setting);
	pConfig->Read(wxT("toggle"),&toggle_setting);
	pConfig->Read(wxT("redraw"),&redraw_setting);
	pConfig->Read(wxT("rendering"),&rendering_setting);
	pConfig->Read(wxT("hinting"),&hinting_setting);

	wxCheckBox *check1 = new wxCheckBox (this, wxID_ANY,
		wxT("Put the window at the top of your desktop after each plot (raise)"),
		wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&raise_setting));
	wxCheckBox *check2 = new wxCheckBox (this, wxID_ANY,
		wxT("Don't quit until all windows are closed (persist)"),
		wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&persist_setting));
	wxCheckBox *check3 = new wxCheckBox (this, wxID_ANY,
		wxT("Replace 'q' by <ctrl>+'q' and <space> by <ctrl>+<space> (ctrl)"),
		wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&ctrl_setting));

	wxCheckBox *check4 = new wxCheckBox (this, wxID_ANY,
		wxT("Toggle plots on/off when key sample is clicked"),
		wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&toggle_setting));

	wxCheckBox *check5 = new wxCheckBox (this, wxID_ANY,
		wxT("Redraw continuously as plot is resized"),
		wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&redraw_setting));

	wxString choices[3];
	choices[0] = wxT("No antialiasing");
	choices[1] = wxT("Antialiasing");
	choices[2] = wxT("Antialiasing and oversampling");

	wxStaticBox *sb2 = new wxStaticBox( this, wxID_ANY,
		wxT("Rendering options (applied to the next plot)"),
		wxDefaultPosition, wxDefaultSize );
	wxStaticBoxSizer *box_sizer2 = new wxStaticBoxSizer( sb2, wxVERTICAL );

	wxStaticText *text_rendering = new wxStaticText(this, wxID_ANY,
		wxT("Rendering method :"));
	wxChoice *box = new wxChoice (this, Config_Rendering, wxDefaultPosition, wxDefaultSize,
		3, choices, 0, wxGenericValidator(&rendering_setting));

	text_hinting = new wxStaticText(this, wxID_ANY,
		wxT("Hinting (100=full,0=none) :"));

	slider = new wxSlider(this, wxID_ANY, 0, 0, 100,
		wxDefaultPosition, wxDefaultSize,
		wxSL_HORIZONTAL|wxSL_LABELS,
		wxGenericValidator(&hinting_setting));

	if (rendering_setting != 2) {
		slider->Enable(false);
		text_hinting->Enable(false);
	}
	box_sizer2->Add(text_rendering,wxSizerFlags().Align(0).Border(wxALL));
	box_sizer2->Add(box,wxSizerFlags().Align(0).Border(wxALL));
	box_sizer2->Add(text_hinting,wxSizerFlags().Align(0).Expand().Border(wxALL));
	box_sizer2->Add(slider,wxSizerFlags().Align(0).Expand().Border(wxALL));

	wxBoxSizer *hsizer = new wxBoxSizer( wxHORIZONTAL );
	hsizer->Add( new wxButton(this, Config_OK, wxT("OK")),
		wxSizerFlags().Align(0).Expand().Border(wxALL));
	hsizer->Add( new wxButton(this, Config_APPLY, wxT("Apply")),
		wxSizerFlags().Align(0).Expand().Border(wxALL));
	hsizer->Add( new wxButton(this, Config_CANCEL, wxT("Cancel")),
		wxSizerFlags().Align(0).Expand().Border(wxALL));

	wxBoxSizer *vsizer = new wxBoxSizer( wxVERTICAL );
	vsizer->Add(check1,wxSizerFlags().Align(0).Expand().Border(wxALL));
	vsizer->Add(check2,wxSizerFlags().Align(0).Expand().Border(wxALL));
	vsizer->Add(check3,wxSizerFlags().Align(0).Expand().Border(wxALL));
	vsizer->Add(check4,wxSizerFlags().Align(0).Expand().Border(wxALL));
	vsizer->Add(check5,wxSizerFlags().Align(0).Expand().Border(wxALL));
	vsizer->Add(box_sizer2,wxSizerFlags().Align(0).Expand().Border(wxALL));
	/*vsizer->Add(CreateButtonSizer(wxOK|wxCANCEL),wxSizerFlags().Align(0).Expand().Border(wxALL));*/
	vsizer->Add(hsizer,wxSizerFlags().Align(0).Expand().Border(wxALL));

	/* use the sizer for layout */
	SetSizer( vsizer );
	/* set size hints to honour minimum size */
	vsizer->SetSizeHints( this );
}

/* enable or disable the hinting slider depending on the selection of the oversampling method */
void wxtConfigDialog::OnRendering( wxCommandEvent& event )
{
	if (event.GetInt() != 2) {
		slider->Enable(false);
		text_hinting->Enable(false);
	} else {
		slider->Enable(true);
		text_hinting->Enable(true);
	}
}

/* ------------------------------------------------------
 * functions that are called by gnuplot
 * ------------------------------------------------------*/

/* "Called once, when the device is first selected."
 * Is the 'main' function of the terminal. */
void wxt_init()
{
	FPRINTF((stderr,"Init\n"));

	if ( wxt_abort_init ) {
		fprintf(stderr,"Previous attempt to initialize wxWidgets has failed. Not retrying.\n");
		return;
	}

	wxt_sigint_init();

	if ( wxt_status == STATUS_UNINITIALIZED ) {
		FPRINTF((stderr,"First Init\n"));

#if !defined(DEVELOPMENT_VERSION) && wxCHECK_VERSION(2, 9, 0)
		// disable all assert()s
		// affects in particular the strcmp(setlocale(LC_ALL, NULL), "C") == 0
		// assertion in wxLocale::GetInfo
		wxSetAssertHandler(NULL);
#endif

#ifdef __WXMSW__
		/* the following is done in wxEntry() with wxMSW only */
		WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);
		wxSetInstance(GetModuleHandle(NULL));
		wxApp::m_nCmdShow = SW_SHOW;
#endif

		if (!wxInitialize()) {
			fprintf(stderr,"Failed to initialize wxWidgets.\n");
			wxt_abort_init = true;
			if (interactive) {
				change_term("unknown",7);
				int_error(NO_CARET,"wxt init failure");
			} else
				gp_exit(EXIT_FAILURE);
		}

		/* app initialization */
		wxTheApp->CallOnInit();


#ifdef WXT_MULTITHREADED
		/* Three commands to create the thread and run it.
		 * We do this at first init only.
		 * If the user sets another terminal and goes back to wxt,
		 * the gui thread is already in action. */
		thread = new wxtThread();
		thread->Create();
		thread->Run();

# ifdef USE_MOUSE
		int filedes[2];

	       if (pipe(filedes) == -1) {
			fprintf(stderr, "Pipe error, mousing will not work\n");
		}

		wxt_event_fd = filedes[0];
		wxt_sendevent_fd = filedes[1];
# endif /* USE_MOUSE */
#endif /* WXT_MULTITHREADED */

 		FPRINTF((stderr,"First Init2\n"));

		term_interlock = (void *)wxt_init;

		/* register call for "persist" effect and cleanup */
		gp_atexit(wxt_atexit);
	}

	wxt_sigint_check();

	/* try to find the requested window in the list of existing ones */
	wxt_current_window = wxt_findwindowbyid(wxt_window_number);

	/* open a new plot window if it does not exist */
	if ( wxt_current_window == NULL ) {
		FPRINTF((stderr,"opening a new plot window\n"));

		/* create a new plot window and show it */
		wxt_window_t window;
		window.id = wxt_window_number;
		if (strlen(wxt_title))
			/* NOTE : this assumes that the title is encoded in the locale charset.
			 * This is probably a good assumption, but it is not guaranteed !
			 * May be improved by using gnuplot encoding setting. */
			window.title << wxString(wxt_title, wxConvLocal);
		else
			window.title.Printf(wxT("Gnuplot (window id : %d)"), window.id);

		window.mutex = new wxMutex();
		window.condition = new wxCondition(*(window.mutex));

		wxCommandEvent event(wxCreateWindowEvent);
		event.SetClientData((void*) &window);

#ifdef WXT_MULTITHREADED
		window.mutex->Lock();
#endif /* WXT_MULTITHREADED */
		wxt_MutexGuiEnter();
		dynamic_cast<wxtApp*>(wxTheApp)->SendEvent( event );
		wxt_MutexGuiLeave();
#ifdef WXT_MULTITHREADED
		/* While we are waiting, the other thread is busy mangling  */
		/* our locale settings. We will have to restore them later. */
		window.condition->Wait();
#endif /* WXT_MULTITHREADED */

		/* store the plot structure in the list and keep shortcuts */
		wxt_window_list.push_back(window);
		wxt_current_window = &(wxt_window_list.back());
	}

	/* initialize helper pointers */
	wxt_current_panel = wxt_current_window->frame->panel;
	wxt_current_plot = &(wxt_current_panel->plot);

	wxt_sigint_check();

	bool raise_setting;
	bool persist_setting;
	bool ctrl_setting;
	bool toggle_setting;
	bool redraw_setting;
	int rendering_setting;
	int hinting_setting;

	/* if needed, restore the setting from the config file/registry keys.
	 * Unset values are set to default reasonable values. */
	wxConfigBase *pConfig = wxConfigBase::Get();

	if (!pConfig->Read(wxT("raise"), &raise_setting)) {
		pConfig->Write(wxT("raise"), true);
		raise_setting = true;
	}
	if (wxt_raise==UNSET)
		wxt_raise = raise_setting?yes:no;

	if (!pConfig->Read(wxT("persist"), &persist_setting))
		pConfig->Write(wxT("persist"), false);

	if (!pConfig->Read(wxT("ctrl"), &ctrl_setting)) {
		pConfig->Write(wxT("ctrl"), false);
		ctrl_setting = false;
	}
	if (wxt_ctrl==UNSET)
		wxt_ctrl = ctrl_setting?yes:no;

	if (!pConfig->Read(wxT("toggle"), &toggle_setting)) {
		pConfig->Write(wxT("toggle"), true);
		toggle_setting = true;
	}
	if (wxt_toggle==UNSET)
		wxt_toggle = toggle_setting?yes:no;

	if (!pConfig->Read(wxT("redraw"), &redraw_setting)) {
		pConfig->Write(wxT("redraw"), false);
		redraw_setting = false;
	}
	if (wxt_redraw==UNSET)
		wxt_redraw = redraw_setting?yes:no;

	if (!pConfig->Read(wxT("rendering"), &rendering_setting)) {
		pConfig->Write(wxT("rendering"), 2);
		rendering_setting = 2;
	}
	switch (rendering_setting) {
	case 0 :
		wxt_current_plot->antialiasing = FALSE;
		wxt_current_plot->oversampling = FALSE;
		break;
	case 1 :
		wxt_current_plot->antialiasing = TRUE;
		wxt_current_plot->oversampling = FALSE;
		break;
	case 2 :
	default :
		wxt_current_plot->antialiasing = TRUE;
		wxt_current_plot->oversampling = TRUE;
		break;
	}

	if (!pConfig->Read(wxT("hinting"), &hinting_setting)) {
		pConfig->Write(wxT("hinting"), 100);
		hinting_setting = 100;
	}
	wxt_current_plot->hinting = hinting_setting;

#ifdef HAVE_LOCALE_H
	/* when wxGTK was initialised above, GTK+ also set the locale of the
	 * program itself;  we must revert it */
	if (wxt_status == STATUS_UNINITIALIZED) {
		extern char *current_locale;
		setlocale(LC_NUMERIC, "C");
		setlocale(LC_TIME, current_locale);
	}
#endif

	/* accept the following commands from gnuplot */
	wxt_status = STATUS_OK;
	wxt_current_plot->interrupt = FALSE;

	wxt_sigint_check();
	wxt_sigint_restore();

	FPRINTF((stderr,"Init finished \n"));
}


/* "Called just before a plot is going to be displayed."
 * Should clear the terminal. */
void wxt_graphics()
{
	if (wxt_status != STATUS_OK)
		return;

	/* The sequence of gnuplot commands is critical as it involves mutexes.
	 * We replace the original interrupt handler with a custom one. */
	wxt_sigint_init();

	/* update the window scale factor first, cairo needs it */
	wxt_current_plot->xscale = 1.0;
	wxt_current_plot->yscale = 1.0;

	/* set the line properties */
	/* FIXME: should this be in wxt_settings_apply() ? */
	wxt_current_plot->linecap = wxt_linecap;
	wxt_current_plot->dashlength = wxt_dashlength;

	/* background as given by set term */
	wxt_current_plot->background = wxt_rgb_background;
	gp_cairo_set_background(wxt_rgb_background);

	/* apply the queued rendering settings */
	wxt_current_panel->wxt_settings_apply();

	wxt_MutexGuiEnter();
	/* set the transformation matrix of the context, and other details */
	/* depends on plot->xscale and plot->yscale */
	gp_cairo_initialize_context(wxt_current_plot);

	/* set or refresh terminal size according to the window size */
	/* oversampling_scale is updated in gp_cairo_initialize_context */
	term->xmax = (unsigned int) wxt_current_plot->device_xmax*wxt_current_plot->oversampling_scale;
	term->ymax = (unsigned int) wxt_current_plot->device_ymax*wxt_current_plot->oversampling_scale;
	wxt_current_plot->xmax = term->xmax;
	wxt_current_plot->ymax = term->ymax;
	/* initialize encoding */
	wxt_current_plot->encoding = encoding;

	wxt_MutexGuiLeave();

	/* set font details (h_char, v_char) according to settings */
	wxt_set_font("");

	term->v_tic = (unsigned int) (term->v_char/2.5);
	term->h_tic = (unsigned int) (term->v_char/2.5);

	/* clear the command list, and free the allocated memory */
	wxt_current_panel->ClearCommandlist();

	/* Don't reset the hide_plot flags if this refresh is a zoom/unzoom */
	if (wxt_zoom_command)
		wxt_zoom_command = FALSE;
	else
		wxt_initialize_hidden(0);

	/* Clear the count of hypertext anchor points */
	wxt_n_anchors = 0;

	wxt_sigint_check();
	wxt_sigint_restore();

	FPRINTF((stderr,"Graphics xmax %d ymax %d v_char %d h_char %d\n",
		term->xmax, term->ymax, term->v_char, term->h_char));
}

void wxt_text()
{
	if (wxt_status != STATUS_OK) {
#ifdef USE_MOUSE
		/* Inform gnuplot that we have finished plotting.
		 * This avoids to lose the mouse after a interrupt.
		 * Do this immediately, instead of posting an event which may not be processed. */
		event_plotdone();
#endif
		return;
	}

#ifdef USE_MOUSE
	/* Save a snapshot of the axis state so that we can continue
	 * to update mouse cursor coordinates even though the plot is not active */
	wxt_current_window->axis_mask = wxt_axis_mask;
	memcpy( wxt_current_window->axis_state,
	 	wxt_axis_state, sizeof(wxt_axis_state) );
#endif

	wxt_sigint_init();

	/* translates the command list to a bitmap */
	wxt_MutexGuiEnter();
	wxt_current_panel->wxt_cairo_refresh();
	wxt_MutexGuiLeave();

	wxt_sigint_check();

	/* raise the window, depending on the user's choice */
	wxt_MutexGuiEnter();
	wxt_raise_window(wxt_current_window, false);
	wxt_MutexGuiLeave();

#ifdef USE_MOUSE
	/* Inform gnuplot that we have finished plotting */
	wxt_exec_event(GE_plotdone, 0, 0, 0, 0, wxt_window_number );
#endif /*USE_MOUSE*/

	wxt_sigint_check();
	wxt_sigint_restore();
}

void wxt_reset()
{
	/* sent when gnuplot exits and when the terminal or the output change.*/
	FPRINTF((stderr,"wxt_reset\n"));

#if defined(WXT_MONOTHREADED) && !defined(_Windows)
	yield = 0;
#endif

	if (wxt_status == STATUS_UNINITIALIZED)
		return;

#ifdef USE_MOUSE
	if (wxt_status == STATUS_INTERRUPT) {
		/* send "reset" event to restore the mouse system in a well-defined state.
		 * Send it directly, not with wxt_exec_event(), which would only enqueue it,
		 * but not process it. */
		FPRINTF((stderr,"send reset event to the mouse system\n"));
		event_reset((gp_event_t *)1);   /* cancel zoombox etc. */
	}
#endif /*USE_MOUSE*/

	FPRINTF((stderr,"wxt_reset ends\n"));
}

void wxt_move(unsigned int x, unsigned int y)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_move;
	temp_command.x1 = x;
	temp_command.y1 = term->ymax - y;

	wxt_command_push(temp_command);
}

void wxt_vector(unsigned int x, unsigned int y)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_vector;
	temp_command.x1 = x;
	temp_command.y1 = term->ymax - y;

	wxt_command_push(temp_command);
}

void wxt_enhanced_flush()
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;
	temp_command.command = command_enhanced_flush;

	wxt_command_push(temp_command);
}

void wxt_enhanced_writec(int c)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;
	temp_command.command = command_enhanced_writec;
	temp_command.integer_value = c;

	wxt_command_push(temp_command);
}

void wxt_enhanced_open(char* fontname, double fontsize, double base, TBOOLEAN widthflag, TBOOLEAN showflag, int overprint)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;
	temp_command.command = command_enhanced_open;
	temp_command.string = new char[strlen(fontname)+1];
	strcpy(temp_command.string, fontname);
	temp_command.double_value = fontsize;
	temp_command.double_value2 = base;
	temp_command.integer_value = overprint;
	temp_command.integer_value2 = widthflag + (showflag << 1);

	wxt_command_push(temp_command);
}

void wxt_put_text(unsigned int x, unsigned int y, const char * string)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	/* if ignore_enhanced_text is set, draw with the normal routine.
	 * This is meant to avoid enhanced syntax when the enhanced mode is on */
	if (wxt_enhanced_enabled && !ignore_enhanced_text) {
		/* Uses enhanced_recursion() to analyse the string to print.
		 * enhanced_recursion() calls _enhanced_open() to initialize the text drawing,
		 * then it calls _enhanced_writec() which buffers the characters to draw,
		 * and finally _enhanced_flush() to draw the buffer with the correct justification. */

		/* init */
		temp_command.command = command_enhanced_init;
		temp_command.integer_value = strlen(string);
		temp_command.x1 = x;
		temp_command.y1 = term->ymax - y;
		wxt_command_push(temp_command);

		/* set up the global variables needed by enhanced_recursion() */
		enhanced_fontscale = wxt_set_fontscale;
		strncpy(enhanced_escape_format, "%c", sizeof(enhanced_escape_format));

		/* Set the recursion going. We say to keep going until a
		* closing brace, but we don't really expect to find one.
		* If the return value is not the nul-terminator of the
		* string, that can only mean that we did find an unmatched
		* closing brace in the string. We increment past it (else
		* we get stuck in an infinite loop) and try again. */

		while (*(string = enhanced_recursion((char*)string, TRUE,
				wxt_enhanced_fontname,
				wxt_current_plot->fontsize * wxt_set_fontscale,
				0.0, TRUE, TRUE, 0))) {
			wxt_enhanced_flush();

			/* we can only get here if *str == '}' */
			enh_err_check(string);

			if (!*++string)
				break; /* end of string */
			/* else carry on and process the rest of the string */
		}

		/* finish */
		temp_command.command = command_enhanced_finish;
		temp_command.x1 = x;
		temp_command.y1 = term->ymax - y;
		wxt_command_push(temp_command);
		return;
	}

	temp_command.command = command_put_text;

	temp_command.x1 = x;
	temp_command.y1 = term->ymax - y;
	/* Note : we must take '\0' (EndOfLine) into account */
	temp_command.string = new char[strlen(string)+1];
	strcpy(temp_command.string, string);

	wxt_command_push(temp_command);
}

void wxt_linetype(int lt)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;
	gp_command temp_command2;

	temp_command2.command = command_linestyle;
	if (lt == LT_AXIS)
		temp_command2.integer_value = GP_CAIRO_DOTS;
	else
		temp_command2.integer_value = GP_CAIRO_SOLID;
	wxt_command_push(temp_command2);

	temp_command.command = command_linetype;
	temp_command.integer_value = lt;
	wxt_command_push(temp_command);

	temp_command.command = command_color;
	temp_command.color = gp_cairo_linetype2color( lt );
	temp_command.double_value = 0.0; // alpha
	wxt_command_push(temp_command);
}

void wxt_dashtype(int type, t_dashtype *custom_dash_pattern)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;
	temp_command.command = command_dashtype;
	temp_command.integer_value = type;
	if (type == DASHTYPE_CUSTOM) {
	    temp_command.dashpattern = (t_dashtype *)malloc(sizeof(t_dashtype));
	    memcpy(temp_command.dashpattern, custom_dash_pattern, sizeof(t_dashtype));
	} else {
	    temp_command.dashpattern = NULL;
	}
	wxt_command_push(temp_command);
}

/* - fonts are selected as strings "name,size".
 * - _set_font("") restores the terminal's default font.*/
int wxt_set_font (const char *font)
{
	if (wxt_status != STATUS_OK)
		return 1;

	char *fontname = NULL;
	gp_command temp_command;
	int fontsize = 0;

	temp_command.command = command_set_font;

	if (font && (*font)) {
		int sep = strcspn(font,",");
		fontname = strdup(font);
		if (font[sep] == ',') {
			sscanf(&(font[sep+1]), "%d", &fontsize);
			fontname[sep] = '\0';
		}
	} else {
		fontname = strdup("");
	}

	wxt_sigint_init();
	wxt_MutexGuiEnter();

	if ( strlen(fontname) == 0 ) {
		if ( !wxt_set_fontname || strlen(wxt_set_fontname) == 0 ) {
			free(fontname);
			fontname = strdup(gp_cairo_default_font());
		} else {
			free(fontname);
			fontname = strdup(wxt_set_fontname);
		}
	}

	if ( fontsize == 0 ) {
		if ( wxt_set_fontsize == 0 )
			fontsize = 10;
		else
			fontsize = wxt_set_fontsize;
	}

	/* Reset the term variables (hchar, vchar, h_tic, v_tic).
	 * They may be taken into account in next plot commands */
	gp_cairo_set_font(wxt_current_plot, fontname, fontsize * wxt_set_fontscale);
	gp_cairo_set_termvar(wxt_current_plot, &(term->v_char),
	                                       &(term->h_char));
	gp_cairo_set_font(wxt_current_plot, fontname, fontsize);

	wxt_MutexGuiLeave();
	wxt_sigint_check();
	wxt_sigint_restore();

	/* Note : we must take '\0' (EndOfLine) into account */
	temp_command.string = new char[strlen(fontname)+1];
	strcpy(temp_command.string, fontname);
	temp_command.integer_value = fontsize * wxt_set_fontscale;

	wxt_command_push(temp_command);

	/* Enhanced text processing needs to know the new font also */
	if (*fontname) {
		free(wxt_enhanced_fontname);
		wxt_enhanced_fontname = strdup(fontname);
	}
	free(fontname);

	/* the returned int is not used anywhere */
	return 1;
}


int wxt_justify_text(enum JUSTIFY mode)
{
	if (wxt_status != STATUS_OK)
		return 1;

	gp_command temp_command;

	temp_command.command = command_justify;
	temp_command.mode = mode;

	wxt_command_push(temp_command);
	return 1; /* we can justify */
}

void wxt_point(unsigned int x, unsigned int y, int pointstyle)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_point;
	temp_command.x1 = x;
	temp_command.y1 = term->ymax - y;
	temp_command.integer_value = pointstyle;

	wxt_command_push(temp_command);

	if (pending_href) {
		/* FIXME:  How to convert current pointsize to term coords? */
		int size = 400;
		wxt_update_anchors(x, y, size);
		pending_href = FALSE;
	}
}

void wxt_pointsize(double ptsize)
{
	if (wxt_status != STATUS_OK)
		return;

	/* same behaviour as x11 terminal */
	if (ptsize<0) ptsize = 1;

	gp_command temp_command;
	temp_command.command = command_pointsize;
	temp_command.double_value = ptsize;

	wxt_command_push(temp_command);
}

void wxt_linewidth(double lw)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_linewidth;
	temp_command.double_value = lw * wxt_lw;

	wxt_command_push(temp_command);
}

int wxt_text_angle(int angle)
{
	if (wxt_status != STATUS_OK)
		return 1;

	gp_command temp_command;

	temp_command.command = command_text_angle;
	/* a double is needed to compute cos, sin, etc. */
	temp_command.double_value = (double) angle;

	wxt_command_push(temp_command);
	return 1; /* 1 means we can rotate */
}

void wxt_fillbox(int style, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_fillbox;
	temp_command.x1 = x;
	temp_command.y1 = term->ymax - y;
	temp_command.x2 = width;
	temp_command.y2 = height;
	temp_command.integer_value = style;

	wxt_command_push(temp_command);
}

int wxt_make_palette(t_sm_palette * palette)
{
	/* we can do continuous colors */
	return 0;
}

void wxt_set_color(t_colorspec *colorspec)
{
	if (wxt_status != STATUS_OK)
		return;

	rgb_color rgb1;
	gp_command temp_command;
	double alpha = 0.0;

	if (colorspec->type == TC_LT) {
		rgb1 = gp_cairo_linetype2color(colorspec->lt);
	} else if (colorspec->type == TC_FRAC)
		rgb1maxcolors_from_gray( colorspec->value, &rgb1 );
	else if (colorspec->type == TC_RGB) {
		rgb1.r = (double) ((colorspec->lt >> 16) & 0xff)/255;
		rgb1.g = (double) ((colorspec->lt >> 8) & 0xff)/255;
		rgb1.b = (double) ((colorspec->lt) & 0xff)/255;
		alpha = (double) ((colorspec->lt >> 24) & 0xff)/255;
	} else return;

	temp_command.command = command_color;
	temp_command.color = rgb1;
	temp_command.double_value = alpha;

	wxt_command_push(temp_command);
}


/* here we send the polygon command */
void wxt_filled_polygon(int n, gpiPoint *corners)
{
	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_filled_polygon;
	temp_command.integer_value = n;
	temp_command.corners = new gpiPoint[n];
	/* can't use memcpy() here, as we have to mirror the y axis */
	gpiPoint *corners_copy = temp_command.corners;
	while (corners_copy < (temp_command.corners + n)) {
		*corners_copy = *corners++;
		corners_copy->y = term->ymax - corners_copy->y;
		++corners_copy;
	}

	wxt_command_push(temp_command);
}

void wxt_image(unsigned int M, unsigned int N, coordval * image, gpiPoint * corner, t_imagecolor color_mode)
{
	/* This routine is to plot a pixel-based image on the display device.
	'M' is the number of pixels along the y-dimension of the image and
	'N' is the number of pixels along the x-dimension of the image.  The
	coordval pointer 'image' is the pixel values normalized to the range
	[0:1].  These values should be scaled accordingly for the output
	device.  They 'image' data starts in the upper left corner and scans
	along rows finishing in the lower right corner.  If 'color_mode' is
	IC_PALETTE, the terminal is to use palette lookup to generate color
	information.  In this scenario the size of 'image' is M*N.  If
	'color_mode' is IC_RGB, the terminal is to use RGB components.  In
	this scenario the size of 'image' is 3*M*N.  The data appears in RGB
	tripples, i.e., image[0] = R(1,1), image[1] = G(1,1), image[2] =
	B(1,1), image[3] = R(1,2), image[4] = G(1,2), ..., image[3*M*N-1] =
	B(M,N).  The 'image' is actually an "input" image in the sense that
	it must also be properly resampled for the output device.  Many output
	mediums, e.g., PostScript, do this work via various driver functions.
	To determine the appropriate rescaling, the 'corner' information
	should be used.  There are four entries in the gpiPoint data array.
	'corner[0]' is the upper left corner (in terms of plot location) of
	the outer edge of the image.  Similarly, 'corner[1]' is the lower
	right corner of the outer edge of the image.  (Outer edge means the
	outer extent of the corner pixels, not the middle of the corner
	pixels.)  'corner[2]' is the upper left corner of the visible part
	of the image, and 'corner[3]' is the lower right corner of the visible
	part of the image.  The information is provided in this way because
	often it is necessary to clip a portion of the outer pixels of the
	image. */

	/* we will draw an image, scale and resize it, copy to bitmap,
	 * give a pointer to it in the list, and when processing the list we will use DrawBitmap */
	/* FIXME add palette support ??? */

	if (wxt_status != STATUS_OK)
		return;

	gp_command temp_command;

	temp_command.command = command_image;
	temp_command.x1 = corner[0].x;
	temp_command.y1 = term->ymax - corner[0].y;
	temp_command.x2 = corner[1].x;
	temp_command.y2 = term->ymax - corner[1].y;
	temp_command.x3 = corner[2].x;
	temp_command.y3 = term->ymax - corner[2].y;
	temp_command.x4 = corner[3].x;
	temp_command.y4 = term->ymax - corner[3].y;
	temp_command.integer_value = M;
	temp_command.integer_value2 = N;

	temp_command.image = gp_cairo_helper_coordval_to_chars(image, M, N, color_mode);

	wxt_command_push(temp_command);
}

/* This is meta-information about the plot state
 */
void wxt_layer(t_termlayer layer)
{
	/* There are two classes of meta-information.  The first class	*/
	/* is tied to the current state of the user interface or the	*/
	/* main gnuplot thread.  Any action on these must be done here,	*/
	/* immediately.  The second class relates to the sequence of	*/
	/* operations in the plot itself.  These are buffered for later	*/
	/* execution in sequential order.				*/
	if (layer == TERM_LAYER_BEFORE_ZOOM) {
		wxt_zoom_command = TRUE;
		return;
	}
	if (layer == TERM_LAYER_RESET || layer == TERM_LAYER_RESET_PLOTNO) {
		if (multiplot)
			return;
	}

	gp_command temp_command;
	temp_command.command = command_layer;
	temp_command.integer_value = layer;
	wxt_command_push(temp_command);
}

void wxt_hypertext(int type, const char * text)
{
	if (wxt_status != STATUS_OK)
		return;

	if (type != TERM_HYPERTEXT_TOOLTIP)
		return;

	gp_command temp_command;

	temp_command.command = command_hypertext;
	temp_command.integer_value = type;
	temp_command.string = new char[strlen(text)+1];
	strcpy(temp_command.string, text);

	wxt_command_push(temp_command);
	pending_href = TRUE;
}


#ifdef USE_MOUSE
/* Display temporary text, after
 * erasing any temporary text displayed previously at this location.
 * The int determines where: 0=statusline, 1,2: at corners of zoom
 * box, with \r separating text above and below the point. */
void wxt_put_tmptext(int n, const char str[])
{
	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	wxt_sigint_init();
	wxt_MutexGuiEnter();

	switch ( n ) {
	case 0:
		{
			wxCommandEvent event(wxStatusTextEvent);
			event.SetString(wxString(str, wxConvLocal));
			wxt_current_window->frame->SendEvent( event );
		}
		break;
	case 1:
		wxt_current_panel->zoom_x1 = wxt_current_panel->mouse_x;
		wxt_current_panel->zoom_y1 = wxt_current_panel->mouse_y;
		wxt_current_panel->zoom_string1 =  wxString(str, wxConvLocal);
		break;
	case 2:
		if ( strlen(str)==0 )
			wxt_current_panel->wxt_zoombox = false;
		else {
			wxt_current_panel->wxt_zoombox = true;
			wxt_current_panel->zoom_string2 =  wxString(str, wxConvLocal);
		}
		wxt_current_panel->Draw();
		break;
	default :
		break;
	}

	wxt_MutexGuiLeave();
	wxt_sigint_check();
	wxt_sigint_restore();
}

/* c selects the action:
 * -4=don't draw (erase) line between ruler and current mouse position,
 * -3=draw line between ruler and current mouse position,
 * -2=warp the cursor to the given point,
 * -1=start zooming,
 * 0=standard cross-hair cursor,
 * 1=cursor during rotation,
 * 2=cursor during scaling,
 * 3=cursor during zooming. */
void wxt_set_cursor(int c, int x, int y)
{
	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	wxt_sigint_init();
	wxt_MutexGuiEnter();

	switch ( c ) {
	case -4:
		wxt_current_panel->wxt_ruler_lineto = false;
		wxt_current_panel->Draw();
		break;
	case -3:
		wxt_current_panel->wxt_ruler_lineto = true;
		wxt_current_panel->Draw();
		break;
	case -2: /* warp the pointer to the given position */
		wxt_current_panel->WarpPointer(
				(int) device_x(wxt_current_plot, x),
				(int) device_y(wxt_current_plot, y) );
		break;
	case -1: /* start zooming */
		wxt_current_panel->SetCursor(wxt_cursor_right);
		break;
	case 0: /* cross-hair cursor, also cancel zoombox when Echap is pressed */
		wxt_current_panel->wxt_zoombox = false;
		wxt_current_panel->SetCursor(wxt_cursor_cross);
		wxt_current_panel->Draw();
		break;
	case 1: /* rotation */
		wxt_current_panel->SetCursor(wxt_cursor_rotate);
		break;
	case 2: /* scaling */
		wxt_current_panel->SetCursor(wxt_cursor_size);
		break;
	case 3: /* zooming */
		wxt_current_panel->SetCursor(wxt_cursor_right);
		break;
	default:
		wxt_current_panel->SetCursor(wxt_cursor_cross);
		break;
	}

	wxt_MutexGuiLeave();
	wxt_sigint_check();
	wxt_sigint_restore();
}


/* Draw a ruler (crosshairs) centered at the
 * indicated screen coordinates.  If x<0, switch ruler off. */
void wxt_set_ruler(int x, int y)
{
	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	wxt_sigint_init();
	wxt_MutexGuiEnter();

	if (x<0) {
		wxt_current_panel->wxt_ruler = false;
		wxt_current_panel->Draw();
	} else {
		wxt_current_panel->wxt_ruler = true;
		wxt_current_panel->wxt_ruler_x = device_x(wxt_current_plot, x);
		wxt_current_panel->wxt_ruler_y = device_y(wxt_current_plot, y);
		wxt_current_panel->Draw();
	}

	wxt_MutexGuiLeave();
	wxt_sigint_check();
	wxt_sigint_restore();
}

/* Write a string to the clipboard */
void wxt_set_clipboard(const char s[])
{
	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	wxt_sigint_init();
	wxt_MutexGuiEnter();

	if (wxTheClipboard->Open()) {
		wxTheClipboard->SetData( new wxTextDataObject(wxString(s, wxConvLocal)) );
		wxTheClipboard->Flush();
		wxTheClipboard->Close();
	}

	wxt_MutexGuiLeave();
	wxt_sigint_check();
	wxt_sigint_restore();
}
#endif /*USE_MOUSE*/

#ifdef EAM_BOXED_TEXT
/* Pass through the boxed text options to cairo */
void wxt_boxed_text(unsigned int x, unsigned int y, int option)
{
	gp_command temp_command;
	if (option != 3)
	    y = term->ymax - y;
	temp_command.command = command_boxed_text;
	temp_command.x1 = x;
	temp_command.y1 = y;
	temp_command.integer_value = option;
	wxt_command_push(temp_command);
}
#endif

void wxt_modify_plots(unsigned int ops, int plotno)
{
	int i;
	plotno++;

	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	for (i=1; i<=wxt_cur_plotno && i<wxt_max_key_boxes; i++) {
		if (plotno > 0 && i != plotno)
			continue;
		if ((ops & MODPLOTS_INVERT_VISIBILITIES) == MODPLOTS_INVERT_VISIBILITIES) {
			wxt_key_boxes[i].hidden = !wxt_key_boxes[i].hidden;
		} else if (ops & MODPLOTS_SET_VISIBLE) {
			wxt_key_boxes[i].hidden = FALSE;
		} else if (ops & MODPLOTS_SET_INVISIBLE) {
			wxt_key_boxes[i].hidden = TRUE;
		}
	}
	wxt_MutexGuiEnter();
	wxt_current_panel->wxt_cairo_refresh();
	wxt_MutexGuiLeave();
}

/* ===================================================================
 * Command list processing
 * =================================================================*/

/* push a command in the current commands list */
void wxt_command_push(gp_command command)
{
	wxt_sigint_init();
	wxt_current_panel->command_list_mutex.Lock();
	wxt_current_panel->command_list.push_back(command);
	wxt_current_panel->command_list_mutex.Unlock();
	wxt_sigint_check();
	wxt_sigint_restore();
}

/* refresh the plot by (re)processing the plot commands list */
void wxtPanel::wxt_cairo_refresh()
{
	/* This check may prevent the assert+die behavior seen with wxgtk3.0 */
	/* Symptom:
	  ./src/gtk/dcclient.cpp(2043): assert "m_window" failed in DoGetSize(): GetSize() doesn't work without window [in thread 7fb21f386700]
	  Call stack:
	  [00] wxOnAssert(char const*, int, char const*, char const*, wchar_t const*)
	  [01] wxClientDCImpl::DoGetSize(int*, int*) const
	  [02] wxBufferedDC::UnMask()                  
	  wxwidgets documentation to the contrary, panel->IsShownOnScreen() is unreliable
	 */
	if (!wxt_current_window) {
		FPRINTF((stderr,"wxt_cairo_refresh called before window exists\n"));
		return;
	}

	/* Clear background. */
	gp_cairo_solid_background(&plot);

	/* Initialize toggle in keybox mechanism */
	wxt_cur_plotno = 0;
	wxt_in_key_sample = FALSE;
#ifdef USE_MOUSE
	wxt_initialize_key_boxes(0);
#endif

	/* Initialize the hypertext tracking mechanism */
	wxt_display_hypertext = NULL;
	wxt_display_anchor.x = 0;
	wxt_display_anchor.y = 0;

	command_list_mutex.Lock();
	command_list_t::iterator wxt_iter; /*declare the iterator*/
	for(wxt_iter = command_list.begin(); wxt_iter != command_list.end(); ++wxt_iter) {
		if (wxt_status == STATUS_INTERRUPT_ON_NEXT_CHECK) {
			FPRINTF((stderr,"interrupt detected inside drawing loop\n"));
#ifdef IMAGE_SURFACE
			wxt_cairo_create_bitmap();
#endif /* IMAGE_SURFACE */
			/* draw the pixmap to the screen */
			Draw();
			return;
		}

		/* Skip the plot commands, but not the key sample commands,
		 * if the plot was toggled off by a mouse click in the GUI
		 */
		if (wxt_in_plot && !wxt_in_key_sample
		&&  wxt_iter->command != command_layer
		&&  wxt_cur_plotno < wxt_max_key_boxes
		&&  wxt_key_boxes[wxt_cur_plotno].hidden)
			continue;

		if (wxt_in_key_sample
		&&  wxt_iter->command == command_layer	/* catches TERM_LAYER_END_KEYSAMPLE */
		&&  wxt_cur_plotno < wxt_max_key_boxes
		&&  wxt_key_boxes[wxt_cur_plotno].hidden)
			gp_cairo_boxed_text(&plot, 0, 0, TEXTBOX_GREY);

		wxt_cairo_exec_command( *wxt_iter );

	}
	command_list_mutex.Unlock();

	/* don't forget to stroke the last path if vector was the last command */
	gp_cairo_stroke(&plot);
	/* and don't forget to draw the polygons if draw_polygon was the last command */
	gp_cairo_end_polygon(&plot);

	/* If we detected the mouse over a hypertext anchor, draw it now. */
	if (wxt_display_hypertext)
		wxt_cairo_draw_hypertext();

#ifdef IMAGE_SURFACE
	wxt_cairo_create_bitmap();
#endif /* !have_gtkcairo */

	/* draw the pixmap to the screen */
	Draw();
}


void wxtPanel::wxt_cairo_exec_command(gp_command command)
{
	static JUSTIFY text_justification_mode = LEFT;
	static char *current_href = NULL;

	switch ( command.command ) {
	case command_color :
		gp_cairo_set_color(&plot,command.color,command.double_value);
		return;
	case command_filled_polygon :
		if (wxt_in_key_sample) {
			wxt_update_key_box(command.x1 - term->h_tic, command.y1 - term->v_tic);
			wxt_update_key_box(command.x1 + term->h_tic, command.y1 + term->v_tic);
		}
		gp_cairo_draw_polygon(&plot, command.integer_value, command.corners);
		return;
	case command_move :
		if (wxt_in_key_sample)
			wxt_update_key_box(command.x1, command.y1);
		gp_cairo_move(&plot, command.x1, command.y1);
		return;
	case command_vector :
		if (wxt_in_key_sample) {
			wxt_update_key_box(command.x1, command.y1+term->v_tic);
			wxt_update_key_box(command.x1, command.y1-term->v_tic);
		}
		gp_cairo_vector(&plot, command.x1, command.y1);
		return;
	case command_linestyle :
		gp_cairo_set_linestyle(&plot, command.integer_value);
		return;
	case command_linetype :
		gp_cairo_set_linetype(&plot, command.integer_value);
		return;
	case command_pointsize :
		gp_cairo_set_pointsize(&plot, command.double_value);
		return;
	case command_dashtype :
		gp_cairo_set_dashtype(&plot, command.integer_value, command.dashpattern);
		return;
	case command_hypertext :
		current_href = command.string;
		return;
	case command_point :
		if (wxt_in_key_sample) {
			wxt_update_key_box(command.x1 - term->h_tic, command.y1 - term->v_tic);
			wxt_update_key_box(command.x1 + term->h_tic, command.y1 + term->v_tic);
		}
		gp_cairo_draw_point(&plot, command.x1, command.y1, command.integer_value);
		/*
		 * If we detect that we have just drawn a point with active hypertext,
		 * save the position and the text to draw after everything else.
		 */
		if (current_href && !wxt_in_key_sample) {
			int xnow = gnuplot_x(&plot, mouse_x);
			int ynow = term->ymax - gnuplot_y(&plot, mouse_y);
			int size = 3 * plot.pointsize * plot.oversampling_scale;
			if (abs(xnow - (int)command.x1) < size && abs(ynow - (int)command.y1) < size) {
				wxt_display_hypertext = current_href;
				wxt_display_anchor.x = command.x1;
				wxt_display_anchor.y = command.y1;
			}
			current_href = NULL;
		}
		return;
	case command_justify :
		gp_cairo_set_justify(&plot,command.mode);
		text_justification_mode = command.mode;
		return;
	case command_put_text :
		if (wxt_in_key_sample) {
			int slen = gp_strlen(command.string) * term->h_char * 0.75;
			if (text_justification_mode == RIGHT) slen = -slen;
			wxt_update_key_box(command.x1, command.y1);
			wxt_update_key_box(command.x1 + slen, command.y1 - term->v_tic);
		}
		gp_cairo_draw_text(&plot, command.x1, command.y1, command.string, NULL, NULL);
		return;
	case command_enhanced_init :
		if (wxt_in_key_sample) {
			int slen = command.integer_value * term->h_char * 0.75;
			if (text_justification_mode == RIGHT) slen = -slen;
			wxt_update_key_box(command.x1, command.y1);
			wxt_update_key_box(command.x1 + slen, command.y1 - term->v_tic);
		}
		gp_cairo_enhanced_init(&plot, command.integer_value);
		return;
	case command_enhanced_finish :
		gp_cairo_enhanced_finish(&plot, command.x1, command.y1);
		return;
	case command_enhanced_flush :
		gp_cairo_enhanced_flush(&plot);
		return;
	case command_enhanced_open :
		gp_cairo_enhanced_open(&plot, command.string, command.double_value,
				command.double_value2, command.integer_value2 & 1, (command.integer_value2 & 2) >> 1, command.integer_value);
		return;
	case command_enhanced_writec :
		gp_cairo_enhanced_writec(&plot, command.integer_value);
		return;
	case command_set_font :
		gp_cairo_set_font(&plot, command.string, command.integer_value);
		return;
	case command_linewidth :
		gp_cairo_set_linewidth(&plot, command.double_value);;
		return;
	case command_text_angle :
		gp_cairo_set_textangle(&plot, command.double_value);
		return;
	case command_fillbox :
		if (wxt_in_key_sample) {
			wxt_update_key_box(command.x1, command.y1);
			wxt_update_key_box(command.x1+command.x2, command.y1-command.y2);
		}
		gp_cairo_draw_fillbox(&plot, command.x1, command.y1,
					command.x2, command.y2,
					command.integer_value);
		return;
	case command_image :
		gp_cairo_draw_image(&plot, command.image,
				command.x1, command.y1,
				command.x2, command.y2,
				command.x3, command.y3,
				command.x4, command.y4,
				command.integer_value, command.integer_value2);
		return;
#ifdef EAM_BOXED_TEXT
	case command_boxed_text :
		gp_cairo_boxed_text(&plot, command.x1, command.y1, command.integer_value);
		return;
#endif /*EAM_BOXED_TEXT */
	case command_layer :
		switch (command.integer_value)
		{
		case TERM_LAYER_RESET:
		case TERM_LAYER_RESET_PLOTNO:
				wxt_cur_plotno = 0;
				break;
		case TERM_LAYER_BEFORE_PLOT:
				wxt_cur_plotno++;
				wxt_in_plot = TRUE;
				current_href = NULL;
				break;
		case TERM_LAYER_AFTER_PLOT:
				wxt_in_plot = FALSE;
				break;
		case TERM_LAYER_BEGIN_KEYSAMPLE:
				wxt_in_key_sample = TRUE;
				gp_cairo_boxed_text(&plot, -1, -1, TEXTBOX_INIT);
				break;
		case TERM_LAYER_END_KEYSAMPLE:
				wxt_in_key_sample = FALSE;
				break;
		default:
				break;
		}
		return;
	}
}

void wxtPanel::wxt_cairo_draw_hypertext()
{
	/* FIXME: Properly, we should save and restore the plot properties, */
	/* but since this box is the very last thing in the plot....        */
	rgb_color grey = {.9, .9, .9};
	int width = 0;
	int height = 0;

	plot.justify_mode = LEFT;
	gp_cairo_draw_text(&plot,
		wxt_display_anchor.x + term->h_char,
		wxt_display_anchor.y + term->v_char / 2,
		wxt_display_hypertext, &width, &height);

	gp_cairo_set_color(&plot, grey, 0.3);
	gp_cairo_draw_fillbox(&plot,
		wxt_display_anchor.x + term->h_char,
		wxt_display_anchor.y + height,
		width, height, FS_OPAQUE);

	gp_cairo_set_color(&plot, gp_cairo_linetype2color(-1), 0.0);
	gp_cairo_draw_text(&plot,
		wxt_display_anchor.x + term->h_char,
		wxt_display_anchor.y + term->v_char / 2,
		wxt_display_hypertext, NULL, NULL);
}

/* given a plot number (id), return the associated plot structure */
wxt_window_t* wxt_findwindowbyid(wxWindowID id)
{
	size_t i;
	for(i=0;i<wxt_window_list.size();++i) {
		if (wxt_window_list[i].id == id)
			return &(wxt_window_list[i]);
	}
	return NULL;
}

/*----------------------------------------------------------------------------
 *   raise-lower functions
 *----------------------------------------------------------------------------*/

void wxt_raise_window(wxt_window_t* window, bool force)
{
	FPRINTF((stderr,"raise window\n"));

	window->frame->Show(true);

	if (wxt_raise != no || force) {
#ifdef USE_GTK
		/* Raise() in wxGTK call wxTopLevelGTK::Raise()
		 * which also gives the focus to the window.
		 * Refresh() also must be called, otherwise
		 * the raise won't happen immediately */
		window->frame->panel->Refresh(false);
		gdk_window_raise(window->frame->GetHandle()->window);
#else
#ifdef __WXMSW__
		// Only restore the window if it is iconized.  In particular
		// leave it alone if it is maximized.
		if (window->frame->IsIconized())
#endif
		window->frame->Restore();
		window->frame->Raise();
#endif /*USE_GTK */
	}
}


void wxt_lower_window(wxt_window_t* window)
{
#ifdef USE_GTK
	window->frame->panel->Refresh(false);
	gdk_window_lower(window->frame->GetHandle()->window);
#else
	window->frame->Lower();
#endif /* USE_GTK */
}


/* raise the plot with given number */
void wxt_raise_terminal_window(int number)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();
	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : raise window %d\n", number));
		wxt_raise_window(window, true);
	}
	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* raise the plot the whole group */
void wxt_raise_terminal_group()
{
	/* declare the iterator */
	std::vector<wxt_window_t>::iterator wxt_iter;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();
	for(wxt_iter = wxt_window_list.begin(); wxt_iter != wxt_window_list.end(); wxt_iter++) {
		FPRINTF((stderr,"wxt : raise window %d\n", wxt_iter->id));
		/* FIXME Why does wxt_iter not work directly? */
		wxt_raise_window(&(*wxt_iter), true);
	}
	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* lower the plot with given number */
void wxt_lower_terminal_window(int number)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();
	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : lower window %d\n",number));
		wxt_lower_window(window);
	}
	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* lower the plot the whole group */
void wxt_lower_terminal_group()
{
	/* declare the iterator */
	std::vector<wxt_window_t>::iterator wxt_iter;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();
	for(wxt_iter = wxt_window_list.begin(); wxt_iter != wxt_window_list.end(); wxt_iter++) {
		FPRINTF((stderr,"wxt : lower window %d\n",wxt_iter->id));
		wxt_lower_window(&(*wxt_iter));
	}
	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* close the specified window */
void wxt_close_terminal_window(int number)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : close window %d\n",number));

		wxCloseEvent event(wxEVT_CLOSE_WINDOW, window->id);
		event.SetCanVeto(true);

		wxt_sigint_init();
		wxt_MutexGuiEnter();

		window->frame->SendEvent(event);

		wxt_MutexGuiLeave();
		wxt_sigint_check();
		wxt_sigint_restore();
	}
}


/* The following two routines allow us to update the cursor position
 * in the specified window even if the window is not active
 */

static double mouse_to_axis(int mouse_coord, wxt_axis_state_t *axis)
{
    double axis_coord;

	if (axis->term_scale == 0.0)
	    return 0;
	axis_coord = axis->min
	           + ((double)mouse_coord - axis->term_lower) / axis->term_scale;
	if (axis->logbase > 0)
		axis_coord = exp(axis_coord * axis->logbase);

    return axis_coord;
}

static void wxt_update_mousecoords_in_window(int number, int mx, int my)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	if ((window = wxt_findwindowbyid(number))) {

		/* TODO: rescale mx and my using stored per-plot scale info */
		char mouse_format[66];
		char *m = mouse_format;
		double x, y, x2, y2;

		if (window->axis_mask & (1<<0)) {
			x = mouse_to_axis(mx, &window->axis_state[0]);
			sprintf(m, "x=  %10g   %c", x, '\0');
			m += 17;
		}
		if (window->axis_mask & (1<<1)) {
			y = mouse_to_axis(my, &window->axis_state[1]);
			sprintf(m, "y=  %10g   %c", y, '\0');
			m += 17;
		}
		if (window->axis_mask & (1<<2)) {
			x2 = mouse_to_axis(mx, &window->axis_state[2]);
			sprintf(m, "x2=  %10g   %c", x2, '\0');
			m += 17;
		}
		if (window->axis_mask & (1<<3)) {
			y2 = mouse_to_axis(my, &window->axis_state[3]);
			sprintf(m, "y2=  %10g %c", y2, '\0');
			m += 15;
		}

		FPRINTF((stderr,"wxt : update mouse coords in window %d\n",number));
		window->frame->SetStatusText(wxString(mouse_format, wxConvLocal));
	}

}

/* update the window title */
void wxt_update_title(int number)
{
	wxt_window_t *window;
	wxString title;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();

	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : close window %d\n",number));
		if (strlen(wxt_title)) {
			/* NOTE : this assumes that the title is encoded in the locale charset.
				* This is probably a good assumption, but it is not guaranteed !
				* May be improved by using gnuplot encoding setting. */
			title << wxString(wxt_title, wxConvLocal);
		} else
			title.Printf(wxT("Gnuplot (window id : %d)"), window->id);

		window->frame->SetTitle(title);
	}

	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* update the size of the plot area, resizes the whole window consequently */
void wxt_update_size(int number)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();

	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : update size of window %d\n",number));
		window->frame->SetClientSize( wxSize(wxt_width, wxt_height) );
	}

	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}

/* update the position of the plot window */
void wxt_update_position(int number)
{
	wxt_window_t *window;

	if (wxt_status != STATUS_OK)
		return;

	wxt_sigint_init();

	wxt_MutexGuiEnter();

	if ((window = wxt_findwindowbyid(number))) {
		FPRINTF((stderr,"wxt : update position of window %d\n",number));
		window->frame->SetPosition( wxPoint(wxt_posx, wxt_posy) );
	}

	wxt_MutexGuiLeave();

	wxt_sigint_check();
	wxt_sigint_restore();
}


/* --------------------------------------------------------
 * Cairo stuff
 * --------------------------------------------------------*/

void wxtPanel::wxt_cairo_create_context()
{
	cairo_surface_t *fake_surface;

	if ( plot.cr )
		cairo_destroy(plot.cr);

	if ( wxt_cairo_create_platform_context() ) {
		/* we are not able to create a true cairo context,
		 * but will create a fake one to give proper initialisation */
		FPRINTF((stderr,"creating temporary fake surface\n"));
		fake_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 1, 1);
		plot.cr = cairo_create( fake_surface );
		cairo_surface_destroy( fake_surface );
		/* this flag will make the program retry later */
		plot.success = FALSE;
	} else {
		plot.success = TRUE;
	}

	/* set the transformation matrix of the context, and other details */
	gp_cairo_initialize_context(&plot);
}

void wxtPanel::wxt_cairo_free_context()
{
	if (plot.cr)
		cairo_destroy(plot.cr);

	if (plot.success)
		wxt_cairo_free_platform_context();
}


#ifdef GTK_SURFACE
/* create a cairo context, where the plot will be drawn on
 * If there is an error, return 1, otherwise return 0 */
int wxtPanel::wxt_cairo_create_platform_context()
{
	cairo_surface_t *surface;
	wxClientDC dc(this);

	FPRINTF((stderr,"wxt_cairo_create_context\n"));

	/* free gdkpixmap */
	wxt_cairo_free_platform_context();

	/* GetWindow is a wxGTK specific wxDC method that returns
	 * the GdkWindow on which painting should be done */

	if ( !GDK_IS_DRAWABLE(dc.GetWindow()) )
		return 1;

	gdkpixmap = gdk_pixmap_new(dc.GetWindow(), plot.device_xmax, plot.device_ymax, -1);

	if ( !GDK_IS_DRAWABLE(gdkpixmap) )
		return 1;

	plot.cr = gdk_cairo_create(gdkpixmap);
	return 0;
}

void wxtPanel::wxt_cairo_free_platform_context()
{
	if (gdkpixmap)
		g_object_unref(gdkpixmap);
}

#elif defined(__WXMSW__)
/* create a cairo context, where the plot will be drawn on
 * If there is an error, return 1, otherwise return 0 */
int wxtPanel::wxt_cairo_create_platform_context()
{
	cairo_surface_t *surface;
	wxClientDC dc(this);

	FPRINTF((stderr,"wxt_cairo_create_context\n"));

	/* free hdc and hbm */
	wxt_cairo_free_platform_context();

	/* GetHDC is a wxMSW specific wxDC method that returns
	 * the HDC on which painting should be done */

	/* Create a compatible DC. */
	hdc = CreateCompatibleDC( (HDC) dc.GetHDC() );

	if (!hdc)
		return 1;

	/* Create a bitmap big enough for our client rectangle. */
	hbm = CreateCompatibleBitmap((HDC) dc.GetHDC(), plot.device_xmax, plot.device_ymax);

	if ( !hbm )
		return 1;

	/* Select the bitmap into the off-screen DC. */
	SelectObject(hdc, hbm);
	surface = cairo_win32_surface_create( hdc );
	plot.cr = cairo_create(surface);
	cairo_surface_destroy( surface );
	return 0;
}

void wxtPanel::wxt_cairo_free_platform_context()
{
	if (hdc)
		DeleteDC(hdc);
	if (hbm)
		DeleteObject(hbm);
}

#else /* generic image surface */
/* create a cairo context, where the plot will be drawn on
 * If there is an error, return 1, otherwise return 0 */
int wxtPanel::wxt_cairo_create_platform_context()
{
	int width, height;
	cairo_surface_t *surface;

	FPRINTF((stderr,"wxt_cairo_create_context\n"));

	if (data32)
		delete[] data32;

	width = plot.device_xmax;
	height = plot.device_ymax;

	if (width<1||height<1)
		return 1;

	data32 = new unsigned int[width*height];

	surface = cairo_image_surface_create_for_data((unsigned char*) data32,
			CAIRO_FORMAT_ARGB32,  width, height, 4*width);
	plot.cr = cairo_create(surface);
	cairo_surface_destroy( surface );
	return 0;
}


/* create a wxBitmap (to be painted to the screen) from the buffered cairo surface. */
void wxtPanel::wxt_cairo_create_bitmap()
{
	int width, height;
	unsigned char *data24;
	wxImage *image;
	wxBitmap *old_bitmap = NULL;

	if (!data32)
		return;

	width = plot.device_xmax;
	height = plot.device_ymax;

	data24 = new unsigned char[3*width*height];

	/* data32 is the cairo image buffer, upper bits are alpha, then r, g and b
	 * Depends on endianess !
	 * It is converted to RGBRGB... in data24 */
	for (int i=0; i<width*height; ++i) {
		*(data24+3*i)=*(data32+i)>>16;
		*(data24+3*i+1)=*(data32+i)>>8;
		*(data24+3*i+2)=*(data32+i);
	}

	/* create a wxImage from data24 */
	image = new wxImage(width, height, data24, true);

	/* In wxWidgets 2.8 it was fine to delete the old bitmap right here */
	/* but in wxWidgets 3.0 this causes a use-after-free fault.	    */
	/* So now we delay deletion until after a new bitmap is assigned.   */
	old_bitmap = cairo_bitmap;

	/* create a wxBitmap from the wxImage. */
	cairo_bitmap = new wxBitmap( *image );

	/* free memory */
	delete image;
	delete[] data24;
	delete old_bitmap;
}


void wxtPanel::wxt_cairo_free_platform_context()
{
	if (data32) {
		delete[] data32;
		data32 = NULL;
	}
	if (cairo_bitmap) {
		delete cairo_bitmap;
		cairo_bitmap = NULL;
	}
}
#endif /* IMAGE_SURFACE */

/* --------------------------------------------------------
 * events handling
 * --------------------------------------------------------*/

/* Debugging events and _waitforinput adds a lot of lines of output
 * (~4 lines for an input character, and a few lines for each mouse move)
 * To debug it, define DEBUG and WXTDEBUGINPUT */

#ifdef WXTDEBUGINPUT
# define FPRINTF2(a) FPRINTF(a)
#else
# define FPRINTF2(a)
#endif

#ifdef USE_MOUSE
/* process one event, returns true if it ends the pause */
bool wxt_process_one_event(struct gp_event_t *event)
{
	FPRINTF2((stderr,"Processing event\n"));
	do_event( event );
	FPRINTF2((stderr,"Event processed\n"));
	if (event->type == GE_buttonrelease && (paused_for_mouse & PAUSE_CLICK)) {
		int button = event->par1;
		if (button == 1 && (paused_for_mouse & PAUSE_BUTTON1))
			paused_for_mouse = 0;
		if (button == 2 && (paused_for_mouse & PAUSE_BUTTON2))
			paused_for_mouse = 0;
		if (button == 3 && (paused_for_mouse & PAUSE_BUTTON3))
			paused_for_mouse = 0;
		if (paused_for_mouse == 0)
			return true;
	}
	if (event->type == GE_keypress && (paused_for_mouse & PAUSE_KEYSTROKE)) {
		/* Ignore NULL keycode */
		if (event->par1 > '\0') {
			paused_for_mouse = 0;
			return true;
		}
	}
	return false;
}

/* Similar to gp_exec_event(),
 * put the event sent by the terminal in a list,
 * to be processed by the main thread.
 * returns true if the event has really been processed.
 */
bool wxt_exec_event(int type, int mx, int my, int par1, int par2, wxWindowID id)
{
	struct gp_event_t event;

	/* Allow a distinction between keys attached to "bind" or "bind all" */
	if ( id != wxt_window_number ) {

	    if (type == GE_motion) {
		/* Update mouse coordinates locally and echo back to originating window */
		wxt_update_mousecoords_in_window(id, mx, my);
		return true;
	    }

	    if (type == GE_keypress)
		type = GE_keypress_old;
	    if (type == GE_buttonpress)
		type = GE_buttonpress_old;
	    if (type == GE_buttonrelease)
		type = GE_buttonrelease_old;
	    else	/* Other special cases? */
		return false;
	}

	event.type = type;
	event.mx = mx;
	event.my = my;
	event.par1 = par1;
	event.par2 = par2;
	event.winid = id;

#if defined(_Windows)
	wxt_process_one_event(&event);
	return true;
#elif defined(WXT_MONOTHREADED)
	if (wxt_process_one_event(&event))
	    ungetc('\n',stdin);
	return true;
#else
	if (!wxt_handling_persist)
	{
		if (wxt_sendevent_fd<0) {
			FPRINTF((stderr,"not sending event, wxt_sendevent_fd error\n"));
			return false;
		}

		if (write(wxt_sendevent_fd, (char*) &event, sizeof(event))<0) {
			wxt_sendevent_fd = -1;
			fprintf(stderr,"not sending event, write error on wxt_sendevent_fd\n");
			return false;
		}
	}
	else
	{
		wxt_process_one_event(&event);
	}

	return true;
#endif /* Windows or single-threaded or default */
}

#ifdef WXT_MULTITHREADED
/* Implements waitforinput used in wxt.trm
 * Returns the next input charachter, meanwhile treats terminal events */
int wxt_waitforinput(int options)
{
	/* wxt_waitforinput *is* launched immediately after the wxWidgets terminal
	 * is set using 'set term wxt' whereas wxt_init has not been called.
	 * So we must ensure that the library has been initialized
	 * before using any wxwidgets functions.
	 * When we just come back from SIGINT,
	 * we must process window events, so the check is not
	 * wxt_status != STATUS_OK */
	if (wxt_status == STATUS_UNINITIALIZED)
		return (options == TERM_ONLY_CHECK_MOUSING) ? '\0' : getc(stdin);

	if (wxt_event_fd<0) {
		if (paused_for_mouse)
			int_error(NO_CARET, "wxt communication error, wxt_event_fd<0");
		FPRINTF((stderr,"wxt communication error, wxt_event_fd<0\n"));
		return (options == TERM_ONLY_CHECK_MOUSING) ? '\0' : getc(stdin);
	}

	int stdin_fd = fileno(stdin);
	fd_set read_fds;
	struct timeval one_msec;

	do {
		struct timeval *timeout = NULL;
		FD_ZERO(&read_fds);
		FD_SET(wxt_event_fd, &read_fds);
		if (!paused_for_mouse)
			FD_SET(stdin_fd, &read_fds);

		/* When taking input from the console, we are willing to wait	*/
		/* here until the next character is typed. But if input is from	*/
		/* a script we just want to check for hotkeys or mouse input	*/
		/* and then leave again without waiting for stdin.		*/
		if (options == TERM_ONLY_CHECK_MOUSING) {
			timeout = &one_msec;
			one_msec.tv_sec = 0;
			one_msec.tv_usec = TERM_EVENT_POLL_TIMEOUT;
		}

		/* HBB FIXME 2015-05-03: why no test for autoconf HAVE_SELECT here ? */
		int n_changed_fds = select(wxt_event_fd+1, &read_fds,
					      NULL /* not watching for write-ready */,
					      NULL /* not watching for exceptions */,
					      timeout );

		if (n_changed_fds < 0) {
			if (paused_for_mouse)
				int_error(NO_CARET, "wxt communication error: select() error");
			FPRINTF((stderr, "wxt communication error: select() error\n"));
			break;
		}

		if (FD_ISSET(wxt_event_fd, &read_fds)) {
			/* terminal event coming */
			struct gp_event_t wxt_event;
			int n_bytes_read = read(wxt_event_fd, (void*) &wxt_event, sizeof(wxt_event));
			if (n_bytes_read < (int)sizeof(wxt_event)) {
				if (paused_for_mouse)
					int_error(NO_CARET, "wxt communication error, not enough bytes read");
				FPRINTF((stderr, "wxt communication error, not enough bytes read\n"));
				break;
			}
			if (wxt_process_one_event(&wxt_event)) {
				/* exit from paused_for_mouse */
				return '\0';
			}
		} else if (options == TERM_ONLY_CHECK_MOUSING) {
			return '\0';
		}
	} while ( paused_for_mouse || !FD_ISSET(stdin_fd, &read_fds) );

	if (options == TERM_ONLY_CHECK_MOUSING)
		return '\0';

	return getchar();
}
#else /* WXT_MONOTHREADED */
/* Implements waitforinput used in wxt.trm
 * the terminal events are directly processed when they are received */
int wxt_waitforinput(int options)
{
#ifdef _Windows
	if (options == TERM_ONLY_CHECK_MOUSING) {
		WinMessageLoop();
		return NUL;
	} else if (paused_for_mouse) {
		MSG msg;
		BOOL ret;

		/* wait for next event  */
		while ((ret = GetMessage(&msg, NULL, 0, 0)) != 0) {
			if (ret == -1)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (!paused_for_mouse)
				break;
		}
		return NUL;
	} else
		return getch();

#else /* !_Windows */
	/* Generic hybrid GUI & console message loop */
	/* (used mainly on MacOSX - still single threaded) */
	if (yield)
		return '\0';

	if (wxt_status == STATUS_UNINITIALIZED)
		return (options == TERM_ONLY_CHECK_MOUSING) ? '\0' : getc(stdin);

	if (options==TERM_ONLY_CHECK_MOUSING) {
		// If we're just checking mouse status, yield to the app for a while
		if (wxTheApp) {
			wxTheApp->Yield();
			yield = 0;
		}
		return '\0'; // gets dropped on floor
	}

	while (wxTheApp) {
	  // Loop with timeout of 10ms until stdin is ready to read,
	  // while also handling window events.
	  yield = 1;
	  wxTheApp->Yield();
	  yield = 0;

	  struct timeval tv;
	  fd_set read_fd;
	  tv.tv_sec = 0;
	  tv.tv_usec = 10000;
	  FD_ZERO(&read_fd);
	  FD_SET(0, &read_fd);
	  if (select(1, &read_fd, NULL, NULL, &tv) != -1 && FD_ISSET(0, &read_fd))
	    if (!paused_for_mouse)
		break;
	}
	return getchar();
#endif
}
#endif /* WXT_MONOTHREADED || WXT_MULTITHREADED */

#endif /*USE_MOUSE*/

/* --------------------------------------------------------
 * 'persist' option handling
 * --------------------------------------------------------*/

TBOOLEAN wxt_active_window_opened(void)
{
	return ((wxt_current_window != NULL) &&
	        (wxt_current_window->id == wxt_window_number) &&
	         wxt_current_window->frame->IsShown());
}


/* returns true if at least one plot window is opened.
 * Used to handle 'persist' */
TBOOLEAN wxt_window_opened(void)
{
	std::vector<wxt_window_t>::iterator wxt_iter; /*declare the iterator*/

	wxt_MutexGuiEnter();
	for (wxt_iter = wxt_window_list.begin(); wxt_iter != wxt_window_list.end(); wxt_iter++) {
		if (wxt_iter->frame->IsShown()) {
			wxt_MutexGuiLeave();
			return TRUE;
		}
	}
	wxt_MutexGuiLeave();
	return FALSE;
}


/* Called when gnuplot exits.
 * Handle the 'persist' setting, ie will continue
 * to handle events and will return when
 * all the plot windows are closed. */
void wxt_atexit()
{
	int i;
	int openwindows = 0;
	int persist_setting;

	if (wxt_status == STATUS_UNINITIALIZED)
		return;

#ifdef WXT_MULTITHREADED
	/* send a message to exit the main loop */
	/* protect the following from interrupt */
	wxt_sigint_init();

	wxCommandEvent event(wxExitLoopEvent);
	wxt_MutexGuiEnter();
	dynamic_cast<wxtApp*>(wxTheApp)->SendEvent( event );
	wxt_MutexGuiLeave();

	/* handle eventual interrupt, and restore original sigint handler */
	wxt_sigint_check();
	wxt_sigint_restore();

	FPRINTF((stderr,"gui thread status %d %d %d\n",
			thread->IsDetached(),
			thread->IsAlive(),
			thread->IsRunning() ));

	/* wait for the gui thread to exit */
	thread->Wait();
	delete thread;
#endif /* WXT_MULTITHREADED */

	/* first look for command_line setting */
	if (wxt_persist==UNSET && persist_cl)
		wxt_persist = TRUE;

	wxConfigBase *pConfig = wxConfigBase::Get();

	/* then look for persistent configuration setting */
	if (wxt_persist==UNSET) {
		if (pConfig->Read(wxT("persist"),&persist_setting))
			wxt_persist = persist_setting?yes:no;
	}

	/* and let's go ! */
	if (wxt_persist==UNSET|| wxt_persist==no) {
		FPRINTF((stderr,"wxt_atexit: no \"persist\" setting, exit\n"));
		wxt_cleanup();
		return;
	}

	/* if the user hits ctrl-c and quits again, really quit */
	wxt_persist = no;

	FPRINTF((stderr,"wxt_atexit: handling \"persist\" setting\n"));

#ifdef _Windows
	if (!persist_cl) {
		interactive = TRUE;
		while (!com_line());
	}
#else /*_Windows*/

	/* process events directly */
	wxt_handling_persist = true;

	/* if fork() is available, use it so that the initial gnuplot process
	 * exits and the child process continues in the background.
	 */
	/* NB:
	 * If there are no plot windows open, then once the parent process
	 * exits the child can receive no input and will become a zombie.
	 * So destroy any closed window first, and only fork if some remain open.
	 */
	/* declare the iterator */
	std::vector<wxt_window_t>::iterator wxt_iter;

	for(wxt_iter = wxt_window_list.begin(), i=0;
			wxt_iter != wxt_window_list.end(); wxt_iter++, i++)
	{
		TBOOLEAN state = wxt_iter->frame->IsShown();
		FPRINTF((stderr,"\tChecking window %d : %s shown\n", i, state?"":"not "));
		if (state) {
			openwindows++;
			/* Disable any toolbar widgets that would require parental help */
			wxt_iter->frame->toolbar->EnableTool(Toolbar_Replot, false);
			wxt_iter->frame->toolbar->EnableTool(Toolbar_ToggleGrid, false);
			wxt_iter->frame->toolbar->EnableTool(Toolbar_ZoomPrevious, false);
			wxt_iter->frame->toolbar->EnableTool(Toolbar_ZoomNext, false);
			wxt_iter->frame->toolbar->EnableTool(Toolbar_Autoscale, false);
		} else {
			wxt_iter->frame->Destroy();
		}
	}

# ifdef HAVE_WORKING_FORK
	/* fork */
	pid_t pid;

	if (openwindows > 0)
		pid = fork();
	else
		pid = -1;

	/* the parent just exits, the child keeps going */
	if (!pid) {
		FPRINTF((stderr,"child process: running\n"));
# endif /* HAVE_WORKING_FORK */

		FPRINTF((stderr,"child process: restarting its event loop\n"));
		/* Some programs executing gnuplot -persist may wait for all default
		 * handles to be closed before they consider the sub-process finished.
		 * Using freopen() ensures that debug fprintf()s won't crash. */
		freopen("/dev/null","w",stdout);
		freopen("/dev/null","w",stderr);

		/* (re)start gui loop */
		wxTheApp->OnRun();

		FPRINTF((stderr,"child process: event loop exited\n"));
# ifdef HAVE_WORKING_FORK
	}
	else
	{
		FPRINTF((stderr,"parent process: exiting, child process "\
				  "has PID %d\n", pid));
	}
# endif /* HAVE_WORKING_FORK */
#endif /* !_Windows */

	/* cleanup and quit */
	wxt_cleanup();
}


/* destroy everything related to wxWidgets */
void wxt_cleanup()
{
	std::vector<wxt_window_t>::iterator wxt_iter; /*declare the iterator*/

	if (wxt_status == STATUS_UNINITIALIZED)
		return;

	FPRINTF((stderr,"wxt_cleanup: start\n"));

	/* prevent wxt_reset (for example) from doing anything bad after that */
	wxt_status = STATUS_UNINITIALIZED;

	/* protect the following from interrupt */
	wxt_sigint_init();

	/* Close all open terminal windows */
	for(wxt_iter = wxt_window_list.begin();
			wxt_iter != wxt_window_list.end(); wxt_iter++)
		wxt_iter->frame->Destroy();

	wxTheApp->OnExit();
	wxUninitialize();

	/* handle eventual interrupt, and restore original sigint handler */
	wxt_sigint_check();
	wxt_sigint_restore();

	FPRINTF((stderr,"wxt_cleanup: finished\n"));
}

/* -------------------------------------
 * GUI Mutex helper functions for porting
 * ----------------------------------------*/

void wxt_MutexGuiEnter()
{
	FPRINTF2((stderr,"locking gui mutex\n"));
#ifdef WXT_MULTITHREADED
	if (!wxt_handling_persist)
		wxMutexGuiEnter();
#endif /* WXT_MULTITHREADED */
}

void wxt_MutexGuiLeave()
{
	FPRINTF2((stderr,"unlocking gui mutex\n"));
#ifdef WXT_MULTITHREADED
	if (!wxt_handling_persist)
		wxMutexGuiLeave();
#endif /* WXT_MULTITHREADED */
}

/* ---------------------------------------------------
 * SIGINT handling : as the terminal is multithreaded, it needs several mutexes.
 * To avoid inconsistencies and deadlock when the user hits ctrl-c,
 * each critical set of instructions (implying mutexes for example) should be written :
 *	wxt_sigint_init();
 *	< critical instructions >
 *	wxt_sigint_check();
 *	wxt_sigint_restore();
 * Or, if the critical instructions are in a loop, wxt_sigint_check() should be
 * called regularly in the loop.
 * ---------------------------------------------------*/


/* our custom SIGINT handler, that just sets a flag */
void wxt_sigint_handler(int WXUNUSED(sig))
{
	FPRINTF((stderr,"custom interrupt handler called\n"));
	signal(SIGINT, wxt_sigint_handler);

	/* If this happens, it's bad.  We already flagged that we want	*/
	/* to quit but nobody acted on it.  This can happen if the 	*/
	/* connection to the X-server is lost, which can be forced by	*/
	/* using xkill on the display window.  See bug #991.		*/
	if (wxt_status == STATUS_INTERRUPT_ON_NEXT_CHECK) {
		fprintf(stderr,"wxt display server shutting down - no response\n");
		exit(-1);
	}

	/* routines must check regularly for wxt_status,
	 * and abort cleanly on STATUS_INTERRUPT_ON_NEXT_CHECK */
	wxt_status = STATUS_INTERRUPT_ON_NEXT_CHECK;
	if (wxt_current_plot)
		wxt_current_plot->interrupt = TRUE;
}

/* To be called when the function has finished cleaning after facing STATUS_INTERRUPT_ON_NEXT_CHECK */
/* Provided for flexibility, but use wxt_sigint_check instead directly */
void wxt_sigint_return()
{
	FPRINTF((stderr,"calling original interrupt handler\n"));
	wxt_status = STATUS_INTERRUPT;
	wxt_sigint_counter = 0;
	/* call the original sigint handler */
	/* this will not return !! */
	(*original_siginthandler)(SIGINT);
}

/* A critical function should call this from a safe zone (no locked mutex, objects destroyed).
 * If the interrupt is asked, this fonction will not return (longjmp) */
void wxt_sigint_check()
{
	FPRINTF2((stderr,"checking interrupt status\n"));
	if (wxt_status == STATUS_INTERRUPT_ON_NEXT_CHECK)
		wxt_sigint_return();
}

/* initialize our custom SIGINT handler */
/* this uses a usage counter, so that it can be encapsulated without problem */
void wxt_sigint_init()
{
	/* put our custom sigint handler, store the original one */
	if (wxt_sigint_counter == 0)
		original_siginthandler = signal(SIGINT, wxt_sigint_handler);
	++wxt_sigint_counter;
	FPRINTF2((stderr,"initialize custom interrupt handler %d\n",wxt_sigint_counter));
}

/* restore the original SIGINT handler */
void wxt_sigint_restore()
{
	if (wxt_sigint_counter==1)
		signal(SIGINT, original_siginthandler);
	--wxt_sigint_counter;
	FPRINTF2((stderr,"restore custom interrupt handler %d\n",wxt_sigint_counter));
	if (wxt_sigint_counter<0)
		fprintf(stderr,"sigint counter < 0 : error !\n");
}
