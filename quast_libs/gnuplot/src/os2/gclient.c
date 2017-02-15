#ifdef INCRCSDATA
static char RCSid[]="$Id: gclient.c,v 1.51.4.1 2014/12/28 12:54:58 markisch Exp $";
#endif

/****************************************************************************

    PROGRAM: Gnupmdrv

    MODULE:  gclient.c

    This file contains the client window procedures for Gnupmdrv

****************************************************************************/

/* PM driver for GNUPLOT */

/*[
 * Copyright 1992, 1993, 1998, 2004   Roger Fearick
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

/*
 * AUTHOR
 *
 *   Gnuplot driver for OS/2:  Roger Fearick
 *
 *
 * CONTRIBUTIONS:
 *
 *   Petr Mikulik  (see PM labels)
 *       - menu item to keep aspect ratio on/off (October 1997)
 *       - mouse support (1998, 1999); changes made after gnuplot 3.7.0.5:
 *	    - use gnuplot's pid in the name of shared memory (11. 5. 1999)
 *	    - mouse in maps; distance in polar coords (14. 5. 1999)
 *	    - event semaphore for sending data via shared memory;
 *	      zooming completely revised;
 *	      remap keys in 'case WM_CHAR';
 *	      Ctrl-C for breaking long drawings;
 *	      new menu items under 'Mouse' and 'Utilities' (August 1999)
 *	- rewrite of mouse support for the new scheme common with X11
 *	      (October 1999 - January 2000)
 *      - pm3d stuff (since January 1999)
 *
 *   Franz Bakan
 *       - communication gnupmdrv -> gnuplot via shared memory (April 1999)
 *       - date and time on x axis (August 1999)
 */

#define INCL_PM
#define INCL_WIN
#define INCL_SPL
#define INCL_SPLDOSPRINT
#define INCL_WINSTDFONT
#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#define INCL_DOSNMPIPES
#define INCL_DOSSESMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSQUEUES
#define INCL_WINSWITCHLIST
#define INCL_GPIPRIMITIVES
#include <os2.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <unistd.h>
#include <process.h>
#include <signal.h>
#include <time.h>
#include "config.h"
#include "term_api.h"
#include "gnupmdrv.h"
#include "pm_msgs.h"
#include "mousecmn.h"


/*==== m i s c e l l a n e o u s =============================================*/

/* February 2008, according to the gnuplot bugs report #1773922
   The standard font selection dialog is somehow broken after saving options.
   Use the font palette dialog instead.
*/
/* #define STANDARD_FONT_DIALOG 1 */
#undef STANDARD_FONT_DIALOG

/*==== d e b u g g i n g =====================================================*/

#if 0
# include "pmprintf.h"
# define DEBUG_IMAGE(a) PmPrintf a
#else
# define DEBUG_IMAGE(a)
#endif

#if 0
# include "pmprintf.h"
# define DEBUG_COLOR(a) PmPrintf a
#else
# define DEBUG_COLOR(a)
#endif


/*==== l o c a l    d a t a ==================================================*/

static long lLineTypes[7] = {
    LINETYPE_SOLID,
    LINETYPE_SHORTDASH,
    LINETYPE_DOT,
    LINETYPE_DASHDOT,
    LINETYPE_LONGDASH,
    LINETYPE_DOUBLEDOT,
    LINETYPE_DASHDOUBLEDOT
};

static long lCols[16] = {
    CLR_BLACK,
    CLR_DARKGRAY,
    CLR_RED,
    CLR_GREEN,
    CLR_BLUE,
    CLR_PINK,
    CLR_CYAN,
    CLR_BROWN,
    CLR_YELLOW,
    CLR_DARKBLUE,
    CLR_DARKRED,
    CLR_DARKGREEN,
    CLR_DARKCYAN,
    CLR_DARKPINK,
    CLR_PALEGRAY,
    CLR_WHITE
};

static long lCols_init = 0;
static long lCols_num = 16;
static long bkColor = -1;

/* Note that there are 16 colours used for drawing. The two more
   entries in this array were added by Ilya to avoid drawing white
   line on white background. */
#define nColors 16
static LONG alColourTable[nColors + 2];

static BOOL bPMPaletteMode = TRUE; /* do we use PM palette manager? */

/* RGB equivalents of all normal CLR_xxx colour constants
   Note: First entry corresponds to CLR_WHITE, which is negative (-2)!
*/
static LONG rgb_colors[18];
/* macro to facilitate translation */
#define RGB_TRANS(v) rgb_colors[(v)-CLR_WHITE]

/* A value of 0 indicates direct rgb colors without palette */
/* FIXME: gnupmdrv crashes if this is > GNUBUF/4 ! */
/* FIXME: this must be <= 256 (see PM_SET_COLOR)   */
#define   RGB_PALETTE_SIZE 0    /* size of the 'virtual' palette used for 
				   translation of index to RGB value */

/* FIXME: increasing GNUBUF circumvents a bug/limitation in BufRead:
   it cannot read datablocks larger than GNUBUF */
#define   GNUBUF    131072
//#define   GNUBUF    2048        /* buffer for gnuplot commands */
#define   PIPEBUF   4096        /* size of pipe buffers */
#define   CMDALLOC  4096        /* command buffer allocation increment (ints) */

#define   PAUSE_DLG 1           /* pause handled in dialog box */
#define   PAUSE_BTN 2           /* pause handled by menu item */
#define   PAUSE_GNU 3           /* pause handled by Gnuplot */

#define   DEFLW     50

// ULONG    ppidGnu = 0L;  /* gnuplot pid  */ -- now in gpexecute.c
static HDC      hdcScreen;
static HPS      hpsScreen;     /* screen pres. space */
static int      iSeg = 1;

static HSWITCH hSwitch = 0;    /* switching between windows */
static SWCNTRL swGnu;

static BOOL     bLineTypes = FALSE;    // true if use dashed linetypes
static BOOL     bColours = TRUE;
static BOOL     bShellPos = FALSE;
static BOOL     bPopFront = TRUE;
static BOOL     bKeepRatio = TRUE;	//PM
#if 0
static BOOL     bNewFont = FALSE;
#endif

static double   multLineHor  = 1.; /* Horizontal and vertical spacing shifts */
static double   multLineVert = 0.; /* for multiline prints.		    */

static int	codepage = 0;

static int 	ulMouseSprintfFormatItem = IDM_MOUSE_FORMAT_XcY;
static BOOL	bSend2gp = FALSE;
static const  char  *SetDataStyles[] = {
    "boxes", "dots", "fsteps", "histeps", "impulses",
    "lines", "linespoints", "points", "steps"
};

struct {
    int on;  // set to 1 during zooming
    POINTL from, to; // corners
} zoombox = { 0 };


static int zooming = 0;  // set to 1 during zooming
static POINTL zoomrect_from, zoomrect_now;

static ULONG    ulPlotPos[4];
static ULONG    ulShellPos[4];
static PAUSEDATA pausedata = {sizeof(PAUSEDATA), NULL, NULL};
static char     szFontNameSize[FONTBUF];	/* default name and size, format: "10.Helvetica" */
/* FIXME: this might not always get updated on font change */
static char	szCurrentFontNameSize[FONTBUF];	/* currently selected font */
static PRQINFO3 infPrinter = { "" };
static QPRINT   qPrintData = {
    sizeof(QPRINT), 0.0, 0.0, 1.0, 1.0, 0,
    "", "", &infPrinter, 0, NULL
};
//static HEV      semStartSeq;   /* semaphore to start things in right sequence */
static HEV      semPause;
static HMTX     semHpsAccess;
static ULONG    ulPauseReply = 1;
static ULONG    ulPauseMode  = PAUSE_DLG;

static HWND     hSysMenu;
            /* stuff for screen-draw thread control */

//static HEV      semDrawDone;

            /* thread control */

static TID     tidDraw, tidSpawn;

static int breakDrawing = 0;
            /* //PM flag for stopping (long) drawings */

            /* font data */

static int lSupOffset = 0;
static int lSubOffset = 0;
static int lBaseSupOffset = 0;
static int lBaseSubOffset = 0;
static int lCharWidth = 217;
static int lCharHeight = 465;


//PM: Now variables for mouse

/* useMouse is set to 1 when user switches mousing on, e.g. the mouse is
   allowed
*/
static int useMouse = 0;

/* gnuplot's PM terminal sends GR_MOUSECAPABLE message from its init routine, which
   sets the variable below to 1. Then we are sure that we talk to the
   mouseable terminal and can read the mouseable data from the pipe.
   Non-mouseable versions of PM terminal or non-new-gnuplot programs
   using gnupmdrv will let this variable set to 0, thus no mousing occurs.
*/
static char mouseTerminal = 0;


/* Lock (hide) mouse when building the plot (redrawing screen).
   Otherwise gnupmdrv would crash when trying to display mouse position
   in a window not yet plotted.
*/
static char lock_mouse = 1;

/* Structure for the ruler: on/off, position,...
*/
static struct {
   int on;
   int x, y;  /* ruler position */
} ruler = {0,0,0,};

// Pointer definition
HWND hptrDefault, hptrCrossHair, hptrScaling,
    hptrRotating, hptrZooming, hptrCurrent;

// After passing gpPMmenu in PM_pipe and SET_GRAPHICS, it seems not to be possible
// to update the menu. Thus this flag is set on and menu is updated
// afterwards.
struct t_gpPMmenu gpPMmenu;
int gpPMmenu_update_req = 0;

// colours of mouse-relating drawings (CLR_DEFAULT is not allowed!)
#define COLOR_MOUSE    CLR_BLACK    // mouse position
#define COLOR_ANNOTATE CLR_BLACK    // annotating strings (MB3)
#define COLOR_RULER    CLR_DARKPINK // colour of the ruler
#define COLOR_ERROR    CLR_RED      // colour of error messages

static enum JUSTIFY jmode;

static SIZEF sizBaseSubSup;
static SIZEF sizCurSubSup;
static SIZEF sizCurFont;
static long lVOffset = 0;
static SIZEF sizBaseFont;

static struct _ft {
    char *name;
    LONG  lcid;
} tabFont[256] = {
    {NULL,0L},
    {NULL}
};


/*==== f u n c t i o n s =====================================================*/

int             DoPrint(HWND);
HBITMAP         CopyToBitmap(HPS);
HMF             CopyToMetaFile(HPS);
MRESULT         WmClientCmdProc(HWND , ULONG, MPARAM, MPARAM);
void            ChangeCheck(HWND, USHORT, USHORT);
BOOL            QueryIni(HAB);
static void     SaveIni(HWND);
static void     ThreadDraw(void*);
static void     DoPaint(HWND, HPS);
static void     SelectFont(HPS, char *);
static void     SwapFont(HPS, char *);
static void     CopyToClipBrd(HWND);
static void     ReadGnu(void*);
static void     EditLineTypes(HWND, HPS, BOOL);
#if 0
static void     EditCharCell(HPS, SIZEF*);
#endif
static HPS      InitScreenPS(void);
static int      BufRead(HFILE, void*, int, PULONG);
static int      GetNewFont(HWND, HPS);
void            SigHandler(int);
static void     FontExpand(char *);
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
static char    *ParseText(HPS, char *, BOOL, char *, int, int, BOOL, BOOL);
static void     CharStringAt(HPS, int, int, int, char *);
static int      QueryTextBox(HPS, int, char *);
#endif
static void     LMove(HPS hps, POINTL *p);
static void     LLine(HPS hps, POINTL *p);
static void     LType(int iType);

/* Functions related to the mouse processing */
static void     TextToClipboard(PCSZ);
static void     GetMousePosViewport(HWND hWnd, int *mx, int *my);
static void     MousePosToViewport(int *x, int *y, SHORT mx, SHORT my);
static void     DisplayStatusLine(HPS hps);
static void     UpdateStatusLine(HPS hps, char *text);
static void     gpPMmenu_update(void);
static void     DrawZoomBox(void);
static void     DrawRuler(void);

#define IGNORE_MOUSE (!mouseTerminal || useMouse==0 || lock_mouse)
/* || !gp4mouse.graph */
/*  don't react to mouse in the event handler, and avoid some crashes */

/* Drag'n'Drop support from PMEmacs */
struct drop {
    unsigned long cookie;
    unsigned len;
    char str[CCHMAXPATH];
};

/* Circular buffer of objects recently dropped.  */

#define DROP_MAX 8
static int drop_count = 0;

/* Each drop action is assigned a number, for relating drop events
   with PMR_DROP requests. */

/* Finally, include the common mousing declarations and routines: */
#include "../gpexecute.h"

/* End of new functions related to the mouse processing */

/*==== c o d e ===============================================================*/

/* An object is being dragged over the client window.  Check whether
   it can be dropped or not. */

void
report_error(HWND hWnd, char* s)
{
      HPS hps;
      POINTL pt;
      RECTL rc;

      hps = WinGetPS(hWnd);
      GpiSetMix(hps, FM_OVERPAINT);
      /* clear the rectangle below the text */
      /* GpiSetColor(hps, RGB_TRANS(CLR_CYAN)); */
      GpiSetColor(hps, RGB_TRANS(CLR_BACKGROUND));
      pt.x = 0;
      pt.y = 0;
      GpiMove(hps,&pt);
      /* clear the whole line */
      GpiQueryPageViewport(hpsScreen,&rc);
      pt.x = rc.xRight;
      pt.y = 16;
      GpiBox(hps, DRO_FILL, &pt, 0,0);
      /* now write the mouse position on the screen */
      GpiSetColor(hps, RGB_TRANS(COLOR_ERROR)); /* set text color */
      GpiSetCharMode(hps,CM_MODE1);

      pt.x = 2;
      pt.y = 2;
      GpiCharStringAt(hps,&pt,(long)strlen(s),s);
      WinReleasePS(hps);
}

MRESULT
drag_over(HWND hWnd, PDRAGINFO pDraginfo)
{
    PDRAGITEM pditem;
    USHORT indicator, operation;

/* Redefine this macro for debugging. */
#define DRAG_FAIL(x) report_error(hWnd,x)

    indicator = DOR_NODROPOP; operation = DO_COPY;
    if (!DrgAccessDraginfo(pDraginfo))
	DRAG_FAIL("DrgAccessDraginfo failed");
    else if (!(pDraginfo->usOperation == DO_DEFAULT
	       || pDraginfo->usOperation == DO_COPY))
	DRAG_FAIL("Invalid operation");
    else if (DrgQueryDragitemCount(pDraginfo) < 1)
	DRAG_FAIL("Invalid count");
    else if (DrgQueryDragitemCount(pDraginfo) > DROP_MAX - drop_count)
	DRAG_FAIL("Circular buffer full");
    else {
	pditem = DrgQueryDragitemPtr(pDraginfo, 0);
	if (!(pditem->fsSupportedOps & DO_COPYABLE))
	    DRAG_FAIL("Not copyable");
	else if (!DrgVerifyRMF(pditem, "DRM_OS2FILE", NULL))
	    DRAG_FAIL("DrgVerifyRMF failed");
	else {
	    /* The object can be dropped (copied). */
	    indicator = DOR_DROP; operation = DO_COPY;
	}
    }
    DrgFreeDraginfo(pDraginfo);
    return (MRFROM2SHORT(indicator, operation));
}


/* An object is dropped on the client window. */

MRESULT
drag_drop(HWND hwnd, PDRAGINFO pDraginfo)
{
  PDRAGITEM pditem;
  POINTL ptl;
  char name[CCHMAXPATH];
  char path[CCHMAXPATH];
  char *p;
  int count, idx, len;

  DrgAccessDraginfo(pDraginfo);
  ptl.x = pDraginfo->xDrop; ptl.y = pDraginfo->yDrop;
  WinMapWindowPoints(HWND_DESKTOP, hwnd, &ptl, 1);
  count = DrgQueryDragitemCount(pDraginfo);
  for (idx = 0; idx < count && drop_count < DROP_MAX; ++idx) {
      pditem = DrgQueryDragitemPtr(pDraginfo, idx);
      DrgQueryStrName(pditem->hstrContainerName, sizeof(path), path);
      DrgQueryStrName(pditem->hstrSourceName, sizeof(name), name);
      DrgSendTransferMsg(pditem->hwndItem, DM_ENDCONVERSATION,
			 MPFROMLONG(pditem->ulItemID),
			 MPFROMSHORT(DMFL_TARGETSUCCESSFUL));
      len = strlen(path);
      if (len >= 1 && strchr("\\/:", path[len-1]) == NULL)
	  path[len++] = '/';
      if (len + strlen(name) + 1 <= sizeof(path)) {
	  strcpy(path + len, name);
	  for (p = path; *p != 0; ++p)
	      if (*p == '\\')
		  *p = '/';
	  if (input_from_PM_Terminal) {
	      sprintf(input_from_PM_Terminal,
		      "set loadpath \"%.*s\"; load \"%s\"",
		      len, path, path);
	      gp_execute(0);
	  }
      } else
	  report_error(hwnd, "Dropped path name too long");
  } /* for(idx) */

  DrgDeleteDraginfoStrHandles(pDraginfo);
  DrgFreeDraginfo(pDraginfo);
  return(0);
}

#if 0
/* A color has been dropped on a frame(or the background color has
   been changed with WinSetPresParam). */

static void
drop_color(HWND hwnd)
{
    RGB rgb;
    char buf[3];

    if (WinQueryPresParam(hwnd, PP_BACKGROUNDCOLOR, 0, NULL,
			  sizeof(rgb), &rgb,
			  QPF_NOINHERIT | QPF_PURERGBCOLOR) == sizeof(rgb)
	) {
	buf[0] = rgb.bRed;
	buf[1] = rgb.bGreen;
	buf[2] = rgb.bBlue;
#if CAN_DROP_COLOR
	send_drop_color(buf, 3);
#endif
    }
}
#endif

