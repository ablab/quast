#ifndef lint
static char *RCSid() { return RCSid("$Id: gplt_x11.c,v 1.246.2.3 2014/12/15 04:24:07 sfeam Exp $"); }
#endif

#define MOUSE_ALL_WINDOWS 1

/* GNUPLOT - gplt_x11.c */

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


/* lph changes:
 * (a) make EXPORT_SELECTION the default and specify NOEXPORT to undefine
 * (b) append X11 terminal number to resource name
 * (c) change cursor for active terminal
 */

/*-----------------------------------------------------------------------------
 *   gnuplot_x11 - X11 outboard terminal driver for gnuplot 3.3
 *
 *   Requires installation of companion inboard x11 driver in gnuplot/term.c
 *
 *   Acknowledgements:
 *      Chris Peterson (MIT)
 *      Dana Chee (Bellcore)
 *      Arthur Smith (Cornell)
 *      Hendri Hondorp (University of Twente, The Netherlands)
 *      Bill Kucharski (Solbourne)
 *      Charlie Kline (University of Illinois)
 *      Yehavi Bourvine (Hebrew University of Jerusalem, Israel)
 *      Russell Lang (Monash University, Australia)
 *      O'Reilly & Associates: X Window System - Volumes 1 & 2
 *
 *   This code is provided as is and with no warranties of any kind.
 *
 * drd: change to allow multiple windows to be maintained independently
 *
 *---------------------------------------------------------------------------*/

/* drd : export the graph via ICCCM primary selection. well... not quite
 * ICCCM since we dont support full list of targets, but this
 * is a start.  define EXPORT_SELECTION if you want this feature
 */

/*lph: add a "feature" to undefine EXPORT_SELECTION
   The following makes EXPORT_SELECTION the default and
   defining NOEXPORT over-rides the default
 */

/* Petr Mikulik and Johannes Zellner: added mouse support (October 1999)
 * Implementation and functionality is based on os2/gclient.c; see mousing.c
 * Pieter-Tjerk de Boer <ptdeboer@cs.utwente.nl>: merged two versions
 * of mouse patches. (November 1999) (See also mouse.[ch]).
 */

/* X11 support for Petr Mikulik's pm3d
 * by Johannes Zellner <johannes@zellner.org>
 * (November 1999 - January 2000, Oct. 2000)
 */

/* Polyline support May 2003
 * Ethan Merritt <merritt@u.washington.edu>
 */

/* Dynamically created windows July 2003
 * Across-pipe title text and close command October 2003
 * Dan Sebald <daniel.sebald@ieee.org>
 */

/* Daniel Sebald: added X11 support for images. (27 February 2003)
 */

/* Shigeharu Takeno <shige@iee.niit.ac.jp> February 2005
 * Support for multi-byte fonts based, with permission, on the "gnuplot+"
 * patches by Masahito Yamaga <ma@yama-ga.com>
 */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#ifdef USE_X11_MULTIBYTE
# include <X11/Xlocale.h>
#endif
#include <X11/XKBlib.h>	/* for XkbKeycodeToKeysym */

#include <assert.h>
#include "syscfg.h"
#include "stdfn.h"
#include "gp_types.h"
#include "term_api.h"
#include "gplt_x11.h"
#include "version.h"

#ifdef EXPORT_SELECTION
# undef EXPORT_SELECTION
#endif /* EXPORT SELECTION */
#ifndef NOEXPORT
# define EXPORT_SELECTION XA_PRIMARY
#endif /* NOEXPORT */


#if !(defined(VMS) || defined(CRIPPLED_SELECT))
# define DEFAULT_X11
#endif

#if defined(VMS) && defined(CRIPPLED_SELECT)
Error. Incompatible options.
#endif

#include <math.h>
#include "getcolor.h"

#ifdef USE_MOUSE
# include <X11/cursorfont.h>
#else
# define XC_crosshair 34
#endif

#include <signal.h>

#ifdef HAVE_SYS_BSDTYPES_H
# include <sys/bsdtypes.h>
#endif

#if defined(HAVE_SYS_SYSTEMINFO_H) && defined(HAVE_SYSINFO)
# include <sys/systeminfo.h>
# define SYSINFO_METHOD "sysinfo"
# define GP_SYSTEMINFO(host) sysinfo (SI_HOSTNAME, (host), MAXHOSTNAMELEN)
#else
# define SYSINFO_METHOD "gethostname"
# define GP_SYSTEMINFO(host) gethostname ((host), MAXHOSTNAMELEN)
#endif /* HAVE_SYS_SYSTEMINFO_H && HAVE_SYSINFO */

#ifdef USE_MOUSE
# ifdef OS2_IPC
#  define INCL_DOSPROCESS
#  define INCL_DOSSEMAPHORES
#  include <os2.h>
# endif
# include "gpexecute.h"
# include "mouse.h"
# include <unistd.h>
# include <fcntl.h>
# include <errno.h>

#ifdef MOUSE_ALL_WINDOWS
# include "axis.h" /* Just to pick up FIRST_X_AXIS enums */
typedef struct axis_scale_t {
    int term_lower;
    double term_scale;
    double min;
    double logbase;
} axis_scale_t;
#endif

#endif /* USE_MOUSE */

#ifdef __EMX__
/* for gethostname ... */
# include <netdb.h>
/* for __XOS2RedirRoot */
#include <X11/Xlibint.h>
#endif


#ifdef VMS
# ifdef __DECC
#  include <starlet.h>
# endif
# define EXIT(status) sys$delprc(0, 0)	/* VMS does not drop itself */
#else /* !VMS */
# ifdef PIPE_IPC
#  define EXIT(status)                         \
    do {                                       \
	gp_exec_event(GE_pending, 0, 0, 0, 0, 0); \
	close(1);                              \
	close(0);                              \
	exit(status);                          \
    } while (0)
# else
#  define EXIT(status) exit(status)
# endif	/* PIPE_IPC */
#endif /* !VMS */

#define Ncolors 13

typedef struct cmap_t {
    struct cmap_t *prev_cmap;  /* Linked list pointers and number */
    struct cmap_t *next_cmap;
    Colormap colormap;
    unsigned long colors[Ncolors];	/* line colors as pixel values */
    unsigned long rgbcolors[Ncolors];	/* line colors in rgb format */
    unsigned long xorpixel;	/* line colors */
    int total;
    int allocated;
    unsigned long *pixels;	/* pm3d colors */
} cmap_t;

/* always allocate a default colormap (done in preset()) */
static cmap_t default_cmap;

/* In order to get multiple palettes on a plot, i.e., multiplot mode,
   we must keep track of all color maps on a plot so that a needed
   color map is not discarded prematurely. */
typedef struct cmap_struct {
    cmap_t *cmap;
    struct cmap_struct *next_cmap_struct;
} cmap_struct;
    
/* Stuff for toggling plots on/off in response to a mouse click on the key entry */
typedef struct {
	unsigned int left;
	unsigned int right;
	unsigned int ytop;
	unsigned int ybot;
	TBOOLEAN hidden;
} x11BoundingBox;
static TBOOLEAN x11_in_key_sample = FALSE;
static TBOOLEAN x11_in_plot = FALSE;
static TBOOLEAN retain_toggle_state = FALSE;
static int x11_cur_plotno = 0;

/* information about one window/plot */
typedef struct plot_struct {
    Window window;
#ifdef EXTERNAL_X11_WINDOW
    Window external_container;
#endif
    Pixmap pixmap;
    unsigned int posn_flags;
    int x, y;
    unsigned int width, height;	/* window size */
    unsigned int gheight;	/* height of the part of the window that
				 * contains the graph (i.e., excluding the
				 * status line at the bottom if mouse is
				 * enabled) */
    unsigned int px, py;
    int ncommands, max_commands;
    char **commands;
    char *titlestring;
#ifdef USE_MOUSE
    int button;			/* buttons which are currently pressed */
    char str[0xff];		/* last displayed string */
#endif
    Time time;			/* time stamp of previous event */
    int lwidth;			/* this and the following 6 lines declare */
    int type;			/* variables used during drawing in exec_cmd() */
    int user_width;
    enum JUSTIFY jmode;
    double angle;		/* Text rotation angle in degrees */
    int lt;
#ifdef USE_MOUSE
    TBOOLEAN mouse_on;		/* is mouse bar on? */
    TBOOLEAN ruler_on;		/* is ruler on? */
    TBOOLEAN ruler_lineto_on;	/* is line between ruler and mouse cursor on? */
    int ruler_x, ruler_y;	/* coordinates of ruler */
    int ruler_lineto_x, ruler_lineto_y;	/* draw line from ruler to current mouse pos */
    TBOOLEAN zoombox_on;	/* is zoombox on? */
    int zoombox_x1, zoombox_y1, zoombox_x2, zoombox_y2;	/* coordinates of zoombox as last drawn */
    char zoombox_str1a[64], zoombox_str1b[64], zoombox_str2a[64], zoombox_str2b[64];	/* strings to be drawn at corners of zoombox ; 1/2 indicate corner; a/b indicate above/below */
    TBOOLEAN resizing;		/* TRUE while waiting for an acknowledgement of resize */

    x11BoundingBox *x11_key_boxes;	/* Track key entry coords for toggle clicks */
    int x11_max_key_boxes;		/* Size of above array */
#endif
    /* points to the cmap which is currently used for drawing.
     * This is always the default colormap, if not in pm3d.
     */
    cmap_t *cmap;
    cmap_struct *first_cmap_struct;
#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
    /* This array holds per-axis scaling information sufficient to reconstruct
     * plot coordinates of a mouse click.  It is a snapshot of the contents of
     * gnuplot's axis_array structure at the time the plot was drawn.
     */
    int almost2d;
    int axis_mask;		/* Bits set to show which axes are active */
    axis_scale_t axis_scale[2*SECOND_AXES];
#endif
    /* Last text position  - used by enhanced text mode */
    int xLast, yLast;
    /* Saved text position  - used by enhanced text mode */
    int xSave, ySave;
    /* Last rgb color that was set */
    unsigned long current_rgb;

    struct plot_struct *prev_plot;  /* Linked list pointers and number */
    struct plot_struct *next_plot;
    int plot_number;
} plot_struct;

static plot_struct *Add_Plot_To_Linked_List __PROTO((int));
static void Remove_Plot_From_Linked_List __PROTO((Window));
static plot_struct *Find_Plot_In_Linked_List_By_Number __PROTO((int));
static plot_struct *Find_Plot_In_Linked_List_By_Window __PROTO((Window));
static plot_struct *Find_Plot_In_Linked_List_By_CMap __PROTO((cmap_t *));

static struct plot_struct *current_plot = NULL;
static int most_recent_plot_number = 0;
static struct plot_struct *plot_list_start = NULL;

static void x11_setfill __PROTO((GC *gc, int style));

/* information about window/plot to be removed */
typedef struct plot_remove_struct {
    Window plot_window_to_remove;
    struct plot_remove_struct *next_remove;
    int processed;
} plot_remove_struct;

static void Add_Plot_To_Remove_FIFO_Queue __PROTO((Window));
static void Process_Remove_FIFO_Queue __PROTO((void));

static struct plot_remove_struct *remove_fifo_queue_start = NULL;
static int process_remove_fifo_queue = 0;

static cmap_t *Add_CMap_To_Linked_List __PROTO((void));
static void Remove_CMap_From_Linked_List __PROTO((cmap_t *));
static cmap_t *Find_CMap_In_Linked_List __PROTO((cmap_t *));
static int cmaps_differ __PROTO((cmap_t *, cmap_t *));

static void clear_used_font_list __PROTO((void));

/* current_cmap always points to a valid colormap.  At start up
 * it is the default colormap.  When a palette command comes
 * across the pipe, the current_cmap is set to point at the
 * resulting colormap which ends up in the linked list of colormaps.
 * The current_cmap should never be removed from the linked list
 * even if all windows are deleted, because that colormap will be
 * used for the next plot.
 */
static struct cmap_t *current_cmap = NULL;
static struct cmap_t *cmap_list_start = NULL;

/* These might work better as fuctions, but defines will do for now. */
#define ERROR_NOTICE(str)         "\nGNUPLOT (gplt_x11):  " str
#define ERROR_NOTICE_NEWLINE(str) "\n                     " str

enum { NOT_AVAILABLE = -1 };

#define SEL_LEN 0xff
static char selection[SEL_LEN] = "";


#ifdef USE_MOUSE
# define GRAPH_HEIGHT(plot)  ((plot)->gheight)
# define PIXMAP_HEIGHT(plot)  ((plot)->gheight + vchar)
  /* note: PIXMAP_HEIGHT is the height of the plot including the status line,
     even if the latter is not enabled right now */
#else
# define GRAPH_HEIGHT(plot)  ((plot)->height)
# define PIXMAP_HEIGHT(plot)  ((plot)->height)
#endif

static void CmapClear __PROTO((cmap_t *));
static void RecolorWindow __PROTO((plot_struct *));
static void FreeColormapList __PROTO((plot_struct *plot));
static void FreeColors __PROTO((cmap_t *));
static void ReleaseColormap __PROTO((cmap_t *));
static unsigned long *ReallocColors __PROTO((cmap_t *, int));
static void PaletteMake __PROTO((t_sm_palette *));
static void PaletteSetColor __PROTO((plot_struct *, double));
static int GetVisual __PROTO((int, Visual **, int *));
static void scan_palette_from_buf __PROTO((void));

static unsigned short BitMaskDetails __PROTO((unsigned long mask, unsigned short *left_shift, unsigned short *right_shift));

TBOOLEAN swap_endian = 0;  /* For binary data. */
/* Petr's byte swapping routine. */
static inline void
byteswap(char* data, int datalen)
{
    char tmp, *dest = data + datalen - 1;
    if (datalen < 2) return;
    while (dest > data) {
	tmp = *dest;
	*dest-- = *data;
	*data++ = tmp;
    }
}
/* Additional macros that should be more efficient when data size is known. */
char byteswap_char;
#define byteswap1(x)
#define byteswap2(x) \
    byteswap_char = ((char *)x)[0]; \
    ((char *)x)[0] = ((char *)x)[1]; \
    ((char *)x)[1] = byteswap_char;
#define byteswap4(x) \
    byteswap_char = ((char *)x)[0]; \
    ((char *)x)[0] = ((char *)x)[3]; \
    ((char *)x)[3] = byteswap_char; \
    byteswap_char = ((char *)x)[1]; \
    ((char *)x)[1] = ((char *)x)[2]; \
    ((char *)x)[2] = byteswap_char

static void store_command __PROTO((char *, plot_struct *));
static void prepare_plot __PROTO((plot_struct *));
static void delete_plot __PROTO((plot_struct *));

static int record __PROTO((void));
static void process_event __PROTO((XEvent *));	/* from Xserver */
static void process_configure_notify_event __PROTO((XEvent *event, TBOOLEAN isRetry ));

static void mainloop __PROTO((void));

static void display __PROTO((plot_struct *));
static void UpdateWindow __PROTO((plot_struct *));
#ifdef USE_MOUSE
static void gp_execute_GE_plotdone __PROTO((int windowid));

static int ErrorHandler __PROTO((Display *, XErrorEvent *));
static void DrawRuler __PROTO((plot_struct *));
static void EventuallyDrawMouseAddOns __PROTO((plot_struct *));
static void DrawBox __PROTO((plot_struct *));
static void DrawLineToRuler __PROTO((plot_struct *));
static void AnnotatePoint __PROTO((plot_struct *, int, int, const char[], const char[]));
static long int SetTime __PROTO((plot_struct *, Time));
static unsigned long AllocateXorPixel __PROTO((cmap_t *));
static void GetGCXor __PROTO((plot_struct *, GC *));
static void GetGCXorDashed __PROTO((plot_struct *, GC *));
static void EraseCoords __PROTO((plot_struct *));
static void DrawCoords __PROTO((plot_struct *, const char *));
static void DisplayCoords __PROTO((plot_struct *, const char *));

static TBOOLEAN is_meta __PROTO((KeySym));

#ifndef DISABLE_SPACE_RAISES_CONSOLE
static unsigned long gnuplotXID = 0; /* WINDOWID of gnuplot */
static char* getMultiTabConsoleSwitchCommand __PROTO((unsigned long *));
#endif /* DISABLE_SPACE_RAISES_CONSOLE */

static void x11_initialize_key_boxes __PROTO((plot_struct *plot, int i));
static void x11_initialize_hidden __PROTO((plot_struct *plot, int i));
static void x11_update_key_box __PROTO((plot_struct *plot,  unsigned int x, unsigned int y ));
static int x11_check_for_toggle __PROTO((plot_struct *plot, unsigned int x, unsigned int y));

#endif /* USE_MOUSE */

static void DrawRotated __PROTO((plot_struct *, Display *, GC,
				 int, int, const char *, int));
static int DrawRotatedErrorHandler __PROTO((Display *, XErrorEvent *));
static void exec_cmd __PROTO((plot_struct *, char *));

static void reset_cursor __PROTO((void));

static void preset __PROTO((int, char **));
static char *pr_GetR __PROTO((XrmDatabase, char *));
static void pr_color __PROTO((cmap_t *));
static void pr_dashes __PROTO((void));
static void pr_encoding __PROTO((void));
static void pr_font __PROTO((char *));
static void pr_geometry __PROTO((char *));
static void pr_pointsize __PROTO((void));
static void pr_width __PROTO((void));
static void pr_window __PROTO((plot_struct *));
static void pr_raise __PROTO((void));
static void pr_replotonresize __PROTO((void));
static void pr_persist __PROTO((void));
static void pr_feedback __PROTO((void));
static void pr_ctrlq __PROTO((void));
static void pr_fastrotate __PROTO((void));

#ifdef EXPORT_SELECTION
static void export_graph __PROTO((plot_struct *));
static void handle_selection_event __PROTO((XEvent *));
static void pr_exportselection __PROTO((void));
#endif

#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
static void mouse_to_coords __PROTO((plot_struct *, XEvent *,
			double *, double *, double *, double *));
static double mouse_to_axis __PROTO((int, axis_scale_t *));
#endif

static char *FallbackFont = "fixed";
#ifdef USE_X11_MULTIBYTE
static char *FallbackFontMB = "mbfont:*-medium-r-normal--14-*,*-medium-r-normal--16-*";
static char *FallbackFontMBUTF = "mbfont:*-medium-r-normal--14-*-iso10646-1";
# define FontSetSep ';'
static int usemultibyte = 0;
static int multibyte_fonts_usable=1;
static int fontset_transsep __PROTO((char *, char *, int));
#endif /* USE_X11_MULTIBYTE */
static int gpXTextWidth __PROTO((XFontStruct *, const char *, int));
static int gpXTextHeight __PROTO((XFontStruct *));
static void gpXSetFont __PROTO((Display *, GC, Font));
static void gpXDrawImageString __PROTO((Display *, Drawable, GC, int, int, const char *, int));
static void gpXDrawString __PROTO((Display *, Drawable, GC, int, int, const char *, int));
static void gpXFreeFont __PROTO((Display *, XFontStruct *));
static XFontStruct *gpXLoadQueryFont __PROTO((Display *, char *));
static char *gpFallbackFont __PROTO((void));
static int gpXGetFontascent __PROTO((XFontStruct *cfont));

enum set_encoding_id encoding = S_ENC_DEFAULT; /* EAM - mirrored from core code by 'QE' */
static char default_font[196] = { '\0' };
static char default_encoding[16] = { '\0' };

#define Nwidths 10
static unsigned int widths[Nwidths] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

#define Ndashes 10
static char dashes[Ndashes][DASHPATTERN_LENGTH+1];

t_sm_palette sm_palette = {
    -1,				/* colorFormulae */
    SMPAL_COLOR_MODE_NONE,	/* colorMode */
    0, 0, 0,			/* formula[RGB] */
    0,				/* positive */
    0,				/* use_maxcolors */
    -1,				/* colors */
    (rgb_color *) 0,            /* color */
    0,				/* ps_allcF */
    0,                          /* gradient_num */
    (gradient_struct *) 0       /* gradient */
    /* Afunc, Bfunc and Cfunc can't be initialised here */
};

static int have_pm3d = 1;
static int num_colormaps = 0;
static unsigned int maximal_possible_colors = 0x100;
static unsigned int minimal_possible_colors;

/* the following visual names must match the
 * definitions in X.h in this order ! I hope
 * this is standard (joze) */
static char *visual_name[] = {
    "StaticGray",
    "GrayScale",
    "StaticColor",
    "PseudoColor",
    "TrueColor",
    "DirectColor",
    (char *) 0
};

static Display *dpy;
static int scr;
static Window root;
static Visual *vis = (Visual *) 0;
static GC gc = (GC) 0;
static GC *current_gc = (GC *) 0;
static GC gc_xor = (GC) 0;
static GC gc_xor_dashed = (GC) 0;
static GC fill_gc = (GC) 0;
static XFontStruct *font = NULL;
#ifdef USE_X11_MULTIBYTE
static XFontSet mbfont = NULL;
#endif
static int do_raise = yes, persist = no;
static TBOOLEAN fast_rotate = TRUE;
static int feedback = yes;
static int ctrlq = no;
static int replot_on_resize = yes;
static int dashedlines = yes;
#ifdef EXPORT_SELECTION
static TBOOLEAN exportselection = TRUE;
#endif
static Cursor cursor;
static Cursor cursor_default;
#ifdef USE_MOUSE
static Cursor cursor_exchange;
static Cursor cursor_sizing;
static Cursor cursor_zooming;
#ifndef TITLE_BAR_DRAWING_MSG
static Cursor cursor_waiting;
static Cursor cursor_save;
static int button_pressed = 0;
#endif
#endif

static int windows_open = 0;

static int gX = 100, gY = 100;

/* gW and gH are the sizes of the plot, when it was first made. If the window is
   resized after the plot is made (and we're not replotting on resize), the
   plot->width and plot->height track the window size, but gW and gH do NOT.
   This allows the plot to be maximally scaled while preserving the aspect
   ratio.

   gW, gH and plot->width, plot->gheight are just the plot; they do NOT include
   the modeline at the bottom of the window. plot->height DOES include the
   modeline */
static unsigned int gW = 640, gH = 450; /* defaults must match those in x11.trm */
static unsigned int gFlags = PSize;

static unsigned int BorderWidth = 2;
static unsigned int dep;		/* depth */
static long max_request_size;

static Bool Mono = 0, Gray = 0, Rv = 0, Clear = 0;
static char X_Name[64] = "gnuplot";
static char X_Class[64] = "Gnuplot";

static int cx = 0, cy = 0;

/* Font characteristics held locally but sent back via pipe to x11.trm */
static int vchar, hchar;

/* Will hold the bounding box of the previous enhanced text string */
#ifdef EAM_BOXED_TEXT
    unsigned int bounding_box[4];
    TBOOLEAN boxing = FALSE;
#define X11_TEXTBOX_MARGIN 2
    int box_xmargin = X11_TEXTBOX_MARGIN;
    int box_ymargin = X11_TEXTBOX_MARGIN;
#endif

/* Specify negative values as indicator of uninitialized state */
static double xscale = -1.;
static double yscale = -1.;
static int    ymax   = 4096;

double pointsize = -1.;
/* Avoid a crash upon using uninitialized variables from
   above and avoid unnecessary calls to display().
   Probably this is not the best fix ... */
#define Call_display(plot) if (xscale<0.) display(plot);
#define X(x) (int) ((x) * xscale)
#define Y(y) (int) (( (  ymax -1)-(y)) * yscale)
#define RevX(x) (((x)+0.5)/xscale)
#define RevY(y) (ymax-1 -((y)+0.5)/yscale)
/* note: the 0.5 term in RevX(x) and RevY(y) compensates for the round-off in X(x) and Y(y) */

static char buf[X11_COMMAND_BUFFER_LENGTH];
static int buffered_input_available = 0;

static FILE *X11_ipc;

/* when using an ICCCM-compliant window manager, we can ask it
 * to send us an event when user chooses 'close window'. We do this
 * by setting WM_DELETE_WINDOW atom in property WM_PROTOCOLS
 */

static Atom WM_PROTOCOLS, WM_DELETE_WINDOW;

static XPoint Diamond[5], Triangle[4];
static XSegment Plus[2], Cross[2], Star[4];

/* pixmaps used for filled boxes (ULIG) */
/* FIXME EAM - These data structures are a duplicate of the ones in bitmap.c */

/* pattern stipples for pattern fillstyle */
#define stipple_pattern_width 8
#define stipple_pattern_height 8
#define stipple_pattern_num 8
static const char stipple_pattern_bits[stipple_pattern_num][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } /* no fill */
   , { 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x41 } /* cross-hatch      (1) */
   , { 0x88, 0x55, 0x22, 0x55, 0x88, 0x55, 0x22, 0x55 } /* double crosshatch(2) */
   , { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } /* solid fill       (3) */
   , { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 } /* diagonal stripes (4) */
   , { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 } /* diagonal stripes (5) */
   , { 0x11, 0x11, 0x22, 0x22, 0x44, 0x44, 0x88, 0x88 } /* diagonal stripes (6) */
   , { 0x88, 0x88, 0x44, 0x44, 0x22, 0x22, 0x11, 0x11 } /* diagonal stripes (7) */
};
static const char stipple_pattern_dots[8] =
    { 0x80, 0x08, 0x20, 0x02, 0x40, 0x04, 0x10, 0x01 };

static Pixmap stipple_pattern[stipple_pattern_num];
static Pixmap stipple_dots;
static int stipple_initialized = 0;

static XPoint *polyline = NULL;
static int polyline_space = 0;
static int polyline_size = 0;

/* whether we're in the middle of receiving image chunks. If so, don't try to
   display() */
static TBOOLEAN currently_receiving_gr_image = FALSE;

static void gpXStoreName __PROTO((Display *, Window, char *));

/*
 * Main program
 */
int
main(int argc, char *argv[])
{

#ifdef PIPE_IPC
    int getfl;
#endif

#ifdef __EMX__
    /* close open file handles */
    fcloseall();
#endif

#ifdef USE_X11_MULTIBYTE
    if (setlocale(LC_ALL, "")==NULL || XSupportsLocale()==False)
      multibyte_fonts_usable=0;
    setlocale(LC_NUMERIC, "C");	/* HBB 20050525 */
#endif /* USE_X11_MULTIBYTE */
    preset(argc, argv);

/* set up the alternative cursor */
    cursor_default = XCreateFontCursor(dpy, XC_crosshair);
    cursor = cursor_default;
#ifdef USE_MOUSE
    /* create cursors for the splot actions */
    cursor_exchange = XCreateFontCursor(dpy, XC_exchange);
    cursor_sizing = XCreateFontCursor(dpy, XC_sizing);
    /* arrow, top_left_arrow, left_ptr, sb_left_arrow, sb_right_arrow,
     * plus, pencil, draft_large, right_ptr, draft_small */
    cursor_zooming = XCreateFontCursor(dpy, XC_draft_small);
#ifndef TITLE_BAR_DRAWING_MSG
    cursor_waiting = XCreateFontCursor(dpy, XC_watch);
    cursor_save = (Cursor)0;
#endif
#endif
#ifdef PIPE_IPC
    if (!pipe_died) {
	/* set up nonblocking stdout */
	getfl = fcntl(1, F_GETFL);	/* get current flags */
	fcntl(1, F_SETFL, getfl | O_NONBLOCK);
	signal(SIGPIPE, pipe_died_handler);
    }
# endif

    polyline_space = 100;
    polyline = calloc(polyline_space, sizeof(XPoint));
    if (!polyline) fprintf(stderr, "Panic: cannot allocate polyline\n");

    mainloop();

    if (persist) {
	FPRINTF((stderr, "waiting for %d windows\n", windows_open));

#ifndef DEBUG
	/* HBB 20030519: Some programs executing gnuplot -persist may
	 * be waiting for all default handles to be closed before they
	 * consider the sub-process finished.  Emacs, e.g., does.  So,
	 * unless this is a DEBUG build, drop our connection to stderr
	 * now.  Using Freopen() ensures that debug fprintf()s won't
	 * crash. */
	freopen("/dev/null", "w", stderr);
#endif

	/* read x events until all windows have been quit */
	while (windows_open > 0) {
	    XEvent event;
	    XNextEvent(dpy, &event);
	    process_event(&event);
	}
    }
    XCloseDisplay(dpy);

    FPRINTF((stderr, "exiting\n"));

    EXIT(0);
}

/*-----------------------------------------------------------------------------
 *   mainloop processing - process X events and input from gnuplot
 *
 *   Three different versions of main loop processing are provided to support
 *   three different platforms.
 *
 *   DEFAULT_X11:     use select() for both X events and input on stdin
 *                    from gnuplot inboard driver
 *
 *   CRIPPLED_SELECT: use select() to service X events and check during
 *                    select timeout for temporary plot file created
 *                    by inboard driver
 *
 *   VMS:             use XNextEvent to service X events and AST to
 *                    service input from gnuplot inboard driver on stdin
 *---------------------------------------------------------------------------*/


#ifdef DEFAULT_X11

/*
 * DEFAULT_X11 mainloop
 */
static void
mainloop()
{
    int nf, cn = ConnectionNumber(dpy), in;
    SELECT_TYPE_ARG1 nfds;
    struct timeval timeout, *timer = (struct timeval *) 0;
    fd_set tset;

#ifdef PIPE_IPC
    int usleep_count = 0;
    int out;
    out = fileno(stdout);
#endif

    X11_ipc = stdin;
    in = fileno(X11_ipc);

#ifdef PIPE_IPC
    if (out > in)
	nfds = ((cn > out) ? cn : out) + 1;
    else
#endif
	nfds = ((cn > in) ? cn : in) + 1;

#ifdef ISC22
/* Added by Robert Eckardt, RobertE@beta.TP2.Ruhr-Uni-Bochum.de */
    timeout.tv_sec = 0;		/* select() in ISC2.2 needs timeout */
    timeout.tv_usec = 300000;	/* otherwise input from gnuplot is */
    timer = &timeout;		/* suspended til next X event. */
#endif /* ISC22   (0.3s are short enough not to be noticed */

    while (1) {

	/* XNextEvent does an XFlush() before waiting. But here.
	 * we must ensure that the queue is flushed, since we
	 * dont call XNextEvent until an event arrives. (I have
	 * twice wasted quite some time over this issue, so now
	 * I am making sure of it !
	 */

	XFlush(dpy);

	FD_ZERO(&tset);
	FD_SET(cn, &tset);

	/* Don't wait for events if we know that input is
	 * already sitting in a buffer.  Also don't wait for
	 * input to become available.
	 */
	if (buffered_input_available) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	    timer = &timeout;
	} else {
	    timer = (struct timeval *) 0;
	    FD_SET(in, &tset);
	}

#ifdef PIPE_IPC
	if (buffered_output_pending && !pipe_died) {
	    /* check, if stdout becomes writable */
	    FD_SET(out, &tset);
	}
#ifdef HAVE_USLEEP
	/* Make sure this loop does not monopolize CPU if the pipe is jammed */
	if (++usleep_count > 10) {
	    usleep(100);
	    usleep_count = 0;
	}
#endif
#endif

	nf = select(nfds, SELECT_TYPE_ARG234 &tset, 0, 0, SELECT_TYPE_ARG5 timer);

	if (nf < 0) {
	    if (errno == EINTR)
		continue;
	    perror("gnuplot_x11: select failed");
	    EXIT(1);
	}

	if (nf > 0)
	    XNoOp(dpy);

	if (XPending(dpy)) {
	    /* used to use CheckMaskEvent() but that cannot receive
	     * maskable events such as ClientMessage. So now we do
	     * one event, then return to the select.
	     * And that almost works, except that under some Xservers
	     * running without a window manager (e.g. Hummingbird Exceed under Win95)
	     * a bogus ConfigureNotify is sent followed by a valid ConfigureNotify
	     * when the window is maximized.  The two events are queued, apparently
	     * in a single I/O because select() above doesn't see the second, valid
	     * event.  This little loop fixes the problem by flushing the
	     * event queue completely.
	     */
	    XEvent xe;
	    do {
		XNextEvent(dpy, &xe);
		process_event(&xe);
	    } while (XPending(dpy));
	}

	if (FD_ISSET(in, &tset) || buffered_input_available) {
	    if (!record())	/* end of input */
		return;
	}
#ifdef PIPE_IPC
	if (!pipe_died && (FD_ISSET(out, &tset) || buffered_output_pending)) {
	    gp_exec_event(GE_pending, 0, 0, 0, 0, 0);
	}
#endif
	/* A method in which the ErrorHandler can queue plots to be
	   removed from the linked list.  This prevents the situation
	   that could arise if the ErrorHandler directly removed
	   plots and some other part of the program were utilizing
	   a pointer to a plot that was destroyed. */
	if (process_remove_fifo_queue) {
	    Process_Remove_FIFO_Queue();
	}
    }
}

#elif defined(CRIPPLED_SELECT)

char X11_ipcpath[32];

/*
 * CRIPPLED_SELECT mainloop
 */
static void
mainloop()
{
    SELECT_TYPE_ARG1 nf, nfds, cn = ConnectionNumber(dpy);
    struct timeval timeout, *timer;
    fd_set tset;
    unsigned long all = (unsigned long) (-1L);
    XEvent xe;

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    timer = &timeout;
    sprintf(X11_ipcpath, "/tmp/Gnuplot_%d", getppid());
    nfds = cn + 1;

    while (1) {
	XFlush(dpy);		/* see above */

	FD_ZERO(&tset);
	FD_SET(cn, &tset);

	/* Don't wait for events if we know that input is
	 * already sitting in a buffer.  Also don't wait for
	 * input to become available.
	 */
	if (buffered_input_available) {
	    timeout.tv_sec = 0;
	    timeout.tv_usec = 0;
	    timer = &timeout;
	} else {
	    timer = (struct timeval *) 0;
	    FD_SET(in, &tset);
	}

	nfds = (cn > in) ? cn + 1 : in + 1;

	nf = select(nfds, SELECT_TYPE_ARG234 &tset, 0, 0, SELECT_TYPE_ARG5 timer);

	if (nf < 0) {
	    if (errno == EINTR)
		continue;
	    perror("gnuplot_x11: select failed");
	    EXIT(1);
	}

	if (nf > 0)
	    XNoOp(dpy);

	if (FD_ISSET(cn, &tset)) {
	    while (XCheckMaskEvent(dpy, all, &xe)) {
		process_event(&xe);
	    }
	}
	if ((X11_ipc = fopen(X11_ipcpath, "r"))) {
	    unlink(X11_ipcpath);
	    record();
	    fclose(X11_ipc);
	}
    }
}


#elif defined(VMS)
/*-----------------------------------------------------------------------------
 *    VMS mainloop - Yehavi Bourvine - YEHAVI@VMS.HUJI.AC.IL
 *---------------------------------------------------------------------------*/

/*  In VMS there is no decent Select(). hence, we have to loop inside
 *  XGetNextEvent for getting the next X window event. In order to get input
 *  from the master we assign a channel to SYS$INPUT and use AST's in order to
 *  receive data. In order to exit the mainloop, we need to somehow make
 *  XNextEvent return from within the ast. We do this with a XSendEvent() to
 *  ourselves !
 *  This needs a window to send the message to, so we create an unmapped window
 *  for this purpose. Event type XClientMessage is perfect for this, but it
 *  appears that such messages come from elsewhere (motif window manager,
 *  perhaps ?) So we need to check fairly carefully that it is the ast event
 *  that has been received.
 */

#include <iodef.h>
char STDIIN[] = "SYS$INPUT:";
short STDIINchannel, STDIINiosb[4];
struct {
    short size, type;
    char *address;
} STDIINdesc;
char STDIINbuffer[64];
int status;

ast()
{
    int status = sys$qio(0, STDIINchannel, IO$_READVBLK, STDIINiosb, record,
			 0, STDIINbuffer, sizeof(STDIINbuffer) - 1, 0, 0, 0, 0);
    if ((status & 0x1) == 0)
	EXIT(status);
}

Window message_window;

static void
mainloop()
{
    /* dummy unmapped window for receiving internally-generated terminate
     * messages
     */
    message_window = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 1, 0, 0);

    STDIINdesc.size = strlen(STDIIN);
    STDIINdesc.type = 0;
    STDIINdesc.address = STDIIN;
    status = sys$assign(&STDIINdesc, &STDIINchannel, 0, 0, 0);
    if ((status & 0x1) == 0)
	EXIT(status);
    ast();

    for (;;) {
	XEvent xe;
	XNextEvent(dpy, &xe);
	if (xe.type == ClientMessage && xe.xclient.window == message_window) {
	    if (xe.xclient.message_type == None && xe.xclient.format == 8 && strcmp(xe.xclient.data.b, "die gnuplot die") == 0) {
		FPRINTF((stderr, "quit message from ast\n"));
		return;
	    } else {
		FPRINTF((stderr, "Bogus XClientMessage event from window manager ?\n"));
	    }
	}
	process_event(&xe);
    }
}
#else /* !(DEFAULT_X11 || CRIPPLED_SELECT || VMS */
# error You lose. No mainloop.
#endif				/* !(DEFAULT_X11 || CRIPPLED_SELECT || VMS */

/* delete a window / plot */
static void
delete_plot(plot_struct *plot)
{
    int i;

    FPRINTF((stderr, "Delete plot %d\n", plot->plot_number));

    for (i = 0; i < plot->ncommands; ++i)
	free(plot->commands[i]);
    plot->ncommands = 0;
    if (plot->commands)
	free(plot->commands);
    plot->commands = NULL;
    plot->max_commands = 0;

    /* Free structure used to track key entries for mouse toggling */
    free(plot->x11_key_boxes);
    plot->x11_key_boxes = NULL;
    plot->x11_max_key_boxes = 0;

    /* Free up memory for window title. */
    if (plot->titlestring) {
	free(plot->titlestring);
	plot->titlestring = 0;
    }

    if (plot->window) {
	FPRINTF((stderr, "Destroy window 0x%x\n", plot->window));
	XDestroyWindow(dpy, plot->window);
	plot->window = None;
	--windows_open;
    }

    if (stipple_initialized) {	/* ULIG */
	int i;
	for (i = 0; i < stipple_pattern_num; i++)
	    XFreePixmap(dpy, stipple_pattern[i]);
	XFreePixmap(dpy, stipple_dots);
	stipple_initialized = 0;
    }

    if (plot->pixmap) {
	XFreePixmap(dpy, plot->pixmap);
	plot->pixmap = None;
    }
    /* Release the colormaps here to free color resources. */
    FreeColormapList(plot);
}


/* prepare the plot structure */
static void
prepare_plot(plot_struct *plot)
{
    int i;

    for (i = 0; i < plot->ncommands; ++i)
	free(plot->commands[i]);
    plot->ncommands = 0;

    if (!plot->posn_flags) {
	/* first time this window has been used - use default or -geometry
	 * settings
	 */
	plot->posn_flags = gFlags;
	plot->x = gX;
	plot->y = gY;
	plot->width = gW;
	plot->height = gH;
	plot->window = None;
	plot->pixmap = None;
#ifdef USE_MOUSE
	plot->gheight = gH;
	plot->resizing = FALSE;
	plot->str[0] = '\0';
	plot->zoombox_on = FALSE;
#endif
	plot->first_cmap_struct = NULL;
    }
    if (plot->window == None) {
	plot->cmap = current_cmap;	/* color space */
	pr_window(plot);
#ifdef USE_MOUSE
	/*
	 * set all mouse parameters
	 * to a well-defined state.
	 */
	plot->button = 0;
	plot->mouse_on = TRUE;
	plot->x = NOT_AVAILABLE;
	plot->y = NOT_AVAILABLE;
	if (plot->str[0] != '\0') {
	    /* if string was non-empty last time, initialize it as
	     * almost empty one space, to prevent window resize */
	    plot->str[0] = ' ';
	    plot->str[1] = '\0';
	}
	plot->time = 0;		/* XXX how should we initialize this ? XXX */
#endif
    }

    /* Release the colormaps here to free color resources. */
    FreeColormapList(plot);
    /* Make the current colormap the first colormap in the plot. */
    plot->cmap = current_cmap;	/* color space */

    /* We don't know that it is the same window as before, so we reset the
     * cursors for all windows and then define the cursor for the active
     * window
     */
    plot->angle = 0;		/* default is horizontal */
    reset_cursor();
    XDefineCursor(dpy, plot->window, cursor);
}

/* store a command in a plot structure */
static void
store_command(char *buffer, plot_struct *plot)
{
    char *p;

    if (plot->ncommands >= plot->max_commands) {
	plot->max_commands = plot->max_commands * 2 + 1;
	plot->commands = (plot->commands)
	    ? (char **) realloc(plot->commands, plot->max_commands * sizeof(char *))
	    : (char **) malloc(sizeof(char *));
    }
    p = (char *) malloc(strlen(buffer) + 1);
    if (!plot->commands || !p) {
	fputs("gnuplot: can't get memory. X11 aborted.\n", stderr);
	EXIT(1);
    }
    plot->commands[plot->ncommands++] = strcpy(p, buffer);
}

#ifndef VMS

static int read_input __PROTO((void));

/*
 * Handle input.  Use read instead of fgets because stdio buffering
 * causes trouble when combined with calls to select.
 */
static int
read_input()
{
    static int rdbuf_size = 10 * X11_COMMAND_BUFFER_LENGTH;
    static char rdbuf[10 * X11_COMMAND_BUFFER_LENGTH];
    static int total_chars;
    static int rdbuf_offset;
    static int buf_offset;
    static int partial_read = 0;
    int fd = fileno(X11_ipc);

    if (!partial_read)
	buf_offset = 0;

    if (!buffered_input_available) {
	total_chars = read(fd, rdbuf, rdbuf_size);
	buffered_input_available = 1;
	partial_read = 0;
	rdbuf_offset = 0;
	if (total_chars == 0)
	    return -2;
	if (total_chars < 0)
	    return -1;
    }

    if (rdbuf_offset < total_chars) {
	while (rdbuf_offset < total_chars && buf_offset < X11_COMMAND_BUFFER_LENGTH) {
	    char c = rdbuf[rdbuf_offset++];
	    buf[buf_offset++] = c;
	    if (c == '\n')
		break;
	}

	if (buf_offset == X11_COMMAND_BUFFER_LENGTH) {
	    fputs("\ngplt_x11.c: buffer overflow in read_input!\n"
		    "            X11 aborted.\n", stderr);
	    EXIT(1);
	} else
	    buf[buf_offset] = NUL;
    }

    if (rdbuf_offset == total_chars) {
	buffered_input_available = 0;
	if (buf[buf_offset - 1] != '\n')
	    partial_read = 1;
    }

    return partial_read;
}

static void read_input_line __PROTO((void));

/*
 * Handle a whole input line, issuing an error message if a complete
 * read does not appear after a few tries.
 */
static void
read_input_line()
{
    int i_read;
    for (i_read = 1; read_input() == 1; i_read++) {
	if (i_read == 5)
	    fprintf(stderr, "\ngplt_x11.c: A complete buffer instruction is not appearing across\n"
			      "            link.  Check for system overload or driver error.\n");
    };
}

/*
 * This function builds back a palette from what x11.trm has written
 * into the pipe.  It cheats:  SMPAL_COLOR_MODE_FUNCTIONS for user defined
 * formulaes to transform gray into the three color components is not
 * implemented.  If this had to be done, one would have to include all
 * the code for evaluating functions here too, and, even worse: how to
 * transmit all function definition from gnuplot to here!
 * To avoid this, the following is done:  For grayscale, and rgbformulae
 * everything is easy.  Gradients are more difficult: Each gradient point
 * is encoded in a 8-byte string (which does not include any '\n' and
 * transmitted here.  Here the gradient is rebuilt.
 * Form user defined formulae x11.trm builds a special gradient:  The gray
 * values are equally spaced and are not transmitted, only the 6 bytes for
 * the rgbcolor are sent.  These are assembled into a gradient which is
 * than used in the palette.
 *
 * This function belongs completely into record(), but is quiet large so it
 * became a function of its own.
*/
static void
scan_palette_from_buf(void)
{
    t_sm_palette tpal;
    char cm, pos, mod;
    if (4 != sscanf( buf+2, "%c %c %c %d", &cm, &pos, &mod,
		     &(tpal.use_maxcolors) ) ) {
        fprintf( stderr, "%s:%d error in setting palette.\n",
		 __FILE__, __LINE__);

	return;
    }

    tpal.colorMode = cm;
    tpal.positive = pos;
    tpal.cmodel = mod;
    tpal.gradient = NULL;

    /* function palettes are transmitted as approximated gradients: */
    if (tpal.colorMode == SMPAL_COLOR_MODE_FUNCTIONS)
      tpal.colorMode = SMPAL_COLOR_MODE_GRADIENT;
    if (tpal.colorMode == SMPAL_COLOR_MODE_CUBEHELIX)
      tpal.colorMode = SMPAL_COLOR_MODE_GRADIENT;

    switch( tpal.colorMode ) {
    case SMPAL_COLOR_MODE_GRAY:
	read_input_line();
	if (1 != sscanf( buf, "%lf", &(tpal.gamma) )) {
	    fprintf( stderr, "%s:%d error in setting palette.\n",
		     __FILE__, __LINE__);
	    return;
	}
	break;
    case SMPAL_COLOR_MODE_RGB:
	read_input_line();
	if (3 != sscanf( buf, "%d %d %d", &(tpal.formulaR),
			 &(tpal.formulaG), &(tpal.formulaB) )) {
	    fprintf( stderr, "%s:%d error in setting palette.\n",
		     __FILE__, __LINE__);
	    return;
	}
	break;
    case SMPAL_COLOR_MODE_GRADIENT: {
	static char frac[8] = {0,0,0,0,0,0,0,0};
	int i=0;
	read_input_line();
	if (1 != sscanf( buf, "%d", &(tpal.gradient_num) )) {
	    fprintf( stderr, "%s:%d error in setting palette.\n",
		     __FILE__, __LINE__);
	    return;
	}
	tpal.gradient = (gradient_struct*)
	  malloc( tpal.gradient_num * sizeof(gradient_struct) );
	assert(tpal.gradient);
	for( i=0; i<tpal.gradient_num; i++ ) {
	    char *b = &(buf[12*(i%50)]);
	    unsigned int rgb_component;
	    /*  this %50 *must* match the corresponding line in x11.trm!  */
	    if (i%50 == 0)
	        read_input_line();
	    /* Read gradient entry as 0.1234RRGGBB */
	    memcpy(frac, b, 6);
	    tpal.gradient[i].pos = atof(frac);
	    sscanf(b+6,"%2x",&rgb_component);
	    tpal.gradient[i].col.r = (double)(rgb_component) / 255.;
	    sscanf(b+8,"%2x",&rgb_component);
	    tpal.gradient[i].col.g = (double)(rgb_component) / 255.;
	    sscanf(b+10,"%2x",&rgb_component);
	    tpal.gradient[i].col.b = (double)(rgb_component) / 255.;
	}
	break;
      }
    case SMPAL_COLOR_MODE_FUNCTIONS:
        fprintf( stderr, "%s:%d ooops: No function palettes for x11!\n",
		 __FILE__, __LINE__ );
	break;
    default:
        fprintf( stderr, "%s:%d ooops: Unknown colorMode '%c'.\n",
		 __FILE__, __LINE__, (char)(tpal.colorMode) );
	tpal.colorMode = SMPAL_COLOR_MODE_GRAY;
	break;
    }
    PaletteMake(&tpal);

    if (tpal.gradient)
	free(tpal.gradient);
}


/*
 * record - record new plot from gnuplot inboard X11 driver (Unix)
 */
/* Would like to change "plot" in this function to "current_plot".
 * "plot" is somewhat general, as though it were a local variable.
 * However, do a redefinition for now. */
#define plot current_plot
static int
record()
{
    while (1) {
	int status = read_input();
	if (status == -2)
	    return 0;
	if (status != 0)
	    return status;

	switch (*buf) {
	case 'G':		/* enter graphics mode */
	    {

#ifndef DISABLE_SPACE_RAISES_CONSOLE
#ifdef USE_MOUSE
#ifdef OS2_IPC
		sscanf(buf, "G%lu %li", &gnuplotXID, &ppidGnu);
#else
		sscanf(buf, "G%lu", &gnuplotXID);
#endif
#endif
#endif /* DISABLE_SPACE_RAISES_CONSOLE */

		if (!current_plot)
		    current_plot = Add_Plot_To_Linked_List(most_recent_plot_number);
		if (current_plot)
		    prepare_plot(current_plot);
#ifdef OS2_IPC
		if (!input_from_PM_Terminal) {	/* get shared memory */
		    sprintf(mouseShareMemName, "\\SHAREMEM\\GP%i_Mouse_Input", (int) ppidGnu);
		    if (DosGetNamedSharedMem(&input_from_PM_Terminal, mouseShareMemName, PAG_WRITE))
			DosBeep(1440L, 1000L);	/* indicates error */
		    semInputReady = 0;
		}
#endif
#ifdef USE_MOUSE
#ifdef TITLE_BAR_DRAWING_MSG
		/* show a message in the wm's title bar that the
		 * graph will be redrawn. This might be useful
		 * for slow redrawing (large plots). The title
		 * string is reset to the default at the end of
		 * display(). We should make this configurable!
		 */
		if (plot && plot->window) {
		    char *msg;
		    char *added_text = " drawing ...";
		    int orig_len = (plot->titlestring ? strlen(plot->titlestring) : 0);
		    if (msg = (char *) malloc(orig_len + strlen(added_text) + 1)) {
			strcpy(msg, plot->titlestring);
			strcat(msg, added_text);
			gpXStoreName(dpy, plot->window, msg);
			free(msg);
		    } else
			gpXStoreName(dpy, plot->window, added_text + 1);
		}
#else
		if (!button_pressed) {
		    cursor_save = cursor;
		    cursor = cursor_waiting;
		    if (plot)
			XDefineCursor(dpy, plot->window, cursor);
		}
#endif
#endif
		/* continue; */
	    }
	    break;
	case 'N':		/* just update the plot number */
	    {
		int itmp;
		if (strcspn(buf+1, " \n") && sscanf(buf, "N%d", &itmp)) {
		    if (itmp >= 0) {
			most_recent_plot_number = itmp;
			current_plot = Add_Plot_To_Linked_List(itmp);
		    }
		}
		return 1;
	    }
	    break;

	case 'Y':		/* TERM_LAYER information */
	    {
		/* Some layering commands must be handling immediately on */
		/* receipt;  the rest are stored for in-line execution.   */
		int layer;
		sscanf(buf+1, "%d", &layer);
		switch(layer)
		{
		case TERM_LAYER_BEFORE_ZOOM:
		    retain_toggle_state = TRUE;
		    break;
		default:
		    if (plot)
			store_command(buf, plot);
		    break;
		}
	    }
	    break;

#ifdef EXTERNAL_X11_WINDOW
	case X11_GR_SET_WINDOW_ID:	/* X11 window ID */
	    {
		unsigned long ultmp;
		if (strcspn(buf+1," \n") && sscanf(buf+1, "%lx", &ultmp)) {
		    Window window_id = (Window) ultmp;
		    plot_struct *tmpplot = Find_Plot_In_Linked_List_By_Window(window_id);
		    if (tmpplot) {
			current_plot = tmpplot;
			most_recent_plot_number = tmpplot->plot_number;
		    }
		    else {
			current_plot = Add_Plot_To_Linked_List(-1); /* Use invalid plot number. */
			if (current_plot) {
			    current_plot->external_container = window_id;
			    prepare_plot(current_plot);
			}
		    }
		}
		return 1;
	    }
	    break;
#endif
	case 'C':		/* close the plot with given number */
	    {
		int itmp;
		if (strcspn(buf+1, " \n") && sscanf(buf, "C%d", &itmp)) {
		    plot_struct *psp;
		    if ((psp = Find_Plot_In_Linked_List_By_Number(itmp)))
			Remove_Plot_From_Linked_List(psp->window);
		} else if (current_plot) {
		    Remove_Plot_From_Linked_List(current_plot->window);
		}
		return 1;
	    }
	    break;
	case '^':		/* raise the plot with given number or the whole group */
	    {
		int itmp;
		if (strcspn(buf+1," \n") && sscanf(buf, "^%d", &itmp)) {
		    plot_struct *psp;
		    if ((psp = Find_Plot_In_Linked_List_By_Number(itmp))) {
			XRaiseWindow(dpy, psp->window);
		    }
		} else {
		    /* Find end of list, i.e., first created. */
		    plot_struct *psp = plot_list_start;
		    while (psp != NULL) {
			if (psp->next_plot == NULL) break;
			psp = psp->next_plot;
		    }
		    while (psp != NULL) {
			XRaiseWindow(dpy, psp->window);
			psp = psp->prev_plot;
		    }
		}
		return 1;
	    }
	    break;
	case 'v':		/* lower the plot with given number or the whole group */
	    {
		int itmp;
		if (strcspn(buf+1," \n") && sscanf(buf, "v%d", &itmp)) {
		    plot_struct *psp;
		    if ((psp = Find_Plot_In_Linked_List_By_Number(itmp))) {
			XLowerWindow(dpy, psp->window);
		    }
		} else if (current_plot) {
		    plot_struct *psp = plot_list_start;
		    while (psp != NULL) {
			XLowerWindow(dpy, psp->window);
			psp = psp->next_plot;
		    }
		}
		return 1;
	    }
	    break;
	case 'n':		/* update the plot name (title) */
	    {
		if (!current_plot)
		    current_plot = Add_Plot_To_Linked_List(most_recent_plot_number);
		if (current_plot) {
		    char *cp;
		    if (current_plot->titlestring)
			free(current_plot->titlestring);
		    if ((current_plot->titlestring = (char *) malloc(strlen(buf+1) + 1) )) {
			strcpy(current_plot->titlestring, buf+1);
			cp = current_plot->titlestring;
		    } else
			cp = "<lost name>";
		    if (current_plot->window)
			gpXStoreName(dpy, current_plot->window, cp);
		}
		return 1;
	    }
	    break;
	case 'E':		/* leave graphics mode / suspend */
	    if (plot)
		display(plot);
#ifdef USE_MOUSE
	    if (current_plot)
		gp_execute_GE_plotdone(plot->window); /* notify main program */
#endif
	    return 1;
	case 'R':		/* leave x11 mode */
	    reset_cursor();
	    return 0;

	case X11_GR_MAKE_PALETTE:
	    if (have_pm3d) {
		char cmapidx[6] = "e";
		int cm_index;
	        cmap_struct *csp;
		/* Get and process palette */
		scan_palette_from_buf();
		/* Compute and store the resulting colormap index as a command
		 * so that a palette change can be made on the same plot. */
	        csp = plot->first_cmap_struct;
	        for (cm_index=0; csp; cm_index++) {
		    if (csp->cmap == current_cmap)
			break;
		    csp = csp->next_cmap_struct;
		}
		sprintf(cmapidx+1, "%3u%c", cm_index, '\0');
		store_command(cmapidx, plot);
	    }
	    return 1;

	case X11_GR_CHECK_ENDIANESS:
	    {
	        /* Initialize variable in case short happens to be longer than two bytes. */
		unsigned short tmp = (unsigned short) ENDIAN_VALUE;
		((char *)&tmp)[0] = buf[1];
		((char *)&tmp)[1] = buf[2];
		if (tmp == (unsigned short) ENDIAN_VALUE) swap_endian = 0;
		else swap_endian = 1;
	    }
	    return 1;

	case 'X':		/* tell the driver about do_raise /  persist */
	    {
		int tmp_do_raise = UNSET, tmp_persist = UNSET;
		int tmp_dashed = UNSET, tmp_ctrlq = UNSET;
		int tmp_replot_on_resize = UNSET;
		sscanf(buf, "X%d %d %d %d %d",
		       &tmp_do_raise, &tmp_persist, &tmp_dashed, &tmp_ctrlq, &tmp_replot_on_resize);
		if (UNSET != tmp_do_raise)
		    do_raise = tmp_do_raise;
		if (UNSET != tmp_persist)
		    persist = tmp_persist;
/* Version 5 - always enabled
		if (UNSET != tmp_dashed)
		    dashedlines = tmp_dashed;
 */
		if (UNSET != tmp_ctrlq)
		    ctrlq = tmp_ctrlq;
		if (UNSET != tmp_replot_on_resize)
		    replot_on_resize = tmp_replot_on_resize;
	    }
	    return 1;

        case 's': /* set window geometry */
	    {
		char strtmp[256];
		sscanf(&buf[2], "%s", strtmp);
		pr_geometry(strtmp);
	    }
	    return 1;

#ifdef USE_MOUSE
	case 'u':
#ifdef PIPE_IPC
	    if (!pipe_died)
#endif
	    {
		/* `set cursor' */
		int c, x, y;
		sscanf(buf, "u%d %d %d", &c, &x, &y);
		if (plot) {
		    switch (c) {
		    case -4:	/* switch off line between ruler and mouse cursor */
			DrawLineToRuler(plot);
			plot->ruler_lineto_on = FALSE;
			break;
		    case -3:	/* switch on line between ruler and mouse cursor */
			if (plot->ruler_on && plot->ruler_lineto_on)
			    break;
			plot->ruler_lineto_x = X(x);
			plot->ruler_lineto_y = Y(y);
			plot->ruler_lineto_on = TRUE;
			DrawLineToRuler(plot);
			break;
		    case -2:	/* warp pointer */
			XWarpPointer(dpy, None /* src_w */ ,
				     plot->window /* dest_w */ , 0, 0, 0, 0, X(x), Y(y));
		    case -1:	/* zoombox */
			plot->zoombox_x1 = plot->zoombox_x2 = X(x);
			plot->zoombox_y1 = plot->zoombox_y2 = Y(y);
			plot->zoombox_on = TRUE;
			DrawBox(plot);
			break;
		    case 0:	/* standard cross-hair cursor */
			cursor = cursor_default;
			XDefineCursor(dpy, plot->window, cursor);
			break;
		    case 1:	/* cursor during rotation */
			cursor = cursor_exchange;
			XDefineCursor(dpy, plot->window, cursor);
			break;
		    case 2:	/* cursor during scaling */
			cursor = cursor_sizing;
			XDefineCursor(dpy, plot->window, cursor);
			break;
		    case 3:	/* cursor during zooming */
			cursor = cursor_zooming;
			XDefineCursor(dpy, plot->window, cursor);
			break;
		    }
		    if (c >= 0 && plot->zoombox_on) {
			/* erase zoom box */
			DrawBox(plot);
			plot->zoombox_on = FALSE;
		    }
		    if (c >= 0 && plot->ruler_lineto_on) {
			/* erase line from ruler to cursor */
			DrawLineToRuler(plot);
			plot->ruler_lineto_on = FALSE;
		    }
		}
	    }
	    return 1;

	case 't':
#ifdef PIPE_IPC
	    if (!pipe_died)
#endif
	    {
		int where = -1;
		char *second;
		char* str;
		int char_byte_offset;

		/* sscanf manpage says %n may or may not count in the return
		   value of sscanf. I thus don't do a strong check on the number
		   of parsed elements, but also check for a valid number being
		   parsed */
		if (sscanf(buf, "t%d%n", &where, &char_byte_offset) < 1 ||
		    where < 0 )
		{
		    return 1;
		}
		buf[strlen(buf) - 1] = 0;	/* remove trailing \n */
		if (plot) {

		    /* extra 1 for the space before the string start */
		    str = &buf[ char_byte_offset + 1 ];

		    switch (where) {
		    case 0:
			DisplayCoords(plot, str);
			break;
		    case 1:
			second = strchr(str, '\r');
			if (second == NULL) {
			    *(plot->zoombox_str1a) = '\0';
			    *(plot->zoombox_str1b) = '\0';
			    break;
			}
			*second = 0;
			second++;
			if (plot->zoombox_on)
			    DrawBox(plot);
			strcpy(plot->zoombox_str1a, str);
			strcpy(plot->zoombox_str1b, second);
			if (plot->zoombox_on)
			    DrawBox(plot);
			break;
		    case 2:
			second = strchr(str, '\r');
			if (second == NULL) {
			    *(plot->zoombox_str2a) = '\0';
			    *(plot->zoombox_str2b) = '\0';
			    break;
			}
			*second = 0;
			second++;
			if (plot->zoombox_on)
			    DrawBox(plot);
			strcpy(plot->zoombox_str2a, str);
			strcpy(plot->zoombox_str2b, second);
			if (plot->zoombox_on)
			    DrawBox(plot);
			break;
		    }
		}
	    }
	    return 1;

	case 'r':
#ifdef PIPE_IPC
	    if (!pipe_died)
#endif
	    {
		if (plot) {
		    int x, y;
		    DrawRuler(plot);	/* erase previous ruler */
		    sscanf(buf, "r%d %d", &x, &y);
		    if (x < 0) {
			DrawLineToRuler(plot);
			plot->ruler_on = FALSE;
		    } else {
			plot->ruler_on = TRUE;
			plot->ruler_x = x;
			plot->ruler_y = y;
			plot->ruler_lineto_x = X(x);
			plot->ruler_lineto_y = Y(y);
			DrawLineToRuler(plot);
		    }
		    DrawRuler(plot);	/* draw new one */
		}
	    }
	    return 1;

	case 'z':
#ifdef PIPE_IPC
	    if (!pipe_died)
#endif
	    {
		int len = strlen(buf + 1) - 1;	/* discard newline '\n' */
		memcpy(selection, buf + 1, len < SEL_LEN ? len : SEL_LEN);
		/* terminate */
		selection[len < SEL_LEN ? len : SEL_LEN - 1] = '\0';
		XStoreBytes(dpy, buf + 1, len);
		XFlush(dpy);
#ifdef EXPORT_SELECTION
		if (plot && exportselection)
		    export_graph(plot);
#endif
	    }
	    return 1;
#endif
	case 'O': /*   Modify plots */
#ifdef PIPE_IPC
	    if (!pipe_died)
#endif
	    {
		unsigned int ops, i;
		int plotno = -1;
		sscanf(buf+1, "%u %d", &ops, &plotno);
		plotno++;

		if (!plot)
			return 1;

		for (i = 1; i <= x11_cur_plotno && i < plot->x11_max_key_boxes; i++) {
		    if (plotno > 0 && i != plotno)
			continue;
		    if ((ops & MODPLOTS_INVERT_VISIBILITIES) == MODPLOTS_INVERT_VISIBILITIES) {
			plot->x11_key_boxes[i].hidden = !plot->x11_key_boxes[i].hidden;
		    } else if (ops & MODPLOTS_SET_VISIBLE) {
			plot->x11_key_boxes[i].hidden = FALSE;
		    } else if (ops & MODPLOTS_SET_INVISIBLE) {
			plot->x11_key_boxes[i].hidden = TRUE;
		    }
		}

		retain_toggle_state = TRUE;
		display(plot);
	    }
	    return 1;
#ifdef USE_MOUSE
	case 'Q':
	    /* Set default font immediately and return size info through pipe */
	    if (buf[1] == 'G') {

	      /* received QG. We're thus setting up the graphics for the first
		 time. We grab the window sizes and send the back to inboard
		 gnuplot via GE_fontprops. For subsequent replots, we receive Qg
		 instead: see below */
		int scaled_hchar, scaled_vchar;
		char *c = &(buf[strlen(buf)-1]);
		while (*c <= ' ') *c-- = '\0';
		strncpy(default_font, &buf[2], strlen(&buf[2])+1);
		pr_font(NULL);
		if (plot) {
		    double scale = (double)plot->width / 4096.0;
		    scaled_hchar = (1.0/scale) * hchar;
		    scaled_vchar = (1.0/scale) * vchar;
		    FPRINTF((stderr, "gplt_x11: preset default font to %s hchar = %d vchar = %d \n",
			     default_font, scaled_hchar, scaled_vchar));
		    gp_exec_event(GE_fontprops, plot->width, plot->gheight,
				  scaled_hchar, scaled_vchar, 0);
		    ymax = 4096.0 * (double)plot->gheight / (double)plot->width;
		}
		return 1;
	    }
	    else if (buf[1] == 'g') {
	      /* received Qg. Unlike QG, this is sent during replots, not plots.
		 We simply take the current window size as the plot size */

		if (plot) {
		  ymax = 4096.0 * (double)plot->gheight / (double)plot->width;
		  gW   = plot->width;
		  gH   = plot->gheight;
		}
		return 1;
	    }
	    /* fall through */
#endif
	default:
	    if (plot) {
		/* set up the currently_receiving_gr_image variable.
		   X11_GR_IMAGE_END exists only for
		   currently_receiving_gr_image, so we don't store it */
		if     ( *buf == X11_GR_IMAGE     ) currently_receiving_gr_image = TRUE;
		else if( *buf == X11_GR_IMAGE_END ) currently_receiving_gr_image = FALSE;

		if( *buf != X11_GR_IMAGE_END )
		    store_command(buf, plot);
            }
	    continue;
	}
    }
    if (feof(X11_ipc) || ferror(X11_ipc))
	return 0;
    else
	return 1;
}
#undef plot