/*
**  Window proc for main window
**  -- passes most stuff to active child window via WmClientCmdProc
**  -- passes DDE messages to DDEProc
*/
MRESULT
EXPENTRY DisplayClientWndProc(HWND hWnd, ULONG message, MPARAM mp1, MPARAM mp2)
{
    static RECTL   rectlPaint = { 0, 0, 0, 0 };
    static int     iPaintCount = 0;
    static int     firstcall = 1;
    static int     prev_mx = 0, prev_my = 0;		/* previous mouse position */
    int            mx, my;				/* current mouse position  */

#if 1
    GetMousePosViewport(hWnd, &mx, &my);
#else
    /* The following cannot be used because `message' is not OK for
     * WM_CHAR */
    /* macro which gets mouse coords in pixels */
    mx = MOUSEMSG(&message)->x;
    my = MOUSEMSG(&message)->y;
    /* viewport units --- this trashes mx,my! */
    MousePosToViewport(&mx, &my, mx, my);
#endif

    /* Graphics part, SET_GRAPHICS, required to update menu */
    if (gpPMmenu_update_req)
	gpPMmenu_update();

    /* mouse events first */
    switch (message) {
    case WM_MOUSEMOVE:
	if (IGNORE_MOUSE) {
	    WinSetPointer(HWND_DESKTOP, hptrDefault); /* set default pointer */
	    return 0L;
	}
	/* was the mouse moved? */
	if ((prev_mx != mx) || (prev_my != my)) {
#if 1
	    WinSetPointer(HWND_DESKTOP, hptrCurrent);
#else
	    WinSetPointer(HWND_DESKTOP, hptrCrossHair);
#endif
	    if (zoombox.on) {
		DrawZoomBox(); /* erase zoom box */
		zoombox.to.x = mx; zoombox.to.y = my;
		DrawZoomBox(); /* draw new zoom box */
	    }
	    /* track(show) mouse position -- send the event to gnuplot */
	    gp_exec_event(GE_motion, mx, my, 0, 0, 0);
	}
	prev_mx = mx;
	prev_my = my;
	return 0L; /* end of WM_MOUSEMOVE */

    case WM_BUTTON1DOWN:
	WinSetFocus(HWND_DESKTOP, hWnd);
	if (! IGNORE_MOUSE)
	    gp_exec_event(GE_buttonpress, mx, my, 1, 0, 0);
	return 0L;

    case WM_BUTTON2DOWN:
	WinSetFocus(HWND_DESKTOP, hWnd);
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonpress, mx, my, 3, 0, 0);
	return 0L;

    case WM_BUTTON3DOWN:
	WinSetFocus(HWND_DESKTOP, hWnd);
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonpress, mx, my, 2, 0, 0);
	return 0L;

    case WM_BUTTON1DBLCLK:
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonrelease, mx, my, 1, 0, 0);
	return 0L;

    case WM_BUTTON2DBLCLK:
	if (!IGNORE_MOUSE)
	    /* Note: 9999 should be replaced by time! */
	    gp_exec_event(GE_buttonrelease, mx, my, 3, 9999, 0);
	return 0L;

    case WM_BUTTON3DBLCLK:
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonrelease, mx, my, 2, 9999, 0);
	return 0L;

#if 1
    case WM_BUTTON1UP:
#else
    case WM_BUTTON1CLICK:
#endif
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonrelease, mx, my, 1, 9999, 0);
	return 0L;

#if 0
    case WM_BUTTON2UP:
#else
    case WM_BUTTON2CLICK:
#endif
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonrelease, mx, my, 3, 9999, 0);
	return 0L;

#if 1
    case WM_BUTTON3UP:
#else
    case WM_BUTTON3CLICK:
#endif
	if (!IGNORE_MOUSE)
	    gp_exec_event(GE_buttonrelease, mx, my, 2, 9999, 0);
	return 0L;

    } /* switch over mouse events */


    switch (message) {
    case WM_CREATE:
    {
	SIZEL   sizlPage;
	LONG    NumColors;
	LONG    PalSupport;

	/* set initial values */
	ChangeCheck(hWnd, IDM_LINES_THICK, bWideLines?IDM_LINES_THICK:0);
	ChangeCheck(hWnd, IDM_LINES_SOLID, bLineTypes?0:IDM_LINES_SOLID);
	ChangeCheck(hWnd, IDM_COLOURS, bColours?IDM_COLOURS:0);
	ChangeCheck(hWnd, IDM_FRONT, bPopFront?IDM_FRONT:0);
	ChangeCheck(hWnd, IDM_KEEPRATIO, bKeepRatio?IDM_KEEPRATIO:0);
	ChangeCheck(hWnd, IDM_USEMOUSE, useMouse?IDM_USEMOUSE:0);
#if 0
	ChangeCheck(hWnd, IDM_MOUSE_POLAR_DISTANCE, mousePolarDistance?IDM_MOUSE_POLAR_DISTANCE:0);
#endif

	/* disable close from system menu(close only from gnuplot) */
	hApp = WinQueryWindow(hWnd, QW_PARENT); /* temporary assignment.. */
	hSysMenu = WinWindowFromID(hApp, FID_SYSMENU);
	/* setup semaphores */
	/* DosCreateEventSem(NULL, &semDrawDone, 0L, 0L); */
	/* DosCreateEventSem(NULL, &semStartSeq, 0L, 0L); */
	DosCreateEventSem(NULL, &semPause, 0L, 0L);
	DosCreateMutexSem(NULL, &semHpsAccess, 0L, 1L);

	/* create a dc and hps to draw on the screen */
	hdcScreen = WinOpenWindowDC(hWnd);

	/* How many colors can be displayed ? */
	DevQueryCaps(hdcScreen, CAPS_COLORS, 1, &NumColors);
	/* Is Palette Manager supported at all? */
	DevQueryCaps(hdcScreen, CAPS_ADDITIONAL_GRAPHICS, 1, &PalSupport);

	/* Determine if PM Palette Manager should be used */
	bPMPaletteMode = (PalSupport & CAPS_PALETTE_MANAGER) &&
			 (NumColors <= 256);
	DEBUG_COLOR(( "WM_CREATE: colors = %i, pm = %i, rgb = %i",
		NumColors, PalSupport & CAPS_PALETTE_MANAGER, !bPMPaletteMode ));

	sizlPage.cx = 0; sizlPage.cy = 0;
	sizlPage.cx = 19500; sizlPage.cy = 12500;
	hpsScreen = GpiCreatePS(hab, hdcScreen, &sizlPage,
				PU_HIMETRIC|GPIT_NORMAL|GPIA_ASSOC);
	/* spawn server for GNUPLOT ... */
	tidSpawn = _beginthread(ReadGnu, NULL, 32768, NULL);
	/* initialize pointers */
	hptrDefault = WinQuerySysPointer(HWND_DESKTOP, SPTR_ARROW, FALSE);
	hptrCrossHair = WinLoadPointer(HWND_DESKTOP,(ULONG)0, IDP_CROSSHAIR);
	hptrScaling = WinLoadPointer(HWND_DESKTOP,(ULONG)0, IDP_SCALING);
	hptrRotating = WinLoadPointer(HWND_DESKTOP,(ULONG)0, IDP_ROTATING);
	hptrZooming = WinQuerySysPointer(HWND_DESKTOP, SPTR_MOVE, FALSE);
	hptrCurrent = hptrCrossHair;

	break;
    }

    case WM_GPSTART:
	/* Show the Mouse menu if connected to mouseable PM terminal */
	if (1 || mouseTerminal) /* PM: workaround for a bug---SEE "BUGS 2" IN README!!! */
	    /* if (mouseTerminal) */
	    WinEnableMenuItem(WinWindowFromID(
				  WinQueryWindow(hApp, QW_PARENT),
				  FID_MENU),
			      IDM_MOUSE, TRUE);
	if (!input_from_PM_Terminal) { /* no feedback */
#define NOFEEDBACK(X)							\
	    WinEnableMenuItem(WinWindowFromID(				\
				  WinQueryWindow(hApp, QW_PARENT),	\
				  FID_MENU),				\
			      X, FALSE)
	    NOFEEDBACK(IDM_SET_GRID);
	    NOFEEDBACK(IDM_SET_LINLOGY);
	    NOFEEDBACK(IDM_SET_AUTOSCALE);
	    NOFEEDBACK(IDM_DO_REPLOT);
	    NOFEEDBACK(IDM_DO_RELOAD);
	    NOFEEDBACK(IDM_DO_SENDCOMMAND);
	    NOFEEDBACK(IDM_SET);
#undef NOFEEDBACK
	}

	/* get details of command-line window */
	hSwitch = WinQuerySwitchHandle(0, ppidGnu);
	WinQuerySwitchEntry(hSwitch, &swGnu);

	if (firstcall) {
	    /* set size of this window */
            WinSetWindowPos(
		WinQueryWindow(hWnd, QW_PARENT),
		bPopFront?HWND_TOP:swGnu.hwnd,
		ulShellPos[0],
		ulShellPos[1],
		ulShellPos[2],
		ulShellPos[3],
		bShellPos
		? (bPopFront
		   ? SWP_SIZE|SWP_MOVE|SWP_SHOW|SWP_ACTIVATE
		   : SWP_SIZE|SWP_MOVE|SWP_SHOW|SWP_ZORDER)
		: (bPopFront
		   ? SWP_SHOW|SWP_ACTIVATE
		   : SWP_SHOW|SWP_ZORDER));
            signal(SIGTERM, SigHandler);
	    firstcall = 0;
	}
	if (!bPopFront) WinSwitchToProgram(hSwitch);
	/* DosPostEventSem(semDrawDone); */
	DosReleaseMutexSem(semHpsAccess);
	return 0;

    case WM_COMMAND:
	return WmClientCmdProc(hWnd, message, mp1, mp2);

    case WM_CHAR:
    {
	/* Note: doc for WM_CHAR is available in PMMSG.INF */
	USHORT usFlag = SHORT1FROMMP(mp1); /* keyboard control codes */
	SHORT  key = SHORT2FROMMP(mp2);    /* virtual key code */
	static int last_modifier_mask = -99;
	int modifier_mask =
	    ((usFlag & KC_SHIFT) ? Mod_Shift : 0)
	    | ((usFlag & KC_CTRL) ? Mod_Ctrl : 0)
	    | ((usFlag & KC_ALT) ? Mod_Alt : 0);

	if (modifier_mask != last_modifier_mask) {
	    gp_exec_event(GE_modifier, mx, my, modifier_mask, 0, 0);
	    last_modifier_mask = modifier_mask;
	}
#if 0
	if (!(usFlag & KC_SCANCODE))
	    return 0L; // only modifier mask has changed */
#endif
	if (usFlag & KC_KEYUP)
	    return 0L;   /* ignore key release events */
#if 0
	{
	    FILE *ff = fopen("deb", "a");
	    fprintf(ff, "key = %i c=%c\n", (int) key, (char) key);
	    fclose(ff);
	}
#endif

	switch (key) {

	case VK_SPACE: {
#ifndef DISABLE_SPACE_RAISES_CONSOLE
	    /* raise gnuplot's window */
	    HWND hw = WinQueryWindow(swGnu.hwnd, QW_BOTTOM);
	    WinSetFocus(HWND_DESKTOP, hw);
	    WinSendMsg(hw, message,
		       MPFROM2SHORT((USHORT)(KC_SCANCODE), 1),
		       0 /* MPFROMSHORT(key) */
		);
	    WinSwitchToProgram(hSwitch);
#else
	    key = ' ';
#endif /* DISABLE_SPACE_RAISES_CONSOLE */
	    break;
	}
	    /* remap virtual keys to gnuplot's codes: */
	case VK_BACKSPACE:
	    key = GP_BackSpace;
	    break;
	case VK_TAB:
	    key = GP_Tab;
	    break;
	case VK_NEWLINE:
	    key = GP_Return;
	    break;
	case VK_PAUSE:
	    key = GP_Pause;
	    break;
	case VK_SCRLLOCK:
	    key = GP_Scroll_Lock;
	    break;
	case VK_SYSRQ:
	    key = GP_Sys_Req;
	    break;
	case VK_ESC:
	    key = GP_Escape;
	    break;
	case VK_DELETE:
	    key = GP_Delete;
	    break;
	case VK_INSERT:
	    key = GP_KP_Insert;
	    break;
	case VK_HOME:
	    key = GP_Home;
	    break;
	case VK_LEFT:
	    key = GP_Left;
	    break;
	case VK_UP:
	    key = GP_Up;
	    break;
	case VK_RIGHT:
	    key = GP_Right;
	    break;
	case VK_DOWN:
	    key = GP_Down;
	    break;
	case VK_END:
	    key = GP_End;
	    break;
	case VK_PAGEUP:
	    key = GP_PageUp;
	    break;
	case VK_PAGEDOWN:
	    key = GP_PageDown;
	    break;
	case VK_F1:
	    key = GP_F1;
	    break;
	case VK_F2:
	    key = GP_F2;
	    break;
	case VK_F3:
	    key = GP_F3;
	    break;
	case VK_F4:
	    key = GP_F4;
	    break;
	case VK_F5:
	    key = GP_F5;
	    break;
	case VK_F6:
	    key = GP_F6;
	    break;
	case VK_F7:
	    key = GP_F7;
	    break;
	case VK_F8:
	    key = GP_F8;
	    break;
	case VK_F9:
	    key = GP_F9;
	    break;
	case VK_F10:
	    key = GP_F10;
	    break;
	case VK_F11:
	    key = GP_F11;
	    break;
	case VK_F12:
	    key = GP_F12;
	    break;
	case VK_SHIFT:
	case VK_CTRL:
	case VK_ALT:
	    gp_exec_event(GE_modifier, mx, my, modifier_mask, 0, 0);
	    return 0L;
	default:
	    key = SHORT1FROMMP(mp2); /* character key code */
	} /* switch(key) */

	if (key)
	    gp_exec_event(GE_keypress, mx, my, key, 0, 0);

	return 0L;
    } /*case(WM_CHAR) */

    case WM_DESTROY:
	if (WinSendMsg(hWnd, WM_USER_PRINT_QBUSY, 0L, 0L) != 0L) {
	    WinMessageBox(HWND_DESKTOP,
			  hWnd,
			  "Still printing - not closed",
			  APP_NAME,
			  0,
			  MB_OK | MB_ICONEXCLAMATION);
	    return 0L;
	}
	return(WinDefWindowProc(hWnd, message, mp1, mp2));

    case WM_PAINT:
    {
	ULONG ulCount;
	PID pid; TID tid;
	HPS hps_tmp;
	RECTL rectl_tmp;

	DosQueryMutexSem(semHpsAccess, &pid, &tid, &ulCount);
	if ((ulCount > 0) &&(tid != tidDraw)) {
	    /* simple repaint while building plot or metafile */
	    /* use temporary PS                   */
	    lock_mouse = 1; /* PM: this may help against gnupmdrv crashes */
	    hps_tmp = WinBeginPaint(hWnd,0,&rectl_tmp);
	    WinFillRect(hps_tmp,&rectl_tmp,CLR_BACKGROUND);
	    WinEndPaint(hps_tmp);
	    /* add dirty rectangle to saved rectangle     */
	    /* to be repainted when PS is available again */
	    WinUnionRect(hab,&rectlPaint,&rectl_tmp,&rectlPaint);
	    lock_mouse = 0;
	    iPaintCount ++;
	    break;
	}
	lock_mouse = 1;
	WinInvalidateRect(hWnd, &rectlPaint, TRUE);
	DoPaint(hWnd, hpsScreen);
	WinSetRectEmpty(hab, &rectlPaint);
	lock_mouse = 0;
	break;
    }

    case WM_SIZE :
	WinInvalidateRect(hWnd, NULL, TRUE);
	break;

    case WM_PRESPARAMCHANGED: {
	char *pp = malloc(FONTBUF);
	ULONG ulID;

	if (WinQueryPresParam(hWnd,
			      PP_FONTNAMESIZE,
			      0,
			      &ulID,
			      FONTBUF,
			      pp,
			      QPF_NOINHERIT) != 0L) {
	    strcpy(szFontNameSize, pp);
#if 0
	    bNewFont = TRUE;
#endif
	    WinInvalidateRect(hWnd, NULL, TRUE);
	}
	free(pp);
#ifndef STANDARD_FONT_DIALOG
	gp_execute("refresh");
#endif
	break;
    }

    case WM_USER_PRINT_BEGIN:
    case WM_USER_PRINT_OK :
    case WM_USER_DEV_ERROR :
    case WM_USER_PRINT_ERROR :
    case WM_USER_PRINT_QBUSY :
	return(PrintCmdProc(hWnd, message, mp1, mp2));

    case WM_GNUPLOT:
	/* display the plot */
	lock_mouse = 1;
	if (bPopFront) {
	    SWP swp; /* pop to front only if the window is not minimized */

	    if ((WinQueryWindowPos(hwndFrame,(PSWP) &swp) == TRUE)
		&&((swp.fl & SWP_MINIMIZE) == 0))
		WinSetWindowPos(hwndFrame, HWND_TOP, 0,0,0,0, SWP_ACTIVATE|SWP_ZORDER);
	}
	if (iPaintCount > 0) { /* if outstanding paint messages, repaint */
	    WinInvalidateRect(hWnd, &rectlPaint, TRUE);
	    iPaintCount = 0;
	}
	lock_mouse = 0;
	return 0L;

    case WM_PAUSEPLOT:
    {
	SWP swp; /* restore the window if it has been minimized */

	if ((WinQueryWindowPos(hwndFrame, &swp) == TRUE)
	    &&((swp.fl & SWP_MINIMIZE) != 0))
	    WinSetWindowPos(hwndFrame, HWND_TOP, 0,0,0,0,
			    SWP_RESTORE|SWP_SHOW|SWP_ACTIVATE);
	/* put pause message on screen, or enable 'continue' button */
	if (ulPauseMode == PAUSE_DLG) {
	    pausedata.pszMessage =(char*)mp1;
	    WinLoadDlg(HWND_DESKTOP,
		       hWnd,
		       (PFNWP)PauseMsgDlgProc,
		       0L,
		       IDD_PAUSEBOX,
		       &pausedata);
	} else {
	    WinEnableMenuItem(WinWindowFromID(
				  WinQueryWindow(hWnd, QW_PARENT), FID_MENU),
			      IDM_CONTINUE, TRUE);
	}
	return 0L;
    }

    case WM_PAUSEEND:
	/* resume plotting */
	ulPauseReply =(ULONG) mp1;
	DosPostEventSem(semPause);
	return 0L;

/* New event handles for mouse processing */

#if 0 /* already defined */
    case WM_MOUSEMOVE:
	return  0L;
#endif

#if 0
    case WM_BUTTON1DBLCLK:
	/* put the mouse coordinates to the clipboard */
	if (!IGNORE_MOUSE) {
	    SHORT mx = MOUSEMSG(&message)->x;
	    SHORT my = MOUSEMSG(&message)->y;
	    double x, y;
	    char s[256];
	    int frm = ulMouseSprintfFormatItem - IDM_MOUSE_FORMAT_X_Y;

	    /* Note: Another solution of getting mouse position
	     *(available at any method, not just in this handle
	     * event) is the following one:
	     *
	     * ok = WinQueryPointerPos(HWND_DESKTOP, &pt); // pt contains pos wrt desktop
	     * WinMapWindowPoints(HWND_DESKTOP, hWnd, &pt, 1); // pt contains pos wrt our hwnd window
	     * sprintf(s,"[%li,%li]",pt.x,pt.y);
	     */

	}
	return 0L; /* end of case WM_BUTTON1DBLCLK */
#endif

    case WM_BUTTON2CLICK: /* WM_BUTTON2UP: */
	/* make zoom */
	if (! IGNORE_MOUSE) {
	    POINTL tmp;
	    HPS hps = WinGetPS(hWnd);

	    if (pausing) { /* zoom is not allowed during pause */
		DosBeep(440,111);
		break;
	    }
	    DosBeep(555,155);
	    zoomrect_from.x = MOUSEMSG(&message)->x;
	    zoomrect_from.y = MOUSEMSG(&message)->y;
	    /* set opposite corner */
	    tmp.x = zoomrect_now.x = zoomrect_from.x + 50;
	    tmp.y = zoomrect_now.y = zoomrect_from.y + 50;
	    /* move mouse to opposite corner */
	    WinMapWindowPoints(hWnd, HWND_DESKTOP, &tmp, 1);
	    WinSetPointerPos(HWND_DESKTOP, tmp.x, tmp.y);
	    WinReleasePS(hps);
	    zooming = 1;
	}
	return 0; /* return from: case WM_BUTTON3DOWN(zoom) */

    case WM_BUTTON3UP: /* WM_BUTTON3DBLCLK: */
	/* write mouse position to screen */
	if (! IGNORE_MOUSE) {
	    SHORT mx = MOUSEMSG(&message)->x; /* mouse position */
	    SHORT my = MOUSEMSG(&message)->y;
	    char s[256];
	    POINTL pt;
	    HPS hps = WinGetPS(hWnd);

	    GpiSetColor(hps, RGB_TRANS(COLOR_ANNOTATE));    /* set color of the text */
	    GpiSetCharMode(hps,CM_MODE1);
	    pt.x = mx; pt.y = my+4;
	    GpiCharStringAt(hps,&pt,(long)strlen(s),s);
	    /* draw a cross at the clicked position */
	    pt.x = mx-3; pt.y = my; GpiMove(hps,&pt);
	    pt.x += 7; GpiLine(hps,&pt);
	    pt.x = mx; pt.y = my - 3; GpiMove(hps,&pt);
	    pt.y += 7; GpiLine(hps,&pt);
	    WinReleasePS(hps);
	}
	return 0L; /* end of case WM_BUTTON3... */

	/* End of new event handles for mouse processing */

#if 0
    case WM_PRESPARAMCHANGED:
	if (LONGFROMMP(mp1) == PP_BACKGROUNDCOLOR)
	    drop_color(hWnd);
	return 0;
#endif

    case DM_DRAGOVER:
	/* Determine whether the object can be dropped. */
	return(drag_over(hWnd, (PDRAGINFO) mp1));

    case DM_DROP:
	/* Drop an object. */
	return(drag_drop(hWnd, (PDRAGINFO) mp1));

    default:         /* Passes it on if unproccessed    */
	return(WinDefWindowProc(hWnd, message, mp1, mp2));
    } /* switch(message) */

    return(NULL);
}