#else /* VMS */

/*
 *   record - record new plot from gnuplot inboard X11 driver (VMS)
 */
static struct plot_struct *plot = NULL;
record()
{
    int status;

    if ((STDIINiosb[0] & 0x1) == 0)
	EXIT(STDIINiosb[0]);
    STDIINbuffer[STDIINiosb[1]] = '\0';
    strcpy(buf, STDIINbuffer);

    switch (*buf) {
    case 'G':			/* enter graphics mode */
	{
	    if (!plot)
		plot = Add_Plot_To_Linked_List(most_recent_plot_number);
	    if (plot)
		prepare_plot(plot);
	    current_plot = plot;
	    break;
	}
    case 'E':			/* leave graphics mode */
	if (plot)
	    display(plot);
	break;
    case 'R':			/* exit x11 mode */
	FPRINTF((stderr, "received R - sending ClientMessage\n"));
	reset_cursor();
	sys$cancel(STDIINchannel);
	/* this is ridiculous - cook up an event to ourselves,
	 * in order to get the mainloop() out of the XNextEvent() call
	 * it seems that window manager can also send clientmessages,
	 * so put a checksum into the message
	 */
	{
	    XClientMessageEvent event;
	    event.type = ClientMessage;
	    event.send_event = True;
	    event.display = dpy;
	    event.window = message_window;
	    event.message_type = None;
	    event.format = 8;
	    strcpy(event.data.b, "die gnuplot die");
	    XSendEvent(dpy, message_window, False, 0, (XEvent *) & event);
	    XFlush(dpy);
	}
	return;			/* no ast */
    default:
	if (plot)
	    store_command(buf, plot);
	break;
    }
    ast();
}
#endif /* VMS */

static int
DrawRotatedErrorHandler(Display * display, XErrorEvent * error_event)
{
  return 0;  /* do nothing */
}

static void
DrawRotated(plot_struct *plot, Display *dpy, GC gc, int xdest, int ydest,
	const char *str, int len)
{
    Window w = plot->window;
    Drawable d = plot->pixmap;
    double angle = plot->angle;
    enum JUSTIFY just = plot->jmode;
    int x, y;
    double src_x, src_y;
    double dest_x, dest_y;
    int width = gpXTextWidth(font, str, len);
    int height = vchar;
    double src_cen_x = (double)width * 0.5;
    double src_cen_y = (double)height * 0.5;
    static const double deg2rad = .01745329251994329576; /* atan2(1, 1) / 45.0; */
    double sa = sin(angle * deg2rad);
    double ca = cos(angle * deg2rad);
    int dest_width = (double)height * fabs(sa) + (double)width * fabs(ca) + 2;
    int dest_height = (double)width * fabs(sa) + (double)height * fabs(ca) + 2;
    double dest_cen_x = (double)dest_width * 0.5;
    double dest_cen_y = (double)dest_height * 0.5;
    char* data = (char*) malloc(dest_width * dest_height * sizeof(char));
    Pixmap pixmap_src = XCreatePixmap(dpy, root, (unsigned int)width, (unsigned int)height, 1);
    XImage *image_src;
    XImage *image_dest;
    XImage *image_scr;
    unsigned long fgpixel = 0;
    unsigned long bgpixel = 0;
    XWindowAttributes win_attrib;
    int xscr, yscr, xoff, yoff;
    unsigned int scr_width, scr_height;
    XErrorHandler prevErrorHandler;

    unsigned long gcFunctionMask = GCFunction;
    XGCValues gcValues;
    int gcCurrentFunction = 0;
    Status s;

    /* bitmapGC is static, so that is has to be initialized only once */
    static GC bitmapGC = (GC) 0;

    /* eventually initialize bitmapGC */
    if ((GC)0 == bitmapGC) {
	bitmapGC = XCreateGC(dpy, pixmap_src, 0, (XGCValues *) 0);
	XSetForeground(dpy, bitmapGC, 1);
	XSetBackground(dpy, bitmapGC, 0);
    }

    s = XGetGCValues(dpy, gc, gcFunctionMask|GCForeground|GCBackground, &gcValues);
    if (s) {
	/* success */
	fgpixel = gcValues.foreground;
	bgpixel = gcValues.background;
	gcCurrentFunction = gcValues.function; /* save current function */
    }

    /* set font for the bitmap GC */
    if (font)
      gpXSetFont(dpy, bitmapGC, font->fid);

    /* draw string to the source bitmap */
    gpXDrawImageString(dpy, pixmap_src, bitmapGC, 0, gpXGetFontascent(font), str, len);

    /* create XImage's of depth 1 */
    /* source from pixmap */
    image_src = XGetImage(dpy, pixmap_src, 0, 0, (unsigned int)width, (unsigned int)height,
	    1, XYPixmap /* ZPixmap, XYBitmap */ );

    /* empty dest */
    assert(data);
    memset((void*)data, 0, (size_t)dest_width * dest_height);
    image_dest = XCreateImage(dpy, vis, 1, XYBitmap,
	    0, data, (unsigned int)dest_width, (unsigned int)dest_height, 8, 0);

#define RotateX(_x, _y) (( (_x) * ca + (_y) * sa + dest_cen_x))
#define RotateY(_x, _y) ((-(_x) * sa + (_y) * ca + dest_cen_y))
    /* copy & rotate from source --> dest */
    for (y = 0, src_y = -src_cen_y; y < height; y++, src_y++) {
	for (x = 0, src_x = -src_cen_x; x < width; x++, src_x++) {
	    /* TODO: move some operations outside the inner loop (joze) */
	    dest_x = rint(RotateX(src_x, src_y));
	    dest_y = rint(RotateY(src_x, src_y));
	    if (dest_x >= 0 && dest_x < dest_width && dest_y >= 0 && dest_y < dest_height)
		XPutPixel(image_dest, (int)dest_x, (int)dest_y, XGetPixel(image_src, x, y));
	}
    }

    src_cen_y = 0; /* EAM 29-Sep-2002 - vertical justification has already been done */

    switch (just) {
	case LEFT:
	default:
	    xdest -= RotateX(-src_cen_x, src_cen_y);
	    ydest -= RotateY(-src_cen_x, src_cen_y);
	    break;
	case CENTRE:
	    xdest -= RotateX(0, src_cen_y);
	    ydest -= RotateY(0, src_cen_y);
	    break;
	case RIGHT:
	    xdest -= RotateX(src_cen_x, src_cen_y);
	    ydest -= RotateY(src_cen_x, src_cen_y);
	    break;
    }

#undef RotateX
#undef RotateY

    if (fast_rotate) {
    /* This default method is a lot faster, but may corrupt the colors
     * underneath the rotated text if the X display Visual is PseudoColor.
     * EAM - August 2005
     */
	assert(s);	/* Previous success in reading XGetGCValues() */
	/* Force pixels of new text to black, background unchanged */
	gcValues.function = GXand;
	gcValues.background = WhitePixel(dpy, scr);
	gcValues.foreground = BlackPixel(dpy, scr);
	XChangeGC(dpy, gc, gcFunctionMask|GCBackground|GCForeground, &gcValues);
	XPutImage(dpy, d, gc, image_dest, 0, 0, xdest, ydest, dest_width, dest_height);

	/* Force pixels of new text to color, background unchanged */
	gcValues.function = GXor;
	gcValues.background = BlackPixel(dpy, scr);
	gcValues.foreground = fgpixel;
	XChangeGC(dpy, gc, gcFunctionMask|GCBackground|GCForeground, &gcValues);
	XPutImage(dpy, d, gc, image_dest, 0, 0, xdest, ydest, dest_width, dest_height);

    } else {
    /* Slow but sure version - grab the current screen area where the new
     * text will go and substitute in the pixels of the new text one by one.
     * NB: selected by X Resource
     *                             gnuplot*fastrotate: off
     */
	assert(s);	/* Previous success in reading XGetGCValues() */
	gcValues.function = GXcopy;
	XChangeGC(dpy, gc, gcFunctionMask, &gcValues);
	s = XGetWindowAttributes(dpy, w, &win_attrib);
	/* compute screen coords that are within the current window */
	xscr = (xdest<0)? 0 : xdest;
	yscr = (ydest<0)? 0 : ydest;;
	scr_width = dest_width; scr_height = dest_height;
	if (xscr + dest_width > win_attrib.width)
	    scr_width = win_attrib.width - xscr;
	if (yscr + dest_height > win_attrib.height)
	    scr_height = win_attrib.height - yscr;
	xoff = xscr - xdest;
	yoff = yscr - ydest;
	scr_width -= xoff;
	scr_height -= yoff;
	prevErrorHandler = XSetErrorHandler(DrawRotatedErrorHandler);

	image_scr = XGetImage(dpy, d, xscr, yscr, scr_width,
			  scr_height, AllPlanes, XYPixmap);
	if (image_scr != 0){
	    /* copy from 1 bit bitmap image of text to the full depth image of screen*/
	    for (y = 0; y < scr_height; y++){
		for (x = 0; x < scr_width; x++){
		    if (XGetPixel(image_dest, x + xoff, y + yoff)){
			XPutPixel(image_scr, x, y, fgpixel);
		    }
		}
	    }
	    /* copy the rotated image to the drawable d */
	    XPutImage(dpy, d, gc, image_scr, 0, 0, xscr, yscr, scr_width, scr_height);

	    XDestroyImage(image_scr);
	}
	XSetErrorHandler(prevErrorHandler);
    } /* End slow rotatation code */

    /* free resources */
    XFreePixmap(dpy, pixmap_src);
    XDestroyImage(image_src);
    XDestroyImage(image_dest);

    if (s) {
	/* restore original state of gc */
	gcValues.function = gcCurrentFunction;
	gcValues.background = bgpixel;
	XChangeGC(dpy, gc, gcFunctionMask|GCBackground, &gcValues);
    }
}

/*
 *   exec_cmd - execute drawing command from inboard driver
 */