#if 0 /* unused */
/*
 * Gets the pointer position(in pixels) in the window hWnd
 */
void
GetPointerPos(HWND hWnd, PPOINTL p)
{
    WinQueryPointerPos(HWND_DESKTOP, p);
    /* this is position wrt desktop */
    WinMapWindowPoints(HWND_DESKTOP, hWnd, p, 1);
    /* pos. wrt our window in pixels */
}
#endif


/* passing either n(n>=0) or f(when n==-1) */
void
SetMouseCoords(HWND hWnd, MPARAM mp1, int n, char *f)
{
    char s[100];

    ChangeCheck(hWnd, ulMouseSprintfFormatItem, SHORT1FROMMP(mp1));
    ulMouseSprintfFormatItem = SHORT1FROMMP(mp1);
    if (n >= 0)
	sprintf(s,"set mouse mouseformat %i clipboardformat %i", n, n);
    else
	sprintf(s,"set mouse mouseformat \"%s\" clipboardformat \"%s\"", f, f);
    gp_execute(s);
}


/*
**   Handle client window command(menu) messages
*/
MRESULT
WmClientCmdProc(HWND hWnd, ULONG message, MPARAM mp1, MPARAM mp2)
{
    static int ulPauseItem = IDM_PAUSEDLG;
    // static int ulMouseCoordItem = IDM_MOUSE_COORDINATES_REAL;
    int mx, my;

    GetMousePosViewport(hWnd,&mx,&my);

    switch ((USHORT) SHORT1FROMMP(mp1)) {
    case IDM_ABOUT :    /* show the 'About' box */
	WinDlgBox(HWND_DESKTOP,
		  hWnd ,
		  (PFNWP)About ,
		  0L,
		  ID_ABOUT,
		  NULL);
	break;

    case IDM_GPLOTINF:  /* view gnuplot.inf */
    {
	const char cmd_prefix[] = "start view ";
	const char helpfile[] = "gnuplot.inf";
	char *cmd;
	char *gnuplot_path;
	unsigned cmd_length;

	cmd_length = strlen(cmd_prefix) + strlen(helpfile);
        gnuplot_path = getenv("GNUPLOT");
	if (gnuplot_path != NULL)
	    cmd_length += strlen(gnuplot_path) + 1;

	cmd = (char *)malloc( cmd_length );
	strcpy(cmd, cmd_prefix);
	if (gnuplot_path != NULL) {
	    strcat(cmd, gnuplot_path);
	    strcat(cmd, "\\");
	}
	strcat(cmd, helpfile);
	system(cmd);
	free(cmd);
	break;
    }

    case IDM_PRINT :    /* print plot */
	if (SetupPrinter(hWnd, &qPrintData)) {
	    WinPostMsg(hWnd,
		       WM_USER_PRINT_BEGIN,
		       (MPARAM) &qPrintData,
		       (MPARAM) hpsScreen);
	}
	break;

    case IDM_PRINTSETUP :    /* select printer */
	WinDlgBox(HWND_DESKTOP,
		  hWnd ,
		  (PFNWP) QPrintersDlgProc,
		  0L,
		  IDD_QUERYPRINT,
		  qPrintData.szPrinterName);
	break;

    case IDM_LINES_THICK:
	/* change line setting */
	bWideLines = !bWideLines;
	ChangeCheck(hWnd, IDM_LINES_THICK, bWideLines?IDM_LINES_THICK:0);
	WinInvalidateRect(hWnd, NULL, TRUE);
	break;

    case IDM_LINES_SOLID:
	/* change line setting */
	bLineTypes = !bLineTypes;
	ChangeCheck(hWnd, IDM_LINES_SOLID, bLineTypes?0:IDM_LINES_SOLID);
	EditLineTypes(hWnd, hpsScreen, bLineTypes);
	WinInvalidateRect(hWnd, NULL, TRUE);
	break;

    case IDM_COLOURS:
	/* change colour setting */
	bColours = !bColours;
	ChangeCheck(hWnd, IDM_COLOURS, bColours?IDM_COLOURS:0);
	WinInvalidateRect(hWnd, NULL, TRUE);
	break;

    case IDM_FRONT:
	/* toggle z-order forcing */
	bPopFront = !bPopFront;
	ChangeCheck(hWnd, IDM_FRONT, bPopFront?IDM_FRONT:0);
	break;

    case IDM_KEEPRATIO:
	/* toggle keep aspect ratio */
	bKeepRatio = !bKeepRatio;
	ChangeCheck(hWnd, IDM_KEEPRATIO, bKeepRatio?IDM_KEEPRATIO:0);
	WinInvalidateRect(hWnd, NULL, TRUE); /* redraw screen */
	break;

    case IDM_FONTS:
	if (GetNewFont(hWnd, hpsScreen)) {
#if 0
	    bNewFont = TRUE;
#endif
	    WinInvalidateRect(hWnd, NULL, TRUE);
	}
	break;

    case IDM_SAVE:
	SaveIni(hWnd);
	break;

    case IDM_COPY:
	/* copy to clipboard */
	if (WinOpenClipbrd(hab)) {
	    CopyToClipBrd(hWnd);
	} else {
	    WinMessageBox(HWND_DESKTOP,
			  hWnd,
			  "Can't open clipboard",
			  APP_NAME,
			  0,
			  MB_OK | MB_ICONEXCLAMATION);
	}
	break;

    case IDM_CLEARCLIP :         /* clear clipboard */
	if (WinOpenClipbrd(hab)) {
	    WinEmptyClipbrd(hab);
	    WinCloseClipbrd(hab);
	} else {
	    WinMessageBox(HWND_DESKTOP,
			  hWnd,
			  "Can't open clipboard",
			  APP_NAME,
			  0,
			  MB_OK | MB_ICONEXCLAMATION);
	}
	break;

    case IDM_COMMAND:       /* go back to GNUPLOT command window */
	WinSwitchToProgram(hSwitch);
	break;

    case IDM_CONTINUE:
	WinPostMsg(hWnd, WM_PAUSEEND,(MPARAM)1L,(MPARAM)0L);
	WinEnableMenuItem(WinWindowFromID(
			      WinQueryWindow(hWnd, QW_PARENT), FID_MENU),
			  IDM_CONTINUE,
			  FALSE);
	break;

    case IDM_PAUSEGNU:  /* gnuplot handles pause */
	ChangeCheck(hWnd, ulPauseItem, IDM_PAUSEGNU);
	ulPauseItem = IDM_PAUSEGNU;
	ulPauseMode = PAUSE_GNU;
	break;

    case IDM_PAUSEDLG:  /* pause message in dlg box */
	ChangeCheck(hWnd, ulPauseItem, IDM_PAUSEDLG);
	ulPauseItem = IDM_PAUSEDLG;
	ulPauseMode = PAUSE_DLG;
	break;

    case IDM_PAUSEBTN:  /* pause uses menu button, no message */
	ChangeCheck(hWnd, ulPauseItem, IDM_PAUSEBTN);
	ulPauseItem = IDM_PAUSEBTN;
	ulPauseMode = PAUSE_BTN;
	break;

    case IDM_HELPFORHELP:
	WinSendMsg(WinQueryHelpInstance(hWnd),
		   HM_DISPLAY_HELP, 0L, 0L);
	return 0L;

    case IDM_EXTENDEDHELP:
	WinSendMsg(WinQueryHelpInstance(hWnd),
		   HM_EXT_HELP, 0L, 0L);
	return 0L;

    case IDM_KEYSHELP:
	WinSendMsg(WinQueryHelpInstance(hWnd),
		   HM_KEYS_HELP, 0L, 0L);
	return 0L;

    case IDM_HELPINDEX:
	WinSendMsg(WinQueryHelpInstance(hWnd),
		   HM_HELP_INDEX, 0L, 0L);
	return 0L;


	/* Now new mousing stuff: */

    case IDM_USEMOUSE: /* toggle using/not using mouse cursor tracking */
	useMouse = !useMouse;
	ChangeCheck(hWnd, IDM_USEMOUSE, useMouse?IDM_USEMOUSE:0);
	gp_execute(useMouse ? "set mouse" : "unset mouse");
#if 0
	if (!useMouse) /* redraw screen */
	    WinInvalidateRect(hWnd, NULL, TRUE);
#endif
	return 0L;

    case IDM_MOUSE_HELP:
	gp_exec_event(GE_keypress, mx, my, 'h', 1, 0);
	return 0L;

#if 0
    case IDM_MOUSE_COORDINATES_REAL:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_REAL);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_REAL;
	return 0L;

    case IDM_MOUSE_COORDINATES_PIXELS:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_PIXELS);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_PIXELS;
	return 0L;

    case IDM_MOUSE_COORDINATES_SCREEN:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_SCREEN);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_SCREEN;
	return 0L;

    case IDM_MOUSE_COORDINATES_XDATE:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_XDATE);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_XDATE;
	return 0L;

    case IDM_MOUSE_COORDINATES_XTIME:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_XTIME);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_XTIME;
	return 0L;

    case IDM_MOUSE_COORDINATES_XDATETIME:
	ChangeCheck(hWnd, ulMouseCoordItem, IDM_MOUSE_COORDINATES_XDATETIME);
	ulMouseCoordItem = IDM_MOUSE_COORDINATES_XDATETIME;
	return 0L;
#endif

    case IDM_MOUSE_CMDS2CLIP:
	/* toggle copying the command sent to gnuplot to clipboard */
	bSend2gp = !bSend2gp;
	ChangeCheck(hWnd, IDM_MOUSE_CMDS2CLIP, bSend2gp?IDM_MOUSE_CMDS2CLIP:0);
	return 0L;

    case IDM_MOUSE_FORMAT_pXcYp:
	SetMouseCoords(hWnd, mp1, -1, "[%g, %g]");
	return 0L;
    case IDM_MOUSE_FORMAT_XcY:
	SetMouseCoords(hWnd, mp1, 1, NULL);
	return 0L;
    case IDM_MOUSE_FORMAT_TIMEFMT:
	SetMouseCoords(hWnd, mp1, 3, NULL);
	return 0L;
    case IDM_MOUSE_FORMAT_DATE:
	SetMouseCoords(hWnd, mp1, 4, NULL);
	return 0L;
    case IDM_MOUSE_FORMAT_TIME:
	SetMouseCoords(hWnd, mp1, 5, NULL);
	return 0L;
    case IDM_MOUSE_FORMAT_DATETIME:
	SetMouseCoords(hWnd, mp1, 6, NULL);
	return 0L;
    case IDM_MOUSE_FORMAT_X_Y:
	SetMouseCoords(hWnd, mp1, -1, "%g %g");
	return 0L;
    case IDM_MOUSE_FORMAT_XcYc:
	SetMouseCoords(hWnd, mp1, -1, "%g %g ");
	return 0L;
    case IDM_MOUSE_FORMAT_XcYs:
	SetMouseCoords(hWnd, mp1, -1, "%g %g, ");
	return 0L;

    case IDM_MOUSE_POLAR_DISTANCE:
	/* toggle using/not using polar coords of distance */
	gp_execute(gpPMmenu.polar_distance
		   ? "set mouse nopolardistance"
		   : "set mouse polardistance");
	return 0L;

    case IDM_MOUSE_ZOOMNEXT: /* zoom to next level */
	gp_exec_event(GE_keypress, mx, my, 'n', 1, 0);
	return 0L;

    case IDM_MOUSE_UNZOOM: /* unzoom one level back */
	gp_exec_event(GE_keypress, mx, my, 'p', 1, 0);
	return 0L;

    case IDM_MOUSE_UNZOOMALL: /* unzoom to the first level */
	gp_exec_event(GE_keypress, mx, my, 'u', 1, 0);
	return 0L;

    case IDM_MOUSE_RULER:
    {
	int mx, my;

	GetMousePosViewport(hWnd,&mx,&my);
	gp_exec_event(GE_keypress, mx, my, 'r', 1, 0);
	return 0L;
    }

    case IDM_BREAK_DRAWING:
	breakDrawing = 1;
	return 0L;

    case IDM_SET_GRID:
    {
	gp_exec_event(GE_keypress, mx, my, 'g', 1, 0);
	return 0L;
    }

    case IDM_SET_LINLOGY:
    {
	gp_exec_event(GE_keypress, mx, my, 'l', 1, 0);
	return 0L;
    }

    case IDM_SET_AUTOSCALE:
	gp_execute("set autoscale; replot");
	return 0L;

    case IDM_DO_REPLOT:
	gp_execute("replot");
	return 0L;

    case IDM_DO_RELOAD:
	gp_execute("history !load");
	return 0L;

    case IDM_DO_SENDCOMMAND:
	if (input_from_PM_Terminal) {
	    if (pausing)
		DosBeep(440,111);
	    else
		WinDlgBox(HWND_DESKTOP, hWnd, SendCommandDlgProc,
			  NULLHANDLE, IDM_DO_SENDCOMMAND,
			  input_from_PM_Terminal);
	}
	return (MRESULT) 0;

    case IDM_SET_D_S_BOXES:
    case IDM_SET_D_S_DOTS:
    case IDM_SET_D_S_FSTEPS:
    case IDM_SET_D_S_HISTEPS:
    case IDM_SET_D_S_IMPULSES:
    case IDM_SET_D_S_LINES:
    case IDM_SET_D_S_LINESPOINTS:
    case IDM_SET_D_S_POINTS:
    case IDM_SET_D_S_STEPS:
	if (input_from_PM_Terminal)
            sprintf(input_from_PM_Terminal, "set style data %s; replot",
		    SetDataStyles[(USHORT) SHORT1FROMMP(mp1) - IDM_SET_D_S_BOXES ]);
	gp_execute(0);
	return 0L;

    case IDM_SET_F_S_BOXES:
    case IDM_SET_F_S_DOTS:
    case IDM_SET_F_S_FSTEPS:
    case IDM_SET_F_S_HISTEPS:
    case IDM_SET_F_S_IMPULSES:
    case IDM_SET_F_S_LINES:
    case IDM_SET_F_S_LINESPOINTS:
    case IDM_SET_F_S_POINTS:
    case IDM_SET_F_S_STEPS:
	if (input_from_PM_Terminal)
            sprintf(input_from_PM_Terminal, "set style function %s; replot",
		    SetDataStyles[(USHORT) SHORT1FROMMP(mp1) - IDM_SET_F_S_BOXES ]);
	gp_execute(0);
	return 0L;

    default :
	return WinDefWindowProc(hWnd, message, mp1, mp2);
    } /* switch(message) */

    return(NULL);
}


/*
**  Utility function:
**
**  move check mark from menu item 1 to item 2
*/
void
ChangeCheck(HWND hWnd , USHORT wItem1 , USHORT wItem2)
{
    HWND hMenu = WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT),
				 FID_MENU);

    if (wItem1 != 0)
        WinSendMsg(hMenu,
		   MM_SETITEMATTR,
		   MPFROM2SHORT(wItem1, TRUE),
		   MPFROM2SHORT(MIA_CHECKED, 0));
    if (wItem2 != 0)
        WinSendMsg(hMenu,
		   MM_SETITEMATTR,
		   MPFROM2SHORT(wItem2, TRUE),
		   MPFROM2SHORT(MIA_CHECKED, MIA_CHECKED));
}


/*
**  Copy window to clipboard as bitmap.
*/
static void
CopyToClipBrd(HWND hWnd)
{
    HAB hab;
    HBITMAP hbm;
    HMF     hmf;

    hab = WinQueryAnchorBlock(hWnd);
    WinEmptyClipbrd(hab);
    hbm = CopyToBitmap(hpsScreen);
    WinSetClipbrdData(hab,(ULONG) hbm, CF_BITMAP, CFI_HANDLE);
    hmf = CopyToMetaFile(hpsScreen);
    WinSetClipbrdData(hab,(ULONG) hmf, CF_METAFILE, CFI_HANDLE);
    WinCloseClipbrd(hab);
}


/*
**  Copy ps to a bitmap.
*/
HBITMAP CopyToBitmap(HPS hps)
{
    HPS     hpsMem;
    HWND    hwnd;
    HAB     hab;
    PSZ     psz[4] = {NULL, "Display", NULL, NULL};
    HDC     hdcMem, hdcScr;
    SIZEL   sizel;
    BITMAPINFOHEADER2 bmp;
    HBITMAP hbm;
    LONG    alData[2];
    RECTL   rectl;
    POINTL  aptl[6];

    hdcScr = GpiQueryDevice(hps);
    hwnd = WinWindowFromDC(hdcScr);
    hab = WinQueryAnchorBlock(hwnd);
    hdcMem = DevOpenDC(hab,
		       OD_MEMORY,
		       "*",
		       4L,
                       (PDEVOPENDATA) psz,
		       hdcScr);
    sizel.cx = 0/*GNUPAGE*/;
    sizel.cy = 0/*GNUPAGE*/;
    hpsMem = GpiCreatePS(hab, hdcMem, &sizel,
			 PU_PELS | GPIA_ASSOC | GPIT_MICRO);
    GpiQueryDeviceBitmapFormats(hpsMem, 2L, alData);
    WinQueryWindowRect(hwnd, &rectl);
    memset(&bmp, 0, sizeof(bmp));
    bmp.cbFix =(ULONG) sizeof(bmp);
    bmp.cx =(SHORT)(rectl.xRight - rectl.xLeft);
    bmp.cy =(SHORT)(rectl.yTop - rectl.yBottom);
    bmp.cPlanes = alData[0];
    bmp.cBitCount = alData[1];
    hbm = GpiCreateBitmap(hpsMem, &bmp, 0, NULL, NULL);
    GpiSetBitmap(hpsMem, hbm);
    aptl[0].x = 0;
    aptl[0].y = 0;
    aptl[1].x =(LONG) bmp.cx;
    aptl[1].y =(LONG) bmp.cy;
    aptl[2].x = 0;
    aptl[2].y = 0;
    GpiBitBlt(hpsMem, hps, 3L, aptl, ROP_SRCCOPY, BBO_IGNORE);
    GpiDestroyPS(hpsMem);
    DevCloseDC(hdcMem);
    return hbm;
}


/*
**  Copy ps to a metafile.
*/
HMF
CopyToMetaFile(HPS hps)
{
    HDC hdcMF, hdcOld;
    HAB hab;
    HWND hwnd;
    PSZ psz[4] = {NULL,"Display",NULL,NULL};
    HMF hmf;
    hdcOld = GpiQueryDevice(hps);
    hwnd = WinWindowFromDC(hdcOld);
    hab = WinQueryAnchorBlock(hwnd);
    hdcMF = DevOpenDC(hab, OD_METAFILE, "*", 4L, psz, hdcOld);

    DosRequestMutexSem(semHpsAccess,(ULONG) SEM_INDEFINITE_WAIT);
    GpiSetDrawingMode(hps, DM_DRAW);
    GpiAssociate(hps, 0);
    GpiAssociate(hps, hdcMF);
    ScalePS(hps);
    GpiDrawChain(hps);
    GpiAssociate(hps, 0);
    GpiAssociate(hps, hdcOld);
    DosReleaseMutexSem(semHpsAccess);
    hmf = DevCloseDC(hdcMF);
    return hmf;
}


/*
** Query INI file
*/
BOOL
QueryIni(HAB hab)
{
    BOOL         bPos, bData, bSwp ;
    ULONG        ulOpts[5];
    HINI         hini;
    ULONG        ulCB;
    char         *p;
    static SWP   pauseswp;

    /* read gnuplot ini file */

    hini = PrfOpenProfile(hab, szIniFile);
    ulCB = sizeof(ulShellPos);
    bPos = PrfQueryProfileData(hini, APP_NAME, INISHELLPOS, &ulShellPos, &ulCB);
    ulCB = sizeof(SWP);
    bSwp = PrfQueryProfileData(hini, APP_NAME, INIPAUSEPOS, &pauseswp, &ulCB);
    if (bSwp)
	pausedata.pswp = &pauseswp;
    ulCB = sizeof(ulOpts);
    bData = PrfQueryProfileData(hini, APP_NAME, INIOPTS, &ulOpts, &ulCB);
    if (bData) {
        bLineTypes =(BOOL)ulOpts[0];
        bWideLines =(BOOL)ulOpts[1];
        bColours =(BOOL)ulOpts[2];
        ulPauseMode = ulOpts[3];
        bPopFront =(BOOL)ulOpts[4];
    } else {
        bLineTypes = FALSE;  /* default values */
	/*   bWideLines = FALSE; */
        bColours = TRUE;
        bPopFront = TRUE;
        ulPauseMode = 1;
    }
    ulCB = 4*sizeof(float);
    PrfQueryProfileData(hini, APP_NAME, INIFRAC, &qPrintData.xsize, &ulCB);
    if (PrfQueryProfileSize(hini, APP_NAME, INIPRDRIV, &ulCB)) {
        PDRIVDATA pdriv =(PDRIVDATA) malloc(ulCB);
        if (pdriv != NULL) {
            PrfQueryProfileData(hini, APP_NAME, INIPRDRIV, pdriv, &ulCB);
            qPrintData.pdriv = pdriv;
            qPrintData.cbpdriv = ulCB;
	}
    }
    PrfQueryProfileString(hini, APP_NAME, INIPRPR, "",
			  qPrintData.szPrinterName,
                          (long) sizeof qPrintData.szPrinterName);
    PrfQueryProfileString(hini, APP_NAME, INIFONT, INITIAL_FONT,
			  szFontNameSize, FONTBUF);
    ulCB = sizeof(ulOpts);
    bData = PrfQueryProfileData(hini, APP_NAME, INICHAR, &ulOpts, &ulCB);
    if (bData) {
        lCharWidth = ulOpts[0];
        lCharHeight = ulOpts[1];
    } else {
        lCharWidth = 217;
        lCharHeight = 465;
    }
    ulCB = sizeof(bKeepRatio);
    bData = PrfQueryProfileData(hini, APP_NAME, INIKEEPRATIO, &ulOpts, &ulCB);
    if (bData) bKeepRatio =(BOOL)ulOpts[0];

    /* Mousing: */
    /* Ignore reading "Use mouse" --- no good idea to have mouse on by default.
       Maybe it was the reason of some crashes(mouse init before draw).
       ulCB = sizeof(useMouse);
       bData = PrfQueryProfileData(hini, APP_NAME, INIUSEMOUSE, &ulOpts, &ulCB);
       if (bData) useMouse =(int)ulOpts[0];
    */
    /* ignore reading mouse cursor(real, relative or pixels).
       Reason/bug: it does not switch the check mark in the menu, even
       though it works as expected.
       ulCB = sizeof(mouse_mode);
       bData = PrfQueryProfileData(hini, APP_NAME, INIMOUSECOORD, &ulOpts, &ulCB);
       if (bData) mouse_mode =(ULONG)ulOpts[0];
    */

    PrfCloseProfile(hini);

    if (qPrintData.szPrinterName[0] == '\0') {
	/* get default printer name     */
        PrfQueryProfileString(HINI_PROFILE,
			      "PM_SPOOLER",
			      "PRINTER",
			      ";",
			      qPrintData.szPrinterName,
                              (long) sizeof qPrintData.szPrinterName);
        if ((p=strchr(qPrintData.szPrinterName, ';')) != NULL)
	    *p = '\0';
    }
    bShellPos = bPos;
    return bPos;
}


/*
** save data in ini file
*/
static void
SaveIni(HWND hWnd)
{
    SWP     swp;
    HINI    hini;
    ULONG   ulOpts[5];
    HAB     hab;

    hab = WinQueryAnchorBlock(hWnd);
    hini = PrfOpenProfile(hab, szIniFile);
    if (hini != NULLHANDLE) {
        WinQueryWindowPos(hwndFrame, &swp);
        ulPlotPos[0] = swp.x;
        ulPlotPos[1] = swp.y;
        ulPlotPos[2] = swp.cx;
        ulPlotPos[3] = swp.cy;
        PrfWriteProfileData(hini, APP_NAME, INISHELLPOS, &ulPlotPos,
			    sizeof(ulPlotPos));
        if (pausedata.pswp != NULL)
            PrfWriteProfileData(hini, APP_NAME, INIPAUSEPOS,
				pausedata.pswp, sizeof(SWP));
        ulOpts[0] =(ULONG)bLineTypes;
        ulOpts[1] =(ULONG)bWideLines;
        ulOpts[2] =(ULONG)bColours;
        ulOpts[3] = ulPauseMode;
        ulOpts[4] =(ULONG)bPopFront;
        PrfWriteProfileData(hini, APP_NAME, INIOPTS, &ulOpts, sizeof(ulOpts));
        PrfWriteProfileData(hini, APP_NAME, INIFRAC, &qPrintData.xsize,
			    4*sizeof(float));
        if (qPrintData.pdriv != NULL)
            PrfWriteProfileData(hini, APP_NAME, INIPRDRIV, qPrintData.pdriv,
				qPrintData.cbpdriv);
        PrfWriteProfileString(hini, APP_NAME, INIPRPR,
			      qPrintData.szPrinterName[0] == '\0'? NULL:
			      qPrintData.szPrinterName);
        PrfWriteProfileString(hini, APP_NAME, INIFONT, szFontNameSize);
        ulOpts[0] =(ULONG)lCharWidth;
        ulOpts[1] =(ULONG)lCharHeight;
        PrfWriteProfileData(hini, APP_NAME, INICHAR, &ulOpts, sizeof(ulOpts));
	PrfWriteProfileData(hini, APP_NAME, INIKEEPRATIO, &bKeepRatio,
			    sizeof(bKeepRatio));

	/* Mouse stuff */
	/* Ignore reading "Use mouse" --- no good idea to have mouse on by default.
	   Maybe it was the reason of some crashes(mouse init before draw).
	   PrfWriteProfileData(hini, APP_NAME, INIUSEMOUSE, &useMouse, sizeof(useMouse));
	*/
	/* Do not write the mouse coord. mode.
	   PrfWriteProfileData(hini, APP_NAME, INIMOUSECOORD, &mouse_mode, sizeof(mouse_mode));
	*/
        PrfCloseProfile(hini);
    } else {
        WinMessageBox(HWND_DESKTOP,
		      HWND_DESKTOP,
		      "Can't write ini file",
		      APP_NAME,
		      0,
		      MB_OK | MB_ICONEXCLAMATION);
    }
}


/*
**  Paint the screen with current data
*/
static void
DoPaint(HWND hWnd, HPS hps)
{
    static RECTL rectl;

    if (tidDraw != 0) {
	/* already drawing - stop it; include the rectl now
	   being drawn in, in the update region; and return
	   without calling beginpaint so that the paint
	   message is resent */
	GpiSetStopDraw(hpsScreen, SDW_ON);
	DosSleep(1);
	WinInvalidateRect(hWnd, &rectl, TRUE);
	return;
    }
    /* winbeginpaint here, so paint message is
       not resent when we return, then spawn a
       thread to do the drawing */
    WinBeginPaint(hWnd, hps, &rectl);                 /*rl */
    tidDraw = _beginthread(ThreadDraw, NULL, 32768, NULL);
}


/*
**  Thread to draw plot on screen
*/
static void
ThreadDraw(void* arg)
{
    HAB hab = WinInitialize(0);

    InitScreenPS();

    DosRequestMutexSem(semHpsAccess,(ULONG) SEM_INDEFINITE_WAIT);
    ScalePS(hpsScreen);
    GpiSetStopDraw(hpsScreen, SDW_OFF);
    GpiSetDrawingMode(hpsScreen, DM_DRAW);
    GpiDrawChain(hpsScreen);
    DrawRuler();
    DisplayStatusLine(hpsScreen);
    WinEndPaint(hpsScreen);
    DosReleaseMutexSem(semHpsAccess);
    WinTerminate(hab);
    tidDraw = 0;
#if 0
    /* This does not work here(why?!), thus moved to pm.trm: PM_text(); */
    gp_exec_event(GE_plotdone, mx, my, 0, 0, 0); /* enable again zoom and scale by mouse motions */
#endif
}


/*
** Initialise the screen ps for drawing
*/
HPS
InitScreenPS()
{
    RECTL   rectClient;
    int     nColour = 0;

    GpiResetPS(hpsScreen, GRES_ATTRS);
#if 0 /* Use default background color(the original version) */
    GpiErase(hpsScreen);
    WinQueryWindowRect(hApp,(PRECTL)&rectClient);
#else /* PM 14.3.2000: Use always white background */
    WinQueryWindowRect(hApp,(PRECTL)&rectClient);
    WinFillRect(hpsScreen,&rectClient,CLR_WHITE);
#endif
    if (bKeepRatio) {
	double ratio = 1.560;
	double xs = rectClient.xRight - rectClient.xLeft;
	double ys = rectClient.yTop - rectClient.yBottom;

	if (ys > xs/ratio) { /* reduce ys to fit */
	    rectClient.yTop = rectClient.yBottom +(int)(xs/ratio);
	} else if (ys < xs/ratio) { /* reduce xs to fit */
	    rectClient.xRight = rectClient.xLeft +(int)(ys*ratio);
	}
    } else
	/* PM: why this -10? Otherwise the right axis is too close to
	 * the right border. However, this -10 should be taken into
	 * account for mousing! Or can it be inside a transformation?
	 */
	rectClient.xRight -= 10;

    GpiSetPageViewport(hpsScreen, &rectClient);
    if (!bColours) {
        int i;

	nColour = 16;
	alColourTable[0] = 0xFFFFFF;
	for (i=1; i<nColour; i++)
	    alColourTable[i] = 0;
    }

    if (bPMPaletteMode) {
	int i;

	GpiCreateLogColorTable(hpsScreen, LCOL_RESET, LCOLF_CONSECRGB,
			       0, nColour, alColourTable);
	if (!lCols_init) { /* Ilya: avoid white line on white background */
	    lCols_init = 1;
	    GpiQueryLogColorTable(hpsScreen, 0, 0, 16, alColourTable + 2);
	    alColourTable[2+CLR_WHITE] = 0xffffff;		/* -2 */
	    alColourTable[2+CLR_BLACK] = 0;			/* -1 */
	    bkColor = alColourTable[2+CLR_BACKGROUND];
	    i = -1;
	    while (i++ < 16) {
		if (alColourTable[2+lCols[i]] == bkColor) {
		    while (i++ < 16)
			lCols[i - 1] = lCols[i];
		    lCols_num--;
		    break;
		}
	    }
	}

	/* init rgb_colors: simple index translation only */
	for (i=0; i<18; i++)
	    rgb_colors[i] = i + CLR_WHITE;
    } else {
	if (!lCols_init) {
	    int i;

	    lCols_init = 1;
	    /* get RGB values of all CLR_xxx constants */
	    for (i=0; i<18; i++)
		rgb_colors[i] = GpiQueryRGBColor(hpsScreen, LCOLOPT_REALIZED, i + CLR_WHITE );
	}

	GpiCreateLogColorTable(hpsScreen, LCOL_RESET, LCOLF_RGB, 0, 0, 0);
    }
    return hpsScreen;
}