static void
exec_cmd(plot_struct *plot, char *command)
{
    int x, y, sw, sl, sj;
    char *buffer, *str;
    char *strx, *stry;

    buffer = command;
    strx = buffer+1;

    /* Skip the plot commands, but not the key sample commands,
     * if the plot was toggled off by a mouse click in the GUI
     */
    if (x11_in_plot && !x11_in_key_sample
    &&  *command != 'Y'
    &&  x11_cur_plotno < plot->x11_max_key_boxes
    &&  plot->x11_key_boxes[x11_cur_plotno].hidden
    )
	    return;

    if (x11_in_key_sample &&  *command == 'Y' /* Catches TERM_LAYER_END_KEYSAMPLE */
    &&  x11_cur_plotno < plot->x11_max_key_boxes
    &&  plot->x11_key_boxes[x11_cur_plotno].hidden
    )
	{
	x11BoundingBox *box = &plot->x11_key_boxes[x11_cur_plotno];
	    /* Grey out key box */
	    if (!fill_gc)
		fill_gc = XCreateGC(dpy, plot->window, 0, 0);
	    XCopyGC(dpy, *current_gc, ~0, fill_gc);
	    XSetForeground(dpy, fill_gc, plot->cmap->colors[1]);
	    XSetStipple(dpy, fill_gc, stipple_dots);
	    XSetFillStyle(dpy, fill_gc, FillStippled);
	    XFillRectangle(dpy, plot->pixmap, fill_gc,
		box->left, box->ybot,
		box->right - box->left, box->ytop - box->ybot);
	}

    /*   X11_vector(x, y) - draw vector  */
    if (*buffer == 'V') {
	x = strtol(strx, &stry, 0);
	y = strtol(stry, NULL, 0);

	if (polyline_size == 0) {
	    polyline[polyline_size].x = X(cx);
	    polyline[polyline_size].y = Y(cy);
	}
	if (++polyline_size >= polyline_space) {
	    polyline_space += 100;
	    polyline = realloc(polyline, polyline_space * sizeof(XPoint));
	    if (!polyline) fprintf(stderr, "Panic: cannot realloc polyline\n");
	}
	polyline[polyline_size].x = X(x);
	polyline[polyline_size].y = Y(y);
	cx = x;
	cy = y;
	/* Limit the number of vertices in any single polyline */
	if (polyline_size > max_request_size) {
	    FPRINTF((stderr, "(display) dumping polyline size %d\n", polyline_size));
	    XDrawLines(dpy, plot->pixmap, *current_gc,
			polyline, polyline_size+1, CoordModeOrigin);
	    polyline_size = 0;
	}
	/* Toggle mechanism */
	if (x11_in_key_sample) {
	    x11_update_key_box(plot, X(x) - hchar, Y(y) - vchar/2);
	    x11_update_key_box(plot, X(x) + hchar, Y(y) + vchar/2);
	}
	return;
    } else if (polyline_size > 0) {
	FPRINTF((stderr, "(display) dumping polyline size %d\n", polyline_size));
	XDrawLines(dpy, plot->pixmap, *current_gc,
			polyline, polyline_size+1, CoordModeOrigin);
	polyline_size = 0;
    }

    /*   X11_move(x, y) - move  */
    if (*buffer == 'M') {
	cx = strtol(strx, &stry, 0);
	cy = strtol(stry, NULL, 0);
    }

    /* change default font (QD) encoding (QE) or current font (QF)  */
    else if (*buffer == 'Q') {
	char *c;
	switch (buffer[1]) {
	case 'F':
		/* Strip out just the font name */
		c = &(buffer[strlen(buffer)-1]);
		while (*c <= ' ') *c-- = '\0';
		pr_font(&buffer[2]);
		if (font)
		  gpXSetFont(dpy, gc, font->fid);
		break;
	case 'E':
		/* Save the requested font encoding */
		{
		    int tmp;
		    sscanf(buffer, "QE%d", &tmp);
		    encoding = (enum set_encoding_id)tmp;
		    clear_used_font_list();
		}
		FPRINTF((stderr, "gnuplot_x11: changing encoding to %d\n", encoding));
		break;
	case 'D':
		/* Save the request default font */
		c = &(buffer[strlen(buffer)-1]);
		while (*c <= ' ') *c-- = '\0';
		strncpy(default_font, &buffer[2], strlen(&buffer[2])+1);
		FPRINTF((stderr, "gnuplot_x11: exec_cmd() set default_font to \"%s\"\n", default_font));
		break;
	}
    }

    /*   X11_put_text(x, y, str) - draw text   */
    else if (*buffer == 'T') {
	/* Enhanced text mode added November 2003 - Ethan A Merritt */
	int x_offset=0, y_offset=0, v_offset=0;
	int char_byte_offset;
#ifdef EAM_BOXED_TEXT
	unsigned int bb[4];
#endif

	switch (buffer[1]) {

	case 'j':	/* Set start for right-justified enhanced text */
		    sscanf(buffer+2, "%d %d", &x_offset, &y_offset);
		    plot->xLast = x_offset - (plot->xLast - x_offset);
		    plot->yLast = y_offset - (vchar/3) / yscale;
#ifdef EAM_BOXED_TEXT
		    bounding_box[0] = bounding_box[2] = X(plot->xLast);
		    bounding_box[1] = bounding_box[3] = Y(plot->yLast);
#endif
		    return;
	case 'k':	/* Set start for center-justified enhanced text */
		    sscanf(buffer+2, "%d %d", &x_offset, &y_offset);
		    plot->xLast = x_offset - 0.5*(plot->xLast - x_offset);
		    plot->yLast = y_offset - (vchar/3) / yscale;
#ifdef EAM_BOXED_TEXT
		    bounding_box[0] = bounding_box[2] = X(plot->xLast);
		    bounding_box[1] = bounding_box[3] = Y(plot->yLast);
#endif
		    return;
	case 'l':	/* Set start for left-justified enhanced text */
		    sscanf(buffer+2, "%d %d", &x_offset, &y_offset);
		    plot->xLast = x_offset;
		    plot->yLast = y_offset - (vchar/3) / yscale;
#ifdef EAM_BOXED_TEXT
		    bounding_box[0] = bounding_box[2] = X(plot->xLast);
		    bounding_box[1] = bounding_box[3] = Y(plot->yLast);
#endif
		    return;
	case 'o':	/* Enhanced mode print with no update */
	case 'c':	/* Enhanced mode print with update to center */
	case 'u':	/* Enhanced mode print with update */
	case 's':	/* Enhanced mode update with no print */
		    sscanf(buffer+2, "%d %d%n", &x_offset, &y_offset, &char_byte_offset);
		    /* EAM FIXME - This code has only been tested for x_offset == 0 */
		    if (plot->angle != 0) {
			int xtmp=0, ytmp=0;
			xtmp += x_offset * cos((double)(plot->angle) * 0.01745);
			xtmp -= y_offset * sin((double)(plot->angle) * 0.01745) * yscale/xscale;
			ytmp += x_offset * sin((double)(plot->angle) * 0.01745) * xscale/yscale;
			ytmp += y_offset * cos((double)(plot->angle) * 0.01745);
			x_offset = xtmp;
			y_offset = ytmp;
		    }
		    x = plot->xLast + x_offset;
		    y = plot->yLast + y_offset;

		    /* buffer+2 was the start point for sscanf above */
		    /* extra 1 for the space before the string start */
		    str = buffer+2 + char_byte_offset + 1;
		    break;
	case 'p':	/* Push (Save) position for later use */
		    plot->xSave = plot->xLast;
		    plot->ySave = plot->yLast;
		    return;
	case 'r':	/* Pop (Restore) saved position */
		    plot->xLast = plot->xSave;
		    plot->yLast = plot->ySave;
		    return;
#ifdef EAM_BOXED_TEXT
	case 'b':	/* Initialize text bounding box */
		    sscanf(buffer, "Tb%d %d", &x, &y);
		    bounding_box[0] = bounding_box[2] = X(x);
		    bounding_box[1] = bounding_box[3] = Y(y) + vchar/3;
		    boxing = TRUE;
		    return;
	case 'B':	/* Draw text bounding box */
		    bb[0] = bounding_box[0] - box_xmargin;
		    bb[1] = bounding_box[1] - box_ymargin;
		    bb[2] = bounding_box[2] + box_xmargin;
		    bb[3] = bounding_box[3] + box_ymargin;
		    XDrawLine(dpy, plot->pixmap, *current_gc, bb[0], bb[1], bb[0], bb[3]);
		    XDrawLine(dpy, plot->pixmap, *current_gc, bb[0], bb[3], bb[2], bb[3]);
		    XDrawLine(dpy, plot->pixmap, *current_gc, bb[2], bb[3], bb[2], bb[1]);
		    XDrawLine(dpy, plot->pixmap, *current_gc, bb[2], bb[1], bb[0], bb[1]);
		    boxing = FALSE;
		    return;
	case 'F':	/* Erase inside of text bounding box */
		    bb[0] = bounding_box[0] - box_xmargin;
		    bb[1] = bounding_box[1] - box_ymargin;
		    bb[2] = bounding_box[2] + box_xmargin;
		    bb[3] = bounding_box[3] + box_ymargin;
		    /* Load selected pattern or fill into a separate gc */
		    if (!fill_gc)
			fill_gc = XCreateGC(dpy,plot->window,0,0);
		    XCopyGC(dpy, *current_gc, ~0, fill_gc);
		    XSetFillStyle(dpy, fill_gc, FillSolid);
		    XSetForeground(dpy, fill_gc, plot->cmap->colors[0]);
		    XFillRectangle(dpy, plot->pixmap, fill_gc, 
			bb[0], bb[1], bb[2]-bb[0], bb[3]-bb[1]);
		    /* boxing = FALSE; */
		    return;
	case 'm':	/* Change textbox margins */
		    sscanf(buffer, "Tm%d %d", &x, &y);
		    box_xmargin = X11_TEXTBOX_MARGIN * (double)(x) / 100.;
		    box_ymargin = X11_TEXTBOX_MARGIN * (double)(y) / 100.;
		    return;
#endif
	default:
		    sscanf(buffer, "T%d %d%n", &x, &y, &char_byte_offset);
		    /* extra 1 for the space before the string start */
		    str = buffer + char_byte_offset + 1;
		    v_offset = vchar/3;		/* Why? */
		    break;
	}

#ifdef USE_X11_MULTIBYTE
	/* FIXME EAM DEBUG: We should not have gotten here without a valid font	*/
	/* but apparently it can happen in the case of UTF-8.  The sanity check	*/
	/* below must surely belong somewhere during font selection instead.	*/
	if (usemultibyte && !mbfont) {
	    usemultibyte = 0;
	    fprintf(stderr,"gnuplot_x11: invalid multibyte font\n");
	}
#endif

	sl = strlen(str) - 1;
	sw = gpXTextWidth(font, str, sl);

/*	EAM - May 2002	Modify to allow colored text.
 *	1) do not force foreground of gc to be black
 *	2) write text to (*current_gc), rather than to gc, so that text color can be set
 *	   using pm3d mappings.
 */

	switch (plot->jmode) {
	    default:
	    case LEFT:
		sj = 0;
		break;
	    case CENTRE:
		sj = -sw / 2;
		break;
	    case RIGHT:
		sj = -sw;
		break;
	}

	if (sl == 0) /* Pointless to draw empty string */
	    ;
	else if (buffer[1] == 's') /* Enhanced text mode reserve space only */
	    ;
	else if (plot->angle != 0) {
	    /* rotated text */
	    DrawRotated(plot, dpy, *current_gc, X(x), Y(y), str, sl);
	} else {
	    /* horizontal text */
	    gpXDrawString(dpy, plot->pixmap, *current_gc,
		    X(x) + sj, Y(y) + v_offset, str, sl);
#ifdef EAM_BOXED_TEXT
	    if (boxing) {
		/* Request bounding box information for this string */
		int direction, ascent, descent;
		unsigned int bb[4];
		XCharStruct overall;
		XTextExtents(font, str, sl, &direction, &ascent, &descent, &overall);
		bb[0] = X(x) + overall.lbearing + sj;
		bb[2] = X(x) + overall.rbearing + sj;
		bb[1] = Y(y) - overall.ascent  + v_offset;
		bb[3] = Y(y) + overall.descent + v_offset;
		if (bb[0] < bounding_box[0]) bounding_box[0] = bb[0];
		if (bb[2] > bounding_box[2]) bounding_box[2] = bb[2];
		if (bb[1] < bounding_box[1]) bounding_box[1] = bb[1];
		if (bb[3] > bounding_box[3]) bounding_box[3] = bb[3];
	    }
#endif

	    /* Toggle mechanism */
	    if (x11_in_key_sample) {
		x11_update_key_box(plot, X(x)+sj, Y(y) - vchar/2);
		x11_update_key_box(plot, X(x)+sj + sw, Y(y) + vchar/2);
	    }
	}

	/* Update current text position */
	if (buffer[1] == 'c') {
	    plot->xLast = RevX(X(x) + sj + sw/2) - x_offset;
	    plot->yLast = y - y_offset;
	} else if (buffer[1] != 'o') {
	    plot->xLast = RevX(X(x) + sj + sw) - x_offset;
	    plot->yLast = y - y_offset;
	    if (plot->angle != 0) { /* This correction is not perfect */
		plot->yLast += RevX(sw) * sin((plot->angle) * 0.01745) * xscale/yscale;
		plot->xLast -= RevX(sw) * (1.0 - cos((plot->angle) * 0.01745));
	    }
	}

    } else if (*buffer == 'F') {	/* fill box */
	int style, xtmp, ytmp, w, h;

	if (sscanf(buffer + 1, "%d %d %d %d %d", &style, &xtmp, &ytmp, &w, &h) == 5) {

	    /* Load selected pattern or fill into a separate gc */
	    if (!fill_gc)
		fill_gc = XCreateGC(dpy, plot->window, 0, 0);
	    XCopyGC(dpy, *current_gc, ~0, fill_gc);
	    x11_setfill(&fill_gc, style);

	    /* gnuplot has origin at bottom left, but X uses top left
	     * There may be an off-by-one (or more) error here.
	     */
	    ytmp += h;		/* top left corner of rectangle to be filled */
	    w *= xscale;
	    h *= yscale;
	    XFillRectangle(dpy, plot->pixmap, fill_gc, X(xtmp), Y(ytmp), w + 1, h + 1);
	    /* Toggle mechanism */
	    if (x11_in_key_sample) {
		x11_update_key_box(plot, X(xtmp), Y(ytmp));
		x11_update_key_box(plot, X(xtmp) + w, Y(ytmp) + h);
	    }
	}
    }
    /*   X11_justify_text(mode) - set text justification mode  */
    else if (*buffer == 'J')
	sscanf(buffer, "J%d", (int *) &plot->jmode);

    else if (*buffer == 'A')
	sscanf(buffer + 1, "%lf", &plot->angle);

    /*  X11_linewidth(plot->lwidth) - set line width */
    else if (*buffer == 'W')
	sscanf(buffer + 1, "%d", &plot->user_width);

    /* X11_dashtype() - set custom dash pattern */
    else if (*buffer == 'D') {
	int len;
	char pattern[DASHPATTERN_LENGTH+1];

	memset(pattern, '\0', sizeof(pattern));
	sscanf(buffer, "D%8s", pattern);
	for (len=0; isalpha((unsigned char)pattern[len]); len++) {
	    pattern[len] = pattern[len] + 1 - 'A';
	    pattern[len] *= plot->lwidth * 0.5;
	}
	pattern[len] = '\0';
	XSetDashes(dpy, gc, 0, pattern, len);
	plot->type = LineOnOffDash;
	XSetLineAttributes(dpy, gc, plot->lwidth, plot->type, CapButt, JoinBevel);
    }

    /*   X11_linetype(plot->type) - set line type  */
    else if (*buffer == 'L') {
	sscanf(buffer, "L%d", &plot->lt);

	plot->lt = (plot->lt % 8) + 2;
	plot->lwidth = plot->user_width;

	/* Fixme: no mechanism to hold width or dashstyle for LT_BACKGROUND */
	if (plot->lt < 0) { /* LT_NODRAW, LT_BACKGROUND, LT_UNDEFINED */
	    plot->lt = -3;

	} else {
	    /* LT_SOLID is a special case because version 5 uses it for all  */
	    /* solid lines, whereas versions < 5 used it only for the border */
	    /* default width is 0 {which X treats as 1} */
	    if (plot->lt > 0 && widths[plot->lt] > 0)
		plot->lwidth *= widths[plot->lt];

	    if (((dashedlines == yes) && dashes[plot->lt][0])
	    ||  (plot->lt == LT_AXIS+2 && dashes[LT_AXIS+2][0])) {
		char pattern[DASHPATTERN_LENGTH+1];
		int len = strlen(dashes[plot->lt]);
		int i;

		pattern[len] = '\0';
		for (i=0; i<len; i++) {
		    pattern[i] = dashes[plot->lt][i];
		    pattern[i] *= plot->lwidth;
		}

		plot->type = LineOnOffDash;
		XSetDashes(dpy, gc, 0, pattern, len);
	    } else {
		plot->type = LineSolid;
	    }
	}

	XSetForeground(dpy, gc, plot->cmap->colors[plot->lt + 3]);
	XSetLineAttributes(dpy, gc, plot->lwidth, plot->type, CapButt, JoinBevel);
	plot->current_rgb = plot->cmap->rgbcolors[plot->lt + 3];
	current_gc = &gc;
    }
    /*   X11_point(number) - draw a point */
    else if (*buffer == 'P') {
	int point;
	point = strtol(buffer+1, &strx, 0);
	x = strtol(strx, &stry, 0);
	y = strtol(stry, NULL, 0);
	if (point == -2) {
	    /* set point size */
	    plot->px = (int) (x * pointsize * 3.0 / 4096.0);
	    plot->py = (int) (y * pointsize * 3.0 / 4096.0);
	} else if (point == -1) {
	    /* dot */
	    XDrawPoint(dpy, plot->pixmap, *current_gc, X(x), Y(y));
	} else {
	    unsigned char fill = 0;
	    unsigned char upside_down_fill = 0;
	    short upside_down_sign = 1;
	    int delta = (plot->px + plot->py + 1)/2;

	    /* Force line type to solid, with round ends */
	    XSetLineAttributes(dpy, *current_gc, plot->lwidth, LineSolid, CapRound, JoinRound);

	    switch (point % 13) {
	    case 0:		/* do plus */
		Plus[0].x1 = (short) X(x) - delta;
		Plus[0].y1 = (short) Y(y);
		Plus[0].x2 = (short) X(x) + delta;
		Plus[0].y2 = (short) Y(y);
		Plus[1].x1 = (short) X(x);
		Plus[1].y1 = (short) Y(y) - delta;
		Plus[1].x2 = (short) X(x);
		Plus[1].y2 = (short) Y(y) + delta;

		XDrawSegments(dpy, plot->pixmap, *current_gc, Plus, 2);
		break;
	    case 1:		/* do X */
		Cross[0].x1 = (short) X(x) - delta;
		Cross[0].y1 = (short) Y(y) - delta;
		Cross[0].x2 = (short) X(x) + delta;
		Cross[0].y2 = (short) Y(y) + delta;
		Cross[1].x1 = (short) X(x) - delta;
		Cross[1].y1 = (short) Y(y) + delta;
		Cross[1].x2 = (short) X(x) + delta;
		Cross[1].y2 = (short) Y(y) - delta;

		XDrawSegments(dpy, plot->pixmap, *current_gc, Cross, 2);
		break;
	    case 2:		/* do star */
		Star[0].x1 = (short) X(x) - delta;
		Star[0].y1 = (short) Y(y);
		Star[0].x2 = (short) X(x) + delta;
		Star[0].y2 = (short) Y(y);
		Star[1].x1 = (short) X(x);
		Star[1].y1 = (short) Y(y) - delta;
		Star[1].x2 = (short) X(x);
		Star[1].y2 = (short) Y(y) + delta;
		Star[2].x1 = (short) X(x) - delta;
		Star[2].y1 = (short) Y(y) - delta;
		Star[2].x2 = (short) X(x) + delta;
		Star[2].y2 = (short) Y(y) + delta;
		Star[3].x1 = (short) X(x) - delta;
		Star[3].y1 = (short) Y(y) + delta;
		Star[3].x2 = (short) X(x) + delta;
		Star[3].y2 = (short) Y(y) - delta;

		XDrawSegments(dpy, plot->pixmap, *current_gc, Star, 4);
		break;
	    case 3:		/* do box */
		XDrawRectangle(dpy, plot->pixmap, *current_gc, X(x) - delta, Y(y) - delta,
			(delta + delta), (delta + delta));
		XDrawPoint(dpy, plot->pixmap, *current_gc, X(x), Y(y));
		break;
	    case 4:		/* filled box */
		XFillRectangle(dpy, plot->pixmap, *current_gc, X(x) - delta, Y(y) - delta,
			(delta + delta), (delta + delta));
		break;
	    case 5:		/* circle */
		XDrawArc(dpy, plot->pixmap, *current_gc, X(x) - delta, Y(y) - delta,
			2 * delta, 2 * delta, 0, 23040 /* 360 * 64 */);
		XDrawPoint(dpy, plot->pixmap, *current_gc, X(x), Y(y));
		break;
	    case 6:		/* filled circle */
		XFillArc(dpy, plot->pixmap, *current_gc, X(x) - delta, Y(y) - delta,
			2 * delta, 2 * delta, 0, 23040 /* 360 * 64 */);
		break;
	    case 10:		/* filled upside-down triangle */
		upside_down_fill = 1;
		/* FALLTHRU */
	    case 9:		/* do upside-down triangle */
		upside_down_sign = (short)-1;
	    case 8:		/* filled triangle */
		fill = 1;
		/* FALLTHRU */
	    case 7:		/* do triangle */
		{
		    short temp_x, temp_y;

		    temp_x = (short) (1.33 * (double) delta + 0.5);
		    temp_y = (short) (1.33 * (double) delta + 0.5);

		    Triangle[0].x = (short) X(x);
		    Triangle[0].y = (short) Y(y) - upside_down_sign * temp_y;
		    Triangle[1].x = (short) temp_x;
		    Triangle[1].y = (short) upside_down_sign * 2 * delta;
		    Triangle[2].x = (short) -(2 * temp_x);
		    Triangle[2].y = (short) 0;
		    Triangle[3].x = (short) temp_x;
		    Triangle[3].y = (short) -(upside_down_sign * 2 * delta);

		    if ((upside_down_sign == 1 && fill) || upside_down_fill) {
			XFillPolygon(dpy, plot->pixmap, *current_gc,
				Triangle, 4, Convex, CoordModePrevious);
		    } else {
			XDrawLines(dpy, plot->pixmap, *current_gc, Triangle, 4, CoordModePrevious);
			XDrawPoint(dpy, plot->pixmap, *current_gc, X(x), Y(y));
		    }
		}
		break;
	    case 12:		/* filled diamond */
		fill = 1;
		/* FALLTHRU */
	    case 11:		/* do diamond */
		Diamond[0].x = (short) X(x) - delta;
		Diamond[0].y = (short) Y(y);
		Diamond[1].x = (short) delta;
		Diamond[1].y = (short) -delta;
		Diamond[2].x = (short) delta;
		Diamond[2].y = (short) delta;
		Diamond[3].x = (short) -delta;
		Diamond[3].y = (short) delta;
		Diamond[4].x = (short) -delta;
		Diamond[4].y = (short) -delta;

		/*
		 * Should really do a check with XMaxRequestSize()
		 */

		if (fill) {
		    XFillPolygon(dpy, plot->pixmap, *current_gc,
			    Diamond, 5, Convex, CoordModePrevious);
		} else {
		    XDrawLines(dpy, plot->pixmap, *current_gc, Diamond, 5, CoordModePrevious);
		    XDrawPoint(dpy, plot->pixmap, *current_gc, X(x), Y(y));
		}
		break;
	    }

	    /* Toggle mechanism */
	    if (x11_in_key_sample) {
		x11_update_key_box(plot, X(x) - hchar, Y(y) - vchar/2);
		x11_update_key_box(plot, X(x) + hchar, Y(y) + vchar/2);
	    }

	    /* Restore original line style */
	    XSetLineAttributes(dpy, *current_gc, plot->lwidth, plot->type, CapButt, JoinBevel);
	}
    }
    else if (*buffer == X11_GR_SET_LINECOLOR) {
	    int lt;
	    lt = strtol(strx, NULL, 0);
	    lt = (lt % 8) + 2;
	    if (lt < 0) /* LT_NODRAW, LT_BACKGROUND, LT_UNDEFINED */
		lt = -3;
	    XSetForeground(dpy, gc, plot->cmap->colors[lt + 3]);
	    plot->current_rgb = plot->cmap->rgbcolors[lt + 3];
	    current_gc = &gc;
    } else if (*buffer == X11_GR_SET_RGBCOLOR) {
	    int rgb255color;
	    XColor xcolor;
	    sscanf(buffer + 1, "%x", &rgb255color);
	    xcolor.red = (double)(0xffff) * (double)((rgb255color >> 16) & 0xff) /255.;
	    xcolor.green = (double)(0xffff) * (double)((rgb255color >> 8) & 0xff) /255.;
	    xcolor.blue = (double)(0xffff) * (double)(rgb255color & 0xff) /255.;
	    FPRINTF((stderr, "gplt_x11: got request for color %d %d %d\n",
		    xcolor.red, xcolor.green, xcolor.blue));
	    if (XAllocColor(dpy, plot->cmap->colormap, &xcolor)) {
		XSetForeground(dpy, gc, xcolor.pixel);
		plot->current_rgb = rgb255color;
	    } else {
		FPRINTF((stderr, "          failed to allocate color\n"));
	    }
	    current_gc = &gc;

    } else if (*buffer == X11_GR_SET_COLOR) {	/* set color */
	if (have_pm3d) {	/* ignore, if your X server is not supported */
	    double gray;
	    sscanf(buffer + 1, "%lf", &gray);
	    PaletteSetColor(plot, gray);
	    current_gc = &gc;
	}
    }

    else if (*buffer == X11_GR_BINARY_COLOR) {	/* set color */
	if (have_pm3d) {	/* ignore, if your X server is not supported */
	    /* This command will fit within a single buffer so it doesn't
	     * need to be so elaborate.
	     */
	    unsigned char *iptr;
	    float gray;
	    unsigned int i_remaining;
	    char *bptr;
	    TBOOLEAN code_detected = 0;

	    iptr = (unsigned char *) &gray;
	    i_remaining = sizeof(gray);

	    /* Decode and reconstruct the data. */
	    for (bptr = buffer + 1; i_remaining; ) {
	      unsigned char uctmp = *bptr++;
	      if (code_detected) {
		code_detected = 0;
		*iptr++ = uctmp - 1 + SET_COLOR_TRANSLATION_CHAR;
		i_remaining--;
	      }
	      else {
		if ( uctmp == SET_COLOR_CODE_CHAR ) {
		  code_detected = 1;
		}
		else {
		  *iptr++ = uctmp + SET_COLOR_TRANSLATION_CHAR;
		  i_remaining--;
		}
	      }
	    }

	    if (swap_endian) {
	      byteswap((char *)&gray, sizeof(gray));
	    }

	    PaletteSetColor(plot, (double)gray);
	    current_gc = &gc;
	}
    }

    else if (*buffer == X11_GR_BINARY_POLYGON) {	/* filled polygon */
	if (have_pm3d) {	/* ignore, if your X server is not supported */
	    static TBOOLEAN transferring = 0;
	    static unsigned char *iptr;
	    static int int_cache[2];
	    static unsigned int i_remaining;
	    unsigned short i_buffer;
	    char *bptr;
	    static TBOOLEAN code_detected = 0;
	    static XPoint *points = NULL;
	    static int st_npoints = 0;
	    static int npoints = 0, style = 0;

	    /* The first value read will be the number of points or the number of
	     * points followed by style.  Set up parameters to point to npoints.
	     */
	    if (!transferring) {
		iptr = (unsigned char *) int_cache;
		i_remaining = sizeof(int_cache);
	    }

	    i_buffer = BINARY_MAX_CHAR_PER_TRANSFER;

	    /* Decode and reconstruct the data. */
	    for (bptr = &buffer[1]; i_buffer && i_remaining; i_buffer--) {

		unsigned char uctmp = *bptr++;

		if (code_detected) {
		    code_detected = 0;
		    *iptr++ = uctmp - 1 + FILLED_POLYGON_TRANSLATION_CHAR;
		    i_remaining--;
		} else {
		    if ( uctmp == FILLED_POLYGON_CODE_CHAR ) {
			code_detected = 1;
		    } else {
			*iptr++ = uctmp + FILLED_POLYGON_TRANSLATION_CHAR;
			i_remaining--;
		    }
		}

		if(!i_remaining && !transferring) {
		    /* The number of points was just read.  Now set up points array and continue. */
		    if (swap_endian) {
			byteswap((char *)&int_cache[0], sizeof(int));
			byteswap((char *)&int_cache[1], sizeof(int));
		    }
		    npoints = int_cache[0];
		    style = int_cache[1];
		    if (npoints > st_npoints) {
			XPoint *new_points = realloc(points, npoints*2*sizeof(int));
			st_npoints = npoints;
			if (!new_points) {
			    perror("gnuplot_x11: exec_cmd()->points");
			    EXIT(1);
			}
			points = new_points;
		    }
		    i_remaining = npoints*2*sizeof(int);
		    iptr = (unsigned char *) points;
		    transferring = 1;
		}

	    }

	    if (!i_remaining) {

		int i;

		transferring = 0;

		/* If the byte order needs to be swapped, do so. */
		if (swap_endian) {
		    i = 2*npoints;
		    for (i--; i >= 0; i--) {
			byteswap((char *)&((int *)points)[i], sizeof(int));
		    }
		}

		/* Convert the ints to XPoint format. This looks like it tramples
		 * on itself, but the XPoint x and y are smaller than an int.
		 */
		for (i=0; i < npoints; i++) {
		    points[i].x = X( ((int *)points)[2*i] );
		    points[i].y = Y( ((int *)points)[2*i+1] );
		}

		/* Load selected pattern or fill into a separate gc */
		if (!fill_gc)
		    fill_gc = XCreateGC(dpy, plot->window, 0, 0);
		XCopyGC(dpy, *current_gc, ~0, fill_gc);

		x11_setfill(&fill_gc, style);

		XFillPolygon(dpy, plot->pixmap, fill_gc, points, npoints,
			     Nonconvex, CoordModeOrigin);

		/* Toggle mechanism */
		if (x11_in_key_sample) {
		    x11_update_key_box(plot, points[0].x - hchar, points[0].y - vchar/2);
		    x11_update_key_box(plot, points[0].x + hchar, points[0].y + vchar/2);
		}
	    }
	}

    }

    else if (*buffer == X11_GR_IMAGE) {	/* image */

	/* the pointer to the next byte to be written, or NULL if we're not currently transferring data */
	static unsigned char *dest_ptr = NULL;

	static unsigned short *image;
	static int M, N;
	static int pixel_1_1_x, pixel_1_1_y, pixel_M_N_x, pixel_M_N_y;
	static int visual_1_1_x, visual_1_1_y, visual_M_N_x, visual_M_N_y;
	static int color_mode;

	/* How many symbols are left to transfer in this image. There is 1
	   symbol for each channel in each pixel */
	static unsigned int i_remaining;

	/* ignore, if your X server is not supported */
	if (!have_pm3d)
	    return;

/* These might work better as fuctions, but defines will do for now. */
#define ERROR_NOTICE(str)         "\nGNUPLOT (gplt_x11):  " str
#define ERROR_NOTICE_NEWLINE(str) "\n                     " str

	if (dest_ptr == NULL) {

	    /* Get variables. */
	    if (11 != sscanf( &buffer[1], "%x %x %x %x %x %x %x %x %x %x %x", &M, &N, &pixel_1_1_x,
			      &pixel_1_1_y, &pixel_M_N_x, &pixel_M_N_y, &visual_1_1_x, &visual_1_1_y,
			      &visual_M_N_x, &visual_M_N_y, &color_mode)) {
		fprintf(stderr, ERROR_NOTICE("Couldn't read image parameters correctly.\n\n"));
	    } else {

		/* Number of symbols depends upon whether it is color or palette lookup. */
		i_remaining = M*N*sizeof(image[0]);
		if (color_mode == IC_RGB)
		    i_remaining *= 3;
		else if (color_mode == IC_RGBA)
		    i_remaining *= 4;

		if (!i_remaining) {
		    fprintf(stderr, ERROR_NOTICE("Image of size zero.\n\n"));
		} else {
		    image = (unsigned short *) malloc(i_remaining);
		    if (image) {
			dest_ptr = (unsigned char *) image;
		    } else {
			fprintf(stderr, ERROR_NOTICE("Cannot allocate memory for image.\n\n"));
		    }
		}
	    }

	} else {

	    unsigned short i_buffer;
	    char *bptr;
	    static TBOOLEAN code_detected = 0;

	    i_buffer = BINARY_MAX_CHAR_PER_TRANSFER;

	    /* Decode and reconstruct the data. */
	    for (bptr = &buffer[1]; i_buffer && i_remaining; i_buffer--) {
		unsigned char uctmp = *bptr++;
		if (code_detected) {
		    code_detected = 0;
		    *dest_ptr++ = uctmp - 1 + IMAGE_TRANSLATION_CHAR;
		    i_remaining--;
		} else {
		    if (uctmp == IMAGE_CODE_CHAR) {
			code_detected = 1;
		    } else {
			*dest_ptr++ = uctmp + IMAGE_TRANSLATION_CHAR;
			i_remaining--;
		    }
		}
	    }

	    if (!i_remaining) {

		/* Expand or contract the image into a new image and place this on the screen. */

		static unsigned short R_msb_mask=0, R_rshift, R_lshift;
		static unsigned short G_msb_mask, G_rshift, G_lshift;
		static unsigned short B_msb_mask, B_rshift, B_lshift;
		static unsigned long prev_red_mask, prev_green_mask, prev_blue_mask;
#define LET_XPUTPIXEL_SWAP_BYTES 1
#if !LET_XPUTPIXEL_SWAP_BYTES
		static TBOOLEAN swap_image_bytes = FALSE;
#endif
		TBOOLEAN create_image = FALSE;
#define SINGLE_PALETTE_BIT_SHIFT 0  /* Discard over time, 16aug2004 */
#if SINGLE_PALETTE_BIT_SHIFT
		static int prev_allocated=0;
		static short palette_bit_shift=0;
#endif

		dest_ptr = NULL;

		/* If the byte order needs to be swapped, do so. */
		if (swap_endian) {
		    int i = M*N;
		    if (color_mode == IC_RGB)
			i *= 3;
		    else if (color_mode == IC_RGBA)
			i *= 4;
		    for (i--; i >= 0; i--) {
			/* The assumption is that image data through the pipe is 16 bits. */
			byteswap2(&image[i]);
		    }
		}

		if (color_mode == IC_PALETTE) {

#if SINGLE_PALETTE_BIT_SHIFT
/* If the palette size is 2 to some power then a single bit shift would work.
 * The multiply/shift method doesn't seem noticably slower and generally works
 * with any size pallete.  Keep this around for the time being in case some
 * use for it comes up.
 */
		    /* Initialize the palette shift, or if the palette size changes then update the shift. */
		    if (!palette_bit_shift || (plot->cmap->allocated != prev_allocated)) {
			short i_bits, n_colors;
			prev_allocated = plot->cmap->allocated;
			for (palette_bit_shift = 16, n_colors = 1; palette_bit_shift > 0; palette_bit_shift--) {
			    if (n_colors >= plot->cmap->allocated) {break;}
			    n_colors <<= 1;
			}
		    }
#endif

		    create_image = TRUE;
		}
		else {

		    switch( vis->class ) {

			static char *display_error_text_after = " display class cannot use component"
			    ERROR_NOTICE_NEWLINE("data.  Try palette mode.\n\n");

		    case TrueColor:

			/* This algorithm for construction of pixel values from the RGB components
			 * doesn't look to be the most portable.  However, it tries to be.  It
			 * gets the red/green/blue masks from the X11 information and generates
			 * appropriate shifts and masks from that.  The X11 documentation mentions
			 * something about using the masks in some odd way to generate an index
			 * for a color table.  It all seemed like a bother for no benefit however.
			 * Certainly we don't want to have only a few bits of color, and we don't
			 * want to have some huge color table.  My feeling on the matter is to
			 * just pack bits according to the masks that X11 indicates.  If someone
			 * finds they have a peculiar display type that doesn't work, that can be
			 * considered when and if it happens.
			 */

			/* Get the color mask information and compute the proper bit shifts, but only upon
			 * first call or if the masks change.  I'm not sure if it is necessary to check if
			 * the masks changed, but I left it in as a safeguard in case there is some strange
			 * system out there somewhere.
			 */
			if (!R_msb_mask || (vis->red_mask != prev_red_mask) ||
			    (vis->green_mask != prev_green_mask) || (vis->blue_mask != prev_blue_mask)) {
			    short R_bits, G_bits, B_bits, min_bits;
			    prev_red_mask = vis->red_mask;
			    prev_green_mask = vis->green_mask;
			    prev_blue_mask = vis->blue_mask;
			    R_msb_mask = BitMaskDetails(vis->red_mask, &R_rshift, &R_lshift);
			    G_msb_mask = BitMaskDetails(vis->green_mask, &G_rshift, &G_lshift);
			    B_msb_mask = BitMaskDetails(vis->blue_mask, &B_rshift, &B_lshift);

			    /* Graphics info in case strange behavior occurs in a particular system.
			     */
			    FPRINTF((stderr, "\n\nvis->visualid: 0x%x   vis->class: %d   vis->bits_per_rgb: %d\n",
				(int)vis->visualid, vis->class, vis->bits_per_rgb));
			    FPRINTF((stderr, "vis->red_mask: %lx   vis->green_mask: %lx   vis->blue_mask: %lx\n",
				vis->red_mask, vis->green_mask, vis->blue_mask));
			    FPRINTF((stderr, "ImageByteOrder:  %d\n\n", ImageByteOrder(dpy)));

			    R_bits = 32-R_rshift-R_lshift;
			    G_bits = 32-G_rshift-G_lshift;
			    B_bits = 32-B_rshift-B_lshift;
			    min_bits = GPMIN(GPMIN(R_bits, G_bits), B_bits);
			    if (R_bits > min_bits) {
				R_msb_mask >>= (R_bits-min_bits);
				R_lshift += (R_bits-min_bits);
				R_bits = min_bits;
			    }
			    if (G_bits > min_bits) {
				G_msb_mask >>= (G_bits-min_bits);
				G_lshift += (G_bits-min_bits);
				G_bits = min_bits;
			    }
			    if (B_bits > min_bits) {
				B_msb_mask >>= (B_bits-min_bits);
				B_lshift += (B_bits-min_bits);
				B_bits = min_bits;
			    }
			    R_rshift = 16-R_bits;
			    G_rshift = 16-G_bits;
			    B_rshift = 16-B_bits;

#if !LET_XPUTPIXEL_SWAP_BYTES
			    /* Now deal with the byte order issue. */
			    if (ImageByteOrder(dpy)) {
				/* If the bit field for each channel is 8 bits, the masks can be rearranged instead
				 * of having to actually perform a byte order swap on the image data.
				 */
				if ((R_bits == 8) && (G_bits == 8) && (B_bits == 8)) {
				    R_lshift = 24 - R_lshift;
				    G_lshift = 24 - G_lshift;
				    B_lshift = 24 - B_lshift;
				    swap_image_bytes = FALSE;
				} else {
				    swap_image_bytes = TRUE;
				}
			    } else {
				swap_image_bytes = FALSE;
			    }
#endif
			}

			create_image = TRUE;
			break;

		    case PseudoColor:
			fprintf(stderr, ERROR_NOTICE("PseudoColor"));
			fprintf(stderr, "%s", display_error_text_after);
			break;

		    case GrayScale:
			fprintf(stderr, ERROR_NOTICE("GrayScale"));
			fprintf(stderr, "%s", display_error_text_after);
			break;

		    case StaticColor:
			fprintf(stderr, ERROR_NOTICE("StaticColor"));
			fprintf(stderr, "%s", display_error_text_after);
			break;

		    case StaticGray:
			fprintf(stderr, ERROR_NOTICE("StaticGray"));
			fprintf(stderr, "%s", display_error_text_after);
			break;

		    case DirectColor:
			fprintf(stderr, ERROR_NOTICE("DirectColor display class currently")
				ERROR_NOTICE_NEWLINE("not supported.\n\n"));
			break;

		    default:
			fprintf(stderr, ERROR_NOTICE("Unknown X11 display class.\n\n"));
			break;

		    }

		}

		if (create_image) {

		    int M_pixel, N_pixel;
		    int M_view, N_view;
		    int pixel_1_1_x_plot, pixel_1_1_y_plot, pixel_M_N_x_plot, pixel_M_N_y_plot;
		    int view_1_1_x_plot, view_1_1_y_plot, view_M_N_x_plot, view_M_N_y_plot;
		    int final_1_1_x_plot, final_1_1_y_plot;
		    int i_start, j_start;
		    int itmp;

		    /* Compute the image extent with sanity check. */
		    pixel_1_1_x_plot = X(pixel_1_1_x);
		    pixel_M_N_x_plot = X(pixel_M_N_x);
		    M_pixel = pixel_M_N_x_plot - pixel_1_1_x_plot;
		    if (M_pixel < 0) {
			M_pixel = -M_pixel;
			itmp = pixel_1_1_x_plot;
			pixel_1_1_x_plot = pixel_M_N_x_plot;
			pixel_M_N_x_plot = itmp;
		    }
		    pixel_1_1_y_plot = Y(pixel_1_1_y);
		    pixel_M_N_y_plot = Y(pixel_M_N_y);
		    N_pixel = pixel_M_N_y_plot - pixel_1_1_y_plot;
		    if (N_pixel < 0) {
			N_pixel = -N_pixel;
			itmp = pixel_1_1_y_plot;
			pixel_1_1_y_plot = pixel_M_N_y_plot;
			pixel_M_N_y_plot = itmp;
		    }

		    /* Compute the visual extent of the plot with sanity check. */
		    view_1_1_x_plot = X(visual_1_1_x);
		    view_M_N_x_plot = X(visual_M_N_x);
		    if (view_M_N_x_plot < view_1_1_x_plot) {
			itmp = view_1_1_x_plot;
			view_1_1_x_plot = view_M_N_x_plot;
			view_M_N_x_plot = itmp;
		    }
		    view_1_1_y_plot = Y(visual_1_1_y);
		    view_M_N_y_plot = Y(visual_M_N_y);
		    if (view_M_N_y_plot < view_1_1_y_plot) {
			itmp = view_1_1_y_plot;
			view_1_1_y_plot = view_M_N_y_plot;
			view_M_N_y_plot = itmp;
		    }

		    /* Determine parameters for the image that will be built and put on screen. */
		    itmp = view_1_1_x_plot - pixel_1_1_x_plot;
		    if (itmp > 0) {
			i_start = itmp; final_1_1_x_plot = view_1_1_x_plot;
		    } else {
			i_start = 0; final_1_1_x_plot = pixel_1_1_x_plot;
		    }
		    itmp = pixel_M_N_x_plot - view_M_N_x_plot;
		    if (itmp > 0) {
			M_view = M_pixel - itmp - i_start;
		    } else {
			M_view = M_pixel - i_start;
		    }

		    itmp = view_1_1_y_plot - pixel_1_1_y_plot;
		    if (itmp > 0) {
			j_start = itmp; final_1_1_y_plot = view_1_1_y_plot;
		    } else {
			j_start = 0; final_1_1_y_plot = pixel_1_1_y_plot;
		    }
		    itmp = pixel_M_N_y_plot - view_M_N_y_plot;
		    if (itmp > 0) {
			N_view = N_pixel - itmp - j_start;
		    } else {
			N_view = N_pixel - j_start;
		    }

		    if ((M_view > 0) && (N_view > 0)) {

			char *sample_data;
			short sample_data_size;
			int i_view, j_view;

			/* Determine if 2 bytes is sufficient or 4 bytes are necessary for color image data. */
			sample_data_size = (dep > 16) ? 4 : 2;

			/* Expand or compress the original image to the pixels it will occupy on the screen. */
			sample_data = (char *) malloc(M_view*N_view*sample_data_size);

			if (sample_data) {

			    XImage *image_src = NULL;
			    XImage *image_dest;

			    /* Create an initialized image object. */
			    image_dest = XCreateImage(dpy, vis, dep, ZPixmap, 0, sample_data, M_view, N_view,
						      8*sample_data_size, M_view*sample_data_size);
			    if (!image_dest) {
				fputs("gnuplot_x11: can't get memory for image object.\n", stderr);
				EXIT(1);
			    }

			    /* Get the drawable image if using alpha blending. */
			    if (color_mode == IC_RGBA)
				image_src = XGetImage(dpy, plot->pixmap, final_1_1_x_plot, final_1_1_y_plot,
						      M_view, N_view, AllPlanes, ZPixmap);

			    /* Fill in the output image data by decimating or repeating the input image data. */
			    for (j_view=0; j_view < N_view; j_view++) {
				int j = ((j_view+j_start) * N) / N_pixel;
				int row_start = j*M;
				for (i_view=0; i_view < M_view; i_view++) {
				    int i = ((i_view+i_start) * M) / M_pixel;
				    if (color_mode == IC_PALETTE) {

#if SINGLE_PALETTE_BIT_SHIFT
/* If the palette size is 2 to some power then a single bit shift would work.
 * The multiply/shift method doesn't seem noticably slower and generally works
 * with any size pallete.  Keep this around for the time being in case some
 * use for it comes up.
 */
					unsigned long pixel = plot->cmap->pixels[image[row_start + i]>>palette_bit_shift];
#else
					/* The methods below avoid using floating point.  The basic idea is to multiply
					 * the palette size by the "normalized" data coming across the pipe.  (I think
					 * some DSP people call this Q1 or Q0 format, or something like that.)  Following
					 * this multiplication by division by 2^16 gives the final answer.  Most moderately
					 * advanced CPUs have bit shifting.  The first method uses this.  However, a bit
					 * shift isn't necessary because we can just take the top word of a two word
					 * number and we've effectively divided by 2^16.  The second method uses this.
					 * However, my guess is that C compilers more effeciently translate the first
					 * method with the bit shift than the second method which pulls out the top word.
					 * (There is one instruction less for the shift version than the word selection
					 * version on gcc for x86.)
					 */
#if 1
					unsigned long pixel;
					unsigned short index  = (plot->cmap->allocated * (unsigned long) image[row_start + i]) >> 16;
					pixel = plot->cmap->pixels[index];
#else
					unsigned long index;
					unsigned long pixel;
					index  = plot->cmap->allocated * (unsigned long) image[row_start + i];
					pixel = plot->cmap->pixels[((unsigned short *)&index)[1]];
#endif
#endif
					XPutPixel(image_dest, i_view, j_view, pixel);
				    } else if (color_mode == IC_RGB) {
					int index3 = 3*(row_start + i);
					unsigned long pixel = ((unsigned int)((image[index3++]>>R_rshift)&R_msb_mask)) << R_lshift;
					pixel |= ((unsigned int)((image[index3++]>>G_rshift)&G_msb_mask)) << G_lshift;
					pixel |= ((unsigned int)((image[index3]>>B_rshift)&B_msb_mask)) << B_lshift;
					XPutPixel(image_dest, i_view, j_view, pixel);
				    } else {
					/* EAM - I'm confused as to where the scaling happens, but by the time
					 * we arrive here, RGB components run 0-0xffff but A runs 0-0xff
					 */
					int index4 = 4*(row_start + i);
					double alpha = (double)(image[index4 + 3] & 0xff) / 255.;

					/* Decompose existing pixel, i.e., reverse process. */
					unsigned long pixel = XGetPixel(image_src, i_view, j_view);
					unsigned int red = ((pixel>>R_lshift)&R_msb_mask) << R_rshift;
					unsigned int green = ((pixel>>G_lshift)&G_msb_mask) << G_rshift;
					unsigned int blue = ((pixel>>B_lshift)&B_msb_mask) << B_rshift;
					/* Apply alpha blending. */
					red   = (alpha * image[index4++] + (1.-alpha) * red);
					green = (alpha * image[index4++] + (1.-alpha) * green);
					blue  = (alpha * image[index4]   + (1.-alpha) * blue);
					pixel = ((unsigned int)((red>>R_rshift)&R_msb_mask)) << R_lshift;
					pixel |= ((unsigned int)((green>>G_rshift)&G_msb_mask)) << G_lshift;
					pixel |= ((unsigned int)((blue>>B_rshift)&B_msb_mask)) << B_lshift;
					XPutPixel(image_dest, i_view, j_view, pixel);
				    }
				}
			    }

#if !LET_XPUTPIXEL_SWAP_BYTES
			    /* Swap the image byte order if necessary. */
			    if (swap_image_bytes) {
				int i_swap = M_view*N_view - 1;
				if (sample_data_size == 2) {
				    for (; i_swap >= 0; i_swap--) {
					byteswap2(&(((unsigned short *)sample_data)[i_swap]));
				    }
				} else {
				    for (; i_swap >= 0; i_swap--) {
					byteswap4(&(((unsigned int *)sample_data)[i_swap]));
				    }
				}
			    }
#endif

			    /* Copy the image to the drawable d. */
			    XPutImage(dpy, plot->pixmap, gc, image_dest, 0, 0,
				      final_1_1_x_plot, final_1_1_y_plot, M_view, N_view);

			    /* Free resources. */
			    XDestroyImage(image_dest);
			    if (color_mode == IC_RGBA)
				XDestroyImage(image_src);

			    /* XDestroyImage frees the sample_data memory, so no "free" here. */

			} else {
			    fprintf(stderr, ERROR_NOTICE("Could not allocate memory for image.\n\n"));
			}
		    }

		}

		/* image was not used as part of X resource, so must "free" here. */
		free(image);

	    }
	}

    }

#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
    /*   Axis scaling information to save for later mouse clicks */
    else if (*buffer == 'S') {
	int axis, axis_mask;

	sscanf(&buffer[1], "%d %d", &axis, &axis_mask);
	if (axis == -2) {
	    plot->almost2d = axis_mask;
	} else if (axis < 0) {
	    plot->axis_mask = axis_mask;
	} else if (axis < 2*SECOND_AXES) {
	    sscanf(&buffer[1], "%d %lg %d %lg %lg", &axis,
		&(plot->axis_scale[axis].min), &(plot->axis_scale[axis].term_lower),
		&(plot->axis_scale[axis].term_scale), &(plot->axis_scale[axis].logbase));
	    FPRINTF((stderr, "gnuplot_x11: axis %d scaling %16.6lg %14d %16.6lg %16.6lg\n",
		axis, plot->axis_scale[axis].min, plot->axis_scale[axis].term_lower,
		plot->axis_scale[axis].term_scale, plot->axis_scale[axis].logbase));
	}
    }
#endif

    /*   X11_layer(syncpoint) */
    else if (*buffer == 'Y') {
    	int layer;
	sscanf(&buffer[1], "%d", &layer);
	switch (layer)
	{
	case TERM_LAYER_RESET:
	case TERM_LAYER_RESET_PLOTNO:
			x11_cur_plotno = 0;
			break;
	case TERM_LAYER_BEFORE_PLOT:
			x11_cur_plotno++;
			x11_in_plot = TRUE;
			break;
	case TERM_LAYER_AFTER_PLOT:
			x11_in_plot = FALSE;
			break;
	case TERM_LAYER_BEGIN_KEYSAMPLE:
			x11_in_key_sample = TRUE;
			break;
	case TERM_LAYER_END_KEYSAMPLE:
			x11_in_key_sample = FALSE;
			break;
	default:
			break;
	/* These layer commands must be handled on receipt rather than in-line */
	case TERM_LAYER_BEFORE_ZOOM:
			break;
	}
    }

    /*   Switch to a different color map */
    else if (*buffer == 'e') {
	if (have_pm3d) {
	    /* Get colormap index and choose find the appropriate cmap */
	    int cm_index, i;
	    cmap_struct *csp;
	    sscanf(&buffer[1], "%u", &cm_index);
	    csp = plot->first_cmap_struct;
	    for (i=0; i < cm_index; i++)
		csp = csp->next_cmap_struct;
	    plot->cmap = csp->cmap;
	}
    }
    else {
	fprintf(stderr, "gnuplot_x11: unknown command <%s>\n", buffer);
    }
}

/*
 *   display - display a stored plot
 */