/*
**  Get a font to use
**  Scale the plot area to world coords for subsequent plotting
*/
short
ScalePS(HPS hps)
{
    SelectFont(hps, szFontNameSize);
    return 0;
}


/*
**  Select a named and sized outline font
*/
void
SelectFont(HPS hps, char *szFontNameSize)
{
    HDC    hdc;
    FATTRS  fat;
    LONG   xDeviceRes, yDeviceRes;
    POINTL ptlFont;
    SIZEF  sizfx;
    static LONG lcid = 0L;
    static char *szFontName;
    static short shPointSize;

    sscanf(szFontNameSize, "%hd", &shPointSize);
    szFontName = strchr(szFontNameSize, '.') + 1;

    fat.usRecordLength  = sizeof fat;
    fat.fsSelection     = 0;
    fat.lMatch          = 0;
    fat.idRegistry      = 0;
    fat.usCodePage      = codepage; /*GpiQueryCp(hps); */
    fat.lMaxBaselineExt = 0;
    fat.lAveCharWidth   = 0;
    fat.fsType          = 0;
    fat.fsFontUse       = FATTR_FONTUSE_OUTLINE |
	FATTR_FONTUSE_TRANSFORMABLE;

    strcpy(fat.szFacename, szFontName);

    if (tabFont[0].name !=NULL)
	free(tabFont[0].name);
    tabFont[0].name = strdup(szFontName);
    tabFont[0].lcid = 10L;

    lcid = GpiQueryCharSet(hps);
    if (lcid != 10L)
	lcid = 10L;
    else {
        GpiSetCharSet(hps, 0L);
        GpiDeleteSetId(hps, lcid);
    }
    GpiCreateLogFont(hps, NULL, lcid, &fat);
    GpiSetCharSet(hps, lcid);

    hdc = GpiQueryDevice(hps);

    DevQueryCaps(hdc, CAPS_HORIZONTAL_RESOLUTION, 1L, &xDeviceRes);
    DevQueryCaps(hdc, CAPS_VERTICAL_RESOLUTION,   1L, &yDeviceRes);

    /* Find desired font size in pixels */

    ptlFont.x = 2540L *(long)shPointSize / 72L;
    ptlFont.y = 2540L *(long)shPointSize / 72L;

    /* Set the character box */

    sizfx.cx = MAKEFIXED(ptlFont.x, 0);
    sizfx.cy = MAKEFIXED(ptlFont.y, 0);
    lVOffset = ptlFont.y;

    sizBaseFont = sizfx;
    GpiSetCharBox(hps, &sizfx);

    /* set up some useful globals */
    {
	FONTMETRICS fm;

	GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm);
	lBaseSubOffset = -fm.lSubscriptYOffset;
	lBaseSupOffset = fm.lSuperscriptYOffset;
	lSubOffset = lBaseSubOffset;
	lSupOffset = lBaseSupOffset;
	lCharHeight = fm.lMaxAscender*1.2;
	lCharWidth  = fm.lAveCharWidth;
	sizBaseSubSup.cx = MAKEFIXED(ptlFont.x*0.7, 0);
	sizBaseSubSup.cy = MAKEFIXED(ptlFont.y*0.7, 0);
    }
    sizCurFont = sizBaseFont;
    sizCurSubSup = sizBaseSubSup;
#if 0
    if (bNewFont) {
	/* EditCharCell(hps, &sizfx); */
        bNewFont = FALSE;
    }
#endif
}


/*
**  Select a named and sized outline(adobe) font
*/
void
SwapFont(HPS hps, char *szFNS)
{
    HDC    hdc;
    FATTRS  fat;
    LONG   xDeviceRes, yDeviceRes;
    POINTL ptlFont;
    static LONG lcid = 0L;
    static int itab = 1;
    static char *szFontName;
    static short shPointSize;

    if (szFNS == NULL) {    /* restore base font */
	sizCurFont = sizBaseFont;
	sizCurSubSup = sizBaseSubSup;
	lSubOffset = lBaseSubOffset;
	lSupOffset = lBaseSupOffset;
	GpiSetCharSet(hps, 10);
	GpiSetCharBox(hps, &sizBaseFont);
    } else {
	sscanf(szFNS, "%hd", &shPointSize);
        szFontName = strchr(szFNS, '.') + 1;

        {
            int i;

            lcid = 0;
            for (i=0; i<itab; i++) {
                if (strcmp(szFontName, tabFont[i].name) == 0) {
                    lcid = tabFont[i].lcid;
                    break;
		}
	    }
        }

        if (lcid == 0) {
	    fat.usRecordLength  = sizeof fat;
	    fat.fsSelection     = 0;
	    fat.lMatch          = 0;
	    fat.idRegistry      = 0;
	    fat.usCodePage      = codepage; /*GpiQueryCp(hps); */
	    fat.lMaxBaselineExt = 0;
	    fat.lAveCharWidth   = 0;
	    fat.fsType          = 0;
	    fat.fsFontUse       = FATTR_FONTUSE_OUTLINE |
		FATTR_FONTUSE_TRANSFORMABLE;

	    strcpy(fat.szFacename, szFontName);

	    tabFont[itab].name = strdup(szFontName);
	    lcid = itab+10;
	    tabFont[itab].lcid = lcid;
	    ++itab;

	    /* lcid = 11L; */
	    GpiSetCharSet(hps, 0L);
	    GpiDeleteSetId(hps, lcid);
	    GpiCreateLogFont(hps, NULL, lcid, &fat);
        } /* if (lcid) */

        GpiSetCharSet(hps, lcid);
	hdc = GpiQueryDevice(hps);

	DevQueryCaps(hdc, CAPS_HORIZONTAL_RESOLUTION, 1L, &xDeviceRes);
	DevQueryCaps(hdc, CAPS_VERTICAL_RESOLUTION,   1L, &yDeviceRes);

	/* Find desired font size in pixels */
	ptlFont.x = 2540L *(long)shPointSize / 72L;
	ptlFont.y = 2540L *(long)shPointSize / 72L;

	/* Set the character box */
	sizCurFont.cx = MAKEFIXED(ptlFont.x, 0);
	sizCurFont.cy = MAKEFIXED(ptlFont.y, 0);
	/* lVOffset = ptlFont.y; */

	GpiSetCharBox(hps, &sizCurFont);
	sizCurSubSup.cx = MAKEFIXED(ptlFont.x*0.7, 0);
	sizCurSubSup.cy = MAKEFIXED(ptlFont.y*0.7, 0);

	/* set up some useful globals */
	{
	    FONTMETRICS fm;

	    GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm);
	    lSubOffset = -fm.lSubscriptYOffset;
	    lSupOffset = fm.lSuperscriptYOffset;
	}
    }
}

typedef struct image_list_entry {
    PBITMAPINFO2 pbmi;
    PBYTE image;
    struct image_list_entry *next;
} image_list_entry;
static image_list_entry *image_list = NULL;