static void
display(plot_struct *plot)
{
    int n;

    /* set scaling factor between internal driver & window geometry */
    unsigned int winW = plot->width;
    unsigned int winH = GRAPH_HEIGHT(plot);

    FPRINTF((stderr, "Display %d ; %d commands\n", plot->plot_number, plot->ncommands));

    if (plot->ncommands == 0)
	return;

    if (currently_receiving_gr_image)
	return;

    /* make the plot as large as possible, while preserving the aspect ratio and
       fitting into the window */
    if( winW * gH > gW * winH )
    {
      /* window is too wide; height dominates */
      yscale = xscale = (double)(winH * gW) / (double)(gH * 4096.0);
    }
    else
    {
      /* window is too tall; width dominates */
      yscale = xscale = (double)winW / 4096.0;
    }

    /* initial point sizes, until overridden with P7xxxxyyyy */
    plot->px = (int) (xscale * pointsize);
    plot->py = (int) (yscale * pointsize);

    /* create new pixmap & GC */
    if (plot->pixmap == None) {
	FPRINTF((stderr, "Create pixmap %d : %dx%dx%d\n", plot->plot_number, plot->width, PIXMAP_HEIGHT(plot), dep));
	plot->pixmap = XCreatePixmap(dpy, root, plot->width, PIXMAP_HEIGHT(plot), dep);
    }

    if (gc)
	XFreeGC(dpy, gc);

    gc = XCreateGC(dpy, plot->pixmap, 0, (XGCValues *) 0);

    if (font)
      gpXSetFont(dpy, gc, font->fid);

    XSetFillStyle(dpy, gc, FillSolid);

    /* initialize stipple for filled boxes (ULIG) */
    if (!stipple_initialized) {
	int i;
	for (i = 0; i < stipple_pattern_num; i++)
	    stipple_pattern[i] =
		XCreateBitmapFromData(dpy, plot->pixmap, stipple_pattern_bits[i],
				stipple_pattern_width, stipple_pattern_height);
	stipple_dots = XCreateBitmapFromData(dpy, plot->pixmap, stipple_pattern_dots,
				stipple_pattern_width, stipple_pattern_height);
	stipple_initialized = 1;
    }

    /* set pixmap background */
    XSetForeground(dpy, gc, plot->cmap->colors[0]);
    XFillRectangle(dpy, plot->pixmap, gc, 0, 0, plot->width, PIXMAP_HEIGHT(plot) + vchar);
    XSetBackground(dpy, gc, plot->cmap->colors[0]);

    if (plot->window == None)
	pr_window(plot);

    /* top the window but don't put keyboard or mouse focus into it. */
    if (do_raise)
	XMapRaised(dpy, plot->window);

    /* momentarily clear the window first if requested */
    if (Clear) {
	XClearWindow(dpy, plot->window);
	XFlush(dpy);
    }

    /* Initialize toggle in keybox mechanism */
    x11_cur_plotno = 0;
    x11_in_key_sample = FALSE;
    x11_initialize_key_boxes(plot, 0);

    /* Maintain on/off toggle state when zooming */
    if (retain_toggle_state)
	retain_toggle_state = FALSE;
    else
	x11_initialize_hidden(plot, 0);

    /* loop over accumulated commands from inboard driver */
    for (n = 0; n < plot->ncommands; n++) {
	exec_cmd(plot, plot->commands[n]);
    }

#ifdef EXPORT_SELECTION
    if (exportselection)
	export_graph(plot);
#endif

    UpdateWindow(plot);
#ifdef USE_MOUSE
#ifdef TITLE_BAR_DRAWING_MSG
    if (plot->window) {
	/* restore default window title */
	char *cp = plot->titlestring;
	if (!cp) cp = "";
	gpXStoreName(dpy, plot->window, cp);
    }
#else
    if (!button_pressed) {
	cursor = cursor_save ? cursor_save : cursor_default;
	cursor_save = (Cursor)0;
	XDefineCursor(dpy, plot->window, cursor);
    }
#endif
#endif
}

static void
UpdateWindow(plot_struct * plot)
{
#ifdef USE_MOUSE
    XEvent event;
#endif
/* CRUFT CHECK 17jul2006.  I added a "None == plot->pixmap"
 * to this test because I believe it may not be necessary
 * to do anything if there is no Pixmap yet.
 */
    if (None == plot->window || None == plot->pixmap) {
	return;
    }

    XSetWindowBackgroundPixmap(dpy, plot->window, plot->pixmap);
    XClearWindow(dpy, plot->window);

#ifdef USE_MOUSE
    EventuallyDrawMouseAddOns(plot);

    XFlush(dpy);

    /* XXX discard expose events. This is a kludge for
     * preventing the event dispatcher calling UpdateWindow()
     * and the latter again generating expose events, which
     * again would trigger the event dispatcher ... (joze) XXX */
    while (XCheckWindowEvent(dpy, plot->window, ExposureMask, &event))
	/* EMPTY */ ;
#endif
}


static void
CmapClear(cmap_t * cmap_ptr)
{
    cmap_ptr->total = (int) 0;
    cmap_ptr->allocated = (int) 0;
    cmap_ptr->pixels = (unsigned long *) 0;
}

static void
RecolorWindow(plot_struct * plot)
{
    cmap_struct **cspp = &plot->first_cmap_struct;
    cmap_struct *csp = plot->first_cmap_struct;
    while (csp) {
	if (csp->cmap == plot->cmap)
	    break;
	cspp = &(csp->next_cmap_struct);
	csp = csp->next_cmap_struct;
    }
    if (!csp) {
	/* This colormap is not in the list, add it. */
	*cspp = (cmap_struct *) malloc(sizeof(cmap_struct));
	if (*cspp) {
	    /* Initialize structure variables. */
	    memset((void*)*cspp, 0, sizeof(cmap_struct));
	    (*cspp)->cmap = plot->cmap;
	}
	else {
	    fprintf(stderr, ERROR_NOTICE("Could not allocate memory for cmap_struct.\n\n"));
	    return;
	}
    }
    if (None != plot->window) {
	XSetWindowColormap(dpy, plot->window, plot->cmap->colormap);
	XSetWindowBackground(dpy, plot->window, plot->cmap->colors[0]);
	XSetWindowBorder(dpy, plot->window, plot->cmap->colors[1]);
#ifdef USE_MOUSE
	if (gc_xor) {
	    XFreeGC(dpy, gc_xor);
	    gc_xor = (GC) 0;
	    GetGCXor(plot, &gc_xor);	/* recreate gc_xor */
	}
#endif
    }
}

static void
FreeColormapList(plot_struct *plot)
{
    while (plot->first_cmap_struct != NULL) {
	cmap_struct *freethis = plot->first_cmap_struct;
	/* Release the colormap here to free color resources, but only
	 * if this plot is using a colormap not used by another plot
	 * and is not using the current colormap.
	 */
	if (plot->first_cmap_struct->cmap != current_cmap && !Find_Plot_In_Linked_List_By_CMap(plot->first_cmap_struct->cmap))
	    Remove_CMap_From_Linked_List(plot->first_cmap_struct->cmap);
	plot->first_cmap_struct = plot->first_cmap_struct->next_cmap_struct;
	free(freethis);
    }
}

/*
 * free all *pm3d* colors (*not* the line colors cmap->colors)
 * of a plot_struct's colormap.  This could be either a private
 * or the default colormap.  Note, that the line colors are not
 * free'd nor even touched.
 */
static void
FreeColors(cmap_t *cmp)
{
    if (cmp->total && cmp->pixels) {
	if (cmp->allocated) {
	    FPRINTF((stderr, "freeing palette colors\n"));
	    XFreeColors(dpy, cmp->colormap, cmp->pixels,
			cmp->allocated, 0 /* XXX ??? XXX */ );
	}
	free(cmp->pixels);
    }
    CmapClear(cmp);
}

/*
 * free pm3d colors and eventually a private colormap.
 * set the plot_struct's colormap to the default colormap
 * and the line `colors' to the line colors of the default
 * colormap.
 */
static void
ReleaseColormap(cmap_t *cmp)
{
    if (cmp && cmp != current_cmap) {
	FreeColors(cmp);
	FPRINTF((stderr, "releasing private colormap\n"));
	if (cmp->colormap && cmp->colormap != current_cmap->colormap) {
	    XFreeColormap(dpy, cmp->colormap);
	}
	free((char *) cmp);
    }
}

static unsigned long *
ReallocColors(cmap_t *cmap, int n)
{
    FreeColors(cmap);
    cmap->total = n;
    cmap->pixels = (unsigned long *)
	malloc(sizeof(unsigned long) * cmap->total);
    return cmap->pixels;
}

/*
 * check if the display supports the visual of type `class'.
 *
 * If multiple visuals of `class' are supported, try to get
 * the one with the highest depth.
 *
 * If visual class and depth are equal to the default visual
 * class and depth, the latter is preferred.
 *
 * modifies: best, depth
 *
 * returns 1 if a visual which matches the request
 * could be found else 0.
 */
static int
GetVisual(int class, Visual ** visual, int *depth)
{
    XVisualInfo *visualsavailable;
    int nvisuals = 0;
    long vinfo_mask = VisualClassMask;
    XVisualInfo vinfo;

    vinfo.class = class;
    *depth = 0;
    *visual = 0;

    visualsavailable = XGetVisualInfo(dpy, vinfo_mask, &vinfo, &nvisuals);

    if (visualsavailable && nvisuals > 0) {
	int i;
	for (i = 0; i < nvisuals; i++) {
	    if (visualsavailable[i].depth > *depth) {
		*visual = visualsavailable[i].visual;
		*depth = visualsavailable[i].depth;
	    }
	}
	XFree(visualsavailable);
	if (*visual && (*visual)->class == (DefaultVisual(dpy, scr))->class && *depth == DefaultDepth(dpy, scr)) {
	    /* prefer the default visual */
	    *visual = DefaultVisual(dpy, scr);
	}
    }
    return nvisuals > 0;
}

static void
PaletteMake(t_sm_palette * tpal)
{
    int max_colors;
    int min_colors;
#ifdef TITLE_BAR_DRAWING_MSG
    char *save_title = (char *) 0;
#endif

    /* The information retained in a linked list is the cmap_t structure.
     * That colormap structure doesn't contain the palette specifications
     * t_sm_palette used to generate the colormap.  Therefore, the making
     * of the colormap must be carried all the way through before we can
     * determine if it is unique from the other colormaps in the linked list.
     *
     * Rather than create and initialize a colormap outside of the list, we'll
     * just put a new one in the list, and if it isn't unique remove it later.
     */

    cmap_t *new_cmap = Add_CMap_To_Linked_List();

    /* Continue until valid palette is built.  May require multiple passes. */
    while (1) {

	if (tpal) {

	    if (tpal->use_maxcolors > 0) {
		max_colors = tpal->use_maxcolors;
	    } else {
		max_colors = maximal_possible_colors;
	    }

	    FPRINTF((stderr, "(PaletteMake) tpal->use_maxcolors = %d\n",
		     tpal->use_maxcolors));

	    /*  free old gradient table  */
	    if (sm_palette.gradient) {
		free( sm_palette.gradient );
		sm_palette.gradient = NULL;
		sm_palette.gradient_num = 0;
	    }

	    sm_palette.colorMode = tpal->colorMode;
	    sm_palette.positive = tpal->positive;
	    sm_palette.use_maxcolors = tpal->use_maxcolors;
	    sm_palette.cmodel = tpal->cmodel;

	    switch( sm_palette.colorMode ) {
	    case SMPAL_COLOR_MODE_GRAY:
		sm_palette.gamma = tpal->gamma;
		break;
	    case SMPAL_COLOR_MODE_RGB:
		sm_palette.formulaR = tpal->formulaR;
		sm_palette.formulaG = tpal->formulaG;
		sm_palette.formulaB = tpal->formulaB;
		break;
	    case SMPAL_COLOR_MODE_FUNCTIONS:
		fprintf( stderr, "Ooops:  no SMPAL_COLOR_MODE_FUNCTIONS here!\n" );
		break;
	    case SMPAL_COLOR_MODE_GRADIENT:
		sm_palette.gradient_num = tpal->gradient_num;
		/* Take over the memory from tpal. */
		sm_palette.gradient = tpal->gradient;
		tpal->gradient = NULL;
		break;
	    default:
		fprintf(stderr,"%s:%d ooops: Unknown color mode '%c'.\n",
			__FILE__, __LINE__, (char)(sm_palette.colorMode) );
	    }

	} else {
	    max_colors = maximal_possible_colors;
	    FPRINTF((stderr, "(PaletteMake) tpal=NULL\n"));
	}

	if (minimal_possible_colors < max_colors)
	    min_colors = minimal_possible_colors;
	else
	    min_colors = max_colors / (num_colormaps > 1 ? 2 : 8);

	if (current_plot) {

#ifdef TITLE_BAR_DRAWING_MSG
	    if (current_plot->window) {
		char *msg;
		char *added_text = " allocating colors ...";
		int orig_len = (current_plot->titlestring ? strlen(current_plot->titlestring) : 0);
		XFetchName(dpy, current_plot->window, &save_title);
		if ((msg = (char *) malloc(orig_len + strlen(added_text) + 1))) {
		    if (current_plot->titlestring)
			strcpy(msg, current_plot->titlestring);
		    else
			msg[0] = '\0';
		    strcat(msg, added_text);
		    gpXStoreName(dpy, current_plot->window, msg);
		    free(msg);
		}
	    }
#endif

	    if (!num_colormaps) {
		XFree(XListInstalledColormaps(dpy, current_plot->window, &num_colormaps));
		FPRINTF((stderr, "(PaletteMake) num_colormaps = %d\n", num_colormaps));
	    }

	}

	/* TODO */
	/* EventuallyChangeVisual(plot); */

	/*
	 * start with trying to allocate max_colors. This should
	 * always succeed with TrueColor visuals >= 16bit. If it
	 * fails (for example for a PseudoColor visual of depth 8),
	 * try it with half of the colors. Proceed until min_colors
	 * is reached. If this fails we should probably install a
	 * private colormap.
	 * Note that I make no difference for different
	 * visual types here. (joze)
	 */
	for ( /* EMPTY */ ; max_colors >= min_colors; max_colors /= 2) {

	    XColor xcolor;
	    double fact = 1.0 / (double)(max_colors-1);

	    if (current_plot)
		ReallocColors(new_cmap, max_colors);

	    for (new_cmap->allocated = 0; new_cmap->allocated < max_colors; new_cmap->allocated++) {

		double gray = (double) new_cmap->allocated * fact;
		rgb_color color;
		rgb1_from_gray( gray, &color );
		xcolor.red = 0xffff * color.r + 0.5;
		xcolor.green = 0xffff * color.g + 0.5;
		xcolor.blue = 0xffff * color.b + 0.5;

		if (XAllocColor(dpy, new_cmap->colormap, &xcolor)) {
		    new_cmap->pixels[new_cmap->allocated] = xcolor.pixel;
		} else {
		    FPRINTF((stderr, "failed at color %d\n", new_cmap->allocated));
		    break;
		}
	    }

	    if (new_cmap->allocated == max_colors) {
		break;		/* success! */
	    }

	    /* reduce the number of max_colors to at
	     * least less than new_cmap->allocated */
	    while (max_colors > new_cmap->allocated && max_colors >= min_colors) {
		max_colors /= 2;
	    }
	}

	FPRINTF((stderr, "(PaletteMake) allocated = %d\n", new_cmap->allocated));
	FPRINTF((stderr, "(PaletteMake) max_colors = %d\n", max_colors));
	FPRINTF((stderr, "(PaletteMake) min_colors = %d\n", min_colors));

	if (new_cmap->allocated < min_colors && tpal) {
	    /* create a private colormap on second pass. */
	    FPRINTF((stderr, "switching to private colormap\n"));
	    tpal = 0;
	} else {
	    break;
	}
    }

    /* Now check the uniqueness of the new colormap against all the other
     * colormaps in the linked list.
     */
    {
	cmap_t *cmp = cmap_list_start;

	while (cmp != NULL) {
	    if ((cmp != new_cmap) && !cmaps_differ(cmp, new_cmap))
		break;
	    cmp = cmp->next_cmap;
	}

	if (cmp != NULL) {
	    /* Found a match. Discard the newly created colormap.  (Could
	     * have simply discarded the old one and combined parts of
	     * code similar to these if statements, but we'll avoid removing
	     * old blocks of memory so that it is more likely that heap
	     * memory will remain more tidy.
	     */
	    Remove_CMap_From_Linked_List(new_cmap);
	    if (current_plot) {
		current_plot->cmap = cmp;
		RecolorWindow(current_plot);
	    }
	    current_cmap = cmp;
	} else {
	    /* Unique and truely new colormap.  Make it the current map. */
	    if (current_plot) {
		current_plot->cmap = new_cmap;
		RecolorWindow(current_plot);
	    }
	    /* If no other plots using colormap, then can be removed. */
	    if (!Find_Plot_In_Linked_List_By_CMap(current_cmap))
		Remove_CMap_From_Linked_List(current_cmap);
	    current_cmap = new_cmap;
	}

    }

#ifdef TITLE_BAR_DRAWING_MSG
    if (save_title) {
	/* Restore window title (current_plot and current_plot->window are
	 * valid, otherwise would not have been able to get save_title.
	 */
	gpXStoreName(dpy, current_plot->window, save_title);
	XFree(save_title);
    }
#endif

}

static void
PaletteSetColor(plot_struct * plot, double gray)
{
    if (plot->cmap->allocated) {
	int index;

	/* FIXME  -  I don't understand why sm_palette is not always in sync	*/
	/* with plot->cmap->allocated, but in practice they can be different.	*/
	if (sm_palette.use_maxcolors == plot->cmap->allocated) {
	    gray = quantize_gray(gray);
	    FPRINTF((stderr," %d",plot->cmap->allocated));
	} else
	    gray = floor(gray * plot->cmap->allocated) / (plot->cmap->allocated - 1);

	index = gray * (plot->cmap->allocated - 1);
	if (index >= plot->cmap->allocated)
		index = plot->cmap->allocated -1;

	XSetForeground(dpy, gc, plot->cmap->pixels[index]);
	plot->current_rgb = plot->cmap->pixels[index];
    }
}

#ifdef USE_MOUSE

/* Notify main program, send windowid for GPVAL_TERM_WINDOWID if it has been changed. */
static void
gp_execute_GE_plotdone (int windowid)
{
    static int last_window_id = -1;
    if (windowid == last_window_id)
	gp_exec_event(GE_plotdone, 0, 0, 0, 0, 0);
    else {
	gp_exec_event(GE_plotdone, 0, 0, 0, 0, windowid);
	last_window_id = windowid;
    }
}

static int
ErrorHandler(Display * display, XErrorEvent * error_event)
{
    /* Don't remove directly.  Main program might be using the memory. */
    (void) display;		/* avoid -Wunused warnings */
    Add_Plot_To_Remove_FIFO_Queue((Window) error_event->resourceid);
    gp_exec_event(GE_reset, 0, 0, 0, 0, 0);
    return 0;
}

static void
DrawRuler(plot_struct * plot)
{
    if (plot->ruler_on) {
	int x = X(plot->ruler_x);
	int y = Y(plot->ruler_y);
	if (!gc_xor) {
	    /* create a gc for `rubberbanding' (well ...) */
	    GetGCXor(plot, &gc_xor);
	}
	/* vertical line */
	XDrawLine(dpy, plot->window, gc_xor, x, 0, x, GRAPH_HEIGHT(plot));
	/* horizontal line */
	XDrawLine(dpy, plot->window, gc_xor, 0, y, plot->width, y);
    }
}

static void
EventuallyDrawMouseAddOns(plot_struct * plot)
{
    DrawRuler(plot);
    DrawLineToRuler(plot);
    if (plot->zoombox_on)
	DrawBox(plot);
    DrawCoords(plot, plot->str);
    /*
       TODO more ...
     */
}


/*
 * draw a line using the gc with the GXxor function.
 * This can be used to turn on *and off* a line between
 * the current mouse pointer and the ruler.
 */
static void
DrawLineToRuler(plot_struct * plot)
{
    if (plot->ruler_on == FALSE || plot->ruler_lineto_on == FALSE)
	return;
    if (plot->ruler_lineto_x < 0)
	return;
    if (!gc_xor) {
	GetGCXor(plot, &gc_xor);
    }
    XDrawLine(dpy, plot->window, gc_xor, 
	    X(plot->ruler_x), Y(plot->ruler_y),
	    plot->ruler_lineto_x, plot->ruler_lineto_y);
}




/*
 * draw a box using the gc with the GXxor function.
 * This can be used to turn on *and off* a box. The
 * corners of the box are annotated with the strings
 * stored in the plot structure.
 */
static void
DrawBox(plot_struct * plot)
{
    int width;
    int height;
    int X0 = plot->zoombox_x1;
    int Y0 = plot->zoombox_y1;
    int X1 = plot->zoombox_x2;
    int Y1 = plot->zoombox_y2;

    if (!gc_xor_dashed) {
	GetGCXorDashed(plot, &gc_xor_dashed);
    }

    if (X1 < X0) {
	int tmp = X1;
	X1 = X0;
	X0 = tmp;
    }

    if (Y1 < Y0) {
	int tmp = Y1;
	Y1 = Y0;
	Y0 = tmp;
    }

    width = X1 - X0;
    height = Y1 - Y0;

    XDrawRectangle(dpy, plot->window, gc_xor_dashed, X0, Y0, width, height);

    if (plot->zoombox_str1a[0] || plot->zoombox_str1b[0])
	AnnotatePoint(plot, plot->zoombox_x1, plot->zoombox_y1, plot->zoombox_str1a, plot->zoombox_str1b);
    if (plot->zoombox_str2a[0] || plot->zoombox_str2b[0])
	AnnotatePoint(plot, plot->zoombox_x2, plot->zoombox_y2, plot->zoombox_str2a, plot->zoombox_str2b);
}


/*
 * draw the strings xstr and ystr centered horizontally
 * and vertically at the point x, y. Use the GXxor
 * as usually, so that we can also remove the coords
 * later.
 */
static void
AnnotatePoint(plot_struct * plot, int x, int y, const char xstr[], const char ystr[])
{
    int xlen = strlen(xstr);
    int ylen = strlen(ystr);

    /* int xwidth = gpXTextWidth(font, xstr, xlen); */
    /* int ywidth = gpXTextWidth(font, ystr, ylen); */

    if (!gc_xor) {
	GetGCXor(plot, &gc_xor);
    }
    gpXDrawString(dpy, plot->window, gc_xor, x, y - 3, xstr, xlen);
    gpXDrawString(dpy, plot->window, gc_xor, x, y + vchar, ystr, ylen);
}

/* returns the time difference to the last click in milliseconds */
static long int
SetTime(plot_struct *plot, Time t)
{
    long int diff = t - plot->time;

    FPRINTF((stderr, "(SetTime) difftime = %ld\n", diff));

    plot->time = t;
    return diff > 0 ? diff : 0;
}

static unsigned long
AllocateXorPixel(cmap_t *cmap_ptr)
{
    unsigned long pixel;
    XColor xcolor;

    xcolor.pixel = cmap_ptr->colors[0];	/* background color */
    XQueryColor(dpy, cmap_ptr->colormap, &xcolor);

    if (xcolor.red + xcolor.green + xcolor.blue < 0xffff) {
	/* it is admittedly somehow arbitrary to call
	 * everything with a gray value < 0xffff a
	 * dark background. Try to use the background's
	 * complement for drawing which will always
	 * result in white when using xor. */
	xcolor.red = ~xcolor.red;
	xcolor.green = ~xcolor.green;
	xcolor.blue = ~xcolor.blue;
	if (XAllocColor(dpy, cmap_ptr->colormap, &xcolor)) {
	    /* use white foreground for dark backgrounds */
	    pixel = xcolor.pixel;
	} else {
	    /* simple xor if we've run out of colors. */
	    pixel = WhitePixel(dpy, scr);
	}
    } else {
	/* use the background itself for drawing.
	 * xoring two same colors will always result
	 * in black. This color is already allocated. */
	pixel = xcolor.pixel;
    }
    cmap_ptr->xorpixel = pixel;
    return pixel;
}

static void
GetGCXor(plot_struct * plot, GC * ret)
{
    XGCValues values;
    unsigned long mask = 0;

    values.foreground = AllocateXorPixel(plot->cmap);

#ifdef USE_X11_MULTIBYTE
    if (usemultibyte) {
	mask = GCForeground | GCFunction;
	values.function = GXxor;
    } else
#endif
    {
	mask = GCForeground | GCFunction | GCFont;
	values.function = GXxor;
	values.font = font->fid;
    }

    *ret = XCreateGC(dpy, plot->window, mask, &values);
}

static void
GetGCXorDashed(plot_struct * plot, GC * gc)
{
    GetGCXor(plot, gc);
    XSetLineAttributes(dpy, *gc, 0,	/* line width, X11 treats 0 as a `thin' line */
		       LineOnOffDash,	/* also: LineDoubleDash */
		       CapNotLast,	/* also: CapButt, CapRound, CapProjecting */
		       JoinMiter /* also: JoinRound, JoinBevel */ );
}


/* erase the last displayed position string */
static void
EraseCoords(plot_struct * plot)
{
    DrawCoords(plot, plot->str);
}


static void
DrawCoords(plot_struct * plot, const char *str)
{
    if (!gc_xor) {
	GetGCXor(plot, &gc_xor);
    }

    if (str[0] != 0)
	gpXDrawString(dpy, plot->window, gc_xor, 1, plot->gheight + vchar - 1, str, strlen(str));
}


/* display text (e.g. mouse position) in the lower left corner of the window. */
static void
DisplayCoords(plot_struct * plot, const char *s)
{
    /* first, erase old text */
    EraseCoords(plot);

    if (s[0] == 0) {
	/* no new text? */
	if (plot->height > plot->gheight) {
	    /* and window has space for text? then make it smaller, unless we're already doing a resize: */
	    if (!plot->resizing) {
#ifdef EXTERNAL_X11_WINDOW
		if (plot->external_container != None) {
		    plot->gheight = plot->height;
		    display(plot);
		}
		else
#endif
		XResizeWindow(dpy, plot->window, plot->width, plot->gheight);
		plot->resizing = TRUE;
	    }
	}
    } else {
	/* so we do have new text */
	if (plot->height == plot->gheight) {
	    /* window not large enough? then make it larger, unless we're already doing a resize: */
	    if (!plot->resizing) {
#ifdef EXTERNAL_X11_WINDOW
		if (plot->external_container != None) {
		    plot->gheight = plot->height - vchar;
		    display(plot);
		} else
#endif
		XResizeWindow(dpy, plot->window, plot->width, plot->gheight + vchar);
		plot->resizing = TRUE;
	    }
	}
    }

    /* finally, draw the new text: */
    DrawCoords(plot, s);

    /* and save it, for later erasing: */
    strcpy(plot->str, s);
}


static TBOOLEAN
is_meta(KeySym mod)
{
    /* CJK keyboards may have these meta keys */
    if (0xFF20 <= mod && mod <= 0xFF3F)
	return TRUE;

    /* Everyone else may have these meta keys */
    switch (mod) {
	case XK_Multi_key:
	case XK_Shift_L:
	case XK_Shift_R:
	case XK_Control_L:
	case XK_Control_R:
	case XK_Meta_L:
	case XK_Meta_R:
	case XK_Alt_L:
	case XK_Alt_R:
			return TRUE;
	default:
			return FALSE;
    }
}

#ifndef DISABLE_SPACE_RAISES_CONSOLE
/* It returns NULL if we are not running in any known (=implemented) multitab
 * console.
 * Otherwise it returns a command to be executed in order to switch to the
 * appropriate tab of a multitab console.
 * In addition, it may return non-zero newGnuplotXID in order to overwrite zero
 * value of gnuplotXID (because Konsole's in KDE <3.2 don't set WINDOWID contrary
 * to all other xterm's).
 * Currently implemented for:
 *	- KDE3 Konsole.
 * Note: if the returned command is !NULL, then it must be free()'d by the caller.
 */
static char*
getMultiTabConsoleSwitchCommand(unsigned long *newGnuplotXID)
{
/* NOTE: This code uses the DCOP mechanism from KDE3, which went away in KDE4 */
#ifdef HAVE_STRDUP	/* We assume that any machine missing strdup is too old for KDE */
    char *cmd = NULL; /* result */
    char *ptr = getenv("KONSOLE_DCOP_SESSION"); /* Try KDE's Konsole first. */
    *newGnuplotXID = 0;
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
	*newGnuplotXID = 0; /* don't change gnuplotXID by default */
	/* use 'while' instead of 'if' to easily break out (aka catch exception) */
	while (1) {
	    char *konsole_tab;
	    unsigned long w;
	    FILE *p;
	    ptr = strchr(ptr, '(');
	    /* the string for tab nb 4 looks like 'DCOPRef(konsole-2866, session-4)' */
	    if (!ptr) return NULL;
	    konsole_name = strdup(ptr+1);
	    konsole_tab = strchr(konsole_name, ',');
	    if (!konsole_tab) break;
	    *konsole_tab++ = 0;
	    ptr = strchr(konsole_tab, ')');
	    if (ptr) *ptr = 0;
	    /* Not necessary to define DCOP_RAISE: returning newly known
	     * newGnuplotXID instead is sufficient.
	     */
/* #define DCOP_RAISE */
#ifdef DCOP_RAISE
	    cmd = malloc(2*strlen(konsole_name) + strlen(konsole_tab) + 128);
#else
	    cmd = malloc(strlen(konsole_name) + strlen(konsole_tab) + 64);
#endif
	    sprintf(cmd, "dcop %s konsole-mainwindow#1 getWinID 2>/dev/null", konsole_name);
		/* is  2>/dev/null  portable among various shells? */
	    p = popen(cmd, "r");
	    if (p) {
		fscanf(p, "%lu", &w);
		pclose(p);
	    }
	    if (gnuplotXID) { /* $WINDOWID is known */
		if (w != gnuplotXID) break;
		    /* `dcop getWinID`==$WINDOWID thus we are running in a window detached from Konsole */
	    } else {
		*newGnuplotXID = w;
		    /* $WINDOWID has not been known (KDE 3.1), thus set it up */
	    }
#ifdef DCOP_RAISE
	    /* not necessary: returning newly known newGnuplotXID instead is sufficient */
	    sprintf(cmd, "dcop %s konsole-mainwindow#1 raise;", konsole_name);
	    sprintf(cmd+strlen(cmd), "dcop %s konsole activateSession %s", konsole_name, konsole_tab);
#else
	    sprintf(cmd, "dcop %s konsole activateSession %s", konsole_name, konsole_tab);
#endif
	    free(konsole_name);
	    return cmd;
	}
	free(konsole_name);
	free(cmd);
	return NULL;
    }
    /* now test for GNOME multitab console */
    /* ... if somebody bothers to implement it ... */

#endif /* HAVE_STRDUP */
/* NOTE: End of DCOP/KDE3 code (no longer works in KDE4) */
    /* we are not running in any known (implemented) multitab console */
    return NULL;
}

#endif
#endif	/* DISABLE_SPACE_RAISES_CONSOLE */


/*---------------------------------------------------------------------------
 *  reset all cursors (since we dont have a record of the previous terminal #)
 *---------------------------------------------------------------------------*/

static void
reset_cursor()
{
    plot_struct *plot = plot_list_start;

    while (plot) {
	if (plot->window) {
	    FPRINTF((stderr, "Window for plot %d exists\n", plot->plot_number));
	    XUndefineCursor(dpy, plot->window);
	}
	plot = plot->next_plot;
    }

    FPRINTF((stderr, "Cursors reset\n"));
    return;
}

/*-----------------------------------------------------------------------------
 *   resize - rescale last plot if window resized
 *---------------------------------------------------------------------------*/

#ifdef USE_MOUSE

/* the status of the shift, ctrl and alt keys
*/
static int modifier_mask = 0;

static void update_modifiers __PROTO((unsigned int));

static void
update_modifiers(unsigned int state)
{
    int old_mod_mask;

    old_mod_mask = modifier_mask;
    modifier_mask = ((state & ShiftMask) ? Mod_Shift : 0)
	| ((state & ControlMask) ? Mod_Ctrl : 0)
	| ((state & Mod1Mask) ? Mod_Alt : 0);
    if (old_mod_mask != modifier_mask) {
	gp_exec_event(GE_modifier, 0, 0, modifier_mask, 0, 0);
    }
}

#endif

static void
process_configure_notify_event(XEvent *event, TBOOLEAN isRetry )
{
    plot_struct *plot;
    int force_redraw = 0;

    /* Filter down to the last ConfigureNotify event */
    XSync(dpy, False);
    while (XCheckTypedWindowEvent(dpy, event->xany.window, ConfigureNotify, event))
	;

    plot = Find_Plot_In_Linked_List_By_Window(event->xconfigure.window);
    if (plot) {
	int w = event->xconfigure.width, h = event->xconfigure.height;

	/* store settings in case window is closed then recreated */
	/* but: don't do this if both x and y are 0, since some
	 * (all?) systems set these to zero when only resizing
	 * (not moving) the window. This does mean that a move to
	 * (0, 0) won't be registered: can we solve that? */
	if (event->xconfigure.x != 0 || event->xconfigure.y != 0) {
	    plot->x = event->xconfigure.x;
	    plot->y = event->xconfigure.y;
	    plot->posn_flags = (plot->posn_flags & ~PPosition) | USPosition;
	}
#ifdef USE_MOUSE
	/* first, check whether we were waiting for completion of a resize */
	if (plot->resizing) {
	    /* it seems to be impossible to distinguish between a
	     * resize caused by our call to XResizeWindow(), and a
	     * resize started by the user/windowmanager; but we can
	     * make a good guess which can only fail if the user
	     * resizes the window while we're also resizing it
	     * ourselves: */
	    if (w == plot->width
	    && (h == plot->gheight || h == plot->gheight + vchar)) {
		/* most likely, it's a resize for showing/hiding the status line.
		 * Test whether the height is now correct; if not, start another resize. */
		if (w == plot->width
		&& h == plot->gheight + (plot->str[0] ? vchar : 0)) {
			/* Was successful, status line can be drawn without rescaling plot. */
			plot->height = h;
			plot->resizing = FALSE;
			return;
#ifdef HAVE_USLEEP
		} else if( !isRetry ) {
			/* Possibly, a resize attempt _failed_ because window
			   manager denied it. Some window managers produce extra
			   ConfigureNotify events at the start (for instance the
			   notion window manager). Thus it's possible that the
			   event we're looking at is one of the initial ones,
			   NOT a response to our XResizeWindow() call. We wait a
			   bit to see if a more likely response follows, and we
			   try again */
			usleep( 100000 );
			process_configure_notify_event( event, TRUE );
			return;
#endif
		} else {
			/* Possibly, a resize attempt _failed_ because window manager denied it.
			   Resizing again goes into a vicious endless loop!
			   (Seen with fluxbox-1.0.0 and a tab group of gnuplot windows.) */
			/* Instead of just appending the status line, redraw/scale the plot. */
			force_redraw = TRUE;
			plot->resizing = FALSE;
		}
	    }
	}
#endif

	if (w > 1 && h > 1 && (force_redraw || w != plot->width || h != plot->height)) {

	    plot->width = w;
	    plot->height = h;
#ifdef USE_MOUSE
	    /* Make sure that unsigned number doesn't underflow. */
	    if (plot->str[0])
		plot->gheight = (vchar > plot->height) ? 0 : plot->height - vchar;
	    else
		plot->gheight = plot->height;
#endif
	    plot->posn_flags = (plot->posn_flags & ~PSize) | USSize;

	    if (stipple_initialized) {
		int i;
		for (i = 0; i < stipple_pattern_num; i++)
			XFreePixmap(dpy, stipple_pattern[i]);
		XFreePixmap(dpy, stipple_dots);
		stipple_initialized = 0;
	    }

	    if (plot->pixmap) {
		/* it is the wrong size now */
		FPRINTF((stderr, "Free pixmap %d\n", 0));
		XFreePixmap(dpy, plot->pixmap);
		plot->pixmap = None;
	    }

#ifdef EXTERNAL_X11_WINDOW
	    if (plot->external_container != None) {
		/* Resize so that all parts of plot remain visible
		 * when the plot is expanded. */
		XResizeWindow(dpy, plot->window,
			      plot->width, plot->height);
		/* This may be redundant because the application might
		 * handle resizing the external window.  However, this
		 * probably isn't necessarily true, so resize the
		 * external window as well. */
		XResizeWindow(dpy, plot->external_container,
			      plot->width, plot->height);
	    }
#endif

		/* Don't replot if we're replotting-on-window-resizes, since replotting
		   happens elsewhere in those cases. If the inboard driver is dead, and
		   the window is still around with -persist, replot also. */
#ifdef PIPE_IPC
		if ((replot_on_resize != yes) || pipe_died)
			display(plot);
#else
		if (replot_on_resize != yes)
			display(plot);
#endif

#ifdef USE_MOUSE
	    {
	    /* the window was resized. Send the sizing information back to
	       inboard gnuplot. I send -plot->width to indicate that this sizing
	       information shouldn't be used yet, just saved for a future
	       replot */
	      double scale = (double)plot->width / 4096.0;
	      int scaled_hchar = (1.0/scale) * hchar;
	      int scaled_vchar = (1.0/scale) * vchar;

	      gp_exec_event(GE_fontprops, -plot->width, plot->gheight,
			    scaled_hchar, scaled_vchar, 0);

	      if (replot_on_resize == yes)
		gp_exec_event(GE_replot, 0, 0, 0, 0, 0);
	    }
#endif
	}
    }
}

static void
process_event(XEvent *event)
{
    plot_struct *plot;
    KeySym keysym;
    char key_sequence[8];

    FPRINTF((stderr, "Event 0x%x\n", event->type));

    switch (event->type) {
    case ConfigureNotify:
	process_configure_notify_event(event, FALSE);
	break;

    case KeyPress:
	plot = Find_Plot_In_Linked_List_By_Window(event->xkey.window);

	/* Unlike XKeycodeToKeysym, XLookupString applies the current */
	/* shift, ctrl, alt, and meta modifiers to yield a character. */
	/* keysym = XKeycodeToKeysym(dpy, event->xkey.keycode, 0); */
	XLookupString((XKeyEvent *)event, key_sequence, sizeof(key_sequence), &keysym, NULL);

#ifdef USE_MOUSE
	update_modifiers(event->xkey.state);
#endif
	    switch (keysym) {
#ifdef USE_MOUSE

#ifndef DISABLE_SPACE_RAISES_CONSOLE
	    case ' ': {
		static int cmd_tried = 0;
		static char *cmd = NULL;
		static unsigned long newGnuplotXID = 0;
		
		/* If the "-ctrlq" resource is set, ignore ' ' unless control key is also pressed */
		if (ctrlq && !(modifier_mask & Mod_Ctrl))
		    break;

		if (!cmd_tried) {
		    cmd = getMultiTabConsoleSwitchCommand(&newGnuplotXID);
		    cmd_tried = 1;
		}
		/* overwrite gnuplotXID (re)set after x11.trm:X11_options() */
		if (newGnuplotXID) gnuplotXID = newGnuplotXID;
		if (cmd) system(cmd);
		}
		if (gnuplotXID) {
		    XMapRaised(dpy, gnuplotXID);
		    XSetInputFocus(dpy, gnuplotXID, 0 /*revert */ , CurrentTime);
		    XFlush(dpy);
		}
		return;
#endif	/* DISABLE_SPACE_RAISES_CONSOLE */

	    case 'm': /* Toggle mouse display, but only if we control the window here */
		if (((plot != current_plot) && (!modifier_mask))
#ifdef PIPE_IPC
		    || pipe_died
#endif
		   ) {
		    plot->mouse_on = !(plot->mouse_on);
		    DisplayCoords(plot, plot->mouse_on ? " " : "");
		}
		break;
#endif
	    case 'q':
#ifdef USE_MOUSE
		/* If the "-ctrlq" resource is set, ignore q unless control key is also pressed */
		if (ctrlq && !(modifier_mask & Mod_Ctrl)) {
		    FPRINTF((stderr, "ignoring q, modifier_mask = %o\n", modifier_mask));
		    break;
		}
#endif
		/* close X window */
		Remove_Plot_From_Linked_List(event->xkey.window);
		return;
	    default:
		break;
	    }			/* switch (keysym) */
#ifdef USE_MOUSE

	if (is_meta(keysym))
	    return;

	switch (keysym) {

#define KNOWN_KEYSYMS(gp_keysym)                                     \
	if (plot == current_plot) {                                  \
	    gp_exec_event(GE_keypress,                               \
		(int)RevX(event->xkey.x), (int)RevY(event->xkey.y),  \
		gp_keysym, 0, plot->plot_number);                    \
	} else {                                                     \
	    gp_exec_event(GE_keypress_old,                              \
		(int)RevX(event->xkey.x), (int)RevY(event->xkey.y),  \
		gp_keysym, 0, plot->plot_number);                    \
	}                                                            \
	return;

/* Prevent hysteresis if redraw cannot keep up with rate of keystrokes */
#define DRAIN_KEYSTROKES(key)					\
	if (plot == current_plot) {				\
	    while (XCheckTypedWindowEvent(dpy, 			\
		    event->xany.window, KeyPress, event));	\
	}

	case XK_BackSpace:
	    KNOWN_KEYSYMS(GP_BackSpace);
	case XK_Tab:
	    KNOWN_KEYSYMS(GP_Tab);
	case XK_Linefeed:
	    KNOWN_KEYSYMS(GP_Linefeed);
	case XK_Clear:
	    KNOWN_KEYSYMS(GP_Clear);
	case XK_Return:
	    KNOWN_KEYSYMS(GP_Return);
	case XK_Pause:
	    KNOWN_KEYSYMS(GP_Pause);
	case XK_Scroll_Lock:
	    KNOWN_KEYSYMS(GP_Scroll_Lock);
#ifdef XK_Sys_Req
	case XK_Sys_Req:
	    KNOWN_KEYSYMS(GP_Sys_Req);
#endif
	case XK_Escape:
	    KNOWN_KEYSYMS(GP_Escape);
	case XK_Insert:
	    KNOWN_KEYSYMS(GP_Insert);
	case XK_Delete:
	    KNOWN_KEYSYMS(GP_Delete);
	case XK_Home:
	    KNOWN_KEYSYMS(GP_Home);
	case XK_Left:
	    DRAIN_KEYSTROKES(XK_Left);
	    KNOWN_KEYSYMS(GP_Left);
	case XK_Up:
	    DRAIN_KEYSTROKES(XK_Up);
	    KNOWN_KEYSYMS(GP_Up);
	case XK_Right:
	    DRAIN_KEYSTROKES(XK_Right);
	    KNOWN_KEYSYMS(GP_Right);
	case XK_Down:
	    DRAIN_KEYSTROKES(XK_Down);
	    KNOWN_KEYSYMS(GP_Down);
	case XK_Prior:		/* XXX */
	    KNOWN_KEYSYMS(GP_PageUp);
	case XK_Next:		/* XXX */
	    KNOWN_KEYSYMS(GP_PageDown);
	case XK_End:
	    KNOWN_KEYSYMS(GP_End);
	case XK_Begin:
	    KNOWN_KEYSYMS(GP_Begin);
	case XK_KP_Space:
	    KNOWN_KEYSYMS(GP_KP_Space);
	case XK_KP_Tab:
	    KNOWN_KEYSYMS(GP_KP_Tab);
	case XK_KP_Enter:
	    KNOWN_KEYSYMS(GP_KP_Enter);
	case XK_KP_F1:
	    KNOWN_KEYSYMS(GP_KP_F1);
	case XK_KP_F2:
	    KNOWN_KEYSYMS(GP_KP_F2);
	case XK_KP_F3:
	    KNOWN_KEYSYMS(GP_KP_F3);
	case XK_KP_F4:
	    KNOWN_KEYSYMS(GP_KP_F4);
#ifdef XK_KP_Home
	case XK_KP_Home:
	    KNOWN_KEYSYMS(GP_KP_Home);
#endif
#ifdef XK_KP_Left
	case XK_KP_Left:
	    KNOWN_KEYSYMS(GP_KP_Left);
#endif
#ifdef XK_KP_Up
	case XK_KP_Up:
	    KNOWN_KEYSYMS(GP_KP_Up);
#endif
#ifdef XK_KP_Right
	case XK_KP_Right:
	    KNOWN_KEYSYMS(GP_KP_Right);
#endif
#ifdef XK_KP_Down
	case XK_KP_Down:
	    KNOWN_KEYSYMS(GP_KP_Down);
#endif
#ifdef XK_KP_Page_Up
	case XK_KP_Page_Up:
	    KNOWN_KEYSYMS(GP_KP_Page_Up);
#endif
#ifdef XK_KP_Page_Down
	case XK_KP_Page_Down:
	    KNOWN_KEYSYMS(GP_KP_Page_Down);
#endif
#ifdef XK_KP_End
	case XK_KP_End:
	    KNOWN_KEYSYMS(GP_KP_End);
#endif
#ifdef XK_KP_Begin
	case XK_KP_Begin:
	    KNOWN_KEYSYMS(GP_KP_Begin);
#endif
#ifdef XK_KP_Insert
	case XK_KP_Insert:
	    KNOWN_KEYSYMS(GP_KP_Insert);
#endif
#ifdef XK_KP_Delete
	case XK_KP_Delete:
	    KNOWN_KEYSYMS(GP_KP_Delete);
#endif
	case XK_KP_Equal:
	    KNOWN_KEYSYMS(GP_KP_Equal);
	case XK_KP_Multiply:
	    KNOWN_KEYSYMS(GP_KP_Multiply);
	case XK_KP_Add:
	    KNOWN_KEYSYMS(GP_KP_Add);
	case XK_KP_Separator:
	    KNOWN_KEYSYMS(GP_KP_Separator);
	case XK_KP_Subtract:
	    KNOWN_KEYSYMS(GP_KP_Subtract);
	case XK_KP_Decimal:
	    KNOWN_KEYSYMS(GP_KP_Decimal);
	case XK_KP_Divide:
	    KNOWN_KEYSYMS(GP_KP_Divide);

	case XK_KP_0:
	    KNOWN_KEYSYMS(GP_KP_0);
	case XK_KP_1:
	    KNOWN_KEYSYMS(GP_KP_1);
	case XK_KP_2:
	    KNOWN_KEYSYMS(GP_KP_2);
	case XK_KP_3:
	    KNOWN_KEYSYMS(GP_KP_3);
	case XK_KP_4:
	    KNOWN_KEYSYMS(GP_KP_4);
	case XK_KP_5:
	    KNOWN_KEYSYMS(GP_KP_5);
	case XK_KP_6:
	    KNOWN_KEYSYMS(GP_KP_6);
	case XK_KP_7:
	    KNOWN_KEYSYMS(GP_KP_7);
	case XK_KP_8:
	    KNOWN_KEYSYMS(GP_KP_8);
	case XK_KP_9:
	    KNOWN_KEYSYMS(GP_KP_9);

	case XK_F1:
	    KNOWN_KEYSYMS(GP_F1);
	case XK_F2:
	    KNOWN_KEYSYMS(GP_F2);
	case XK_F3:
	    KNOWN_KEYSYMS(GP_F3);
	case XK_F4:
	    KNOWN_KEYSYMS(GP_F4);
	case XK_F5:
	    KNOWN_KEYSYMS(GP_F5);
	case XK_F6:
	    KNOWN_KEYSYMS(GP_F6);
	case XK_F7:
	    KNOWN_KEYSYMS(GP_F7);
	case XK_F8:
	    KNOWN_KEYSYMS(GP_F8);
	case XK_F9:
	    KNOWN_KEYSYMS(GP_F9);
	case XK_F10:
	    KNOWN_KEYSYMS(GP_F10);
	case XK_F11:
	    KNOWN_KEYSYMS(GP_F11);
	case XK_F12:
	    KNOWN_KEYSYMS(GP_F12);

	default:
	    KNOWN_KEYSYMS((int) keysym);
	    break;
	}
#endif
	break;
    case KeyRelease:
#ifdef USE_MOUSE
	update_modifiers(event->xkey.state);
	/* keysym = XKeycodeToKeysym(dpy, event->xkey.keycode, 0); */
	keysym = XkbKeycodeToKeysym(dpy, event->xkey.keycode, 0, 0);
	if (is_meta(keysym)) {
	    plot = Find_Plot_In_Linked_List_By_Window(event->xkey.window);
	    cursor = cursor_default;
	    if (plot)
		XDefineCursor(dpy, plot->window, cursor);
	}
#endif
	break;

    case ClientMessage:
	if (event->xclient.message_type == WM_PROTOCOLS &&
	    event->xclient.format == 32 && event->xclient.data.l[0] == WM_DELETE_WINDOW) {
	    Remove_Plot_From_Linked_List(event->xclient.window);
	    /* EAM FIXME - may have to generate GE_reset event here also */
	}
	break;

#ifdef USE_MOUSE
    case DestroyNotify:
	plot = Find_Plot_In_Linked_List_By_Window(event->xconfigure.window);
	if (plot == current_plot) {
	    gp_exec_event(GE_reset, 0, 0, 0, 0, 0);
	}
	break;

    case Expose:
	/*
	 * we need to handle expose events here, because
	 * there might stuff like rulers which has to
	 * be redrawn. Well. It's not really hard to handle
	 * this. Note that the x and y fields are not used
	 * to update the pointer pos because the pointer
	 * might be on another window which generates the
	 * expose events. (joze)
	 */

	/* Check for any ConfigureNotify events further down in the X11
	 * event queue. If one is found, handle it first and let the
	 * expose event that is generated be handled later.
	 * Jay Painter Nov 2003.
	 */
	if (XCheckTypedWindowEvent(dpy, event->xany.window, ConfigureNotify, event)) {
	    process_configure_notify_event(event, FALSE);
	    break;
	}
	while (XCheckTypedWindowEvent(dpy, event->xany.window, Expose, event));

	plot = Find_Plot_In_Linked_List_By_Window(event->xexpose.window);

	if (plot)
	    UpdateWindow(plot);
	break;

    case EnterNotify:
	plot = Find_Plot_In_Linked_List_By_Window(event->xcrossing.window);
	if (!plot)
	    break;
	if (plot == current_plot) {
	    Call_display(plot);
	    gp_exec_event(GE_motion, (int) RevX(event->xcrossing.x), (int) RevY(event->xcrossing.y), 0, 0, 0);
	}
	if (plot->zoombox_on) {
	    DrawBox(plot);
	    plot->zoombox_x2 = event->xcrossing.x;
	    plot->zoombox_y2 = event->xcrossing.y;
	    DrawBox(plot);
	}
	if (plot->ruler_on) {
	    DrawLineToRuler(plot);
	    plot->ruler_lineto_x = event->xcrossing.x;
	    plot->ruler_lineto_y = event->xcrossing.y;
	    DrawLineToRuler(plot);
	}
	break;
    case MotionNotify:
	update_modifiers(event->xmotion.state);
	plot = Find_Plot_In_Linked_List_By_Window(event->xmotion.window);
	if (!plot)
	    break;
	{
	    Window root, child;
	    int root_x, root_y, pos_x, pos_y;
	    unsigned int keys_buttons;
	    if (!XQueryPointer(dpy, event->xmotion.window, &root, &child, &root_x, &root_y, &pos_x, &pos_y, &keys_buttons))
		break;

	    if (plot == current_plot
#ifdef PIPE_IPC
		&& !pipe_died
#endif
	       ) {
		Call_display(plot);
		gp_exec_event(GE_motion, (int) RevX(pos_x), (int) RevY(pos_y), 0, 0, 0);
#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
	    } else if (plot->axis_mask && plot->mouse_on && plot->almost2d) {
		/* This is not the active plot window, but we can still update the mouse coords */
		char mouse_format[60];
		char *m = mouse_format;
		double x, y, x2, y2;

		*m = '\0';
		mouse_to_coords(plot, event, &x, &y, &x2, &y2);
		if (plot->axis_mask & (1<<FIRST_X_AXIS)) {
		    sprintf(m, "x=  %10g %c", x, '\0');
		    m += 15;
		}
		if (plot->axis_mask & (1<<SECOND_X_AXIS)) {
		    sprintf(m, "x2= %10g %c", x2, '\0');
		    m += 15;
		}
		if (plot->axis_mask & (1<<FIRST_Y_AXIS)) {
		    sprintf(m, "y=  %10g %c", y, '\0');
		    m += 15;
		}
		if (plot->axis_mask & (1<<SECOND_Y_AXIS)) {
		    sprintf(m, "y2= %10g %c", y2, '\0');
		    m += 15;
		}
		DisplayCoords(plot, mouse_format);
	    } else if (!plot->mouse_on) {
		DisplayCoords(plot, "");
#endif
	    }

	    if (plot->zoombox_on) {
		DrawBox(plot);
		plot->zoombox_x2 = pos_x;
		plot->zoombox_y2 = pos_y;
		DrawBox(plot);
	    }
	    if (plot->ruler_on && plot->ruler_lineto_on) {
		DrawLineToRuler(plot);
		plot->ruler_lineto_x = event->xcrossing.x;
		plot->ruler_lineto_y = event->xcrossing.y;
		DrawLineToRuler(plot);
	    }
	}
	break;
    case ButtonPress:
	update_modifiers(event->xbutton.state);
#ifndef TITLE_BAR_DRAWING_MSG
	button_pressed |= (1 << event->xbutton.button);
#endif
	plot = Find_Plot_In_Linked_List_By_Window(event->xbutton.window);
	if (!plot)
	    break;

	/* Toggle mechanism */
	/* Note: we can toggle even if the plot is not in the active window */
	if (event->xbutton.button == 1) {
	    if (x11_check_for_toggle(plot, event->xbutton.x, event->xbutton.y)) {
		retain_toggle_state = TRUE;
		display(plot);
	    }
	}

	if (plot == current_plot) {
	    Call_display(plot);
	    gp_exec_event(GE_buttonpress, (int) RevX(event->xbutton.x), (int) RevY(event->xbutton.y), event->xbutton.button, 0, 0);
	}
	break;
    case ButtonRelease:
#ifndef TITLE_BAR_DRAWING_MSG
	button_pressed &= ~(1 << event->xbutton.button);
#endif
	plot = Find_Plot_In_Linked_List_By_Window(event->xbutton.window);
	if (!plot)
	    break;
	if (plot == current_plot) {

	    long int doubleclick = SetTime(plot, event->xbutton.time);

	    update_modifiers(event->xbutton.state);
	    Call_display(plot);
	    gp_exec_event(GE_buttonrelease,
			  (int) RevX(event->xbutton.x), (int) RevY(event->xbutton.y),
			  event->xbutton.button, (int) doubleclick, 0);
	}

#ifdef DEBUG
#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
	/* This causes gnuplot_x11 to pass mouse clicks back from all plot windows,
	 * not just the current plot. But who should we notify that a click has
	 * happened, and how?  The fprintf to stderr is just for debugging. */
	else if (plot->axis_mask) {
	    double x, y, x2, y2;
	    mouse_to_coords(plot, event, &x, &y, &x2, &y2);
	    fprintf(stderr, "gnuplot_x11 %d: mouse button %1d from window %d at %g %g\n",
		    __LINE__, event->xbutton.button, (plot ? plot->plot_number : 0), x, y);
	}
#endif
#endif

	break;
#endif /* USE_MOUSE */
#ifdef EXPORT_SELECTION
    case SelectionNotify:
	break;
    case SelectionRequest:
	if (exportselection)
	    handle_selection_event(event);
	break;
#endif

    }
}

/*-----------------------------------------------------------------------------
 *   preset - determine options, open display, create window
 *---------------------------------------------------------------------------*/
/*
#define On(v) ( !strcmp(v, "on") || !strcmp(v, "true") || \
                !strcmp(v, "On") || !strcmp(v, "True") || \
                !strcmp(v, "ON") || !strcmp(v, "TRUE") )
*/
#define On(v) ( !strncasecmp(v, "on", 2) || !strncasecmp(v, "true", 4) )

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

static XrmDatabase dbCmd, dbApp, dbDef, dbEnv, db = (XrmDatabase) 0;

static char *type[20];
static XrmValue value;

static XrmOptionDescRec options[] = {
    {"-mono", ".mono", XrmoptionNoArg, (XPointer) "on"},
    {"-gray", ".gray", XrmoptionNoArg, (XPointer) "on"},
    {"-clear", ".clear", XrmoptionNoArg, (XPointer) "on"},
    {"-tvtwm", ".tvtwm", XrmoptionNoArg, (XPointer) "on"},
    {"-pointsize", ".pointsize", XrmoptionSepArg, (XPointer) NULL},
    {"-display", ".display", XrmoptionSepArg, (XPointer) NULL},
    {"-name", ".name", XrmoptionSepArg, (XPointer) NULL},
    {"-geometry", "*geometry", XrmoptionSepArg, (XPointer) NULL},
    {"-background", "*background", XrmoptionSepArg, (XPointer) NULL},
    {"-bg", "*background", XrmoptionSepArg, (XPointer) NULL},
    {"-foreground", "*foreground", XrmoptionSepArg, (XPointer) NULL},
    {"-fg", "*foreground", XrmoptionSepArg, (XPointer) NULL},
    {"-bordercolor", "*bordercolor", XrmoptionSepArg, (XPointer) NULL},
    {"-bd", "*bordercolor", XrmoptionSepArg, (XPointer) NULL},
    {"-borderwidth", ".borderWidth", XrmoptionSepArg, (XPointer) NULL},
    {"-bw", ".borderWidth", XrmoptionSepArg, (XPointer) NULL},
    {"-font", "*font", XrmoptionSepArg, (XPointer) NULL},
    {"-fn", "*font", XrmoptionSepArg, (XPointer) NULL},
    {"-reverse", "*reverseVideo", XrmoptionNoArg, (XPointer) "on"},
    {"-rv", "*reverseVideo", XrmoptionNoArg, (XPointer) "on"},
    {"+rv", "*reverseVideo", XrmoptionNoArg, (XPointer) "off"},
    {"-iconic", "*iconic", XrmoptionNoArg, (XPointer) "on"},
    {"-synchronous", "*synchronous", XrmoptionNoArg, (XPointer) "on"},
    {"-xnllanguage", "*xnllanguage", XrmoptionSepArg, (XPointer) NULL},
    {"-selectionTimeout", "*selectionTimeout", XrmoptionSepArg, (XPointer) NULL},
    {"-title", ".title", XrmoptionSepArg, (XPointer) NULL},
    {"-xrm", NULL, XrmoptionResArg, (XPointer) NULL},
    {"-raise", "*raise", XrmoptionNoArg, (XPointer) "on"},
    {"-noraise", "*raise", XrmoptionNoArg, (XPointer) "off"},
    {"-replotonresize", "*replotonresize", XrmoptionNoArg, (XPointer) "on"},
    {"-noreplotonresize", "*replotonresize", XrmoptionNoArg, (XPointer) "off"},
    {"-feedback", "*feedback", XrmoptionNoArg, (XPointer) "on"},
    {"-nofeedback", "*feedback", XrmoptionNoArg, (XPointer) "off"},
    {"-ctrlq", "*ctrlq", XrmoptionNoArg, (XPointer) "on"},
    {"-dashed", "*dashed", XrmoptionNoArg, (XPointer) "on"},
    {"-solid", "*dashed", XrmoptionNoArg, (XPointer) "off"},
    {"-persist", "*persist", XrmoptionNoArg, (XPointer) "on"}
};

#define Nopt (sizeof(options) / sizeof(options[0]))

static void
preset(int argc, char *argv[])
{
    int Argc = argc;
    char **Argv = argv;

#ifdef VMS
    char *ldisplay = (char *) 0;
#else
    char *ldisplay = getenv("DISPLAY");
#endif
    char *home = getenv("HOME");
    char *server_defaults, *env, buffer[256];
#if 0
    Visual *TrueColor_vis, *PseudoColor_vis, *StaticGray_vis, *GrayScale_vis;
    int TrueColor_depth, PseudoColor_depth, StaticGray_depth, GrayScale_depth;
#endif
    char *db_string;

    FPRINTF((stderr, "(preset) \n"));

    /* avoid bus error when env vars are not set */
    if (ldisplay == NULL)
	ldisplay = "";
    if (home == NULL)
	home = "";

/*---set to ignore ^C and ^Z----------------------------------------------*/

    signal(SIGINT, SIG_IGN);
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN);
#endif

/*---prescan arguments for "-name"  or "--version"  ----------------------*/

    while (++Argv, --Argc > 0) {
	if (!strcmp(*Argv, "-name") && Argc > 1) {
	    strncpy(X_Name, Argv[1], sizeof(X_Name) - 1);
	    strncpy(X_Class, Argv[1], sizeof(X_Class) - 1);
	    /* just in case */
	    X_Name[sizeof(X_Name) - 1] = NUL;
	    X_Class[sizeof(X_Class) - 1] = NUL;
	    if (X_Class[0] >= 'a' && X_Class[0] <= 'z')
		X_Class[0] -= 0x20;
	}
	if (!strcmp(*Argv, "--version")) {
	    printf("gnuplot %s patchlevel %s\n",
		    gnuplot_version, gnuplot_patchlevel);
	    exit(1);
	}
    }
    Argc = argc;
    Argv = argv;

/*---parse command line---------------------------------------------------*/

    XrmInitialize();
    XrmParseCommand(&dbCmd, options, Nopt, X_Name, &Argc, Argv);
    if (Argc > 1) {
#ifdef PIPE_IPC
	if (!strcmp(Argv[1], "-noevents")) {
	    pipe_died = 1;
	} else {
#endif
	    fprintf(stderr, "\n\
gnuplot: bad option: %s\n\
gnuplot: X11 aborted.\n", Argv[1]);
	    EXIT(1);
#ifdef PIPE_IPC
	}
#endif
    }
    if (pr_GetR(dbCmd, ".display"))
	ldisplay = (char *) value.addr;

/*---open display---------------------------------------------------------*/

#ifdef USE_MOUSE
    XSetErrorHandler(ErrorHandler);
#endif
    dpy = XOpenDisplay(ldisplay);
    if (!dpy) {
	fprintf(stderr, "\n\
gnuplot: unable to open display '%s'\n\
gnuplot: X11 aborted.\n", ldisplay);
	EXIT(1);
    }
    scr = DefaultScreen(dpy);
    root = DefaultRootWindow(dpy);
    server_defaults = XResourceManagerString(dpy);
    vis = DefaultVisual(dpy, scr);
    dep = DefaultDepth(dpy, scr);
    default_cmap.colormap = DefaultColormap(dpy, scr);
    current_cmap = &default_cmap;
    max_request_size = XMaxRequestSize(dpy) / 2;


/**** atoms we will need later ****/

    WM_PROTOCOLS = XInternAtom(dpy, "WM_PROTOCOLS", False);
    WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);


/*---get application defaults--(subset of Xt processing)------------------*/

#ifdef VMS
    strcpy(buffer, "DECW$USER_DEFAULTS:GNUPLOT_X11.INI");
#elif defined OS2
/* for XFree86 ... */
    {
	char appdefdir[MAXPATHLEN];
	strncpy(appdefdir,
	        __XOS2RedirRoot("/XFree86/lib/X11/app-defaults"),
	        sizeof(appdefdir));
	sprintf(buffer, "%s/%s", appdefdir, "Gnuplot");
    }
#else /* !OS/2 */
    {
    char *appdefdir;
	if ((appdefdir = getenv("XAPPLRESDIR")) == NULL) {
#ifdef XAPPLRESDIR
    	    strcpy(buffer, XAPPLRESDIR);
    	    strcat(buffer, "/");
    	    strcat(buffer, "Gnuplot");
#else
	    *buffer = '\0';
#endif
	} else {
    	    strcpy(buffer, appdefdir);
    	    strcat(buffer, "/");
    	    strcat(buffer, "Gnuplot");
	}
    }
#endif /* !VMS */

    dbApp = XrmGetFileDatabase(buffer);
    XrmMergeDatabases(dbApp, &db);

/*---get server or ~/.Xdefaults-------------------------------------------*/

    if (server_defaults)
	dbDef = XrmGetStringDatabase(server_defaults);
    else {
#ifdef VMS
	strcpy(buffer, "DECW$USER_DEFAULTS:DECW$XDEFAULTS.DAT");
#else
	strcpy(buffer, home);
	strcat(buffer, "/.Xdefaults");
#endif
	dbDef = XrmGetFileDatabase(buffer);
    }
    XrmMergeDatabases(dbDef, &db);

/*---get XENVIRONMENT or  ~/.Xdefaults-hostname---------------------------*/

#ifndef VMS
    if ((env = getenv("XENVIRONMENT")) != NULL)
	dbEnv = XrmGetFileDatabase(env);
    else {
	char *p = NULL, host[MAXHOSTNAMELEN];

	if (GP_SYSTEMINFO(host) < 0) {
	    fprintf(stderr, "gnuplot: %s failed. X11 aborted.\n", SYSINFO_METHOD);
	    EXIT(1);
	}
	if ((p = strchr(host, '.')) != NULL)
	    *p = '\0';
	strcpy(buffer, home);
	strcat(buffer, "/.Xdefaults-");
	strcat(buffer, host);
	dbEnv = XrmGetFileDatabase(buffer);
    }
    XrmMergeDatabases(dbEnv, &db);