/*
** Thread to read plot commands from GNUPLOT pm driver.
** Opens named pipe, then clears semaphore to allow GNUPLOT driver to proceed.
** Reads commands and builds a command list.
*/
static void
ReadGnu(void* arg)
{
    HPIPE hRead = 0L;
    POINTL ptl;
    long lOldLine = 0;
    BOOL bPath = FALSE;
    BOOL bDots = FALSE;
    ULONG rc;
    USHORT usErr;
    ULONG cbR;
    USHORT i;
    unsigned char buff[2];
    HEV hev;
    static char *szPauseText = NULL;
    ULONG ulPause;
    char *pszPipeName, *pszSemName;
    HPS hps;
    HAB hab;
    int linewidth = DEFLW;
    HPAL pm3d_hpal = 0;     /* palette used for make_palette() */
    HPAL pm3d_hpal_old = 0; /* default palette used before make_palette() */
    LONG pm3d_color = 0;    /* current colour (used if it is >0) */
    ULONG *rgbTable = NULL; /* current colour table (this is a 'virtual' palette) */

    hab = WinInitialize(0);
    DosEnterCritSec();
    pszPipeName = malloc(256);
    pszSemName  = malloc(256);
    DosExitCritSec();
    strcpy(pszPipeName, "\\pipe\\");
    strcpy(pszSemName, "\\sem32\\");
    strcat(pszPipeName, szIPCName);
    strcat(pszSemName, szIPCName);

    /* open a named pipe for communication with gnuplot */

    rc = DosCreateNPipe(pszPipeName,
			&hRead,
			NP_ACCESS_DUPLEX|NP_NOINHERIT|NP_NOWRITEBEHIND ,
			1|NP_WAIT|NP_READMODE_MESSAGE|NP_TYPE_MESSAGE,
			PIPEBUF,
			PIPEBUF,
			0xFFFFFFFF);
    hev = 0;       /* OK, gnuplot can try to open npipe ... */
    DosOpenEventSem(pszSemName, &hev);
    DosPostEventSem(hev);

    /* attach to gnuplot */
  server:

    if (DosConnectNPipe(hRead) == 0L) {
	WinPostMsg(hSysMenu,
		   MM_SETITEMATTR,
		   MPFROM2SHORT(SC_CLOSE, TRUE),
		   MPFROM2SHORT(MIA_DISABLED, MIA_DISABLED));

	/* store graphics commands */
	/* use semaphore to prevent problems with drawing while reallocating
	   the command buffers */

        DosRead(hRead, &ppidGnu, 4, &cbR);

	sprintf(mouseShareMemName, "\\SHAREMEM\\GP%i_Mouse_Input",(int)ppidGnu);
	if (DosGetNamedSharedMem(&input_from_PM_Terminal,
				 mouseShareMemName,
				 PAG_WRITE)) {
	    /*now: gray menu items; old code: DosBeep(1440L,1000L); // indicates error */
	    input_from_PM_Terminal = 0;
	}

	semInputReady = 0;
	/* semaphore 'semInputReady' must be open later in order to avoid problems */
	/* with the server mode; also 'bhave_*' init here because of server */

	/* DosPostEventSem(semStartSeq);         // once we've got pidGnu */
	WinPostMsg(hApp, WM_GPSTART, 0, 0);


	hps = hpsScreen;
	InitScreenPS();
	while (1) {
            usErr=BufRead(hRead,buff, 1, &cbR);
            if (usErr != 0)
		break;

	    if (breakDrawing) {
		/* PM: drawing has been stopped(by Ctrl-C)... */
		hps = 0;	/*     ...thus drawings go to nowhere... */
		if (*buff == SET_TEXT) { /* ...unless 'plot finished' command */
		    POINTL p;
		    hps = hpsScreen;  /* drawings back to screen */
		    breakDrawing = 0;
		    GpiSetColor(hps, RGB_TRANS(CLR_RED)); /* cross the unfinished plot */
		    GpiBeginPath(hps, 1);
		    p.x = p.y = 0; GpiMove(hps, &p);
		    p.x = 19500; p.y = 12500; GpiLine(hps, &p);
		    p.x = 0; p.y = 12500; GpiMove(hps, &p);
		    p.x = 19500; p.y = 0; GpiLine(hps, &p);
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		}
	    }

            switch (*buff) {
	    case SET_GRAPHICS :    /* enter graphics mode */
	    {
		image_list_entry *ile;

		if (tidDraw != 0) {
		    /* already drawing - stop it */
		    GpiSetStopDraw(hpsScreen, SDW_ON);
		    while (tidDraw != 0) DosSleep(1);
		}
		/* wait for access to command list and lock it */
		/* DosWaitEventSem(semDrawDone, SEM_INDEFINITE_WAIT);                     */
		/* DosEnterCritSec(); */
		DosRequestMutexSem(semHpsAccess,(ULONG) SEM_INDEFINITE_WAIT);
		InitScreenPS();
		ScalePS(hps);
		/* DosResetEventSem(semDrawDone, &ulCount); */
		GpiSetDrawingMode(hps, DM_DRAWANDRETAIN);
		for (i=1;i<=iSeg;i++)
		    GpiDeleteSegment(hps, i);
		iSeg = 1;
		GpiOpenSegment(hps, iSeg);
		/* DosExitCritSec(); */
		GpiSetLineEnd(hps, LINEEND_ROUND);
		GpiSetLineWidthGeom(hps, linewidth);
		GpiSetCharBox(hps, &sizBaseFont);

		/* free image buffers from previous plot, if any */
		while (image_list) {
		    DEBUG_IMAGE(("freeing image from last plot"));
		    ile = image_list;
		    image_list = ile->next;
		    free(ile->image);
		    free(ile->pbmi);
		    free(ile);
		}

		break;
	    }

	    case GR_QUERY :     /* query terminal info */
		/* mouseable gnupmdrv sends greetings to mouseable PM terminal */
		if (mouseTerminal) {
		    int i=0xABCD;

		    DosWrite(hRead, &i, sizeof(int), &cbR);
		}
		DosWrite(hRead, &lCharWidth, sizeof(int), &cbR);
		DosWrite(hRead, &lCharHeight, sizeof(int), &cbR);
		break;

	    case SET_TEXT :     /* leave graphics mode(graph completed) */
		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		GpiCloseSegment(hps);
		DrawRuler();
		DisplayStatusLine(hps);
		/* DosPostEventSem(semDrawDone); */
		DosReleaseMutexSem(semHpsAccess);
		WinPostMsg(hApp, WM_GNUPLOT, 0L, 0L);
		break;

	    case GR_RESET :
		/* gnuplot has reset drivers, allow user to kill this */
		WinPostMsg(hSysMenu,
			   MM_SETITEMATTR,
			   MPFROM2SHORT(SC_CLOSE, TRUE),
			   MPFROM2SHORT(MIA_DISABLED,(USHORT)0));

		/* if we are keeping us on the screen, wait for new connection */
		if (bServer||bPersist) {
		    DosDisConnectNPipe(hRead);
		    goto server;
		}
		break;

	    case GR_RESUME :
	    {
		/* resume after multiplot */
		DosRequestMutexSem(semHpsAccess,(ULONG) SEM_INDEFINITE_WAIT);
		/* DosWaitEventSem(semDrawDone, SEM_INDEFINITE_WAIT); */
		iSeg++;
		/* DosResetEventSem(semDrawDone, &ulCount); */
		GpiSetDrawingMode(hps, DM_DRAWANDRETAIN);
		GpiOpenSegment(hps, iSeg);
		break;
	    }

	    case 's' :
		/* suspend after multiplot */
		break;

	    case GR_MOVE :   /* move */
	    case GR_DRAW :   /* draw vector */
	    {
		LONG curr_color;

		if (pm3d_color >= 0) {
		    curr_color = GpiQueryColor(hps);
		    GpiSetColor(hps, pm3d_color);
		}
		if (*buff=='M') {
		    if (bPath) {
			GpiEndPath(hps);
			GpiStrokePath(hps, 1, 0);
			bPath = FALSE;
		    }
		} else {
		    if (bWideLines/*bWideLines*/ && !bPath) {
			GpiBeginPath(hps, 1);
			bPath = TRUE;
		    }
		}
		BufRead(hRead,&ptl.x, 2*sizeof(int), &cbR);
		if ((*buff=='V') && bDots)
		    ptl.x += 5;
		else if ((*buff=='M') && bDots)
		    ptl.x -= 5;
		if (*buff == 'M')
		    LMove(hps, &ptl);
		else
		    LLine(hps, &ptl);

		if (pm3d_color >= 0)
		    GpiSetColor(hps, curr_color);
	    }
	    break;

	    case GR_PAUSE  :   /* pause */
	    {
		int len;

		pausing = 1;
		BufRead(hRead, &len, sizeof(int), &cbR);
		len = (len + sizeof(int) - 1) / sizeof(int);
		if (len > 0){  /* get pause text */
		    DosEnterCritSec();
		    szPauseText = malloc(len*sizeof(int));
		    DosExitCritSec();
		    BufRead(hRead,szPauseText, len*sizeof(int), &cbR);
		}
		if (ulPauseMode != PAUSE_GNU) {
		    /* pause and wait for semaphore to be cleared */
		    DosResetEventSem(semPause, &ulPause);
		    WinPostMsg(hApp, WM_PAUSEPLOT,(MPARAM) szPauseText, 0L);
		    DosWaitEventSem(semPause, SEM_INDEFINITE_WAIT);
		} else { /* gnuplot handles pause */
		    ulPauseReply = 2;
		}
		DosEnterCritSec();
		if (szPauseText != NULL)
		    free(szPauseText);
		szPauseText = NULL;
		DosExitCritSec();
		/* reply to gnuplot so it can continue */
		DosWrite(hRead, &ulPauseReply, sizeof(int), &cbR);
		pausing = 0;
		break;
	    }

	    case GR_ENH_TEXT :  /* write enhanced text */
		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		{
                    unsigned int x, y, len;
		    unsigned int mode;
		    int textwidth, textheight;
                    char *str;
                    POINTL aptl[TXTBOX_COUNT];

		    /* read x, y, mode, len */
		    BufRead(hRead, &x, sizeof(int), &cbR);
                    BufRead(hRead, &y, sizeof(int), &cbR);
                    BufRead(hRead, &mode, sizeof(int), &cbR);
                    BufRead(hRead, &len, sizeof(int), &cbR);

                    DosEnterCritSec();
                    len =(len+sizeof(int)-1)/sizeof(int);
                    if (len == 0) len = 1; /*?? how about read */
                    str = malloc(len*sizeof(int));
		    *str = '\0';
                    DosExitCritSec();
                    BufRead(hRead, str, len*sizeof(int), &cbR);

                    GpiQueryTextBox(hps, strlen(str), str, TXTBOX_COUNT, aptl);
		    textwidth = aptl[TXTBOX_CONCAT].x;
		    textheight = aptl[TXTBOX_CONCAT].y;

		    /* only display text if requested */
		    if (mode & 0x01) {
			LONG curr_color;

			if (pm3d_color >= 0) {
			    curr_color = GpiQueryColor(hps);
			    GpiSetColor(hps, pm3d_color);
			}
			ptl.x = (LONG) (x + multLineVert * (lVOffset / 4));
			ptl.y = (LONG) (y - multLineHor * (lVOffset / 4));

			GpiSetBackMix(hps, BM_LEAVEALONE);
			GpiCharStringAt(hps, &ptl, strlen(str), str);

			if (pm3d_color >= 0)
			    GpiSetColor(hps, curr_color);
		    }

		    /* report back textwidth */
		    DosWrite(hRead, &textwidth, sizeof(textwidth), &cbR);
		    DosWrite(hRead, &textheight, sizeof(textheight), &cbR);

                    DosEnterCritSec();
                    free(str);
		    DosExitCritSec();

		    break;
		}

	    case GR_TEXT :   /* write text */
		/* read x, y, len */
		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		{
                    unsigned int x, y, len;
		    int sw;
                    char *str;
		    LONG curr_color;
#ifndef PM_KEEP_OLD_ENHANCED_TEXT
                    POINTL aptl[TXTBOX_COUNT];
#endif
		    BufRead(hRead,&x, sizeof(int), &cbR);
                    BufRead(hRead,&y, sizeof(int), &cbR);
                    BufRead(hRead,&len, sizeof(int), &cbR);

                    DosEnterCritSec();
                    len =(len+sizeof(int)-1)/sizeof(int);
                    if (len == 0) len = 1; /*?? how about read */
                    str = malloc(len*sizeof(int));
		    *str = '\0';
                    DosExitCritSec();
                    BufRead(hRead, str, len*sizeof(int), &cbR);
		    if (pm3d_color >= 0) {
                        curr_color = GpiQueryColor(hps);
			GpiSetColor(hps, pm3d_color);
		    }

#ifdef PM_KEEP_OLD_ENHANCED_TEXT
                    sw = QueryTextBox(hps, strlen(str), str);
#else
                    GpiQueryTextBox(hps, strlen(str), str, TXTBOX_COUNT, aptl);
		    sw = aptl[TXTBOX_BOTTOMRIGHT].x;
#endif

                    switch (jmode) {
		    case LEFT:
			sw = 0;
			break;
		    case CENTRE:
			sw /= -2;
			break;
		    case RIGHT:
			sw *= -1;
			break;
		    }

		    ptl.x = (LONG) (x + multLineHor * sw + multLineVert * (lVOffset / 4));
		    ptl.y = (LONG) (y + multLineVert * sw - multLineHor * (lVOffset / 4));	

		    GpiSetBackMix(hps, BM_LEAVEALONE);

#ifdef PM_KEEP_OLD_ENHANCED_TEXT
                    if (bEnhanced)
                        CharStringAt(hps, ptl.x, ptl.y, strlen(str) , str);
                    else
#endif
			GpiCharStringAt(hps, &ptl, strlen(str), str);

		    if (pm3d_color >= 0)
                        GpiSetColor(hps, curr_color);

                    DosEnterCritSec();
                    free(str);
		    DosExitCritSec();

		    break;
		}

	    case SET_JUSTIFY :   /* justify */
		BufRead(hRead, &jmode, sizeof(int), &cbR);
		//printf( "SET_JUSTIFY: %i", jmode );
		break;

	    case SET_ANGLE :   /* text angle */
	    {
		int ta, t1;
		GRADIENTL grdl;

		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		BufRead(hRead, &ta, sizeof(int), &cbR);
		t1 = ta % 360;
		if (t1 < 0)
		    t1 += 360;
		switch (t1) {
		case   0:
		    grdl.x =  1L;
		    grdl.y =  0L;
		    multLineHor = 1;
		    multLineVert = 0;
		    break;
		case  90:
		    grdl.x =  0L;
		    grdl.y =  1L;
		    multLineHor = 0;
		    multLineVert = 1;
		    break;
		case 180:
		    grdl.x = -1L;
		    grdl.y =  0L;
		    multLineHor = -1;
		    multLineVert = 0;
		    break;
		case 270:
		    grdl.x =  0L;
		    grdl.y = -1L;
		    multLineHor = 0;
		    multLineVert = -1;
		    break;
		default:  {
		    double t = t1 * M_PI/180;

		    grdl.x = (LONG) (100 * cos(t));
		    grdl.y = (LONG) (100 * sin(t));
		    multLineHor = cos(t);
		    multLineVert = sin(t);
		} /* default case */
		} /* switch(t1) */

		GpiSetCharAngle(hps, &grdl);
		break;
	    }

	    case SET_LINE :   /* line type */
	    {
		int lt, col;

		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		BufRead(hRead,&lt, sizeof(int), &cbR);
		/* linetype = -2 axes, -1 border, 0 arrows, all to 0 */
		col = lt;
		if (lt == -2)     GpiSetLineWidthGeom(hps, DEFLW*0.85);
		else if (lt == -1) GpiSetLineWidthGeom(hps, DEFLW*0.6);
		else GpiSetLineWidthGeom(hps, linewidth);
		if (lt < 0) lt = 0;
		lt =(lt%8);
		col =(col+2)%16;
		GpiLabel(hps, lLineTypes[lt]);
		lOldLine = lt;
		LType((bLineTypes) ? lt : 0);
		/* GpiSetLineType(hps, (bLineTypes) ? lLineTypes[lt] : lLineTypes[0]); */
		/* maintain some flexibility here in case we don't want
		 * the model T option */
		if (bColours)
		    GpiSetColor(hps, RGB_TRANS(lCols[col]));
		/* else GpiSetColor(hps, RGB_TRANS(CLR_BLACK)); */
		else
		    GpiSetColor(hps, RGB_TRANS(CLR_NEUTRAL));
		pm3d_color = -1; /* switch off using pm3d colours */
		break;
	    }

	    case SET_FILLBOX :   /* fill box */
	    {
		int style;
		unsigned int x, y, w, h;
		POINTL pt;

		BufRead(hRead,&style, sizeof(style), &cbR);
		BufRead(hRead,&x, sizeof(x), &cbR);
		BufRead(hRead,&y, sizeof(y), &cbR);
		BufRead(hRead,&w, sizeof(w), &cbR);
		BufRead(hRead,&h, sizeof(h), &cbR);
		pt.x = x;
		pt.y = y;
		GpiMove(hpsScreen, &pt);
		pt.x += w;
		pt.y += h;

		switch(style & 0xf) {

		    case FS_SOLID:
		    case FS_TRANSPARENT_SOLID:
		    {
			/* style == 1 --> fill with intensity according to filldensity */
			static const ULONG patternlist[] = {
			    PATSYM_NOSHADE, PATSYM_DENSE8, PATSYM_DENSE7,
			    PATSYM_DENSE6, PATSYM_DENSE5, PATSYM_DENSE4,
			    PATSYM_DENSE3, PATSYM_DENSE2, PATSYM_DENSE1,
			    PATSYM_SOLID
			};
			unsigned pattern;

			pattern = (unsigned) trunc(9*((style >> 4) / 100.0) + 0.5);
			if (pattern > 10)
			    pattern = 9; /* only 10 patterns in list */
			GpiSetMix(hps, FM_OVERPAINT);
			GpiSetBackMix(hps, BM_OVERPAINT);
			GpiSetPattern(hps, patternlist[pattern]);
			break;
		    }

		    case FS_PATTERN:
		    case FS_TRANSPARENT_PATTERN:
		    {
			/* style == 2 --> fill with pattern according to fillpattern */
			/* the upper 3 nibbles of 'style' contain pattern number */
			static const ULONG patternlist[] = {
			    PATSYM_NOSHADE, PATSYM_DIAGHATCH,
			    PATSYM_HATCH, PATSYM_SOLID,
			    PATSYM_DIAG4, PATSYM_DIAG2,
			    PATSYM_DIAG3, PATSYM_DIAG1
			};
			unsigned pattern;

			pattern = (style >> 4) % 8;
			GpiSetMix(hps, FM_OVERPAINT);
			GpiSetBackMix(hps, BM_OVERPAINT);
			GpiSetPattern(hps, patternlist[pattern]);
			break;
		    }

		    case FS_EMPTY:
		    default:
		    {
			/* style == 0 or unknown --> fill with background color */
			GpiSetMix(hps, FM_OVERPAINT);
			GpiSetBackMix(hps, BM_OVERPAINT);
			//GpiSetColor(hps, RGB_TRANS(CLR_BACKGROUND));  // fixes 'with boxes' white on white
			GpiSetPattern(hps, PATSYM_SOLID);
		    }
		}
		GpiBox(hps, DRO_FILL, &pt, 0,0);
		break;
	    }

	    case SET_LINEWIDTH :   /* line width */
	    {
		int lw;

		if (bPath) {
		    GpiEndPath(hps);
		    GpiStrokePath(hps, 1, 0);
		    bPath = FALSE;
		}
		BufRead(hRead,&lw, sizeof(int), &cbR);
		GpiSetLineWidthGeom(hps, DEFLW*lw/100);
		linewidth = DEFLW*lw/100;
		break;
	    }

	    case SET_POINTMODE :   /* points mode */
	    {
		int lt;

		BufRead(hRead,&lt, sizeof(int), &cbR);
		/* 1: enter point mode, 0: exit */
		if (bLineTypes) {
		    if (lt==1)
			LType(0);
		    else
			LType(lOldLine);
#if 0
		    if (lt == 1)
			lOldLine = GpiSetLineType(hps, lLineTypes[0]);
		    else
			GpiSetLineType(hps, lOldLine);
#endif /* 0 */
		}
#if 0
		if (lt == 1)
		    GpiSetLineWidthGeom(hps, 20);
		else
		    GpiSetLineWidthGeom(hps, 50);
#endif /* 0 */
		bDots = lt;
	    }
	    break;

	    case SET_FONT :   /* set font */
	    {
		int len;

		BufRead(hRead, &len, sizeof(int), &cbR);
		len = (len + sizeof(int) - 1) / sizeof(int);

		if (len == 0) {
		    SwapFont(hps, NULL);
		    strcpy(szCurrentFontNameSize, szFontNameSize);
		} else {
		    char font[FONTBUF];
		    char *p, *tmp, *str;

		    tmp = str = malloc(len * sizeof(int));
		    BufRead(hRead, str, len * sizeof(int), &cbR);
		    p = strchr(str, ',');
		    if (p==NULL)
			strcpy(font, "10");
		    else {
			*p = '\0';
			strcpy(font, p+1);
		    }
		    strcat(font, ".");
		    /* allow abbreviation of some well known font names */
		    FontExpand(str);
		    strcat(font, str);
		    free(tmp);
		    SwapFont(hps, font);
		    strcpy(szCurrentFontNameSize, font);
		} /* else(len==0) */
		break;
	    }

	    case GR_QUERY_FONT : /* query current font */
	    {
		int namelen;

		namelen = strlen(szCurrentFontNameSize);
		DosWrite(hRead, &namelen, sizeof(int), &cbR);
		DosWrite(hRead, szCurrentFontNameSize, namelen, &cbR);
		/* FIXME: is padding necessary? */
		break;
	    }

	    case SET_OPTIONS :   /* set options */
	    {
		int len;
		char *str;

		BufRead(hRead,&len, sizeof(int), &cbR);
		len =(len + sizeof(int) - 1) / sizeof(int);
		bWideLines = FALSE; /* reset options */
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
		bEnhanced = FALSE;
#endif
		if (len > 0) {
		    char *p;

		    p = str = malloc(len * sizeof(int));
		    BufRead(hRead, str, len * sizeof(int), &cbR);
		    while ((p=strchr(p,'-')) != NULL) {
			++p;
			if (*p == 'w')
			    bWideLines = TRUE;
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
			if (*p == 'e')
			    bEnhanced = TRUE;
#endif
			++p;
		    }
		    free(str);
		}
		break;
	    }

	    case SET_SPECIAL :   /* set special options */
	    {
		char opt;
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
		char param;
		static int prev_bEnhanced = 0;
#endif

		BufRead(hRead,&opt, 1, &cbR);
		switch (opt) {
#ifdef PM_KEEP_OLD_ENHANCED_TEXT
		case 'e': /* enhanced mode on, off and restore */
		    BufRead(hRead,&param, 1, &cbR);	    
		    switch (param) {
		    case '0': prev_bEnhanced = bEnhanced;
			bEnhanced = 0;
			break;
		    case '1': prev_bEnhanced = bEnhanced;
			bEnhanced = 1;
			break;
		    case '2': bEnhanced = prev_bEnhanced;
			break;
		    }
		    break;
#endif
		case 'c': /* set codepage */
		    BufRead(hRead,&codepage, sizeof(codepage), &cbR);
		    break;
	        case '^': /* raise window */
		    WinSetWindowPos( hwndFrame, HWND_TOP, 0,0,0,0, SWP_RESTORE|SWP_SHOW|SWP_ACTIVATE|SWP_ZORDER ) ;
		    WinSetFocus( HWND_DESKTOP, hApp ) ;
		    break;
	        case '_': /* lower window */
		    WinSetWindowPos( hwndFrame, HWND_BOTTOM, 0,0,0,0, SWP_ZORDER ) ;
		    break;
		}
		break;
	    }

	    case PUT_TMPTEXT :
	    {  /* put_tmptext(int i, char c[]) term API */
		/* i = 0 at statusline,
		   1,2: at corners of zoom box, with \r separating text */
		int where, l;
		static char *text = NULL;
		static int text_alloc = -1;

		/* Position of the "table" of values resulting from
		 * mouse movement(and clicks).  Negative y value would
		 * position it at the top of the window---not
		 * implemented. */
		BufRead(hRead,&where, sizeof(int), &cbR);
		BufRead(hRead,&l, sizeof(int), &cbR);
		if (text_alloc < l)
		    text = realloc(text, text_alloc = l+10);
		BufRead(hRead,&text[0], l, &cbR);
		switch (where) {
		case 0:
		    UpdateStatusLine(hps,text);
		    break;
		case 1:
		case 2:
		    break; /* not implemented */
		}
		break;
	    }

	    /* Implementation problems(I haven't understood that from
	     * .INF doc): what is the difference between
	     * GpiCreateLogColorTable and GpiCreatePalette? */
	    case GR_MAKE_PALETTE :
	    {
		unsigned char c;
		int smooth_colors;
		LONG lRetCount;

		/* read switch */
		BufRead(hRead, &c, sizeof(c), &cbR);
		if (c == 0) {
		    /* gnuplot asks for the number of colours in palette */
		    smooth_colors = (bPMPaletteMode ? (256 - nColors) : RGB_PALETTE_SIZE);
		    DosWrite(hRead, &smooth_colors, sizeof(int), &cbR);
		    DEBUG_COLOR(("GR_MAKE_PALETTE: max %i colours", smooth_colors));
		    break;
		}

		/* read the number of colours for the palette */
		BufRead(hRead, &smooth_colors, sizeof(int), &cbR);
		free(rgbTable);
		rgbTable = malloc(smooth_colors * sizeof(ULONG));

		/* append new RGB table after */
		DEBUG_COLOR(("GR_MAKE_PALETTE: reading palette with %i colours", smooth_colors));
		BufRead(hRead, &rgbTable[bPMPaletteMode ? nColors : 0],
			smooth_colors * sizeof(ULONG), &cbR);

		if (bPMPaletteMode) {
		    int i;
		    ULONG cclr;

		    /* preserve the first nColors entries of current palette */
		    /* retrieve the current table */
		    lRetCount = GpiQueryLogColorTable(hps, 0L, 0L, nColors, alColourTable);
		    if ((lRetCount > 0) && (lRetCount != nColors)) /* ring for developers! */
			DosBeep(880, 777);
		    for (i=0; i<nColors; i++)
			rgbTable[i] = alColourTable[i];

		    if (pm3d_hpal != 0)
			GpiDeletePalette(pm3d_hpal);
		    pm3d_hpal = GpiCreatePalette(hab, 0L, LCOLF_CONSECRGB,
					     (long) (nColors + smooth_colors), rgbTable);
		    pm3d_hpal_old = GpiSelectPalette(hps, pm3d_hpal);

		    /* tell presentation manager to use the new palette */
		    WinRealizePalette(WinWindowFromDC(hdcScreen), hps, &cclr);
		}
		break;
	    }

	    case GR_RELEASE_PALETTE :
#if 0 /* FIXME: REMOVE THIS ROUTINE COMPLETELY! */
		if (pm3d_hpal) {
		    GpiDeletePalette(pm3d_hpal);
		    pm3d_hpal = 0;
		}
		/* GpiSelectPalette(hps, pm3d_hpal_old); */
#endif
		break;

	    case GR_SET_COLOR :
	    {
		/* FIXME: usgage of uchar limits the size of the 'virtual'
			  palette to 256 entries. (see also RGB_PALETTE_SIZE) */
		unsigned char c;

		BufRead(hRead, &c, sizeof(c), &cbR);
		if (bPMPaletteMode)
		    pm3d_color = c + nColors;
		else
		    pm3d_color = rgbTable[c];
		DEBUG_COLOR(("GR_SET_COLOR: %i -> 0x%x", (int)c, pm3d_color)); 
		break;
	    }

	    case GR_SET_RGBCOLOR :
	    {
		int rgb_color;

		BufRead(hRead, &rgb_color, sizeof(rgb_color), &cbR);

		/* Find an approximate color in the current palette */
		if (bPMPaletteMode)
		    pm3d_color = GpiQueryColorIndex(hps, 0, rgb_color);
		else
		    pm3d_color = rgb_color;
#if 0
		{
		int real_rgb = GpiQueryRGBColor(hps, LCOLOPT_REALIZED, pm3d_color);
		DEBUG_COLOR(( "GR_SET_RGBCOLOR: req = %x  nearest = %x  index = %x", 
		              rgb_color, real_rgb, pm3d_color ));
		}
#endif
		break;
	    }

	    case GR_FILLED_POLYGON :
	    {
		int points, x,y, i;
		LONG curr_color;
		POINTL p;

		BufRead(hRead, &points, sizeof(points), &cbR);
		GpiSetPattern(hps, PATSYM_SOLID);
		GpiSetBackMix(hps, BM_OVERPAINT);
		if (pm3d_color >= 0) {
		    curr_color = GpiQueryColor(hps);
		    GpiSetColor(hps, pm3d_color);
		}

		/* using colours defined in the palette */
		GpiBeginArea(hps, BA_BOUNDARY | BA_ALTERNATE);
		for (i = 0; i < points; i++) {
		    BufRead(hRead, &x, sizeof(x), &cbR);
		    BufRead(hRead, &y, sizeof(y), &cbR);
		    p.x = x; p.y = y;
		    if (i)
			GpiLine(hps, &p);
		    else
			GpiMove(hps, &p);
		}
		GpiEndArea(hps);
		if (pm3d_color >= 0)
		    GpiSetColor(hps, curr_color);
		break;
	    }

	    case GR_RGB_IMAGE : 
	    {
		unsigned int i, M, N, image_size;
		POINTL corner[4];
		PBYTE image;
		PBITMAPINFO2 pbmi;
		POINTL points[4];
		LONG hits;
		image_list_entry *ile;

		BufRead(hRead, &M, sizeof(M), &cbR);
		BufRead(hRead, &N, sizeof(N), &cbR);
		for (i=0; i<4; i++) {
		    BufRead(hRead, &(corner[i].x), sizeof(int), &cbR);
		    BufRead(hRead, &(corner[i].y), sizeof(int), &cbR);
		}
		BufRead(hRead, &image_size, sizeof(image_size), &cbR);
		DEBUG_IMAGE(("GR_IMAGE: M=%i, N=%i, size=%i", M, N, image_size));
		DEBUG_IMAGE(("GR_IMAGE: corner [0]=(%i,%i) [1]=(%i,%i)", corner[0].x, corner[0].y, corner[1].x, corner[1].y));
		image = (PBYTE) malloc(image_size);
		/* FIXME: does not work if GNUBUF < image_size ! */
		BufRead(hRead, image, image_size, &cbR);

		points[0].x = corner[0].x;
		points[0].y = corner[1].y;
		points[1].x = corner[1].x; 
		points[1].y = corner[0].y;
		points[2].x = points[2].y = 0;
		points[3].x = M;
		points[3].y = N;

		pbmi = (PBITMAPINFO2) calloc( sizeof(BITMAPINFOHEADER2), 1 );
		pbmi->cbFix = sizeof(BITMAPINFOHEADER2);
		pbmi->cx = M;
		pbmi->cy = N;
		pbmi->cPlanes = 1;
		pbmi->cBitCount = 24;
		pbmi->ulCompression = BCA_UNCOMP;
		hits = GpiDrawBits(hps, image, pbmi, 4, &points, ROP_SRCCOPY, BBO_IGNORE );

#if 0
		if (hits == GPI_ERROR) {
		    PERRINFO perriBlk = WinGetErrorInfo(hab);
		    if (perriBlk) {
			PSZ pszOffset, pszErrMsg;
			pszOffset = ((PSZ)perriBlk) + perriBlk->offaoffszMsg;
			pszErrMsg = ((PSZ)perriBlk) + *((PULONG)pszOffset);
			if (perriBlk->cDetailLevel >= 2)
				pszErrMsg = ((PSZ)perriBlk) + ((PULONG)pszOffset)[1];
			DEBUG_IMAGE(("GpiDrawBits code=%x msg=%s", perriBlk->idError, pszErrMsg));
			// DEBUG_IMAGE(("GpiDrawBits code=%x", perriErrorInfo->idError)); 
			WinFreeErrorInfo(perriBlk);
		    }
		}
#endif

		/* We have to keep the image and the image header in memory since
		   we use retained graphics */
		ile = (image_list_entry *) malloc(sizeof(image_list_entry));
		ile->next = image_list;
		ile->pbmi = pbmi;
		ile->image = image;
		image_list = ile;

		break;
	    }

	    case SET_RULER : { /* set_ruler(int x, int y) term API: x<0 switches ruler off */
		int x, y;

		BufRead(hRead, &x, sizeof(x), &cbR);
		BufRead(hRead, &y, sizeof(y), &cbR);
		DrawRuler(); /* remove previous drawing, if any */
		if (x < 0) {
		    ruler.on = 0;
		    break;
		}
		ruler.on = 1;
		ruler.x = x;
		ruler.y = y;
		DrawRuler();
		break;
	    }

	    case SET_CURSOR : { /* set_cursor(int c, int x, int y) term API */
		int c, x, y;

		BufRead(hRead, &c, sizeof(x), &cbR);
		BufRead(hRead, &x, sizeof(x), &cbR);
		BufRead(hRead, &y, sizeof(y), &cbR);
		switch (c) {
		case -2: { /* move mouse to the given point */
		    RECTL rc;
		    POINTL pt;

		    GpiQueryPageViewport(hpsScreen,&rc);
		    /* only distance is important */
		    rc.xRight -= rc.xLeft;
		    rc.yTop -= rc.yBottom;
		    /* window => pixels coordinates */
		    pt.x =(long int) ((x * (double) rc.xRight) / 19500.0);
		    pt.y =(long int) ((x * (double) rc.yTop) / 12500.0);
		    WinMapWindowPoints(hApp, HWND_DESKTOP, &pt, 1);
		    WinSetPointerPos(HWND_DESKTOP, pt.x, pt.y);
		    break;
		}
		case -1: /* start zooming; zooming cursor */
		    zoombox.on = 1;
		    zoombox.from.x = zoombox.to.x = x;
		    zoombox.from.y = zoombox.to.y = y;
		    break;
		case 0:  /* standard cross-hair cursor */
		    WinSetPointer(HWND_DESKTOP,
				  hptrCurrent =(useMouse
						?hptrCrossHair
						:hptrDefault));
		    break;
		case 1:  /* cursor during rotation */
		    WinSetPointer(HWND_DESKTOP, hptrCurrent = hptrRotating);
		    break;
		case 2:  /* cursor during scaling */
		    WinSetPointer(HWND_DESKTOP, hptrCurrent = hptrScaling);
		    break;
		case 3:  /* cursor during zooming */
		    WinSetPointer(HWND_DESKTOP, hptrCurrent = hptrZooming);
		    break;
		}
		if (c>=0 && zoombox.on) { /* erase zoom box */
		    DrawZoomBox();
		    zoombox.on = 0;
		}
		break;
	    }

	    case SET_CLIPBOARD : {  /* set_clipboard(const char s[]) term API */
		int len;
		char *s;

		BufRead(hRead, &len, sizeof(int), &cbR);
		s = malloc(len + 1);
		BufRead(hRead,s, len+1, &cbR);
		TextToClipboard(s);
		free(s);
		break;
	    }

	    case SET_MENU :   /* update menu according to the gnuplot core
			  * settings, e.g.(un)checking menu items */
		if (mouseTerminal) {
		    /* we are connected to mouseable terminal */
		    BufRead(hRead, &gpPMmenu, sizeof(gpPMmenu), &cbR);
		    if (useMouse != gpPMmenu.use_mouse)
			WinSetPointer(HWND_DESKTOP,
				      hptrCurrent = (gpPMmenu.use_mouse
						     ?hptrCrossHair
						     :hptrDefault));
		    useMouse = gpPMmenu.use_mouse;
		    gpPMmenu_update_req = 1;
		}
		break;

	    case GR_MOUSECAPABLE :
		/* notification of being connected to a mouse-enabled terminal */
		mouseTerminal = 1;
		break;

	    default :  /* should handle error */
		break;
	    }
	}
    }
    DosDisConnectNPipe(hRead);
    WinPostMsg(hApp, WM_CLOSE, 0L, 0L);
}