#endif /* not VMS */

/*---merge command line options-------------------------------------------*/

    XrmMergeDatabases(dbCmd, &db);

/*---set geometry, font, colors, line widths, dash styles, point size-----*/

    /* a specific visual can be forced by the X resource visual */
    db_string = pr_GetR(db, ".visual") ? (char *) value.addr : (char *) 0;
    if (db_string) {
	Visual *visual = (Visual *) 0;
	int depth = (int) 0;
	char **ptr = visual_name;
	int i;
	for (i = 0; *ptr; i++, ptr++) {
	    if (!strcmp(db_string, *ptr)) {
#if 0
		if (DirectColor == i) {
		    fprintf(stderr, "DirectColor not supported by pm3d, using default.\n");
		} else
#endif
		if (GetVisual(i, &visual, &depth)) {
		    vis = visual;
		    dep = depth;
		    if (vis != DefaultVisual(dpy, scr)) {
			/* this will be the default colormap */
			default_cmap.colormap = XCreateColormap(dpy, root, vis, AllocNone);
		    }
		} else {
		    fprintf(stderr, "%s not supported by %s, using default.\n", *ptr, ldisplay);
		}
		break;
	    }
	}
    }
#if 0
    if (DirectColor == vis->class) {
	have_pm3d = 0;
    }
#endif
    CmapClear(&default_cmap);

    /* set default of maximal_possible_colors */
    if (dep > 12) {
	maximal_possible_colors = 0x200;
    } else if (dep > 8) {
	maximal_possible_colors = 0x100;
    } else {
	/* will be something like PseudoColor * 8 */
	maximal_possible_colors = 240;	/* leave 16 for line colors */
    }

    /* check database for maxcolors */
    db_string = pr_GetR(db, ".maxcolors") ? (char *) value.addr : (char *) 0;
    if (db_string) {
	int itmp;
	if (sscanf(db_string, "%d", &itmp)) {
	    if (itmp <= 0) {
		fprintf(stderr, "\nmaxcolors must be strictly positive.\n");
	    } else if (itmp > pow((double) 2, (double) dep)) {
		fprintf(stderr, "\noops, cannot use this many colors on a %d bit deep display.\n", dep);
	    } else {
		maximal_possible_colors = itmp;
	    }
	} else {
	    fprintf(stderr, "\nunable to parse '%s' as integer\n", db_string);
	}
    }

    /* setting a default for minimal_possible_colors */
    minimal_possible_colors = maximal_possible_colors / (num_colormaps > 1 ? 2 : 8);	/* 0x20 / 30 */
    /* check database for mincolors */
    db_string = pr_GetR(db, ".mincolors") ? (char *) value.addr : (char *) 0;
    if (db_string) {
	int itmp;
	if (sscanf(db_string, "%d", &itmp)) {
	    if (itmp <= 0) {
		fprintf(stderr, "\nmincolors must be strictly positive.\n");
	    } else if (itmp > pow((double) 2, (double) dep)) {
		fprintf(stderr, "\noops, cannot use this many colors on a %d bit deep display.\n", dep);
	    } else if (itmp > maximal_possible_colors) {
		fprintf(stderr, "\nmincolors must be <= %d\n", maximal_possible_colors);
	    } else {
		minimal_possible_colors = itmp;
	    }
	} else {
	    fprintf(stderr, "\nunable to parse '%s' as integer\n", db_string);
	}
    }

    pr_geometry(NULL);
    pr_encoding();		/* check for global default encoding */
    pr_font(NULL);		/* set current font to default font */
    pr_color(&default_cmap);	/* set colors for default colormap */
    pr_width();
    pr_dashes();
    pr_pointsize();
    pr_raise();
    pr_replotonresize();
    pr_persist();
    pr_feedback();
    pr_ctrlq();
    pr_fastrotate();
#ifdef EXPORT_SELECTION
    pr_exportselection();
#endif

}

/*-----------------------------------------------------------------------------
 *   pr_GetR - get resource from database using "-name" option (if any)
 *---------------------------------------------------------------------------*/

static char *
pr_GetR(XrmDatabase xrdb, char *resource)
{
    char name[128], class[128], *rc;

    strcpy(name, X_Name);
    strcat(name, resource);
    strcpy(class, X_Class);
    strcat(class, resource);
    rc = XrmGetResource(xrdb, name, class, type, &value)
	? (char *) value.addr : (char *) 0;
    return (rc);
}

/*-----------------------------------------------------------------------------
 *   pr_color - determine color values
 *---------------------------------------------------------------------------*/

static const char color_keys[Ncolors][30] = {
    "background", "bordercolor", "text", "border", "axis",
    "line1", "line2", "line3", "line4",
    "line5", "line6", "line7", "line8"
};
static char color_values[Ncolors][30] = {
    "white", "black", "black", "black", "black",
    "red", "green", "blue", "magenta",
    "cyan", "sienna", "orange", "coral"
};
static char color_values_rv[Ncolors][30] = {
    "black", "white", "white", "white", "white",
    "red", "green", "blue", "magenta",
    "cyan", "sienna", "orange", "coral"
};
static char gray_values[Ncolors][30] = {
    "black", "white", "white", "gray50", "gray50",
    "gray100", "gray60", "gray80", "gray40",
    "gray90", "gray50", "gray70", "gray30"
};

static void
pr_color(cmap_t * cmap_ptr)
{
    unsigned long black = BlackPixel(dpy, scr), white = WhitePixel(dpy, scr);
    char option[20], color[30], *v, *ctype;
    XColor xcolor;
    double intensity = -1;
    int n;

    if (pr_GetR(db, ".mono") && On(value.addr))
	Mono++;
    if (pr_GetR(db, ".gray") && On(value.addr))
	Gray++;
    if (pr_GetR(db, ".reverseVideo") && On(value.addr))
	Rv++;

    if (!Gray && (vis->class == GrayScale || vis->class == StaticGray))
	Mono++;

    if (!Mono) {

	ctype = (Gray) ? "Gray" : "Color";

	if (current_cmap != cmap_ptr) {
	    /* for private colormaps: make sure
	     * that pixel 0 gets black (joze) */
	    xcolor.red = 0;
	    xcolor.green = 0;
	    xcolor.blue = 0;
	    XAllocColor(dpy, cmap_ptr->colormap, &xcolor);
	}

	for (n = 0; n < Ncolors; n++) {
	    strcpy(option, ".");
	    strcat(option, color_keys[n]);
	    if (n > 1)
		strcat(option, ctype);
	    v = pr_GetR(db, option) ? (char *) value.addr
		: ((Gray) ? gray_values[n]
		   : (Rv ? color_values_rv[n] : color_values[n]));

	    if (sscanf(v, "%30[^, ], %lf", color, &intensity) == 2) {
		if (intensity < 0 || intensity > 1) {
		    fprintf(stderr, "\ngnuplot: invalid color intensity in '%s'\n", color);
		    intensity = 1;
		}
	    } else {
		strcpy(color, v);
		intensity = 1;
	    }

	    if (!XParseColor(dpy, cmap_ptr->colormap, color, &xcolor)) {
		fprintf(stderr, "\ngnuplot: unable to parse '%s'. Using black.\n", color);
		cmap_ptr->colors[n] = black;
	    } else {
		xcolor.red *= intensity;
		xcolor.green *= intensity;
		xcolor.blue *= intensity;
		if (XAllocColor(dpy, cmap_ptr->colormap, &xcolor)) {
		    cmap_ptr->colors[n] = xcolor.pixel;
		    cmap_ptr->rgbcolors[n] = ((xcolor.red>>8 & 0xff) << 16)
					   + ((xcolor.green>>8 & 0xff) << 8)
					   + (xcolor.blue>>8);
		} else {
		    fprintf(stderr, "\ngnuplot: can't allocate '%s'. Using black.\n", v);
		    cmap_ptr->colors[n] = black;
		}
	    }
	}
    } else {
	cmap_ptr->colors[0] = (Rv) ? black : white;
	for (n = 1; n < Ncolors; n++)
	    cmap_ptr->colors[n] = (Rv) ? white : black;
    }
#ifdef USE_MOUSE
    {
	/* create the xor GC just for allocating the xor value
	 * before a palette is created. This way the xor foreground
	 * will be available. */
	AllocateXorPixel(cmap_ptr);
    }
#endif
}

/*-----------------------------------------------------------------------------
 *   pr_dashes - determine line dash styles
 *---------------------------------------------------------------------------*/

static const char dash_keys[Ndashes][10] = {
    "border", "axis",
    "line1", "line2", "line3", "line4", "line5", "line6", "line7", "line8"
};

static char dash_mono[Ndashes][10] = {
    "0", "16",
    "0", "42", "13", "44", "15", "4441", "42", "13"
};

/* Version 5 default dash types */
static char dash_color[Ndashes][10] = {
    "0", "16",
    "0", "64", "26", "6424", "642424", "0", "64", "26" 
};

static void
pr_dashes()
{
    int n, j, l, ok;
    char option[20], *v;

/*  Version 5 - always enabled
    if (pr_GetR(db, ".dashed")) {
	dashedlines = (!strncasecmp(value.addr, "on", 2) || !strncasecmp(value.addr, "true", 4));
    }
 */

    for (n = 0; n < Ndashes; n++) {
	strcpy(option, ".");
	strcat(option, dash_keys[n]);
	strcat(option, "Dashes");
	v = pr_GetR(db, option)
	    ? (char *) value.addr : ((Mono) ? dash_mono[n] : dash_color[n]);
	l = GPMIN( strlen(v), DASHPATTERN_LENGTH );
	if (l == 1 && *v == '0') {
	    /* "0" solid line */
	    dashes[n][0] = (unsigned char) 0;
	    continue;
	}
	for (ok = 0, j = 0; j < l; j++) {
	    if (v[j] >= '1' && v[j] <= '9')
		ok++;
	}
	if (ok != l) {
	    fprintf(stderr, "gnuplot: illegal dashes value %s:%s\n",
		    option, v);
	    dashes[n][0] = (unsigned char) 0;
	    continue;
	}
	for (j = 0; j < l; j++) {
	    dashes[n][j] = (unsigned char) (v[j] - '0');
	}
	dashes[n][l] = (unsigned char) 0;
    }
}

/*-----------------------------------------------------------------------------
 *   pr_font - determine font
 *---------------------------------------------------------------------------*/

/* wrapper functions */
int gpXTextWidth (XFontStruct *cfont, const char *str, int count)
{
#ifdef USE_X11_MULTIBYTE
    if (usemultibyte)
	return XmbTextEscapement(mbfont, str, count);
#endif
    return XTextWidth(cfont, str, count);
}

int gpXTextHeight (XFontStruct *cfont)
{
#ifdef USE_X11_MULTIBYTE
    static XFontSetExtents *extents;
    if (usemultibyte) {
	extents = XExtentsOfFontSet(mbfont);
	return extents->max_logical_extent.height;
    } else
#endif
	return cfont->ascent + cfont->descent;
}

void gpXSetFont (Display *disp, GC gc, Font fontid)
{
#ifdef USE_X11_MULTIBYTE
    if (!usemultibyte)
#endif
	XSetFont(disp, gc, fontid);
}

void gpXDrawImageString (Display *disp, Drawable d, GC gc, int x, int y,
			 const char *str, int len)
{
#ifdef USE_X11_MULTIBYTE
    if (usemultibyte) {
	XmbDrawImageString(disp, d, mbfont, gc, x, y, str, len);
	return;
    }
#endif
    XDrawImageString(disp, d, gc, x, y, str, len);
}

void gpXDrawString (Display *disp, Drawable d, GC gc, int x, int y,
		    const char *str, int len)
{
#ifdef USE_X11_MULTIBYTE
    if (usemultibyte) {
	XmbDrawString(disp, d, mbfont, gc, x, y, str, len);
	return;
    }
#endif
    XDrawString(disp, d, gc, x, y, str, len);
}

void gpXFreeFont(Display *disp, XFontStruct *cfont)
{
#ifndef USE_X11_MULTIBYTE
    if (cfont)
	XFreeFont(disp, cfont);
#else
    if (font) {
	XFreeFont(disp, font);
	font=NULL;
    }
    if (mbfont) {
	XFreeFontSet(disp, mbfont);
	mbfont=NULL;
    }
#endif
}

XFontStruct *gpXLoadQueryFont (Display *disp, char *fontname)
{
#ifndef USE_X11_MULTIBYTE
    return XLoadQueryFont(disp, fontname);
#else
    static char **miss, *def;
    static TBOOLEAN first_time = TRUE;
    int n_miss;
    char tmpfname[256];

    if (!usemultibyte)
	return XLoadQueryFont(disp, fontname);
    else {
	fontset_transsep(tmpfname, fontname, 256-1);
	mbfont = XCreateFontSet(disp, tmpfname, &miss, &n_miss, &def);

	/* This test seemed to make sense for Japanese locales, which only */
	/* claim to require a small number of character sets.  But it is   */
	/* highly likely to fail for more generic locales like en_US.UTF-8 */
	/* that claim to "require" about 2 dozen obscure character sets.   */
	/* EAM - do not fail the request; just continue after a warning.   */
	if (n_miss>0) {
#if (0)
	    if (mbfont) {
		XFreeFontSet(disp, mbfont);
		mbfont=NULL;
	    }
#else
	    if (first_time) {
		fprintf(stderr,"gnuplot_x11: Some character sets not available\n");
		first_time = FALSE;
	    }
	    while (n_miss-- > 0)
		FPRINTF((stderr,"Missing charset: %s\n", miss[n_miss]));
#endif
	    XFreeStringList(miss);
	}

	return NULL;
    }
#endif
}

char *gpFallbackFont(void)
{
#ifdef USE_X11_MULTIBYTE
    if (usemultibyte)
	return (encoding == S_ENC_UTF8) ? FallbackFontMBUTF : FallbackFontMB;
#endif
    return FallbackFont;
}

int gpXGetFontascent(XFontStruct *cfont)
{
#ifndef USE_X11_MULTIBYTE
    return cfont->ascent;
#else
    static XFontStruct **eachfonts;
    char **fontnames;
    int max_ascent = 0;
    int i, n_fonts;

    if(!usemultibyte) return font->ascent;
    n_fonts = XFontsOfFontSet(mbfont, &eachfonts, &fontnames);
    for (i = 0; i < n_fonts; i++) {
	if (eachfonts[i]->ascent > max_ascent)
	  max_ascent = eachfonts[i]->ascent;
    }
    return max_ascent;
#endif
}

static void
gpXStoreName (Display *dpy, Window w, char *name)
{
    XStoreName(dpy, w, name);
    XSetIconName(dpy, w, name);
}

#ifdef USE_X11_MULTIBYTE
int fontset_transsep(char *nfname, char *ofname, int n)
{
    char *s;

    strncpy(nfname, ofname, n);
    if (nfname[n-1]!='\0')
	nfname[n]='\0';
    if (strchr(nfname, ','))
	return 1;
    s = nfname;
    while ((s = strchr(nfname, FontSetSep)) != NULL){
	*s = ',';
	nfname = s;
    }
    return 0;
}
#endif

static void
pr_encoding()
{
    char *encoding;
    if ((encoding = pr_GetR(db, ".encoding"))) {
	strncpy(default_encoding, encoding, sizeof(default_encoding)-1);
    }
}

/* EAM Dec 2012 - To save a lot of overhead in looking for, allocating, and	*/
/* freeing fonts, we will keep a list of all the fonts we have used so far.	*/
/* Each new request will be first checked against the list.  If it is already	*/
/* known, we use it.  If it isn't know, we add it to the list.			*/
/* Because the requested names may be of the form ",size" we must clear the	*/
/* list whenever a new default font is set.					*/
struct used_font {
	char *requested_name;	/* The name passed to pr_font() */
	XFontStruct *font;	/* The font we ended up with for that request */
#ifdef USE_X11_MULTIBYTE
        XFontSet mbfont;
        int ismbfont;
#endif
	int vchar;
	int hchar;
	struct used_font *next;	/* pointer to next font in list */
} used_font;
#ifndef USE_X11_MULTIBYTE
static struct used_font fontlist = {NULL, NULL, 12, 8, NULL};
#else
static struct used_font fontlist = {NULL, NULL, NULL, 0, 12, 8, NULL};
#endif

/* Helper routine to clear the used font list */
static void
clear_used_font_list() {
    struct used_font *f;
    while (fontlist.next) {
	f = fontlist.next;
#ifndef USE_X11_MULTIBYTE
	gpXFreeFont(dpy, f->font);
#else
	if (f->font) XFreeFont(dpy, f->font);
	if (f->mbfont) XFreeFontSet(dpy, f->mbfont);
#endif
	free(f->requested_name);
	fontlist.next = f->next;
	free(f);
    }
}

static void
pr_font( fontname )
char *fontname;
{
    char fontspec[128];
    int  fontsize = 0;
#ifdef USE_X11_MULTIBYTE
    char *orgfontname = NULL;
#endif
    struct used_font *search;
    char *requested_name;
    char *try_name;	/* Only for debugging */

    /* Blank string means "default font".  If none has been set this session,
     * try to find one from the X11 settings.  Clear any previous font results.
     */
    if (!fontname)
	clear_used_font_list();
    if (!fontname || !(*fontname))
	fontname = default_font;
    if (!fontname || !(*fontname)) {
	if ((fontname = pr_GetR(db, ".font"))) {
	    strncpy(default_font, fontname, sizeof(default_font)-1);
    /* shige: default_font may be clear for each plot command by 
     * X11_set_default_font() in x11.trm, since the function is called
     * in X11_graphics(). And then the font list will be cleared by the
     * next line in the case the default font is defined in X11 Resources.
     * But it may not be a desired behaviour.
     */
#if 0
	    clear_used_font_list();
#endif
	}
    }

#ifdef USE_X11_MULTIBYTE
    if (fontname && strncmp(fontname, "mbfont:", 7) == 0) {
	if (multibyte_fonts_usable) {
	    usemultibyte = 1;
	    orgfontname = fontname;
	    fontname = &fontname[7];
	    if (!*fontname)
		fontname = NULL;
	} else {
	    usemultibyte=0;
	    fontname=NULL;
	}
    } else usemultibyte=0;
#endif
    if (!fontname)
      fontname = gpFallbackFont();

    /* Look in the used font list to see if this one was requested before. */
    /* FIXME: This is probably the wrong thing to do for multibyte fonts.  */
    for (search = fontlist.next; search; search = search->next) {
	if (!strcmp(fontname, search->requested_name)) {
#ifndef USE_X11_MULTIBYTE
	    font = search->font;
	    vchar = search->vchar;
	    hchar = search->hchar;
	    return;
#else
	    if (!usemultibyte && !search->ismbfont) { 
		font = search->font;
		vchar = search->vchar;
		hchar = search->hchar;
		return;
	    } else if (usemultibyte && search->ismbfont) {
		mbfont = search->mbfont;
		vchar = search->vchar;
		hchar = search->hchar;
		return;
	    } else break;
#endif
	}
    }
    /* If we get here, the request doesn't match a previously used font.
     * Whatever font we end up with should be recorded in the used_font
     * list so that we can find it cheaply next time.		
     */
    requested_name = strdup(fontname);

    /* Try using the requested name directly as an x11 font spec. */
    font = gpXLoadQueryFont(dpy, try_name = fontname);

#ifndef USE_X11_MULTIBYTE
    if (!font) {
#else
    if (!font && !mbfont && !strchr(fontname, FontSetSep)) {
#endif
	/* EAM 19-Aug-2002 Try to construct a plausible X11 full font spec */
	/* We are passed "font<, size><, slant>"                             */
	char shortname[64], *fontencoding, slant, *weight;
	int  sep;
#ifdef USE_X11_MULTIBYTE
	int backfont = 0;
#endif

	/* Enhanced font processing wants a method of requesting a new size  */
	/* for whatever the previously selected font was, so we have to save */
	/* and reuse the previous font name to construct the new spec.       */
	if (*fontname == ',') {
	    fontsize = atof(&(fontname[1])) + 0.5;
	    fontname = default_font;
#ifdef USE_X11_MULTIBYTE
	    backfont = 1;
#endif
	}
#ifdef USE_X11_MULTIBYTE
	if (backfont && fontname && strncmp(fontname, "mbfont:", 7) == 0
	    && multibyte_fonts_usable) {
	    usemultibyte = 1;
	    orgfontname = fontname;
	    fontname = &fontname[7];
	}
#endif

	sep = strcspn(fontname, ",");
	if (sep >= sizeof(shortname))
	    sep = sizeof(shortname) - 1;
	strncpy(shortname, fontname, sep);
	shortname[sep] = '\0';
	if (!fontsize)
	    fontsize = atof(&(fontname[sep+1])) + 0.5;
	if (fontsize > 99 || fontsize < 1)
	    fontsize = 12;

	slant = strstr(&fontname[sep+1], "italic")  ? 'i' :
		strstr(&fontname[sep+1], "oblique") ? 'o' :
		                                     'r' ;

	weight = strstr(&fontname[sep+1], "bold")   ? "bold" :
		 strstr(&fontname[sep+1], "medium") ? "medium" :
		                                     "*" ;
	if (!strncmp("Symbol", shortname, 6) || !strncmp("symbol", shortname, 6))
	    fontencoding = "*-*";
#ifdef USE_X11_MULTIBYTE
	else if (usemultibyte)
	    fontencoding = (
		encoding == S_ENC_UTF8      ? "iso10646-1" :
		"*-*" ) ;	/* EAM 2011 - This used to work but, alas, no longer. */
#endif
	else
	    fontencoding = (
		encoding == S_ENC_CP437     ? "dosencoding-cp437" :
		encoding == S_ENC_CP850     ? "dosencoding-cp850" :
		encoding == S_ENC_ISO8859_1 ? "iso8859-1" :
		encoding == S_ENC_ISO8859_2 ? "iso8859-2" :
		encoding == S_ENC_ISO8859_15 ? "iso8859-15" :
		encoding == S_ENC_KOI8_R    ? "koi8-r" :
		encoding == S_ENC_KOI8_U    ? "koi8-u" :
		encoding == S_ENC_UTF8      ? "iso10646-1" :
		default_encoding[0] ? default_encoding :
#if (0)
		"*-*" ) ;	/* EAM 2011 - This used to work but, alas, no longer. */
#else
		"iso8859-1" ) ;	/* biased to English, but since the wildcard doesn't work ... */
#endif

	sprintf(fontspec, "-*-%s-%s-%c-*--%d-*-*-*-*-*-%s",
		shortname, weight, slant, fontsize, fontencoding
		);
	font = gpXLoadQueryFont(dpy, try_name = fontspec);

#ifndef USE_X11_MULTIBYTE
	if (!font) {
#else
	if (!font && !mbfont) {
#endif
	    /* Try to decode some common PostScript font names */
	    if (!strcmp("Times-Bold", shortname)
		|| !strcmp("times-bold", shortname)) {
		sprintf(fontspec, "-*-times-bold-r-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Times-Roman", shortname)
		       || !strcmp("times-roman", shortname)) {
		sprintf(fontspec, "-*-times-medium-r-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Times-Italic", shortname)
		       || !strcmp("times-italic", shortname)) {
		sprintf(fontspec, "-*-times-medium-i-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Times-BoldItalic", shortname)
		       || !strcmp("times-bolditalic", shortname)) {
		sprintf(fontspec, "-*-times-bold-i-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Helvetica-Bold", shortname) ||
		       !strcmp("helvetica-bold", shortname)) {
		sprintf(fontspec, "-*-helvetica-bold-r-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Helvetica-Oblique", shortname)
		       || !strcmp("helvetica-oblique", shortname)) {
		sprintf(fontspec, "-*-helvetica-medium-o-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Helvetica-BoldOblique", shortname)
		       || !strcmp("helvetica-boldoblique", shortname)) {
		sprintf(fontspec, "-*-helvetica-bold-o-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    } else if (!strcmp("Helvetica-Narrow-Bold", shortname)
		       || !strcmp("helvetica-narrow-bold", shortname)) {
		sprintf(fontspec, "-*-arial narrow-bold-r-*-*-%d-*-*-*-*-*-%s",
			fontsize, fontencoding);
	    }
#ifdef USE_X11_MULTIBYTE
	    /* Japanese standard PostScript font names (adviced from
	     * N.Matsuda). */
	    else if (multibyte_fonts_usable
		     && (!strncmp("Ryumin-Light", shortname,
				  strlen("Ryumin-Light"))
			 || !strncmp("ryumin-light", shortname,
				     strlen("ryumin-light")))) {
		if (!usemultibyte) {
		    usemultibyte = 1;
		    orgfontname = fontname;
		}
		sprintf(fontspec, "-*-mincho-medium-%c-*--%d-*",
			slant, fontsize);
	    }
	    else if (multibyte_fonts_usable
		     && (!strncmp("GothicBBB-Medium", shortname,
				  strlen("GothicBBB-Medium"))
			 || !strncmp("gothicbbb-medium", shortname,
				     strlen("gothicbbb-medium")))) {
		if (!usemultibyte) {
		    usemultibyte = 1;
		    orgfontname = fontname;
		}
		/* FIXME: Doesn't work on most non-japanese setups, because */
		/* many purely Western fonts are gothic-bold.               */
		sprintf(fontspec, "-*-gothic-bold-%c-*--%d-*", slant, fontsize);
	    }
#endif /* USE_X11_MULTIBYTE */
	    font = gpXLoadQueryFont(dpy, try_name = fontspec);

#ifdef USE_X11_MULTIBYTE
	    if (usemultibyte && !mbfont) {
		/* But (mincho|gothic) X fonts are not provided
		 * on some X servers even in Japan
		 */
		sprintf(fontspec, "*-%s-%c-*--%d-*", 
			weight, slant, fontsize);
		font = gpXLoadQueryFont(dpy, try_name = fontspec);
	    }
#endif /* USE_X11_MULTIBYTE */
	}

    }

#ifndef USE_X11_MULTIBYTE
    if (font) {
#else
    if (font || mbfont) {
	if (usemultibyte && orgfontname)
	  fontname = orgfontname;
#endif
    }

    /* By now we have tried everything we can to honor the specific request. */
    /* Try some common scaleable fonts before falling back to a last resort  */
    /* fixed font.                                                           */
#ifndef USE_X11_MULTIBYTE
    if (!font) {
#else
    if (!usemultibyte && !font) {
#endif
	sprintf(fontspec, "-*-bitstream vera sans-bold-r-*-*-%d-*-*-*-*-*-*-*", fontsize);
	font = gpXLoadQueryFont(dpy, try_name = fontspec);
	fontname = fontspec;
	if (!font) {
	    sprintf(fontspec, "-*-arial-medium-r-*-*-%d-*-*-*-*-*-*-*", fontsize);
	    font = gpXLoadQueryFont(dpy, try_name = fontspec);
	}
	if (!font) {
	    sprintf(fontspec, "-*-helvetica-medium-r-*-*-%d-*-*-*-*-*-*", fontsize);
	    font = gpXLoadQueryFont(dpy, try_name = fontspec);
	}
	if (!font) {
	    fontname = gpFallbackFont();
	    font = gpXLoadQueryFont(dpy, try_name = fontname);
	}
	if (!font) {
	    fprintf(stderr, "\ngnuplot_x11: can't find usable font - X11 aborted.\n");
	    EXIT(1);
	}
	FPRINTF((stderr, "\ngnuplot_x11: requested font not found, using '%s' instead.\n", fontname));
    }
#ifdef USE_X11_MULTIBYTE
    if (usemultibyte && !mbfont) { /* multibyte font setting */
	font = gpXLoadQueryFont(dpy, try_name = gpFallbackFont());
	if (!mbfont) {
	    usemultibyte=0;
	    font = gpXLoadQueryFont(dpy, try_name = gpFallbackFont());
	    if (!font) {
		fprintf(stderr, "\ngnuplot_x11: can't find usable font - X11 aborted.\n");
		EXIT(1);
	    }
	}
	fontname = gpFallbackFont();
    }
#endif /* USE_X11_MULTIBYTE */

    vchar = gpXTextHeight(font);
    hchar = gpXTextWidth(font, "0123456789", 10) / 10;

    /* Save a pointer to this font indexed by the name used to request it */
    search = &fontlist;
    while (search->next)
	search = search->next;
    search->next = malloc(sizeof(used_font));
    search = search->next;
    search->next = NULL;
#ifndef USE_X11_MULTIBYTE
    search->font = font;
#else
    if (!usemultibyte) { 
	search->ismbfont = 0;
	search->font = font;
	search->mbfont = NULL;
    } else {
	search->ismbfont = 1;
	search->font = NULL;
	search->mbfont = mbfont;
    }
#endif
    search->requested_name = requested_name;
    search->vchar = vchar;
    search->hchar = hchar;

    FPRINTF((stderr, "gnuplot_x11: pr_font() set font %s, vchar %d hchar %d\n",
		fontname, vchar, hchar));
    FPRINTF((stderr, "gnuplot_x11: requested \"%s\" succeeded with \"%s\"\n", 
    		requested_name, try_name));

}

/*-----------------------------------------------------------------------------
 *   pr_geometry - determine window geometry
 *---------------------------------------------------------------------------*/

static void
pr_geometry(char *instr)
{
    char *geometry = (instr != NULL)? instr : pr_GetR(db, ".geometry");
    int x, y, flags;
    unsigned int w, h;

    if (geometry) {
	flags = XParseGeometry(geometry, &x, &y, &w, &h);
	if (flags & WidthValue)
	    gW = w;
	if (flags & HeightValue)
	    gH = h;
	if (flags & (WidthValue | HeightValue))
	    gFlags = (gFlags & ~PSize) | USSize;

	if (flags & XValue)
	    gX = (flags & XNegative) ? x + DisplayWidth(dpy, scr) - gW - BorderWidth * 2 : x;

	if (flags & YValue)
	    gY = (flags & YNegative) ? y + DisplayHeight(dpy, scr) - gH - BorderWidth * 2 : y;

	if (flags & (XValue | YValue))
	    gFlags = (gFlags & ~PPosition) | USPosition;
    }
}

/*-----------------------------------------------------------------------------
 *   pr_pointsize - determine size of points for 'points' plotting style
 *---------------------------------------------------------------------------*/

static void
pr_pointsize()
{
    if (pr_GetR(db, ".pointsize")) {
	if (sscanf((char *) value.addr, "%lf", &pointsize) == 1) {
	    if (pointsize <= 0 || pointsize > 10) {
		fprintf(stderr, "\ngnuplot: invalid pointsize '%s'\n", value.addr);
		pointsize = 1;
	    }
	} else {
	    fprintf(stderr, "\ngnuplot: invalid pointsize '%s'\n", value.addr);
	    pointsize = 1;
	}
    } else {
	pointsize = 1;
    }
}

/*-----------------------------------------------------------------------------
 *   pr_width - determine line width values
 *---------------------------------------------------------------------------*/

static const char width_keys[Nwidths][30] = {
    "border", "axis",
    "line1", "line2", "line3", "line4", "line5", "line6", "line7", "line8"
};

static void
pr_width()
{
    int n;
    char option[20], *v;

    for (n = 0; n < Nwidths; n++) {
	strcpy(option, ".");
	strcat(option, width_keys[n]);
	strcat(option, "Width");
	if ((v = pr_GetR(db, option)) != NULL) {
	    if (*v < '0' || *v > '9' || strlen(v) > 1)
		fprintf(stderr, "gnuplot: illegal width value %s:%s\n", option, v);
	    else
		widths[n] = (unsigned int) atoi(v);
	}
    }
}

/*-----------------------------------------------------------------------------
 *   pr_window - create window
 *---------------------------------------------------------------------------*/

static void
pr_window(plot_struct *plot)
{
    char *title = pr_GetR(db, ".title");
    static XSizeHints hints;
    static XClassHint class_hint;
    int Tvtwm = 0;
    long event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask
	| PointerMotionMask | PointerMotionHintMask | ButtonPressMask
	| ButtonReleaseMask | ExposureMask | EnterWindowMask;

    FPRINTF((stderr, "(pr_window) \n"));

#ifdef EXTERNAL_X11_WINDOW
    if (plot->external_container != None) {
	XWindowAttributes gattr;
	XGetWindowAttributes(dpy, plot->external_container, &gattr);
	plot->x = 0;
	plot->y = 0;
	plot->width = gattr.width;
	plot->height = gattr.height;
	plot->gheight = gattr.height;
	if (!plot->window) {
	    plot->window = XCreateWindow(dpy, plot->external_container, plot->x, plot->y, plot->width,
					 plot->height, 0, dep, InputOutput, vis, 0, NULL);
		gp_execute_GE_plotdone(plot->window); /* notify main program, send WINDOWID */
	}
    }
#endif /* EXTERNAL_X11_WINDOW */

    if (have_pm3d) {
	XSetWindowAttributes attr;
	unsigned long mask = CWBackPixel | CWBorderPixel | CWColormap;
	attr.background_pixel = plot->cmap->colors[0];
	attr.border_pixel = plot->cmap->colors[1];
	attr.colormap = plot->cmap->colormap;
	if (!plot->window) {
	    plot->window = XCreateWindow(dpy, root, plot->x, plot->y, plot->width,
					 plot->height, BorderWidth, dep, InputOutput, vis, mask, &attr);
		gp_execute_GE_plotdone(plot->window); /* notify main program, send WINDOWID */
	}
	else
	    XChangeWindowAttributes(dpy, plot->window, mask, &attr);
    } else
#ifdef EXTERNAL_X11_WINDOW
    if (!plot->window)
#endif
	{
	plot->window = XCreateSimpleWindow(dpy, root, plot->x, plot->y, plot->width, plot->height,
					   BorderWidth, plot->cmap->colors[1], plot->cmap->colors[0]);
		gp_execute_GE_plotdone(plot->window); /* notify main program, send WINDOWID */
	}

    /* Return if something wrong. */
    if (plot->window == None)
	return;

    /* ask ICCCM-compliant window manager to tell us when close window
     * has been chosen, rather than just killing us
     */

    XChangeProperty(dpy, plot->window, WM_PROTOCOLS, XA_ATOM, 32, PropModeReplace, (unsigned char *) &WM_DELETE_WINDOW, 1);

    if (pr_GetR(db, ".clear") && On(value.addr))
	Clear++;
    if (pr_GetR(db, ".tvtwm") && On(value.addr))
	Tvtwm++;

    if (!Tvtwm) {
	hints.flags = plot->posn_flags;
    } else {
	hints.flags = (plot->posn_flags & ~USPosition) | PPosition;	/* ? */
    }
    hints.x = gX;
    hints.y = gY;
    hints.width = plot->width;
    hints.height = plot->height;

    XSetNormalHints(dpy, plot->window, &hints);

    /* set WM_CLASS for interaction with gnome-shell */
    class_hint.res_name = "gnuplot";
    class_hint.res_class = "Gnuplot";
    XSetClassHint(dpy, plot->window, &class_hint);

    if (pr_GetR(db, ".iconic") && On(value.addr)) {
	XWMHints wmh;

	wmh.flags = StateHint;
	wmh.initial_state = IconicState;
	XSetWMHints(dpy, plot->window, &wmh);
    }
#if 0 /* 1 clear, 0 do not clear */
    if (plot->titlestring) {
	free(plot->titlestring);
	plot->titlestring = 0;
    }
#endif

    /* Set up the events to process */
    XSelectInput(dpy, plot->window, event_mask);
#ifdef EXTERNAL_X11_WINDOW
    /* Two clients of an X window cannot share ButtonPress events at the same time.
     * The outside application may still have ButtonPress selected, and that is the
     * reason for using the external window as a container. */
    if (plot->external_container != None)
	XSelectInput(dpy, plot->external_container, event_mask & (~(ButtonPressMask|ButtonReleaseMask)));
#endif
    XSync(dpy, 0);

    /* If title doesn't exist, create one. */
#if 1
#define ICON_TEXT "gplt"
#define TEMP_NUM_LEN 16
    {
    /* append the X11 terminal number (if greater than zero) */
    char numstr[sizeof(ICON_TEXT)+TEMP_NUM_LEN+1]; /* space for text, number and terminating \0 */
    if (plot->plot_number > 0)
	sprintf(numstr, "%s%d%c", ICON_TEXT, plot->plot_number, '\0');
    else
	sprintf(numstr, "%s%c", ICON_TEXT, '\0');
    FPRINTF((stderr, "term_number is %d", plot->plot_number));
    XSetIconName(dpy, plot->window, numstr);
#undef TEMP_NUM_LEN
    if (!plot->titlestring) {
	int orig_len;
	if (!title) title = X_Class;
	orig_len = strlen(title);
	/* memory for text, white space, number and terminating \0 */
	if ((plot->titlestring = (char *) malloc(orig_len + ((orig_len && (plot->plot_number > 0)) ? 1 : 0) + strlen(numstr) - strlen(ICON_TEXT) + 1))) {
	    strcpy(plot->titlestring, title);
	    if (orig_len && (plot->plot_number > 0))
		plot->titlestring[orig_len++] = ' ';
	    strcpy(plot->titlestring + orig_len, numstr + strlen(ICON_TEXT));
	    gpXStoreName(dpy, plot->window, plot->titlestring);
	} else
	    gpXStoreName(dpy, plot->window, title);
    } else {
	gpXStoreName(dpy, plot->window, plot->titlestring);
    }
    }
#undef ICON_TEXT
#endif

    XMapWindow(dpy, plot->window);
#ifdef EXTERNAL_X11_WINDOW
    if (plot->external_container != None)
	XMapWindow(dpy, plot->external_container);
#endif

    windows_open++;
}


/***** pr_raise ***/
static void
pr_raise()
{
    if (pr_GetR(db, ".raise"))
	do_raise = (On(value.addr));
}

static void
pr_replotonresize()
{
    if (pr_GetR(db, ".replotonresize"))
	replot_on_resize = (On(value.addr));
}

static void
pr_persist()
{
    if (pr_GetR(db, ".persist"))
	persist = (On(value.addr));
}

static void
pr_feedback()
{
    if (pr_GetR(db, ".feedback"))
	feedback = !(!strncasecmp(value.addr, "off", 3) || !strncasecmp(value.addr, "false", 5));
    FPRINTF((stderr, "gplt_x11: set feedback to %d (%s)\n", feedback, value.addr));
}

static void
pr_ctrlq()
{
    if (pr_GetR(db, ".ctrlq")) {
	ctrlq = (!strncasecmp(value.addr, "on", 2) || !strncasecmp(value.addr, "true", 4));
	FPRINTF((stderr, "gplt_x11: require <ctrl>q and <ctrl><space>\n"));
    }
}

static void
pr_fastrotate()
{
    if (pr_GetR(db, ".fastrotate")) {
	fast_rotate = (!strncasecmp(value.addr, "on", 2) || !strncasecmp(value.addr, "true", 4));
	FPRINTF((stderr, "gplt_x11: Use fast but imperfect text rotation\n"));
    }
}

#ifdef EXPORT_SELECTION
static void
pr_exportselection()
{
    /* Allow export selection to be turned on or off using X resource *exportselection */
    if (pr_GetR(db, ".exportselection")) {
	if (!strncmp((char *)value.addr, "off", 3) || !strncmp((char *)value.addr, "false", 5)) {
	    exportselection = FALSE;
	    FPRINTF((stderr, "gnuplot_x11: exportselection is disabled\n"));
	}
    }
}
#endif

/************ code to handle selection export *********************/

#ifdef EXPORT_SELECTION

/* bit of a bodge, but ... */
static struct plot_struct *exported_plot;
static Time export_time;

static void
export_graph(struct plot_struct *plot)
{
    FPRINTF((stderr, "export_graph(0x%x)\n", plot));

    XSetSelectionOwner(dpy, EXPORT_SELECTION, plot->window, CurrentTime);
    /* to check we have selection, we would have to do a
     * GetSelectionOwner(), but if it failed, it failed - no big deal
     */
    if (! *selection) {
	exported_plot = plot;
	export_time = (!plot || !(plot->time)) ? 1 : plot->time;
    }
}

static void
handle_selection_event(XEvent *event)
{
    switch (event->type) {
    case SelectionRequest:
	{
	    XEvent reply;

	    static Atom XA_TARGETS = (Atom) 0;
	    static Atom XA_TIMESTAMP = (Atom) 0;

	    if (XA_TARGETS == 0)
		XA_TARGETS = XInternAtom(dpy, "TARGETS", False);
	    if (XA_TIMESTAMP == 0)
		XA_TIMESTAMP = XInternAtom(dpy, "TIMESTAMP", False);

	    reply.type = SelectionNotify;
	    reply.xselection.send_event = True;
	    reply.xselection.display = event->xselectionrequest.display;
	    reply.xselection.requestor = event->xselectionrequest.requestor;
	    reply.xselection.selection = event->xselectionrequest.selection;
	    reply.xselection.target = event->xselectionrequest.target;
	    reply.xselection.property = event->xselectionrequest.property;
	    reply.xselection.time = event->xselectionrequest.time;

	    if (reply.xselection.target == XA_TARGETS) {
		static Atom targets[] = { XA_PIXMAP, XA_COLORMAP };
		static Atom mousecoord[] = { XA_STRING };

		FPRINTF((stderr, "Targets request from %d\n", reply.xselection.requestor));

		if (*selection)
		    XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				32, PropModeReplace, (unsigned char *) (mousecoord), 1);
		else if (exported_plot)
		    XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				32, PropModeReplace, (unsigned char *) targets, 2);
	    } else if (reply.xselection.target == XA_COLORMAP) {

		FPRINTF((stderr, "colormap request from %d\n", reply.xselection.requestor));

		XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				32, PropModeReplace, (unsigned char *) &(default_cmap.colormap), 1);
	    } else if (reply.xselection.target == XA_PIXMAP && exported_plot) {

		FPRINTF((stderr, "pixmap request from %d\n", reply.xselection.requestor));

		XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				32, PropModeReplace, (unsigned char *) &(exported_plot->pixmap), 1);
		exported_plot = NULL;
	    } else if (reply.xselection.target == XA_TIMESTAMP) {

		FPRINTF((stderr, "timestamp request from %d : %ld\n",
			reply.xselection.requestor, export_time));

		XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				32, PropModeReplace, (unsigned char *) &(export_time), 1);
#ifdef PIPE_IPC
	    } else if (reply.xselection.target == XA_STRING && *selection) {
		FPRINTF((stderr, "XA_STRING request\n"));
		XChangeProperty(dpy, reply.xselection.requestor,
				reply.xselection.property, reply.xselection.target,
				8, PropModeReplace, (unsigned char *) selection, strlen(selection));
#endif
	    } else {
		FPRINTF((stderr, "selection request target: %d\n", reply.xselection.target));
		reply.xselection.property = None;
		if (!exported_plot && ! *selection)
		    /* We have now satisfied the select request. Say goodbye */
		    XSetSelectionOwner(dpy, EXPORT_SELECTION, None, CurrentTime);
	    }

	    XSendEvent(dpy, reply.xselection.requestor, False, 0L, &reply);
	    /* we never block on XNextEvent(), so must flush manually
	     * (took me *ages* to find this out !)
	     */

	    XFlush(dpy);
	}
	break;
    }
}