static void
EditLineTypes(HWND hwnd, HPS hps, BOOL bDashed)
{
    int i;

    GpiSetDrawingMode(hps, DM_RETAIN);
    GpiOpenSegment(hps, iSeg);
    GpiSetEditMode(hps, SEGEM_REPLACE);
    for (i=0; i<7; i++) {
        while (GpiSetElementPointerAtLabel(hps, lLineTypes[i])) {
            GpiOffsetElementPointer(hps, 1);
            GpiSetLineType(hps, bDashed?lLineTypes[i]:lLineTypes[0]);
	}
        GpiSetElementPointer(hps, 0);
    }
    GpiSetEditMode(hps, SEGEM_INSERT);
    GpiCloseSegment(hps);
}


#if 0
/*
** Edit segment to change char cell(font size)
*/
static void
EditCharCell(HPS hps, SIZEF *psize)
{
    int i;
    LONG rl, rc;
    SIZEF sizH, sizV;
    int iVert = 0;

    sizH = *psize;
    sizV.cx = sizH.cy;
    sizV.cy = sizH.cx;
    GpiSetDrawingMode(hps, DM_RETAIN);
    GpiOpenSegment(hps, iSeg);
    GpiSetEditMode(hps, SEGEM_REPLACE);
    i=0;
    while (GpiSetElementPointer(hps, i)) {
        rc = GpiQueryElementPointer(hps);
        if (rc != i)
	    break;
        rl = GpiQueryElementType(hps, &rc, 0, NULL);
        if (rc == 0x34 || rc == 0x74) {
            LONG gdata;

            GpiQueryElement(hps, 5, 4, (PBYTE) &gdata);
            if (gdata == 0)
		iVert = 0;
            else
		iVert = 1;
	}
        else if (rc==0x33 || rc==0x03)
	    GpiSetCharBox(hps, iVert ? &sizV : &sizH);
        ++i;
    }
    GpiSetEditMode(hps, SEGEM_INSERT);
    GpiCloseSegment(hps);
}
#endif

/*
** pull next plot command out of buffer read from GNUPLOT
*/
static int
BufRead(HFILE hfile, void *buf, int nBytes, ULONG *pcbR)
{
    ULONG ulR, ulRR;
    int rc;
    static char buffer[GNUBUF];
    static char *pbuffer = buffer+GNUBUF, *ebuffer = buffer+GNUBUF;

    for (; nBytes > 0; nBytes--) {
        if (pbuffer >= ebuffer) {
            ulR = GNUBUF;
            rc = DosRead(hfile, buffer, ulR, &ulRR);
            if (rc != 0)
		return rc;
            if (ulRR == 0)
		return 1;
            pbuffer = buffer;
            ebuffer = pbuffer+ulRR;
	}
        *(char*)buf++ = *pbuffer++;
    }
    return 0L;
}


#ifdef STANDARD_FONT_DIALOG

/*
** Get a new font using standard font dialog
*/
int
GetNewFont(HWND hwnd, HPS hps)
{
    static FONTDLG pfdFontdlg;      /* Font dialog info structure */
    static int i1 =1;
    static int iSize;
    char szPtList[64];
    HWND hwndFontDlg;     /* Font dialog window handle */
    char szFamilyname[FACESIZE];

    if (i1) {
        strcpy(pfdFontdlg.fAttrs.szFacename, strchr(szFontNameSize, '.') + 1);
        strcpy(szFamilyname, strchr(szFontNameSize, '.') + 1);
        sscanf(szFontNameSize, "%d", &iSize);
        memset(&pfdFontdlg, 0, sizeof(FONTDLG));

        pfdFontdlg.cbSize = sizeof(FONTDLG);
        pfdFontdlg.hpsScreen = hps;
	/*   szFamilyname[0] = 0; */
        pfdFontdlg.pszFamilyname = szFamilyname;
        pfdFontdlg.usFamilyBufLen = FACESIZE;
        pfdFontdlg.fl = FNTS_HELPBUTTON |
	    FNTS_CENTER | FNTS_VECTORONLY |
	    FNTS_OWNERDRAWPREVIEW;
        pfdFontdlg.clrFore = CLR_BLACK;
        pfdFontdlg.clrBack = CLR_WHITE;
        pfdFontdlg.usWeight = FWEIGHT_NORMAL;
        pfdFontdlg.fAttrs.usCodePage = codepage;
        pfdFontdlg.fAttrs.usRecordLength = sizeof(FATTRS);
    }
    sprintf(szPtList, "%d 8 10 12 14 18 24", iSize);
    pfdFontdlg.pszPtSizeList = szPtList;
    pfdFontdlg.fxPointSize = MAKEFIXED(iSize,0);
    hwndFontDlg = WinFontDlg(HWND_DESKTOP, hwnd, &pfdFontdlg);
    if (i1) {
        pfdFontdlg.fl = FNTS_HELPBUTTON |
	    FNTS_CENTER | FNTS_VECTORONLY |
	    FNTS_INITFROMFATTRS;
        i1=0;
    }
    if (hwndFontDlg &&(pfdFontdlg.lReturn == DID_OK)) {
        iSize = FIXEDINT(pfdFontdlg.fxPointSize);
        sprintf(szFontNameSize, "%d.%s", iSize, pfdFontdlg.fAttrs.szFacename);
        return 1;
    } else
	return 0;
}

#else

/*
** Get a new font using standard font palette
*/
int
GetNewFont(HWND hwnd, HPS hps)
{
    HOBJECT hObject;
    ULONG ulView = 0; /* OPEN_DEFAULT */
    BOOL fSuccess = FALSE;

    hObject = WinQueryObject("<WP_FNTPAL>");
    if (hObject != NULLHANDLE)
        fSuccess = WinOpenObject(hObject, ulView, TRUE);
    return fSuccess ? 1 : 0;
}

#endif /* STANDARD_FONT_DIALOG */


/*
**  Handle termination signal to free up resources before
**  termination.
*/
void SigHandler(int sig)
{
    if (sig == SIGTERM) {
        if (bPersist) {
            DosKillThread(tidSpawn);
            return;
	}
        DosEnterCritSec();
        DosKillThread(tidSpawn);
        DosKillThread(tidDraw);
        DosExitCritSec();
        exit(0);
    }
}


#ifdef PM_KEEP_OLD_ENHANCED_TEXT

/* disable debugging info */
#define TEXT_DEBUG(x) /* fprintf x */ ;

/* process a bit of string, and return the last character used.
 * p is start of string
 * brace is TRUE to keep processing to }, FALSE for do one character
 * fontname & fontsize are obvious
 * base is the current baseline
 * widthflag is TRUE if the width of this should count,
 *              FALSE for zero width boxes
 * showflag is TRUE if this should be shown,
 *             FALSE if it should not be shown(like TeX \phantom)
 */

static char *starttext = NULL;
static int  textlen = 0;
static BOOL bText = FALSE;
static int  textwidth = 0;
static POINTL ptlText;
static FILE *ff;

static char
*ParseText(
    HPS hps,
    char *p,
    BOOL brace,
    char *fontname,
    int fontsize, int base,
    BOOL widthflag, BOOL showflag)
{
    POINTL aptl[TXTBOX_COUNT];
    BOOL bChangeFont = FALSE;
    TEXT_DEBUG((ff, "RECURSE WITH [%p] %s, %d %s %.1f %.1f %d %d\n",
		p, p, brace, fontname, fontsize, base,
		widthflag, showflag));

    /* Start each recursion with a clean string */
#if 0
    {
	FILE *ff = fopen("deb","a");
	int i=textlen;

	for (i=0;i<textlen;i++)
	    fputc(starttext[i], ff);
	fputc('\n',ff);
	fclose(ff);
    }
#endif /* 0 */

    if (textlen > 0) {
	GpiQueryTextBox(hps, textlen, starttext, TXTBOX_COUNT, aptl);
	textwidth += aptl[TXTBOX_BOTTOMRIGHT].x * multLineHor;
	textwidth += aptl[TXTBOX_BOTTOMRIGHT].y * multLineVert;
    }

    if (bText) {
	if (textlen > 0) {
	    GpiCharStringAt(hps, &ptlText, textlen, starttext);
	    ptlText.x += aptl[TXTBOX_CONCAT].x - multLineVert * base;
	    ptlText.y += aptl[TXTBOX_CONCAT].y + multLineHor * base;
	} else {
	    ptlText.x -= multLineVert * base;
	    ptlText.y += multLineHor * base;
	}
    }
    textlen = 0;
    starttext = p;

    if (fontname != NULL) {
	char szFont[FONTBUF];

	sprintf(szFont, "%d.%s", fontsize, fontname);
	SwapFont(hps, szFont);
	bChangeFont = TRUE;
    }
    if (base != 0)
	GpiSetCharBox(hps, &sizCurSubSup);

    for (; *p; ++p) {
	int shift;

	switch (*p) {
	case '}':
	    /*{{{  deal with it*/
	    if (brace) {
		brace = 0;
	    }
	    break;
	    /*}}}*/

	case '_':
	case '^':
	    /*{{{  deal with super/sub script*/
	    shift = ((*p == '^') ? lSupOffset : lSubOffset);
	    p = ParseText(hps, p+1, FALSE,
			  NULL/*fontname*/, fontsize*0.8,
			  base+shift, widthflag, showflag);
	    break;
	    /*}}}*/

	case '{':
	{
	    char *savepos=NULL, save=0;
	    char *localfontname=fontname, ch;
	    char localfontbuf[FONTBUF];
	    int recode=1;
	    int f=fontsize;
	    char *q=localfontbuf;

	    /*{{{  recurse(possibly with a new font) */

	    TEXT_DEBUG((ff,"Dealing with {\n"));
	    if (*++p == '/') {
		/* then parse a fontname, optional fontsize */
		while (*++p == ' ')
		    ;		/* do nothing */
		if (*p=='-') {
		    recode=0;
		    while (*++p == ' ')
			;	/* do nothing */
		}
		localfontname = p;
		while ((ch = *p) > ' ' && ch != '=') {
		    localfontname=localfontbuf;
		    if (*p=='_')
			*q=' ';
		    else
			*q=*p;
		    ++p;++q;
		}
		*q = '\0';
		FontExpand(localfontbuf);
		save = *(savepos=p);
		if (ch == '=') {
		    *p++ = '\0';
		    /*{{{  get optional font size*/
		    TEXT_DEBUG((ff,"Calling strtod(%s) ...", p));
		    f = strtod(p, &p);
		    TEXT_DEBUG((ff,"Retured %.1f and %s\n", f, p));

		    if (!f)
			f = fontsize;

		    TEXT_DEBUG((ff,"Font size %.1f\n", f));
		    /*}}}*/
		} else {
		    *p++ = '\0';
		    f = fontsize;
		}

		while (*p == ' ')
		    ++p;

		if (!(*localfontname)) {
		    localfontname = fontname;
		    if (f != fontsize)
			localfontname = strchr(szFontNameSize, '.') + 1;
		}
	    } /* if ('/') */
	    /*}}}*/

	    TEXT_DEBUG((ff,"Before recursing, we are at [%p] %s\n", p, p));

	    p = ParseText(hps,p, TRUE, localfontname, f, base, widthflag, showflag);

	    TEXT_DEBUG((ff,"BACK WITH %s\n", p));
	    if (savepos)
		/* restore overwritten character */
		*savepos = save;

	    break;
	}

	case '@' :
	    /*{{{  phantom box - prints next 'char', then restores currentpoint */

	    p = ParseText(hps, ++p, FALSE, NULL/*fontname*/, fontsize,
			  base, FALSE, showflag);

	    break;
	    /*}}}*/

	case '&' :
	    /*{{{  character skip - skips space equal to length of character(s) */

	    p = ParseText(hps, ++p, FALSE, NULL/*fontname*/, fontsize,
			  base, widthflag, FALSE);

	    break;
	    /*}}}*/

	case '\\':
	{
	    char buffer[4]; /* should need only one char.. */
	    char *q = buffer;

	    *q = '\0';
	    ParseText(hps,q, FALSE, NULL, fontsize, base, widthflag, showflag);

	    /*{{{  is it an escape */
	    /* special cases */
	    if (p[1]=='\\' || p[1]=='{' || p[1]=='}') {
		*q++=p[1];
		++p;
	    }
#if 0
	    else if (p[1] >= '0' && p[1] <= '7') {
		/* up to 3 octal digits */
		int c = 0;

		c+=p[1];
		++p;
		if (p[1] >= '0' && p[1] <= '7') {
		    c = c * 8 + p[1];
		    ++p;
		    if (p[1] >= '0' && p[1] <= '7') {
			c = c * 8 + p[1];
			++p;
		    }
		}
		*q++ = c;
		break;
	    }
#endif /* 0 */

	    *q = '\0';
	    textlen = 1;
	    starttext=buffer;
	    ParseText(hps,q, FALSE, NULL/*fontname*/, fontsize,
		      base, widthflag, showflag);
	    starttext=p+1;
	    textlen = 0;
	    /*}}}*/
	    break;
	}

	default:
	    ++textlen;

	    /*}}}*/
	} /* switch(character) */

	/* like TeX, we only do one character in a recursion, unless it's
	 * in braces
	 */
	if (!brace)
	    break;
    }

    if (textlen > 0) {
	GpiQueryTextBox(hps, textlen, starttext, TXTBOX_COUNT, aptl);
	if (widthflag) {
	    textwidth += aptl[TXTBOX_BOTTOMRIGHT].x * multLineHor;
	    textwidth += aptl[TXTBOX_BOTTOMRIGHT].y * multLineVert;
	}
    }
    if (bText) {
	if (textlen > 0) {
	    if (showflag)
		GpiCharStringAt(hps, &ptlText, textlen, starttext);
	    if (widthflag) {
		ptlText.x += aptl[TXTBOX_CONCAT].x;
		ptlText.y += aptl[TXTBOX_CONCAT].y;
	    }
	}
	if (base != 0) {
	    ptlText.x += multLineVert * base;
	    ptlText.y -= multLineHor * base;
	}
    }
    if (bChangeFont) {
	SwapFont(hps, NULL);
	bChangeFont = FALSE;
    }
    if (base != 0)
	GpiSetCharBox(hps, &sizBaseFont);

    textlen = 0;
    starttext = p+1;
    return p;
}


static void
CharStringAt(HPS hps, int x, int y, int len, char *str)
{
    /* flush any pending graphics(all the XShow routines do this...) */
    char *fontname;
    int fontsize;

    if (!strlen(str))
        return;

    /* set up the globals */

    ptlText.x = x;
    ptlText.y = y;
    bText = TRUE;
    starttext = NULL;
    textlen = 0;
    textwidth = 0;
    sscanf(szFontNameSize, "%d", &fontsize);
    fontname = strchr(szFontNameSize, '.') + 1;

    while (*(str = ParseText(hps, str, TRUE, NULL,
			     fontsize,
			     0.0, TRUE, TRUE)))
	; /* do nothing */
}


static int
QueryTextBox(HPS hps, int len, char *str)
{
    char *fontname;
    int fontsize;

    if (!strlen(str))
        return 0;

    /* set up the globals */
    bText = FALSE;
    starttext = NULL;
    textlen = 0;
    textwidth = 0;
    sscanf(szFontNameSize, "%d", &fontsize);
    fontname = strchr(szFontNameSize, '.') + 1;

    while (*(str = ParseText(hps, str, TRUE, NULL,
			     fontsize,
			     0.0, TRUE, TRUE)))
	; /* do nothing */

    return textwidth;
}

#endif


void
FontExpand(char *name)
{
    if (strcmp(name,"S") == 0)
	strcpy(name, "Symbol Set");
    if (strcmp(name,"Symbol") == 0)
	strcpy(name, "Symbol Set");
    else if (strcmp(name,"H") == 0)
	strcpy(name, "Helvetica");
    else if (strcmp(name,"T") == 0)
	strcpy(name, "Times New Roman");
    else if (strcmp(name,"C") == 0)
	strcpy(name, "Courier");
}


/*=======================================*/

static POINTL pCur;
static int iLinebegin = 1;
static int iLtype = 0;
static int iState = 0;
static double togo = 0.0;
static int iPatt[8][9] = {
    {   0,   0,   0,   0,   0,   0,   0,   0,  0 },
    { 300, 200,  -1,   0,   0,   0,   0,   0,  0 },
    { 150, 150,  -1,   0,   0,   0,   0,   0,  0 },
    { 300, 200, 150, 200,  -1,   0,   0,   0,  0 },
    { 500, 200,  -1,   0,   0,   0,   0,   0,  0 },
    { 300, 200, 150, 200, 150, 200,  -1,   0,  0 },
    { 300, 200, 150, 200, 150, 200, 150, 200, -1 },
    { 500, 200, 150, 200,  -1,   0,   0,   0,  0 }
};


void
LMove(HPS hps, POINTL *p)
{
    double ds, dx, dy;

    if (iLinebegin) {
        pCur = *p;
        GpiMove(hps, p);
    } else if (iLtype == 0)
	GpiMove(hps, p);
    else {
        dx = p->x - pCur.x;
        dy = p->y - pCur.y;
        ds = sqrt(dx*dx + dy*dy);
        dx /= ds; dy /= ds;
        while (ds > 0.0) {
            if (ds < togo) {
                togo -= ds;
                ds = 0.0;
                GpiMove(hps, p);
                pCur = *p;
	    } else {
                POINTL pn;

                pn.x = pCur.x + togo * dx;
                pn.y = pCur.y + togo * dy;
                GpiMove(hps, &pn);
                pCur = pn;
                ds -= togo;
                iState++;
                if (iPatt[iLtype][iState] < 0) {
                    togo = iPatt[iLtype][0];
                    iState = 0;
		} else
		    togo = iPatt[iLtype][iState];
	    }
	}
    }
}


void
LLine(HPS hps, POINTL *p)
{
    double ds, dx, dy;

    if (iLinebegin)
	iLinebegin = 0;

    if (iLtype == 0)
	GpiLine(hps, p);
    else {
        dx = p->x - pCur.x;
        dy = p->y - pCur.y;
        ds = sqrt(dx*dx + dy*dy);
        dx /= ds; dy /= ds;
        while (ds > 0.0) {
            if (ds < togo) {
                togo -= ds;
                ds = 0.0;
                if (iState & 1)
		    GpiMove(hps, p);
                else
		    GpiLine(hps, p);
                pCur = *p;
	    } else {
                POINTL pn;

                pn.x = pCur.x + togo * dx;
                pn.y = pCur.y + togo * dy;
                if (iState & 1)
		    GpiMove(hps, &pn);
                else
		    GpiLine(hps, &pn);
                pCur = pn;
                ds -= togo;
                iState++;
                if (iPatt[iLtype][iState] < 0) {
                    togo = iPatt[iLtype][0];
		    iState = 0;
		} else
		    togo = iPatt[iLtype][iState];
	    }
	}
    }
}

void
LType(int iType)
{
    iLinebegin = 1;
    if (iType > 7)
	iType = 0;
    iLtype = iType;
    iState = 0;
    togo = iPatt[iLtype][0];
}


/* Now routines for mouse etc. */

static void
TextToClipboard(PCSZ szTextIn)
{
    /* copy string given by szTextIn to the clipboard */
    PSZ szTextOut = NULL;
    ULONG ulRC = DosAllocSharedMem(
	(PVOID*) &szTextOut, NULL,
	strlen(szTextIn) + 1,
	PAG_READ | PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE);

    if (!ulRC) {
	HAB hab = 0;

	strcpy(szTextOut, szTextIn);
	WinOpenClipbrd(hab);
	WinEmptyClipbrd(hab);
	WinSetClipbrdData(hab,(ULONG) szTextOut, CF_TEXT, CFI_POINTER);
	WinCloseClipbrd(hab);
    }
}


/* Draw the ruler
*/
static void
DrawRuler()
{
    POINTL p;

    if (!ruler.on || ruler.x < 0)
	return;
    /* GpiSetColor(hpsScreen, RGB_TRANS(COLOR_RULER)); */
    GpiSetColor(hpsScreen, RGB_TRANS(CLR_RED));
    GpiSetLineWidth(hpsScreen, LINEWIDTH_THICK);
    GpiSetMix(hpsScreen, FM_INVERT);
    /*GpiBeginPath(hpsScreen, 1); // will this help? I don't know, but makes thic cross */

    p.x = 0;
    p.y = ruler.y;
    GpiMove(hpsScreen, &p);

    p.x = 19500;
    GpiLine(hpsScreen, &p);

    p.x = ruler.x;
    p.y = 0;
    GpiMove(hpsScreen, &p);

    p.y = 12500;
    GpiLine(hpsScreen, &p);

    /*GpiEndPath(hpsScreen); */
    /*GpiStrokePath(hpsScreen, 1, 0); */
}


#if 0
/* This routine recalculates mouse/pointer position [mx,my] in [in pixels]
current window to the real/true [x,y] coordinates of the plotted graph.
*/
void
MousePosToGraphPos(
    double *x, double *y,
    HWND hWnd,
    SHORT mx, SHORT my,
    ULONG mouse_mode)
{
    RECTL rc;

    if (mouse_mode == MOUSE_COORDINATES_PIXELS) {
	*x = mx;
	*y = my;
	return;
    }

    /* Rectangle where we are moving: viewport, not the full window! */
    GpiQueryPageViewport(hpsScreen, &rc);

    /* only distance is important */
    rc.xRight -= rc.xLeft;
    rc.yTop -= rc.yBottom;

    if (mouse_mode == MOUSE_COORDINATES_SCREEN) {
	*x =(double) mx / rc.xRight;
	*y =(double) my / rc.yTop;
	return;
    }

    /* px=px(mx); mouse=>gnuplot driver coordinates */
    *x = mx * 19500.0 / rc.xRight;
    *y = my * 12500.0 / rc.yTop;

    /* main job of transformation, which is not device dependent */
    MousePosToGraphPosReal(x, y);
}
#endif


/*
 * This routine recalculates mouse/pointer position [mx,my] in [in pixels]
 * current window to the viewport coordinates
 */
static void
MousePosToViewport(int *x, int *y, SHORT mx, SHORT my)
{
    RECTL rc;

    /* Rectangle where we are moving: viewport, not the full window! */
    GpiQueryPageViewport(hpsScreen, &rc);

    rc.xRight -= rc.xLeft;
    rc.yTop -= rc.yBottom; /* only distance is important */

    /* px=px(mx); mouse=>gnuplot driver coordinates */
    *x =(int)(mx * 19500.0 / rc.xRight + 0.5);
    *y =(int)(my * 12500.0 / rc.yTop + 0.5);
}


/*
 * This routine gets the mouse/pointer position in the current window and
 * recalculates it to the viewport coordinates
 */
static void
GetMousePosViewport(HWND hWnd, int *mx, int *my)
{
    POINTL p;

    WinQueryPointerPos(HWND_DESKTOP, &p); /* this is position wrt desktop */
    WinMapWindowPoints(HWND_DESKTOP, hWnd, &p, 1); /* pos. wrt our window in pixels */
    MousePosToViewport(mx,my,p.x,p.y);
}


/*
 * Status line previous and current text:
 */
static char *sl_curr_text = NULL;

/*
 * Display the status line by the text
 */
static void
DisplayStatusLine(HPS hps)
{
    POINTL pt;

    if (!sl_curr_text)
	return;
    GpiSetColor(hps, RGB_TRANS(COLOR_MOUSE)); /* set text color */
    GpiSetCharMode(hps, CM_MODE1);
    pt.x = 2;
    pt.y = 2;
    GpiSetMix(hps, FM_INVERT);
    /* GpiSetMix(hps, FM_XOR); */
    GpiCharStringAt(hps, &pt,(long) strlen(sl_curr_text), sl_curr_text);
}


/*
 * Update the status line by the text; firstly erase the previous text
 */
static void
UpdateStatusLine(HPS hps, char *text)
{
    if (gpPMmenu_update_req)
	gpPMmenu_update(); /* check for updated menu */
    if (sl_curr_text) {
	/* erase the previous text */
	DisplayStatusLine(hps);
	free(sl_curr_text);
    }
    if (!text || !*text)
	sl_curr_text = 0;
    else {
	/* display new text */
	sl_curr_text = strdup(text);
	DisplayStatusLine(hps);
    }
}


/*
 * Graphics part SET_GRAPHICS or anything else required to update menu according to
 * the items in gpPMmenu
 */
static void
gpPMmenu_update()
{
#if 0
    if (!gpPMmenu_update_req)
	return;
#endif

    ChangeCheck(hApp, IDM_USEMOUSE, useMouse ? IDM_USEMOUSE:0);
    WinEnableMenuItem(/* can this situation be unzoomed back? */
	WinWindowFromID(WinQueryWindow(hApp, QW_PARENT), FID_MENU),
	IDM_MOUSE_UNZOOM,(gpPMmenu.where_zoom_queue & 1) ? TRUE : FALSE);
    WinEnableMenuItem(/* can this situation be unzoomed to the beginning? */
	WinWindowFromID(WinQueryWindow(hApp, QW_PARENT), FID_MENU),
	IDM_MOUSE_UNZOOMALL,(gpPMmenu.where_zoom_queue & 2) ? TRUE : FALSE);
    if (
	WinEnableMenuItem(/* can this situation be zoomed next? */
	    WinWindowFromID(WinQueryWindow(hApp, QW_PARENT), FID_MENU),
	    IDM_MOUSE_ZOOMNEXT,(gpPMmenu.where_zoom_queue & 4) ? TRUE : FALSE)
	== TRUE) gpPMmenu_update_req = 0;
    ChangeCheck(hApp, IDM_MOUSE_POLAR_DISTANCE, gpPMmenu.polar_distance?IDM_MOUSE_POLAR_DISTANCE:0);
}


static void
DrawZoomBox()
{
    if (!zoombox.on)
	return;
    GpiSetLineWidth(hpsScreen, LINEWIDTH_NORMAL);
    GpiSetLineType(hpsScreen,LINETYPE_SHORTDASH);
    GpiSetMix(hpsScreen,FM_INVERT);
    GpiMove(hpsScreen, &zoombox.from);
    GpiBox(hpsScreen, DRO_OUTLINE, &zoombox.to, 0,0);
    GpiSetLineType(hpsScreen,LINETYPE_DEFAULT);
}

/* eof gclient.c */