#endif /* EXPORT_SELECTION */

#if defined(USE_MOUSE) && defined(MOUSE_ALL_WINDOWS)
/* Convert X-window mouse coordinates to coordinate system of plot axes */
static void
mouse_to_coords(plot_struct *plot, XEvent *event,
		double *x, double *y, double *x2, double *y2)
{
    int xx = RevX( event->xbutton.x );
    int yy = RevY( event->xbutton.y );

    FPRINTF((stderr, "gnuplot_x11 %d: mouse at %d %d\t", __LINE__, xx, yy));

    *x  = mouse_to_axis(xx, &(plot->axis_scale[FIRST_X_AXIS]));
    *y  = mouse_to_axis(yy, &(plot->axis_scale[FIRST_Y_AXIS]));
    *x2 = mouse_to_axis(xx, &(plot->axis_scale[SECOND_X_AXIS]));
    *y2 = mouse_to_axis(yy, &(plot->axis_scale[SECOND_Y_AXIS]));

    FPRINTF((stderr, "mouse x y %10g %10g x2 y2 %10g %10g\n", *x, *y, *x2, *y2 ));
}

static double
mouse_to_axis(int mouse_coord, axis_scale_t *axis)
{
    double axis_coord;

    if (axis->term_scale == 0.0)
	return 0.;

    axis_coord = ((double)(mouse_coord - axis->term_lower)) / axis->term_scale + axis->min;
    if (axis->logbase > 0.0)
	axis_coord = exp(axis_coord * axis->logbase);

    return axis_coord;
}
#endif


/*-----------------------------------------------------------------------------
 *   Add_Plot_To_Linked_List - Create space for plot and put in linked list.
 *---------------------------------------------------------------------------*/

static plot_struct *
Add_Plot_To_Linked_List(int plot_number)
{
    plot_struct *psp;
    if (plot_number >= 0)
	/* Make sure plot does not already exist in the list. */
	psp = Find_Plot_In_Linked_List_By_Number(plot_number);
    else
	psp = NULL;

    if (psp == NULL) {
	psp = (plot_struct *) malloc(sizeof(plot_struct));
	if (psp) {
	    /* Initialize structure variables. */
	    memset((void*)psp, 0, sizeof(plot_struct));
	    psp->plot_number = plot_number;
#if EXTERNAL_X11_WINDOW
	    /* Number and container methods are mutually exclusive. */
	    if (plot_number >= 0)
		psp->external_container = None;
#endif
	    /* Add link to beginning of the list. */
	    psp->prev_plot = NULL;
	    if (plot_list_start != NULL) {
		plot_list_start->prev_plot = psp;
		psp->next_plot = plot_list_start;
	    } else
		psp->next_plot = NULL;
	    plot_list_start = psp;
	}
	else {
	    psp = NULL;
	    fprintf(stderr, ERROR_NOTICE("Could not allocate memory for plot.\n\n"));
	}
    }

    return psp;
}


/*-----------------------------------------------------------------------------
 *   Remove_Plot_From_Linked_List - Remove from linked list and free memory.
 *---------------------------------------------------------------------------*/

static void
Remove_Plot_From_Linked_List(Window plot_window)
{
    /* Make sure plot exists in the list. */
    plot_struct *psp = Find_Plot_In_Linked_List_By_Window(plot_window);

    if (psp != NULL) {
	/* Remove link from the list. */
	if (psp->next_plot != NULL)
	    psp->next_plot->prev_plot = psp->prev_plot;
	if (psp->prev_plot != NULL) {
	    psp->prev_plot->next_plot = psp->next_plot;
	} else {
	    plot_list_start = psp->next_plot;
	}
	/* If global pointers point at this plot, reassign them. */
	if (current_plot == psp) {
#if 0 /* Make some other plot current. */
	    if (psp->prev_plot != NULL)
		current_plot = psp->prev_plot;
	    else
		current_plot = psp->next_plot;
#else /* No current plot. */
	    current_plot = NULL;
#endif
	}
	/* Deallocate memory.  Make sure plot removed from list first. */
	delete_plot(psp);
	free(psp);
    }
}


/*-----------------------------------------------------------------------------
 *   Find_Plot_In_Linked_List_By_Number - Search for the plot in the linked list.
 *---------------------------------------------------------------------------*/

static plot_struct *
Find_Plot_In_Linked_List_By_Number(int plot_number)
{
    plot_struct *psp = plot_list_start;

    while (psp != NULL) {
	if (psp->plot_number == plot_number)
	    break;
	psp = psp->next_plot;
    }

    return psp;
}


/*-----------------------------------------------------------------------------
 *   Find_Plot_In_Linked_List_By_Window - Search for the plot in the linked list.
 *---------------------------------------------------------------------------*/

static plot_struct *
Find_Plot_In_Linked_List_By_Window(Window window)
{
    plot_struct *psp = plot_list_start;

    while (psp != NULL) {
	if (psp->window == window)
	    break;
	psp = psp->next_plot;
    }

#ifdef EXTERNAL_X11_WINDOW
    if (psp != NULL)
	return psp;

    /* Search through the containers, but do so as a separate loop to not
     * effect performance for normal numbered plot window searches. */
    psp = plot_list_start;

    while (psp != NULL) {
	if (psp->external_container != None && psp->external_container == window)
	    break;
	psp = psp->next_plot;
    }
#endif

    return psp;

}


/*-----------------------------------------------------------------------------
 *   Find_Plot_In_Linked_List_By_CMap - Search for the plot in the linked list.
 *---------------------------------------------------------------------------*/

static plot_struct *
Find_Plot_In_Linked_List_By_CMap(cmap_t *cmp)
{
    plot_struct *psp = plot_list_start;

    while (psp != NULL) {
	cmap_struct *csp = psp->first_cmap_struct;
	while (csp != NULL) {
	    if (csp->cmap == cmp)
		break;
	    csp = csp->next_cmap_struct;
	}
	if (csp != NULL)
	    break;
	psp = psp->next_plot;
    }

    return psp;
}


/* NOTE:  The removing of plots via the ErrorHandler routine is rather
   tricky.  The error events can happen at any time during execution of
   the program, very similar to an interrupt.  The consequence is that
   the error handling routine can't remove plots from the linked list
   directly.  Instead we use a queuing system in which the main code
   eventually removes the plots.

   Furthermore, to be safe, only the error handling routine should create
   and delete elements in the FIFO.  Otherwise, the possibility of bogus
   pointers can arise if error events happen at the exact wrong time.
   (Requires a lot of thought.)

   The scheme here is for the error handler to put elements in the
   queue marked as "processed = 0" and then indicate that the main
   code should process elements in the queue.  The main code then
   copies the information about the plot to remove and sets the value
   "processed = 1".  Afterward the main code removes the plot.
*/

/*-----------------------------------------------------------------------------
 *   Add_Plot_To_Remove_FIFO_Queue - Method for error handler to destroy plot.
 *---------------------------------------------------------------------------*/

static void
Add_Plot_To_Remove_FIFO_Queue(Window plot_window)
{
    /* Clean up any processed links. */
    plot_remove_struct *prsp = remove_fifo_queue_start;
    FPRINTF((stderr, "Add plot to remove FIFO queue called.\n"));
    while (prsp != NULL) {
	if (prsp->processed) {
	    remove_fifo_queue_start = prsp->next_remove;
	    free(prsp);
	    prsp = remove_fifo_queue_start;
	    FPRINTF((stderr, "  -> Removed a processed element from FIFO queue.\n"));
	} else {
	    break;
	}
    }

    /* Go to end of list while checking if this window is already in list. */
    while (prsp != NULL) {
	if (prsp->plot_window_to_remove == plot_window) {
	    /* Discard this request because the same window is yet to be processed.
	       X11 could be stuck sending the same error message again and again
	       while the main program is not responding for some reason.  This would
	       lead to the FIFO queue growing indefinitely. */
	    return;
	}
	if (prsp->next_remove == NULL)
	    break;
	else
	    prsp = prsp->next_remove;
    }

    /* Create link and add to end of queue. */
    {plot_remove_struct *prsp_new = (plot_remove_struct *) malloc(sizeof(plot_remove_struct));
    if (prsp_new) {
	/* Initialize structure variables. */
	prsp_new->next_remove = NULL;
	prsp_new->plot_window_to_remove = plot_window;
	prsp_new->processed = 0;
	if (remove_fifo_queue_start)
	    prsp->next_remove = prsp_new;
	else
	    remove_fifo_queue_start = prsp_new;
	process_remove_fifo_queue = 1; /* Indicate to main loop that there is a plot to remove. */
	FPRINTF((stderr, "  -> Added an element to FIFO queue.\n"));
    }
    else {
	fprintf(stderr, ERROR_NOTICE("Could not allocate memory for plot remove queue.\n\n"));
    }}
}


/*-----------------------------------------------------------------------------
 *   Process_Remove_FIFO_Queue - Remove plots queued by error handler.
 *---------------------------------------------------------------------------*/

static void
Process_Remove_FIFO_Queue()
{
    plot_remove_struct *prsp = remove_fifo_queue_start;

    /* Clear flag before processing so that if an ErrorHandler event
     * comes along while running the remainder of this routine, any new
     * error events that were missed because of timing issues will be
     * processed next time through the main loop.
     */
    process_remove_fifo_queue = 0;

    /* Go through the list and process any unprocessed queue request.
     * No clean up is done here because having two asynchronous routines
     * modifying the queue would be too dodgy.  The ErrorHandler creates
     * and removes links in the queue based upon the processed flag.
     */
    while (prsp != NULL) {

	/* Make a copy of the remove information structure before changing flag.
	 * Otherwise, there would be the possibility of the error handler routine
	 * removing the associated link upon seeing the "processed" flag set.  From
	 * this side of things, the pointer becomes invalid once that flag is set.
	 */
	plot_remove_struct prs;
	prs.plot_window_to_remove = prsp->plot_window_to_remove;
	prs.next_remove = prsp->next_remove;
	prs.processed = prsp->processed;

	/* Set processed flag before processing the event.  This is so
	 * that the FIFO queue does not have to repeat window entries.
	 * If the error handler were to break in right before the
	 * "processed" flag is set to 1 and not put another link in
	 * the FIFO because it sees the window in question is already
	 * in the FIFO, we're OK.  The reason is that the window in
	 * question is still to be processed.  On the other hand, if
	 * we were to process then set the flag, an entry in the queue
	 * could potentially be lost.
	 */
	prsp->processed = 1;
	FPRINTF((stderr, "Processed element in remove FIFO queue.\n"));

	/* NOW process the plot to remove. */
	if (!prs.processed)
	    Remove_Plot_From_Linked_List(prs.plot_window_to_remove);

	prsp = prs.next_remove;
    }

    /* Issue an X11 error so that error handler cleans up queue?
     * Really, this isn't super important.  Without issuing a bogus
     * error, the processed queue elements will be deleted the
     * next time there is an error.  Until another error comes
     * along it means there is maybe ten or so words of memory
     * reserved on the heap for the FIFO queue.  In some sense,
     * the extra code is hardly worth the effort, especially
     * when X11 documentation is so sparse on the matter of errors.
     */

}


/* Extract details about the extent of a bit mask by doing a
 * single bit shift up and then down (left shift) and down
 * and then up (right shift).  When the pre- and post-shift
 * numbers are not equal, a bit was lost so that is the
 * extent of the mask.
 */
static unsigned short
BitMaskDetails(unsigned long mask, unsigned short *left_shift, unsigned short *right_shift)
{
    unsigned short i;
    unsigned long m = mask;

    if (mask == 0) {
	*left_shift = 0;
	*right_shift = 0;
	return 0;
    }

    for (i=0; i < 32; i++) {
	if ( (((m << 1)&0xffffffff) >> 1) != m )
	    break;
	else
	    m <<= 1;
    }
    *left_shift = i;

    m = mask;
    for (i=0; i < 32 ; i++) {
	if ( ((m >> 1) << 1) != m )
	    break;
	else
	    m >>= 1;
    }
    *right_shift = i;

    return (unsigned short) m;
}


/*-----------------------------------------------------------------------------
 *   Add_CMap_To_Linked_List - Create space for colormap and put in linked list.
 *---------------------------------------------------------------------------*/

static cmap_t *
Add_CMap_To_Linked_List(void)
{
    cmap_t *cmp = (cmap_t *) malloc(sizeof(cmap_t));
    if (cmp) {
	/* Add link to beginning of the list. */
	cmp->prev_cmap = NULL;
	if (cmap_list_start != NULL) {
	    cmap_list_start->prev_cmap = cmp;
	    cmp->next_cmap = cmap_list_start;
	} else
	    cmp->next_cmap = NULL;
	cmap_list_start = cmp;
    } else {
	cmp = NULL;
	fprintf(stderr, ERROR_NOTICE("Could not allocate memory for color map.\n\n"));
    }
    /* Initialize structure variables. */
    CmapClear(cmp);
    cmp->colormap = XCreateColormap(dpy, root, vis, AllocNone);
    assert(cmp->colormap);
    pr_color(cmp);	/* set default colors for lines */
    return cmp;
}


/*-----------------------------------------------------------------------------
 *   Remove_CMap_From_Linked_List - Remove from linked list and free memory.
 *---------------------------------------------------------------------------*/

static void
Remove_CMap_From_Linked_List(cmap_t *cmp)
{
    /* Make sure colormap exists in the list. */
    cmp = Find_CMap_In_Linked_List(cmp);

    if (cmp != NULL) {
	/* Remove link from the list. */
	if (cmp->next_cmap != NULL)
	    cmp->next_cmap->prev_cmap = cmp->prev_cmap;
	if (cmp->prev_cmap != NULL) {
	    cmp->prev_cmap->next_cmap = cmp->next_cmap;
	} else {
	    cmap_list_start = cmp->next_cmap;
	}
	/* If global pointers point at this plot, reassign them. */
	if (current_cmap == cmp) {
#if 0 /* Make some other cmap current. */
	    if (cmp->prev_cmap != NULL)
		current_cmap = cmp->prev_cmap;
	    else
		current_cmap = psp->next_cmap;
#else /* No current cmap. */
	    current_cmap = &default_cmap;
#endif
	}
	/* Remove any memory for pixels and memory for colormap structure. */
	ReleaseColormap(cmp);
    }
}


/*-----------------------------------------------------------------------------
 *   Find_CMap_In_Linked_List - Search for the color map in the linked list.
 *---------------------------------------------------------------------------*/

static cmap_t *
Find_CMap_In_Linked_List(cmap_t *colormap)
{
    cmap_t *cmp = cmap_list_start;

    while (cmp != NULL) {
	if (cmp == colormap)
	    break;
	cmp = cmp->next_cmap;
    }

    return cmp;
}


/*-----------------------------------------------------------------------------
 *   cmaps_differ - Compare two colormaps, return 1 if differ.
 *---------------------------------------------------------------------------*/

static int
cmaps_differ(cmap_t *cmap1, cmap_t *cmap2)
{

    /* First compare non-pointer elements. */
    if ( memcmp(&(cmap1->colors[0]), &(cmap2->colors[0]), (long)&(cmap1->pixels)-(long)&(cmap1->colors[0])) )
	return 1;

    /* Now compare pointer elements. */
    if (cmap1->allocated) {
	if (cmap1->pixels && cmap2->pixels) {
	    if ( memcmp(cmap1->pixels, cmap2->pixels, cmap1->allocated*sizeof(cmap1->pixels[0])) )
		return 1;
	} else
	    return 1;
    }

    return 0;  /* They are the same. */

}

/*
 * Shared code for setting fill style
 */
#define plot current_plot
static void
x11_setfill(GC *gc, int style)
{
    int fillpar = style >> 4;
    XColor xcolor, bgnd;
    float dim;
    int idx;

    style = style & 0xf;

	switch (style) {
	case FS_SOLID:
	case FS_TRANSPARENT_SOLID:
	    /* filldensity is from 0..100 percent */
	    if (fillpar >= 100)
		break;
	    dim = (double)(fillpar)/100.;
	    /* use halftone fill pattern according to filldensity */
	    xcolor.red = (double)(0xffff) * (double)((plot->current_rgb >> 16) & 0xff) /255.;
	    xcolor.green = (double)(0xffff) * (double)((plot->current_rgb >> 8) & 0xff) /255.;
	    xcolor.blue = (double)(0xffff) * (double)(plot->current_rgb & 0xff) /255.;
	    bgnd.red = (double)(0xffff) * (double)((plot->cmap->rgbcolors[0] >> 16) & 0xff) /255.;
	    bgnd.green = (double)(0xffff) * (double)((plot->cmap->rgbcolors[0] >> 8) & 0xff) /255.;
	    bgnd.blue = (double)(0xffff) * (double)(plot->cmap->rgbcolors[0] & 0xff) /255.;
	    xcolor.red   = dim*xcolor.red   + (1.-dim)*bgnd.red;
	    xcolor.green = dim*xcolor.green + (1.-dim)*bgnd.green;
	    xcolor.blue  = dim*xcolor.blue  + (1.-dim)*bgnd.blue;
	    FPRINTF((stderr,"Dimming poly color %.6x by %.2f to %2d %2d %2d\n",
			(unsigned long)(plot->current_rgb), dim, xcolor.red, xcolor.green, xcolor.blue));
	    if (XAllocColor(dpy, plot->cmap->colormap, &xcolor))
		XSetForeground(dpy, *gc, xcolor.pixel);
	    break;
	case FS_PATTERN:
	case FS_TRANSPARENT_PATTERN:
	    /* use fill pattern according to fillpattern */
	    idx = (int) fillpar;	/* fillpattern is enumerated */
	    if (idx < 0)
		idx = 0;
	    idx = idx % stipple_pattern_num;
	    XSetStipple(dpy, *gc, stipple_pattern[idx]);
	    if (style == FS_TRANSPARENT_PATTERN)
		XSetFillStyle(dpy, *gc, FillStippled);
	    else
		XSetFillStyle(dpy, *gc, FillOpaqueStippled);
	    XSetBackground(dpy, *gc, plot->cmap->colors[0]);
	    break;
	case FS_EMPTY:
	    /* fill with background color */
	    XSetFillStyle(dpy, *gc, FillSolid);
	    XSetForeground(dpy, *gc, plot->cmap->colors[0]);
	    break;
	default:
	    /* fill with current color */
	    XSetFillStyle(dpy, *gc, FillSolid);
	    break;
	}
}
#undef plot


/* -------------------------------------------------------
 * Bookkeeping for clickable hot spots and hypertext anchors
 * --------------------------------------------------------*/

#ifdef USE_MOUSE
/* Initialize boxes starting from i */
static void x11_initialize_key_boxes(plot_struct *plot, int i)
{
	for (; i < plot->x11_max_key_boxes; i++) {
		plot->x11_key_boxes[i].left = plot->x11_key_boxes[i].ybot = INT_MAX;
		plot->x11_key_boxes[i].right = plot->x11_key_boxes[i].ytop = 0;
	}
}
static void x11_initialize_hidden(plot_struct *plot, int i)
{
	for (; i < plot->x11_max_key_boxes; i++)
		plot->x11_key_boxes[i].hidden = FALSE;
}


/* Update the box enclosing the key sample for the current plot
 * so that later we can detect mouse clicks in that area
 */
static void x11_update_key_box( plot_struct *plot, unsigned int x, unsigned int y )
{
	x11BoundingBox *bb;
	if (plot->x11_max_key_boxes <= x11_cur_plotno) {
		plot->x11_max_key_boxes = x11_cur_plotno + 10;
		plot->x11_key_boxes = (x11BoundingBox *)realloc(plot->x11_key_boxes,
				plot->x11_max_key_boxes * sizeof(x11BoundingBox));
		x11_initialize_key_boxes(plot, x11_cur_plotno);
		x11_initialize_hidden(plot, x11_cur_plotno);
	}
	bb = &(plot->x11_key_boxes[x11_cur_plotno]);
	if (x < bb->left)  bb->left = x;
	if (x > bb->right) bb->right = x;
	if (y < bb->ybot)  bb->ybot = y;
	if (y > bb->ytop)  bb->ytop = y;
}

/* Called from x11Panel::OnLeftDown
 * If the mouse click was on top of a key sample then toggle the
 * corresponding plot on/off
 */
static int x11_check_for_toggle(plot_struct *plot, unsigned int x, unsigned int y)
{
	int i;
	int hit = 0;
	for (i = 1; i <= x11_cur_plotno && i < plot->x11_max_key_boxes; i++) {
		if (plot->x11_key_boxes[i].left == INT_MAX)
			continue;
		if (x < plot->x11_key_boxes[i].left)
			continue;
		if (x > plot->x11_key_boxes[i].right)
			continue;
		if (y < plot->x11_key_boxes[i].ybot)
			continue;
		if (y > plot->x11_key_boxes[i].ytop)
			continue;
		plot->x11_key_boxes[i].hidden = !plot->x11_key_boxes[i].hidden;
		hit++;
	}
    return hit;
}

#endif /*USE_MOUSE*/
